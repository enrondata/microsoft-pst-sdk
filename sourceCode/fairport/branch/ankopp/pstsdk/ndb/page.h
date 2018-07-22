//! \file
//! \brief Page definitions
//! \author Terry Mahaffey
//!
//! A page is 512 bytes of metadata contained in a PST file. Pages come in
//! two general flavors:
//! - BTree Pages (immutable)
//! - Allocation Related Pages (mutable)
//!
//! The former are pages which collectively form the BBT and NBT. They are
//! immutable. The later exist at predefined locations in the file, and are
//! always modified in place.
//! \ingroup ndb

#ifndef PSTSDK_NDB_PAGE_H
#define PSTSDK_NDB_PAGE_H

#include <vector>
#include <set>
#include <map>

#include "pstsdk/util/btree.h"
#include "pstsdk/util/util.h"
#include "pstsdk/ndb/database_iface.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4250)
#endif

namespace pstsdk
{

	//! \defgroup ndb_pagerelated Pages
	//! \ingroup ndb

	//! \brief Generic base class for all page types
	//!
	//! This class provides an abstraction around the page trailer located at the
	//! end of every page. The actual page content is interpretted by the child
	//! classes of \ref page.
	//!
	//! All pages in the pages hierarchy are also in the category of what is known
	//! as <i>dependant objects</i>. This means is they only keep a weak 
	//! reference to the database context to which they're a member. Contrast this
	//! to an independant object such as the \ref heap, which keeps a strong ref
	//! or a full shared_ptr to the related context. This implies that someone
	//! must externally make sure the database context outlives it's pages -
	//! this is usually done by the database context itself.
	//! \sa [MS-PST] 2.2.2.7
	//! \ingroup ndb_pagerelated
	class page
	{
	public:
		//! \brief Construct a page from disk
		//! \param[in] db The database context
		//! \param[in] pi Information about this page
		page(const shared_db_ptr& db, const page_info& pi)
			: m_modified(false), m_db(db), m_pid(pi.id), m_address(pi.address) { }

		page(const page& other)
			: m_modified(false), m_db(other.m_db), m_pid(0), m_address(0) { }

		virtual ~page() { }

		//! \brief Get the page id
		//! \returns The page id
		page_id get_page_id() const { return m_pid; }

		//! \brief Get the physical address of this page
		//! \returns The address of this page, or 0 for a new page
		ulonglong get_address() const { return m_address; }

		//! \brief Set the physical address of this page
		//! \param[in] address The physical address for this page
		void set_address(ulonglong address) { m_address = address; }

		//! \brief Mark page as modified
		void touch();

		//! \brief Check if page is modified
		bool is_modified() { return m_modified; }

	protected:
		shared_db_ptr get_db_ptr() const { return shared_db_ptr(m_db); }
		bool m_modified;        //!< Tells if the page is modified or not
		weak_db_ptr m_db;       //!< The database context we're a member of
		page_id m_pid;          //!< Page id
		ulonglong m_address;    //!< Address of this page
		mutable lock_var m_page_lock;
	};

	//!< \brief A page which forms a node in the NBT or BBT
	//!
	//! The NBT and BBT form the core of the internals of the PST, and need to be
	//! well understood if working at the \ref ndb. The bt_page class forms the
	//! nodes of both the NBT and BBT, with child classes for leaf and nonleaf
	//! pages.
	//!
	//! This hierarchy also models the \ref btree_node structure, inheriting the 
	//! actual iteration and lookup logic.
	//! \tparam K key type
	//! \tparam V value type
	//! \sa [MS-PST] 1.3.1.1
	//! \sa [MS-PST] 2.2.2.7.7
	//! \ingroup ndb_pagerelated
	template<typename K, typename V>
	class bt_page : 
		public page, 
		public virtual btree_node<K,V>
	{
	public:
		//! \brief Construct a bt_page from disk
		//! \param[in] db The database context
		//! \param[in] pi Information about this page
		//! \param[in] level 0 for a leaf, or distance from leaf
		bt_page(const shared_db_ptr& db, const page_info& pi, ushort level, size_t max_entries)
			: page(db, pi), m_level(level), m_max_entries(max_entries) { }

		virtual ~bt_page() { }

		//! \cond write_api
		//! \brief Insert a new element into the page
		//! \param[in] key The key of new element to be inserted
		//! \param[in] val The value of new element to be inserted
		//! \returns A pair of new pages with the new element inserted
		virtual std::pair<std::tr1::shared_ptr<bt_page<K,V>>, std::tr1::shared_ptr<bt_page<K,V>>> insert(const K& key, const V& val) = 0;

		//! \brief Modify an existing element
		//! \param[in] key The key of element to be modified
		//! \param[in] val The new value of the element to be modified
		//! \returns A new page with the specified element modified
		virtual std::tr1::shared_ptr<bt_page<K,V>> modify(const K& key, const V& val) = 0;

		//! \brief Delete an existing element
		//! \param[in] key The key of element to be removed
		//! \returns A new page with the specified element removed
		virtual std::tr1::shared_ptr<bt_page<K,V>> remove(const K& key) = 0;
		//! \endcond

		//! \brief Return the level of this bt_page
		//! \returns The level of this page
		ushort get_level() const { return m_level; }

		//! \brief Return the maximum number of entries this bt_page can hold
		//! \return The maximum number of entries this page can hold
		size_t get_max_entries() const { return m_max_entries; }

	private:
		ushort m_level;         //!< Our level
		size_t m_max_entries;   //!< Maximum number of entries page can hold
	};

	//! \brief Contains references to other bt_pages
	//!
	//! A bt_nonleaf_page makes up the body of the NBT and BBT (which differ only
	//! at the leaf). 
	//! \tparam K key type
	//! \tparam V value type
	//! \sa [MS-PST] 2.2.2.7.7.2
	//! \ingroup ndb_pagerelated
	template<typename K, typename V>
	class bt_nonleaf_page : 
		public bt_page<K,V>, 
		public btree_node_nonleaf<K,V>, 
		public std::tr1::enable_shared_from_this<bt_nonleaf_page<K,V>>
	{
	public:
		//! \brief Construct a bt_nonleaf_page from disk
		//! \param[in] db The database context
		//! \param[in] pi Information about this page
		//! \param[in] level Distance from leaf
		//! \param[in] subpi Information about the child pages
#ifndef BOOST_NO_RVALUE_REFERENCES
		bt_nonleaf_page(const shared_db_ptr& db, const page_info& pi, ushort level, std::vector<std::pair<K, page_info>> subpi, size_t max_entries)
			: bt_page<K,V>(db, pi, level, max_entries), m_page_info(std::move(subpi)), m_child_pages(m_page_info.size()) { }
		bt_nonleaf_page(const shared_db_ptr& db, ushort level, std::vector<std::pair<K, page_info>> subpi, size_t max_entries)
			: bt_page<K,V>(db, page_info(), level, max_entries), m_page_info(std::move(subpi)), m_child_pages(m_page_info.size()) { touch(); }
		bt_nonleaf_page(const shared_db_ptr& db, ushort level, std::vector<std::pair<K, page_info>> subpi, std::vector<std::tr1::shared_ptr<bt_page<K,V>>> child_pages, size_t max_entries)
			: bt_page<K,V>(db, page_info(), level, max_entries), m_page_info(std::move(subpi)), m_child_pages(std::move(child_pages)) { touch(); }
#else
		bt_nonleaf_page(const shared_db_ptr& db, const page_info& pi, ushort level, const std::vector<std::pair<K, page_info>>& subpi, size_t max_entries)
			: bt_page<K,V>(db, pi, level, max_entries), m_page_info(subpi), m_child_pages(m_page_info.size()) { }
		bt_nonleaf_page(const shared_db_ptr& db, ushort level, const std::vector<std::pair<K, page_info>> subpi, size_t max_entries)
			: bt_page<K,V>(db, page_info(), level, max_entries), m_page_info(subpi), m_child_pages(m_page_info.size()) { touch(); }
		bt_nonleaf_page(const shared_db_ptr& db, ushort level, std::vector<std::pair<K, page_info>> subpi, std::vector<std::tr1::shared_ptr<bt_page<K,V>>> child_pages, size_t max_entries)
			: bt_page<K,V>(db, page_info(), level, max_entries), m_page_info(subpi), m_child_pages(child_pages) { touch(); }
#endif

		bt_nonleaf_page(const bt_nonleaf_page& other)
			: bt_page<K,V>(other.get_db_ptr(), page_info(), other.get_level(), other.get_max_entries()), m_page_info(other.m_page_info),
			m_child_pages(other.m_child_pages) { touch(); }

		//! \cond write_api
		//! \brief Insert a new element into the page
		//! \param[in] key The key of new element to be inserted
		//! \param[in] val The value of new element to be inserted
		//! \returns A pair of new pages with the new element inserted
		virtual std::pair<std::tr1::shared_ptr<bt_page>, std::tr1::shared_ptr<bt_page<K,V>>> insert(const K& key, const V& val);
		
		//! \brief Modify an existing element
		//! \param[in] key The key of element to be modified
		//! \param[in] val The new value of the element to be modified
		//! \returns A new page with the specified element modified
		virtual std::tr1::shared_ptr<bt_page<K,V>> modify(const K& key, const V& val);
		
		//! \brief Delete an existing element
		//! \param[in] key The key of element to be removed
		//! \returns A new page with the specified element removed
		virtual std::tr1::shared_ptr<bt_page<K,V>> remove(const K& key);
		//! \endcond

		//! \brief Get key of element stored at particular position in the page
		//! \param[in] pos The position from which key of element to be retrieved
		//! \returns The key of element stored at given position
		const K& get_key(uint pos) const { return m_page_info[pos].first; }
		
		//! \brief Get child page info stored at particular position in the page
		//! \param[in] pos The position from which child page info is to be retrieved
		//! \returns The page info at given position
		const page_info& get_child_page_info(uint pos) const { return m_page_info[pos].second; }
		
		//! \brief Store child page info at particular position in the page
		//! \param[in] pos The position at which child page info is to be stored
		void set_page_info(uint pos, page_info& pi);
		
		//! \brief Get child page linked at particular position in the page
		//! \param[in] pos The position from which page info can be retrieved to read the child page from disk
		//! \returns The child page object
		bt_page<K,V>* get_child(uint pos);
		
		//! \brief Get child page linked at particular position in the page
		//! \param[in] pos The position from which page info can be retrieved to read the child page from disk
		//! \returns The child page const object
		const bt_page<K,V>* get_child(uint pos) const;
		
		//! \brief Get the number of children held by the page
		//! \returns The number of children this page has
		uint num_values() const { return m_child_pages.size(); }

		//! \brief Get child page linked at particular position in the page
		//! \param[in] pos The position from which page info can be retrieved to read the child page from disk
		//! \param[in] dummy Ignored
		//! \returns The child page shared_ptr object
		std::tr1::shared_ptr<bt_page<K,V>> get_child(uint pos, bool dummy);

	private:
		std::vector<std::pair<K, page_info>> m_page_info;   //!< Information about the child pages
		mutable std::vector<std::tr1::shared_ptr<bt_page<K,V>>> m_child_pages; //!< Cached child pages
	};

	//! \brief Contains the actual key value pairs of the btree
	//! \tparam K key type
	//! \tparam V value type
	//! \ingroup ndb_pagerelated
	template<typename K, typename V>
	class bt_leaf_page : 
		public bt_page<K,V>, 
		public btree_node_leaf<K,V>, 
		public std::tr1::enable_shared_from_this<bt_leaf_page<K,V>>
	{
	public:
		//! \brief Construct a leaf page from disk
		//! \param[in] db The database context
		//! \param[in] pi Information about this page
		//! \param[in] data The key/value pairs on this leaf page
#ifndef BOOST_NO_RVALUE_REFERENCES
		bt_leaf_page(const shared_db_ptr& db, const page_info& pi, std::vector<std::pair<K,V>> data, size_t max_entries)
			: bt_page<K,V>(db, pi, 0, max_entries), m_page_data(std::move(data)) { }
		bt_leaf_page(const shared_db_ptr& db, std::vector<std::pair<K,V>> data, size_t max_entries)
			: bt_page<K,V>(db, page_info(), 0, max_entries), m_page_data(std::move(data)) { touch(); }
#else
		bt_leaf_page(const shared_db_ptr& db, const page_info& pi, const std::vector<std::pair<K,V>>& data, size_t max_entries)
			: bt_page<K,V>(db, pi, 0, max_entries), m_page_data(data) { }
		bt_leaf_page(const shared_db_ptr& db, const std::vector<std::pair<K,V>> data, size_t max_entries)
			: bt_page<K,V>(db, page_info(), 0, max_entries), m_page_data(data) { touch(); }
#endif
		bt_leaf_page(const bt_leaf_page& other)
			: bt_page<K,V>(other.get_db_ptr(), page_info(), 0, other.get_max_entries()), m_page_data(other.m_page_data) { touch(); }

		//! \cond write_api
		//! \brief Insert a new element into the page
		//! \param[in] key The key of new element to be inserted
		//! \param[in] val The value of new element to be inserted
		//! \returns A pair of new pages with the new element inserted
		virtual std::pair<std::tr1::shared_ptr<bt_page<K,V>>, std::tr1::shared_ptr<bt_page<K,V>>> insert(const K& key, const V& val);
		
		//! \brief Modify an existing element
		//! \param[in] key The key of element to be modified
		//! \param[in] val The new value of the element to be modified
		//! \returns A new page with the specified element modified
		virtual std::tr1::shared_ptr<bt_page<K,V>> modify(const K& key, const V& val);
		
		//! \brief Delete an existing element
		//! \param[in] key The key of element to be removed
		//! \returns A new page with the specified element removed
		virtual std::tr1::shared_ptr<bt_page<K,V>> remove(const K& key);
		//! \endcond

		//! \brief Get value of element stored at particular position in the page
		//! \param[in] pos The position from which value of element to be retrieved
		//! \returns The value of element stored at given position
		const V& get_value(uint pos) const { return m_page_data[pos].second; }
		
		//! \brief Get key of element stored at particular position in the page
		//! \param[in] pos The position from which key of element to be retrieved
		//! \returns The key of element stored at given position
		const K& get_key(uint pos) const { return m_page_data[pos].first; }
		
		//! \brief Get the number of elements stored in the page
		//! \returns The number of elements this page has
		uint num_values() const { return m_page_data.size(); }

	private:
		std::vector<std::pair<K,V>> m_page_data; //!< The key/value pairs on this leaf page
	};

	//! \cond write_api
	//!< \brief A page which is source of free space in the file
	//!
	//! Each bit in an AMap page refers to \ref bytes_per_slot bytes of the file.
	//! If that bit is set, that indicates those bytes in the file are occupied.
	//! If the bit is not set, that indicates those bytes in the file are available
	//! for allocation. Note that each AMap page "maps" itself. Since an AMap page
	//! (like all pages) is \ref page_size bytes (512), this means the first 8 bytes
	//! of an AMap page are by definition always 0xFF.
	//! \sa [MS-PST] 1.3.2.4
	//! \sa [MS-PST] 2.2.2.7.2.1
	//! \ingroup ndb_pagerelated
	class amap_page : public page
	{
	public:
		//! \brief Construct an amap page
		//! \param[in] db The database context
		//! \param[in] pi The page info to read from disk
		amap_page(const shared_db_ptr& db, const page_info& pi)
			:page(db, pi), m_total_free_slots(0) { build_page_metadata(true);; }

		//! \brief Construct an amap page
		//! \param[in] db The database context
		//! \param[in] pi The page info to read from disk
		//! \param[in] data The page data
		amap_page(const shared_db_ptr& db, const page_info& pi, std::vector<byte> data)
			:page(db, pi), m_page_data(std::move(data)), m_total_free_slots(0) { build_page_metadata(false); }

		//! \brief Attempt to allocate space through amap page
		//! \param[in] size The size of block in bytes to allocate out of amap
		//! \param[in] align Flag depicting if allocation address is to be 512 byte aligned
		//! \returns The address where space is allocated
		ulonglong allocate_space(size_t size, bool align);

		//! \brief Mark bits corresponding to specific location as allocated
		//! \param[in] location The adrress which is to be marked as allocated
		//! \param[in] size The number of bytes to be marked as allocated
		void mark_location_allocated(ulonglong location, size_t size);

		//! \brief Mark bits corresponding to specific location as free
		//! \param[in] location The adrress which is to be marked as free
		//! \param[in] size The number of bytes to be marked as free
		void free_allocated_space(ulonglong location, size_t size);

		//! \brief Check if given address is allocated
		//! \param[in] location The location which is to be checked
		//! \param[in] size The number of bytes to be checked
		//! \returns True if given bytes at location are marked as allocated
		bool is_location_allocated(ulonglong location, size_t size);

		//! \brief Get page data
		//! \returns The page data
		std::vector<byte> get_page_data() { return m_page_data; }

		//! \brief Get total free slots available in amap page
		//! \returns The total number of free slots available in amap page
		size_t get_total_free_slots() { return m_total_free_slots; }

		//! \brief Get total free space available through amap page
		//! \returns The total number of free bytes available through amap page
		size_t get_total_free_space() { return m_total_free_slots * disk::bytes_per_slot; }


	protected:
		//! \brief Calculate number of bits to change for given size
		size_t get_reqd_bits(size_t size);

		//! \brief Find whether given bit is set or reset
		bool get_bit_state(size_t bit_index);

		//! \brief Find whether a range of bits are set
		bool are_bits_set(size_t start_bit_index, size_t count);

		//! \brief Set a single bit to 1
		void set_bit(size_t bit_index);

		//! \brief Set a single bit to 0
		void reset_bit(size_t bit_index);

		//! \brief Set a range of bits to 1
		void set_bits(size_t start_bit_index, size_t count);

		//! \brief Set a range of bits to 0
		void reset_bits(size_t start_bit_index, size_t count);

		//! \brief Calculate bit index based on given absolute location
		size_t get_bit_index(ulonglong location);

		//! \brief Calculate byte index based on given bit index
		size_t get_byte_index(size_t bit_index) { return bit_index / disk::bits_per_byte; }

		//! \brief Build all the meta data required by the page
		void build_page_metadata(bool is_empty);


	protected:
		std::vector<byte> m_page_data;
		size_t m_total_free_slots;
	};

	//!< \brief A metapage holding information about AMap pages
	//!
	//! The Density List page contains a list of AMap pages in the file, in ascending
	//! order of density. That is to say, the "emptiest" page is at the top. This is the
	//! data backing the new allocation scheme which replaced the old pmap/fmap/fpmap
	//! scheme, starting in Outlook 2007 SP2.
	//! \sa [MS-PST] 1.3.2.3
	//! \sa [MS-PST] 2.2.2.7.4.1
	//! \ingroup ndb_pagerelated
	class dlist_page: public page
	{
	public:
		//! \brief Construct an amap page
		//! \param[in] db The database context
		//! \param[in] pi The page info to read from disk
		dlist_page(const shared_db_ptr& db, const page_info& pi)
			: page(db, pi), m_flags(0), m_current_page(0) {	}

		//! \brief Construct an amap page
		//! \param[in] db The database context
		//! \param[in] pi The page info to read from disk
		//! \param[in] flags The flag values
		//! \param[in] current_page The current amap page to allocate out of
		dlist_page(const shared_db_ptr& db, const page_info& pi, byte flags, ulong current_page,
			std::vector<ulong> page_entries): page(db, pi), m_flags(flags),
			m_current_page(current_page) { build_page_entries(page_entries); }

		//! \brief Add page number and number of free slots entry to dlist
		//! \param[in] page_num The amap page index
		//! \param[in] free_space The amount of free space available in ampa page
		void add_page_entry(size_t page_num, size_t free_space);

		//! \brief Get amap page index from which to allocate memory
		//! \param[in] attempt_no The number of attemps made to find enough free space from list of amap pages held in DLIST
		//! \returns The amap page number
		size_t get_page_number(size_t attempt_no = 1);

		//! \brief Get list of all dlist entries
		//! \returns The list of all dlist entries in the page
		void get_entries(std::vector<ulong>& entries);

		//! \brief Get flag values from dlist page
		//! \returns The flag values
		byte get_flags() { return m_flags; }

		//! \brief Get the current amap page index from which allocations can be made
		//! \returns The amap page index
		ulong get_current_page() { return m_current_page; }

	protected:
		//! \brief Build DLISTPAGEENT out of page num and free slots values
		ulong build_entry(ulong page_num, ulong free_space);

		//! \brief Build internal data structures of dlist entries from page entries
		void build_page_entries(std::vector<ulong>& entries);

	protected:
		byte m_flags; 
		ulong m_current_page;
		std::multimap<size_t, size_t> m_space_page_map;
		std::map<size_t, std::multimap<size_t, size_t>::iterator> m_page_space_map;
	};

	//!< \brief A metapage holding information about PMap pages
	//!
	//! A Page Map is a block of data that is 512 bytes in size (including overhead),
	//! which is used for storing almost all of the metadata in the PST (that is, the BBT and NBT).
	//! The PMap is created to optimize for the search of available pages. The PMap is almost identical to the AMap,
	//! except that each bit in the PMap maps the allocation state of 512 bytes rather than instead of 64 because
	//! each bit in the PMap covers eight times the data of an AMap, a PMap page appears roughly every 2MB
	//! (or one PMap for every eight AMaps).
	//! 
	//! The functionality of the PMap has been deprecated by the Density List.
	//! If a Density List is present in the PST file, then implementations SHOULD NOT use the PMap to locate free pages,
	//! and SHOULD instead use the Density List instead. However, implementations MUST ensure the presence of PMaps at the correct
	//! intervals and maintain valid checksums to ensure backward-compatibility with older clients.
	//! \sa [MS-PST] 1.3.2.5
	//! \sa [MS-PST] 2.2.2.7.3
	//! \ingroup ndb_pagerelated
	class pmap_page: public page
	{
	public:
		//! \brief Construct a pmap page
		//! \param[in] db The database context
		//! \param[in] pi The page info to read from disk
		pmap_page(const shared_db_ptr& db, const page_info& pi)
			: page(db, pi) { m_page_data.assign(disk::max_map_bytes, 0xFF); }

		//! \brief Construct a pmap page
		//! \param[in] db The database context
		//! \param[in] pi The page info to read from disk
		//! \param[in] data The page data
		pmap_page(const shared_db_ptr& db, const page_info& pi, std::vector<byte> data)
			: page(db, pi), m_page_data(std::move(data)) {	}

		//! \brief Get page data
		//! \returns The pmap page data
		std::vector<byte> get_page_data() { return m_page_data; }

	protected:
		std::vector<byte> m_page_data;
	};

	//!< \brief A metapage holding information about FMap pages
	//! 
	//! An FMap page provides a mechanism to quickly locate contiguous free space.
	//! Each byte in the FMap corresponds to one AMap page. The value of each byte indicates the longest number of free bits
	//! found in the corresponding AMap page. Because each bit in the AMap maps to 64 bytes, 
	//! the FMap contains the maximum amount of contiguous free space in that AMap, up to about 16KB. 
	//! Generally, because each AMap covers about 250KB of data, each FMap page (496 bytes) covers around 125MB of data.
	//! 
	//! Implementations SHOULD NOT use FMaps. The Density List SHOULD be used for location free space.
	//! However, the presence of FMap pages at the correct intervals MUST be preserved, and all corresponding checksums MUST
	//! be maintained for a PST file to remain valid.
	//! \sa [MS-PST] 1.3.2.7
	//! \sa [MS-PST] 2.2.2.7.5
	//! \ingroup ndb_pagerelated
	class fmap_page: public page
	{
	public:
		//! \brief Construct a fmap page
		//! \param[in] db The database context
		//! \param[in] pi The page info to read from disk
		fmap_page(const shared_db_ptr& db, const page_info& pi)
			: page(db, pi) { m_page_data.assign(disk::max_map_bytes, 0x00); }

		//! \brief Construct a fmap page
		//! \param[in] db The database context
		//! \param[in] pi The page info to read from disk
		//! \param[in] data The page data
		fmap_page(const shared_db_ptr& db, const page_info& pi, std::vector<byte> data)
			: page(db, pi), m_page_data(std::move(data)) {	}

		//! \brief Get page data
		//! \returns The fmap page data
		std::vector<byte> get_page_data() { return m_page_data; }

	protected:
		std::vector<byte> m_page_data;
	};

	//!< \brief A metapage holding information about FPMap pages
	//!
	//! An FPMap is similar to the FMap except that it is used to quickly find free pages.
	//! Each bit in the FPMap corresponds to a PMap page, and the value of the bit indicates whether
	//! there are any free pages within that PMap page. With each PMap covering about 2MB, and an FPMap page
	//! at 496 bytes, it follows that an FPMap page covers about 8GB of space.
	//! 
	//! Implementations SHOULD NOT use FPMaps. The Density List SHOULD be used for location free space.
	//! However, the presence of FPMap pages at the correct intervals MUST be preserved, and all corresponding checksums MUST
	//! be maintained for a PST file to remain valid.
	//! \sa [MS-PST] 1.3.2.8
	//! \sa [MS-PST] 2.2.2.7.6
	//! \ingroup ndb_pagerelated
	class fpmap_page: public page
	{
	public:
		//! \brief Construct a fpmap page
		//! \param[in] db The database context
		//! \param[in] pi The page info to read from disk
		fpmap_page(const shared_db_ptr& db, const page_info& pi)
			: page(db, pi) { m_page_data.assign(disk::max_map_bytes, 0xFF); }

		//! \brief Construct a fpmap page
		//! \param[in] db The database context
		//! \param[in] pi The page info to read from disk
		//! \param[in] data The page data
		fpmap_page(const shared_db_ptr& db, const page_info& pi, std::vector<byte> data)
			: page(db, pi), m_page_data(std::move(data)) {	}

		//! \brief Get page data
		//! \returns The fpmap page data
		std::vector<byte> get_page_data() { return m_page_data; }

	protected:
		std::vector<byte> m_page_data;
	};
	//! \endcond

} // end namespace


inline void pstsdk::page::touch()
{
	pstsdk::thread_lock lock(&m_page_lock);
	lock.aquire_lock();

	if(!m_modified)
	{
		m_modified = true;
		m_address = 0;
		m_pid = get_db_ptr()->alloc_pid();
	}

	lock.release_lock();
}

//! \cond dont_show_these_member_function_specializations
template<>
inline std::tr1::shared_ptr<pstsdk::bt_page<pstsdk::node_id, pstsdk::node_info>> pstsdk::bt_nonleaf_page<pstsdk::node_id, pstsdk::node_info>::get_child(uint pos, bool dummy)
{
	pstsdk::thread_lock lock(&m_page_lock);
	lock.aquire_lock();

	if(m_child_pages[pos] == NULL)
	{
		m_child_pages[pos] = this->get_db_ptr()->read_nbt_page(m_page_info[pos].second);
	}

	lock.release_lock();

	return m_child_pages[pos];
}

template<>
inline std::tr1::shared_ptr<pstsdk::bt_page<pstsdk::block_id, pstsdk::block_info>> pstsdk::bt_nonleaf_page<pstsdk::block_id, pstsdk::block_info>::get_child(uint pos, bool dummy)
{
	pstsdk::thread_lock lock(&m_page_lock);
	lock.aquire_lock();

	if(m_child_pages[pos] == NULL)
	{
		m_child_pages[pos] = this->get_db_ptr()->read_bbt_page(m_page_info[pos].second);
	}

	lock.release_lock();

	return m_child_pages[pos];
}

template<>
inline pstsdk::bt_page<pstsdk::block_id, pstsdk::block_info>* pstsdk::bt_nonleaf_page<pstsdk::block_id, pstsdk::block_info>::get_child(uint pos)
{
	pstsdk::thread_lock lock(&m_page_lock);
	lock.aquire_lock();

	if(m_child_pages[pos] == NULL)
	{
		m_child_pages[pos] = this->get_db_ptr()->read_bbt_page(m_page_info[pos].second);
	}

	lock.release_lock();

	return m_child_pages[pos].get();
}

template<>
inline const pstsdk::bt_page<pstsdk::block_id, pstsdk::block_info>* pstsdk::bt_nonleaf_page<pstsdk::block_id, pstsdk::block_info>::get_child(uint pos) const
{
	pstsdk::thread_lock lock(&m_page_lock);
	lock.aquire_lock();

	if(m_child_pages[pos] == NULL)
	{
		m_child_pages[pos] = this->get_db_ptr()->read_bbt_page(m_page_info[pos].second);
	}

	lock.release_lock();

	return m_child_pages[pos].get();
}

template<>
inline pstsdk::bt_page<pstsdk::node_id, pstsdk::node_info>* pstsdk::bt_nonleaf_page<pstsdk::node_id, pstsdk::node_info>::get_child(uint pos)
{
	pstsdk::thread_lock lock(&m_page_lock);
	lock.aquire_lock();

	if(m_child_pages[pos] == NULL)
	{
		m_child_pages[pos] = this->get_db_ptr()->read_nbt_page(m_page_info[pos].second); 
	}

	lock.release_lock();

	return m_child_pages[pos].get();
}

template<>
inline const pstsdk::bt_page<pstsdk::node_id, pstsdk::node_info>* pstsdk::bt_nonleaf_page<pstsdk::node_id, pstsdk::node_info>::get_child(uint pos) const
{
	pstsdk::thread_lock lock(&m_page_lock);
	lock.aquire_lock();

	if(m_child_pages[pos] == NULL)
	{
		m_child_pages[pos] = this->get_db_ptr()->read_nbt_page(m_page_info[pos].second); 
	}

	lock.release_lock();

	return m_child_pages[pos].get();
}
//! \endcond

//! \cond write_api
template<typename K, typename V>
inline std::pair<std::tr1::shared_ptr<pstsdk::bt_page<K,V>>, std::tr1::shared_ptr<pstsdk::bt_page<K,V>>> pstsdk::bt_nonleaf_page<K, V>::insert(const K& key, const V& val)
{
	std::tr1::shared_ptr<pstsdk::bt_nonleaf_page<K, V>> copiedPage1 = shared_from_this();
	if(copiedPage1.use_count() > 2)
	{
		std::tr1::shared_ptr<pstsdk::bt_nonleaf_page<K, V>> cnewPage = std::tr1::make_shared<pstsdk::bt_nonleaf_page<K, V>>(*this);
		return cnewPage->insert(key, val);
	}

	touch(); // mutate ourselves inplace

	std::tr1::shared_ptr<pstsdk::bt_nonleaf_page<K, V>> copiedPage2 (0);

	int pos = this->binary_search(key);
	if(pos == -1)
	{
		pos = 0;
	}

	std::pair<std::tr1::shared_ptr<bt_page<K,V>>, std::tr1::shared_ptr<bt_page<K,V>>> result (this->get_child(pos)->insert(key, val));

	pstsdk::page_info pi;
	pi.id = result.first->get_page_id();
	pi.address = result.first->get_address();
	this->m_page_info[pos].first = result.first->get_key(0);
	this->m_page_info[pos].second = pi;
	this->m_child_pages[pos] = result.first;

	if(result.second.get() != 0)
	{
		pi.id = result.second->get_page_id();
		pi.address = result.second->get_address();
		this->m_page_info.insert(this->m_page_info.begin() + pos + 1, std::make_pair(result.second->get_key(0), pi));
		this->m_child_pages.insert(this->m_child_pages.begin() + pos + 1, result.second);

		if(this->m_page_info.size() > this->get_max_entries())
		{
			copiedPage2 = std::tr1::make_shared<bt_nonleaf_page<K, V>>(this->get_db_ptr(), this->get_level(), std::vector<std::pair<K, page_info>> (), this->get_max_entries());
			copiedPage2->m_page_info.push_back(this->m_page_info.back());
			copiedPage2->m_child_pages.push_back(this->m_child_pages.back());
			this->m_page_info.pop_back();
			this->m_child_pages.pop_back();
		}
	}

	return std::make_pair(copiedPage1, copiedPage2);
}

template<typename K, typename V>
inline std::tr1::shared_ptr<pstsdk::bt_page<K, V>> pstsdk::bt_nonleaf_page<K, V>::modify(const K& key, const V& val)
{
	std::tr1::shared_ptr<bt_nonleaf_page<K, V>> copiedPage = shared_from_this();
	if(copiedPage.use_count() > 2)
	{
		std::tr1::shared_ptr<bt_nonleaf_page<K, V>> cnewPage = std::tr1::make_shared<bt_nonleaf_page<K, V>>(*this);
		return cnewPage->modify(key, val);
	}

	touch(); // mutate ourselves inplace

	int pos = this->binary_search(key);
	if(pos == -1)
	{
		throw key_not_found<K>(key);
	}

	std::tr1::shared_ptr<bt_page<K, V>> result(this->get_child(pos)->modify(key, val));

	page_info pi;
	pi.id = result->get_page_id();
	pi.address = result->get_address();
	this->m_page_info[pos].first = result->get_key(0);
	this->m_page_info[pos].second = pi;
	this->m_child_pages[pos] = result;

	return copiedPage;
}

template<typename K, typename V>
inline std::tr1::shared_ptr<pstsdk::bt_page<K, V>> pstsdk::bt_nonleaf_page<K, V>::remove(const K& key)
{
	std::tr1::shared_ptr<bt_nonleaf_page<K, V>> copiedPage = shared_from_this();
	if(copiedPage.use_count() > 2)
	{
		std::tr1::shared_ptr<bt_nonleaf_page<K, V>> cnewPage = std::tr1::make_shared<bt_nonleaf_page<K, V>>(*this);
		return cnewPage->remove(key);
	}

	touch(); // mutate ourselves inplace

	int pos = this->binary_search(key);
	if(pos == -1)
	{
		throw key_not_found<K>(key);
	}

	std::tr1::shared_ptr<bt_page<K, V>> result(this->get_child(pos)->remove(key));

	if(result.get() == 0)
	{
		this->m_page_info.erase(this->m_page_info.begin() + pos);
		this->m_child_pages.erase(this->m_child_pages.begin() + pos);
		if(this->m_page_info.size() == 0)
		{
			return std::tr1::shared_ptr<bt_nonleaf_page<K, V>>(0);
		}
	}
	else
	{
		page_info pi;
		pi.id = result->get_page_id();
		pi.address = result->get_address();
		this->m_page_info[pos].first = result->get_key(0);
		this->m_page_info[pos].second = pi;
		this->m_child_pages[pos] = result;
	}

	return copiedPage;
}

template<typename K, typename V>
inline std::pair<std::shared_ptr<pstsdk::bt_page<K,V>>, std::tr1::shared_ptr<pstsdk::bt_page<K,V>>> pstsdk::bt_leaf_page<K, V>::insert(const K& key, const V& val)
{
	std::tr1::shared_ptr<bt_leaf_page<K, V>> copiedPage1 = shared_from_this();
	if(copiedPage1.use_count() > 2)
	{
		std::tr1::shared_ptr<bt_leaf_page<K, V>> cnewPage = std::tr1::make_shared<bt_leaf_page<K, V>>(*this);
		return cnewPage->insert(key, val);
	}

	touch(); // mutate ourselves inplace

	std::tr1::shared_ptr<bt_leaf_page<K, V>> copiedPage2 (0);

	int pos = this->binary_search(key);
	uint idx = pos + 1;

	if((pos > -1) && (static_cast<unsigned int>(pos) < this->m_page_data.size()) && (this->get_key(pos) == key))
	{
		// If key already exists, behave like modify
		this->m_page_data[pos].second = val;
	}
	else
	{
		this->m_page_data.insert(this->m_page_data.begin() + idx, std::make_pair(key, val));

		if(this->m_page_data.size() > this->get_max_entries())
		{
			copiedPage2 = std::tr1::make_shared<bt_leaf_page<K, V>>(this->get_db_ptr(), std::vector<std::pair<K,V>>(), this->get_max_entries());
			copiedPage2->m_page_data.push_back(this->m_page_data.back());
			this->m_page_data.pop_back();
		}
	}

	return std::make_pair(copiedPage1, copiedPage2);
}

template<typename K, typename V>
inline std::tr1::shared_ptr<pstsdk::bt_page<K, V>> pstsdk::bt_leaf_page<K, V>::modify(const K& key, const V& val)
{
	std::tr1::shared_ptr<bt_leaf_page<K, V>> copiedPage = shared_from_this();
	if(copiedPage.use_count() > 2)
	{
		std::tr1::shared_ptr<bt_leaf_page<K, V>> cnewPage = std::tr1::make_shared<bt_leaf_page<K, V>>(*this);
		return cnewPage->modify(key, val);
	}

	touch(); // mutate ourselves inplace

	int pos = this->binary_search(key);
	if(pos == -1)
	{
		throw key_not_found<K>(key);
	}

	if(this->get_key(pos) != key)
	{
		throw key_not_found<K>(key);
	}

	this->m_page_data[pos].second = val;

	return copiedPage;
}

template<typename K, typename V>
inline std::tr1::shared_ptr<pstsdk::bt_page<K, V>> pstsdk::bt_leaf_page<K, V>::remove(const K& key)
{
	std::tr1::shared_ptr<bt_leaf_page<K, V>> copiedPage = shared_from_this();
	if(copiedPage.use_count() > 2)
	{
		std::tr1::shared_ptr<bt_leaf_page<K, V>> cnewPage = std::tr1::make_shared<bt_leaf_page<K, V>>(*this);
		return cnewPage->remove(key);
	}

	touch(); // mutate ourselves inplace

	int pos = this->binary_search(key);
	if(pos == -1)
	{
		throw key_not_found<K>(key);
	}

	if(this->get_key(pos) != key)
	{
		throw key_not_found<K>(key);
	}

	this->m_page_data.erase(this->m_page_data.begin() + pos);

	if(this->num_values() == 0)
	{
		return std::tr1::shared_ptr<bt_leaf_page<K, V>>(0);
	}

	return copiedPage;
}

template<typename K, typename V>
inline void pstsdk::bt_nonleaf_page<K, V>::set_page_info(uint pos, pstsdk::page_info& pi)
{
	pstsdk::thread_lock lock(&m_page_lock);
	lock.aquire_lock();
	m_page_info[pos].second = pi;
	lock.release_lock();
}

inline size_t pstsdk::amap_page::get_reqd_bits(size_t size)
{ 
	return size % disk::bytes_per_slot == 0 ? size / disk::bytes_per_slot : (size / disk::bytes_per_slot) + 1;
}

inline bool pstsdk::amap_page::get_bit_state(size_t bit_index)
{
	return (m_page_data[get_byte_index(bit_index)] & (1 << (7 - (bit_index % disk::bits_per_byte)))) != 0;
}

inline void pstsdk::amap_page::set_bit(size_t bit_index)
{
	m_page_data[get_byte_index(bit_index)] |= (1 << (7 - (bit_index % disk::bits_per_byte)));
}

inline void pstsdk::amap_page::reset_bit(size_t bit_index)
{
	m_page_data[get_byte_index(bit_index)] &= ~(1 << (7 - (bit_index % disk::bits_per_byte)));
}

inline void pstsdk::amap_page::mark_location_allocated(pstsdk::ulonglong location, size_t size)
{
	size_t bit_index = get_bit_index(location);
	size_t reqd_bits = get_reqd_bits(size);

	// check if attempting to free bits from first byte of page
	if(bit_index < 8)
		throw std::invalid_argument("invalid location address");

	size_t max_bit_index = bit_index + (size / disk::bytes_per_slot);

	// Check if size exceeding page boundary
	if(max_bit_index > (disk::max_map_bytes * disk::bits_per_byte))
		throw std::invalid_argument("size crossing data section boundary");

	set_bits(bit_index, reqd_bits);
}

inline pstsdk::ulonglong pstsdk::amap_page::allocate_space(size_t size, bool align)
{
	ulonglong location = 0;
	size_t reqd_bits = get_reqd_bits(size);
	size_t max_bits = disk::max_map_bytes * disk::bits_per_byte;


	// check if enough space available in amap page
	if(reqd_bits > m_total_free_slots)
		return location;

	// sector aligned allocation for pages.
	if(align)
	{
		for(size_t ind = 1; ind < m_page_data.size(); ind++)
		{
			if(m_page_data[ind] == 0x0)
			{
				location = m_address + (ind * disk::bits_per_byte * disk::bytes_per_slot);
				set_bits(get_bit_index(location), 8);
				break;
			}
		}
	}
	else
	{
		size_t alloc_start_bit = 0;
		size_t free_bits = 0;

		for(size_t bit_index = 8; bit_index < max_bits; bit_index++)
		{
			alloc_start_bit = free_bits == 0 ? bit_index : alloc_start_bit;

			if(!get_bit_state(bit_index))
			{
				free_bits++;
			}
			else
			{
				free_bits = 0;
				alloc_start_bit = 0;
				continue;
			}

			if(free_bits >= reqd_bits)
			{
				location = m_address + (alloc_start_bit * disk::bytes_per_slot);
				set_bits(alloc_start_bit, reqd_bits);
				break;
			}
		}
	}

	return location;
}

inline void pstsdk::amap_page::free_allocated_space(pstsdk::ulonglong location, size_t size)
{
	size_t bit_index = get_bit_index(location);
	size_t reqd_bits = get_reqd_bits(size);

	// check if attempting to free bits from first byte of page
	if(bit_index < 8)
		throw std::invalid_argument("invalid location address");

	size_t max_bit_index = bit_index + (size / disk::bytes_per_slot);

	// Check if size exceeding page boundary
	if(max_bit_index > (disk::max_map_bytes * disk::bits_per_byte))
		throw std::invalid_argument("size crossing data section boundary");

	// Check if freeing already freed location
	if(!are_bits_set(bit_index, reqd_bits))
		throw std::invalid_argument("invalid location.");

	reset_bits(bit_index, reqd_bits);
}

inline bool pstsdk::amap_page::is_location_allocated(pstsdk::ulonglong location, size_t size)
{
	size_t bit_index = get_bit_index(location);
	size_t max_bit_index = bit_index + (size / disk::bytes_per_slot);

	// Check if size exceeding page boundary
	if(max_bit_index > (disk::max_map_bytes * disk::bits_per_byte))
		throw std::invalid_argument("size crossing data section boundary");

	return are_bits_set(bit_index, get_reqd_bits(size));
}

inline bool pstsdk::amap_page::are_bits_set(size_t start_bit_index, size_t count)
{
	bool bRet = true;

	for(size_t bit_index = start_bit_index; bit_index < (start_bit_index + count); bit_index++)
	{
		bRet &= get_bit_state(bit_index);
	}

	return bRet;
}

inline void pstsdk::amap_page::set_bits(size_t start_bit_index, size_t count)
{
	for(size_t bit_index = start_bit_index; bit_index < (start_bit_index + count); bit_index++)
	{
		set_bit(bit_index);
		m_total_free_slots--;
	}
}

inline void pstsdk::amap_page::reset_bits(size_t start_bit_index, size_t count)
{
	for(size_t bit_index = start_bit_index; bit_index < (start_bit_index + count); bit_index++)
	{
		reset_bit(bit_index);
		m_total_free_slots++;
	}
}

inline size_t pstsdk::amap_page::get_bit_index(ulonglong location)
{
	ulonglong page_start_address = disk::first_amap_page_location + (disk::amap_page_interval * disk::get_amap_page_index(location));

	if(page_start_address != m_address)
		throw std::invalid_argument("location not corresponding to current page");

	return (size_t)(((location - page_start_address) / disk::bytes_per_slot));
}

inline void pstsdk::amap_page::build_page_metadata(bool is_new)
{
	size_t max_bits = disk::max_map_bytes * disk::bits_per_byte;
	m_total_free_slots = 0;

	// Incase of new page added
	if(is_new)
	{
		m_page_data.assign(disk::max_map_bytes, 0);

		m_page_data[0] = 0xFF;	// for amap itself
		m_total_free_slots = (max_bits - 8);

		if((m_address - disk::first_amap_page_location) % disk::pmap_page_interval == 0)
		{
			m_page_data[1] = 0xFF;	// for fmap
			m_total_free_slots -= 8;
		}

		ulonglong map_page_offset = 2 * disk::page_size;

		if(m_address >= disk::second_fmap_page_location - map_page_offset)
		{
			if(((m_address + map_page_offset) - disk::second_fmap_page_location) % disk::fmap_page_interval == 0)
			{
				m_page_data[2] = 0xFF;	// for pmap
				m_total_free_slots -= 8;
				map_page_offset += disk::page_size;
			}

			if(m_address >= disk::second_fpmap_page_location - map_page_offset)
			{
				if(((m_address + map_page_offset) - disk::second_fpmap_page_location) % disk::fpmap_page_interval == 0)
				{
					m_page_data[3] = 0xFF;	// for fpmap
					m_total_free_slots -= 8;
				}
			}
		}
	}
	else
	{
		for(size_t bit_index = 0; bit_index < max_bits; bit_index++)
		{
			if(!get_bit_state(bit_index))
				m_total_free_slots++;
		}
	}
}

inline void pstsdk::dlist_page::add_page_entry(size_t page_num, size_t free_space)
{
	std::map<size_t, std::multimap<size_t, size_t>::iterator>::iterator itr = m_page_space_map.find(page_num);

	if(m_page_space_map.end() == itr)
	{
		m_page_space_map.insert(std::make_pair<size_t, std::multimap<size_t, size_t>::iterator>(page_num,
			m_space_page_map.insert(std::make_pair<size_t, size_t>(free_space, page_num))));
	}
	else
	{
		m_space_page_map.erase(itr->second);
		m_page_space_map[page_num] = m_space_page_map.insert(std::make_pair<size_t, size_t>(free_space, page_num));
	}

	m_current_page = (m_space_page_map.rbegin())->second;
}

inline size_t pstsdk::dlist_page::get_page_number(size_t attempt_no)
{
	size_t page_num = 0;

	std::multimap<size_t, size_t>::reverse_iterator itr = m_space_page_map.rbegin();

	while(attempt_no <= m_space_page_map.size() && attempt_no > 0)
	{
		page_num = itr->second;
		itr++; attempt_no--;
	}

	return page_num;
}

inline void pstsdk::dlist_page::get_entries(std::vector<ulong>& entries)
{
	for(std::multimap<size_t, size_t>::reverse_iterator itr = m_space_page_map.rbegin();
		itr != m_space_page_map.rend(); itr++)
	{
		entries.push_back(build_entry(itr->second, itr->first));
	}
}

inline pstsdk::ulong pstsdk::dlist_page::build_entry(ulong page_num, ulong free_space)
{
	// shift page number 12 bits to make space for free slots entry
	free_space = free_space << disk::dlist_slots_shift;
	return page_num | free_space;
}

inline void pstsdk::dlist_page::build_page_entries(std::vector<ulong>& entries)
{
	for(size_t i = 0; i < entries.size(); i++)
	{
		add_page_entry(disk::dlist_get_page_num(entries[i]), disk::dlist_get_slots(entries[i]));
	}
}
//! \encond


#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
