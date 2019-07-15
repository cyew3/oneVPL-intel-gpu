/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once
#include <memory>

//class for automatically proxy generation for particular interface
//only several mfx_player's custom interfaces supported
template<class TIface>
class InterfaceProxy
{
private:
    InterfaceProxy(){}//class cannot be created as well as child class, to prevent cases with not defined proxyies
};


template<class TIface>
class InterfaceProxyBase
    : public TIface
{
public:
    //trait necessary when interfaces proxy used together with adapters
    typedef TIface result_type;

    InterfaceProxyBase(TIface *pTarget)
        : m_pTarget (pTarget)
    {
    }
    InterfaceProxyBase(std::unique_ptr<TIface> &&pTarget)
        : m_pTarget (std::move(pTarget))
    {
    }
    virtual ~InterfaceProxyBase(){}

protected:
    TIface &Target(){return *m_pTarget.get();}
    std::unique_ptr<TIface> m_pTarget;
};

//rationale : interface should be derived from that base to enable automatic proxy binding, especially if there several implemntations
//i.e. class B : public InterfaceA
//i.e. class C : public InterfaceA
//InterfaceA *proxy = CreateProxy<B>();
//InterfaceA *proxy = CreateProxy<C>();
//Then CreateProxy template function can create a correct new InterfaceProxy<InterfaceA>(new B), or new C respectively
//by using trait : InterfaceProxy<EnableProxyForThis<T>::InterfaceType>(new T)

template <class T>
struct EnableProxyForThis
{
    typedef T InterfaceType;
    virtual ~EnableProxyForThis(){}
};
