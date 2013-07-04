/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2011-2012 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include "mfx_shared_ptr.h"

//generic interface for facade like interface that hides collection of similar objects
template <class TTo, class TFrom>
class UniversalRenterpretCast_t
{
public:
    UniversalRenterpretCast_t(TFrom ptr)
        : from(ptr)
    {}
    union
    {
        TFrom from;
        TTo   to;
    };
};

template <class TTo, class TFrom>
TTo reinterpret_cast_ex(TFrom ptr)
{
    return UniversalRenterpretCast_t<TTo, TFrom>(ptr).to;
}


template <class IFace>
class IMultiple
    : public IFace
{
public:
    IMultiple()
        : m_bRatioDependend()
    {
    }
    //imultiple uses interfaces depending on their ratios distributed
    //in that case timestorepeat should be a degree of 2 *base_ratio
    //default this is off
    void SetRatioOrder(bool bRatioDependendOrder)
    {
        m_bRatioDependend = bRatioDependendOrder;
    }

    void Register(IFace *pItem, size_t nTimesToRepeat) 
    {
        if (!m_bRatioDependend)
        {
            m_items.push_back(std::make_pair(pItem, nTimesToRepeat));
            return;
        }

        //inserting by descending order
        typename Items_t::iterator i = m_items.begin();

        for (i = m_items.begin(); i != m_items.end(); i++)
        {
            if (i->second > nTimesToRepeat)
                break;
        }

        m_items.insert(i, std::make_pair(pItem, nTimesToRepeat));
    }
    virtual ~IMultiple()
    {
       //ForEach( deleter<IFace *>());
    }
protected:
    //TODO: better to have pointer to member in template form here, to reduce casting in child code
    template<class T>
    void RegisterFunction(T ptr_to_member)
    {
        m_nCurrentItem[reinterpret_cast_ex<void*>(ptr_to_member)] = FncPos();
    }
    template<class T>
    IFace* NextItemForFunction(T ptr_to_member)
    {
        IFace *p = m_items[m_nCurrentItem[reinterpret_cast_ex<void*>(ptr_to_member)].nObjIdx].first;
        IncrementItemIdx(ptr_to_member);
        return p;
    }
    template<class T>
    size_t CurrentItemIdx(T ptr_to_member)
    {
        return m_nCurrentItem[reinterpret_cast_ex<void*>(ptr_to_member)].nObjIdx;
    }
    
    IFace* ItemFromIdx(size_t idx)
    {
        return m_items[idx].first;
    }

    template<class T>
    void IncrementItemIdx(T ptr_to_member)
    {
        void *p = reinterpret_cast_ex<void*>(ptr_to_member);
        FncPos &fnc = m_nCurrentItem[p];

        if (!m_bRatioDependend)
        {
            
            fnc.nRepeaIdx = (fnc.nRepeaIdx + 1) % m_items[fnc.nObjIdx].second;

            if (!fnc.nRepeaIdx)
            {
                fnc.nObjIdx = (fnc.nObjIdx + 1) % m_items.size();
            }
            return;
        }

        //build layers that are accepted with current frame order
        //1st stage calc frequencies vector
        if (m_freq.empty())
        {
            for(size_t i = 0; i  < m_items.size(); i++)
            {
                m_freq.push_back(m_items.back().second / m_items[i].second);
            }
        }

        //2nd stage find next suitable element by its frequency for current frameorder
        bool bFound = false;
        for(size_t i = fnc.nObjIdx + 1; i < m_items.size(); i++)
        {
            if (0 == (fnc.nRepeaIdx % m_freq[i]))
            {
                fnc.nObjIdx = i;
                bFound = true;
                break;
            }
        }

        //current layer completed need to increase frameorder
        if (!bFound)
        {
            //move to next frameorder
            fnc.nRepeaIdx++;

            //restarting search
            for(size_t i = 0; i < m_items.size(); i++)
            {
                if (0 == (fnc.nRepeaIdx % m_freq[i]))
                {
                    fnc.nObjIdx = i;
                    break;
                }
            }
        }
    }

    template<class T>
    void ForEach(T functor)
    {
        std::for_each(m_items.begin(), m_items.end(), first_of(functor));
    }

    
    //second in pair is number of times to repeat each function
    typedef std::vector<std::pair < mfx_shared_ptr<IFace>, size_t> > Items_t;
    Items_t m_items;
    
    //frequencies vector is calculated directly from number of repetitions
    std::vector<size_t> m_freq;

    //order of interfaces invocation
    bool m_bRatioDependend;

    //index in m_items array for each operation pointer, in term of pointer to member 
    struct FncPos
    {
        size_t nObjIdx;//position in object array
        size_t nRepeaIdx;//times repeated, or globalFrameOrder
        
        FncPos()
            : nObjIdx()
            , nRepeaIdx()
        {
        }
    };

    //used in non frequency related puling
    std::map<void*, FncPos> m_nCurrentItem;
};

