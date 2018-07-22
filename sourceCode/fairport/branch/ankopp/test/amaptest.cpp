#include <iostream>
#include <pstsdk/ndb.h>
#include <pstsdk/disk.h>
#include <pstsdk/util.h>
#include "testutils.h"


void test_amap_unicode_read(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	ulonglong address = 0;
	shared_db_ptr db = open_database(filename);

	shared_ptr<allocation_map> amap = db->get_allocation_map();

	bool bRet = amap->is_allocated(test_utilities::allocated_address);
	assert(bRet);

	bRet = amap->is_allocated(test_utilities::free_address);
	assert(!bRet);

	try
	{
		// check location out of file boundary
		bRet = amap->is_allocated(271361);
		assert(!bRet);
	}
	catch(unexpected_page)
	{
	}

	// allocate small chunk of memory
	amap->begin_transaction();
	address = amap->allocate(test_utilities::small_chunk);
	assert(amap->is_allocated(address, test_utilities::small_chunk));
	amap->commit_transaction();

	// test abort transaction
	amap->begin_transaction();
	amap->free_allocation(address, test_utilities::small_chunk);
	assert(!amap->is_allocated(address, test_utilities::small_chunk));
	amap->abort_transaction();
	assert(amap->is_allocated(address));

	// test free small chunk of memory
	amap->begin_transaction();
	amap->free_allocation(address, test_utilities::small_chunk);
	assert(!amap->is_allocated(address, test_utilities::small_chunk));
	amap->commit_transaction();
	amap->begin_transaction();
	amap->abort_transaction();
	assert(!amap->is_allocated(address, test_utilities::small_chunk));

	try
	{
		// free un-allocated memory
		amap->begin_transaction();
		amap->free_allocation(271361, test_utilities::small_chunk);
		amap->commit_transaction();
	}
	catch(unexpected_page)
	{
	}


	// allocate large chunk of memory
	amap->begin_transaction();
	address = amap->allocate(test_utilities::very_large_chunk);
	assert(amap->is_allocated(address));
	amap->commit_transaction();

	// free large chunk of memory
	// test abort transaction
	amap->begin_transaction();
	amap->free_allocation(address, test_utilities::very_large_chunk);
	assert(!amap->is_allocated(address));
	amap->abort_transaction();
	assert(amap->is_allocated(address, test_utilities::very_large_chunk));

	try
	{
		// allocate memory out of amap page span
		amap->begin_transaction();
		amap->allocate(test_utilities::out_of_bound_chunk);
		assert(address == 0);
	}
	catch(invalid_argument)
	{
	}
}

void test_amap_ansi_read(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	ulonglong address = 0;
	shared_db_ptr db = open_database(filename);

	shared_ptr<allocation_map> amap = db->get_allocation_map();

	bool bRet = amap->is_allocated(test_utilities::allocated_address);
	assert(bRet);

	bRet = amap->is_allocated(test_utilities::free_address);
	assert(!bRet);

	try
	{
		// check location out of file boundary
		bRet = amap->is_allocated(271361);
		assert(!bRet);
	}
	catch(unexpected_page)
	{
	}

	// allocate small chunk of memory
	amap->begin_transaction();
	address = amap->allocate(test_utilities::small_chunk);
	assert(amap->is_allocated(address, test_utilities::small_chunk));
	amap->commit_transaction();

	// test abort transaction
	amap->begin_transaction();
	amap->free_allocation(address, test_utilities::small_chunk);
	assert(!amap->is_allocated(address, test_utilities::small_chunk));
	amap->abort_transaction();
	assert(amap->is_allocated(address));

	// test free small chunk of memory
	amap->begin_transaction();
	amap->free_allocation(address, test_utilities::small_chunk);
	assert(!amap->is_allocated(address, test_utilities::small_chunk));
	amap->commit_transaction();
	amap->begin_transaction();
	amap->abort_transaction();
	assert(!amap->is_allocated(address, test_utilities::small_chunk));

	try
	{
		// free un-allocated memory
		amap->begin_transaction();
		amap->free_allocation(271361, test_utilities::small_chunk);
		amap->commit_transaction();
	}
	catch(unexpected_page)
	{
	}


	// allocate large chunk of memory
	amap->begin_transaction();
	address = amap->allocate(test_utilities::very_large_chunk);
	assert(amap->is_allocated(address));
	amap->commit_transaction();

	// free large chunk of memory
	// test abort transaction
	amap->begin_transaction();
	amap->free_allocation(address, test_utilities::very_large_chunk);
	assert(!amap->is_allocated(address));
	amap->abort_transaction();
	assert(amap->is_allocated(address, test_utilities::very_large_chunk));

	try
	{
		// allocate memory out of amap page span
		amap->begin_transaction();
		amap->allocate(test_utilities::out_of_bound_chunk);
		assert(address == 0);
	}
	catch(invalid_argument)
	{
	}
}

void test_amap_unicode_write(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	ulonglong address1 = 0;
	ulonglong address2 = 0;

	{
		// Allocate memory, commit and flush 1
		shared_db_ptr db1 = open_database(filename);
		shared_ptr<allocation_map> amap1 = db1->get_allocation_map();
		vector<pstsdk::byte> dummy_data(disk::page_size, 0xFF);

		amap1->begin_transaction();
		address1 = amap1->allocate(test_utilities::small_chunk);
		assert(amap1->is_allocated(address1, test_utilities::small_chunk));

		amap1->commit_transaction();
	}

	{
		// Verify 1
		shared_db_ptr db2 = open_database(filename);
		shared_ptr<allocation_map> amap2 = db2->get_allocation_map();
		assert(amap2->is_allocated(address1));
	}

	vector<ulonglong> alloc_results(0);

	{
		// Allocate memory, commit and flush 2
		shared_db_ptr db1 = open_database(filename);
		shared_ptr<allocation_map> amap1 = db1->get_allocation_map();
		vector<pstsdk::byte> dummy_data(disk::page_size, 0xFF);

		amap1->begin_transaction();

		for(int i = 0; i < test_utilities::max_new_allocations; i++)
		{
			alloc_results.push_back(amap1->allocate(test_utilities::very_large_chunk));
		}

		for(int i = 0; i < test_utilities::max_new_allocations; i++)
		{
			assert(amap1->is_allocated(alloc_results[i], test_utilities::very_large_chunk));
		}

		amap1->commit_transaction();
	}

	{
		// Verify 2
		shared_db_ptr db2 = open_database(filename);
		shared_ptr<allocation_map> amap2 = db2->get_allocation_map();

		for(size_t i = 0; i < alloc_results.size(); i++)
		{
			assert(amap2->is_allocated(alloc_results[i], test_utilities::very_large_chunk));
		}
	}
}

void test_amap_ansi_write(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	ulonglong address1 = 0;
	ulonglong address2 = 0;

	{
		// Allocate memory, commit and flush 1
		shared_db_ptr db1 = open_database(filename);
		shared_ptr<allocation_map> amap1 = db1->get_allocation_map();
		vector<pstsdk::byte> dummy_data(disk::page_size, 0xFF);

		amap1->begin_transaction();
		address1 = amap1->allocate(test_utilities::small_chunk);
		assert(amap1->is_allocated(address1, test_utilities::small_chunk));

		amap1->commit_transaction();
	}

	{
		// Verify 1
		shared_db_ptr db2 = open_database(filename);
		shared_ptr<allocation_map> amap2 = db2->get_allocation_map();
		assert(amap2->is_allocated(address1));
	}

	vector<ulonglong> alloc_results(0);

	{
		// Allocate memory, commit and flush 2
		shared_db_ptr db1 = open_database(filename);
		shared_ptr<allocation_map> amap1 = db1->get_allocation_map();
		vector<pstsdk::byte> dummy_data(disk::page_size, 0xFF);

		amap1->begin_transaction();

		for(int i = 0; i < test_utilities::max_new_allocations; i++)
		{
			alloc_results.push_back(amap1->allocate(test_utilities::very_large_chunk));
		}

		for(int i = 0; i < test_utilities::max_new_allocations; i++)
		{
			assert(amap1->is_allocated(alloc_results[i], test_utilities::very_large_chunk));
		}

		amap1->commit_transaction();
	}

	{
		// Verify 2
		shared_db_ptr db2 = open_database(filename);
		shared_ptr<allocation_map> amap2 = db2->get_allocation_map();

		for(size_t i = 0; i < alloc_results.size(); i++)
		{
			assert(amap2->is_allocated(alloc_results[i], test_utilities::very_large_chunk));
		}
	}
}

void validate_dlist_unicode(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	shared_db_ptr db = open_database(filename);
	shared_ptr<dlist_page> dl_page = db->read_dlist_page();

	vector<ulong> entries;
	dl_page->get_entries(entries);

	// check if dlist is getting built up
	assert(entries.size() > 0);

	// verify entries are sorted on descending free slots count
	for(size_t i = 0; i < entries.size() -1; i++)
	{
		assert(disk::dlist_get_slots(entries[i]) >= disk::dlist_get_slots(entries[i+1]));
	}

	// check if current page was set to a valid page number
	assert(dl_page->get_current_page() <= test_utilities::max_new_allocations);
}

void validate_dlist_ansi(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	shared_db_ptr db = open_database(filename);
	shared_ptr<dlist_page> dl_page = db->read_dlist_page();

	vector<ulong> entries;
	dl_page->get_entries(entries);

	// check if dlist is getting built up
	assert(entries.size() > 0);

	// verify entries are sorted on descending free slots count
	for(size_t i = 0; i < entries.size() -1; i++)
	{
		assert(disk::dlist_get_slots(entries[i]) >= disk::dlist_get_slots(entries[i+1]));
	}

	// check if current page was set to a valid page number
	assert(dl_page->get_current_page() <= test_utilities::max_new_allocations);
}

void test_amap_rebuild_unicode(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	shared_db_ptr db = open_database(filename);

	// invalidate amap
	header_values_amap header_values;
	db->read_header_values_amap(header_values);
	header_values.fAMapValid = disk::invalid_amap;
	db->read_header_values_amap(header_values);

	shared_ptr<allocation_map> amap = db->get_allocation_map();

	// rebuild amap
	amap->begin_transaction();
	amap->commit_transaction();

	// verify consistency
	bool bRet = amap->is_allocated(test_utilities::allocated_address);
	assert(bRet);

	bRet = amap->is_allocated(test_utilities::free_address);
	assert(!bRet);

	// verify normal operation
	amap->begin_transaction();
	amap->allocate(disk::page_size);
	amap->commit_transaction();
}

void test_amap_rebuild_ansi(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	shared_db_ptr db = open_database(filename);

	// invalidate amap
	header_values_amap header_values;
	db->read_header_values_amap(header_values);
	header_values.fAMapValid = disk::invalid_amap;
	db->read_header_values_amap(header_values);

	shared_ptr<allocation_map> amap = db->get_allocation_map();

	// rebuild amap
	amap->begin_transaction();
	amap->commit_transaction();

	// verify consistency
	bool bRet = amap->is_allocated(test_utilities::allocated_address);
	assert(bRet);

	bRet = amap->is_allocated(test_utilities::free_address);
	assert(!bRet);

	// verify normal operation
	amap->begin_transaction();
	amap->allocate(disk::page_size);
	amap->commit_transaction();
}

void test_amap()
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
		test_amap_unicode_read(tmp_large_file);

		test_amap_unicode_write(tmp_large_file);

		validate_dlist_unicode(tmp_large_file);

		test_amap_rebuild_unicode(tmp_large_file);


		// only valid to call on test_ansi.pst
		test_amap_ansi_read(tmp_small_file);

		test_amap_ansi_write(tmp_small_file);

		validate_dlist_ansi(tmp_small_file);

		test_amap_rebuild_ansi(tmp_small_file);


		test_utilities::delete_file(tmp_large_file);
		test_utilities::delete_file(tmp_small_file);
	}
	else
	{
		throw runtime_error("error creating temp files");
	}
}