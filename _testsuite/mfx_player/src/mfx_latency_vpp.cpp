/***********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

***********************************************************************************/

#include "mfx_pipeline_defs.h"
#include "mfx_latency_vpp.h"
#include <iomanip>

LatencyVPP::LatencyVPP(bool bAggregateStat, IStringPrinter * pPrinter, ITime * pTime, IMFXVideoVPP* pTarget)
    : InterfaceProxy<IMFXVideoVPP>(pTarget)
    , m_bAggregateStat(bAggregateStat)
    , m_lastAssignedPts((mfxU64)-1)
    , m_pTime(pTime)
    , m_pPrinter(pPrinter)
{
    if (NULL == m_pPrinter.get())
    {
        m_pPrinter.reset(new ConsolePrinter());
    }
}

LatencyVPP::~LatencyVPP()
{
    Close();
}

mfxStatus LatencyVPP::RunFrameVPPAsync(mfxFrameSurface1 *in, mfxFrameSurface1 *out,  mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
{
    if (!m_bAggregateStat && in && in->Data.TimeStamp != m_lastAssignedPts)
    {
        m_lastAssignedPts = in->Data.TimeStamp = m_pTime->GetTick();
    }

    mfxStatus sts = InterfaceProxy<IMFXVideoVPP>::RunFrameVPPAsync(in, out, aux, syncp);

    if (sts >= MFX_ERR_NONE && sts != MFX_WRN_DEVICE_BUSY && NULL != out && NULL != syncp)
    {
        FrameRecord data;

        data.pOutSurf = out;
        data.syncp = *syncp;
        m_Queued.push_back(data);
        m_lastAssignedPts = (mfxU64)-1;
    }

    return sts;
}

mfxStatus LatencyVPP::RunFrameVPPAsyncEx(mfxFrameSurface1 *in, mfxFrameSurface1 *work, mfxFrameSurface1 **out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
{
    if (!m_bAggregateStat && in && in->Data.TimeStamp != m_lastAssignedPts)
    {
        m_lastAssignedPts = in->Data.TimeStamp = m_pTime->GetTick();
    }

    mfxStatus sts = InterfaceProxy<IMFXVideoVPP>::RunFrameVPPAsyncEx(in, work, out, aux, syncp);

    if (sts >= MFX_ERR_NONE && sts != MFX_WRN_DEVICE_BUSY && NULL != out && NULL != *out && NULL != syncp)
    {
        FrameRecord data;

        data.pOutSurf = *out;
        data.syncp = *syncp;
        m_Queued.push_back(data);
        m_lastAssignedPts = (mfxU64)-1;
    }

    return sts;
}

mfxStatus LatencyVPP::SyncOperation(mfxSyncPoint syncp, mfxU32 wait)
{
    mfxStatus sts = InterfaceProxy<IMFXVideoVPP>::SyncOperation(syncp, wait);

    if (sts >= MFX_ERR_NONE && sts != MFX_WRN_IN_EXECUTION)
    {
        FrameRecord record;        

        //searching for syncpoint should match to first
        if (!m_Queued.empty() && syncp == m_Queued.front().syncp)
        {
            m_Done.push_back(m_Queued.front());
            m_Done.back().nTimeOut = m_pTime->GetTick();
            if (NULL != m_Done.back().pOutSurf)
            {
                m_Done.back().nTimeIn = m_Done.back().pOutSurf->Data.TimeStamp;
            }
            m_Queued.pop_front();
        }
    }

    return sts;
}

//TODO : create a generic class for statistics collecting
mfxStatus LatencyVPP::Close()
{
    if (!m_Done.empty())
    {
        double dFreq = (double)m_pTime->GetFreq();
        m_pPrinter->PrintLine(VM_STRING("VPP Latency Stat"));

        //print statisticks here
        //for now lets print all frames offsets
        std::list<FrameRecord> ::iterator it;
        int i = 0;
        double summ = 0;
        double max_latency = 0;
        for(it = m_Done.begin(); it!= m_Done.end(); it++, i++)
        {
            tstringstream stream;

            double dStart = 1000.0*(double)(it->nTimeIn) / dFreq;
            double dEnd = 1000.0*(double)(it->nTimeOut)  / dFreq;

            stream<<std::setiosflags(std::ios::fixed) << std::setprecision(2);
            stream<<i<<VM_STRING(": ")<<dEnd - dStart<<VM_STRING("  (")<<dStart<<VM_STRING(" ")<<dEnd<<VM_STRING(")");

            summ += dEnd - dStart;
            max_latency = (std::max)(max_latency, dEnd - dStart);
            m_pPrinter->PrintLine(stream.str());
        }

        tstringstream stream;

        stream<<std::setiosflags(std::ios::fixed) << std::setprecision(2);
        stream<<VM_STRING("VPP average latency(ms) = ") << summ / (double)i;
        m_pPrinter->PrintLine(stream.str());
        
        //clearing stream
        stream.str(VM_STRING(""));

        stream<<VM_STRING("VPP max latency(ms) = ") << max_latency;
        m_pPrinter->PrintLine(stream.str());
        
        m_Done.clear();
    }
        
    return InterfaceProxy<IMFXVideoVPP>::Close();
}
