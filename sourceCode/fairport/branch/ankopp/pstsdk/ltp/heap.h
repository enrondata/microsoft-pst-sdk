//! \file
//! \brief Heap-on-Node (HN) and BTree-on-Heap (BTH) implementation
//! \author Terry Mahaffey
//! \ingroup ltp

#ifndef PSTSDK_LTP_HEAP_H
#define PSTSDK_LTP_HEAP_H

#include <vector>
#include <algorithm>
#include <boost/noncopyable.hpp>
#include <boost/iostreams/concepts.hpp>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4244)
#endif
#include <boost/iostreams/stream.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if __GNUC__
#include <tr1/memory>
#endif

#include "pstsdk/util/primitives.h"
#include "pstsdk/disk/disk.h"
#include "pstsdk/ndb/node.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4250)
#endif

namespace pstsdk
{

	//! \defgroup ltp_heaprelated Heap
	//! \ingroup ltp

	template<typename K, typename V>
	class bth_nonleaf_node;

	template<typename K, typename V>
	class bth_leaf_node;

	template<typename K, typename V>
	class bth_node;

	class heap_impl;
	typedef std::tr1::shared_ptr<heap_impl> heap_ptr;

	//! \brief Defines a stream device for a heap allocation for use by boost iostream
	//!
	//! The boost iostream library requires that one defines a device, which
	//! implements a few key operations. This is the device for streaming out
	//! of a heap allocation.
	//! \ingroup ltp_heaprelated
	class hid_stream_device : public boost::iostreams::device<boost::iostreams::input_seekable>
	{
	public:
		hid_stream_device() : m_pos(0), m_hid(0) { }
		//! \copydoc node_stream_device::read()
		std::streamsize read(char* pbuffer, std::streamsize n);
		//! \copydoc node_stream_device::seek()
		std::streampos seek(boost::iostreams::stream_offset off, std::ios_base::seekdir way);

		//! \cond write_api
		//! \copydoc node_stream_device::write()
		std::streamsize write(const char* pbuffer, std::streamsize n);
		//! \endcond
	private:
		friend class heap_impl;
		hid_stream_device(const heap_ptr& _heap, heap_id id) : m_pos(0), m_hid(id), m_pheap(_heap) { }

		std::streamsize m_pos;
		heap_id m_hid;
		heap_ptr m_pheap;
	};

	//! \brief The actual heap allocation stream, defined using the boost iostream library
	//! and the \ref hid_stream_device.
	//! \ingroup ltp_heaprelated
	typedef boost::iostreams::stream<hid_stream_device> hid_stream;

	//! \brief The HN implementation
	//!
	//! Similar to how a \ref node is implemented, the HN implemention is actually
	//! divided up into two classes, heap_impl and heap, for the exact same
	//! reasons. The implementation details are in heap_impl, which is always a 
	//! dynamically allocated object, where as clients actually create heap
	//! objects. As more and more "child" objects are created and opened from
	//! inside the heap, they will reference the heap_impl as appropriate.
	//! \ingroup ltp_heaprelated
	class heap_impl : public std::tr1::enable_shared_from_this<heap_impl>
	{
	public:
		//! \brief Get the size of the given allocation
		//! \param[in] id The heap allocation to get the size of
		//! \throws length_error (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page or index of the allocation as indicated by the id are out of bounds for this node
		//! \returns The size of the allocation
		size_t size(heap_id id) const;

		//! \brief Get the count of pages (..external blocks) in this heap
		//! \returns The count of pages
		uint get_page_count() const { return m_node.get_page_count(); }

		//! \brief Returns the client root allocation out of this heap
		//!
		//! This value has specific meaning to the owner of the heap. It may point
		//! to a special structure which contains information about the data
		//! structures implemented in this heap or the larger node and subnodes
		//! (such as the \ref table). Or, it could just point to the root BTH 
		//! allocation (such as the \ref property_bag). In any event, the heap
		//! itself gives no special meaning to this value.
		//! \returns The client's root allocation
		heap_id get_root_id() const;

		//! \brief Returns the client signature of the heap
		//!
		//! Each heap is stamped with a signature byte by it's creator. This value
		//! is used mostly for validation purposes by the client when opening the
		//! heap.
		//! \sa heap_client_signature
		//! returns The client sig byte
		byte get_client_signature() const;

		//! \brief Read data out of a specified allocation at the specified offset
		//! \throws length_error (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page, index, or size of the allocation are out of bounds
		//! \param[in] buffer The location to store the data. The size of the buffer indicates the amount of data to read
		//! \param[in] id The heap allocation to read from
		//! \param[in] offset The offset into id to read starting at
		//! \returns The number of bytes read
		size_t read(std::vector<byte>& buffer, heap_id id, ulong offset) const;

		//! \brief Read an entire allocation
		//! \throws length_error (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page or index of the allocation as indicated by the id are out of bounds for this node
		//! \param[in] id The allocation to read
		//! \returns The entire allocation
		std::vector<byte> read(heap_id id) const;

		//! \brief Creates a stream device over a specified heap allocation
		//!
		//! The returned stream device can be used to construct a proper stream:
		//! \code
		//! heap h;
		//! hid_stream hstream(h.open_stream(0x30));
		//! \endcode
		//! Which can then be used as any iostream would be.
		//! \param[in] id The heap allocation to open a stream on
		//! \returns A heap allocation stream device
		hid_stream_device open_stream(heap_id id);
		//! \brief Get the node underlying this heap
		//! \returns The node
		const node& get_node() const { return m_node; }
		//! \brief Get the node underlying this heap
		//! \returns The node
		node& get_node() { return m_node; }

		//! \brief Opens a BTH from this heap with the given root id
		//! \tparam K The key type of this BTH
		//! \tparam V The value type of this BTH
		//! \param[in] root The root allocation of this BTH
		//! \returns The BTH object
		template<typename K, typename V>
		std::tr1::shared_ptr<bth_node<K,V> > open_bth(heap_id root);

		//! \cond write_api
		//! \brief Create a BTH from this heap
		//! \tparam K The key type of this BTH
		//! \tparam V The value type of this BTH
		//! \throws logic_error If the key size is invalid
		//! \throws logic_error If the valuse size is invalid
		//! \returns The BTH root id
		template<typename K, typename V>
		heap_id create_bth();

		//! \brief Create new heap item of give size on the heap
		//! \param[in] size The size of heam item
		//! \throws invalid_argument If the size is > 3580 bytes
		//! \returns The heap_id of the new heap item
		heap_id allocate_heap_item(size_t size);

		//! \brief Resize an existing heap item
		//! \param[in] id The heap id
		//! \param[in] size The new size
		//! \throws invalid_argument If the size is > 3580 bytes
		//! \returns The heap id of the heap item of new size
		heap_id re_allocate_heap_item(heap_id id, size_t size);

		//! \brief Free an heap item
		//! \param[in] hid Heap id of the heap item to be feed
		void free_heap_item(heap_id hid);

		//! \brief Save changes made to heap item
		void save_heap() { m_node.save_node(); }

		//! \brief Write data on to heap item
		//! \param[in] buffer The data to be written
		//! \param[in] id The heap id of the heap item
		//! \param[in] offset The position to start writing
		//! \throws length_error If data length is > than heap item size
		//! \throws length_error If offset is beyond heap item boundary
		//! \throws length_error If total data spans beyond heap item boundary
		//! \returns The amount of data written
		size_t write(std::vector<byte>& buffer, heap_id id, ulong offset);

		//! \brief Write data on to heap item
		//! \param[in] buffer The data to be written
		//! \param[in] id The heap id of the heap item
		//! \returns The amount of data written
		size_t write(std::vector<byte>& buffer, heap_id id);

		//! \brief Set the heap id of the root heap item for this heap
		//! \param[in] id The heap id to be set
		void set_root_id(heap_id id);

		friend class heap;

	private:
		//! \cond write_api
		//! \brief Get the closest data block with atleast 'size' of free sapce
		pstsdk::uint get_free_block(size_t size);

		//! \brief Create a default heap on node (HN)
		void create_default_heap(pstsdk::byte client_sig);

		//! \brief Get the first header of the heap on node
		//! \sa [MS-PST] 2.3.1.2
		disk::heap_first_header get_first_header() const;

		//! \brief Get the page map for given page
		//! \sa [MS-PST] 2.3.1.5
		std::vector<byte> get_page_map(uint page_id);

		//! \brief Get the page header for given page
		//! \sa [MS-PST] 2.3.1.3
		disk::heap_page_header get_page_header(uint page_id) const;

		//! \brief Get the page fill header for given page
		//! \sa [MS-PST] 2.3.1.4
		disk::heap_page_fill_header get_page_fill_header(uint page_id) const;

		//! \brief Get fill level map for given block
		//! \sa [MS-PST] 2.3.1.2
		pstsdk::byte get_fill_level(ulong heap_page_id, size_t heap_occ_size);

		//! \brief Check if required free space is available on the HN
		bool is_space_available(disk::heap_fill_level fill_level, size_t size);

		//! \brief Update fill level map
		void update_fill_header(pstsdk::ulong page_index, size_t heap_occ_size);

		//! \brief Update HN header
		void update_heap_header(std::vector<byte>& buffer, ulong page_index, size_t heap_occ_size, disk::heap_page_header& page_header);
		//! \endcond

		heap_impl();
		explicit heap_impl(const node& n);
		heap_impl(const node& n, alias_tag);
		heap_impl(const node& n, byte client_sig);
		heap_impl(const node& n, byte client_sig, alias_tag);
		heap_impl(const heap_impl& other) 
			: m_node(other.m_node) { }

		node m_node;
	};

	//! \brief Heap-on-Node implementation
	//!
	//! The HN is the first concept built on top of the \ref node abstraction
	//! exposed by the NDB. It treats the node similar to how a memory heap is
	//! treated, allowing the client to allocate memory up to 3.8k and free 
	//! those allocations from inside the node. To faciliate this, metadata is 
	//! kept at the start and end of each block in the HN node (which are 
	//! confusingly sometimes called pages in this context). So the HN has 
	//! detailed knowledge of how blocks and extended_blocks work in order to
	//! do it's book keeping, and find the appropriate block (..page) to 
	//! satisfy a given allocation request.
	//!
	//! Note a heap completely controls a node - you can not have multiple
	//! heaps per node. You can not use a node which has a heap on it for
	//! any other purpose beyond the heap interface.
	//! \sa [MS-PST 2.3.1]
	//! \ingroup ltp_heaprelated
	class heap
	{
	public:
		//! \brief Open a heap object on a node
		//! \param[in] n The node to open on top of. It will be copied.
		explicit heap(const node& n)
			: m_pheap(new heap_impl(n)) { }
		//! \brief Open a heap object on a node alias
		//! \param[in] n The node to alias
		heap(const node& n, alias_tag)
			: m_pheap(new heap_impl(n, alias_tag())) { }
		//! \brief Open a heap object on the specified node, and validate the client sig
		//! \throws sig_mismatch If the specified client_sig doesn't match what is in the node
		//! \param[in] n The node to open on top of. It will be copied.
		//! \param[in] client_sig Validate the heap has this value for the client sig
		heap(const node& n, byte client_sig)
			: m_pheap(new heap_impl(n, client_sig)) { }
		//! \brief Open a heap object on the specified node (alias), and validate the client sig
		//! \throws sig_mismatch If the specified client_sig doesn't match what is in the node
		//! \param[in] n The node to alias
		//! \param[in] client_sig Validate the heap has this value for the client sig
		heap(const node& n, byte client_sig, alias_tag)
			: m_pheap(new heap_impl(n, client_sig, alias_tag())) { }
		//! \brief Copy constructor
		//! \param[in] other The heap to copy
		heap(const heap& other)
			: m_pheap(new heap_impl(*(other.m_pheap))) { }
		//! \brief Alias constructor
		//! \param[in] other The heap to alias. The constructed object will share a heap_impl object with other.
		heap(const heap& other, alias_tag)
			: m_pheap(other.m_pheap) { }

#ifndef BOOST_NO_RVALUE_REFERENCES
		//! \brief Move constructor
		//! \param[in] other The heap to move from
		heap(heap&& other)
			: m_pheap(std::move(other.m_pheap)) { }
#endif

		//! \copydoc heap_impl::size()
		size_t size(heap_id id) const
		{ return m_pheap->size(id); }
		//! \copydoc heap_impl::get_root_id()
		heap_id get_root_id() const
		{ return m_pheap->get_root_id(); }
		//! \copydoc heap_impl::get_client_signature()
		byte get_client_signature() const
		{ return m_pheap->get_client_signature(); }
		//! \copydoc heap_impl::read(std::vector<byte>&,heap_id,ulong) const
		size_t read(std::vector<byte>& buffer, heap_id id, ulong offset) const
		{ return m_pheap->read(buffer, id, offset); }
		//! \copydoc heap_impl::read(heap_id) const
		std::vector<byte> read(heap_id id) const
		{ return m_pheap->read(id); }
		//! \copydoc heap_impl::open_stream()
		hid_stream_device open_stream(heap_id id)
		{ return m_pheap->open_stream(id); }

		//! \copydoc heap_impl::get_node() const
		const node& get_node() const
		{ return m_pheap->get_node(); }
		//! \copydoc heap_impl::get_node()    
		node& get_node()
		{ return m_pheap->get_node(); }

		//! \copydoc heap_impl::open_bth()
		template<typename K, typename V>
		std::tr1::shared_ptr<bth_node<K,V> > open_bth(heap_id root)
		{ return m_pheap->open_bth<K,V>(root); }

		//! \cond write_api
		//! \copydoc heap_impl::create_bth()
		template<typename K, typename V>
		heap_id create_bth()
		{ return m_pheap->create_bth<K, V>(); }

		//! \copydoc heap_impl::allocate_heap_item(size_t size)
		heap_id allocate_heap_item(size_t size)
		{ return m_pheap->allocate_heap_item(size); }

		//! \copydoc heap_impl::re_allocate_heap_item(heap_id id, size_t size)
		heap_id re_allocate_heap_item(heap_id id, size_t size)
		{ return m_pheap->re_allocate_heap_item(id, size); }

		//! \copydoc heap_impl::free_heap_item(heap_id hid)
		void free_heap_item(heap_id hid)
		{ m_pheap->free_heap_item(hid); }

		//! \copydoc heap_impl::save_heap()
		void save_heap()
		{ m_pheap->save_heap(); }

		//! \copydoc heap_impl::write(std::vector<byte>& buffer, heap_id id, ulong offset)
		size_t write(std::vector<byte>& buffer, heap_id id, ulong offset)
		{ return m_pheap->write(buffer, id, offset); }

		//! \copydoc heap_impl::write(std::vector<byte>& buffer, heap_id id)
		size_t write(std::vector<byte>& buffer, heap_id id)
		{ return m_pheap->write(buffer, id); }

		//! \copydoc heap_impl::set_root_id(heap_id id)
		void set_root_id(heap_id id)
		{ m_pheap->set_root_id(id); }
		//! \endcond


	private:
		heap& operator=(const heap& other); // = delete
		heap_ptr m_pheap;
	};

	//! \defgroup ltp_bthrelated BTH
	//! \ingroup ltp

	//! \brief The object which forms the root of the BTH hierarchy
	//!
	//! A BTH is a simple tree structure built using allocations out of a 
	//! \ref heap. bth_node forms the root of the object hierarchy 
	//! representing this tree. The child classes bth_nonleaf_node and 
	//! bth_leaf_node contain the implementation and represent nonleaf and
	//! leaf bth_nodes, respectively.
	//!
	//! Because a single bth_node is backed by a HN allocation (max 3.8k),
	//! most BTH "trees" consist of a BTH header which points directly to a
	//! single BTH leaf node.
	//!
	//! This hierarchy also models the \ref btree_node structure, inheriting the 
	//! actual iteration and lookup logic.
	//! \tparam K The key type for this BTH
	//! \tparam V The value type for this BTH
	//! \ingroup ltp_bthrelated
	//! \sa [MS-PST] 2.3.2
	template<typename K, typename V>
	class bth_node : 
		public virtual btree_node<K,V>, 
		private boost::noncopyable
	{
	public:
		//! \brief Opens a BTH node from the specified heap at the given root
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If this allocation doesn't have the BTH stamp
		//! \throws logic_error If the specified key/value type sizes do not match what is in the BTH header
		//! \param[in] h The heap to open out of
		//! \param[in] bth_root The allocation containing the bth header
		static std::tr1::shared_ptr<bth_node<K,V> > open_root(const heap_ptr& h, heap_id bth_root);
		//! \brief Open a non-leaf BTH node
		//! \param[in] h The heap to open out of
		//! \param[in] id The id to interpret as a non-leaf BTH node
		//! \param[in] level The level of this non-leaf node (must be non-zero)
		static std::tr1::shared_ptr<bth_nonleaf_node<K,V> > open_nonleaf(const heap_ptr& h, heap_id id, ushort level);
		//! \brief Open a leaf BTH node
		//! \param[in] h The heap to open out of
		//! \param[in] id The id to interpret as a leaf BTH node   
		static std::tr1::shared_ptr<bth_leaf_node<K,V> > open_leaf(const heap_ptr& h, heap_id id);

		//! \brief Construct a bth_node object
		//! \param[in] h The heap to open out of
		//! \param[in] id The id to interpret as a non-leaf BTH node
		//! \param[in] level The level of this non-leaf node (must be non-zero)
		bth_node(const heap_ptr& h, heap_id id, ushort level)
			: m_heap(h), m_id(id), m_level(level) { }
		virtual ~bth_node() { }

		//! \brief Return the heap_id of this bth_node
		//! \returns the heap_id of this bth_node
		heap_id get_id() const { return m_id; }
		//! \brief Return the leve of this bth_node
		//! \returns the leve of this bth_node
		ushort get_level() const { return m_level; }
		//! \brief Return the key size of this bth
		//! \returns the key size of this bth
		size_t get_key_size() const { return sizeof(K); }
		//! \brief Return the value size of this bth
		//! \returns the value size of this bth
		size_t get_value_size() const { return sizeof(V); }

		//! \brief Returns the heap this bth_node is in
		//! \brief The heap this bth_node is in
		const heap_ptr get_heap_ptr() const { return m_heap; }
		//! \brief Returns the heap this bth_node is in
		//! \brief The heap this bth_node is in
		heap_ptr get_heap_ptr() { return m_heap; }

		//! \brief Get the node underlying this BTH
		//! \returns the node
		const node& get_node() const { return m_heap->get_node(); }
		//! \brief Get the node underlying this BTH
		//! \returns the node
		node& get_node() { return m_heap->get_node(); }

		//! \cond write_api
		//! \brief Insert new item into this BTH node 
		//! \tparam K The key type for this BTH
		//! \tparam V The value type for this BTH
		//! \param[in] key The new key
		//! \param[in] val The new value
		//! \returns The modified BTH node
		virtual std::tr1::shared_ptr<bth_node<K, V>> insert(const K& key, const V& val) = 0;

		//! \brief Modify an item in this BTH node
		//! \tparam K The key type for this BTH
		//! \tparam V The value type for this BTH
		//! \param[in] key The key whose value is to be modified
		//! \param[in] val The new value
		//! \throws invalid_argument If key does not exist
		virtual void modify(const K& key, const V& val) = 0;

		//! \brief Remove an item from this BTH node
		//! \tparam K The key type for this BTH
		//! \param[in] key The key of item to be removed
		//! \throws invalid_argument If key does not exist
		virtual void remove(const K& key) = 0;

		//! \cond write_api
		//! \brief Save changes made to this BTH
		void save_bth() { m_heap->save_heap(); }
		//! \endcond

	protected:
		//! \cond write_api
		//! \brief Get the root heap id of this BTH
		heap_id get_root_id() const;
		//! \endcond

		heap_ptr m_heap;		//!< The under lying heap item	
		heap_id m_id;			//!< The heap_id of this bth_node
		ushort m_level;			//!< The level of this bth_node (0 for leaf, or distance from leaf)
	};

	//! \brief Contains references to other bth_node allocations
	//! \tparam K The key type for this BTH
	//! \tparam V The value type for this BTH
	//! \ingroup ltp_bthrelated
	//! \sa [MS-PST] 2.3.2.2
	template<typename K, typename V>
	class bth_nonleaf_node : 
		public bth_node<K,V>, 
		public btree_node_nonleaf<K,V>,
		public std::tr1::enable_shared_from_this<bth_nonleaf_node<K, V>>
	{
	public:
		//! \brief Construct a bth_nonleaf_node
		//! \param[in] h The heap to open out of
		//! \param[in] id The id to interpret as a non-leaf BTH node
		//! \param[in] level The level of this bth_nonleaf_node (non-zero)
		//! \param[in] bth_info The info about child bth_node allocations
#ifndef BOOST_NO_RVALUE_REFERENCES
		bth_nonleaf_node(const heap_ptr& h, heap_id id, ushort level, std::vector<std::pair<K, heap_id> > bth_info)
			: bth_node<K,V>(h, id, level), m_bth_info(std::move(bth_info)), m_child_nodes(m_bth_info.size()) { }
#else
		bth_nonleaf_node(const heap_ptr& h, heap_id id, ushort level, const std::vector<std::pair<K, heap_id> >& bth_info)
			: bth_node<K,V>(h, id, level), m_bth_info(bth_info), m_child_nodes(m_bth_info.size()) { }
#endif
		// btree_node_nonleaf implementation
		const K& get_key(uint pos) const { return m_bth_info[pos].first; }
		bth_node<K,V>* get_child(uint pos);
		const bth_node<K,V>* get_child(uint pos) const;
		uint num_values() const { return m_child_nodes.size(); }

		//! \cond write_api
		//! \copydoc bth_node::insert(const K& key, const V& val)
		virtual std::tr1::shared_ptr<bth_node<K, V>> insert(const K& key, const V& val);

		//! \copydoc bth_node::modify(const K& key, const V& val)
		virtual void modify(const K& key, const V& val);

		//! \copydoc bth_node::remove(const K& key)
		virtual void remove(const K& key);
		//! \endcond

	private:
		std::vector<std::pair<K, heap_id> > m_bth_info;
		mutable std::vector<std::tr1::shared_ptr<bth_node<K,V> > > m_child_nodes;
	};

	//! \brief Contains the actual key value pairs of the BTH
	//! \tparam K The key type for this BTH
	//! \tparam V The value type for this BTH
	//! \ingroup ltp_bthrelated
	//! \sa [MS-PST] 2.3.2.3
	template<typename K, typename V>
	class bth_leaf_node : 
		public bth_node<K,V>, 
		public btree_node_leaf<K,V>,
		public std::tr1::enable_shared_from_this<bth_leaf_node<K, V>>
	{
	public:
		//! \brief Construct a bth_leaf_node
		//! \param[in] h The heap to open out of
		//! \param[in] id The id to interpret as a non-leaf BTH node
		//! \param[in] data The key/value pairs stored in this leaf
#ifndef BOOST_NO_RVALUE_REFERENCES
		bth_leaf_node(const heap_ptr& h, heap_id id, std::vector<std::pair<K,V> > data)
			: bth_node<K,V>(h, id, 0), m_bth_data(std::move(data)) { }
#else
		bth_leaf_node(const heap_ptr& h, heap_id id, const std::vector<std::pair<K,V> >& data)
			: bth_node<K,V>(h, id, 0), m_bth_data(data) { }
#endif

		virtual ~bth_leaf_node() { }

		// btree_node_leaf implementation
		const V& get_value(uint pos) const
		{ return m_bth_data[pos].second; }
		const K& get_key(uint pos) const
		{ return m_bth_data[pos].first; }
		uint num_values() const
		{ return m_bth_data.size(); }

		//! \cond write_api
		//! \copydoc bth_node::insert(const K& key, const V& val)
		virtual std::tr1::shared_ptr<bth_node<K, V>> insert(const K& key, const V& val);

		//! \copydoc bth_node::modify(const K& key, const V& val);
		virtual void modify(const K& key, const V& val);

		//! \copydoc bth_node::remove(const K& key)
		virtual void remove(const K& key);
		//! \endcond

	private:
		std::vector<std::pair<K,V> > m_bth_data;
	};

} // end pstsdk namespace

//! \cond write_api
template<typename K, typename V>
inline std::tr1::shared_ptr<pstsdk::bth_node<K, V>> pstsdk::bth_leaf_node<K,V>::insert(const K& key, const V& val)
{
	std::tr1::shared_ptr<bth_node<K, V>> ret_node = shared_from_this();

	uint num_entries = num_values();
	disk::bth_leaf_entry<K, V> leaf_entry = { key, val };
	uint max_entries = disk::heap_max_alloc_size / sizeof(leaf_entry);

	//Space avail in node
	int pos = this->binary_search(key);
	uint idx = pos + 1;

	if((pos > -1) && (static_cast<unsigned int>(pos) < this->m_bth_data.size()) && (this->get_key(pos) == key))
	{
		// If key already exists, behave like modify
		this->m_bth_data[pos].second = val;
	}
	else
	{
		m_bth_data.insert(this->m_bth_data.begin() + idx, std::make_pair(key, val));
	}

	if(num_entries == 0)
	{
		disk::bth_header* pheader;
		std::vector<byte> buffer_hdr(sizeof(disk::bth_header), 0);
		pheader = (disk::bth_header*)&buffer_hdr[0];
		m_heap->read(buffer_hdr, get_root_id(), 0);

		m_id = m_heap->allocate_heap_item(m_bth_data.size() * sizeof(leaf_entry));

		if(pheader->num_levels == 0)
		{
			pheader->root = m_id;
			pheader->num_levels = static_cast<byte>(m_level);
			m_heap->write(buffer_hdr, get_root_id(), 0);
		}
	}
	else
	{
		// this will happen only in case of single leaf node bth
		if(m_bth_data.size() > max_entries)
		{
			// split node
			std::vector<std::pair<K, heap_id>> bth_inf;
			bth_inf.push_back(std::make_pair<K, heap_id>(get_key(0), m_id));
			heap_id hid = m_heap->allocate_heap_item(sizeof(disk::bth_nonleaf_entry<K>));

			disk::bth_nonleaf_entry<K> nonleaf_entry = { key, m_id };
			std::vector<byte> buffer_entry(sizeof(nonleaf_entry), 0);
			memcpy(&buffer_entry[0], &nonleaf_entry, sizeof(nonleaf_entry));
			m_heap->write(buffer_entry, hid);

			disk::bth_header* pheader;
			std::vector<byte> buffer_hdr(sizeof(disk::bth_header), 0);
			pheader = (disk::bth_header*)&buffer_hdr[0];
			m_heap->read(buffer_hdr, get_root_id(), 0);

			pheader->root = hid;
			pheader->num_levels = 1;
			m_heap->write(buffer_hdr, get_root_id(), 0);

			std::tr1::shared_ptr<pstsdk::bth_nonleaf_node<K, V>> non_leaf(new bth_nonleaf_node<K, V>(m_heap, hid, pheader->num_levels, bth_inf));
			ret_node = non_leaf->insert(m_bth_data[max_entries].first, m_bth_data[max_entries].second);
			m_bth_data.pop_back();
		}
	}

	size_t tot_size = m_bth_data.size() * sizeof(leaf_entry);
	m_id = m_heap->re_allocate_heap_item(m_id, tot_size);
	std::vector<byte> buffer_entry(tot_size, 0);
	memcpy(&buffer_entry[0], &m_bth_data[0], tot_size);
	m_heap->write(buffer_entry, m_id);

	return ret_node;
}

template<typename K, typename V>
inline void pstsdk::bth_leaf_node<K,V>::modify(const K& key, const V& val)
{
	int pos = this->binary_search(key);

	if(pos < 0)
		throw std::invalid_argument("invalid key");

	disk::bth_leaf_entry<K, V> new_leaf_entry = { key, val };

	std::vector<byte> buffer_entry(sizeof(new_leaf_entry), 0);
	memcpy(&buffer_entry[0], &new_leaf_entry, sizeof(new_leaf_entry));
	m_heap->write(buffer_entry, get_id(), pos * sizeof(new_leaf_entry));

	m_bth_data[pos].second = val;
}

template<typename K, typename V>
inline void pstsdk::bth_leaf_node<K,V>::remove(const K& key)
{
	int pos = this->binary_search(key);

	if(pos < 0)
		throw std::invalid_argument("invalid key");

	size_t entry_size = sizeof(K) + sizeof(V);
	uint num_entries = num_values();
	heap_id hid = get_id();

	std::vector<byte> buff_one(pos * entry_size, 0);
	m_heap->read(buff_one, hid, 0);

	std::vector<byte> buff_two((num_entries  - (pos + 1)) * entry_size, 0);
	m_heap->read(buff_two, hid, (pos + 1) * entry_size);

	hid = m_heap->re_allocate_heap_item(hid, m_heap->size(hid) - entry_size);

	if(buff_one.size() > 0)
		m_heap->write(buff_one, hid, 0);

	if(buff_two.size() > 0)
		m_heap->write(buff_two, hid, pos * entry_size);

	if(hid == 0)
	{
		disk::bth_header* pheader;
		std::vector<byte> buffer_hdr(sizeof(disk::bth_header), 0);
		pheader = (disk::bth_header*)&buffer_hdr[0];
		m_heap->read(buffer_hdr, get_root_id(), 0);

		pheader->root = hid;
		pheader->num_levels = 0;
		m_heap->write(buffer_hdr, get_root_id(), 0);
	}

	m_bth_data.erase(m_bth_data.begin() + pos);
}

template<typename K, typename V>
inline std::tr1::shared_ptr<pstsdk::bth_node<K, V>> pstsdk::bth_nonleaf_node<K,V>::insert(const K& key, const V& val)
{
	std::tr1::shared_ptr<bth_node<K, V>> ret_node = shared_from_this();

	int pos = this->binary_search(key);

	if(pos == -1)
	{
		pos = 0;
	}

	uint num_entries = num_values();
	disk::bth_nonleaf_entry<K> nonleaf_entry;
	std::tr1::shared_ptr<bth_node<K,V>> new_child_node;

	nonleaf_entry.key = key;

	if(num_entries == 0)
	{
		if(m_level > 1)
		{
			std::vector<std::pair<K, heap_id>> bth_inf;
			std::tr1::shared_ptr<pstsdk::bth_nonleaf_node<K, V>> non_leaf(new bth_nonleaf_node<K, V>(m_heap, 0, m_level - 1, bth_inf));
			new_child_node = non_leaf->insert(key, val);
		}
		else
		{
			std::vector<std::pair<K,V>> data;
			std::tr1::shared_ptr<pstsdk::bth_leaf_node<K, V>> leaf(new bth_leaf_node<K, V>(m_heap, 0, data));
			new_child_node = leaf->insert(key, val);
		}

		m_id = m_heap->allocate_heap_item(sizeof(nonleaf_entry));
		m_bth_info.push_back(std::make_pair(key, new_child_node->get_id()));
		m_child_nodes.insert(m_child_nodes.begin() + pos + 1, new_child_node);
	}
	else
	{
		pstsdk::bth_node<K,V>* child_node = get_child(pos);
		uint max_entries = disk::heap_max_alloc_size / sizeof(disk::bth_nonleaf_entry<K>);
		uint child_max_entries = disk::heap_max_alloc_size / (child_node->get_key_size() + child_node->get_value_size());

		if(child_node->num_values() < child_max_entries)
		{
			// there is space in child node
			new_child_node = child_node->insert(key, val);
			m_bth_info[pos].first = new_child_node->get_key(0);
			m_bth_info[pos].second = new_child_node->get_id();
			m_child_nodes[pos] = new_child_node;
			nonleaf_entry.page = new_child_node->get_id();
		}
		else
		{
			//child node is full so split
			if(m_level > 1)
			{
				std::vector<std::pair<K, heap_id>> bth_inf;
				std::tr1::shared_ptr<pstsdk::bth_nonleaf_node<K, V>> non_leaf(new bth_nonleaf_node<K, V>(m_heap, 0, m_level - 1, bth_inf));
				new_child_node = non_leaf->insert(key, val);
			}
			else
			{
				std::vector<std::pair<K,V>> data;
				std::tr1::shared_ptr<pstsdk::bth_leaf_node<K, V>> leaf(new bth_leaf_node<K, V>(m_heap, 0, data));
				new_child_node = leaf->insert(key, val);
			}

			m_bth_info.insert(m_bth_info.begin() + pos + 1, std::make_pair(key, new_child_node->get_id()));
			m_child_nodes.insert(m_child_nodes.begin() + pos + 1, new_child_node);

			if(m_bth_info.size() > max_entries)
			{
				// split this node
				std::vector<std::pair<K, heap_id>> bth_inf;
				bth_inf.push_back(std::make_pair<K, heap_id>(get_key(0), m_id));
				heap_id hid = m_heap->allocate_heap_item(sizeof(disk::bth_nonleaf_entry<K>));

				disk::bth_header* pheader;
				std::vector<byte> buffer_hdr(sizeof(disk::bth_header), 0);
				pheader = (disk::bth_header*)&buffer_hdr[0];
				m_heap->read(buffer_hdr, get_root_id(), 0);

				pheader->root = hid;
				pheader->num_levels = static_cast<byte>(m_level + 1);
				m_heap->write(buffer_hdr, get_root_id(), 0);

				std::tr1::shared_ptr<pstsdk::bth_nonleaf_node<K, V>> non_leaf(new bth_nonleaf_node<K, V>(m_heap, 0, pheader->num_levels, bth_inf));
				ret_node = non_leaf->insert(m_bth_info[max_entries].first, val);
				m_bth_info.pop_back();
			}
		}
	}

	size_t tot_size = m_bth_info.size() * sizeof(nonleaf_entry);
	m_id = m_heap->re_allocate_heap_item(m_id, tot_size);
	std::vector<byte> buffer_entry(tot_size, 0);

	for(size_t ind = 0; ind < m_bth_info.size(); ++ind)
	{
		nonleaf_entry.key = m_bth_info[ind].first;
		nonleaf_entry.page = m_bth_info[ind].second;
		memcpy(&buffer_entry[ind * sizeof(nonleaf_entry)], &nonleaf_entry, sizeof(nonleaf_entry));
	}

	m_heap->write(buffer_entry, m_id);

	return ret_node;
}

template<typename K, typename V>
inline void pstsdk::bth_nonleaf_node<K,V>::modify(const K& key, const V& val)
{
	int pos = this->binary_search(key);

	if(pos == -1)
	{
		throw key_not_found<K>(key);
	}

	get_child(pos)->modify(key, val);
}

template<typename K, typename V>
inline void pstsdk::bth_nonleaf_node<K,V>::remove(const K& key)
{
	int pos = this->binary_search(key);

	if(pos == -1)
	{
		throw key_not_found<K>(key);
	}

	get_child(pos)->remove(key);
}

template<typename K, typename V>
inline pstsdk::heap_id pstsdk::bth_node<K,V>::get_root_id() const
{
	heap_id root_id = m_heap->get_root_id();

	if(m_heap->get_client_signature() == disk::heap_sig_tc)
	{
		std::vector<byte> table_info = m_heap->read(root_id);
		disk::tc_header* pheader = (disk::tc_header*)&table_info[0];
		return root_id = pheader->row_btree_id;
	}

	return root_id;
}
//! \endcond

template<typename K, typename V>
inline std::tr1::shared_ptr<pstsdk::bth_node<K,V> > pstsdk::bth_node<K,V>::open_root(const heap_ptr& h, heap_id bth_root)
{
	disk::bth_header* pheader;
	std::vector<byte> buffer(sizeof(disk::bth_header));
	pheader = (disk::bth_header*)&buffer[0];

	h->read(buffer, bth_root, 0);

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(pheader->bth_signature != disk::heap_sig_bth)
		throw sig_mismatch("bth_signature expected", 0, bth_root, pheader->bth_signature, disk::heap_sig_bth);

	if(pheader->key_size != sizeof(K))
		throw std::logic_error("invalid key size");

	if(pheader->entry_size != sizeof(V))
		throw std::logic_error("invalid entry size");
#endif

	if(pheader->num_levels > 0)
		return open_nonleaf(h, pheader->root, pheader->num_levels);
	else
		return open_leaf(h, pheader->root);
}

template<typename K, typename V>
inline std::tr1::shared_ptr<pstsdk::bth_nonleaf_node<K,V> > pstsdk::bth_node<K,V>::open_nonleaf(const heap_ptr& h, heap_id id, ushort level)
{
	uint num_entries = h->size(id) / sizeof(disk::bth_nonleaf_entry<K>);
	std::vector<byte> buffer(h->size(id));
	disk::bth_nonleaf_node<K>* pbth_nonleaf_node = (disk::bth_nonleaf_node<K>*)&buffer[0];
	std::vector<std::pair<K, heap_id> > child_nodes;

	h->read(buffer, id, 0);

	child_nodes.reserve(num_entries);

	for(uint i = 0; i < num_entries; ++i)
	{
		child_nodes.push_back(std::make_pair(pbth_nonleaf_node->entries[i].key, pbth_nonleaf_node->entries[i].page));
	}

#ifndef BOOST_NO_RVALUE_REFERENCES
	return std::tr1::shared_ptr<bth_nonleaf_node<K,V> >(new bth_nonleaf_node<K,V>(h, id, level, std::move(child_nodes)));
#else
	return std::tr1::shared_ptr<bth_nonleaf_node<K,V> >(new bth_nonleaf_node<K,V>(h, id, level, child_nodes));
#endif
}

template<typename K, typename V>
inline std::tr1::shared_ptr<pstsdk::bth_leaf_node<K,V> > pstsdk::bth_node<K,V>::open_leaf(const heap_ptr& h, heap_id id)
{
	std::vector<std::pair<K, V> > entries; 

	if(id)
	{
		uint num_entries = h->size(id) / (sizeof(K) + sizeof(V));
		std::vector<byte> buffer(h->size(id));
		disk::bth_leaf_node<K,V>* pbth_leaf_node = (disk::bth_leaf_node<K,V>*)&buffer[0];

		h->read(buffer, id, 0);

		entries.reserve(num_entries);

		for(uint i = 0; i < num_entries; ++i)
		{
			entries.push_back(std::make_pair(pbth_leaf_node->entries[i].key, pbth_leaf_node->entries[i].value));
		}
#ifndef BOOST_NO_RVALUE_REFERENCES
		return std::tr1::shared_ptr<bth_leaf_node<K,V> >(new bth_leaf_node<K,V>(h, id, std::move(entries)));
#else
		return std::tr1::shared_ptr<bth_leaf_node<K,V> >(new bth_leaf_node<K,V>(h, id, entries));
#endif
	}
	else
	{
		// id == 0 means an empty tree
		return std::tr1::shared_ptr<bth_leaf_node<K,V> >(new bth_leaf_node<K,V>(h, id, entries));
	}
}

template<typename K, typename V>
inline pstsdk::bth_node<K,V>* pstsdk::bth_nonleaf_node<K,V>::get_child(uint pos)
{
	if(m_child_nodes[pos] == NULL)
	{
		if(this->get_level() > 1)
			m_child_nodes[pos] = bth_node<K,V>::open_nonleaf(this->get_heap_ptr(), m_bth_info[pos].second, this->get_level()-1);
		else
			m_child_nodes[pos] = bth_node<K,V>::open_leaf(this->get_heap_ptr(), m_bth_info[pos].second);
	}

	return m_child_nodes[pos].get();
}

template<typename K, typename V>
inline const pstsdk::bth_node<K,V>* pstsdk::bth_nonleaf_node<K,V>::get_child(uint pos) const
{
	if(m_child_nodes[pos] == NULL)
	{
		if(this->get_level() > 1)
			m_child_nodes[pos] = bth_node<K,V>::open_nonleaf(this->get_heap_ptr(), m_bth_info[pos].second, this->get_level()-1);
		else
			m_child_nodes[pos] = bth_node<K,V>::open_leaf(this->get_heap_ptr(), m_bth_info[pos].second);
	}

	return m_child_nodes[pos].get();
}

inline pstsdk::heap_impl::heap_impl(const node& n)
	: m_node(n)
{
	// need to throw if the node is smaller than first_header
	disk::heap_first_header first_header = get_first_header();

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(first_header.signature != disk::heap_signature)
		throw sig_mismatch("invalid heap_sig", 0, n.get_id(), first_header.signature, disk::heap_signature);
#endif
}

inline pstsdk::heap_impl::heap_impl(const node& n, alias_tag)
	: m_node(n, alias_tag())
{
	// need to throw if the node is smaller than first_header
	disk::heap_first_header first_header = get_first_header();

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(first_header.signature != disk::heap_signature)
		throw sig_mismatch("invalid heap_sig", 0, n.get_id(), first_header.signature, disk::heap_signature);
#endif
}

//! \cond write_api
inline pstsdk::heap_impl::heap_impl(const node& n, byte client_sig)
	: m_node(n)
{

	if(m_node.get_data_id() == 0) // empty node to create default heap
	{
		create_default_heap(client_sig);
	}
	else
	{
		// need to throw if the node is smaller than first_header
		disk::heap_first_header first_header = get_first_header();

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
		if(first_header.signature != disk::heap_signature)
			throw sig_mismatch("invalid heap_sig", 0, n.get_id(), first_header.signature, disk::heap_signature);
#endif
		if(first_header.client_signature != client_sig)
			throw sig_mismatch("invalid client_sig", 0, n.get_id(), first_header.client_signature, client_sig);
	}
}
//! \endcond

inline pstsdk::heap_impl::heap_impl(const node& n, byte client_sig, alias_tag)
	: m_node(n, alias_tag())
{
	// need to throw if the node is smaller than first_header
	disk::heap_first_header first_header = get_first_header();

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(first_header.signature != disk::heap_signature)
		throw sig_mismatch("invalid heap_sig", 0, n.get_id(), first_header.signature, disk::heap_signature);
#endif
	if(first_header.client_signature != client_sig)
		throw sig_mismatch("invalid client_sig", 0, n.get_id(), first_header.client_signature, client_sig);
}

inline pstsdk::heap_id pstsdk::heap_impl::get_root_id() const
{
	disk::heap_first_header first_header = get_first_header();
	return first_header.root_id;
}

inline pstsdk::byte pstsdk::heap_impl::get_client_signature() const
{
	disk::heap_first_header first_header = get_first_header();
	return first_header.client_signature;
}

inline size_t pstsdk::heap_impl::size(heap_id id) const
{
	if(id == 0)
		return 0;

	disk::heap_page_header header = m_node.read<disk::heap_page_header>(get_heap_page(id), 0);

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(header.page_map_offset > m_node.get_page_size(get_heap_page(id)))
		throw std::length_error("page_map_offset > node size");
#endif

	std::vector<byte> buffer(m_node.get_page_size(get_heap_page(id)) - header.page_map_offset);
	m_node.read(buffer, get_heap_page(id), header.page_map_offset);
	disk::heap_page_map* pmap = reinterpret_cast<disk::heap_page_map*>(&buffer[0]);

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(get_heap_index(id) > pmap->num_allocs)
		throw std::length_error("index > num_allocs");
#endif

	return pmap->allocs[get_heap_index(id) + 1] - pmap->allocs[get_heap_index(id)];
}

inline size_t pstsdk::heap_impl::read(std::vector<byte>& buffer, heap_id id, ulong offset) const
{
	size_t hid_size = size(id);

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(buffer.size() > hid_size)
		throw std::length_error("buffer.size() > size()");

	if(offset > hid_size)
		throw std::length_error("offset > size()");

	if(offset + buffer.size() > hid_size)
		throw std::length_error("size + offset > size()");
#endif

	if(hid_size == 0)
		return 0;

	disk::heap_page_header header = m_node.read<disk::heap_page_header>(get_heap_page(id), 0);
	std::vector<byte> map_buffer(m_node.get_page_size(get_heap_page(id)) - header.page_map_offset);
	m_node.read(map_buffer, get_heap_page(id), header.page_map_offset);
	disk::heap_page_map* pmap = reinterpret_cast<disk::heap_page_map*>(&map_buffer[0]);

	return m_node.read(buffer, get_heap_page(id), pmap->allocs[get_heap_index(id)] + offset);
}

inline pstsdk::hid_stream_device pstsdk::heap_impl::open_stream(heap_id id)
{
	return hid_stream_device(shared_from_this(), id);
}

inline std::streamsize pstsdk::hid_stream_device::read(char* pbuffer, std::streamsize n)
{
	if(m_hid && (static_cast<size_t>(m_pos) + n > m_pheap->size(m_hid)))
		n = m_pheap->size(m_hid) - m_pos;

	if(n == 0 || m_hid == 0)
		return -1;

	std::vector<byte> buff(static_cast<uint>(n));
	size_t read = m_pheap->read(buff, m_hid, static_cast<ulong>(m_pos));

	memcpy(pbuffer, &buff[0], read);

	m_pos += read;

	return read;
}

inline std::streampos pstsdk::hid_stream_device::seek(boost::iostreams::stream_offset off, std::ios_base::seekdir way)
{
#if defined(_MSC_VER) && (_MSC_VER < 1600)
#pragma warning(push)
#pragma warning(disable:4244)
#endif
	if(way == std::ios_base::beg)
		m_pos = off;
	else if(way == std::ios_base::end)
		m_pos = m_pheap->size(m_hid) + off;
	else
		m_pos += off;
#if defined(_MSC_VER) && (_MSC_VER < 1600)
#pragma warning(pop)
#endif

	if(m_pos < 0)
		m_pos = 0;
	else if(static_cast<size_t>(m_pos) > m_pheap->size(m_hid))
		m_pos = m_pheap->size(m_hid);

	return m_pos;
}

inline std::vector<pstsdk::byte> pstsdk::heap_impl::read(heap_id id) const
{
	std::vector<byte> result(size(id));
	read(result, id, 0);
	return result;
}

template<typename K, typename V>
inline std::tr1::shared_ptr<pstsdk::bth_node<K,V> > pstsdk::heap_impl::open_bth(heap_id root)
{ 
	return bth_node<K,V>::open_root(shared_from_this(), root); 
}

//! \cond write_api
inline void pstsdk::heap_impl::set_root_id(pstsdk::heap_id id)
{
	disk::heap_first_header first_header = get_first_header();
	first_header.root_id = id;
	m_node.write<disk::heap_first_header>(first_header, 0);
}

inline std::streamsize pstsdk::hid_stream_device::write(const char* pbuffer, std::streamsize n)
{
	std::vector<byte> buff(static_cast<uint>(n));
	memcpy(&buff[0], pbuffer, static_cast<size_t>(n));

	size_t written = m_pheap->write(buff, m_hid, static_cast<ulong>(m_pos));
	m_pos += written;
	return written;
}

inline void pstsdk::heap_impl::create_default_heap(pstsdk::byte client_sig)
{
	pstsdk::disk::heap_first_header header;

	header.client_signature = client_sig;
	header.page_map_offset = sizeof(disk::heap_first_header);
	header.signature = disk::heap_signature;
	header.root_id = 0x0;
	memset(header.page_fill_levels, disk::heap_fill_empty, sizeof(header.page_fill_levels));

	disk::heap_page_map pg_map;
	pg_map.num_allocs = 0;
	pg_map.num_frees = 0;
	pg_map.allocs[0] = header.page_map_offset;

	std::vector<pstsdk::byte> buffer(sizeof(disk::heap_first_header) + sizeof(disk::heap_page_map));
	memcpy(&buffer[0], &header, sizeof(disk::heap_first_header));
	memcpy(&buffer[sizeof(disk::heap_first_header)], &pg_map, sizeof(disk::heap_page_map));

	m_node.resize(buffer.size());
	m_node.write(buffer, 0);
}

inline pstsdk::heap_id pstsdk::heap_impl::allocate_heap_item(size_t size)
{
	if(size > disk::heap_max_alloc_size)
		throw std::invalid_argument("size exceeding max heap allocation size");

	// get block large enough for allocation
	uint page_index = get_free_block(size);

	// read existing block data
	disk::heap_page_header page_header = get_page_header(page_index);
	std::vector<byte> buff_one(page_header.page_map_offset, 0);
	m_node.read(buff_one, page_index, 0);

	// update page mag to accomodate new heap entry
	std::vector<byte> buff_pgmap = get_page_map(page_index);
	buff_pgmap.resize(buff_pgmap.size() + sizeof(ushort), 0);
	disk::heap_page_map* ppg_map = reinterpret_cast<disk::heap_page_map*>(&buff_pgmap[0]);

	ushort new_alloc_offset = ppg_map->allocs[ppg_map->num_allocs] + size;
	ppg_map->num_allocs += 1;
	ppg_map->allocs[ppg_map->num_allocs] = new_alloc_offset;
	page_header.page_map_offset = new_alloc_offset;

	// generate new heap id
	heap_id new_id = make_heap_id(page_index, ppg_map->num_allocs - 1);

	// update heap data and meta-data.
	ulong heap_start_offset = ppg_map->allocs[0];
	size_t heap_occ_size = ppg_map->allocs[ppg_map->num_allocs] + buff_pgmap.size();
	std::vector<byte> buffer(heap_occ_size, 0);
	disk::heap_first_header first_header = get_first_header();

	// update rgbFillLevel [MS-PST] 2.3.1.2 | [MS-PST] 2.3.1.4
	update_heap_header(buffer, page_index, heap_occ_size, page_header);

	// write updated heap contents to node
	if(buff_one.size() > heap_start_offset)
		memcpy(&buffer[heap_start_offset], &buff_one[heap_start_offset], (min(buff_one.size(), buffer.size()) - heap_start_offset));

	memcpy(&buffer[page_header.page_map_offset], ppg_map, buff_pgmap.size());

	m_node.resize((page_index * (m_node.get_data_block()->get_page(0)->get_max_size())) + heap_occ_size);
	m_node.write(buffer, page_index, 0);

	return new_id;
}

inline pstsdk::uint pstsdk::heap_impl::get_free_block(size_t size)
{
	uint page_index = 0;
	uint page_cnt = get_page_count();
	disk::heap_first_header first_header = get_first_header();

	// check if space available within first 8 blocks
	for(size_t ind = 0; ind < first_header.fill_level_size; ind++)
	{
		pstsdk::byte fill_level = first_header.page_fill_levels[ind];

		size_t even_val = 2 * ind;

		if(even_val < page_cnt && is_space_available(static_cast<disk::heap_fill_level>(fill_level & 0x0F), size))
			return page_index = 2 * ind;

		size_t odd_val = (2 * ind) + 1;

		if(odd_val < page_cnt && is_space_available(static_cast<disk::heap_fill_level>(fill_level >> 4), size))
			return page_index = (2 * ind) + 1;
	}

	// check if space available within next 128 blocks
	for(uint ind_bmp = 8; ind_bmp < page_cnt;)
	{
		disk::heap_page_fill_header fill_header = get_page_fill_header(ind_bmp);

		size_t max_val = fill_header.fill_level_size < (page_cnt - ind_bmp) ? fill_header.fill_level_size : (page_cnt - ind_bmp);

		for(size_t ind = 0; ind < max_val; ind++)
		{
			pstsdk::byte fill_level = fill_header.page_fill_levels[ind];

			size_t even_val = ind_bmp + (2 * ind);

			if(even_val < page_cnt && is_space_available(static_cast<disk::heap_fill_level>(fill_level & 0x0F), size))
				return page_index = ind_bmp + (2 * ind);

			size_t odd_val = ind_bmp + ((2 * ind) + 1);

			if(odd_val < page_cnt && is_space_available(static_cast<disk::heap_fill_level>(fill_level >> 4), size))
				return page_index = ind_bmp + ((2 * ind) + 1);
		}

		ind_bmp += 128;
	}

	// no free page found so add a new one
	page_index = page_cnt++;

	if(((page_index - 8) % 128) == 0)
	{
		disk::heap_page_fill_header fill_header;
		disk::heap_page_map page_map;

		size_t heap_occ_size = sizeof(disk::heap_page_fill_header) + sizeof(disk::heap_page_map);
		std::vector<byte> buffer(heap_occ_size, 0);

		memset(&fill_header.page_fill_levels, 0, sizeof(fill_header.page_fill_levels[0]) * fill_header.fill_level_size);

		fill_header.page_map_offset = sizeof(disk::heap_page_fill_header);
		page_map.num_allocs = 0;
		page_map.num_frees = 0;
		page_map.allocs[0] = fill_header.page_map_offset;

		memcpy(&buffer[0], &fill_header, sizeof(disk::heap_page_fill_header));
		memcpy(&buffer[fill_header.page_map_offset], &page_map, sizeof(disk::heap_page_map));

		m_node.resize((page_index * (m_node.get_data_block()->get_page(0)->get_max_size())) + heap_occ_size);
		m_node.write(buffer, page_index, 0);
	}
	else
	{
		disk::heap_page_header page_header;
		disk::heap_page_map page_map;

		size_t heap_occ_size = sizeof(disk::heap_page_header) + sizeof(disk::heap_page_map);
		std::vector<byte> buffer(heap_occ_size, 0);

		page_header.page_map_offset = sizeof(disk::heap_page_header);
		page_map.num_allocs = 0;
		page_map.num_frees = 0;
		page_map.allocs[0] = page_header.page_map_offset;

		memcpy(&buffer[0], &page_header, sizeof(disk::heap_page_header));
		memcpy(&buffer[page_header.page_map_offset], &page_map, sizeof(disk::heap_page_map));

		m_node.resize((page_index * (m_node.get_data_block()->get_page(0)->get_max_size())) + heap_occ_size);
		m_node.write(buffer, page_index, 0);
	}

	return page_index;
}

inline bool pstsdk::heap_impl::is_space_available(disk::heap_fill_level fill_level, size_t size)
{
	switch(fill_level)
	{
	case disk::heap_fill_empty:
		return true;

	case disk::heap_fill_1:
		return (size < 2560) ? true : false;

	case disk::heap_fill_2:
		return (size < 2048) ? true : false;

	case disk::heap_fill_3:
		return (size < 1792) ? true : false;

	case disk::heap_fill_4:
		return (size < 1536) ? true : false;

	case disk::heap_fill_5:
		return (size < 1280) ? true : false;

	case disk::heap_fill_6:
		return (size < 1024) ? true : false;

	case disk::heap_fill_7:
		return (size < 768) ? true : false;

	case disk::heap_fill_8:
		return (size < 512) ? true : false;

	case disk::heap_fill_9:
		return (size < 256) ? true : false;

	case disk::heap_fill_10:
		return (size < 128) ? true : false;

	case disk::heap_fill_11:
		return (size < 64) ? true : false;

	case disk::heap_fill_12:
		return (size < 32) ? true : false;

	case disk::heap_fill_13:
		return (size < 16) ? true : false;

	case disk::heap_fill_14:
		return (size < 8) ? true : false;

	case disk::heap_fill_full:
		return false;

	default:
		return false;
	}
}

inline pstsdk::heap_id pstsdk::heap_impl::re_allocate_heap_item(pstsdk::heap_id id, size_t size)
{
	if(size > disk::heap_max_alloc_size)
		throw std::invalid_argument("size exceeding max heap allocation size");

	size_t item_size = heap_impl::size(id);

	if(item_size == size)
		return id;

	if(size == 0)
	{
		free_heap_item(id);
		return 0;
	}

	if(item_size == 0)
		return allocate_heap_item(size);

	ulong hid_index = get_heap_index(id);
	ulong page_index = get_heap_page(id);

	disk::heap_page_header page_header = get_page_header(page_index);

	std::vector<byte> buff_pgmap = get_page_map(page_index);
	disk::heap_page_map* ppg_map = reinterpret_cast<disk::heap_page_map*>(&buff_pgmap[0]);

	size_t heap_occ_size = page_header.page_map_offset + buff_pgmap.size();
	int size_diff = size - item_size;

	// check if this re-allocation can be accomodated
	if(size_diff > 0 && !is_space_available(static_cast<disk::heap_fill_level>(get_fill_level(page_index, heap_occ_size)), size_diff))
	{
		free_heap_item(id);
		return allocate_heap_item(size);
	}

	// compute offsets
	ulong item_end_offset = ppg_map->allocs[hid_index] + item_size;
	ulong item_new_end_offset = ppg_map->allocs[hid_index] + size;

	// check boundaries
	if(ppg_map->allocs[hid_index] > page_header.page_map_offset || item_end_offset > page_header.page_map_offset)
		throw std::invalid_argument("invalid heap id");

	// buffer for heap contents
	std::vector<byte> buffer(heap_occ_size + size_diff, 0);

	std::vector<byte> buff_one(item_end_offset, 0);
	m_node.read(buff_one, page_index, 0);

	std::vector<byte> buff_two(page_header.page_map_offset - item_end_offset, 0);
	m_node.read(buff_two, page_index, item_end_offset);

	for(ulong ind = hid_index + 1; ind <= ppg_map->num_allocs; ind++)
		ppg_map->allocs[ind] += size_diff;

	page_header.page_map_offset += size_diff;

	ulong heap_start_offset = ppg_map->allocs[0];
	heap_occ_size += size_diff;

	// update rgbFillLevel [MS-PST] 2.3.1.2 | [MS-PST] 2.3.1.4
	update_heap_header(buffer, page_index, heap_occ_size, page_header);

	// write updated heap contents to node
	if(buff_one.size() > heap_start_offset)
		memcpy(&buffer[heap_start_offset], &buff_one[heap_start_offset], min((buff_one.size() - heap_start_offset), (item_new_end_offset - heap_start_offset)));

	if(buff_two.size() > 0)
		memcpy(&buffer[item_new_end_offset], &buff_two[0], min(buff_two.size(), buffer.size()));

	memcpy(&buffer[page_header.page_map_offset], ppg_map, buff_pgmap.size());

	m_node.resize(m_node.size() + size_diff);
	m_node.write(buffer, page_index, 0);

	return id;
}

inline void pstsdk::heap_impl::update_heap_header(std::vector<byte>& buffer, pstsdk::ulong page_index, size_t heap_occ_size, pstsdk::disk::heap_page_header& page_header)
{
	if(page_index == 0)
	{
		disk::heap_first_header first_header = get_first_header();
		first_header.page_fill_levels[page_index / 2] |= get_fill_level(page_index, heap_occ_size);
		first_header.page_map_offset = page_header.page_map_offset;
		memcpy(&buffer[0], &first_header, sizeof(disk::heap_first_header));
		return;
	}

	if(((page_index - 8) % 128) == 0)
	{
		ulong fill_hrd_page_index = page_index >= 136 ? 136 + (128 * ((page_index / 128) - 1 )) : 8;

		disk::heap_page_fill_header fill_header = get_page_fill_header(page_index);
		fill_header.page_fill_levels[(page_index - fill_hrd_page_index) / 2] |= get_fill_level(page_index, heap_occ_size);
		fill_header.page_map_offset = page_header.page_map_offset;

		memcpy(&buffer[0], &fill_header, sizeof(disk::heap_page_fill_header));
	}
	else
	{
		memcpy(&buffer[0], &page_header, sizeof(disk::heap_page_header));
		update_fill_header(page_index, heap_occ_size);
	}
}

inline void pstsdk::heap_impl::update_fill_header(pstsdk::ulong page_index, size_t heap_occ_size)
{
	if(page_index < 8)
	{
		disk::heap_first_header first_header = get_first_header();
		first_header.page_fill_levels[page_index / 2] |= get_fill_level(page_index, heap_occ_size);
		m_node.write<disk::heap_first_header>(first_header, 0, 0);
	}
	else
	{
		ulong fill_hrd_page_index = page_index >= 136 ? 136 + (128 * ((page_index / 128) - 1 )) : 8;

		disk::heap_page_fill_header fill_header = get_page_fill_header(fill_hrd_page_index);
		fill_header.page_fill_levels[(page_index - fill_hrd_page_index) / 2] |= get_fill_level(page_index, heap_occ_size);
		m_node.write<disk::heap_page_fill_header>(fill_header, fill_hrd_page_index, 0);
	}
}

inline void pstsdk::heap_impl::free_heap_item(pstsdk::heap_id hid)
{
	ulong hid_index = get_heap_index(hid);
	ulong heap_page_id = get_heap_page(hid);
	size_t item_size = size(hid);

	if(item_size == 0)
		return;

	disk::heap_page_header page_header = get_page_header(heap_page_id);

	// read page map
	std::vector<byte> buff_pgmap = get_page_map(heap_page_id);
	disk::heap_page_map* ppg_map = reinterpret_cast<disk::heap_page_map*>(&buff_pgmap[0]);

	// compute offsets
	ulong item_start_offset = ppg_map->allocs[hid_index];
	ulong item_end_offset = ppg_map->allocs[hid_index] + item_size;

	// check boundaries
	if(item_start_offset > page_header.page_map_offset || item_end_offset > page_header.page_map_offset)
		throw std::invalid_argument("invalid heap id");

	// buffer for heap contents
	std::vector<byte> buffer(page_header.page_map_offset + buff_pgmap.size(), 0);

	std::vector<byte> buff_one(item_start_offset, 0);
	m_node.read(buff_one, heap_page_id, 0);

	std::vector<byte> buff_two(page_header.page_map_offset - item_end_offset, 0);
	m_node.read(buff_two, heap_page_id, item_end_offset);

	// update rgibAlloc [MS-PST] 2.3.1.5
	for(ulong ind = hid_index + 1; ind <= ppg_map->num_allocs; ind++)
		ppg_map->allocs[ind] -= item_size;

	ppg_map->num_frees += 1;

	ulong heap_start_offset = ppg_map->allocs[0];
	size_t heap_occ_size = buffer.size();

	// update rgbFillLevel [MS-PST] 2.3.1.2 | [MS-PST] 2.3.1.4
	update_heap_header(buffer, heap_page_id, heap_occ_size, page_header);

	if(buff_one.size() > heap_start_offset)
		memcpy(&buffer[heap_start_offset], &buff_one[heap_start_offset], (min(buff_one.size(), buffer.size()) - heap_start_offset));

	if(buff_two.size() > 0)
		memcpy(&buffer[item_start_offset], &buff_two[0], min(buff_two.size(), buffer.size()));

	memcpy(&buffer[page_header.page_map_offset], ppg_map, buff_pgmap.size());
	m_node.write(buffer, heap_page_id, 0);
}

inline pstsdk::byte pstsdk::heap_impl::get_fill_level(ulong heap_page_id, size_t heap_occ_size)
{
	int max_avlbl_size = (m_node.get_data_block()->get_page(heap_page_id)->get_max_size()) - heap_occ_size;

	if(max_avlbl_size >= 3584)
		return disk::heap_fill_empty;

	if(max_avlbl_size >= 2560 && max_avlbl_size < 3584)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_1 : disk::heap_fill_1 << 4;

	if(max_avlbl_size >= 2048 && max_avlbl_size < 2560)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_2 : disk::heap_fill_2 << 4;

	if(max_avlbl_size >= 1792 && max_avlbl_size < 2048)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_3 : disk::heap_fill_3 << 4;

	if(max_avlbl_size >= 1536 && max_avlbl_size < 1792)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_4 : disk::heap_fill_4 << 4;

	if(max_avlbl_size >= 1280 && max_avlbl_size < 1536)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_5 : disk::heap_fill_5 << 4;

	if(max_avlbl_size >= 1024 && max_avlbl_size < 1280)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_6 : disk::heap_fill_6 << 4;

	if(max_avlbl_size >= 768 && max_avlbl_size < 1024)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_7 : disk::heap_fill_7 << 4;

	if(max_avlbl_size >= 512 && max_avlbl_size < 768)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_8 : disk::heap_fill_8 << 4;

	if(max_avlbl_size >= 256 && max_avlbl_size < 512)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_9 : disk::heap_fill_9 << 4;

	if(max_avlbl_size >= 128 && max_avlbl_size < 256)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_10 : disk::heap_fill_10 << 4;

	if(max_avlbl_size >= 64 && max_avlbl_size < 128)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_11 : disk::heap_fill_11 << 4;

	if(max_avlbl_size >= 32 && max_avlbl_size < 64)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_12 : disk::heap_fill_12 << 4;

	if(max_avlbl_size >= 16 && max_avlbl_size < 32)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_13 : disk::heap_fill_13 << 4;

	if(max_avlbl_size >= 8 && max_avlbl_size < 16)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_14 : disk::heap_fill_14 << 4;

	if(max_avlbl_size < 8)
		return (heap_page_id % 2 == 0) ? disk::heap_fill_full : disk::heap_fill_full << 4;

	return disk::heap_fill_full;
}

inline size_t pstsdk::heap_impl::write(std::vector<byte>& buffer, pstsdk::heap_id id, ulong offset)
{
	size_t hid_size = size(id);
	ulong page_id = get_heap_page(id);

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(buffer.size() > hid_size)
		throw std::length_error("buffer.size() > size()");

	if(offset > hid_size)
		throw std::length_error("offset > size()");

	if(offset + buffer.size() > hid_size)
		throw std::length_error("size + offset > size()");
#endif

	if(hid_size == 0)
		return 0;

	std::vector<byte> pgmp_buff = get_page_map(page_id);
	disk::heap_page_map* pmap = reinterpret_cast<disk::heap_page_map*>(&pgmp_buff[0]);

	return m_node.write(buffer, page_id, pmap->allocs[get_heap_index(id)] + offset);
}

inline size_t pstsdk::heap_impl::write(std::vector<byte>& buffer, pstsdk::heap_id id)
{
	return write(buffer, id, 0);
}

inline pstsdk::disk::heap_first_header pstsdk::heap_impl::get_first_header() const
{
	return m_node.read<disk::heap_first_header>(0);
}

inline std::vector<byte> pstsdk::heap_impl::get_page_map(uint page_id)
{
	disk::heap_page_header header = get_page_header(page_id);

	std::vector<byte> buffer(m_node.get_page_size(page_id) - header.page_map_offset);
	m_node.read(buffer, page_id, header.page_map_offset);

	return buffer;
}

inline pstsdk::disk::heap_page_header pstsdk::heap_impl::get_page_header(uint page_id) const
{
	return m_node.read<disk::heap_page_header>(page_id, 0);
}

inline pstsdk::disk::heap_page_fill_header pstsdk::heap_impl::get_page_fill_header(uint page_id) const
{
	return m_node.read<disk::heap_page_fill_header>(page_id, 0);
}

template<typename K, typename V>
inline pstsdk::heap_id pstsdk::heap_impl::create_bth()
{
	heap_id hid = 0;
	size_t key_size = sizeof(K);
	size_t data_size = sizeof(V);

#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(key_size < 2 || key_size > 16 || key_size % 2 != 0)
		throw std::logic_error("invalid key size");

	if(data_size < 1 || data_size > 32)
		throw std::logic_error("invalid entry size");
#endif

	disk::bth_header bth_hdr;
	std::vector<byte> buffer(sizeof(disk::bth_header));

	bth_hdr.bth_signature = disk::heap_sig_bth;
	bth_hdr.key_size =  sizeof(K);
	bth_hdr.entry_size = sizeof(V);
	bth_hdr.num_levels = 0;
	bth_hdr.root = 0;

	hid = allocate_heap_item(sizeof(disk::bth_header));

	memcpy(&buffer[0], &bth_hdr, sizeof(disk::bth_header));

	write(buffer, hid);

	return hid;
}
//! \endcond

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif