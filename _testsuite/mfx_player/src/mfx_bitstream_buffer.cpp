/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2017 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_bitsream_buffer.h"
#include "umc_automatic_mutex.h"
#include "mfxpcp.h"

using namespace UMC;

MFXBistreamBuffer::MFXBistreamBuffer()
    : mfxBitstream2()
    , m_bDonotUseLinear()
    , m_bStartedBuffering()
    , m_bEnable()
    , m_bEos()
{
}

MFXBistreamBuffer::~MFXBistreamBuffer()
{
    Close();
}

mfxStatus MFXBistreamBuffer::Init( mfxU32 nBufferSizeMin
                                 , mfxU32 nBufferSizeMax)
{
    m_bEos = false;
    //Close();
    MFX_CHECK_WITH_ERR(nBufferSizeMin > 4 || nBufferSizeMin == 0, MFX_ERR_UNKNOWN);
    if (nBufferSizeMax <= nBufferSizeMin)
    {
        nBufferSizeMax += nBufferSizeMin;//at least min + 1 frame
    }
    MFX_CHECK_WITH_ERR(nBufferSizeMax > nBufferSizeMin && nBufferSizeMax > 4, MFX_ERR_UNKNOWN);

    m_bDonotUseLinear = nBufferSizeMin == 0;
    m_nBufferSizeMin  = IPP_MAX(nBufferSizeMin, 1);//0 size doesn't make sense
    m_nBufferSizeMax  = nBufferSizeMax;

    ExtendBs((mfxU32)nBufferSizeMax, this);

    if (!m_bDonotUseLinear)
    {
        MediaBufferParams mbParams;
        mbParams.m_numberOfFrames       = 1;
        mbParams.m_prefInputBufferSize  = m_nBufferSizeMax;
        mbParams.m_prefOutputBufferSize = m_nBufferSizeMin;
        
        MFX_CHECK_UMC_STS(m_UMCBuffer.Init(&mbParams));
    }
    
    return MFX_ERR_NONE;
}

mfxStatus MFXBistreamBuffer::SetMinBuffSize(mfxU32 nBufferSizeMin)
{
    if ((nBufferSizeMin <= 4 && nBufferSizeMin > 1) || nBufferSizeMin > m_nBufferSizeMax)
    {
        return MFX_ERR_UNKNOWN;
    }
    m_nBufferSizeMin = nBufferSizeMin;
    return MFX_ERR_NONE;
}

mfxStatus MFXBistreamBuffer::GetMinBuffSize(mfxU32 &nBufferSizeMin)
{
    nBufferSizeMin = m_nBufferSizeMin;
    return MFX_ERR_NONE;
}

mfxStatus MFXBistreamBuffer::Reset()
{
    this->DataLength = 0;

    if (!m_bDonotUseLinear)
    {
        m_UMCBuffer.UnLockInputBuffer(NULL, UMC_ERR_END_OF_STREAM);
        mfxBitstream2 bs;
        if (UMC_OK == static_cast<int>(LockOutput(&bs)))
        {
            bs.DataLength = 0;
            UnLockOutput(&bs);
        }
        m_UMCBuffer.Reset();
    }
    
    return MFX_ERR_NONE;
}

mfxStatus MFXBistreamBuffer::Close()
{
    m_UMCBuffer.Close();
    m_bEos = false;
    
    MFX_DELETE_ARRAY(this->Data);
    mfxBitstream2_ZERO_MEM(*(mfxBitstream2*)this);
    
    return MFX_ERR_NONE;
}

mfxStatus MFXBistreamBuffer::ExtendBs(mfxU32 nNewLen, mfxBitstream *src)
{
    if (NULL == src)
        return MFX_ERR_NONE;

    if (nNewLen <= src->MaxLength)
    {
        return MFX_ERR_NONE;
    }
    
    nNewLen = mfx_align(nNewLen , 1024 * 1024);

    mfxU8 * p;
    MFX_CHECK_WITH_ERR(p = new mfxU8[nNewLen], MFX_ERR_MEMORY_ALLOC);

    src->MaxLength = nNewLen;
    memcpy(p, src->Data + src->DataOffset, src->DataLength);
    delete [] src->Data;
    src->Data = p;
    src->DataOffset = 0;
    
    return MFX_ERR_NONE;
}

mfxStatus MFXBistreamBuffer::MoveBsExtended(mfxBitstream2 *dest, mfxBitstream2 *src)
{
    MFX_CHECK_STS(CopyBsExtended(dest, src));
    
    if (NULL != src)
    {
        src->DataLength = 0;
        mfxEncryptedData *cur = src->EncryptedData;
        while (NULL != cur)
        {
            cur->DataLength = 0;
            cur = cur->Next;
        }
    }
    
    return MFX_ERR_NONE;
}

mfxStatus MFXBistreamBuffer::CopyBsExtended(mfxBitstream2 *dest, mfxBitstream2 *src)
{
    if (NULL == dest || NULL == src) 
    {
        return MFX_ERR_NONE;
    }

    if (NULL != dest->EncryptedData) 
    {
        // mfxBitstream::EncryptedData do not support mutli frame combinating. 
        // Not sure if it is safe to extend it so just exit with error.
        if (0 != dest->DataLength)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    
    //extending destination if necessary
    MFX_CHECK_STS(ExtendBs(dest->DataLength + src->DataLength, dest));

    //copying params : timestamp, frametype, etc
    dest->FrameType  = src->FrameType;
    dest->PicStruct  = src->PicStruct;
    dest->TimeStamp  = src->TimeStamp;
    dest->DependencyId   = src->DependencyId;

    //if there is a room we will copy to end
    mfxU32 nCanCopy = dest->MaxLength - dest->DataLength - dest->DataOffset;

    if (nCanCopy < src->DataLength)
    {
        //moving of data
        memmove(dest->Data,  dest->Data + dest->DataOffset, dest->DataLength);
        dest->DataOffset = 0;
    }

    memcpy(dest->Data + dest->DataLength + dest->DataOffset, src->Data + src->DataOffset, src->DataLength);
    dest->DataLength += src->DataLength; 
    
    if (NULL != src->EncryptedData)
    {
        mfxU32 size = 0;
        mfxU32 count = 0;
        mfxEncryptedData* cur = src->EncryptedData;
        while (NULL != cur)
        {
            size += cur->MaxLength; // asomsiko todo: we should be smart to understand slices comes in continuous buffers (one per field).
            count++;
            cur = cur->Next;
        }

        dest->m_enryptedData.resize(count);
        dest->m_enryptedDataBuffer.resize(size);

        mfxU8 *buf = &dest->m_enryptedDataBuffer[0];
        count = 0;
        cur = src->EncryptedData;
        mfxEncryptedData** curDest = &(dest->EncryptedData);
        while (NULL != cur)
        {
            *curDest = &(dest->m_enryptedData[count++]);
            **curDest = *cur;
            (*curDest)->Data = buf;
            buf += cur->MaxLength;
            memcpy((*curDest)->Data, cur->Data, cur->MaxLength);
            cur = cur->Next;
            curDest = &((*curDest)->Next);
            *curDest = NULL;
        }
    }
    else
        dest->EncryptedData = NULL;

    return MFX_ERR_NONE;
}

mfxStatus MFXBistreamBuffer::PutBuffer(bool bEos)
{
    if (!m_bEos)
        m_bEos = bEos;

//dump splitted file
#if 0
    if (NULL != pBs)
    {
        static int i = 0;
        FILE * pFd;
        if (i==0)
        {
            i++;
            pFd = fopen("c:\\x1.264", "wb");
        }else
        {
            pFd = fopen("c:\\x1.264", "ab");
        }
        fwrite(pBs->Data + pBs->DataOffset, 1, pBs->DataLength, pFd);
        fclose(pFd);
    }
#endif
    if (m_bDonotUseLinear)
    {
        return MFX_ERR_NONE;
    }

    if (bEos)
    {
        m_UMCBuffer.UnLockInputBuffer(NULL, UMC_ERR_END_OF_STREAM);
        return MFX_ERR_NONE;
    }

    mfxBitstream2 * pBs = this;

    if (pBs->DataLength == 0)
    {
        return MFX_ERR_NONE;
    }

    MediaData mData;
    MFX_CHECK_UMC_STS(m_UMCBuffer.LockInputBuffer(&mData));

    if (pBs->DataLength > mData.GetBufferSize())
    {
        VM_ASSERT(pBs->DataLength > mData.GetBufferSize());//shouldn't happen
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    }

    memcpy( mData.GetDataPointer()
          , pBs->Data + pBs->DataOffset
          , pBs->DataLength );

    mData.SetDataSize(pBs->DataLength);
    pBs->DataLength = 0;

    m_UMCBuffer.UnLockInputBuffer(&mData);

    return MFX_ERR_NONE;
}

mfxStatus MFXBistreamBuffer::LockOutput(mfxBitstream2 * pBs)
{
    MFX_CHECK_POINTER(pBs);

    if (m_bDonotUseLinear)
    {
        *pBs = *this;

        return 0 == this->DataLength ? MFX_ERR_MORE_DATA : MFX_ERR_NONE;
    }

    MediaData mData;

    if (UMC_OK != m_UMCBuffer.LockOutputBuffer(&mData))
        return MFX_ERR_MORE_DATA;

    if (!m_bEos)
    {
        if (mData.GetDataSize() < m_nBufferSizeMin)
        {
            m_UMCBuffer.UnLockOutputBuffer(&mData);
            return MFX_ERR_MORE_DATA;
        }
    }

   *pBs = *this;
    pBs->Data = (mfxU8*)mData.GetDataPointer();
        //data length is limited only if buffer inited with non zero size
    pBs->DataLength = (mfxU32)IPP_MIN( mData.GetDataSize()
                                     , m_nBufferSizeMin == 1 ? mData.GetDataSize() : m_nBufferSizeMin);    
    pBs->DataOffset = 0;
    pBs->MaxLength  = (mfxU32)mData.GetDataSize();
    
    return MFX_ERR_NONE;
}

mfxStatus MFXBistreamBuffer::UnLockOutput(mfxBitstream2 * pBs)
{
    MFX_CHECK_POINTER(pBs);

    if (m_bDonotUseLinear)
    {
        this->DataLength = pBs->DataLength;
        this->DataOffset = pBs->DataOffset;
        return MFX_ERR_NONE;
    }

    MediaData mData;
    mData.SetBufferPointer(pBs->Data, pBs->MaxLength);
    mData.SetDataSize(pBs->MaxLength);
    mData.MoveDataPointer(pBs->DataOffset);
    m_UMCBuffer.UnLockOutputBuffer(&mData);

    return MFX_ERR_NONE;
}

mfxStatus MFXBistreamBuffer::UndoInputBS()
{
    if (m_bDonotUseLinear)
        return MFX_ERR_NONE;

    mfxStatus sts;
    mfxBitstream2 inputBs;

    mfxU32 nminSize;
    GetMinBuffSize(nminSize);

    PutBuffer(true);
    MFX_CHECK_STS(SetMinBuffSize(1));
    MFX_CHECK_STS_SKIP(sts = LockOutput(&inputBs), MFX_ERR_MORE_DATA);
    if (MFX_ERR_MORE_DATA == sts)//still don't have enough data in buffer
    {
        SetMinBuffSize(nminSize);
        return MFX_ERR_MORE_DATA;
    }

    MFX_CHECK_STS(MFXBistreamBuffer::MoveBsExtended(this, &inputBs));
    MFX_CHECK_STS(UnLockOutput(&inputBs));
    MFX_CHECK_STS(SetMinBuffSize(nminSize));

    return MFX_ERR_NONE;
}