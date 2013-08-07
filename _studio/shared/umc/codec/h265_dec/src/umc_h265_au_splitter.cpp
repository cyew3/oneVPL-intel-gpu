/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "memory"

#include "umc_h265_au_splitter.h"

#include "umc_h265_nal_spl.h"
#include "umc_h265_bitstream.h"
#include "umc_h265_dec_defs_dec.h"
#include "umc_structures.h"

#include "umc_frame_data.h"
#include "umc_h265_dec_debug.h"


namespace UMC_HEVC_DECODER
{

#if 0
static bool IsNeedSPSInvalidate(const H265SeqParamSet *old_sps, const H265SeqParamSet *new_sps)
{
    if (!old_sps || !new_sps)
        return false;

    //if (new_sps->no_output_of_prior_pics_flag)
      //  return true;

    if (old_sps->pic_width_in_luma_samples != new_sps->pic_width_in_luma_samples)
        return true;

    if (old_sps->pic_height_in_luma_samples != new_sps->pic_height_in_luma_samples)
        return true;

    //if (old_sps->max_dec_frame_buffering != new_sps->max_dec_frame_buffering)
      //  return true;

    /*if (old_sps->frame_cropping_rect_bottom_offset != new_sps->frame_cropping_rect_bottom_offset)
        return true;

    if (old_sps->frame_cropping_rect_left_offset != new_sps->frame_cropping_rect_left_offset)
        return true;

    if (old_sps->frame_cropping_rect_right_offset != new_sps->frame_cropping_rect_right_offset)
        return true;

    if (old_sps->frame_cropping_rect_top_offset != new_sps->frame_cropping_rect_top_offset)
        return true;

    if (old_sps->aspect_ratio_idc != new_sps->aspect_ratio_idc)
        return true; */

    return false;
}
#endif

AU_Splitter_H265::AU_Splitter_H265(Heap *heap, Heap_Objects *objectHeap)
    : m_Headers(objectHeap)
    , m_objHeap(objectHeap)
    , m_heap(heap)
{
}

AU_Splitter_H265::~AU_Splitter_H265()
{
    Close();
}

void AU_Splitter_H265::Init(UMC::VideoDecoderParams *)
{
    Close();

    m_pNALSplitter.reset(new NALUnitSplitter_H265(m_heap));
    m_pNALSplitter->Init();
}

void AU_Splitter_H265::Close()
{
    m_pNALSplitter.reset(0);
}

void AU_Splitter_H265::Reset()
{
    if (m_pNALSplitter.get())
        m_pNALSplitter->Reset();

    m_Headers.Reset();
}

Ipp32s AU_Splitter_H265::CheckNalUnitType(UMC::MediaData * src)
{
    return m_pNALSplitter->CheckNalUnitType(src);
}

UMC::MediaDataEx * AU_Splitter_H265::GetNalUnit(UMC::MediaData * src)
{
    return m_pNALSplitter->GetNalUnits(src);
}

NALUnitSplitter_H265 * AU_Splitter_H265::GetNalUnitSplitter()
{
    return m_pNALSplitter.get();
}


} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
