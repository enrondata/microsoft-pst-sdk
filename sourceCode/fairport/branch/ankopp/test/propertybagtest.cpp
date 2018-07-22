#include <iostream>
#include <pstsdk/ndb.h>
#include <pstsdk/disk.h>
#include <pstsdk/ltp.h>
#include <pstsdk/util.h>
#include "testutils.h"


void prop_iterate(pstsdk::shared_db_ptr db, pstsdk::node_id nid)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node nd = db->lookup_node(nid);
	property_bag bag(nd);
	std::vector<pstsdk::ushort> proplist(bag.get_prop_list());

	// look for all props
	for(pstsdk::uint i = 0; i < proplist.size(); ++i)
	{
		switch(bag.get_prop_type(proplist[i]))
		{
		case prop_type_unspecified:
			{
				cout << "prop_type_unspecified" << endl;
			}
			break;

		case prop_type_null:
			{
				cout << "prop_type_null" << endl;
			}
			break;

		case prop_type_short:
			{
				cout << "prop_type_short" << endl;
				cout <<bag.read_prop<short>(proplist[i]) << endl;
			}
			break;

		case prop_type_mv_short:
			{
				vector<short> shorts = bag.read_prop_array<short>(proplist[i]);
				cout << "prop_type_mv_short" << endl;
				for(size_t i = 0; i < shorts.size(); ++i)
					cout << shorts[i] << endl;
			}
			break;

		case prop_type_long:
			{
				cout << "prop_type_long" << endl;
				cout <<bag.read_prop<long>(proplist[i]) << endl;
			}
			break;

		case prop_type_mv_long:
			{
				vector<long> longs = bag.read_prop_array<long>(proplist[i]);
				cout << "prop_type_mv_long" << endl;
				for(size_t i = 0; i < longs.size(); ++i)
					cout << longs[i] << endl;
			}
			break;

		case prop_type_float:
			{
				cout << "prop_type_float" << endl;
				cout <<bag.read_prop<float>(proplist[i]) << endl;
			}
			break;

		case prop_type_mv_float:
			{
				vector<float> floats = bag.read_prop_array<float>(proplist[i]);
				cout << "prop_type_mv_float" << endl;
				for(size_t i = 0; i < floats.size(); ++i)
					cout << floats[i] << endl;
			}
			break;

		case prop_type_double:
			{
				cout << "prop_type_double" << endl;
				cout <<bag.read_prop<double>(proplist[i]) << endl;
			}
			break;

		case prop_type_mv_double:
			{
				vector<double> doubles = bag.read_prop_array<double>(proplist[i]);
				cout << "prop_type_mv_double" << endl;
				for(size_t i = 0; i < doubles.size(); ++i)
					cout << doubles[i] << endl;
			}
			break;

		case prop_type_longlong:
			{
				cout << "prop_type_longlong" << endl;
				cout <<bag.read_prop<ulonglong>(proplist[i]) << endl;
			}
			break;

		case prop_type_mv_longlong:
			{
				vector<ulonglong> doubles = bag.read_prop_array<ulonglong>(proplist[i]);
				cout << "prop_type_mv_longlong" << endl;
				for(size_t i = 0; i < doubles.size(); ++i)
					cout << doubles[i] << endl;
			}
			break;

		case prop_type_boolean:
			{
				cout << "prop_type_boolean" << endl;
				cout <<bag.read_prop<bool>(proplist[i]) << endl;
			}
			break;

		case prop_type_string:
			{
				cout << "prop_type_string" << endl;
				cout <<bag.read_prop<std::string>(proplist[i]) << endl;
			}
			break;

		case prop_type_mv_wstring:
			{
				vector<wstring> wstrings = bag.read_prop_array<wstring>(proplist[i]);
				cout << "prop_type_mv_wstring" << endl;
				for(size_t i = 0; i < wstrings.size(); ++i)
					wcout << wstrings[i] << endl;
			}
			break;

		case prop_type_wstring:
			{
				cout << "prop_type_wstring" << endl;
				wcout << bag.read_prop<std::wstring>(proplist[i]).c_str() << endl;
			}
			break;

		case prop_type_mv_string:
			{
				vector<string> strings = bag.read_prop_array<string>(proplist[i]);
				cout << "prop_type_mv_string" << endl;
				for(size_t i = 0; i < strings.size(); ++i)
					cout << strings[i] << endl;
			}
			break;

		case prop_type_binary:
			{
				cout << "prop_type_binary" << endl;
				cout <<bag.read_prop<pstsdk::byte>(proplist[i]) << endl;
			}
			break;

		case prop_type_mv_binary:
			{
				vector<vector<pstsdk::byte>> bins = bag.read_prop_array<vector<pstsdk::byte>>(proplist[i]);

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

void named_id_iterate(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	shared_db_ptr db = open_database(filename);
	node nd = db->lookup_node(0x61);
	heap hn(nd);

	name_id_map nmap(db);

	size_t prop_cnt = nmap.get_prop_count();
	cout << prop_cnt << endl;

	vector<prop_id> prop_list = nmap.get_prop_list();

	for(size_t ind = 0; ind < prop_list.size(); ++ind)
	{
		named_prop prop = nmap.lookup(prop_list[ind]);
		if(prop.is_string())
		{			
			cout<< "**********************************" << endl;
			cout<< hex << prop.get_id() << endl;
			cout<< prop.get_guid().data1 << " " << prop.get_guid().data2 << " " << prop.get_guid().data3 << " " << prop.get_guid().data4 <<endl;
			cout<< "is_string() " << prop.is_string() << endl;
			wcout<< prop.get_name() << endl;
			cout<< "**********************************" << endl;
		}
	}
}

void test_pc_read(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x8022;
	shared_db_ptr db = open_database(filename);
	prop_iterate(db, nid1);
}

void test_pc_create(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x42;

	{
		shared_db_ptr db = open_database(filename);

		node nd1 = db->create_node(nid1);

		property_bag bag(nd1);

		pstsdk::byte val1 = 111;
		prop_id pid1 = 0x1;
		bag.write_prop<pstsdk::byte>(pid1, val1);

		pstsdk::ushort val2 = 22222;
		prop_id pid2 = 0x2;
		bag.write_prop<pstsdk::ushort>(pid2, val2);

		pstsdk::ulong val3 = 333333333;
		prop_id pid3 = 0x3;
		bag.write_prop<pstsdk::ulong>(pid3, val3);

		pstsdk::ulonglong val4 = 444444444444444;
		prop_id pid4 = 0x4;
		bag.write_prop<pstsdk::ulonglong>(pid4, val4);

		std::string val5 = "test_value_string";
		prop_id pid5 = 0x5;
		bag.write_prop<std::string>(pid5, val5);

		std::wstring val6 = L"test_value_wstring";
		prop_id pid6 = 0x6;
		bag.write_prop<std::wstring>(pid6, val6);

		assert(bag.prop_exists(pid1));
		assert(bag.prop_exists(pid2));
		assert(bag.prop_exists(pid3));
		assert(bag.prop_exists(pid4));
		assert(bag.prop_exists(pid5));
		assert(bag.prop_exists(pid6));

		bag.save_property_bag();

		assert(bag.prop_exists(pid1));
		assert(bag.prop_exists(pid2));
		assert(bag.prop_exists(pid3));
		assert(bag.prop_exists(pid4));
		assert(bag.prop_exists(pid5));
		assert(bag.prop_exists(pid6));

		db->commit_db();
	}

	{
		shared_db_ptr db = open_database(filename);
		prop_iterate(db, nid1);
	}
}

void test_pc_insert_single_value(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x44;

	{
		shared_db_ptr db = open_database(filename);

		node nd1 = db->create_node(nid1);

		property_bag bag(nd1);

		pstsdk::byte val1 = 65;
		prop_id pid1 = 0x1;
		bag.write_prop<pstsdk::byte>(pid1, val1);

		pstsdk::ushort val2 = 65500;
		prop_id pid2 = 0x2;
		bag.write_prop<pstsdk::ushort>(pid2, val2);

		pstsdk::ulong val3 = 4294967290;
		prop_id pid3 = 0x3;
		bag.write_prop<pstsdk::ulong>(pid3, val3);

		pstsdk::ulonglong val4 = 18446744073709551610;
		prop_id pid4 = 0x4;
		bag.write_prop<pstsdk::ulonglong>(pid4, val4);

		std::string val5 = "test_value_string";
		prop_id pid5 = 0x5;
		bag.write_prop<std::string>(pid5, val5);

		std::wstring val6 = L"test_value_wstring";
		prop_id pid6 = 0x6;
		bag.write_prop<std::wstring>(pid6, val6);

		bool val7 = false;
		prop_id pid7 = 0x7;
		bag.write_prop<bool>(pid7, val7);

		vector<pstsdk::byte> val8(4000, 0);
		prop_id pid8 = 0x8;
		bag.write_prop<vector<pstsdk::byte>>(pid8, val8);

		float val9 = 567.789F;
		prop_id pid9 = 0x9;
		bag.write_prop<float>(pid9, val9);

		double val10 = 8569.3254;
		prop_id pid10 = 0xA;
		bag.write_prop<double>(pid10, val10);

		bag.save_property_bag();

		assert(bag.prop_exists(pid1));
		assert(bag.prop_exists(pid2));
		assert(bag.prop_exists(pid3));
		assert(bag.prop_exists(pid4));
		assert(bag.prop_exists(pid5));
		assert(bag.prop_exists(pid6));
		assert(bag.prop_exists(pid7));
		assert(bag.prop_exists(pid8));
		assert(bag.prop_exists(pid9));
		assert(bag.prop_exists(pid10));

		db->commit_db();
	}

	{
		shared_db_ptr db = open_database(filename);
		prop_iterate(db, nid1);
	}
}

void test_pc_insert_multi_value(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x46;

	{
		shared_db_ptr db = open_database(filename);

		node nd1 = db->create_node(nid1);

		property_bag bag(nd1);

		vector<pstsdk::ushort> val1(10, 128);
		prop_id pid1 = 0x1;
		bag.write_prop_array<pstsdk::ushort>(pid1, val1);

		vector<float> val2(10, 55.65F);
		prop_id pid2 = 0x2;
		bag.write_prop_array<float>(pid2, val2);

		vector<pstsdk::ulonglong> val3(10, 1598534628);
		prop_id pid3 = 0x3;
		bag.write_prop_array<pstsdk::ulonglong>(pid3, val3);

		vector<double> val4(10, 34628.5656);
		prop_id pid4 = 0x4;
		bag.write_prop_array<double>(pid4, val4);

		vector<bool> val5(10, true);
		prop_id pid5 = 0x5;
		bag.write_prop_array<bool>(pid5, val5);

		vector<std::string> val6(10, "test string 10");
		prop_id pid6 = 0x6;
		bag.write_prop_array<std::string>(pid6, val6);

		vector<std::wstring> val7(10, L"test wstring 10");
		prop_id pid7 = 0x7;
		bag.write_prop_array<std::wstring>(pid7, val7);

		vector<std::wstring> val8(150, L"test wstring 150");
		prop_id pid8 = 0x8;
		bag.write_prop_array<std::wstring>(pid8, val8);

		// Variable size MV property
		size_t size1 = val1.size() * sizeof(val1[0]);
		size_t size2 = val2.size() * sizeof(val2[0]);
		size_t size3 = val3.size() * sizeof(val3[0]);

		vector<pstsdk::byte> val11(size1);
		vector<pstsdk::byte> val22(size2);
		vector<pstsdk::byte> val33(size3);

		memcpy(&val11[0], &val1[0], size1);
		memcpy(&val22[0], &val2[0], size2);
		memcpy(&val33[0], &val3[0], size3);

		vector<vector<pstsdk::byte>> val9(3);
		val9[0] = val11;
		val9[1] = val22;
		val9[2] = val33;

		prop_id pid9 = 0x9;
		bag.write_prop_array<std::vector<pstsdk::byte>>(pid9, val9);

		bag.save_property_bag();

		assert(bag.prop_exists(pid1));
		assert(bag.prop_exists(pid2));
		assert(bag.prop_exists(pid3));
		assert(bag.prop_exists(pid4));
		assert(bag.prop_exists(pid5));
		assert(bag.prop_exists(pid6));
		assert(bag.prop_exists(pid7));
		assert(bag.prop_exists(pid8));
		assert(bag.prop_exists(pid9));

		db->commit_db();
	}

	{
		shared_db_ptr db = open_database(filename);
		prop_iterate(db, nid1);
	}
}

void test_pc_modify(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x21;

	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);
		heap hn(nd);

		property_bag bag(hn);

		bool val1 = bag.read_prop<bool>(0x6633);
		bag.modify_prop<bool>(0x6633, false);

		wstring val2 = bag.read_prop<wstring>(0x3001);
		bag.modify_prop<wstring>(0x3001, L"Modified Personal Folder");

		ulong val3 = bag.read_prop<ulong>(0x35DF);
		bag.modify_prop<ulong>(0x35DF, 1001);

		bag.save_property_bag();

		val1 = bag.read_prop<bool>(0x6633);
		assert(!val1);

		val2 = bag.read_prop<wstring>(0x3001);
		assert(val2.compare(L"Modified Personal Folder") == 0);

		val3 = bag.read_prop<ulong>(0x35DF);
		assert(val3 == 1001);

		db->commit_db();
	}

	{
		shared_db_ptr db = open_database(filename);
		prop_iterate(db, nid1);
	}
}

void test_pc_remove(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x122;

	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);
		heap hn(nd);

		property_bag bag(hn);

		bag.remove_prop(0x360A);
		bag.remove_prop(0x3001);

		bag.save_property_bag();

		assert(!bag.prop_exists(0x360A));
		assert(!bag.prop_exists(0x3001));

		db->commit_db();
	}

	{
		shared_db_ptr db = open_database(filename);
		prop_iterate(db, nid1);
	}
}

void test_named_id(std::wstring filename)
{
	using namespace std;
	using namespace std::tr1;
	using namespace pstsdk;

	node_id nid1 =  0x61;

	named_id_iterate(filename);

	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);
		heap hn(nd);

		name_id_map nmap(db);

		const guid g1 = { 0x20386, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
		named_prop t1(g1, L"www.microsoft.com");

		nmap.add_prop(t1);
		nmap.save_name_id_map();

		db->commit_db();
	}

	named_id_iterate(filename);

	{
		shared_db_ptr db = open_database(filename);
		node nd = db->lookup_node(nid1);
		heap hn(nd);

		name_id_map nmap(db);

		const guid g2 = { 0x62002, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
		named_prop t2(g2, 0x8233);

		nmap.add_prop(t2);
		nmap.save_name_id_map();

		db->commit_db();
	}

	named_id_iterate(filename);
}

void test_pc()
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
		test_pc_read(tmp_large_file);

		test_pc_create(tmp_large_file);

		test_pc_insert_single_value(tmp_large_file);

		test_pc_insert_multi_value(tmp_large_file);

		test_pc_modify(tmp_large_file);

		test_pc_remove(tmp_large_file);

		test_named_id(tmp_large_file);


		// only valid to call on test_ansi.pst
		test_pc_read(tmp_small_file);

		test_pc_create(tmp_small_file);

		test_pc_insert_single_value(tmp_small_file);

		test_pc_insert_multi_value(tmp_small_file);

		test_pc_modify(tmp_small_file);

		test_pc_remove(tmp_small_file);


		test_utilities::delete_file(tmp_large_file);
		test_utilities::delete_file(tmp_small_file);
	}
	else
	{
		throw runtime_error("error creating temp files");
	}
}