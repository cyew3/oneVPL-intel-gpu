// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
