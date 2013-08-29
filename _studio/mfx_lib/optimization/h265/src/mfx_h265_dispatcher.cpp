/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"
#include "mfx_h265_dispatcher.h"
#include "ipp.h"

//namespace MFX_HEVC_PP
//{
using namespace MFX_HEVC_PP;

#if defined(MFX_TARGET_OPTIMIZATION_AUTO)

    void SetTargetAVX2(void);
    void SetTargetSSE4(void);
    void SetTargetSSE4_ATOM(void);
    void SetTargetPX(void);

    MFX_HEVC_PP::Dispatcher MFX_HEVC_PP::g_dispatcher = {NULL, false};

    IppStatus MFX_HEVC_PP::InitDispatcher( void )
    {
        // [0] No multiple initialization
        if(g_dispatcher.isInited)
        {
            return ippStsNoErr;
        }

        // [1] GetPlatformType()
        Ipp32u cpuIdInfoRegs[4];
        Ipp64u featuresMask;
        IppStatus sts = ippGetCpuFeatures( &featuresMask, cpuIdInfoRegs);
        if(ippStsNoErr != sts)    return sts;
        
        //aya: AVX2 has issues, wait response from opt guys, PX not completed. 
        // so, SSE4 only is worked
        // will be fixed ASAP
        /*if ( featuresMask & (Ipp64u)(ippCPUID_AVX2) )
        {
            SetTargetAVX2();
        }
        else if (featuresMask & (Ipp64u)(ippCPUID_SSE42))*/
        {        
            SetTargetSSE4();
        }
        /*else 
        {
            SetTargetPX();
        }*/
        
        g_dispatcher.isInited = true;

        return ippStsNoErr;

    } // IppStatus InitDispatcher( void )


    // [sad.special - dispatcher]
    int MFX_HEVC_PP::h265_SAD_MxN_special_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY)
    {
        if (SizeX == 4)
        {
            if(SizeY == 4) { return g_dispatcher. h265_SAD_4x4_8u(image,  ref, stride); }
            if(SizeY == 8) { return g_dispatcher. h265_SAD_4x8_8u(image,  ref, stride); }
            else           { return g_dispatcher. h265_SAD_4x16_8u(image,  ref, stride); }
        }
        else if (SizeX == 8)
        {
            if(SizeY ==  4) { return g_dispatcher. h265_SAD_8x4_8u(image,  ref, stride); }
            if(SizeY ==  8) { return g_dispatcher. h265_SAD_8x8_8u(image,  ref, stride); }
            if(SizeY == 16) { return g_dispatcher. h265_SAD_8x16_8u(image,  ref, stride); }
            else            { return g_dispatcher. h265_SAD_8x32_8u(image,  ref, stride); }
        }
        else if (SizeX == 12)
        {            
            return g_dispatcher. h265_SAD_12x16_8u(image,  ref, stride);
        }
        else if (SizeX == 16)
        {
            if(SizeY ==  4) { return g_dispatcher. h265_SAD_16x4_8u(image,  ref, stride); }
            if(SizeY ==  8) { return g_dispatcher. h265_SAD_16x8_8u(image,  ref, stride); }
            if(SizeY == 12) { return g_dispatcher. h265_SAD_16x12_8u(image,  ref, stride);}
            if(SizeY == 16) { return g_dispatcher. h265_SAD_16x16_8u(image,  ref, stride);}
            if(SizeY == 32) { return g_dispatcher. h265_SAD_16x32_8u(image,  ref, stride);}
            else            { return g_dispatcher. h265_SAD_16x64_8u(image,  ref, stride);}
        }
        else if (SizeX == 24)
        {
           return g_dispatcher. h265_SAD_24x32_8u(image,  ref, stride);
        }
        else if (SizeX == 32)
        {
            if(SizeY ==  8) { return g_dispatcher. h265_SAD_32x8_8u(image,  ref, stride); }
            if(SizeY == 16) { return g_dispatcher. h265_SAD_32x16_8u(image,  ref, stride);}
            if(SizeY == 24) { return g_dispatcher. h265_SAD_32x24_8u(image,  ref, stride); }
            if(SizeY == 32) { return g_dispatcher. h265_SAD_32x32_8u(image,  ref, stride);}
            else            { return g_dispatcher. h265_SAD_32x64_8u(image,  ref, stride);}
        }
        else if (SizeX == 48)
        {
            return g_dispatcher. h265_SAD_48x64_8u(image,  ref, stride);
        }
        else if (SizeX == 64)
        {
            if(SizeY == 16) { return g_dispatcher. h265_SAD_64x16_8u(image,  ref, stride);}
            if(SizeY == 32) { return g_dispatcher. h265_SAD_64x32_8u(image,  ref, stride);}
            if(SizeY == 48) { return g_dispatcher. h265_SAD_64x48_8u(image,  ref, stride);}
            else            { return g_dispatcher. h265_SAD_64x64_8u(image,  ref, stride);}
        }

        //funcTimes[index] += (endTime - startTime);
        else return -1;
        
    } // int MFX_HEVC_PP::h265_SAD_MxN_special_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY)


    // [sad.general - dispatcher]
    int MFX_HEVC_PP::h265_SAD_MxN_general_8u(const unsigned char *image,  int stride_img, const unsigned char *ref, int stride_ref, int SizeX, int SizeY)
    {
        int index = 0;
        int cost; 
        if (SizeX == 4)
        {
            if(SizeY == 4) { return g_dispatcher. h265_SAD_4x4_general_8u(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 8) { return g_dispatcher. h265_SAD_4x8_general_8u(image,  ref, stride_img, stride_ref); }
            else           { return g_dispatcher. h265_SAD_4x16_general_8u(image,  ref, stride_img, stride_ref); }
        }
        else if (SizeX == 8)
        {
            if(SizeY ==  4) { return g_dispatcher. h265_SAD_8x4_general_8u(image,  ref, stride_img, stride_ref); }
            else if(SizeY ==  8) { return g_dispatcher. h265_SAD_8x8_general_8u(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 16) { return g_dispatcher. h265_SAD_8x16_general_8u(image,  ref, stride_img, stride_ref); }
            else             { return g_dispatcher. h265_SAD_8x32_general_8u(image,  ref, stride_img, stride_ref); }
        }
        else if (SizeX == 12)
        {
            return g_dispatcher. h265_SAD_12x16_general_8u(image,  ref, stride_img, stride_ref);
        }
        else if (SizeX == 16)
        {
            if(SizeY ==  4) { return g_dispatcher. h265_SAD_16x4_general_8u(image,  ref, stride_img, stride_ref); }
            else if(SizeY ==  8) { return g_dispatcher. h265_SAD_16x8_general_8u(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 12) { return g_dispatcher. h265_SAD_16x12_general_8u(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 16) { return g_dispatcher. h265_SAD_16x16_general_8u(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 32) { return g_dispatcher. h265_SAD_16x32_general_8u(image,  ref, stride_img, stride_ref); }
            else            { return g_dispatcher. h265_SAD_16x64_general_8u(image,  ref, stride_img, stride_ref); }
        }
        else if (SizeX == 24)
        {
            return g_dispatcher. h265_SAD_24x32_general_8u(image,  ref, stride_img, stride_ref);
        }
        else if (SizeX == 32)
        {
            if(SizeY ==  8) { return g_dispatcher. h265_SAD_32x8_general_8u(image,  ref, stride_img, stride_ref);}
            else if(SizeY == 16) { return g_dispatcher. h265_SAD_32x16_general_8u(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 24) { return g_dispatcher. h265_SAD_32x24_general_8u(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 32) { return g_dispatcher. h265_SAD_32x32_general_8u(image,  ref, stride_img, stride_ref);}
            else            { return g_dispatcher. h265_SAD_32x64_general_8u(image,  ref, stride_img, stride_ref);}
        }
        else if (SizeX == 48)
        {
            return g_dispatcher. h265_SAD_48x64_general_8u(image,  ref, stride_img, stride_ref);
        }
        else if (SizeX == 64)
        {
            if(SizeY == 16) { return g_dispatcher. h265_SAD_64x16_general_8u(image,  ref, stride_img, stride_ref);}
            else if(SizeY == 32) { return g_dispatcher. h265_SAD_64x32_general_8u(image,  ref, stride_img, stride_ref);}
            else if(SizeY == 48) { return g_dispatcher. h265_SAD_64x48_general_8u(image,  ref, stride_img, stride_ref);}
            else            { return g_dispatcher. h265_SAD_64x64_general_8u(image,  ref, stride_img, stride_ref); }
        }
        
        else return -1;

    } // int h265_SAD_MxN_general_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY)


    /* ************************************************* */
    /*       Set Target Platform                         */
    /* ************************************************* */
    void SetTargetSSE4(void)
    {
        // [Sad.special]===================================
        g_dispatcher. h265_SAD_4x4_8u =  &MFX_HEVC_PP::SAD_4x4_sse;
        g_dispatcher. h265_SAD_4x8_8u =  &MFX_HEVC_PP::SAD_4x8_sse;
        g_dispatcher. h265_SAD_4x16_8u =  &MFX_HEVC_PP::SAD_4x16_sse;

        g_dispatcher. h265_SAD_8x4_8u =  &MFX_HEVC_PP::SAD_8x4_sse;
        g_dispatcher. h265_SAD_8x8_8u =  &MFX_HEVC_PP::SAD_8x8_sse;
        g_dispatcher. h265_SAD_8x16_8u =  &MFX_HEVC_PP::SAD_8x16_sse;
        g_dispatcher. h265_SAD_8x32_8u =  &MFX_HEVC_PP::SAD_8x32_sse;

        g_dispatcher. h265_SAD_12x16_8u =  &MFX_HEVC_PP::SAD_12x16_sse;

        g_dispatcher. h265_SAD_16x4_8u =  &MFX_HEVC_PP::SAD_16x4_sse;
        g_dispatcher. h265_SAD_16x8_8u =  &MFX_HEVC_PP::SAD_16x8_sse;
        g_dispatcher. h265_SAD_16x12_8u =  &MFX_HEVC_PP::SAD_16x12_sse;
        g_dispatcher. h265_SAD_16x16_8u =  &MFX_HEVC_PP::SAD_16x16_sse;
        g_dispatcher. h265_SAD_16x32_8u =  &MFX_HEVC_PP::SAD_16x32_sse;
        g_dispatcher. h265_SAD_16x64_8u =  &MFX_HEVC_PP::SAD_16x64_sse;

        g_dispatcher. h265_SAD_24x32_8u =  &MFX_HEVC_PP::SAD_24x32_sse;

        g_dispatcher. h265_SAD_32x8_8u =  &MFX_HEVC_PP::SAD_32x8_sse;
        g_dispatcher. h265_SAD_32x16_8u =  &MFX_HEVC_PP::SAD_32x16_sse;
        g_dispatcher. h265_SAD_32x24_8u =  &MFX_HEVC_PP::SAD_32x24_sse;
        g_dispatcher. h265_SAD_32x32_8u =  &MFX_HEVC_PP::SAD_32x32_sse;
        g_dispatcher. h265_SAD_32x64_8u =  &MFX_HEVC_PP::SAD_32x64_sse;

        g_dispatcher. h265_SAD_48x64_8u =  &MFX_HEVC_PP::SAD_48x64_sse;

        g_dispatcher. h265_SAD_64x16_8u =  &MFX_HEVC_PP::SAD_64x16_sse;
        g_dispatcher. h265_SAD_64x32_8u =  &MFX_HEVC_PP::SAD_64x32_sse;
        g_dispatcher. h265_SAD_64x48_8u =  &MFX_HEVC_PP::SAD_64x48_sse;
        g_dispatcher. h265_SAD_64x64_8u =  &MFX_HEVC_PP::SAD_64x64_sse;

        // [Sad.general]===================================
        g_dispatcher. h265_SAD_4x4_general_8u =  &MFX_HEVC_PP::SAD_4x4_general_sse;
        g_dispatcher. h265_SAD_4x8_general_8u =  &MFX_HEVC_PP::SAD_4x8_general_sse;
        g_dispatcher. h265_SAD_4x16_general_8u =  &MFX_HEVC_PP::SAD_4x16_general_sse;

        g_dispatcher. h265_SAD_8x4_general_8u =  &MFX_HEVC_PP::SAD_8x4_general_sse;
        g_dispatcher. h265_SAD_8x8_general_8u =  &MFX_HEVC_PP::SAD_8x8_general_sse;
        g_dispatcher. h265_SAD_8x16_general_8u =  &MFX_HEVC_PP::SAD_8x16_general_sse;
        g_dispatcher. h265_SAD_8x32_general_8u =  &MFX_HEVC_PP::SAD_8x32_general_sse;

        g_dispatcher. h265_SAD_12x16_general_8u =  &MFX_HEVC_PP::SAD_12x16_general_sse;

        g_dispatcher. h265_SAD_16x4_general_8u =  &MFX_HEVC_PP::SAD_16x4_general_sse;
        g_dispatcher. h265_SAD_16x8_general_8u =  &MFX_HEVC_PP::SAD_16x8_general_sse;
        g_dispatcher. h265_SAD_16x12_general_8u =  &MFX_HEVC_PP::SAD_16x12_general_sse;
        g_dispatcher. h265_SAD_16x16_general_8u =  &MFX_HEVC_PP::SAD_16x16_general_sse;
        g_dispatcher. h265_SAD_16x32_general_8u =  &MFX_HEVC_PP::SAD_16x32_general_sse;
        g_dispatcher. h265_SAD_16x64_general_8u =  &MFX_HEVC_PP::SAD_16x64_general_sse;

        g_dispatcher. h265_SAD_24x32_general_8u =  &MFX_HEVC_PP::SAD_24x32_general_sse;

        g_dispatcher. h265_SAD_32x8_general_8u =  &MFX_HEVC_PP::SAD_32x8_general_sse;
        g_dispatcher. h265_SAD_32x16_general_8u =  &MFX_HEVC_PP::SAD_32x16_general_sse;
        g_dispatcher. h265_SAD_32x24_general_8u =  &MFX_HEVC_PP::SAD_32x24_general_sse;
        g_dispatcher. h265_SAD_32x32_general_8u =  &MFX_HEVC_PP::SAD_32x32_general_sse;
        g_dispatcher. h265_SAD_32x64_general_8u =  &MFX_HEVC_PP::SAD_32x64_general_sse;

        g_dispatcher. h265_SAD_48x64_general_8u =  &MFX_HEVC_PP::SAD_48x64_general_sse;

        g_dispatcher. h265_SAD_64x16_general_8u =  &MFX_HEVC_PP::SAD_64x16_general_sse;
        g_dispatcher. h265_SAD_64x32_general_8u =  &MFX_HEVC_PP::SAD_64x32_general_sse;
        g_dispatcher. h265_SAD_64x48_general_8u =  &MFX_HEVC_PP::SAD_64x48_general_sse;
        g_dispatcher. h265_SAD_64x64_general_8u =  &MFX_HEVC_PP::SAD_64x64_general_sse;

        //[transform.inv]==================================
        g_dispatcher. h265_DST4x4Inv_16sT = &MFX_HEVC_PP::h265_DST4x4Inv_16sT_sse;
        g_dispatcher. h265_DCT4x4Inv_16sT = &MFX_HEVC_PP::h265_DCT4x4Inv_16sT_sse;
        g_dispatcher. h265_DCT8x8Inv_16sT = &MFX_HEVC_PP::h265_DCT8x8Inv_16sT_sse;
        g_dispatcher. h265_DCT16x16Inv_16sT = &MFX_HEVC_PP::h265_DCT16x16Inv_16sT_sse;
        g_dispatcher. h265_DCT32x32Inv_16sT = &MFX_HEVC_PP::h265_DCT32x32Inv_16sT_sse;

        //[transform.fwd]==================================
        g_dispatcher. h265_DST4x4Fwd_16s = &MFX_HEVC_PP::h265_DST4x4Fwd_16s_sse;
        g_dispatcher. h265_DCT4x4Fwd_16s = &MFX_HEVC_PP::h265_DCT4x4Fwd_16s_sse;
        g_dispatcher. h265_DCT8x8Fwd_16s = &MFX_HEVC_PP::h265_DCT8x8Fwd_16s_sse;
        g_dispatcher. h265_DCT16x16Fwd_16s = &MFX_HEVC_PP::h265_DCT16x16Fwd_16s_sse;
        g_dispatcher. h265_DCT32x32Fwd_16s = &MFX_HEVC_PP::h265_DCT32x32Fwd_16s_sse;

        //[deblocking]=====================================
        g_dispatcher. h265_FilterEdgeLuma_8u_I = &MFX_HEVC_PP::h265_FilterEdgeLuma_8u_I_sse;

        //[SAO]============================================
        g_dispatcher. h265_ProcessSaoCuOrg_Luma_8u = &MFX_HEVC_PP::h265_ProcessSaoCuOrg_Luma_8u_sse;
        g_dispatcher. h265_ProcessSaoCu_Luma_8u = &MFX_HEVC_PP::h265_ProcessSaoCu_Luma_8u_sse;

        //[Interpoaltion]==================================
        // average
        g_dispatcher.h265_AverageModeB = &MFX_HEVC_PP::h265_AverageModeB_sse;
        g_dispatcher.h265_AverageModeP = &MFX_HEVC_PP::h265_AverageModeP_sse;
        g_dispatcher.h265_AverageModeN = &MFX_HEVC_PP::h265_AverageModeN_sse;        
        
        // algo
        g_dispatcher.h265_InterpLuma_s8_d16_H = &MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_sse;
        g_dispatcher.h265_InterpChroma_s8_d16_H = &MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_sse;
        g_dispatcher.h265_InterpLuma_s8_d16_V = &MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_sse;
        g_dispatcher.h265_InterpChroma_s8_d16_V = &MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_sse;
        g_dispatcher.h265_InterpLuma_s16_d16_V = &MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_sse;
        g_dispatcher.h265_InterpChroma_s16_d16_V = &MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_sse;

        // [INTRA prediction]
        g_dispatcher.h265_PredictIntra_Ang_8u = &MFX_HEVC_PP::h265_PredictIntra_Ang_8u_sse;

    } // void SetTargetSSE4(void)


    void SetTargetAVX2(void)
    {
        // [Sad.special]===================================
        g_dispatcher. h265_SAD_4x4_8u =  &MFX_HEVC_PP::SAD_4x4_avx2;
        g_dispatcher. h265_SAD_4x8_8u =  &MFX_HEVC_PP::SAD_4x8_avx2;
        g_dispatcher. h265_SAD_4x16_8u =  &MFX_HEVC_PP::SAD_4x16_avx2;

        g_dispatcher. h265_SAD_8x4_8u =  &MFX_HEVC_PP::SAD_8x4_avx2;
        g_dispatcher. h265_SAD_8x8_8u =  &MFX_HEVC_PP::SAD_8x8_avx2;
        g_dispatcher. h265_SAD_8x16_8u =  &MFX_HEVC_PP::SAD_8x16_avx2;
        g_dispatcher. h265_SAD_8x32_8u =  &MFX_HEVC_PP::SAD_8x32_avx2;

        g_dispatcher. h265_SAD_12x16_8u =  &MFX_HEVC_PP::SAD_12x16_avx2;

        g_dispatcher. h265_SAD_16x4_8u =  &MFX_HEVC_PP::SAD_16x4_avx2;
        g_dispatcher. h265_SAD_16x8_8u =  &MFX_HEVC_PP::SAD_16x8_avx2;
        g_dispatcher. h265_SAD_16x12_8u =  &MFX_HEVC_PP::SAD_16x12_avx2;
        g_dispatcher. h265_SAD_16x16_8u =  &MFX_HEVC_PP::SAD_16x16_avx2;
        g_dispatcher. h265_SAD_16x32_8u =  &MFX_HEVC_PP::SAD_16x32_avx2;
        g_dispatcher. h265_SAD_16x64_8u =  &MFX_HEVC_PP::SAD_16x64_avx2;

        g_dispatcher. h265_SAD_24x32_8u =  &MFX_HEVC_PP::SAD_24x32_avx2;

        g_dispatcher. h265_SAD_32x8_8u =  &MFX_HEVC_PP::SAD_32x8_avx2;
        g_dispatcher. h265_SAD_32x16_8u =  &MFX_HEVC_PP::SAD_32x16_avx2;
        g_dispatcher. h265_SAD_32x24_8u =  &MFX_HEVC_PP::SAD_32x24_avx2;
        g_dispatcher. h265_SAD_32x32_8u =  &MFX_HEVC_PP::SAD_32x32_avx2;
        g_dispatcher. h265_SAD_32x64_8u =  &MFX_HEVC_PP::SAD_32x64_avx2;

        g_dispatcher. h265_SAD_48x64_8u =  &MFX_HEVC_PP::SAD_48x64_avx2;

        g_dispatcher. h265_SAD_64x16_8u =  &MFX_HEVC_PP::SAD_64x16_avx2;
        g_dispatcher. h265_SAD_64x32_8u =  &MFX_HEVC_PP::SAD_64x32_avx2;
        g_dispatcher. h265_SAD_64x48_8u =  &MFX_HEVC_PP::SAD_64x48_avx2;
        g_dispatcher. h265_SAD_64x64_8u =  &MFX_HEVC_PP::SAD_64x64_avx2;

        // [Sad.general]===================================
        g_dispatcher. h265_SAD_4x4_general_8u =  &MFX_HEVC_PP::SAD_4x4_general_avx2;
        g_dispatcher. h265_SAD_4x8_general_8u =  &MFX_HEVC_PP::SAD_4x8_general_avx2;
        g_dispatcher. h265_SAD_4x16_general_8u =  &MFX_HEVC_PP::SAD_4x16_general_avx2;

        g_dispatcher. h265_SAD_8x4_general_8u =  &MFX_HEVC_PP::SAD_8x4_general_avx2;
        g_dispatcher. h265_SAD_8x8_general_8u =  &MFX_HEVC_PP::SAD_8x8_general_avx2;
        g_dispatcher. h265_SAD_8x16_general_8u =  &MFX_HEVC_PP::SAD_8x16_general_avx2;
        g_dispatcher. h265_SAD_8x32_general_8u =  &MFX_HEVC_PP::SAD_8x32_general_avx2;

        g_dispatcher. h265_SAD_12x16_general_8u =  &MFX_HEVC_PP::SAD_12x16_general_avx2;

        g_dispatcher. h265_SAD_16x4_general_8u =  &MFX_HEVC_PP::SAD_16x4_general_avx2;
        g_dispatcher. h265_SAD_16x8_general_8u =  &MFX_HEVC_PP::SAD_16x8_general_avx2;
        g_dispatcher. h265_SAD_16x12_general_8u =  &MFX_HEVC_PP::SAD_16x12_general_avx2;
        g_dispatcher. h265_SAD_16x16_general_8u =  &MFX_HEVC_PP::SAD_16x16_general_avx2;
        g_dispatcher. h265_SAD_16x32_general_8u =  &MFX_HEVC_PP::SAD_16x32_general_avx2;
        g_dispatcher. h265_SAD_16x64_general_8u =  &MFX_HEVC_PP::SAD_16x64_general_avx2;

        g_dispatcher. h265_SAD_24x32_general_8u =  &MFX_HEVC_PP::SAD_24x32_general_avx2;

        g_dispatcher. h265_SAD_32x8_general_8u =  &MFX_HEVC_PP::SAD_32x8_general_avx2;
        g_dispatcher. h265_SAD_32x16_general_8u =  &MFX_HEVC_PP::SAD_32x16_general_avx2;
        g_dispatcher. h265_SAD_32x24_general_8u =  &MFX_HEVC_PP::SAD_32x24_general_avx2;
        g_dispatcher. h265_SAD_32x32_general_8u =  &MFX_HEVC_PP::SAD_32x32_general_avx2;
        g_dispatcher. h265_SAD_32x64_general_8u =  &MFX_HEVC_PP::SAD_32x64_general_avx2;

        g_dispatcher. h265_SAD_48x64_general_8u =  &MFX_HEVC_PP::SAD_48x64_general_avx2;

        g_dispatcher. h265_SAD_64x16_general_8u =  &MFX_HEVC_PP::SAD_64x16_general_avx2;
        g_dispatcher. h265_SAD_64x32_general_8u =  &MFX_HEVC_PP::SAD_64x32_general_avx2;
        g_dispatcher. h265_SAD_64x48_general_8u =  &MFX_HEVC_PP::SAD_64x48_general_avx2;
        g_dispatcher. h265_SAD_64x64_general_8u =  &MFX_HEVC_PP::SAD_64x64_general_avx2;

        //[transform.inv]==================================
        g_dispatcher. h265_DST4x4Inv_16sT = &MFX_HEVC_PP::h265_DST4x4Inv_16sT_avx2;
        g_dispatcher. h265_DCT4x4Inv_16sT = &MFX_HEVC_PP::h265_DCT4x4Inv_16sT_avx2;
        g_dispatcher. h265_DCT8x8Inv_16sT = &MFX_HEVC_PP::h265_DCT8x8Inv_16sT_avx2;
        g_dispatcher. h265_DCT16x16Inv_16sT = &MFX_HEVC_PP::h265_DCT16x16Inv_16sT_avx2;
        g_dispatcher. h265_DCT32x32Inv_16sT = &MFX_HEVC_PP::h265_DCT32x32Inv_16sT_avx2;

        //[transform.fwd]==================================
        g_dispatcher. h265_DST4x4Fwd_16s = &MFX_HEVC_PP::h265_DST4x4Fwd_16s_sse;
        g_dispatcher. h265_DCT4x4Fwd_16s = &MFX_HEVC_PP::h265_DCT4x4Fwd_16s_sse;
        g_dispatcher. h265_DCT8x8Fwd_16s = &MFX_HEVC_PP::h265_DCT8x8Fwd_16s_sse;
        g_dispatcher. h265_DCT16x16Fwd_16s = &MFX_HEVC_PP::h265_DCT16x16Fwd_16s_sse;
        g_dispatcher. h265_DCT32x32Fwd_16s = &MFX_HEVC_PP::h265_DCT32x32Fwd_16s_sse;

        //[deblocking]=====================================
        g_dispatcher. h265_FilterEdgeLuma_8u_I = &MFX_HEVC_PP::h265_FilterEdgeLuma_8u_I_sse;

        //[SAO]============================================
        g_dispatcher. h265_ProcessSaoCuOrg_Luma_8u = &MFX_HEVC_PP::h265_ProcessSaoCuOrg_Luma_8u_sse;
        g_dispatcher. h265_ProcessSaoCu_Luma_8u = &MFX_HEVC_PP::h265_ProcessSaoCu_Luma_8u_sse;

        //[Interpoaltion]==================================
        // NIY

    } // void SetTargetAVX2(void)


    void SetTargetPX(void)
    {

    } // void SetTargetPX(void)

#else // PREDEFINED_TARGET_OPTIMIZATION (PX/SSE4/AVX2)
    IppStatus MFX_HEVC_PP::InitDispatcher( void )
    {
        // Nothing 

        return ippStsNoErr;

    } // IppStatus InitDispatcher( void )
#endif
    // COMMON CASE
    static enum EnumPlane
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
    void MFX_HEVC_PP::Interp_S8_NoAvg(
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
        int plane)
    {
        VM_ASSERT( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

        if (plane == TEXT_LUMA) {
            if (dir == INTERP_HOR)
                NAME(h265_InterpLuma_s8_d16_H)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
            else
                NAME(h265_InterpLuma_s8_d16_V)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
        } else {
            if (dir == INTERP_HOR)
                NAME(h265_InterpChroma_s8_d16_H)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset, plane);
            else
                NAME(h265_InterpChroma_s8_d16_V)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
        }
    }

    /* typically used for ~15% of total pixels, does vertical filter pass only */
    void MFX_HEVC_PP::Interp_S16_NoAvg(
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
        int plane)
    {
        VM_ASSERT( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

        /* only V is supported/needed */
        if (dir != INTERP_VER)
            return;

        if (plane == TEXT_LUMA)
            NAME(h265_InterpLuma_s16_d16_V)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
        else
            NAME(h265_InterpChroma_s16_d16_V)(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
    }

    /* NOTE: average functions assume maximum block size of 64x64, including 7 extra output rows for H pass */

    /* typically used for ~15% of total pixels, does first-pass horizontal or vertical filter, optionally with average against reference */
    void MFX_HEVC_PP::Interp_S8_WithAvg(
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
        int plane)
    {
        /* pretty big stack buffer, probably want to pass this in (allocate on heap) */
        ALIGN_DECL(16) short tmpBuf[(64+8)*(64)];

        VM_ASSERT( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

        if (plane == TEXT_LUMA) {
            if (dir == INTERP_HOR)
                NAME(h265_InterpLuma_s8_d16_H)(pSrc, srcPitch, tmpBuf, 64, tab_index, width, height, shift, offset);
            else
                NAME(h265_InterpLuma_s8_d16_V)(pSrc, srcPitch, tmpBuf, 64, tab_index, width, height, shift, offset);
        } else {
            if (dir == INTERP_HOR)
                NAME(h265_InterpChroma_s8_d16_H)(pSrc, srcPitch, tmpBuf, 64, tab_index, width, height, shift, offset, plane);
            else
                NAME(h265_InterpChroma_s8_d16_V)(pSrc, srcPitch, tmpBuf, 64, tab_index, width, height, shift, offset);
        }

        if (avgMode == AVERAGE_NO)
            NAME(h265_AverageModeN)(tmpBuf, 64, pDst, dstPitch, width, height);
        else if (avgMode == AVERAGE_FROM_PIC)
            NAME(h265_AverageModeP)(tmpBuf, 64, (unsigned char *)pvAvg, avgPitch, pDst, dstPitch, width, height);
        else if (avgMode == AVERAGE_FROM_BUF)
            NAME(h265_AverageModeB)(tmpBuf, 64, (short *)pvAvg, avgPitch, pDst, dstPitch, width, height);
    }


    /* typically used for ~20% of total pixels, does second-pass vertical filter only, optionally with average against reference */
    void MFX_HEVC_PP::Interp_S16_WithAvg(
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
        int plane)
    {
        /* pretty big stack buffer, probably want to pass this in (allocate on heap) */
        ALIGN_DECL(16) short tmpBuf[(64+8)*(64)];

        /* only V is supported/needed */
        if (dir != INTERP_VER)
            return;

        VM_ASSERT( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

        if (plane == TEXT_LUMA)
            NAME(h265_InterpLuma_s16_d16_V)(pSrc, srcPitch, tmpBuf, 64, tab_index, width, height, shift, offset);
        else
            NAME(h265_InterpChroma_s16_d16_V)(pSrc, srcPitch, tmpBuf, 64, tab_index, width, height, shift, offset);

        if (avgMode == AVERAGE_NO)
            NAME(h265_AverageModeN)(tmpBuf, 64, pDst, dstPitch, width, height);
        else if (avgMode == AVERAGE_FROM_PIC)
            NAME(h265_AverageModeP)(tmpBuf, 64, (unsigned char *)pvAvg, avgPitch, pDst, dstPitch, width, height);
        else if (avgMode == AVERAGE_FROM_BUF)
            NAME(h265_AverageModeB)(tmpBuf, 64, (short *)pvAvg, avgPitch, pDst, dstPitch, width, height);
    }

//} // namespace MFX_HEVC_PP

//#endif // #if defined (MFX_TARGET_OPTIMIZATION_PX) || (MFX_TARGET_OPTIMIZATION_SSE4) || (MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
/* EOF */