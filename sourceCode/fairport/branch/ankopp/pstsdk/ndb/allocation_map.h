#ifndef ALLOCATION_MAP_H
#define ALLOCATION_MAP_H

#include <map>
#include <vector>
#include "pstsdk/disk/disk.h"
#include "pstsdk/ndb/database_iface.h"



namespace pstsdk
{
	typedef std::map<pstsdk::ulonglong, size_t> sorted_allocation_list;

	//!< \brief An abstraction for allocation map
	//!
	//! Reads amap pages from disk and builds up in-memory copies for manipulation.
	//! Supports transaction semantics for allocation and de-allocation of memory space.
	//! Flushes modified in-memory amap pages back to disk. Grows pst file if needed
	//! to create additional space.
	//! 
	//! \sa [MS-PST] 1.3.2.4
	//! \ingroup ndb
	class allocation_map
	{
	public:

		//! \brief Construct an allocation_map
		//! \param[in] db The database context
		allocation_map(const shared_db_ptr& db):m_db(db), m_amap_pages(0), m_pmap_pages(0), m_fmap_pages(0), m_fpmap_pages(0) { init_amap_data(); }

		//! \brief Begins an amap transaction
		void begin_transaction();

		//! \brief Attempt to allocate space through some amap page
		//! \param[in] size The size of block in bytes to allocate out of amap
		//! \param[in] align Flag depicting if allocation address is to be 512 byte aligned
		//! \returns The address where space is allocated
		ulonglong allocate(size_t size, bool align = false);

		//! \brief Free allocation from appropriate amap page
		//! \throws unexpected_page If loaction is past end of file
		//! \param[in] location Address at which space is to be freed
		//! \param[in] size The amount of bytes to free
		void free_allocation(ulonglong location, size_t size);

		//! \brief Check if location is already allocated
		//! \throws unexpected_page If location is past end of file
		//! \param[in] location Address to check if free
		//! \param[in] size Number of bytes to check if free
		//! \returns Whether space at given location is free
		bool is_allocated(ulonglong location, size_t size=1);

		//! \brief Update amap pages in memory as per queued allocation/deallocation requests
		void commit_transaction();

		//! \brief Aborts an amap transaction
		void abort_transaction();

		//! \brief Accessor to amap specific header values
		//! \returns The header_values_amap structure with values in header related to amap
		//! \sa [MS-PST] 2.2.2.5
		header_values_amap get_header_values() { return m_header_values; }


	protected:
		//! \brief Flush amap pages in memory to disk
		void flush(bool validate);

		//! \brief Read all amap pages from disk
		void read_all_amap_pages();

		//! \brief Read pages from disk on demand
		void read_specific_page(size_t page_index);

		//! \brief Get actual index at which required amap page is stored in m_amap_pages
		size_t get_actual_page_index(size_t mapped_index);

		//! \brief Rebuild amap pages by scanning through PST file
		void rebuild_amap();

		//! \brief Traverse and mark corresponding locations occupied by all pages in NBT and BBT as allocated
		template <typename K, typename V>
		void rebuild_amap_process_bt_pages(pstsdk::bt_page<K, V>* page, sorted_allocation_list& alloc_list);

		//! \brief Iterate through all blocks and marks corresponding locations as allocated
		void rebuild_amap_process_blocks(sorted_allocation_list& alloc_list);

		//! \brief Build new amap pages and marks them appropriately based on existing pages and blocks
		void pstsdk::allocation_map::rebuild_amap_process_amap_pages(sorted_allocation_list& alloc_list);

		//! \brief Build empty PMap/FMap/FPMap with valid checksums for maintaining backward compatibility
		void pstsdk::allocation_map::rebuild_amap_process_legacy_map_pages();

		//! \brief Free allocation from appropriate amap page in memory
		void commit_free_allocation(ulonglong location, size_t size);

		//! \brief Attempt to allocate space through some amap page in memory
		ulonglong commit_allocate(size_t size, bool align = false);

		//! \brief Get value of fAMapValid in header
		disk::amap_validity get_amap_validity() { return m_header_values.fAMapValid; }

		//! \brief Set value of fAMapValid to 0x0 in header
		void invalidate_amap();

		//! \brief Set value of fAMapValid to 0x2 in header
		void validate_amap();

		//! \brief Update value of cbAMapFree in header
		void update_amap_free(size_t size, bool is_freed);

		//! \brief Update value of ibAMapLast, ibFileEof in header
		void incr_amap_last();

		//! \brief Get address of last amap page
		ulonglong get_last_amap() { return m_header_values.ibAMapLast; }

		//! \brief Build up dlist if dlist page on disk is not present or is invalid
		void initialize_dlist();

		//! \brief Helper for restoring invalid amap
		void allocate_specific(ulonglong location, size_t size);

		//! \brief Grow pst file by disk::amap_page_interval if allocations have caused addition of a new amap page
		void grow_file();

		//! \brief Initialize amap data
		void init_amap_data();

		//! \brief Update legacy structures wherever applicable
		void maintain_legacy_intigrity(ulonglong location);


	protected:
		static const size_t page_cache_thresh = 1000;
		pstsdk::weak_db_ptr m_db;
		shared_db_ptr get_db_ptr() const { return shared_db_ptr(m_db); }
		std::vector<std::tr1::shared_ptr<pstsdk::amap_page>> m_amap_pages;		// collection of amap pages read from disk
		std::map<size_t, size_t> m_read_pages_map;								// map of pages actually read from disk. (disk index Vs m_amap_pages index)
		size_t m_total_amap_pages;												// Total amap pages in PST file
		header_values_amap m_header_values;										// set of amap specific header values
		std::tr1::shared_ptr<pstsdk::dlist_page> m_dlist_page;					// dlist page for optimized allocation
		std::vector<std::tr1::shared_ptr<pstsdk::pmap_page>> m_pmap_pages;		// collection of pmap pages
		std::vector<std::tr1::shared_ptr<pstsdk::fmap_page>> m_fmap_pages;		// collection of fmap pages
		std::vector<std::tr1::shared_ptr<pstsdk::fpmap_page>> m_fpmap_pages;	// collection of fpmap pages
	};

} // end namespace

inline void pstsdk::allocation_map::init_amap_data()
{
	get_db_ptr()->read_header_values_amap(m_header_values);
	m_total_amap_pages = (size_t)(get_last_amap() / disk::amap_page_interval) + 1;
	initialize_dlist();
}

inline void pstsdk::allocation_map::read_all_amap_pages()
{
	for(size_t ind = 0; ind < m_total_amap_pages; ind++)
		read_specific_page(ind);
}

inline void pstsdk::allocation_map::read_specific_page(size_t actual_index)
{
	ulonglong page_location = pstsdk::disk::first_amap_page_location + (actual_index * pstsdk::disk::amap_page_interval);
	ulonglong last_page_location = get_last_amap();
	pstsdk::page_info pi = {page_location, page_location};

	if(page_location <= last_page_location)
	{
		m_amap_pages.push_back(get_db_ptr()->read_amap_page(pi));
		m_read_pages_map[actual_index] = m_amap_pages.size() - 1;
	}
}

inline size_t pstsdk::allocation_map::get_actual_page_index(size_t mapped_index)
{
	std::map<size_t, size_t>::iterator itr = m_read_pages_map.find(mapped_index);

	if(itr == m_read_pages_map.end())
		read_specific_page(mapped_index);

	return m_read_pages_map[mapped_index];
}

inline void pstsdk::allocation_map::rebuild_amap()
{
	m_header_values.cbAMapFree = 0;
	m_header_values.cbPMapFree = 0;
	m_amap_pages.clear();
	m_pmap_pages.clear();
	m_fmap_pages.clear();
	m_fpmap_pages.clear();
	m_read_pages_map.clear();

	sorted_allocation_list alloc_list;

	// mark space for NBT pages
	rebuild_amap_process_bt_pages((pstsdk::bt_page<node_id, node_info>*)get_db_ptr()->read_nbt_root().get(), alloc_list);

	// mark space for BBT pages
	rebuild_amap_process_bt_pages((pstsdk::bt_page<block_id, block_info>*)get_db_ptr()->read_bbt_root().get(), alloc_list);

	// mark space for blocks
	rebuild_amap_process_blocks(alloc_list);

	// build amap pages
	rebuild_amap_process_amap_pages(alloc_list);

	// build legacy map pages if applicable
	rebuild_amap_process_legacy_map_pages();

	// restore amap on disk
	flush(true);
	alloc_list.clear();
}

inline void pstsdk::allocation_map::rebuild_amap_process_amap_pages(sorted_allocation_list& alloc_list)
{
	sorted_allocation_list::iterator alloc_itr = alloc_list.begin();
	size_t amap_ind = 0;

	while(amap_ind < m_total_amap_pages)
	{
		ulonglong amap_page_location = pstsdk::disk::first_amap_page_location + (amap_ind * pstsdk::disk::amap_page_interval);

		// create empty amap page
		pstsdk::page_info pi = {amap_page_location, amap_page_location};
		m_amap_pages.push_back(get_db_ptr()->create_amap_page(pi));
		m_read_pages_map[amap_ind] = m_amap_pages.size() - 1;

		// update header root values
		m_header_values.cbAMapFree += m_amap_pages[m_amap_pages.size() - 1]->get_total_free_space();
		m_header_values.ibAMapLast = amap_page_location;

		while((alloc_itr != alloc_list.end()) && (amap_ind == disk::get_amap_page_index(alloc_itr->first)))
		{
			allocate_specific(alloc_itr->first, alloc_itr->second);
			alloc_itr++;
		}

		if(m_read_pages_map.size() >= page_cache_thresh)
			flush(false);

		amap_ind++;
	}
}

inline void pstsdk::allocation_map::rebuild_amap_process_legacy_map_pages()
{
	// Create PMap pages with valid checksum for backward compatibility.
	ulonglong pmap_page_location = pstsdk::disk::first_pmap_page_location;

	while(pmap_page_location < m_header_values.ibFileEof)
	{
		pstsdk::page_info pi = {pmap_page_location, pmap_page_location};
		m_pmap_pages.push_back(std::tr1::shared_ptr<pstsdk::pmap_page>(new pstsdk::pmap_page(get_db_ptr(), pi)));
		pmap_page_location += disk::pmap_page_interval;

		if(m_pmap_pages.size() >= page_cache_thresh)
			flush(false);
	}

	// Create FMap pages with valid checksum for backward compatibility.
	ulonglong fmap_page_location = pstsdk::disk::second_fmap_page_location;

	while(fmap_page_location < m_header_values.ibFileEof)
	{
		pstsdk::page_info pi = {fmap_page_location, fmap_page_location};
		m_fmap_pages.push_back(std::tr1::shared_ptr<pstsdk::fmap_page>(new pstsdk::fmap_page(get_db_ptr(), pi)));
		fmap_page_location += disk::fmap_page_interval;

		if(m_fmap_pages.size() >= page_cache_thresh)
			flush(false);
	}

	// Create FPMap pages with valid checksum for backward compatibility.
	ulonglong fpmap_page_location = pstsdk::disk::second_fpmap_page_location;

	while(fpmap_page_location < m_header_values.ibFileEof)
	{
		pstsdk::page_info pi = {fpmap_page_location, fpmap_page_location};
		m_fpmap_pages.push_back(std::tr1::shared_ptr<pstsdk::fpmap_page>(new pstsdk::fpmap_page(get_db_ptr(), pi)));
		fpmap_page_location += disk::fpmap_page_interval;
	}
}

template <typename K, typename V>
inline void pstsdk::allocation_map::rebuild_amap_process_bt_pages(pstsdk::bt_page<K, V>* page, sorted_allocation_list& alloc_list)
{
	alloc_list.insert(std::make_pair<pstsdk::ulonglong, size_t>(page->get_address(), disk::page_size));

	if(page->get_level() > 0)
	{
		bt_nonleaf_page<K, V>* nonleaf_page = (bt_nonleaf_page<K, V>*)page;

		for(size_t pos = 0; pos < nonleaf_page->num_values(); ++pos)
		{
			rebuild_amap_process_bt_pages(nonleaf_page->get_child(pos), alloc_list);
		}
	}
}

inline void pstsdk::allocation_map::rebuild_amap_process_blocks(sorted_allocation_list& alloc_list)
{
	std::tr1::shared_ptr<const bbt_page> bbt_root = get_db_ptr()->read_bbt_root();

	for(const_blockinfo_iterator iter = bbt_root->begin(); iter != bbt_root->end(); ++iter)
	{
		alloc_list.insert(std::make_pair<pstsdk::ulonglong, size_t>(iter->address, get_db_ptr()->get_block_disk_size(iter->size)));
		//allocate_specific(iter->address, get_db_ptr()->get_block_disk_size(iter->size));
	}
}

inline void pstsdk::allocation_map::allocate_specific(pstsdk::ulonglong location, size_t size)
{
	size_t page_index = disk::get_amap_page_index(location);

	if(page_index >= m_total_amap_pages)
		throw unexpected_page("nonsensical page location; past eof");

	size_t actual_indx = get_actual_page_index(page_index);

	m_amap_pages[actual_indx]->mark_location_allocated(location, size);
	m_dlist_page->add_page_entry(page_index, m_amap_pages[actual_indx]->get_total_free_slots());
	update_amap_free(size, false);
}

inline void pstsdk::allocation_map::commit_free_allocation(ulonglong location, size_t size)
{
	size_t page_index = disk::get_amap_page_index(location);

	if(page_index >= m_total_amap_pages)
		throw unexpected_page("nonsensical page location; past eof");

	size_t actual_indx = get_actual_page_index(page_index);

	m_amap_pages[actual_indx]->free_allocated_space(location, size);
	m_dlist_page->add_page_entry(page_index, m_amap_pages[actual_indx]->get_total_free_slots());
	update_amap_free(size, true);
}

inline pstsdk::ulonglong pstsdk::allocation_map::commit_allocate(size_t size, bool align)
{
	ulonglong location = 0;

	//check if size is crossing data section boundary
	if(size > (disk::amap_page_interval - disk::page_size))
		throw std::invalid_argument("size crossing data section boundary");

	// try to allocate from dlist page
	size_t ind = m_dlist_page->get_current_page();
	size_t actual_indx = get_actual_page_index(ind);
	location = m_amap_pages[actual_indx]->allocate_space(size, align);

	if(location != 0)
	{
		m_dlist_page->add_page_entry(ind, m_amap_pages[actual_indx]->get_total_free_slots());
		update_amap_free(size, false);
		return location;
	}

	// try to allocate from either of existing amap pages
	if(size <= m_amap_pages[actual_indx]->get_total_free_space())
	{
		for(size_t ind = 0; ind < m_total_amap_pages; ind++)
		{
			size_t actual_indx = get_actual_page_index(ind);

			if((location = m_amap_pages[actual_indx]->allocate_space(size, align)) != 0)
			{
				m_dlist_page->add_page_entry(ind, m_amap_pages[actual_indx]->get_total_free_slots());
				update_amap_free(size, false);
				return location;
			}
		}
	}

	// No space available in any of existing amaps so add new amap page as part of file growth operation
	do
	{
		ulonglong new_page_location = m_header_values.ibAMapLast + disk::amap_page_interval;
		pstsdk::page_info pi = {new_page_location, new_page_location};
		m_amap_pages.push_back(get_db_ptr()->create_amap_page(pi));
		m_read_pages_map[m_total_amap_pages++] = m_amap_pages.size() - 1;

		location = m_amap_pages[m_amap_pages.size() - 1]->allocate_space(size, align);
		m_dlist_page->add_page_entry(m_total_amap_pages - 1, m_amap_pages[m_amap_pages.size() - 1]->get_total_free_slots());

		// Update header values
		incr_amap_last();
		update_amap_free(size, false);
		grow_file();

		// Mark out locations and maintain pmap/fmap/fpmap integrity for backward compatibility
		maintain_legacy_intigrity(new_page_location);
	} 
	while(location == 0);

	return location;
}

inline void pstsdk::allocation_map::maintain_legacy_intigrity(pstsdk::ulonglong new_page_location)
{
	// Create PMap pages with valid checksum for backward compatibility.
	ulonglong location_interval = new_page_location - disk::first_amap_page_location;

	if(location_interval % disk::pmap_page_interval == 0)
	{
		ulonglong pmap_page_location = new_page_location + disk::page_size;
		pstsdk::page_info pi = {pmap_page_location, pmap_page_location};
		m_pmap_pages.push_back(std::tr1::shared_ptr<pstsdk::pmap_page>(new pstsdk::pmap_page(get_db_ptr(), pi)));
	}

	ulonglong map_page_offset = 2 * disk::page_size;

	// Create FMap pages with valid checksum for backward compatibility.
	if(new_page_location >= disk::second_fmap_page_location - map_page_offset)
	{
		location_interval = (new_page_location + map_page_offset) - disk::second_fmap_page_location;

		if(location_interval % disk::fmap_page_interval == 0)
		{
			ulonglong fmap_page_location = new_page_location + map_page_offset;
			pstsdk::page_info pi = {fmap_page_location, fmap_page_location};
			m_fmap_pages.push_back(std::tr1::shared_ptr<pstsdk::fmap_page>(new pstsdk::fmap_page(get_db_ptr(), pi)));
			map_page_offset += disk::page_size;
		}

		// Create FPMap pages with valid checksum for backward compatibility.
		if(new_page_location >= disk::second_fpmap_page_location - map_page_offset)
		{
			location_interval = (new_page_location + map_page_offset) - disk::second_fpmap_page_location;

			if(location_interval % disk::fpmap_page_interval == 0)
			{
				ulonglong fpmap_page_location = new_page_location + map_page_offset;
				pstsdk::page_info pi = {fpmap_page_location, fpmap_page_location};
				m_fpmap_pages.push_back(std::tr1::shared_ptr<pstsdk::fpmap_page>(new pstsdk::fpmap_page(get_db_ptr(), pi)));
			}
		}
	}
}

inline void pstsdk::allocation_map::invalidate_amap()
{
	m_header_values.fAMapValid = disk::invalid_amap;
	get_db_ptr()->write_header_values_amap(m_header_values);
}

inline void pstsdk::allocation_map::grow_file()
{
	std::vector<byte> dummy(disk::amap_page_interval, 0);
	get_db_ptr()->write_raw_bytes(dummy, (m_header_values.ibFileEof - disk::amap_page_interval));
}

inline void pstsdk::allocation_map::validate_amap()
{
	m_header_values.fAMapValid = disk::valid_amap_2;
	get_db_ptr()->write_header_values_amap(m_header_values);
}

inline void pstsdk::allocation_map::update_amap_free(size_t size, bool is_freed)
{
	is_freed ? m_header_values.cbAMapFree += disk::align_slot(size) : m_header_values.cbAMapFree -= disk::align_slot(size);
}

inline void pstsdk::allocation_map::incr_amap_last()
{
	m_header_values.ibAMapLast += disk::amap_page_interval;
	m_header_values.ibFileEof += disk::amap_page_interval;
	m_header_values.cbAMapFree += (disk::amap_page_interval - disk::page_size);
}

inline void pstsdk::allocation_map::initialize_dlist()
{
	if(!m_dlist_page)
	{
		try
		{
			m_dlist_page = get_db_ptr()->read_dlist_page();
		}
		catch(unexpected_page)	// dlist page is invalid or not available.
		{
			m_dlist_page = get_db_ptr()->create_dlist_page();
		}
	}
}

inline void pstsdk::allocation_map::begin_transaction()
{
	pstsdk::thread_lock lock;
	lock.aquire_lock();

	if(get_amap_validity() != disk::valid_amap_2)
		rebuild_amap();

	lock.release_lock();
}

inline pstsdk::ulonglong pstsdk::allocation_map::allocate(size_t size, bool align)
{
	pstsdk::thread_lock lock;
	lock.aquire_lock();
	return commit_allocate(size, align);
	lock.release_lock();
}

inline void pstsdk::allocation_map::free_allocation(ulonglong location, size_t size)
{
	pstsdk::thread_lock lock;
	lock.aquire_lock();
	commit_free_allocation(location, size);
	lock.release_lock();
}

inline bool pstsdk::allocation_map::is_allocated(ulonglong location, size_t size)
{
	size_t page_index = disk::get_amap_page_index(location);

	if(page_index >= m_total_amap_pages)
		throw unexpected_page("nonsensical page location; past eof");

	size_t actual_indx = get_actual_page_index(page_index);

	return m_amap_pages[actual_indx]->is_location_allocated(location, size);
}

inline void pstsdk::allocation_map::commit_transaction()
{
	flush(true);
}

inline void pstsdk::allocation_map::abort_transaction()
{
	m_read_pages_map.clear();
	m_amap_pages.clear();
	init_amap_data();
}

inline void pstsdk::allocation_map::flush(bool validate)
{
	pstsdk::thread_lock lock;
	lock.aquire_lock();

	invalidate_amap();

	// write amap pages with updated trailers
	for(std::vector<std::tr1::shared_ptr<pstsdk::amap_page>>::iterator iter = m_amap_pages.begin(); iter != m_amap_pages.end(); iter++)
	{
		get_db_ptr()->write_page(*iter);
	}

	// flush out if too many pages cached in memory
	if(m_amap_pages.size() >= page_cache_thresh)
	{
		m_read_pages_map.clear();
		m_amap_pages.clear();
	}

	// write pmap pages with updated trailers
	for(std::vector<std::tr1::shared_ptr<pstsdk::pmap_page>>::iterator iter = m_pmap_pages.begin(); iter != m_pmap_pages.end(); iter++)
	{
		get_db_ptr()->write_page(*iter);
	}

	m_pmap_pages.clear();

	// write fmap pages if any with updated trailers
	for(std::vector<std::tr1::shared_ptr<pstsdk::fmap_page>>::iterator iter = m_fmap_pages.begin(); iter != m_fmap_pages.end(); iter++)
	{
		get_db_ptr()->write_page(*iter);
	}

	m_fmap_pages.clear();

	// write fpmap pages if any with updated trailers
	for(std::vector<std::tr1::shared_ptr<pstsdk::fpmap_page>>::iterator iter = m_fpmap_pages.begin(); iter != m_fpmap_pages.end(); iter++)
	{
		get_db_ptr()->write_page(*iter);
	}

	m_fpmap_pages.clear();

	// write dlist page with updated trailer
	get_db_ptr()->write_page(m_dlist_page);

	if(validate)
		validate_amap();

	lock.release_lock();
}

#endif 