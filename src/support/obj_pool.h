//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _OBJ_POOL_H
#define _OBJ_POOL_H
//! ----------------------------------------------------------------------------
//! Includes
//! ----------------------------------------------------------------------------
#include <vector>
#include <set>
namespace ns_is2 {
//! ----------------------------------------------------------------------------
//! Obj Pool
//! ----------------------------------------------------------------------------
template<class _Tp>
class obj_pool
{
public:
        // ---------------------------------------
        // Types
        // ---------------------------------------
        typedef uint64_t idx_t;
        typedef std::set<idx_t> obj_idx_set_t;
        typedef std::vector<_Tp *> obj_vec_t;
        //typedef int (*delete_cb_t) (void* o_1, void *a_2);
        // ---------------------------------------
        // Public methods
        // ---------------------------------------
        _Tp *get_free(void)
        {
                if(m_free_idx_set.size())
                {
                        idx_t l_idx = *(m_free_idx_set.begin());
                        m_free_idx_set.erase(l_idx);
                        m_used_idx_set.insert(l_idx);
                        return m_obj_vec[l_idx];
                }
                return NULL;
        }
        void add(_Tp *a_obj)
        {
                if(!a_obj)
                {
                        return;
                }
                m_obj_vec.push_back(a_obj);
                idx_t l_idx = m_obj_vec.size() - 1;
                a_obj->set_idx(l_idx);
                m_used_idx_set.insert(l_idx);
        }
        void release(_Tp *a_obj)
        {
                if(!a_obj)
                {
                        return;
                }
                idx_t l_idx = a_obj->get_idx();
                m_used_idx_set.erase(l_idx);
                m_free_idx_set.insert(a_obj->get_idx());
        }
        uint64_t free_size(void) const
        {
                return m_free_idx_set.size();
        }
        uint64_t used_size(void) const
        {
                return m_used_idx_set.size();
        }
        // TODO --release all unused -shrink routine...
        obj_pool(void):
                m_obj_vec(),
                m_used_idx_set(),
                m_free_idx_set()
        {}
        ~obj_pool(void)
        {
                for(typename obj_vec_t::iterator i_obj = m_obj_vec.begin();
                    i_obj != m_obj_vec.end();
                    ++i_obj)
                {
                        if(*i_obj)
                        {
                                delete *i_obj;
                                *i_obj = NULL;
                        }
                }
                m_obj_vec.clear();
        }
        //void set_delete_cb(delete_cb_t a_delete_cb, void *a_o_1)
        //{
        //        m_delete_cb = a_delete_cb;
        //        m_o_1 = a_o_1;
        //}
private:
        // ---------------------------------------
        // Private methods
        // ---------------------------------------
        // Disallow copy/assign
        obj_pool& operator=(const obj_pool &);
        obj_pool(const obj_pool &);
        // ---------------------------------------
        // Private members
        // ---------------------------------------
        //delete_cb_t m_delete_cb;
        //void *m_o_1;
        obj_vec_t m_obj_vec;
        obj_idx_set_t m_used_idx_set;
        obj_idx_set_t m_free_idx_set;
};
} //namespace ns_is2 {
#endif

