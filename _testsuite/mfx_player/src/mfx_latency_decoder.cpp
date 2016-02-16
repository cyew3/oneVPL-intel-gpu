/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_latency_decoder.h"
#include <iomanip>

LatencyDecoder::LatencyDecoder(bool bAggregateInfo, IStringPrinter * pPrinter, ITime * pTimer, const tstring & name, std::auto_ptr<IYUVSource>&  pTarget)
    : InterfaceProxy<IYUVSource>(pTarget)
    , m_bAggregateInfo(bAggregateInfo)
    , m_bFirstCall(true)
    , m_decodeHeaderTimestamp()
    , m_lastAssignedTimestamp((mfxU64)-1)
    , m_pTime(pTimer)
    , m_pPrinter(NULL == pPrinter ? new ConsolePrinter() : pPrinter)
    , m_name(name)
{
}

LatencyDecoder::~LatencyDecoder()
{
    CloseLocal();
}

mfxStatus LatencyDecoder::DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) 
{
    //
    if (m_bFirstCall)
    {
        m_decodeHeaderTimestamp = m_pTime->GetTick();
        m_bFirstCall = false;
    }
    return InterfaceProxy<IYUVSource>::DecodeHeader(bs, par);
}

mfxStatus LatencyDecoder::DecodeFrameAsync(mfxBitstream2 &bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
{
    //assuming resolution of timer is high enough to have second condition passed only once for each bitstream in handling of
    //wrn device busy
    if ((!bs.isNull && m_lastAssignedTimestamp != bs.TimeStamp) || bs.TimeStamp == (mfxU64)-1)
    {
#if DECODE_HEADER_LATENCY
        if (0 != m_decodeHeaderTimestamp)
        {
            m_lastAssignedTimestamp = bs.TimeStamp = m_decodeHeaderTimestamp;
            m_decodeHeaderTimestamp = 0;
        }
        else
#endif
        {
            m_lastAssignedTimestamp = bs.TimeStamp = m_pTime->GetTick();
        }
    }
    //packing frametype into last 4 bits in timestamp, it donot affect timestamps since they have big resolution
    if (!bs.isNull)
    {
        bs.TimeStamp = (bs.TimeStamp & ~0x0F) | bs.FrameType;
    }
    
    mfxStatus sts =  m_pTarget->DecodeFrameAsync(bs, surface_work, surface_out, syncp);

    if (!m_bAggregateInfo && sts >= MFX_ERR_NONE && sts != MFX_WRN_DEVICE_BUSY && syncp != NULL && *syncp != NULL && surface_out != NULL && *surface_out != NULL)
    {
        FrameRecord rc;
        rc.nTimeSplitted = (*surface_out)->Data.TimeStamp & ~0x0F;
        rc.syncp         = *syncp;
        rc.FrameType     = (mfxU16)((*surface_out)->Data.TimeStamp & 0x0F);
        m_Queued.push_back(rc);
        m_lastAssignedTimestamp = (mfxU64)-1;
    }
    return sts;
}

mfxStatus LatencyDecoder::SyncOperation(mfxSyncPoint syncp, mfxU32 wait) 
{ 
    mfxStatus sts = m_pTarget->SyncOperation(syncp, wait); 

    if (sts >= MFX_ERR_NONE && sts != MFX_WRN_IN_EXECUTION)
    {
        FrameRecord record;        
        
        //searching for syncpoint should match to first
        if (!m_Queued.empty() && syncp == m_Queued.front().syncp)
        {
            m_Decoded.push_back(m_Queued.front());
            m_Decoded.back().nTimeDecoded = m_pTime->GetTick();
            m_Queued.pop_front();
        }
    }

    return sts;
}

mfxStatus LatencyDecoder::Close() 
{
    MFX_CHECK_STS(CloseLocal());

    return InterfaceProxy<IYUVSource>::Close(); 
}

mfxStatus LatencyDecoder::CloseLocal() 
{
    //in case of agregating decoder is first component and printing will be done in next
    if (!m_bAggregateInfo && !m_Decoded.empty())
    {
        double dFreq = (double)m_pTime->GetFreq();
        m_pPrinter->Print(m_name);
        m_pPrinter->PrintLine(VM_STRING(" Latency Stat"));

        //print statisticks here
        //for now lets print all frames offsets
        std::list<FrameRecord> ::iterator it;
        int i = 0;
        double summ = 0;
        double max_latency = 0;

        for(it = m_Decoded.begin(); it!= m_Decoded.end(); it++, i++)
        {
            tstringstream stream;

            double dStart = 1000.0*(double)(it->nTimeSplitted) / dFreq;
            double dEnd   = 1000.0*(double)(it->nTimeDecoded)  / dFreq;
            
            stream<<std::setiosflags(std::ios::fixed) << std::setprecision(2);
            static vm_char chFrameType[] = VM_STRING("?IP?B???");
            stream<<i<<VM_STRING(":")<<chFrameType[it->FrameType]<<VM_STRING(": ")<<dEnd - dStart<<VM_STRING("  (")<<dStart<<VM_STRING(" ")<<dEnd<<VM_STRING(")");

            summ += dEnd - dStart;
            max_latency = (std::max)(max_latency, dEnd - dStart);
         
            m_pPrinter->PrintLine(stream.str());
        }

        tstringstream stream;

        stream<<std::setiosflags(std::ios::fixed) << std::setprecision(2);
        stream<<m_name<<VM_STRING(" average latency(ms) = ") << summ / (double)i;
        m_pPrinter->PrintLine(stream.str());

        //clearing stream
        stream.str(VM_STRING(""));

        stream<<m_name<<VM_STRING(" max latency(ms) = ") << max_latency;
        m_pPrinter->PrintLine(stream.str());

        m_Decoded.clear();
    }

    return MFX_ERR_NONE;
}
