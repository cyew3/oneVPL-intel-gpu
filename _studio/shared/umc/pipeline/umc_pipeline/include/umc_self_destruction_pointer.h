//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_SELF_DESTRUCTION_POINTER_H__
#define __UMC_SELF_DESTRUCTION_POINTER_H__

namespace UMC
{

template <class T>
class SelfDestructionPointer
{
public:
    // Default constructor
    SelfDestructionPointer(T *lpItem = 0)
    {
        m_lpItem = lpItem;
    };

    // Destructor
    virtual ~SelfDestructionPointer(void)
    {
        Release();
    };

    SelfDestructionPointer & operator = (T *lpItem)
    {
        if (lpItem != m_lpItem)
            Release();

        m_lpItem = lpItem;

        return *this;
    };

    SelfDestructionPointer & operator = (SelfDestructionPointer &Item)
    {
        if (Item.m_lpItem != m_lpItem)
            Release();

        m_lpItem = Item.m_lpItem;
        Item.m_lpItem = 0;

        return *this;
    };

    operator T * (void)
    {
        return m_lpItem;
    };

    T & operator * (void)
    {
        return *m_lpItem;
    };

    bool operator == (T *lpItem)
    {
        return (lpItem == m_lpItem);
    };

    bool operator == (SelfDestructionPointer &Item)
    {
        return (Item.m_lpItem == m_lpItem);
    };

    // extract guarded pointer
    T *Extract(void)
    {
        T *lpReturn = m_lpItem;

        m_lpItem = NULL;
        return lpReturn;
    };

protected:
    void Release(void)
    {
        if (m_lpItem)
            delete m_lpItem;

        m_lpItem = 0;
    };

    T *m_lpItem;
};

// comparing operator(s)
template <class T>
bool operator == (T *lpItem, SelfDestructionPointer<T> &Pointer)
{
    return Pointer.opertator == (lpItem);
}
template <class T>
bool operator == (void* pvValue, SelfDestructionPointer<T> &Pointer)
{
    return Pointer.operator == (reinterpret_cast<T *> (pvValue));
}

template <class T>
class SelfDestructionObject : public SelfDestructionPointer<T>
{
public:
    T * operator -> (void)
    {
        return this->m_lpItem;
    };

    SelfDestructionObject & operator = (T *lpItem)
    {
        return static_cast<SelfDestructionObject &> (SelfDestructionPointer<T>::operator =(lpItem));
    };

    SelfDestructionObject & operator = (SelfDestructionObject &Item)
    {
        return static_cast<SelfDestructionObject &> (SelfDestructionPointer<T>::operator =(Item));
    };
};

} // end namespace UMC

#endif // __UMC_SELF_DESTRUCTION_POINTER_H__
