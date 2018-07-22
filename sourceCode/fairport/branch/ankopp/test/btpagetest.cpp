#include <iostream>
#include <pstsdk/ndb.h>
#include <pstsdk/ltp.h>
#include <pstsdk/pst.h>
#include <cassert>

bool validateTree(const pstsdk::bbt_page* bbtPage)
{
    for(unsigned int i = 0; i < bbtPage->num_values(); ++i)
    {
        if(i < (bbtPage->num_values() - 1))
        {
            if(bbtPage->get_key(i) >= bbtPage->get_key(i + 1))
            {
                return false;
            }
        }
        if(bbtPage->get_level() > 0)
        {
            const pstsdk::bbt_nonleaf_page* bbtNonLeaf = dynamic_cast<const pstsdk::bbt_nonleaf_page*>(bbtPage);
            if(bbtPage->get_key(i) != bbtNonLeaf->get_child(i)->get_key(0))
            {
                return false;
            }
            if(!validateTree(bbtNonLeaf->get_child(i)))
            {
                return false;
            }
        }
    }

    return true;
}

void displayValidity(const pstsdk::bbt_page* bbtPage)
{
    assert(validateTree(bbtPage));    
}

void test_btpage()
{
    std::wstring pstFile = L"test_unicode.pst";
    pstsdk::shared_db_ptr db = pstsdk::open_database(pstFile);
    std::tr1::shared_ptr<pstsdk::bbt_page> bbtRoot = db->read_bbt_root();
    std::tr1::shared_ptr<pstsdk::bbt_page> bbtPage;

    pstsdk::block_id key;
    pstsdk::block_info val = {0, 0, 10, 1};
    std::pair<std::tr1::shared_ptr<pstsdk::bbt_page>, std::tr1::shared_ptr<pstsdk::bbt_page>> bbtPagePair;

    displayValidity(bbtRoot.get());

    //pstsdk::page_info pgInfo = {379, 28160};
    //std::tr1::shared_ptr<pstsdk::bbt_nonleaf_page> bbtNonLeaf = db->read_bbt_nonleaf_page(pgInfo);
    //for(unsigned int i = 0; i < bbtNonLeaf->num_values(); ++i)
    //{
    //    bbtNonLeaf->get_child(i);
    //}

    // Insert an existing key
    key = 4; val.id = key;
    bbtPagePair = bbtRoot->insert(key, val);
    displayValidity(bbtPagePair.first.get());
    //

    // Insert key at the beginning of list of page entries, without split
    key = 3; val.id = key;
    bbtPagePair = bbtRoot->insert(key, val);
    displayValidity(bbtPagePair.first.get());
    //

    // Insert keys to cause a page split
    key = 2; val.id = key;
    bbtPagePair.first->insert(key, val);

    key = 1; val.id = key;
    bbtPagePair.first->insert(key, val);

    key = 7; val.id = key;
    bbtPagePair.first->insert(key, val);
    //

    // Insert a key in middle of list of page entries, without split
    key = 300; val.id = key;
    bbtPagePair = bbtRoot->insert(key, val);
    displayValidity(bbtPagePair.first.get());
    //

    // Insert a key at the end of list of page entries, without split
    key = 1000; val.id = key;
    bbtPagePair = bbtRoot->insert(key, val);
    displayValidity(bbtPagePair.first.get());
    //


    // Remove key at the beginning of list of page entries, without emptying the page
    key = 4;
    bbtPage = bbtRoot->remove(key);
    displayValidity(bbtPage.get());
    //

    // Remove a key in middle of list of page entries, without emptying the page
    key = 104;
    bbtPage = bbtRoot->remove(key);
    displayValidity(bbtPage.get());
    //

    // Remove a key at the end of list of page entries, without emptying the page
    key = 156;
    bbtPage = bbtRoot->remove(key);
    displayValidity(bbtPage.get());
    //

	// Remove non existing keys
    try
	{
		key = 43256;
		bbtPage = bbtRoot->remove(key);
	}
	catch(...)
	{
		std::cout<< "Key not found\n";
	}

    // Remove all keys to make the page empty
    pstsdk::block_id tKeys[10] = {324, 330, 336, 344, 352, 356, 360, 364, 368, 372};
    std::tr1::shared_ptr<pstsdk::bbt_page> temp = bbtRoot;
    for(unsigned int i = 0; i < 10; ++i)
    {
        temp = temp->remove(tKeys[i]);
    }
    displayValidity(temp.get());
    //


    // Modify key at the beginning of list of page entries
    key = 4; val.id = 10;
    bbtPage = bbtRoot->modify(key, val);
    displayValidity(bbtPage.get());
    //

    // Modify a key in middle of list of page entries
    key = 104; val.id = 10;
    bbtPage = bbtRoot->modify(key, val);
    displayValidity(bbtPage.get());
    //

    // Modify a key at the end of list of page entries, without emptying the page
    key = 156; val.id = 10;
    bbtPage = bbtRoot->modify(key, val);
    displayValidity(bbtPage.get());
    //

	// Modify non existing keys
    key = 43256; val.id = 10;
	try
	{
		bbtPage = bbtRoot->modify(key, val);
	}
	catch(...)
	{
		std::cout<< "Key not found\n";
	}
    displayValidity(bbtPage.get());
    
}
