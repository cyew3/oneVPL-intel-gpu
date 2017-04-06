/******************************************************************************* *\

Copyright (C) 2016-2017 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

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

    tsExtBufType(const tsExtBufType& that)
    {
        TB& base = *this;
        memset(&base, 0, sizeof(TB));
        *this = that;
    }

    ~tsExtBufType()
    {
        for(EBIterator it = m_buf.begin(); it != m_buf.end(); it++ )
        {
            delete [] (mfxU8*)(*it);
        }
    }

    void ReserveBuffers(mfxU32 n_buffers)
    {
        m_buf.reserve(n_buffers);
    }

    void RefreshBuffers()
    {
        this->NumExtParam = (mfxU32)m_buf.size();
        this->ExtParam    = this->NumExtParam ? m_buf.data() : 0;
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

    void AddExtBuffer(mfxU32 id, mfxU32 size, mfxU32 nbBytesExtraSize = 0)
    {
        if(!size)
        {
             m_buf.push_back(0);
        } else
        {
            EBIterator it = std::find_if(m_buf.begin(), m_buf.end(), tsCmpExtBufById(id));
            if(it == m_buf.end())
            {
                m_buf.push_back( (mfxExtBuffer*)new mfxU8[size+ nbBytesExtraSize] );
                mfxExtBuffer& eb = *m_buf.back();

                memset(&eb, 0, size);
                eb.BufferId = id;
                eb.BufferSz = size;
            }
        }

        RefreshBuffers();
    }

    template <typename T> void AddExtBuffer(mfxU32 nbBytesExtraSize)
    {
        return AddExtBuffer(tsExtBufTypeToId<T>::id, sizeof(T), nbBytesExtraSize);
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

    template <typename T> void RemoveExtBuffer()
    {
        return RemoveExtBuffer(tsExtBufTypeToId<T>::id);
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

    tsExtBufType<TB>& operator=(const tsExtBufType<TB>& other) // copy assignment
    {
        //remove all existing extended buffers
        if(0 != m_buf.size())
        {
            for(EBIterator it = m_buf.begin(); it != m_buf.end(); it++ )
            {
                delete [] (mfxU8*)(*it);
            }
            m_buf.clear();
            RefreshBuffers();
        }
        //copy content of main buffer
        TB& base = *this;
        const TB& other_base = other;
        memcpy(&base, &other_base, sizeof(TB));
        this->NumExtParam = 0;
        this->ExtParam = 0;

        //reproduce list of extended buffers and copy its content
        for(size_t i(0); i < other.NumExtParam; ++i)
        {
            const mfxExtBuffer* those_buffer = other.ExtParam[i];
            if(those_buffer)
            {
                AddExtBuffer(those_buffer->BufferId, those_buffer->BufferSz);
                mfxExtBuffer* this_buffer = GetExtBuffer(those_buffer->BufferId);

                memcpy((void*) this_buffer, (void*) those_buffer, those_buffer->BufferSz);
            }
            else
            {
                AddExtBuffer(0,0);
            }
        }

        return *this;
    }
};

template <typename TB>
inline bool operator==(const tsExtBufType<TB>& lhs, const tsExtBufType<TB>& rhs)
{
    const TB& this_base = lhs;
    const TB& that_base = rhs;
    TB tmp_this = this_base;
    TB tmp_that = that_base;

    tmp_this.ExtParam = 0;
    tmp_that.ExtParam = 0;

    if( memcmp(&tmp_this, &tmp_that, sizeof(TB)) )
        return false; //base structure differes

    //order of ext buffers should not matter
    for(size_t i(0); i < rhs.NumExtParam; ++i)
    {
        const mfxExtBuffer* those_buffer = rhs.ExtParam[i];
        if(those_buffer)
        {
            const mfxExtBuffer* this_buffer = const_cast<tsExtBufType<TB>& >(lhs).GetExtBuffer(those_buffer->BufferId);
            if( memcmp(this_buffer, those_buffer, (std::min)(those_buffer->BufferSz,this_buffer->BufferSz)) )
                return false; //ext buffer structure differes
        }
        else
        {
            //check for nullptr presence
            for(size_t j(0); j < this_base.NumExtParam; ++j)
            {
                if(0 == this_base.ExtParam[j])
                    break;
            }
            //one has nullptr buffer while another not
            return false;
        }
    }

    return true;
}
template <typename TB>
inline bool operator!=(const tsExtBufType<TB>& lhs, const tsExtBufType<TB>& rhs){return !(lhs == rhs);}