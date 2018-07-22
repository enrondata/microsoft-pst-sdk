//! \file
//! \brief Property Bag (or Property Context, or PC) implementation
//! \author Terry Mahaffey
//! \ingroup ltp

#ifndef PSTSDK_LTP_PROPBAG_H
#define PSTSDK_LTP_PROPBAG_H

#include <vector>
#include <algorithm>

#include "pstsdk/util/primitives.h"
#include "pstsdk/util/errors.h"

#include "pstsdk/ndb/node.h"

#include "pstsdk/ltp/object.h"
#include "pstsdk/ltp/heap.h"

namespace pstsdk
{

	//! \addtogroup ltp_objectrelated
	//@{
	typedef bth_node<prop_id, disk::prop_entry> pc_bth_node;
	typedef bth_nonleaf_node<prop_id, disk::prop_entry> pc_bth_nonleaf_node;
	typedef bth_leaf_node<prop_id, disk::prop_entry> pc_bth_leaf_node;
	//@}

	//! \brief Property Context (PC) Implementation
	//!
	//! A Property Context is simply a BTH where the BTH is stored as the client
	//! root allocation in the heap. The BTH contains a "prop_entry", which is
	//! defines the type of the property and it's storage. 
	//!
	//! const_property_object does most of the heavy lifting in terms of 
	//! property access and interpretation.
	//! \sa [MS-PST] 2.3.3
	//! \ingroup ltp_objectrelated
	class property_bag : public const_property_object
	{
	public:
		//! \brief Construct a property_bag from this node
		//! \param[in] n The node to copy and interpret as a property_bag
		explicit property_bag(const node& n);
		//! \brief Construct a property_bag from this node
		//! \param[in] n The node to alias and interpret as a property_bag
		property_bag(const node& n, alias_tag);
		//! \brief Construct a property_bag from this heap
		//! \param[in] h The heap to copy and interpret as a property_bag
		explicit property_bag(const heap& h);
		//! \brief Construct a property_bag from this heap
		//! \param[in] h The heap to alias and interpret as a property_bag
		property_bag(const heap& h, alias_tag);
		//! \brief Copy construct a property_bag
		//! \param other The property bag to copy
		property_bag(const property_bag& other);
		//! \brief Alias a property_bag
		//! \param other The property bag to alias
		property_bag(const property_bag& other, alias_tag);

#ifndef BOOST_NO_RVALUE_REFERENCES
		//! \brief Move construct a property_bag
		//! \param other The property bag to move from
		property_bag(property_bag&& other) : m_pbth(std::move(other.m_pbth)) { }
#endif

		std::vector<prop_id> get_prop_list() const;
		prop_type get_prop_type(prop_id id) const
		{ return (prop_type)m_pbth->lookup(id).type; }
		bool prop_exists(prop_id id) const;
		size_t size(prop_id id) const;
		hnid_stream_device open_prop_stream(prop_id id);

		//! \brief Get the node underlying this property_bag
		//! \returns The node
		const node& get_node() const { return m_pbth->get_node(); }
		//! \brief Get the node underlying this property_bag
		//! \returns The node
		node& get_node() { return m_pbth->get_node(); }

		//! \cond write_api
		//! \brief Save changes made to this property_bag
		void save_property_bag() { m_pbth->save_bth(); }

		//! \brief Write a new property of given type
		//!
		//! It is the callers responsibility to ensure the prop_id is of or 
		//! convertable to the requested type.
		//! \tparam T The type to interpret the property as
		//! \param[in] id The property id
		//! \param[in] value The property value
		//! \throws invalid_argument if the property type is not a POD or one of the specialized classes
		template<typename T>
		void write_prop(prop_id id, T value);

		//! \brief Write a new property as an array of the given type
		//!
		//! It is the callers responsibility to ensure the prop_id is of or 
		//! convertable to the requested type.
		//! \tparam T The type to interpret the property as
		//! \param[in] id The prop_id
		//! \param[in] write_buffer A vector of the property values
		//! \throws invalid_argument if the property type is not a POD or one of the specialized classes
		template<typename T>
		void write_prop_array(prop_id id, std::vector<T> &write_buffer);

		//! \brief Modify a property value
		//! \tparam T The type to interpret the property as
		//! \param[in] id The prop_id
		//! \param[in] value The new property value
		//! \throws key_not_found<prop_id> If the specified property is not present
		template<typename T>
		void modify_prop(prop_id id, T value);

		//! \brief Modify a property as an array of values
		//! \tparam T The type to interpret the property as
		//! \param[in] id The prop_id
		//! \param[in] value A vector of new property values
		//! \throws key_not_found<prop_id> If the specified property is not present
		template<typename T>
		void modify_prop_array(prop_id id, std::vector<T> &value);

		//! \brief Remove a property value
		//! \param[in] id The prop_id
		void remove_prop(prop_id id);
		//! \endcond

	private:
		//! \cond write_api
		template<typename T>
		void write_multi_values(prop_id id, std::vector<T> &buffer, prop_type type);
		void set_value_1(prop_id id, byte value);
		void set_value_2(prop_id id, ushort value);
		void set_value_4(prop_id id, ulong value);
		void set_value_8(prop_id id, ulonglong value);
		void set_value_variable(prop_id id, std::vector<byte> &buffer, prop_type type);
		//! \endcond

		property_bag& operator=(const property_bag& other); // = delete

		byte get_value_1(prop_id id) const
		{ return (byte)m_pbth->lookup(id).id; }
		ushort get_value_2(prop_id id) const
		{ return (ushort)m_pbth->lookup(id).id; }
		ulong get_value_4(prop_id id) const
		{ return (ulong)m_pbth->lookup(id).id; }
		ulonglong get_value_8(prop_id id) const;
		std::vector<byte> get_value_variable(prop_id id) const;
		void get_prop_list_impl(std::vector<prop_id>& proplist, const pc_bth_node* pbth_node) const;

		std::tr1::shared_ptr<pc_bth_node> m_pbth;
	};

} // end pstsdk namespace

//! \cond write_api
inline void pstsdk::property_bag::set_value_1(pstsdk::prop_id id, byte value)
{
	disk::prop_entry entry = { prop_type_binary, value };
	m_pbth = m_pbth->insert(id, entry);
}

inline void pstsdk::property_bag::set_value_2(pstsdk::prop_id id, ushort value)
{
	disk::prop_entry entry = { prop_type_short, value };
	m_pbth = m_pbth->insert(id, entry);
}

inline void pstsdk::property_bag::set_value_4(pstsdk::prop_id id, ulong value)
{
	disk::prop_entry entry = { prop_type_long, value };
	m_pbth = m_pbth->insert(id, entry);
}

inline void pstsdk::property_bag::set_value_8(pstsdk::prop_id id, ulonglong value)
{
	std::vector<byte> buffer(sizeof(ulonglong));
	memcpy(&buffer[0], &value, sizeof(ulonglong));
	set_value_variable(id, buffer, prop_type_longlong);
}

inline void pstsdk::property_bag::set_value_variable(pstsdk::prop_id id, std::vector<byte> &buffer, prop_type type)
{
	disk::prop_entry entry = { 0, 0 };

	if(prop_exists(id))
	{
		entry = m_pbth->lookup(id);
		entry.id = (heapnode_id)get_value_4(id);

		if(is_subnode_id(entry.id))
		{
			node sb_nd(m_pbth->get_node().lookup(entry.id));
			sb_nd.resize(buffer.size());
			sb_nd.write(buffer, 0);
			sb_nd.save_node();

			m_pbth = m_pbth->insert(id, entry);
			return;
		}
	}

	if(buffer.size() < disk::heap_max_alloc_size)
	{
		entry.id =  entry.id == 0 ? m_pbth->get_heap_ptr()->allocate_heap_item(buffer.size()) : m_pbth->get_heap_ptr()->re_allocate_heap_item(entry.id, buffer.size());
		m_pbth->get_heap_ptr()->write(buffer, entry.id);
		entry.type = type;
	}
	else
	{
		if(entry.id != 0)
			m_pbth->get_heap_ptr()->free_heap_item(entry.id);

		node sb_nd = m_pbth->get_node().create_subnode(make_nid(nid_type_ltp, id));
		sb_nd.resize(buffer.size());
		sb_nd.write(buffer, 0);
		sb_nd.save_node();

		entry.type = type;
		entry.id = sb_nd.get_id();
	}

	m_pbth = m_pbth->insert(id, entry);
}

template<typename T>
inline void pstsdk::property_bag::write_multi_values(pstsdk::prop_id id, std::vector<T> &buffer, pstsdk::prop_type type)
{
	disk::prop_entry entry = { 0, 0 };
	size_t tot_prop_size = buffer.size() * sizeof(T);

	if(prop_exists(id))
	{
		entry = m_pbth->lookup(id);
		entry.id = (heapnode_id)get_value_4(id);

		if(is_subnode_id(entry.id))
		{
			node sb_nd(m_pbth->get_node().lookup(entry.id));
			sb_nd.resize(tot_prop_size);
			pstsdk::byte *begin = reinterpret_cast<pstsdk::byte*>(&buffer[0]);
			std::vector<pstsdk::byte> buff(begin, begin + tot_prop_size);
			sb_nd.write(buff, 0);
			sb_nd.save_node();

			m_pbth = m_pbth->insert(id, entry);
			return;
		}
	}

	if(tot_prop_size < disk::heap_max_alloc_size)
	{
		entry.id =  entry.id == 0 ? m_pbth->get_heap_ptr()->allocate_heap_item(tot_prop_size) : m_pbth->get_heap_ptr()->re_allocate_heap_item(entry.id, tot_prop_size);
		entry.type = type;
		pstsdk::byte *begin = reinterpret_cast<pstsdk::byte*>(&buffer[0]);
		std::vector<pstsdk::byte> buff(begin, begin + tot_prop_size);
		m_pbth->get_heap_ptr()->write(buff, entry.id);
	}
	else
	{
		if(entry.id != 0)
			m_pbth->get_heap_ptr()->free_heap_item(entry.id);

		node sb_nd = m_pbth->get_node().create_subnode(make_nid(nid_type_ltp, id));
		sb_nd.resize(tot_prop_size);

		pstsdk::byte *begin = reinterpret_cast<pstsdk::byte*>(&buffer[0]);
		std::vector<pstsdk::byte> buff(begin, begin + tot_prop_size);
		sb_nd.write(buff, 0);
		sb_nd.save_node();

		entry.type = type;
		entry.id = sb_nd.get_id();
	}

	m_pbth = m_pbth->insert(id, entry);
}

inline void pstsdk::property_bag::remove_prop(prop_id id)
{
	disk::prop_entry entry = m_pbth->lookup(id);

	heap_id hid = entry.id;

	if(entry.type != prop_type_null && entry.type != prop_type_boolean && entry.type != prop_type_float &&
		entry.type != prop_type_long && entry.type != prop_type_short)
	{
		if(is_subnode_id(hid))
		{
			m_pbth->get_node().delete_subnode(hid);
		}
		else
		{
			m_pbth->get_heap_ptr()->free_heap_item(hid);
		}
	}

	m_pbth->remove(id);
}

template<typename T>
inline void pstsdk::property_bag::modify_prop(prop_id id, T value)
{
	if(!prop_exists(id))
		throw key_not_found<prop_id>(id);

	write_prop<T>(id, value);
}

template<typename T>
inline void pstsdk::property_bag::modify_prop_array(prop_id id, std::vector<T> &value)
{
	if(!prop_exists(id))
		throw key_not_found<prop_id>(id);

	write_prop_array<T>(id, value);
}

template<typename T>
inline void pstsdk::property_bag::write_prop(prop_id id, T value)
{
	if(!std::tr1::is_pod<T>::value)
		throw std::invalid_argument("T must be a POD or one of the specialized classes");

	if(sizeof(T) == sizeof(pstsdk::byte))
	{
		set_value_1(id, static_cast<pstsdk::byte>(value));
	}
	else if(sizeof(T) == sizeof(ushort))
	{
		set_value_2(id, static_cast<ushort>(value));
	}
	else if(sizeof(T) == sizeof(ulong))
	{
		set_value_4(id, static_cast<ulong>(value));
	}
	else if(sizeof(T) == sizeof(ulonglong))
	{
		set_value_8(id, static_cast<ulonglong>(value));
	}
	else
	{
		set_value_variable(id, *(reinterpret_cast<std::vector<byte>*>(&value)), prop_type_unspecified);
	}
}

template<typename T>
inline void pstsdk::property_bag::write_prop_array(prop_id id, std::vector<T> &write_buffer)
{
	if(!std::tr1::is_pod<T>::value)
		throw std::invalid_argument("T must be a POD or one of the specialized classes");

	if(sizeof(T) == sizeof(pstsdk::byte))
		write_multi_values(id, write_buffer, prop_type_binary);
	else if(sizeof(T) == sizeof(ushort))
		write_multi_values(id, write_buffer, prop_type_mv_short);
	else if(sizeof(T) == sizeof(ulong))
		write_multi_values(id, write_buffer, prop_type_mv_long);
	else if(sizeof(T) == sizeof(ulonglong))
		write_multi_values(id, write_buffer, prop_type_mv_longlong);
	else
		write_multi_values(id, write_buffer, prop_type_unspecified);
}

template<>
inline void pstsdk::property_bag::write_prop<std::vector<byte>>(prop_id id, std::vector<byte> value)
{
	set_value_variable(id, value, pstsdk::prop_type_binary);
}

template<>
inline void pstsdk::property_bag::write_prop_array<std::vector<pstsdk::byte>>(prop_id id, std::vector<std::vector<pstsdk::byte>> &write_buffer)
{
	std::vector<byte> mv_toc(sizeof(ulong) + write_buffer.size() * sizeof(ulong));
	disk::mv_toc *ptoc = reinterpret_cast<disk::mv_toc*>(&mv_toc[0]);

	std::vector<byte> mv_buff(mv_toc.size());

	ptoc->count = write_buffer.size();

	for(size_t ind = 0; ind < write_buffer.size(); ++ind)
	{
		std::vector<pstsdk::byte> item(write_buffer[ind]); 
		ptoc->offsets[ind] = mv_buff.size();
		mv_buff.insert(mv_buff.end(), item.begin(), item.end());
	}

	memcpy(&mv_buff[0], &mv_toc[0], mv_toc.size());
	write_multi_values(id, mv_buff, prop_type_mv_binary);
}

template<>
inline void pstsdk::property_bag::write_prop<std::string>(prop_id id, std::string value)
{
	if(value.size() == 0)
		return;

	pstsdk::byte *begin = reinterpret_cast<pstsdk::byte*>(&value[0]);
	std::vector<pstsdk::byte> buff(begin, begin + value.size()*sizeof(value[0]));

	set_value_variable(id, buff, pstsdk::prop_type_string);
}

template<>
inline void pstsdk::property_bag::write_prop_array<std::string>(prop_id id, std::vector<std::string> &write_buffer)
{
	std::vector<std::vector<pstsdk::byte>> str_buff;

	for(size_t ind = 0; ind < write_buffer.size(); ++ind)
	{
		std::string value = write_buffer[ind];
		pstsdk::byte *begin = reinterpret_cast<pstsdk::byte*>(&value[0]);
		std::vector<pstsdk::byte> buff(begin, begin + value.size()*sizeof(value[0]));
		str_buff.push_back(buff);
	}

	std::vector<byte> mv_toc(sizeof(ulong) + str_buff.size() * sizeof(ulong));
	disk::mv_toc *ptoc = reinterpret_cast<disk::mv_toc*>(&mv_toc[0]);

	std::vector<byte> mv_buff(mv_toc.size());

	ptoc->count = str_buff.size();

	for(size_t ind = 0; ind < str_buff.size(); ++ind)
	{
		std::vector<pstsdk::byte> item(str_buff[ind]); 
		ptoc->offsets[ind] = mv_buff.size();
		mv_buff.insert(mv_buff.end(), item.begin(), item.end());
	}

	memcpy(&mv_buff[0], &mv_toc[0], mv_toc.size());
	write_multi_values(id, mv_buff, prop_type_mv_string);
}

template<>
inline void pstsdk::property_bag::write_prop<std::wstring>(prop_id id, std::wstring value)
{
	if(value.size() == 0)
		return;

	if(prop_exists(id) && get_prop_type(id) == prop_type_string)
	{
		std::string str(value.begin(), value.end());
		pstsdk::byte *begin = reinterpret_cast<pstsdk::byte*>(&str[0]);
		std::vector<pstsdk::byte> buff(begin, begin + str.size()*sizeof(str[0]));
		set_value_variable(id, buff, pstsdk::prop_type_string);
	}
	else
	{
		set_value_variable(id, pstsdk::wstring_to_bytes(value), pstsdk::prop_type_wstring);
	}
}

template<>
inline void pstsdk::property_bag::write_prop_array<std::wstring>(prop_id id, std::vector<std::wstring> &write_buffer)
{
	std::vector<std::vector<pstsdk::byte>> str_buff;

	for(size_t ind = 0; ind < write_buffer.size(); ++ind)
	{
		if(prop_exists(id) && get_prop_type(id) == prop_type_string)
		{
			std::wstring value = write_buffer[ind];
			std::string str(value.begin(), value.end());
			pstsdk::byte *begin = reinterpret_cast<pstsdk::byte*>(&str[0]);
			std::vector<pstsdk::byte> buff(begin, begin + str.size()*sizeof(str[0]));
			str_buff.push_back(buff);
		}
		else
		{
			str_buff.push_back(wstring_to_bytes(write_buffer[ind]));
		}
	}

	std::vector<byte> mv_toc(sizeof(ulong) + str_buff.size() * sizeof(ulong));
	disk::mv_toc *ptoc = reinterpret_cast<disk::mv_toc*>(&mv_toc[0]);

	std::vector<byte> mv_buff(mv_toc.size());

	ptoc->count = str_buff.size();

	for(size_t ind = 0; ind < str_buff.size(); ++ind)
	{
		std::vector<pstsdk::byte> item(str_buff[ind]); 
		ptoc->offsets[ind] = mv_buff.size();
		mv_buff.insert(mv_buff.end(), item.begin(), item.end());
	}

	memcpy(&mv_buff[0], &mv_toc[0], mv_toc.size());
	write_multi_values(id, mv_buff, prop_type_mv_wstring);
}

template<>
inline void pstsdk::property_bag::write_prop<std::time_t>(prop_id id, time_t value)
{
	ulonglong time_val = time_t_to_filetime(value);
	write_prop<ulonglong>(id, time_val);
}

template<>
inline void pstsdk::property_bag::write_prop_array<std::time_t>(prop_id id, std::vector<std::time_t> &write_buffer)
{
	write_multi_values(id, write_buffer, prop_type_mv_apptime);
}

template<>
inline void pstsdk::property_bag::write_prop<bool>(prop_id id, bool value)
{
	disk::prop_entry entry = { prop_type_boolean, value };
	m_pbth = m_pbth->insert(id, entry);
}

template<>
inline void pstsdk::property_bag::write_prop_array<bool>(prop_id id, std::vector<bool> &write_buffer)
{
	write_multi_values(id, write_buffer, prop_type_binary);
}
//! \endcond

//! \cond write_api
inline pstsdk::property_bag::property_bag(const pstsdk::node& n)
{
	heap h(n, disk::heap_sig_pc);

	if(h.get_root_id() == 0)
	{
		// empty heap node
		h.set_root_id(h.create_bth<prop_id, disk::prop_entry>());
	}

	m_pbth = h.open_bth<prop_id, disk::prop_entry>(h.get_root_id());
}
//! \endcond

inline pstsdk::property_bag::property_bag(const pstsdk::node& n, alias_tag)
{
	heap h(n, disk::heap_sig_pc, alias_tag());

	m_pbth = h.open_bth<prop_id, disk::prop_entry>(h.get_root_id());
}

//! \cond write_api
inline pstsdk::property_bag::property_bag(const pstsdk::heap& h)
{
#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(h.get_client_signature() != disk::heap_sig_pc)
		throw sig_mismatch("expected heap_sig_pc", 0, h.get_node().get_id(), h.get_client_signature(), disk::heap_sig_pc);
#endif

	heap my_heap(h);

	if(my_heap.get_root_id() == 0)
	{
		// empty heap node
		my_heap.set_root_id(my_heap.create_bth<prop_id, disk::prop_entry>());
	}

	m_pbth = my_heap.open_bth<prop_id, disk::prop_entry>(my_heap.get_root_id());
}
//! \endcond

inline pstsdk::property_bag::property_bag(const pstsdk::heap& h, alias_tag)
{
#ifdef PSTSDK_VALIDATION_LEVEL_WEAK
	if(h.get_client_signature() != disk::heap_sig_pc)
		throw sig_mismatch("expected heap_sig_pc", 0, h.get_node().get_id(), h.get_client_signature(), disk::heap_sig_pc);
#endif

	heap my_heap(h, alias_tag());

	m_pbth = my_heap.open_bth<prop_id, disk::prop_entry>(my_heap.get_root_id());
}

inline pstsdk::property_bag::property_bag(const property_bag& other)
{
	heap h(other.m_pbth->get_node());

	m_pbth = h.open_bth<prop_id, disk::prop_entry>(h.get_root_id());
}

inline pstsdk::property_bag::property_bag(const property_bag& other, alias_tag)
{
	heap h(other.m_pbth->get_node(), alias_tag());

	m_pbth = h.open_bth<prop_id, disk::prop_entry>(h.get_root_id());
}

inline std::vector<pstsdk::prop_id> pstsdk::property_bag::get_prop_list() const
{
	std::vector<prop_id> proplist;

	get_prop_list_impl(proplist, m_pbth.get());

	return proplist;
}

inline void pstsdk::property_bag::get_prop_list_impl(std::vector<prop_id>& proplist, const pc_bth_node* pbth_node) const
{
	if(pbth_node->get_level() == 0)
	{
		// leaf
		const pc_bth_leaf_node* pleaf = static_cast<const pc_bth_leaf_node*>(pbth_node);

		for(uint i = 0; i < pleaf->num_values(); ++i)
			proplist.push_back(pleaf->get_key(i));
	}
	else
	{
		// non-leaf
		const pc_bth_nonleaf_node* pnonleaf = static_cast<const pc_bth_nonleaf_node*>(pbth_node); 
		for(uint i = 0; i < pnonleaf->num_values(); ++i)
			get_prop_list_impl(proplist, pnonleaf->get_child(i));
	}
}

inline bool pstsdk::property_bag::prop_exists(prop_id id) const
{
	try
	{
		(void)m_pbth->lookup(id);
	}
	catch(key_not_found<prop_id>&)
	{
		return false;
	}

	return true;
}

inline pstsdk::ulonglong pstsdk::property_bag::get_value_8(prop_id id) const
{
	std::vector<byte> buffer = get_value_variable(id);

	return *(ulonglong*)&buffer[0];
}

inline std::vector<pstsdk::byte> pstsdk::property_bag::get_value_variable(prop_id id) const
{
	heapnode_id h_id = (heapnode_id)get_value_4(id);
	std::vector<byte> buffer;

	if(is_subnode_id(h_id))
	{
		node sub(m_pbth->get_node().lookup(h_id));
		buffer.resize(sub.size());
		sub.read(buffer, 0);
	}
	else
	{
		buffer = m_pbth->get_heap_ptr()->read(h_id);
	}

	return buffer;
}

inline size_t pstsdk::property_bag::size(prop_id id) const
{
	heapnode_id h_id = (heapnode_id)get_value_4(id);

	if(is_subnode_id(h_id))
		return node(m_pbth->get_node().lookup(h_id)).size();
	else
		return m_pbth->get_heap_ptr()->size(h_id);
}

inline pstsdk::hnid_stream_device pstsdk::property_bag::open_prop_stream(prop_id id)
{
	heapnode_id h_id = (heapnode_id)get_value_4(id);

	if(h_id == 0)
		return m_pbth->get_heap_ptr()->open_stream(h_id);

	if(is_subnode_id(h_id))
		return m_pbth->get_node().lookup(h_id).open_as_stream();
	else
		return m_pbth->get_heap_ptr()->open_stream(h_id);
}
#endif
