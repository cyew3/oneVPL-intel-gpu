/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"
#include "mfx_h265_dispatcher.h"
//#include "ipp.h"

using namespace MFX_HEVC_PP;

#if defined (MFX_TARGET_OPTIMIZATION_AUTO)

void SetTargetAVX2(void);
void SetTargetSSE4(void);
void SetTargetSSSE3(void);
void SetTargetSSE4_ATOM(void);
void SetTargetPX(void);

MFX_HEVC_PP::Dispatcher MFX_HEVC_PP::g_dispatcher = {};

IppStatus MFX_HEVC_PP::InitDispatcher(Ipp32s cpuFeature)
{
    // No multiple initialization
    if(g_dispatcher.isInited)
        return ippStsNoErr;

    if (cpuFeature == CPU_FEAT_AUTO) {
        // GetPlatformType
        Ipp32u cpuIdInfoRegs[4];
        Ipp64u featuresMask;
        IppStatus sts = ippGetCpuFeatures(&featuresMask, cpuIdInfoRegs);
        if (ippStsNoErr != sts)
            return sts;

        if (featuresMask & (Ipp64u)(ippCPUID_AVX2)) // means AVX2 + BMI_I + BMI_II to prevent issues with BMI
            cpuFeature = CPU_FEAT_AVX2;
        else if ((featuresMask & (Ipp64u)(ippCPUID_MOVBE)) && (featuresMask & (Ipp64u)(ippCPUID_SSE42)) && !(featuresMask & (Ipp64u)(ippCPUID_AVX)))
            cpuFeature = CPU_FEAT_SSE4_ATOM;
        else if (featuresMask & (Ipp64u)(ippCPUID_SSE42))
            cpuFeature = CPU_FEAT_SSE4;
        else if (featuresMask & (Ipp64u)(ippCPUID_SSSE3))
            cpuFeature = CPU_FEAT_SSSE3;
        else
            cpuFeature = CPU_FEAT_PX;
    }

    switch (cpuFeature) {
    case CPU_FEAT_AVX2:      SetTargetAVX2(); break;
    case CPU_FEAT_SSE4_ATOM: SetTargetSSE4_ATOM(); break;
    case CPU_FEAT_SSE4:      SetTargetSSE4(); break;
    case CPU_FEAT_SSSE3:     SetTargetSSSE3(); break;
    default:                 VM_ASSERT(!"requested unknown cpu feature");
    case CPU_FEAT_PX:        SetTargetPX(); break;
    }

    g_dispatcher.isInited = true;
    return ippStsNoErr;
}


#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
// [sad.special - dispatcher]
int MFX_HEVC_PP::h265_SAD_MxN_special_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY)
{
    if (SizeX == 4)
    {
        if(SizeY == 4) { return g_dispatcher.h265_SAD_4x4_8u(image,  ref, stride); }
        if(SizeY == 8) { return g_dispatcher.h265_SAD_4x8_8u(image,  ref, stride); }
        else           { return g_dispatcher.h265_SAD_4x16_8u(image,  ref, stride); }
    }
    else if (SizeX == 8)
    {
        if(SizeY ==  4) { return g_dispatcher.h265_SAD_8x4_8u(image,  ref, stride); }
        if(SizeY ==  8) { return g_dispatcher.h265_SAD_8x8_8u(image,  ref, stride); }
        if(SizeY == 16) { return g_dispatcher.h265_SAD_8x16_8u(image,  ref, stride); }
        else            { return g_dispatcher.h265_SAD_8x32_8u(image,  ref, stride); }
    }
    else if (SizeX == 12)
    {            
        return g_dispatcher.h265_SAD_12x16_8u(image,  ref, stride);
    }
    else if (SizeX == 16)
    {
        if(SizeY ==  4) { return g_dispatcher.h265_SAD_16x4_8u(image,  ref, stride); }
        if(SizeY ==  8) { return g_dispatcher.h265_SAD_16x8_8u(image,  ref, stride); }
        if(SizeY == 12) { return g_dispatcher.h265_SAD_16x12_8u(image,  ref, stride);}
        if(SizeY == 16) { return g_dispatcher.h265_SAD_16x16_8u(image,  ref, stride);}
        if(SizeY == 32) { return g_dispatcher.h265_SAD_16x32_8u(image,  ref, stride);}
        else            { return g_dispatcher.h265_SAD_16x64_8u(image,  ref, stride);}
    }
    else if (SizeX == 24)
    {
        return g_dispatcher.h265_SAD_24x32_8u(image,  ref, stride);
    }
    else if (SizeX == 32)
    {
        if(SizeY ==  8) { return g_dispatcher.h265_SAD_32x8_8u(image,  ref, stride); }
        if(SizeY == 16) { return g_dispatcher.h265_SAD_32x16_8u(image,  ref, stride);}
        if(SizeY == 24) { return g_dispatcher.h265_SAD_32x24_8u(image,  ref, stride); }
        if(SizeY == 32) { return g_dispatcher.h265_SAD_32x32_8u(image,  ref, stride);}
        else            { return g_dispatcher.h265_SAD_32x64_8u(image,  ref, stride);}
    }
    else if (SizeX == 48)
    {
        return g_dispatcher.h265_SAD_48x64_8u(image,  ref, stride);
    }
    else if (SizeX == 64)
    {
        if(SizeY == 16) { return g_dispatcher.h265_SAD_64x16_8u(image,  ref, stride);}
        if(SizeY == 32) { return g_dispatcher.h265_SAD_64x32_8u(image,  ref, stride);}
        if(SizeY == 48) { return g_dispatcher.h265_SAD_64x48_8u(image,  ref, stride);}
        else            { return g_dispatcher.h265_SAD_64x64_8u(image,  ref, stride);}
    }
    else
        return -1;
}


// [sad.general - dispatcher]
int MFX_HEVC_PP::h265_SAD_MxN_general_8u(const unsigned char *image,  int stride_img, const unsigned char *ref, int stride_ref, int SizeX, int SizeY)
{
    if (SizeX == 4)
    {
        if(SizeY == 4) { return g_dispatcher.h265_SAD_4x4_general_8u(image,  ref, stride_img, stride_ref); }
        else if(SizeY == 8) { return g_dispatcher.h265_SAD_4x8_general_8u(image,  ref, stride_img, stride_ref); }
        else           { return g_dispatcher.h265_SAD_4x16_general_8u(image,  ref, stride_img, stride_ref); }
    }
    else if (SizeX == 8)
    {
        if(SizeY ==  4) { return g_dispatcher.h265_SAD_8x4_general_8u(image,  ref, stride_img, stride_ref); }
        else if(SizeY ==  8) { return g_dispatcher.h265_SAD_8x8_general_8u(image,  ref, stride_img, stride_ref); }
        else if(SizeY == 16) { return g_dispatcher.h265_SAD_8x16_general_8u(image,  ref, stride_img, stride_ref); }
        else             { return g_dispatcher.h265_SAD_8x32_general_8u(image,  ref, stride_img, stride_ref); }
    }
    else if (SizeX == 12)
    {
        return g_dispatcher.h265_SAD_12x16_general_8u(image,  ref, stride_img, stride_ref);
    }
    else if (SizeX == 16)
    {
        if(SizeY ==  4) { return g_dispatcher.h265_SAD_16x4_general_8u(image,  ref, stride_img, stride_ref); }
        else if(SizeY ==  8) { return g_dispatcher.h265_SAD_16x8_general_8u(image,  ref, stride_img, stride_ref); }
        else if(SizeY == 12) { return g_dispatcher.h265_SAD_16x12_general_8u(image,  ref, stride_img, stride_ref); }
        else if(SizeY == 16) { return g_dispatcher.h265_SAD_16x16_general_8u(image,  ref, stride_img, stride_ref); }
        else if(SizeY == 32) { return g_dispatcher.h265_SAD_16x32_general_8u(image,  ref, stride_img, stride_ref); }
        else            { return g_dispatcher.h265_SAD_16x64_general_8u(image,  ref, stride_img, stride_ref); }
    }
    else if (SizeX == 24)
    {
        return g_dispatcher.h265_SAD_24x32_general_8u(image,  ref, stride_img, stride_ref);
    }
    else if (SizeX == 32)
    {
        if(SizeY ==  8) { return g_dispatcher.h265_SAD_32x8_general_8u(image,  ref, stride_img, stride_ref);}
        else if(SizeY == 16) { return g_dispatcher.h265_SAD_32x16_general_8u(image,  ref, stride_img, stride_ref); }
        else if(SizeY == 24) { return g_dispatcher.h265_SAD_32x24_general_8u(image,  ref, stride_img, stride_ref); }
        else if(SizeY == 32) { return g_dispatcher.h265_SAD_32x32_general_8u(image,  ref, stride_img, stride_ref);}
        else            { return g_dispatcher.h265_SAD_32x64_general_8u(image,  ref, stride_img, stride_ref);}
    }
    else if (SizeX == 48)
    {
        return g_dispatcher.h265_SAD_48x64_general_8u(image,  ref, stride_img, stride_ref);
    }
    else if (SizeX == 64)
    {
        if(SizeY == 16) { return g_dispatcher.h265_SAD_64x16_general_8u(image,  ref, stride_img, stride_ref);}
        else if(SizeY == 32) { return g_dispatcher.h265_SAD_64x32_general_8u(image,  ref, stride_img, stride_ref);}
        else if(SizeY == 48) { return g_dispatcher.h265_SAD_64x48_general_8u(image,  ref, stride_img, stride_ref);}
        else            { return g_dispatcher.h265_SAD_64x64_general_8u(image,  ref, stride_img, stride_ref); }
    }
    else
        return -1;
}
#endif


/* ************************************************* */
/*       Set Target Platform                         */
/* ************************************************* */
void SetTargetSSE4_ATOM()
{
    SetTargetSSE4();

    // for the Atom few _interpolation_ functions needs to be replaced (PSHUFB issue)
    g_dispatcher.h265_InterpLuma_s8_d16_H = &MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_atom;
    g_dispatcher.h265_InterpChroma_s8_d16_H = &MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_atom;
    g_dispatcher.h265_InterpLuma_s16_d16_H = &MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_atom;
    g_dispatcher.h265_InterpChroma_s16_d16_H = &MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_atom;

    //FastInterp
    g_dispatcher.h265_InterpLumaFast_s8_d16_H = &MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_H_atom;
    g_dispatcher.h265_InterpLumaFast_s16_d16_H = &MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_atom;


}


void SetTargetSSE4()
{
#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
    // [Sad.special]===================================
    g_dispatcher.h265_SAD_4x4_8u =  &MFX_HEVC_PP::SAD_4x4_sse;
    g_dispatcher.h265_SAD_4x8_8u =  &MFX_HEVC_PP::SAD_4x8_sse;
    g_dispatcher.h265_SAD_4x16_8u =  &MFX_HEVC_PP::SAD_4x16_sse;

    g_dispatcher.h265_SAD_8x4_8u =  &MFX_HEVC_PP::SAD_8x4_sse;
    g_dispatcher.h265_SAD_8x8_8u =  &MFX_HEVC_PP::SAD_8x8_sse;
    g_dispatcher.h265_SAD_8x16_8u =  &MFX_HEVC_PP::SAD_8x16_sse;
    g_dispatcher.h265_SAD_8x32_8u =  &MFX_HEVC_PP::SAD_8x32_sse;

    g_dispatcher.h265_SAD_12x16_8u =  &MFX_HEVC_PP::SAD_12x16_sse;

    g_dispatcher.h265_SAD_16x4_8u =  &MFX_HEVC_PP::SAD_16x4_sse;
    g_dispatcher.h265_SAD_16x8_8u =  &MFX_HEVC_PP::SAD_16x8_sse;
    g_dispatcher.h265_SAD_16x12_8u =  &MFX_HEVC_PP::SAD_16x12_sse;
    g_dispatcher.h265_SAD_16x16_8u =  &MFX_HEVC_PP::SAD_16x16_sse;
    g_dispatcher.h265_SAD_16x32_8u =  &MFX_HEVC_PP::SAD_16x32_sse;
    g_dispatcher.h265_SAD_16x64_8u =  &MFX_HEVC_PP::SAD_16x64_sse;

    g_dispatcher.h265_SAD_24x32_8u =  &MFX_HEVC_PP::SAD_24x32_sse;

    g_dispatcher.h265_SAD_32x8_8u =  &MFX_HEVC_PP::SAD_32x8_sse;
    g_dispatcher.h265_SAD_32x16_8u =  &MFX_HEVC_PP::SAD_32x16_sse;
    g_dispatcher.h265_SAD_32x24_8u =  &MFX_HEVC_PP::SAD_32x24_sse;
    g_dispatcher.h265_SAD_32x32_8u =  &MFX_HEVC_PP::SAD_32x32_sse;
    g_dispatcher.h265_SAD_32x64_8u =  &MFX_HEVC_PP::SAD_32x64_sse;

    g_dispatcher.h265_SAD_48x64_8u =  &MFX_HEVC_PP::SAD_48x64_sse;

    g_dispatcher.h265_SAD_64x16_8u =  &MFX_HEVC_PP::SAD_64x16_sse;
    g_dispatcher.h265_SAD_64x32_8u =  &MFX_HEVC_PP::SAD_64x32_sse;
    g_dispatcher.h265_SAD_64x48_8u =  &MFX_HEVC_PP::SAD_64x48_sse;
    g_dispatcher.h265_SAD_64x64_8u =  &MFX_HEVC_PP::SAD_64x64_sse;

    // [Sad.general]===================================
    g_dispatcher.h265_SAD_4x4_general_8u =  &MFX_HEVC_PP::SAD_4x4_general_sse;
    g_dispatcher.h265_SAD_4x8_general_8u =  &MFX_HEVC_PP::SAD_4x8_general_sse;
    g_dispatcher.h265_SAD_4x16_general_8u =  &MFX_HEVC_PP::SAD_4x16_general_sse;

    g_dispatcher.h265_SAD_8x4_general_8u =  &MFX_HEVC_PP::SAD_8x4_general_sse;
    g_dispatcher.h265_SAD_8x8_general_8u =  &MFX_HEVC_PP::SAD_8x8_general_sse;
    g_dispatcher.h265_SAD_8x16_general_8u =  &MFX_HEVC_PP::SAD_8x16_general_sse;
    g_dispatcher.h265_SAD_8x32_general_8u =  &MFX_HEVC_PP::SAD_8x32_general_sse;

    g_dispatcher.h265_SAD_12x16_general_8u =  &MFX_HEVC_PP::SAD_12x16_general_sse;

    g_dispatcher.h265_SAD_16x4_general_8u =  &MFX_HEVC_PP::SAD_16x4_general_sse;
    g_dispatcher.h265_SAD_16x8_general_8u =  &MFX_HEVC_PP::SAD_16x8_general_sse;
    g_dispatcher.h265_SAD_16x12_general_8u =  &MFX_HEVC_PP::SAD_16x12_general_sse;
    g_dispatcher.h265_SAD_16x16_general_8u =  &MFX_HEVC_PP::SAD_16x16_general_sse;
    g_dispatcher.h265_SAD_16x32_general_8u =  &MFX_HEVC_PP::SAD_16x32_general_sse;
    g_dispatcher.h265_SAD_16x64_general_8u =  &MFX_HEVC_PP::SAD_16x64_general_sse;

    g_dispatcher.h265_SAD_24x32_general_8u =  &MFX_HEVC_PP::SAD_24x32_general_sse;

    g_dispatcher.h265_SAD_32x8_general_8u =  &MFX_HEVC_PP::SAD_32x8_general_sse;
    g_dispatcher.h265_SAD_32x16_general_8u =  &MFX_HEVC_PP::SAD_32x16_general_sse;
    g_dispatcher.h265_SAD_32x24_general_8u =  &MFX_HEVC_PP::SAD_32x24_general_sse;
    g_dispatcher.h265_SAD_32x32_general_8u =  &MFX_HEVC_PP::SAD_32x32_general_sse;
    g_dispatcher.h265_SAD_32x64_general_8u =  &MFX_HEVC_PP::SAD_32x64_general_sse;

    g_dispatcher.h265_SAD_48x64_general_8u =  &MFX_HEVC_PP::SAD_48x64_general_sse;

    g_dispatcher.h265_SAD_64x16_general_8u =  &MFX_HEVC_PP::SAD_64x16_general_sse;
    g_dispatcher.h265_SAD_64x32_general_8u =  &MFX_HEVC_PP::SAD_64x32_general_sse;
    g_dispatcher.h265_SAD_64x48_general_8u =  &MFX_HEVC_PP::SAD_64x48_general_sse;
    g_dispatcher.h265_SAD_64x64_general_8u =  &MFX_HEVC_PP::SAD_64x64_general_sse;

    g_dispatcher.h265_SAD_MxN_x3_8u        =  &MFX_HEVC_PP::h265_SAD_MxN_x3_8u_sse;
    g_dispatcher.h265_SAD_MxN_x4_8u        =  &MFX_HEVC_PP::h265_SAD_MxN_x4_8u_sse;
    g_dispatcher.h265_SAD_MxN_general_16s  =  &MFX_HEVC_PP::h265_SAD_MxN_general_16s_sse;
    g_dispatcher.h265_SAD_MxN_16s          =  &MFX_HEVC_PP::h265_SAD_MxN_16s_sse;

    //[transform.fwd]==================================
    g_dispatcher.h265_DST4x4Fwd_16s = &MFX_HEVC_PP::h265_DST4x4Fwd_16s_sse;
    g_dispatcher.h265_DCT4x4Fwd_16s = &MFX_HEVC_PP::h265_DCT4x4Fwd_16s_sse;
    g_dispatcher.h265_DCT8x8Fwd_16s = &MFX_HEVC_PP::h265_DCT8x8Fwd_16s_sse;
    g_dispatcher.h265_DCT16x16Fwd_16s = &MFX_HEVC_PP::h265_DCT16x16Fwd_16s_sse;
    g_dispatcher.h265_DCT32x32Fwd_16s = &MFX_HEVC_PP::h265_DCT32x32Fwd_16s_sse;

    // quantization
    g_dispatcher.h265_QuantFwd_16s = &MFX_HEVC_PP::h265_QuantFwd_16s_sse;
    g_dispatcher.h265_QuantFwd_SBH_16s = &MFX_HEVC_PP::h265_QuantFwd_SBH_16s_sse;
    g_dispatcher.h265_Quant_zCost_16s = &MFX_HEVC_PP::h265_Quant_zCost_16s_sse;
    g_dispatcher.h265_QuantInv_16s = &MFX_HEVC_PP::h265_QuantInv_16s_px;

    g_dispatcher.h265_SSE_8u  = &MFX_HEVC_PP::h265_SSE_px<Ipp8u>;
    g_dispatcher.h265_SSE_16u = &MFX_HEVC_PP::h265_SSE_px<Ipp16u>;

    g_dispatcher.h265_Diff_8u  = &MFX_HEVC_PP::h265_Diff_px<Ipp8u>;
    g_dispatcher.h265_Diff_16u = &MFX_HEVC_PP::h265_Diff_px<Ipp16u>;
    g_dispatcher.h265_DiffNv12_8u  = &MFX_HEVC_PP::h265_DiffNv12_px<Ipp8u>;
    g_dispatcher.h265_DiffNv12_16u = &MFX_HEVC_PP::h265_DiffNv12_px<Ipp16u>;

    g_dispatcher.h265_SplitChromaCtb_8u  = &MFX_HEVC_PP::h265_SplitChromaCtb_px<Ipp8u>;
    g_dispatcher.h265_SplitChromaCtb_16u = &MFX_HEVC_PP::h265_SplitChromaCtb_px<Ipp16u>;

    g_dispatcher.h265_ImageDiffHistogram_8u  = &MFX_HEVC_PP::h265_ImageDiffHistogram_8u_sse;

    g_dispatcher.h265_SearchBestBlock8x8_8u  = &MFX_HEVC_PP::h265_SearchBestBlock8x8_8u_sse;

    g_dispatcher.h265_ComputeRsCsDiff  = &MFX_HEVC_PP::h265_ComputeRsCsDiff_sse;

    g_dispatcher.h265_ComputeRsCs4x4_8u  = &MFX_HEVC_PP::h265_ComputeRsCs4x4_8u_sse;
    // SATD
    g_dispatcher.h265_SATD_4x4_8u = &MFX_HEVC_PP::h265_SATD_4x4_8u_sse;
    g_dispatcher.h265_SATD_8x8_8u = &MFX_HEVC_PP::h265_SATD_8x8_8u_sse;

    g_dispatcher.h265_SATD_4x4_Pair_8u = &MFX_HEVC_PP::h265_SATD_4x4_Pair_8u_sse;
    g_dispatcher.h265_SATD_8x8_Pair_8u = &MFX_HEVC_PP::h265_SATD_8x8_Pair_8u_sse;

    g_dispatcher.h265_SATD_4x4_16u = &MFX_HEVC_PP::h265_SATD_4x4_16u_sse;
    g_dispatcher.h265_SATD_8x8_16u = &MFX_HEVC_PP::h265_SATD_8x8_16u_sse;

    g_dispatcher.h265_SATD_4x4_Pair_16u = &MFX_HEVC_PP::h265_SATD_4x4_Pair_16u_sse;
    g_dispatcher.h265_SATD_8x8_Pair_16u = &MFX_HEVC_PP::h265_SATD_8x8_Pair_16u_sse;
#endif

    //[transform.inv]==================================
    g_dispatcher.h265_DST4x4Inv_16sT = &MFX_HEVC_PP::h265_DST4x4Inv_16sT_sse;
    g_dispatcher.h265_DCT4x4Inv_16sT = &MFX_HEVC_PP::h265_DCT4x4Inv_16sT_sse;
    g_dispatcher.h265_DCT8x8Inv_16sT = &MFX_HEVC_PP::h265_DCT8x8Inv_16sT_sse;
    g_dispatcher.h265_DCT16x16Inv_16sT = &MFX_HEVC_PP::h265_DCT16x16Inv_16sT_sse;
    g_dispatcher.h265_DCT32x32Inv_16sT = &MFX_HEVC_PP::h265_DCT32x32Inv_16sT_sse;

    //[deblocking]=====================================
    g_dispatcher.h265_FilterEdgeLuma_8u_I = &MFX_HEVC_PP::h265_FilterEdgeLuma_8u_I_sse;
    g_dispatcher.h265_FilterEdgeChroma_Interleaved_8u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Interleaved_8u_I_sse;
    g_dispatcher.h265_FilterEdgeChroma_Plane_8u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Plane_8u_I_sse;

    g_dispatcher.h265_FilterEdgeLuma_16u_I = &MFX_HEVC_PP::h265_FilterEdgeLuma_16u_I_sse;
    g_dispatcher.h265_FilterEdgeChroma_Interleaved_16u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Interleaved_16u_I_sse;
    g_dispatcher.h265_FilterEdgeChroma_Plane_16u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Plane_16u_I_sse;

    //[SAO]============================================
    g_dispatcher.h265_ProcessSaoCuOrg_Luma_8u = &MFX_HEVC_PP::h265_ProcessSaoCuOrg_Luma_8u_sse;
    g_dispatcher.h265_ProcessSaoCu_Luma_8u = &MFX_HEVC_PP::h265_ProcessSaoCu_Luma_8u_sse;
    g_dispatcher.h265_GetCtuStatistics_8u = &MFX_HEVC_PP::h265_GetCtuStatistics_8u_sse;
    g_dispatcher.h265_GetCtuStatistics_16u = &MFX_HEVC_PP::h265_GetCtuStatistics_16u_px;

    g_dispatcher.h265_ProcessSaoCuOrg_Luma_16u = &MFX_HEVC_PP::h265_ProcessSaoCuOrg_Luma_16u_sse;
    g_dispatcher.h265_ProcessSaoCu_Luma_16u = &MFX_HEVC_PP::h265_ProcessSaoCu_Luma_16u_px;

    //[Interpoaltion]==================================
    // average
    g_dispatcher.h265_AverageModeB = &MFX_HEVC_PP::h265_AverageModeB_sse;
    g_dispatcher.h265_AverageModeP = &MFX_HEVC_PP::h265_AverageModeP_sse;
    g_dispatcher.h265_AverageModeN = &MFX_HEVC_PP::h265_AverageModeN_sse;
        
    g_dispatcher.h265_AverageModeB_U16 = &MFX_HEVC_PP::h265_AverageModeB_U16_sse;
    g_dispatcher.h265_AverageModeP_U16 = &MFX_HEVC_PP::h265_AverageModeP_U16_sse;
    g_dispatcher.h265_AverageModeN_U16 = &MFX_HEVC_PP::h265_AverageModeN_U16_sse;

    // algo
    g_dispatcher.h265_InterpLuma_s8_d16_H = &MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_sse;
    g_dispatcher.h265_InterpChroma_s8_d16_H = &MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_sse;
    g_dispatcher.h265_InterpLuma_s8_d16_V = &MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_sse;
    g_dispatcher.h265_InterpChroma_s8_d16_V = &MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_sse;
    g_dispatcher.h265_InterpLuma_s16_d16_V = &MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_sse;
    g_dispatcher.h265_InterpChroma_s16_d16_V = &MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_sse;
    g_dispatcher.h265_InterpLuma_s16_d16_H = &MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_sse;
    g_dispatcher.h265_InterpChroma_s16_d16_H = &MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_sse;

    // FastInterp //kolya
    g_dispatcher.h265_InterpLumaFast_s8_d16_H =  &MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_H_sse;
    g_dispatcher.h265_InterpLumaFast_s8_d16_V =  &MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_V_sse;
    g_dispatcher.h265_InterpLumaFast_s16_d16_V = &MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_sse;
    g_dispatcher.h265_InterpLumaFast_s16_d16_H = &MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_sse;

    g_dispatcher.h265_InterpLumaPack_d8 = &MFX_HEVC_PP::h265_InterpLumaPack_sse<Ipp8u>;
    g_dispatcher.h265_InterpLumaPack_d16 = &MFX_HEVC_PP::h265_InterpLumaPack_sse<Ipp16u>;

    // Convert w/ rshift
    g_dispatcher.h265_ConvertShiftR = &MFX_HEVC_PP::h265_ConvertShiftR_sse;

    // [INTRA prediction]
    g_dispatcher.h265_PredictIntra_Ang_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_8u_sse;
    g_dispatcher.h265_PredictIntra_Ang_All_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_sse;
    g_dispatcher.h265_PredictIntra_Ang_NoTranspose_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_sse;
    g_dispatcher.h265_PredictIntra_Ang_All_Even_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_sse;

    g_dispatcher.h265_PredictIntra_Ang_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_16u_sse;
    g_dispatcher.h265_PredictIntra_Ang_NoTranspose_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_sse;
    g_dispatcher.h265_PredictIntra_Ang_All_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_sse;
    g_dispatcher.h265_PredictIntra_Ang_All_Even_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_sse;

    g_dispatcher.h265_FilterPredictPels_8u = &MFX_HEVC_PP::h265_FilterPredictPels_8u_sse;
    g_dispatcher.h265_FilterPredictPels_Bilinear_8u = &MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_8u_sse;
    g_dispatcher.h265_PredictIntra_Planar_8u = &MFX_HEVC_PP::h265_PredictIntra_Planar_8u_sse;
    g_dispatcher.h265_PredictIntra_Planar_ChromaNV12_8u = h265_PredictIntra_Planar_ChromaNV12_8u_px;

    g_dispatcher.h265_FilterPredictPels_16s = &MFX_HEVC_PP::h265_FilterPredictPels_16s_sse;
    g_dispatcher.h265_FilterPredictPels_Bilinear_16s = &MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_16s_sse;
    g_dispatcher.h265_PredictIntra_Planar_16s = &MFX_HEVC_PP::h265_PredictIntra_Planar_16s_sse;

    g_dispatcher.h265_AnalyzeGradient_8u = &MFX_HEVC_PP::h265_AnalyzeGradient_sse<Ipp8u, Ipp16u>;
    g_dispatcher.h265_AnalyzeGradient_16u = &MFX_HEVC_PP::h265_AnalyzeGradient_sse<Ipp16u, Ipp32u>;

    g_dispatcher.h265_ComputeRsCs_8u = &MFX_HEVC_PP::h265_ComputeRsCs_8u_sse;
    g_dispatcher.h265_ComputeRsCs_16u = &MFX_HEVC_PP::h265_ComputeRsCs_16u_sse;
    g_dispatcher.h265_AddClipNv12UV_8u = &MFX_HEVC_PP::h265_AddClipNv12UV_8u_sse;
    g_dispatcher.h265_DiffDc_8u  = &MFX_HEVC_PP::h265_DiffDc_px<Ipp8u>;
    g_dispatcher.h265_DiffDc_16u = &MFX_HEVC_PP::h265_DiffDc_px<Ipp16u>;

    // [WeightedPred]
    g_dispatcher.h265_CopyWeighted_S16U8 = &MFX_HEVC_PP::h265_CopyWeighted_S16U8_sse;
    g_dispatcher.h265_CopyWeightedBidi_S16U8 = &MFX_HEVC_PP::h265_CopyWeightedBidi_S16U8_sse;
    g_dispatcher.h265_CopyWeighted_S16U16 = &MFX_HEVC_PP::h265_CopyWeighted_S16U16_sse;
    g_dispatcher.h265_CopyWeightedBidi_S16U16 = &MFX_HEVC_PP::h265_CopyWeightedBidi_S16U16_sse;
}


void SetTargetSSSE3()
{
#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
    // [Sad.special]===================================
    g_dispatcher.h265_SAD_4x4_8u =  &MFX_HEVC_PP::SAD_4x4_ssse3;
    g_dispatcher.h265_SAD_4x8_8u =  &MFX_HEVC_PP::SAD_4x8_ssse3;
    g_dispatcher.h265_SAD_4x16_8u =  &MFX_HEVC_PP::SAD_4x16_ssse3;

    g_dispatcher.h265_SAD_8x4_8u =  &MFX_HEVC_PP::SAD_8x4_ssse3;
    g_dispatcher.h265_SAD_8x8_8u =  &MFX_HEVC_PP::SAD_8x8_ssse3;
    g_dispatcher.h265_SAD_8x16_8u =  &MFX_HEVC_PP::SAD_8x16_ssse3;
    g_dispatcher.h265_SAD_8x32_8u =  &MFX_HEVC_PP::SAD_8x32_ssse3;

    g_dispatcher.h265_SAD_12x16_8u =  &MFX_HEVC_PP::SAD_12x16_ssse3;

    g_dispatcher.h265_SAD_16x4_8u =  &MFX_HEVC_PP::SAD_16x4_ssse3;
    g_dispatcher.h265_SAD_16x8_8u =  &MFX_HEVC_PP::SAD_16x8_ssse3;
    g_dispatcher.h265_SAD_16x12_8u =  &MFX_HEVC_PP::SAD_16x12_ssse3;
    g_dispatcher.h265_SAD_16x16_8u =  &MFX_HEVC_PP::SAD_16x16_ssse3;
    g_dispatcher.h265_SAD_16x32_8u =  &MFX_HEVC_PP::SAD_16x32_ssse3;
    g_dispatcher.h265_SAD_16x64_8u =  &MFX_HEVC_PP::SAD_16x64_ssse3;

    g_dispatcher.h265_SAD_24x32_8u =  &MFX_HEVC_PP::SAD_24x32_ssse3;

    g_dispatcher.h265_SAD_32x8_8u =  &MFX_HEVC_PP::SAD_32x8_ssse3;
    g_dispatcher.h265_SAD_32x16_8u =  &MFX_HEVC_PP::SAD_32x16_ssse3;
    g_dispatcher.h265_SAD_32x24_8u =  &MFX_HEVC_PP::SAD_32x24_ssse3;
    g_dispatcher.h265_SAD_32x32_8u =  &MFX_HEVC_PP::SAD_32x32_ssse3;
    g_dispatcher.h265_SAD_32x64_8u =  &MFX_HEVC_PP::SAD_32x64_ssse3;

    g_dispatcher.h265_SAD_48x64_8u =  &MFX_HEVC_PP::SAD_48x64_ssse3;

    g_dispatcher.h265_SAD_64x16_8u =  &MFX_HEVC_PP::SAD_64x16_ssse3;
    g_dispatcher.h265_SAD_64x32_8u =  &MFX_HEVC_PP::SAD_64x32_ssse3;
    g_dispatcher.h265_SAD_64x48_8u =  &MFX_HEVC_PP::SAD_64x48_ssse3;
    g_dispatcher.h265_SAD_64x64_8u =  &MFX_HEVC_PP::SAD_64x64_ssse3;

    // [Sad.general]===================================
    g_dispatcher.h265_SAD_4x4_general_8u =  &MFX_HEVC_PP::SAD_4x4_general_ssse3;
    g_dispatcher.h265_SAD_4x8_general_8u =  &MFX_HEVC_PP::SAD_4x8_general_ssse3;
    g_dispatcher.h265_SAD_4x16_general_8u =  &MFX_HEVC_PP::SAD_4x16_general_sse;

    g_dispatcher.h265_SAD_8x4_general_8u =  &MFX_HEVC_PP::SAD_8x4_general_ssse3;
    g_dispatcher.h265_SAD_8x8_general_8u =  &MFX_HEVC_PP::SAD_8x8_general_ssse3;
    g_dispatcher.h265_SAD_8x16_general_8u =  &MFX_HEVC_PP::SAD_8x16_general_ssse3;
    g_dispatcher.h265_SAD_8x32_general_8u =  &MFX_HEVC_PP::SAD_8x32_general_ssse3;

    g_dispatcher.h265_SAD_12x16_general_8u =  &MFX_HEVC_PP::SAD_12x16_general_ssse3;

    g_dispatcher.h265_SAD_16x4_general_8u =  &MFX_HEVC_PP::SAD_16x4_general_ssse3;
    g_dispatcher.h265_SAD_16x8_general_8u =  &MFX_HEVC_PP::SAD_16x8_general_ssse3;
    g_dispatcher.h265_SAD_16x12_general_8u =  &MFX_HEVC_PP::SAD_16x12_general_ssse3;
    g_dispatcher.h265_SAD_16x16_general_8u =  &MFX_HEVC_PP::SAD_16x16_general_ssse3;
    g_dispatcher.h265_SAD_16x32_general_8u =  &MFX_HEVC_PP::SAD_16x32_general_ssse3;
    g_dispatcher.h265_SAD_16x64_general_8u =  &MFX_HEVC_PP::SAD_16x64_general_ssse3;

    g_dispatcher.h265_SAD_24x32_general_8u =  &MFX_HEVC_PP::SAD_24x32_general_ssse3;

    g_dispatcher.h265_SAD_32x8_general_8u =  &MFX_HEVC_PP::SAD_32x8_general_ssse3;
    g_dispatcher.h265_SAD_32x16_general_8u =  &MFX_HEVC_PP::SAD_32x16_general_ssse3;
    g_dispatcher.h265_SAD_32x24_general_8u =  &MFX_HEVC_PP::SAD_32x24_general_ssse3;
    g_dispatcher.h265_SAD_32x32_general_8u =  &MFX_HEVC_PP::SAD_32x32_general_ssse3;
    g_dispatcher.h265_SAD_32x64_general_8u =  &MFX_HEVC_PP::SAD_32x64_general_ssse3;

    g_dispatcher.h265_SAD_48x64_general_8u =  &MFX_HEVC_PP::SAD_48x64_general_ssse3;

    g_dispatcher.h265_SAD_64x16_general_8u =  &MFX_HEVC_PP::SAD_64x16_general_ssse3;
    g_dispatcher.h265_SAD_64x32_general_8u =  &MFX_HEVC_PP::SAD_64x32_general_ssse3;
    g_dispatcher.h265_SAD_64x48_general_8u =  &MFX_HEVC_PP::SAD_64x48_general_ssse3;
    g_dispatcher.h265_SAD_64x64_general_8u =  &MFX_HEVC_PP::SAD_64x64_general_ssse3;

    g_dispatcher.h265_SAD_MxN_x3_8u        =  &MFX_HEVC_PP::h265_SAD_MxN_x3_8u_px;
    g_dispatcher.h265_SAD_MxN_x4_8u        =  &MFX_HEVC_PP::h265_SAD_MxN_x4_8u_px;
    g_dispatcher.h265_SAD_MxN_general_16s  =  &MFX_HEVC_PP::h265_SAD_MxN_general_16s_px;
    g_dispatcher.h265_SAD_MxN_16s          =  &MFX_HEVC_PP::h265_SAD_MxN_16s_px;

    //[transform.fwd]==================================
    g_dispatcher.h265_DST4x4Fwd_16s = &MFX_HEVC_PP::h265_DST4x4Fwd_16s_ssse3;
    g_dispatcher.h265_DCT4x4Fwd_16s = &MFX_HEVC_PP::h265_DCT4x4Fwd_16s_ssse3;
    g_dispatcher.h265_DCT8x8Fwd_16s = &MFX_HEVC_PP::h265_DCT8x8Fwd_16s_ssse3;
    g_dispatcher.h265_DCT16x16Fwd_16s = &MFX_HEVC_PP::h265_DCT16x16Fwd_16s_ssse3;
    g_dispatcher.h265_DCT32x32Fwd_16s = &MFX_HEVC_PP::h265_DCT32x32Fwd_16s_ssse3;

    // quantization
    g_dispatcher.h265_QuantFwd_16s = &MFX_HEVC_PP::h265_QuantFwd_16s_px;
    g_dispatcher.h265_QuantFwd_SBH_16s = &MFX_HEVC_PP::h265_QuantFwd_SBH_16s_px;
    g_dispatcher.h265_Quant_zCost_16s = &MFX_HEVC_PP::h265_Quant_zCost_16s_px;
    g_dispatcher.h265_QuantInv_16s = &MFX_HEVC_PP::h265_QuantInv_16s_px;

    g_dispatcher.h265_SSE_8u  = &MFX_HEVC_PP::h265_SSE_px<Ipp8u>;
    g_dispatcher.h265_SSE_16u = &MFX_HEVC_PP::h265_SSE_px<Ipp16u>;

    g_dispatcher.h265_Diff_8u  = &MFX_HEVC_PP::h265_Diff_px<Ipp8u>;
    g_dispatcher.h265_Diff_16u = &MFX_HEVC_PP::h265_Diff_px<Ipp16u>;
    g_dispatcher.h265_DiffNv12_8u  = &MFX_HEVC_PP::h265_DiffNv12_px<Ipp8u>;
    g_dispatcher.h265_DiffNv12_16u = &MFX_HEVC_PP::h265_DiffNv12_px<Ipp16u>;

    g_dispatcher.h265_SplitChromaCtb_8u  = &MFX_HEVC_PP::h265_SplitChromaCtb_px<Ipp8u>;
    g_dispatcher.h265_SplitChromaCtb_16u = &MFX_HEVC_PP::h265_SplitChromaCtb_px<Ipp16u>;

    g_dispatcher.h265_ImageDiffHistogram_8u  = &MFX_HEVC_PP::h265_ImageDiffHistogram_8u_px;

    g_dispatcher.h265_SearchBestBlock8x8_8u  = &MFX_HEVC_PP::h265_SearchBestBlock8x8_8u_px;

    g_dispatcher.h265_ComputeRsCsDiff  = &MFX_HEVC_PP::h265_ComputeRsCsDiff_px;

    g_dispatcher.h265_ComputeRsCs4x4_8u  = &MFX_HEVC_PP::h265_ComputeRsCs4x4_8u_px;
    // SATD
    g_dispatcher.h265_SATD_4x4_8u = &MFX_HEVC_PP::h265_SATD_4x4_8u_px;
    g_dispatcher.h265_SATD_8x8_8u = &MFX_HEVC_PP::h265_SATD_8x8_8u_px;

    g_dispatcher.h265_SATD_4x4_Pair_8u = &MFX_HEVC_PP::h265_SATD_4x4_Pair_8u_px;
    g_dispatcher.h265_SATD_8x8_Pair_8u = &MFX_HEVC_PP::h265_SATD_8x8_Pair_8u_px;

    g_dispatcher.h265_SATD_4x4_16u = &MFX_HEVC_PP::h265_SATD_4x4_16u_px;
    g_dispatcher.h265_SATD_8x8_16u = &MFX_HEVC_PP::h265_SATD_8x8_16u_px;

    g_dispatcher.h265_SATD_4x4_Pair_16u = &MFX_HEVC_PP::h265_SATD_4x4_Pair_16u_px;
    g_dispatcher.h265_SATD_8x8_Pair_16u = &MFX_HEVC_PP::h265_SATD_8x8_Pair_16u_px;
#endif

    //[transform.inv]==================================
    g_dispatcher.h265_DST4x4Inv_16sT = &MFX_HEVC_PP::h265_DST4x4Inv_16sT_ssse3;
    g_dispatcher.h265_DCT4x4Inv_16sT = &MFX_HEVC_PP::h265_DCT4x4Inv_16sT_ssse3;
    g_dispatcher.h265_DCT8x8Inv_16sT = &MFX_HEVC_PP::h265_DCT8x8Inv_16sT_ssse3;
    g_dispatcher.h265_DCT16x16Inv_16sT = &MFX_HEVC_PP::h265_DCT16x16Inv_16sT_ssse3;
    g_dispatcher.h265_DCT32x32Inv_16sT = &MFX_HEVC_PP::h265_DCT32x32Inv_16sT_ssse3;

    //[deblocking]=====================================
    g_dispatcher.h265_FilterEdgeLuma_8u_I = &MFX_HEVC_PP::h265_FilterEdgeLuma_8u_I_ssse3;
    g_dispatcher.h265_FilterEdgeChroma_Interleaved_8u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Interleaved_8u_I_ssse3;
    g_dispatcher.h265_FilterEdgeChroma_Plane_8u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Plane_8u_I_ssse3;

    g_dispatcher.h265_FilterEdgeLuma_16u_I = &MFX_HEVC_PP::h265_FilterEdgeLuma_16u_I_ssse3;
    g_dispatcher.h265_FilterEdgeChroma_Interleaved_16u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Interleaved_16u_I_ssse3;
    g_dispatcher.h265_FilterEdgeChroma_Plane_16u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Plane_16u_I_ssse3;

    //[SAO]============================================
    g_dispatcher.h265_ProcessSaoCuOrg_Luma_8u = &MFX_HEVC_PP::h265_ProcessSaoCuOrg_Luma_8u_ssse3;
    g_dispatcher.h265_ProcessSaoCu_Luma_8u = &MFX_HEVC_PP::h265_ProcessSaoCu_Luma_8u_ssse3;
    g_dispatcher.h265_GetCtuStatistics_8u = &MFX_HEVC_PP::h265_GetCtuStatistics_8u_px;
    g_dispatcher.h265_GetCtuStatistics_16u = &MFX_HEVC_PP::h265_GetCtuStatistics_16u_px;

    g_dispatcher.h265_ProcessSaoCuOrg_Luma_16u = &MFX_HEVC_PP::h265_ProcessSaoCuOrg_Luma_16u_ssse3;
    g_dispatcher.h265_ProcessSaoCu_Luma_16u = &MFX_HEVC_PP::h265_ProcessSaoCu_Luma_16u_px;

    //[Interpoaltion]==================================
    // average
    g_dispatcher.h265_AverageModeB = &MFX_HEVC_PP::h265_AverageModeB_ssse3;
    g_dispatcher.h265_AverageModeP = &MFX_HEVC_PP::h265_AverageModeP_ssse3;
    g_dispatcher.h265_AverageModeN = &MFX_HEVC_PP::h265_AverageModeN_ssse3;        

    g_dispatcher.h265_AverageModeB_U16 = &MFX_HEVC_PP::h265_AverageModeB_U16_ssse3;
    g_dispatcher.h265_AverageModeP_U16 = &MFX_HEVC_PP::h265_AverageModeP_U16_ssse3;
    g_dispatcher.h265_AverageModeN_U16 = &MFX_HEVC_PP::h265_AverageModeN_U16_ssse3;

    // algo
    g_dispatcher.h265_InterpLuma_s8_d16_H = &MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_ssse3;
    g_dispatcher.h265_InterpChroma_s8_d16_H = &MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_ssse3;
    g_dispatcher.h265_InterpLuma_s8_d16_V = &MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_ssse3;
    g_dispatcher.h265_InterpChroma_s8_d16_V = &MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_ssse3;
    g_dispatcher.h265_InterpLuma_s16_d16_V = &MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_ssse3;
    g_dispatcher.h265_InterpChroma_s16_d16_V = &MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_ssse3;
    g_dispatcher.h265_InterpLuma_s16_d16_H = &MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_ssse3;
    g_dispatcher.h265_InterpChroma_s16_d16_H = &MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_ssse3;

    // FastInterp //kolya
    g_dispatcher.h265_InterpLumaFast_s8_d16_H  = &MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_H_ssse3;
    g_dispatcher.h265_InterpLumaFast_s8_d16_V  = &MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_V_ssse3;
    g_dispatcher.h265_InterpLumaFast_s16_d16_V = &MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_ssse3;
    g_dispatcher.h265_InterpLumaFast_s16_d16_H = &MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_ssse3;

    g_dispatcher.h265_InterpLumaPack_d8 = &MFX_HEVC_PP::h265_InterpLumaPack_sse<Ipp8u>;
    g_dispatcher.h265_InterpLumaPack_d16 = &MFX_HEVC_PP::h265_InterpLumaPack_sse<Ipp16u>;

    // Convert w/ rshift
    g_dispatcher.h265_ConvertShiftR = &MFX_HEVC_PP::h265_ConvertShiftR_sse;

    // [INTRA prediction]
    g_dispatcher.h265_PredictIntra_Ang_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_8u_ssse3;
    g_dispatcher.h265_PredictIntra_Ang_All_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_ssse3;
    g_dispatcher.h265_PredictIntra_Ang_NoTranspose_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_ssse3;
    g_dispatcher.h265_PredictIntra_Ang_All_Even_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_ssse3;

    g_dispatcher.h265_PredictIntra_Ang_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_16u_ssse3;
    g_dispatcher.h265_PredictIntra_Ang_NoTranspose_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_ssse3;
    g_dispatcher.h265_PredictIntra_Ang_All_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_px;
    g_dispatcher.h265_PredictIntra_Ang_All_Even_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_px;

    g_dispatcher.h265_FilterPredictPels_8u = &MFX_HEVC_PP::h265_FilterPredictPels_8u_px;
    g_dispatcher.h265_FilterPredictPels_Bilinear_8u = &MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_8u_px;
    g_dispatcher.h265_PredictIntra_Planar_8u = &MFX_HEVC_PP::h265_PredictIntra_Planar_8u_px;
    g_dispatcher.h265_PredictIntra_Planar_ChromaNV12_8u = h265_PredictIntra_Planar_ChromaNV12_8u_px;

    g_dispatcher.h265_FilterPredictPels_16s = &MFX_HEVC_PP::h265_FilterPredictPels_16s_px;
    g_dispatcher.h265_FilterPredictPels_Bilinear_16s = &MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_16s_px;
    g_dispatcher.h265_PredictIntra_Planar_16s = &MFX_HEVC_PP::h265_PredictIntra_Planar_16s_px;

    g_dispatcher.h265_AnalyzeGradient_8u = &MFX_HEVC_PP::h265_AnalyzeGradient_px<Ipp8u, Ipp16u>;
    g_dispatcher.h265_AnalyzeGradient_16u = &MFX_HEVC_PP::h265_AnalyzeGradient_px<Ipp16u, Ipp32u>;

    g_dispatcher.h265_ComputeRsCs_8u = &MFX_HEVC_PP::h265_ComputeRsCs_px<Ipp8u>;
    g_dispatcher.h265_ComputeRsCs_16u = &MFX_HEVC_PP::h265_ComputeRsCs_px<Ipp16u>;

    // [WeightedPred]
    g_dispatcher.h265_CopyWeighted_S16U8 = &MFX_HEVC_PP::h265_CopyWeighted_S16U8_ssse3;
    g_dispatcher.h265_CopyWeightedBidi_S16U8 = &MFX_HEVC_PP::h265_CopyWeightedBidi_S16U8_ssse3;
    g_dispatcher.h265_CopyWeighted_S16U16 = &MFX_HEVC_PP::h265_CopyWeighted_S16U16_ssse3;
    g_dispatcher.h265_CopyWeightedBidi_S16U16 = &MFX_HEVC_PP::h265_CopyWeightedBidi_S16U16_ssse3;

}


void SetTargetAVX2()
{
#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
    // [Sad.special]===================================
    g_dispatcher.h265_SAD_4x4_8u =  &MFX_HEVC_PP::SAD_4x4_avx2;
    g_dispatcher.h265_SAD_4x8_8u =  &MFX_HEVC_PP::SAD_4x8_avx2;
    g_dispatcher.h265_SAD_4x16_8u =  &MFX_HEVC_PP::SAD_4x16_avx2;

    g_dispatcher.h265_SAD_8x4_8u =  &MFX_HEVC_PP::SAD_8x4_avx2;
    g_dispatcher.h265_SAD_8x8_8u =  &MFX_HEVC_PP::SAD_8x8_avx2;
    g_dispatcher.h265_SAD_8x16_8u =  &MFX_HEVC_PP::SAD_8x16_avx2;
    g_dispatcher.h265_SAD_8x32_8u =  &MFX_HEVC_PP::SAD_8x32_avx2;

    g_dispatcher.h265_SAD_12x16_8u =  &MFX_HEVC_PP::SAD_12x16_avx2;

    g_dispatcher.h265_SAD_16x4_8u =  &MFX_HEVC_PP::SAD_16x4_avx2;
    g_dispatcher.h265_SAD_16x8_8u =  &MFX_HEVC_PP::SAD_16x8_avx2;
    g_dispatcher.h265_SAD_16x12_8u =  &MFX_HEVC_PP::SAD_16x12_avx2;
    g_dispatcher.h265_SAD_16x16_8u =  &MFX_HEVC_PP::SAD_16x16_avx2;
    g_dispatcher.h265_SAD_16x32_8u =  &MFX_HEVC_PP::SAD_16x32_avx2;
    g_dispatcher.h265_SAD_16x64_8u =  &MFX_HEVC_PP::SAD_16x64_avx2;

    g_dispatcher.h265_SAD_24x32_8u =  &MFX_HEVC_PP::SAD_24x32_avx2;

    g_dispatcher.h265_SAD_32x8_8u =  &MFX_HEVC_PP::SAD_32x8_avx2;
    g_dispatcher.h265_SAD_32x16_8u =  &MFX_HEVC_PP::SAD_32x16_avx2;
    g_dispatcher.h265_SAD_32x24_8u =  &MFX_HEVC_PP::SAD_32x24_avx2;
    g_dispatcher.h265_SAD_32x32_8u =  &MFX_HEVC_PP::SAD_32x32_avx2;
    g_dispatcher.h265_SAD_32x64_8u =  &MFX_HEVC_PP::SAD_32x64_avx2;

    g_dispatcher.h265_SAD_48x64_8u =  &MFX_HEVC_PP::SAD_48x64_avx2;

    g_dispatcher.h265_SAD_64x16_8u =  &MFX_HEVC_PP::SAD_64x16_avx2;
    g_dispatcher.h265_SAD_64x32_8u =  &MFX_HEVC_PP::SAD_64x32_avx2;
    g_dispatcher.h265_SAD_64x48_8u =  &MFX_HEVC_PP::SAD_64x48_avx2;
    g_dispatcher.h265_SAD_64x64_8u =  &MFX_HEVC_PP::SAD_64x64_avx2;

    // [Sad.general]===================================
    g_dispatcher.h265_SAD_4x4_general_8u =  &MFX_HEVC_PP::SAD_4x4_general_avx2;
    g_dispatcher.h265_SAD_4x8_general_8u =  &MFX_HEVC_PP::SAD_4x8_general_avx2;
    g_dispatcher.h265_SAD_4x16_general_8u =  &MFX_HEVC_PP::SAD_4x16_general_avx2;

    g_dispatcher.h265_SAD_8x4_general_8u =  &MFX_HEVC_PP::SAD_8x4_general_avx2;
    g_dispatcher.h265_SAD_8x8_general_8u =  &MFX_HEVC_PP::SAD_8x8_general_avx2;
    g_dispatcher.h265_SAD_8x16_general_8u =  &MFX_HEVC_PP::SAD_8x16_general_avx2;
    g_dispatcher.h265_SAD_8x32_general_8u =  &MFX_HEVC_PP::SAD_8x32_general_avx2;

    g_dispatcher.h265_SAD_12x16_general_8u =  &MFX_HEVC_PP::SAD_12x16_general_avx2;

    g_dispatcher.h265_SAD_16x4_general_8u =  &MFX_HEVC_PP::SAD_16x4_general_avx2;
    g_dispatcher.h265_SAD_16x8_general_8u =  &MFX_HEVC_PP::SAD_16x8_general_avx2;
    g_dispatcher.h265_SAD_16x12_general_8u =  &MFX_HEVC_PP::SAD_16x12_general_avx2;
    g_dispatcher.h265_SAD_16x16_general_8u =  &MFX_HEVC_PP::SAD_16x16_general_avx2;
    g_dispatcher.h265_SAD_16x32_general_8u =  &MFX_HEVC_PP::SAD_16x32_general_avx2;
    g_dispatcher.h265_SAD_16x64_general_8u =  &MFX_HEVC_PP::SAD_16x64_general_avx2;

    g_dispatcher.h265_SAD_24x32_general_8u =  &MFX_HEVC_PP::SAD_24x32_general_avx2;

    g_dispatcher.h265_SAD_32x8_general_8u =  &MFX_HEVC_PP::SAD_32x8_general_avx2;
    g_dispatcher.h265_SAD_32x16_general_8u =  &MFX_HEVC_PP::SAD_32x16_general_avx2;
    g_dispatcher.h265_SAD_32x24_general_8u =  &MFX_HEVC_PP::SAD_32x24_general_avx2;
    g_dispatcher.h265_SAD_32x32_general_8u =  &MFX_HEVC_PP::SAD_32x32_general_avx2;
    g_dispatcher.h265_SAD_32x64_general_8u =  &MFX_HEVC_PP::SAD_32x64_general_avx2;

    g_dispatcher.h265_SAD_48x64_general_8u =  &MFX_HEVC_PP::SAD_48x64_general_avx2;

    g_dispatcher.h265_SAD_64x16_general_8u =  &MFX_HEVC_PP::SAD_64x16_general_avx2;
    g_dispatcher.h265_SAD_64x32_general_8u =  &MFX_HEVC_PP::SAD_64x32_general_avx2;
    g_dispatcher.h265_SAD_64x48_general_8u =  &MFX_HEVC_PP::SAD_64x48_general_avx2;
    g_dispatcher.h265_SAD_64x64_general_8u =  &MFX_HEVC_PP::SAD_64x64_general_avx2;

    g_dispatcher.h265_SAD_MxN_x3_8u        =  &MFX_HEVC_PP::h265_SAD_MxN_x3_8u_avx2;
    g_dispatcher.h265_SAD_MxN_x4_8u        =  &MFX_HEVC_PP::h265_SAD_MxN_x4_8u_avx2;
    g_dispatcher.h265_SAD_MxN_general_16s  =  &MFX_HEVC_PP::h265_SAD_MxN_general_16s_avx2;
    g_dispatcher.h265_SAD_MxN_16s          =  &MFX_HEVC_PP::h265_SAD_MxN_16s_avx2;

    //[transform.fwd]==================================
    g_dispatcher.h265_DST4x4Fwd_16s = &MFX_HEVC_PP::h265_DST4x4Fwd_16s_avx2;
    g_dispatcher.h265_DCT4x4Fwd_16s = &MFX_HEVC_PP::h265_DCT4x4Fwd_16s_avx2;
    g_dispatcher.h265_DCT8x8Fwd_16s = &MFX_HEVC_PP::h265_DCT8x8Fwd_16s_avx2;
    g_dispatcher.h265_DCT16x16Fwd_16s = &MFX_HEVC_PP::h265_DCT16x16Fwd_16s_avx2;
    g_dispatcher.h265_DCT32x32Fwd_16s = &MFX_HEVC_PP::h265_DCT32x32Fwd_16s_avx2;

    // quantization
    g_dispatcher.h265_QuantFwd_16s = &MFX_HEVC_PP::h265_QuantFwd_16s_avx2;
    g_dispatcher.h265_QuantFwd_SBH_16s = &MFX_HEVC_PP::h265_QuantFwd_SBH_16s_avx2;
    g_dispatcher.h265_Quant_zCost_16s = &MFX_HEVC_PP::h265_Quant_zCost_16s_avx2;
    g_dispatcher.h265_QuantInv_16s = &MFX_HEVC_PP::h265_QuantInv_16s_avx2;

    g_dispatcher.h265_SSE_8u  = &MFX_HEVC_PP::h265_SSE_avx2<Ipp8u>;
    g_dispatcher.h265_SSE_16u = &MFX_HEVC_PP::h265_SSE_avx2<Ipp16u>;

    g_dispatcher.h265_Diff_8u  = &MFX_HEVC_PP::h265_Diff_avx2<Ipp8u>;
    g_dispatcher.h265_Diff_16u = &MFX_HEVC_PP::h265_Diff_avx2<Ipp16u>;
    g_dispatcher.h265_DiffNv12_8u  = &MFX_HEVC_PP::h265_DiffNv12_avx2<Ipp8u>;
    g_dispatcher.h265_DiffNv12_16u = &MFX_HEVC_PP::h265_DiffNv12_avx2<Ipp16u>;

    g_dispatcher.h265_SplitChromaCtb_8u  = &MFX_HEVC_PP::h265_SplitChromaCtb_avx2<Ipp8u>;
    g_dispatcher.h265_SplitChromaCtb_16u = &MFX_HEVC_PP::h265_SplitChromaCtb_avx2<Ipp16u>;

    g_dispatcher.h265_ImageDiffHistogram_8u  = &MFX_HEVC_PP::h265_ImageDiffHistogram_8u_avx2;

    g_dispatcher.h265_SearchBestBlock8x8_8u  = &MFX_HEVC_PP::h265_SearchBestBlock8x8_8u_avx2;

    g_dispatcher.h265_ComputeRsCsDiff  = &MFX_HEVC_PP::h265_ComputeRsCsDiff_avx2;

    g_dispatcher.h265_ComputeRsCs4x4_8u  = &MFX_HEVC_PP::h265_ComputeRsCs4x4_8u_avx2;
    // SATD
    g_dispatcher.h265_SATD_4x4_8u = &MFX_HEVC_PP::h265_SATD_4x4_8u_avx2;
    g_dispatcher.h265_SATD_8x8_8u = &MFX_HEVC_PP::h265_SATD_8x8_8u_avx2;

    g_dispatcher.h265_SATD_4x4_Pair_8u = &MFX_HEVC_PP::h265_SATD_4x4_Pair_8u_avx2;
    g_dispatcher.h265_SATD_8x8_Pair_8u = &MFX_HEVC_PP::h265_SATD_8x8_Pair_8u_avx2;

    g_dispatcher.h265_SATD_4x4_16u = &MFX_HEVC_PP::h265_SATD_4x4_16u_avx2;
    g_dispatcher.h265_SATD_8x8_16u = &MFX_HEVC_PP::h265_SATD_8x8_16u_avx2;

    g_dispatcher.h265_SATD_4x4_Pair_16u = &MFX_HEVC_PP::h265_SATD_4x4_Pair_16u_avx2;
    g_dispatcher.h265_SATD_8x8_Pair_16u = &MFX_HEVC_PP::h265_SATD_8x8_Pair_16u_avx2;
#endif

    //[deblocking]=====================================
    g_dispatcher.h265_FilterEdgeLuma_8u_I = &MFX_HEVC_PP::h265_FilterEdgeLuma_8u_I_sse;
    g_dispatcher.h265_FilterEdgeChroma_Interleaved_8u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Interleaved_8u_I_sse;
    g_dispatcher.h265_FilterEdgeChroma_Plane_8u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Plane_8u_I_sse;

    g_dispatcher.h265_FilterEdgeLuma_16u_I = &MFX_HEVC_PP::h265_FilterEdgeLuma_16u_I_sse;
    g_dispatcher.h265_FilterEdgeChroma_Interleaved_16u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Interleaved_16u_I_sse;
    g_dispatcher.h265_FilterEdgeChroma_Plane_16u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Plane_16u_I_sse;

    //[SAO]============================================
    g_dispatcher.h265_ProcessSaoCuOrg_Luma_8u = &MFX_HEVC_PP::h265_ProcessSaoCuOrg_Luma_8u_sse;
    g_dispatcher.h265_ProcessSaoCu_Luma_8u = &MFX_HEVC_PP::h265_ProcessSaoCu_Luma_8u_sse;
    g_dispatcher.h265_GetCtuStatistics_8u = &MFX_HEVC_PP::h265_GetCtuStatistics_8u_avx2;
    g_dispatcher.h265_GetCtuStatistics_16u = &MFX_HEVC_PP::h265_GetCtuStatistics_16u_avx2;

    g_dispatcher.h265_ProcessSaoCuOrg_Luma_16u = &MFX_HEVC_PP::h265_ProcessSaoCuOrg_Luma_16u_sse;
    g_dispatcher.h265_ProcessSaoCu_Luma_16u = &MFX_HEVC_PP::h265_ProcessSaoCu_Luma_16u_px;

    //[Interpoaltion]==================================
    // average
    g_dispatcher.h265_AverageModeB = &MFX_HEVC_PP::h265_AverageModeB_avx2;
    g_dispatcher.h265_AverageModeP = &MFX_HEVC_PP::h265_AverageModeP_avx2;
    g_dispatcher.h265_AverageModeN = &MFX_HEVC_PP::h265_AverageModeN_avx2;

    g_dispatcher.h265_AverageModeB_U16 = &MFX_HEVC_PP::h265_AverageModeB_U16_avx2;
    g_dispatcher.h265_AverageModeP_U16 = &MFX_HEVC_PP::h265_AverageModeP_U16_avx2;
    g_dispatcher.h265_AverageModeN_U16 = &MFX_HEVC_PP::h265_AverageModeN_U16_avx2;

#ifdef __INTEL_COMPILER
    // algo
    g_dispatcher.h265_InterpLuma_s8_d16_H = &MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_avx2;
    g_dispatcher.h265_InterpChroma_s8_d16_H = &MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_avx2;

    g_dispatcher.h265_InterpLuma_s8_d16_V = &MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_avx2;
    g_dispatcher.h265_InterpChroma_s8_d16_V = &MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_avx2;

    //[transform.inv]==================================
    g_dispatcher.h265_DST4x4Inv_16sT = &MFX_HEVC_PP::h265_DST4x4Inv_16sT_avx2;
    g_dispatcher.h265_DCT4x4Inv_16sT = &MFX_HEVC_PP::h265_DCT4x4Inv_16sT_avx2;
    g_dispatcher.h265_DCT8x8Inv_16sT = &MFX_HEVC_PP::h265_DCT8x8Inv_16sT_avx2;
    g_dispatcher.h265_DCT16x16Inv_16sT = &MFX_HEVC_PP::h265_DCT16x16Inv_16sT_avx2;
    g_dispatcher.h265_DCT32x32Inv_16sT = &MFX_HEVC_PP::h265_DCT32x32Inv_16sT_avx2;
#else
    // use sse instead of avx2 because MSVC incorrect generate _mm256_maddubs_epi16 intrinsic
    g_dispatcher.h265_InterpLuma_s8_d16_H = &MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_sse;
    g_dispatcher.h265_InterpChroma_s8_d16_H = &MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_sse;

    // MSFT compiler generates wrong x64 avx2 for below functions also
    g_dispatcher.h265_InterpLuma_s8_d16_V = &MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_sse;
    g_dispatcher.h265_InterpChroma_s8_d16_V = &MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_sse;

    //[transform.inv]==================================
    g_dispatcher.h265_DST4x4Inv_16sT = &MFX_HEVC_PP::h265_DST4x4Inv_16sT_sse;
    g_dispatcher.h265_DCT4x4Inv_16sT = &MFX_HEVC_PP::h265_DCT4x4Inv_16sT_sse;
    g_dispatcher.h265_DCT8x8Inv_16sT = &MFX_HEVC_PP::h265_DCT8x8Inv_16sT_sse;
    g_dispatcher.h265_DCT16x16Inv_16sT = &MFX_HEVC_PP::h265_DCT16x16Inv_16sT_sse;
    g_dispatcher.h265_DCT32x32Inv_16sT = &MFX_HEVC_PP::h265_DCT32x32Inv_16sT_sse;
#endif

    g_dispatcher.h265_InterpLuma_s16_d16_V = &MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_avx2;
    g_dispatcher.h265_InterpChroma_s16_d16_V = &MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_avx2;
    g_dispatcher.h265_InterpLuma_s16_d16_H = &MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_avx2;
    g_dispatcher.h265_InterpChroma_s16_d16_H = &MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_avx2;

    g_dispatcher.h265_InterpLumaFast_s16_d16_V = &MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_avx2;
    g_dispatcher.h265_InterpLumaFast_s16_d16_H = &MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_avx2;      
    g_dispatcher.h265_InterpLumaFast_s8_d16_H =  &MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_H_avx2;
    g_dispatcher.h265_InterpLumaFast_s8_d16_V =  &MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_V_avx2;

    g_dispatcher.h265_InterpLumaPack_d8 = &MFX_HEVC_PP::h265_InterpLumaPack_avx2<Ipp8u>;
    g_dispatcher.h265_InterpLumaPack_d16 = &MFX_HEVC_PP::h265_InterpLumaPack_avx2<Ipp16u>;

    // Convert w/ rshift
    g_dispatcher.h265_ConvertShiftR = &MFX_HEVC_PP::h265_ConvertShiftR_avx2;

    // [INTRA prediction]
    g_dispatcher.h265_PredictIntra_Ang_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_8u_avx2;
    g_dispatcher.h265_PredictIntra_Ang_All_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_avx2;
    g_dispatcher.h265_PredictIntra_Ang_NoTranspose_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_avx2;
    g_dispatcher.h265_PredictIntra_Ang_All_Even_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_avx2;

    g_dispatcher.h265_PredictIntra_Ang_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_16u_avx2;
    g_dispatcher.h265_PredictIntra_Ang_NoTranspose_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_avx2;
    g_dispatcher.h265_PredictIntra_Ang_All_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_avx2;
    g_dispatcher.h265_PredictIntra_Ang_All_Even_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_avx2;

    g_dispatcher.h265_FilterPredictPels_8u = &MFX_HEVC_PP::h265_FilterPredictPels_8u_avx2;
    g_dispatcher.h265_FilterPredictPels_Bilinear_8u = &MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_8u_avx2;
    g_dispatcher.h265_PredictIntra_Planar_8u = &MFX_HEVC_PP::h265_PredictIntra_Planar_8u_avx2;
    g_dispatcher.h265_PredictIntra_Planar_ChromaNV12_8u = h265_PredictIntra_Planar_ChromaNV12_8u_avx2;

    g_dispatcher.h265_FilterPredictPels_16s = &MFX_HEVC_PP::h265_FilterPredictPels_16s_avx2;
    g_dispatcher.h265_FilterPredictPels_Bilinear_16s = &MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_16s_avx2;
    g_dispatcher.h265_PredictIntra_Planar_16s = &MFX_HEVC_PP::h265_PredictIntra_Planar_16s_avx2;

#if defined( __INTEL_COMPILER )
    g_dispatcher.h265_AnalyzeGradient_8u = &MFX_HEVC_PP::h265_AnalyzeGradient_avx2<Ipp8u, Ipp16u>;
    g_dispatcher.h265_AnalyzeGradient_16u = &MFX_HEVC_PP::h265_AnalyzeGradient_avx2<Ipp16u, Ipp32u>;
#else
    g_dispatcher.h265_AnalyzeGradient_8u = &MFX_HEVC_PP::h265_AnalyzeGradient_sse<Ipp8u, Ipp16u>;
    g_dispatcher.h265_AnalyzeGradient_16u = &MFX_HEVC_PP::h265_AnalyzeGradient_sse<Ipp16u, Ipp32u>;
#endif

    g_dispatcher.h265_ComputeRsCs_8u = &MFX_HEVC_PP::h265_ComputeRsCs_8u_avx2;
    g_dispatcher.h265_ComputeRsCs_16u = &MFX_HEVC_PP::h265_ComputeRsCs_16u_sse;
    g_dispatcher.h265_AddClipNv12UV_8u = &MFX_HEVC_PP::h265_AddClipNv12UV_8u_avx2;
    g_dispatcher.h265_DiffDc_8u  = &MFX_HEVC_PP::h265_DiffDc_avx2<Ipp8u>;
    g_dispatcher.h265_DiffDc_16u = &MFX_HEVC_PP::h265_DiffDc_px<Ipp16u>;

    // [WeightedPred]
    g_dispatcher.h265_CopyWeighted_S16U8 = &MFX_HEVC_PP::h265_CopyWeighted_S16U8_avx2;
    g_dispatcher.h265_CopyWeightedBidi_S16U8 = &MFX_HEVC_PP::h265_CopyWeightedBidi_S16U8_avx2;
    g_dispatcher.h265_CopyWeighted_S16U16 = &MFX_HEVC_PP::h265_CopyWeighted_S16U16_avx2;
    g_dispatcher.h265_CopyWeightedBidi_S16U16 = &MFX_HEVC_PP::h265_CopyWeightedBidi_S16U16_avx2;
}


void SetTargetPX()
{
#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
    // [Sad.special]===================================
    g_dispatcher.h265_SAD_4x4_8u =  &MFX_HEVC_PP::SAD_4x4_px;
    g_dispatcher.h265_SAD_4x8_8u =  &MFX_HEVC_PP::SAD_4x8_px;
    g_dispatcher.h265_SAD_4x16_8u =  &MFX_HEVC_PP::SAD_4x16_px;

    g_dispatcher.h265_SAD_8x4_8u =  &MFX_HEVC_PP::SAD_8x4_px;
    g_dispatcher.h265_SAD_8x8_8u =  &MFX_HEVC_PP::SAD_8x8_px;
    g_dispatcher.h265_SAD_8x16_8u =  &MFX_HEVC_PP::SAD_8x16_px;
    g_dispatcher.h265_SAD_8x32_8u =  &MFX_HEVC_PP::SAD_8x32_px;

    g_dispatcher.h265_SAD_12x16_8u =  &MFX_HEVC_PP::SAD_12x16_px;

    g_dispatcher.h265_SAD_16x4_8u =  &MFX_HEVC_PP::SAD_16x4_px;
    g_dispatcher.h265_SAD_16x8_8u =  &MFX_HEVC_PP::SAD_16x8_px;
    g_dispatcher.h265_SAD_16x12_8u =  &MFX_HEVC_PP::SAD_16x12_px;
    g_dispatcher.h265_SAD_16x16_8u =  &MFX_HEVC_PP::SAD_16x16_px;
    g_dispatcher.h265_SAD_16x32_8u =  &MFX_HEVC_PP::SAD_16x32_px;
    g_dispatcher.h265_SAD_16x64_8u =  &MFX_HEVC_PP::SAD_16x64_px;

    g_dispatcher.h265_SAD_24x32_8u =  &MFX_HEVC_PP::SAD_24x32_px;

    g_dispatcher.h265_SAD_32x8_8u =  &MFX_HEVC_PP::SAD_32x8_px;
    g_dispatcher.h265_SAD_32x16_8u =  &MFX_HEVC_PP::SAD_32x16_px;
    g_dispatcher.h265_SAD_32x24_8u =  &MFX_HEVC_PP::SAD_32x24_px;
    g_dispatcher.h265_SAD_32x32_8u =  &MFX_HEVC_PP::SAD_32x32_px;
    g_dispatcher.h265_SAD_32x64_8u =  &MFX_HEVC_PP::SAD_32x64_px;

    g_dispatcher.h265_SAD_48x64_8u =  &MFX_HEVC_PP::SAD_48x64_px;

    g_dispatcher.h265_SAD_64x16_8u =  &MFX_HEVC_PP::SAD_64x16_px;
    g_dispatcher.h265_SAD_64x32_8u =  &MFX_HEVC_PP::SAD_64x32_px;
    g_dispatcher.h265_SAD_64x48_8u =  &MFX_HEVC_PP::SAD_64x48_px;
    g_dispatcher.h265_SAD_64x64_8u =  &MFX_HEVC_PP::SAD_64x64_px;

    // [Sad.general]===================================
    g_dispatcher.h265_SAD_4x4_general_8u =  &MFX_HEVC_PP::SAD_4x4_general_px;
    g_dispatcher.h265_SAD_4x8_general_8u =  &MFX_HEVC_PP::SAD_4x8_general_px;
    g_dispatcher.h265_SAD_4x16_general_8u =  &MFX_HEVC_PP::SAD_4x16_general_px;

    g_dispatcher.h265_SAD_8x4_general_8u =  &MFX_HEVC_PP::SAD_8x4_general_px;
    g_dispatcher.h265_SAD_8x8_general_8u =  &MFX_HEVC_PP::SAD_8x8_general_px;
    g_dispatcher.h265_SAD_8x16_general_8u =  &MFX_HEVC_PP::SAD_8x16_general_px;
    g_dispatcher.h265_SAD_8x32_general_8u =  &MFX_HEVC_PP::SAD_8x32_general_px;

    g_dispatcher.h265_SAD_12x16_general_8u =  &MFX_HEVC_PP::SAD_12x16_general_px;

    g_dispatcher.h265_SAD_16x4_general_8u =  &MFX_HEVC_PP::SAD_16x4_general_px;
    g_dispatcher.h265_SAD_16x8_general_8u =  &MFX_HEVC_PP::SAD_16x8_general_px;
    g_dispatcher.h265_SAD_16x12_general_8u =  &MFX_HEVC_PP::SAD_16x12_general_px;
    g_dispatcher.h265_SAD_16x16_general_8u =  &MFX_HEVC_PP::SAD_16x16_general_px;
    g_dispatcher.h265_SAD_16x32_general_8u =  &MFX_HEVC_PP::SAD_16x32_general_px;
    g_dispatcher.h265_SAD_16x64_general_8u =  &MFX_HEVC_PP::SAD_16x64_general_px;

    g_dispatcher.h265_SAD_24x32_general_8u =  &MFX_HEVC_PP::SAD_24x32_general_px;

    g_dispatcher.h265_SAD_32x8_general_8u =  &MFX_HEVC_PP::SAD_32x8_general_px;
    g_dispatcher.h265_SAD_32x16_general_8u =  &MFX_HEVC_PP::SAD_32x16_general_px;
    g_dispatcher.h265_SAD_32x24_general_8u =  &MFX_HEVC_PP::SAD_32x24_general_px;
    g_dispatcher.h265_SAD_32x32_general_8u =  &MFX_HEVC_PP::SAD_32x32_general_px;
    g_dispatcher.h265_SAD_32x64_general_8u =  &MFX_HEVC_PP::SAD_32x64_general_px;

    g_dispatcher.h265_SAD_48x64_general_8u =  &MFX_HEVC_PP::SAD_48x64_general_px;

    g_dispatcher.h265_SAD_64x16_general_8u =  &MFX_HEVC_PP::SAD_64x16_general_px;
    g_dispatcher.h265_SAD_64x32_general_8u =  &MFX_HEVC_PP::SAD_64x32_general_px;
    g_dispatcher.h265_SAD_64x48_general_8u =  &MFX_HEVC_PP::SAD_64x48_general_px;
    g_dispatcher.h265_SAD_64x64_general_8u =  &MFX_HEVC_PP::SAD_64x64_general_px;

    g_dispatcher.h265_SAD_MxN_x3_8u        =  &MFX_HEVC_PP::h265_SAD_MxN_x3_8u_px;
    g_dispatcher.h265_SAD_MxN_x4_8u        =  &MFX_HEVC_PP::h265_SAD_MxN_x4_8u_px;
    g_dispatcher.h265_SAD_MxN_general_16s  =  &MFX_HEVC_PP::h265_SAD_MxN_general_16s_px;
    g_dispatcher.h265_SAD_MxN_16s          =  &MFX_HEVC_PP::h265_SAD_MxN_16s_px;

    //[transform.fwd]==================================
    g_dispatcher.h265_DST4x4Fwd_16s = &MFX_HEVC_PP::h265_DST4x4Fwd_16s_px;
    g_dispatcher.h265_DCT4x4Fwd_16s = &MFX_HEVC_PP::h265_DCT4x4Fwd_16s_px;
    g_dispatcher.h265_DCT8x8Fwd_16s = &MFX_HEVC_PP::h265_DCT8x8Fwd_16s_px;
    g_dispatcher.h265_DCT16x16Fwd_16s = &MFX_HEVC_PP::h265_DCT16x16Fwd_16s_px;
    g_dispatcher.h265_DCT32x32Fwd_16s = &MFX_HEVC_PP::h265_DCT32x32Fwd_16s_px;

    // quantization
    g_dispatcher.h265_QuantFwd_16s = &MFX_HEVC_PP::h265_QuantFwd_16s_px;
    g_dispatcher.h265_QuantFwd_SBH_16s = &MFX_HEVC_PP::h265_QuantFwd_SBH_16s_px;
    g_dispatcher.h265_Quant_zCost_16s = &MFX_HEVC_PP::h265_Quant_zCost_16s_px;
    g_dispatcher.h265_QuantInv_16s = &MFX_HEVC_PP::h265_QuantInv_16s_px;

    g_dispatcher.h265_SSE_8u  = &MFX_HEVC_PP::h265_SSE_px<Ipp8u>;
    g_dispatcher.h265_SSE_16u = &MFX_HEVC_PP::h265_SSE_px<Ipp16u>;

    g_dispatcher.h265_Diff_8u  = &MFX_HEVC_PP::h265_Diff_px<Ipp8u>;
    g_dispatcher.h265_Diff_16u = &MFX_HEVC_PP::h265_Diff_px<Ipp16u>;
    g_dispatcher.h265_DiffNv12_8u  = &MFX_HEVC_PP::h265_DiffNv12_px<Ipp8u>;
    g_dispatcher.h265_DiffNv12_16u = &MFX_HEVC_PP::h265_DiffNv12_px<Ipp16u>;

    g_dispatcher.h265_SplitChromaCtb_8u  = &MFX_HEVC_PP::h265_SplitChromaCtb_px<Ipp8u>;
    g_dispatcher.h265_SplitChromaCtb_16u = &MFX_HEVC_PP::h265_SplitChromaCtb_px<Ipp16u>;

    g_dispatcher.h265_ImageDiffHistogram_8u  = &MFX_HEVC_PP::h265_ImageDiffHistogram_8u_px;

    g_dispatcher.h265_SearchBestBlock8x8_8u  = &MFX_HEVC_PP::h265_SearchBestBlock8x8_8u_px;

    g_dispatcher.h265_ComputeRsCsDiff  = &MFX_HEVC_PP::h265_ComputeRsCsDiff_px;

    g_dispatcher.h265_ComputeRsCs4x4_8u  = &MFX_HEVC_PP::h265_ComputeRsCs4x4_8u_px;
    // SATD
    g_dispatcher.h265_SATD_4x4_8u = &MFX_HEVC_PP::h265_SATD_4x4_8u_px;
    g_dispatcher.h265_SATD_8x8_8u = &MFX_HEVC_PP::h265_SATD_8x8_8u_px;

    g_dispatcher.h265_SATD_4x4_Pair_8u = &MFX_HEVC_PP::h265_SATD_4x4_Pair_8u_px;
    g_dispatcher.h265_SATD_8x8_Pair_8u = &MFX_HEVC_PP::h265_SATD_8x8_Pair_8u_px;

    g_dispatcher.h265_SATD_4x4_16u = &MFX_HEVC_PP::h265_SATD_4x4_16u_px;
    g_dispatcher.h265_SATD_8x8_16u = &MFX_HEVC_PP::h265_SATD_8x8_16u_px;

    g_dispatcher.h265_SATD_4x4_Pair_16u = &MFX_HEVC_PP::h265_SATD_4x4_Pair_16u_px;
    g_dispatcher.h265_SATD_8x8_Pair_16u = &MFX_HEVC_PP::h265_SATD_8x8_Pair_16u_px;
#endif

    //[transform.inv]==================================
    g_dispatcher.h265_DST4x4Inv_16sT = &MFX_HEVC_PP::h265_DST4x4Inv_16sT_px;
    g_dispatcher.h265_DCT4x4Inv_16sT = &MFX_HEVC_PP::h265_DCT4x4Inv_16sT_px;
    g_dispatcher.h265_DCT8x8Inv_16sT = &MFX_HEVC_PP::h265_DCT8x8Inv_16sT_px;
    g_dispatcher.h265_DCT16x16Inv_16sT = &MFX_HEVC_PP::h265_DCT16x16Inv_16sT_px;
    g_dispatcher.h265_DCT32x32Inv_16sT = &MFX_HEVC_PP::h265_DCT32x32Inv_16sT_px;

    //[deblocking]=====================================
    g_dispatcher.h265_FilterEdgeLuma_8u_I = &MFX_HEVC_PP::h265_FilterEdgeLuma_8u_I_px;
    g_dispatcher.h265_FilterEdgeChroma_Interleaved_8u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Interleaved_8u_I_px;
    g_dispatcher.h265_FilterEdgeChroma_Plane_8u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Plane_8u_I_px;

    g_dispatcher.h265_FilterEdgeLuma_16u_I = &MFX_HEVC_PP::h265_FilterEdgeLuma_16u_I_px;
    g_dispatcher.h265_FilterEdgeChroma_Interleaved_16u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Interleaved_16u_I_px;
    g_dispatcher.h265_FilterEdgeChroma_Plane_16u_I = &MFX_HEVC_PP::h265_FilterEdgeChroma_Plane_16u_I_px;

    //[SAO]============================================
    g_dispatcher.h265_ProcessSaoCuOrg_Luma_8u = &MFX_HEVC_PP::h265_ProcessSaoCuOrg_Luma_8u_px;
    g_dispatcher.h265_ProcessSaoCu_Luma_8u = &MFX_HEVC_PP::h265_ProcessSaoCu_Luma_8u_px;
    g_dispatcher.h265_GetCtuStatistics_8u = &MFX_HEVC_PP::h265_GetCtuStatistics_8u_px;
    g_dispatcher.h265_GetCtuStatistics_16u = &MFX_HEVC_PP::h265_GetCtuStatistics_16u_px;

    g_dispatcher.h265_ProcessSaoCuOrg_Luma_16u = &MFX_HEVC_PP::h265_ProcessSaoCuOrg_Luma_16u_px;
    g_dispatcher.h265_ProcessSaoCu_Luma_16u = &MFX_HEVC_PP::h265_ProcessSaoCu_Luma_16u_px;

    //[Interpoaltion]==================================
    // average
    g_dispatcher.h265_AverageModeB = &MFX_HEVC_PP::h265_AverageModeB_px;
    g_dispatcher.h265_AverageModeP = &MFX_HEVC_PP::h265_AverageModeP_px;
    g_dispatcher.h265_AverageModeN = &MFX_HEVC_PP::h265_AverageModeN_px;
        
    g_dispatcher.h265_AverageModeB_U16 = &MFX_HEVC_PP::h265_AverageModeB_U16_px;
    g_dispatcher.h265_AverageModeP_U16 = &MFX_HEVC_PP::h265_AverageModeP_U16_px;
    g_dispatcher.h265_AverageModeN_U16 = &MFX_HEVC_PP::h265_AverageModeN_U16_px;

    // algo
    g_dispatcher.h265_InterpLuma_s8_d16_H = &MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_px;
    g_dispatcher.h265_InterpChroma_s8_d16_H = &MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_px;
    g_dispatcher.h265_InterpLuma_s8_d16_V = &MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_px;
    g_dispatcher.h265_InterpChroma_s8_d16_V = &MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_px;
    g_dispatcher.h265_InterpLuma_s16_d16_V = &MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px;
    g_dispatcher.h265_InterpChroma_s16_d16_V = &MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px;
    g_dispatcher.h265_InterpLuma_s16_d16_H = &MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_px;
    g_dispatcher.h265_InterpChroma_s16_d16_H = &MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_px;

    // FastInterp //kolya
    g_dispatcher.h265_InterpLumaFast_s8_d16_H = &MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_H_px;
    g_dispatcher.h265_InterpLumaFast_s8_d16_V = &MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_V_px;
    g_dispatcher.h265_InterpLumaFast_s16_d16_V = &MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px;
    g_dispatcher.h265_InterpLumaFast_s16_d16_H = &MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_px;

    g_dispatcher.h265_InterpLumaPack_d8 = &MFX_HEVC_PP::h265_InterpLumaPack_px<Ipp8u>;
    g_dispatcher.h265_InterpLumaPack_d16 = &MFX_HEVC_PP::h265_InterpLumaPack_px<Ipp16u>;

    // Convert w/ rshift
    g_dispatcher.h265_ConvertShiftR = &MFX_HEVC_PP::h265_ConvertShiftR_px;

    // [INTRA prediction]
    g_dispatcher.h265_PredictIntra_Ang_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_8u_px;
    g_dispatcher.h265_PredictIntra_Ang_All_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_px;
    g_dispatcher.h265_PredictIntra_Ang_NoTranspose_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_px;
    g_dispatcher.h265_PredictIntra_Ang_All_Even_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_px;

    g_dispatcher.h265_PredictIntra_Ang_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_16u_px;
    g_dispatcher.h265_PredictIntra_Ang_NoTranspose_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_px;
    g_dispatcher.h265_PredictIntra_Ang_All_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_px;
    g_dispatcher.h265_PredictIntra_Ang_All_Even_16u = &MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_px;

    g_dispatcher.h265_FilterPredictPels_8u = &MFX_HEVC_PP::h265_FilterPredictPels_8u_px;
    g_dispatcher.h265_FilterPredictPels_Bilinear_8u = &MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_8u_px;
    g_dispatcher.h265_PredictIntra_Planar_8u = &MFX_HEVC_PP::h265_PredictIntra_Planar_8u_px;
    g_dispatcher.h265_PredictIntra_Planar_ChromaNV12_8u = h265_PredictIntra_Planar_ChromaNV12_8u_px;

    g_dispatcher.h265_FilterPredictPels_16s = &MFX_HEVC_PP::h265_FilterPredictPels_16s_px;
    g_dispatcher.h265_FilterPredictPels_Bilinear_16s = &MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_16s_px;
    g_dispatcher.h265_PredictIntra_Planar_16s = &MFX_HEVC_PP::h265_PredictIntra_Planar_16s_px;

    g_dispatcher.h265_AnalyzeGradient_8u = &MFX_HEVC_PP::h265_AnalyzeGradient_px<Ipp8u, Ipp16u>;
    g_dispatcher.h265_AnalyzeGradient_16u = &MFX_HEVC_PP::h265_AnalyzeGradient_px<Ipp16u, Ipp32u>;

    g_dispatcher.h265_ComputeRsCs_8u = &MFX_HEVC_PP::h265_ComputeRsCs_px<Ipp8u>;
    g_dispatcher.h265_ComputeRsCs_16u = &MFX_HEVC_PP::h265_ComputeRsCs_px<Ipp16u>;
    g_dispatcher.h265_AddClipNv12UV_8u = &MFX_HEVC_PP::h265_AddClipNv12UV_8u_px;
    g_dispatcher.h265_DiffDc_8u  = &MFX_HEVC_PP::h265_DiffDc_px<Ipp8u>;
    g_dispatcher.h265_DiffDc_16u = &MFX_HEVC_PP::h265_DiffDc_px<Ipp16u>;

    // [WeightedPred]
    g_dispatcher.h265_CopyWeighted_S16U8 = &MFX_HEVC_PP::h265_CopyWeighted_S16U8_px;
    g_dispatcher.h265_CopyWeightedBidi_S16U8 = &MFX_HEVC_PP::h265_CopyWeightedBidi_S16U8_px;
    g_dispatcher.h265_CopyWeighted_S16U16 = &MFX_HEVC_PP::h265_CopyWeighted_S16U16_px;
    g_dispatcher.h265_CopyWeightedBidi_S16U16 = &MFX_HEVC_PP::h265_CopyWeightedBidi_S16U16_px;
}

#else // MFX_TARGET_OPTIMIZATION_AUTO (PREDEFINED_TARGET_OPTIMIZATION PX/SSE4/AVX2)

IppStatus MFX_HEVC_PP::InitDispatcher(Ipp32s cpuFeature)
{
    return ippStsNoErr;
}

#endif // MFX_TARGET_OPTIMIZATION_AUTO

// COMMON CASE
enum EnumPlane
{
    TEXT_LUMA = 0,
    TEXT_CHROMA,
    TEXT_CHROMA_U,
    TEXT_CHROMA_V,
};

/* ******************************************************** */
/*       Interfaces for Interpoaltion primitives            */
/* ******************************************************** */

/* this is the most heavily used path (~50% of total pixels) and most highly optimized 
* read in 8-bit pixels and apply H or V filter, save output to 16-bit buffer (for second filtering pass or for averaging with reference)
*/
void MFX_HEVC_PP::Interp_NoAvg(
    const unsigned char* pSrc, 
    unsigned int srcPitch, 
    short *pDst, 
    unsigned int dstPitch, 
    int tab_index, 
    int width, 
    int height, 
    int shift, 
    short offset, 
    int dir, 
    int plane, int isFast)
{
    VM_ASSERT( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

    if (plane == TEXT_LUMA) {
        if (dir == INTERP_HOR) {
            if ( isFast )
                NAME(h265_InterpLumaFast_s8_d16_H)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
            else
                NAME(h265_InterpLuma_s8_d16_H)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
        } else {
            if ( isFast )
                NAME(h265_InterpLumaFast_s8_d16_V)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
            else
                NAME(h265_InterpLuma_s8_d16_V)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
        }
    } else {
        if (dir == INTERP_HOR)
            NAME(h265_InterpChroma_s8_d16_H)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset, plane);
        else
            NAME(h265_InterpChroma_s8_d16_V)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
    }
}

/* typically used for ~15% of total pixels, does vertical filter pass only */
void MFX_HEVC_PP::Interp_NoAvg(
    const short* pSrc, 
    unsigned int srcPitch, 
    short *pDst, 
    unsigned int dstPitch, 
    int tab_index, 
    int width, 
    int height, 
    int shift, 
    short offset, 
    int dir, 
    int plane, int isFast)
{
    VM_ASSERT( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

    /* only V is supported/needed */
    if (dir != INTERP_VER)
        return;

    if (plane == TEXT_LUMA) {
        if ( isFast )
            NAME(h265_InterpLumaFast_s16_d16_V)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
        else
            NAME(h265_InterpLuma_s16_d16_V)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
    } else
        NAME(h265_InterpChroma_s16_d16_V)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
}

/* NOTE: average functions assume maximum block size of 64x64, including 7 extra output rows for H pass */

/* typically used for ~15% of total pixels, does first-pass horizontal or vertical filter, optionally with average against reference */
void MFX_HEVC_PP::Interp_WithAvg(
    const unsigned char* pSrc, 
    unsigned int srcPitch, 
    unsigned char *pDst, 
    unsigned int dstPitch, 
    void *pvAvg, 
    unsigned int avgPitch, 
    int avgMode, 
    int tab_index, 
    int width, 
    int height, 
    int shift, 
    short offset, 
    int dir, 
    int plane, unsigned, int isFast)
{
    /* pretty big stack buffer, probably want to pass this in (allocate on heap) */
    ALIGN_DECL(16) short tmpBuf[(64+8)*(64+16)];

    VM_ASSERT( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

    if (plane == TEXT_LUMA) {
        if (dir == INTERP_HOR) {
            if ( isFast )
                NAME(h265_InterpLumaFast_s8_d16_H)(pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
            else
                NAME(h265_InterpLuma_s8_d16_H)(pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
        } else {
            if ( isFast )
                NAME(h265_InterpLumaFast_s8_d16_V)(pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
            else
                NAME(h265_InterpLuma_s8_d16_V)(pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
        }
    } else {
        if (dir == INTERP_HOR)
            NAME(h265_InterpChroma_s8_d16_H)(pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset, plane);
        else
            NAME(h265_InterpChroma_s8_d16_V)(pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
    }

    if (avgMode == AVERAGE_NO)
        NAME(h265_AverageModeN)(tmpBuf, 64+16, pDst, dstPitch, width, height);
    else if (avgMode == AVERAGE_FROM_PIC)
        NAME(h265_AverageModeP)(tmpBuf, 64+16, (unsigned char *)pvAvg, avgPitch, pDst, dstPitch, width, height);
    else if (avgMode == AVERAGE_FROM_BUF)
        NAME(h265_AverageModeB)(tmpBuf, 64+16, (short *)pvAvg, avgPitch, pDst, dstPitch, width, height);
}


/* typically used for ~20% of total pixels, does second-pass vertical filter only, optionally with average against reference */
void MFX_HEVC_PP::Interp_WithAvg(
    const short* pSrc, 
    unsigned int srcPitch, 
    unsigned char *pDst, 
    unsigned int dstPitch, 
    void *pvAvg, 
    unsigned int avgPitch, 
    int avgMode, 
    int tab_index, 
    int width, 
    int height, 
    int shift, 
    short offset, 
    int dir, 
    int plane, unsigned , int isFast)
{
    /* pretty big stack buffer, probably want to pass this in (allocate on heap) */
    ALIGN_DECL(16) short tmpBuf[(64+8)*(64+16)];

    /* only V is supported/needed */
    if (dir != INTERP_VER)
        return;

    VM_ASSERT( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

    if (plane == TEXT_LUMA) {
        if ( isFast )
            NAME(h265_InterpLumaFast_s16_d16_V)(pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
        else
            NAME(h265_InterpLuma_s16_d16_V)(pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
    } else 
        NAME(h265_InterpChroma_s16_d16_V)(pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);

    if (avgMode == AVERAGE_NO)
        NAME(h265_AverageModeN)(tmpBuf, 64+16, pDst, dstPitch, width, height);
    else if (avgMode == AVERAGE_FROM_PIC)
        NAME(h265_AverageModeP)(tmpBuf, 64+16, (unsigned char *)pvAvg, avgPitch, pDst, dstPitch, width, height);
    else if (avgMode == AVERAGE_FROM_BUF)
        NAME(h265_AverageModeB)(tmpBuf, 64+16, (short *)pvAvg, avgPitch, pDst, dstPitch, width, height);
}

/* this is the most heavily used path (~50% of total pixels) and most highly optimized 
* read in 8-bit pixels and apply H or V filter, save output to 16-bit buffer (for second filtering pass or for averaging with reference)
*/
void MFX_HEVC_PP::Interp_NoAvg(
    const Ipp16u* pSrc, 
    unsigned int srcPitch, 
    short *pDst, 
    unsigned int dstPitch, 
    int tab_index, 
    int width, 
    int height, 
    int shift, 
    short offset, 
    int dir, 
    int plane, int isFast)
{
    VM_ASSERT( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

    if (plane == TEXT_LUMA) {
        if (dir == INTERP_HOR) {
            if ( isFast )
                NAME(h265_InterpLumaFast_s16_d16_H)((const short *)pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
            else
                NAME(h265_InterpLuma_s16_d16_H)((const short *)pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
        } else {
            if ( isFast )
                NAME(h265_InterpLuma_s16_d16_V)((const short *)pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
            else
                NAME(h265_InterpLuma_s16_d16_V)((const short *)pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
        }
    } else {
        if (dir == INTERP_HOR)
            NAME(h265_InterpChroma_s16_d16_H)((const short *)pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset, plane);
        else
            NAME(h265_InterpChroma_s16_d16_V)((const short *)pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
    }
}

/* NOTE: average functions assume maximum block size of 64x64, including 7 extra output rows for H pass */

/* typically used for ~15% of total pixels, does first-pass horizontal or vertical filter, optionally with average against reference */
void MFX_HEVC_PP::Interp_WithAvg(
    const Ipp16u* pSrc, 
    unsigned int srcPitch, 
    Ipp16u *pDst, 
    unsigned int dstPitch, 
    void *pvAvg, 
    unsigned int avgPitch, 
    int avgMode, 
    int tab_index, 
    int width, 
    int height, 
    int shift, 
    short offset, 
    int dir, 
    int plane,
    unsigned bit_depth, int isFast)
{
    /* pretty big stack buffer, probably want to pass this in (allocate on heap) */
    ALIGN_DECL(16) short tmpBuf[(64+8)*(64+16)];

    VM_ASSERT( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

    if (plane == TEXT_LUMA) {
        if (dir == INTERP_HOR) {
            if ( isFast )
                NAME(h265_InterpLumaFast_s16_d16_H)((const short *)pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
            else
                NAME(h265_InterpLuma_s16_d16_H)((const short *)pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
        } else {
            if ( isFast )
                NAME(h265_InterpLumaFast_s16_d16_V)((const short *)pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
            else
                NAME(h265_InterpLuma_s16_d16_V)((const short *)pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
        }
    } else {
        if (dir == INTERP_HOR)
            NAME(h265_InterpChroma_s16_d16_H)((const short *)pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset, plane);
        else
            NAME(h265_InterpChroma_s16_d16_V)((const short *)pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
    }

    if (avgMode == AVERAGE_NO)
        NAME(h265_AverageModeN_U16)(tmpBuf, 64+16, pDst, dstPitch, width, height, bit_depth);
    else if (avgMode == AVERAGE_FROM_PIC)
        NAME(h265_AverageModeP_U16)(tmpBuf, 64+16, (Ipp16u *)pvAvg, avgPitch, pDst, dstPitch, width, height, bit_depth);
    else if (avgMode == AVERAGE_FROM_BUF)
        NAME(h265_AverageModeB_U16)(tmpBuf, 64+16, (short *)pvAvg, avgPitch, pDst, dstPitch, width, height, bit_depth);
}


/* typically used for ~20% of total pixels, does second-pass vertical filter only, optionally with average against reference */
void MFX_HEVC_PP::Interp_WithAvg(
    const short* pSrc, 
    unsigned int srcPitch, 
    Ipp16u *pDst, 
    unsigned int dstPitch, 
    void *pvAvg, 
    unsigned int avgPitch, 
    int avgMode, 
    int tab_index, 
    int width, 
    int height, 
    int shift, 
    short offset, 
    int dir, 
    int plane,
    unsigned bit_depth, int isFast)
{
    /* pretty big stack buffer, probably want to pass this in (allocate on heap) */
    ALIGN_DECL(16) short tmpBuf[(64+8)*(64+16)];

    /* only V is supported/needed */
    if (dir != INTERP_VER)
        return;

    VM_ASSERT( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

    if (plane == TEXT_LUMA) {
        if ( isFast )
            NAME(h265_InterpLumaFast_s16_d16_V)(pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
        else
            NAME(h265_InterpLuma_s16_d16_V)(pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);
    } else
        NAME(h265_InterpChroma_s16_d16_V)(pSrc, srcPitch, tmpBuf, 64+16, tab_index, width, height, shift, offset);

    if (avgMode == AVERAGE_NO)
        NAME(h265_AverageModeN_U16)(tmpBuf, 64+16, pDst, dstPitch, width, height, bit_depth);
    else if (avgMode == AVERAGE_FROM_PIC)
        NAME(h265_AverageModeP_U16)(tmpBuf, 64+16, (Ipp16u *)pvAvg, avgPitch, pDst, dstPitch, width, height, bit_depth);
    else if (avgMode == AVERAGE_FROM_BUF)
        NAME(h265_AverageModeB_U16)(tmpBuf, 64+16, (short *)pvAvg, avgPitch, pDst, dstPitch, width, height, bit_depth);
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
