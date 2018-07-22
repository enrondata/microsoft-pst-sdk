#ifdef _MSC_VER
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include <typeinfo>
#include <iostream>
#include "test.h"


int main()
{
	using namespace std;
	try
	{
		cout << "test_util();" << endl;
		test_util();
		cout << "test_btree();" << endl;
		test_btree();
		cout << "test_disk();" << endl;
		test_disk();
		cout << "test_db();" << endl;
		test_db();
		cout << "test_highlevel();" << endl;
		test_highlevel();
		cout << "test_pstlevel();" << endl;
		test_pstlevel();
		cout << "test_btpage();" << endl;
		test_btpage();
		cout << "test_subnode();" << endl;
		test_subnode();
		cout << "test_amap();" << endl;
		test_amap();
		cout << "test_node_save();" << endl;
		test_node_save();
		cout << "test_db_context();" << endl;
		test_db_context();
		cout << "test_thread_safety();" << endl;
		test_thread_safety();
		cout << "test_heap_node();" << endl;
		test_heap_node();
		cout << "test_bth();" << endl;
		test_bth();
		cout << "test_pc();" << endl;
		test_pc();
		cout << "test_tc();" << endl;
		test_tc();
		cout << "test_pst();" << endl;
		test_pst();
	} 
	catch(exception& e)
	{
		cout << "*****" << typeid(e).name() << "*****" << endl;
		cout << e.what();
		throw;
	}

#ifdef _MSC_VER
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDOUT );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDOUT );
	_CrtDumpMemoryLeaks();
#endif
}
