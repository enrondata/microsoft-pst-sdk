#include <iostream>
#include <pstsdk/ndb.h>
#include <pstsdk/disk.h>
#include <pstsdk/util.h>
#include "testutils.h"

const pstsdk::node_id new_nids[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
const pstsdk::node_id exstng_nids[] = { 0x60F, 0x610, 0x62B };
const pstsdk::node_id new_sb_nids[] = { 0x431, 0x432, 0x433, 0x434, 0x435, 0x436 };

void test_mutex_node_create(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	size_t vect_cnt = 3;
	size_t div = (sizeof(new_nids) / sizeof(node_id)) / vect_cnt;

	vector<node_id> nids1(new_nids, new_nids + div);
	vector<node_id> nids2(new_nids + div, new_nids + (2 * div));
	vector<node_id> nids3(new_nids + (2 * div), new_nids + (div * vect_cnt));

	{
		// create db contexts
		shared_db_ptr db_root = open_database(filename);

		{
			shared_db_ptr db_ctx1 = db_root->create_context();
			shared_db_ptr db_ctx2 = db_root->create_context();
			shared_db_ptr db_ctx3 = db_root->create_context();

			// create threads
			test_utilities::pst_thread thread1;
			test_utilities::pst_thread thread2;
			test_utilities::pst_thread thread3;

			// perform distinct create operations in parallel
			thread1.start_create_nodes(db_ctx1, nids1);
			thread2.start_create_nodes(db_ctx2, nids2);
			thread3.start_create_nodes(db_ctx3, nids3);

			thread1.wait_for_completion();
			thread2.wait_for_completion();
			thread3.wait_for_completion();
		}

		db_root->commit_db();
	}

	// verify all create operations commited to db and no corruption occured
	{
		shared_db_ptr db_root = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db_root->read_nbt_root();

		for(size_t ind = 0; ind < nids1.size(); ++ind)
			nbt_root->lookup(nids1[ind]);

		for(size_t ind = 0; ind < nids2.size(); ++ind)
			nbt_root->lookup(nids2[ind]);

		for(size_t ind = 0; ind < nids3.size(); ++ind)
			nbt_root->lookup(nids3[ind]);

		for(int ind = 0; ind < sizeof(exstng_nids) / sizeof(node_id); ++ind)
			nbt_root->lookup(exstng_nids[ind]);
	}
}

void test_mutex_node_modify(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	size_t vect_cnt = 3;
	size_t div = (sizeof(new_nids) / sizeof(node_id)) / vect_cnt;

	vector<node_id> nids1(new_nids, new_nids + div);
	vector<node_id> nids2(new_nids + div, new_nids + (2 * div));
	vector<node_id> nids3(new_nids + (2 * div), new_nids + (div * vect_cnt));

	{
		// create db contexts
		shared_db_ptr db_root = open_database(filename);

		{
			shared_db_ptr db_ctx1 = db_root->create_context();
			shared_db_ptr db_ctx2 = db_root->create_context();
			shared_db_ptr db_ctx3 = db_root->create_context();

			// create threads
			test_utilities::pst_thread thread1;
			test_utilities::pst_thread thread2;
			test_utilities::pst_thread thread3;

			// perform distinct modify operations in parallel
			thread1.start_modify_nodes(db_ctx1, nids1);
			thread2.start_modify_nodes(db_ctx2, nids2);
			thread3.start_modify_nodes(db_ctx3, nids3);

			thread1.wait_for_completion();
			thread2.wait_for_completion();
			thread3.wait_for_completion();
		}

		db_root->commit_db();
	}

	// verify no corruption occured
	{
		shared_db_ptr db_root = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db_root->read_nbt_root();

		for(size_t ind = 0; ind < nids1.size(); ++ind)
			nbt_root->lookup(nids1[ind]);

		for(size_t ind = 0; ind < nids2.size(); ++ind)
			nbt_root->lookup(nids2[ind]);

		for(size_t ind = 0; ind < nids3.size(); ++ind)
			nbt_root->lookup(nids3[ind]);

		for(int ind = 0; ind < sizeof(exstng_nids) / sizeof(node_id); ++ind)
			nbt_root->lookup(exstng_nids[ind]);
	}
}

void test_mutex_node_delete(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	size_t vect_cnt = 3;
	size_t div = (sizeof(new_nids) / sizeof(node_id)) / vect_cnt;

	vector<node_id> nids1(new_nids, new_nids + div);
	vector<node_id> nids2(new_nids + div, new_nids + (2 * div));
	vector<node_id> nids3(new_nids + (2 * div), new_nids + (div * vect_cnt));

	{
		// create db contexts
		shared_db_ptr db_root = open_database(filename);

		{
			shared_db_ptr db_ctx1 = db_root->create_context();
			shared_db_ptr db_ctx2 = db_root->create_context();
			shared_db_ptr db_ctx3 = db_root->create_context();

			// create threads
			test_utilities::pst_thread thread1;
			test_utilities::pst_thread thread2;
			test_utilities::pst_thread thread3;

			// perform distinct delete operations in parallel
			thread1.start_delete_nodes(db_ctx1, nids1);
			thread2.start_delete_nodes(db_ctx2, nids2);
			thread3.start_delete_nodes(db_ctx3, nids3);

			thread1.wait_for_completion();
			thread2.wait_for_completion();
			thread3.wait_for_completion();
		}

		db_root->commit_db();
	}

	// verify all delete operations commited to db and no corruption occured
	{
		shared_db_ptr db_root = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db_root->read_nbt_root();

		for(size_t ind = 0; ind < nids1.size(); ++ind)
		{
			try
			{
				nbt_root->lookup(nids1[ind]);
				assert(false);
			}
			catch(key_not_found<node_id>) {  }
		}

		for(size_t ind = 0; ind < nids2.size(); ++ind)
		{
			try
			{
				nbt_root->lookup(nids2[ind]);
				assert(false);
			}
			catch(key_not_found<node_id>) {  }
		}

		for(size_t ind = 0; ind < nids3.size(); ++ind)
		{
			try
			{
				nbt_root->lookup(nids3[ind]);
				assert(false);
			}
			catch(key_not_found<node_id>) {  }
		}

		for(int ind = 0; ind < sizeof(exstng_nids) / sizeof(node_id); ++ind)
			nbt_root->lookup(exstng_nids[ind]);
	}
}

void test_mutex_subnode_create(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	size_t vect_cnt = 3;
	size_t div = (sizeof(new_nids) / sizeof(node_id)) / vect_cnt;

	vector<node_id> sb_nids1(new_nids, new_nids + div);
	vector<node_id> sb_nids2(new_nids + div, new_nids + (2 * div));
	vector<node_id> sb_nids3(new_nids + (2 * div), new_nids + (div * vect_cnt));

	{
		// create db contexts
		shared_db_ptr db_root = open_database(filename);

		{
			shared_db_ptr db_ctx1 = db_root->create_context();
			shared_db_ptr db_ctx2 = db_root->create_context();
			shared_db_ptr db_ctx3 = db_root->create_context();

			// create threads
			test_utilities::pst_thread thread1;
			test_utilities::pst_thread thread2;
			test_utilities::pst_thread thread3;

			// perform distinct subnode create operations in parallel
			thread1.start_create_subnodes(db_ctx1, exstng_nids[0], sb_nids1);
			thread2.start_create_subnodes(db_ctx2, exstng_nids[1], sb_nids2);
			thread3.start_create_subnodes(db_ctx3, exstng_nids[2], sb_nids3);

			thread1.wait_for_completion();
			thread2.wait_for_completion();
			thread3.wait_for_completion();
		}

		db_root->commit_db();
	}

	// verify all create operations commited to db and no corruption occured
	{
		shared_db_ptr db_root = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db_root->read_nbt_root();

		node nd1 = db_root->lookup_node(exstng_nids[0]);
		shared_ptr<subnode_block> sb_blk1 = nd1.get_subnode_block();

		for(size_t ind = 0; ind < sb_nids1.size(); ++ind)
			sb_blk1->lookup(sb_nids1[ind]);

		node nd2 = db_root->lookup_node(exstng_nids[1]);
		shared_ptr<subnode_block> sb_blk2 = nd2.get_subnode_block();

		for(size_t ind = 0; ind < sb_nids2.size(); ++ind)
			sb_blk2->lookup(sb_nids2[ind]);

		node nd3 = db_root->lookup_node(exstng_nids[2]);
		shared_ptr<subnode_block> sb_blk3 = nd3.get_subnode_block();

		for(size_t ind = 0; ind < sb_nids3.size(); ++ind)
			sb_blk3->lookup(sb_nids3[ind]);

		for(int ind = 0; ind < sizeof(exstng_nids) / sizeof(node_id); ++ind)
			nbt_root->lookup(exstng_nids[ind]);
	}
}

void test_mutex_subnode_delete(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	size_t vect_cnt = 3;
	size_t div = (sizeof(new_nids) / sizeof(node_id)) / vect_cnt;

	vector<node_id> sb_nids1(new_nids, new_nids + div);
	vector<node_id> sb_nids2(new_nids + div, new_nids + (2 * div));
	vector<node_id> sb_nids3(new_nids + (2 * div), new_nids + (div * vect_cnt));

	{
		// create db contexts
		shared_db_ptr db_root = open_database(filename);

		//{
		shared_db_ptr db_ctx1 = db_root->create_context();
		shared_db_ptr db_ctx2 = db_root->create_context();
		shared_db_ptr db_ctx3 = db_root->create_context();

		// create threads
		test_utilities::pst_thread thread1;
		test_utilities::pst_thread thread2;
		test_utilities::pst_thread thread3;

		// perform distinct subnode delete operations in parallel
		thread1.start_delete_subnodes(db_ctx1, exstng_nids[0], sb_nids1);
		thread2.start_delete_subnodes(db_ctx2, exstng_nids[1], sb_nids2);
		thread3.start_delete_subnodes(db_ctx3, exstng_nids[2], sb_nids3);

		thread1.wait_for_completion();
		thread2.wait_for_completion();
		thread3.wait_for_completion();
		//}

		db_root->commit_db();
	}

	// verify all deleted operations commited to db and no corruption occured
	{
		shared_db_ptr db_root = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db_root->read_nbt_root();

		node nd1 = db_root->lookup_node(exstng_nids[0]);
		shared_ptr<subnode_block> sb_blk1 = nd1.get_subnode_block();

		for(size_t ind = 0; ind < sb_nids1.size(); ++ind)
		{
			try
			{
				sb_blk1->lookup(sb_nids1[ind]);
				assert(false);
			}
			catch(key_not_found<node_id>) {  }
		}

		node nd2 = db_root->lookup_node(exstng_nids[1]);
		shared_ptr<subnode_block> sb_blk2 = nd2.get_subnode_block();

		for(size_t ind = 0; ind < sb_nids2.size(); ++ind)
		{
			try
			{
				sb_blk2->lookup(sb_nids2[ind]);
				assert(false);
			}
			catch(key_not_found<node_id>) {  }
		}

		node nd3 = db_root->lookup_node(exstng_nids[2]);
		shared_ptr<subnode_block> sb_blk3 = nd3.get_subnode_block();

		for(size_t ind = 0; ind < sb_nids3.size(); ++ind)
		{
			try
			{
				sb_blk3->lookup(sb_nids3[ind]);
				assert(false);
			}
			catch(key_not_found<node_id>) {  }
		}

		for(int ind = 0; ind < sizeof(exstng_nids) / sizeof(node_id); ++ind)
			nbt_root->lookup(exstng_nids[ind]);
	}
}

void test_integrity(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	size_t vect_cnt = 2;
	size_t div = (sizeof(new_nids) / sizeof(node_id)) / vect_cnt;

	vector<node_id> nids1(new_nids, new_nids + div);
	vector<node_id> nids2(new_nids + div, new_nids + (2 * div));

	{
		// create db contexts
		shared_db_ptr db_root = open_database(filename);

		{
			shared_db_ptr db_ctx1 = db_root->create_context();
			shared_db_ptr db_ctx2 = db_root->create_context();

			// create threads
			test_utilities::pst_thread thread1;
			test_utilities::pst_thread thread2;

			// perform repetive operations in parallel
			thread1.start_create_nodes(db_ctx1, nids1);
			thread2.start_create_nodes(db_ctx2, nids2);

			thread1.wait_for_completion();
			thread2.wait_for_completion();
		}

		db_root->commit_db();
	}

	// verify new nodes created only once.
	{
		shared_db_ptr db_root = open_database(filename);
		shared_ptr<nbt_page> nbt_root = db_root->read_nbt_root();

		for(size_t ind = 0; ind < nids1.size(); ++ind)
			nbt_root->lookup(nids1[ind]);

		for(size_t ind = 0; ind < nids2.size(); ++ind)
			nbt_root->lookup(nids2[ind]);

		db_root->delete_node(nids1[1]);
		db_root->delete_node(nids2[1]);

		db_root->commit_db();

		nbt_root = db_root->read_nbt_root();

		try
		{
			nbt_root->lookup(nids1[1]);
			assert(false);
		}
		catch(key_not_found<node_id>) {  }

		try
		{
			nbt_root->lookup(nids2[1]);
			assert(false);
		}
		catch(key_not_found<node_id>) {  }
	}
}

void test_thread_safety()
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	std::wstring large_file = L"test_unicode.pst";
	std::wstring small_file = L"test_ansi.pst";

	std::wstring tmp_large_file = L"tmp_test_unicode.pst";
	std::wstring tmp_small_file = L"tmp_test_ansi.pst";

#if defined (_SINGLE_THREADED)
	cout << "_SINGLE_THREADED defined. Attempting to run tests in single threaded mode."<<endl;
#endif

	if(test_utilities::copy_file(large_file, tmp_large_file) && test_utilities::copy_file(small_file, tmp_small_file))
	{
		// only valid to call on test_unicode.pst
		test_mutex_node_create(tmp_large_file);

		test_mutex_node_modify(tmp_large_file);

		test_mutex_node_delete(tmp_large_file);

		test_mutex_subnode_create(tmp_large_file);

		test_mutex_subnode_delete(tmp_large_file);

		test_integrity(tmp_large_file);


		// only valid to call on test_ansi.pst
		test_mutex_node_create(tmp_small_file);

		test_mutex_node_modify(tmp_small_file);

		test_mutex_node_delete(tmp_small_file);

		test_mutex_subnode_create(tmp_small_file);

		test_mutex_subnode_delete(tmp_small_file);

		test_integrity(tmp_small_file);


		test_utilities::delete_file(tmp_large_file);
		test_utilities::delete_file(tmp_small_file);
	}
	else
	{
		throw runtime_error("error creating temp files");
	}
}

