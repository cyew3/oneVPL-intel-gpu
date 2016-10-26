//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2012 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined (MFX_ENABLE_H264_VIDEO_PAK)

#include <assert.h>
#include "mfx_h264_pak_pack.h"
#include "mfx_h264_pak_utils.h"

using namespace H264Pak;

H264PakBitstream::H264PakBitstream()
{
    memset(&m_bs, 0, sizeof(m_bs));
}

void H264PakBitstream::StartPicture(const mfxFrameParamAVC& fp, void* buf, mfxU32 size)
{
    UMC::Status tmp;
    H264BsReal_Create_8u16s(&m_bs, (mfxU8 *)buf, size, fp.ChromaFormatIdc, tmp);
    H264BsReal_Reset_8u16s(&m_bs);
}

void H264PakBitstream::StartSlice(const mfxFrameParamAVC& fp, const mfxSliceParamAVC& sp)
{
    m_prevMbQpY = 26 + fp.PicInitQpMinus26 + sp.SliceQpDelta;
    m_prevMbQpDelta = 0;
    m_skipRun = 0;

    if (fp.EntropyCodingModeFlag)
    {
        if (sp.SliceType & MFX_SLICETYPE_I)
        {
            H264BsReal_InitializeContextVariablesIntra_CABAC_8u16s(&m_bs, m_prevMbQpY);
        }
        else
        {
            H264BsReal_InitializeContextVariablesInter_CABAC_8u16s(&m_bs, m_prevMbQpY, sp.CabacInitIdc);
        }
    }
}

void H264PakBitstream::EndSlice()
{
}

void H264PakBitstream::EndPicture()
{
}

static void PackStartCode(
    H264BsReal_8u16s& bitstream,
    mfxU32 nalRefIdc,
    mfxU32 nalUnitType)
{
    H264BsReal_PutBits_8u16s(&bitstream, 0, 8);
    H264BsReal_PutBits_8u16s(&bitstream, 0, 8);
    H264BsReal_PutBits_8u16s(&bitstream, 0, 8);
    H264BsReal_PutBits_8u16s(&bitstream, 1, 8);
    H264BsReal_PutBits_8u16s(&bitstream, 0, 1); // forbidden_zero_bit
    H264BsReal_PutBits_8u16s(&bitstream, nalRefIdc & 0x03, 2);
    H264BsReal_PutBits_8u16s(&bitstream, nalUnitType & 0x1f, 5);
}

void H264PakBitstream::PackAccessUnitDelimiter(
    mfxU32 picType)
{
    assert(picType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B));
    mfxU32 value = (picType & MFX_FRAMETYPE_I) ? 0 : (picType & MFX_FRAMETYPE_P) ? 1 : 2;

    PackStartCode(m_bs, 0, NAL_UT_AUD);
    H264BsReal_PutBits_8u16s(&m_bs, value, 3);
    H264BsBase_WriteTrailingBits(&m_bs.m_base);
}

static const mfxU8 EXTENDED_SAR = 0xff;

static mfxU8 GetAspectRatioIdc(mfxU32 sarWidth, mfxU32 sarHeight)
{
    static const struct { mfxU16 w, h; } s_TableE1[] =
    {
        {   0,   0 }, {   1,   1 }, {  12,  11 }, {  10,  11 }, {  16,  11 },
        {  40,  33 }, {  24,  11 }, {  20,  11 }, {  32,  11 }, {  80,  33 },
        {  18,  11 }, {  15,  11 }, {  64,  33 }, { 160,  99 }, {   4,   3 },
        {   3,   2 }, {   2,   1 }
    };
    assert(sizeof(s_TableE1) < 255 * sizeof(s_TableE1[0]));

    if (sarWidth == 0 || sarHeight == 0)
    {
        return 0;
    }

    for (mfxU8 i = 1; i < sizeof(s_TableE1) / sizeof(s_TableE1[0]); i++)
    {
        if ((sarWidth % s_TableE1[i].w) == 0 &&
            (sarHeight % s_TableE1[i].h) == 0 &&
            (sarWidth / s_TableE1[i].w) == (sarHeight / s_TableE1[i].h))
        {
            return i;
        }
    }

    return EXTENDED_SAR;
}

namespace H264Pak
{
    struct HrdParam
    {
        HrdParam() { memset(this, 0, sizeof(*this)); }

        mfxU8 nal_cpb_cnt_minus1;
        mfxU8 bit_rate_scale;
        mfxU8 cpb_size_scale;
        mfxU32 bit_rate_value_minus1[32];
        mfxU32 cpb_size_value_minus1[32];
        mfxU8 cbr_flag[32];
        mfxU8 initial_cpb_removal_delay_length_minus1;
        mfxU8 cpb_removal_delay_length_minus1;
        mfxU8 dpb_output_delay_length_minus1;
        mfxU8 time_offset_length;
    };
}

static void FillHrdParam(const mfxVideoParam& vp, HrdParam& hrd)
{
    hrd.cpb_size_scale = 3;
    hrd.bit_rate_value_minus1[0] = ((1000 * vp.mfx.MaxKbps) >> (6 + hrd.bit_rate_scale)) - 1;
    hrd.cpb_size_value_minus1[0] = ((1000 * vp.mfx.BufferSizeInKB) >> (1 + hrd.cpb_size_scale)) - 1;
    hrd.cbr_flag[0] = (vp.mfx.RateControlMethod == MFX_RATECONTROL_CBR);
    hrd.initial_cpb_removal_delay_length_minus1 = 23;
    hrd.cpb_removal_delay_length_minus1 = 23;
    hrd.dpb_output_delay_length_minus1 = 23;
    hrd.time_offset_length = 24;
}

static void PackHrdParam(
    H264BsReal_8u16s& bitstream,
    const HrdParam& hrd)
{
    assert(hrd.nal_cpb_cnt_minus1 <= 31);
    mfxU32 nal_cpb_cnt = IPP_MIN(hrd.nal_cpb_cnt_minus1 + 1, 32);

    H264BsReal_PutVLCCode_8u16s(&bitstream, hrd.nal_cpb_cnt_minus1);
    H264BsReal_PutBits_8u16s(&bitstream, hrd.bit_rate_scale, 4);
    H264BsReal_PutBits_8u16s(&bitstream, hrd.cpb_size_scale, 4);

    for (mfxU32 i = 0; i < nal_cpb_cnt; i++)
    {
        assert(hrd.bit_rate_value_minus1[i] != 0xffffffff); // 0 ... 2^32 - 2
        assert(hrd.cpb_size_value_minus1[i] != 0xffffffff); // 0 ... 2^32 - 2
        H264BsReal_PutVLCCode_8u16s(&bitstream, IPP_MIN(hrd.bit_rate_value_minus1[i], 0xfffffffe));
        H264BsReal_PutVLCCode_8u16s(&bitstream, IPP_MIN(hrd.cpb_size_value_minus1[i], 0xfffffffe));
        H264BsReal_PutBit_8u16s(&bitstream, hrd.cbr_flag[i]);
    }

    H264BsReal_PutBits_8u16s(&bitstream, hrd.initial_cpb_removal_delay_length_minus1, 5);
    H264BsReal_PutBits_8u16s(&bitstream, hrd.cpb_removal_delay_length_minus1, 5);
    H264BsReal_PutBits_8u16s(&bitstream, hrd.dpb_output_delay_length_minus1, 5);
    H264BsReal_PutBits_8u16s(&bitstream, hrd.time_offset_length, 5);
}

void H264PakBitstream::PackSeqParamSet(
    const mfxVideoParam& vp,
    const mfxFrameParamAVC& fp,
    bool vuiParamPresentFlag)
{
    PackStartCode(m_bs, 1, NAL_UT_SPS);
    H264BsReal_PutBits_8u16s(&m_bs, vp.mfx.CodecProfile, 8);
    H264BsReal_PutBit_8u16s(&m_bs, 0); // constraint_set0_flag
    H264BsReal_PutBit_8u16s(&m_bs, 1); // constraint_set1_flag
    H264BsReal_PutBit_8u16s(&m_bs, 0); // constraint_set2_flag
    H264BsReal_PutBit_8u16s(&m_bs, 0); // constraint_set3_flag
    H264BsReal_PutBits_8u16s(&m_bs, 0, 4); // 5 reserved zero bits
    H264BsReal_PutBits_8u16s(&m_bs, vp.mfx.CodecLevel, 8);
    H264BsReal_PutVLCCode_8u16s(&m_bs, 0); // seq_parameter_set_id

    if (vp.mfx.CodecProfile >= MFX_PROFILE_AVC_HIGH)
    {
        H264BsReal_PutVLCCode_8u16s(&m_bs, fp.ChromaFormatIdc);

        if (fp.ChromaFormatIdc == 3)
        {
            H264BsReal_PutBit_8u16s(&m_bs, fp.ResColorTransFlag);
        }

        H264BsReal_PutVLCCode_8u16s(&m_bs, fp.BitDepthLumaMinus8);
        H264BsReal_PutVLCCode_8u16s(&m_bs, fp.BitDepthChromaMinus8);
        H264BsReal_PutBit_8u16s(&m_bs, 0); // qpprime_y_zero_transform_bypass_flag
        H264BsReal_PutBit_8u16s(&m_bs, 0); // seq_scaling_matrix_present_flag
    }

    H264BsReal_PutVLCCode_8u16s(&m_bs, fp.Log2MaxFrameCntMinus4);
    H264BsReal_PutVLCCode_8u16s(&m_bs, fp.PicOrderCntType);

    if (fp.PicOrderCntType == 0)
    {
        H264BsReal_PutVLCCode_8u16s(&m_bs, fp.Log2MaxPicOrdCntMinus4);
    }

    mfxU32 pic_height_in_map_units = vp.mfx.FrameInfo.Height >> (5 - fp.FrameMbsOnlyFlag);
    assert(pic_height_in_map_units > 0);

    H264BsReal_PutVLCCode_8u16s(&m_bs, vp.mfx.NumRefFrame);
    H264BsReal_PutBit_8u16s(&m_bs, fp.GapsInFrameNumAllowed);
    H264BsReal_PutVLCCode_8u16s(&m_bs, fp.FrameWinMbMinus1);
    H264BsReal_PutVLCCode_8u16s(&m_bs, pic_height_in_map_units - 1);
    H264BsReal_PutBit_8u16s(&m_bs, fp.FrameMbsOnlyFlag);

    if (fp.FrameMbsOnlyFlag == 0)
    {
        H264BsReal_PutBit_8u16s(&m_bs, !fp.FieldPicFlag && fp.MbaffFrameFlag);
    }

    H264BsReal_PutBit_8u16s(&m_bs, fp.Direct8x8InferenceFlag);

    if (vp.mfx.FrameInfo.CropX > 0 || vp.mfx.FrameInfo.CropW < vp.mfx.FrameInfo.Width ||
        vp.mfx.FrameInfo.CropY > 0 || vp.mfx.FrameInfo.CropH < vp.mfx.FrameInfo.Height)
    {
        mfxI32 leftCrop = vp.mfx.FrameInfo.CropX / 2;
        mfxI32 rightCrop = (vp.mfx.FrameInfo.Width - vp.mfx.FrameInfo.CropW - vp.mfx.FrameInfo.CropX) / 2;
        mfxI32 topCrop = vp.mfx.FrameInfo.CropY / 2;
        mfxI32 bottomCrop = (vp.mfx.FrameInfo.Height - vp.mfx.FrameInfo.CropH - vp.mfx.FrameInfo.CropY) / 2;

        topCrop /= (2 - fp.FrameMbsOnlyFlag);
        bottomCrop /= (2 - fp.FrameMbsOnlyFlag);

        H264BsReal_PutBit_8u16s(&m_bs, 1); // frame_cropping_flag
        H264BsReal_PutVLCCode_8u16s(&m_bs, leftCrop);
        H264BsReal_PutVLCCode_8u16s(&m_bs, rightCrop);
        H264BsReal_PutVLCCode_8u16s(&m_bs, topCrop);
        H264BsReal_PutVLCCode_8u16s(&m_bs, bottomCrop);
    }
    else
    {
        H264BsReal_PutBit_8u16s(&m_bs, 0); // frame_cropping_flag
    }

    const mfxExtCodingOption& opt = GetExtCodingOption(vp);

    H264BsReal_PutBit_8u16s(&m_bs, vuiParamPresentFlag);

    if (vuiParamPresentFlag)
    {
        const mfxFrameInfo& fi = vp.mfx.FrameInfo;

        mfxU8 pic_struct_present_flag =
            GetMfxOptDefaultOff(opt.VuiNalHrdParameters) ||
            GetMfxOptDefaultOff(opt.PicTimingSEI);

        mfxU8 aspect_ratio_info_present_flag = fi.AspectRatioH && fi.AspectRatioW;
        mfxU8 timing_info_present_flag = fi.FrameRateExtN > 0 && fi.FrameRateExtD > 0;
        mfxU8 overscan_info_present_flag = 0;
        mfxU8 video_signal_type_present_flag = 0;
        mfxU8 chroma_loc_info_present_flag = 0;
        mfxU8 bs_restriction_flag = 0;

        mfxU8 nal_hrd_parameters_present_flag = GetMfxOptDefaultOff(opt.VuiNalHrdParameters);
        mfxU8 vcl_hrd_parameters_present_flag = 0;

        H264BsReal_PutBit_8u16s(&m_bs, aspect_ratio_info_present_flag);
        if (aspect_ratio_info_present_flag)
        {
            mfxU8 aspect_ratio_idc = GetAspectRatioIdc(fi.AspectRatioW, fi.AspectRatioH);
            H264BsReal_PutBits_8u16s(&m_bs, aspect_ratio_idc, 8);

            if (aspect_ratio_idc == EXTENDED_SAR)
            {
                H264BsReal_PutBits_8u16s(&m_bs, vp.mfx.FrameInfo.AspectRatioW, 16);
                H264BsReal_PutBits_8u16s(&m_bs, vp.mfx.FrameInfo.AspectRatioH, 16);
            }
        }

        H264BsReal_PutBit_8u16s(&m_bs, overscan_info_present_flag);
        if (overscan_info_present_flag)
        {
            mfxU8 overscan_appropriate_flag = 0;
            H264BsReal_PutBit_8u16s(&m_bs, overscan_appropriate_flag);
        }

        H264BsReal_PutBit_8u16s(&m_bs, video_signal_type_present_flag);
        if (video_signal_type_present_flag)
        {
            mfxU8 video_format = 0;
            mfxU8 video_full_range_flag = 0;
            mfxU8 colour_description_present_flag = 0;

            H264BsReal_PutBits_8u16s(&m_bs, video_format, 3);
            H264BsReal_PutBit_8u16s(&m_bs, video_full_range_flag);
            H264BsReal_PutBit_8u16s(&m_bs, colour_description_present_flag);

            if (colour_description_present_flag)
            {
                mfxU8 colour_primaries = 0;
                mfxU8 transfer_characteristics = 0;
                mfxU8 matrix_coefficients = 0;

                H264BsReal_PutBits_8u16s(&m_bs, colour_primaries, 8);
                H264BsReal_PutBits_8u16s(&m_bs, transfer_characteristics, 8);
                H264BsReal_PutBits_8u16s(&m_bs, matrix_coefficients, 8);
            }
        }

        H264BsReal_PutBit_8u16s(&m_bs, chroma_loc_info_present_flag);
        if (chroma_loc_info_present_flag)
        {
            mfxU8 chroma_sample_loc_type_top_field = 0;
            mfxU8 chroma_sample_loc_type_bottom_field = 0;

            H264BsReal_PutVLCCode_8u16s(&m_bs, chroma_sample_loc_type_top_field);
            H264BsReal_PutVLCCode_8u16s(&m_bs, chroma_sample_loc_type_bottom_field);
        }

        H264BsReal_PutBit_8u16s(&m_bs, timing_info_present_flag);
        if (timing_info_present_flag)
        {
            mfxU32 num_units_in_tick = vp.mfx.FrameInfo.FrameRateExtD;
            mfxU32 time_scale = 2 * vp.mfx.FrameInfo.FrameRateExtN;
            mfxU8 fixed_frame_rate_flag = 1;

            H264BsReal_PutBits_8u16s(&m_bs, num_units_in_tick >> 16, 16);
            H264BsReal_PutBits_8u16s(&m_bs, num_units_in_tick & 0xffff, 16);
            H264BsReal_PutBits_8u16s(&m_bs, time_scale >> 16, 16);
            H264BsReal_PutBits_8u16s(&m_bs, time_scale & 0xffff, 16);
            H264BsReal_PutBit_8u16s(&m_bs, fixed_frame_rate_flag);
        }

        H264BsReal_PutBit_8u16s(&m_bs, nal_hrd_parameters_present_flag);
        if (nal_hrd_parameters_present_flag)
        {
            HrdParam hrd;
            FillHrdParam(vp, hrd);
            PackHrdParam(m_bs, hrd);
        }

        H264BsReal_PutBit_8u16s(&m_bs, vcl_hrd_parameters_present_flag);
        if (vcl_hrd_parameters_present_flag)
        {
            HrdParam hrd;
            FillHrdParam(vp, hrd);
            PackHrdParam(m_bs, hrd);
        }

        if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
        {
            mfxU8 low_delay_hrd_flag = 0;
            H264BsReal_PutBit_8u16s(&m_bs, low_delay_hrd_flag);
        }

        H264BsReal_PutBit_8u16s(&m_bs, pic_struct_present_flag);
        H264BsReal_PutBit_8u16s(&m_bs, bs_restriction_flag);
        if (bs_restriction_flag)
        {
            mfxU8 motion_vectors_over_pic_boundaries_flag = 0;
            mfxU32 max_bytes_per_pic_denom = 0;
            mfxU32 max_bits_per_mb_denom = 0;
            mfxU8 log2_max_mv_length_horizontal = 0;
            mfxU8 log2_max_mv_length_vertical = 0;
            mfxU8 num_reorder_frames = 0;
            mfxU16 max_dec_frame_buffering = 0;

            H264BsReal_PutBit_8u16s(&m_bs, motion_vectors_over_pic_boundaries_flag);
            H264BsReal_PutVLCCode_8u16s(&m_bs, max_bytes_per_pic_denom);
            H264BsReal_PutVLCCode_8u16s(&m_bs, max_bits_per_mb_denom);
            H264BsReal_PutVLCCode_8u16s(&m_bs, log2_max_mv_length_horizontal);
            H264BsReal_PutVLCCode_8u16s(&m_bs, log2_max_mv_length_vertical);
            H264BsReal_PutVLCCode_8u16s(&m_bs, num_reorder_frames);
            H264BsReal_PutVLCCode_8u16s(&m_bs, max_dec_frame_buffering);
        }
    }

    H264BsBase_WriteTrailingBits(&m_bs.m_base);
}

void H264PakBitstream::PackPicParamSet(
    const mfxVideoParam& vp,
    const mfxFrameParamAVC& fp)
{
    PackStartCode(m_bs, 1, NAL_UT_PPS);
    H264BsReal_PutVLCCode_8u16s(&m_bs, 0); // pic_parameter_set_id
    H264BsReal_PutVLCCode_8u16s(&m_bs, 0); // seq_parameter_set_id
    H264BsReal_PutBit_8u16s(&m_bs, fp.EntropyCodingModeFlag);
    H264BsReal_PutBit_8u16s(&m_bs, fp.PicOrderPresent);
    H264BsReal_PutVLCCode_8u16s(&m_bs, fp.NumSliceGroupMinus1);
    H264BsReal_PutVLCCode_8u16s(&m_bs, fp.NumRefIdxL0Minus1);
    H264BsReal_PutVLCCode_8u16s(&m_bs, fp.NumRefIdxL1Minus1);
    H264BsReal_PutBit_8u16s(&m_bs, fp.WeightedPredFlag);
    H264BsReal_PutBits_8u16s(&m_bs, fp.WeightedBipredIdc, 2);
    H264BsReal_PutVLCCode_8u16s(&m_bs, SIGNED_VLC_CODE(fp.PicInitQpMinus26));
    H264BsReal_PutVLCCode_8u16s(&m_bs, SIGNED_VLC_CODE(fp.PicInitQsMinus26));
    H264BsReal_PutVLCCode_8u16s(&m_bs, SIGNED_VLC_CODE(fp.ChromaQp1stOffset));
    H264BsReal_PutBit_8u16s(&m_bs, fp.ILDBControlPresent);
    H264BsReal_PutBit_8u16s(&m_bs, fp.ConstraintIntraFlag);
    H264BsReal_PutBit_8u16s(&m_bs, fp.RedundantPicCntPresent);

    if (vp.mfx.CodecProfile >= MFX_PROFILE_AVC_HIGH)
    {
        H264BsReal_PutBit_8u16s(&m_bs, fp.Transform8x8Flag);
        H264BsReal_PutBit_8u16s(&m_bs, fp.ScalingListPresent);

        if (fp.ScalingListPresent)
        {
            assert(!"scaling matrices coding is not supported");
        }

        H264BsReal_PutVLCCode_8u16s(&m_bs, SIGNED_VLC_CODE(fp.ChromaQp2ndOffset));
    }

    H264BsBase_WriteTrailingBits(&m_bs.m_base);
}

static void PackSeiHeader(
    H264BsReal_8u16s& bitstream,
    mfxU32 payloadType,
    mfxU32 payloadSize)
{
    while (payloadType >= 255)
    {
        H264BsReal_PutBits_8u16s(&bitstream, 0xff, 8);
        payloadType -= 255;
    }

    H264BsReal_PutBits_8u16s(&bitstream, payloadType, 8);

    while (payloadSize >= 255)
    {
        H264BsReal_PutBits_8u16s(&bitstream, 0xff, 8);
        payloadSize -= 255;
    }

    H264BsReal_PutBits_8u16s(&bitstream, payloadSize, 8);
}

static void PackSeiBufferingPeriod(
    H264BsReal_8u16s& bitstream,
    const mfxExtAvcSeiBufferingPeriod& msg)
{
    mfxU32 dataSizeInBits = 2 * msg.initial_cpb_removal_delay_length * (msg.nal_cpb_cnt + msg.vcl_cpb_cnt);
    dataSizeInBits += CeilLog2(msg.seq_parameter_set_id) * 2 + 1;
    mfxU32 dataSizeInBytes = (dataSizeInBits + 7) >> 3;

    PackSeiHeader(bitstream, SEI_TYPE_BUFFERING_PERIOD, dataSizeInBytes);
    H264BsReal_PutVLCCode_8u16s(&bitstream, msg.seq_parameter_set_id);

    for (mfxU32 i = 0; i < msg.nal_cpb_cnt; i++)
    {
        H264BsReal_PutBits_8u16s(
            &bitstream,
            msg.nal_initial_cpb_removal_delay[i],
            msg.initial_cpb_removal_delay_length);
        H264BsReal_PutBits_8u16s(
            &bitstream,
            msg.nal_initial_cpb_removal_delay_offset[i],
            msg.initial_cpb_removal_delay_length);
    }

    for (mfxU32 i = 0; i < msg.vcl_cpb_cnt; i++)
    {
        H264BsReal_PutBits_8u16s(
            &bitstream,
            msg.vcl_initial_cpb_removal_delay[i],
            msg.initial_cpb_removal_delay_length);
        H264BsReal_PutBits_8u16s(
            &bitstream,
            msg.vcl_initial_cpb_removal_delay_offset[i],
            msg.initial_cpb_removal_delay_length);
    }

    if (H264BsBase_GetBsOffset(&bitstream.m_base) & 0x07)
    {
        H264BsBase_WriteTrailingBits(&bitstream.m_base);
    }
}

static void PackSeiPicTiming(
    H264BsReal_8u16s& bitstream,
    const mfxExtPictureTimingSEI& extPicTiming,
    const mfxExtAvcSeiPicTiming& msg)
{
    msg;

    mfxU32 dataSizeInBits = 0;

    if (msg.cpb_dpb_delays_present_flag)
    {
        dataSizeInBits += msg.cpb_removal_delay_length;
        dataSizeInBits += msg.dpb_output_delay_length;
    }

    static const mfxU32 s_numClockTS[9] = { 1, 1, 1, 2, 2, 3, 3, 2, 3 };

    if (msg.pic_struct_present_flag)
    {
        dataSizeInBits += 4; // msg.pic_struct;

        assert(msg.pic_struct <= 8);
        mfxU32 numClockTS = s_numClockTS[IPP_MIN(msg.pic_struct, 8)];

        dataSizeInBits += numClockTS; // clock_timestamp_flag[i]
        for (mfxU32 i = 0; i < numClockTS; i++)
        {
            if (extPicTiming.TimeStamp[i].ClockTimestampFlag)
            {
                mfxU32 tsSize = 19;

                if (extPicTiming.TimeStamp[i].FullTimestampFlag)
                {
                    tsSize += 17;
                }
                else
                {
                    tsSize += ((
                        extPicTiming.TimeStamp[i].HoursFlag * 5 + 7) *
                        extPicTiming.TimeStamp[i].MinutesFlag + 7) *
                        extPicTiming.TimeStamp[i].SecondsFlag + 1;
                }

                dataSizeInBits += tsSize + msg.time_offset_length;
            }
        }
    }


    mfxU32 dataSizeInBytes = (dataSizeInBits + 7) >> 3;
    PackSeiHeader(bitstream, SEI_TYPE_PIC_TIMING, dataSizeInBytes);

    if (msg.cpb_dpb_delays_present_flag)
    {
        H264BsReal_PutBits_8u16s(&bitstream, msg.cpb_removal_delay, msg.cpb_removal_delay_length);
        H264BsReal_PutBits_8u16s(&bitstream, msg.dpb_output_delay, msg.dpb_output_delay_length);
    }

    if (msg.pic_struct_present_flag)
    {
        assert(msg.pic_struct <= 8);
        mfxU32 numClockTS = s_numClockTS[IPP_MIN(msg.pic_struct, 8)];

        H264BsReal_PutBits_8u16s(&bitstream, msg.pic_struct, 4);
        for (mfxU32 i = 0; i < numClockTS; i ++)
        {
            H264BsReal_PutBit_8u16s(&bitstream, extPicTiming.TimeStamp[i].ClockTimestampFlag);
            if (extPicTiming.TimeStamp[i].ClockTimestampFlag)
            {
                H264BsReal_PutBits_8u16s(&bitstream, extPicTiming.TimeStamp[i].CtType, 2);
                H264BsReal_PutBit_8u16s (&bitstream, extPicTiming.TimeStamp[i].NuitFieldBasedFlag);
                H264BsReal_PutBits_8u16s(&bitstream, extPicTiming.TimeStamp[i].CountingType, 5);
                H264BsReal_PutBit_8u16s (&bitstream, extPicTiming.TimeStamp[i].FullTimestampFlag);
                H264BsReal_PutBit_8u16s (&bitstream, extPicTiming.TimeStamp[i].DiscontinuityFlag);
                H264BsReal_PutBit_8u16s (&bitstream, extPicTiming.TimeStamp[i].CntDroppedFlag);
                H264BsReal_PutBits_8u16s(&bitstream, extPicTiming.TimeStamp[i].NFrames, 8);
                if (extPicTiming.TimeStamp[i].FullTimestampFlag)
                {
                    H264BsReal_PutBits_8u16s(&bitstream, extPicTiming.TimeStamp[i].SecondsValue, 6);
                    H264BsReal_PutBits_8u16s(&bitstream, extPicTiming.TimeStamp[i].MinutesValue, 6);
                    H264BsReal_PutBits_8u16s(&bitstream, extPicTiming.TimeStamp[i].HoursValue,   5);
                }
                else
                {
                    H264BsReal_PutBit_8u16s(&bitstream, extPicTiming.TimeStamp[i].SecondsFlag);
                    if (extPicTiming.TimeStamp[i].SecondsFlag)
                    {
                        H264BsReal_PutBits_8u16s(&bitstream, extPicTiming.TimeStamp[i].SecondsValue, 6);
                        H264BsReal_PutBit_8u16s(&bitstream, extPicTiming.TimeStamp[i].MinutesFlag);
                        if (extPicTiming.TimeStamp[i].MinutesFlag)
                        {
                            H264BsReal_PutBits_8u16s(&bitstream, extPicTiming.TimeStamp[i].MinutesValue, 6);
                            H264BsReal_PutBit_8u16s(&bitstream, extPicTiming.TimeStamp[i].HoursFlag);
                            if (extPicTiming.TimeStamp[i].HoursFlag)
                                H264BsReal_PutBits_8u16s(&bitstream, extPicTiming.TimeStamp[i].HoursValue, 5);
                        }
                    }
                }

                H264BsReal_PutBits_8u16s(&bitstream, extPicTiming.TimeStamp[i].TimeOffset, msg.time_offset_length);
            }
        }
    }

    H264BsBase_WriteTrailingBits(&bitstream.m_base);
}

static void PackFormattedSeiMessages(
    H264BsReal_8u16s& bitstream,
    const mfxExtAvcSeiMessage& formattedSeiMessages)
{
    mfxU32 first = formattedSeiMessages.layout == MFX_PAYLOAD_LAYOUT_ODD ? 1 : 0;
    mfxU32 step  = formattedSeiMessages.layout == MFX_PAYLOAD_LAYOUT_ALL ? 1 : 2;

    for (mfxU32 i = first; i < formattedSeiMessages.numPayload; i += step)
    {
        mfxPayload& payload = *formattedSeiMessages.payload[i];
        assert(payload.Data);
        assert(payload.Type <= 21);
        assert(payload.NumBit <= 8u * payload.BufSize);
        assert(payload.Type != 0);
        assert(payload.Type != 1);

        for (mfxU32 byte = 0; byte < payload.NumBit / 8; ++byte)
        {
            H264BsReal_PutBits_8u16s(&bitstream.m_base, payload.Data[byte], 8);
        }
    }
}

void H264PakBitstream::PackSei(
    const mfxExtAvcSeiBufferingPeriod* bufferingPeriod,
    const mfxExtAvcSeiPicTiming* picTiming,
    const mfxExtAvcSeiMessage* formattedSeiMessages)
{
    if (bufferingPeriod || picTiming || formattedSeiMessages)
    {
        PackStartCode(m_bs, 0, NAL_UT_SEI);

        if (bufferingPeriod)
        {
            PackSeiBufferingPeriod(m_bs, *bufferingPeriod);
        }

        if (picTiming)
        {
            mfxExtPictureTimingSEI extPicTiming = { 0 };
            PackSeiPicTiming(m_bs, extPicTiming, *picTiming);
        }

        if (formattedSeiMessages)
        {
            PackFormattedSeiMessages(m_bs, *formattedSeiMessages);
        }

        H264BsBase_WriteTrailingBits(&m_bs.m_base);
    }
}

void H264PakBitstream::PackSliceHeader(
    const mfxVideoParam& vp,
    const mfxFrameParamAVC& fp,
    const mfxExtAvcRefFrameParam& refFrameParam,
    const mfxSliceParamAVC& sp)
{
    mfxU32 sliceType = (sp.SliceType & MFX_SLICETYPE_I) ? 2 : (sp.SliceType & MFX_SLICETYPE_P) ? 0 : 1;
    mfxU32 firstMbInSlice = sp.FirstMbX + sp.FirstMbY * (fp.FrameWinMbMinus1 + 1);
    mfxU32 bottomFieldFlag = fp.FrameType & MFX_FRAMETYPE_BFF ? 1 : 0;
    const mfxExtAvcRefPicInfo& refPicInfo = refFrameParam.PicInfo[bottomFieldFlag];

    mfxU32 nalUnitType = (fp.FrameType & MFX_FRAMETYPE_IDR) ? NAL_UT_IDR_SLICE : NAL_UT_SLICE;
    PackStartCode(m_bs, fp.RefPicFlag, nalUnitType);

    H264BsReal_PutVLCCode_8u16s(&m_bs, firstMbInSlice);
    H264BsReal_PutVLCCode_8u16s(&m_bs, sliceType);
    H264BsReal_PutVLCCode_8u16s(&m_bs, 0); // pic_parameter_set_id
    H264BsReal_PutBits_8u16s(&m_bs, refFrameParam.FrameNum, fp.Log2MaxFrameCntMinus4 + 4);

    if (fp.FrameMbsOnlyFlag == 0)
    {
        H264BsReal_PutBit_8u16s(&m_bs, fp.FieldPicFlag);

        if (fp.FieldPicFlag == 1)
        {
            H264BsReal_PutBit_8u16s(&m_bs, bottomFieldFlag);
        }
    }

    if (fp.FrameType & MFX_FRAMETYPE_IDR)
    {
        H264BsReal_PutVLCCode_8u16s(&m_bs, sp.IdrPictureId);
    }

    if (fp.PicOrderCntType == 0)
    {
        H264BsReal_PutBits_8u16s(
            &m_bs,
            refPicInfo.FieldOrderCnt[bottomFieldFlag],
            fp.Log2MaxPicOrdCntMinus4 + 4);

        if (fp.PicOrderPresent == 1 && fp.FieldPicFlag == 0)
        {
            H264BsReal_PutVLCCode_8u16s(&m_bs, 0); // delta_pic_order_cnt_bottom
        }
    }

    if (fp.RedundantPicCntPresent)
    {
        H264BsReal_PutVLCCode_8u16s(&m_bs, sp.RedundantPicCnt);
    }

    if (sp.SliceType & MFX_SLICETYPE_B)
    {
        H264BsReal_PutBit_8u16s(&m_bs, sp.DirectPredType);
    }

    if ((sp.SliceType & MFX_SLICETYPE_I) == 0)
    {
        mfxU8 overrideFlag =
            (sp.NumRefIdxL0Minus1 != fp.NumRefIdxL0Minus1) ||
            (sp.NumRefIdxL1Minus1 != fp.NumRefIdxL1Minus1);

        H264BsReal_PutBit_8u16s(&m_bs, overrideFlag);

        if (overrideFlag)
        {
            H264BsReal_PutVLCCode_8u16s(&m_bs, sp.NumRefIdxL0Minus1);

            if (sp.SliceType & MFX_SLICETYPE_B)
            {
                H264BsReal_PutVLCCode_8u16s(&m_bs, sp.NumRefIdxL1Minus1);
            }
        }
    }


    if ((sp.SliceType & MFX_SLICETYPE_I) == 0)
    {
        H264BsReal_PutBit_8u16s(&m_bs, 0); // ref_pic_list_reordering_flag_l0

        if (sp.SliceType & MFX_SLICETYPE_B)
        {
            H264BsReal_PutBit_8u16s(&m_bs, 0); // ref_pic_list_reordering_flag_l1
        }
    }

    if (((sp.SliceType & MFX_SLICETYPE_P) && fp.WeightedPredFlag) ||
        ((sp.SliceType & MFX_SLICETYPE_B) && fp.WeightedBipredIdc))
    {
        assert(!"pred_weight_table unsupported");
    }

    if (fp.FrameType & MFX_FRAMETYPE_REF)
    {
        if (fp.FrameType & MFX_FRAMETYPE_IDR)
        {
            H264BsReal_PutBit_8u16s(&m_bs, 0); // no_output_of_prior_pics_flag
            H264BsReal_PutBit_8u16s(&m_bs, 0); // long_term_reference_flag
        }
        else
        {
            // need to write mmco1 to make gop closed
            mfxU8 adaptive_ref_pic_marking_mode_flag =
                (vp.mfx.GopOptFlag & MFX_GOP_CLOSED) &&
                (fp.FrameType & MFX_FRAMETYPE_I) &&
                (!fp.FieldPicFlag || !fp.SecondFieldPicFlag);

            H264BsReal_PutBit_8u16s(&m_bs, adaptive_ref_pic_marking_mode_flag);
            if (adaptive_ref_pic_marking_mode_flag)
            {
                mfxU32 maxFrameNum = 1 << (fp.Log2MaxFrameCntMinus4 + 4);

                for (mfxU32 i = 0; i < 16; i++)
                {
                    assert(fp.RefFrameListP[i] >= 16 || refPicInfo.FrameNumList[i] < maxFrameNum);
                    if (fp.RefFrameListP[i] < 16 && refPicInfo.FrameNumList[i] < maxFrameNum)
                    {
                        mfxI32 currPicNum = refFrameParam.FrameNum;
                        mfxI32 refPicNum = refPicInfo.FrameNumList[i];

                        if (refPicNum > currPicNum)
                        {
                            refPicNum -= maxFrameNum;
                        }

                        if (fp.FieldPicFlag)
                        {
                            currPicNum = 2 * currPicNum + 1;
                            refPicNum = 2 * refPicNum;

                            // 7.4.3.3:
                            // - If the current picture is a frame, the term reference picture refers
                            // either to a reference frame or a complementary reference field pair
                            // - Otherwise (the current picture is a field), the term reference picture refers
                            // either to a reference field or a field of a reference frame

                            assert(refPicNum < currPicNum);
                            H264BsReal_PutVLCCode_8u16s(&m_bs, 1); // memory_management_control_operation
                            H264BsReal_PutVLCCode_8u16s(&m_bs, currPicNum - refPicNum - 1); // difference_of_pic_nums_minus1
                            refPicNum++;
                        }

                        assert(refPicNum < currPicNum);
                        H264BsReal_PutVLCCode_8u16s(&m_bs, 1); // memory_management_control_operation
                        H264BsReal_PutVLCCode_8u16s(&m_bs, currPicNum - refPicNum - 1); // difference_of_pic_nums_minus1
                    }
                }

                H264BsReal_PutVLCCode_8u16s(&m_bs, 0); // exit mmco loop
            }
        }
    }

    if (fp.EntropyCodingModeFlag && (sp.SliceType & MFX_SLICETYPE_I) == 0)
    {
        H264BsReal_PutVLCCode_8u16s(&m_bs, sp.CabacInitIdc);
    }

    H264BsReal_PutVLCCode_8u16s(&m_bs, SIGNED_VLC_CODE(sp.SliceQpDelta));

    if (fp.ILDBControlPresent)
    {
        H264BsReal_PutVLCCode_8u16s(&m_bs, sp.DeblockDisableIdc);

        if (sp.DeblockDisableIdc != 1)
        {
            H264BsReal_PutVLCCode_8u16s(&m_bs, SIGNED_VLC_CODE(sp.DeblockAlphaC0OffsetDiv2));
            H264BsReal_PutVLCCode_8u16s(&m_bs, SIGNED_VLC_CODE(sp.DeblockBetaOffsetDiv2));
        }
    }

    if (fp.NumSliceGroupMinus1 > 0)
    {
        assert(!"slice_group_change_cycle unsupported");
    }
}

mfxU8 Get4x4IntraModeA(const mfxMbCodeAVC& curMb, const mfxMbCodeAVC* mbA, mfxU32 blk4x4)
{
    if (blk4x4 & 5)
    {
        return Get4x4IntraMode(curMb, g_BlockA[blk4x4]);
    }
    else if (mbA != 0)
    {
        if (mbA->IntraMbFlag && mbA->MbType5Bits == MBTYPE_I_4x4)
        {
            return mbA->TransformFlag == 0
                ? Get4x4IntraMode(*mbA, g_BlockA[blk4x4])
                : Get8x8IntraMode(*mbA, g_BlockA[blk4x4] / 4);
        }
        else
        {
            return 2;// LUMA_INTRA_PRED_MODE_DC;
        }
    }
    else
    {
        return 0xff;
    }
}

mfxU8 Get4x4IntraModeB(const mfxMbCodeAVC& curMb, const mfxMbCodeAVC* mbB, mfxU32 blk4x4)
{
    if (blk4x4 & 10)
    {
        return Get4x4IntraMode(curMb, g_BlockB[blk4x4]);
    }
    else if (mbB != 0)
    {
        if (mbB->IntraMbFlag && mbB->MbType5Bits == MBTYPE_I_4x4)
        {
            return mbB->TransformFlag == 0
                ? Get4x4IntraMode(*mbB, g_BlockB[blk4x4])
                : Get8x8IntraMode(*mbB, g_BlockB[blk4x4] / 4);
        }
        else
        {
            return 2;// LUMA_INTRA_PRED_MODE_DC;
        }
    }
    else
    {
        return 0xff;
    }
}

mfxU8 Get8x8IntraModeA(const mfxMbCodeAVC& curMb, const mfxMbCodeAVC* mbA, mfxU32 blk8x8)
{
    if (blk8x8 & 1)
    {
        return Get8x8IntraMode(curMb, blk8x8 - 1);
    }
    else if (mbA != 0 )
    {
        if (mbA->IntraMbFlag && mbA->MbType5Bits == MBTYPE_I_4x4)
        {
            return mbA->TransformFlag == 1
                ? Get8x8IntraMode(*mbA, blk8x8 + 1)
                : Get4x4IntraMode(*mbA, 4 * (blk8x8 + 1) + 1);
        }
        else
        {
            return 2;// LUMA_INTRA_PRED_MODE_DC;
        }
    }
    else
    {
        return 0xff;
    }
}

mfxU8 Get8x8IntraModeB(const mfxMbCodeAVC& curMb, const mfxMbCodeAVC* mbB, mfxU32 blk8x8)
{
    if (blk8x8 > 1)
    {
        return Get8x8IntraMode(curMb, blk8x8 - 2);
    }
    else if (mbB != 0)
    {
        if (mbB->IntraMbFlag && mbB->MbType5Bits == MBTYPE_I_4x4)
        {
            return mbB->TransformFlag == 1
                ? Get8x8IntraMode(*mbB, blk8x8 + 2)
                : Get4x4IntraMode(*mbB, 4 * (blk8x8 + 2) + 2);
        }
        else
        {
            return 2;// LUMA_INTRA_PRED_MODE_DC;
        }
    }
    else
    {
        return 0xff;
    }
}

static mfxU8 GetValueForSubTypeP(const mfxMbCodeAVC& mb, mfxU32 part)
{
    assert(part < 4);
    assert(mb.SubMbPredMode == 0);
    assert(mb.MbType5Bits == MBTYPE_BP_8x8);
    return (mfxU8)((mb.SubMbShape >> (2 * part)) & 3);
}

static mfxU8 GetValueForSubTypeB(const mfxMbCodeAVC& mb, mfxU32 part)
{
    assert(part < 4);

    static const mfxU8 mapTable[3][4] = {
        1, 4, 5, 10,
        2, 6, 7, 11,
        3, 8, 9, 12
    };

    return (mb.Skip8x8Flag >> part) & 1
        ? 0
        : mapTable[(mb.SubMbPredMode >> (2 * part)) & 3][(mb.SubMbShape >> (2 * part)) & 3];
}

static mfxU8 GetValueForMbTypeI(const mfxMbCodeAVC& mb)
{
    if (mb.MbType5Bits == MBTYPE_I_4x4)
    {
        return 0;
    }

    mfxU32 chromaCbp = mb.CodedPattern4x4U == 0 && mb.CodedPattern4x4V == 0
        ? mb.DcBlockCodedCbFlag | mb.DcBlockCodedCrFlag
        : 2;

    return (mfxU8)(12 * (mb.CodedPattern4x4Y != 0) +
        4 * (chromaCbp) + (mb.LumaIntraModes[0] & 0x3) + 1);
}

static mfxU8 GetValueForMbTypeP(const mfxMbCodeAVC& mb)
{
    assert(mb.IntraMbFlag == 0);

    switch (mb.MbType5Bits)
    {
    case MBTYPE_BP_L0_16x16:
        return 0;
    case MBTYPE_BP_L0_L0_16x8:
        return 1;
    case MBTYPE_BP_L0_L0_8x16:
        return 2;
    case MBTYPE_BP_8x8:
        if (mb.RefPicSelect[0][0] == 0 && mb.RefPicSelect[0][1] == 0 &&
            mb.RefPicSelect[0][2] == 0 && mb.RefPicSelect[0][3] == 0)
        {
            return 4;
        }
        else
        {
            return 3;
        }

    default:
        assert(!"not P macroblock");
        return 0xff;
    }
}

static mfxU8 GetValueForMbTypeB(const mfxMbCodeAVC& mb)
{
    return (mb.Skip8x8Flag == 0xf) ? 0 : mb.MbType5Bits;
}

static void PackTransform8x8FlagCabac(
    H264BsReal_8u16s& bitstream,
    const MbDescriptor& mb)
{
    mfxI32 flagA = mb.mbA != 0 && mb.mbA->reserved3c == 0 && mb.mbA->TransformFlag == 1;
    mfxI32 flagB = mb.mbB != 0 && mb.mbB->reserved3c == 0 && mb.mbB->TransformFlag == 1;
    mfxU32 ctx = MB_TRANSFORM_SIZE_8X8_FLAG + flagA + flagB;

    H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx, mb.mbN.TransformFlag != 0);
}

static void PackMbTypeICabac(
    H264BsReal_8u16s& bitstream,
    const mfxSliceParamAVC& sp,
    const MbDescriptor& mb)
{
    if (sp.SliceType & MFX_SLICETYPE_B)
    {
        mfxU32 ctx = ctxIdxOffset[MB_TYPE_B];
        mfxU32 ctxOff0 = (mb.mbA && mb.mbA->Skip8x8Flag != 0xf) + (mb.mbB && mb.mbB->Skip8x8Flag != 0xf);

        mfxU8 value = GetValueForMbTypeI(mb.mbN) + 23;

        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + ctxOff0, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 4, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, 0);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, 1);

        if (mb.mbN.MbType5Bits == MBTYPE_I_4x4)
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, 0);
        }
        else if (mb.mbN.MbType5Bits == MBTYPE_I_IPCM)
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, 1);
            H264BsReal_EncodeFinalSingleBin_CABAC_8u16s(&bitstream, 1);
        }
        else // 16x16 Intra
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, 1);
            H264BsReal_EncodeFinalSingleBin_CABAC_8u16s(&bitstream, 0);
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 6, (value - 24) / 12 != 0);
            value %= 12;

            if (value < 4)
            {
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 7, 0);
            }
            else
            {
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 7, 1);
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 7, value >= 8);
            }

            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 8, (value & 2) != 0);
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 8, (value & 1) != 0);
        }
    }
    else
    {
        mfxU32 ctx = ctxIdxOffset[MB_TYPE_I];
        mfxU32 ctxOff0 =
            (mb.mbA && mb.mbA->MbType5Bits != MBTYPE_I_4x4) +
            (mb.mbB && mb.mbB->MbType5Bits != MBTYPE_I_4x4);
        mfxU8 ctxOff1 = 0;

        if (sp.SliceType & MFX_SLICETYPE_P)
        {
            ctx = ctxIdxOffset[MB_TYPE_P_SP];
            ctxOff0 = 3;
            ctxOff1 = 1;

            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 0, 1); // prefix
        }

        if (mb.mbN.MbType5Bits == MBTYPE_I_4x4)
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + ctxOff0, 0);
        }
        else if (mb.mbN.MbType5Bits == MBTYPE_I_IPCM)
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + ctxOff0, 1);
            H264BsReal_EncodeFinalSingleBin_CABAC_8u16s(&bitstream, 1);
        }
        else // 16x16 Intra
        {
            mfxU8 value = GetValueForMbTypeI(mb.mbN) - 1;

            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + ctxOff0, 1);
            H264BsReal_EncodeFinalSingleBin_CABAC_8u16s(&bitstream, 0);
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3 + ctxOff1, value / 12 != 0);

            value %= 12;
            if (value < 4)
            {
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 4 + ctxOff1, 0);
            }
            else
            {
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 4 + ctxOff1, 1);
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, value >= 8);
            }

            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 6, (value & 2) != 0);
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 7 - ctxOff1, (value & 1) != 0);
        }
    }
}

static void PackMbTypePCabac(
    H264BsReal_8u16s& bitstream,
    const MbDescriptor& mb)
{
    mfxU32 ctx = ctxIdxOffset[MB_TYPE_P_SP];

    H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 0, 0);
    if (mb.mbN.MbType5Bits == MBTYPE_BP_L0_16x16)
    {
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 1, 0);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 2, 0);
    }
    else if (mb.mbN.MbType5Bits == MBTYPE_BP_L0_L0_16x8)
    {
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 1, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, 1);
    }
    else if (mb.mbN.MbType5Bits == MBTYPE_BP_L0_L0_8x16)
    {
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 1, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, 0);
    }
    else if (mb.mbN.MbType5Bits == MBTYPE_BP_8x8)
    {
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 1, 0);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 2, 1);
    }
    else
    {
        assert(0);
    }
}

static void PackMbTypeBCabac(
    H264BsReal_8u16s& bitstream,
    const MbDescriptor& mb)
{
    mfxU8 flagA = mb.mbA && mb.mbA->Skip8x8Flag != 0xf;
    mfxU8 flagB = mb.mbB && mb.mbB->Skip8x8Flag != 0xf;
    mfxU32 ctx = ctxIdxOffset[MB_TYPE_B];

    mfxU8 value = GetValueForMbTypeB(mb.mbN);

    if (value == 0)
    {
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + flagA + flagB, 0);
    }
    else if (value <= 2)
    {
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + flagA + flagB, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, 0);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, value - 1);
    }
    else if (value <= 10)
    {
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + flagA + flagB, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 4, 0);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, ((value - 3) >> 2) & 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, ((value - 3) >> 1) & 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, ((value - 3) >> 0) & 1);
    }
    else if (value == 11)
    {
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + flagA + flagB, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 4, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, 0);
    }
    else if (value == 22) // B8x8
    {
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + flagA + flagB, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 4, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, 1);
    }
    else
    {
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + flagA + flagB, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 4, 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, ((value - 12) >> 3) & 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, ((value - 12) >> 2) & 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, ((value - 12) >> 1) & 1);
        H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 5, ((value - 12) >> 0) & 1);
    }
}

static void PackIntra4x4TypeCabac(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const mfxSliceParamAVC& sp,
    const MbDescriptor& mb)
{
    PackMbTypeICabac(bitstream, sp, mb);

    if (fp.Transform8x8Flag)
    {
        PackTransform8x8FlagCabac(bitstream, mb);
    }

    if (mb.mbN.TransformFlag)
    {
        for (mfxU32 blk8x8 = 0; blk8x8 < 4; blk8x8++)
        {
            mfxU8 curType = Get8x8IntraMode(mb.mbN, blk8x8);
            mfxU8 typeA = Get8x8IntraModeA(mb.mbN, mb.mbA, blk8x8);
            mfxU8 typeB = Get8x8IntraModeB(mb.mbN, mb.mbB, blk8x8);

            mfxU8 mostProbType = (typeA < 0xff && typeB < 0xff)
                ? MIN(typeA, typeB)
                : 2; // DC

            // The probability of the current mode is 0 if it equals the Most Probable Mode
            mfxI32 prob0 = mostProbType == curType
                ? -1
                : curType < mostProbType
                    ? curType
                    : curType - 1;

            H264BsReal_IntraPredMode_CABAC_8u16s(&bitstream, prob0);
        }
    }
    else
    {
        for (mfxU32 blk4x4 = 0; blk4x4 < 16; blk4x4++)
        {
            mfxU8 curType = Get4x4IntraMode(mb.mbN, blk4x4);
            mfxU8 typeA = Get4x4IntraModeA(mb.mbN, mb.mbA, blk4x4);
            mfxU8 typeB = Get4x4IntraModeB(mb.mbN, mb.mbB, blk4x4);

            mfxU8 mostProbType = (typeA < 0xff && typeB < 0xff)
                ? MIN(typeA, typeB)
                : 2; // DC

            // The probability of the current mode is 0 if it equals the Most Probable Mode
            mfxI32 prob0 = mostProbType == curType
                ? -1
                : curType < mostProbType
                    ? curType
                    : curType - 1;

            H264BsReal_IntraPredMode_CABAC_8u16s(&bitstream, prob0);
        }
    }

    if (fp.ChromaFormatIdc != 0)
    {
        mfxU8 typeA = mb.mbA && mb.mbA->IntraMbFlag ? mb.mbA->ChromaIntraPredMode : 0;
        mfxU8 typeB = mb.mbB && mb.mbB->IntraMbFlag ? mb.mbB->ChromaIntraPredMode : 0;

        H264BsReal_ChromaIntraPredMode_CABAC_8u16s(
            &bitstream,
            mb.mbN.ChromaIntraPredMode,
            typeA != 0,
            typeB != 0);
    }
}

static void PackIntra4x4TypeCavlc(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const mfxSliceParamAVC& sp,
    const MbDescriptor& mb)
{
    mfxU8 value = (sp.SliceType & MFX_SLICETYPE_I) ? 0 : (sp.SliceType & MFX_SLICETYPE_P) ? 5 : 23;
    H264BsReal_PutVLCCode_8u16s(&bitstream, value);

    if (fp.Transform8x8Flag)
    {
        H264BsReal_PutBit_8u16s(&bitstream, mb.mbN.TransformFlag);
    }

    if (mb.mbN.TransformFlag)
    {
        for (mfxU32 blk8x8 = 0; blk8x8 < 4; blk8x8++)
        {
            mfxU8 curType = Get8x8IntraMode(mb.mbN, blk8x8);
            mfxU8 typeA = Get8x8IntraModeA(mb.mbN, mb.mbA, blk8x8);
            mfxU8 typeB = Get8x8IntraModeB(mb.mbN, mb.mbB, blk8x8);

            mfxU8 mostProbType = (typeA < 0xff && typeB < 0xff)
                ? MIN(typeA, typeB)
                : 2; // DC

            // The probability of the current mode is 0 if it equals the Most Probable Mode
            mfxI32 prob0 = mostProbType == curType
                ? -1
                : curType < mostProbType
                    ? curType
                    : curType - 1;

            if (prob0 >= 0)
            {
                // Not the most probable type, 1 bit(0) +3 bits to signal actual mode
                H264BsReal_PutBits_8u16s(&bitstream, prob0, 4);
            }
            else
            {
                // most probable type in 1 bit
                H264BsReal_PutBit_8u16s(&bitstream, 1);
            }
        }
    }
    else
    {
        for (mfxU32 blk4x4 = 0; blk4x4 < 16; blk4x4++)
        {
            mfxU8 curType = Get4x4IntraMode(mb.mbN, blk4x4);
            mfxU8 typeA = Get4x4IntraModeA(mb.mbN, mb.mbA, blk4x4);
            mfxU8 typeB = Get4x4IntraModeB(mb.mbN, mb.mbB, blk4x4);

            mfxU8 mostProbType = (typeA < 0xff && typeB < 0xff)
                ? MIN(typeA, typeB)
                : 2; // DC

            // The probability of the current mode is 0 if it equals the Most Probable Mode
            mfxI32 prob0 = mostProbType == curType
                ? -1
                : curType < mostProbType
                    ? curType
                    : curType - 1;

            if (prob0 >= 0)
            {
                // Not the most probable type, 1 bit(0) +3 bits to signal actual mode
                H264BsReal_PutBits_8u16s(&bitstream, prob0, 4);
            }
            else
            {
                // most probable type in 1 bit
                H264BsReal_PutBit_8u16s(&bitstream, 1);
            }
        }
    }

    if (fp.ChromaFormatIdc != 0)
    {
        H264BsReal_PutVLCCode_8u16s(&bitstream, mb.mbN.ChromaIntraPredMode);
    }
}

static void PackIntra16x16TypeCabac(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const mfxSliceParamAVC& sp,
    const MbDescriptor& mb)
{
    PackMbTypeICabac(bitstream, sp, mb);

    if (fp.ChromaFormatIdc != 0)
    {
        mfxU8 flagA = mb.mbA != 0 && mb.mbA->IntraMbFlag ? mb.mbA->ChromaIntraPredMode : 0;
        mfxU8 flagB = mb.mbB != 0 && mb.mbB->IntraMbFlag ? mb.mbB->ChromaIntraPredMode : 0;

        H264BsReal_ChromaIntraPredMode_CABAC_8u16s(
            &bitstream,
            mb.mbN.ChromaIntraPredMode,
            flagA != 0,
            flagB != 0);
    }
}

static void PackIntra16x16TypeCavlc(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const mfxSliceParamAVC& sp,
    const MbDescriptor& mb)
{
    mfxU8 value = GetValueForMbTypeI(mb.mbN);

    value += (sp.SliceType & MFX_SLICETYPE_I)
        ? 0
        : (sp.SliceType & MFX_SLICETYPE_P) ? 5 : 23;

    H264BsReal_PutVLCCode_8u16s(&bitstream, value);

    if (fp.ChromaFormatIdc != 0)
    {
        H264BsReal_PutVLCCode_8u16s(&bitstream, mb.mbN.ChromaIntraPredMode);
    }
}

static mfxU8 GetCbp8x8(const mfxMbCodeAVC& mb)
{
    if (mb.IntraMbFlag == 1 && mb.MbType5Bits != MBTYPE_I_4x4)
    {
        return mb.CodedPattern4x4Y > 0 ? 0xf : 0;
    }
    else
    {
        mfxU16 cbp4x4 = mb.CodedPattern4x4Y;
        mfxU8 cbp8x8 = 0;
        cbp8x8 |= ((cbp4x4 >> 0) | (cbp4x4 >>  1) | (cbp4x4 >>  2) | (cbp4x4 >>  3)) & 1;
        cbp8x8 |= ((cbp4x4 >> 3) | (cbp4x4 >>  4) | (cbp4x4 >>  5) | (cbp4x4 >>  6)) & 2;
        cbp8x8 |= ((cbp4x4 >> 6) | (cbp4x4 >>  7) | (cbp4x4 >>  8) | (cbp4x4 >>  9)) & 4;
        cbp8x8 |= ((cbp4x4 >> 9) | (cbp4x4 >> 10) | (cbp4x4 >> 11) | (cbp4x4 >> 12)) & 8;
        return cbp8x8;
    }
}

static const mfxU8 enc_cbp_intra[64] = {
     3, 29, 30, 17, 31, 18, 37,  8, 32, 38,
    19,  9, 20, 10, 11,  2, 16, 33, 34, 21,
    35, 22, 39, 04, 36, 40, 23,  5, 24,  6,
    07, 01, 41, 42, 43, 25, 44, 26, 46, 12,
    45, 47, 27, 13, 28, 14, 15, 00, 00, 00,
    00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
    00, 00, 00, 00
};

static const mfxU8 enc_cbp_inter[64] = {
     0,  2,  3,  7,  4,  8, 17, 13,  5, 18,
     9, 14, 10, 15, 16, 11,  1, 32, 33, 36,
    34, 37, 44, 40, 35, 45, 38, 41, 39, 42,
    43, 19, 06, 24, 25, 20, 26, 21, 46, 28,
    27, 47, 22, 29, 23, 30, 31, 12, 00, 00,
    00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
    00, 00, 00, 00
};

static const mfxU8 enc_cbp_intra_monochrome[16] = {
     1, 10, 11, 6, 12, 7, 14, 2, 13, 15, 8, 3, 9, 4, 5, 0
};

static const mfxU8 enc_cbp_inter_monochrome[16] = {
    0, 1, 2, 5, 3, 6, 14, 10, 4, 15, 7, 11, 8, 12, 13, 9
};

static const mfxU8* cavlc_cbp_tables[2][2] =
{
    { enc_cbp_intra, enc_cbp_intra_monochrome },
    { enc_cbp_inter, enc_cbp_inter_monochrome }
};

static void PackCbpCabac(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const MbDescriptor& mb)
{
    mfxU32 cbp = GetCbp8x8(mb.mbN);
    mfxU32 cbpA = mb.mbA != 0 ? GetCbp8x8(*mb.mbA) : 0xf;
    mfxU32 cbpB = mb.mbB != 0 ? GetCbp8x8(*mb.mbB) : 0xf;

    mfxU8 blkFlagA[4] =
    {
        (cbpA & 0x2) ? 0 : 1,
        ( cbp & 0x1) ? 0 : 1,
        (cbpA & 0x8) ? 0 : 1,
        ( cbp & 0x4) ? 0 : 1
    };

    mfxU8 blkFlagB[4] =
    {
        (cbpB & 0x4) ? 0 : 1,
        (cbpB & 0x8) ? 0 : 1,
        ( cbp & 0x1) ? 0 : 1,
        ( cbp & 0x2) ? 0 : 1
    };

    for (mfxU32 i = 0; i < 2; i++)
    {
        for (mfxU32 j = 0; j < 2; j++)
        {
            mfxU32 mask = (1 << (2 * i + j));

            mfxI32 ctxIdxInc = blkFlagA[2 * i + j] + 2 * blkFlagB[2 * i + j];
            H264BsReal_EncodeSingleBin_CABAC_8u16s(
                &bitstream,
                ctxIdxOffset[CODED_BLOCK_PATTERN_LUMA] + ctxIdxInc,
                (cbp & mask) != 0);
        }
    }

    if (fp.ChromaFormatIdc)
    {
        mfxU32 curFlag =
            mb.mbN.CodedPattern4x4U != 0 ||
            mb.mbN.CodedPattern4x4V != 0 ||
            mb.mbN.DcBlockCodedCbFlag != 0 ||
            mb.mbN.DcBlockCodedCrFlag != 0;

        mfxU8 flagA = mb.mbA != 0 && (
            mb.mbA->CodedPattern4x4U != 0 || mb.mbA->DcBlockCodedCbFlag != 0 ||
            mb.mbA->CodedPattern4x4V != 0 || mb.mbA->DcBlockCodedCrFlag != 0);

        mfxU8 flagB = mb.mbB != 0 && (
            mb.mbB->CodedPattern4x4U != 0 || mb.mbB->DcBlockCodedCbFlag != 0 ||
            mb.mbB->CodedPattern4x4V != 0 || mb.mbB->DcBlockCodedCrFlag != 0);

        mfxI32 ctxIdxInc = flagA + 2 * flagB;
        H264BsReal_EncodeSingleBin_CABAC_8u16s(
            &bitstream,
            ctxIdxOffset[CODED_BLOCK_PATTERN_CHROMA] + ctxIdxInc,
            curFlag);

        if (curFlag)
        {
            curFlag = mb.mbN.CodedPattern4x4U != 0 || mb.mbN.CodedPattern4x4V != 0;
            flagA = mb.mbA && (mb.mbA->CodedPattern4x4U != 0 || mb.mbA->CodedPattern4x4V != 0);
            flagB = mb.mbB && (mb.mbB->CodedPattern4x4U != 0 || mb.mbB->CodedPattern4x4V != 0);

            ctxIdxInc = flagA + 2 * flagB;
            H264BsReal_EncodeSingleBin_CABAC_8u16s(
                &bitstream,
                ctxIdxOffset[CODED_BLOCK_PATTERN_CHROMA] + ctxIdxInc + 4,
                curFlag);
        }
    }
}

static void PackCbpCavlc(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const MbDescriptor& mb)
{
    mfxU32 cbp = GetCbp8x8(mb.mbN);

    cbp += 16 * (mb.mbN.CodedPattern4x4U == 0 && mb.mbN.CodedPattern4x4V == 0
        ? mb.mbN.DcBlockCodedCbFlag | mb.mbN.DcBlockCodedCrFlag
        : 2);

    mfxU32 chromaCbp = cavlc_cbp_tables[!mb.mbN.IntraMbFlag][!fp.ChromaFormatIdc][cbp];
    H264BsReal_PutVLCCode_8u16s(&bitstream, chromaCbp);
}

static void PackRefIdxCabac(
    H264BsReal_8u16s& bitstream,
    const MbDescriptor& mb,
    mfxU32 block,
    mfxU32 list)
{
    Block4x4 neighA = mb.GetLeft(block);
    Block4x4 neighB = mb.GetAbove(block);

    mfxU8 flagA =
        neighA.mb != 0 &&
        neighA.mb->reserved3c == 0 &&
        neighA.mb->IntraMbFlag == 0 &&
        ((neighA.mb->Skip8x8Flag >> (neighA.block / 4)) & 1) == 0 &&
        neighA.mb->RefPicSelect[list][neighA.block / 4] != 0xff &&
        neighA.mb->RefPicSelect[list][neighA.block / 4] != 0;

    mfxU8 flagB =
        neighB.mb != 0 &&
        neighB.mb->reserved3c == 0 &&
        neighB.mb->IntraMbFlag == 0 &&
        ((neighB.mb->Skip8x8Flag >> (neighB.block / 4)) & 1) == 0 &&
        neighB.mb->RefPicSelect[list][neighB.block / 4] != 0xff &&
        neighB.mb->RefPicSelect[list][neighB.block / 4] != 0;

    mfxU8 refIdx = mb.mbN.RefPicSelect[list][block / 4];
    H264BsReal_EncodeSingleBin_CABAC_8u16s(
        &bitstream,
        ctxIdxOffset[REF_IDX_L0 + list] + 2 * flagB + flagA,
        refIdx != 0);

    mfxU8 ctx = 3;
    while (refIdx)
    {
        refIdx--;
        ctx = (ctx < 5) ? ctx + 1 : ctx;
        H264BsReal_EncodeSingleBin_CABAC_8u16s(
            &bitstream,
            ctxIdxOffset[REF_IDX_L0 + list] + ctx,
            refIdx != 0);
    }
}

static void PackMvsCabac(
    H264BsReal_8u16s& bitstream,
    const mfxSliceParamAVC& sp,
    const MbDescriptor& mb)
{
    if (sp.NumRefIdxL0Minus1 > 0)
    {
        mfxU8 last8x8Part = 0xff;
        for (mfxU32 i = 0; i < mb.parts.m_numPart; i++)
        {
            mfxU8 cur8x8Part = mb.parts.parts[i].block / 4;
            if (cur8x8Part != last8x8Part &&
                mb.mbN.RefPicSelect[0][cur8x8Part] != 0xff &&
                ((mb.mbN.Skip8x8Flag >> cur8x8Part) & 1) == 0)
            {
                PackRefIdxCabac(bitstream, mb, mb.parts.parts[i].block, 0);
                last8x8Part = cur8x8Part;
            }
        }
    }

    if (sp.NumRefIdxL1Minus1 > 0)
    {
        mfxU8 last8x8Part = 0xff;
        for (mfxU32 i = 0; i < mb.parts.m_numPart; i++)
        {
            mfxU8 cur8x8Part = mb.parts.parts[i].block / 4;
            if (cur8x8Part != last8x8Part &&
                mb.mbN.RefPicSelect[1][cur8x8Part] != 0xff &&
                ((mb.mbN.Skip8x8Flag >> cur8x8Part) & 1) == 0)
            {
                PackRefIdxCabac(bitstream, mb, mb.parts.parts[i].block, 1);
                last8x8Part = cur8x8Part;
            }
        }
    }

    for (mfxU32 i = 0; i < mb.parts.m_numPart; i++)
    {
        const Partition& part = mb.parts.parts[i];

        if (mb.mbN.RefPicSelect[0][part.block / 4] != 0xff &&
            ((mb.mbN.Skip8x8Flag >> (part.block / 4)) & 1) == 0)
        {
            mfxI16Pair pred = GetMvPred(mb, 0, part.block, part.width, part.height);

            mfxI16Pair mvd = {
                mb.mvN->mv[0][g_Geometry[part.block].offBlk4x4].x - pred.x,
                mb.mvN->mv[0][g_Geometry[part.block].offBlk4x4].y - pred.y
            };

            mfxU32 xoff = g_Geometry[part.block].xBlk;
            mfxU32 yoff = g_Geometry[part.block].yBlk;
            for (mfxU32 y = yoff; y < yoff + part.height; y++)
            {
                for (mfxU32 x = xoff; x < xoff + part.width; x++)
                {
                    mb.mvdN->mv[0][4 * y + x] = mvd;
                }
            }

            Block4x4 neighbourA = mb.GetLeft(part.block);
            Block4x4 neighbourB = mb.GetAbove(part.block);

            mfxI16Pair mvdA = neighbourA.mvd
                ? neighbourA.mvd->mv[0][g_Geometry[neighbourA.block].offBlk4x4]
                : nullMv;
            mfxI16Pair mvdB = neighbourB.mvd
                ? neighbourB.mvd->mv[0][g_Geometry[neighbourB.block].offBlk4x4]
                : nullMv;

            H264BsReal_MVD_CABAC_8u16s(&bitstream, mvd.x, mvdA.x, mvdB.x, MVD_L0_0);
            H264BsReal_MVD_CABAC_8u16s(&bitstream, mvd.y, mvdA.y, mvdB.y, MVD_L0_1);
        }
    }

    for (mfxU32 i = 0; i < mb.parts.m_numPart; i++)
    {
        const Partition& part = mb.parts.parts[i];

        if (mb.mbN.RefPicSelect[1][part.block / 4] != 0xff &&
            ((mb.mbN.Skip8x8Flag >> (part.block / 4)) & 1) == 0)
        {
            mfxI16Pair pred = GetMvPred(mb, 1, part.block, part.width, part.height);

            mfxI16Pair mvd = {
                mb.mvN->mv[1][g_Geometry[part.block].offBlk4x4].x - pred.x,
                mb.mvN->mv[1][g_Geometry[part.block].offBlk4x4].y - pred.y
            };

            mfxU32 xoff = g_Geometry[part.block].xBlk;
            mfxU32 yoff = g_Geometry[part.block].yBlk;
            for (mfxU32 y = yoff; y < yoff + part.height; y++)
            {
                for (mfxU32 x = xoff; x < xoff + part.width; x++)
                {
                    mb.mvdN->mv[1][4 * y + x] = mvd;
                }
            }

            Block4x4 neighbourA = mb.GetLeft(part.block);
            Block4x4 neighbourB = mb.GetAbove(part.block);

            mfxI16Pair mvdA = neighbourA.mb
                ? neighbourA.mvd->mv[1][g_Geometry[neighbourA.block].offBlk4x4]
                : nullMv;
            mfxI16Pair mvdB = neighbourB.mb
                ? neighbourB.mvd->mv[1][g_Geometry[neighbourB.block].offBlk4x4]
                : nullMv;

            H264BsReal_MVD_CABAC_8u16s(&bitstream, mvd.x, mvdA.x, mvdB.x, MVD_L1_0);
            H264BsReal_MVD_CABAC_8u16s(&bitstream, mvd.y, mvdA.y, mvdB.y, MVD_L1_1);
        }
    }
}

static void PackMvsCavlc(
    H264BsReal_8u16s& bitstream,
    const mfxSliceParamAVC& sp,
    const MbDescriptor& mb)
{
    bool isPref0 =
        (sp.SliceType & MFX_SLICETYPE_P) &&
        mb.mbN.MbType5Bits == MBTYPE_BP_8x8 &&
        mb.mbN.RefPicSelect[0][0] == 0 &&
        mb.mbN.RefPicSelect[0][1] == 0 &&
        mb.mbN.RefPicSelect[0][2] == 0 &&
        mb.mbN.RefPicSelect[0][3] == 0;

    if (sp.NumRefIdxL0Minus1 > 0 && !isPref0)
    {
        mfxU8 last8x8Part = 0xff;
        for (mfxU32 i = 0; i < mb.parts.m_numPart; i++)
        {
            mfxU8 cur8x8Part = mb.parts.parts[i].block / 4;
            if (cur8x8Part != last8x8Part &&
                mb.mbN.RefPicSelect[0][cur8x8Part] != 0xff &&
                ((mb.mbN.Skip8x8Flag >> cur8x8Part) & 1) == 0)
            {
                if (sp.NumRefIdxL0Minus1 > 1)
                {
                    H264BsReal_PutVLCCode_8u16s(
                        &bitstream,
                        mb.mbN.RefPicSelect[0][mb.parts.parts[i].block / 4]);
                }
                else
                {
                    H264BsReal_PutBit_8u16s(
                        &bitstream,
                        1 - mb.mbN.RefPicSelect[0][mb.parts.parts[i].block / 4]);
                }

                last8x8Part = cur8x8Part;
            }
        }
    }

    if (sp.NumRefIdxL1Minus1 > 0)
    {
        mfxU8 last8x8Part = 0xff;
        for (mfxU32 i = 0; i < mb.parts.m_numPart; i++)
        {
            mfxU8 cur8x8Part = mb.parts.parts[i].block / 4;
            if (cur8x8Part != last8x8Part &&
                mb.mbN.RefPicSelect[1][cur8x8Part] != 0xff &&
                ((mb.mbN.Skip8x8Flag >> cur8x8Part) & 1) == 0)
            {
                if (sp.NumRefIdxL1Minus1 > 1)
                {
                    H264BsReal_PutVLCCode_8u16s(
                        &bitstream,
                        mb.mbN.RefPicSelect[1][mb.parts.parts[i].block / 4]);
                }
                else
                {
                    H264BsReal_PutBit_8u16s(
                        &bitstream,
                        1 - mb.mbN.RefPicSelect[1][mb.parts.parts[i].block / 4]);
                }

                last8x8Part = cur8x8Part;
            }
        }
    }

    for (mfxU32 i = 0; i < mb.parts.m_numPart; i++)
    {
        const Partition& part = mb.parts.parts[i];

        if (mb.mbN.RefPicSelect[0][part.block / 4] != 0xff &&
            ((mb.mbN.Skip8x8Flag >> (part.block / 4)) & 1) == 0)
        {
            mfxI16Pair pred = GetMvPred(mb, 0, part.block, part.width, part.height);

            mfxI16Pair mvd = {
                mb.mvN->mv[0][g_Geometry[part.block].offBlk4x4].x - pred.x,
                mb.mvN->mv[0][g_Geometry[part.block].offBlk4x4].y - pred.y
            };

            H264BsReal_PutVLCCode_8u16s(&bitstream, SIGNED_VLC_CODE(mvd.x));
            H264BsReal_PutVLCCode_8u16s(&bitstream, SIGNED_VLC_CODE(mvd.y));
        }
    }

    for (mfxU32 i = 0; i < mb.parts.m_numPart; i++)
    {
        const Partition& part = mb.parts.parts[i];

        if (mb.mbN.RefPicSelect[1][part.block / 4] != 0xff &&
            ((mb.mbN.Skip8x8Flag >> (part.block / 4)) & 1) == 0)
        {
            mfxI16Pair pred = GetMvPred(mb, 1, part.block, part.width, part.height);

            mfxI16Pair mvd = {
                mb.mvN->mv[1][g_Geometry[part.block].offBlk4x4].x - pred.x,
                mb.mvN->mv[1][g_Geometry[part.block].offBlk4x4].y - pred.y
            };

            H264BsReal_PutVLCCode_8u16s(&bitstream, SIGNED_VLC_CODE(mvd.x));
            H264BsReal_PutVLCCode_8u16s(&bitstream, SIGNED_VLC_CODE(mvd.y));
        }
    }
}

static void PackSubMbTypePCabac(
    H264BsReal_8u16s& bitstream,
    const MbDescriptor& mb)
{
    mfxU32 ctx = ctxIdxOffset[SUB_MB_TYPE_P_SP];
    for (mfxU32 part = 0; part < 4; part++)
    {
        mfxU32 shape = (mb.mbN.SubMbShape >> (2 * part)) & 0x3;

        if (shape == SUB_MB_SHAPE_8x8)
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx, 1);
        }
        else if (shape == SUB_MB_SHAPE_8x4)
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 0, 0);
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 1, 0);
        }
        else if (shape == SUB_MB_SHAPE_4x8)
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 0, 0);
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 1, 1);
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 2, 1);
        }
        else // SUB_MB_SHAPE_4x4
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 0, 0);
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 1, 1);
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 2, 0);
        }
    }
}

static void PackSubMbTypeBCabac(
    H264BsReal_8u16s& bitstream,
    const MbDescriptor& mb)
{
    mfxU32 ctx = ctxIdxOffset[SUB_MB_TYPE_B];

    for (mfxU32 part = 0; part < 4; part++)
    {
        if ((mb.mbN.Skip8x8Flag >> part) & 1)
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 0, 0);
        }
        else
        {
            mfxU8 value = GetValueForSubTypeB(mb.mbN, part);
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 0, 1);
            value--;

            if (value < 2)
            {
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 1, 0);
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, value & 1);
            }
            else if (value < 6)
            {
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 1, 1);
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 2, 0);
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, ((value - 2) >> 1) & 1);
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, ((value - 2) >> 0) & 1);
            }
            else
            {
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 1, 1);
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 2, 1);

                if (((value - 6) >> 2) & 1)
                {
                    H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, 1);
                    H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, ((value - 6) >> 0) & 1);
                }
                else
                {
                    H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, 0);
                    H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, ((value - 6) >> 1) & 1);
                    H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + 3, ((value - 6) >> 0) & 1);
                }
            }
        }
    }
}

void H264PakBitstream::PackMbHeaderCabac(
    const mfxFrameParamAVC& fp,
    const mfxSliceParamAVC& sp,
    const MbDescriptor& mb)
{
    mfxI32 mbQpDelta = mb.mbN.QpPrimeY - m_prevMbQpY;
    mfxU32 ctxInc = m_prevMbQpDelta != 0;
    m_prevMbQpDelta = 0; // will be updated if mb is not skipped

    if ((sp.SliceType & MFX_SLICETYPE_I) == 0)
    {
        mfxU8 skipFlagA = mb.mbA ? mb.mbA->reserved3c == 0 : 0;
        mfxU8 skipFlagB = mb.mbB ? mb.mbB->reserved3c == 0 : 0;
        mfxU32 ctxOff = ctxIdxOffset[(sp.SliceType & MFX_SLICETYPE_P) == 0];

        H264BsReal_EncodeSingleBin_CABAC_8u16s(
            &m_bs,
            skipFlagA + skipFlagB + ctxOff,
            mb.mbN.reserved3c);
    }

    if (mb.mbN.IntraMbFlag)
    {
        if (mb.mbN.MbType5Bits == MBTYPE_I_4x4)
        {
            PackIntra4x4TypeCabac(m_bs, fp, sp, mb);
            PackCbpCabac(m_bs, fp, mb);

            if (mb.mbN.CodedPattern4x4Y > 0 ||
                mb.mbN.CodedPattern4x4U > 0 ||
                mb.mbN.CodedPattern4x4V > 0 ||
                mb.mbN.DcBlockCodedCbFlag ||
                mb.mbN.DcBlockCodedCrFlag)
            {
                H264BsReal_DQuant_CABAC_8u16s(&m_bs, mbQpDelta, ctxInc);
                m_prevMbQpY = mb.mbN.QpPrimeY;
                m_prevMbQpDelta = mbQpDelta;
            }
        }
        else if (mb.mbN.MbType5Bits == MBTYPE_I_IPCM)
        {
            assert(0);
        }
        else // INTRA 16x16
        {
            PackIntra16x16TypeCabac(m_bs, fp, sp, mb);
            H264BsReal_DQuant_CABAC_8u16s(&m_bs, mbQpDelta, ctxInc);
            m_prevMbQpY = mb.mbN.QpPrimeY;
            m_prevMbQpDelta = mbQpDelta;
        }
    }
    else if (mb.mbN.reserved3c == 0)
    {
        if (sp.SliceType & MFX_SLICETYPE_P)
        {
            PackMbTypePCabac(m_bs, mb);

            if (mb.mbN.MbType5Bits == MBTYPE_BP_8x8)
            {
                PackSubMbTypePCabac(m_bs, mb);
            }
        }
        else // MFX_SLICETYPE_B
        {
            PackMbTypeBCabac(m_bs, mb);

            if (mb.mbN.MbType5Bits == MBTYPE_BP_8x8 && mb.mbN.Skip8x8Flag != 0xf)
            {
                PackSubMbTypeBCabac(m_bs, mb);
            }
        }

        PackMvsCabac(m_bs, sp, mb);
        PackCbpCabac(m_bs, fp, mb);

        if (mb.mbN.CodedPattern4x4Y > 0 ||
            mb.mbN.CodedPattern4x4U > 0 ||
            mb.mbN.CodedPattern4x4V > 0 ||
            mb.mbN.DcBlockCodedCbFlag ||
            mb.mbN.DcBlockCodedCrFlag)
        {
            if (fp.Transform8x8Flag &&
                mb.parts.noPartsLessThan8x8 &&
                mb.mbN.CodedPattern4x4Y > 0 &&
                (mb.mbN.Skip8x8Flag != 0xf || fp.Direct8x8InferenceFlag))
            {
                PackTransform8x8FlagCabac(m_bs, mb);
            }

            H264BsReal_DQuant_CABAC_8u16s(&m_bs, mbQpDelta, ctxInc);
            m_prevMbQpY = mb.mbN.QpPrimeY;
            m_prevMbQpDelta = mbQpDelta;
        }
    }
}

void H264PakBitstream::PackMbHeaderCavlc(
    const mfxFrameParamAVC& fp,
    const mfxSliceParamAVC& sp,
    const MbDescriptor& mb)
{
    if ((sp.SliceType & MFX_SLICETYPE_I) == 0)
    {
        if (mb.mbN.reserved3c == 0)
        {
            H264BsReal_PutVLCCode_8u16s(&m_bs, m_skipRun);
            m_skipRun = 0;
        }
        else
        {
            m_skipRun++;
        }
    }

    if (mb.mbN.IntraMbFlag)
    {
        if (mb.mbN.MbType5Bits == MBTYPE_I_4x4)
        {
            PackIntra4x4TypeCavlc(m_bs, fp, sp, mb);
            PackCbpCavlc(m_bs, fp, mb);

            if (mb.mbN.CodedPattern4x4Y > 0 ||
                mb.mbN.CodedPattern4x4U > 0 ||
                mb.mbN.CodedPattern4x4V > 0 ||
                mb.mbN.DcBlockCodedCbFlag ||
                mb.mbN.DcBlockCodedCrFlag)
            {
                H264BsReal_PutDQUANT_8u16s(&m_bs, mb.mbN.QpPrimeY, m_prevMbQpY);
                m_prevMbQpY = mb.mbN.QpPrimeY;
            }
        }
        else if (mb.mbN.MbType5Bits == MBTYPE_I_IPCM)
        {
            assert(0);
        }
        else // INTRA 16x16
        {
            PackIntra16x16TypeCavlc(m_bs, fp, sp, mb);
            H264BsReal_PutDQUANT_8u16s(&m_bs, mb.mbN.QpPrimeY, m_prevMbQpY);
            m_prevMbQpY = mb.mbN.QpPrimeY;
        }
    }
    else if (mb.mbN.reserved3c == 0)
    {
        if (sp.SliceType & MFX_SLICETYPE_P)
        {
            H264BsReal_PutVLCCode_8u16s(&m_bs, GetValueForMbTypeP(mb.mbN));

            if (mb.mbN.MbType5Bits == MBTYPE_BP_8x8)
            {
                for (mfxU32 part = 0; part < 4; part++)
                {
                    H264BsReal_PutVLCCode_8u16s(
                        &m_bs,
                        GetValueForSubTypeP(mb.mbN, part));
                }
            }
        }
        else // MFX_SLICETYPE_B
        {
            H264BsReal_PutVLCCode_8u16s(&m_bs, GetValueForMbTypeB(mb.mbN));

            if (mb.mbN.MbType5Bits == MBTYPE_BP_8x8 && mb.mbN.Skip8x8Flag != 0xf)
            {
                for (mfxU32 part = 0; part < 4; part++)
                {
                    H264BsReal_PutVLCCode_8u16s(
                        &m_bs,
                        GetValueForSubTypeB(mb.mbN, part));
                }
            }
        }

        PackMvsCavlc(m_bs, sp, mb);
        PackCbpCavlc(m_bs, fp, mb);

        if (mb.mbN.CodedPattern4x4Y > 0 ||
            mb.mbN.CodedPattern4x4U > 0 ||
            mb.mbN.CodedPattern4x4V > 0 ||
            mb.mbN.DcBlockCodedCbFlag ||
            mb.mbN.DcBlockCodedCrFlag)
        {
            if (fp.Transform8x8Flag &&
                mb.parts.noPartsLessThan8x8 &&
                mb.mbN.CodedPattern4x4Y > 0 &&
                (mb.mbN.Skip8x8Flag != 0xf || fp.Direct8x8InferenceFlag))
            {
                H264BsReal_PutBit_8u16s(&m_bs, mb.mbN.TransformFlag);
            }
            else
            {
                mb.mbN.TransformFlag = 0;
            }

            H264BsReal_PutDQUANT_8u16s(&m_bs, mb.mbN.QpPrimeY, m_prevMbQpY);
            m_prevMbQpY = mb.mbN.QpPrimeY;
        }
        else
        {
            mb.mbN.TransformFlag = 0;
        }
    }
    else if (mb.mbN.LastMbFlag && m_skipRun > 0)
    {
        H264BsReal_PutVLCCode_8u16s(&m_bs, m_skipRun);
    }
}

static const mfxU32 ctx_id_trans0[8] =
{
    1, 2, 3, 3, 4, 5, 6, 7
};

static const mfxU32 ctx_id_trans1[8] =
{
    4, 4, 4, 4, 5, 6, 7, 7
};

static const mfxU32 ctx_id_trans13[7] =
{
    4, 4, 4, 4, 5, 6, 6
};

static const mfxU32 ctx_neq1p1[8] =
{
    1, 2, 3, 4, 0, 0, 0, 0
};

static const mfxU32 ctx_ngt1[8] =
{
    5, 5, 5, 5, 6, 7, 8, 9
};

static const mfxI32* g_CtxIdxOffset[2] =
{
    ctxIdxOffsetFrameCoded,
    ctxIdxOffsetFieldCoded
};

static const mfxI32* g_CtxIdxOffset8x8[2] =
{
    ctxIdxOffsetFrameCoded_BlockCat_5,
    ctxIdxOffsetFieldCoded_BlockCat_5
};

static void PackResidualBlockCabac(
    H264BsReal_8u16s& bitstream,
    const CabacData4x4& cabacData,
    mfxU32 numSigCoeff,
    mfxU32 fieldPicFlag,
    mfxU32 ctxBlockCat)
{
    mfxU32 ctx_sig =
        g_CtxIdxOffset[fieldPicFlag][SIGNIFICANT_COEFF_FLAG] +
        ctxIdxBlockCatOffset[SIGNIFICANT_COEFF_FLAG][ctxBlockCat];

    mfxU32 ctx_last =
        g_CtxIdxOffset[fieldPicFlag][LAST_SIGNIFICANT_COEFF_FLAG] +
        ctxIdxBlockCatOffset[LAST_SIGNIFICANT_COEFF_FLAG][ctxBlockCat];

    mfxU32 ctx =
        g_CtxIdxOffset[fieldPicFlag][COEFF_ABS_LEVEL_MINUS1] +
        ctxIdxBlockCatOffset[COEFF_ABS_LEVEL_MINUS1][ctxBlockCat];

    const mfxU32* ctx_trans0 = ctx_id_trans0;
    const mfxU32* ctx_trans1 = ctxBlockCat == BLOCK_CHROMA_DC_LEVELS ? ctx_id_trans13 : ctx_id_trans1;
    mfxU8 sigMap[16];

    mfxU8 sigCoeffCounter = 0;
    if (ctxBlockCat == BLOCK_CHROMA_DC_LEVELS)
    {
        const mfxU8 lastCoeff = 3;

        mfxU8 i = 0;
        for (; i < lastCoeff && sigCoeffCounter < numSigCoeff; i++)
        {
            if (cabacData.coeffs[i] == 0)
            {
                H264BsReal_EncodeSingleBin_CABAC_8u16s(
                    &bitstream,
                    ctx_sig + MIN((i >> bitstream.num8x8Cshift2), 2),
                    0);
            }
            else
            {
                H264BsReal_EncodeSingleBin_CABAC_8u16s(
                    &bitstream,
                    ctx_sig + MIN((i >> bitstream.num8x8Cshift2), 2),
                    1);

                sigMap[sigCoeffCounter++] = i;

                mfxU32 lastSig = (sigCoeffCounter == numSigCoeff);
                H264BsReal_EncodeSingleBin_CABAC_8u16s(
                    &bitstream,
                    ctx_last + MIN((i >> bitstream.num8x8Cshift2), 2),
                    lastSig);
            }
        }

        if (cabacData.coeffs[lastCoeff] != 0)
        {
            sigMap[sigCoeffCounter++] = lastCoeff;
        }
    }
    else
    {
        const mfxU8 lastCoeff =
            ctxBlockCat == BLOCK_LUMA_AC_LEVELS || ctxBlockCat == BLOCK_CHROMA_AC_LEVELS ? 14 : 15;

        mfxU8 i = 0;
        for (; i < lastCoeff && sigCoeffCounter < numSigCoeff; i++)
        {
            if (cabacData.coeffs[i] == 0)
            {
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx_sig + i, 0);
            }
            else
            {
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx_sig + i, 1);
                sigMap[sigCoeffCounter++] = i;

                mfxU32 lastSig = (sigCoeffCounter == numSigCoeff);
                H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx_last + i, lastSig);
            }
        }

        if (cabacData.coeffs[lastCoeff] != 0)
        {
            sigMap[sigCoeffCounter++] = lastCoeff;
        }
    }

    assert(sigCoeffCounter == numSigCoeff);

    mfxU32 ctx_id = 0;

    for (mfxI32 i = sigCoeffCounter - 1; i >= 0; i--)
    {
        mfxI16 coeff = cabacData.coeffs[sigMap[i]];
        mfxU32 ctxInc = ctx_neq1p1[ctx_id];
        mfxU16 sign = coeff >> 15; // sign = coeff > 0 ? 0 : 0xffff;
        mfxU16 level = (coeff + (~sign)) ^ sign; // level = abs(coeff) - 1

        if (level)
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + ctxInc, 1);
            H264BsReal_EncodeExGRepresentedLevels_CABAC_8u16s(&bitstream, ctx + ctx_ngt1[ctx_id], --level);
            ctx_id = ctx_trans1[ctx_id];
        }
        else
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + ctxInc, 0);
            ctx_id = ctx_trans0[ctx_id];
        }

        H264BsReal_EncodeBypass_CABAC_8u16s(&bitstream, sign);
    }
}

static void PackResidualBlock8x8Cabac(
    H264BsReal_8u16s& bitstream,
    const CabacData8x8& cabacData,
    mfxU32 numSigCoeff,
    bool fieldPicFlag)
{
    mfxU32 ctx_sig =
        g_CtxIdxOffset8x8[fieldPicFlag][SIGNIFICANT_COEFF_FLAG] +
        ctxIdxBlockCatOffset[SIGNIFICANT_COEFF_FLAG][BLOCK_LUMA_64_LEVELS];

    mfxU32 ctx_last =
        g_CtxIdxOffset8x8[fieldPicFlag][LAST_SIGNIFICANT_COEFF_FLAG] +
        ctxIdxBlockCatOffset[LAST_SIGNIFICANT_COEFF_FLAG][BLOCK_LUMA_64_LEVELS];

    mfxU32 ctx =
        g_CtxIdxOffset8x8[fieldPicFlag][COEFF_ABS_LEVEL_MINUS1] +
        ctxIdxBlockCatOffset[COEFF_ABS_LEVEL_MINUS1][BLOCK_LUMA_64_LEVELS];

    const mfxU32* ctx_trans0 = ctx_id_trans0;
    const mfxU32* ctx_trans1 = ctx_id_trans1;
    const mfxU8 lastCoeff = 63;
    mfxU8 sigMap[64];

    mfxU8 sigCoeffCounter = 0;
    mfxU8 i = 0;
    for (; i < lastCoeff && sigCoeffCounter < numSigCoeff; i++)
    {
        if (cabacData.coeffs[i] == 0)
        {
           H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx_sig + Table_9_34[fieldPicFlag][i], 0);
        }
        else
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx_sig + Table_9_34[fieldPicFlag][i], 1);
            sigMap[sigCoeffCounter++] = i;

            mfxU32 lastSig = (sigCoeffCounter == numSigCoeff);
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx_last + Table_9_34[2][i], lastSig);
        }
    }

    if (cabacData.coeffs[lastCoeff] != 0)
    {
        sigMap[sigCoeffCounter++] = lastCoeff;
    }

    assert(sigCoeffCounter == numSigCoeff);

    mfxI32 ctx_id = 0;

    for (mfxI32 i = sigCoeffCounter - 1; i >= 0; i--)
    {
        mfxI16 coeff = cabacData.coeffs[sigMap[i]];
        mfxU32 ctxInc = ctx_neq1p1[ctx_id];
        mfxU16 sign = coeff >> 15; // sign = coeff > 0 ? 0 : 0xffff;
        mfxU16 level = (coeff + (~sign)) ^ sign; // level = abs(coeff) - 1

        if (level)
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + ctxInc, 1);
            H264BsReal_EncodeExGRepresentedLevels_CABAC_8u16s(&bitstream, ctx + ctx_ngt1[ctx_id], --level);
            ctx_id = ctx_trans1[ctx_id];
        }
        else
        {
            H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx + ctxInc, 0);
            ctx_id = ctx_trans0[ctx_id];
        }

        H264BsReal_EncodeBypass_CABAC_8u16s(&bitstream, sign);
    }
}

static void PackDcLumaCabac(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const MbDescriptor& mb,
    const CoeffData& coeffData)
{
    bool isCurIntra = IsTrueIntra(mb.mbN);
    mfxU32 flagA = mb.mbA ? mb.mbA->DcBlockCodedYFlag : isCurIntra;
    mfxU32 flagB = mb.mbB ? mb.mbB->DcBlockCodedYFlag : isCurIntra;

    mfxU32 ctx =
        ctxIdxOffsetFrameCoded[CODED_BLOCK_FLAG] +
        ctxIdxBlockCatOffset[CODED_BLOCK_FLAG][BLOCK_LUMA_DC_LEVELS] +
        2 * flagB + flagA;

    mfxU32 numSigCoeff = coeffData.cabac.dcAux[BLOCK_DC_Y].numSigCoeff;
    const CabacData4x4& cabacData = coeffData.cabac.dc[BLOCK_DC_Y];

    H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx, numSigCoeff != 0);

    PackResidualBlockCabac(
        bitstream,
        cabacData,
        numSigCoeff,
        fp.FieldPicFlag,
        BLOCK_LUMA_DC_LEVELS);
}

static void PackDcChromaCabac(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const MbDescriptor& mb,
    const CoeffData& coeffData,
    mfxU32 component)
{
    bool isCurIntra = IsTrueIntra(mb.mbN);

    mfxU32 flagA = mb.mbA == 0
        ? isCurIntra
        : component == BLOCK_DC_U ? mb.mbA->DcBlockCodedCbFlag : mb.mbA->DcBlockCodedCrFlag;

    mfxU32 flagB = mb.mbB == 0
        ? isCurIntra
        : component == BLOCK_DC_U ? mb.mbB->DcBlockCodedCbFlag : mb.mbB->DcBlockCodedCrFlag;

    mfxU32 ctx =
        ctxIdxOffsetFrameCoded[CODED_BLOCK_FLAG] +
        ctxIdxBlockCatOffset[CODED_BLOCK_FLAG][BLOCK_CHROMA_DC_LEVELS] +
        2 * flagB + flagA;

    mfxU32 numSigCoeff = coeffData.cabac.dcAux[component].numSigCoeff;
    const CabacData4x4& cabacData = coeffData.cabac.dc[component];

    H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx, numSigCoeff != 0);

    PackResidualBlockCabac(
        bitstream,
        cabacData,
        numSigCoeff,
        fp.FieldPicFlag,
        BLOCK_CHROMA_DC_LEVELS);
}

static void PackDcCavlc(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const MbDescriptor& mb,
    const CoeffData& coeffData,
    mfxU32 coeffDataIdx)
{
    assert(coeffDataIdx < 3);

    const CavlcData& cavlcData = coeffData.cavlc.dc[coeffDataIdx];
    mfxU8 lastSigCoeff = coeffData.cavlc.dcAux[coeffDataIdx].lastSigCoeff;

    mfxU32 N = 0;
    mfxU32 maxCoeffs = 0;
    mfxU32 chromaDc = (coeffDataIdx == BLOCK_DC_Y) ? 0 : fp.ChromaFormatIdc;

    mfxI16 levels[16];
    mfxU8 runs[16];
    mfxU8 trailingOnes = 0;
    mfxU8 trailingOneSigns = 0;
    mfxU8 numCoeffs = 0;
    mfxU8 numZeros = 0;

    if (coeffDataIdx == BLOCK_DC_Y)
    {
        ippiEncodeCoeffsCAVLC_H264_16s(
            cavlcData.coeffs,
            0,
            g_ScanMatrixDec4x4[fp.FieldPicFlag],
            lastSigCoeff,
            &trailingOnes,
            &trailingOneSigns,
            &numCoeffs,
            &numZeros,
            levels,
            runs);

        maxCoeffs = 16;

        N = mb.mbA && mb.mbB
            ? (GetNumCavlcCoeffLuma(*mb.mbA, 5) + GetNumCavlcCoeffLuma(*mb.mbB, 10) + 1) / 2
            : mb.mbA
                ? GetNumCavlcCoeffLuma(*mb.mbA, 5)
                : mb.mbB
                    ? GetNumCavlcCoeffLuma(*mb.mbB, 10)
                    : 0;
    }
    else
    {
        ippiEncodeChromaDcCoeffsCAVLC_H264_16s(
            cavlcData.coeffs,
            &trailingOnes,
            &trailingOneSigns,
            &numCoeffs,
            &numZeros,
            levels,
            runs);

        maxCoeffs = 2 << fp.ChromaFormatIdc;
    }

    H264BsReal_PutNumCoeffAndTrailingOnes_8u16s(
        &bitstream,
        N,
        chromaDc,
        numCoeffs,
        trailingOnes,
        trailingOneSigns,
        16);

    if (numCoeffs != 0)
    {
        mfxI32 iCoeffstoWrite = numCoeffs - trailingOnes;

        if (iCoeffstoWrite)
        {
            H264BsReal_PutLevels_8u16s(&bitstream, levels, iCoeffstoWrite, trailingOnes);
        }

        if (numCoeffs != maxCoeffs)
        {
            H264BsReal_PutTotalZeros_8u16s(&bitstream, numZeros, numCoeffs, chromaDc, 16);

            if (numZeros && numCoeffs > 1)
            {
                H264BsReal_PutRuns_8u16s(&bitstream, runs, numZeros, numCoeffs);
            }
        }
    }
}

static void PackSubBlockLuma4x4Cabac(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const MbDescriptor& mb,
    const CoeffData& coeffData,
    mfxU32 block)
{
    assert(block < 16);

    bool isCurIntra = IsTrueIntra(mb.mbN);
    bool isIntra16x16 = IsIntra16x16(mb.mbN);

    mfxU32 flagA = (block & 5)
        ? ((mb.mbN.CodedPattern4x4Y >> g_BlockA[block]) & 1)
        : mb.mbA
            ? ((mb.mbA->CodedPattern4x4Y >> g_BlockA[block]) & 1)
            : isCurIntra;

    mfxU32 flagB = (block & 10)
        ? ((mb.mbN.CodedPattern4x4Y >> g_BlockB[block]) & 1)
        : mb.mbB
            ? ((mb.mbB->CodedPattern4x4Y >> g_BlockB[block]) & 1)
            : isCurIntra;

    mfxU32 ctxBlockIdx = isIntra16x16 ? BLOCK_LUMA_AC_LEVELS : BLOCK_LUMA_LEVELS;
    mfxU32 ctx =
        ctxIdxOffsetFrameCoded[CODED_BLOCK_FLAG] +
        ctxIdxBlockCatOffset[CODED_BLOCK_FLAG][ctxBlockIdx] +
        2 * flagB + flagA;

    mfxU32 numSigCoeff = coeffData.cabac.luma4x4Aux[block].numSigCoeff;
    const CabacData4x4& cabacData = coeffData.cabac.luma4x4[block];

    H264BsReal_EncodeSingleBin_CABAC_8u16s(&bitstream, ctx, numSigCoeff != 0);

    if (numSigCoeff)
    {
        PackResidualBlockCabac(bitstream, cabacData, numSigCoeff, fp.FieldPicFlag, ctxBlockIdx);
    }
}

static void PackSubBlockLuma8x8Cabac(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const CoeffData& coeffData,
    mfxU32 block)
{
    assert(block < 4);

    mfxU32 numSigCoeff = coeffData.cabac.luma8x8Aux[block].numSigCoeff;
    const CabacData8x8& cabacData = coeffData.cabac.luma8x8[block];

    if (numSigCoeff)
    {
        PackResidualBlock8x8Cabac(bitstream, cabacData, numSigCoeff, fp.FieldPicFlag);
    }
}

static void PackSubBlockLumaCavlc(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const MbDescriptor& mb,
    const CoeffData& coeffData,
    mfxU32 block)
{
    mfxU32 maxCoeffs = 16;

    if (IsIntra16x16(mb.mbN))
    {
        maxCoeffs = 15;
    }

    const CavlcData& cavlcData = coeffData.cavlc.luma[block];
    mfxU8 lastSigCoeff = coeffData.cavlc.lumaAux[block].lastSigCoeff;

    mfxU8 coeffsA = !IsOnLeftEdge(block)
        ? GetNumCavlcCoeffLuma(mb.mbN, g_BlockA[block])
        : mb.mbA
            ? GetNumCavlcCoeffLuma(*mb.mbA, g_BlockA[block])
            : 0xff;

    mfxU8 coeffsB = !IsOnTopEdge(block)
        ? GetNumCavlcCoeffLuma(mb.mbN, g_BlockB[block])
        : mb.mbB
            ? GetNumCavlcCoeffLuma(*mb.mbB, g_BlockB[block])
            : 0xff;

    mfxU8 N = coeffsA < 0xff && coeffsB < 0xff
        ? (coeffsA + coeffsB + 1) / 2
        : coeffsA < 0xff
            ? coeffsA
            : coeffsB < 0xff
                ? coeffsB
                : 0;

    mfxI16 levels[16];
    mfxU8 runs[16];
    mfxU8 trailingOnes = 0;
    mfxU8 trailingOneSigns = 0;
    mfxU8 numCoeffs = 0;
    mfxU8 numZeros = 0;

    if (lastSigCoeff != 0xff)
    {
        ippiEncodeCoeffsCAVLC_H264_16s(
            cavlcData.coeffs,
            maxCoeffs == 15 ? 1 : 0,
            g_ScanMatrixDec4x4[fp.FieldPicFlag],
            lastSigCoeff,
            &trailingOnes,
            &trailingOneSigns,
            &numCoeffs,
            &numZeros,
            levels,
            runs);
    }

    SetNumCavlcCoeffLuma(mb.mbN, block, numCoeffs);

    H264BsReal_PutNumCoeffAndTrailingOnes_8u16s(
        &bitstream,
        N,
        0,
        numCoeffs,
        trailingOnes,
        trailingOneSigns,
        16);

    if (numCoeffs != 0)
    {
        mfxU32 coeffstoWrite = numCoeffs - trailingOnes;

        if (coeffstoWrite)
        {
            H264BsReal_PutLevels_8u16s(&bitstream, levels, coeffstoWrite, trailingOnes);
        }

        if (numCoeffs != maxCoeffs)
        {
            H264BsReal_PutTotalZeros_8u16s(&bitstream, numZeros, numCoeffs, 0, 16);

            if (numZeros && numCoeffs > 1)
            {
                H264BsReal_PutRuns_8u16s(&bitstream, runs, numZeros, numCoeffs);
            }
        }
    }
}

static void PackSubBlockChromaCabac(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const MbDescriptor& mb,
    const CoeffData& coeffData,
    mfxU32 block)
{
    assert(block < 8);

    mfxU32 numChromaBlock = (2 << fp.ChromaFormatIdc);
    mfxU32 uvSelect = block >= numChromaBlock;
    mfxU32 block4x4 = block - numChromaBlock * uvSelect;

    bool isCurIntra = mb.mbN.IntraMbFlag && mb.mbN.MbType5Bits != MBTYPE_I_IPCM;

    mfxU32 flagA;
    mfxU32 flagB;

    if (uvSelect == 0)
    {
        flagA = (block4x4 & 1)
            ? ((mb.mbN.CodedPattern4x4U >> (block4x4 - 1)) & 1)
            : mb.mbA
                ? ((mb.mbA->CodedPattern4x4U >> (block4x4 + 1)) & 1)
                : isCurIntra;

        flagB = (block4x4 > 1)
            ? ((mb.mbN.CodedPattern4x4U >> (block4x4 - 2)) & 1)
            : mb.mbB
                ? ((mb.mbB->CodedPattern4x4U >> (block4x4 + 2)) & 1)
                : isCurIntra;
    }
    else
    {
        flagA = (block4x4 & 1)
            ? ((mb.mbN.CodedPattern4x4V >> (block4x4 - 1)) & 1)
            : mb.mbA
                ? ((mb.mbA->CodedPattern4x4V >> (block4x4 + 1)) & 1)
                : isCurIntra;

        flagB = (block4x4 > 1)
            ? ((mb.mbN.CodedPattern4x4V >> (block4x4 - 2)) & 1)
            : mb.mbB
                ? ((mb.mbB->CodedPattern4x4V >> (block4x4 + 2)) & 1)
                : isCurIntra;
    }

    mfxU32 ctxInc = 2 * flagB + flagA;
    H264BsReal_EncodeSingleBin_CABAC_8u16s(
        &bitstream,
        ctxIdxOffsetFrameCoded[CODED_BLOCK_FLAG] +
            ctxIdxBlockCatOffset[CODED_BLOCK_FLAG][BLOCK_CHROMA_AC_LEVELS] +
            ctxInc,
        coeffData.cabac.chromaAux[block].numSigCoeff != 0);

    if (coeffData.cabac.chromaAux[block].numSigCoeff != 0)
    {
        PackResidualBlockCabac(
            bitstream,
            coeffData.cabac.chroma[block],
            coeffData.cabac.chromaAux[block].numSigCoeff,
            fp.FieldPicFlag,
            BLOCK_CHROMA_AC_LEVELS);
    }
}

static void PackSubBlockChromaCavlc(
    H264BsReal_8u16s& bitstream,
    const mfxFrameParamAVC& fp,
    const MbDescriptor& mb,
    const CoeffData& coeffData,
    mfxU32 block)
{
    assert(block < 8);
    const CavlcData& cavlcData = coeffData.cavlc.chroma[block];
    mfxU8 lastSigCoeff = coeffData.cavlc.chromaAux[block].lastSigCoeff;

    mfxU8 coeffsA = (block & 1)
        ? GetNumCavlcCoeffChroma(mb.mbN, block - 1)
        : mb.mbA
            ? GetNumCavlcCoeffChroma(*mb.mbA, block + 1)
            : 0xff;

    mfxU8 coeffsB = (block & 10)
        ? GetNumCavlcCoeffChroma(mb.mbN, block - 2)
        : mb.mbB
            ? GetNumCavlcCoeffChroma(*mb.mbB, block + 2)
            : 0xff;

    mfxU8 N = coeffsA < 0xff && coeffsB < 0xff
        ? (coeffsA + coeffsB + 1) / 2
        : coeffsA < 0xff
            ? coeffsA
            : coeffsB < 0xff
                ? coeffsB
                : 0;

    mfxI16 levels[16];
    mfxU8 runs[16];
    mfxU8 trailingOnes = 0;
    mfxU8 trailingOneSigns = 0;
    mfxU8 numCoeffs = 0;
    mfxU8 numZeros = 0;

    if (lastSigCoeff != 0xff)
    {
        ippiEncodeCoeffsCAVLC_H264_16s(
            cavlcData.coeffs,
            1,
            g_ScanMatrixDec4x4[fp.FieldPicFlag],
            lastSigCoeff,
            &trailingOnes,
            &trailingOneSigns,
            &numCoeffs,
            &numZeros,
            levels,
            runs);
    }

    SetNumCavlcCoeffChroma(mb.mbN, block, numCoeffs);

    H264BsReal_PutNumCoeffAndTrailingOnes_8u16s(
        &bitstream,
        N,
        0,
        numCoeffs,
        trailingOnes,
        trailingOneSigns,
        16);

    if (numCoeffs != 0)
    {
        if (numCoeffs - trailingOnes > 0)
        {
            H264BsReal_PutLevels_8u16s(&bitstream, levels, numCoeffs - trailingOnes, trailingOnes);
        }

        if (numCoeffs != 15)
        {
            H264BsReal_PutTotalZeros_8u16s(&bitstream, numZeros, numCoeffs, 0, 16);

            if (numZeros && numCoeffs > 1)
            {
                H264BsReal_PutRuns_8u16s(&bitstream, runs, numZeros, numCoeffs);
            }
        }
    }
}

void H264PakBitstream::PackMbCabac(
    const mfxFrameParamAVC& fp,
    const mfxSliceParamAVC& sp,
    const MbDescriptor& mb,
    const CoeffData& coeffData)
{
    memset(mb.mvdN, 0, sizeof(*mb.mvdN));

    PackMbHeaderCabac(fp, sp, mb);

    if (mb.mbN.reserved3c == 0) // not skipped
    {
        if (mb.mbN.IntraMbFlag &&
            mb.mbN.MbType5Bits >= MBTYPE_I_16x16_000 &&
            mb.mbN.MbType5Bits <= MBTYPE_I_16x16_321)
        {
            PackDcLumaCabac(m_bs, fp, mb, coeffData);
        }

        mfxU32 cbp8x8 = GetCbp8x8(mb.mbN);
        for (mfxU32 blk8x8 = 0; blk8x8 < 4; blk8x8++)
        {
            if (mb.mbN.TransformFlag == 0)
            {
                if (cbp8x8 & (1 << blk8x8))
                {
                    for (mfxU32 blk4x4 = 0; blk4x4 < 4; blk4x4++)
                    {
                        PackSubBlockLuma4x4Cabac(m_bs, fp, mb, coeffData, 4 * blk8x8 + blk4x4);
                    }
                }
            }
            else if (cbp8x8 & (1 << blk8x8))
            {
                PackSubBlockLuma8x8Cabac(m_bs, fp, coeffData, blk8x8);
            }
        }

        if (fp.ChromaFormatIdc != 0) // not monochrome
        {
            if (mb.mbN.CodedPattern4x4U || mb.mbN.DcBlockCodedCbFlag ||
                mb.mbN.CodedPattern4x4V || mb.mbN.DcBlockCodedCrFlag)
            {
                PackDcChromaCabac(m_bs, fp, mb, coeffData, BLOCK_DC_U);
                PackDcChromaCabac(m_bs, fp, mb, coeffData, BLOCK_DC_V);
            }

            if (mb.mbN.CodedPattern4x4U != 0 || mb.mbN.CodedPattern4x4V != 0)
            {
                mfxU32 blocks = 4 << fp.ChromaFormatIdc;

                for (mfxU32 i = 0; i < blocks; i++)
                {
                    PackSubBlockChromaCabac(m_bs, fp, mb, coeffData, i);
                }
            }
        }
    }

    H264BsReal_EncodeFinalSingleBin_CABAC_8u16s(&m_bs, mb.mbN.LastMbFlag);

    if (mb.mbN.LastMbFlag)
    {
        H264BsReal_TerminateEncode_CABAC_8u16s(&m_bs);
    }
}

void H264PakBitstream::PackMbCavlc(
    const mfxFrameParamAVC& fp,
    const mfxSliceParamAVC& sp,
    const MbDescriptor& mb,
    const CoeffData& coeffData)
{
    PackMbHeaderCavlc(fp, sp, mb);

    if (mb.mbN.reserved3c == 0) // not skipped
    {
        if (mb.mbN.IntraMbFlag &&
            mb.mbN.MbType5Bits >= MBTYPE_I_16x16_000 &&
            mb.mbN.MbType5Bits <= MBTYPE_I_16x16_321)
        {
            PackDcCavlc(m_bs, fp, mb, coeffData, BLOCK_DC_Y);
        }

        mfxU32 cbp8x8 = GetCbp8x8(mb.mbN);
        for (mfxU32 blk8x8 = 0; blk8x8 < 4; blk8x8++)
        {
            if (cbp8x8 & (1 << blk8x8))
            {
                for (mfxU32 blk4x4 = 0; blk4x4 < 4; blk4x4++)
                {
                    PackSubBlockLumaCavlc(m_bs, fp, mb, coeffData, 4 * blk8x8 + blk4x4);
                }
            }
        }

        if (fp.ChromaFormatIdc != 0) // not monochrome
        {
            if (mb.mbN.CodedPattern4x4U || mb.mbN.DcBlockCodedCbFlag ||
                mb.mbN.CodedPattern4x4V || mb.mbN.DcBlockCodedCrFlag)
            {
                 PackDcCavlc(m_bs, fp, mb, coeffData, BLOCK_DC_U);
                 PackDcCavlc(m_bs, fp, mb, coeffData, BLOCK_DC_V);
            }

            if (mb.mbN.CodedPattern4x4U != 0 || mb.mbN.CodedPattern4x4V != 0)
            {
                mfxU32 blocks = 4 << fp.ChromaFormatIdc;

                for (mfxU32 i = 0; i < blocks; i++)
                {
                    PackSubBlockChromaCavlc(m_bs, fp, mb, coeffData, i);
                }
            }
        }
    }

    if (mb.mbN.LastMbFlag)
    {
        H264BsBase_WriteTrailingBits(&m_bs.m_base);
    }
}

void H264PakBitstream::PackMb(
    const mfxFrameParamAVC& fp,
    const mfxSliceParamAVC& sp,
    const MbDescriptor& mb,
    const CoeffData& coeffData)
{
    if (fp.EntropyCodingModeFlag)
    {
        PackMbCabac(fp, sp, mb, coeffData);
    }
    else
    {
        PackMbCavlc(fp, sp, mb, coeffData);
    }
}

mfxStatus H264PakBitstream::GetRbsp(mfxBitstream& bs)
{
    mfxU8* end = m_bs.m_base.m_pbs;
    mfxU8* begin = m_bs.m_pbsRBSPBase;
    mfxU8* out = bs.Data + bs.DataOffset + bs.DataLength;
    mfxU8* stop = bs.Data + bs.MaxLength;

    if (end - begin > stop - out)
    {
        // assert(!"not enough bitstream buffer");
        bs.DataLength += mfxU32(end - begin);
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    }

    // copy start code
    while (begin + 2 < end && *begin == 0)
    {
        *out++ = *begin++;
    }

    // copy other data checking for start code emulation
    while (begin + 2 < end)
    {
        *out++ = *begin++;

        if (*(begin - 1) == 0 && *begin == 0 && (*(begin + 1) & 0xfc) == 0)
        {
            if (end - begin + 1 > stop - out)
            {
                // assert(!"not enough bitstream buffer");
                mfxU32 written = mfxU32(out - bs.Data - bs.DataOffset - bs.DataLength);
                bs.DataLength += mfxU32((end - begin) + written);
                return MFX_ERR_NOT_ENOUGH_BUFFER;
            }

            *out++ = *begin++;
            *out++ = 0x03; // start code emulation prevention byte
        }
    }

    // copy remainder
    while (begin < end)
    {
        *out++ = *begin++;
    }

    m_bs.m_pbsRBSPBase = m_bs.m_base.m_pbs;
    bs.DataLength += mfxU32(out - (bs.Data + bs.DataOffset + bs.DataLength));

    return MFX_ERR_NONE;
}

#endif // MFX_ENABLE_H264_VIDEO_ENCODER
