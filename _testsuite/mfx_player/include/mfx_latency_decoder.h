/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_itime.h"
#include "mfx_iyuv_source.h"
#include "mfx_istring_printer.h"

//turnon to include decodeheader+init into latency calculation
#define DECODE_HEADER_LATENCY 0

class LatencyDecoder : public InterfaceProxy<IYUVSource>
{
public:
    //if aggreage info specified decoder will not buffer frames in it's queue, since all the statistics will be in encode latency stat
    LatencyDecoder( bool bAggregateInfo
                  , IStringPrinter * pPrinter
                  , ITime * pTimer
                  , const tstring & name
                  , std::unique_ptr<IYUVSource> &&pTarget);
    virtual ~LatencyDecoder();
    //decode header also impact to latency
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) ;
    virtual mfxStatus DecodeFrameAsync(mfxBitstream2 &bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait);
    virtual mfxStatus Close() ;

protected:

    //since we calling close from destructor we cannot delegate this close to decorated object
    virtual mfxStatus CloseLocal() ;

    bool    m_bAggregateInfo;
    //decode header time acuisited in first call
    bool    m_bFirstCall;
    mfxU64  m_decodeHeaderTimestamp;

    mfxU64  m_lastAssignedTimestamp;
    ITime * m_pTime;
    std::auto_ptr<IStringPrinter> m_pPrinter;
    tstring m_name;

    struct FrameRecord
    {
        //indicates that frame already passed to decodeframeasync queue
        mfxU64  nTimeSplitted;
        mfxU64  nTimeDecoded;
        mfxU16  FrameType;
        mfxSyncPoint syncp; //point to search for reference
        FrameRecord()
        {
            MFX_ZERO_MEM(*this);
        }
    };

    std::list<FrameRecord> m_Queued;
    std::list<FrameRecord> m_Decoded;
};
