/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_HEADERS_H
#define __UMC_H265_HEADERS_H

#include "umc_h265_dec_defs.h"
#include "umc_media_data_ex.h"
#include "umc_h265_heap.h"

namespace UMC_HEVC_DECODER
{

// Headers container class
template <typename T>
class HeaderSet
{
public:

    HeaderSet(Heap_Objects  *pObjHeap)
        : m_pObjHeap(pObjHeap)
        , m_currentID(-1)
    {
    }

    virtual ~HeaderSet()
    {
        Reset(false);
    }

    T * AddHeader(T* hdr)
    {
        Ipp32u id = hdr->GetID();

        if (id >= m_Header.size())
        {
            m_Header.resize(id + 1);
        }

        m_currentID = id;

        if (m_Header[id])
        {
            m_Header[id]->DecrementReference();
        }

        T * header = m_pObjHeap->AllocateObject<T>();
        *header = *hdr;

        //ref. counter may not be 0 here since it can be copied from given [hdr] object
        header->ResetRefCounter();
        header->IncrementReference();

        m_Header[id] = header;
        return header;
    }

    T * GetHeader(Ipp32s id)
    {
        if ((Ipp32u)id >= m_Header.size())
        {
            return 0;
        }

        return m_Header[id];
    }

    const T * GetHeader(Ipp32s id) const
    {
        if ((Ipp32u)id >= m_Header.size())
        {
            return 0;
        }

        return m_Header[id];
    }

    void RemoveHeader(void * hdr)
    {
        T * tmp = (T *)hdr;
        if (!tmp)
        {
            VM_ASSERT(false);
            return;
        }

        Ipp32u id = tmp->GetID();

        if (id >= m_Header.size())
        {
            VM_ASSERT(false);
            return;
        }

        if (!m_Header[id])
        {
            VM_ASSERT(false);
            return;
        }

        VM_ASSERT(m_Header[id] == hdr);
        m_Header[id]->DecrementReference();
        m_Header[id] = 0;
    }

    void Reset(bool isPartialReset = false)
    {
        if (!isPartialReset)
        {
            for (Ipp32u i = 0; i < m_Header.size(); i++)
            {
                m_pObjHeap->FreeObject(m_Header[i]);
            }

            m_Header.clear();
            m_currentID = -1;
        }
    }

    void SetCurrentID(Ipp32s id)
    {
        if (GetHeader(id))
            m_currentID = id;
    }

    Ipp32s GetCurrentID() const
    {
        return m_currentID;
    }

    T * GetCurrentHeader()
    {
        if (m_currentID == -1)
            return 0;

        return GetHeader(m_currentID);
    }

    const T * GetCurrentHeader() const
    {
        if (m_currentID == -1)
            return 0;

        return GetHeader(m_currentID);
    }

private:
    std::vector<T*>           m_Header;
    Heap_Objects             *m_pObjHeap;

    Ipp32s                    m_currentID;
};

/****************************************************************************************************/
// Headers stuff
/****************************************************************************************************/
class Headers
{
public:

    Headers(Heap_Objects  *pObjHeap)
        : m_VideoParams(pObjHeap)
        , m_SeqParams(pObjHeap)
        , m_PicParams(pObjHeap)
        , m_SEIParams(pObjHeap)
        , m_pObjHeap(pObjHeap)
    {
    }

    void Reset(bool isPartialReset = false)
    {
        m_SeqParams.Reset(isPartialReset);
        m_PicParams.Reset(isPartialReset);
        m_SEIParams.Reset(isPartialReset);
        m_VideoParams.Reset(isPartialReset);
    }

    HeaderSet<H265VideoParamSet>           m_VideoParams;
    HeaderSet<H265SeqParamSet>             m_SeqParams;
    HeaderSet<H265PicParamSet>             m_PicParams;
    HeaderSet<H265SEIPayLoad>              m_SEIParams;

private:
    Heap_Objects  *m_pObjHeap;
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_HEADERS_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
