/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once
//serial collection represent collection of arbitrary data passed to array node initializer
//most of them could be implemented as vector however in some cases different references not required

template<class T>
class SerialCollection
    : public std::vector<T>
{
    public:
        SerialCollection()
        {
        }
};
//#if 0
template<class T>
class SerialSingleElement
    : private mfx_no_assign
{
    T m_realElement;
    bool m_bOwnElement;
public:
    SerialSingleElement()
        : m_element(m_realElement)
        , m_bOwnElement(true)
    {
    }
    SerialSingleElement(const T& realElement)
        : m_realElement(realElement)
        , m_element(m_realElement)
        , m_bOwnElement(true)
    {
    }
    SerialSingleElement(T & ref_element, bool /*unused1*/)
        : m_element(ref_element)
        , m_bOwnElement(false)
    {
    }
    SerialSingleElement(const SerialSingleElement & that)
        : m_realElement(that.m_element)//copy
        , m_bOwnElement(that.m_bOwnElement)//copy
        , m_element(m_bOwnElement ? m_realElement : that.m_element)//reference to copied or to original 
    {
    }
    //ref points either o real or to external element
    T & m_element;
};

template <class T>
SerialSingleElement<T> make_serial_single_element( T obj)
{
    return SerialSingleElement<T>(obj);
}

template <class U>
class SerialCollection< SerialSingleElement<U> >
    : private mfx_no_assign
{
    SerialSingleElement<U> m_SingleElement;
public:

    SerialCollection()
    {
    }
    SerialCollection(const SerialSingleElement<U>& single_element)
        : m_SingleElement(single_element)
    {
    }
    SerialCollection(const SerialCollection& that)
        : m_SingleElement(that.m_SingleElement)
    {
    }
    SerialSingleElement<U>& operator [](size_t /*index*/)
    {
        return m_SingleElement;
    }
    SerialSingleElement<U> operator [](size_t /*index*/)const
    {
        return m_SingleElement;
    }
    void resize(size_t /*len*/){}

};

//#endif