/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include <string>
#include <list>
#include <map>
#include "test_single_tone.h"
#include "test_object_params.h"

class TestParamsRegistry
    : public TestFrmwkNTSSingleton<TestParamsRegistry>
{
    std::map<std::string, std::list<BaseHolder*> > m_store;

public:
    void SetParams(const std::string & key, BaseHolder * pParams)
    {
        m_store[key].push_back(pParams);
    }
    bool GetParams(const std::string & key, int nCallNumber, BaseHolder ** ppParams)
    {
        std::map<std::string, std::list<BaseHolder*> >::iterator it;
        if (m_store.end() == (it = m_store.find(key)))
            return false;

        if (ppParams)
        {
            *ppParams = NULL;
        }

        if (nCallNumber < (int)it->second.size())
        {
            std::list<BaseHolder*>::iterator it2 = it->second.begin();
            for (int i = 0; i < nCallNumber; i++)it2++;

            if (ppParams)
            {
                *ppParams = *it2;
            }

            return true;
        }

        return false;
    }
    void clear()
    {
        m_store.clear();
    }
};

template <class TReturn, class T0>
void RegisterCallEvent(std::string key, TReturn ret,  T0 value0)
{
    ParameterHolder1<TReturn, T0> *H0 = new ParameterHolder1<TReturn, T0>();
    H0->ret_val = ret;
    H0->value0 = value0;
    TestParamsRegistry::Instance().SetParams(key, H0);
}

template <class TReturn, class T0, class T1>
void RegisterCallEvent(std::string key, TReturn ret,  T0 value0, T1 value1)
{
    ParameterHolder2<TReturn, T0, T1> *H0 = new ParameterHolder2<TReturn, T0, T1>();
    H0->ret_val = ret;
    H0->value0 = value0;
    H0->value1 = value1;
    TestParamsRegistry::Instance().SetParams(key, H0);
}

template <class TReturn, class T0, class T1, class T2>
void RegisterCallEvent(std::string key, TReturn ret,  T0 value0, T1 value1, T2 value2)
{
    ParameterHolder3<TReturn, T0, T1, T2> *H0 = new ParameterHolder3<TReturn, T0, T1, T2>();
    H0->ret_val = ret;
    H0->value0 = value0;
    H0->value1 = value1;
    H0->value2 = value2;
    TestParamsRegistry::Instance().SetParams(key, H0);
};
