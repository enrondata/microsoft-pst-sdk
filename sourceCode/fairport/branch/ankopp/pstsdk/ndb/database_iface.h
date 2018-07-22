//! \file
//! \brief Database interface
//! \author Terry Mahaffey
//!
//! Contains the db_context interface as well as some broadly used primitive 
//! in memory types and typedefs.
//! \ingroup ndb

#ifndef PSTSDK_NDB_DATABASE_IFACE_H
#define PSTSDK_NDB_DATABASE_IFACE_H

#include <memory>
#ifdef __GNUC__
#include <tr1/memory>
#endif

#include "pstsdk/util/util.h"
#include "pstsdk/util/primitives.h"

namespace pstsdk
{

	class node;

	//! \brief An in memory, database format agnostic version of \ref disk::bbt_leaf_entry.
	//!
	//! It doesn't contain a ref count because that value is of no interest to the
	//! in memory objects.
	//! \ingroup ndb
	struct block_info
	{
		block_id id;
		ulonglong address;
		ushort size;
		ushort ref_count;
	};

	//! \brief An in memory, database format agnostic version of \ref disk::block_reference
	//! used specifically for the \ref page class hierarchy
	//! \ingroup ndb
	struct page_info
	{
		page_id id;
		ulonglong address;
	};

	//! \brief An in memory, database format agnostic version of \ref disk::nbt_leaf_entry.
	//! \ingroup ndb
	struct node_info
	{
		node_id id;
		block_id data_bid;
		block_id sub_bid;
		node_id parent_id;
	};

	//! \brief An in memory, database format agnostic version of \ref disk::sub_leaf_entry.
	//! \ingroup ndb
	struct subnode_info
	{
		node_id id;
		block_id data_bid;
		block_id sub_bid;
	};

	template<typename K, typename V>
	class bt_page;
	typedef bt_page<node_id, node_info> nbt_page;
	typedef bt_page<block_id, block_info> bbt_page;

	template<typename K, typename V>
	class bt_nonleaf_page;
	typedef bt_nonleaf_page<node_id, node_info> nbt_nonleaf_page;
	typedef bt_nonleaf_page<block_id, block_info> bbt_nonleaf_page;

	template<typename K, typename V>
	class bt_leaf_page;
	typedef bt_leaf_page<node_id, node_info> nbt_leaf_page;
	typedef bt_leaf_page<block_id, block_info> bbt_leaf_page;

	template<typename K, typename V>
	class const_btree_node_iter;

	//! \brief set of amap specific header values
	struct header_values_amap
	{
		disk::amap_validity fAMapValid;
		ulonglong ibAMapLast;
		ulonglong ibFileEof;
		ulonglong cbAMapFree;
		ulonglong cbPMapFree;
	};

	class page;
	class amap_page;
	class allocation_map;
	class dlist_page;
	class pmap_page;
	class fmap_page;
	class fpmap_page;

	//! \brief set of possible btree operations
	enum bt_operation
	{
		bt_insert,
		bt_modify,
		bt_remove
	};

	//! \brief set of values for a nbt operation
	struct nbt_update_action
	{
		bt_operation action;
		node_id nd_id;
		node_info nd_inf;
	};

	//! \brief set of values for a bbt operation
	struct bbt_update_action
	{
		bt_operation action;
		block_id blk_id;
		block_info blk_inf;
	};
	//! \endcond


	//! \addtogroup ndb
	//@{
	typedef const_btree_node_iter<node_id, node_info> const_nodeinfo_iterator;
	typedef const_btree_node_iter<node_id, subnode_info> const_subnodeinfo_iterator;
	typedef const_btree_node_iter<block_id, block_info> const_blockinfo_iterator;
	//@}

	class block;
	class data_block;
	class extended_block;
	class external_block;
	class subnode_block;
	class subnode_leaf_block;
	class subnode_nonleaf_block;

	class db_context;

	//! \addtogroup ndb
	//@{
	typedef std::tr1::shared_ptr<db_context> shared_db_ptr;
	typedef std::tr1::weak_ptr<db_context> weak_db_ptr;
	//@}

	//! \defgroup ndb_databaserelated Database
	//! \ingroup ndb

	//! \brief Database external interface
	//!
	//! The db_context is the iterface which all components, \ref ndb and up,
	//! use to access the PST file. It is basically an object factory; given an
	//! address (or other relative context to help locate the physical piece of
	//! data) a db_context will produce an in memory version of that data
	//! structure, with all the Unicode vs. ANSI differences abstracted away.
	//! \ingroup ndb_databaserelated
	class db_context : public std::tr1::enable_shared_from_this<db_context>
	{
	public:
		virtual ~db_context() { }

		//! \name Lookup functions
		//@{
		//! \brief Open a node
		//! \throws key_not_found<node_id> if the specified node was not found
		//! \param[in] nid The id of the node to lookup
		//! \returns A node instance
		virtual node lookup_node(node_id nid) = 0;

		//! \brief Lookup information about a node
		//! \throws key_not_found<node_id> if the specified node was not found
		//! \param[in] nid The id of the node to lookup
		//! \returns Information about the specified node
		virtual node_info lookup_node_info(node_id nid) = 0;

		//! \brief Lookup information about a block
		//! \throws key_not_found<block_id> if the specified block was not found
		//! \param[in] bid The id of the block to lookup
		//! \returns Information about the specified block
		virtual block_info lookup_block_info(block_id bid) = 0;

		//! \brief Check whether given node exists
		//! \param[in] nid The id of the node to check
		//! \returns Whether the given node exists in the pst
		virtual bool node_exists(node_id nid) = 0;

		//! \brief Check whether given block exists
		//! \param[in] bid The id of the block to check
		//! \returns Whether the given block exists in the pst
		virtual bool block_exists(block_id bid) = 0;
		//@}

		//! \name Page factory functions
		//@{
		//! \brief Get the root of the BBT of this context
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the page appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's signature appears incorrect
		//! \throws database_corrupt (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's ptypeRepeat != ptype
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the page's CRC doesn't match the trailer
		//! \returns The requested page
		virtual std::tr1::shared_ptr<bbt_page> read_bbt_root() = 0;

		//! \brief Get the root of the NBT of this context
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the page appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's signature appears incorrect
		//! \throws database_corrupt (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's ptypeRepeat != ptype
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the page's CRC doesn't match the trailer
		//! \returns The requested page
		virtual std::tr1::shared_ptr<nbt_page> read_nbt_root() = 0;

		//! \brief Open a BBT page
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the page appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's signature appears incorrect
		//! \throws database_corrupt (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's ptypeRepeat != ptype
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the page's CRC doesn't match the trailer
		//! \returns The requested page
		virtual std::tr1::shared_ptr<bbt_page> read_bbt_page(const page_info& pi) = 0;

		//! \brief Open a NBT page
		//! \param[in] pi Information about the page to open
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the page appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's signature appears incorrect
		//! \throws database_corrupt (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's ptypeRepeat != ptype
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the page's CRC doesn't match the trailer
		//! \returns The requested page
		virtual std::tr1::shared_ptr<nbt_page> read_nbt_page(const page_info& pi) = 0;

		//! \brief Open a NBT leaf page
		//! \param[in] pi Information about the page to open
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the page appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's signature appears incorrect
		//! \throws database_corrupt (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's ptypeRepeat != ptype
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the page's CRC doesn't match the trailer
		//! \returns The requested page
		virtual std::tr1::shared_ptr<nbt_leaf_page> read_nbt_leaf_page(const page_info& pi) = 0;

		//! \brief Open a BBT leaf page
		//! \param[in] pi Information about the page to open
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the page appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's signature appears incorrect
		//! \throws database_corrupt (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's ptypeRepeat != ptype
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the page's CRC doesn't match the trailer
		//! \returns The requested page
		virtual std::tr1::shared_ptr<bbt_leaf_page> read_bbt_leaf_page(const page_info& pi) = 0;

		//! \brief Open a NBT nonleaf page
		//! \param[in] pi Information about the page to open
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the page appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's signature appears incorrect
		//! \throws database_corrupt (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's ptypeRepeat != ptype
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the page's CRC doesn't match the trailer
		//! \returns The requested page
		virtual std::tr1::shared_ptr<nbt_nonleaf_page> read_nbt_nonleaf_page(const page_info& pi) = 0;

		//! \brief Open a BBT nonleaf page
		//! \param[in] pi Information about the page to open
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the page appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's signature appears incorrect
		//! \throws database_corrupt (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's ptypeRepeat != ptype
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the page's CRC doesn't match the trailer
		//! \returns The requested page
		virtual std::tr1::shared_ptr<bbt_nonleaf_page> read_bbt_nonleaf_page(const page_info& pi) = 0;
		//@}

		//! \name Block factory functions
		//@{
		//! \brief Open a block in this context
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<block> read_block(block_id bid) { return read_block(shared_from_this(), bid); }

		//! \brief Open a data_block in this context
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<data_block> read_data_block(block_id bid) { return read_data_block(shared_from_this(), bid); }

		//! \brief Open a extended_block in this context
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<extended_block> read_extended_block(block_id bid) { return read_extended_block(shared_from_this(), bid); }

		//! \brief Open a external_block in this context
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<external_block> read_external_block(block_id bid) { return read_external_block(shared_from_this(), bid); }

		//! \brief Open a subnode_block in this context
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<subnode_block> read_subnode_block(block_id bid) { return read_subnode_block(shared_from_this(), bid); }

		//! \brief Open a subnode_leaf_block in this context
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<subnode_leaf_block> read_subnode_leaf_block(block_id bid) { return read_subnode_leaf_block(shared_from_this(), bid); }

		//! \brief Open a subnode_nonleaf_block in this context
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<subnode_nonleaf_block> read_subnode_nonleaf_block(block_id bid) { return read_subnode_nonleaf_block(shared_from_this(), bid); }

		//! \brief Open a block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<block> read_block(const shared_db_ptr& parent, block_id bid) = 0;

		//! \brief Open a data_block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<data_block> read_data_block(const shared_db_ptr& parent, block_id bid) = 0;

		//! \brief Open an extended_block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<extended_block> read_extended_block(const shared_db_ptr& parent, block_id bid) = 0;

		//! \brief Open a external_block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<external_block> read_external_block(const shared_db_ptr& parent, block_id bid) = 0;

		//! \brief Open a subnode_block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<subnode_block> read_subnode_block(const shared_db_ptr& parent, block_id bid) = 0;

		//! \brief Open a subnode_leaf_block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<subnode_leaf_block> read_subnode_leaf_block(const shared_db_ptr& parent, block_id bid) = 0;

		//! \brief Open a subnode_nonleaf_block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bid The id of the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<subnode_nonleaf_block> read_subnode_nonleaf_block(const shared_db_ptr& parent, block_id bid) = 0;

		//! \brief Open a block in this context
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<block> read_block(const block_info& bi) { return read_block(shared_from_this(), bi); }

		//! \brief Open a data_block in this context
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<data_block> read_data_block(const block_info& bi) { return read_data_block(shared_from_this(), bi); }

		//! \brief Open a extended_block in this context
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<extended_block> read_extended_block(const block_info& bi) { return read_extended_block(shared_from_this(), bi); }

		//! \brief Open a block in this context
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<external_block> read_external_block(const block_info& bi) { return read_external_block(shared_from_this(), bi); }

		//! \brief Open a subnode_block in this context
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<subnode_block> read_subnode_block(const block_info& bi) { return read_subnode_block(shared_from_this(), bi); }

		//! \brief Open a subnode_leaf_block in this context
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<subnode_leaf_block> read_subnode_leaf_block(const block_info& bi) { return read_subnode_leaf_block(shared_from_this(), bi); }

		//! \brief Open a subnode_nonleaf_block in this context
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		std::tr1::shared_ptr<subnode_nonleaf_block> read_subnode_nonleaf_block(const block_info& bi) { return read_subnode_nonleaf_block(shared_from_this(), bi); }

		//! \brief Open a block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<block> read_block(const shared_db_ptr& parent, const block_info& bi) = 0;

		//! \brief Open a data_block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<data_block> read_data_block(const shared_db_ptr& parent, const block_info& bi) = 0;

		//! \brief Open a extended_block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<extended_block> read_extended_block(const shared_db_ptr& parent, const block_info& bi) = 0;

		//! \brief Open a external_block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<external_block> read_external_block(const shared_db_ptr& parent, const block_info& bi) = 0;

		//! \brief Open a subnode_block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<subnode_block> read_subnode_block(const shared_db_ptr& parent, const block_info& bi) = 0;

		//! \brief Open a subnode_leaf_block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<subnode_leaf_block> read_subnode_leaf_block(const shared_db_ptr& parent, const block_info& bi) = 0;

		//! \brief Open a subnode_nonleaf_block in the specified context
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] bi Information about the block to open
		//! \throws unexpected_block (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the block appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the block trailer's signature appears incorrect
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the block's CRC doesn't match the trailer
		//! \returns The requested block
		virtual std::tr1::shared_ptr<subnode_nonleaf_block> read_subnode_nonleaf_block(const shared_db_ptr& parent, const block_info& bi) = 0;
		//@}

		//! \cond write_api
		//! \brief Create new external block
		//! \param[in] size Initial size of the block
		//! \returns A new external block
		std::tr1::shared_ptr<external_block> create_external_block(size_t size) { return create_external_block(shared_from_this(), size); }

		//! \brief Create new extended block
		//! \param[in] pblock Existing block to copy from
		//! \returns A new extended block
		std::tr1::shared_ptr<extended_block> create_extended_block(std::tr1::shared_ptr<external_block>& pblock) { return create_extended_block(shared_from_this(), pblock); }

		//! \brief Create new extended block
		//! \param[in] pblock Existing block to copy from
		//! \returns A new extended block
		std::tr1::shared_ptr<extended_block> create_extended_block(std::tr1::shared_ptr<extended_block>& pblock) { return create_extended_block(shared_from_this(), pblock); }

		//! \brief Create new extended block
		//! \param[in] size Initial size of the block
		//! \returns A new extended block
		std::tr1::shared_ptr<extended_block> create_extended_block(size_t size) { return create_extended_block(shared_from_this(), size); }

		//! \brief Create new external block
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] size Initial size of the block
		//! \returns A new external block
		virtual std::tr1::shared_ptr<external_block> create_external_block(const shared_db_ptr& parent, size_t size) = 0;

		//! \brief Create new extended block
		//! \param[in] initial size of the block
		//! \returns A new extended block
		virtual std::tr1::shared_ptr<extended_block> create_extended_block(const shared_db_ptr& parent, std::tr1::shared_ptr<external_block>& pblock) = 0;

		//! \brief Create new extended block
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] size Initial size of the block
		//! \returns A new extended block
		virtual std::tr1::shared_ptr<extended_block> create_extended_block(const shared_db_ptr& parent, std::tr1::shared_ptr<extended_block>& pblock) = 0;

		//! \brief Create new extended block
		//! \param[in] parent The context to open this block in. It must be either this context or a child context of this context.
		//! \param[in] size Initial size of the block
		//! \returns A new extended block
		virtual std::tr1::shared_ptr<extended_block> create_extended_block(const shared_db_ptr& parent, size_t size) = 0;

		//! \brief Create new subnode nonleaf block
		//! \param[in] pblock Existing block to copy from
		//! \returns A new subnode nonleaf block
		virtual std::tr1::shared_ptr<subnode_nonleaf_block> create_subnode_nonleaf_block(std::tr1::shared_ptr<pstsdk::subnode_block>& pblock) = 0;

		//! \brief Create new block id
		//! \param[in] is_internal Flag indicating if the block id for an 'internal' block
		//! \returns A new block id
		virtual block_id alloc_bid(bool is_internal) = 0;

		//! \brief Create new page id
		//! \returns A new page id
		virtual page_id alloc_pid() = 0;

		//! \brief Create new unique node id
		//! \param[in] type The type of node
		//! \returns A new unique node id
		virtual node_id alloc_nid(nid_type node_type) = 0;

		//! \brief Read amap page from disk
		//! \param[in] pi The page information to read from disk
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the page appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's signature appears incorrect
		//! \throws database_corrupt (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's ptypeRepeat != ptype
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the page's CRC doesn't match the trailer
		//! \returns The requested amap page
		virtual	std::tr1::shared_ptr<pstsdk::amap_page> read_amap_page(const page_info& pi) = 0;

		//! \brief Create new amap page
		//! \param[in] pi The page information to write to disk
		//! \returns A new amap page
		virtual	std::tr1::shared_ptr<pstsdk::amap_page> create_amap_page(const page_info& pi) = 0;

		//! \brief Read dilst page from disk
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the parameters of the page appear incorrect
		//! \throws sig_mismatch (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's signature appears incorrect
		//! \throws database_corrupt (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page trailer's ptypeRepeat != ptype
		//! \throws crc_fail (\ref PSTSDK_VALIDATION_LEVEL_WEAK "PSTSDK_VALIDATION_LEVEL_FULL") If the page's CRC doesn't match the trailer
		//! \returns The dilst page
		virtual std::tr1::shared_ptr<pstsdk::dlist_page> read_dlist_page() = 0;

		//! \brief Create new dilst page
		//! \returns A new dilst page
		virtual std::tr1::shared_ptr<pstsdk::dlist_page> create_dlist_page() = 0;

		//! \brief Write a dlist page to disk
		//! \param[in] the_page The dlist page to write to disk
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the offset + data size if past eof
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page is not sector aligned
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page size is not \ref page_size (512 bytes)
		//! \returns Number of bytes written
		virtual	size_t write_page(std::tr1::shared_ptr<pstsdk::dlist_page>& the_page) = 0;

		//! \brief Write an amap page to disk
		//! \param[in] the_page The amap page to write to disk
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the offset + data size if past eof
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page is not sector aligned
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page size is not \ref page_size (512 bytes)
		//! \returns Number of bytes written
		virtual	size_t write_page(std::tr1::shared_ptr<pstsdk::amap_page>& the_page) = 0;

		//! \brief Write a pmap page to disk
		//! \param[in] the_page The pmap page to write to disk
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the offset + data size if past eof
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page is not sector aligned
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page size is not \ref page_size (512 bytes)
		//! \returns Number of bytes written
		virtual	size_t write_page(std::tr1::shared_ptr<pstsdk::pmap_page>& the_page) = 0;

		//! \brief Write a fmap page to disk
		//! \param[in] the_page The fmap page to write to disk
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the offset + data size if past eof
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page is not sector aligned
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page size is not \ref page_size (512 bytes)
		//! \returns Number of bytes written
		virtual	size_t write_page(std::tr1::shared_ptr<pstsdk::fmap_page>& the_page) = 0;

		//! \brief Write a fpmap page to disk
		//! \param[in] the_page The fpmap page to write to disk
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the offset + data size if past eof
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page is not sector aligned
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page size is not \ref page_size (512 bytes)
		//! \returns Number of bytes written
		virtual	size_t write_page(std::tr1::shared_ptr<pstsdk::fpmap_page>& the_page) = 0;

		//! \brief Write a nbt leaf page to disk
		//! \param[in] the_page The nbt leaf page to write to disk
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the offset + data size if past eof
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page is not sector aligned
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page size is not \ref page_size (512 bytes)
		//! \returns Number of bytes written
		virtual	size_t write_page(std::tr1::shared_ptr<pstsdk::nbt_leaf_page>& the_page) = 0;

		//! \brief Write a nbt nonleaf page to disk
		//! \param[in] the_page The nbt nonleaf page to write to disk
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the offset + data size if past eof
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page is not sector aligned
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page size is not \ref page_size (512 bytes)
		//! \returns Number of bytes written
		virtual	size_t write_page(std::tr1::shared_ptr<pstsdk::nbt_nonleaf_page>& the_page) = 0;

		//! \brief Write a bbt leaf page to disk
		//! \param[in] the_page The bbt leaf page to write to disk
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the offset + data size if past eof
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page is not sector aligned
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page size is not \ref page_size (512 bytes)
		//! \returns Number of bytes written
		virtual	size_t write_page(std::tr1::shared_ptr<pstsdk::bbt_leaf_page>& the_page) = 0;

		//! \brief Write a bbt nonleaf page to disk
		//! \param[in] the_page The bbt nonleaf page to write to disk
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the offset + data size if past eof
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page is not sector aligned
		//! \throws unexpected_page (\ref PSTSDK_VALIDATION_LEVEL_WEAK) If the page size is not \ref page_size (512 bytes)
		//! \returns Number of bytes written
		virtual	size_t write_page(std::tr1::shared_ptr<pstsdk::bbt_nonleaf_page>& the_page) = 0;

		//! \brief Create new nbt nonleaf page
		//! \param[in] page Child page whose info is to be used for initialization of new page
		//! \returns New nbt nonleaf page
		virtual std::tr1::shared_ptr<nbt_nonleaf_page> create_nbt_nonleaf_page(std::tr1::shared_ptr<pstsdk::nbt_page>& page) = 0;

		//! \brief Create new bbt nonleaf page
		//! \param[in] page Child page whose info is to be used for initialization of new page
		//! \returns New bbt nonleaf page
		virtual std::tr1::shared_ptr<bbt_nonleaf_page> create_bbt_nonleaf_page(std::tr1::shared_ptr<pstsdk::bbt_page>& page) = 0;

		//! \brief Read raw bytes from disk
		//! \param[out] buffer Buffer to be filled with read data
		//! \param[in] offset The offset to start reading from disk
		//! \throws out_of_range If file read fails
		//! \returns Number of bytes read
		virtual	size_t read_raw_bytes(std::vector<byte>& buffer, ulonglong offset) = 0;

		//! \brief Write raw bytes to disk
		//! \param[in] buffer Buffer whose contents are to be written to disk
		//! \param[in] offset The offset to start writing to disk
		//! \throws out_of_range If file write fails
		//! \returns Number of bytes written
		virtual	size_t write_raw_bytes(std::vector<byte>& buffer, ulonglong offset) = 0;

		//! \brief Accessor to amap object
		//! \returns Return allocation_map object
		virtual std::tr1::shared_ptr<pstsdk::allocation_map> get_allocation_map() = 0;

		//! \brief Get amap specific header values from disk
		//! \param[out] values Structure that gets filled up with amap specific header values
		virtual void read_header_values_amap(header_values_amap& values) = 0;

		//! \brief Set amap specific header values from/to disk
		//! \param[in] values Structure with new values to be set in header
		virtual void write_header_values_amap(header_values_amap& values) = 0;

		//! \brief Update nbt with a set of operations (insert/modify/remove)
		//! \param[in] nbt_updates Structure denoting details of operations to be performed on nbt
		virtual void update_btree(std::vector<pstsdk::nbt_update_action>& nbt_updates) = 0;

		//! \brief Update bbt with a set of operations (insert/modify/remove)
		//! \param[in] bbt_updates Structure denoting details of operations to be performed on bbt
		virtual void update_btree(std::vector<pstsdk::bbt_update_action>& bbt_updates) = 0;

		//! \brief Add new data blocks to database write queue which are written only upon call to commit db
		//! \param[in] data_block_queue The data blocks to be written to disk
		virtual void add_to_block_write_queue(std::map<block_id, std::tr1::shared_ptr<data_block>>& data_block_queue) = 0;

		//! \brief Add new subnode blocks to database write queue which are written only upon call to commit db
		//! \param[in] data_block_queue The subnode blocks to be written to disk
		virtual void add_to_block_write_queue(std::map<block_id, std::tr1::shared_ptr<subnode_block>>& subnode_block_queue) = 0;

		//! \brief Create insert/modify/remove action item for NBT updates
		//! \param[in] nd_inf Node information of node involved in the operation
		//! \param[in] del Flag indicating Delete/Update action
		//! \returns Details of NBT operation to perform
		virtual nbt_update_action create_nbt_update_action(node_info& nd_inf, bool del = false) = 0;

		//! \brief Create insert/modify/remove action item for BBT updates
		//! \param[in] blk_inf Block information of block involved in the operation
		//! \param[in] del Flag indicating Delete/Update action
		//! \returns Details of BBT operation to perform
		virtual bbt_update_action create_bbt_update_action(block_info& blk_inf, bool del = false) = 0;

		//! \brief Commit current state of db to disk
		//! \throws node_save_error If nodes have already been modified by some other db context
		virtual void commit_db() = 0;

		//! \brief Discard all dirty nodes and revert back to last successful commited state
		virtual void discard_changes() = 0;

		//! \brief Write a data block to disk
		//! \param[in] the_block The data block to write
		//! \returns The number of bytes written
		virtual size_t write_block(std::tr1::shared_ptr<data_block>& the_block) = 0;

		//! \brief Write a subnode block to disk
		//! \param[in] the_block The subnode block to write
		//! \returns The number of bytes written
		virtual	size_t write_block(std::tr1::shared_ptr<pstsdk::subnode_block>& the_block) = 0;

		//! \brief Get disk aligned size for given logical size for a block
		//! \param[in] logical_size Logical size of the block
		//! \returns Returns disk aligned size for given logical size for a block
		virtual	size_t get_block_disk_size(size_t logical_size) = 0;

		//! \brief Create a new node
		//! \param[in] id Node id to be assigned to new node
		//! \throws duplicate_key If a node already exisits with same id
		//! \returns A new node with empty data block
		virtual pstsdk::node create_node(pstsdk::node_id id) = 0;

		//! \brief Delete a node
		//! \param[in] id Id of node to be deleted
		//! \throws key_not_found If the node does not exisit
		virtual void delete_node(pstsdk::node_id id) = 0;

		//! \brief Create child db context
		//! \returns Snapshot of current state of Db context 
		virtual shared_db_ptr create_context() = 0;

		//! \brief Commit changes to parent db context 
		//! \param[in] ctx Db context to commit
		//! \throws node_save_error If nodes have already been modified by some other db context
		virtual void commit_db(const shared_db_ptr& ctx) = 0;

		//! \brief Add to virtual reference count of blocks in use when child contexts are created
		virtual void add_ref_context() = 0;

		//! \brief Drop virtual reference count of blocks in use when child contexts are committed
		virtual void release_context() = 0;

		//! \brief Lock db context for exclusive access
		virtual void lock_db() = 0;

		//! \brief Release exlusive lock aquired on db context
		virtual void unlock_db() = 0;
		//! \endcond
	};

}

#endif