#include <iostream>
#include <pstsdk/ndb.h>
#include <pstsdk/disk.h>
#include <pstsdk/ltp.h>
#include <pstsdk/util.h>
#include "testutils.h"


void test_bth_read(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x122;
	shared_db_ptr db = open_database(filename);

	node nd = db->lookup_node(nid1);

	heap hn(nd);

	heap_id hid = hn.get_root_id();

	std::tr1::shared_ptr<bth_node<pstsdk::ushort, disk::prop_entry>> bth = hn.open_bth<pstsdk::ushort, disk::prop_entry>(hid);

	cout<<"Listing Properties for Heap Id: "<<hid<<endl;

	for(const_btree_node_iter<pstsdk::ushort, disk::prop_entry> itr = bth->begin(); itr != bth->end(); ++itr)
	{
		cout<<"Property Id: "<<itr->id<<" Property Type: "<<itr->type<<endl;
	}

	cout<<endl;
}

void test_bth_create(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x12D;

	{
		shared_db_ptr db = open_database(filename);

		node nd = db->lookup_node(nid1);

		heap hn(nd);

		heap_id hid = hn.create_bth<ushort, disk::prop_entry>();
		hn.set_root_id(hid);

		std::tr1::shared_ptr<bth_node<pstsdk::ushort, disk::prop_entry>> bth = hn.open_bth<pstsdk::ushort, disk::prop_entry>(hid);

		cout<<"Listing Properties for Heap Id: "<<hid<<endl;

		for(const_btree_node_iter<pstsdk::ushort, disk::prop_entry> itr = bth->begin(); itr != bth->end(); ++itr)
		{
			cout<<"Property Id: "<<itr->id<<" Property Type: "<<itr->type<<endl;
		}

		cout<<endl;

		bth->save_bth();
		db->commit_db();
	}

	// test intigrity
	test_bth_read(filename);
}

void test_bth_insert(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x42;

	{
		shared_db_ptr db = open_database(filename);

		node nd = db->create_node(nid1);
		heap hn(nd, disk::heap_sig_bth);

		heap_id hid = hn.create_bth<ushort, disk::prop_entry>();
		hn.set_root_id(hid);

		std::tr1::shared_ptr<bth_node<pstsdk::ushort, disk::prop_entry>> bth = hn.open_bth<pstsdk::ushort, disk::prop_entry>(hid);

		// insert 1
		disk::prop_entry entry1 = { 900, 9000 };
		bth = bth->insert(900, entry1);

		//insert 2
		for(int i = 1000; i < 1450; i++)
		{
			disk::prop_entry entry = { i, i * 10 };
			bth = bth->insert(i, entry);
		}

		bth->save_bth();
		db->commit_db();
	}

	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);
		heap hn(nd);

		heap_id hid = hn.get_root_id();
		std::tr1::shared_ptr<bth_node<pstsdk::ushort, disk::prop_entry>> bth = hn.open_bth<pstsdk::ushort, disk::prop_entry>(hid);

		assert(bth->get_level() > 0);

		pstsdk::ushort key = bth->get_key(0);
		disk::prop_entry entry = bth->lookup(900);
		assert(entry.type == 900);
		assert(entry.id == 9000);

		entry = bth->lookup(1449);
		assert(entry.type == 1449);
		assert(entry.id == 14490);
	} 

	// test intigrity
	test_bth_read(filename);
}

void test_bth_modify(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x122;
	node_id nid2 =  0x42;

	// modify 1
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);
		heap hn(nd);

		heap_id hid = hn.get_root_id();

		std::tr1::shared_ptr<bth_node<pstsdk::ushort, disk::prop_entry>> bth = hn.open_bth<pstsdk::ushort, disk::prop_entry>(hid);

		disk::prop_entry new_entry = { 0x3603, 1450 };
		bth->modify(0x3603, new_entry);

		bth->save_bth();
		db->commit_db();
	}

	// modify 2
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid2);
		heap hn(nd);

		heap_id hid = hn.get_root_id();
		std::tr1::shared_ptr<bth_node<pstsdk::ushort, disk::prop_entry>> bth = hn.open_bth<pstsdk::ushort, disk::prop_entry>(hid);

		disk::prop_entry new_entry = { 900, 1450 };
		bth->modify(900, new_entry);

		bth->save_bth();
		db->commit_db();
	}

	// verify 1
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);
		heap hn(nd);

		heap_id hid = hn.get_root_id();
		std::tr1::shared_ptr<bth_node<pstsdk::ushort, disk::prop_entry>> bth = hn.open_bth<pstsdk::ushort, disk::prop_entry>(hid);

		disk::prop_entry entry = bth->lookup(0x3603);
		assert(entry.type == 0x3603);
		assert(entry.id == 1450);
	}

	// verify 2
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid2);
		heap hn(nd);

		heap_id hid = hn.get_root_id();
		std::tr1::shared_ptr<bth_node<pstsdk::ushort, disk::prop_entry>> bth = hn.open_bth<pstsdk::ushort, disk::prop_entry>(hid);

		disk::prop_entry entry = bth->lookup(900);
		assert(entry.type == 900);
		assert(entry.id == 1450);
	}

	// test intigrity
	test_bth_read(filename);
}

void test_bth_remove(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x122;

	// remove from between
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);
		heap hn(nd);

		heap_id hid = hn.get_root_id();

		std::tr1::shared_ptr<bth_node<pstsdk::ushort, disk::prop_entry>> bth = hn.open_bth<pstsdk::ushort, disk::prop_entry>(hid);
		bth->remove(0x3602);

		bth->save_bth();
		db->commit_db();
	}

	// remove first item
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);
		heap hn(nd);

		heap_id hid = hn.get_root_id();

		std::tr1::shared_ptr<bth_node<pstsdk::ushort, disk::prop_entry>> bth = hn.open_bth<pstsdk::ushort, disk::prop_entry>(hid);
		bth->remove(0x3001);

		bth->save_bth();
		db->commit_db();
	}

	// remove last item
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);
		heap hn(nd);

		heap_id hid = hn.get_root_id();

		std::tr1::shared_ptr<bth_node<pstsdk::ushort, disk::prop_entry>> bth = hn.open_bth<pstsdk::ushort, disk::prop_entry>(hid);
		bth->remove(0x360A);

		bth->save_bth();
		db->commit_db();
	}

	// verify
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);
		heap hn(nd);

		heap_id hid = hn.get_root_id();

		std::tr1::shared_ptr<bth_node<pstsdk::ushort, disk::prop_entry>> bth = hn.open_bth<pstsdk::ushort, disk::prop_entry>(hid);

		try
		{
			bth->lookup(0x3001);
			assert(false);
		} catch(key_not_found<prop_id>) { }

		try
		{
			bth->lookup(0x3602);
			assert(false);
		} catch(key_not_found<prop_id>) { }

		try
		{
			bth->lookup(0x360A);
			assert(false);
		} catch(key_not_found<prop_id>) { }
	}

	// test intigrity
	test_bth_read(filename);
}

void test_bth()
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	std::wstring large_file = L"test_unicode.pst";
	std::wstring small_file = L"test_ansi.pst";

	std::wstring tmp_large_file = L"tmp_test_unicode.pst";
	std::wstring tmp_small_file = L"tmp_test_ansi.pst";

	if(test_utilities::copy_file(large_file, tmp_large_file) && test_utilities::copy_file(small_file, tmp_small_file))
	{
		// only valid to call on test_unicode.pst
		test_bth_read(tmp_large_file);

		test_bth_create(tmp_large_file);

		test_bth_insert(tmp_large_file);

		test_bth_modify(tmp_large_file);

		test_bth_remove(tmp_large_file);


		// only valid to call on test_ansi.pst
		test_bth_read(tmp_small_file);

		test_bth_create(tmp_small_file);

		test_bth_insert(tmp_small_file);

		test_bth_modify(tmp_small_file);

		test_bth_remove(tmp_small_file);


		test_utilities::delete_file(tmp_large_file);
		test_utilities::delete_file(tmp_small_file);
	}
	else
	{
		throw runtime_error("error creating temp files");
	}
}