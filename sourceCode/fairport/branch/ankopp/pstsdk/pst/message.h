//! \file
//! \brief Message related classes
//! \author Terry Mahaffey
//!
//! Defines the message, attachment, and recipient abstractions.
//! \ingroup pst

#ifndef PSTSDK_PST_MESSAGE_H
#define PSTSDK_PST_MESSAGE_H

#include <functional>
#include <ostream>
#include <boost/iterator/transform_iterator.hpp>

#include "pstsdk/ndb/database_iface.h"
#include "pstsdk/ndb/node.h"

#include "pstsdk/ltp/propbag.h"
#include "pstsdk/ltp/table.h"

namespace pstsdk
{

	//! \defgroup pst_messagerelated Message Objects
	//! \ingroup pst

	class message;
	//! \brief Encapsulates an attachment to a message
	//! 
	//! Attachment objects allow you to query for some basic information about
	//! an attachment, get access to the bytes of the attachment (as a blob or
	//! stream), as well as open the attachment as a message if applicable.
	//! \ingroup pst_messagerelated
	class attachment
	{
	public:
		// property access
		//! \brief Get the filename of this attachment
		//! \returns The filename
		std::wstring get_filename() const;
		//! \brief Get the attachment data, as a blob
		//!
		//! You might want to consider open_byte_stream if content_size()
		//! is too large for your tastes.
		//! \returns A vector of bytes
		std::vector<byte> get_bytes() const
		{ return m_bag.read_prop<std::vector<byte> >(0x3701); }
		//! \brief Open a stream of the attachment data
		//! 
		//! The returned stream device can be used to construct a proper stream:
		//! \code
		//! attachment a = ...;
		//! prop_stream nstream(a.open_byte_stream());
		//! \endcode
		//! Which can then be used as any iostream would be.
		//! \returns A stream device for the attachment data
		hnid_stream_device open_byte_stream()
		{ return m_bag.open_prop_stream(0x3701); }
		//! \brief Read the size of this attachment
		//!
		//! The size returned here includes metadata, and as such will be
		//! larger than just the byte stream.
		//! \sa attachment::content_size()
		//! \returns The size of the attachment object, in bytes
		size_t size() const
		{ return m_bag.read_prop<uint>(0xe20); }
		//! \brief Read the size of the content in this attachment
		//!
		//! The size here is just for the binary data of the attachment.
		//! \returns The size of the data stream of the attachment, in bytes
		size_t content_size() const
		{ return m_bag.size(0x3701); }
		//! \brief Returns if this attachment is actually an embedded message
		//!
		//! If an attachment is a message, one should use open_as_message() to
		//! access the data rather than open_byte_stream() or similar.
		//! \returns true if this attachment is an attached message
		bool is_message() const
		{ return m_bag.read_prop<uint>(0x3705) == 5; }
		//! \brief Interpret this attachment as a message
		//! \pre is_message() == true
		//! \returns A message object
		message open_as_message() const;

		// lower layer access
		//! \brief Get the property bag backing this attachment
		//! \returns The property bag
		property_bag& get_property_bag()
		{ return m_bag; }
		//! \copydoc attachment::get_property_bag()
		const property_bag& get_property_bag() const
		{ return m_bag; }

		//! \cond write_api
		//! \brief Write the size of this attachment
		//!
		//! The size includes metadata, and as such will be larger than just the byte stream size.
		//! \param[in] val The size value
		//! \param[in] container_msg The message this attachment is part of
		void set_size(size_t val, pstsdk::message& container_msg);

		//! \brief Set the attachment data as blob 
		//! \param[in] val The vector of attachment data
		//! \param[in] container_msg The message this attachment is part of
		void set_bytes(std::vector<byte> val, pstsdk::message& container_msg);

		//! \brief Set the filename of this attachment
		//! \param[in] val The file name of this attachment
		//! \param[in] container_msg The message this attachment is part of
		void set_filename(std::wstring val, pstsdk::message& container_msg);

		//! \brief Save this attachment onto parent message
		//! \param[in] container_msg The message this attachment is part of
		void save_attachment(pstsdk::message& container_msg);

		//! \brief Set the attachment method
		//! \param[in] val The attachment method value
		//! \param[in] container_msg The message this attachment is part of
		void set_attachment_method(ulong val, message& container_msg);
		//! \endcond
	private:
		attachment& operator=(const attachment&); // = delete
		friend class message;
		friend class attachment_transform;

#ifndef BOOST_NO_RVALUE_REFERENCES
		explicit attachment(property_bag attachment)
			: m_bag(std::move(attachment)) { }
#else
		explicit attachment(const property_bag& attachment)
			: m_bag(attachment) { }
#endif
		property_bag m_bag;
	};

	//! \brief operator<< overload for writing an attachment to an ostream
	//! \param[in,out] out the ostream to write an attachment to
	//! \param[in] attach The attachment to write
	//! \returns The output stream
	//! \ingroup pst_messagerelated
	inline std::ostream& operator<<(std::ostream& out, const attachment& attach)
	{
		std::vector<byte> data = attach.get_bytes();
		out.write(reinterpret_cast<char*>(&data[0]), data.size());
		return out;
	}

	//! \brief Defines a transform from a row to an attachment object
	//!
	//! Used by the boost iterator library to provide iterators over attachment objects
	//! \ingroup pst_messagerelated
	class attachment_transform : public std::unary_function<const_table_row, attachment>
	{
	public:
		//! \brief Construct the transform object
		//! \param[in] n The node backing the message which has these attachments
		explicit attachment_transform(const node& n) 
			: m_node(n) { }
#ifndef BOOST_NO_RVALUE_REFERENCES
		//! \brief Move constructor for transform objects
		//! \param[in] other The transform object to move from
		attachment_transform(attachment_transform&& other)
			: m_node(std::move(other.m_node)) { }
#endif
		//! \brief Perform the transform
		//! \param[in] row A row from the messages attachment table
		//! \returns An attachment object
		attachment operator()(const const_table_row& row) const
		{ return attachment(property_bag(m_node.lookup(row.get_row_id()))); }

	private:
		node m_node;
	};

	//! \brief A recipient of a message
	//!
	//! A friendly wrapper around a recipient table row.
	//! \ingroup pst_messagerelated
	class recipient
	{
	public:
		// property access
		//! \brief Get the display name of this recipient
		//! \returns The recipient name
		std::wstring get_name() const
		{ return m_row.read_prop<std::wstring>(0x3001); }
		//! \brief Get the type of this recipient
		//! \returns The recipient type
		recipient_type get_type() const
		{ return static_cast<recipient_type>(m_row.read_prop<ulong>(0xc15)); }
		//! \brief Get the address type of the recipient
		//! \returns The address type
		std::wstring get_address_type() const
		{ return m_row.read_prop<std::wstring>(0x3002); }
		//! \brief Get the email address of the recipient
		//! \returns The email address
		std::wstring get_email_address() const
		{ return m_row.read_prop<std::wstring>(0x39fe); }
		//! \brief Checks to see if this recipient has an email address
		//! \returns true if get_email_address() doesn't throw
		bool has_email_address() const
		{ return m_row.prop_exists(0x39fe); }
		//! \brief Get the name of the recipients account
		//! \returns The account name
		std::wstring get_account_name() const
		{ return m_row.read_prop<std::wstring>(0x3a00); }
		//! \brief Checks to see if this recipient has an account name
		//! \returns true if get_account_name() doesn't throw
		bool has_account_name() const
		{ return m_row.prop_exists(0x3a00); }

		// lower layer access
		//! \brief Get the property row underying this recipient object
		//! \returns The property row
		const_table_row& get_property_row()
		{ return m_row; }
		//! \copydoc recipient::get_property_row()
		const const_table_row& get_property_row() const
		{ return m_row; }

		//! \cond write_api
		//! \brief Set the name of this recipient
		//! \param[in] val The recipient name
		void set_name(std::wstring val)
		{ m_row.get_table()->write_cell(m_row.get_row_pos(), 0x3001, wstring_to_bytes(val)); }

		//! \brief Set the type of this recipient
		//! \param[in] val The recipient type value
		void set_type(recipient_type val)
		{ m_row.get_table()->set_cell_value(m_row.get_row_pos(), 0xc15, static_cast<ulong>(val)); }

		//! \brief Set the email address type of this recipient
		//! \param[in] val The email address type
		void set_address_type(std::wstring val)
		{ m_row.get_table()->write_cell(m_row.get_row_pos(), 0x3002, wstring_to_bytes(val)); }

		//! \brief Set the email address of this recipient
		//! \param[in] val The email address
		void set_email_address(std::wstring val)
		{ m_row.get_table()->write_cell(m_row.get_row_pos(), 0x39fe, wstring_to_bytes(val)); }

		//! \brief Set the account name of this recipient
		//! \param[in] val The account name
		void set_account_name(std::wstring val)
		{ m_row.get_table()->write_cell(m_row.get_row_pos(), 0x3a00, wstring_to_bytes(val)); }

		//! \brief Save this recipient
		//! \param[in] container_msg The message this recipient is part of
		void save_recipient(pstsdk::message &container_msg);
		//! \endcond

	private:
		recipient& operator=(const recipient&); // = delete
		friend struct recipient_transform;

		explicit recipient(const const_table_row& row)
			: m_row(row) { }
		const_table_row m_row;
	};

	//! \brief Defines a transform from a row to a recipient object
	//!
	//! Used by the boost iterator library to provide iterators over recipient objects
	//! \ingroup pst_messagerelated
	struct recipient_transform : public std::unary_function<const_table_row, recipient>
	{
		recipient operator()(const_table_row row) const
		{ return recipient(row); }
	};

	//! \brief Represents a message in a PST file
	//!
	//! A message is the basic abstraction exposed by MAPI - everything is a
	//! message; a mail item, a calendar item, a contact, etc.
	//!
	//! This class exposes some common properties on messages
	//! as well as iteration mechanisms to get to the attachments and recipients
	//! on the message. You will most likely find it necessary to get the 
	//! underylying property bag and look up specific properties not exposed here.
	//! \ingroup pst_messagerelated
	class message
	{
	public:
		//! \brief Attachment iterator type; a transform iterator over a table row iterator
		typedef boost::transform_iterator<attachment_transform, const_table_row_iter> attachment_iterator;
		//! \brief Recipient iterator type; a transform iterator over a table row iterator
		typedef boost::transform_iterator<recipient_transform, const_table_row_iter> recipient_iterator;

		//! \brief Construct a message object
		//! \param[in] n A message node
		explicit message(const node& n)
			: m_bag(n) { }
		message(const message& other);

#ifndef BOOST_NO_RVALUE_REFERENCES
		message(message&& other)
			: m_bag(std::move(other.m_bag)), m_attachment_table(std::move(other.m_attachment_table)), m_recipient_table(std::move(other.m_recipient_table)) { }
#endif     

		// subobject discovery/enumeration
		//! \brief Get an iterator to the first message on this message
		//! \returns an iterator positioned on the first attachment on this message
		attachment_iterator attachment_begin() const
		{ return boost::make_transform_iterator(get_attachment_table().begin(), attachment_transform(m_bag.get_node())); }
		//! \brief Get the end attachment iterator
		//! \returns An iterator at the end position
		attachment_iterator attachment_end() const
		{ return boost::make_transform_iterator(get_attachment_table().end(), attachment_transform(m_bag.get_node())); }
		//! \brief Get an iterator to the first recipient of this message
		//! \returns An iterator positioned on the first recipient of this message
		recipient_iterator recipient_begin() const
		{ return boost::make_transform_iterator(get_recipient_table().begin(), recipient_transform()); }
		//! \brief Get the end recipient iterator
		//! \returns An iterator at the end position
		recipient_iterator recipient_end() const
		{ return boost::make_transform_iterator(get_recipient_table().end(), recipient_transform()); }

		// property access
		//! \brief Get the subject of this message
		//! \returns The message subject
		std::wstring get_subject() const;
		//! \brief Check to see if a subject is set on this message
		//! \returns true if a subject is set on this message
		bool has_subject() const
		{ return m_bag.prop_exists(0x37); }
		//! \brief Get the body of this message
		//! \returns The message body as a string
		std::wstring get_body() const
		{ return m_bag.read_prop<std::wstring>(0x1000); }
		//! \brief Get the body of this message
		//! 
		//! The returned stream device can be used to construct a proper stream:
		//! \code
		//! message m = ...;
		//! prop_stream body(a.open_body_stream());
		//! \endcode
		//! Which can then be used as any iostream would be.
		//! \returns The message body as a stream
		hnid_stream_device open_body_stream()
		{ return m_bag.open_prop_stream(0x1000); }
		//! \brief Size of the body, in bytes
		//! \returns The size of the body
		size_t body_size() const
		{ return m_bag.size(0x1000); }
		//! \brief Checks to see if this message has a body
		//! \returns true if the body prop exists
		bool has_body() const
		{ return m_bag.prop_exists(0x1000); }
		//! \brief Get the HTML body of this message
		//! \returns The HTML message body as a string
		std::wstring get_html_body() const
		{ return m_bag.read_prop<std::wstring>(0x1013); }
		//! \brief Get the HTML body of this message
		//! 
		//! The returned stream device can be used to construct a proper stream: 
		//! \code
		//! message m = ...;
		//! prop_stream htmlbody(a.open_html_body_stream());
		//! \endcode
		//! Which can then be used as any iostream would be.
		//! \returns The message body as a stream
		hnid_stream_device open_html_body_stream()
		{ return m_bag.open_prop_stream(0x1013); }
		//! \brief Size of the HTML body, in bytes
		//! \returns The size of the HTML body
		size_t html_body_size() const
		{ return m_bag.size(0x1013); }
		//! \brief Checks to see if this message has a HTML body
		//! \returns true if the HTML body property exists
		bool has_html_body() const
		{ return m_bag.prop_exists(0x1013); }
		// \brief Get the total size of this message
		//! \returns The message size
		size_t size() const
		{ return m_bag.read_prop<slong>(0xe08); }
		//! \brief Get the number of attachments on this message
		//! \returns The number of attachments
		size_t get_attachment_count() const;
		//! \brief Get the number of recipients on this message
		//! \returns The number of recipients
		size_t get_recipient_count() const;

		// lower layer access
		//! \brief Get the property bag backing this message
		//! \returns The property bag
		property_bag& get_property_bag()
		{ return m_bag; }
		//! \copydoc message::get_property_bag()
		const property_bag& get_property_bag() const
		{ return m_bag; }
		//! \brief Get the attachment table of this message
		//! \returns The attachment table
		table& get_attachment_table();
		//! \brief Get the recipient table of this message
		//! \returns The recipient table
		table& get_recipient_table();
		//! \copydoc message::get_attachment_table()
		const table& get_attachment_table() const;
		//! \copydoc message::get_recipient_table()
		const table& get_recipient_table() const;

		//! \brief Get the node_id of this message
		//! \returns The node_id of the message
		node_id get_id() const
		{ return m_bag.get_node().get_id(); }

		//! \cond write_api
		//! \brief Set the subject to this message
		//! \param[in] val The subject line
		void set_subject(std::wstring val);
		
		//! \brief Set the body of this message
		//! \param[in] val The message body contents
		void set_body(std::wstring val);
		
		//! \brief Set the body in HTML format of this message
		//! \param[in] val The body contents in HTML format
		void set_html_body(std::wstring val);
		
		//! \brief Set the size of this message 
		//! \param[in] val The size value
		void set_message_size(slong val);
		
		//! \brief Save this message
		void save_message();
		
		//! \brief Add a new attachment to this message
		//! \param[in] val The file name of the attachment
		//! \param[in] val The attachment data as blob
		//! \param[in] val The size of the attachment (data + metadata)
		//! \param[in] val The attachment method
		void add_attachment(std::wstring file_name, std::vector<pstsdk::byte> data, ulong size, ulong attachment_method);
		
		//! \brief Set the account name of this recipient
		//! \param[in] val The recipient name
		//! \param[in] val The recipient type
		//! \param[in] val The recipient email address
		//! \param[in] val The recipient email address type
		void add_recipient(std::wstring name, recipient_type type, std::wstring address, std::wstring address_type);
		
		//! \brief Delete a recipient of this message
		//! \param[in] val The recipient to be deleted
		void delete_recipient(pstsdk::recipient& recpnt);
		
		//! \brief Delete an attachment of this message
		//! \param[in] val The attachment to be deleted
		void delete_attachment(pstsdk::attachment& atchmnt);
		//! \endcond

	private:
		message& operator=(const message&); // = delete

		property_bag m_bag;
		mutable std::tr1::shared_ptr<table> m_attachment_table;
		mutable std::tr1::shared_ptr<table> m_recipient_table;
	};

	class message_transform_row : public std::unary_function<const_table_row, message>
	{
	public:
		message_transform_row(const shared_db_ptr& db) 
			: m_db(db) { }
		message operator()(const const_table_row& row) const
		{ return message(m_db->lookup_node(row.get_row_id())); }

	private:
		shared_db_ptr m_db;
	};

	class message_transform_info : public std::unary_function<node_info, message>
	{
	public:
		message_transform_info(const shared_db_ptr& db) 
			: m_db(db) { }
		message operator()(const node_info& info) const
		{ return message(node(m_db, info)); }

	private:
		shared_db_ptr m_db;
	};

} // end namespace pstsdk

inline std::wstring pstsdk::attachment::get_filename() const
{
	try
	{
		return m_bag.read_prop<std::wstring>(0x3707);
	} 
	catch(key_not_found<prop_id>&)
	{
		return m_bag.read_prop<std::wstring>(0x3704);
	}
}

inline pstsdk::message pstsdk::attachment::open_as_message() const
{
	if(!is_message()) 
		throw std::bad_cast();

	std::vector<byte> buffer = get_bytes();
	disk::sub_object* psubo = (disk::sub_object*)&buffer[0];

	return message(m_bag.get_node().lookup(psubo->nid));
}

inline pstsdk::message::message(const pstsdk::message& other)
	: m_bag(other.m_bag)
{
	if(other.m_attachment_table)
		m_attachment_table.reset(new table(*other.m_attachment_table));
	if(other.m_recipient_table)
		m_recipient_table.reset(new table(*other.m_recipient_table));
}

inline const pstsdk::table& pstsdk::message::get_attachment_table() const
{
	if(!m_attachment_table)
		m_attachment_table.reset(new table(m_bag.get_node().lookup(nid_attachment_table)));

	return *m_attachment_table;
}

inline const pstsdk::table& pstsdk::message::get_recipient_table() const
{
	if(!m_recipient_table)
		m_recipient_table.reset(new table(m_bag.get_node().lookup(nid_recipient_table)));

	return *m_recipient_table;
}

inline pstsdk::table& pstsdk::message::get_attachment_table()
{
	return const_cast<table&>(const_cast<const message*>(this)->get_attachment_table());
}

inline pstsdk::table& pstsdk::message::get_recipient_table()
{
	return const_cast<table&>(const_cast<const message*>(this)->get_recipient_table());
}

inline size_t pstsdk::message::get_attachment_count() const
{
	size_t count = 0;
	try 
	{
		count = get_attachment_table().size();
	} 
	catch (const key_not_found<node_id>&) { }

	return count;
}

inline size_t pstsdk::message::get_recipient_count() const
{
	size_t count = 0;
	try
	{
		count = get_recipient_table().size();
	}
	catch (const key_not_found<node_id>&) { }

	return count;
}

inline std::wstring pstsdk::message::get_subject() const
{
	std::wstring buffer = m_bag.read_prop<std::wstring>(0x37);

	if(buffer.size() && buffer[0] == message_subject_prefix_lead_byte)
	{
		// Skip the second chracter as well
		return buffer.substr(2);
	}
	else
	{
		return buffer;
	}
}

//! \cond write_api
inline void pstsdk::attachment::save_attachment(pstsdk::message& container_msg)
{ 
	container_msg.get_property_bag().get_node().save_subnode(m_bag.get_node());
	container_msg.save_message();
}

inline void pstsdk::recipient::save_recipient(pstsdk::message& container_msg)
{ 
	container_msg.get_property_bag().get_node().save_subnode(m_row.get_table()->get_node());
	container_msg.save_message();
}

inline void pstsdk::message::set_subject(std::wstring val)
{
	m_bag.write_prop<std::wstring>(0x37, val);

	node_id parent_id = m_bag.get_node().get_parent_id();
	table ct(m_bag.get_node().get_db()->lookup_node(make_nid(nid_type_contents_table, get_nid_index(parent_id))));
	ulong row = ct.lookup_row(get_id());
	ct.write_cell(row, 0x37, wstring_to_bytes(val));
	ct.save_table();
}

inline void pstsdk::message::set_body(std::wstring val)
{
	m_bag.write_prop<std::wstring>(0x1000, val);

	node_id parent_id = m_bag.get_node().get_parent_id();
	table ct(m_bag.get_node().get_db()->lookup_node(make_nid(nid_type_contents_table, get_nid_index(parent_id))));
	ulong row = ct.lookup_row(get_id());

	if(ct.prop_exists(row, 0x1000))
	{
		ct.write_cell(row, 0x1000, wstring_to_bytes(val));
		ct.save_table();
	}
}

inline void pstsdk::message::set_html_body(std::wstring val)
{
	m_bag.write_prop<std::wstring>(0x1013, val);

	node_id parent_id = m_bag.get_node().get_parent_id();
	table ct(m_bag.get_node().get_db()->lookup_node(make_nid(nid_type_contents_table, get_nid_index(parent_id))));
	ulong row = ct.lookup_row(get_id());

	if(ct.prop_exists(row, 0x1013))
	{
		ct.write_cell(row, 0x1013, wstring_to_bytes(val));
		ct.save_table();
	}
}

inline void pstsdk::message::set_message_size(pstsdk::slong val)
{
	m_bag.write_prop<slong>(0x0E08, val);

	node_id parent_id = m_bag.get_node().get_parent_id();
	table ct(m_bag.get_node().get_db()->lookup_node(make_nid(nid_type_contents_table, get_nid_index(parent_id))));
	ulong row = ct.lookup_row(get_id());
	ct.set_cell_value(row, 0x0E08, val);
	ct.save_table();
}

inline void pstsdk::message::save_message()
{
	m_bag.save_property_bag();
}

inline void pstsdk::attachment::set_size(size_t val, pstsdk::message& container_msg)
{
	m_bag.write_prop<slong>(0x0E20, val);
	container_msg.get_attachment_table().set_cell_value(container_msg.get_attachment_table().lookup_row(m_bag.get_node().get_id()), 0x0E20, val);
	container_msg.get_property_bag().get_node().save_subnode(container_msg.get_attachment_table().get_node());
}

inline void pstsdk::attachment::set_bytes(std::vector<byte> val, pstsdk::message& container_msg)
{
	m_bag.write_prop<std::vector<byte>>(0x3701, val);
}

inline void pstsdk::attachment::set_attachment_method(pstsdk::ulong val, pstsdk::message& container_msg)
{
	m_bag.write_prop<slong>(0x3705, val);
	container_msg.get_attachment_table().set_cell_value(container_msg.get_attachment_table().lookup_row(m_bag.get_node().get_id()), 0x3705, val);
	container_msg.get_property_bag().get_node().save_subnode(container_msg.get_attachment_table().get_node());
}

inline void pstsdk::attachment::set_filename(std::wstring val, pstsdk::message& container_msg)
{
	m_bag.write_prop<std::wstring>(0x3707, val);
	container_msg.get_attachment_table().write_cell(container_msg.get_attachment_table().lookup_row(m_bag.get_node().get_id()), 0x3704, wstring_to_bytes(val));
	container_msg.get_property_bag().get_node().save_subnode(container_msg.get_attachment_table().get_node());
}

inline void pstsdk::message::add_attachment(std::wstring file_name, std::vector<pstsdk::byte> data, ulong size, ulong attachment_method)
{
	node *pnode = &m_bag.get_node();

	if(get_attachment_count() == 0)
	{
		node atc_sbnd = pnode->create_subnode(nid_attachment_table);
		atc_sbnd = pnode->get_db()->lookup_node(nid_attachment_table);
		m_attachment_table.reset(new table(atc_sbnd));
	}

	size_t nid_index = 0;

	for(node::subnode_iterator sbnd_iter = pnode->subnode_begin(); sbnd_iter != pnode->subnode_end(); ++sbnd_iter)
		nid_index = get_nid_index(sbnd_iter->get_id()) > nid_index ? get_nid_index(sbnd_iter->get_id()) : nid_index;

	property_bag bag(pnode->create_subnode(make_nid(nid_type_attachment, ++nid_index)));

	node data_sbnd = bag.get_node().create_subnode(make_nid(nid_type_ltp, ++nid_index));
	data_sbnd.resize(data.size());
	data_sbnd.write(data, 0);

	bag.write_prop<slong>(0x0E20, size);
	bag.write_prop<std::wstring>(0x3707, file_name);
	bag.write_prop<slong>(0x3705, attachment_method);
	bag.write_prop<ulong>(0x3701, data_sbnd.get_id());

	ulong row = get_attachment_table().add_row(bag.get_node().get_id());

	m_attachment_table->write_cell(row, 0x3704, wstring_to_bytes(file_name));
	m_attachment_table->set_cell_value(row, 0x0E20, size);

	bag.get_node().save_subnode(data_sbnd);
	m_bag.get_node().save_subnode(bag.get_node());
	m_bag.get_node().save_subnode(m_attachment_table->get_node());
}

inline void pstsdk::message::add_recipient(std::wstring name, recipient_type type, std::wstring address, std::wstring address_type)
{
	row_id new_id = get_recipient_table().size() + 1;

	for(size_t ind = 0; ind < m_recipient_table->size(); ++ind)
	{
		row_id id = m_recipient_table->get_row_id(ind);
		new_id = id > new_id ? id + 1 : new_id;
	}

	ulong row = get_recipient_table().add_row(new_id);

	m_recipient_table->write_cell(row, 0x3001, wstring_to_bytes(name));
	m_recipient_table->set_cell_value(row, 0x0C15, type);

	if(!m_recipient_table->column_exists(0x39FE))
	{
		m_recipient_table->add_column(0x39FE, prop_type_wstring);
	}

	m_recipient_table->write_cell(row, 0x39FE, wstring_to_bytes(address));
	m_recipient_table->write_cell(row, 0x3003, wstring_to_bytes(address));
	m_recipient_table->write_cell(row, 0x3002, wstring_to_bytes(address_type));

	m_bag.get_node().save_subnode(m_recipient_table->get_node());
}

inline void pstsdk::message::delete_recipient(pstsdk::recipient& recpnt)
{
	get_recipient_table().delete_row(recpnt.get_property_row().get_row_pos());
	m_bag.get_node().save_subnode(m_recipient_table->get_node());
}

inline void pstsdk::message::delete_attachment(pstsdk::attachment& atchmnt)
{
	node_id atch_pc_nid = atchmnt.get_property_bag().get_node().get_id();

	get_attachment_table().delete_row(get_attachment_table().lookup_row(atch_pc_nid));

	if(m_attachment_table->size() == 0)
		m_bag.get_node().delete_subnode(nid_attachment_table);
	else
		m_bag.get_node().save_subnode(m_attachment_table->get_node());

	m_bag.get_node().delete_subnode(atch_pc_nid);
}
//! \endcond

#endif
