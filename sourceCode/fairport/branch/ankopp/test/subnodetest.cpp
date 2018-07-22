#include <iostream>
#include <pstsdk/ndb.h>
#include <pstsdk/ltp.h>
#include <pstsdk/pst.h>
#include <cassert>

void fillSLBlock(std::tr1::shared_ptr<pstsdk::subnode_block> * block)
{
	pstsdk::subnode_info sbInfo1;// = {4, 5, 6};
	std::pair<std::shared_ptr<pstsdk::subnode_block>, std::shared_ptr<pstsdk::subnode_block>> subNodeBlockPair;
	for(int i = 1; subNodeBlockPair.second == 0; i++ )
	{
		sbInfo1.id = i;
		sbInfo1.data_bid = i + 1;
		sbInfo1.sub_bid = 0;
		try
		{
			subNodeBlockPair = (*block)->insert(sbInfo1.id, sbInfo1);
		}
		catch(pstsdk::key_not_found<pstsdk::node_id> pError )
		{
		}
		*block = subNodeBlockPair.first;
	}
	std::cout<< "SLBlock split\n";
}

void emptySLBlock(std::tr1::shared_ptr<pstsdk::subnode_block> * block)
{
	pstsdk::subnode_info sbInfo1;// = {4, 5, 6};
	std::tr1::shared_ptr<pstsdk::subnode_block> block1;
	for(int i = 0; 1 ; i++ )
	{
		sbInfo1.id = i;
		sbInfo1.data_bid = i + 1;
		sbInfo1.sub_bid = 0;
		try
		{
			block1 = (*block)->remove(sbInfo1.id);
			*block = block1;
			if(!block1)
			{
				break;
			}
		}
		catch(pstsdk::key_not_found<pstsdk::node_id>  pError) 
		{
			std::cout<< pError.what()  << "\n";
		}	
		
	}
	std::cout<< "SLBlock empty\n";
}

void testSIBlockSplit(std::tr1::shared_ptr<pstsdk::subnode_block> block)
{
	std::pair<std::shared_ptr<pstsdk::subnode_block>, std::shared_ptr<pstsdk::subnode_block>> subNodeBlockPair;
	pstsdk::subnode_info sbInfo1;// = {4, 5, 6};	
	
	std::pair<std::shared_ptr<pstsdk::subnode_block>, std::shared_ptr<pstsdk::subnode_block>> subNodeBlockPair1;// = t->insert(sbInfo1.id,sbInfo1);
	for(int i = 1; subNodeBlockPair1.second == 0; i++ )
	{
		sbInfo1.id = i;
		sbInfo1.data_bid = i + 1;
		sbInfo1.sub_bid = 0;
		subNodeBlockPair1 = block->insert(sbInfo1.id, sbInfo1);
		block = subNodeBlockPair1.first;
	}
	std::cout<< "SIBlock split\n";
}

void test_subnode()
{
    std::wstring pstFile = L"test_unicode.pst";
	pstsdk::shared_db_ptr db = pstsdk::open_database(pstFile);
	std::tr1::shared_ptr<pstsdk::nbt_page> nbtRoot = db->read_nbt_root();
	pstsdk::node_id nid = 0x61;

	//find the node which has a sub_node 
	pstsdk::node_info nInfo = nbtRoot->lookup(nid);

	//get sub_node block
	std::tr1::shared_ptr<pstsdk::subnode_block> block = db->read_subnode_block(nInfo.sub_bid);
	

	fillSLBlock(&block);
	//insert already existing sub_node
	pstsdk::subnode_info sbInfo = {1, 4, 5};
	block->insert(sbInfo.id, sbInfo);

	//test remove()
	std::tr1::shared_ptr<pstsdk::subnode_block> block1;
	//remove last element
	block1 = block->remove(340);
	//remove first element
	block = block1->remove(1);
	//remove intermediate element
	block1 = block->remove(10);
	//remove non existing element
	try
	{
		block1 = block->remove(341);
	}
	catch(pstsdk::key_not_found<pstsdk::node_id>  pError )
	{
		std::cout<< pError.what()  << "\n";
	}

	//modify already existing key
	sbInfo.id = 5;
	sbInfo.data_bid = 10;
	block->modify(sbInfo.id, sbInfo);
	//modify non existing element
	try
	{
		sbInfo.id = 1234;
		block->modify(sbInfo.id, sbInfo);
	}
	catch(pstsdk::key_not_found<pstsdk::node_id>  pError )
	{
		std::cout<< pError.what() << "\n";
	}


	emptySLBlock(&block1);
	

	//create siblock
	block = db->read_subnode_block(nInfo.sub_bid);
	std::vector<std::pair<pstsdk::node_id, pstsdk::block_id>> v;
	v.push_back(std::make_pair(block->get_key(0),block->get_id()));	

	std::tr1::shared_ptr<pstsdk::subnode_nonleaf_block> t(new pstsdk::subnode_nonleaf_block(db,v,block->get_max_entries())); 
	testSIBlockSplit(t);
	
}
