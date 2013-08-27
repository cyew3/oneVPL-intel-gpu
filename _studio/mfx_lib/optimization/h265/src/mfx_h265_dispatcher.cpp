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
        // NIY

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

#else
    IppStatus MFX_HEVC_PP::InitDispatcher( void )
    {
        // Nothing 

        return ippStsNoErr;

    } // IppStatus InitDispatcher( void )
#endif

//} // namespace MFX_HEVC_PP

//#endif // #if defined (MFX_TARGET_OPTIMIZATION_PX) || (MFX_TARGET_OPTIMIZATION_SSE4) || (MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
/* EOF */