/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#ifdef PAVP_BUILD

#include "mfx_pipeline_defs.h"
#include  "mfx_bitstream_reader_encryptor.h"
#include "shared_utils.h"

#define MY_MEMCPY_BITSTREAM(bitstream, offset, src, count) memcpy_s((bitstream).Data + (offset), (bitstream).MaxLength - (offset), (src), (count))

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

    MY_MEMCPY_BITSTREAM(*dest, dest->DataOffset, src->Data, src->DataLength);
    dest->DataLength = src->DataLength;

    dest->DataFlag = src->DataFlag;

    //common Extended buffer will be for src and dest bit streams
    dest->EncryptedData = src->EncryptedData;

    return MFX_ERR_NONE;
}

BitstreamReaderEncryptor::BitstreamReaderEncryptor(const pcpEncryptor& encryptor, IBitstreamReader *pReader, SOProxy *pavpdll, params *par)
    :base(pReader)
    ,m_encryptor(encryptor)
    ,m_pReader(pReader)
    ,m_pavpdll(pavpdll)
    ,m_params(*par)
{
}

BitstreamReaderEncryptor::~BitstreamReaderEncryptor()
{
    Close();
}

void BitstreamReaderEncryptor::Close()
{
    if (NULL != m_originalBS.get())
        m_originalBS.get()->EncryptedData = NULL;
    if (NULL != m_sessionPCP)
    {
        SO_CALL(PCPVideoDecode_Close, *m_pavpdll, MFX_ERR_UNSUPPORTED, (m_sessionPCP));
        m_sessionPCP = NULL;
    }
    WipeMFXBitstream(m_originalBS.get());
    base::Close();
}

mfxStatus MyInitMfxBitstream(mfxBitstream* pBitstream, mfxU32 nSize)
{
    //check input params
    CHECK_POINTER(pBitstream, MFX_ERR_NULL_PTR);
    CHECK_ERROR(nSize, 0, MFX_ERR_NOT_INITIALIZED);

    //prepare pBitstream
    WipeMFXBitstream(pBitstream);

    //prepare buffer
    pBitstream->Data = new mfxU8[nSize];
    CHECK_POINTER(pBitstream->Data, MFX_ERR_MEMORY_ALLOC);

    pBitstream->MaxLength = nSize;

    return MFX_ERR_NONE;
}

mfxStatus BitstreamReaderEncryptor::PostInit()
{
    m_isEndOfStream = false;
    m_sessionPCP = NULL;
    m_processedBS = NULL;

    m_originalBS.reset(new mfxBitstream2());
    MFX_CHECK_STS(MyInitMfxBitstream(m_originalBS.get(), 1024*1024));

    sStreamInfo streamInfo;
    base::GetStreamInfo(&streamInfo);
    MFX_CHECK_STS(SO_CALL(PCPVideoDecode_Init, *m_pavpdll, MFX_ERR_UNSUPPORTED, (&m_sessionPCP, streamInfo.videoType, &m_encryptor, m_params.isFullEncrypt)));

    return MFX_ERR_NONE; 

}

mfxStatus BitstreamReaderEncryptor::Init(const vm_char *strFileName)
{
    MFX_CHECK_STS(base::Init(strFileName));

    MFX_CHECK_STS(PostInit());

    return MFX_ERR_NONE; 
}

mfxStatus BitstreamReaderEncryptor::ReadNextFrame(mfxBitstream2 &pBS)
{
    mfxStatus sts = MFX_ERR_NONE;

    //read bit stream from source
    while (!m_originalBS->DataLength)
    {
        sts = base::ReadNextFrame(*(m_originalBS.get()));
        if (sts != MFX_ERR_NONE && sts != MFX_ERR_MORE_DATA)
            return sts;
        if (sts == MFX_ERR_MORE_DATA || (m_originalBS->DataFlag & MFX_BITSTREAM_EOS))
        {
            m_isEndOfStream = true;
            break;
        }
    }
    do
    {
        sts = SO_CALL(PCPVideoDecode_PrepareNextFrame, *m_pavpdll, MFX_ERR_UNSUPPORTED, (
            m_sessionPCP,
            m_isEndOfStream ? NULL : m_originalBS.get(),
            &m_processedBS,
            m_params.sparse));
        if (sts == MFX_ERR_MORE_DATA)
        {
            if (m_isEndOfStream)
            {
                break;
            }

            sts = base::ReadNextFrame(*(m_originalBS.get()));
            if (sts == MFX_ERR_MORE_DATA || (m_originalBS->DataFlag & MFX_BITSTREAM_EOS))
                m_isEndOfStream = true;
            else
            {
                CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            continue;
        } 
        else 
            CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    } while (MFX_ERR_NONE != sts || NULL == m_processedBS);

    // get output stream
    if (NULL != m_processedBS)
    {
        mfxStatus copySts = CopyBitstream2(
            &pBS,
            m_processedBS);
        pBS.DataFlag |= MFX_BITSTREAM_COMPLETE_FRAME;
        if (copySts < MFX_ERR_NONE)
            return copySts;
        m_processedBS = NULL;
    }

    return sts;
}

#endif//PAVP_BUILD