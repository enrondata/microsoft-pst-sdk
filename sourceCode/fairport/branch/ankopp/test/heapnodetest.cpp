#include <iostream>
#include <pstsdk/ndb.h>
#include <pstsdk/disk.h>
#include <pstsdk/ltp.h>
#include <pstsdk/util.h>
#include "testutils.h"


void test_heap_read(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x122;
	shared_db_ptr db = open_database(filename);

	node nd = db->lookup_node(nid1);

	heap h(nd);
	disk::heap_first_header first_header = nd.read<disk::heap_first_header>(0);

	disk::heap_page_map page_map = nd.read<disk::heap_page_map>(first_header.page_map_offset);

	heap_id hid = h.get_root_id();
	size_t heap_size = h.size(hid);
	pstsdk::byte cli_sig = h.get_client_signature();
	vector<pstsdk::byte> buff = h.read(hid);

	assert(hid == first_header.root_id);
	assert(cli_sig == first_header.client_signature);
	assert(buff.size() == heap_size);

	std::tr1::shared_ptr<bth_node<pstsdk::ushort, disk::prop_entry>> bth = h.open_bth<pstsdk::ushort, disk::prop_entry>(hid);

	cout<<"Listing Properties for Heap Id: "<<hid<<endl;

	for(const_btree_node_iter<pstsdk::ushort, disk::prop_entry> itr = bth->begin(); itr != bth->end(); ++itr)
	{
		cout<<"Property Id: "<<itr->id<<" Property Type: "<<itr->type<<endl;
	}

	cout<<endl;
}

void test_heap_write(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	shared_db_ptr db = open_database(filename);

	node_id nid1 = 0x43;
	node new_nd = db->create_node(nid1);
	heap hn1(new_nd, disk::heap_sig_bth);

	hn1.save_heap();

	node_id nid2 = 0x12D;
	heap_id hid1 = 0x80;
	heap_id hid2 = 0xC0;
	heap hn2(db->lookup_node(nid2));
	hn2.free_heap_item(hid1);
	hn2.save_heap();

	hn2.re_allocate_heap_item(hid2, 1870);
	hn2.save_heap();

	db->commit_db();
}

void test_heap_create(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 = 0x43;
	node_id nid2 = 0x45;
	heap_id usr_hp_id1 = 0x0;
	heap_id usr_hp_id2 = 0x0;

	{
		shared_db_ptr db = open_database(filename);

		// Create Heap 1
		node nd1 = db->create_node(nid1);
		heap hn1(nd1, disk::heap_sig_bth);

		// create user root 
		usr_hp_id1 = hn1.allocate_heap_item(20);
		hn1.set_root_id(usr_hp_id1);

		// verify user root created
		assert(hn1.get_root_id() == usr_hp_id1);
		assert(hn1.size(usr_hp_id1) == 20);

		// save heap changes
		hn1.save_heap();
		
		// Create Heap 2
		node nd2 = db->create_node(nid2);
		heap hn2(nd2, disk::heap_sig_bth);

		// create user root 
		usr_hp_id2 = hn2.allocate_heap_item(40);
		hn2.set_root_id(usr_hp_id2);

		// verify user root created
		assert(hn2.get_root_id() == usr_hp_id2);
		assert(hn2.size(usr_hp_id2) == 40);

		// save heap changes
		hn2.save_heap();

		// commit to disk
		db->commit_db();
	}

	// verify changes committed to disk
	{
		shared_db_ptr db = open_database(filename);

		node nd1 = db->lookup_node(nid1);
		heap hn1(nd1);

		// verify user root created
		assert(hn1.get_root_id() == usr_hp_id1);
		assert(hn1.size(usr_hp_id1) == 20);

		node nd2 = db->lookup_node(nid2);
		heap hn2(nd2);

		// verify user root created
		assert(hn2.get_root_id() == usr_hp_id2);
		assert(hn2.size(usr_hp_id2) == 40);
	}
}

void test_heap_allocate(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 = 0x47;
	heap_id hid1 = 0x0;
	node_id nid2 = 0x12D;
	heap_id hid2 = 0x0;
	heap_id hid3 = 0x40;
	node_id nid3 = 0x2223;
	heap_id hid4 = 0x40;
	heap_id hid5 = 0x60;

	// create new heap
	{
		shared_db_ptr db = open_database(filename);
		node nd1 = db->create_node(nid1);
		heap hn1(nd1, disk::heap_sig_bth);

		// try allocating > 3.8K heap
		try
		{
			hn1.allocate_heap_item(test_utilities::large_chunk);
			assert(false);
		}
		catch(std::invalid_argument) { }


		// allocate large heap item
		hid1 = hn1.allocate_heap_item(test_utilities::small_chunk);

		// verify heap item created
		assert(hn1.size(hid1) == test_utilities::small_chunk);
		assert(hn1.get_node().get_page_count() == 1);

		// re-allocate (grow) heap item
		hid1 = hn1.re_allocate_heap_item(hid1, (2 * test_utilities::small_chunk));

		// verify heap item grown
		assert(hn1.size(hid1) == (2 * test_utilities::small_chunk));

		// re-allocate (shrink) heap item
		hid1 = hn1.re_allocate_heap_item(hid1, (test_utilities::small_chunk / 2));

		// verify heap item shrinked
		assert(hn1.size(hid1) == (test_utilities::small_chunk / 2));

		vector<heap_id> hids;
		// allocate multiple heap items to cause data tree creation
		for(size_t ind = 0; ind < 25; ++ind)
		{
			hids.push_back(hn1.allocate_heap_item(3 * test_utilities::small_chunk));
		}

		assert(hn1.get_node().get_page_count() > 8);

		// verify allocations
		for(size_t ind = 0; ind < hids.size(); ++ind)
		{
			assert(hn1.size(hids[ind]) == (3 * test_utilities::small_chunk));
		}

		// save heap changes
		hn1.save_heap();

		for(size_t ind = 0; ind < 275; ++ind)
		{
			hn1.allocate_heap_item(3 * test_utilities::small_chunk);
		}

		assert(hn1.get_node().get_page_count() > 128);

		// commit to disk
		db->commit_db();
	}

	// use existing heap
	{
		shared_db_ptr db = open_database(filename);
		node nd2 = db->lookup_node(nid2);
		heap hn2(nd2);

		// try allocating > 3.8K heap
		try
		{
			hn2.allocate_heap_item(test_utilities::large_chunk);
			assert(false);
		}
		catch(std::invalid_argument) { }


		// allocate large heap item
		hid2 = hn2.allocate_heap_item(test_utilities::small_chunk);

		// verify heap item created
		assert(hn2.size(hid2) == test_utilities::small_chunk);
		assert(hn2.get_node().get_page_count() == 1);

		// re-allocate (grow) heap item
		heap_id hid4 = hn2.re_allocate_heap_item(hid3, (2 * test_utilities::small_chunk));

		// verify heap item grown
		assert(hn2.size(hid3) == (2 * test_utilities::small_chunk));
		assert(hid4 == hid3);

		// re-allocate (shrink) heap item
		hid3 = hn2.re_allocate_heap_item(hid3, (test_utilities::small_chunk / 2));

		// verify heap item shrinked
		assert(hn2.size(hid3) == (test_utilities::small_chunk / 2));

		vector<heap_id> hids;
		// allocate multiple heap items to cause data tree creation
		for(size_t ind = 0; ind < 25; ++ind)
		{
			hids.push_back(hn2.allocate_heap_item(3 * test_utilities::small_chunk));
		}

		assert(hn2.get_node().get_page_count() > 8);

		// verify allocations
		for(size_t ind = 0; ind < hids.size(); ++ind)
		{
			assert(hn2.size(hids[ind]) == (3 * test_utilities::small_chunk));
		}
		
		// save heap changes
		hn2.save_heap();
		
		for(size_t ind = 0; ind < 275; ++ind)
		{
			hn2.allocate_heap_item(3 * test_utilities::small_chunk);
		}

		assert(hn2.get_node().get_page_count() > 128);

		// commit to disk
		db->commit_db();
	}

	// test re-allocate
	{
		shared_db_ptr db = open_database(filename);
		node nd3 = db->lookup_node(nid3);
		heap hn3(nd3);

		vector<pstsdk::byte> buff1 = hn3.read(hid4);
		vector<pstsdk::byte> buff2 = hn3.read(hid5);

		buff1.resize(buff1.size() * 2, 0);
		buff2.resize(buff2.size() * 2, 0);

		hid5 = hn3.re_allocate_heap_item(hid5, buff2.size());
		hn3.write(buff2, hid5);

		hid4 = hn3.re_allocate_heap_item(hid4, buff1.size());
		hn3.write(buff1, hid4);

		vector<pstsdk::byte> buff11 = hn3.read(hid4);
		vector<pstsdk::byte> buff22 = hn3.read(hid5);

		assert(memcmp(&buff1[0], &buff11[0], buff1.size()) == 0);
		assert(memcmp(&buff2[0], &buff22[0], buff2.size()) == 0);

		// save changes to heap
		hn3.save_heap();

		// commit to disk
		db->commit_db();
	}

	// verify changes committed to disk
	{
		shared_db_ptr db = open_database(filename);
		node nd1 = db->lookup_node(nid1);
		heap hn1(nd1);

		// verify large heap item created
		assert(hn1.size(hid1) == test_utilities::small_chunk / 2);

		// verify data tree created
		assert(hn1.get_node().get_page_count() > 8);

		node nd2 = db->lookup_node(nid2);
		heap hn2(nd2);

		// verify large heap item created
		assert(hn2.size(hid3) == test_utilities::small_chunk / 2);

		// verify data tree created
		assert(hn2.get_node().get_page_count() > 8);
	}
}

void test_heap_free(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 = 0x12D;
	heap_id hid1 = 0x80;
	heap_id hid2 = 0xC0;
	heap_id hid3 = 0x20;
	heap_id hid4 = 0xE0;

	{
		shared_db_ptr db = open_database(filename);
		node nd1 = db->lookup_node(nid1);
		heap hn1(nd1);

		// Free heap items in between
		hn1.free_heap_item(hid1);
		assert(hn1.size(hid1) == 0);

		hn1.free_heap_item(hid2);
		assert(hn1.size(hid2) == 0);

		// Free heap item at begining
		hn1.free_heap_item(hid3);
		assert(hn1.size(hid3) == 0);

		// Free heap item at end
		hn1.free_heap_item(hid4);
		assert(hn1.size(hid4) == 0);

		// Free already freed heap item
		hn1.free_heap_item(hid4);
		assert(hn1.size(hid4) == 0);

		// save heap changes
		hn1.save_heap();

		// commit to disk
		db->commit_db();
	}

	// verify changes committed to disk
	{
		shared_db_ptr db = open_database(filename);
		node nd1 = db->lookup_node(nid1);
		heap hn1(nd1);

		assert(hn1.size(hid1) == 0);
		assert(hn1.size(hid2) == 0);
		assert(hn1.size(hid3) == 0);
		assert(hn1.size(hid4) == 0);

		vector<pstsdk::byte> buff = hn1.read(0xA0);
		assert(buff[0] == 'T');
	}
}

void test_heap_modify(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 = 0x21;
	heap_id hid1 = 0x80;

	wstring wstr1 = L"Modified Personal Folders";
	wstring wstr2 = L"Again Modified Personal Folders";

	{
		shared_db_ptr db = open_database(filename);
		node nd1 = db->lookup_node(nid1);
		heap hn1(nd1);

		vector<pstsdk::byte> buff = wstring_to_bytes(wstr1);
		hid1 = hn1.re_allocate_heap_item(hid1, buff.size());
		hn1.write(buff, hid1);

		// save heap changes
		hn1.save_heap();

		buff = hn1.read(hid1);
		assert(memcmp(&buff[0], &wstring_to_bytes(wstr1)[0], wstr1.size()) == 0);

		buff = wstring_to_bytes(wstr2);
		hid1 = hn1.re_allocate_heap_item(hid1, buff.size());
		hn1.write(buff, hid1);

		// save heap changes
		hn1.save_heap();

		// commit to disk
		db->commit_db();
	}

	// verify changes committed to disk
	{
		shared_db_ptr db = open_database(filename);
		node nd1 = db->lookup_node(nid1);
		heap hn1(nd1);

		vector<pstsdk::byte> buff = hn1.read(hid1);
		assert(memcmp(&buff[0], &wstring_to_bytes(wstr2)[0], wstr2.size()) == 0);
	}
}

void test_heap_node()
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
		test_heap_read(tmp_large_file);

		test_heap_create(tmp_large_file);

		test_heap_free(tmp_large_file);

		test_heap_modify(tmp_large_file);

		test_heap_allocate(tmp_large_file);


		// only valid to call on test_ansi.pst
		test_heap_read(tmp_small_file);

		test_heap_create(tmp_small_file);

		test_heap_free(tmp_small_file);

		test_heap_allocate(tmp_small_file);

		test_heap_modify(tmp_small_file);


		test_utilities::delete_file(tmp_large_file);
		test_utilities::delete_file(tmp_small_file);
	}
	else
	{
		throw runtime_error("error creating temp files");
	}
}