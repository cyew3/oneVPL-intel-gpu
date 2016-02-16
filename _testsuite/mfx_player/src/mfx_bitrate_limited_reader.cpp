/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include  "mfx_bitrate_limited_reader.h"

BitrateLimitedReader :: BitrateLimitedReader( mfxU32 nBytesSec
                                            , ITime * pTime
                                            , mfxU32 nMinChunkSize
                                            , IBitstreamReader * pReader)
    : base(pReader)
    , m_inputBs()
    , m_nBytesPerSec(nBytesSec)
    , m_nMinChunkSize(nMinChunkSize)
    , m_pTime(pTime)
    , m_nBytesWritten()
    , m_bEOS()
    , m_firstDataWrittenTick()
{
}

//mfxStatus BitrateLimitedReader :: ReadNextFrame(mfxBitstream2 &bs)
//{
//    //TODO: remove memory leak
//    //reading into internal buffer
//    if (!bs.isNull && bs.MaxLength > m_inputBs.MaxLength)
//    {
//        m_bsData.resize(bs.MaxLength);
//        m_inputBs.MaxLength = bs.MaxLength;
//        m_inputBs.Data = &m_bsData.front();
//    }
//
//    //copy all available data
//    mfxU32 nBytesCopied = m_nMinChunkSize ? m_nMinChunkSize : m_inputBs.DataLength;
//    mfxStatus sts = BSUtil::MoveNBytesTail(bs.isNull ? NULL : &bs, &m_inputBs, nBytesCopied);
//
//    if (sts == MFX_ERR_MORE_DATA || 0 == nBytesCopied)
//    {
//        //need to update internal buffer
//        sts = PullData();
//
//        //TODO: comment this and fix crash
//        nBytesCopied = m_nMinChunkSize ? m_nMinChunkSize : m_inputBs.DataLength;
//
//        //and copy data again
//        if (sts != MFX_ERR_NONE)
//        {
//            return sts;
//        }
//        sts = BSUtil::MoveNBytesTail(&bs, &m_inputBs, nBytesCopied);
//    }
//    
//    if (MFX_ERR_NONE != sts)
//    {
//        return sts;
//    }
//
//    m_nBytesWritten += nBytesCopied;
//
//    ControlBitrate();
//    bs.FrameType = m_inputBs.FrameType;
//    
//    return MFX_ERR_NONE;
//}

mfxStatus BitrateLimitedReader :: ReadNextFrame(mfxBitstream2 &bs)
{
    mfxU32 nDataLen = bs.DataLength;

    mfxStatus sts = base::ReadNextFrame(bs);

    m_nBytesWritten += bs.DataLength - nDataLen;

    ControlBitrate();

    return sts;
}

mfxStatus BitrateLimitedReader :: PullData()
{
    //do we have a room in current buffer
    if (0 == BSUtil::WriteAvailTail(&m_inputBs))
    {
        m_inputBs.DataOffset = 0;
        //check this len always = 0
        m_inputBs.DataLength = 0;
    }
    return base::ReadNextFrame(m_inputBs);
}

void BitrateLimitedReader :: ControlBitrate()
{
    if (0 == m_firstDataWrittenTick)
    {
        m_firstDataWrittenTick = m_pTime->Current();
        return;
    }
    double nNeedExecutionTime = (double)m_nBytesWritten / (double)m_nBytesPerSec ;
    
    double dCurrentTime = m_pTime->Current() - m_firstDataWrittenTick;

    if (nNeedExecutionTime > dCurrentTime)
    {
        m_pTime->Wait((mfxU32)(1000.0 * (nNeedExecutionTime - dCurrentTime)));
    }
    else
    {
        //todo: findout printf threshold
        if (nNeedExecutionTime* 1.1 < dCurrentTime)
        {
            PipelineTrace((VM_STRING("[BRL] Cannot Reach Target Bitrate: current = %.0f bps\n"), 8*(double)(m_nBytesWritten) / dCurrentTime));
        }
    }
}