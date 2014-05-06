#pragma once

#include "ts_common.h"
#include <vector>
#include <algorithm>

template<class T> struct tsExtBufTypeToId { enum { id = 0 }; };

#define EXTBUF(TYPE, ID) template<> struct tsExtBufTypeToId<TYPE> { enum { id = ID }; };
#include "ts_ext_buffers_decl.h"
#undef EXTBUF

struct tsCmpExtBufById
{
    mfxU32 m_id;

    tsCmpExtBufById(mfxU32 id)
        : m_id(id)
    {
    };

    bool operator () (mfxExtBuffer* b)
    { 
        return  (b && b->BufferId == m_id);
    };
};

template <typename TB> class tsExtBufType : public TB
{
private:
    typedef std::vector<mfxExtBuffer*> EBStorage;
    typedef EBStorage::iterator        EBIterator;

    EBStorage m_buf;

public:
    tsExtBufType()
    {
        TB& base = *this;
        memset(&base, 0, sizeof(TB));
    }

    ~tsExtBufType()
    {
        for(EBIterator it = m_buf.begin(); it != m_buf.end(); it++ )
        {
            delete [] (mfxU8*)(*it);
        }
    }

    void RefreshBuffers()
    {
        NumExtParam = (mfxU32)m_buf.size();
        ExtParam = NumExtParam ? m_buf.data() : 0;
    }

    mfxExtBuffer* GetExtBuffer(mfxU32 id)
    {
        EBIterator it = std::find_if(m_buf.begin(), m_buf.end(), tsCmpExtBufById(id));
        if(it != m_buf.end())
        {
            return *it;
        }
        return 0;
    }

    void AddExtBuffer(mfxU32 id, mfxU32 size)
    {
        if(!size)
        {
             m_buf.push_back(0);
        } else
        {
            m_buf.push_back( (mfxExtBuffer*)new mfxU8[size] );
            mfxExtBuffer& eb = *m_buf.back();

            memset(&eb, 0, size);
            eb.BufferId = id;
            eb.BufferSz = size;
        }

        RefreshBuffers();
    }

    void RemoveExtBuffer(mfxU32 id)
    {
        EBIterator it = std::find_if(m_buf.begin(), m_buf.end(), tsCmpExtBufById(id));
        if(it != m_buf.end())
        {
            delete [] (mfxU8*)(*it);
            m_buf.erase(it);
            RefreshBuffers();
        }
    }

    template <typename T> operator T&()
    {
        mfxU32 id = tsExtBufTypeToId<T>::id;
        mfxExtBuffer * p = GetExtBuffer(id);
        
        if(!p)
        {
            AddExtBuffer(id, sizeof(T));
            p = GetExtBuffer(id);
        }

        return *(T*)p;
    }

    template <typename T> operator T*()
    {
        return (T*)GetExtBuffer(tsExtBufTypeToId<T>::id);
    }
};