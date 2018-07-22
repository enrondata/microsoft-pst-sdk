#include <iostream>
#include <pstsdk/ndb.h>
#include <pstsdk/disk.h>
#include <pstsdk/util.h>
#include "testutils.h"


void test_node_save(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 = 0x38;
	node_id nid2 = 0x12F;
	node_id nid3 = 0x41;
	node_id nid4 = 0x43;
	node_id nid5 = 0x45;

	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();
		shared_ptr<bbt_page> bbt_root = db->read_bbt_root();

		// new node + new data block
		node new_node1 = db->create_node(nid1);
		new_node1.resize(test_utilities::small_chunk);
		vector<pstsdk::byte> v1(test_utilities::small_chunk, 1);
		new_node1.write(v1, 0);
		new_node1.save_node();

		// try create existing node
		try
		{
			node new_node2 = db->create_node(nid2);
			assert(false);
		}
		catch(pstsdk::duplicate_key<node_id>) { }

		// existing node + modified data block
		node_info nd_inf2 = nbt_root->lookup(nid2);
		node new_node2(db, nd_inf2);
		new_node2.resize(new_node2.get_data_block()->get_total_size() + test_utilities::small_chunk);
		vector<pstsdk::byte> v2(test_utilities::small_chunk, 1);
		new_node2.write(v2, 0);
		new_node2.save_node();

		// new node + existing data block
		node_info nd_inf3 = {nid3, 0x144, 0x0, 0x0};
		node new_node3(db, nd_inf3);
		new_node3.save_node();

		// new node + empty data block
		node_info nd_inf4 = {nid4, 0x0, 0x0, 0x0};
		node new_node4(db, nd_inf4);
		new_node4.save_node();

		// new node + new data block (data tree)
		node new_node5 = db->create_node(nid5);
		new_node5.resize(test_utilities::large_chunk);
		vector<pstsdk::byte> v5(test_utilities::large_chunk, 1);
		new_node5.write(v5, 0);
		new_node5.save_node();

		//verify changes in memory
		nbt_root = db->read_nbt_root();
		bbt_root = db->read_bbt_root();

		nbt_root->lookup(nid1);
		nbt_root->lookup(nid2);
		nbt_root->lookup(nid3);
		nbt_root->lookup(nid4);
		nbt_root->lookup(nid5);
		bbt_root->lookup(new_node1.get_data_id());
		bbt_root->lookup(nd_inf2.data_bid);
		bbt_root->lookup(nd_inf3.data_bid);
		bbt_root->lookup(new_node5.get_data_id());
	}

	// verify changes not committed to db
	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();
		shared_ptr<bbt_page> bbt_root = db->read_bbt_root();

		try
		{
			nbt_root->lookup(nid1);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}

		nbt_root->lookup(nid2);

		try
		{
			nbt_root->lookup(nid3);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}

		try
		{
			nbt_root->lookup(nid4);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}
	}
}

void test_node_save_commit(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 = 0x38;
	node_id nid2 = 0x12F;
	node_id nid3 = 0x41;
	node_id nid4 = 0x43;
	node_id nid5 = 0x45;
	node_id nid6 = 0x46;

	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();

		// new node + new data block
		node new_node1 = db->create_node(nid1);
		new_node1.resize(test_utilities::small_chunk);
		vector<pstsdk::byte> v1(test_utilities::small_chunk, 1);
		new_node1.write(v1, 0);
		new_node1.save_node();

		// exising node + modified data block
		node_info nd_inf2 = nbt_root->lookup(nid2);
		node new_node2(db, nd_inf2);
		new_node2.resize(new_node2.get_data_block()->get_total_size() + test_utilities::small_chunk);
		vector<pstsdk::byte> v2(test_utilities::small_chunk, 1);
		new_node2.write(v2, 0);
		new_node2.save_node();

		// new node + existing data block
		node_info nd_inf3 = {nid3, 0x144, 0x0, 0x0};
		node new_node3(db, nd_inf3);
		new_node3.save_node();

		// new node + empty data block
		node_info nd_inf4 = {nid4, 0x0, 0x0, 0x0};
		node new_node4(db, nd_inf4);
		new_node4.save_node();

		// assign existing node to new node and save
		node new_node6 = db->create_node(nid6);
		new_node6 = new_node2;
		new_node6.resize(new_node6.size() + 1024);
		new_node6.save_node();

		db->commit_db();
	}

	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();
		shared_ptr<bbt_page> bbt_root = db->read_bbt_root();

		// existing node + modified data block
		node_info nd_inf4 = nbt_root->lookup(nid4);
		node new_node4(db, nd_inf4);
		new_node4.resize(new_node4.get_data_block()->get_total_size() + test_utilities::small_chunk);
		vector<pstsdk::byte> v4(test_utilities::small_chunk, 1);
		new_node4.write(v4, 0);
		new_node4.save_node();

		// new node + new data block (data tree)
		node new_node5 = db->create_node(nid5);
		new_node5.resize(test_utilities::large_chunk);
		vector<pstsdk::byte> v5(test_utilities::large_chunk, 1);
		new_node5.write(v5, 0);
		new_node5.save_node();
		db->commit_db();
	}

	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();
		shared_ptr<bbt_page> bbt_root = db->read_bbt_root();

		// existing node + modify data tree
		node_info nd_inf5 = nbt_root->lookup(nid5);
		node new_node5(db, nd_inf5);
		new_node5.resize((2 * test_utilities::large_chunk));
		vector<pstsdk::byte> v5((2 * test_utilities::large_chunk), 1);
		new_node5.write(v5, test_utilities::large_chunk);
		new_node5.save_node();
		db->commit_db();
	}

	//verify changes committed to disk
	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();
		shared_ptr<bbt_page> bbt_root = db->read_bbt_root();

		node_info nd_inf1 = nbt_root->lookup(nid1);
		node_info nd_inf2 = nbt_root->lookup(nid2);
		node_info nd_inf3 = nbt_root->lookup(nid3);
		node_info nd_inf4 = nbt_root->lookup(nid4);
		node_info nd_inf5 = nbt_root->lookup(nid5);
		node_info nd_inf6 = nbt_root->lookup(nid6);

		bbt_root->lookup(nd_inf1.data_bid);
		bbt_root->lookup(nd_inf2.data_bid);
		bbt_root->lookup(nd_inf3.data_bid);
		bbt_root->lookup(nd_inf4.data_bid);
		bbt_root->lookup(nd_inf5.data_bid);
		bbt_root->lookup(nd_inf6.data_bid);
	}
}

void test_create_subnode(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	pstsdk::node_id nid = 0x61;
	pstsdk::node_id sb_nid = 0x430;

	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();

		pstsdk::node_info nInfo = nbt_root->lookup(nid);
		node nd(db, nInfo);

		node sb_nd = nd.create_subnode(sb_nid);
		sb_nd.resize(test_utilities::small_chunk);
		vector<pstsdk::byte> v(test_utilities::small_chunk, 1);
		sb_nd.write(v, 0);

		// verify changes not reflected in parent until save
		shared_ptr<subnode_block> sub_blk = nd.get_subnode_block();

		try
		{
			sub_blk->lookup(sb_nid);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}

		nd.save_subnode(sb_nd);

		// verify changes reflected in parent after save
		sub_blk = nd.get_subnode_block();
		sub_blk->lookup(sb_nid);

		// try create existing subnode
		try
		{
			node sb_nd = nd.create_subnode(sb_nid);
			assert(false);
		}
		catch(pstsdk::duplicate_key<node_id>) { }
	}

	// verify changes not committed to db
	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();

		pstsdk::node_info nInfo = nbt_root->lookup(nid);
		node nd(db, nInfo);
		shared_ptr<subnode_block> sub_blk = nd.get_subnode_block();

		try
		{
			sub_blk->lookup(sb_nid);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}
	}
}

void test_create_subnode_commit(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	pstsdk::node_id nid1 = 0x122;
	pstsdk::node_id sb_nid1 = 0x430;
	pstsdk::node_id nid2 = 0x60D;
	pstsdk::node_id sb_nid2 = 0x431;
	pstsdk::node_id sb_nid3 = 0x432;
	pstsdk::node_id sb_nid4 = 0x433;

	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();

		// save using save_subnode()
		pstsdk::node_info nInfo1 = nbt_root->lookup(nid1);
		node nd1(db, nInfo1);

		node sb_nd1 = nd1.create_subnode(sb_nid1);
		sb_nd1.resize(test_utilities::small_chunk);
		vector<pstsdk::byte> v1(test_utilities::small_chunk, 1);
		sb_nd1.write(v1, 0);
		nd1.save_subnode(sb_nd1);
		nd1.save_node();

		// save using save_node()
		pstsdk::node_info nInfo2 = nbt_root->lookup(nid2);
		node nd2(db, nInfo2);

		node sb_nd2 = nd2.create_subnode(sb_nid2);
		sb_nd2.resize(test_utilities::small_chunk);
		vector<pstsdk::byte> v2(test_utilities::small_chunk, 1);
		sb_nd2.write(v2, 0);
		sb_nd2.save_node();

		// create subnode to subnode
		node sb_nd3 = sb_nd2.create_subnode(sb_nid3);
		sb_nd3.resize(test_utilities::large_chunk);
		vector<pstsdk::byte> v3(test_utilities::large_chunk, 1);
		sb_nd3.write(v3, 0);
		sb_nd3.save_node();
		sb_nd2.save_node();

		node sb_nd4 = nd2.create_subnode(sb_nid4);
		sb_nd4.resize(test_utilities::small_chunk);
		vector<pstsdk::byte> v4(test_utilities::small_chunk, 1);
		sb_nd4.write(v4, 0);
		sb_nd4.save_node();
		nd2.save_node();

		db->commit_db();
	}

	// verify changes committed to db
	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();

		pstsdk::node_info nInfo1 = nbt_root->lookup(nid1);
		node nd1(db, nInfo1);
		shared_ptr<subnode_block> sub_blk1 = nd1.get_subnode_block();
		sub_blk1->lookup(sb_nid1);

		pstsdk::node_info nInfo2 = nbt_root->lookup(nid2);
		node nd2(db, nInfo2);
		shared_ptr<subnode_block> sub_blk2 = nd2.get_subnode_block();
		sub_blk2->lookup(sb_nid2);
	}

	// create large number of subnodes such that a subnode tree is created
	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();

		node nd1 = db->lookup_node(nid1);

		for(size_t sb_nid = sb_nid4 + 1; sb_nid < (sb_nid4 + test_utilities::small_chunk); ++sb_nid)
		{
			node sbnd = nd1.create_subnode(sb_nid);
			sbnd.save_node();
		}

		nd1.save_node();
		db->commit_db();

		// verify all subnodes created
		shared_ptr<subnode_block> sub_blk1 = nd1.get_subnode_block();

		for(size_t sb_nid = sb_nid4 + 1; sb_nid < (sb_nid4 + test_utilities::small_chunk); ++sb_nid)
		{
			sub_blk1->lookup(sb_nid);
		}
	}
}

void test_node_delete(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 = 0x38;
	node_id nid2 = 0x41;
	node_id nid3 = 0x45;

	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();
		shared_ptr<bbt_page> bbt_root = db->read_bbt_root();

		node_info nd_inf1 = nbt_root->lookup(nid1);
		db->delete_node(nid1);

		node_info nd_inf2 = nbt_root->lookup(nid2);
		db->delete_node(nid2);

		node_info nd_inf3 = nbt_root->lookup(nid3);
		db->delete_node(nid3);


		//verify changes in memory
		nbt_root = db->read_nbt_root();
		bbt_root = db->read_bbt_root();


		try
		{
			nbt_root->lookup(nid1);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}

		try
		{
			nbt_root->lookup(nid2);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}

		try
		{
			nbt_root->lookup(nid3);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}
	}

	// verify changes not committed to db
	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();
		shared_ptr<bbt_page> bbt_root = db->read_bbt_root();

		node_info nd_inf1 = nbt_root->lookup(nid1);
		node_info nd_inf2 = nbt_root->lookup(nid2);
		node_info nd_inf3 = nbt_root->lookup(nid3);
		bbt_root->lookup(nd_inf1.data_bid);
		bbt_root->lookup(nd_inf2.data_bid);
		bbt_root->lookup(nd_inf3.data_bid);
	}
}

void test_node_delete_commit(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 = 0x38;
	node_id nid2 = 0x12F;
	node_id nid3 = 0x41;
	node_id nid4 = 0x43;
	node_id nid5 = 0x45;

	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();
		shared_ptr<bbt_page> bbt_root = db->read_bbt_root();

		node_info nd_inf1 = nbt_root->lookup(nid1);
		db->delete_node(nid1);

		node_info nd_inf2 = nbt_root->lookup(nid2);
		db->delete_node(nid2);

		node_info nd_inf3 = nbt_root->lookup(nid3);
		db->delete_node(nid3);

		node_info nd_inf4 = nbt_root->lookup(nid4);
		db->delete_node(nid4);

		node_info nd_inf5 = nbt_root->lookup(nid5);
		db->delete_node(nid5);

		db->commit_db();
	}

	// verify changes committed to db
	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();
		shared_ptr<bbt_page> bbt_root = db->read_bbt_root();

		try
		{
			nbt_root->lookup(nid1);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}

		try
		{
			nbt_root->lookup(nid2);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}

		try
		{
			nbt_root->lookup(nid3);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}

		try
		{
			nbt_root->lookup(nid4);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}

		try
		{
			nbt_root->lookup(nid4);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}
	}
}

void test_subnode_delete(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	pstsdk::node_id nid1 = 0x122;
	pstsdk::node_id nid2 = 0x60D;
	pstsdk::node_id sb_nid1 = 0x430;
	pstsdk::node_id sb_nid2 = 0x431;
	pstsdk::node_id sb_nid3 = 0x432;
	pstsdk::node_id sb_nid4 = 0x433;

	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();

		// delete subnode
		pstsdk::node_info nd_inf1 = nbt_root->lookup(nid1);
		node nd1(db, nd_inf1);
		nd1.delete_subnode(sb_nid1);

		// delete subnode
		pstsdk::node_info nd_inf2 = nbt_root->lookup(nid2);
		node nd2(db, nd_inf2);
		nd2.delete_subnode(sb_nid4);

		// delete subnode of a subnode
		shared_ptr<subnode_block> sub_blk2 = nd2.get_subnode_block();
		subnode_info sbnd_inf2 =  sub_blk2->lookup(sb_nid2);
		node sb_nd2(nd2, sbnd_inf2);
		sb_nd2.delete_subnode(sb_nid3);

		// verify changes done in memory
		nbt_root = db->read_nbt_root();
		nd_inf1 = nbt_root->lookup(nid1);
		node nd11(db, nd_inf1);
		shared_ptr<subnode_block> sub_blk11 = nd11.get_subnode_block();

		try
		{
			sub_blk11->lookup(sb_nid1);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}

		nd_inf2 = nbt_root->lookup(nid2);
		node nd21(db, nd_inf2);
		shared_ptr<subnode_block> sub_blk21 = nd21.get_subnode_block();

		try
		{
			sub_blk21->lookup(sb_nid4);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}

		subnode_info sbnd_inf21 = sub_blk21->lookup(sb_nid2);
		node sb_nd22(nd21, sbnd_inf21);
		shared_ptr<subnode_block> sub_blk22 = sb_nd22.get_subnode_block();

		try
		{
			sub_blk22->lookup(sb_nid3);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}
	}

	// verify changes not committed to db
	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();

		pstsdk::node_info nd_inf1 = nbt_root->lookup(nid1);
		node nd1(db, nd_inf1);
		shared_ptr<subnode_block> sub_blk1 = nd1.get_subnode_block();
		sub_blk1->lookup(sb_nid1);

		pstsdk::node_info nd_inf2 = nbt_root->lookup(nid2);
		node nd2(db, nd_inf2);
		shared_ptr<subnode_block> sub_blk2 = nd2.get_subnode_block();
		subnode_info sbnd_inf2 = sub_blk2->lookup(sb_nid2);
		sub_blk2->lookup(sb_nid4);

		node sd_nd2(nd2, sbnd_inf2);
		shared_ptr<subnode_block> sub_blk22 = sd_nd2.get_subnode_block();
		sub_blk22->lookup(sb_nid3);
	}
}

void test_subnode_delete_commit(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	pstsdk::node_id nid1 = 0x122;
	pstsdk::node_id nid2 = 0x60D;
	pstsdk::node_id sb_nid1 = 0x430;
	pstsdk::node_id sb_nid2 = 0x431;
	pstsdk::node_id sb_nid3 = 0x432;
	pstsdk::node_id sb_nid4 = 0x433;

	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();

		// delete subnode
		pstsdk::node_info nd_inf1 = nbt_root->lookup(nid1);
		node nd1(db, nd_inf1);
		nd1.delete_subnode(sb_nid1);

		// delete subnode
		pstsdk::node_info nd_inf2 = nbt_root->lookup(nid2);
		node nd2(db, nd_inf2);
		nd2.delete_subnode(sb_nid4);

		// delete subnode of a subnode
		shared_ptr<subnode_block> sub_blk2 = nd2.get_subnode_block();
		subnode_info sbnd_inf2 =  sub_blk2->lookup(sb_nid2);
		node sb_nd2(nd2, sbnd_inf2);
		sb_nd2.delete_subnode(sb_nid3);

		db->commit_db();
	}

	// verify changes not committed to db
	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();

		nbt_root = db->read_nbt_root();
		node_info nd_inf1 = nbt_root->lookup(nid1);
		node nd11(db, nd_inf1);
		shared_ptr<subnode_block> sub_blk11 = nd11.get_subnode_block();

		try
		{
			sub_blk11->lookup(sb_nid1);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}

		node_info nd_inf2 = nbt_root->lookup(nid2);
		node nd21(db, nd_inf2);
		shared_ptr<subnode_block> sub_blk21 = nd21.get_subnode_block();

		try
		{
			sub_blk21->lookup(sb_nid4);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}

		subnode_info sbnd_inf21 = sub_blk21->lookup(sb_nid2);
		node sb_nd22(nd21, sbnd_inf21);
		shared_ptr<subnode_block> sub_blk22 = sb_nd22.get_subnode_block();

		try
		{
			sub_blk22->lookup(sb_nid3);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}
	}

	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();

		// delete subnode
		pstsdk::node_info nd_inf2 = nbt_root->lookup(nid2);
		node nd2(db, nd_inf2);
		nd2.delete_subnode(sb_nid2);

		db->commit_db();
	}

	// verify changes not committed to db
	{
		shared_db_ptr db = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db->read_nbt_root();

		nbt_root = db->read_nbt_root();
		node_info nd_inf2 = nbt_root->lookup(nid2);
		node nd2(db, nd_inf2);
		shared_ptr<subnode_block> sub_blk2 = nd2.get_subnode_block();

		try
		{
			sub_blk2->lookup(sb_nid2);
			assert(false);
		}
		catch(key_not_found<node_id>)
		{	}
	}
}

void test_node_save()
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
		test_node_save(tmp_large_file);

		test_node_save_commit(tmp_large_file);

		test_node_delete(tmp_large_file);

		test_node_delete_commit(tmp_large_file);

		test_create_subnode(tmp_large_file);

		test_create_subnode_commit(tmp_large_file);

		test_subnode_delete(tmp_large_file);

		test_subnode_delete_commit(tmp_large_file);


		// only valid to call on test_ansi.pst
		test_node_save(tmp_small_file);

		test_node_save_commit(tmp_small_file);

		test_node_delete(tmp_small_file);

		test_node_delete_commit(tmp_small_file);

		test_create_subnode(tmp_small_file);

		test_create_subnode_commit(tmp_small_file);

		test_subnode_delete(tmp_small_file);

		test_subnode_delete_commit(tmp_small_file);


		test_utilities::delete_file(tmp_large_file);
		test_utilities::delete_file(tmp_small_file);
	}
}