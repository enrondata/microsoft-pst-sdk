//! \file
//! \brief Database implementation
//! \author Terry Mahaffey
//!
//! Contains the db_context implementations for ANSI and Unicode stores
//! \ingroup ndb

#ifndef PSTSDK_NDB_DATABASE_H
#define PSTSDK_NDB_DATABASE_H

#include <fstream>
#include <memory>
#include <vector>
#include <map>

#include "pstsdk/util/btree.h"
#include "pstsdk/util/errors.h"
#include "pstsdk/util/primitives.h"
#include "pstsdk/util/util.h"

#include "pstsdk/disk/disk.h"

#include "pstsdk/ndb/node.h"
#include "pstsdk/ndb/page.h"
#include "pstsdk/ndb/database_iface.h"
#include "pstsdk/ndb/allocation_map.h"

namespace pstsdk 
{ 

	class node;

	template<typename T> 
	class database_impl;
	typedef database_impl<ulonglong> large_pst;
	typedef database_impl<ulong> small_pst;

	//! \brief Open a db_context for the given file
	//! \throws invalid_format if the file format is not understood
	//! \throws runtime_error if an error occurs opening the file
	//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK) if the CRC of this header doesn't match
	//! \param[in] filename The filename to open
	//! \returns A shared_ptr to the opened context
	//! \ingroup ndb_databaserelated
	shared_db_ptr open_database(const std::wstring& filename);

	//! \brief Try to open the given file as an ANSI store
	//! \throws invalid_format if the file format is not ANSI
	//! \throws runtime_error if an error occurs opening the file
	//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK) if the CRC of this header doesn't match
	//! \param[in] filename The filename to open
	//! \returns A shared_ptr to the opened context
	//! \ingroup ndb_databaserelated
	std::tr1::shared_ptr<small_pst> open_small_pst(const std::wstring& filename);

	//! \brief Try to open the given file as a Unicode store
	//! \throws invalid_format if the file format is not Unicode
	//! \throws runtime_error if an error occurs opening the file
	//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK) if the CRC of this header doesn't match
	//! \param[in] filename The filename to open
	//! \returns A shared_ptr to the opened context
	//! \ingroup ndb_databaserelated
	std::tr1::shared_ptr<large_pst> open_large_pst(const std::wstring& filename);

	//! \brief PST implementation
	//!
	//! The actual implementation of a database context - this class is responsible
	//! for translating between the disk format (as indicated by the 
	//! template parameter) and the disk-agnostic in memory classes returned from
	//! the various factory methods.
	//!
	//! \ref open_database will instantiate the correct database_impl type for a
	//! given filename.
	//! \tparam T ulonglong for a Unicode store, ulong for an ANSI store
	//! \ingroup ndb_databaserelated
	template<typename T>
	class database_impl : public db_context
	{
	public:

		//! \name Lookup functions
		//@{
		node lookup_node(node_id nid)
		{ return node(this->shared_from_this(), lookup_node_info(nid)); }
		node_info lookup_node_info(node_id nid);
		block_info lookup_block_info(block_id bid); 
		bool node_exists(node_id nid);
		bool block_exists(block_id bid);
		//@}

		//! \name Page factory functions
		//@{
		std::tr1::shared_ptr<bbt_page> read_bbt_root();
		std::tr1::shared_ptr<nbt_page> read_nbt_root();
		std::tr1::shared_ptr<bbt_page> read_bbt_page(const page_info& pi);
		std::tr1::shared_ptr<nbt_page> read_nbt_page(const page_info& pi);
		std::tr1::shared_ptr<nbt_leaf_page> read_nbt_leaf_page(const page_info& pi);
		std::tr1::shared_ptr<bbt_leaf_page> read_bbt_leaf_page(const page_info& pi);
		std::tr1::shared_ptr<nbt_nonleaf_page> read_nbt_nonleaf_page(const page_info& pi);
		std::tr1::shared_ptr<bbt_nonleaf_page> read_bbt_nonleaf_page(const page_info& pi);
		//@}

		//! \name Block factory functions
		//@{
		std::tr1::shared_ptr<block> read_block(const shared_db_ptr& parent, block_id bid)
		{ return read_block(parent, lookup_block_info(bid)); }
		std::tr1::shared_ptr<data_block> read_data_block(const shared_db_ptr& parent, block_id bid)
		{ return read_data_block(parent, lookup_block_info(bid)); }
		std::tr1::shared_ptr<extended_block> read_extended_block(const shared_db_ptr& parent, block_id bid)
		{ return read_extended_block(parent, lookup_block_info(bid)); }
		std::tr1::shared_ptr<external_block> read_external_block(const shared_db_ptr& parent, block_id bid)
		{ return read_external_block(parent, lookup_block_info(bid)); }
		std::tr1::shared_ptr<subnode_block> read_subnode_block(const shared_db_ptr& parent, block_id bid)
		{ return read_subnode_block(parent, lookup_block_info(bid)); }
		std::tr1::shared_ptr<subnode_leaf_block> read_subnode_leaf_block(const shared_db_ptr& parent, block_id bid)
		{ return read_subnode_leaf_block(parent, lookup_block_info(bid)); }
		std::tr1::shared_ptr<subnode_nonleaf_block> read_subnode_nonleaf_block(const shared_db_ptr& parent, block_id bid)
		{ return read_subnode_nonleaf_block(parent, lookup_block_info(bid)); }

		std::tr1::shared_ptr<block> read_block(const shared_db_ptr& parent, const block_info& bi);
		std::tr1::shared_ptr<data_block> read_data_block(const shared_db_ptr& parent, const block_info& bi);
		std::tr1::shared_ptr<extended_block> read_extended_block(const shared_db_ptr& parent, const block_info& bi);
		std::tr1::shared_ptr<external_block> read_external_block(const shared_db_ptr& parent, const block_info& bi);
		std::tr1::shared_ptr<subnode_block> read_subnode_block(const shared_db_ptr& parent, const block_info& bi);
		std::tr1::shared_ptr<subnode_leaf_block> read_subnode_leaf_block(const shared_db_ptr& parent, const block_info& bi);
		std::tr1::shared_ptr<subnode_nonleaf_block> read_subnode_nonleaf_block(const shared_db_ptr& parent, const block_info& bi);
		//@}

		//! \cond write_api
		//! \name Block Creation functions
		//@{
		std::tr1::shared_ptr<external_block> create_external_block(const shared_db_ptr& parent, size_t size);
		std::tr1::shared_ptr<extended_block> create_extended_block(const shared_db_ptr& parent, std::tr1::shared_ptr<external_block>& pblock);
		std::tr1::shared_ptr<extended_block> create_extended_block(const shared_db_ptr& parent, std::tr1::shared_ptr<extended_block>& pblock);
		std::tr1::shared_ptr<extended_block> create_extended_block(const shared_db_ptr& parent, size_t size);
		std::tr1::shared_ptr<subnode_nonleaf_block> create_subnode_nonleaf_block(std::tr1::shared_ptr<pstsdk::subnode_block>& pblock);
		//@}

		block_id alloc_bid(bool is_internal);
		page_id alloc_pid();
		node_id alloc_nid(nid_type node_type);

		//! \name Page creation functions
		//@{
		std::tr1::shared_ptr<amap_page> create_amap_page(const page_info& pi);
		std::tr1::shared_ptr<pstsdk::dlist_page> create_dlist_page();
		std::tr1::shared_ptr<nbt_nonleaf_page> create_nbt_nonleaf_page(std::tr1::shared_ptr<pstsdk::nbt_page>& page);
		std::tr1::shared_ptr<bbt_nonleaf_page> create_bbt_nonleaf_page(std::tr1::shared_ptr<pstsdk::bbt_page>& page);
		//@}

		//! \name Page read functions
		//@{
		std::tr1::shared_ptr<pstsdk::dlist_page> read_dlist_page();
		std::tr1::shared_ptr<amap_page> read_amap_page(const page_info& pi);
		//@}

		//! \name Page write functions
		//@{
		size_t write_page(std::tr1::shared_ptr<pstsdk::dlist_page>& the_page);
		size_t write_page(std::tr1::shared_ptr<pstsdk::amap_page>& the_page);
		size_t write_page(std::tr1::shared_ptr<pstsdk::pmap_page>& the_page);
		size_t write_page(std::tr1::shared_ptr<pstsdk::fmap_page>& the_page);
		size_t write_page(std::tr1::shared_ptr<pstsdk::fpmap_page>& the_page);
		size_t write_page(std::tr1::shared_ptr<pstsdk::nbt_leaf_page>& the_page);
		size_t write_page(std::tr1::shared_ptr<pstsdk::nbt_nonleaf_page>& the_page);
		size_t write_page(std::tr1::shared_ptr<pstsdk::bbt_leaf_page>& the_page);
		size_t write_page(std::tr1::shared_ptr<pstsdk::bbt_nonleaf_page>& the_page);
		//@}

		//! \name Block write functions
		//@{
		size_t get_block_disk_size(size_t logical_size);
		size_t read_raw_bytes(std::vector<byte>& buffer, ulonglong offset);
		size_t write_raw_bytes(std::vector<byte>& buffer, ulonglong offset);
		size_t write_block(std::tr1::shared_ptr<pstsdk::data_block>& the_block);
		size_t write_block(std::tr1::shared_ptr<pstsdk::subnode_block>& the_block);
		//@}

		//! \name Amap functions
		//@{
		std::tr1::shared_ptr<allocation_map> get_allocation_map();
		void read_header_values_amap(header_values_amap& values);
		void write_header_values_amap(header_values_amap& values);
		//@}

		//! \name Db context functions
		//@{
		void update_btree(std::vector<pstsdk::nbt_update_action>& nbt_updates);
		void update_btree(std::vector<pstsdk::bbt_update_action>& bbt_updates);
		void add_to_block_write_queue(std::map<block_id, std::tr1::shared_ptr<data_block>>& data_block_queue);
		void add_to_block_write_queue(std::map<block_id, std::tr1::shared_ptr<subnode_block>>& subnode_block_queue);
		nbt_update_action create_nbt_update_action(node_info& nd_inf, bool del = false);
		bbt_update_action create_bbt_update_action(block_info& blk_inf, bool del = false);
		void commit_db();
		void discard_changes();
		shared_db_ptr create_context();
		void commit_db(const shared_db_ptr& ctx);
		void add_ref_context();
		void release_context();
		void do_cleanup();
		//@}

		node create_node(node_id id);
		void delete_node(node_id id);
		void lock_db();
		void unlock_db();
		~database_impl() { do_cleanup(); }
		//! \endcond


	protected:
		//! \cond write_api
		//! \brief Read PST file header structure
		disk::header<T> read_header();

		//! \brief Write updated PST file header structure to disk
		void write_header(disk::header<T>& hdr);

		//! \brief Write page data to disk
		size_t write_page_data(const page_info& pi, const std::vector<byte>& data);

		//! \brief Initialize allocation map
		std::tr1::shared_ptr<allocation_map> ensure_allocation_map();

		//! \brief Write B-Tree pages to disk
		template <typename K, typename V>
		void write_out_bt_pages(std::tr1::shared_ptr<bt_page<K, V>>& page, std::map<pstsdk::page_id, pstsdk::ulonglong>& page_list);

		//! \brief Build a list of all B-Tree pages
		template <typename K, typename V>
		void build_bt_page_list(std::tr1::shared_ptr<bt_page<K, V>>& page, std::map<pstsdk::page_id, pstsdk::ulonglong>& page_list);

		//! \brief Free B-Tree pages from disk
		void free_bt_pages(std::map<pstsdk::page_id, pstsdk::ulonglong>& page_list);

		//! \brief Free blocks from disk whose ref count is < 2
		void free_blocks();

		//! \brief Write an external block to disk
		size_t write_external_block(std::tr1::shared_ptr<pstsdk::external_block>& the_block);

		//! \brief Write a subnode leaf block to disk
		size_t write_subnode_leaf_block(std::tr1::shared_ptr<subnode_leaf_block>& the_block);

		//! \brief Write a subnode nonleaf block to disk
		size_t write_subnode_nonleaf_block(std::tr1::shared_ptr<subnode_nonleaf_block>& the_block);

		//! \brief Write an extended block to disk
		size_t write_extended_block(std::tr1::shared_ptr<extended_block>& the_block);

		//! \brief Commit db context state to disk
		void commit_to_disk();

		//! \brief Commit db context state to parent context
		void commit_to_context();

		//! \brief Check if db context can be commited
		bool is_ok_to_commit();

		//! \brief Construct a database_impl
		database_impl(database_impl& other_db, shared_db_ptr parent_ctx): m_file(other_db.m_file), m_header(other_db.m_header), m_bbt_root(other_db.m_bbt_root),
			m_nbt_root(other_db.m_nbt_root), m_allocation_map(other_db.m_allocation_map), m_parent_ctx(parent_ctx), m_ctx_ref(1),
			m_bt_start(std::make_pair<std::tr1::shared_ptr<nbt_page>, std::tr1::shared_ptr<bbt_page>>(other_db.m_nbt_root, other_db.m_bbt_root)) { }
		//! \endcond

		//! \brief Construct a database_impl from this filename
		//! \throws invalid_format if the file format is not understood
		//! \throws runtime_error if an error occurs opening the file
		//! \param[in] filename The filename to open
		database_impl(const std::wstring& filename);

		//! \brief Validate the header of this file
		//! \throws invalid_format if this header is for a database format incompatible with this object
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK) if the CRC of this header doesn't match
		void validate_header();

		//! \brief Read block data, perform validation checks
		//! \param[in] bi The block information to read from disk
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The validated block data (still "encrypted")
		std::vector<byte> read_block_data(const block_info& bi);

		//! \brief Read page data, perform validation checks
		//! \param[in] pi The page information to read from disk
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the page appear incorrect 
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's signature appears incorrect
		//! \throws database_corrupt (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's ptypeRepeat != ptype
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the page's CRC doesn't match the trailer
		//! \returns The validated page data
		std::vector<byte> read_page_data(const page_info& pi);

		//! \brief Read a nbt leaf page from disk
		std::tr1::shared_ptr<nbt_leaf_page> read_nbt_leaf_page(const page_info& pi, disk::nbt_leaf_page<T>& the_page);

		//! \brief Read a bbt leaf page from disk
		std::tr1::shared_ptr<bbt_leaf_page> read_bbt_leaf_page(const page_info& pi, disk::bbt_leaf_page<T>& the_page);

		//! \brief Read a b-tree non leaf page from disk
		template<typename K, typename V>
		std::tr1::shared_ptr<bt_nonleaf_page<K,V>> read_bt_nonleaf_page(const page_info& pi, disk::bt_page<T, disk::bt_entry<T>>& the_page);

		//! \brief Read a subnode leaf block from disk
		std::tr1::shared_ptr<subnode_leaf_block> read_subnode_leaf_block(const shared_db_ptr& parent, const block_info& bi, disk::sub_leaf_block<T>& sub_block);

		//! \brief Read a subnode non leaf block from disk
		std::tr1::shared_ptr<subnode_nonleaf_block> read_subnode_nonleaf_block(const shared_db_ptr& parent, const block_info& bi, disk::sub_nonleaf_block<T>& sub_block);

		//! \brief Open database from PST file
		friend shared_db_ptr open_database(const std::wstring& filename);

		//! \brief Open ANSI PST file
		friend std::tr1::shared_ptr<small_pst> open_small_pst(const std::wstring& filename);

		//! \brief Open UNICODE PST file
		friend std::tr1::shared_ptr<large_pst> open_large_pst(const std::wstring& filename);

		std::tr1::shared_ptr<file> m_file;
		disk::header<T> m_header;
		std::tr1::shared_ptr<bbt_page> m_bbt_root;
		std::tr1::shared_ptr<nbt_page> m_nbt_root;

		//! \cond write_api
		std::tr1::shared_ptr<allocation_map> m_allocation_map;
		shared_db_ptr m_parent_ctx;
		std::vector<nbt_update_action> m_nbt_updates;
		std::vector<bbt_update_action> m_bbt_updates;
		std::pair<std::tr1::shared_ptr<nbt_page>, std::tr1::shared_ptr<bbt_page>> m_bt_start;
		std::map<block_id, std::tr1::shared_ptr<data_block>> m_data_block_queue;
		std::map<block_id, std::tr1::shared_ptr<subnode_block>> m_subnode_block_queue;
		size_t m_ctx_ref;
		lock_var m_db_lock;
		//! \endcond
	};

	//! \cond dont_show_these_member_function_specializations
	template<>
	inline void database_impl<ulong>::validate_header()
	{
		// the behavior of open_database depends on this throw; this can not go under PSTSDK_VALIDATION_WEAK
		if(m_header.wVer >= disk::database_format_unicode_min)
			throw invalid_format();

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
		ulong crc = disk::compute_crc(((byte*)&m_header) + disk::header_crc_locations<ulong>::start, disk::header_crc_locations<ulong>::length);

		if(crc != m_header.dwCRCPartial)
			throw crc_fail("header dwCRCPartial failure", 0, 0, crc, m_header.dwCRCPartial);
#endif
	}

	template<>
	inline void database_impl<ulonglong>::validate_header()
	{
		// the behavior of open_database depends on this throw; this can not go under PSTSDK_VALIDATION_WEAK
		if(m_header.wVer < disk::database_format_unicode_min)
			throw invalid_format();

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
		ulong crc_partial = disk::compute_crc(((byte*)&m_header) + disk::header_crc_locations<ulonglong>::partial_start, disk::header_crc_locations<ulonglong>::partial_length);
		ulong crc_full = disk::compute_crc(((byte*)&m_header) + disk::header_crc_locations<ulonglong>::full_start, disk::header_crc_locations<ulonglong>::full_length);

		if(crc_partial != m_header.dwCRCPartial)
			throw crc_fail("header dwCRCPartial failure", 0, 0, crc_partial, m_header.dwCRCPartial);

		if(crc_full != m_header.dwCRCFull)
			throw crc_fail("header dwCRCFull failure", 0, 0, crc_full, m_header.dwCRCFull);
#endif
	}
	//! \endcond

	//! \cond write_api
	template<>
	inline void pstsdk::database_impl<ulong>::write_header(disk::header<ulong>& hdr)
	{
		hdr.dwCRCPartial = disk::compute_crc(((byte*)&hdr) + disk::header_crc_locations<ulong>::start, disk::header_crc_locations<ulong>::length);
		std::vector<byte> header_bytes(sizeof(hdr));
		memmove(&header_bytes[0], &hdr, sizeof(hdr));

		pstsdk::thread_lock lock;
		lock.aquire_lock();
		m_file->write(header_bytes, 0);
		lock.release_lock();
	}

	template<>
	inline void pstsdk::database_impl<ulonglong>::write_header(disk::header<ulonglong>& hdr)
	{
		hdr.dwCRCPartial = disk::compute_crc(((byte*)&hdr) + disk::header_crc_locations<ulonglong>::partial_start, disk::header_crc_locations<ulonglong>::partial_length);
		hdr.dwCRCFull = disk::compute_crc(((byte*)&hdr) + disk::header_crc_locations<ulonglong>::full_start, disk::header_crc_locations<ulonglong>::full_length);
		std::vector<byte> header_bytes(sizeof(hdr));
		memmove(&header_bytes[0], &hdr, sizeof(hdr));

		pstsdk::thread_lock lock;
		lock.aquire_lock();
		m_file->write(header_bytes, 0);
		lock.release_lock();
	}
	//! \endcond

} // end namespace

inline pstsdk::shared_db_ptr pstsdk::open_database(const std::wstring& filename)
{
	try 
	{
		shared_db_ptr db = open_small_pst(filename);
		return db;
	}
	catch(invalid_format&)
	{
		// well, that didn't work
	}

	shared_db_ptr db = open_large_pst(filename);

	return db;
}

inline std::tr1::shared_ptr<pstsdk::small_pst> pstsdk::open_small_pst(const std::wstring& filename)
{
	std::tr1::shared_ptr<small_pst> db(new small_pst(filename));
	return db;
}

inline std::tr1::shared_ptr<pstsdk::large_pst> pstsdk::open_large_pst(const std::wstring& filename)
{
	std::tr1::shared_ptr<large_pst> db(new large_pst(filename));
	return db;
}

template<typename T>
inline std::vector<pstsdk::byte> pstsdk::database_impl<T>::read_block_data(const block_info& bi)
{
	size_t aligned_size = disk::align_disk<T>(bi.size);

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(aligned_size > disk::max_block_disk_size)
		throw unexpected_block("nonsensical block size");

	if(bi.address + aligned_size > m_header.root_info.ibFileEof)
		throw unexpected_block("nonsensical block location; past eof");
#endif

	std::vector<byte> buffer(aligned_size);
	disk::block_trailer<T>* bt = (disk::block_trailer<T>*)(&buffer[0] + aligned_size - sizeof(disk::block_trailer<T>));

	pstsdk::thread_lock lock;
	lock.aquire_lock();
	m_file->read(buffer, bi.address);    
	lock.release_lock();

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(bt->bid != bi.id)
		throw unexpected_block("wrong block id");

	if(bt->cb != bi.size)
		throw unexpected_block("wrong block size");

	if(bt->signature != disk::compute_signature(bi.id, bi.address))
		throw sig_mismatch("block sig mismatch", bi.address, bi.id, disk::compute_signature(bi.id, bi.address), bt->signature);
#endif

#ifdef PSTSDK_VALIDATION_LEVEL_FULL
	ulong crc = disk::compute_crc(&buffer[0], bi.size);
	if(crc != bt->crc)
		throw crc_fail("block crc failure", bi.address, bi.id, crc, bt->crc);
#endif

	return buffer;
}

template<typename T>
std::vector<pstsdk::byte> pstsdk::database_impl<T>::read_page_data(const page_info& pi)
{
#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(pi.address + disk::page_size > m_header.root_info.ibFileEof)
		throw unexpected_page("nonsensical page location; past eof");

	if(((pi.address - disk::first_amap_page_location) % disk::page_size) != 0)
		throw unexpected_page("nonsensical page location; not sector aligned");
#endif

	std::vector<byte> buffer(disk::page_size);
	disk::page<T>* ppage = (disk::page<T>*)&buffer[0];

	pstsdk::thread_lock lock;
	lock.aquire_lock();
	m_file->read(buffer, pi.address);
	lock.release_lock();

#ifdef PSTSDK_VALIDATION_LEVEL_FULL
	ulong crc = disk::compute_crc(&buffer[0], disk::page<T>::page_data_size);
	if(crc != ppage->trailer.crc)
		throw crc_fail("page crc failure", pi.address, pi.id, crc, ppage->trailer.crc);
#endif

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(ppage->trailer.bid != pi.id)
		throw unexpected_page("wrong page id");

	if(ppage->trailer.page_type != ppage->trailer.page_type_repeat)
		throw database_corrupt("ptype != ptype repeat?");

	if(ppage->trailer.signature != disk::compute_signature(pi.id, pi.address))
		throw sig_mismatch("page sig mismatch", pi.address, pi.id, disk::compute_signature(pi.id, pi.address), ppage->trailer.signature);
#endif

	return buffer;
}


template<typename T>
inline std::tr1::shared_ptr<pstsdk::bbt_page> pstsdk::database_impl<T>::read_bbt_root()
{ 
	pstsdk::thread_lock lock(&m_db_lock);
	lock.aquire_lock();

	if(!m_bbt_root)
	{
		page_info pi = { m_header.root_info.brefBBT.bid, m_header.root_info.brefBBT.ib };
		m_bbt_root = read_bbt_page(pi); 
	}

	lock.release_lock();

	return m_bbt_root;
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::nbt_page> pstsdk::database_impl<T>::read_nbt_root()
{ 
	pstsdk::thread_lock lock(&m_db_lock);
	lock.aquire_lock();

	if(!m_nbt_root)
	{
		page_info pi = { m_header.root_info.brefNBT.bid, m_header.root_info.brefNBT.ib };
		m_nbt_root = read_nbt_page(pi);
	}

	lock.release_lock();

	return m_nbt_root;
}

template<typename T>
inline pstsdk::database_impl<T>::database_impl(const std::wstring& filename)
	: m_file(new file(filename)), m_ctx_ref(1)
{
	std::vector<byte> buffer(sizeof(m_header));

	pstsdk::thread_lock lock;
	lock.aquire_lock();
	m_file->read(buffer, 0);
	lock.release_lock();

	memcpy(&m_header, &buffer[0], sizeof(m_header));

	validate_header();
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::nbt_leaf_page> pstsdk::database_impl<T>::read_nbt_leaf_page(const page_info& pi)
{
	std::vector<byte> buffer = read_page_data(pi);
	disk::page<T>* ppage = (disk::page<T>*)&buffer[0];

	if(ppage->trailer.page_type == disk::page_type_nbt)
	{
		disk::nbt_leaf_page<T>* leaf_page = (disk::nbt_leaf_page<T>*)ppage;

		if(leaf_page->level == 0)
			return read_nbt_leaf_page(pi, *leaf_page);
	}

	throw unexpected_page("page_type != page_type_nbt");
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::nbt_leaf_page> pstsdk::database_impl<T>::read_nbt_leaf_page(const page_info& pi, disk::nbt_leaf_page<T>& the_page)
{
	node_info ni;
	std::vector<std::pair<node_id, node_info>> nodes;

	for(int i = 0; i < the_page.num_entries; ++i)
	{
		ni.id = static_cast<node_id>(the_page.entries[i].nid);
		ni.data_bid = the_page.entries[i].data;
		ni.sub_bid = the_page.entries[i].sub;
		ni.parent_id = the_page.entries[i].parent_nid;

		nodes.push_back(std::make_pair(ni.id, ni));
	}

#ifndef BOOST_NO_RVALUE_REFERENCES
	return std::tr1::shared_ptr<nbt_leaf_page>(new nbt_leaf_page(shared_from_this(), pi, std::move(nodes), pstsdk::disk::bt_page<T, pstsdk::disk::nbt_leaf_entry<T>>::max_entries));
#else
	return std::tr1::shared_ptr<nbt_leaf_page>(new nbt_leaf_page(shared_from_this(), pi, nodes, pstsdk::disk::bt_page<T, pstsdk::disk::nbt_leaf_entry<T>>::max_entries));
#endif
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::bbt_leaf_page> pstsdk::database_impl<T>::read_bbt_leaf_page(const page_info& pi)
{
	std::vector<byte> buffer = read_page_data(pi);
	disk::page<T>* ppage = (disk::page<T>*)&buffer[0];

	if(ppage->trailer.page_type == disk::page_type_bbt)
	{
		disk::bbt_leaf_page<T>* leaf_page = (disk::bbt_leaf_page<T>*)ppage;

		if(leaf_page->level == 0)
			return read_bbt_leaf_page(pi, *leaf_page);
	}

	throw unexpected_page("page_type != page_type_bbt");
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::bbt_leaf_page> pstsdk::database_impl<T>::read_bbt_leaf_page(const page_info& pi, disk::bbt_leaf_page<T>& the_page)
{
	block_info bi;
	std::vector<std::pair<block_id, block_info>> blocks;

	for(int i = 0; i < the_page.num_entries; ++i)
	{
		bi.id = the_page.entries[i].ref.bid;
		bi.address = the_page.entries[i].ref.ib;
		bi.size = the_page.entries[i].size;
		bi.ref_count = the_page.entries[i].ref_count;

		blocks.push_back(std::make_pair(bi.id, bi));
	}

#ifndef BOOST_NO_RVALUE_REFERENCES
	return std::tr1::shared_ptr<bbt_leaf_page>(new bbt_leaf_page(shared_from_this(), pi, std::move(blocks), pstsdk::disk::bt_page<T, pstsdk::disk::bbt_leaf_entry<T>>::max_entries));
#else
	return std::tr1::shared_ptr<bbt_leaf_page>(new bbt_leaf_page(shared_from_this(), pi, blocks, pstsdk::disk::bt_page<T, pstsdk::disk::bbt_leaf_entry<T>>::max_entries));
#endif
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::nbt_nonleaf_page> pstsdk::database_impl<T>::read_nbt_nonleaf_page(const page_info& pi)
{
	std::vector<byte> buffer = read_page_data(pi);
	disk::page<T>* ppage = (disk::page<T>*)&buffer[0];

	if(ppage->trailer.page_type == disk::page_type_nbt)
	{
		disk::nbt_nonleaf_page<T>* nonleaf_page = (disk::nbt_nonleaf_page<T>*)ppage;

		if(nonleaf_page->level > 0)
			return read_bt_nonleaf_page<node_id, node_info>(pi, *nonleaf_page);
	}

	throw unexpected_page("page_type != page_type_nbt");
}

template<typename T>
template<typename K, typename V>
inline std::tr1::shared_ptr<pstsdk::bt_nonleaf_page<K,V>> pstsdk::database_impl<T>::read_bt_nonleaf_page(const page_info& pi, pstsdk::disk::bt_page<T, disk::bt_entry<T>>& the_page)
{
	std::vector<std::pair<K, page_info>> nodes;

	for(int i = 0; i < the_page.num_entries; ++i)
	{
		page_info subpi = { the_page.entries[i].ref.bid, the_page.entries[i].ref.ib };
		nodes.push_back(std::make_pair(static_cast<K>(the_page.entries[i].key), subpi));
	}

#ifndef BOOST_NO_RVALUE_REFERENCES
	return std::tr1::shared_ptr<bt_nonleaf_page<K,V>>(new bt_nonleaf_page<K,V>(shared_from_this(), pi, the_page.level, std::move(nodes), pstsdk::disk::bt_page<T, pstsdk::disk::bt_entry<T>>::max_entries));
#else
	return std::tr1::shared_ptr<bt_nonleaf_page<K,V>>(new bt_nonleaf_page<K,V>(shared_from_this(), pi, the_page.level, nodes, pstsdk::disk::bt_page<T, pstsdk::disk::bt_entry<T>>::max_entries));
#endif
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::bbt_nonleaf_page> pstsdk::database_impl<T>::read_bbt_nonleaf_page(const page_info& pi)
{
	std::vector<byte> buffer = read_page_data(pi);
	disk::page<T>* ppage = (disk::page<T>*)&buffer[0];

	if(ppage->trailer.page_type == disk::page_type_bbt)
	{
		disk::bbt_nonleaf_page<T>* nonleaf_page = (disk::bbt_nonleaf_page<T>*)ppage;

		if(nonleaf_page->level > 0)
			return read_bt_nonleaf_page<block_id, block_info>(pi, *nonleaf_page);
	}

	throw unexpected_page("page_type != page_type_bbt");
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::bbt_page> pstsdk::database_impl<T>::read_bbt_page(const page_info& pi)
{
	std::vector<byte> buffer = read_page_data(pi);
	disk::page<T>* ppage = (disk::page<T>*)&buffer[0];

	if(ppage->trailer.page_type == disk::page_type_bbt)
	{
		disk::bbt_leaf_page<T>* leaf = (disk::bbt_leaf_page<T>*)ppage;
		if(leaf->level == 0)
		{
			// it really is a leaf!
			return read_bbt_leaf_page(pi, *leaf);
		}
		else
		{
			disk::bbt_nonleaf_page<T>* nonleaf = (disk::bbt_nonleaf_page<T>*)ppage;
			return read_bt_nonleaf_page<block_id, block_info>(pi, *nonleaf);
		}
	}
	else
	{
		throw unexpected_page("page_type != page_type_bbt");
	}  
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::nbt_page> pstsdk::database_impl<T>::read_nbt_page(const page_info& pi)
{
	std::vector<byte> buffer = read_page_data(pi);
	disk::page<T>* ppage = (disk::page<T>*)&buffer[0];

	if(ppage->trailer.page_type == disk::page_type_nbt)
	{
		disk::nbt_leaf_page<T>* leaf = (disk::nbt_leaf_page<T>*)ppage;
		if(leaf->level == 0)
		{
			// it really is a leaf!
			return read_nbt_leaf_page(pi, *leaf);
		}
		else
		{
			disk::nbt_nonleaf_page<T>* nonleaf = (disk::nbt_nonleaf_page<T>*)ppage;
			return read_bt_nonleaf_page<node_id, node_info>(pi, *nonleaf);
		}
	}
	else
	{
		throw unexpected_page("page_type != page_type_nbt");
	}  
}

template<typename T>
inline pstsdk::node_info pstsdk::database_impl<T>::lookup_node_info(node_id nid)
{
	return read_nbt_root()->lookup(nid); 
}

template<typename T>
inline pstsdk::block_info pstsdk::database_impl<T>::lookup_block_info(block_id bid)
{
	if(bid == 0)
	{
		block_info bi;
		bi.id = bi.address = bi.size = bi.ref_count = 0;
		return bi;
	}
	else
	{
		return read_bbt_root()->lookup(bid & (~(block_id(disk::block_id_attached_bit))));
	}
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::block> pstsdk::database_impl<T>::read_block(const shared_db_ptr& parent, const block_info& bi)
{
	std::tr1::shared_ptr<block> pblock;

	try
	{
		pblock = read_data_block(parent, bi);
	}
	catch(unexpected_block&)
	{
		pblock = read_subnode_block(parent, bi);
	}

	return pblock;
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::data_block> pstsdk::database_impl<T>::read_data_block(const shared_db_ptr& parent, const block_info& bi)
{
	// check if we have the block in queue waiting to be commited to disk
	std::map<block_id, std::tr1::shared_ptr<data_block>>::iterator itr = m_data_block_queue.find(bi.id);
	if(itr != m_data_block_queue.end())
		return itr->second;

	if(disk::bid_is_external(bi.id))
		return read_external_block(parent, bi);

	std::vector<byte> buffer(sizeof(disk::extended_block<T>));
	disk::extended_block<T>* peblock = (disk::extended_block<T>*)&buffer[0];

	pstsdk::thread_lock lock;
	lock.aquire_lock();
	m_file->read(buffer, bi.address);
	lock.release_lock();

	// the behavior of read_block depends on this throw; this can not go under PSTSDK_VALIDATION_WEAK
	if(peblock->block_type != disk::block_type_extended)
		throw unexpected_block("extended block expected");

	return read_extended_block(parent, bi);
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::extended_block> pstsdk::database_impl<T>::read_extended_block(const shared_db_ptr& parent, const block_info& bi)
{
	if(!disk::bid_is_internal(bi.id))
		throw unexpected_block("internal bid expected");

	std::vector<byte> buffer = read_block_data(bi);
	disk::extended_block<T>* peblock = (disk::extended_block<T>*)&buffer[0];
	std::vector<block_id> child_blocks;

	for(int i = 0; i < peblock->count; ++i)
		child_blocks.push_back(peblock->bid[i]);

#ifdef __GNUC__
	// GCC gave link errors on extended_block<T> and external_block<T> max_size
	// with the below alernative
	uint sub_size = 0;
	if(peblock->level == 1)
		sub_size = disk::external_block<T>::max_size;
	else
		sub_size = disk::extended_block<T>::max_size;
#else
	uint sub_size = (peblock->level == 1 ? disk::external_block<T>::max_size : disk::extended_block<T>::max_size);
#endif
	uint sub_page_count = peblock->level == 1 ? 1 : disk::extended_block<T>::max_count;

#ifndef BOOST_NO_RVALUE_REFERENCES
	return std::tr1::shared_ptr<extended_block>(new extended_block(parent, bi, peblock->level, peblock->total_size, sub_size, disk::extended_block<T>::max_count, sub_page_count, std::move(child_blocks)));
#else
	return std::tr1::shared_ptr<extended_block>(new extended_block(parent, bi, peblock->level, peblock->total_size, sub_size, disk::extended_block<T>::max_count, sub_page_count, child_blocks));
#endif
}

//! \cond write_api
template<typename T>
inline std::tr1::shared_ptr<pstsdk::external_block> pstsdk::database_impl<T>::create_external_block(const shared_db_ptr& parent, size_t size)
{
	return std::tr1::shared_ptr<external_block>(new external_block(parent, disk::external_block<T>::max_size, size));
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::extended_block> pstsdk::database_impl<T>::create_extended_block(const shared_db_ptr& parent, std::tr1::shared_ptr<external_block>& pchild_block)
{
	std::vector<std::tr1::shared_ptr<data_block>> child_blocks;
	child_blocks.push_back(pchild_block);

#ifndef BOOST_NO_RVALUE_REFERENCES
	return std::tr1::shared_ptr<extended_block>(new extended_block(parent, 1, pchild_block->get_total_size(), disk::external_block<T>::max_size, disk::extended_block<T>::max_count, 1, std::move(child_blocks)));
#else
	return std::tr1::shared_ptr<extended_block>(new extended_block(parent, 1, pchild_block->get_total_size(), disk::external_block<T>::max_size, disk::extended_block<T>::max_count, 1, child_blocks));
#endif
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::extended_block> pstsdk::database_impl<T>::create_extended_block(const shared_db_ptr& parent, std::tr1::shared_ptr<extended_block>& pchild_block)
{
	std::vector<std::tr1::shared_ptr<data_block>> child_blocks;
	child_blocks.push_back(pchild_block);

	assert(pchild_block->get_level() == 1);

#ifndef BOOST_NO_RVALUE_REFERENCES
	return std::tr1::shared_ptr<extended_block>(new extended_block(parent, 2, pchild_block->get_total_size(), disk::extended_block<T>::max_size, disk::extended_block<T>::max_count, disk::extended_block<T>::max_count, std::move(child_blocks)));
#else
	return std::tr1::shared_ptr<extended_block>(new extended_block(parent, 2, pchild_block->get_total_size(), disk::extended_block<T>::max_size, disk::extended_block<T>::max_count, disk::extended_block<T>::max_count, child_blocks));
#endif
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::extended_block> pstsdk::database_impl<T>::create_extended_block(const shared_db_ptr& parent, size_t size)
{
	ushort level = size > disk::extended_block<T>::max_size ? 2 : 1;
#ifdef __GNUC__
	// More strange link errors
	size_t child_max_size;
	if(level == 1)
		child_max_size = disk::external_block<T>::max_size;
	else
		child_max_size = disk::extended_block<T>::max_size;
#else
	size_t child_max_size = level == 1 ? disk::external_block<T>::max_size : disk::extended_block<T>::max_size;
#endif
	ulong child_max_blocks = level == 1 ? 1 : disk::extended_block<T>::max_count;

	return std::tr1::shared_ptr<extended_block>(new extended_block(parent, level, size, child_max_size, disk::extended_block<T>::max_count, child_max_blocks));
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::subnode_nonleaf_block> pstsdk::database_impl<T>::create_subnode_nonleaf_block(std::tr1::shared_ptr<pstsdk::subnode_block>& pchild_block)
{
	std::vector<std::pair<node_id, block_id>> subnode_info;
	subnode_info.push_back(std::make_pair<node_id, block_id>(pchild_block->get_key(0), pchild_block->get_id()));

	std::vector<std::tr1::shared_ptr<subnode_block>> child_blocks;
	child_blocks.push_back(pchild_block);

#ifndef BOOST_NO_RVALUE_REFERENCES
	return std::tr1::shared_ptr<subnode_nonleaf_block>(new subnode_nonleaf_block(shared_from_this(), std::move(subnode_info), std::move(child_blocks), pstsdk::disk::sub_nonleaf_block<T>::maxEntries));
#else
	return std::tr1::shared_ptr<subnode_nonleaf_block>(new subnode_nonleaf_block(shared_from_this(), subnode_info, child_blocks, pstsdk::disk::sub_nonleaf_block<T>::maxEntries));
#endif
}
//! \endcond

template<typename T>
inline std::tr1::shared_ptr<pstsdk::external_block> pstsdk::database_impl<T>::read_external_block(const shared_db_ptr& parent, const block_info& bi)
{
	if(bi.id == 0)
	{
		return std::tr1::shared_ptr<external_block>(new external_block(parent, bi, disk::external_block<T>::max_size,  std::vector<byte>()));
	}

	if(!disk::bid_is_external(bi.id))
		throw unexpected_block("External BID expected");

	std::vector<byte> buffer = read_block_data(bi);

	if(m_header.bCryptMethod == disk::crypt_method_permute)
	{
		disk::permute(&buffer[0], bi.size, false);
	}
	else if(m_header.bCryptMethod == disk::crypt_method_cyclic)
	{
		disk::cyclic(&buffer[0], bi.size, (ulong)bi.id);
	}

#ifndef BOOST_NO_RVALUE_REFERENCES
	return std::tr1::shared_ptr<external_block>(new external_block(parent, bi, disk::external_block<T>::max_size, std::move(buffer)));
#else
	return std::tr1::shared_ptr<external_block>(new external_block(parent, bi, disk::external_block<T>::max_size, buffer));
#endif
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::subnode_block> pstsdk::database_impl<T>::read_subnode_block(const shared_db_ptr& parent, const block_info& bi)
{
	if(bi.id == 0)
	{
		return std::tr1::shared_ptr<subnode_block>(new subnode_leaf_block(parent, bi, std::vector<std::pair<node_id, subnode_info>>(), pstsdk::disk::sub_block<T, pstsdk::disk::sub_leaf_entry<T>>::maxEntries));
	}

	// check if we have the block in queue waiting to be commited to disk
	std::map<block_id, std::tr1::shared_ptr<subnode_block>>::iterator itr = m_subnode_block_queue.find(bi.id);
	if(itr != m_subnode_block_queue.end())
		return itr->second;

	std::vector<byte> buffer = read_block_data(bi);
	disk::sub_leaf_block<T>* psub = (disk::sub_leaf_block<T>*)&buffer[0];
	std::tr1::shared_ptr<subnode_block> sub_block;

	if(psub->level == 0)
	{
		sub_block = read_subnode_leaf_block(parent, bi, *psub);
	}
	else
	{
		sub_block = read_subnode_nonleaf_block(parent, bi, *(disk::sub_nonleaf_block<T>*)&buffer[0]);
	}

	return sub_block;
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::subnode_leaf_block> pstsdk::database_impl<T>::read_subnode_leaf_block(const shared_db_ptr& parent, const block_info& bi)
{
	std::vector<byte> buffer = read_block_data(bi);
	disk::sub_leaf_block<T>* psub = (disk::sub_leaf_block<T>*)&buffer[0];
	std::tr1::shared_ptr<subnode_leaf_block> sub_block;

	if(psub->level == 0)
	{
		sub_block = read_subnode_leaf_block(parent, bi, *psub);
	}
	else
	{
		throw unexpected_block("psub->level != 0");
	}

	return sub_block; 
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::subnode_nonleaf_block> pstsdk::database_impl<T>::read_subnode_nonleaf_block(const shared_db_ptr& parent, const block_info& bi)
{
	std::vector<byte> buffer = read_block_data(bi);
	disk::sub_nonleaf_block<T>* psub = (disk::sub_nonleaf_block<T>*)&buffer[0];
	std::tr1::shared_ptr<subnode_nonleaf_block> sub_block;

	if(psub->level != 0)
	{
		sub_block = read_subnode_nonleaf_block(parent, bi, *psub);
	}
	else
	{
		throw unexpected_block("psub->level == 1");
	}

	return sub_block;
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::subnode_leaf_block> pstsdk::database_impl<T>::read_subnode_leaf_block(const shared_db_ptr& parent, const block_info& bi, disk::sub_leaf_block<T>& sub_block)
{
	subnode_info ni;
	std::vector<std::pair<node_id, subnode_info>> subnodes;

	for(int i = 0; i < sub_block.count; ++i)
	{
		ni.id = sub_block.entry[i].nid;
		ni.data_bid = sub_block.entry[i].data;
		ni.sub_bid = sub_block.entry[i].sub;

		subnodes.push_back(std::make_pair(sub_block.entry[i].nid, ni));
	}

#ifndef BOOST_NO_RVALUE_REFERENCES
	return std::tr1::shared_ptr<subnode_leaf_block>(new subnode_leaf_block(parent, bi, std::move(subnodes), pstsdk::disk::sub_block<T, pstsdk::disk::sub_leaf_entry<T>>::maxEntries));
#else
	return std::tr1::shared_ptr<subnode_leaf_block>(new subnode_leaf_block(parent, bi, subnodes, pstsdk::disk::sub_block<T, pstsdk::disk::sub_leaf_entry<T>>::maxEntries));
#endif
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::subnode_nonleaf_block> pstsdk::database_impl<T>::read_subnode_nonleaf_block(const shared_db_ptr& parent, const block_info& bi, disk::sub_nonleaf_block<T>& sub_block)
{
	std::vector<std::pair<node_id, block_id>> subnodes;

	for(int i = 0; i < sub_block.count; ++i)
	{
		subnodes.push_back(std::make_pair(sub_block.entry[i].nid_key, sub_block.entry[i].sub_block_bid));
	}

#ifndef BOOST_NO_RVALUE_REFERENCES
	return std::tr1::shared_ptr<subnode_nonleaf_block>(new subnode_nonleaf_block(parent, bi, std::move(subnodes), pstsdk::disk::sub_block<T,pstsdk::disk::sub_nonleaf_entry<T>>::maxEntries ));
#else
	return std::tr1::shared_ptr<subnode_nonleaf_block>(new subnode_nonleaf_block(parent, bi, subnodes, pstsdk::disk::sub_block<T,pstsdk::disk::sub_nonleaf_entry<T>>::maxEntries));
#endif
}

//! \cond write_api
template<typename T>
inline pstsdk::node_id pstsdk::database_impl<T>::alloc_nid(pstsdk::nid_type node_type)
{
	if(m_parent_ctx)
		return m_parent_ctx->alloc_nid(node_type);

	pstsdk::thread_lock lock;
	lock.aquire_lock();
	node_id new_nid = make_nid(node_type, ++m_header.rgnid[node_type]);
	lock.release_lock();

	return new_nid;
}

template<typename T>
inline pstsdk::block_id pstsdk::database_impl<T>::alloc_bid(bool is_internal)
{
	if(m_parent_ctx)
		return m_parent_ctx->alloc_bid(is_internal);

	pstsdk::thread_lock lock;
	lock.aquire_lock();
#ifdef __GNUC__
	typename disk::header<T>::block_id_disk disk_id;
	memcpy(&disk_id, m_header.bidNextB, sizeof(disk_id));

	block_id next_bid = disk_id;

	disk_id += disk::block_id_increment;
	memcpy(m_header.bidNextB, &disk_id, sizeof(disk_id));
#else
	block_id next_bid = m_header.bidNextB;
	m_header.bidNextB += disk::block_id_increment;
#endif

	if(is_internal)
		next_bid |= disk::block_id_internal_bit;

	lock.release_lock();

	return next_bid;
}

template<typename T>
inline pstsdk::page_id pstsdk::database_impl<T>::alloc_pid()
{
	if(m_parent_ctx)
		return m_parent_ctx->alloc_pid();

	pstsdk::thread_lock lock;
	lock.aquire_lock();
	page_id next_pid = m_header.bidNextP++;
	lock.release_lock();

	return next_pid;
}

template<typename T>
inline pstsdk::disk::header<T> pstsdk::database_impl<T>::read_header()
{
	disk::header<T> pst_header;
	std::vector<byte> buffer(sizeof(pst_header));

	pstsdk::thread_lock lock;
	lock.aquire_lock();
	m_file->read(buffer, 0);
	lock.release_lock();

	memmove(&pst_header, &buffer[0], sizeof(pst_header));
	return pst_header;
}

template<typename T>
inline void pstsdk::database_impl<T>::read_header_values_amap(pstsdk::header_values_amap& values)
{
	values.ibAMapLast = m_header.root_info.ibAMapLast;
	values.ibFileEof = m_header.root_info.ibFileEof;
	values.cbAMapFree = m_header.root_info.cbAMapFree;
	values.fAMapValid = static_cast<disk::amap_validity>(m_header.root_info.fAMapValid);
	values.cbPMapFree = m_header.root_info.cbPMapFree;
}

template<typename T>
inline void pstsdk::database_impl<T>::write_header_values_amap(pstsdk::header_values_amap& values)
{
	disk::header<T> header = read_header();
	header.root_info.ibAMapLast = m_header.root_info.ibAMapLast = static_cast<T>(values.ibAMapLast);
	header.root_info.ibFileEof = m_header.root_info.ibFileEof = static_cast<T>(values.ibFileEof);
	header.root_info.cbAMapFree = m_header.root_info.cbAMapFree = static_cast<T>(values.cbAMapFree);
	header.root_info.fAMapValid = m_header.root_info.fAMapValid = static_cast<disk::amap_validity>(values.fAMapValid);
	header.root_info.cbPMapFree = m_header.root_info.cbPMapFree = static_cast<T>(values.cbPMapFree);
	write_header(header);
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::amap_page> pstsdk::database_impl<T>::create_amap_page(const page_info& pi)
{	
	return std::tr1::shared_ptr<pstsdk::amap_page>(new pstsdk::amap_page(shared_from_this(), pi));
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::dlist_page> pstsdk::database_impl<T>::create_dlist_page()
{	
	page_info pi = {disk::dlist_page_location, disk::dlist_page_location};
	return std::tr1::shared_ptr<dlist_page>(new dlist_page(shared_from_this(), pi));
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::nbt_nonleaf_page> pstsdk::database_impl<T>::create_nbt_nonleaf_page(std::tr1::shared_ptr<pstsdk::nbt_page>& child_page)
{
	std::vector<std::pair<pstsdk::node_id, pstsdk::page_info>> child_page_info;
	page_info pi = { child_page->get_page_id(), child_page->get_address() };
	child_page_info.push_back(std::make_pair<node_id, pstsdk::page_info>(child_page->get_key(0), pi));

	std::vector<std::tr1::shared_ptr<nbt_page>> child_pages;
	child_pages.push_back(child_page);

#ifndef BOOST_NO_RVALUE_REFERENCES
	return std::tr1::shared_ptr<nbt_nonleaf_page>(new nbt_nonleaf_page(shared_from_this(), child_page->get_level() + 1, std::move(child_page_info),
		std::move(child_pages), pstsdk::disk::nbt_nonleaf_page<T>::max_entries));
#else
	return std::tr1::shared_ptr<nbt_nonleaf_page>(new nbt_nonleaf_page(shared_from_this(), child_page->get_level() + 1, child_page_info,
		child_pages, pstsdk::disk::nbt_nonleaf_page<T>::max_entries));
#endif
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::bbt_nonleaf_page> pstsdk::database_impl<T>::create_bbt_nonleaf_page(std::tr1::shared_ptr<pstsdk::bbt_page>& child_page)
{
	std::vector<std::pair<pstsdk::block_id, pstsdk::page_info>> child_page_info;
	page_info pi = { child_page->get_page_id(), child_page->get_address() };
	child_page_info.push_back(std::make_pair<block_id, pstsdk::page_info>(child_page->get_key(0), pi));

	std::vector<std::tr1::shared_ptr<bbt_page>> child_pages;
	child_pages.push_back(child_page);

#ifndef BOOST_NO_RVALUE_REFERENCES
	return std::tr1::shared_ptr<bbt_nonleaf_page>(new bbt_nonleaf_page(shared_from_this(), child_page->get_level() + 1, std::move(child_page_info),
		std::move(child_pages), pstsdk::disk::bbt_nonleaf_page<T>::max_entries));
#else
	return std::tr1::shared_ptr<bbt_nonleaf_page>(new bbt_nonleaf_page(shared_from_this(), child_page->get_level() + 1, child_page_info,
		child_pages, pstsdk::disk::bbt_nonleaf_page<T>::max_entries));
#endif
}

template<typename T>
inline size_t pstsdk::database_impl<T>::read_raw_bytes(std::vector<byte>& buffer, ulonglong offset)
{	
	size_t retVal = 0;

	pstsdk::thread_lock lock;
	lock.aquire_lock();
	retVal = m_file->read(buffer, offset);
	lock.release_lock();

	return retVal;
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_raw_bytes(std::vector<byte>& buffer, ulonglong offset)
{	
	size_t retVal = 0;

	pstsdk::thread_lock lock;
	lock.aquire_lock();
	retVal = m_file->write(buffer, offset);
	lock.release_lock();

	return retVal;
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::allocation_map> pstsdk::database_impl<T>::get_allocation_map()
{
	return ensure_allocation_map();
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::allocation_map> pstsdk::database_impl<T>::ensure_allocation_map()
{
	pstsdk::thread_lock lock(&m_db_lock);
	lock.aquire_lock();

	if(!m_allocation_map)
	{
		m_allocation_map = std::tr1::shared_ptr<pstsdk::allocation_map>(new pstsdk::allocation_map(shared_from_this()));
	}

	lock.release_lock();

	return m_allocation_map;
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::amap_page> pstsdk::database_impl<T>::read_amap_page(const page_info& pi)
{
	std::vector<byte> buffer = read_page_data(pi);
	disk::page<T>* ppage = (disk::page<T>*)&buffer[0];

	if(ppage->trailer.page_type == disk::page_type_amap)
	{
		disk::amap_page<T>* the_page = (disk::amap_page<T>*)ppage;

		std::vector<byte> map_data;

		for(int i = disk::amap_page<T>::padding_bytes_cnt; i < the_page->page_data_size; i++)
		{	
			map_data.push_back(the_page->data[i]);
		}

#ifndef BOOST_NO_RVALUE_REFERENCES
		return std::tr1::shared_ptr<amap_page>(new amap_page(shared_from_this(), pi, std::move(map_data)));
#else
		return std::tr1::shared_ptr<amap_page>(new amap_page(shared_from_this(), pi, map_data));
#endif
	}

	throw unexpected_page("page_type != page_type_amap");
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_page(std::tr1::shared_ptr<pstsdk::pmap_page>& the_page)
{
	disk::page_trailer<T> pmap_page_trailer;
	pstsdk::page_info pi = { the_page->get_page_id(), the_page->get_address() };

	std::vector<byte> buffer(((disk::page_size - sizeof(pmap_page_trailer)) - disk::max_map_bytes), 0);

	std::vector<byte> page_data = the_page->get_page_data();
	buffer.insert(buffer.end(), page_data.begin(), page_data.end());

	pmap_page_trailer.page_type = disk::page_type_pmap;
	pmap_page_trailer.page_type_repeat = disk::page_type_pmap;
	pmap_page_trailer.crc = disk::compute_crc(&buffer[0], buffer.size());
	pmap_page_trailer.bid = static_cast<T>(pi.id);
	pmap_page_trailer.signature = 0x0;

	std::vector<byte> trailer_vect(sizeof(pmap_page_trailer));
	memmove(&trailer_vect[0], &pmap_page_trailer, sizeof(pmap_page_trailer));

	buffer.insert(buffer.end(), trailer_vect.begin(), trailer_vect.end());

	return write_page_data(pi, buffer);
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_page(std::tr1::shared_ptr<pstsdk::fmap_page>& the_page)
{
	disk::page_trailer<T> fmap_page_trailer;
	pstsdk::page_info pi = { the_page->get_page_id(), the_page->get_address() };

	std::vector<byte> buffer(((disk::page_size - sizeof(fmap_page_trailer)) - disk::max_map_bytes), 0);

	std::vector<byte> page_data = the_page->get_page_data();
	buffer.insert(buffer.end(), page_data.begin(), page_data.end());

	fmap_page_trailer.page_type = disk::page_type_fmap;
	fmap_page_trailer.page_type_repeat = disk::page_type_fmap;
	fmap_page_trailer.crc = disk::compute_crc(&buffer[0], buffer.size());
	fmap_page_trailer.bid = static_cast<T>(pi.id);
	fmap_page_trailer.signature = 0x0;

	std::vector<byte> trailer_vect(sizeof(fmap_page_trailer));
	memmove(&trailer_vect[0], &fmap_page_trailer, sizeof(fmap_page_trailer));

	buffer.insert(buffer.end(), trailer_vect.begin(), trailer_vect.end());

	return write_page_data(pi, buffer);
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_page(std::tr1::shared_ptr<pstsdk::fpmap_page>& the_page)
{
	disk::page_trailer<T> fpmap_page_trailer;
	pstsdk::page_info pi = { the_page->get_page_id(), the_page->get_address() };

	std::vector<byte> buffer(((disk::page_size - sizeof(fpmap_page_trailer)) - disk::max_map_bytes), 0);

	std::vector<byte> page_data = the_page->get_page_data();
	buffer.insert(buffer.end(), page_data.begin(), page_data.end());

	fpmap_page_trailer.page_type = disk::page_type_fpmap;
	fpmap_page_trailer.page_type_repeat = disk::page_type_fpmap;
	fpmap_page_trailer.crc = disk::compute_crc(&buffer[0], buffer.size());
	fpmap_page_trailer.bid = static_cast<T>(pi.id);
	fpmap_page_trailer.signature = 0x0;

	std::vector<byte> trailer_vect(sizeof(fpmap_page_trailer));
	memmove(&trailer_vect[0], &fpmap_page_trailer, sizeof(fpmap_page_trailer));

	buffer.insert(buffer.end(), trailer_vect.begin(), trailer_vect.end());

	return write_page_data(pi, buffer);
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_page(std::tr1::shared_ptr<pstsdk::amap_page>& the_page)
{
	pstsdk::page_info pi = { the_page->get_page_id(), the_page->get_address() };

	std::vector<byte> buffer(the_page->get_page_data());
	buffer.insert(buffer.begin(), disk::amap_page<T>::padding_bytes_cnt, 0);

	disk::page_trailer<T> amap_page_trailer;
	amap_page_trailer.page_type = disk::page_type_amap;
	amap_page_trailer.page_type_repeat = disk::page_type_amap;
	amap_page_trailer.crc = disk::compute_crc(&buffer[0], buffer.size());
	amap_page_trailer.bid = static_cast<T>(pi.id);
	amap_page_trailer.signature = 0x0;

	std::vector<byte> trailer_vect(sizeof(amap_page_trailer));
	memmove(&trailer_vect[0], &amap_page_trailer, sizeof(amap_page_trailer));

	buffer.insert(buffer.end(), trailer_vect.begin(), trailer_vect.end());

	return write_page_data(pi, buffer);
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_page_data(const page_info& pi, const std::vector<byte>& data)
{

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(pi.address + disk::page_size > m_header.root_info.ibFileEof)
		throw unexpected_page("nonsensical page location; past eof");

	if(((pi.address - disk::first_amap_page_location) % disk::page_size) != 0)
		throw unexpected_page("nonsensical page location; not sector aligned");

	if(data.size() != disk::page_size)
		throw unexpected_page("nonsensical page data");
#endif

	size_t retVal = 0;

	pstsdk::thread_lock lock;
	lock.aquire_lock();
	retVal = m_file->write(data, pi.address);
	lock.release_lock();

	return retVal;
}

template<typename T>
inline std::tr1::shared_ptr<pstsdk::dlist_page> pstsdk::database_impl<T>::read_dlist_page()
{
	page_info pi = {disk::dlist_page_location, disk::dlist_page_location};

	std::vector<byte> buffer = read_page_data(pi);
	disk::page<T>* ppage = (disk::page<T>*)&buffer[0];

	ulong actual_crc = disk::compute_crc(ppage->data, ppage->page_data_size);

	if(ppage->trailer.page_type == disk::page_type_dlist)
	{
		disk::dlist_page<T>* the_page = (disk::dlist_page<T>*)ppage;

		std::vector<ulong> entries(the_page->num_entries);

		for(int i = 0 ; i < the_page->num_entries; i++)
			entries[i] = the_page->entries[i];

#ifndef BOOST_NO_RVALUE_REFERENCES
		return std::tr1::shared_ptr<dlist_page>(new dlist_page(shared_from_this(), pi, the_page->flags, the_page->current_page, std::move(entries)));
#else
		return std::tr1::shared_ptr<dlist_page>(new dlist_page(shared_from_this(), pi, the_page->flags, the_page->current_page, entries));
#endif
	}

	throw unexpected_page("page_type != page_type_dlist");
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_page(std::tr1::shared_ptr<pstsdk::dlist_page>& the_page)
{
	page_info pi = {disk::dlist_page_location, disk::dlist_page_location};

	std::vector<ulong> entries;
	the_page->get_entries(entries);

	disk::dlist_page<T> dlist_page;
	dlist_page.flags = the_page->get_flags();
	dlist_page.backfill_location = 0;
	dlist_page.current_page = the_page->get_current_page();
	dlist_page.num_entries = entries.size() > dlist_page.max_entries ? dlist_page.max_entries : entries.size();

	for(size_t i = 0; i < dlist_page.num_entries; i++)
	{
		dlist_page.entries[i] = entries[i];
	}

	dlist_page.trailer.page_type = disk::page_type_dlist;
	dlist_page.trailer.page_type_repeat = disk::page_type_dlist;
	dlist_page.trailer.crc = disk::compute_crc(&dlist_page, (sizeof(dlist_page) - sizeof(dlist_page.trailer)));
	dlist_page.trailer.bid = static_cast<T>(pi.id);
	dlist_page.trailer.signature = disk::compute_signature(pi.id, pi.address);

	std::vector<byte> data(sizeof(dlist_page));
	std::memmove(&data[0], &dlist_page, sizeof(dlist_page));

	return write_page_data(pi, data);
}

template<typename T>
inline void pstsdk::database_impl<T>::update_btree(std::vector<pstsdk::nbt_update_action>& nbt_updates)
{
	// ensure nbt
	read_nbt_root();

	pstsdk::thread_lock lock(&m_db_lock);
	lock.aquire_lock();

	m_nbt_updates.insert(m_nbt_updates.end(), nbt_updates.begin(), nbt_updates.end());

	for(size_t ind = 0; ind < nbt_updates.size(); ind++)
	{
		pstsdk::nbt_update_action nbt_action = nbt_updates[ind];

		switch(nbt_action.action)
		{
		case bt_insert:
			{
				bool exists = false;

				// make sure this node is not already inserted
				try
				{
					m_nbt_root->lookup(nbt_action.nd_id);
					exists = true;
				}
				catch(pstsdk::key_not_found<pstsdk::node_id>) { exists = false; }

				if(exists)
				{
					throw pstsdk::duplicate_key<node_id>(nbt_action.nd_id);
				}
				else
				{
					std::pair<std::tr1::shared_ptr<nbt_page>, std::tr1::shared_ptr<nbt_page>> result = m_nbt_root->insert(nbt_action.nd_id, nbt_action.nd_inf);
					m_nbt_root = result.first;

					// case when nbt root is a leaf page and there is a split in it
					if(m_nbt_root->get_level() == 0 && result.second != 0)
					{
						std::tr1::shared_ptr<pstsdk::nbt_nonleaf_page> new_page = create_nbt_nonleaf_page(result.first);

						for(pstsdk::const_nodeinfo_iterator itr = result.second->begin(); itr != result.second->end(); ++itr)
						{
							new_page->insert(itr->id, *itr);
						}

						m_nbt_root = new_page;
					}
				}
			}
			continue;

		case bt_modify:
			m_nbt_root = m_nbt_root->modify(nbt_action.nd_id, nbt_action.nd_inf);
			continue;

		case bt_remove:
			m_nbt_root = m_nbt_root->remove(nbt_action.nd_id);
			continue;
		}
	}

	lock.release_lock();
	nbt_updates.clear();
}

template<typename T>
inline void pstsdk::database_impl<T>::update_btree(std::vector<pstsdk::bbt_update_action>& bbt_updates)
{
	// ensure bbt
	read_bbt_root();

	pstsdk::thread_lock lock(&m_db_lock);
	lock.aquire_lock();

	m_bbt_updates.insert(m_bbt_updates.end(), bbt_updates.begin(), bbt_updates.end());

	for(size_t ind = 0; ind < bbt_updates.size(); ind++)
	{
		pstsdk::bbt_update_action bbt_action = bbt_updates[ind];

		switch(bbt_action.action)
		{
		case bt_insert:
			{
				bool exists = false;

				// make sure this node is not already inserted
				try
				{
					m_bbt_root->lookup(bbt_action.blk_id);
					exists = true;
				}
				catch(pstsdk::key_not_found<pstsdk::block_id>) { exists = false; }

				if(exists)
				{
					throw pstsdk::duplicate_key<block_id>(bbt_action.blk_id);
				}
				else
				{
					std::pair<std::tr1::shared_ptr<bbt_page>, std::tr1::shared_ptr<bbt_page>> result = m_bbt_root->insert(bbt_action.blk_id, bbt_action.blk_inf);
					m_bbt_root = result.first;

					// case when bbt root is a leaf page and there is a split in it
					if(m_bbt_root->get_level() == 0 && result.second != 0)
					{
						std::tr1::shared_ptr<pstsdk::bbt_nonleaf_page> new_page = create_bbt_nonleaf_page(result.first);

						for(pstsdk::const_blockinfo_iterator itr = result.second->begin(); itr != result.second->end(); ++itr)
						{
							new_page->insert(itr->id, *itr);
						}

						m_bbt_root = new_page;
					}
				}
			}
			continue;

		case bt_modify:
			m_bbt_root = m_bbt_root->modify(bbt_action.blk_id, bbt_action.blk_inf);
			continue;

		case bt_remove:
			m_bbt_root = m_bbt_root->remove(bbt_action.blk_id);
			continue;
		}
	}

	lock.release_lock();
	bbt_updates.clear();
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_page(std::tr1::shared_ptr<nbt_nonleaf_page>& the_page)
{
	disk::bt_page<T, disk::bt_entry<T>> page;
	pstsdk::page_info pi = { the_page->get_page_id(), the_page->get_address() };

	page.num_entries_max = the_page->get_max_entries();
	page.num_entries = the_page->num_values();
	page.level = (byte)the_page->get_level();
	page.entry_size = sizeof(disk::bt_entry<T>);

	memset(&page._ignore, 0, page.extra_space);

	for(size_t ind = 0; ind < the_page->num_values(); ind++)
	{
		page_info pi = the_page->get_child_page_info(ind);
		disk::block_reference<T> ref = { static_cast<T>(pi.id), static_cast<T>(pi.address) };
		disk::bt_entry<T> entry = { the_page->get_key(ind), ref };
		page.entries[ind] = entry;
	}

	page.trailer.page_type = disk::page_type_nbt;
	page.trailer.page_type_repeat = disk::page_type_nbt;
	page.trailer.crc = disk::compute_crc(&page, (sizeof(page) - sizeof(page.trailer)));
	page.trailer.bid = static_cast<T>(pi.id);
	page.trailer.signature = disk::compute_signature(pi.id, pi.address) ;

	std::vector<byte> page_data(sizeof(page));
	memmove(&page_data[0], &page, sizeof(page));
	return write_page_data(pi, page_data);
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_page(std::tr1::shared_ptr<nbt_leaf_page>& the_page)
{
	disk::bt_page<T, disk::nbt_leaf_entry<T>> page;
	pstsdk::page_info pi = { the_page->get_page_id(), the_page->get_address() };

	page.num_entries_max = the_page->get_max_entries();
	page.num_entries = the_page->num_values();
	page.level = (byte)the_page->get_level();
	page.entry_size = sizeof(disk::nbt_leaf_entry<T>);

	memset(&page._ignore, 0, page.extra_space);

	for(size_t ind = 0; ind < the_page->num_values(); ind++)
	{
		node_info nd_inf = the_page->get_value(ind);
		disk::nbt_leaf_entry<T> entry = { static_cast<T>(nd_inf.id), static_cast<T>(nd_inf.data_bid), static_cast<T>(nd_inf.sub_bid), static_cast<T>(nd_inf.parent_id) };
		page.entries[ind] = entry;
	}

	page.trailer.page_type = disk::page_type_nbt;
	page.trailer.page_type_repeat = disk::page_type_nbt;
	page.trailer.crc = disk::compute_crc(&page, (sizeof(page) - sizeof(page.trailer)));
	page.trailer.bid = static_cast<T>(pi.id);
	page.trailer.signature = disk::compute_signature(pi.id, pi.address) ;

	std::vector<byte> page_data(sizeof(page));
	memmove(&page_data[0], &page, sizeof(page));
	return write_page_data(pi, page_data);
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_page(std::tr1::shared_ptr<bbt_nonleaf_page>& the_page)
{
	disk::bt_page<T, disk::bt_entry<T>> page;
	pstsdk::page_info pi = { the_page->get_page_id(), the_page->get_address() };

	page.num_entries_max = the_page->get_max_entries();
	page.num_entries = the_page->num_values();
	page.level = (byte)the_page->get_level();
	page.entry_size = sizeof(disk::bt_entry<T>);

	memset(&page._ignore, 0, page.extra_space);

	for(size_t ind = 0; ind < the_page->num_values(); ind++)
	{
		page_info pi = the_page->get_child_page_info(ind);
		disk::block_reference<T> ref = { static_cast<T>(pi.id), static_cast<T>(pi.address) };
		disk::bt_entry<T> entry = { static_cast<T>(the_page->get_key(ind)), ref };
		page.entries[ind] = entry;
	}

	page.trailer.page_type = disk::page_type_bbt;
	page.trailer.page_type_repeat = disk::page_type_bbt;
	page.trailer.crc = disk::compute_crc(&page, (sizeof(page) - sizeof(page.trailer)));
	page.trailer.bid = static_cast<T>(pi.id);
	page.trailer.signature = disk::compute_signature(pi.id, pi.address) ;

	std::vector<byte> page_data(sizeof(page));
	memmove(&page_data[0], &page, sizeof(page));
	return write_page_data(pi, page_data);
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_page(std::tr1::shared_ptr<bbt_leaf_page>& the_page)
{
	disk::bt_page<T, disk::bbt_leaf_entry<T>> page;
	pstsdk::page_info pi = { the_page->get_page_id(), the_page->get_address() };

	page.num_entries_max = the_page->get_max_entries();
	page.num_entries = the_page->num_values();
	page.level = (byte)the_page->get_level();
	page.entry_size = sizeof(disk::bbt_leaf_entry<T>);

	memset(&page._ignore, 0, page.extra_space);

	for(size_t ind = 0; ind < the_page->num_values(); ind++)
	{
		block_info blk_inf = the_page->get_value(ind);
		disk::block_reference<T> ref = { static_cast<T>(blk_inf.id), static_cast<T>(blk_inf.address) };
		disk::bbt_leaf_entry<T> entry = { ref, blk_inf.size, blk_inf.ref_count };
		page.entries[ind] = entry;
	}

	page.trailer.page_type = disk::page_type_bbt;
	page.trailer.page_type_repeat = disk::page_type_bbt;
	page.trailer.crc = disk::compute_crc(&page, (sizeof(page) - sizeof(page.trailer)));
	page.trailer.bid = static_cast<T>(pi.id);
	page.trailer.signature = disk::compute_signature(pi.id, pi.address) ;

	std::vector<byte> page_data(sizeof(page));
	memmove(&page_data[0], &page, sizeof(page));
	return write_page_data(pi, page_data);
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_block(std::tr1::shared_ptr<pstsdk::data_block>& the_block)
{
	if(the_block->is_internal())
		return write_extended_block(std::tr1::static_pointer_cast<pstsdk::extended_block>(the_block));
	else
		return write_external_block(std::tr1::static_pointer_cast<pstsdk::external_block>(the_block));
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_block(std::tr1::shared_ptr<pstsdk::subnode_block>& the_block)
{
	if(the_block->get_level() == 0)
		return write_subnode_leaf_block(std::tr1::static_pointer_cast<pstsdk::subnode_leaf_block>(the_block));
	else
		return write_subnode_nonleaf_block(std::tr1::static_pointer_cast<pstsdk::subnode_nonleaf_block>(the_block));
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_external_block(std::tr1::shared_ptr<pstsdk::external_block>& the_block)
{
	size_t ret_val = 0;
	disk::block_trailer<T> trailer;
	size_t logical_size = the_block->get_total_size();
	size_t disk_size = disk::align_disk<T>(logical_size);
	ulonglong address = the_block->get_address();

	if(address == 0)
	{
		address = ensure_allocation_map()->allocate(disk_size);
		the_block->set_address(address);
		the_block->set_disk_size(logical_size);
		m_data_block_queue[the_block->get_id()] = the_block;
	}
	else
	{
		if(!block_exists(the_block->get_id()))
			return 0;

		std::vector<byte> block_data(logical_size);
		the_block->read(block_data, 0);
		block_data.resize(disk_size, 0);

		if(m_header.bCryptMethod == disk::crypt_method_permute)
		{
			disk::permute(&block_data[0], logical_size, true);
		}
		else if(m_header.bCryptMethod == disk::crypt_method_cyclic)
		{
			disk::cyclic(&block_data[0], logical_size, static_cast<ulong>(the_block->get_id()));
		}

		trailer.cb = logical_size;
		trailer.crc = disk::compute_crc(&block_data[0], logical_size);
		trailer.signature = disk::compute_signature(the_block->get_id(), address);
		trailer.bid = static_cast<T>(the_block->get_id());

		std::memmove(&block_data[disk_size - sizeof(trailer)], &trailer, sizeof(trailer));
		ret_val = write_raw_bytes(block_data, address);
	}

	return ret_val;
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_extended_block(std::tr1::shared_ptr<pstsdk::extended_block>& the_block)
{
	size_t ret_val = 0;
	disk::extended_block<T> extnd_block;
	disk::block_trailer<T> trailer;

	ulonglong address = the_block->get_address();
	size_t logical_size = the_block->get_disk_size();
	size_t disk_size = disk::align_disk<T>(logical_size);

	extnd_block.block_type = disk::block_type_extended;
	extnd_block.level = (byte)the_block->get_level();
	extnd_block.count = the_block->get_page_count();

	for(size_t ind = 0; ind < extnd_block.count; ind++)
	{ 
		extnd_block.bid[ind] = static_cast<T>(the_block->get_page(ind)->get_id());
	}

	if(address == 0)
	{
		logical_size = (sizeof(extnd_block.bid[0]) * extnd_block.count) + 8;
		disk_size = disk::align_disk<T>(logical_size);

		address = ensure_allocation_map()->allocate(disk_size);
		the_block->set_address(address);
		the_block->set_disk_size(logical_size);
		m_data_block_queue[the_block->get_id()] = the_block;
	}
	else
	{
		if(!block_exists(the_block->get_id()))
			return 0;

		extnd_block.total_size = the_block->get_total_size();
		std::vector<byte> block_data(disk_size, 0);
		std::memmove(&block_data[0], &extnd_block, logical_size);

		trailer.cb = logical_size;
		trailer.crc = disk::compute_crc(&block_data[0], logical_size);
		trailer.signature = disk::compute_signature(the_block->get_id(), address);
		trailer.bid = static_cast<T>(the_block->get_id());

		std::memmove(&block_data[disk_size - sizeof(trailer)], &trailer, sizeof(trailer));
		ret_val = write_raw_bytes(block_data, address);
	}

	return ret_val;
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_subnode_leaf_block(std::tr1::shared_ptr<pstsdk::subnode_leaf_block>& the_block)
{
	size_t ret_val = 0;
	disk::sub_leaf_block<T> sub_block;
	disk::block_trailer<T> trailer;

	ulonglong address = the_block->get_address();
	size_t logical_size = the_block->get_disk_size();
	size_t disk_size = disk::align_disk<T>(logical_size);

	sub_block.block_type = disk::block_type_sub;
	sub_block.level = (byte)the_block->get_level();
	sub_block.count = the_block->num_values();

	for(size_t pos = 0; pos < sub_block.count; pos++)
	{
		subnode_info sb_inf = the_block->get_value(pos);

		disk::sub_leaf_entry<T> entry;
		entry.nid = sb_inf.id;
		entry.data = static_cast<T>(sb_inf.data_bid);
		entry.sub = static_cast<T>(sb_inf.sub_bid);

		sub_block.entry[pos] = entry;
	}

	if(address == 0)
	{
		logical_size = (sizeof(sub_block.entry[0]) * sub_block.count) + 8;
		disk_size = disk::align_disk<T>(logical_size);

		address = ensure_allocation_map()->allocate(disk_size);
		the_block->set_address(address);
		the_block->set_disk_size(logical_size);
		m_subnode_block_queue[the_block->get_id()] = the_block;
	}
	else
	{
		if(!block_exists(the_block->get_id()))
			return 0;

		std::vector<byte> block_data(disk_size, 0);
		std::memmove(&block_data[0], &sub_block, logical_size);

		trailer.cb = logical_size;
		trailer.crc = disk::compute_crc(&block_data[0], logical_size);
		trailer.signature = disk::compute_signature(the_block->get_id(), address);
		trailer.bid = static_cast<T>(the_block->get_id());

		std::memmove(&block_data[disk_size - sizeof(trailer)], &trailer, sizeof(trailer));
		ret_val = write_raw_bytes(block_data, address);
	}

	return ret_val;
}

template<typename T>
inline size_t pstsdk::database_impl<T>::write_subnode_nonleaf_block(std::tr1::shared_ptr<pstsdk::subnode_nonleaf_block>& the_block)
{
	size_t ret_val = 0;
	disk::sub_nonleaf_block<T> sub_block;
	disk::block_trailer<T> trailer;

	ulonglong address = the_block->get_address();
	size_t logical_size = the_block->get_disk_size();
	size_t disk_size = disk::align_disk<T>(logical_size);

	sub_block.block_type = disk::block_type_sub;
	sub_block.level = (byte)the_block->get_level();
	sub_block.count = the_block->num_values();

	for(size_t pos = 0; pos < the_block->num_values(); pos++)
	{
		std::pair<node_id, block_id> sb_inf = the_block->get_subnode_info(pos);

		disk::sub_nonleaf_entry<T> entry;
		entry.nid_key = sb_inf.first;
		entry.sub_block_bid = static_cast<T>(sb_inf.second);
		sub_block.entry[pos] = entry;
	}

	if(address == 0)
	{
		logical_size = (sizeof(sub_block.entry[0]) * sub_block.count) + 8;
		disk_size = disk::align_disk<T>(logical_size);

		address = ensure_allocation_map()->allocate(disk_size);
		the_block->set_address(address);
		the_block->set_disk_size(logical_size);
		m_subnode_block_queue[the_block->get_id()] = the_block;
	}
	else
	{
		if(!block_exists(the_block->get_id()))
			return 0;

		std::vector<byte> block_data(disk_size, 0);
		std::memmove(&block_data[0], &sub_block, logical_size);

		trailer.cb = logical_size;
		trailer.crc = disk::compute_crc(&block_data[0], logical_size);
		trailer.signature = disk::compute_signature(the_block->get_id(), address);
		trailer.bid = static_cast<T>(the_block->get_id());

		std::memmove(&block_data[disk_size - sizeof(trailer)], &trailer, sizeof(trailer));
		ret_val = write_raw_bytes(block_data, address);
	}

	return ret_val;
}

template<typename T>
inline void pstsdk::database_impl<T>::free_bt_pages(std::map<pstsdk::page_id, pstsdk::ulonglong>& page_list)
{
	ensure_allocation_map();

	for(std::map<pstsdk::page_id, pstsdk::ulonglong>::iterator page_iter = page_list.begin(); page_iter != page_list.end(); ++page_iter)
	{
		m_allocation_map->free_allocation(page_iter->second, disk::page_size);
	}
}

template<typename T>
inline void pstsdk::database_impl<T>::free_blocks()
{
	ensure_allocation_map();
	std::vector<block_id> delete_list(0);
	pstsdk::thread_lock lock(&m_db_lock);

	lock.aquire_lock();

	for(pstsdk::const_blockinfo_iterator iter = m_bbt_root->begin(); iter != m_bbt_root->end(); ++iter)
	{
		if(iter->ref_count < 2)
		{
			m_allocation_map->free_allocation(iter->address, disk::align_disk<T>(iter->size));
			delete_list.push_back(iter->id);
		}
	}

	for(size_t ind = 0; ind < delete_list.size(); ind++)
	{
		m_bbt_root = m_bbt_root->remove(delete_list[ind]);
	}

	lock.release_lock();
}

template<typename T>
inline size_t pstsdk::database_impl<T>::get_block_disk_size(size_t logical_size)
{
	return disk::align_disk<T>(logical_size);
}

template<typename T>
inline pstsdk::node pstsdk::database_impl<T>::create_node(pstsdk::node_id id)
{
	pstsdk::node_info nd_inf = {id, 0, 0, 0};
	bool node_exists = false;

	// make sure this node already does not exist
	try  
	{  
		read_nbt_root()->lookup(id);  
		node_exists = true;
	}  
	catch(pstsdk::key_not_found<node_id>) { }  

	if(node_exists)
		throw pstsdk::duplicate_key<node_id>( id) ; 
	else
		return node(shared_from_this(), nd_inf);
}

template<typename T>
inline void pstsdk::database_impl<T>::delete_node(pstsdk::node_id id)
{
	// ensure nbt root
	read_nbt_root();

	// look up node info
	pstsdk::node_info nd_inf = m_nbt_root->lookup(id);
	pstsdk::node del_node(shared_from_this(), nd_inf);

	// drop all data blocks of this node
	del_node.drop_data_blocks();

	// drop all subnodes of this node
	del_node.drop_subnodes();

	// remove this node from nbt
	pstsdk::thread_lock lock(&m_db_lock);
	lock.aquire_lock();
	m_nbt_updates.push_back(create_nbt_update_action(nd_inf, true));
	m_nbt_root = m_nbt_root->remove(id);
	lock.release_lock();
}

template<typename T>
template <typename K, typename V>
inline void pstsdk::database_impl<T>::build_bt_page_list(std::tr1::shared_ptr<bt_page<K, V>>& page, std::map<pstsdk::page_id, pstsdk::ulonglong>& page_list)
{
	page_list.insert(std::make_pair<pstsdk::page_id, pstsdk::ulonglong>(page->get_page_id(), page->get_address()));

	if(page->get_level() > 0)
	{
		std::tr1::shared_ptr<pstsdk::bt_nonleaf_page<K, V>> nonleaf_page = std::tr1::static_pointer_cast<pstsdk::bt_nonleaf_page<K, V>>(page);

		for(size_t pos = 0; pos < nonleaf_page->num_values(); pos++)
		{
			build_bt_page_list(nonleaf_page->get_child(pos, false), page_list);
		}
	}
}

template<typename T>
template <typename K, typename V>
inline void pstsdk::database_impl<T>::write_out_bt_pages(std::tr1::shared_ptr<bt_page<K, V>>& page, std::map<pstsdk::page_id, pstsdk::ulonglong>& page_list)
{
	page_list.erase(page->get_page_id());

	if(page->is_modified())
	{
		if(page->get_address() == 0)
			page->set_address(ensure_allocation_map()->allocate(disk::page_size, true));

		if(page->get_level() == 0)
			write_page(std::tr1::static_pointer_cast<pstsdk::bt_leaf_page<K, V>>(page));

		if(page->get_level() > 0)
		{
			std::tr1::shared_ptr<pstsdk::bt_nonleaf_page<K, V>> nonleaf_page = std::tr1::static_pointer_cast<pstsdk::bt_nonleaf_page<K, V>>(page);

			for(size_t pos = 0; pos < nonleaf_page->num_values(); pos++)
			{
				page_info pg_inf = nonleaf_page->get_child_page_info(pos);

				if(pg_inf.address == 0)
				{
					if(nonleaf_page->get_child(pos)->get_address() == 0)
						nonleaf_page->get_child(pos)->set_address(ensure_allocation_map()->allocate(disk::page_size, true));

					pg_inf.address = nonleaf_page->get_child(pos)->get_address();
					nonleaf_page->set_page_info(pos, pg_inf);
				}

				write_out_bt_pages(nonleaf_page->get_child(pos, false), page_list);
			}

			write_page(std::tr1::static_pointer_cast<pstsdk::bt_nonleaf_page<K, V>>(page));
		}
	}
}

template<typename T>
pstsdk::shared_db_ptr pstsdk::database_impl<T>::create_context()
{
	read_nbt_root();
	read_bbt_root();
	ensure_allocation_map();

	shared_db_ptr ctx = std::tr1::shared_ptr<database_impl<T>>(new database_impl<T>(*this, shared_from_this()));
	add_ref_context();
	return ctx;
}

template<typename T>
inline void pstsdk::database_impl<T>::commit_db()
{
	m_parent_ctx ? commit_to_context() : commit_to_disk();
}

template<typename T>
void pstsdk::database_impl<T>::commit_db(const pstsdk::shared_db_ptr& ctx)
{
	ctx->commit_db();
}

template<typename T>
inline void pstsdk::database_impl<T>::commit_to_disk()
{
	// ensure NBT and BBT
	read_nbt_root();
	read_bbt_root();

	pstsdk::thread_lock lock;
	lock.aquire_lock();

	ensure_allocation_map()->begin_transaction();


	// do not free blocks from disk if there are child contexts active in memory
	if(m_ctx_ref < 2)
		free_blocks();

	std::map<pstsdk::page_id, pstsdk::ulonglong> page_list;

	if(m_nbt_root->is_modified())
	{
		page_info pi_nbt = { m_header.root_info.brefNBT.bid, m_header.root_info.brefNBT.ib };
		build_bt_page_list(read_nbt_page(pi_nbt), page_list);

		write_out_bt_pages(m_nbt_root, page_list);
	}

	if(m_bbt_root->is_modified())
	{
		page_info pi_bbt = { m_header.root_info.brefBBT.bid, m_header.root_info.brefBBT.ib };
		build_bt_page_list(read_bbt_page(pi_bbt), page_list);

		write_out_bt_pages(m_bbt_root, page_list);

		for(std::map<pstsdk::block_id, std::tr1::shared_ptr<pstsdk::data_block>>::iterator itr = m_data_block_queue.begin();
			itr != m_data_block_queue.end(); ++itr) { write_block(itr->second); }

		for(std::map<pstsdk::block_id, std::tr1::shared_ptr<pstsdk::subnode_block>>::iterator itr = m_subnode_block_queue.begin();
			itr != m_subnode_block_queue.end(); ++itr) { write_block(itr->second); }
	}

	free_bt_pages(page_list);

	m_header.root_info.brefNBT.bid = static_cast<T>(m_nbt_root->get_page_id());
	m_header.root_info.brefNBT.ib = static_cast<T>(m_nbt_root->get_address());

	m_header.root_info.brefBBT.bid = static_cast<T>(m_bbt_root->get_page_id());
	m_header.root_info.brefBBT.ib = static_cast<T>(m_bbt_root->get_address());

	m_allocation_map->commit_transaction();
	write_header(m_header);

	lock.release_lock();
}

template<typename T>
inline void pstsdk::database_impl<T>::commit_to_context()
{
	// ensure NBT and BBT
	read_nbt_root();
	read_bbt_root();

	pstsdk::thread_lock lock;
	lock.aquire_lock();

	// check if any of the nodes have already been modified in parent context
	if(!is_ok_to_commit())
	{
		m_nbt_root = m_bt_start.first;
		m_bbt_root = m_bt_start.second;

		m_nbt_updates.clear();
		m_bbt_updates.clear();

		throw pstsdk::node_save_error("some node(s) already modified.");
	}

	// commit changes to parent btrees
	m_parent_ctx->update_btree(m_nbt_updates);
	m_parent_ctx->update_btree(m_bbt_updates);

	// add pending blocks to parents write queue
	m_parent_ctx->add_to_block_write_queue(m_data_block_queue);
	m_parent_ctx->add_to_block_write_queue(m_subnode_block_queue);

	// new check point for nbt/bbt
	m_bt_start.first = m_nbt_root;
	m_bt_start.second = m_bbt_root;

	m_nbt_updates.clear();
	m_bbt_updates.clear();
	m_data_block_queue.clear();
	m_subnode_block_queue.clear();

	lock.release_lock();
}

template<typename T>
inline void pstsdk::database_impl<T>::discard_changes()
{
	m_nbt_root = m_bt_start.first;
	m_bbt_root = m_bt_start.second;

	m_nbt_updates.clear();
	m_bbt_updates.clear();
	m_data_block_queue.clear();
	m_subnode_block_queue.clear();
}

template<typename T>
inline bool pstsdk::database_impl<T>::is_ok_to_commit()
{
	// ensure NBT
	read_nbt_root();

	std::tr1::shared_ptr<pstsdk::nbt_page> parent_nbt_root = m_parent_ctx->read_nbt_root();

	for(size_t ind = 0; ind < m_nbt_updates.size(); ++ind)
	{
		pstsdk::nbt_update_action updt_action = m_nbt_updates[ind];

		switch(updt_action.action)
		{
		case pstsdk::bt_insert:
			{
				try	{ pstsdk::node_info parent_nd_info = parent_nbt_root->lookup(updt_action.nd_id); return false; }
				catch(pstsdk::key_not_found<node_id>) { }
			}
			continue;

		case pstsdk::bt_modify:
			{
				try
				{ 
					pstsdk::node_info ctx_nd_info = m_bt_start.first->lookup(updt_action.nd_id);
					pstsdk::node_info parent_nd_info = parent_nbt_root->lookup(updt_action.nd_id);
					if(ctx_nd_info.data_bid != parent_nd_info.data_bid || ctx_nd_info.sub_bid != parent_nd_info.sub_bid || ctx_nd_info.parent_id != parent_nd_info.parent_id)
						return false;
				}
				catch(pstsdk::key_not_found<node_id>) { return false; }
			}
			continue;

		case pstsdk::bt_remove:
			{
				try	{ pstsdk::node_info parent_nd_info = parent_nbt_root->lookup(updt_action.nd_id); }
				catch(pstsdk::key_not_found<node_id>) { return false; }
			}
			continue;
		}
	}

	return true;
}

template<typename T>
inline void pstsdk::database_impl<T>::add_ref_context()
{
	pstsdk::thread_lock lock(&m_db_lock);
	lock.aquire_lock();
	m_ctx_ref++;
	lock.release_lock();

	if(m_parent_ctx) m_parent_ctx->add_ref_context();
}

template<typename T>
inline void pstsdk::database_impl<T>::release_context()
{
	pstsdk::thread_lock lock(&m_db_lock);
	lock.aquire_lock();
	m_ctx_ref--;
	lock.release_lock();

	if(m_parent_ctx) m_parent_ctx->release_context();
}

template<typename T>
inline void pstsdk::database_impl<T>::do_cleanup()
{
	release_context();
}

template<typename T>
inline void pstsdk::database_impl<T>::add_to_block_write_queue(std::map<block_id, std::tr1::shared_ptr<data_block>>& data_block_queue)
{
	pstsdk::thread_lock lock(&m_db_lock);
	lock.aquire_lock();
	m_data_block_queue.insert(data_block_queue.begin(), data_block_queue.end());
	lock.release_lock();
}

template<typename T>
inline void pstsdk::database_impl<T>::add_to_block_write_queue(std::map<block_id, std::tr1::shared_ptr<subnode_block>>& subnode_block_queue)
{
	pstsdk::thread_lock lock(&m_db_lock);
	lock.aquire_lock();
	m_subnode_block_queue.insert(subnode_block_queue.begin(), subnode_block_queue.end());
	lock.release_lock();
}

template<typename T>
inline pstsdk::nbt_update_action pstsdk::database_impl<T>::create_nbt_update_action(pstsdk::node_info& new_nd_info, bool del)
{
	bool exists = false;
	pstsdk::nbt_update_action action_item;
	pstsdk::node_info prev_nd_info = {0, 0, 0, 0};

	read_nbt_root();

	try
	{
		prev_nd_info = m_nbt_root->lookup(new_nd_info.id);
		exists = true;
	}
	catch(pstsdk::key_not_found<node_id>) { exists = false; }

	if(exists)
	{
		// update existing node
		del ? action_item.action = pstsdk::bt_remove : action_item.action = pstsdk::bt_modify;
		action_item.nd_id = prev_nd_info.id;
		action_item.nd_inf = new_nd_info;
	}
	else
	{
		assert(!del);

		// add new node
		action_item.action = pstsdk::bt_insert;
		action_item.nd_id = new_nd_info.id;
		action_item.nd_inf = new_nd_info;
	}

	return action_item;
}

template<typename T>
inline pstsdk::bbt_update_action pstsdk::database_impl<T>::create_bbt_update_action(pstsdk::block_info& new_blk_inf, bool del)
{
	bool exists = false;
	pstsdk::bbt_update_action action_item;
	pstsdk::block_info prev_blk_info = {0, 0, 0, 0};

	read_bbt_root();

	try
	{
		prev_blk_info = m_bbt_root->lookup(new_blk_inf.id);
		exists = true;
	}
	catch(pstsdk::key_not_found<block_id>) { exists = false; }

	if(exists)
	{
		// update ref of existing block.
		del ? prev_blk_info.ref_count-- : prev_blk_info.ref_count++;
		action_item.action = pstsdk::bt_modify;
		action_item.blk_id = prev_blk_info.id;
		action_item.blk_inf = prev_blk_info;
	}
	else
	{
		assert(!del);

		// add new block
		action_item.action = pstsdk::bt_insert;
		action_item.blk_id = new_blk_inf.id;
		action_item.blk_inf = new_blk_inf;
	}

	return action_item;
}

template<typename T>
inline void pstsdk::database_impl<T>::lock_db()
{
	if(m_parent_ctx)
	{
		// if child context then aquire a local lock else lock db with global lock
		pstsdk::thread_lock lock(&m_db_lock, false);
		lock.aquire_lock();
	}
	else
	{
		pstsdk::thread_lock lock(false);
		lock.aquire_lock();
	}
}

template<typename T>
inline void pstsdk::database_impl<T>::unlock_db()
{
	// release previously aquired lock
	if(m_parent_ctx)
	{
		pstsdk::thread_lock lock(&m_db_lock, false);
		lock.release_lock();
	}
	else
	{
		pstsdk::thread_lock lock(false);
		lock.release_lock();
	}
}

template<typename T>
inline bool pstsdk::database_impl<T>::node_exists(node_id nid)
{
	try
	{
		lookup_node_info(nid);
		return true;
	}
	catch(key_not_found<node_id>)
	{
		return false;
	}
}

template<typename T>
inline bool pstsdk::database_impl<T>::block_exists(block_id bid)
{
	try
	{
		lookup_block_info(bid);
		return true;
	}
	catch(key_not_found<block_id>)
	{
		return false;
	}
}

//! \endcond

#endif