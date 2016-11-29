/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_frame_constructor.h"
#include <vector>

//utility functions to bistream accessing
static inline mfxU32 GetValue16(mfxU8* pBuf)
{
    MFX_CHECK_WITH_ERR(NULL != pBuf, 0);
    return ((*pBuf) << 8) | (*(pBuf + 1));
}

static inline mfxU32 GetValue24(mfxU8* pBuf)
{
    MFX_CHECK_WITH_ERR(NULL != pBuf, 0);
    return ((*pBuf) << 16) | (*(pBuf + 1) << 8) | (*(pBuf + 2));
}

static inline mfxU32 GetValue32(mfxU8* pBuf)
{
    MFX_CHECK_WITH_ERR(NULL != pBuf, 0);
    return ((*pBuf) << 24) | (*(pBuf + 1) << 16) | (*(pBuf + 2) << 8) | (*(pBuf + 3));
}

static inline mfxU32 GetLength(mfxU32 nBytesCount, mfxU8* pBuf)
{   
    MFX_CHECK_WITH_ERR(pBuf , 0);
    
    mfxU32 nLenght = 0;

    switch (nBytesCount)
    {
        case 1: nLenght = *pBuf;            break;
        case 2: nLenght = GetValue16(pBuf); break;
        case 3: nLenght = GetValue24(pBuf); break;
        case 4: nLenght = GetValue32(pBuf); break;
        default:
//            MFX_CHECK(nBytesCount <= 4 && nBytesCount >= 1);
            break;
    }

    return nLenght;
}

static inline void SetValue32(mfxU32 nValue, mfxU8* pBuf)
{
    if (NULL == pBuf)
        return ;

    for (mfxU32 i = 0; i < 4; i++)
    {
        *pBuf = (mfxU8)(nValue >> 8 * i);
        pBuf++;
    }

    return;
}

//////////////////////////////////////////////////////////////////////////

MFXFrameConstructor::MFXFrameConstructor() 
{
}

MFXFrameConstructor::~MFXFrameConstructor() 
{
}

mfxStatus MFXFrameConstructor::ConstructFrame(mfxBitstream *pBSIn, mfxBitstream *pBSOut)
{
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_HOTSPOTS);
//    mfxU8*          pDataBuffer = NULL;
//    mfxU32          nDataSize   = 0;

    MFX_CHECK_POINTER(pBSIn);
    MFX_CHECK_POINTER(pBSOut);

    if (0 == pBSIn->DataLength)
    {
        return MFX_ERR_MORE_DATA;
    }
    
    //checking whether we can survive in current array
    if (pBSOut->DataLength + pBSIn->DataLength> pBSOut->MaxLength)
    {
        mfxU32 nNewLen = pBSIn->DataLength + pBSOut->DataLength + 100;
        mfxU8 * p;
        MFX_CHECK_WITH_ERR(p = new mfxU8[nNewLen], MFX_ERR_MEMORY_ALLOC);

        pBSOut->MaxLength = nNewLen;

        memcpy(p, pBSOut->Data + pBSOut->DataOffset, pBSOut->DataLength);
        delete [] pBSOut->Data;
        pBSOut->Data = p;
    }else
    {
        memmove(pBSOut->Data, pBSOut->Data + pBSOut->DataOffset, pBSOut->DataLength);
    }

    //copying params : timestamp, frametype, etc
    pBSOut->FrameType  = pBSIn->FrameType;
    pBSOut->PicStruct  = pBSIn->PicStruct;
    pBSOut->TimeStamp  = pBSIn->TimeStamp;
    pBSOut->DataOffset = 0;

    memcpy(pBSOut->Data + pBSOut->DataLength, pBSIn->Data, pBSIn->DataLength);
    pBSOut->DataLength += pBSIn->DataLength;

    return MFX_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////

MFXVC1FrameConstructor::MFXVC1FrameConstructor() 
: MFXFrameConstructor()
{
}

mfxStatus MFXVC1FrameConstructor::Init(mfxVideoParam *stream_params)
{
    MFX_CHECK_POINTER(stream_params);
    m_VideoParam = *stream_params;
    m_isSeqHeaderConstructed = false;
    return MFX_ERR_NONE;
}

mfxStatus MFXVC1FrameConstructor::ConstructFrame( mfxBitstream* pBSIn
                                                , mfxBitstream* pBSOut)
{
    mfxStatus       sts         = MFX_ERR_NONE;
    std::vector<mfxU8> tempBuffer;

    MFX_CHECK_POINTER(pBSIn);

    if (MFX_PROFILE_VC1_ADVANCED != m_VideoParam.mfx.CodecProfile)
    {
        sts = ConstructSequenceHeaderSM(pBSIn, pBSOut);
    }
    else
    {
        if (0 == pBSIn->DataLength)
        {
            sts = MFX_ERR_MORE_DATA;
        }

        if (MFX_ERR_NONE == sts && NULL == pBSIn->Data)
        {
            sts = MFX_ERR_MORE_DATA;
        }

        if (MFX_ERR_NONE == sts)
        {
            /*SAFE_DELETE_ARRAY(pBSOut->Data);

            
            pBSOut->MaxLength = 
                pBSOut->DataLength = nDataSize + 4 + m_mfxResidialBS.DataLength;

            if (MFX_PROFILE_VC1_ADVANCED != m_VideoParam->mfx.CodecProfile)
            {
                pBS->MaxLength = 
                    pBS->DataLength = pBS->DataLength + 4;
            }

            pBS->Data = new mfxU8[pBS->DataLength];
            CHECK_POINTER_SET_STS(pBS->Data, sts);*/
            //tempBuffer.resize(pBSIn->DataLength + 4);
        }

        if (MFX_ERR_NONE == sts)
        {
            if (MFX_PROFILE_VC1_ADVANCED != m_VideoParam.mfx.CodecProfile)
            {
                tempBuffer.resize(8);
                SetValue32(pBSIn->DataLength, &tempBuffer.front());     
                SetValue32(0, &tempBuffer[4]);
            }
            else 
            {
                if (false == IsStartCodeExist(pBSIn->Data))
                {
                    // set start code to first 4 bytes
                    tempBuffer.resize(4);
                    SetValue32(0x0D010000, &tempBuffer[0]);
                }
            }
            
            tempBuffer.insert(tempBuffer.end()
                , pBSIn->Data + pBSIn->DataOffset
                , pBSIn->Data + pBSIn->DataOffset + pBSIn->DataLength);

            if (!tempBuffer.empty())
            {
                mfxBitstream bs;
                bs.Data = &tempBuffer.front();
                bs.DataOffset = 0;
                bs.MaxLength = 
                    bs.DataLength = (mfxU32)tempBuffer.size();

                MFX_CHECK_STS(MFXFrameConstructor::ConstructFrame(&bs, pBSOut));

                pBSOut->FrameType  = pBSIn->FrameType;
                pBSOut->PicStruct  = pBSIn->PicStruct;
                pBSOut->TimeStamp  = pBSIn->TimeStamp;
            }
        }
    }    

    return sts;
}

mfxStatus MFXVC1FrameConstructor::ConstructSequenceHeaderSM( mfxBitstream* pBSIn
                                                           , mfxBitstream* pBSOut)
{
    std::vector<mfxU8> tempBuffer;

    MFX_CHECK_POINTER(pBSIn);

    if (0 == pBSIn->DataLength || NULL == pBSIn->Data)
    {
        return MFX_ERR_MORE_DATA;
    }

    //if (pBSIn->GetActualDataLength() + 20 > pSample->GetSize())
    //{
    //    sts = MFX_ERR_MORE_DATA;
    //}

    //if (MFX_ERR_NONE == sts)
    //{
    //    pSequenceHeaderSM = new mfxU8[nDataSize];
    //    CHECK_POINTER_SET_STS(pSequenceHeaderSM, sts);
    //}

    // save input data
    //memcpy(pSequenceHeaderSM, pDataBuffer, nDataSize);

    tempBuffer.resize(8);
    // set start code
    SetValue32(0xC5000000, &tempBuffer.front());

    // set size of sequence header is 4 bytes
    SetValue32(pBSIn->DataLength, &tempBuffer.front() + 4);

    // copy saved data back
    tempBuffer.insert(tempBuffer.end()
        , pBSIn->Data + pBSIn->DataOffset
        , pBSIn->Data + pBSIn->DataOffset + pBSIn->DataLength);

    pBSIn->DataLength = 0;
    pBSIn->DataOffset = 0;
    
    tempBuffer.resize(tempBuffer.size() + 12);

    // set resolution 
    SetValue32(m_VideoParam.mfx.FrameInfo.Height, &tempBuffer.back() - 11);
    SetValue32(m_VideoParam.mfx.FrameInfo.Width,  &tempBuffer.back() - 7);

    //set 0 to the last 4 bytes
    SetValue32(0,  &tempBuffer.back() - 3);

    // copy back to input bitstream extend if necessary
    mfxBitstream bs;
    bs.Data = &tempBuffer.front();
    bs.DataOffset = 0;
    bs.MaxLength = 
        bs.DataLength = (mfxU32)tempBuffer.size();

    MFX_CHECK_STS(MFXFrameConstructor::ConstructFrame(&bs, pBSOut));

    m_isSeqHeaderConstructed = true;

    return MFX_ERR_NONE;
}

bool MFXVC1FrameConstructor::IsStartCodeExist(mfxU8* pStart)
{
    if (!pStart)
        return false;

    //check first 4 bytes to be start code
    switch (GetLength(4, pStart))
    {
    case 0x010D:
    case 0x010F:
    case 0x010A:
    case 0x010E:
    case 0x010C:
    case 0x011B:
    case 0x011C:
    case 0x011D:
    case 0x011E:
    case 0x011F:
        // start code found
        return true;
    default:
        // start code not found
        return false; 
    }
}

//////////////////////////////////////////////////////////////////////////

StartCodeIteratorMP4::StartCodeIteratorMP4()
: m_pSource(0)
, m_nSourceSize(0)
, m_pSourceBase(0)
, m_nSourceBaseSize(0)
{
}

mfxU32 StartCodeIteratorMP4::GetCurrentOffset()
{
    return (mfxU32)(m_pSource - m_pSourceBase);
}

void StartCodeIteratorMP4::SetLenInBytes(mfxU32 nByteLen)
{
    m_nByteLen = nByteLen;
}

mfxU32 StartCodeIteratorMP4::Init(mfxBitstream *pBS)
{
    m_pSourceBase = 
        m_pSource = (mfxU8*) pBS->Data + pBS->DataOffset;

    m_nSourceBaseSize = 
        m_nSourceSize = pBS->DataLength;

    m_nFullSize     = pBS->DataLength;
    m_nSourceSize   = 0;

    return (*(m_pSource + m_nByteLen));
}

mfxU32 StartCodeIteratorMP4::GetNext()
{
    mfxU32 nLength = GetLength(m_nByteLen, m_pSource);

    m_pSource     += (nLength + m_nByteLen);
    m_nSourceSize += (nLength + m_nByteLen);

    //arithmetical overflow
    if (m_nSourceSize >= m_nFullSize || nLength > m_nFullSize)
    {
        return 0;
    }

    return (*(m_pSource + m_nByteLen));
}

//////////////////////////////////////////////////////////////////////////

MFXAVCFrameConstructor::MFXAVCFrameConstructor() 
: m_bHeaderReaded(false)
{
    memset(&m_StartCodeBS, 0, sizeof(mfxBitstream));

    m_StartCodeBS.Data = m_startCodesBuf;
    m_StartCodeBS.DataLength =
        m_StartCodeBS.MaxLength = 4;

    SetValue32(0x01000000, m_StartCodeBS.Data);   
}

MFXAVCFrameConstructor::~MFXAVCFrameConstructor()
{
}

mfxStatus MFXAVCFrameConstructor::ConstructFrame(mfxBitstream* pBSIn, mfxBitstream *pBSOut)
{
    std::vector<mfxU8>      tempBuffer;
//    mfxU8*                  pDataBuffer = NULL;
//    mfxU32                  nDataSize   = 0;
    StartCodeIteratorMP4    m_pStartCodeIter;

    MFX_CHECK_POINTER(pBSIn);
    MFX_CHECK_POINTER(pBSOut);

    if (!m_bHeaderReaded)
    {
        return ReadHeader(pBSIn, pBSOut);
    }

    if (0 == pBSIn->DataLength)
    {
        return MFX_ERR_MORE_DATA;
    }

    while (pBSIn->DataLength)
    {
        mfxU32 nOriginalSize    = pBSIn->DataLength;
        mfxU32 nCode            = 0;
        mfxU32 nFirstOffset     = 0;

        m_pStartCodeIter.SetLenInBytes(m_avcRecord.lengthSizeMinusOne + 1);

        nCode = m_pStartCodeIter.Init(pBSIn); // find first start code

        if (0 == nCode)
        {
            break;
        }

        nFirstOffset = m_pStartCodeIter.GetCurrentOffset();
        nFirstOffset += (m_avcRecord.lengthSizeMinusOne + 1);

        nCode = m_pStartCodeIter.GetNext();

        if (nCode)
        {
            nOriginalSize = m_pStartCodeIter.GetCurrentOffset() - nFirstOffset;
        }
        else
        {
            nOriginalSize -= nFirstOffset;
        }

        tempBuffer.insert(tempBuffer.end(), m_StartCodeBS.Data, m_StartCodeBS.Data + 4);
        tempBuffer.insert(tempBuffer.end(), pBSIn->Data + pBSIn->DataOffset + nFirstOffset, pBSIn->Data + pBSIn->DataOffset + nFirstOffset + nOriginalSize);

        pBSIn->DataOffset += nOriginalSize + nFirstOffset;
        pBSIn->DataLength -= nOriginalSize + nFirstOffset;
    }

    if (!tempBuffer.empty())
    {
        mfxBitstream bs;
        bs.Data = &tempBuffer.front();
        bs.DataOffset = 0;
        bs.MaxLength = 
            bs.DataLength = (mfxU32)tempBuffer.size();
        
        MFX_CHECK_STS(MFXFrameConstructor::ConstructFrame(&bs, pBSOut));

        pBSOut->FrameType  = pBSIn->FrameType;
        pBSOut->PicStruct  = pBSIn->PicStruct;
        pBSOut->TimeStamp  = pBSIn->TimeStamp;
    }

    //copy frame parameters
    return MFX_ERR_NONE;
}

#define MOVE_BS(bs, mov)\
MFX_CHECK((bs).DataLength - mov>=0);\
p = (bs).DataOffset + (bs).Data;\
(bs).DataOffset += (mfxU32)(mov);\
(bs).DataLength  = (mfxU32)(mov);

mfxStatus MFXAVCFrameConstructor::ReadHeader(mfxBitstream* pBSIn, mfxBitstream *pBSOut)
{
    if (0 == pBSIn->DataLength)
    {
        return MFX_ERR_MORE_DATA;
    }

    mfxU8 *p;
    MOVE_BS(*pBSIn, 1);
    m_avcRecord.configurationVersion = *p;
    MOVE_BS(*pBSIn, 1);
    m_avcRecord.AVCProfileIndication = *p;
    MOVE_BS(*pBSIn, 1);
    m_avcRecord.profile_compatibility = *p;
    MOVE_BS(*pBSIn, 1);
    m_avcRecord.AVCLevelIndication = *p;
    MOVE_BS(*pBSIn, 1);
    m_avcRecord.lengthSizeMinusOne = *p;
    m_avcRecord.lengthSizeMinusOne <<= 6;
    m_avcRecord.lengthSizeMinusOne >>= 6; // remove reserved bits
    MOVE_BS(*pBSIn, 1);
    m_avcRecord.numOfSequenceParameterSets = *p;
    m_avcRecord.numOfSequenceParameterSets <<= 3;
    m_avcRecord.numOfSequenceParameterSets >>= 3;// remove reserved bits

    std::vector<mfxU8> tempBuffer;

    for (int i = 0; i < m_avcRecord.numOfSequenceParameterSets; i++)    
    {
        tempBuffer.insert(tempBuffer.end(), m_StartCodeBS.Data, m_StartCodeBS.Data + 4);

        MOVE_BS(*pBSIn, 2);
        mfxU32 length = GetValue16(p);

        MOVE_BS(*pBSIn, length);
        tempBuffer.insert(tempBuffer.end(), p, p + length);
    }

    MOVE_BS(*pBSIn, 1);
    m_avcRecord.numOfPictureParameterSets = *p;

    for (int i = 0; i < m_avcRecord.numOfPictureParameterSets; i++)
    {
        tempBuffer.insert(tempBuffer.end(), m_StartCodeBS.Data, m_StartCodeBS.Data + 4);
        MOVE_BS(*pBSIn, 2);
        mfxU32 length = GetValue16(p);

        MOVE_BS(*pBSIn, length);
        tempBuffer.insert(tempBuffer.end(), p, p + length);
    }

    m_bHeaderReaded = true;

    if (!tempBuffer.empty())
    {
        mfxBitstream bs;
        bs.Data = &tempBuffer.front();
        bs.DataOffset = 0;
        bs.MaxLength = 
            bs.DataLength = (mfxU32)tempBuffer.size();

        MFX_CHECK_STS(MFXFrameConstructor::ConstructFrame(&bs, pBSOut));

        pBSOut->FrameType  = pBSIn->FrameType;
        pBSOut->PicStruct  = pBSIn->PicStruct;
        pBSOut->TimeStamp  = pBSIn->TimeStamp;
    }

    return MFX_ERR_NONE;
}

MFXHEVCFrameConstructor::MFXHEVCFrameConstructor():
    m_bHeaderReaded(false)
{
    memset(&m_StartCodeBS, 0, sizeof(mfxBitstream));

    m_StartCodeBS.Data = m_startCodesBuf;
    m_StartCodeBS.DataLength = m_StartCodeBS.MaxLength = 4;

    SetValue32(0x01000000, m_StartCodeBS.Data);   
}

MFXHEVCFrameConstructor::~MFXHEVCFrameConstructor()
{
}

mfxStatus MFXHEVCFrameConstructor::ConstructFrame(mfxBitstream* pBSIn, mfxBitstream *pBSOut)
{
    std::vector<mfxU8>      tempBuffer;

    MFX_CHECK_POINTER(pBSIn);
    MFX_CHECK_POINTER(pBSOut);

    if (!m_bHeaderReaded)
    {
        return ReadHeader(pBSIn, pBSOut);
    }

    if (0 == pBSIn->DataLength)
    {
        return MFX_ERR_MORE_DATA;
    }

    while (pBSIn->DataLength > 4)
    {
        mfxU32 nDataLength = GetLength(4, pBSIn->Data + pBSIn->DataOffset);
        
        if (nDataLength > (pBSIn->DataLength - 4)) break; // not enough data in the buffer

        pBSIn->DataOffset += 4;
        pBSIn->DataLength -= 4;
        tempBuffer.insert(tempBuffer.end(), m_StartCodeBS.Data, m_StartCodeBS.Data + 4);
        tempBuffer.insert(tempBuffer.end(), pBSIn->Data + pBSIn->DataOffset, pBSIn->Data + pBSIn->DataOffset + nDataLength);

        pBSIn->DataOffset += nDataLength;
        pBSIn->DataLength -= nDataLength;
    }

    if (!tempBuffer.empty())
    {
        mfxBitstream bs;
        bs.Data = &tempBuffer.front();
        bs.DataOffset = 0;
        bs.MaxLength = bs.DataLength = (mfxU32)tempBuffer.size();
        
        MFX_CHECK_STS(MFXFrameConstructor::ConstructFrame(&bs, pBSOut));

        pBSOut->FrameType  = pBSIn->FrameType;
        pBSOut->PicStruct  = pBSIn->PicStruct;
        pBSOut->TimeStamp  = pBSIn->TimeStamp;
    }

    //copy frame parameters
    return MFX_ERR_NONE;
}

mfxStatus MFXHEVCFrameConstructor::ReadHeader(mfxBitstream* pBSIn, mfxBitstream *pBSOut)
{
    if (0 == pBSIn->DataLength)
    {
        return MFX_ERR_MORE_DATA;
    }

    mfxU8 *p;
    MOVE_BS(*pBSIn, 1);
    m_hevcRecord.configurationVersion = *p;
    MOVE_BS(*pBSIn, 21);
    MOVE_BS(*pBSIn, 1);
    m_hevcRecord.numOfParameterSets = *p;

    std::vector<mfxU8> tempBuffer;

    for (int i = 0; i < m_hevcRecord.numOfParameterSets; ++i)
    {

        MOVE_BS(*pBSIn, 1); // skipping array completeness and type
        MOVE_BS(*pBSIn, 2);
        mfxU16 numOfNALs = GetValue16(p);
        for (int j = 0; j < numOfNALs; ++j)
        {
            tempBuffer.insert(tempBuffer.end(), m_StartCodeBS.Data, m_StartCodeBS.Data + 4);

            MOVE_BS(*pBSIn, 2);
            mfxU32 length = GetValue16(p);

            MOVE_BS(*pBSIn, length);
            tempBuffer.insert(tempBuffer.end(), p, p + length);
        }
    }

    m_bHeaderReaded = true;

    if (!tempBuffer.empty())
    {
        mfxBitstream bs;
        bs.Data = &tempBuffer.front();
        bs.DataOffset = 0;
        bs.MaxLength = bs.DataLength = (mfxU32)tempBuffer.size();

        MFX_CHECK_STS(MFXFrameConstructor::ConstructFrame(&bs, pBSOut));

        pBSOut->FrameType  = pBSIn->FrameType;
        pBSOut->PicStruct  = pBSIn->PicStruct;
        pBSOut->TimeStamp  = pBSIn->TimeStamp;
    }

    return MFX_ERR_NONE;
}
