/***********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

***********************************************************************************/

#pragma once

#include "mfx_ivpp.h"
#include "mfx_itime.h"
#include "mfx_istring_printer.h"

class LatencyVPP : public InterfaceProxy<IMFXVideoVPP>
{
public:
    //if bAggregateStat=true it means that timestamp that was in decode input will be used for latency calculation
    //always use this in async pipelines
    LatencyVPP(bool bAggregateStat, IStringPrinter * pPrinter, ITime * pTime, IMFXVideoVPP*  pTarget);
    virtual ~LatencyVPP();   
    virtual mfxStatus Close();
    virtual mfxStatus RunFrameVPPAsync  (mfxFrameSurface1 *in, mfxFrameSurface1 *out,  mfxExtVppAuxData *aux, mfxSyncPoint *syncp);
    virtual mfxStatus RunFrameVPPAsyncEx(mfxFrameSurface1 *in, mfxFrameSurface1 *work, mfxFrameSurface1 **out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp);
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait);

protected:

    struct FrameRecord
    {
        mfxU64 nTimeIn;
        mfxU64 nTimeOut;
        mfxFrameSurface1* pOutSurf;
        mfxSyncPoint syncp;
        FrameRecord()
        {
            MFX_ZERO_MEM(*this);
        }
    };

    std::list<FrameRecord> m_Queued;
    std::list<FrameRecord> m_Done;

    bool m_bAggregateStat;
    mfxU64 m_lastAssignedPts;
    ITime* m_pTime;
    std::auto_ptr<IStringPrinter> m_pPrinter;
};
