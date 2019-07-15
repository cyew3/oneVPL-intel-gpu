/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.


File Name: mfx_encode.h

\* ****************************************************************************** */

#pragma once

#include "mfx_ivideo_encode.h"
#include "mfx_itime.h"
#include "mfx_istring_printer.h"

class LatencyEncode : public InterfaceProxy<IVideoEncode>
{
public:
    //if bAggregateStat=true it means that timestamp that was in decode input will be used for latency calculation
    //always use this in async pipelines
    LatencyEncode(bool bAggregateStat, IStringPrinter * pPrinter, ITime * pTime, std::unique_ptr<IVideoEncode> &&pTarget);
    virtual ~LatencyEncode();
    virtual mfxStatus Close();
    virtual mfxStatus EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp);
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait);

protected:

    struct FrameRecord
    {
        mfxU64  nTimeIn;
        mfxU64  nTimeOut;
        mfxSyncPoint syncp; //point to search for reference
        mfxBitstream *pBs;
        mfxU16        FrameType;
        FrameRecord()
        {
            MFX_ZERO_MEM(*this);
        }
    };

    std::list<FrameRecord> m_Queued;
    std::list<FrameRecord> m_Done;

    ITime *m_pTime;
    std::auto_ptr<IStringPrinter> m_pPrinter;
    bool    m_bAggregateStat ;
    mfxU64  m_lastAssignedPts;
};
