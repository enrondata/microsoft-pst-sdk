//! \file
//! \brief Folder objects
//!
//! Defines the folder and search folder abstractions, as well as the
//! transformations used by the boost iterator library to create the folder
//! iterators. Also defines a generic filter used to filter by node type,
//! used by both the folder object and pst object to filter through to the
//! specific nodes of interest when creating iterators.
//! \author Terry Mahaffey
//! \ingroup pst

#ifndef PSTSDK_PST_FOLDER_H
#define PSTSDK_PST_FOLDER_H

#include <algorithm>
#include <boost/iterator/filter_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include "pstsdk/util/primitives.h"
#include "pstsdk/util/errors.h"

#include "pstsdk/ndb/database_iface.h"

#include "pstsdk/ltp/propbag.h"
#include "pstsdk/ltp/table.h"

#include "pstsdk/pst/message.h"

namespace pstsdk
{

	//! \defgroup pst_folderrelated Folder Objects
	//! \ingroup pst

	//! \brief Functor to determine if an object is of the specified node type
	//!
	//! This functor is used to build filters for use by the boost iterator
	//! library. It has overloads for both node_info objects (when filtering
	//! over the NBT) and for const_table_rows (when filtering over a table).
	//! It is used by both the \ref pst object and the \ref folder object.
	//! \tparam Type The node type to identify
	//! \ingroup pst
	template<node_id Type>
	struct is_nid_type
	{
		bool operator()(const node_info& info)
		{ return get_nid_type(info.id) == Type; }
		bool operator()(const const_table_row& row)
		{ return get_nid_type(row.get_row_id()) == Type; }
	};

	//! \brief Search Folder object
	//!
	//! Search folders are different from regular folders mainly in that they
	//! do not have a hierarchy table (and thus no subfolders). The messages
	//! they "contain" are actually in other folders.
	//!
	//! This object exists to reflect that limited interface. Eventually this
	//! object may support querying the criteria used to create the search folder.
	//! \ingroup pst_folderrelated
	class search_folder
	{
	public:
		//! \brief Message iterator type; a transform iterator over a table row iterator
		typedef boost::transform_iterator<message_transform_row, const_table_row_iter> message_iterator;

		//! \brief Construct a search folder object
		//! \param[in] db The database pointer
		//! \param[in] n A search folder node
		search_folder(const shared_db_ptr& db, const node& n)
			: m_db(db), m_bag(n) { }
		//! \brief Copy construct a search folder object
		search_folder(const search_folder& other);

#ifndef BOOST_NO_RVALUE_REFERENCES
		//! \brief Move construct a search folder
		search_folder(search_folder&& other)
			: m_db(std::move(other.m_db)), m_bag(std::move(other.m_bag)), m_contents_table(std::move(other.m_contents_table)) { }
#endif

		// subobject discovery/enumeration
		//! \brief Get an iterator to the first message in this folder
		//! \returns an iterator positioned on the first message in this folder
		message_iterator message_begin() const
		{ return boost::make_transform_iterator(get_contents_table().begin(), message_transform_row(m_db)); }
		//! \brief Get the end message iterator
		//! \returns an iterator at the end position
		message_iterator message_end() const
		{ return boost::make_transform_iterator(get_contents_table().end(), message_transform_row(m_db)); }

		// property access
		//! \brief Get the display name of this folder
		//! \returns The name of this folder
		std::wstring get_name() const
		{ return m_bag.read_prop<std::wstring>(0x3001); }
		//! \brief Get the number of unread messages in this folder
		//! \returns The number of unread messages
		size_t get_unread_message_count() const
		{ return m_bag.read_prop<slong>(0x3603); }
		//! \brief Get the number of messages in this folder
		//! \returns The number of messages
		size_t get_message_count() const
		{ return m_bag.read_prop<slong>(0x3602); }

		// lower layer access
		//! \brief Get the property bag backing this folder
		//! \returns The property bag
		property_bag& get_property_bag()
		{ return m_bag; }
		//! \copydoc search_folder::get_property_bag()
		const property_bag& get_property_bag() const
		{ return m_bag; }
		//! \brief Get the database pointer used by this folder
		//! \returns The database pointer
		shared_db_ptr get_db() const 
		{ return m_db; }
		//! \brief Get the contents table of this folder
		//! \returns The contents table
		table& get_contents_table();
		//! \copydoc search_folder::get_contents_table()
		const table& get_contents_table() const;

		//! \brief Get the node_id of this search folder
		//! \returns The node_id of the search folder
		node_id get_id() const
		{ return m_bag.get_node().get_id(); }

		//! \cond write_api
		//! \brief Set the count of messages under this folder
		//! \param[in] cnt The message count value
		void set_message_count(size_t cnt);
		
		//! \brief Set count of unread messages under this folder
		//! \param[in] cnt The unread message count value
		void set_unread_message_count(size_t cnt);
		
		//! \brief Set the name of this folder
		//! \param[in] name The folder name
		void set_name(std::wstring name);
		//! \endcond

	private:
		shared_db_ptr m_db;
		property_bag m_bag;
		mutable std::tr1::shared_ptr<table> m_contents_table;
	};

	//! \brief Defines a transform from a row of a hierarchy table to a search_folder
	//!
	//! Used by the boost iterator library to provide friendly iterators over
	//! search_folders
	//! \ingroup pst_folderrelated
	class search_folder_transform_row : public std::unary_function<const_table_row, search_folder>
	{
	public:
		//! \brief Construct a search_folder_transform object
		//! \param[in] db The database pointer
		search_folder_transform_row(const shared_db_ptr& db) 
			: m_db(db) { }
		//! \brief Perform the transform
		//! \param[in] row A row from a hierarchy table refering to a search folder
		//! \returns a search folder object.
		search_folder operator()(const const_table_row& row) const
		{ return search_folder(m_db, m_db->lookup_node(row.get_row_id())); }

	private:
		shared_db_ptr m_db;
	};

	class folder;
	//! \brief Defines a transform from a row of a hierarchy table to a folder
	//!
	//! Used by the boost iterator library to provide iterators over folder objects
	//! \ingroup pst_folderrelated
	class folder_transform_row : public std::unary_function<const_table_row, folder>
	{
	public:
		//! \brief Construct a folder_transform_row object
		//! \param[in] db The database pointer
		folder_transform_row(const shared_db_ptr& db) 
			: m_db(db) { }
		//! \brief Perform the transform
		//! \param[in] row A row from a hierarchy table refering to a folder
		//! \returns a folder object.
		folder operator()(const const_table_row& row) const;

	private:
		shared_db_ptr m_db;
	};

	//! \brief A folder in a PST file
	//!
	//! The folder object allows access to subfolders, messages, and associated 
	//! messagse which are contained in the folder. Similar to the \ref pst object,
	//! the folder also offers a way to lookup subfolders by name.
	//!
	//! A folder currently doesn't have a concept of sorting. This was deemed
	//! unnecessary because of the iterator based approach used for exposing
	//! sub messages and folders - one can use these iterators to build up a 
	//! container of messages or folders to be sorted, and calling std::sort
	//! directly with an arbitrary sorting functor.
	//! \ingroup pst_folderrelated
	class folder
	{
		typedef boost::filter_iterator<is_nid_type<nid_type_search_folder>, const_table_row_iter> search_folder_filter_iterator;
		typedef boost::filter_iterator<is_nid_type<nid_type_folder>, const_table_row_iter> folder_filter_iterator;

	public:
		//! \brief Message iterator type; a transform iterator over a table row iterator
		typedef boost::transform_iterator<message_transform_row, const_table_row_iter> message_iterator;
		//! \brief Folder iterator type; a transform iterator over a filter iterator over table row iterator
		typedef boost::transform_iterator<folder_transform_row, folder_filter_iterator> folder_iterator;
		//! \brief Search folder iterator type; a transform iterator over a filter iterator over table row iterator
		typedef boost::transform_iterator<search_folder_transform_row, search_folder_filter_iterator> search_folder_iterator;

		//! \brief Construct a folder object
		//! \param[in] db The database pointer
		//! \param[in] n A folder node
		folder(const shared_db_ptr& db, const node& n)
			: m_db(db), m_bag(n) { }
		//! \brief Copy construct a folder object
		//! \param[in] other folder to copy
		folder(const folder& other);

#ifndef BOOST_NO_RVALUE_REFERENCES
		//! \brief Move construct a folder object
		//! \param[in] other folder to move from
		folder(folder&& other)
			: m_db(std::move(other.m_db)), m_bag(std::move(other.m_bag)), m_contents_table(std::move(other.m_contents_table)), m_associated_contents_table(std::move(other.m_associated_contents_table)), m_hierarchy_table(std::move(other.m_hierarchy_table)) { }
#endif

		// subobject discovery/enumeration
		//! \brief Get an iterator to the first folder in this folder
		//! \returns an iterator positioned on the first folder in this folder
		folder_iterator sub_folder_begin() const
		{ return boost::make_transform_iterator(boost::make_filter_iterator<is_nid_type<nid_type_folder> >(get_hierarchy_table().begin(), get_hierarchy_table().end()), folder_transform_row(m_db)); }
		//! \brief Get the end folder iterator
		//! \returns an iterator at the end position
		folder_iterator sub_folder_end() const
		{ return boost::make_transform_iterator(boost::make_filter_iterator<is_nid_type<nid_type_folder> >(get_hierarchy_table().end(), get_hierarchy_table().end()), folder_transform_row(m_db)); }

		//! \brief Get an iterator to the first search folder in this folder
		//! \returns an iterator positioned on the first search folder in this folder
		search_folder_iterator sub_search_folder_begin() const
		{ return boost::make_transform_iterator(boost::make_filter_iterator<is_nid_type<nid_type_search_folder> >(get_hierarchy_table().begin(), get_hierarchy_table().end()), search_folder_transform_row(m_db)); }
		//! \brief Get the end search folder iterator
		//! \returns an iterator at the end position
		search_folder_iterator sub_search_folder_end() const
		{ return boost::make_transform_iterator(boost::make_filter_iterator<is_nid_type<nid_type_search_folder> >(get_hierarchy_table().begin(), get_hierarchy_table().end()), search_folder_transform_row(m_db)); }

		//! \brief Open a specific subfolder in this folder, not recursive
		//! \param[in] name The name of the folder to open
		//! \throws key_not_found<std::wstring> If a folder of the specified name was not found in this folder
		//! \returns The first folder by that name found in this folder
		folder open_sub_folder(const std::wstring& name);

		//! \copydoc search_folder::message_begin()
		message_iterator message_begin() const
		{ return boost::make_transform_iterator(get_contents_table().begin(), message_transform_row(m_db)); }
		//! \copydoc search_folder::message_end()
		message_iterator message_end() const
		{ return boost::make_transform_iterator(get_contents_table().end(), message_transform_row(m_db)); }

		//! \brief Get an iterator to the first associated message in this folder
		//! \returns an iterator positioned on the first associated message in this folder
		message_iterator associated_message_begin() const
		{ return boost::make_transform_iterator(get_associated_contents_table().begin(), message_transform_row(m_db)); }
		//! \brief Get the end associated message iterator
		//! \returns an iterator at the end position
		message_iterator associated_message_end() const
		{ return boost::make_transform_iterator(get_associated_contents_table().end(), message_transform_row(m_db)); }

		// property access
		//! \copydoc search_folder::get_name()
		std::wstring get_name() const
		{ return m_bag.read_prop<std::wstring>(0x3001); }
		//! \brief Get the number of sub folders in this folder
		//! \returns The number of subfolders
		size_t get_subfolder_count() const
		{ return get_hierarchy_table().size(); } 
		//! \copydoc search_folder::get_unread_message_count()
		size_t get_unread_message_count() const
		{ return m_bag.read_prop<slong>(0x3603); }
		//! \copydoc search_folder::get_message_count()
		size_t get_message_count() const
		{ return m_bag.read_prop<slong>(0x3602); }
		//! \brief Get the number of associated messages in this folder
		//! \returns The number of associated messages
		size_t get_associated_message_count() const
		{ return m_bag.read_prop<slong>(0x3617); }

		// lower layer access
		//! \copydoc search_folder::get_property_bag()
		property_bag& get_property_bag()
		{ return m_bag; }
		//! \copydoc search_folder::get_property_bag()
		const property_bag& get_property_bag() const
		{ return m_bag; }
		//! \copydoc search_folder::get_db()
		shared_db_ptr get_db() const 
		{ return m_db; }
		//! \brief Get the hierarchy table of this folder
		//! \returns The hierarchy table
		table& get_hierarchy_table();
		//! \copydoc search_folder::get_contents_table()
		table& get_contents_table();
		//! \brief Get the associated contents table of this folder
		//! \returns The associated contents table
		table& get_associated_contents_table();
		//! \copydoc folder::get_hierarchy_table()
		const table& get_hierarchy_table() const;
		//! \copydoc search_folder::get_contents_table()
		const table& get_contents_table() const;
		//! \copydoc folder::get_associated_contents_table()
		const table& get_associated_contents_table() const;

		//! \brief Get the node_id of this folder
		//! \returns The node_id of the folder
		node_id get_id() const
		{ return m_bag.get_node().get_id(); }

		//! \cond write_api
		//! \brief Set the count of messages under this folder
		//! \param[in] cnt The message count value
		void set_message_count(size_t cnt);
		
		//! \brief Set count of unread messages under this folder
		//! \param[in] cnt The unread message count value
		void set_unread_message_count(size_t cnt);
		
		//! \brief Set the name of this folder
		//! \param[in] name The folder name
		void set_name(std::wstring name);
		
		//! \brief Set whether this folder has any subfolders
		//! \param[in] val Flag indicating if this folder has subfolders
		void set_has_subfolders(bool val);
		
		//! \brief Save this folder
		void save_folder();
		
		//! \brief Create a new sub folder under this folder
		//! \param[in] name The sub folder name
		folder create_subfolder(std::wstring name);
		
		//! \brief Create a new message under this folder
		//! \param[in] name The class name of the message
		message create_message(std::wstring class_name);
		
		//! \brief Delete a message under this folder
		//! \param[in] msg The message to be deleted
		void delete_message(message& msg);
		
		//! \brief Delete a message under this folder
		//! \param[in] nid_msg The node_id of the message to be deleted
		void delete_message(node_id nid_msg);
		
		//! \brief Delete a sub folder under this folder
		//! \param[in] fldr The sub folder to be deleted
		void delete_subfolder(folder& fldr);
		
		//! \brief Delete a sub folder under this folder
		//! \param[in] nid_fldr The noide_id of the sub folder to deleted
		void delete_subfolder(node_id nid_fldr);
		//! \endcond

	private:
		shared_db_ptr m_db;
		property_bag m_bag;
		mutable std::tr1::shared_ptr<table> m_contents_table;
		mutable std::tr1::shared_ptr<table> m_associated_contents_table;
		mutable std::tr1::shared_ptr<table> m_hierarchy_table;
	};

	//! \brief Defines a transform from a node_info to a folder
	//!
	//! Used by the boost iterator library to provide iterators over folder objects
	//! \ingroup pst_folderrelated
	class folder_transform_info : public std::unary_function<node_info, folder>
	{
	public:
		//! \brief Construct a folder_transform_info object
		//! \param[in] db The database pointer
		folder_transform_info(const shared_db_ptr& db) 
			: m_db(db) { }
		//! \brief Perform the transform
		//! \param[in] info info about a folder node
		//! \returns a folder object
		folder operator()(const node_info& info) const
		{ return folder(m_db, node(m_db, info)); }

	private:
		shared_db_ptr m_db;
	};

} // end namespace pstsdk

inline pstsdk::search_folder::search_folder(const pstsdk::search_folder& other)
	: m_db(other.m_db), m_bag(other.m_bag) 
{ 
	if(other.m_contents_table)
		m_contents_table.reset(new table(*other.m_contents_table));
}

inline pstsdk::folder::folder(const pstsdk::folder& other)
	: m_db(other.m_db), m_bag(other.m_bag) 
{ 
	if(other.m_contents_table)
		m_contents_table.reset(new table(*other.m_contents_table));
	if(other.m_associated_contents_table)
		m_contents_table.reset(new table(*other.m_associated_contents_table));
	if(other.m_hierarchy_table)
		m_contents_table.reset(new table(*other.m_hierarchy_table));
}

inline pstsdk::folder pstsdk::folder_transform_row::operator()(const pstsdk::const_table_row& row) const
{ 
	return folder(m_db, m_db->lookup_node(row.get_row_id()));
}

inline const pstsdk::table& pstsdk::search_folder::get_contents_table() const
{
	if(!m_contents_table)
		m_contents_table.reset(new table(m_db->lookup_node(make_nid(nid_type_search_contents_table, get_nid_index(get_id())))));

	return *m_contents_table;
}

inline pstsdk::table& pstsdk::search_folder::get_contents_table()
{
	return const_cast<table&>(const_cast<const search_folder*>(this)->get_contents_table());
}


namespace compiler_workarounds
{

	struct folder_name_equal : public std::unary_function<bool, const pstsdk::folder&>
	{
		folder_name_equal(const std::wstring& name) : m_name(name) { }
		bool operator()(const pstsdk::folder& f) const { return f.get_name() == m_name; }
		std::wstring m_name;
	};

} // end namespace compiler_workarounds


inline pstsdk::folder pstsdk::folder::open_sub_folder(const std::wstring& name)
{
	folder_iterator iter = std::find_if(sub_folder_begin(), sub_folder_end(), compiler_workarounds::folder_name_equal(name));

	if(iter != sub_folder_end())
		return *iter;

	throw key_not_found<std::wstring>(name);
}

inline const pstsdk::table& pstsdk::folder::get_contents_table() const
{
	if(!m_contents_table)
		m_contents_table.reset(new table(m_db->lookup_node(make_nid(nid_type_contents_table, get_nid_index(get_id())))));

	return *m_contents_table;
}

inline pstsdk::table& pstsdk::folder::get_contents_table()
{
	return const_cast<table&>(const_cast<const folder*>(this)->get_contents_table());
}

inline const pstsdk::table& pstsdk::folder::get_hierarchy_table() const
{
	if(!m_hierarchy_table)
		m_hierarchy_table.reset(new table(m_db->lookup_node(make_nid(nid_type_hierarchy_table, get_nid_index(get_id())))));

	return *m_hierarchy_table;
}

inline pstsdk::table& pstsdk::folder::get_hierarchy_table()
{
	return const_cast<table&>(const_cast<const folder*>(this)->get_hierarchy_table());
}

inline const pstsdk::table& pstsdk::folder::get_associated_contents_table() const
{
	if(!m_associated_contents_table)
		m_associated_contents_table.reset(new table(m_db->lookup_node(make_nid(nid_type_associated_contents_table, get_nid_index(get_id())))));

	return *m_associated_contents_table;
}

inline pstsdk::table& pstsdk::folder::get_associated_contents_table()
{
	return const_cast<table&>(const_cast<const folder*>(this)->get_associated_contents_table());
}

//! \cond write_api
inline void pstsdk::folder::save_folder()
{
	m_bag.save_property_bag();

	if(m_associated_contents_table)
		m_associated_contents_table->save_table();

	if(m_contents_table)
		m_contents_table->save_table();

	if(m_hierarchy_table)
		m_hierarchy_table->save_table();
}

inline pstsdk::folder pstsdk::folder::create_subfolder(std::wstring name)
{
	node_id nid = m_db->alloc_nid(nid_type_folder);

	node fd_nd = m_db->create_node(nid);
	fd_nd.set_parent_id(get_id());

	folder fld(m_db, fd_nd);

	// create folder PC & update parent folder properties
	ulong row = get_hierarchy_table().add_row(nid);
	set_has_subfolders(true);

	fld.m_bag.write_prop<std::wstring>(0x3001, name);
	get_hierarchy_table().write_cell(row, 0x3001, wstring_to_bytes(name));

	fld.m_bag.write_prop<slong>(0x3602, 0);
	m_hierarchy_table->set_cell_value(row, 0x3602, 0);

	fld.m_bag.write_prop<slong>(0x3603, 0);
	m_hierarchy_table->set_cell_value(row, 0x3603, 0);

	fld.m_bag.write_prop<bool>(0x360A, false);
	m_hierarchy_table->set_cell_value(row, 0x360A, false);

	// create heirarchy table
	node ht_nd = m_db->create_node(make_nid(nid_type_hierarchy_table, get_nid_index(nid)));
	ht_nd = m_db->lookup_node(nid_hierarchy_table_template);
	fld.m_hierarchy_table.reset(new table(ht_nd));

	// create contents table
	node ct_nd = m_db->create_node(make_nid(nid_type_contents_table, get_nid_index(nid)));
	ct_nd = m_db->lookup_node(nid_contents_table_template); 
	fld.m_contents_table.reset(new table(ct_nd));

	// create associated contents table
	node act_nd = m_db->create_node(make_nid(nid_type_associated_contents_table, get_nid_index(nid)));
	act_nd = m_db->lookup_node(nid_associated_contents_table_template);
	fld.m_associated_contents_table.reset(new table(act_nd));

	return fld; 
}

inline pstsdk::message pstsdk::folder::create_message(std::wstring class_name)
{
	node_id nid = m_db->alloc_nid(nid_type_message);

	node msg_nd = m_db->create_node(nid);
	msg_nd.set_parent_id(get_id());

	property_bag bag(msg_nd);
	bag.write_prop<std::wstring>(0x001A, class_name);
	bag.write_prop<slong>(0x0E07, 0);
	bag.write_prop<slong>(0x0E08, 0);
	bag.write_prop<slong>(0x0E17, 0);
	bag.write_prop<std::time_t>(0x3007, time_t(0));
	bag.write_prop<std::time_t>(0x3008, time_t(0));
	bag.write_prop<std::vector<pstsdk::byte>>(0x300B, wstring_to_bytes(class_name));

	ulong row = get_contents_table().add_row(nid);
	m_bag.write_prop<slong>(0x3602, get_message_count() + 1);

	m_contents_table->write_cell(row, 0x001A, wstring_to_bytes(class_name));
	m_contents_table->set_cell_value(row, 0x0E07, 0);
	m_contents_table->set_cell_value(row, 0x0E08, 0);
	m_contents_table->set_cell_value(row, 0x0E17, 0);
	m_contents_table->set_cell_value(row, 0x3008, time_t(0));

	node rcpt_nd = msg_nd.create_subnode(nid_recipient_table);
	rcpt_nd = m_db->lookup_node(nid_recipient_table);

	msg_nd.save_subnode(rcpt_nd);

	return message(msg_nd);
}

inline void pstsdk::search_folder::set_message_count(size_t size)
{
	m_bag.write_prop<slong>(0x3602, size);

	node_id parent_id = m_bag.get_node().get_parent_id();

	if(parent_id == get_id())
		return;

	folder parent(m_db, m_db->lookup_node(parent_id));
	table ht = parent.get_hierarchy_table();
	ht.set_cell_value(ht.lookup_row(get_id()), 0x3602, size);
	ht.save_table();
}

inline void pstsdk::search_folder::set_unread_message_count(size_t size)
{
	m_bag.write_prop<slong>(0x3603, size);

	node_id parent_id = m_bag.get_node().get_parent_id();

	if(parent_id == get_id())
		return;

	folder parent(m_db, m_db->lookup_node(parent_id));
	table ht = parent.get_hierarchy_table();
	ht.set_cell_value(ht.lookup_row(get_id()), 0x3603, size);
	ht.save_table();
}

inline void pstsdk::search_folder::set_name(std::wstring name)
{
	m_bag.write_prop<std::wstring>(0x3001, name);

	node_id parent_id = m_bag.get_node().get_parent_id();

	if(parent_id == get_id())
		return;

	folder parent(m_db, m_db->lookup_node(parent_id));
	table ht = parent.get_hierarchy_table();
	ht.write_cell(ht.lookup_row(get_id()), 0x3001, wstring_to_bytes(name));
	ht.save_table();
}

inline void pstsdk::folder::set_message_count(size_t size)
{
	m_bag.write_prop<slong>(0x3602, size);

	node_id parent_id = m_bag.get_node().get_parent_id();

	if(parent_id == get_id())
		return;

	folder parent(m_db, m_db->lookup_node(parent_id));
	table ht = parent.get_hierarchy_table();
	ht.set_cell_value(ht.lookup_row(get_id()), 0x3602, size);
	ht.save_table();
}

inline void pstsdk::folder::set_unread_message_count(size_t size)
{
	m_bag.write_prop<slong>(0x3603, size);

	node_id parent_id = m_bag.get_node().get_parent_id();

	if(parent_id == get_id())
		return;

	folder parent(m_db, m_db->lookup_node(parent_id));
	table ht = parent.get_hierarchy_table();
	ht.set_cell_value(ht.lookup_row(get_id()), 0x3603, size);
	ht.save_table();
}

inline void pstsdk::folder::set_name(std::wstring name)
{
	m_bag.write_prop<std::wstring>(0x3001, name);

	node_id parent_id = m_bag.get_node().get_parent_id();

	if(parent_id == get_id())
		return;

	folder parent(m_db, m_db->lookup_node(parent_id));
	table ht = parent.get_hierarchy_table();
	ht.write_cell(ht.lookup_row(get_id()), 0x3001, wstring_to_bytes(name));
	ht.save_table();
}

inline void pstsdk::folder::set_has_subfolders(bool val)
{
	m_bag.write_prop<bool>(0x360A, val);

	node_id parent_id = m_bag.get_node().get_parent_id();

	if(parent_id == get_id())
		return;

	folder parent(m_db, m_db->lookup_node(parent_id));
	table ht = parent.get_hierarchy_table();
	ht.set_cell_value(ht.lookup_row(get_id()), 0x360A, val);
	ht.save_table();
}

inline void pstsdk::folder::delete_message(pstsdk::message& msg)
{
	delete_message(msg.get_property_bag().get_node().get_id());
}

inline void pstsdk::folder::delete_message(pstsdk::node_id nid_msg)
{
	get_contents_table().delete_row(get_contents_table().lookup_row(nid_msg));

	size_t msg_cnt = get_message_count();
	m_bag.write_prop<slong>(0x3602, (msg_cnt > 0 ? msg_cnt - 1 : 0));

	m_bag.get_node().get_db()->delete_node(nid_msg);
}

inline void pstsdk::folder::delete_subfolder(pstsdk::folder& fldr)
{
	pstsdk::node_id nid_fldr = fldr.get_property_bag().get_node().get_id();

	if(nid_fldr == nid_root_folder)
		throw std::invalid_argument("root folder cannot be deleted");

	// remove row from hierarchy table of parent folder
	get_hierarchy_table().delete_row(get_hierarchy_table().lookup_row(nid_fldr));

	if(m_hierarchy_table->size() == 0)
		m_bag.write_prop<bool>(0x360A, false);

	// delete all contained messages
	std::vector<pstsdk::node_id> msg_ids;

	for(message_iterator msg_iter = fldr.message_begin(); msg_iter != fldr.message_end(); ++msg_iter)
		msg_ids.push_back(msg_iter->get_property_bag().get_node().get_id());

	for(size_t ind = 0; ind < msg_ids.size(); ++ind)
		fldr.delete_message(msg_ids[ind]);

	// delete all contained subfolders
	std::vector<pstsdk::node_id> fld_ids;

	for(folder_iterator fld_iter = fldr.sub_folder_begin(); fld_iter != fldr.sub_folder_end(); ++fld_iter)
		fld_ids.push_back(fld_iter->get_property_bag().get_node().get_id());

	for(size_t ind = 0; ind < fld_ids.size(); ++ind)
		fldr.delete_subfolder(fld_ids[ind]);

	// delete subfolder object
	m_bag.get_node().get_db()->delete_node(nid_fldr);
	m_bag.get_node().get_db()->delete_node(make_nid(nid_type_hierarchy_table, get_nid_index(nid_fldr)));
	m_bag.get_node().get_db()->delete_node(make_nid(nid_type_contents_table, get_nid_index(nid_fldr)));
	m_bag.get_node().get_db()->delete_node(make_nid(nid_type_associated_contents_table, get_nid_index(nid_fldr)));
}

inline void pstsdk::folder::delete_subfolder(pstsdk::node_id nid_fldr)
{
	if(nid_fldr == nid_root_folder)
		throw std::invalid_argument("root folder cannot be deleted");

	delete_subfolder(folder(m_db, m_db->lookup_node(nid_fldr)));
}
//! \endcond

#endif
