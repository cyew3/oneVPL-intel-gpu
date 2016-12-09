/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include <stddef.h>
#include <stdexcept> /* for std exceptions on Linux/Android */

#include "mfx_pipeline_defs.h"
#include "mfx_extended_buffer.h"

MFXExtBufferVector::MFXExtBufferVector(mfxExtBuffer**ppBuffers , mfxU16 nBuffers)
: m_bOwnBuffers(true)
{
    if (0 != nBuffers && ppBuffers)
    {
        m_bOwnBuffers = false;
        for (int i = 0 ; i < nBuffers;i++)
        {
            push_back(ppBuffers[i]);
        }
    }
}

MFXExtBufferVector::MFXExtBufferVector(mfxVideoParam & vParam)
: m_bOwnBuffers()
{
    if (0 != vParam.NumExtParam && vParam.ExtParam)
    {
        m_bOwnBuffers = false;
        for (int i = 0 ; i < vParam.NumExtParam;i++)
        {
            push_back(vParam.ExtParam[i]);
        }
    }
}

void MFXExtBufferVector::push_back(const mfxExtBuffer& pBuffer)
{
    //TODO: weird design
    std::auto_ptr<mfxExtBuffer> pTmp ((mfxExtBuffer*) new char[pBuffer.BufferSz]);
    memcpy(pTmp.get(), &pBuffer, pBuffer.BufferSz);

    std::auto_ptr<MFXExtBufferPtrBase> pBuf (
        MFXExtBufferFactory::Instance().CreateBuffer(pBuffer.BufferId));

    if (!pBuf.get())
    {
        char * sBuffer = (char*)&pBuffer.BufferId;
        std::stringstream stream;
        stream<<"Unknown Buffer ID : "<<sBuffer[0]<<sBuffer[1]<<sBuffer[2]<<sBuffer[3];
        throw std::runtime_error(stream.str().c_str());
    }

    pBuf->reset(pTmp.release());

    //cloning buffer with attached vectors
    push_back(pBuf.release());
}

void MFXExtBufferVector::push_back(mfxExtBuffer* pBuffer)
{
    if (NULL == pBuffer)
        return;

    std::auto_ptr<MFXExtBufferPtrBase> pBuf (
        MFXExtBufferFactory::Instance().CreateBuffer(pBuffer->BufferId));

    if (!pBuf.get())
    {
        char * sBuffer = (char*)&pBuffer->BufferId;
        std::stringstream stream;
        stream<<"Unknown Buffer ID : "<<sBuffer[0]<<sBuffer[1]<<sBuffer[2]<<sBuffer[3];
        throw std::runtime_error(stream.str());
    }

    pBuf->reset(pBuffer);

    push_back(pBuf.get());
    pBuf->release();
}

void MFXExtBufferVector::push_back(MFXExtBufferPtrBase * pBuffer)
{
    if (NULL == pBuffer)
        return ;
    
    push_back(*pBuffer);
}

void MFXExtBufferVector::push_back(MFXExtBufferPtrBase & pBuffer)
{
    if (NULL == pBuffer.get())
        return;

    iterator it =
        std::find_if(begin(),end(),bind2nd(MFXExtBufferCompareByID<mfxExtBuffer>(), pBuffer->BufferId));

    //dont need to populate buffer
    if (it != end())
        return;

    std::auto_ptr<MFXExtBufferPtrBase> ptr (pBuffer.Clone());

    std::vector<mfxExtBuffer*>::push_back(ptr->get());
    m_buffers.push_back(ptr.release());
}

MFXExtBufferPtrBase* MFXExtBufferVector::get_by_id(mfxU32 nId)const
{
    std::list<MFXExtBufferPtrBase*>::const_iterator it =
        std::find_if(m_buffers.begin(), m_buffers.end(), bind2nd(MFXExtBufferCompareByID<MFXExtBufferPtrBase>(), nId));

    if (m_buffers.end() == it)
    {
        return NULL;
    }

    ///TODO: points of restoration should be extended of course, it is just for current version
    (*it)->restore_consistency();

    return *it;
}

mfxExtBuffer ** MFXExtBufferVector::operator &()
{
    if (empty())
        return NULL;

    return &front();
}

MFXExtBufferVector::~MFXExtBufferVector()
{
    clear();
}

void MFXExtBufferVector::clear()
{
    if (m_bOwnBuffers){
        //for_each(begin(), end(), deleter<mfxExtBuffer*>());
        for_each(m_buffers.begin(), m_buffers.end(), deleter<MFXExtBufferPtrBase*>());
    }
    std::vector<mfxExtBuffer*>::clear();
    m_buffers.clear();
}
//////////////////////////////////////////////////////////////////////////
 MFXExtBufferPtrRef<mfxExtVPPDoNotUse> MFXExtBufferPtr<mfxExtVPPDoNotUse> :: operator -> ()
{
    return  MFXExtBufferPtrRef<mfxExtVPPDoNotUse>(this);
}

 MFXExtBufferPtrRef<mfxExtVPPDoNotUse> MFXExtBufferPtr<mfxExtVPPDoNotUse> :: operator *  ()
{
    return  MFXExtBufferPtrRef<mfxExtVPPDoNotUse>(this);
}
 
 MFXExtBufferPtrRef<mfxExtVPPDoUse> MFXExtBufferPtr<mfxExtVPPDoUse> :: operator -> ()
 {
     return  MFXExtBufferPtrRef<mfxExtVPPDoUse>(this);
 }

 MFXExtBufferPtrRef<mfxExtVPPDoUse> MFXExtBufferPtr<mfxExtVPPDoUse> :: operator *  ()
 {
     return  MFXExtBufferPtrRef<mfxExtVPPDoUse>(this);
 }


MFXExtBufferPtrRef<mfxExtMVCSeqDesc> MFXExtBufferPtr<mfxExtMVCSeqDesc> :: operator -> ()
{
    return MFXExtBufferPtrRef<mfxExtMVCSeqDesc>(this);
}

MFXExtBufferPtrRef<mfxExtMVCSeqDesc> MFXExtBufferPtr<mfxExtMVCSeqDesc> :: operator *  ()
{
    return MFXExtBufferPtrRef<mfxExtMVCSeqDesc>(this);
}

void MFXExtBufferPtr<mfxExtMVCSeqDesc> :: reset(element_type_ext * pBuffer)
{
    reset(reinterpret_cast<element_type*>(pBuffer));
}

void MFXExtBufferPtr<mfxExtMVCSeqDesc> :: copy_local(element_type *pBuffer)
{
    m_OP.clear();
    m_View.clear();
    m_ViewId.clear();

    if (NULL == pBuffer)
        return;

    mfxExtMVCSeqDesc *pBufferCasted = reinterpret_cast<mfxExtMVCSeqDesc*>(pBuffer);

    //copying target views offseted pointers
    std::vector<ptrdiff_t> targetVidewOffsets;
    for (size_t i = 0; i < m_pParticular->NumOP; i++)
    {
        targetVidewOffsets.push_back(pBufferCasted->OP[i].TargetViewId - pBufferCasted->ViewId);
    }

    if (m_pParticular->NumViewAlloc>= m_pParticular->NumView && m_pParticular->NumViewAlloc != 0)
    {
        m_View.reserve(m_pParticular->NumViewAlloc);
        m_View.insert(m_View.end(), pBufferCasted->View, pBufferCasted->View + m_pParticular->NumView);
        //at least one element to get buffer pointer
        m_pParticular->View = vector_front(m_View);
    }
    if (m_pParticular->NumViewIdAlloc >= m_pParticular->NumViewId && m_pParticular->NumViewIdAlloc != 0)
    {
        m_ViewId.reserve(m_pParticular->NumViewIdAlloc);
        m_ViewId.insert(m_ViewId.end(), pBufferCasted->ViewId, pBufferCasted->ViewId + m_pParticular->NumViewId);
        m_pParticular->ViewId = vector_front(m_ViewId);
    }
    if (m_pParticular->NumOPAlloc >= m_pParticular->NumOP && m_pParticular->NumOPAlloc != 0)
    {
        m_OP.reserve(m_pParticular->NumOPAlloc);
        m_OP.insert(m_OP.end(), pBufferCasted->OP, pBufferCasted->OP + m_pParticular->NumOP);
        
        //restoring offsets
        for (size_t i = 0; i < m_OP.size(); i++)
        {
            m_OP[i].TargetViewId = m_pParticular->ViewId + targetVidewOffsets[i];
        }

        m_pParticular->OP = vector_front(m_OP);
    }
}

void MFXExtBufferPtr<mfxExtMVCSeqDesc> :: reset(element_type * pBuffer)
{
    _MFXExtBufferPtr<mfxExtMVCSeqDesc>::reset(pBuffer);
    copy_local(pBuffer);
}

void MFXExtBufferPtr<mfxExtMVCSeqDesc> :: restore_consistency()
{
    //reestablishing buffer pointers and copying data from pbuffer
    if (m_pParticular->NumViewAlloc >= m_pParticular->NumView && m_pParticular->NumViewAlloc != 0)
    {
        int nItems_to_insert = (int)(m_pParticular->NumView - m_View->size());
        m_View.insert(m_View.end()
            , m_pParticular->View + m_View->size()
            , m_pParticular->View + m_View->size() + nItems_to_insert);
    }
    if (m_pParticular->NumViewIdAlloc >= m_pParticular->NumViewId && m_pParticular->NumViewIdAlloc != 0)
    {
        int nItems_to_insert = (int)(m_pParticular->NumViewId - m_ViewId->size());
        m_ViewId.insert(m_ViewId.end()
            , m_pParticular->ViewId + m_ViewId->size()
            , m_pParticular->ViewId + m_ViewId->size() + nItems_to_insert);
    }
    if (m_pParticular->NumOPAlloc >= m_pParticular->NumOP && m_pParticular->NumOPAlloc != 0)
    {
        int nItems_to_insert = (int)(m_pParticular->NumOP - m_OP->size());
        m_OP.insert(m_OP.end()
            , m_pParticular->OP + m_OP->size()
            , m_pParticular->OP + m_OP->size() + nItems_to_insert);
    }
}


//////////////////////////////////////////////////////////////////////////



