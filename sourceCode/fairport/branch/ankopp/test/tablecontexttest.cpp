#include <iostream>
#include <pstsdk/ndb.h>
#include <pstsdk/disk.h>
#include <pstsdk/ltp.h>
#include <pstsdk/util.h>
#include "testutils.h"


void row_prop_iterate(pstsdk::const_table_row row)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	std::vector<pstsdk::prop_id> proplist(row.get_prop_list());

	// look for all props
	for(pstsdk::uint i = 0; i < proplist.size(); ++i)
	{
		cout<< "Property Id: " << hex << proplist[i]<< "\t";

		switch(row.get_prop_type(proplist[i]))
		{
		case prop_type_unspecified:
			cout << "prop_type_unspecified" << endl;
			break;

		case prop_type_null:
			cout << "prop_type_null" << endl;
			break;

		case prop_type_short:
			cout << "prop_type_short" << "\t" <<row.read_prop<short>(proplist[i]) << endl;
			break;

		case prop_type_mv_short:
			{
				vector<short> shorts = row.read_prop_array<short>(proplist[i]);
				cout << "prop_type_mv_short" << endl;
				for(size_t i = 0; i < shorts.size(); ++i)
					wcout << shorts[i] << endl;
			}
			break;

		case prop_type_long:
			cout << "prop_type_long" << "\t" <<row.read_prop<long>(proplist[i]) << endl;
			break;

		case prop_type_mv_long:
			{
				vector<long> longs = row.read_prop_array<long>(proplist[i]);
				cout << "prop_type_mv_long" << endl;
				for(size_t i = 0; i < longs.size(); ++i)
					wcout << longs[i] << endl;
			}
			break;

		case prop_type_float:
			cout << "prop_type_float" << "\t" <<row.read_prop<float>(proplist[i]) << endl;
			break;

		case prop_type_mv_float:
			{
				vector<float> floats = row.read_prop_array<float>(proplist[i]);
				cout << "prop_type_mv_float" << endl;
				for(size_t i = 0; i < floats.size(); ++i)
					wcout << floats[i] << endl;
			}
			break;

		case prop_type_double:
			cout << "prop_type_double" << "\t" <<row.read_prop<double>(proplist[i]) << endl;
			break;

		case prop_type_mv_double:
			{
				vector<double> doubles = row.read_prop_array<double>(proplist[i]);
				cout << "prop_type_mv_double" << endl;
				for(size_t i = 0; i < doubles.size(); ++i)
					wcout << doubles[i] << endl;
			}
			break;

		case prop_type_longlong:
			cout << "prop_type_longlong" << "\t" <<row.read_prop<ulonglong>(proplist[i]) << endl;
			break;

		case prop_type_mv_longlong:
			{
				vector<ulonglong> doubles = row.read_prop_array<ulonglong>(proplist[i]);
				cout << "prop_type_mv_longlong" << endl;
				for(size_t i = 0; i < doubles.size(); ++i)
					wcout << doubles[i] << endl;
			}
			break;

		case prop_type_boolean:
			cout << "prop_type_boolean" << "\t" <<row.read_prop<bool>(proplist[i]) << endl;
			break;

		case prop_type_string:
			cout << "prop_type_string" << "\t" <<row.read_prop<std::string>(proplist[i]) << endl;
			break;

		case prop_type_mv_wstring:
			{
				vector<wstring> wstrings = row.read_prop_array<wstring>(proplist[i]);
				cout << "prop_type_mv_wstring" << endl;
				for(size_t i = 0; i < wstrings.size(); ++i)
					wcout << string(wstrings[i].begin(), wstrings[i].begin()).c_str() << endl;
			}
			break;

		case prop_type_wstring:
			{
				wstring wstring_val = row.read_prop<std::wstring>(proplist[i]);
				cout << "prop_type_wstring" << "\t" << string(wstring_val.begin(), wstring_val.end()).c_str() << endl;
			}
			break;

		case prop_type_mv_string:
			{
				vector<string> strings = row.read_prop_array<string>(proplist[i]);
				cout << "prop_type_mv_string" << endl;
				for(size_t i = 0; i < strings.size(); ++i)
					cout << strings[i] << endl;
			}
			break;

		case prop_type_binary:
			cout << "prop_type_binary" << "\t" <<row.read_prop<pstsdk::byte>(proplist[i]) << endl;
			break;

		case prop_type_mv_binary:
			{
				vector<vector<pstsdk::byte>> bins = row.read_prop_array<vector<pstsdk::byte>>(proplist[i]);

				for(size_t i = 0; i < bins.size(); ++i)
				{
					cout << "MV # "<< i+1 << endl;

					for(size_t j = 0; j < bins[i].size(); ++j)
					{
						cout << bins[i][j] << endl;
					}
				}
				cout << "prop_type_mv_binary" << endl;
			}
			break;

		default:
			break;
		}
	}

	cout<<endl;
}

void test_tc_read(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x12D;

	shared_db_ptr db = open_database(filename);

	node nd = db->lookup_node(nid1);

	table tab(nd);

	size_t tab_size = tab.size();
	cout<< "Table size: " << tab_size << endl<< endl;

	for(const_table_row_iter iter = tab.begin(); iter != tab.end(); ++iter)
	{
		cout<< "Row Id: " << (*iter).get_row_id()<<endl;
		row_prop_iterate(*iter);
	}

	cout<< "*******************************************************************************" << endl;
}

void test_tc_add_row_unicode(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x12D;
	node_id nid2 =  0x12E;
	ulong new_row1 = 0;
	ulong new_row2 = 0;
	row_id row_id1 = 0;
	row_id row_id2 = 0;

	// add row and write new values
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		row_id1 = db->alloc_nid(nid_type_folder);
		table tab(nd);
		new_row1 = tab.add_row(row_id1);

		tab.set_cell_value(new_row1, 0x3603, 0x156);
		tab.write_cell(new_row1, 0x3001, wstring_to_bytes(L"test_string"));
		tab.set_cell_value(new_row1, 0x360A, true);
		tab.set_cell_value(new_row1, 0x3602, 0x45);
		tab.set_cell_value(new_row1, 0x6635, 0x675);
		tab.set_cell_value(new_row1, 0x6636, 0x0);
		tab.set_cell_value(new_row1, 0x67F3, 0x1);

		tab.write_cell(0, 0x3001, wstring_to_bytes(L"some_string"));
		tab.write_cell(1, 0x3001, wstring_to_bytes(L"another_string"));
		tab.write_cell(2, 0x3001, wstring_to_bytes(L"some_other_string"));

		tab.lookup_row(row_id1);

		tab.save_table();
		db->commit_db();
	}

	// verify row added and new values stored
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);
		assert(new_row1 + 1 == tab.size());

		tab.lookup_row(row_id1);

		assert(tab.get_cell_value(new_row1, 0x3603) == 0x156);
		assert(tab.get_cell_value(new_row1, 0x360A) == 0x1);
		assert(tab.get_cell_value(new_row1, 0x3602) == 0x45);
		assert(tab.get_cell_value(new_row1, 0x6635) == 0x675);
		assert(tab.get_cell_value(new_row1, 0x6636) == 0x0);
		assert(tab.get_cell_value(new_row1, 0x67F3) == 0x1);

		vector<pstsdk::byte> buff1 = wstring_to_bytes(L"some_string");
		assert(memcmp(&(tab.read_cell(0, 0x3001)[0]), &buff1[0], buff1.size()) == 0);

		vector<pstsdk::byte> buff2 = wstring_to_bytes(L"another_string");
		assert(memcmp(&(tab.read_cell(1, 0x3001)[0]), &buff2[0], buff1.size()) == 0);

		vector<pstsdk::byte> buff3 = wstring_to_bytes(L"some_other_string");
		assert(memcmp(&(tab.read_cell(2, 0x3001)[0]), &buff3[0], buff1.size()) == 0);
	}

	// add row and write new values
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid2);

		row_id2 = db->alloc_nid(nid_type_folder);
		table tab(nd);
		new_row2 = tab.add_row(row_id2);

		tab.set_cell_value(new_row2, 0x0017, 1);
		tab.write_cell(new_row2, 0x0037, wstring_to_bytes(L"test_string"));
		tab.set_cell_value(new_row2, 0x0057, true);

		tab.lookup_row(row_id2);

		tab.save_table();
		db->commit_db();
	}

	// verify row added and new values stored
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid2);

		table tab(nd);
		assert(new_row2 + 1 == tab.size());

		tab.lookup_row(row_id2);

		assert(tab.get_cell_value(new_row2, 0x0017) == 1);
		assert(tab.get_cell_value(new_row2, 0x0057) == 0x1);

		vector<pstsdk::byte> buff1 = wstring_to_bytes(L"test_string");
		assert(memcmp(&(tab.read_cell(new_row2, 0x0037)[0]), &buff1[0], buff1.size()) == 0);
	}

	// check intigrity
	test_tc_read(filename);
}

void test_tc_add_column_unicode(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x12D;

	// add new columns and set new values
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);
		tab.add_column(0x2000, prop_type_long);

		tab.set_cell_value(0, 0x2000, 0x156);
		tab.set_cell_value(1, 0x2000, 0x8684);
		tab.set_cell_value(2, 0x2000, 0x45);

		tab.add_column(0x2001, prop_type_short);

		tab.set_cell_value(0, 0x2001, 0x154);
		tab.set_cell_value(1, 0x2001, 0x864);
		tab.set_cell_value(2, 0x2001, 0x674);

		tab.add_column(0x2002, prop_type_longlong);

		tab.set_cell_value(0, 0x2002, 0x146556);
		tab.set_cell_value(1, 0x2002, 0x845684);
		tab.set_cell_value(2, 0x2002, 0x90845);

		tab.add_column(0x2003, prop_type_wstring);

		tab.write_cell(0, 0x2003, wstring_to_bytes(L"new_column_some_string"));
		tab.write_cell(1, 0x2003, wstring_to_bytes(L"new_column_another_string"));
		tab.write_cell(2, 0x2003, wstring_to_bytes(L"new_column_some_other_string"));

		tab.add_column(0x2004, prop_type_boolean);

		tab.set_cell_value(0, 0x2004, true);
		tab.set_cell_value(1, 0x2004, false);
		tab.set_cell_value(2, 0x2004, true);

		tab.save_table();
		db->commit_db();
	}

	// verify columns added and values saved
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);

		assert(tab.get_cell_value(0, 0x2000) == 0x156);
		assert(tab.get_cell_value(1, 0x2000) == 0x8684);
		assert(tab.get_cell_value(2, 0x2000) == 0x45);

		assert(tab.get_cell_value(0, 0x2001) == 0x154);
		assert(tab.get_cell_value(1, 0x2001) == 0x864);
		assert(tab.get_cell_value(2, 0x2001) == 0x674);

		assert(tab.get_cell_value(0, 0x2002) == 0x146556);
		assert(tab.get_cell_value(1, 0x2002) == 0x845684);
		assert(tab.get_cell_value(2, 0x2002) == 0x90845);

		vector<pstsdk::byte> buff1 = wstring_to_bytes(L"new_column_some_string");
		assert(memcmp(&(tab.read_cell(0, 0x2003)[0]), &buff1[0], buff1.size()) == 0);

		vector<pstsdk::byte> buff2 = wstring_to_bytes(L"new_column_another_string");
		assert(memcmp(&(tab.read_cell(1, 0x2003)[0]), &buff2[0], buff2.size()) == 0);

		vector<pstsdk::byte> buff3 = wstring_to_bytes(L"new_column_some_other_string");
		assert(memcmp(&(tab.read_cell(2, 0x2003)[0]), &buff3[0], buff2.size()) == 0);

		assert(tab.get_cell_value(0, 0x2004) == 0x1);
		assert(tab.get_cell_value(1, 0x2004) == 0x0);
		assert(tab.get_cell_value(2, 0x2004) == 0x1);
	}

	// check intigrity
	test_tc_read(filename);
}

void test_tc_modify_unicode(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x12D;

	// modify existing cell values 1
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);

		tab.set_cell_value(0, 0x3603, 0x156);
		tab.set_cell_value(0, 0x67F3, 0x8684);
		tab.write_cell(0, 0x3001, wstring_to_bytes(L"test_string"));
		tab.set_cell_value(1, 0x360A, true);
		tab.set_cell_value(1, 0x3602, 0x45);
		tab.set_cell_value(1, 0x6635, 0x675);
		tab.set_cell_value(2, 0x6636, 0x0);
		tab.set_cell_value(2, 0x67F3, 0x1);

		tab.write_cell(0, 0x3001, wstring_to_bytes(L"some_string"));
		tab.write_cell(1, 0x3001, wstring_to_bytes(L"another_string"));
		tab.write_cell(2, 0x3001, wstring_to_bytes(L"some_other_string"));

		tab.save_table();
		db->commit_db();
	}

	// verify values modified
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);

		assert(tab.get_cell_value(0, 0x3603) == 0x156);
		assert(tab.get_cell_value(0, 0x67F3) ==  0x8684);
		assert(tab.get_cell_value(1, 0x360A) == 1);
		assert(tab.get_cell_value(1, 0x3602) == 0x45);
		assert(tab.get_cell_value(1, 0x6635) == 0x675);
		assert(tab.get_cell_value(2, 0x6636) == 0x0);
		assert(tab.get_cell_value(2, 0x67F3) == 0x1);

		vector<pstsdk::byte> buff1 = wstring_to_bytes(L"some_string");
		assert(memcmp(&(tab.read_cell(0, 0x3001)[0]), &buff1[0], buff1.size()) == 0);

		vector<pstsdk::byte> buff2 = wstring_to_bytes(L"another_string");
		assert(memcmp(&(tab.read_cell(1, 0x3001)[0]), &buff2[0], buff2.size()) == 0);

		vector<pstsdk::byte> buff3 = wstring_to_bytes(L"some_other_string");
		assert(memcmp(&(tab.read_cell(2, 0x3001)[0]), &buff3[0], buff2.size()) == 0);
	}

	// modify existing cell values 2
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);

		vector<pstsdk::byte> buff1(5000, 1);
		vector<pstsdk::byte> buff2(4000, 2);
		vector<pstsdk::byte> buff3(6000, 3);

		tab.write_cell(0, 0x3001, buff1);
		tab.write_cell(1, 0x3001, buff2);
		tab.write_cell(2, 0x3001, buff3);

		assert(memcmp(&(tab.read_cell(2, 0x3001)[0]), &buff3[0], buff3.size()) == 0);

		tab.write_cell(2, 0x3001, buff1);

		tab.save_table();
		db->commit_db();
	}

	// verify values modified
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);

		vector<pstsdk::byte> buff1(5000, 1);
		assert(memcmp(&(tab.read_cell(0, 0x3001)[0]), &buff1[0], buff1.size()) == 0);

		vector<pstsdk::byte> buff2(4000, 2);
		assert(memcmp(&(tab.read_cell(1, 0x3001)[0]), &buff2[0], buff2.size()) == 0);

		assert(memcmp(&(tab.read_cell(2, 0x3001)[0]), &buff1[0], buff1.size()) == 0);
	}

	// check intigrity
	test_tc_read(filename);
}

void test_tc_add_row_ansi(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x12D;
	node_id nid2 =  0x12E;
	ulong new_row1 = 0;
	ulong new_row2 = 0;
	row_id row_id1 = 0;
	row_id row_id2 = 0;

	// add row and write new values
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		row_id1 = db->alloc_nid(nid_type_folder);
		table tab(nd);
		new_row1 = tab.add_row(row_id1);

		tab.set_cell_value(new_row1, 0x3603, 0x156);
		tab.write_cell(new_row1, 0x3001, wstring_to_bytes(L"test_string"));
		tab.set_cell_value(new_row1, 0x360A, true);
		tab.set_cell_value(new_row1, 0x3602, 0x45);
		tab.set_cell_value(new_row1, 0x6635, 0x675);
		tab.set_cell_value(new_row1, 0x6636, 0x0);
		tab.set_cell_value(new_row1, 0x67F3, 0x1);

		tab.write_cell(0, 0x3001, wstring_to_bytes(L"some_string"));
		tab.write_cell(1, 0x3001, wstring_to_bytes(L"another_string"));
		tab.write_cell(2, 0x3001, wstring_to_bytes(L"some_other_string"));

		tab.lookup_row(row_id1);

		tab.save_table();
		db->commit_db();
	}

	// verify row added and new values stored
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);
		assert(new_row1 + 1 == tab.size());

		tab.lookup_row(row_id1);

		assert(tab.get_cell_value(new_row1, 0x3603) == 0x156);
		assert(tab.get_cell_value(new_row1, 0x360A) == 0x1);
		assert(tab.get_cell_value(new_row1, 0x3602) == 0x45);
		assert(tab.get_cell_value(new_row1, 0x6635) == 0x675);
		assert(tab.get_cell_value(new_row1, 0x6636) == 0x0);
		assert(tab.get_cell_value(new_row1, 0x67F3) == 0x1);

		vector<pstsdk::byte> buff1 = test_utilities::string_to_bytes("some_string");
		assert(memcmp(&(tab.read_cell(0, 0x3001)[0]), &buff1[0], buff1.size()) == 0);

		vector<pstsdk::byte> buff2 = test_utilities::string_to_bytes("another_string");
		assert(memcmp(&(tab.read_cell(1, 0x3001)[0]), &buff2[0], buff1.size()) == 0);

		vector<pstsdk::byte> buff3 = test_utilities::string_to_bytes("some_other_string");
		assert(memcmp(&(tab.read_cell(2, 0x3001)[0]), &buff3[0], buff1.size()) == 0);
	}

	// add row and write new values
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid2);

		row_id2 = db->alloc_nid(nid_type_folder);
		table tab(nd);
		new_row2 = tab.add_row(row_id2);

		tab.set_cell_value(new_row2, 0x0017, 1);
		tab.write_cell(new_row2, 0x0037, wstring_to_bytes(L"test_string"));
		tab.set_cell_value(new_row2, 0x0057, true);

		tab.lookup_row(row_id2);

		tab.save_table();
		db->commit_db();
	}

	// verify row added and new values stored
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid2);

		table tab(nd);
		assert(new_row2 + 1 == tab.size());

		tab.lookup_row(row_id2);

		assert(tab.get_cell_value(new_row2, 0x0017) == 1);
		assert(tab.get_cell_value(new_row2, 0x0057) == 0x1);

		vector<pstsdk::byte> buff1 = test_utilities::string_to_bytes("test_string");
		assert(memcmp(&(tab.read_cell(new_row2, 0x0037)[0]), &buff1[0], buff1.size()) == 0);
	}

	// check intigrity
	test_tc_read(filename);
}

void test_tc_add_column_ansi(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x12D;

	// add new columns and set new values
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);
		tab.add_column(0x2000, prop_type_long);

		tab.set_cell_value(0, 0x2000, 0x156);
		tab.set_cell_value(1, 0x2000, 0x8684);
		tab.set_cell_value(2, 0x2000, 0x45);

		tab.add_column(0x2001, prop_type_short);

		tab.set_cell_value(0, 0x2001, 0x154);
		tab.set_cell_value(1, 0x2001, 0x864);
		tab.set_cell_value(2, 0x2001, 0x674);

		tab.add_column(0x2002, prop_type_longlong);

		tab.set_cell_value(0, 0x2002, 0x146556);
		tab.set_cell_value(1, 0x2002, 0x845684);
		tab.set_cell_value(2, 0x2002, 0x90845);

		tab.add_column(0x2003, prop_type_string);

		tab.write_cell(0, 0x2003, wstring_to_bytes(L"new_column_some_string"));
		tab.write_cell(1, 0x2003, wstring_to_bytes(L"new_column_another_string"));
		tab.write_cell(2, 0x2003, wstring_to_bytes(L"new_column_some_other_string"));

		tab.add_column(0x2004, prop_type_boolean);

		tab.set_cell_value(0, 0x2004, true);
		tab.set_cell_value(1, 0x2004, false);
		tab.set_cell_value(2, 0x2004, true);

		tab.save_table();
		db->commit_db();
	}

	// verify columns added and values saved
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);

		assert(tab.get_cell_value(0, 0x2000) == 0x156);
		assert(tab.get_cell_value(1, 0x2000) == 0x8684);
		assert(tab.get_cell_value(2, 0x2000) == 0x45);

		assert(tab.get_cell_value(0, 0x2001) == 0x154);
		assert(tab.get_cell_value(1, 0x2001) == 0x864);
		assert(tab.get_cell_value(2, 0x2001) == 0x674);

		assert(tab.get_cell_value(0, 0x2002) == 0x146556);
		assert(tab.get_cell_value(1, 0x2002) == 0x845684);
		assert(tab.get_cell_value(2, 0x2002) == 0x90845);

		vector<pstsdk::byte> buff1 = test_utilities::string_to_bytes("new_column_some_string");
		assert(memcmp(&(tab.read_cell(0, 0x2003)[0]), &buff1[0], buff1.size()) == 0);

		vector<pstsdk::byte> buff2 = test_utilities::string_to_bytes("new_column_another_string");
		assert(memcmp(&(tab.read_cell(1, 0x2003)[0]), &buff2[0], buff2.size()) == 0);

		vector<pstsdk::byte> buff3 = test_utilities::string_to_bytes("new_column_some_other_string");
		assert(memcmp(&(tab.read_cell(2, 0x2003)[0]), &buff3[0], buff2.size()) == 0);

		assert(tab.get_cell_value(0, 0x2004) == 0x1);
		assert(tab.get_cell_value(1, 0x2004) == 0x0);
		assert(tab.get_cell_value(2, 0x2004) == 0x1);
	}

	// check intigrity
	test_tc_read(filename);
}

void test_tc_modify_ansi(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x12D;

	// modify existing cell values 1
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);

		tab.set_cell_value(0, 0x3603, 0x156);
		tab.set_cell_value(0, 0x67F3, 0x8684);
		tab.write_cell(0, 0x3001, wstring_to_bytes(L"test_string"));
		tab.set_cell_value(1, 0x360A, true);
		tab.set_cell_value(1, 0x3602, 0x45);
		tab.set_cell_value(1, 0x6635, 0x675);
		tab.set_cell_value(2, 0x6636, 0x0);
		tab.set_cell_value(2, 0x67F3, 0x1);

		tab.write_cell(0, 0x3001, wstring_to_bytes(L"some_string"));
		tab.write_cell(1, 0x3001, wstring_to_bytes(L"another_string"));
		tab.write_cell(2, 0x3001, wstring_to_bytes(L"some_other_string"));

		tab.save_table();
		db->commit_db();
	}

	// verify values modified
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);

		assert(tab.get_cell_value(0, 0x3603) == 0x156);
		assert(tab.get_cell_value(0, 0x67F3) ==  0x8684);
		assert(tab.get_cell_value(1, 0x360A) == 1);
		assert(tab.get_cell_value(1, 0x3602) == 0x45);
		assert(tab.get_cell_value(1, 0x6635) == 0x675);
		assert(tab.get_cell_value(2, 0x6636) == 0x0);
		assert(tab.get_cell_value(2, 0x67F3) == 0x1);

		vector<pstsdk::byte> buff1 = test_utilities::string_to_bytes("some_string");
		assert(memcmp(&(tab.read_cell(0, 0x3001)[0]), &buff1[0], buff1.size()) == 0);

		vector<pstsdk::byte> buff2 = test_utilities::string_to_bytes("another_string");
		assert(memcmp(&(tab.read_cell(1, 0x3001)[0]), &buff2[0], buff2.size()) == 0);

		vector<pstsdk::byte> buff3 = test_utilities::string_to_bytes("some_other_string");
		assert(memcmp(&(tab.read_cell(2, 0x3001)[0]), &buff3[0], buff2.size()) == 0);
	}

	// modify existing cell values 2
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);

		vector<pstsdk::byte> buff1(10000, 1);
		vector<pstsdk::byte> buff2(8000, 2);
		vector<pstsdk::byte> buff3(12000, 3);

		tab.write_cell(0, 0x3001, buff1);
		tab.write_cell(1, 0x3001, buff2);
		tab.write_cell(2, 0x3001, buff3);
		tab.write_cell(2, 0x3001, buff1);

		tab.save_table();
		db->commit_db();
	}

	// verify values modified
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);

		vector<pstsdk::byte> buff1(5000, 1);
		assert(memcmp(&(tab.read_cell(0, 0x3001)[0]), &buff1[0], buff1.size()) == 0);

		vector<pstsdk::byte> buff2(4000, 2);
		assert(memcmp(&(tab.read_cell(1, 0x3001)[0]), &buff2[0], buff2.size()) == 0);

		vector<pstsdk::byte> buff11(2500, 1);
		assert(memcmp(&(tab.read_cell(2, 0x3001)[0]), &buff11[0], buff11.size()) == 0);
	}

	// check intigrity
	test_tc_read(filename);
}

void test_tc_remove_row(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x12D;
	node_id nid2 =  0x808E;
	ulong row_to_delete = 1;
	ulong rows = 0;
	vector<row_id> rowids;

	// delete row from table having > 1 rows
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);
		rows = tab.size();

		for(size_t i = 0; i < rows; ++i)
			rowids.push_back(tab.get_row_id(i));

		tab.delete_row(row_to_delete);

		tab.save_table();
		db->commit_db();
	}

	// verify row removed
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);
		assert(tab.size() == rows - 1);

		// verify given row only deleted
		for(size_t i = 0; i < rows - 1; ++i)
			assert(tab.get_row_id(i) != rowids[row_to_delete]);

		//verify other rows not affected
		for(size_t i = 0; i < rows - 1; ++i)
			assert(find(rowids.begin(), rowids.end(), tab.get_row_id(i)) != rowids.end());
	}

	row_to_delete = 0;

	// delete row from table having just 1 row
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid2);

		table tab(nd);
		rows = tab.size();

		for(size_t i = 0; i < rows; ++i)
			rowids.push_back(tab.get_row_id(i));

		tab.delete_row(row_to_delete);

		tab.save_table();
		db->commit_db();
	}

	// verify row removed
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid2);

		table tab(nd);
		assert(tab.size() == rows - 1);

		// verify given row only deleted
		for(size_t i = 0; i < rows - 1; ++i)
			assert(tab.get_row_id(i) != rowids[row_to_delete]);

		//verify other rows not affected
		for(size_t i = 0; i < rows - 1; ++i)
			assert(find(rowids.begin(), rowids.end(), tab.get_row_id(i)) != rowids.end());
	}

	// check intigrity
	test_tc_read(filename);
}

void test_tc_remove_column_val(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x12D;

	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);
		tab.delete_cell_value(0, 0x67f3);
		tab.delete_cell_value(2, 0x3001);
		tab.delete_cell_value(1, 0x360A);

		tab.save_table();
		db->commit_db();
	}

	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);

		table tab(nd);

		try
		{
			tab.get_cell_value(0, 0x67f3);
			assert(false);
		}
		catch(key_not_found<prop_id>) { }

		try
		{
			tab.get_cell_value(1, 0x360A);
			assert(false);
		}
		catch(key_not_found<prop_id>) { }

		try
		{
			tab.get_cell_value(2, 0x3001);
			assert(false);
		}
		catch(key_not_found<prop_id>) { }
	}

	// check intigrity
	test_tc_read(filename);
}

void test_tc_row_matrix_on_subnode_1(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x12D;

	// create row matrix on subnode (single page)
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);
		table tab(nd);

		for(size_t ind = 0; ind < 65; ++ind)
			tab.add_row(make_nid(nid_type_ltp, ind));

		tab.save_table();
		db->commit_db();
	}

	// test adding a row
	test_tc_add_row_unicode(filename);

	// test adding a new column
	test_tc_add_column_unicode(filename);

	// test deleting a row
	test_tc_remove_row(filename);

	// test modifying row values
	test_tc_modify_unicode(filename);

	// test deleting a column value
	test_tc_remove_column_val(filename);

	// check intigrity
	test_tc_read(filename);
}

void test_tc_row_matrix_on_subnode_2(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x12D;

	// create row matrix on subnode (multi page)
	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);
		table tab(nd);

		for(size_t ind = 0; ind < 320; ++ind)
			tab.add_row(make_nid(nid_type_ltp, ind));

		tab.save_table();
		db->commit_db();
	}

	// test adding a row
	test_tc_add_row_unicode(filename);

	// test adding a new column
	test_tc_add_column_unicode(filename);

	// test deleting a row
	test_tc_remove_row(filename);

	// test modifying row values
	test_tc_modify_unicode(filename);

	// test deleting a column value
	test_tc_remove_column_val(filename);

	// check intigrity
	test_tc_read(filename);
}

void test_tc()
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	std::wstring large_file = L"test_unicode.pst";
	std::wstring small_file = L"test_ansi.pst";

	std::wstring tmp_large_file = L"tmp_test_unicode.pst";
	std::wstring tmp_small_file = L"tmp_test_ansi.pst";

	if(test_utilities::copy_file(large_file, tmp_large_file) && test_utilities::copy_file(small_file, tmp_small_file))
	{
		// only valid to call on test_unicode.pst
		test_tc_read(tmp_large_file);

		test_tc_modify_unicode(tmp_large_file);

		test_tc_remove_row(tmp_large_file);

		test_tc_add_row_unicode(tmp_large_file);

		test_tc_add_column_unicode(tmp_large_file);

		test_tc_remove_column_val(tmp_large_file);

		test_utilities::copy_file(large_file, tmp_large_file);
		test_tc_row_matrix_on_subnode_1(tmp_large_file);

		test_utilities::copy_file(large_file, tmp_large_file);
		test_tc_row_matrix_on_subnode_2(tmp_large_file);


		// only valid to call on test_ansi.pst
		test_tc_read(tmp_small_file);

		test_tc_modify_ansi(tmp_small_file);

		test_tc_remove_row(tmp_small_file);

		test_tc_add_row_ansi(tmp_small_file);

		test_tc_add_column_ansi(tmp_small_file);

		test_tc_remove_column_val(tmp_small_file);


		test_utilities::delete_file(tmp_large_file);
		test_utilities::delete_file(tmp_small_file);
	}
	else
	{
		throw runtime_error("error creating temp files");
	}
}