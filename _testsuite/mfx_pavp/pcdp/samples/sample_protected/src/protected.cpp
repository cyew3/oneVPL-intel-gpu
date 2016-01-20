//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2013 Intel Corporation. All Rights Reserved.
//
//
//*/
#include "protected.h"
#include <iostream>
using namespace std;

#include "avc_spl.h"
#include "vc1_spl.h"
#include "mpeg2_spl.h"

#include "intel_pavp_core_api.h"


namespace ProtectedLibrary
{


mfxStatus CopyBitstream2(mfxBitstream *dest, mfxBitstream *src)
{
    if (!dest || !src)
        return MFX_ERR_NULL_PTR;

    if (!dest->DataLength)
    {
        dest->DataOffset = 0;
    }
    else
    {
        memmove(dest->Data, dest->Data + dest->DataOffset, dest->DataLength);
        dest->DataOffset = 0;
    }

    if (src->DataLength > dest->MaxLength - dest->DataLength - dest->DataOffset)
        return MFX_ERR_NOT_ENOUGH_BUFFER;

    MSDK_MEMCPY_BITSTREAM(*dest, dest->DataOffset, src->Data, src->DataLength);
    dest->DataLength = src->DataLength;

    dest->DataFlag = src->DataFlag;

    //common Extended buffer will be for src and dest bit streams
    dest->EncryptedData = src->EncryptedData;

    return MFX_ERR_NONE;
}

CProtectedSmplBitstreamReader::CProtectedSmplBitstreamReader(
    const pcpEncryptor& encryptor,
    mfxU32 codecID,
    bool isFullEncrypt,
    bool sparse)
{
    m_codecID = codecID;
    m_isSparse = sparse;
    m_sessionPCP = NULL;
    m_encryptor = encryptor;
    m_isFullEncrypt = isFullEncrypt;
}

CProtectedSmplBitstreamReader::CProtectedSmplBitstreamReader(
    const CProtectedSmplBitstreamReader &reader)
{
    UNREFERENCED_PARAMETER(reader);
}

CProtectedSmplBitstreamReader& CProtectedSmplBitstreamReader::operator =(
    const CProtectedSmplBitstreamReader &reader)
{
    UNREFERENCED_PARAMETER(reader);
    return *this;
}

CProtectedSmplBitstreamReader::~CProtectedSmplBitstreamReader()
{
    Close();
}

void CProtectedSmplBitstreamReader::Close()
{
    if (NULL != m_originalBS.get())
        m_originalBS.get()->EncryptedData = NULL;
    if (NULL != m_sessionPCP)
    {
        PCPVideoDecode_Close(m_sessionPCP);
        m_sessionPCP = NULL;
    }
    WipeMfxBitstream(m_originalBS.get());
    CSmplBitstreamReader::Close();
}

mfxStatus CProtectedSmplBitstreamReader::Init(const msdk_char *strFileName)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = CSmplBitstreamReader::Init(strFileName);
    if (sts != MFX_ERR_NONE)
        return sts;

    m_isEndOfStream = false;
    m_sessionPCP = NULL;
    m_processedBS = NULL;

    m_originalBS.reset(new mfxBitstream());
    sts = InitMfxBitstream(m_originalBS.get(), 1024*1024);
    if (sts != MFX_ERR_NONE)
        return sts;

    //initialize protected decoding session
    sts = PCPVideoDecode_Init(&m_sessionPCP, m_codecID, &m_encryptor, m_isFullEncrypt);
    if (sts != MFX_ERR_NONE)
        return sts;

    return sts;
}
mfxStatus CProtectedSmplBitstreamReader::ReadNextFrame(mfxBitstream *pBS)
{
    mfxStatus sts = MFX_ERR_NONE;

    //read bit stream from source
    while (!m_originalBS->DataLength)
    {
        sts = CSmplBitstreamReader::ReadNextFrame(m_originalBS.get());
        if (sts != MFX_ERR_NONE && sts != MFX_ERR_MORE_DATA)
            return sts;
        if (sts == MFX_ERR_MORE_DATA)
        {
            m_isEndOfStream = true;
            break;
        }
    }
    do
    {
        sts = PCPVideoDecode_PrepareNextFrame(
            m_sessionPCP,
            m_isEndOfStream ? NULL : m_originalBS.get(),
            &m_processedBS,
            m_isSparse);
        if (sts == MFX_ERR_MORE_DATA)
        {
            if (m_isEndOfStream)
            {
                break;
            }

            sts = CSmplBitstreamReader::ReadNextFrame(m_originalBS.get());
            if (sts == MFX_ERR_MORE_DATA)
                m_isEndOfStream = true;
            continue;
        }
        else
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    } while (MFX_ERR_NONE != sts);

    // get output stream
    if (NULL != m_processedBS)
    {
        mfxStatus copySts = CopyBitstream2(
            pBS,
            m_processedBS);
        if (copySts < MFX_ERR_NONE)
            return copySts;
        m_processedBS = NULL;
    }

    return sts;
}
mfxStatus CProtectedSmplBitstreamWriter::WriteNextFrame(mfxBitstream *pMfxBitstream, bool isPrint)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (NULL != pMfxBitstream->EncryptedData)
    {
        mfxEncryptedData *head = pMfxBitstream->EncryptedData;
        mfxEncryptedData *cur = head;

        while (NULL != cur)
        {
            if (0 != cur->DataLength)
            {
                if (NULL != m_processor.Decrypt)
                {
                    sts = m_processor.Decrypt(
                        m_processor.pthis,
                        cur->Data + cur->DataOffset,
                        cur->Data + cur->DataOffset,
                        (cur->DataLength + 15)&(~0xf),
                        cur->CipherCounter);
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                }
                mfxBitstream bs = { 0, };
                bs.Data = cur->Data + cur->DataOffset;
                bs.DataLength = cur->DataLength;
                sts = CSmplBitstreamWriter::WriteNextFrame(&bs, isPrint);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            cur = cur->Next;
        }
    }
    else
    {
        sts = CSmplBitstreamWriter::WriteNextFrame(pMfxBitstream, isPrint);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    return sts;
}


CSplitterBitstreamReader::CSplitterBitstreamReader(mfxU32 codecID)
    : CSmplBitstreamReader()
    , m_codecID(codecID)
    , m_isEndOfStream(false)
    , m_processedBS(0)
    , m_plainBuffer(0)
    , m_plainBufferSize(0)
{
}

CSplitterBitstreamReader::~CSplitterBitstreamReader()
{
}

void CSplitterBitstreamReader::Close()
{
    WipeMfxBitstream(m_originalBS.get());
    CSmplBitstreamReader::Close();

    if (NULL != m_plainBuffer)
    {
        free(m_plainBuffer);
        m_plainBuffer = NULL;
        m_plainBufferSize = 0;
    }
}

mfxStatus CSplitterBitstreamReader::Init(const msdk_char *strFileName)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = CSmplBitstreamReader::Init(strFileName);
    if (sts != MFX_ERR_NONE)
        return sts;

    m_isEndOfStream = false;
    m_processedBS = NULL;

    m_originalBS.reset(new mfxBitstream());
    sts = InitMfxBitstream(m_originalBS.get(), 1024*1024);
    if (sts != MFX_ERR_NONE)
        return sts;

    if (MFX_CODEC_AVC == m_codecID)
    {
        m_pNALSplitter.reset(new ProtectedLibrary::AVC_Spl());
    }
    else if (MFX_CODEC_MPEG2 == m_codecID)
        m_pNALSplitter.reset(new ProtectedLibrary::MPEG2_Spl());
    else if (MFX_CODEC_VC1 == m_codecID)
        m_pNALSplitter.reset(new ProtectedLibrary::VC1_Spl());

    m_frame = 0;
    m_plainBuffer = 0;
    m_plainBufferSize = 0;

    return sts;
}

mfxStatus CSplitterBitstreamReader::ReadNextFrame(mfxBitstream *pBS)
{
    mfxStatus sts = MFX_ERR_NONE;

    //read bit stream from source
    while (!m_originalBS->DataLength)
    {
        sts = CSmplBitstreamReader::ReadNextFrame(m_originalBS.get());
        if (sts != MFX_ERR_NONE && sts != MFX_ERR_MORE_DATA)
            return sts;
        if (sts == MFX_ERR_MORE_DATA)
        {
            m_isEndOfStream = true;
            break;
        }
    }

    do
    {
        sts = PrepareNextFrame(m_isEndOfStream ? NULL : m_originalBS.get(), &m_processedBS);

        if (sts == MFX_ERR_MORE_DATA)
        {
            if (m_isEndOfStream)
            {
                break;
            }

            sts = CSmplBitstreamReader::ReadNextFrame(m_originalBS.get());
            if (sts == MFX_ERR_MORE_DATA)
                m_isEndOfStream = true;
            continue;
        }
        else if (MFX_ERR_NONE != sts)
            return sts;

    } while (MFX_ERR_NONE != sts);

    // get output stream
    if (NULL != m_processedBS)
    {
        mfxStatus copySts = CopyBitstream2(
            pBS,
            m_processedBS);
        if (copySts < MFX_ERR_NONE)
            return copySts;
        m_processedBS = NULL;
    }

    return sts;
}

mfxStatus CSplitterBitstreamReader::PrepareNextFrame(mfxBitstream *in, mfxBitstream **out)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (NULL == out)
        return MFX_ERR_NULL_PTR;

    *out = NULL;

    // get frame if it is not ready yet
    if (NULL == m_frame)
    {
        sts = m_pNALSplitter->GetFrame(in, &m_frame);
        if (sts != MFX_ERR_NONE)
            return sts;
    }

    if (m_plainBufferSize < m_frame->DataLength)
    {
        if (NULL != m_plainBuffer)
        {
            free(m_plainBuffer);
            m_plainBuffer = NULL;
            m_plainBufferSize = 0;
        }
        m_plainBuffer = (mfxU8*)malloc(m_frame->DataLength);
        if (NULL == m_plainBuffer)
            return MFX_ERR_MEMORY_ALLOC;
        m_plainBufferSize = m_frame->DataLength;
    }

    memcpy_s(m_plainBuffer, m_plainBufferSize, m_frame->Data, m_frame->DataLength);

    memset(&m_outBS, 0, sizeof(mfxBitstream));
    m_outBS.Data = m_plainBuffer;
    m_outBS.DataOffset = 0;
    m_outBS.DataLength = m_frame->DataLength;
    m_outBS.MaxLength = m_frame->DataLength;
    m_outBS.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
    m_outBS.TimeStamp = m_frame->TimeStamp;

    m_pNALSplitter->ResetCurrentState();
    m_frame = NULL;

    *out = &m_outBS;

    return sts;
}


} // namespace ProtectedLibrary