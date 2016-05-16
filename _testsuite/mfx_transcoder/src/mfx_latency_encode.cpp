/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_latency_encode.h"
#include <iomanip>

LatencyEncode::LatencyEncode(bool bAggregateStat, IStringPrinter * pPrinter, ITime * pTime, std::auto_ptr<IVideoEncode>& pTarget)
    : InterfaceProxy<IVideoEncode>(pTarget)
    , m_pTime(pTime)
    , m_pPrinter(pPrinter)
    , m_bAggregateStat(bAggregateStat)
    , m_lastAssignedPts((mfxU64)-1)
{
    if (NULL == m_pPrinter.get())
    {
        m_pPrinter.reset(new ConsolePrinter());
    }
}

LatencyEncode::~LatencyEncode()
{
    Close();
}

mfxStatus LatencyEncode::EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
{
    if (!m_bAggregateStat && surface && surface->Data.TimeStamp != m_lastAssignedPts)
    {
        m_lastAssignedPts = surface->Data.TimeStamp = m_pTime->GetTick();
    }
    
    mfxStatus sts = InterfaceProxy<IVideoEncode>::EncodeFrameAsync(ctrl, surface, bs, syncp);

    if (sts >= MFX_ERR_NONE && sts != MFX_WRN_DEVICE_BUSY && NULL != bs && NULL != syncp)
    {
        FrameRecord data;
        
        //Encoder set timestamps in async part
        //data.nTimeIn = bs->TimeStamp;

        data.pBs     = bs;
        data.syncp   = *syncp;
        m_Queued.push_back(data);
        m_lastAssignedPts = (mfxU64)-1;
    }

    return sts;
}

mfxStatus LatencyEncode::SyncOperation(mfxSyncPoint syncp, mfxU32 wait)
{
    mfxStatus sts = InterfaceProxy<IVideoEncode>::SyncOperation(syncp, wait);

    if (sts >= MFX_ERR_NONE && sts != MFX_WRN_IN_EXECUTION)
    {
        FrameRecord record;        

        //searching for syncpoint should match to first
        if (!m_Queued.empty() && syncp == m_Queued.front().syncp)
        {
            m_Done.push_back(m_Queued.front());
            m_Done.back().nTimeOut = m_pTime->GetTick();
            if (NULL != m_Done.back().pBs)
            {
                m_Done.back().nTimeIn = m_Done.back().pBs->TimeStamp;
                m_Done.back().FrameType = m_Done.back().pBs->FrameType;
            }
            m_Queued.pop_front();
        }

    }

    return sts;
}

//TODO : create a generic class for statistics collecting
mfxStatus LatencyEncode::Close()
{
    if (!m_Done.empty())
    {
        double dFreq = (double)m_pTime->GetFreq();
        m_pPrinter->PrintLine(VM_STRING("Encode Latency Stat"));

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
            static vm_char chFrameType[] = VM_STRING("?IP?B???");
            stream<<i<<VM_STRING(":")<<chFrameType[it->FrameType & 0x0F]<<VM_STRING(": ")<<dEnd - dStart<<VM_STRING("  (")<<dStart<<VM_STRING(" ")<<dEnd<<VM_STRING(")");

            summ += dEnd - dStart;
            max_latency = (std::max)(max_latency, dEnd - dStart);
            m_pPrinter->PrintLine(stream.str());
        }

        tstringstream stream;

        stream<<std::setiosflags(std::ios::fixed) << std::setprecision(2);
        stream<<VM_STRING("Encode average latency(ms) = ") << summ / (double)i;
        m_pPrinter->PrintLine(stream.str());
        
        //clearing stream
        stream.str(VM_STRING(""));

        stream<<VM_STRING("Encode max latency(ms) = ") << max_latency;
        m_pPrinter->PrintLine(stream.str());
        
        m_Done.clear();
    }
        
    return InterfaceProxy<IVideoEncode>::Close();
}
