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


#include "vc1_spl.h"
#include "sample_defs.h"
#include <assert.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

namespace ProtectedLibrary
{

    VC1_Spl::VC1_Spl():m_iFrameReady(0),
        m_iFrameSize(0),
        m_iSliceNum(0),
        m_pHeaders(NULL),
        m_iLastFrame(0),
        m_pts((mfxU64)-1),
        m_iFrameStartCode(0)
    {
        Init();
    }

    VC1_Spl::VC1_Spl(const VC1_Spl &spl)
    {
        UNREFERENCED_PARAMETER(spl);
    }

    VC1_Spl& VC1_Spl::operator =(const VC1_Spl &spl)
    {
        UNREFERENCED_PARAMETER(spl);
        return *this;
    }

    VC1_Spl::~VC1_Spl()
    {
        Close();
    }

    mfxStatus VC1_Spl::Init()
    {
        m_currentFrame.resize(BUFFER_SIZE);
        m_slices.resize(128);
        memset(&m_frame, 0, sizeof(m_frame));
        m_frame.Data = &m_currentFrame[0];
        m_frame.Slice = &m_slices[0];
        memset(m_frame.Slice,0,sizeof(SliceSplitterInfo)*128);

        memset(m_StartCodeData,0,sizeof(VC1_StartCode)*MAX_START_CODE_NUM);

        m_pts = (mfxU64)-1;

        m_pHeaders = new VC1_SPL_Headers;
        if(!m_pHeaders)
            return MFX_ERR_MEMORY_ALLOC;


        return MFX_ERR_NONE;
    }

    mfxStatus VC1_Spl::Close()
    {
        if(m_pHeaders)
        {
            delete m_pHeaders;
            m_pHeaders = NULL;
        }
        return MFX_ERR_NONE;
    }

    mfxStatus VC1_Spl::Reset()
    {
        m_iFrameReady = 0;
        m_iFrameSize = 0;

        memset(&m_frame, 0, sizeof(m_frame));
        m_frame.Data = &m_currentFrame[0];
        m_frame.Slice = &m_slices[0];

        m_iSliceNum = 0;
        memset(m_StartCodeData,0,sizeof(VC1_StartCode)*MAX_START_CODE_NUM);

        return MFX_ERR_NONE;
    }

    mfxStatus VC1_Spl::GetFrame(mfxBitstream * bs_in, FrameSplitterInfo ** frame)
    {
        mfxStatus mfxSts = MFX_ERR_NONE;

        if(!frame)
            return MFX_ERR_NULL_PTR;
        *frame = 0;

        m_frame.TimeStamp  = (mfxU64)-1;

        if(!bs_in)
        {
            if(m_iLastFrame)
            {
                m_iLastFrame = 0;
                mfxSts =  FillOutData();
                m_frame.TimeStamp = m_pts;
                *frame = &m_frame;
                return MFX_ERR_NONE;
            }
            return MFX_ERR_MORE_DATA;
        }

        m_pts = bs_in->TimeStamp;

        mfxSts = GetNextData(bs_in);

        if((mfxSts == MFX_ERR_NONE) && (m_iFrameReady == 1))
        {
            mfxSts =  FillOutData();
            if(mfxSts == MFX_ERR_NONE)
                m_frame.TimeStamp = m_pts;
            *frame = &m_frame;
        }

        return mfxSts;
    }

    mfxStatus VC1_Spl::GetNextData(mfxBitstream * bs_in)
    {
        VC1_Sts_Type Sts = VC1_STS_ERR_NONE;

        Sts = StartCodeParsing(bs_in);

        switch(Sts)
        {
        case(VC1_STS_ERR_NONE):
            m_iFrameReady = 1;
            break;
        case(VC1_STS_END_OF_STREAM):
            m_iLastFrame = 1;
            return MFX_ERR_MORE_DATA;
            break;
        case(VC1_STS_NOT_ENOUGH_DATA):
            if(m_iFrameSize)
                m_iLastFrame = 1;
            return MFX_ERR_MORE_DATA;
            break;
        case(VC1_STS_INVALID_STREAM):
            return MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
        case(VC1_STS_NOT_ENOUGH_BUFFER):
            return MFX_ERR_NOT_ENOUGH_BUFFER;
            break;
        default:
            assert(0);
            break;
        }

        return MFX_ERR_NONE;
    }

    VC1_Sts_Type VC1_Spl::StartCodeParsing(mfxBitstream * bs_in)
    {
        mfxU32 readDataSize = bs_in->DataOffset;
        mfxU32 readBufSize  = bs_in->DataOffset + bs_in->DataLength;
        mfxU8* readBuf      = bs_in->Data;

        mfxU8* readPos = readBuf + bs_in->DataOffset;

        mfxU32 frameSize = m_iFrameSize;
        mfxU8* currFramePos = m_frame.Data + m_iFrameSize;
        mfxU32 frameBufSize = BUFFER_SIZE;
        mfxU32 currFramePosBufSize = frameBufSize - m_iFrameSize;

        mfxU8* ptr = currFramePos;
       // mfxU32 zeroNum = 0;
        mfxI32 size = 0;
        mfxU32 a = 0x0000FF00 | (*readPos);
        mfxU32 b = 0xFFFFFFFF;
        mfxU8 curStartCode = 0;

        while(readPos < (readBuf + readBufSize))
        {
            if (m_iSliceNum > MAX_START_CODE_NUM)
                return VC1_STS_INVALID_STREAM;

            //find sequence of 0x000001 or 0x000003
            while(!( b == 0x00000001 || b == 0x00000003 )
                &&(++readPos < (readBuf + readBufSize)))
            {
                a = (a<<8)| (mfxU32)(*readPos);
                b = a & 0x00FFFFFF;
            }

            //check end of read buffer
            if(readPos < (readBuf + readBufSize - 1))
            {
                if(*readPos == 0x01)
                {
                    if(((m_iFrameStartCode == 0)) ||
                        (*(readPos + 1) == VC1_Slice) ||
                        (*(readPos + 1) == VC1_Field)
                        || IS_VC1_USER_DATA(*(readPos + 1)))
                    {
                        if(m_iSliceNum >= MAX_START_CODE_NUM)
                        {
                            return VC1_STS_NOT_ENOUGH_BUFFER;
                        }

                        if(*(readPos + 1) == VC1_FrameHeader)
                        {
                            m_iFrameStartCode++;
                        }

                        curStartCode = (*(readPos + 1));
                        readPos+=2;
                        ptr = readPos - 5;


                        //slice or field size
                        size = (mfxU32)(ptr - readBuf - readDataSize+1);

                        if(frameSize + size > frameBufSize)
                            return VC1_STS_NOT_ENOUGH_BUFFER;

                        if(size)
                            memcpy_s(currFramePos, currFramePosBufSize, readBuf + readDataSize, size);

                        currFramePos = currFramePos + size;
                        currFramePosBufSize -= size;
                        frameSize = frameSize + size;
                        bs_in->DataLength -= size;

                        m_StartCodeData[m_iSliceNum].offset = frameSize;// + 4;
                        m_StartCodeData[m_iSliceNum].value  = ((*(readPos-1))) + ((*(readPos-2))<<8) + ((*(readPos-3))<<16) + (*(readPos-4)<<24);
                        readDataSize = (mfxU32)(readPos - readBuf - 4);

                        a = 0x00010b00 |(mfxU32)(*readPos);
                        b = a & 0x00FFFFFF;

                        m_iSliceNum++;
                    }
                    else if(m_iSliceNum != 0)
                    {
                        //end of frame
                        readPos = readPos - 2;
                        ptr = readPos - 1;

                        //trim zero bytes
                        //doesn't affect quality but buffer lengths are diff for protected and unprotected
                        //while ( (*ptr==0) && (ptr > readBuf) )
                        //    ptr--;

                        //slice or field size
                        size = (mfxU32)(ptr - readBuf - readDataSize +1);

                        if(frameSize + size > frameBufSize)
                            return VC1_STS_NOT_ENOUGH_BUFFER;

                        memcpy_s(currFramePos, currFramePosBufSize, readBuf + readDataSize, size);
                        m_iFrameSize = frameSize + size;
                        m_iFrameStartCode=0;

                        //prepare read buffer
                        size = (mfxU32)(readPos - readBuf - readDataSize);
                        readDataSize = readDataSize + size;
                        bs_in->DataOffset = readDataSize;
                        bs_in->DataLength -= size;

                        return VC1_STS_ERR_NONE;
                    }
                }
                else //if(*readPos == 0x03)
                {
                    //000003
                    readPos++;
                    a = (a<<8)| (mfxU32)(*readPos);
                    b = a & 0x00FFFFFF;
                }
            }
            else    //if (readPos >= (readBuf + readBufSize - 1 ))
            {
                if(readBufSize > 4)
                {
                    readPos = readBuf + readBufSize;
                    size = (mfxU32)(readBufSize - readDataSize);

                    if(size >= 0)
                    {
                        if(frameSize + size > frameBufSize)
                            return VC1_STS_NOT_ENOUGH_BUFFER;

                        memcpy_s(currFramePos, currFramePosBufSize, readBuf + readDataSize, size);
                        m_iFrameSize = frameSize + size;

                        bs_in->DataOffset += size;
                        bs_in->DataLength -= size;
                    }
                    else
                    {
                        m_iFrameSize = frameSize;

                        if(frameSize + size > frameBufSize)
                            return VC1_STS_NOT_ENOUGH_BUFFER;

                        memcpy_s(readBuf, bs_in->MaxLength, readBuf + readDataSize, readBufSize - readDataSize);
                        bs_in->DataOffset += (readBufSize - readDataSize);
                        bs_in->DataLength -= (readBufSize - readDataSize);
                    }

                    return VC1_STS_NOT_ENOUGH_DATA;
                }
                else
                {
                    //end of stream
                    size = (mfxU32)(readPos- readBuf - readDataSize);

                    if(frameSize + size > frameBufSize)
                        return VC1_STS_NOT_ENOUGH_BUFFER;

                    memcpy_s(currFramePos, currFramePosBufSize, readBuf + readDataSize, size);
                    m_iFrameSize = frameSize + size;
                    m_iFrameStartCode=0;

                    bs_in->DataOffset += size;
                    bs_in->DataLength -= size;
                    return VC1_STS_ERR_NONE;
                }
            }
        }

        //field or slice start code
        if(frameSize + 4 > frameBufSize)
            return VC1_STS_NOT_ENOUGH_BUFFER;

        if(readBufSize > readDataSize)
        {
            memcpy_s(currFramePos, currFramePosBufSize, readPos - 4, 4);
            m_iFrameSize = frameSize + 4;

            return VC1_STS_NOT_ENOUGH_DATA;
        }
        else
            return VC1_STS_END_OF_STREAM;
    }

    void VC1_Spl::ResetCurrentState()
    {
        mfxU32 i = 0;

        m_iFrameReady = 0;
        m_iFrameSize = 0;
        for (i = 0; i < m_iSliceNum; i++)
        {
            m_frame.Slice[i].DataLength = 0;
            m_frame.Slice[i].DataOffset = 0;
            m_frame.Slice[i].HeaderLength = 0;
        }
        m_iSliceNum = 0;
        memset(m_frame.Data,0,BUFFER_SIZE);
        memset(m_StartCodeData,0,sizeof(VC1_StartCode)*MAX_START_CODE_NUM);
    }

    mfxStatus VC1_Spl::FillOutData()
    {
        mfxU32 i = 0;
        mfxU32 j = 0;
        mfxStatus mfxSts = MFX_ERR_NONE;

        m_frame.DataLength = m_iFrameSize;
        m_frame.SliceNum   = m_iSliceNum;
        assert(m_iSliceNum > 0);

        for (i = 0; i < m_iSliceNum; i++)
        {
            m_frame.Slice[j].DataOffset = m_StartCodeData[i].offset;
            if(i < m_iSliceNum - 1)
            {
                m_frame.Slice[j].DataLength = m_StartCodeData[i+1].offset - m_StartCodeData[i].offset;
            }
            else
            {
                //last slice
                m_frame.Slice[j].DataLength = m_iFrameSize - m_StartCodeData[i].offset;
            }

            switch(m_StartCodeData[i].value & 0x000000FF)
            {
            case(VC1_FrameHeader):
                {
                    mfxSts = m_pHeaders->ParsePictureHeader((mfxU32*)(m_frame.Data + m_StartCodeData[i].offset),
                        &m_frame.Slice[j].HeaderLength,m_frame.Slice[j].DataLength);
                    if (mfxSts != MFX_ERR_NONE)
                        return mfxSts;

                    m_frame.Slice[j].DataOffset += m_frame.Slice[j].HeaderLength;
                    m_frame.Slice[j].DataLength -= m_frame.Slice[j].HeaderLength;

                    mfxSts = m_pHeaders->GetPicType(&(m_frame.Slice[j].SliceType));
                    if (mfxSts != MFX_ERR_NONE)
                        return mfxSts;

                    j++;
                }
                break;
            case(VC1_Field):
                {
                    mfxSts = m_pHeaders->ParseFieldHeader((mfxU32*)(m_frame.Data + m_StartCodeData[i].offset),
                        &m_frame.Slice[j].HeaderLength,m_frame.Slice[j].DataLength);
                    if (mfxSts != MFX_ERR_NONE)
                        return mfxSts;

                    m_frame.Slice[j].DataOffset += m_frame.Slice[j].HeaderLength;
                    m_frame.Slice[j].DataLength -= m_frame.Slice[j].HeaderLength;
                    m_frame.FirstFieldSliceNum = j;
                    mfxSts = m_pHeaders->GetPicType(&(m_frame.Slice[j].SliceType));
                    if (mfxSts != MFX_ERR_NONE)
                        return mfxSts;

                    j++;

                }
                break;
            case(VC1_Slice):
                mfxSts = m_pHeaders->ParseSliceHeader((mfxU32*)(m_frame.Data + m_StartCodeData[i].offset),
                    &m_frame.Slice[j].HeaderLength,m_frame.Slice[j].DataLength);
                if (mfxSts != MFX_ERR_NONE)
                    return mfxSts;

                m_frame.Slice[j].DataOffset += m_frame.Slice[j].HeaderLength;
                m_frame.Slice[j].DataLength -= m_frame.Slice[j].HeaderLength;
                mfxSts = m_pHeaders->GetPicType(&(m_frame.Slice[j].SliceType));
                if (mfxSts != MFX_ERR_NONE)
                    return mfxSts;

                j++;
                break;
            case(VC1_EntryPointHeader):
                mfxSts = m_pHeaders->ParseEntryPoint((mfxU32*)(m_frame.Data + m_StartCodeData[i].offset),
                    &m_frame.Slice[j].HeaderLength,m_frame.Slice[j].DataLength);
                break;
            case(VC1_SequenceHeader):
                mfxSts = m_pHeaders->ParseSeqHeader((mfxU32*)(m_frame.Data + m_StartCodeData[i].offset),
                    &m_frame.Slice[j].HeaderLength,m_frame.Slice[j].DataLength);
                break;
            case(VC1_SliceLevelUserData):
                break;
            case(VC1_FieldLevelUserData):
                break;
            case(VC1_FrameLevelUserData):
                break;
            case(VC1_EntryPointLevelUserData):
                break;
            case(VC1_EndOfSequence):
                break;
            case(VC1_SequenceLevelUserData):
                break;
            default:
                return MFX_ERR_UNDEFINED_BEHAVIOR;
                break;
            }
        }

        m_frame.SliceNum   = j;

        return MFX_ERR_NONE;
    }

    mfxStatus VC1_Spl::PostProcessing(FrameSplitterInfo *frame, mfxU32 sliceNum)
    {

        mfxU8* ptr = NULL;

        if(frame->Slice[sliceNum].DataOffset > 2)
        {
            ptr = frame->Data+frame->Slice[sliceNum].DataOffset;

            if((*ptr == 0x03) && (*(ptr-1)== 0) && (*(ptr-2)== 0) && (*(ptr+1)< 0x04))
            {
                *ptr = 0;
            }

            if((*(ptr-1) == 0x0) && (*(ptr)== 0) && (*(ptr+1)== 0x3) && (*(ptr+2)< 0x04))
            {
                *(ptr+1) = 0;
            }

            if((*(ptr) == 0) && (*(ptr+1)== 0) && (*(ptr+2)== 0x03) && (*(ptr+3)< 0x04))
            {
                frame->Slice[sliceNum].DataOffset++;
                frame->Slice[sliceNum].DataLength--;
                *(ptr+2) = 0;
            }

            if((*(ptr+1) == 0) && (*(ptr+2)== 0) && (*(ptr+3)== 0x03) && (*(ptr+4)< 0x04))
            {
                frame->Slice[sliceNum].DataOffset++;
                frame->Slice[sliceNum].DataLength--;
                *(ptr+3) = *(ptr+2);
                *(ptr+2) = *(ptr+1);
                *(ptr+1) = *(ptr+0);
            }
        }

        return MFX_ERR_NONE;
    }

}