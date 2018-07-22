//! \file
//! \brief Page definitions
//! \author Terry Mahaffey
//!
//! A page is 512 bytes of metadata contained in a PST file. Pages come in
//! two general flavors:
//! - BTree Pages (immutable)
//! - Allocation Related Pages (mutable)
//!
//! The former are pages which collectively form the BBT and NBT. They are
//! immutable. The later exist at predefined locations in the file, and are
//! always modified in place.
//! \ingroup ndb

#ifndef PSTSDK_NDB_PAGE_H
#define PSTSDK_NDB_PAGE_H

#include <vector>

#include "pstsdk/util/btree.h"
#include "pstsdk/util/util.h"

#include "pstsdk/ndb/database_iface.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4250)
#endif

namespace pstsdk
{

//! \defgroup ndb_pagerelated Pages
//! \ingroup ndb

//! \brief Generic base class for all page types
//!
//! This class provides an abstraction around the page trailer located at the
//! end of every page. The actual page content is interpretted by the child
//! classes of \ref page.
//!
//! All pages in the pages hierarchy are also in the category of what is known
//! as <i>dependant objects</i>. This means is they only keep a weak 
//! reference to the database context to which they're a member. Contrast this
//! to an independant object such as the \ref heap, which keeps a strong ref
//! or a full shared_ptr to the related context. This implies that someone
//! must externally make sure the database context outlives it's pages -
//! this is usually done by the database context itself.
//! \sa [MS-PST] 2.2.2.7
//! \ingroup ndb_pagerelated
class page
{
public:
    //! \brief Construct a page from disk
    //! \param[in] db The database context
    //! \param[in] pi Information about this page
    page(const shared_db_ptr& db, const page_info& pi)
        : m_modified(false), m_db(db), m_pid(pi.id), m_address(pi.address) { }

    page(const page& other)
        : m_modified(false), m_db(other.m_db), m_pid(0), m_address(0) { }

    virtual ~page() { }

    //! \brief Get the page id
    //! \returns The page id
    page_id get_page_id() const { return m_pid; }

    //! \brief Get the physical address of this page
    //! \returns The address of this page, or 0 for a new page
    ulonglong get_address() const { return m_address; }

    void touch();

protected:
    shared_db_ptr get_db_ptr() const { return shared_db_ptr(m_db); }
    bool m_modified;        //!< Tells if the page is modified or not
    weak_db_ptr m_db;       //!< The database context we're a member of
    page_id m_pid;          //!< Page id
    ulonglong m_address;    //!< Address of this page
};

//!< \brief A page which forms a node in the NBT or BBT
//!
//! The NBT and BBT form the core of the internals of the PST, and need to be
//! well understood if working at the \ref ndb. The bt_page class forms the
//! nodes of both the NBT and BBT, with child classes for leaf and nonleaf
//! pages.
//!
//! This hierarchy also models the \ref btree_node structure, inheriting the 
//! actual iteration and lookup logic.
//! \tparam K key type
//! \tparam V value type
//! \sa [MS-PST] 1.3.1.1
//! \sa [MS-PST] 2.2.2.7.7
//! \ingroup ndb_pagerelated
template<typename K, typename V>
class bt_page : 
    public page, 
    public virtual btree_node<K,V>
{
public:
    //! \brief Construct a bt_page from disk
    //! \param[in] db The database context
    //! \param[in] pi Information about this page
    //! \param[in] level 0 for a leaf, or distance from leaf
    bt_page(const shared_db_ptr& db, const page_info& pi, ushort level, size_t max_entries)
        : page(db, pi), m_level(level), m_max_entries(max_entries) { }

    virtual ~bt_page() { }

    //! \cond write_api
    //! brief Inserts a new element into the page
    //! \param[in] key The key of new element to be inserted
    //! \param[in] val The value of new element to be inserted
    //! \returns A pair of new pages with the new element inserted
    virtual std::pair<std::tr1::shared_ptr<bt_page<K,V> >, std::tr1::shared_ptr<bt_page<K,V> > > insert(const K& key, const V& val) = 0;
    
    //! brief Modifies an existing element
    //! \param[in] key The key of element to be modified
    //! \param[in] val The new value of the element to be modified
    //! \returns A new page with the specified element modified
    virtual std::tr1::shared_ptr<bt_page<K,V> > modify(const K& key, const V& val) = 0;
    
    //! brief Deletes an existing element
    //! \param[in] key The key of element to be removed
    //! \returns A new page with the specified element removed
    virtual std::tr1::shared_ptr<bt_page<K,V> > remove(const K& key) = 0;
    //! \endcond

    //! \brief Returns the level of this bt_page
    //! \returns The level of this page
    ushort get_level() const { return m_level; }

    //! \brief Returns the maximum number of entries this bt_page can hold
    //! \return The maximum number of entries this page can hold
    size_t get_max_entries() const { return m_max_entries; }

private:
    ushort m_level;         //!< Our level
    size_t m_max_entries;   //!< Maximum number of entries page can hold
};

//! \brief Contains references to other bt_pages
//!
//! A bt_nonleaf_page makes up the body of the NBT and BBT (which differ only
//! at the leaf). 
//! \tparam K key type
//! \tparam V value type
//! \sa [MS-PST] 2.2.2.7.7.2
//! \ingroup ndb_pagerelated
template<typename K, typename V>
class bt_nonleaf_page : 
    public bt_page<K,V>, 
    public btree_node_nonleaf<K,V>, 
    public std::tr1::enable_shared_from_this<bt_nonleaf_page<K,V> >
{
public:
    //! \brief Construct a bt_nonleaf_page from disk
    //! \param[in] db The database context
    //! \param[in] pi Information about this page
    //! \param[in] level Distance from leaf
    //! \param[in] subpi Information about the child pages
#ifndef BOOST_NO_RVALUE_REFERENCES
    bt_nonleaf_page(const shared_db_ptr& db, const page_info& pi, ushort level, std::vector<std::pair<K, page_info> > subpi, size_t max_entries)
        : bt_page<K,V>(db, pi, level, max_entries), m_page_info(std::move(subpi)), m_child_pages(m_page_info.size()) { }
    bt_nonleaf_page(const shared_db_ptr& db, ushort level, std::vector<std::pair<K, page_info> > subpi, size_t max_entries)
        : bt_page<K,V>(db, page_info(), level, max_entries), m_page_info(std::move(subpi)), m_child_pages(m_page_info.size())
    { touch(); }
#else
    bt_nonleaf_page(const shared_db_ptr& db, const page_info& pi, ushort level, const std::vector<std::pair<K, page_info> >& subpi, size_t max_entries)
        : bt_page<K,V>(db, pi, level, max_entries), m_page_info(subpi), m_child_pages(m_page_info.size()) { }
    bt_nonleaf_page(const shared_db_ptr& db, ushort level, const std::vector<std::pair<K, page_info> > subpi, size_t max_entries)
        : bt_page<K,V>(db, page_info(), level, max_entries), m_page_info(subpi), m_child_pages(m_page_info.size())
    { touch(); }
#endif

    bt_nonleaf_page(const bt_nonleaf_page& other)
        : bt_page<K,V>(other.get_db_ptr(), page_info(), other.get_level(), other.get_max_entries()), m_page_info(other.m_page_info), m_child_pages(other.m_child_pages)
    { touch(); }

    //! \cond write_api
    virtual std::pair<std::tr1::shared_ptr<bt_page>, std::tr1::shared_ptr<bt_page<K,V> > > insert(const K& key, const V& val);
    virtual std::tr1::shared_ptr<bt_page<K,V> > modify(const K& key, const V& val);
    virtual std::tr1::shared_ptr<bt_page<K,V> > remove(const K& key);
    //! \endcond

    // btree_node_nonleaf implementation
    const K& get_key(uint pos) const { return m_page_info[pos].first; }
    bt_page<K,V>* get_child(uint pos);
    const bt_page<K,V>* get_child(uint pos) const;
    uint num_values() const { return m_child_pages.size(); }

private:
    std::vector<std::pair<K, page_info> > m_page_info;   //!< Information about the child pages
    mutable std::vector<std::tr1::shared_ptr<bt_page<K,V> > > m_child_pages; //!< Cached child pages
};

//! \brief Contains the actual key value pairs of the btree
//! \tparam K key type
//! \tparam V value type
//! \ingroup ndb_pagerelated
template<typename K, typename V>
class bt_leaf_page : 
    public bt_page<K,V>, 
    public btree_node_leaf<K,V>, 
    public std::tr1::enable_shared_from_this<bt_leaf_page<K,V> >
{
public:
    //! \brief Construct a leaf page from disk
    //! \param[in] db The database context
    //! \param[in] pi Information about this page
    //! \param[in] data The key/value pairs on this leaf page
#ifndef BOOST_NO_RVALUE_REFERENCES
    bt_leaf_page(const shared_db_ptr& db, const page_info& pi, std::vector<std::pair<K,V> > data, size_t max_entries)
        : bt_page<K,V>(db, pi, 0, max_entries), m_page_data(std::move(data)) { }
    bt_leaf_page(const shared_db_ptr& db, std::vector<std::pair<K,V> > data, size_t max_entries)
        : bt_page<K,V>(db, page_info(), 0, max_entries), m_page_data(std::move(data))
    { touch(); }
#else
    bt_leaf_page(const shared_db_ptr& db, const page_info& pi, const std::vector<std::pair<K,V> >& data, size_t max_entries)
        : bt_page<K,V>(db, pi, 0, max_entries), m_page_data(data) { }
    bt_leaf_page(const shared_db_ptr& db, const std::vector<std::pair<K,V> > data, size_t max_entries)
        : bt_page<K,V>(db, page_info(), 0, max_entries), m_page_data(data)
    { touch(); }
#endif

    
    bt_leaf_page(const bt_leaf_page& other)
        : bt_page<K,V>(other.get_db_ptr(), page_info(), 0, other.get_max_entries()), m_page_data(other.m_page_data)
    { touch(); }

    //! \cond write_api
    virtual std::pair<std::tr1::shared_ptr<bt_page<K,V> >, std::tr1::shared_ptr<bt_page<K,V> > > insert(const K& key, const V& val);
    virtual std::tr1::shared_ptr<bt_page<K,V> > modify(const K& key, const V& val);
    virtual std::tr1::shared_ptr<bt_page<K,V> > remove(const K& key);
    //! \endcond

    // btree_node_leaf implementation
    const V& get_value(uint pos) const
        { return m_page_data[pos].second; }
    const K& get_key(uint pos) const
        { return m_page_data[pos].first; }
    uint num_values() const
        { return m_page_data.size(); }

private:
    std::vector<std::pair<K,V> > m_page_data; //!< The key/value pairs on this leaf page
};

inline void page::touch()
{
    if(!m_modified)
    {
        m_modified = true;
        m_address = 0;
        m_pid = get_db_ptr()->alloc_pid();
    }
}

//! \cond dont_show_these_member_function_specializations
template<>
inline bt_page<block_id, block_info>* bt_nonleaf_page<block_id, block_info>::get_child(uint pos)
{
    if(m_child_pages[pos] == NULL)
    {
        m_child_pages[pos] = this->get_db_ptr()->read_bbt_page(m_page_info[pos].second);
    }

    return m_child_pages[pos].get();
}

template<>
inline const bt_page<block_id, block_info>* bt_nonleaf_page<block_id, block_info>::get_child(uint pos) const
{
    if(m_child_pages[pos] == NULL)
    {
        m_child_pages[pos] = this->get_db_ptr()->read_bbt_page(m_page_info[pos].second);
    }

    return m_child_pages[pos].get();
}

template<>
inline bt_page<node_id, node_info>* bt_nonleaf_page<node_id, node_info>::get_child(uint pos)
{
    if(m_child_pages[pos] == NULL)
    {
        m_child_pages[pos] = this->get_db_ptr()->read_nbt_page(m_page_info[pos].second); 
    }

    return m_child_pages[pos].get();
}

template<>
inline const bt_page<node_id, node_info>* bt_nonleaf_page<node_id, node_info>::get_child(uint pos) const
{
    if(m_child_pages[pos] == NULL)
    {
        m_child_pages[pos] = this->get_db_ptr()->read_nbt_page(m_page_info[pos].second); 
    }

    return m_child_pages[pos].get();
}
//! \endcond

template<typename K, typename V>
inline std::pair<std::tr1::shared_ptr<bt_page<K,V> >, std::tr1::shared_ptr<bt_page<K,V> > > bt_nonleaf_page<K, V>::insert(const K& key, const V& val)
{
    std::tr1::shared_ptr<bt_nonleaf_page<K, V> > copiedPage1 = shared_from_this();
    if(copiedPage1.use_count() > 2)
    {
        std::tr1::shared_ptr<bt_nonleaf_page<K, V> > cnewPage = std::tr1::make_shared<bt_nonleaf_page<K, V> >(*this);
        return cnewPage->insert(key, val);
    }

    touch(); // mutate ourselves inplace

    std::tr1::shared_ptr<bt_nonleaf_page<K, V> > copiedPage2 (0);

    int pos = this->binary_search(key);
    if(pos == -1)
    {
        pos = 0;
    }

    std::pair<std::tr1::shared_ptr<bt_page<K,V> >, std::tr1::shared_ptr<bt_page<K,V> > > result (this->get_child(pos)->insert(key, val));

    page_info pi;
    pi.id = result.first->get_page_id();
    pi.address = result.first->get_address();
    this->m_page_info[pos].first = result.first->get_key(0);
    this->m_page_info[pos].second = pi;
    this->m_child_pages[pos] = result.first;

    if(result.second.get() != 0)
    {
        pi.id = result.second->get_page_id();
        pi.address = result.second->get_address();
        this->m_page_info.insert(this->m_page_info.begin() + pos + 1, std::make_pair(result.second->get_key(0), pi));
        this->m_child_pages.insert(this->m_child_pages.begin() + pos + 1, result.second);

        if(this->m_page_info.size() > this->get_max_entries())
        {
            copiedPage2 = std::tr1::make_shared<bt_nonleaf_page<K, V> >(this->get_db_ptr(), this->get_level(), std::vector<std::pair<K, page_info> > (), this->get_max_entries());
            copiedPage2->m_page_info.push_back(this->m_page_info.back());
            copiedPage2->m_child_pages.push_back(this->m_child_pages.back());
            this->m_page_info.pop_back();
            this->m_child_pages.pop_back();
        }
    }

    return std::make_pair(copiedPage1, copiedPage2);
}

template<typename K, typename V>
inline std::tr1::shared_ptr<bt_page<K, V> > bt_nonleaf_page<K, V>::modify(const K& key, const V& val)
{
    std::tr1::shared_ptr<bt_nonleaf_page<K, V> > copiedPage = shared_from_this();
    if(copiedPage.use_count() > 2)
    {
        std::tr1::shared_ptr<bt_nonleaf_page<K, V> > cnewPage = std::tr1::make_shared<bt_nonleaf_page<K, V> >(*this);
        return cnewPage->modify(key, val);
    }

    touch(); // mutate ourselves inplace

    int pos = this->binary_search(key);
    if(pos == -1)
    {
        throw key_not_found<K>(key);
    }

    std::tr1::shared_ptr<bt_page<K, V> > result(this->get_child(pos)->modify(key, val));
    page_info pi;
    pi.id = result->get_page_id();
    pi.address = result->get_address();
    this->m_page_info[pos].first = result->get_key(0);
    this->m_page_info[pos].second = pi;
    this->m_child_pages[pos] = result;

    return copiedPage;
}

template<typename K, typename V>
inline std::tr1::shared_ptr<bt_page<K, V> > bt_nonleaf_page<K, V>::remove(const K& key)
{
    std::tr1::shared_ptr<bt_nonleaf_page<K, V> > copiedPage = shared_from_this();
    if(copiedPage.use_count() > 2)
    {
        std::tr1::shared_ptr<bt_nonleaf_page<K, V> > cnewPage = std::tr1::make_shared<bt_nonleaf_page<K, V> >(*this);
        return cnewPage->remove(key);
    }

    touch(); // mutate ourselves inplace

    int pos = this->binary_search(key);
    if(pos == -1)
    {
        throw key_not_found<K>(key);
    }

    std::tr1::shared_ptr<bt_page<K, V> > result(this->get_child(pos)->remove(key));
    if(result.get() == 0)
    {
        this->m_page_info.erase(this->m_page_info.begin() + pos);
        this->m_child_pages.erase(this->m_child_pages.begin() + pos);
        if(this->m_page_info.size() == 0)
        {
            return std::tr1::shared_ptr<bt_nonleaf_page<K, V> >(0);
        }
    }
    else
    {
        page_info pi;
        pi.id = result->get_page_id();
        pi.address = result->get_address();
        this->m_page_info[pos].first = result->get_key(0);
        this->m_page_info[pos].second = pi;
        this->m_child_pages[pos] = result;
    }

    return copiedPage;
}

template<typename K, typename V>
inline std::pair<std::shared_ptr<bt_page<K,V> >, std::tr1::shared_ptr<bt_page<K,V> > > bt_leaf_page<K, V>::insert(const K& key, const V& val)
{
    std::tr1::shared_ptr<bt_leaf_page<K, V> > copiedPage1 = shared_from_this();
    if(copiedPage1.use_count() > 2)
    {
        std::tr1::shared_ptr<bt_leaf_page<K, V> > cnewPage = std::tr1::make_shared<bt_leaf_page<K, V> >(*this);
        return cnewPage->insert(key, val);
    }

    touch(); // mutate ourselves inplace

    std::tr1::shared_ptr<bt_leaf_page<K, V> > copiedPage2 (0);

    int pos = this->binary_search(key);
    uint idx = pos + 1;
    if((pos > -1) && (static_cast<unsigned int>(pos) < this->m_page_data.size()) && (this->get_key(pos) == key))
    {
        // If key already exists, behave like modify
        this->m_page_data[pos].second = val;
    }
    else
    {
        this->m_page_data.insert(this->m_page_data.begin() + idx, std::make_pair(key, val));

        if(this->m_page_data.size() > this->get_max_entries())
        {
            copiedPage2 = std::tr1::make_shared<bt_leaf_page<K, V> >(this->get_db_ptr(), std::vector<std::pair<K,V> >(), this->get_max_entries());
            copiedPage2->m_page_data.push_back(this->m_page_data.back());
            this->m_page_data.pop_back();
        }
    }

    return std::make_pair(copiedPage1, copiedPage2);
}

template<typename K, typename V>
inline std::tr1::shared_ptr<bt_page<K, V> > bt_leaf_page<K, V>::modify(const K& key, const V& val)
{
    std::tr1::shared_ptr<bt_leaf_page<K, V> > copiedPage = shared_from_this();
    if(copiedPage.use_count() > 2)
    {
        std::tr1::shared_ptr<bt_leaf_page<K, V> > cnewPage = std::tr1::make_shared<bt_leaf_page<K, V> >(*this);
        return cnewPage->modify(key, val);
    }

    touch(); // mutate ourselves inplace

    int pos = this->binary_search(key);
    if(pos == -1)
    {
        throw key_not_found<K>(key);
    }
    if(this->get_key(pos) != key)
    {
        throw key_not_found<K>(key);
    }
    this->m_page_data[pos].second = val;

    return copiedPage;
}

template<typename K, typename V>
inline std::tr1::shared_ptr<bt_page<K, V> > bt_leaf_page<K, V>::remove(const K& key)
{
    std::tr1::shared_ptr<bt_leaf_page<K, V> > copiedPage = shared_from_this();
    if(copiedPage.use_count() > 2)
    {
        std::tr1::shared_ptr<bt_leaf_page<K, V> > cnewPage = std::tr1::make_shared<bt_leaf_page<K, V> >(*this);
        return cnewPage->remove(key);
    }

    touch(); // mutate ourselves inplace

    int pos = this->binary_search(key);
    if(pos == -1)
    {
        throw key_not_found<K>(key);
    }
    if(this->get_key(pos) != key)
    {
        throw key_not_found<K>(key);
    }
    this->m_page_data.erase(this->m_page_data.begin() + pos);

    if(this->num_values() == 0)
    {
        return std::tr1::shared_ptr<bt_leaf_page<K, V> >(0);
    }

    return copiedPage;
}

} // end namespace

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
