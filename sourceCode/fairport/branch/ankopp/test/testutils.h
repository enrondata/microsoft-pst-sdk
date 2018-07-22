#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <vector>

#if defined (_WINDOWS_PLATFORM)
#include <Windows.h>
#include <process.h>
#elif !defined (_SINGLE_THREADED)
#include <boost/thread/thread.hpp>
#endif

namespace test_utilities
{
	const size_t small_chunk = 1024;
	const size_t large_chunk = 10240;
	const size_t very_large_chunk = 252928;
	const size_t out_of_bound_chunk = 253441;
	const int max_new_allocations = 20;

	const pstsdk::ulonglong allocated_address = 17500;
	const pstsdk::ulonglong free_address = 98304;


	static bool copy_file(std::wstring src_file, std::wstring dest_file);
	static void delete_file(std::wstring file);

	typedef struct
	{
		pstsdk::shared_db_ptr ctx;
		pstsdk::node_id parent_nid;
		std::vector<pstsdk::node_id> nids;
	} thread_params;

	static void create_nodes(void* params);
	static void create_subnodes(void* params);
	static void modify_nodes(void* params);
	static void delete_nodes(void* params);
	static void delete_subnodes(void* params);

	std::vector<byte> string_to_bytes(std::string str);

	class pst_thread
	{
	public:
		pst_thread() { }
		~pst_thread() { }
		void start_create_nodes(pstsdk::shared_db_ptr ctx, std::vector<pstsdk::node_id> nids);
		void start_create_subnodes(pstsdk::shared_db_ptr ctx, pstsdk::node_id parent_nid, std::vector<pstsdk::node_id> sb_nids);
		void start_modify_nodes(pstsdk::shared_db_ptr ctx, std::vector<pstsdk::node_id> nids);
		void start_delete_nodes(pstsdk::shared_db_ptr ctx, std::vector<pstsdk::node_id> nids);
		void start_delete_subnodes(pstsdk::shared_db_ptr ctx, pstsdk::node_id parent_nid, std::vector<pstsdk::node_id> sb_nids);
		void wait_for_completion();

	private:
		thread_params m_thrdparams;

#if defined (_WINDOWS_PLATFORM)
		HANDLE m_hw_thread;
#elif !defined (_SINGLE_THREADED)
		boost::shared_ptr<boost::thread> m_bst_thread;
#endif

	};
}

inline void test_utilities::pst_thread::start_create_nodes(pstsdk::shared_db_ptr ctx, std::vector<pstsdk::node_id> nids)
{
	m_thrdparams.ctx = ctx;
	m_thrdparams.parent_nid = 0;
	m_thrdparams.nids = nids;

#if defined (_WINDOWS_PLATFORM)
	if(INVALID_HANDLE_VALUE != m_hw_thread)
		m_hw_thread = (HANDLE)_beginthread((void(*)(void*))&create_nodes, 0, &m_thrdparams);
#elif !defined (_SINGLE_THREADED)
	if(!m_bst_thread)
		m_bst_thread = boost::shared_ptr<boost::thread>(new boost::thread(&create_nodes, &m_thrdparams));
#else
	create_nodes(&m_thrdparams);
#endif

}

inline void test_utilities::pst_thread::start_create_subnodes(pstsdk::shared_db_ptr ctx, pstsdk::node_id parent_nid, std::vector<pstsdk::node_id> sb_nids)
{
	m_thrdparams.ctx = ctx;
	m_thrdparams.parent_nid = parent_nid;
	m_thrdparams.nids = sb_nids;

#if defined (_WINDOWS_PLATFORM)
	if(INVALID_HANDLE_VALUE != m_hw_thread)
		m_hw_thread = (HANDLE)_beginthread((void(*)(void*))&create_subnodes, 0, &m_thrdparams);
#elif !defined (_SINGLE_THREADED)
	if(!m_bst_thread)
		m_bst_thread = boost::shared_ptr<boost::thread>(new boost::thread(&create_subnodes, &m_thrdparams));
#else
	create_subnodes(&m_thrdparams);
#endif

}

inline void test_utilities::pst_thread::start_modify_nodes(pstsdk::shared_db_ptr ctx, std::vector<pstsdk::node_id> nids)
{
	m_thrdparams.ctx = ctx;
	m_thrdparams.parent_nid = 0;
	m_thrdparams.nids = nids;

#if defined (_WINDOWS_PLATFORM)
	if(INVALID_HANDLE_VALUE != m_hw_thread)
		m_hw_thread = (HANDLE)_beginthread((void(*)(void*))&modify_nodes, 0, (void*)&m_thrdparams);
#elif !defined (_SINGLE_THREADED)
	if(!m_bst_thread)
		m_bst_thread = boost::shared_ptr<boost::thread>(new boost::thread(&modify_nodes, &m_thrdparams));
#else
	modify_nodes(&m_thrdparams);
#endif

}

inline void test_utilities::pst_thread::start_delete_nodes(pstsdk::shared_db_ptr ctx, std::vector<pstsdk::node_id> nids)
{
	m_thrdparams.ctx = ctx;
	m_thrdparams.parent_nid = 0;
	m_thrdparams.nids = nids;

#if defined (_WINDOWS_PLATFORM)
	if(INVALID_HANDLE_VALUE != m_hw_thread)
		m_hw_thread = (HANDLE)_beginthread((void(*)(void*))&delete_nodes, 0, (void*)&m_thrdparams);
#elif !defined (_SINGLE_THREADED)
	if(!m_bst_thread)
		m_bst_thread = boost::shared_ptr<boost::thread>(new boost::thread(&delete_nodes, &m_thrdparams));
#else
	delete_nodes(&m_thrdparams);
#endif

}

inline void test_utilities::pst_thread::start_delete_subnodes(pstsdk::shared_db_ptr ctx, pstsdk::node_id parent_nid, std::vector<pstsdk::node_id> sb_nids)
{
	m_thrdparams.ctx = ctx;
	m_thrdparams.parent_nid = parent_nid;
	m_thrdparams.nids = sb_nids;

#if defined (_WINDOWS_PLATFORM)
	if(INVALID_HANDLE_VALUE != m_hw_thread)
		m_hw_thread = (HANDLE)_beginthread((void(*)(void*))&delete_subnodes, 0, (void*)&m_thrdparams);
#elif !defined (_SINGLE_THREADED)
	if(!m_bst_thread)
		m_bst_thread = boost::shared_ptr<boost::thread>(new boost::thread(&delete_subnodes, &m_thrdparams));
#else
	delete_subnodes(&m_thrdparams);
#endif

}

inline void test_utilities::pst_thread::wait_for_completion()
{
#if defined (_WINDOWS_PLATFORM)
	if(INVALID_HANDLE_VALUE != m_hw_thread)
		WaitForSingleObject(m_hw_thread, INFINITE);
#elif !defined (_SINGLE_THREADED)
	if(m_bst_thread)
		m_bst_thread->join();
#endif
}

inline void test_utilities::create_nodes(void* thrd_params)
{
	test_utilities::thread_params *params = reinterpret_cast<test_utilities::thread_params*>(thrd_params);
	pstsdk::shared_db_ptr ctx = params->ctx;
	std::vector<pstsdk::node_id> nids(params->nids);

	try
	{
		for(size_t ind = 0; ind < nids.size(); ++ind)
		{
			pstsdk::node nd = ctx->create_node(nids[ind]);
			nd.resize(ind % 2 == 0 ? test_utilities::small_chunk : test_utilities::large_chunk);
			nd.save_node();
		}

		ctx->commit_db();
	}
	catch(pstsdk::node_save_error) { }

}

inline static void test_utilities::create_subnodes(void* thrd_params)
{
	test_utilities::thread_params *params = reinterpret_cast<test_utilities::thread_params*>(thrd_params);
	pstsdk::shared_db_ptr ctx = params->ctx;
	pstsdk::node_id parent_nid = params->parent_nid;
	std::vector<pstsdk::node_id> sb_nids(params->nids);

	try
	{
		pstsdk::node nd = ctx->lookup_node(parent_nid);

		for(size_t ind = 0; ind < sb_nids.size(); ++ind)
		{
			pstsdk::node sb_nd = nd.create_subnode(sb_nids[ind]);
			sb_nd.resize(ind % 2 == 0 ? test_utilities::small_chunk : test_utilities::large_chunk);
			nd.save_subnode(sb_nd);
		}

		nd.save_node();
		ctx->commit_db();
	}
	catch(pstsdk::node_save_error) { }
}

inline static void test_utilities::modify_nodes(void* thrd_params)
{
	test_utilities::thread_params *params = reinterpret_cast<test_utilities::thread_params*>(thrd_params);
	pstsdk::shared_db_ptr ctx = params->ctx;
	std::vector<pstsdk::node_id> nids(params->nids);

	try
	{
		for(size_t ind = 0; ind < nids.size(); ++ind)
		{
			pstsdk::node nd = ctx->lookup_node(nids[ind]);
			nd.resize(test_utilities::large_chunk + nd.get_data_block()->get_disk_size());
			nd.save_node();
		}

		ctx->commit_db();
	}
	catch(pstsdk::node_save_error) { }
}

inline static void test_utilities::delete_nodes(void* thrd_params)
{
	test_utilities::thread_params *params = reinterpret_cast<test_utilities::thread_params*>(thrd_params);
	pstsdk::shared_db_ptr ctx = params->ctx;
	std::vector<pstsdk::node_id> nids(params->nids);

	try
	{
		for(size_t ind = 0; ind < nids.size(); ++ind)
			ctx->delete_node(nids[ind]);

		ctx->commit_db();
	}
	catch(pstsdk::key_not_found<pstsdk::block_id>) { }
}

inline static void test_utilities::delete_subnodes(void* thrd_params)
{
	test_utilities::thread_params *params = reinterpret_cast<test_utilities::thread_params*>(thrd_params);
	pstsdk::shared_db_ptr ctx = params->ctx;
	pstsdk::node_id parent_nid = params->parent_nid;
	std::vector<pstsdk::node_id> sb_nids(params->nids);

	try
	{
		pstsdk::node nd = ctx->lookup_node(parent_nid);

		for(size_t ind = 0; ind < sb_nids.size(); ++ind)
		{
			nd.delete_subnode(sb_nids[ind]);
			nd.save_node();
		}

		ctx->commit_db();
	}
	catch(pstsdk::key_not_found<pstsdk::block_id>) { }
}

inline static bool test_utilities::copy_file(std::wstring src_file, std::wstring dest_file)
{
	using namespace std;

	ifstream in(src_file, ios::binary);
	ofstream out(dest_file, ios::binary);

	if(in.fail() || out.fail())
		return false;

	out<<in.rdbuf();

	in.close();
	out.close();

	return true;
}

inline static void test_utilities::delete_file(std::wstring file)
{
	char tmpStr[255] = {0};
	sprintf_s(tmpStr, "%ls", file.c_str());
	remove(tmpStr);
}

inline static std::vector<byte> test_utilities::string_to_bytes(std::string str)
{
	pstsdk::byte *begin = reinterpret_cast<pstsdk::byte*>(&str[0]);
	return std::vector<byte>(begin, begin + str.size()*sizeof(str[0]));
}

#endif