#include <iostream>
#include <algorithm>
#include <pstsdk/ndb.h>
#include <pstsdk/disk.h>
#include <pstsdk/ltp.h>
#include <pstsdk/util.h>
#include "testutils.h"

#include "pstsdk/pst/message.h"
#include "pstsdk/pst/folder.h"
#include "pstsdk/pst/pst.h"


void read_messages(const pstsdk::message& m);

void read_recipients(const pstsdk::recipient& r)
{
	using namespace std;
	using namespace pstsdk;

	wcout << "\t\t" << r.get_name() << "(" << r.get_email_address() << ")\n";
	cout << "\t\t" << r.get_type() << "\n";
}

void read_attachments(const pstsdk::attachment& a)
{
	using namespace std;
	using namespace pstsdk;

	wcout << "\t\tFile Name: " << a.get_filename() << endl;
	wcout << "\t\tSize: " << a.size() << endl;

	if(a.is_message())
	{
		read_messages(a.open_as_message());
	}
	else
	{
		std::wstring wfilename = a.get_filename();
		std::string filename(wfilename.begin(), wfilename.end());
		ofstream newfile(filename.c_str(), ios::out | ios::binary);
		newfile << a;

		std::vector<pstsdk::byte> contents = a.get_bytes();
		assert(contents.size() == a.content_size());
	}
}

void read_messages(const pstsdk::message& m)
{
	using namespace std;
	using namespace pstsdk;

	wcout << "\tMessage Subject: " << m.get_subject() << endl;
	//wcout << "\tMessage Body: " << m.get_body() << endl;
	//wcout << "\tMessage HTML Body: " << m.get_html_body() << endl;
	wcout << "\tAttachment Count: " << m.get_attachment_count() << endl;

	if(m.get_attachment_count() > 0)
	{
		for_each(m.attachment_begin(), m.attachment_end(), read_attachments);
	}

	wcout << "\tRecipient Count: " << m.get_recipient_count() << endl;

	if(m.get_recipient_count() > 0)
	{
		for_each(m.recipient_begin(), m.recipient_end(), read_recipients);
	}
}

void read_folders(const pstsdk::folder& f)
{
	using namespace std;
	using namespace pstsdk;

	wcout << "Folder (M" << f.get_message_count() << ", F" << f.get_subfolder_count() << ") : " << f.get_name() << endl;

	for_each(f.message_begin(), f.message_end(), read_messages);

	for_each(f.sub_folder_begin(), f.sub_folder_end(), read_folders);
}

void test_pst_read(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	pst pstfile(filename);

	cout << "**************************************************************************" << endl;
	wcout << "PST Name: " << pstfile.get_name() << endl;

	for(pst::folder_iterator iter = pstfile.folder_begin(); iter != pstfile.folder_end(); ++iter)
		read_folders(*iter);

	cout << "**************************************************************************" << endl;
}

void test_pst_create_message_store(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	// drop message store if already exists
	{
		shared_db_ptr db = open_database(filename);

		try
		{
			db->delete_node(nid_message_store);
			db->commit_db();
		}
		catch(duplicate_key<node_id>) { }
	}

	{
		pst pstfile(filename);
		pstfile.create_empty_message_store();
		pstfile.set_name(L"MailBox");
		pstfile.save_pst();
	}

	// verify 1
	{
		shared_db_ptr db = open_database(filename);
		assert(db->node_exists(nid_message_store));
	}

	// verify 2
	{
		pst pstfile(filename);
		assert(pstfile.get_name().compare(L"MailBox") == 0);
	}

	// check intigrity
	test_pst_read(filename);
}

void test_pst_set_folder_prop(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid_folder = 0x8062;

	{
		pst pstfile(filename);
		folder fldr = pstfile.open_folder(nid_folder);

		// set folder properties
		fldr.set_message_count(10);
		fldr.set_name(L"Modified_Search_Root");
		fldr.set_unread_message_count(3);
		fldr.set_has_subfolders(true);

		//save folder
		fldr.save_folder();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify property changes
	{
		pst pstfile(filename);
		folder fldr = pstfile.open_folder(nid_folder);

		assert(fldr.get_message_count() == 10);
		assert(fldr.get_name().compare(L"Modified_Search_Root") == 0);
		assert(fldr.get_unread_message_count() == 3);

		folder parent = pstfile.open_folder(pstfile.get_db()->lookup_node(nid_folder).get_parent_id());
		table htc = parent.get_hierarchy_table();
		ulong row = htc.lookup_row(nid_folder);
		assert(htc.get_cell_value(row, 0x3602) == 10);
		assert(htc.get_cell_value(row, 0x3603) == 3);
	}

	// check intigrity
	test_pst_read(filename);
}

void test_pst_set_message_prop(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x200024;

	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid1);

		m.set_subject(L"New subject");
		m.set_body(L"Sample 1 folder: Message 1 : New Body");
		m.set_html_body(L"<body>Sample 1 folder: Message 1 : New Body<\body>");
		m.set_message_size(1024);

		assert(m.get_subject().compare(L"New subject") == 0);
		assert(m.get_body().compare(L"Sample 1 folder: Message 1 : New Body") == 0);
		assert(m.get_html_body().compare(L"<body>Sample 1 folder: Message 1 : New Body<\body>") == 0);
		assert(m.size() == 1024);

		//save message
		m.save_message();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify property changes
	{
		pst pstfile(filename);
		message m = pstfile.open_message(nid1);

		assert(m.get_subject().compare(L"New subject") == 0);
		assert(m.get_body().compare(L"Sample 1 folder: Message 1 : New Body") == 0);
		assert(m.get_html_body().compare(L"<body>Sample 1 folder: Message 1 : New Body<\body>") == 0);
		assert(m.size() == 1024);

		folder parent = pstfile.open_folder(pstfile.get_db()->lookup_node(nid1).get_parent_id());
		table ctc = parent.get_contents_table();
		ulong row = ctc.lookup_row(nid1);
		assert(ctc.get_cell_value(row, 0x0E08) == 1024);
		vector<pstsdk::byte> vect1 = ctc.read_cell(row, 0x37);
	}

	// check intigrity
	test_pst_read(filename);
}

void test_pst_set_attachment_prop(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x200024;

	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid1);
		attachment a = (*(m.attachment_begin()));

		a.set_filename(L"new_attachment.txt", m);
		a.set_bytes(wstring_to_bytes(L"These are contents of new_attachment.txt"), m);
		a.set_attachment_method(3, m);
		a.set_size(120, m);

		// verify
		assert(a.get_filename().compare(L"new_attachment.txt") == 0);
		vector<pstsdk::byte> vect = a.get_bytes();
		assert(memcmp(&(a.get_bytes()[0]), &(wstring_to_bytes(L"These are contents of new_attachment.txt")[0]), a.content_size()) == 0);
		assert(a.size() == 120);
		assert(!a.is_message());

		//save attachment
		a.save_attachment(m);

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify property changes
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid1);
		attachment a = (*(m.attachment_begin()));

		assert(a.get_filename().compare(L"new_attachment.txt") == 0);
		assert(memcmp(&(a.get_bytes()[0]), &(wstring_to_bytes(L"These are contents of new_attachment.txt")[0]), a.content_size()) == 0);
		assert(a.size() == 120);
		assert(!a.is_message());

		table act = m.get_attachment_table();
		ulong row = act.lookup_row(a.get_property_bag().get_node().get_id());
		assert(act.get_cell_value(row, 0x0E20) == 120);
		assert(act.get_cell_value(row, 0x3705) == 3);
		vector<pstsdk::byte> vect1 = act.read_cell(row, 0x3704);
	}

	// check intigrity
	test_pst_read(filename);
}

void test_pst_set_recipient_prop(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x200024;

	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid1);
		recipient r = (*(m.recipient_begin()));

		r.set_account_name(L"redmond\\username");
		r.set_address_type(L"POP");
		r.set_email_address(L"username@microsoft.com");
		r.set_name(L"User Name");
		r.set_type(mapi_bcc);

		assert(r.get_account_name().compare(L"redmond\\username") == 0);
		assert(r.get_address_type().compare(L"POP") == 0);
		assert(r.get_email_address().compare(L"username@microsoft.com") == 0);
		assert(r.get_name().compare(L"User Name") == 0);
		assert(r.get_type() == mapi_bcc);

		//save recipient
		r.save_recipient(m);

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify property changes
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid1);
		recipient r = (*(m.recipient_begin()));

		assert(r.get_account_name().compare(L"redmond\\username") == 0);
		assert(r.get_address_type().compare(L"POP") == 0);
		assert(r.get_email_address().compare(L"username@microsoft.com") == 0);
		assert(r.get_name().compare(L"User Name") == 0);
		assert(r.get_type() == mapi_bcc);
	}

	// check intigrity
	test_pst_read(filename);
}

void test_pst_folder_create_subfolder(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid_subfolder = 0x8062;
	node_id nid_folder = 0x0;

	// create sub folder to root folder
	{
		pst pstfile(filename);
		folder root_folder = pstfile.open_root_folder();
		folder new_folder = root_folder.create_subfolder(L"New_SubFolder1");

		nid_folder = new_folder.get_property_bag().get_node().get_id();

		assert(new_folder.get_message_count() == 0);
		assert(new_folder.get_subfolder_count() == 0);
		assert(new_folder.get_name().compare(L"New_SubFolder1") == 0);
		assert(new_folder.get_property_bag().get_node().get_parent_id() != 0);

		// save folders
		new_folder.save_folder();
		root_folder.save_folder();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify 1
	{
		pst pstfile(filename);
		folder new_folder = pstfile.open_folder(nid_folder);

		assert(new_folder.get_message_count() == 0);
		assert(new_folder.get_subfolder_count() == 0);
		assert(new_folder.get_name().compare(L"New_SubFolder1") == 0);
		assert(new_folder.get_property_bag().get_node().get_parent_id() != 0);

		folder root_folder = pstfile.open_root_folder();
		assert(root_folder.get_property_bag().read_prop<bool>(0x360A));
	}

	// create sub folder to another sub folder
	{
		pst pstfile(filename);
		folder fldr(pstfile.get_db(), pstfile.get_db()->lookup_node(nid_subfolder));
		folder new_folder = fldr.create_subfolder(L"New_SubFolder2");

		nid_folder = new_folder.get_property_bag().get_node().get_id();

		assert(new_folder.get_message_count() == 0);
		assert(new_folder.get_subfolder_count() == 0);
		assert(new_folder.get_name().compare(L"New_SubFolder2") == 0);
		assert(new_folder.get_property_bag().get_node().get_parent_id() != 0);

		// save folders
		new_folder.save_folder();
		fldr.save_folder();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify 2
	{
		pst pstfile(filename);
		folder new_folder = pstfile.open_folder(nid_folder);

		assert(new_folder.get_message_count() == 0);
		assert(new_folder.get_subfolder_count() == 0);
		assert(new_folder.get_name().compare(L"New_SubFolder2") == 0);
		assert(new_folder.get_property_bag().get_node().get_parent_id() != 0);

		folder fldr(pstfile.get_db(), pstfile.get_db()->lookup_node(nid_subfolder));
		assert(fldr.get_property_bag().read_prop<bool>(0x360A));
	}

	// check intigrity
	test_pst_read(filename);
}

void test_pst_folder_create_message(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid_folder1 = 0x8022;
	node_id nid_folder2 = 0x8082;

	node_id nid_message1 = 0x0;
	node_id nid_message2 = 0x0;

	// create message to folder which does not have any messages
	{
		{
			pst pstfile(filename);

			folder fldr = pstfile.open_folder(nid_folder1);

			message m = fldr.create_message(L"New_Message1");

			nid_message1 = m.get_property_bag().get_node().get_id();

			// save message
			m.save_message();
			fldr.save_folder();

			// commit changes to PST onto disk
			pstfile.save_pst();
		}

		{
			pst pstfile(filename);

			message m = pstfile.open_message(nid_message1);

			m.set_subject(L"New_Message1 subject");
			m.set_body(L"New_Message1 body");
			m.set_html_body(L"<body> New_Message1 html body <\body>");
			m.set_message_size(100);

			assert(m.get_subject().compare(L"New_Message1 subject") == 0);
			assert(m.get_body().compare(L"New_Message1 body") == 0);
			assert(m.get_html_body().compare(L"<body> New_Message1 html body <\body>") == 0);
			assert(m.size() == 100);

			// save message
			m.save_message();

			// commit changes to PST onto disk
			pstfile.save_pst();
		}
	}

	// verify 1
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid_message1);

		assert(m.get_subject().compare(L"New_Message1 subject") == 0);
		assert(m.get_body().compare(L"New_Message1 body") == 0);
		assert(m.get_html_body().compare(L"<body> New_Message1 html body <\body>") == 0);
		assert(m.size() == 100);

		folder parent = pstfile.open_folder(pstfile.get_db()->lookup_node(nid_message1).get_parent_id());
		table ctc = parent.get_contents_table();
		ulong row = ctc.lookup_row(nid_message1);
		assert(ctc.get_cell_value(row, 0x0E08) == 100);
		vector<pstsdk::byte> vect1 = ctc.read_cell(row, 0x37);
	}

	// create message to folder which already has message and also add recipient and attachment
	{
		{
			pst pstfile(filename);

			folder fldr = pstfile.open_folder(nid_folder2);

			message m = fldr.create_message(L"New_Message2");

			nid_message2 = m.get_property_bag().get_node().get_id();

			// save message
			m.save_message();
			fldr.save_folder();

			// commit changes to PST onto disk
			pstfile.save_pst();
		}

		{
			pst pstfile(filename);

			message m = pstfile.open_message(nid_message2);

			m.set_subject(L"New_Message2 subject");
			m.set_body(L"New_Message2 body");
			m.set_html_body(L"<body> New_Message2 html body <\body>");
			m.set_message_size(200);

			assert(m.get_subject().compare(L"New_Message2 subject") == 0);
			assert(m.get_body().compare(L"New_Message2 body") == 0);
			assert(m.get_html_body().compare(L"<body> New_Message2 html body <\body>") == 0);
			assert(m.size() == 200);

			m.add_recipient(L"New_Message2_User", mapi_to, L"New_Message2_User@microsoft.com", L"POP");
			m.add_attachment(L"New_Message2_Attachment.txt", wstring_to_bytes(L"New_Message2_Attachment contents."), 200, 0);

			// save message
			m.save_message();

			// commit changes to PST onto disk
			pstfile.save_pst();
		}
	}

	// verify 2
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid_message2);

		assert(m.get_subject().compare(L"New_Message2 subject") == 0);
		assert(m.get_body().compare(L"New_Message2 body") == 0);
		assert(m.get_html_body().compare(L"<body> New_Message2 html body <\body>") == 0);
		assert(m.size() == 200);

		folder parent = pstfile.open_folder(pstfile.get_db()->lookup_node(nid_message2).get_parent_id());
		table ctc = parent.get_contents_table();
		ulong row = ctc.lookup_row(nid_message2);
		assert(ctc.get_cell_value(row, 0x0E08) == 200);
		vector<pstsdk::byte> vect1 = ctc.read_cell(row, 0x37);
	}

	// check intigrity
	test_pst_read(filename);
}

void test_pst_message_add_recipient(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x200024;
	size_t rcpt_cnt = 0;

	// add new recipient
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid1);

		rcpt_cnt = m.get_recipient_count();

		m.add_recipient(L"New_Recipient", mapi_cc, L"new_recipient@microsoft.com", L"SMTP");

		//save message
		m.save_message();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify recipient added
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid1);

		assert(m.get_recipient_count() == rcpt_cnt + 1);

		for(message::recipient_iterator iter = m.recipient_begin(); iter != m.recipient_end(); ++iter)
		{
			if(iter->get_name().compare(L"New_Recipient") == 0)
			{
				assert(iter->get_address_type().compare(L"SMTP") == 0);
				assert(iter->get_email_address().compare(L"new_recipient@microsoft.com") == 0);
				assert(iter->get_name().compare(L"New_Recipient") == 0);
				assert(iter->get_type() == mapi_cc);
			}
		}
	}

	// check intigrity
	test_pst_read(filename);
}

void test_pst_message_add_attachment(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x200024;
	size_t atch_cnt = 0;

	// add new attachment
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid1);

		atch_cnt = m.get_attachment_count();

		m.add_attachment(L"added_attachment.txt", wstring_to_bytes(L"These are contents of added_attachment.txt"), 100, 0);

		//save attachment
		m.save_message();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify attachment added
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid1);

		assert(m.get_attachment_count() == atch_cnt + 1);

		for(message::attachment_iterator iter = m.attachment_begin(); iter != m.attachment_end(); ++iter)
		{
			if(iter->get_filename().compare(L"added_attachment.txt") == 0)
			{
				assert(iter->get_filename().compare(L"added_attachment.txt") == 0);
				assert(memcmp(&(iter->get_bytes()[0]), &(wstring_to_bytes(L"These are contents of added_attachment.txt")[0]), iter->content_size()) == 0);
				assert(iter->size() == 100);
				assert(!iter->is_message());
			}
		}
	}

	// check intigrity
	test_pst_read(filename);
}

void test_pst_message_delete_recipient(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x200024;
	node_id nid2 =  0x200064;
	size_t rcpt_cnt = 0;
	row_id rcpt_rw_id = 0;
	bool run_two = false;

	// delete recipient 1
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid1);

		rcpt_cnt = m.get_recipient_count();

		recipient r = (*(m.recipient_begin()));

		rcpt_rw_id = r.get_property_row().get_row_id();

		m.delete_recipient(r);

		//save message
		m.save_message();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify 1
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid1);

		assert(m.get_recipient_count() == rcpt_cnt - 1);

		try
		{
			m.get_recipient_table().lookup_row(rcpt_rw_id);
			assert(false);
		}
		catch(exception) { }

		run_two = pstfile.get_db()->node_exists(nid2);
	}

	// delete recipient 2
	if(run_two)
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid2);

		rcpt_cnt = m.get_recipient_count();

		recipient r = (*(m.recipient_begin()));

		rcpt_rw_id = r.get_property_row().get_row_id();

		m.delete_recipient(r);

		//save message
		m.save_message();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify 2
	if(run_two)
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid2);

		assert(m.get_recipient_count() == rcpt_cnt - 1);

		try
		{
			m.get_recipient_table().lookup_row(rcpt_rw_id);
			assert(false);
		}
		catch(exception) { }
	}

	// check intigrity
	test_pst_read(filename);
}

void test_pst_message_delete_attachment(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x200024;
	node_id nid2 =  0x200064;
	size_t atchmnt_cnt = 0;
	bool run_two = false;

	// delete attachment 1
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid1);

		atchmnt_cnt = m.get_attachment_count();

		attachment a = (*(m.attachment_begin()));

		m.delete_attachment(a);

		//save message
		m.save_message();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify 1
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid1);

		assert(m.get_attachment_count() == atchmnt_cnt - 1);

		run_two = pstfile.get_db()->node_exists(nid2);
	}

	// delete attachment 2
	if(run_two)
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid2);

		atchmnt_cnt = m.get_attachment_count();

		attachment a = (*(m.attachment_begin()));

		m.delete_attachment(a);

		//save message
		m.save_message();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify 2
	if(run_two)
	{
		pst pstfile(filename);

		message m = pstfile.open_message(nid2);

		assert(m.get_attachment_count() == atchmnt_cnt - 1);
	}

	// check intigrity
	test_pst_read(filename);
}

void test_pst_folder_delete_message(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid_folder = 0x8082;
	node_id nid_msg1 =  0x200024;
	node_id nid_msg2 =  0x200064;
	size_t msg_cnt = 0;
	bool run_two = false;

	// delete message 1
	{
		pst pstfile(filename);
		folder fldr = pstfile.open_folder(nid_folder);

		msg_cnt = fldr.get_message_count();

		fldr.delete_message(nid_msg1);

		//save folder
		fldr.save_folder();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify delete
	{
		pst pstfile(filename);
		folder fldr = pstfile.open_folder(nid_folder);

		assert(fldr.get_message_count() == msg_cnt - 1);

		try
		{
			fldr.get_contents_table().lookup_row(nid_msg1);
			assert(false);
		}
		catch(key_not_found<row_id>) { }

		run_two = pstfile.get_db()->node_exists(nid_msg2);
	}

	// delete message 2
	if(run_two)
	{
		pst pstfile(filename);
		folder fldr = pstfile.open_folder(nid_folder);

		msg_cnt = fldr.get_message_count();

		message m = pstfile.open_message(nid_msg2);

		fldr.delete_message(m);

		//save folder
		fldr.save_folder();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify delete
	if(run_two)
	{
		pst pstfile(filename);
		folder fldr = pstfile.open_folder(nid_folder);

		assert(fldr.get_message_count() == msg_cnt - 1);

		try
		{
			fldr.get_contents_table().lookup_row(nid_msg2);
			assert(false);
		}
		catch(key_not_found<row_id>) { }
	}

	// check intigrity
	test_pst_read(filename);
}

void test_pst_folder_delete_subfolder(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid_parent_folder = 0x8022;
	node_id nid_folder1 =  0x8062;
	node_id nid_folder2 =  0x8082;
	size_t sub_fldr_cnt = 0;

	// delete folder 1
	{
		pst pstfile(filename);
		folder fldr = pstfile.open_folder(nid_parent_folder);

		sub_fldr_cnt = fldr.get_subfolder_count();

		fldr.delete_subfolder(nid_folder1);

		//save folder
		fldr.save_folder();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify delete
	{
		pst pstfile(filename);
		folder fldr = pstfile.open_folder(nid_parent_folder);

		assert(fldr.get_subfolder_count() == sub_fldr_cnt - 1);

		try
		{
			fldr.get_hierarchy_table().lookup_row(nid_folder1);
			assert(false);
		}
		catch(key_not_found<row_id>) { }
	}

	// delete folder 2
	{
		pst pstfile(filename);
		folder fldr = pstfile.open_folder(nid_parent_folder);

		sub_fldr_cnt = fldr.get_subfolder_count();

		folder sub_fldr(pstfile.get_db(), pstfile.get_db()->lookup_node(nid_folder2));

		fldr.delete_subfolder(sub_fldr);

		//save folder
		fldr.save_folder();

		// commit changes to PST onto disk
		pstfile.save_pst();
	}

	// verify delete
	{
		pst pstfile(filename);
		folder fldr = pstfile.open_folder(nid_parent_folder);

		assert(fldr.get_subfolder_count() == sub_fldr_cnt - 1);

		try
		{
			fldr.get_hierarchy_table().lookup_row(nid_folder2);
			assert(false);
		}
		catch(key_not_found<row_id>) { }
	}

	// check intigrity
	test_pst_read(filename);
}

void test_pst()
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	std::wstring large_file = L"sample1.pst";
	std::wstring small_file = L"sample2.pst";

	std::wstring tmp_large_file = L"tmp_sample1.pst";
	std::wstring tmp_small_file = L"tmp_sample2.pst";


	if(test_utilities::copy_file(large_file, tmp_large_file) && test_utilities::copy_file(small_file, tmp_small_file))
	{
		// only valid to call on sample1.pst
		test_pst_read(tmp_large_file);

		test_pst_set_folder_prop(tmp_large_file);

		test_pst_set_message_prop(tmp_large_file);

		test_pst_set_attachment_prop(tmp_large_file);

		test_pst_set_recipient_prop(tmp_large_file);

		test_pst_folder_create_subfolder(tmp_large_file);

		test_pst_folder_create_message(tmp_large_file);

		test_pst_message_add_recipient(tmp_large_file);

		test_pst_message_add_attachment(tmp_large_file);

		test_pst_message_delete_recipient(tmp_large_file);

		test_pst_message_delete_attachment(tmp_large_file);

		test_pst_folder_delete_message(tmp_large_file);

		test_pst_folder_delete_subfolder(tmp_large_file);

		test_pst_create_message_store(tmp_large_file);


		// only valid to call on sample2.pst
		test_pst_read(tmp_small_file);

		test_pst_set_folder_prop(tmp_small_file);

		test_pst_set_message_prop(tmp_small_file);

		test_pst_set_attachment_prop(tmp_small_file);

		test_pst_set_recipient_prop(tmp_small_file);

		test_pst_folder_create_subfolder(tmp_small_file);

		test_pst_folder_create_message(tmp_small_file);

		test_pst_message_add_recipient(tmp_small_file);

		test_pst_message_add_attachment(tmp_small_file);

		test_pst_message_delete_recipient(tmp_small_file);

		test_pst_message_delete_attachment(tmp_small_file);

		test_pst_folder_delete_message(tmp_small_file);

		//test_pst_folder_delete_subfolder(tmp_small_file);

		test_pst_create_message_store(tmp_small_file);


		test_utilities::delete_file(tmp_large_file);
		test_utilities::delete_file(tmp_small_file);
	}
	else
	{
		throw runtime_error("error creating temp files");
	}
}