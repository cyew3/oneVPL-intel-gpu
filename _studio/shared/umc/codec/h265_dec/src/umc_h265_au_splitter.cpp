//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include <memory>
#include "umc_h265_au_splitter.h"
#include "umc_h265_nal_spl.h"

namespace UMC_HEVC_DECODER
{

AU_Splitter_H265::AU_Splitter_H265()
    : m_Headers(&m_ObjHeap)
{
}

AU_Splitter_H265::~AU_Splitter_H265()
{
    Close();
}

void AU_Splitter_H265::Init(UMC::VideoDecoderParams *)
{
    Close();

    m_pNALSplitter.reset(new NALUnitSplitter_H265());
    m_pNALSplitter->Init();
}

void AU_Splitter_H265::Close()
{
    m_pNALSplitter.reset(0);
    m_Headers.Reset(false);
    m_ObjHeap.Release();
}

void AU_Splitter_H265::Reset()
{
    if (m_pNALSplitter.get())
        m_pNALSplitter->Reset();

    m_Headers.Reset(false);
    m_ObjHeap.Release();
}

// Wrapper for NAL unit splitter CheckNalUnitType
Ipp32s AU_Splitter_H265::CheckNalUnitType(UMC::MediaData * src)
{
    return m_pNALSplitter->CheckNalUnitType(src);
}

// Wrapper for NAL unit splitter CheckNalUnitType GetNalUnit
UMC::MediaDataEx * AU_Splitter_H265::GetNalUnit(UMC::MediaData * src)
{
    return m_pNALSplitter->GetNalUnits(src);
}

// Returns internal NAL unit splitter
NALUnitSplitter_H265 * AU_Splitter_H265::GetNalUnitSplitter()
{
    return m_pNALSplitter.get();
}


} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
