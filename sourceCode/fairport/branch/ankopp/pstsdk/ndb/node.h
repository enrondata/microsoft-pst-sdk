//! \file
//! \brief Node and Block definitions
//! \author Terry Mahaffey
//!
//! The concept of a node is the primary abstraction exposed by the NDB layer.
//! Closely related is the concept of blocks, also defined here.
//! \ingroup ndb

#ifndef PSTSDK_NDB_NODE_H
#define PSTSDK_NDB_NODE_H

#include <vector>
#include <algorithm>
#include <memory>
#include <cassert>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iostreams/concepts.hpp>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4244)
#endif
#include <boost/iostreams/stream.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "pstsdk/util/util.h"
#include "pstsdk/util/btree.h"
#include "pstsdk/ndb/page.h"
#include "pstsdk/ndb/allocation_map.h"
#include "pstsdk/ndb/database_iface.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4250)
#endif


namespace pstsdk
{
	//! \defgroup ndb_noderelated Node
	//! \ingroup ndb

	//! \brief The node implementation
	//!
	//! The node class is really divided into two classes, node and
	//! node_impl. The purpose of this design is to allow a node to conceptually
	//! "outlive" the stack based object encapsulating it. This is necessary
	//! if subobjects are opened which still need to refer to their parent.
	//! This is accomplished in this case, like others, via having a thin stack
	//! based object (\ref node) with a shared pointer to the implemenetation object
	//! (node_impl). Subnodes will grab a shared_ptr ref to the parents node_impl.
	//!
	//! See the documentation for \ref node to read more about what a node is.
	//! 
	//! The interface of node_impl follows closely that of node; differing mainly 
	//! in the construction and destruction semantics plus the addition of some 
	//! convenience functions.
	//! 
	//! \ref node and its \ref node_impl class generally have a one to one mapping,
	//! this isn't true only if someone opens an \ref alias_tag "alias" for a node.
	//! \ingroup ndb_noderelated
	class node_impl : public std::tr1::enable_shared_from_this<node_impl>
	{
	public:
		//! \brief Constructor for top level nodes
		//!
		//! This constructor is specific to nodes defined in the NBT
		//! \param[in] db The database context we're located in
		//! \param[in] info Information about this node
		node_impl(const shared_db_ptr& db, const node_info& info)
			: m_id(info.id), m_original_data_id(info.data_bid), m_original_sub_id(info.sub_bid), m_original_parent_id(info.parent_id),
			m_parent_id(info.parent_id), m_db(db) { }

		//! \brief Constructor for subnodes
		//!
		//! This constructor is specific to nodes defined in other nodes
		//! \param[in] container_node The parent or containing node
		//! \param[in] info Information about this node
		node_impl(const std::tr1::shared_ptr<node_impl>& container_node, const subnode_info& info)
			: m_id(info.id), m_original_data_id(info.data_bid), m_original_sub_id(info.sub_bid), m_original_parent_id(0), m_parent_id(0),
			m_pcontainer_node(container_node), m_db(container_node->m_db) { }

		//! \brief Set one node equal to another
		//!
		//! The assignment semantics of a node cause the assigned to node to refer
		//! to the same data on disk as the assigned from node. It still has it's 
		//! own unique id, parent, etc - only the data contained in this node is 
		//! 'assigned'
		//! \param[in] other The node to assign from
		//! \returns *this after the assignment is done
		node_impl& operator=(const node_impl& other) { m_pdata = other.get_data_block(); m_psub = other.get_subnode_block(); return *this; }

		//! \brief Get the id of this node
		//! \returns The id
		node_id get_id() const { return m_id; }

		//! \brief Get the block_id of the data block of this node
		//! \returns The id
		block_id get_data_id() const;

		//! \brief Get the block_id of the subnode block of this node
		//! \returns The id
		block_id get_sub_id() const;

		//! \brief Get the parent id
		//!
		//! The parent id here is the field in the NBT. It is not the id of the
		//! container node, if any. Generally the parent id of a message will
		//! be a folder, etc.
		//! \returns The id, zero if this is a subnode
		node_id get_parent_id() const { return m_parent_id; }

		//! \brief Tells you if this is a subnode
		//! \returns true if this is a subnode, false otherwise
		bool is_subnode() { return m_pcontainer_node; }

		//! \brief Returns the data block associated with this node
		//! \returns A shared pointer to the data block
		std::tr1::shared_ptr<data_block> get_data_block() const { ensure_data_block(); return m_pdata; }

		//! \brief Returns the subnode block associated with this node
		//! \returns A shared pointer to the subnode block
		std::tr1::shared_ptr<subnode_block> get_subnode_block() const { ensure_sub_block(); return m_psub; }

		//! \brief Read data from this node
		//!
		//! Fills the specified buffer with data starting at the specified offset.
		//! The size of the buffer indicates how much data to read.
		//! \param[in,out] buffer The buffer to fill
		//! \param[in] offset The location to read from
		//! \returns The amount of data read
		size_t read(std::vector<byte>& buffer, ulong offset) const;

		//! \brief Read data from this node
		//!
		//! Returns a "T" located as the specified offset
		//! \tparam T The type to read
		//! \param[in] offset The location to read from
		//! \returns The type read
		template<typename T> T read(ulong offset) const;

		//! \brief Read data from this node
		//!
		//! Fills the specified buffer with data on the specified page at the
		//! specified offset. The size of the buffer indicates how much data to
		//! read.
		//! \note In this context, a "page" is an external block
		//! \param[in,out] buffer The buffer to fill
		//! \param[in] page_num The page to read from
		//! \param[in] offset The location to read from
		//! \returns The amount of data read
		size_t read(std::vector<byte>& buffer, uint page_num, ulong offset) const;

		//! \brief Read data from a specific block on this node
		//! \note In this context, a "page" is an external block
		//! \tparam T The type to read
		//! \param[in] page_num The block (ordinal) to read data from
		//! \param[in] offset The offset into that block to read from
		//! \returns The type read
		template<typename T> T read(uint page_num, ulong offset) const;

		//! \brief Read data from this node
		//! \param[out] pdest_buffer The location to read the data into
		//! \param[in] size The amount of data to read
		//! \param[in] offset The location to read from
		//! \returns The amount of data read
		size_t read_raw(byte* pdest_buffer, size_t size, ulong offset) const;

		//! \cond write_api
		//! \brief Write data to this node
		//! \param[in] buffer The data to be written
		//! \param[in] offset  The location to write at
		//! \returns The amount of data written
		size_t write(const std::vector<byte>& buffer, ulong offset);

		//! \brief Write a 'T' to this node
		//! \param[in] obj The data of type T to be written
		//! \param[in] offset  The location to write at
		template<typename T> void write(const T& obj, ulong offset);

		//! \brief Write data to this node
		//! \param[in] buffer The data to be written
		//! \param[in] page_num The block (ordinal) to write to
		//! \param[in] offset  The location to write at
		//! \returns The amount of data written
		size_t write(const std::vector<byte>& buffer, uint page_num, ulong offset);

		//! \brief Write a 'T' to this node
		//! \param[in] obj The data of type T to be written
		//! \param[in] page_num The block (ordinal) to write to
		//! \param[in] offset  The location to write at
		template<typename T> void write(const T& obj, uint page_num, ulong offset);

		//! \brief Write data to this node
		//! \param[in] pdest_buffer The data to be written
		//! \param[in] size The amount of data to write
		//! \param[in] offset  The location to write at
		//! \returns The amount of data written
		size_t write_raw(const byte* pdest_buffer, size_t size, ulong offset);

		//! \brief Resize node's data capacity to new size 
		//! \param[in] size the new data node size
		//! \returns The new size of node
		size_t resize(size_t size);
		//! \endcond

		//! \brief Returns the size of this node
		//! \returns The node size
		size_t size() const;

		//! \brief Returns the size of a page in this node
		//! \note In this context, a "page" is an external block
		//! \param[in] page_num The page to get the size of
		//! \returns The size of the page
		size_t get_page_size(uint page_num) const;

		//! \brief Returns the number of pages in this node
		//! \note In this context, a "page" is an external block
		//! \returns The number of pages
		uint get_page_count() const;

		// iterate over subnodes
		//! \brief Returns an iterator positioned at first subnodeinfo
		//! \returns An iterator over subnodeinfos
		const_subnodeinfo_iterator subnode_info_begin() const;

		//! \brief Returns an iterator positioned past the last subnodeinfo
		//! \returns An iterator over subnodeinfos
		const_subnodeinfo_iterator subnode_info_end() const;

		//! \brief Lookup a subnode by node id
		//! \throws key_not_found<node_id> if a subnode with the specified node_id was not found
		//! \param[in] id The subnode id to find
		//! \returns The subnode
		node lookup(node_id id) const;

		//! \cond write_api
		//! \brief Create subnode for this node
		//! \throws duplicate_key<node_id> If a subnode with same id already exists
		//! \param[in] id The id to be assigned to the subnode
		//! \returns A new empty subnode
		node create_subnode(node_id id);

		//! \brief Delete a subnode of this node
		//! \param[in] id The id of subnode to be deleted
		void delete_subnode(node_id id);

		//! \brief Save subnode in this node
		//! \param[in] sb_nd the subnode to be saved
		void save_subnode(pstsdk::node& sb_nd);

		//! \brief Save this node
		//! \param[in] recursive Flag indicating whether the save should recurse until topmost node
		void save_node(bool recursive = false);

		//! \brief C'tor
		//! \param[in] db The database context
		//! \param[in] info The node info
		//! \param[in] pdata The data block
		//! \param[in] psub The subnode block
		node_impl(const shared_db_ptr& db, const node_info& info, std::tr1::shared_ptr<data_block> pdata, std::tr1::shared_ptr<subnode_block> psub)
			: m_id(info.id), m_original_data_id(info.data_bid), m_original_sub_id(info.sub_bid),
			m_original_parent_id(info.parent_id), m_parent_id(info.parent_id), m_db(db), m_pdata(pdata), m_psub(psub) { }

		//! \brief Delete all subnodes of this node
		void drop_subnodes();

		//! \brief Drop ref counts for all data blocks of this node
		void drop_data_blocks();

		//! \brief Save all data and subnode blocks
		void save_blocks();

		//! \brief Lock node for exclusive access
		void lock_node();

		//! \brief Release exclusive lock aquired on this node.
		void unlock_node();

		//! \brief Set the parent id for this node. (Used only by higher layers)
		void set_parent_id(node_id pid) { m_parent_id = pid; }

		//! \brief Get the db context
		shared_db_ptr get_db() { return m_db; }

		//! \brief Get the container node id if we are a subnode
		node_id get_container_id() { return m_pcontainer_node ? m_pcontainer_node->get_id() : 0; }
		//! \endcond

	protected:
		//! \cond write_api
		//! \brief Write out subnode block tree to disk
		//! \param[in] block The root subnode block
		//! \param[in] bbt_updates List of bbt_updates to be done
		//! \param[in] blk_list List of subnode block ids that are part of this node
		void write_out_subnode_block(std::tr1::shared_ptr<subnode_block>& block, std::vector<bbt_update_action>& bbt_updates, std::vector<block_id>& blk_list);

		//! \brief Write out data block tree to disk
		//! \param[in] block The root data block
		//! \param[in] bbt_updates List of bbt_updates to be done
		//! \param[in] blk_list List of data block ids that are part of this node
		void write_out_data_block(std::tr1::shared_ptr<data_block>& block, std::vector<bbt_update_action>& bbt_updates, std::vector<block_id>& blk_list);

		//! \brief Reduce reference count for all data blocks
		//! \param[in] block The root data block
		//! \param[in] bbt_updates List of bbt_updates to be done
		void drop_data_blocks(std::tr1::shared_ptr<data_block>& block, std::vector<bbt_update_action>& bbt_updates);

		//! \brief Reduce reference count for all subnode blocks
		//! \param[in] block The root subnode block
		//! \param[in] bbt_updates List of bbt_updates to be done
		void drop_subnode_blocks(std::tr1::shared_ptr<subnode_block>& block, std::vector<bbt_update_action>& bbt_updates);

		//! \brief Reduce reference counts and update BBT for given list of block id's
		//! \param[in] blk_list List of data and subnode block ids that are part of this node
		void drop_block_ref_count(std::vector<block_id>& blk_list);

		//! \brief Create list of block ids in a given subnode block tree
		//! \param[in] block The root subnode block
		//! \param[out] blk_list List of subnode block ids that are part of this node
		void build_subnode_block_list(std::tr1::shared_ptr<subnode_block>& block, std::vector<block_id>& blk_list);

		//! \brief Create list of block ids in a given data block tree
		//! \param[in] block The root data block
		//! \param[out] blk_list List of data block ids that are part of this node
		void build_data_block_list(std::tr1::shared_ptr<data_block>& block, std::vector<block_id>& blk_list);
		//! \endcond


	private:
		//! \brief Loads the data block from disk
		//! \returns The data block for this node
		data_block* ensure_data_block() const;

		//! \brief Loads the subnode block from disk
		//! \returns The subnode block for this node
		subnode_block* ensure_sub_block() const;

		const node_id m_id;									 //!< The node_id of this node
		block_id m_original_data_id;						 //!< The original block_id of the data block of this node
		block_id m_original_sub_id;							 //!< The original block_id of the subnode block of this node
		node_id m_original_parent_id;						 //!< The original node_id of the parent node of this node

		mutable std::tr1::shared_ptr<data_block> m_pdata;	 //!< The data block
		mutable std::tr1::shared_ptr<subnode_block> m_psub;	 //!< The subnode block
		node_id m_parent_id;					             //!< The parent node_id to this node
		std::tr1::shared_ptr<node_impl> m_pcontainer_node;	 //!< The container node, of which we're a subnode, if applicable
		shared_db_ptr m_db;									 //!< The database context pointer
		mutable lock_var m_node_lock;						 //!< Mutex variable
	};


	//! \brief Defines a transform from a subnode_info to an actual node object
	//! 
	//! Nodes <i>actually</i> contain a collection of subnode_info objects, not
	//! a collection of subnodes. In order to provide the friendly facade of nodes
	//! appearing to have a collection of subnodes, we define this transform. It is
	//! used later with the boost iterator library to define a proxy iterator
	//! for the subnodes of a node.
	//! \ingroup ndb_noderelated
	class subnode_transform_info : public std::unary_function<subnode_info, node>
	{
	public:
		//! \brief Initialize this functor with the container node involved
		//! \param[in] parent The containing node
		subnode_transform_info(const std::tr1::shared_ptr<node_impl>& parent)
			: m_parent(parent) { }

		//! \brief Given a subnode_info, construct a subnode
		//! \param[in] info The info about the subnode
		//! \returns A constructed node
		node operator()(const subnode_info& info) const;

	private:
		std::tr1::shared_ptr<node_impl> m_parent; //!< The container node
	};

	//! \brief Defines a stream device for a node for use by boost iostream
	//!
	//! The boost iostream library requires that one defines a device, which
	//! implements a few key operations. This is the device for streaming out
	//! of a node.
	//! \ingroup ndb_noderelated
	class node_stream_device : public boost::iostreams::device<boost::iostreams::seekable>
	{
	public:
		//! \brief Default construct the node stream device
		node_stream_device() : m_pos(0) { }

		//! \brief Read data from this node into the buffer at the current position
		//! \param[out] pbuffer The buffer to store the results into
		//! \param[in] n The amount of data to read
		//! \returns The amount of data read
		std::streamsize read(char* pbuffer, std::streamsize n); 

		//! \cond write_api
		//! \brief Write data to this node from the buffer at the current position
		//! \param[in] pbuffer The data to write
		//! \param[in] n The amount of data to write
		//! \returns The amount of data written
		std::streamsize write(const char* pbuffer, std::streamsize n);
		//! \endcond

		//! \brief Move the current position in the stream
		//! \param[in] off The offset to move the current position
		//! \param[in] way The location to move the current position from
		//! \returns The new position
		std::streampos seek(boost::iostreams::stream_offset off, std::ios_base::seekdir way);

	private:
		friend class node;

		//! \brief Construct the device from a node
		node_stream_device(std::tr1::shared_ptr<node_impl>& _node) : m_pos(0), m_pnode(_node) { }

		std::streamsize m_pos;						//!< The stream's current position
		std::tr1::shared_ptr<node_impl> m_pnode;	//!< The node this stream is over
	};

	//! \brief The actual node stream, defined using the boost iostream library
	//! and the \ref node_stream_device.
	//! \ingroup ndb_noderelated
	typedef boost::iostreams::stream<node_stream_device> node_stream;

	//! \brief An in memory representation of the "node" concept in a PST data file
	//!
	//! A node is the primary abstraction exposed by the \ref ndb to the
	//! upper layers. It's purpose is to abstract away the details of working with
	//! immutable blocks and subnode blocks. It can therefor be thought of simply
	//! as a mutable stream of bytes and a collection of sub nodes, themselves
	//! a mutable stream of bytes potentially with another collection of subnodes,
	//! etc.
	//!
	//! When using the \ref node class, think of it as creating an in memory 
	//! "instance" of the node on the disk. You can have several in memory
	//! instances of the same node on disk. You can even have an \ref alias_tag 
	//! "alias" of another in memory instance, as is sometimes required when 
	//! creating higher level abstractions. 
	//!
	//! There isn't much interesting to do with a node, you can query its size,
	//! read from a specific location, get it's id and parent id, iterate over
	//! subnodes, etc. Most of the interesting things are done by higher level
	//! abstractions which are built on top of and know specifically how to 
	//! interpret the bytes in a node - such as the \ref heap, \ref table, and \ref
	//! property_bag.
	//! \sa [MS-PST] 2.2.1.1
	//! \ingroup ndb_noderelated
	class node
	{
	public:
		//! \brief A transform iterator, so we can expose the subnodes as a
		//! collection of nodes rather than \ref subnode_info objects.
		typedef boost::transform_iterator<subnode_transform_info, const_subnodeinfo_iterator> subnode_iterator;

		//! \copydoc node_impl::node_impl(const shared_db_ptr&,const node_info&)
		node(const shared_db_ptr& db, const node_info& info)
			: m_pimpl(new node_impl(db, info)) { }

		//! \brief Constructor for subnodes
		//!
		//! This constructor is specific to nodes defined in other nodes
		//! \param[in] container_node The parent or containing node
		//! \param[in] info Information about this node
		node(const node& container_node, const subnode_info& info)
			: m_pimpl(new node_impl(container_node.m_pimpl, info)) { }

		//! \copydoc node_impl::node_impl(const std::tr1::shared_ptr<node_impl>&,const subnode_info&)
		node(const std::tr1::shared_ptr<node_impl>& container_node, const subnode_info& info)
			: m_pimpl(new node_impl(container_node, info)) { }

		//! \brief Copy construct this node
		//! 
		//! This node will be another instance of the specified node
		//! \param[in] other The node to copy
		node(const node& other)
			: m_pimpl(new node_impl(*other.m_pimpl)) { }

		//! \brief Alias constructor
		//!
		//! This node will be an alias of the specified node. They will refer to the
		//! same in memory object - they share a \ref node_impl instance.
		//! \param[in] other The node to alias
		node(const node& other, alias_tag) : m_pimpl(other.m_pimpl) { }

#ifndef BOOST_NO_RVALUE_REFERENCES
		//! \brief Move constructor
		//! \param[in] other Node to move from
		node(node&& other) : m_pimpl(std::move(other.m_pimpl)) { }
#endif

		//! \copydoc node_impl::operator=()
		node& operator=(const node& other) { *m_pimpl = *(other.m_pimpl); return *this; }

		//! \copydoc node_impl::get_id()
		node_id get_id() const { return m_pimpl->get_id(); }

		//! \copydoc node_impl::get_data_id()
		block_id get_data_id() const { return m_pimpl->get_data_id(); }

		//! \copydoc node_impl::get_sub_id()
		block_id get_sub_id() const { return m_pimpl->get_sub_id(); }

		//! \copydoc node_impl::get_parent_id()
		node_id get_parent_id() const { return m_pimpl->get_parent_id(); } 

		//! \copydoc node_impl::is_subnode()
		bool is_subnode() { return m_pimpl->is_subnode(); } 

		//! \copydoc node_impl::get_data_block()
		std::tr1::shared_ptr<data_block> get_data_block() const { return m_pimpl->get_data_block(); }

		//! \copydoc node_impl::get_subnode_block()
		std::tr1::shared_ptr<subnode_block> get_subnode_block() const { return m_pimpl->get_subnode_block(); }

		//! \copydoc node_impl::read(std::vector<byte>&,ulong) const
		size_t read(std::vector<byte>& buffer, ulong offset) const { return m_pimpl->read(buffer, offset); }

		//! \copydoc node_impl::read(ulong) const
		template<typename T> T read(ulong offset) const { return m_pimpl->read<T>(offset); }

		//! \copydoc node_impl::read(std::vector<byte>&,uint,ulong) const
		size_t read(std::vector<byte>& buffer, uint page_num, ulong offset) const { return m_pimpl->read(buffer, page_num, offset); }

		//! \copydoc node_impl::read(uint,ulong) const
		template<typename T> T read(uint page_num, ulong offset) const { return m_pimpl->read<T>(page_num, offset); }

		//! \copydoc node_impl::write(std::vector<byte>& buffer, ulong offset)
		size_t write(std::vector<byte>& buffer, ulong offset) { return m_pimpl->write(buffer, offset); }

		//! \copydoc node_impl::write(const T& obj, ulong offset)
		template<typename T> void write(const T& obj, ulong offset) { return m_pimpl->write<T>(obj, offset); }

		//! \copydoc node_impl::write(std::vector<byte>& buffer, uint page_num, ulong offset)
		size_t write(std::vector<byte>& buffer, uint page_num, ulong offset) { return m_pimpl->write(buffer, page_num, offset); }

		//! \copydoc node_impl::write(const T& obj, uint page_num, ulong offset)
		template<typename T> void write(const T& obj, uint page_num, ulong offset) { return m_pimpl->write<T>(obj, page_num, offset); }

		//! \copydoc node_impl::resize(size_t size)
		size_t resize(size_t size) { return m_pimpl->resize(size); }

		//! \copydoc node_impl::get_db()
		shared_db_ptr get_db() { return m_pimpl->get_db(); }

		//! \copydoc node_impl::get_container_id()
		node_id get_container_id() { return m_pimpl->get_container_id();	}
		//! \endcond

		//! \brief Creates a stream device over this node
		//!
		//! The returned stream device can be used to construct a proper stream:
		//! \code
		//! node n;
		//! node_stream nstream(n.open_as_stream());
		//! \endcode
		//! Which can then be used as any iostream would be.
		//! \returns A node stream device for this node
		node_stream_device open_as_stream() { return node_stream_device(m_pimpl); }

		//! \copydoc node_impl::size()
		size_t size() const { return m_pimpl->size(); }

		//! \copydoc node_impl::get_page_size()
		size_t get_page_size(uint page_num) const { return m_pimpl->get_page_size(page_num); }

		//! \copydoc node_impl::get_page_count()
		uint get_page_count() const { return m_pimpl->get_page_count(); }

		// iterate over subnodes
		//! \copydoc node_impl::subnode_info_begin()
		const_subnodeinfo_iterator subnode_info_begin() const { return m_pimpl->subnode_info_begin(); }

		//! \copydoc node_impl::subnode_info_end()
		const_subnodeinfo_iterator subnode_info_end() const { return m_pimpl->subnode_info_end(); } 

		//! \brief Returns a proxy iterator for the first subnode
		//!
		//! This is known as a proxy iterator because the dereferenced type is
		//! of type node, not node& or const node&. This means the object is an
		//! rvalue, constructed specifically for the purpose of being returned
		//! from this iterator.
		//! \returns The proxy iterator
		subnode_iterator subnode_begin() const { return boost::make_transform_iterator(subnode_info_begin(), subnode_transform_info(m_pimpl)); }

		//! \brief Returns a proxy iterator positioned past the last subnode
		//!
		//! This is known as a proxy iterator because the dereferenced type is
		//! of type node, not node& or const node&. This means the object is an
		//! rvalue, constructed specifically for the purpose of being returned
		//! from this iterator.
		//! \returns The proxy iterator
		subnode_iterator subnode_end() const { return boost::make_transform_iterator(subnode_info_end(), subnode_transform_info(m_pimpl)); }

		//! \copydoc node_impl::lookup()
		node lookup(node_id id) const { return m_pimpl->lookup(id); }

		//! \copydoc node_impl::create_subnode(node_id id)
		node create_subnode(node_id id) { return m_pimpl->create_subnode(id); }

		//! \copydoc node_impl::delete_subnode(node_id id)
		void delete_subnode(node_id id) { m_pimpl->delete_subnode(id); }

		//! \copydoc node_impl::save_subnode(pstsdk::node& sb_nd)
		void save_subnode(pstsdk::node& sb_nd) { m_pimpl->save_subnode(sb_nd); }

		//! \brief Save this node
		void save_node() { m_pimpl->save_node(); }

		//! \copydoc node_impl::drop_data_blocks()
		void drop_data_blocks() { m_pimpl->drop_data_blocks(); }

		//! \copydoc node_impl::drop_subnodes()
		void drop_subnodes() { m_pimpl->drop_subnodes(); }

		//! \copydoc node_impl::node(const shared_db_ptr& db, const node_info& info, std::tr1::shared_ptr<data_block> pdata, std::tr1::shared_ptr<subnode_block> psub)
		node(const shared_db_ptr& db, const node_info& info, std::tr1::shared_ptr<data_block> pdata, std::tr1::shared_ptr<subnode_block> psub)
			:m_pimpl(new node_impl(db, info, pdata, psub)) { }

		//! \copydoc node_impl::save_blocks()
		void save_blocks()  { m_pimpl->save_blocks(); }

		//! \copydoc node_impl::lock_node()
		void lock_node() { m_pimpl->lock_node(); }

		//! \copydoc node_impl::unlock_node()
		void unlock_node() { m_pimpl->unlock_node(); }

		//! \copydoc node_impl::set_parent_id(node_id pid)
		void set_parent_id(node_id pid) { m_pimpl->set_parent_id(pid); }
		//! \endcond

	private:
		std::tr1::shared_ptr<node_impl> m_pimpl; //!< Pointer to the node implementation
	};

	//! \defgroup ndb_blockrelated Blocks
	//! \ingroup ndb

	//! \brief The base class of the block class hierarchy
	//!
	//! A block is an atomic unit of storage in a PST file. This class, and other
	//! classes in this hierarchy, are in memory representations of those blocks
	//! on disk. They are immutable, and are shared freely between different node
	//! instances as needed (via shared pointers). A block also knows how to read
	//! any blocks it may refer to (in the case of extended_block or a 
	//! subnode_block).
	//!
	//! All blocks in the block hierarchy are also in the category of what is known
	//! as <i>dependant objects</i>. This means is they only keep a weak 
	//! reference to the database context to which they're a member. Contrast this
	//! to an independant object such as the \ref node, which keeps a strong ref
	//! or a full shared_ptr to the related context. This implies that someone
	//! must externally make sure the database context outlives it's blocks -
	//! this is usually done by the database context itself or the node which
	//! holds these blocks.
	//! \sa disk_blockrelated
	//! \sa [MS-PST] 2.2.2.8
	//! \ingroup ndb_blockrelated
	class block
	{
	public:
		//! \brief Basic block constructor
		//! \param[in] db The database context this block was opened in
		//! \param[in] info Information about this block
		block(const shared_db_ptr& db, const block_info& info)
			: m_modified(false), m_size(info.size), m_id(info.id), m_address(info.address), m_db(db) { }

		//! \cond write_api
		//! \brief Construct block from another block
		//! \param[in] other The other block
		block(const block& other)
			: m_modified(false), m_size(other.m_size), m_id(0), m_address(0), m_db(other.m_db) { }
		//! \endcond

		virtual ~block() { }

		//! \brief Returns the blocks internal/external state
		//! \returns true if this is an internal block, false otherwise
		virtual bool is_internal() const = 0;

		//! \brief Get the last known size of this block on disk
		//! \returns The last known size of this block on disk
		size_t get_disk_size() const { return m_size; }

		//! \brief Get the block_id of this block
		//! \returns The block_id of this block
		block_id get_id() const { return m_id; }

		//! \brief Get the address of this block on disk
		//! \returns The address of this block, 0 for a new block.
		ulonglong get_address() const { return m_address; }

		//! \brief Set address on disk for this block
		//! \param[in] new_address Address on disk of this block
		void set_address(ulonglong new_address) { m_address = new_address; }

		//! \brief Set the size on disk for this block
		//! \param[in] new_size Size on disk of this block
		void set_disk_size(size_t new_size) { m_size = new_size; }

		//! \brief Mutate and mark block as modified
		void touch();

		//! \brief Check if block is modified
		//! \returns Returns whether block is modified
		bool is_modified() { return m_modified; }
		//! \endcond

	protected:
		shared_db_ptr get_db_ptr() const { return shared_db_ptr(m_db); }

		bool m_modified;					//!< True if this block has been modified and needs to be saved
		size_t m_size;						//!< The size of this specific block on disk at last save
		block_id m_id;						//!< The id of this block
		ulonglong m_address;				//!< The address of this specific block on disk, 0 if unknown
		weak_db_ptr m_db;					//!< The database context pointer
		mutable lock_var m_block_lock;		//!< Mutex variable
	};

	//! \brief A block which represents end user data
	//!
	//! This class is the base class of both \ref extended_block and \ref
	//! external_block. This base class exists to abstract away their 
	//! differences, so a node can treat a given block (be it extended or
	//! external) simply as a stream of bytes.
	//! \ingroup ndb_blockrelated
	class data_block : public block
	{
	public:
		//! \brief Constructor for a data_block
		//! \param[in] db The database context
		//! \param[in] info Information about this block
		//! \param[in] total_size The total logical size of this block
		data_block(const shared_db_ptr& db, const block_info& info, size_t total_size)
			: block(db, info), m_total_size(total_size) { }

		virtual ~data_block() { }

		//! \brief Read data from this block
		//!
		//! Fills the specified buffer with data starting at the specified offset.
		//! The size of the buffer indicates how much data to read.
		//! \param[in,out] buffer The buffer to fill
		//! \param[in] offset The location to read from
		//! \returns The amount of data read
		size_t read(std::vector<byte>& buffer, ulong offset) const;

		//! \brief Read data from this block
		//!
		//! Returns a "T" located as the specified offset
		//! \tparam T The type to read
		//! \param[in] offset The location to read from
		//! \returns The type read
		template<typename T> T read(ulong offset) const;

		//! \brief Read data from this block
		//! \param[out] pdest_buffer The location to read the data into
		//! \param[in] size The amount of data to read
		//! \param[in] offset The location to read from
		//! \returns The amount of data read
		virtual size_t read_raw(byte* pdest_buffer, size_t size, ulong offset) const = 0;

		//! \cond write_api
		//! \brief Write data to this block
		//! \throws std::out_of_range If offset is beyond block size
		//! \param[in] buffer The data to be written
		//! \param[in] offset The location to write at
		//! \param[out] presult A new data block with modified contents
		//! \returns The amount of data written
		size_t write(const std::vector<byte>& buffer, ulong offset, std::tr1::shared_ptr<data_block>& presult);

		//! \brief Write a 'T' to this block
		//! \throws std::out_of_range If offset is beyond block size
		//! \throws std::out_of_range If sizeof(T) + offset is beyond block size
		//! \param[in] buffer The data of type 'T' to be written
		//! \param[in] offset The location to write at
		//! \param[out] presult A new data block with modified contents
		template<typename T> void write(const T& buffer, ulong offset, std::tr1::shared_ptr<data_block>& presult);

		//! \brief Write data to this block
		//! \param[in] psrc_buffer The data to be written
		//! \param[in] size The size of data to be written
		//! \param[in] offset The location to write at
		//! \param[out] presult A new data block with modified contents
		virtual size_t write_raw(const byte* psrc_buffer, size_t size, ulong offset, std::tr1::shared_ptr<data_block>& presult) = 0;

		//! \brief Resize this block
		//! \param[in] size The new size of this block
		//! \param[out] presult A new data block of new size
		virtual size_t resize(size_t size, std::tr1::shared_ptr<data_block>& presult) = 0;
		//! \endcond

		//! \brief Get the number of physical pages in this data_block
		//! \note In this context, "page" refers to an external_block
		//! \returns The total number of external_blocks under this data_block
		virtual uint get_page_count() const = 0;

		//! \brief Get a specific page of this data_block
		//! \note In this context, "page" refers to an external_block
		//! \throws out_of_range If page_num >= get_page_count()
		//! \param[in] page_num The ordinal of the external_block to get, zero based
		//! \returns The requested external_block
		virtual std::tr1::shared_ptr<external_block> get_page(uint page_num) const = 0;

		//! \brief Get the total logical size of this block
		//! \returns The total logical size of this block
		size_t get_total_size() const { return m_total_size; }


	protected:
		size_t m_total_size;    //!< the total or logical size (sum of all external child blocks)
	};

	//! \brief A data block which refers to other data blocks, in order to extend
	//! the physical size limit (8k) to a larger logical size.
	//!
	//! An extended_block is essentially a list of block_ids of other \ref 
	//! data_block, which themselves may be an extended_block or an \ref
	//! external_block. Ultimately they form a "data tree", the leafs of which
	//! form the "logical" contents of the block.
	//! 
	//! This class is an in memory representation of the disk::extended_block
	//! structure.
	//! \sa [MS-PST] 2.2.2.8.3.2
	//! \ingroup ndb_blockrelated
	class extended_block : 
		public data_block, 
		public std::tr1::enable_shared_from_this<extended_block>
	{
	public:
		//! \brief Construct an extended_block from disk
		//! \param[in] db The database context
		//! \param[in] info Information about this block
		//! \param[in] level The level of this extended block (1 or 2)
		//! \param[in] total_size The total logical size of this block
		//! \param[in] child_max_total_size The maximum logical size of a child block
		//! \param[in] page_max_count The maximum number of external blocks that can be contained in this block
		//! \param[in] child_page_max_count The maximum number of external blocks that can be contained in a child block
		//! \param[in] bi The \ref block_info for all child blocks
#ifndef BOOST_NO_RVALUE_REFERENCES
		extended_block(const shared_db_ptr& db, const block_info& info, ushort level, size_t total_size, size_t child_max_total_size, ulong page_max_count,
			ulong child_page_max_count, std::vector<block_id> bi) : data_block(db, info, total_size), m_child_max_total_size(child_max_total_size),
			m_child_max_page_count(child_page_max_count), m_max_page_count(page_max_count), m_level(level), m_block_info(std::move(bi)),
			m_child_blocks(m_block_info.size()) { }
#else
		extended_block(const shared_db_ptr& db, const block_info& info, ushort level, size_t total_size, size_t child_max_total_size, ulong page_max_count,
			ulong child_page_max_count, const std::vector<block_id>& bi) : data_block(db, info, total_size), m_child_max_total_size(child_max_total_size),
			m_child_max_page_count(child_page_max_count), m_max_page_count(page_max_count), m_level(level), m_block_info(bi), m_child_blocks(m_block_info.size()) { }
#endif

		//! \cond write_api
		//! \brief Construct an extended_block from disk
		//! \param[in] db The database context
		//! \param[in] level The level of this extended block (1 or 2)
		//! \param[in] total_size The total logical size of this block
		//! \param[in] child_max_total_size The maximum logical size of a child block
		//! \param[in] page_max_count The maximum number of external blocks that can be contained in this block
		//! \param[in] child_page_max_count The maximum number of external blocks that can be contained in a child block
		//! \param[in] child_blocks List of child blocks
#ifndef BOOST_NO_RVALUE_REFERENCES
		extended_block(const shared_db_ptr& db, ushort level, size_t total_size, size_t child_max_total_size, ulong page_max_count,
			ulong child_page_max_count,	std::vector<std::tr1::shared_ptr<data_block>> child_blocks) : data_block(db, block_info(), total_size),
			m_child_max_total_size(child_max_total_size), m_child_max_page_count(child_page_max_count), m_max_page_count(page_max_count), m_level(level),
			m_child_blocks(std::move(child_blocks)) { m_block_info.resize(m_child_blocks.size()); touch(); }
#else
		extended_block(const shared_db_ptr& db, ushort level, size_t total_size, size_t child_max_total_size, ulong page_max_count, ulong child_page_max_count,
			const std::vector<std::tr1::shared_ptr<data_block>>& child_blocks)	: data_block(db, block_info(), total_size),
			m_child_max_total_size(child_max_total_size), m_child_max_page_count(child_page_max_count), m_max_page_count(page_max_count), m_level(level),
			m_child_blocks(child_blocks) { m_block_info.resize(m_child_blocks.size()); touch(); }
#endif
		extended_block(const shared_db_ptr& db, ushort level, size_t total_size, size_t child_max_total_size, ulong page_max_count, ulong child_page_max_count);

		//! \copydoc data_block::write_raw(const byte* psrc_buffer, size_t size, ulong offset, std::tr1::shared_ptr<data_block>& presult)
		size_t write_raw(const byte* psrc_buffer, size_t size, ulong offset, std::tr1::shared_ptr<data_block>& presult);

		//! \copydoc data_block::resize(size_t size, std::tr1::shared_ptr<data_block>& presult)
		size_t resize(size_t size, std::tr1::shared_ptr<data_block>& presult);

		//! \brief Set block info for given child block of this block
		//! \param[in] index The index at which block info is to be updated
		//! \param[in] id The new id to be set in block info list
		void set_block_info(size_t index, block_id id) { m_block_info[index] = id; }
		//! \endcond

		//! \copydoc data_block::read_raw(byte* pdest_buffer, size_t size, ulong offset) const
		size_t read_raw(byte* pdest_buffer, size_t size, ulong offset) const;

		//! \brief Get the number of physical child pages in this block
		//! \note In this context, "page" refers to an external_block/extended_block
		//! \returns The total number of child blocks under this extended block
		uint get_page_count() const;

		//! \brief Get a specific page of this block
		//! \note In this context, "page" refers to an external_block/extended_block
		//! \param[in] page_num The ordinal of the child block to get, zero based
		//! \returns The requested external_block
		std::tr1::shared_ptr<external_block> get_page(uint page_num) const;

		//! \brief Get the "level" of this extended_block
		//! 
		//! A level 1 extended_block (or "xblock") points to external blocks.
		//! A level 2 extended_block (or "xxblock") points to other extended_blocks
		//! \returns 1 for an xblock, 2 for an xxblock
		ushort get_level() const { return m_level; }

		//! \brief Check if this block is an internal (i.e. other than external_block type) block
		//! \returns True
		bool is_internal() const { return true; }


	private:
		//! \brief Return the max logical size of this xblock
		size_t get_max_size() const { return m_child_max_total_size * m_max_page_count; }
		extended_block& operator=(const extended_block& other); // = delete
		data_block* get_child_block(uint index) const;

		const size_t m_child_max_total_size;									//!< maximum (logical) size of a child block
		const ulong m_child_max_page_count;										//!< maximum number of child blocks a child can contain
		const ulong m_max_page_count;											//!< maximum number of child blocks this block can contain

		const ushort m_level;													//!< The level of this block
		std::vector<block_id> m_block_info;										//!< block_ids of the child blocks in this tree
		mutable std::vector<std::tr1::shared_ptr<data_block>> m_child_blocks;	//!< Cached child blocks
	};

	//! \brief Contains actual data
	//!
	//! An external_block contains the actual data contents used by the higher
	//! layers. This data is also "encrypted", although the encryption/decryption
	//! process occurs immediately before/after going to disk, not here.
	//! \sa [MS-PST] 2.2.2.8.3.1
	//! \ingroup ndb_blockrelated
	class external_block : 
		public data_block, 
		public std::tr1::enable_shared_from_this<external_block>
	{
	public:
		//! \brief Construct an external_block from disk
		//! \param[in] db The database context
		//! \param[in] info Information about this block
		//! \param[in] max_size The maximum possible size of this block
		//! \param[in] buffer The actual external data (decoded)
#ifndef BOOST_NO_RVALUE_REFERENCES
		external_block(const shared_db_ptr& db, const block_info& info, size_t max_size, std::vector<byte> buffer)
			: data_block(db, info, info.size), m_max_size(max_size), m_buffer(std::move(buffer)) { }
#else
		external_block(const shared_db_ptr& db, const block_info& info, size_t max_size, const std::vector<byte>& buffer)
			: data_block(db, info, info.size), m_max_size(max_size), m_buffer(buffer) { }
#endif

		//! \cond write_api
		//! \brief Construct an external_block from disk
		//! \param[in] db The database context
		//! \param[in] max_size The maximum possible size of this block
		//! \param[in] current_size The actual size of this block
		external_block(const shared_db_ptr& db, size_t max_size, size_t current_size)
			: data_block(db, block_info(), current_size), m_max_size(max_size), m_buffer(current_size) { touch(); }
		//! \endcond

		//! \copydoc data_block::read_raw(byte* pdest_buffer, size_t size, ulong offset) const
		size_t read_raw(byte* pdest_buffer, size_t size, ulong offset) const;

		//! \cond write_api
		//! \copydoc data_block::write_raw(const byte* psrc_buffer, size_t size, ulong offset, std::tr1::shared_ptr<data_block>& presult)
		size_t write_raw(const byte* psrc_buffer, size_t size, ulong offset, std::tr1::shared_ptr<data_block>& presult);
		//! \endcond

		//! \brief Get the number of child pages in this block
		//! \note In this context, "page" refers to an external_block
		//! \returns 1
		uint get_page_count() const { return 1; }

		//! \brief Get a specific page of this block
		//! \note In this context, "page" refers to an external_block/extended_block
		//! \param[in] page_num The ordinal of the child block to get, zero based
		//! \returns The requested external_block
		std::tr1::shared_ptr<external_block> get_page(uint page_num) const;

		//! \cond write_api
		//! \copydoc data_block::resize(size_t size, std::tr1::shared_ptr<data_block>& presult)
		size_t resize(size_t size, std::tr1::shared_ptr<data_block>& presult);
		//! \endcond

		//! \brief Check if this block is an internal (i.e. other than external_block type) block
		//! \returns False
		bool is_internal() const { return false; }

		//! \brief Get the max possible size of this block
		//! \returns the max size of this block 
		size_t get_max_size() const { return m_max_size; }

	private:
		const size_t m_max_size;
		std::vector<byte> m_buffer;
	};


	//! \brief A block which contains information about subnodes
	//!
	//! Subnode blocks form a sort of "private NBT" for a node. This class is
	//! the root class of that hierarchy, with child classes for the leaf and
	//! non-leaf versions of a subnode block.
	//!
	//! This hierarchy also models the \ref btree_node structure, inheriting the 
	//! actual iteration and lookup logic.
	//! \sa [MS-PST] 2.2.2.8.3.3
	//! \ingroup ndb_blockrelated
	class subnode_block : 
		public block, 
		public virtual btree_node<node_id, subnode_info>
	{
	public:
		//! \brief Construct a block from disk
		//! \param[in] db The database context
		//! \param[in] info Information about this block
		//! \param[in] level The level of this subnode_block (0 or 1)
		subnode_block(const shared_db_ptr& db, const block_info& info, ushort level, size_t max_entries)
			: block(db, info), m_level(level), m_max_entries(max_entries){ }

		virtual ~subnode_block() { }

		//! \brief Get the level of this subnode_block
		//! \returns 0 for a leaf block, 1 otherwise
		ushort get_level() const { return m_level; }

		//! \brief Check if this block is an internal (i.e. other than external_block type) block
		//! \returns True
		bool is_internal() const { return true; }

		//! \cond write_api
		//! \brief Inserts a new element into the block
		//! \param[in] nID The sub node id of new element to be inserted
		//! \param[in] val The subnode info of new element to be inserted
		//! \returns A pair of new sub node blocks with the new element inserted
		virtual std::pair<std::tr1::shared_ptr<subnode_block>, std::tr1::shared_ptr<subnode_block>> insert(node_id nID, const subnode_info& val) = 0;

		//! \brief Modifies an existing element
		//! \param[in] nID The key of element to be modified
		//! \param[in] val The new value of the element to be modified
		//! \returns A new subnode block with the specified element modified
		virtual std::tr1::shared_ptr<subnode_block> modify(node_id nID, const subnode_info& val) = 0;

		//! \brief Deletes an existing element
		//! \param[in] nID The key of element to be removed
		//! \returns A new subnode block with the specified element removed
		virtual std::tr1::shared_ptr<subnode_block> remove(node_id nID) = 0;
		//! \endcond

		//! \brief Returns the maximum number of entries this subnode_block can hold
		//! \return The maximum number of entries this sub node block can hold
		size_t get_max_entries() const { return m_max_entries; }


	protected:
		ushort m_level;			 //!< Level of this subnode_block
		size_t m_max_entries;    //!< Maximum number of entries block can hold
	};

	//! \brief Contains references to subnode_leaf_blocks.
	//!
	//! Because of the width of a subnode_leaf_block and the relative scarcity of
	//! subnodes, it's actually pretty uncommon to encounter a subnode non-leaf
	//! block in practice. But it does occur, typically on large tables.
	//!
	//! This is the in memory version of one of these blocks. It forms the node
	//! of a tree, similar to the NBT, pointing to child blocks. There can only
	//! be one level of these - a subnode_nonleaf_block can not point to other
	//! subnode_nonleaf_blocks.
	//! \sa [MS-PST] 2.2.2.8.3.3.2
	//! \ingroup ndb_blockrelated
	class subnode_nonleaf_block : 
		public subnode_block, 
		public btree_node_nonleaf<node_id, subnode_info>, 
		public std::tr1::enable_shared_from_this<subnode_nonleaf_block>
	{
	public:
		//! \brief Construct a subnode_nonleaf_block from disk
		//! \param[in] db The database context
		//! \param[in] info Information about this block
		//! \param[in] subblocks Information about the child blocks
#ifndef BOOST_NO_RVALUE_REFERENCES
		subnode_nonleaf_block(const shared_db_ptr& db, const block_info& info, std::vector<std::pair<node_id, block_id>> subblocks, size_t max_entries)
			: subnode_block(db, info, 1, max_entries), m_subnode_info(std::move(subblocks)), m_child_blocks(m_subnode_info.size()) { }
		subnode_nonleaf_block(const shared_db_ptr& db, std::vector<std::pair<node_id, block_id>> subblocks, size_t max_entries)
			: subnode_block(db, block_info(), 1, max_entries), m_subnode_info(std::move(subblocks)), m_child_blocks(m_subnode_info.size()) { touch(); }
		subnode_nonleaf_block(const shared_db_ptr& db, std::vector<std::pair<node_id, block_id>> subblocks, std::vector<std::tr1::shared_ptr<subnode_block>> child_blocks, size_t max_entries)
			: subnode_block(db, block_info(), 1, max_entries), m_subnode_info(std::move(subblocks)), m_child_blocks(std::move(child_blocks)) { touch(); }
#else
		subnode_nonleaf_block(const shared_db_ptr& db, const block_info& info, const std::vector<std::pair<node_id, block_id>>& subblocks, size_t max_entries)
			: subnode_block(db, info, 1, max_entries), m_subnode_info(subblocks), m_child_blocks(m_subnode_info.size()) { }
		subnode_nonleaf_block(const shared_db_ptr& db, const std::vector<std::pair<node_id, block_id>> subblocks, size_t max_entries)
			: subnode_block(db, block_info(), 1, max_entries), m_subnode_info(subblocks), m_child_blocks(m_subnode_info.size()) { touch(); }
		subnode_nonleaf_block(const shared_db_ptr& db, std::vector<std::pair<node_id, block_id>> subblocks, std::vector<std::tr1::shared_ptr<subnode_block>> child_blocks, size_t max_entries)
			: subnode_block(db, block_info(), 1, max_entries), m_subnode_info(subblocks), m_child_blocks(child_blocks) { touch(); }
#endif

		//! \brief Construct a subnode_nonleaf_block from another subnode_nonleaf_block
		//! \param[in] other The other subnode_nonleaf_block
		subnode_nonleaf_block(const subnode_nonleaf_block& other) : subnode_block(other.get_db_ptr(), block_info(),1, other.get_max_entries()),
			m_subnode_info(other.m_subnode_info), m_child_blocks(other.m_child_blocks) { touch(); }

		//! \brief Get the node id of subnode stored at ordinal position
		//! \param[in] pos Ordinal postion of sub_node info entry whose id is to be found 
		//! \returns Node id of the subnode from subnode info entries at given position
		const node_id& get_key(uint pos) const { return m_subnode_info[pos].first; }

		//! \brief Get subnode block at given ordinal position under this block
		//! \param[in] Ordinal postion of the child subnode
		//! \returns The child subnode at given position under this block
		subnode_block* get_child(uint pos);

		//! \brief Get subnode block at given ordinal position under this block
		//! \param[in] Ordinal postion of the child subnode
		//! \returns The child subnode at given position under this block
		const subnode_block* get_child(uint pos) const;

		//! \brief Get the number of child subnode blocks under this block
		//! \returns The number of children of this block 
		uint num_values() const { return m_subnode_info.size(); }

		//! \brief Get subnode info at given position
		//! \param[in] pos Ordinal postion of sub_node info entry
		//! \returns The node info at given position
		std::pair<node_id, block_id> get_subnode_info(uint pos) const { return m_subnode_info[pos]; }

		//! \cond write_api
		//! \brief Update node info at given postion
		//! \param[in] pos Ordinal postion of sub_node info entry to be set
		//! \param[in] val the new value to be set
		void set_subnode_info(uint pos, std::pair<node_id, block_id>& val);

		//! \brief Insert a new child entry into this block
		//! \param[in] nID The subnode id
		//! \param[in] val The subnode info
		//! \returns Pair of new subnode_block with modified contents
		virtual std::pair<std::tr1::shared_ptr<subnode_block>, std::tr1::shared_ptr<subnode_block>> insert(node_id nID, const subnode_info& val);

		//! \brief Modify an existing child entry of this block
		//! \param[in] nID The subnode id
		//! \param[in] val The subnode info
		//! \returns A new subnode_block with modified contents
		virtual std::tr1::shared_ptr<subnode_block> modify(node_id nID, const subnode_info& val);

		//! \brief Remove a child entry from this block
		//! \param[in] nID The subnode id
		//! \returns A new subnode_block with modified contents
		virtual std::tr1::shared_ptr<subnode_block> remove(node_id nID);

		//! \brief Get child block at given position under this block
		//! \param[in] pos The ordinal postion of child subnode_block under this block
		//! \returns The child subnode_block at pos under this block
		std::tr1::shared_ptr<subnode_block> get_child(uint pos, bool dummy);
		//! \endcond

	private:
		std::vector<std::pair<node_id, block_id>> m_subnode_info;				  //!< Info about the sub-blocks
		mutable std::vector<std::tr1::shared_ptr<subnode_block>> m_child_blocks; //!< Cached sub-blocks (leafs)
	};

	//! \brief Contains the actual subnode information
	//!
	//! Typically a node will point directly to one of these. Because they are
	//! blocks, and thus up to 8k in size, they can hold information for about
	//! ~300 subnodes in a unicode store, and up to ~600 in an ANSI store.
	//! \sa [MS-PST] 2.2.2.8.3.3.1
	//! \ingroup ndb_blockrelated
	class subnode_leaf_block : 
		public subnode_block, 
		public btree_node_leaf<node_id, subnode_info>, 
		public std::tr1::enable_shared_from_this<subnode_leaf_block>
	{
	public:
		//! \brief Construct a subnode_leaf_block from disk
		//! \param[in] db The database context
		//! \param[in] info Information about this block
		//! \param[in] subnodes Information about the subnodes
#ifndef BOOST_NO_RVALUE_REFERENCES
		subnode_leaf_block(const shared_db_ptr& db, const block_info& info, std::vector<std::pair<node_id, subnode_info>> subnodes, size_t max_entries)
			: subnode_block(db, info, 0, max_entries), m_subnodes(std::move(subnodes)) { }
		subnode_leaf_block(const shared_db_ptr& db, std::vector<std::pair<node_id, subnode_info >> subnodes, size_t max_entries)
			: subnode_block(db, block_info(), 0, max_entries), m_subnodes(std::move(subnodes)) { touch(); }
#else
		subnode_leaf_block(const shared_db_ptr& db, const block_info& info, const std::vector<std::pair<node_id, subnode_info>>& subnodes, size_t max_entries)
			: subnode_block(db, info, 0, max_entries), m_subnodes(subnodes) { }
		subnode_leaf_block(const shared_db_ptr& db, const std::vector<std::pair<node_id, subnode_info >> subnodes, size_t max_entries)
			: subnode_block(db, block_info(), 0, max_entries), m_subnodes(subnodes) { touch(); }
#endif

		//! \brief Construct a subnode_leaf_block from another subnode_leaf_block
		//! \param[in] other The other subnode_leaf_block
		subnode_leaf_block(const subnode_leaf_block& other)	: subnode_block(other.get_db_ptr(), block_info(), 0, other.get_max_entries()),
			m_subnodes(other.m_subnodes) { touch(); }

		//! \brief Get subnode info at given ordinal position under this block
		//! \param[in] Ordinal postion of the subnode info entry
		//! \returns The subnode info entry stored at given position in this block
		const subnode_info& get_value(uint pos) const { return m_subnodes[pos].second; }

		//! \brief Get subnode id from entry stored at given position in this block
		//! \param[in] Ordinal postion of the subnode info entry
		//! \returns The subnode id from entry stored at given position in this block
		const node_id& get_key(uint pos) const { return m_subnodes[pos].first; }

		//! \brief Get the number of subnodes whose information is stored in this block
		//! \returns Then number of subnode information stored in this block 
		uint num_values() const { return m_subnodes.size(); }

		//! \cond write_api
		//! \brief Insert a new subnode info entry into this block
		//! \param[in] nID The subnode id
		//! \param[in] val The subnode info
		//! \returns Pair of new subnode_block with modified contents
		virtual std::pair<std::tr1::shared_ptr<subnode_block>, std::tr1::shared_ptr<subnode_block>> insert(node_id nID, const subnode_info& val);

		//! \brief Modify an existing subnode info entry of this block
		//! \param[in] nID The subnode id
		//! \param[in] val The subnode info
		//! \returns A new subnode_block with modified contents
		virtual std::tr1::shared_ptr<subnode_block> modify(node_id nID, const subnode_info& val);

		//! \brief Remove a subnode info entry from this block
		//! \param[in] nID The subnode id
		//! \returns A new subnode_block with modified contents
		virtual std::tr1::shared_ptr<subnode_block> remove(node_id nID);
		//! \endcond

	private:
		std::vector<std::pair<node_id, subnode_info>> m_subnodes;   //!< The actual subnode information
	};

} // end pstsdk namespace

inline void pstsdk::subnode_nonleaf_block::set_subnode_info(pstsdk::uint pos, std::pair<pstsdk::node_id, pstsdk::block_id>& val)
{
	pstsdk::thread_lock lock (&m_block_lock);
	lock.aquire_lock();
	m_subnode_info[pos] = val;
	lock.release_lock();
}

inline std::tr1::shared_ptr<pstsdk::subnode_block> pstsdk::subnode_nonleaf_block::remove(pstsdk::node_id nID)
{
	std::tr1::shared_ptr<subnode_nonleaf_block > copiedBlock = shared_from_this();

	if(copiedBlock.use_count() > 2)
	{
		std::tr1::shared_ptr<subnode_nonleaf_block> cnewBlock = std::tr1::make_shared<subnode_nonleaf_block>(*this);
		return cnewBlock->remove(nID);
	}

	touch(); // mutate ourselves inplace

	int pos = this->binary_search(nID);

	if(pos == -1)
	{
		throw key_not_found<node_id>(nID);
	}

	std::tr1::shared_ptr<subnode_block> result(this->get_child(pos)->remove(nID));

	if(result.get() == 0)
	{
		this->m_subnode_info.erase(this->m_subnode_info.begin() + pos);
		this->m_child_blocks.erase(this->m_child_blocks.begin() + pos);
		if(this->m_subnode_info.size() == 0)
		{
			return std::tr1::shared_ptr<subnode_nonleaf_block > (0);
		}
	}
	else
	{
		this->m_subnode_info[pos].first = result->get_key(0);
		this->m_subnode_info[pos].second = result->get_id();
		this->m_child_blocks[pos] = result;
	}

	return copiedBlock;
}

inline std::pair<std::tr1::shared_ptr<pstsdk::subnode_block>, std::tr1::shared_ptr<pstsdk::subnode_block>> pstsdk::subnode_nonleaf_block::insert(pstsdk::node_id nID, const pstsdk::subnode_info& val)
{
	std::tr1::shared_ptr<subnode_nonleaf_block > copiedBlock1 = shared_from_this();

	if(copiedBlock1.use_count() > 2)
	{
		std::tr1::shared_ptr<subnode_nonleaf_block> cnewBlock = std::tr1::make_shared<subnode_nonleaf_block>(*this);
		return cnewBlock->insert(nID, val);
	}

	touch(); // mutate ourselves inplace

	std::tr1::shared_ptr<subnode_nonleaf_block > copiedBlock2 (0);

	int pos = this->binary_search(nID);

	if(pos == -1)
	{
		pos = 0;
	}

	std::pair<std::tr1::shared_ptr<subnode_block>, std::tr1::shared_ptr<subnode_block>> result (this->get_child(pos)->insert(nID, val));

	this->m_subnode_info[pos].first = result.first->get_key(0);
	this->m_subnode_info[pos].second = result.first->get_id();
	this->m_child_blocks[pos] = result.first;

	if(result.second.get() != 0)
	{       
		this->m_subnode_info.insert(this->m_subnode_info.begin() + pos + 1, std::make_pair(result.second->get_key(0), result.second->get_id()));
		this->m_child_blocks.insert(this->m_child_blocks.begin() + pos + 1, result.second);

		if(this->m_subnode_info.size() > this->get_max_entries())
		{
			copiedBlock2 = std::tr1::make_shared<subnode_nonleaf_block>(this->get_db_ptr(), std::vector<std::pair<node_id, block_id>> (), this->get_max_entries());
			copiedBlock2->m_subnode_info.push_back(this->m_subnode_info.back());
			copiedBlock2->m_child_blocks.push_back(this->m_child_blocks.back());
			this->m_subnode_info.pop_back();
			this->m_child_blocks.pop_back();
		}
	}

	return std::make_pair(copiedBlock1, copiedBlock2);
}

inline std::tr1::shared_ptr<pstsdk::subnode_block> pstsdk::subnode_nonleaf_block::modify(pstsdk::node_id nID, const pstsdk::subnode_info& val)
{
	std::tr1::shared_ptr<subnode_nonleaf_block> copiedBlock = shared_from_this();

	if(copiedBlock.use_count() > 2)
	{
		std::tr1::shared_ptr<subnode_nonleaf_block> cnewBlock(std::tr1::make_shared<subnode_nonleaf_block>(*this));
		return cnewBlock->modify(nID, val);
	}

	touch(); // mutate ourselves inplace

	int pos = this->binary_search(nID);

	if(pos == -1)
	{
		throw key_not_found<node_id>(nID);
	}    

	std::tr1::shared_ptr<subnode_block> result(this->get_child(pos)->modify(nID, val));

	this->m_subnode_info[pos].second =result->get_id();
	this->m_child_blocks[pos] = result;

	return copiedBlock;
}

inline std::tr1::shared_ptr<pstsdk::subnode_block> pstsdk::subnode_leaf_block::remove(pstsdk::node_id nID)
{
	std::tr1::shared_ptr<subnode_leaf_block> copiedBlock = shared_from_this();

	if(copiedBlock.use_count() > 2)
	{
		std::tr1::shared_ptr<subnode_leaf_block> cnewBlock = std::tr1::make_shared<subnode_leaf_block>(*this);
		return cnewBlock->remove(nID);
	}

	touch(); // mutate ourselves inplace

	int pos = this->binary_search(nID);

	if(pos == -1)
	{
		throw key_not_found<node_id>(nID);
	}

	if(this->get_key(pos) != nID)
	{
		throw key_not_found<node_id>(nID);
	}

	this->m_subnodes.erase(this->m_subnodes.begin() + pos);

	if(this->num_values() == 0)
	{
		return std::tr1::shared_ptr<subnode_leaf_block>(0);
	}

	return copiedBlock;
}

inline std::pair<std::shared_ptr<pstsdk::subnode_block>, std::tr1::shared_ptr<pstsdk::subnode_block>> pstsdk::subnode_leaf_block::insert(pstsdk::node_id nID, const pstsdk::subnode_info& val)
{
	std::tr1::shared_ptr<subnode_leaf_block> copiedBlock1 = shared_from_this();

	if(copiedBlock1.use_count() > 2)
	{
		std::tr1::shared_ptr<subnode_leaf_block> cnewBlock = std::tr1::make_shared<subnode_leaf_block >(*this);
		return cnewBlock->insert(nID, val);
	}

	touch(); // mutate ourselves inplace

	std::tr1::shared_ptr<subnode_leaf_block > copiedBlock2 (0);

	int pos = this->binary_search(nID);
	uint idx = pos + 1;

	if((pos > -1) && (static_cast<unsigned int>(pos) < this->m_subnodes.size()) && (this->get_key(pos) == nID))
	{
		// If key already exists, behave like modify
		this->m_subnodes[pos].second = val;
	}
	else
	{
		this->m_subnodes.insert(this->m_subnodes.begin() + idx, std::make_pair(nID, val));

		if(this->m_subnodes.size() > this->get_max_entries())
		{
			copiedBlock2 = std::tr1::make_shared<subnode_leaf_block >(this->get_db_ptr(), std::vector<std::pair <node_id, subnode_info>>(), this->get_max_entries());
			copiedBlock2->m_subnodes.push_back(this->m_subnodes.back());
			this->m_subnodes.pop_back();
		}
	}

	return std::make_pair(copiedBlock1, copiedBlock2);
}

inline std::tr1::shared_ptr<pstsdk::subnode_block> pstsdk::subnode_leaf_block::modify(node_id nID, const subnode_info& val)
{
	std::tr1::shared_ptr<subnode_leaf_block> copiedBlock = shared_from_this();

	if(copiedBlock.use_count() > 2)
	{
		std::tr1::shared_ptr<subnode_leaf_block> cnewBlock(std::tr1::make_shared<subnode_leaf_block>(*this));
		return cnewBlock->modify(nID, val);
	}

	touch(); // mutate ourselves inplace

	int pos = this->binary_search(nID);

	if(pos == -1)
	{
		throw key_not_found<node_id>(nID);
	}

	if(this->get_key(pos) != nID)
	{
		throw key_not_found<node_id>(nID);
	}

	this->m_subnodes[pos].second = val;

	return copiedBlock;
}

inline pstsdk::node pstsdk::subnode_transform_info::operator()(const pstsdk::subnode_info& info) const
{ 
	return node(m_parent, info); 
}

inline pstsdk::block_id pstsdk::node_impl::get_data_id() const
{ 
	if(m_pdata)
		return m_pdata->get_id();

	return m_original_data_id;
}

inline pstsdk::block_id pstsdk::node_impl::get_sub_id() const
{ 
	if(m_psub)
		return m_psub->get_id();

	return m_original_sub_id;
}

inline size_t pstsdk::node_impl::size() const
{
	return ensure_data_block()->get_total_size();
}

inline size_t pstsdk::node_impl::get_page_size(uint page_num) const
{
	return ensure_data_block()->get_page(page_num)->get_total_size();
}

inline pstsdk::uint pstsdk::node_impl::get_page_count() const 
{ 
	return ensure_data_block()->get_page_count(); 
}

inline size_t pstsdk::node_impl::read(std::vector<byte>& buffer, ulong offset) const
{ 
	return ensure_data_block()->read(buffer, offset); 
}

inline size_t pstsdk::node_impl::read_raw(byte* pdest_buffer, size_t size, ulong offset) const
{ 
	return ensure_data_block()->read_raw(pdest_buffer, size, offset); 
}

template<typename T> 
inline T pstsdk::node_impl::read(ulong offset) const
{
	return ensure_data_block()->read<T>(offset); 
}

inline size_t pstsdk::node_impl::read(std::vector<byte>& buffer, uint page_num, ulong offset) const
{ 
	return ensure_data_block()->get_page(page_num)->read(buffer, offset); 
}

template<typename T> 
inline T pstsdk::node_impl::read(uint page_num, ulong offset) const
{
	return ensure_data_block()->get_page(page_num)->read<T>(offset); 
}

//! \cond write_api
inline size_t pstsdk::node_impl::write(const std::vector<byte>& buffer, ulong offset)
{
	return ensure_data_block()->write(buffer, offset, m_pdata);
}

inline size_t pstsdk::node_impl::write_raw(const byte* pdest_buffer, size_t size, ulong offset)
{
	ensure_data_block();
	return m_pdata->write_raw(pdest_buffer, size, offset, m_pdata);
}

template<typename T> 
inline void pstsdk::node_impl::write(const T& obj, ulong offset)
{
	return ensure_data_block()->write<T>(obj, offset, m_pdata);
}

inline size_t pstsdk::node_impl::write(const std::vector<byte>& buffer, uint page_num, ulong offset)
{
	return ensure_data_block()->write(buffer, page_num * get_page_size(0) + offset, m_pdata);
}

template<typename T> 
inline void pstsdk::node_impl::write(const T& obj, uint page_num, ulong offset)
{
	return ensure_data_block()->write<T>(obj, page_num * get_page_size(0) + offset, m_pdata);
}

inline size_t pstsdk::node_impl::resize(size_t size)
{
	return ensure_data_block()->resize(size, m_pdata);
}
//! \endcond

inline pstsdk::data_block* pstsdk::node_impl::ensure_data_block() const
{ 
	pstsdk::thread_lock lock(&m_node_lock);
	lock.aquire_lock();

	if(!m_pdata) 
	{
		m_pdata = m_db->read_data_block(m_original_data_id); 
	}

	lock.release_lock();

	return m_pdata.get();
}

inline pstsdk::subnode_block* pstsdk::node_impl::ensure_sub_block() const
{ 
	pstsdk::thread_lock lock(&m_node_lock);
	lock.aquire_lock();

	if(!m_psub) 
	{
		m_psub = m_db->read_subnode_block(m_original_sub_id); 
	}

	lock.release_lock();

	return m_psub.get();
}

//! \cond write_api
inline void pstsdk::block::touch()
{ 
	pstsdk::thread_lock lock(&m_block_lock);
	lock.aquire_lock();

	if(!m_modified)
	{
		m_modified = true; 
		m_address = 0;
		m_size = 0;
		m_id = get_db_ptr()->alloc_bid(is_internal()); 
	}

	lock.release_lock();
}
//! \endcond

inline std::streamsize pstsdk::node_stream_device::read(char* pbuffer, std::streamsize n)
{
	size_t read = m_pnode->read_raw(reinterpret_cast<byte*>(pbuffer), static_cast<size_t>(n), static_cast<size_t>(m_pos));
	m_pos += read;

	if(read)
		return read;
	else
		return -1;
}

//! \cond write_api
inline std::streamsize pstsdk::node_stream_device::write(const char* pbuffer, std::streamsize n)
{
	size_t written = m_pnode->write_raw(reinterpret_cast<const byte*>(pbuffer), static_cast<size_t>(n), static_cast<size_t>(m_pos));
	m_pos += written;
	return written;
}
//! \endcond

inline std::streampos pstsdk::node_stream_device::seek(boost::iostreams::stream_offset off, std::ios_base::seekdir way)
{
#if defined(_MSC_VER) && (_MSC_VER < 1600)
#pragma warning(push)
#pragma warning(disable:4244)
#endif
	if(way == std::ios_base::beg)
		m_pos = off;
	else if(way == std::ios_base::end)
		m_pos = m_pnode->size() + off;
	else
		m_pos += off;
#if defined(_MSC_VER) && (_MSC_VER < 1600)
#pragma warning(pop)
#endif

	if(m_pos < 0)
		m_pos = 0;
	else if(static_cast<size_t>(m_pos) > m_pnode->size())
		m_pos = m_pnode->size();

	return m_pos;
}

inline pstsdk::subnode_block* pstsdk::subnode_nonleaf_block::get_child(uint pos)
{
	pstsdk::thread_lock lock(&m_block_lock);
	lock.aquire_lock();

	if(m_child_blocks[pos] == NULL)
	{
		m_child_blocks[pos] = get_db_ptr()->read_subnode_block(m_subnode_info[pos].second);
	}

	lock.release_lock();

	return m_child_blocks[pos].get();
}

inline const pstsdk::subnode_block* pstsdk::subnode_nonleaf_block::get_child(uint pos) const
{
	pstsdk::thread_lock lock(&m_block_lock);
	lock.aquire_lock();

	if(m_child_blocks[pos] == NULL)
	{
		m_child_blocks[pos] = get_db_ptr()->read_subnode_block(m_subnode_info[pos].second);
	}

	lock.release_lock();

	return m_child_blocks[pos].get();
}

inline size_t pstsdk::data_block::read(std::vector<byte>& buffer, ulong offset) const
{
	size_t read_size = buffer.size();

	if(read_size > 0)
	{
		if(offset >= get_total_size())
			throw std::out_of_range("offset >= size()");

		read_size = read_raw(&buffer[0], read_size, offset);
	}

	return read_size;
}

template<typename T> 
inline T pstsdk::data_block::read(ulong offset) const
{
	if(offset >= get_total_size())
		throw std::out_of_range("offset >= size()");

	if(sizeof(T) + offset > get_total_size())
		throw std::out_of_range("sizeof(T) + offset >= size()");

	T t;
	read_raw(reinterpret_cast<byte*>(&t), sizeof(T), offset);

	return t;
}

//! \cond write_api
inline std::tr1::shared_ptr<pstsdk::subnode_block> pstsdk::subnode_nonleaf_block::get_child(uint pos, bool dummy)
{
	pstsdk::thread_lock lock(&m_block_lock);
	lock.aquire_lock();

	if(m_child_blocks[pos] == NULL)
	{
		m_child_blocks[pos] = get_db_ptr()->read_subnode_block(m_subnode_info[pos].second);
	}

	lock.release_lock();

	return m_child_blocks[pos];
}

inline size_t pstsdk::data_block::write(const std::vector<byte>& buffer, ulong offset, std::tr1::shared_ptr<data_block>& presult)
{
	size_t write_size = buffer.size();

	if(write_size > 0)
	{
		if(offset >= get_total_size())
			throw std::out_of_range("offset >= size()");

		write_size = write_raw(&buffer[0], write_size, offset, presult);
	}

	return write_size;
}

template<typename T> 
void pstsdk::data_block::write(const T& buffer, ulong offset, std::tr1::shared_ptr<data_block>& presult)
{
	if(offset >= get_total_size())
		throw std::out_of_range("offset >= size()");

	if(sizeof(T) + offset > get_total_size())
		throw std::out_of_range("sizeof(T) + offset >= size()");

	(void)write_raw(reinterpret_cast<const byte*>(&buffer), sizeof(T), offset, presult);
}
//! \endcond

inline pstsdk::uint pstsdk::extended_block::get_page_count() const
{
	assert(m_child_max_total_size % m_child_max_page_count == 0);
	uint page_size = m_child_max_total_size / m_child_max_page_count;
	uint page_count = (get_total_size() / page_size) + ((get_total_size() % page_size) != 0 ? 1 : 0);
	assert(get_level() == 2 || page_count == m_block_info.size());

	return page_count;
}

//! \cond write_api
inline pstsdk::extended_block::extended_block(const shared_db_ptr& db, ushort level, size_t total_size, size_t child_max_total_size, ulong page_max_count, ulong child_page_max_count)
	: data_block(db, block_info(), total_size), m_child_max_total_size(child_max_total_size), m_child_max_page_count(child_page_max_count), m_max_page_count(page_max_count), m_level(level)
{
	int total_subblocks = total_size / m_child_max_total_size;

	if(total_size % m_child_max_total_size != 0)
		total_subblocks++;

	m_child_blocks.resize(total_subblocks);
	m_block_info.resize(total_subblocks, 0);

	touch();
}
//! \endcond

inline pstsdk::data_block* pstsdk::extended_block::get_child_block(uint index) const
{
	pstsdk::thread_lock lock(&m_block_lock);
	lock.aquire_lock();

	if(index >= m_child_blocks.size())
		throw std::out_of_range("index >= m_child_blocks.size()");

	if(m_child_blocks[index] == NULL)
	{
		if(m_block_info[index] == 0)
		{
			if(get_level() == 1)
				m_child_blocks[index] = get_db_ptr()->create_external_block(m_child_max_total_size);
			else
				m_child_blocks[index] = get_db_ptr()->create_extended_block(m_child_max_total_size);
		}
		else
			m_child_blocks[index] = get_db_ptr()->read_data_block(m_block_info[index]);
	}

	lock.release_lock();

	return m_child_blocks[index].get();
}

inline std::tr1::shared_ptr<pstsdk::external_block> pstsdk::extended_block::get_page(uint page_num) const
{
	uint page = page_num / m_child_max_page_count;
	return get_child_block(page)->get_page(page_num % m_child_max_page_count);
}

inline std::tr1::shared_ptr<pstsdk::external_block> pstsdk::external_block::get_page(uint index) const
{
	if(index != 0)
		throw std::out_of_range("index > 0");

	return std::tr1::const_pointer_cast<external_block>(this->shared_from_this());
}

inline size_t pstsdk::external_block::read_raw(byte* pdest_buffer, size_t size, ulong offset) const
{
	size_t read_size = size;

	assert(offset <= get_total_size());

	if(offset + size > get_total_size())
		read_size = get_total_size() - offset;

	memcpy(pdest_buffer, &m_buffer[offset], read_size);

	return read_size;
}

//! \cond write_api
inline size_t pstsdk::external_block::write_raw(const byte* psrc_buffer, size_t size, ulong offset, std::tr1::shared_ptr<data_block>& presult)
{
	std::tr1::shared_ptr<pstsdk::external_block> pblock = shared_from_this();

	if(pblock.use_count() > 2) // one for me, one for the caller
	{
		std::tr1::shared_ptr<pstsdk::external_block> pnewblock(new external_block(*this));
		return pnewblock->write_raw(psrc_buffer, size, offset, presult);
	}

	touch(); // mutate ourselves inplace

	assert(offset <= get_total_size());

	size_t write_size = size;

	if(offset + size > get_total_size())
		write_size = get_total_size() - offset;

	memcpy(&m_buffer[0]+offset, psrc_buffer, write_size);

	// assign out param
#ifndef BOOST_NO_RVALUE_REFERENCES
	presult = std::move(pblock);
#else
	presult = pblock;
#endif

	return write_size;
}
//! \endcond

inline size_t pstsdk::extended_block::read_raw(byte* pdest_buffer, size_t size, ulong offset) const
{
	assert(offset <= get_total_size());

	if(offset + size > get_total_size())
		size = get_total_size() - offset;

	byte* pend = pdest_buffer + size;

	size_t total_bytes_read = 0;

	while(pdest_buffer != pend)
	{
		// the child this read starts on
		uint child_pos = offset / m_child_max_total_size;
		// offset into the child block this read starts on
		ulong child_offset = offset % m_child_max_total_size;

		// call into our child to read the data
		size_t bytes_read = get_child_block(child_pos)->read_raw(pdest_buffer, size, child_offset);
		assert(bytes_read <= size);

		// adjust pointers accordingly
		pdest_buffer += bytes_read;
		offset += bytes_read;
		size -= bytes_read;
		total_bytes_read += bytes_read;

		assert(pdest_buffer <= pend);
	}

	return total_bytes_read;
}

//! \cond write_api
inline size_t pstsdk::extended_block::write_raw(const byte* psrc_buffer, size_t size, ulong offset, std::tr1::shared_ptr<data_block>& presult)
{
	std::tr1::shared_ptr<extended_block> pblock = shared_from_this();

	if(pblock.use_count() > 2) // one for me, one for the caller
	{
		std::tr1::shared_ptr<extended_block> pnewblock(new extended_block(*this));
		return pnewblock->write_raw(psrc_buffer, size, offset, presult);
	}

	touch(); // mutate ourselves inplace

	assert(offset <= get_total_size());

	if(offset + size > get_total_size())
		size = get_total_size() - offset;

	const byte* pend = psrc_buffer + size;
	size_t total_bytes_written = 0;

	while(psrc_buffer != pend)
	{
		// the child this read starts on
		uint child_pos = offset / m_child_max_total_size;
		// offset into the child block this read starts on
		ulong child_offset = offset % m_child_max_total_size;

		// call into our child to write the data
		size_t bytes_written = get_child_block(child_pos)->write_raw(psrc_buffer, size, child_offset, m_child_blocks[child_pos]);
		assert(bytes_written <= size);

		// adjust pointers accordingly
		psrc_buffer += bytes_written;
		offset += bytes_written;
		size -= bytes_written;
		total_bytes_written += bytes_written;

		assert(psrc_buffer <= pend);
	}

	// assign out param
#ifndef BOOST_NO_RVALUE_REFERENCES
	presult = std::move(pblock);
#else
	presult = pblock;
#endif

	return total_bytes_written;
}

inline size_t pstsdk::external_block::resize(size_t size, std::tr1::shared_ptr<data_block>& presult)
{
	std::tr1::shared_ptr<external_block> pblock = shared_from_this();

	if(pblock.use_count() > 2) // one for me, one for the caller
	{
		std::tr1::shared_ptr<external_block> pnewblock(new external_block(*this));
		return pnewblock->resize(size, presult);
	}

	touch(); // mutate ourselves inplace

	m_buffer.resize(size > m_max_size ? m_max_size : size);
	m_total_size = m_buffer.size();

	if(size > get_max_size())
	{
		// we need to create an extended_block with us as the first entry
		std::tr1::shared_ptr<extended_block> pnewxblock = get_db_ptr()->create_extended_block(pblock);
		return pnewxblock->resize(size, presult);
	}

	// assign out param
#ifndef BOOST_NO_RVALUE_REFERENCES
	presult = std::move(pblock);
#else
	presult = pblock;
#endif

	return size;
}

inline size_t pstsdk::extended_block::resize(size_t size, std::tr1::shared_ptr<data_block>& presult)
{
	// calculate the number of subblocks needed
	uint old_num_subblocks = m_block_info.size();
	uint num_subblocks = size / m_child_max_total_size;

	if(size % m_child_max_total_size != 0)
		num_subblocks++;

	if(num_subblocks > m_max_page_count)
		num_subblocks = m_max_page_count;

	// defer to child if it's 1 (or less)
	assert(!m_child_blocks.empty());

	if(num_subblocks < 2)
		return get_child_block(0)->resize(size, presult);

	std::tr1::shared_ptr<extended_block> pblock = shared_from_this();

	if(pblock.use_count() > 2) // one for me, one for the caller
	{
		std::tr1::shared_ptr<extended_block> pnewblock(new extended_block(*this));
		return pnewblock->resize(size, presult);
	}

	touch(); // mutate ourselves inplace

	// set the total number of subblocks needed
	m_block_info.resize(num_subblocks, 0);
	m_child_blocks.resize(num_subblocks);

	if(old_num_subblocks < num_subblocks)
		get_child_block(old_num_subblocks-1)->resize(m_child_max_total_size, m_child_blocks[old_num_subblocks-1]);

	// size the last subblock appropriately
	size_t last_child_size = size - (num_subblocks-1) * m_child_max_total_size;
	get_child_block(num_subblocks-1)->resize(last_child_size, m_child_blocks[num_subblocks-1]);

	if(size > get_max_size())
	{
		m_total_size = get_max_size();

		if(get_level() == 2)
			throw can_not_resize("size > max_size");

		// we need to create a level 2 extended_block with us as the first entry
		std::tr1::shared_ptr<extended_block> pnewxblock = get_db_ptr()->create_extended_block(pblock);
		return pnewxblock->resize(size, presult);
	}

	// assign out param
	m_total_size = size;
#ifndef BOOST_NO_RVALUE_REFERENCES
	presult = std::move(pblock);
#else
	presult = pblock;
#endif

	return size;
}
//! \endcond

inline pstsdk::const_subnodeinfo_iterator pstsdk::node_impl::subnode_info_begin() const
{
	const subnode_block* pblock = ensure_sub_block();
	return pblock->begin();
}

inline pstsdk::const_subnodeinfo_iterator pstsdk::node_impl::subnode_info_end() const
{
	const subnode_block* pblock = ensure_sub_block();
	return pblock->end();
}

inline pstsdk::node pstsdk::node_impl::lookup(node_id id) const
{
	return node(std::tr1::const_pointer_cast<node_impl>(shared_from_this()), ensure_sub_block()->lookup(id));
}

//! \cond write_api
inline pstsdk::node pstsdk::node_impl::create_subnode(node_id id)
{
	pstsdk::subnode_info subnd_info = {id, 0, 0};
	bool node_exists = false;

	// make sure this subnode already does not exist
	try  
	{  
		ensure_sub_block()->lookup(id);  
		node_exists = true;
	}  
	catch(pstsdk::key_not_found<node_id>) { }  

	if(node_exists)
		throw pstsdk::duplicate_key<node_id>( id) ; 
	else
		return pstsdk::node(std::tr1::const_pointer_cast<node_impl>(shared_from_this()), subnd_info);
}

inline void pstsdk::node_impl::delete_subnode(node_id id)
{
	// ensure subnode block
	ensure_sub_block();

	std::vector<pstsdk::block_id> blk_list(0);
	std::vector<pstsdk::bbt_update_action> bbt_updates;
	build_subnode_block_list(m_psub, blk_list);

	// lookup subnode information
	pstsdk::subnode_info sbnd_inf = m_psub->lookup(id);
	pstsdk::node sb_nd(shared_from_this(), sbnd_inf);

	// drop subnodes if any for this subnode
	sb_nd.drop_subnodes();

	// drop data blocks for this subnode
	sb_nd.drop_data_blocks();

	// remove entry from parents' subnode tree
	pstsdk::thread_lock lock(&m_node_lock);
	lock.aquire_lock();
	m_psub = m_psub->remove(id);

	// free blocks if subnode block tree goes empty
	if(!m_psub)
	{
		m_original_sub_id = 0;
		drop_block_ref_count(blk_list);
		m_db->update_btree(bbt_updates);
	}

	// reflect changes up to topmost node
	save_node(true);
	lock.release_lock();
}

inline void pstsdk::node_impl::drop_block_ref_count(std::vector<pstsdk::block_id>& blk_list)
{
	std::vector<pstsdk::bbt_update_action> bbt_updates(0);

	for(std::vector<pstsdk::block_id>::iterator blk_iter = blk_list.begin(); blk_iter != blk_list.end(); ++blk_iter)
	{
		pstsdk::block_info blk_info = { *blk_iter, 0, 0, 2 };
		bbt_updates.push_back(m_db->create_bbt_update_action(blk_info, true));
	}

	m_db->update_btree(bbt_updates);
	blk_list.clear();
}

inline void pstsdk::node_impl::build_subnode_block_list(std::tr1::shared_ptr<pstsdk::subnode_block>& block, std::vector<pstsdk::block_id>& blk_list)
{
	block_id blk_id = block->get_id();

	if(blk_id == 0)
		return;

	blk_list.push_back(blk_id);

	if(block->get_level() > 0)
	{
		std::tr1::shared_ptr<pstsdk::subnode_nonleaf_block> nonleaf_block = std::tr1::static_pointer_cast<pstsdk::subnode_nonleaf_block>(block);

		for(size_t pos = 0; pos < nonleaf_block->num_values(); ++pos)
		{
			build_subnode_block_list(nonleaf_block->get_child(pos, false), blk_list);
		}
	}
}

inline void pstsdk::node_impl::build_data_block_list(std::tr1::shared_ptr<pstsdk::data_block>& block, std::vector<pstsdk::block_id>& blk_list)
{
	block_id blk_id = block->get_id();

	if(blk_id == 0)
		return;

	blk_list.push_back(blk_id);

	if(block->is_internal())
	{
		std::tr1::shared_ptr<pstsdk::extended_block> int_block = std::tr1::static_pointer_cast<pstsdk::extended_block>(block);

		for(size_t ind = 0; ind < int_block->get_page_count(); ++ind)
		{
			std::tr1::shared_ptr<pstsdk::data_block> child_block = int_block->get_page(ind);
			build_data_block_list(child_block, blk_list);
		}
	}
}

inline void pstsdk::node_impl::drop_subnodes()
{
	// ensure subnode block
	ensure_sub_block();

	std::vector<node_id> sub_nd_list;

	// delete all subnodes for this node
	for(pstsdk::const_subnodeinfo_iterator sb_iter = m_psub->begin(); sb_iter != m_psub->end(); sb_iter++)
	{
		sub_nd_list.push_back(sb_iter->id);
	}

	for(size_t ind = 0; ind < sub_nd_list.size(); ++ind)
		delete_subnode(sub_nd_list[ind]);

	// drop subnode block tree
	std::vector<pstsdk::bbt_update_action> bbt_updates(0);
	drop_subnode_blocks(m_psub, bbt_updates);
	m_db->update_btree(bbt_updates);

	m_psub.reset();
	m_original_sub_id = 0;
}

inline void pstsdk::node_impl::drop_data_blocks()
{
	// ensure data block
	ensure_data_block();

	// drop data block tree
	std::vector<pstsdk::bbt_update_action> bbt_updates(0);
	drop_data_blocks(m_pdata, bbt_updates);
	m_db->update_btree(bbt_updates);
	m_pdata.reset();
	m_original_data_id = 0;
}

inline void pstsdk::node_impl::drop_data_blocks(std::tr1::shared_ptr<pstsdk::data_block>& block, std::vector<pstsdk::bbt_update_action>& bbt_updates)
{
	if(block->get_id() == 0)
		return;

	// drop ref count for all blocks in data block tree
	pstsdk::block_info new_blk_info = { block->get_id(), block->get_address(), block->get_disk_size(), 2 };
	bbt_updates.push_back(m_db->create_bbt_update_action(new_blk_info, true));

	if(block->is_internal())
	{
		std::tr1::shared_ptr<pstsdk::extended_block> int_block = std::tr1::static_pointer_cast<pstsdk::extended_block>(block);

		for(size_t ind = 0; ind < int_block->get_page_count(); ++ind)
		{
			std::tr1::shared_ptr<pstsdk::data_block> child_block = int_block->get_page(ind);
			drop_data_blocks(child_block, bbt_updates);
		}
	}
}

inline void pstsdk::node_impl::drop_subnode_blocks(std::tr1::shared_ptr<pstsdk::subnode_block>& block, std::vector<pstsdk::bbt_update_action>& bbt_updates)
{
	if(block->get_id() == 0)
		return;

	// drop ref count for all blocks in subnode block tree
	pstsdk::block_info new_blk_info = { block->get_id(), block->get_address(), block->get_disk_size(), 2 };
	bbt_updates.push_back(m_db->create_bbt_update_action(new_blk_info, true));

	if(block->get_level() > 0)
	{
		std::tr1::shared_ptr<pstsdk::subnode_nonleaf_block> nonleaf_block = std::tr1::static_pointer_cast<pstsdk::subnode_nonleaf_block>(block);

		for(size_t pos = 0; pos < nonleaf_block->num_values(); ++pos)
		{
			drop_subnode_blocks(nonleaf_block->get_child(pos, false), bbt_updates);
		}
	}
}

inline void pstsdk::node_impl::save_subnode(pstsdk::node& sb_nd)
{
	ensure_sub_block();

	bool modify = false;
	pstsdk::subnode_info prev_sbnd_inf = { 0, 0, 0 };
	pstsdk::subnode_info new_sbnd_inf = { sb_nd.get_id(), sb_nd.get_data_id(), sb_nd.get_sub_id() };

	try
	{
		prev_sbnd_inf = m_psub->lookup(new_sbnd_inf.id);
		modify = true;
	}
	catch(key_not_found<node_id>){ modify = false; }

	if(modify)
	{
		// save modified subnode
		pstsdk::thread_lock lock(&m_node_lock);
		lock.aquire_lock();
		m_psub = m_psub->modify(prev_sbnd_inf.id, new_sbnd_inf);
		lock.release_lock();
	}
	else
	{ 
		pstsdk::thread_lock lock(&m_node_lock);

		// save newly created subnode.
		std::pair<std::tr1::shared_ptr<pstsdk::subnode_block>, std::tr1::shared_ptr<pstsdk::subnode_block>> result = m_psub->insert(new_sbnd_inf.id, new_sbnd_inf);
		lock.aquire_lock();
		m_psub = result.first;
		lock.release_lock();

		// case when subnode block root is a leaf block and there is a split in it
		if(m_psub->get_level() == 0 && result.second != 0)
		{
			std::tr1::shared_ptr<pstsdk::subnode_nonleaf_block> new_sub_blk = m_db->create_subnode_nonleaf_block(result.first);

			for(pstsdk::const_subnodeinfo_iterator itr = result.second->begin(); itr != result.second->end(); ++itr)
			{
				new_sub_blk->insert(itr->id, *itr);
			}

			lock.aquire_lock();
			m_psub = new_sub_blk;
			lock.release_lock();
		}
	}

	// save all data and subnode blocks for this subnode
	sb_nd.save_blocks();
}

inline void pstsdk::node_impl::save_node(bool recursive)
{
	ensure_data_block();
	ensure_sub_block();

	// check if this is a subnode
	if(is_subnode())
	{
		pstsdk::node_info nd_info = {get_id(), get_data_id(), get_sub_id(), get_parent_id()};
		m_pcontainer_node->save_subnode(pstsdk::node(m_db, nd_info, get_data_block(), get_subnode_block()));

		// recursive save to until topmost node. Used by delete subnode
		if(recursive) { m_pcontainer_node->save_node(); }
	}
	else
	{
		// perform nbt operations
		std::vector<pstsdk::nbt_update_action> nbt_updates;
		pstsdk::node_info new_nd_info = { get_id(), get_data_id(), get_sub_id(), get_parent_id() };
		nbt_updates.push_back(m_db->create_nbt_update_action(new_nd_info));

		// save all data and subnode blocks for this node
		save_blocks();

		// update nbt with all the changes
		m_db->update_btree(nbt_updates);
	}
}

inline void pstsdk::node_impl::save_blocks()
{
	ensure_data_block();
	ensure_sub_block();

	std::vector<pstsdk::block_id> blk_list(0);
	std::vector<pstsdk::bbt_update_action> bbt_updates;

	// data block operations
	build_data_block_list(m_original_data_id == m_pdata->get_id() ? m_pdata : m_db->read_data_block(m_original_data_id), blk_list);
	write_out_data_block(m_pdata, bbt_updates, blk_list);
	m_original_data_id = m_pdata->get_id();

	// subnode block operations
	build_subnode_block_list(m_original_sub_id == m_psub->get_id() ? m_psub : m_db->read_subnode_block(m_original_sub_id), blk_list);
	write_out_subnode_block(m_psub, bbt_updates, blk_list);
	m_original_sub_id = m_psub->get_id();

	// drop ref counts for just modified blocks
	drop_block_ref_count(blk_list);

	// update bbt with all the changes
	m_db->update_btree(bbt_updates);
}

inline void pstsdk::node_impl::write_out_data_block(std::tr1::shared_ptr<pstsdk::data_block>& block, std::vector<pstsdk::bbt_update_action>& bbt_updates, std::vector<pstsdk::block_id>& blk_list)
{
	block_id blk_id = block->get_id();

	// remove un-touched block id's from list of blocks in data block tree so that their ref counts are not affected
	std::vector<pstsdk::block_id>::iterator iter = std::find(blk_list.begin(), blk_list.end(), blk_id);
	if(blk_list.end() != iter) { blk_list.erase(iter); }

	if(block->get_address() == 0 && blk_id != 0)
	{
		if(!block->is_internal())
		{
			m_db->write_block(block);
		}
		else
		{
			std::tr1::shared_ptr<pstsdk::extended_block> int_block = std::tr1::static_pointer_cast<pstsdk::extended_block>(block);

			for(size_t ind = 0; ind < int_block->get_page_count(); ++ind)
			{
				std::tr1::shared_ptr<pstsdk::data_block> child_block = int_block->get_page(ind);
				int_block->set_block_info(ind, child_block->get_id());
				write_out_data_block(child_block, bbt_updates, blk_list);
			}

			m_db->write_block(block);
		}
	}

	// add entries of new blocks to BBT
	pstsdk::block_info new_blk_info = { blk_id, block->get_address(), block->get_disk_size(), 2 };
	bbt_updates.push_back(m_db->create_bbt_update_action(new_blk_info));
}

inline void pstsdk::node_impl::write_out_subnode_block(std::tr1::shared_ptr<pstsdk::subnode_block>& block, std::vector<pstsdk::bbt_update_action>& bbt_updates, std::vector<pstsdk::block_id>& blk_list)
{
	block_id blk_id = block->get_id();

	// remove un-touched block id's from list of blocks in subnode block tree so that their ref counts are not affected
	std::vector<pstsdk::block_id>::iterator iter = std::find(blk_list.begin(), blk_list.end(), blk_id);
	if(blk_list.end() != iter) { blk_list.erase(iter); }

	if(block->get_address() == 0 && blk_id != 0)
	{
		m_db->write_block(block);

		if(block->get_level() > 0)
		{
			std::tr1::shared_ptr<pstsdk::subnode_nonleaf_block> nonleaf_block = std::tr1::static_pointer_cast<pstsdk::subnode_nonleaf_block>(block);

			for(size_t pos = 0; pos < nonleaf_block->num_values(); ++pos)
			{
				write_out_subnode_block(nonleaf_block->get_child(pos, false), bbt_updates, blk_list);
			}
		}

		// add entries of new blocks to BBT
		pstsdk::block_info new_blk_info = { blk_id, block->get_address(), block->get_disk_size(), 2 };
		bbt_updates.push_back(m_db->create_bbt_update_action(new_blk_info));
	}
}

inline void pstsdk::node_impl::lock_node()
{
	pstsdk::thread_lock lock(&m_node_lock, false);
	lock.aquire_lock();
}

inline void pstsdk::node_impl::unlock_node()
{
	pstsdk::thread_lock lock(&m_node_lock, false);
	lock.release_lock();
}
//! \endcond

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
