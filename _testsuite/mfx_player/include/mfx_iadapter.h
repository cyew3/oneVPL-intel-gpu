/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

//intend for interfaces adoptation
template <class TFrom, class TTo>
class AdapterBase
    : public TFrom
{
public:
    typedef TFrom base;
    //traiting of adapter
    typedef TFrom result_type;

    AdapterBase(TTo* pTo)
        : m_pTo(pTo)
    {}
    
    virtual ~AdapterBase(){}

protected:
    std::auto_ptr<TTo> m_pTo;
};

//definition of adapter
template <class TFrom, class TTo>
class Adapter
    : public AdapterBase<TFrom, TTo>
{
public:
    Adapter(TTo* pTo)
        : AdapterBase<TFrom, TTo>(pTo)
    {}
};
