#include <iostream>
#include <pstsdk/ndb.h>
#include <pstsdk/disk.h>
#include <pstsdk/util.h>
#include "testutils.h"



void test_context_create(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 = 0x38;
	node_id nid2 = 0x39;
	node_id nid3 = 0x41;
	node_id nid4 = 0x43;

	{
		shared_db_ptr db_root = open_database(filename);

		node nd1 = db_root->create_node(nid1);
		nd1.save_node();

		// verify changes reflected in root context
		shared_ptr<nbt_page> nbt_root = db_root->read_nbt_root();
		nbt_root->lookup(nid1);

		// create child context from root context
		shared_db_ptr db_ctx1 = db_root->create_context();

		// verify child context has taken latest snapshot
		shared_ptr<nbt_page> nbt_root1 = db_ctx1->read_nbt_root();
		nbt_root1->lookup(nid1);

		// create child context from another child context
		shared_db_ptr db_ctx2 = db_ctx1->create_context();

		node nd2 = db_root->create_node(nid2);
		nd2.save_node();

		// verify changes reflected in root context
		nbt_root = db_root->read_nbt_root();
		nbt_root->lookup(nid2);

		// verify changes not reflected in child contexts
		nbt_root1 = db_ctx1->read_nbt_root();

		try
		{
			nbt_root1->lookup(nid2);
			assert(false);
		}
		catch(key_not_found<node_id>) {	}

		shared_ptr<nbt_page> nbt_root2 = db_ctx2->read_nbt_root();

		try
		{ 
			nbt_root2->lookup(nid2);
			assert(false);
		}
		catch(key_not_found<node_id>) {	}

		node nd3 = db_ctx1->create_node(nid3);
		nd3.save_node();

		node nd4 = db_ctx2->create_node(nid4);
		nd4.save_node();

		// verify nodes not reflected in parent contexts
		nbt_root = db_root->read_nbt_root();

		try
		{ 
			nbt_root->lookup(nid3);
			assert(false);
		}
		catch(key_not_found<node_id>) {	}

		nbt_root1 = db_ctx1->read_nbt_root();

		try
		{ 
			nbt_root1->lookup(nid4);
			assert(false);
		}
		catch(key_not_found<node_id>) {	}
	}
}

void test_context_commit_node(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 = 0x38;
	node_id nid2 = 0x39;
	node_id nid3 = 0x41;
	node_id extng_nid1 = 0x610;

	{
		shared_db_ptr db_root = open_database(filename);
		shared_db_ptr db_ctx1 = db_root->create_context();
		shared_db_ptr db_ctx2 = db_ctx1->create_context();

		node nd1 = db_root->create_node(nid1);
		nd1.save_node();

		// create a node
		node nd2 = db_ctx1->create_node(nid2);
		nd2.resize(test_utilities::small_chunk);
		nd2.save_node();

		// delete a node
		db_ctx1->delete_node(extng_nid1);

		// commit to parent
		db_ctx1->commit_db();

		node nd3 = db_ctx2->create_node(nid3);
		nd3.resize(test_utilities::large_chunk);
		nd3.save_node();

		// commit child ctx on parent
		db_ctx1->commit_db(db_ctx2);

		// commit root ctx to disk
		db_root->commit_db();

		// verify commit on root
		shared_ptr<nbt_page> nbt_root = db_root->read_nbt_root();
		nbt_root->lookup(nid1);
		nbt_root->lookup(nid2);

		// verify commit on child
		shared_ptr<nbt_page> nbt_root1 = db_ctx1->read_nbt_root();
		nbt_root1->lookup(nid3);
	}

	{
		shared_db_ptr db_root = open_database(filename);

		// verify commit on root went to disk
		shared_ptr<nbt_page> nbt_root = db_root->read_nbt_root();
		nbt_root->lookup(nid1);
		nbt_root->lookup(nid2);

		try
		{
			nbt_root->lookup(extng_nid1);
			assert(false);
		}
		catch(key_not_found<node_id>) {	}

		// verify commit on child did not go to disk
		try
		{
			nbt_root->lookup(nid3);
			assert(false);
		}
		catch(key_not_found<node_id>) {	}
	}
}

void test_context_commit_subnode(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id sb_nid1 = 0x431;
	node_id sb_nid2 = 0x432;
	node_id extng_nid1 = 0x60D;

	{
		shared_db_ptr db_root = open_database(filename);

		//{
		shared_db_ptr db_ctx1 = db_root->create_context();

		// create a subnode
		node nd = db_ctx1->lookup_node(extng_nid1);
		node sb_nd1 = nd.create_subnode(sb_nid1);
		sb_nd1.resize(test_utilities::large_chunk);
		sb_nd1.save_node();

		node sb_nd2 = nd.create_subnode(sb_nid2);
		sb_nd2.resize(test_utilities::small_chunk);
		sb_nd2.save_node();

		nd.save_node();

		// commit to parent
		db_ctx1->commit_db();
		//}

		// commit root ctx to disk
		db_root->commit_db();
	}

	{
		// verify commit on root went to disk
		shared_db_ptr db_root = open_database(filename);

		node nd = db_root->lookup_node(extng_nid1);
		shared_ptr<subnode_block> sb_blk = nd.get_subnode_block();

		sb_blk->lookup(sb_nid1);
		sb_blk->lookup(sb_nid2);
	}

	{
		shared_db_ptr db_root = open_database(filename);

		{
			shared_db_ptr db_ctx1 = db_root->create_context();

			// delete a subnode
			node nd = db_ctx1->lookup_node(extng_nid1);
			nd.delete_subnode(sb_nid1);
			nd.delete_subnode(sb_nid2);
			nd.save_node();

			// commit to parent
			db_ctx1->commit_db();
		}

		// commit root ctx to disk
		db_root->commit_db();
	}

	{
		// verify commit on root went to disk
		shared_db_ptr db_root = open_database(filename);

		node nd = db_root->lookup_node(extng_nid1);
		shared_ptr<subnode_block> sb_blk = nd.get_subnode_block();

		try
		{
			sb_blk->lookup(sb_nid1);
			assert(false);
		}
		catch(key_not_found<node_id>) {	}

		try
		{
			sb_blk->lookup(sb_nid2);
			assert(false);
		}
		catch(key_not_found<node_id>) {	}
	}
}

void test_context_rollback(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id existing_nid = 0x21;
	node_id nid4 = 0x43;
	node_id nid5 = 0x45;

	{
		shared_db_ptr db_root = open_database(filename);
		shared_db_ptr db_ctx1 = db_root->create_context();
		shared_db_ptr db_ctx2 = db_ctx1->create_context();

		node nd4 = db_root->create_node(nid4);
		nd4.save_node();

		// create node on child ctx which is already present on parent ctx
		node nd44 = db_ctx1->create_node(nid4);
		nd44.save_node();

		// delete a node from child ctx
		db_ctx1->delete_node(existing_nid);

		// verify delete on ctx
		shared_ptr<nbt_page> nbt_root1 = db_ctx1->read_nbt_root();

		try
		{
			nbt_root1->lookup(existing_nid);
			assert(false);
		}
		catch(key_not_found<node_id>) {	}

		// commit to parent should fail and rollback
		try
		{
			db_ctx1->commit_db();
			assert(false);
		}
		catch(node_save_error) { }

		// verify rollback
		nbt_root1 = db_ctx1->read_nbt_root();
		nbt_root1->lookup(existing_nid);

		// continue normal operations
		node nd5 = db_ctx2->create_node(nid5);
		nd5.save_node();

		// commit child ctx on parent
		db_ctx1->commit_db(db_ctx2);
		db_ctx1->commit_db();

		// commit root ctx to disk
		db_root->commit_db();

		// verify commit on root
		shared_ptr<nbt_page> nbt_root = db_root->read_nbt_root();
		nbt_root->lookup(nid4);
		nbt_root->lookup(nid5);

		// verify invalid commit on child failed
		nbt_root1 = db_ctx1->read_nbt_root();

		try
		{
			nbt_root1->lookup(nid4);
			assert(false);
		}
		catch(key_not_found<node_id>) {	}
	}

	{
		shared_db_ptr db_root = open_database(filename);

		// verify commit on root went to disk and failed operation restored back
		shared_ptr<nbt_page> nbt_root = db_root->read_nbt_root();
		nbt_root->lookup(existing_nid);
		nbt_root->lookup(nid4);
		nbt_root->lookup(nid5);
	}
}

void test_context_reftracking(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id ext_nid1 = 0x122;
	node_id ext_nid2 = 0x6B6;
	node_id ext_nid3 = 0x8042;

	block_id ext_blk_id1 = 0x64;
	block_id ext_blk_id2 = 0x24;
	block_id ext_blk_id3 = 0x68;

	{
		shared_db_ptr db_root = open_database(filename);
		shared_db_ptr db_ctx1 = db_root->create_context();
		shared_db_ptr db_ctx2 = db_ctx1->create_context();

		// modify node data blocks
		node nd1 = db_root->lookup_node(ext_nid1);
		nd1.resize(test_utilities::small_chunk);
		nd1.save_node();

		node nd2 = db_root->lookup_node(ext_nid2);
		nd2.resize(test_utilities::small_chunk);
		nd2.save_node();

		node nd3 = db_root->lookup_node(ext_nid3);
		nd3.resize(test_utilities::small_chunk);
		nd3.save_node();

		// commit child ctx on parent
		db_ctx1->commit_db(db_ctx2);

		// commit root ctx to disk
		db_root->commit_db();

		// verify commit on root
		shared_ptr<nbt_page> nbt_root = db_root->read_nbt_root();
		nbt_root->lookup(ext_nid1);
		nbt_root->lookup(ext_nid2);

		// verify commit on child
		shared_ptr<nbt_page> nbt_root1 = db_ctx1->read_nbt_root();
		nbt_root1->lookup(ext_nid3);

		// verify previous blocks not yet freed from disk since child contexts still valid in memory
		shared_ptr<bbt_page> bbt_root = db_root->read_bbt_root();
		bbt_root->lookup(ext_blk_id1);
		bbt_root->lookup(ext_blk_id2);
		bbt_root->lookup(ext_blk_id3);

		// commit child ctx to parent
		db_ctx1->commit_db();

		// release child contexts from memory
		db_ctx1.reset();
		db_ctx2.reset();

		// commit root db
		db_root->commit_db();

		// verify now previous blocks actually removed from disk
		bbt_root = db_root->read_bbt_root();

		try
		{
			bbt_root->lookup(ext_blk_id1);
			assert(false);
		}
		catch(key_not_found<block_id>) { }

		try
		{
			bbt_root->lookup(ext_blk_id2);
			assert(false);
		}
		catch(key_not_found<block_id>) { }

		try
		{
			bbt_root->lookup(ext_blk_id3);
			assert(false);
		}
		catch(key_not_found<block_id>) { }
	}
}

void test_db_context()
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
		test_context_create(tmp_large_file);

		test_context_commit_node(tmp_large_file);

		test_context_commit_subnode(tmp_large_file);

		test_context_rollback(tmp_large_file);

		test_context_reftracking(tmp_large_file);

		
		// only valid to call on test_ansi.pst
		test_context_create(tmp_small_file);

		test_context_commit_node(tmp_small_file);

		test_context_commit_subnode(tmp_small_file);

		test_context_rollback(tmp_small_file);

		test_context_reftracking(tmp_small_file);

		
		test_utilities::delete_file(tmp_large_file);
		test_utilities::delete_file(tmp_small_file);
	}
	else
	{
		throw runtime_error("error creating temp files");
	}
}