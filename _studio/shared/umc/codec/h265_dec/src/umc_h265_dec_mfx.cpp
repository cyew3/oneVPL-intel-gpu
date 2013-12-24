/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_dec_mfx.h"
#include "umc_h265_dec_tables.h"

namespace UMC_HEVC_DECODER
{

UMC::Status FillVideoParam(const H265SeqParamSet * seq, mfxVideoParam *par, bool full)
{
    par->mfx.CodecId = MFX_CODEC_HEVC;
    par->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

    if (seq->bit_depth_luma > 8 || seq->bit_depth_chroma > 8)
    {
        par->mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
    }

    par->mfx.FrameInfo.Width = (mfxU16) (seq->pic_width_in_luma_samples);
    par->mfx.FrameInfo.Height = (mfxU16) (seq->pic_height_in_luma_samples);

    par->mfx.FrameInfo.Width = UMC::align_value<mfxU16>(par->mfx.FrameInfo.Width, 16);
    par->mfx.FrameInfo.Height = UMC::align_value<mfxU16>(par->mfx.FrameInfo.Height, 16);

    //if (seq->frame_cropping_flag)
    {
        par->mfx.FrameInfo.CropX = (mfxU16)(SubWidthC[seq->chroma_format_idc] * (seq->conf_win_left_offset + seq->def_disp_win_left_offset));
        par->mfx.FrameInfo.CropY = (mfxU16)(SubHeightC[seq->chroma_format_idc] * (seq->conf_win_top_offset + seq->def_disp_win_top_offset));
        par->mfx.FrameInfo.CropH = (mfxU16)(par->mfx.FrameInfo.Height - SubHeightC[seq->chroma_format_idc] *
            (seq->conf_win_top_offset + seq->conf_win_bottom_offset + seq->def_disp_win_top_offset + seq->def_disp_win_bottom_offset));

        par->mfx.FrameInfo.CropW = (mfxU16)(par->mfx.FrameInfo.Width - SubWidthC[seq->chroma_format_idc] *
            (seq->conf_win_left_offset + seq->conf_win_right_offset + seq->def_disp_win_left_offset + seq->def_disp_win_right_offset));

        par->mfx.FrameInfo.CropH -= (mfxU16)(par->mfx.FrameInfo.Height - seq->pic_height_in_luma_samples);
        par->mfx.FrameInfo.CropW -= (mfxU16)(par->mfx.FrameInfo.Width - seq->pic_width_in_luma_samples);
    }

    par->mfx.FrameInfo.PicStruct = (mfxU8) (seq->field_seq_flag  ? MFX_PICSTRUCT_UNKNOWN : MFX_PICSTRUCT_PROGRESSIVE);
    par->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;//seq->chroma_format_idc;

    if (seq->aspect_ratio_info_present_flag || full)
    {
        par->mfx.FrameInfo.AspectRatioW = (mfxU16)seq->sar_width;
        par->mfx.FrameInfo.AspectRatioH = (mfxU16)seq->sar_height;
    }
    else
    {
        par->mfx.FrameInfo.AspectRatioW = 0;
        par->mfx.FrameInfo.AspectRatioH = 0;
    }

    if (seq->getTimingInfo()->vps_timing_info_present_flag || full)
    {
        par->mfx.FrameInfo.FrameRateExtD = seq->getTimingInfo()->vps_num_units_in_tick;
        par->mfx.FrameInfo.FrameRateExtN = seq->getTimingInfo()->vps_time_scale;
    }
    else
    {
        par->mfx.FrameInfo.FrameRateExtD = 0;
        par->mfx.FrameInfo.FrameRateExtN = 0;
    }

    par->mfx.CodecProfile = (mfxU16)seq->m_pcPTL.GetGeneralPTL()->profile_idc;
    par->mfx.CodecLevel = (mfxU16)seq->m_pcPTL.GetGeneralPTL()->level_idc;

    par->mfx.DecodedOrder = 0;

    // video signal section
    mfxExtVideoSignalInfo * videoSignal = (mfxExtVideoSignalInfo *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    if (videoSignal)
    {
        videoSignal->VideoFormat = (mfxU16)seq->video_format;
        videoSignal->VideoFullRange = (mfxU16)seq->video_full_range_flag;
        videoSignal->ColourDescriptionPresent = (mfxU16)seq->colour_description_present_flag;
        videoSignal->ColourPrimaries = (mfxU16)seq->colour_primaries;
        videoSignal->TransferCharacteristics = (mfxU16)seq->transfer_characteristics;
        videoSignal->MatrixCoefficients = (mfxU16)seq->matrix_coeffs;
    }

    return UMC::UMC_OK;
}

} // namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H265_VIDEO_DECODER
