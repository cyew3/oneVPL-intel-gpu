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

// Forward HEVC Transform functions optimised by intrinsics
// Sizes: 4, 8, 16, 32
// NOTE: we don't have AVX2 optimization, so the same SSE2(4) impl is used

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

namespace MFX_HEVC_PP
{

#define ALIGNED_SSE2 ALIGN_DECL(16)

// koefs for horizontal 1-D transform
ALIGNED_SSE2 signed int rounder_8[]   = {8, 8, 8, 8};
ALIGNED_SSE2 signed int rounder_16[]   = {16, 16, 16, 16};
ALIGNED_SSE2 signed int rounder_32[]   = {32, 32, 32, 32};
ALIGNED_SSE2 unsigned char kfh_shuff[] = {14,15,12,13,10,11,8,9,6,7,4,5,2,3,0,1};

ALIGNED_SSE2 signed short kfh_0000[]  = {90, 90,  90, 82,  88, 67,  85, 46};
ALIGNED_SSE2 signed short kfh_0001[]  = {82, 22,  78, -4,  73,-31,  67,-54};
ALIGNED_SSE2 signed short kfh_0002[]  = {88, 85,  67, 46,  31,-13, -13,-67};
ALIGNED_SSE2 signed short kfh_0003[]  ={-54,-90, -82,-73, -90,-22, -78, 38};
ALIGNED_SSE2 signed short kfh_0004[]  = {82, 78,  22, -4, -54,-82, -90,-73};
ALIGNED_SSE2 signed short kfh_0005[]  ={-61, 13,  13, 85,  78, 67,  85,-22};
ALIGNED_SSE2 signed short kfh_0006[]  = {73, 67, -31,-54, -90,-78, -22, 38};
ALIGNED_SSE2 signed short kfh_0007[]  = {78, 85,  67,-22, -38,-90, -90,  4};
ALIGNED_SSE2 signed short kfh_0008[]  = {61, 54, -73,-85, -46, -4,  82, 88};
ALIGNED_SSE2 signed short kfh_0009[]  = {31,-46, -88,-61, -13, 82,  90, 13};
ALIGNED_SSE2 signed short kfh_0010[]  = {46, 38, -90,-88,  38, 73,  54, -4};
ALIGNED_SSE2 signed short kfh_0011[]  ={-90,-67,  31, 90,  61,-46, -88,-31};
ALIGNED_SSE2 signed short kfh_0012[]  = {31, 22, -78,-61,  90, 85, -61,-90};
ALIGNED_SSE2 signed short kfh_0013[]  = { 4, 73,  54,-38, -88, -4,  82, 46};
ALIGNED_SSE2 signed short kfh_0014[]  = {13,  4, -38,-13,  61, 22, -78,-31};
ALIGNED_SSE2 signed short kfh_0015[]  = {88, 38, -90,-46,  85, 54, -73,-61};

ALIGNED_SSE2 signed short kfh_0100[]  = {61,-73,  54,-85,  46,-90,  38,-88};
ALIGNED_SSE2 signed short kfh_0101[]  = {31,-78,  22,-61,  13,-38,   4,-13};
ALIGNED_SSE2 signed short kfh_0102[]  ={-46, 82,  -4, 88,  38, 54,  73, -4};
ALIGNED_SSE2 signed short kfh_0103[]  = {90,-61,  85,-90,  61,-78,  22,-31};
ALIGNED_SSE2 signed short kfh_0104[]  = {31,-88, -46,-61, -90, 31, -67, 90};
ALIGNED_SSE2 signed short kfh_0105[]  = { 4, 54,  73,-38,  88,-90,  38,-46};
ALIGNED_SSE2 signed short kfh_0106[]  ={-13, 90,  82, 13,  61,-88, -46,-31};
ALIGNED_SSE2 signed short kfh_0107[]  ={-88, 82,  -4, 46,  85,-73,  54,-61};
ALIGNED_SSE2 signed short kfh_0108[]  ={ -4,-90, -90, 38,  22, 67,  85,-78};
ALIGNED_SSE2 signed short kfh_0109[]  ={-38,-22, -78, 90,  54,-31,  67,-73};
ALIGNED_SSE2 signed short kfh_0110[]  = {22, 85,  67,-78, -85, 13,  13, 61};
ALIGNED_SSE2 signed short kfh_0111[]  = {73,-90, -82, 54,   4, 22,  78,-82};
ALIGNED_SSE2 signed short kfh_0112[]  ={-38,-78, -22, 90,  73,-82, -90, 54};
ALIGNED_SSE2 signed short kfh_0113[]  = {67,-13, -13,-31, -46, 67,  85,-88};
ALIGNED_SSE2 signed short kfh_0114[]  = {54, 67, -31,-73,   4, 78,  22,-82};
ALIGNED_SSE2 signed short kfh_0115[]  ={-46, 85,  67,-88, -82, 90,  90,-90};

ALIGNED_SSE2 signed short kfh_0200[]  = {90, 87,  87, 57,  80,  9,  70,-43};
ALIGNED_SSE2 signed short kfh_0201[]  = {57,-80,  43,-90,  25,-70,   9,-25};
ALIGNED_SSE2 signed short kfh_0202[]  = {80, 70,   9,-43, -70,-87, -87,  9};
ALIGNED_SSE2 signed short kfh_0203[]  ={-25, 90,  57, 25,  90,-80,  43,-57};
ALIGNED_SSE2 signed short kfh_0204[]  = {57, 43, -80,-90, -25, 57,  90, 25};
ALIGNED_SSE2 signed short kfh_0205[]  ={ -9,-87, -87, 70,  43,  9,  70,-80};
ALIGNED_SSE2 signed short kfh_0206[]  = {25,  9, -70,-25,  90, 43, -80,-57};
ALIGNED_SSE2 signed short kfh_0207[]  = {43, 70,   9,-80, -57, 87,  87,-90};

ALIGNED_SSE2 signed short kfh_0300[]  = {89, 75,  75,-18,  50,-89,  18,-50};
ALIGNED_SSE2 signed short kfh_0301[]  = {50, 18, -89,-50,  18, 75,  75,-89};
ALIGNED_SSE2 signed short kfh_0400[]  = {64, 64,  64,-64,  83, 36,  36,-83};

// koefs for vertical 1-D transform
ALIGNED_SSE2 signed int rounder_1k[]  = {1024, 1024, 1024, 1024};

ALIGNED_SSE2 signed short kfv_0000[]  = {90, 90,  67, 46, -54,-82, -22, 38};
ALIGNED_SSE2 signed short kfv_0001[]  = {82, 22, -82,-73,  78, 67, -90,  4};
ALIGNED_SSE2 signed short kfv_0002[]  = {88, 85,  22, -4, -90,-78,  85, 46};
ALIGNED_SSE2 signed short kfv_0003[]  ={-54,-90,  13, 85, -38,-90,  67,-54};
ALIGNED_SSE2 signed short kfv_0004[]  = {82, 78, -31,-54,  88, 67, -13,-67};
ALIGNED_SSE2 signed short kfv_0005[]  ={-61, 13,  67,-22,  73,-31, -78, 38};
ALIGNED_SSE2 signed short kfv_0006[]  = {73, 67,  90, 82,  31,-13, -90,-73};
ALIGNED_SSE2 signed short kfv_0007[]  = {78, 85,  78, -4, -90,-22,  85,-22};
ALIGNED_SSE2 signed short kfv_0008[]  = {61, 54, -90,-88,  90, 85, -78,-31};
ALIGNED_SSE2 signed short kfv_0009[]  = {31,-46,  31, 90, -88, -4, -73,-61};
ALIGNED_SSE2 signed short kfv_0010[]  = {46, 38, -78,-61,  61, 22,  82, 88};
ALIGNED_SSE2 signed short kfv_0011[]  ={-90,-67,  54,-38,  85, 54,  90, 13};
ALIGNED_SSE2 signed short kfv_0012[]  = {31, 22, -38,-13, -46, -4,  54, -4};
ALIGNED_SSE2 signed short kfv_0013[]  = { 4, 73, -90,-46, -13, 82, -88,-31};
ALIGNED_SSE2 signed short kfv_0014[]  = {13,  4, -73,-85,  38, 73, -61,-90};
ALIGNED_SSE2 signed short kfv_0015[]  = {88, 38, -88,-61,  61,-46,  82, 46};

ALIGNED_SSE2 signed short kfv_0100[]  = {61,-73,  -4, 88, -90, 31, -46,-31};
ALIGNED_SSE2 signed short kfv_0101[]  = {31,-78,  85,-90,  88,-90,  54,-61};
ALIGNED_SSE2 signed short kfv_0102[]  ={-46, 82, -46,-61,  61,-88,  38,-88};
ALIGNED_SSE2 signed short kfv_0103[]  = {90,-61,  73,-38,  85,-73,   4,-13};
ALIGNED_SSE2 signed short kfv_0104[]  = {31,-88,  82, 13,  46,-90,  73, -4};
ALIGNED_SSE2 signed short kfv_0105[]  = { 4, 54,  -4, 46,  13,-38,  22,-31};
ALIGNED_SSE2 signed short kfv_0106[]  ={-13, 90,  54,-85,  38, 54, -67, 90};
ALIGNED_SSE2 signed short kfv_0107[]  ={-88, 82,  22,-61,  61,-78,  38,-46};
ALIGNED_SSE2 signed short kfv_0108[]  ={ -4,-90,  67,-78,  73,-82,  22,-82};
ALIGNED_SSE2 signed short kfv_0109[]  ={-38,-22, -82, 54, -46, 67,  90,-90};
ALIGNED_SSE2 signed short kfv_0110[]  = {22, 85, -22, 90,   4, 78,  85,-78};
ALIGNED_SSE2 signed short kfv_0111[]  = {73,-90, -13,-31, -82, 90,  67,-73};
ALIGNED_SSE2 signed short kfv_0112[]  ={-38,-78, -31,-73,  22, 67,  13, 61};
ALIGNED_SSE2 signed short kfv_0113[]  = {67,-13,  67,-88,  54,-31,  78,-82};
ALIGNED_SSE2 signed short kfv_0114[]  = {54, 67, -90, 38, -85, 13, -90, 54};
ALIGNED_SSE2 signed short kfv_0115[]  ={-46, 85, -78, 90,   4, 22,  85,-88};

ALIGNED_SSE2 signed int kfv_0200[] = {90,  57, -70,   9};
ALIGNED_SSE2 signed int kfv_0201[] = {57, -90,  90, -57};
ALIGNED_SSE2 signed int kfv_0202[] = {57, -90,  90, -57};
ALIGNED_SSE2 signed int kfv_0203[] ={ -9,  70, -57, -90};
ALIGNED_SSE2 signed int kfv_0204[] = {87,   9, -87,  70};
ALIGNED_SSE2 signed int kfv_0205[] = {43, -70,  43,  90};
ALIGNED_SSE2 signed int kfv_0206[] ={-80,  57, -80,   9};
ALIGNED_SSE2 signed int kfv_0207[] ={-87,   9,  87,  70};
ALIGNED_SSE2 signed int kfv_0208[] = {80, -43,  80, -43};
ALIGNED_SSE2 signed int kfv_0209[] = {25, -25, -25,  25};
ALIGNED_SSE2 signed int kfv_0210[] ={-25,  25,  25, -25};
ALIGNED_SSE2 signed int kfv_0211[] = {43, -80,  43, -80};
ALIGNED_SSE2 signed int kfv_0212[] = {70,  87,   9, -87};
ALIGNED_SSE2 signed int kfv_0213[] = { 9, -80,  57, -80};
ALIGNED_SSE2 signed int kfv_0214[] = {90,  43, -70,  43};
ALIGNED_SSE2 signed int kfv_0215[] = {70, -87,   9,  87};

ALIGNED_SSE2 signed int kfv_0300[] = {64,  36, -64, -36};
ALIGNED_SSE2 signed int kfv_0301[] = {89, -18,  18, -89};
ALIGNED_SSE2 signed int kfv_0302[] = {64, -36,  64,  36};
ALIGNED_SSE2 signed int kfv_0303[] = {75, -89,  75,  18};
ALIGNED_SSE2 signed int kfv_0304[] = {64, -83,  64, -83};
ALIGNED_SSE2 signed int kfv_0305[] = {50, -50,  50, -50};
ALIGNED_SSE2 signed int kfv_0306[] = {64,  83, -64,  83};
ALIGNED_SSE2 signed int kfv_0307[] = {18,  75, -89,  75};

    // 32-bits constants
    signed int rounder_64[]   = {64, 64, 0, 0, 64, 64, 0, 0};
    signed int rounder_2048[] = {2048, 2048, 0, 0, 2048, 2048, 0, 0};
    signed int rounder_1024[] = {1024, 1024, 0, 0, 1024, 1024, 0, 0};
    signed int rounder_512[] = {512, 512, 0, 0, 512, 512, 0, 0};

    // 16-bits constants
    signed short koef0000[]  = {64, 64,  64,-64,  83, 36,  36,-83};
    signed short koef0001[]  = {89, 75,  75,-18,  50,-89,  18,-50};
    signed short koef0002[]  = {50, 18, -89,-50,  18, 75,  75,-89};
    signed short koef0003[]  = {90, 87,  87, 57,  80,  9,  70,-43};
    signed short koef0004[]  = {57,-80,  43,-90,  25,-70,   9,-25};
    signed short koef0005[]  = {80, 70,   9,-43, -70,-87, -87,  9};
    signed short koef0006[]  ={-25, 90,  57, 25,  90,-80,  43,-57};
    signed short koef0007[]  = {57, 43, -80,-90, -25, 57,  90, 25};
    signed short koef0008[]  = {-9,-87, -87, 70,  43,  9,  70,-80};
    signed short koef0009[]  = {25,  9, -70,-25,  90, 43, -80,-57};
    signed short koef0010[]  = {43, 70,   9,-80, -57, 87,  87,-90};
    signed short koef00010[] = {90, 90,  90, 82,  88, 67,  85, 46};
    signed short koef02030[] = {88, 85,  67, 46,  31,-13, -13,-67};
    signed short koef04050[] = {82, 78,  22, -4, -54,-82, -90,-73};
    signed short koef06070[] = {73, 67, -31,-54, -90,-78, -22, 38};
    signed short koef08090[] = {61, 54, -73,-85, -46, -4,  82, 88};
    signed short koef10110[] = {46, 38, -90,-88,  38, 73,  54, -4};
    signed short koef12130[] = {31, 22, -78,-61,  90, 85, -61,-90};
    signed short koef14150[] = {13,  4, -38,-13,  61, 22, -78,-31};
    signed short koef00011[] = {82, 22,  78, -4,  73,-31,  67,-54};
    signed short koef02031[] ={-54,-90, -82,-73, -90,-22, -78, 38};
    signed short koef04051[] ={-61, 13,  13, 85,  78, 67,  85,-22};
    signed short koef06071[] = {78, 85,  67,-22, -38,-90, -90,  4};
    signed short koef08091[] = {31,-46, -88,-61, -13, 82,  90, 13};
    signed short koef10111[] ={-90,-67,  31, 90,  61,-46, -88,-31};
    signed short koef12131[] = { 4, 73,  54,-38, -88, -4,  82, 46};
    signed short koef14151[] = {88, 38, -90,-46,  85, 54, -73,-61};
    signed short koef00012[] = {61,-73,  54,-85,  46,-90,  38,-88};
    signed short koef00013[] = {31,-78,  22,-61,  13,-38,   4,-13};
    signed short koef02032[] ={-46, 82,  -4, 88,  38, 54,  73, -4};
    signed short koef02033[] = {90,-61,  85,-90,  61,-78,  22,-31};
    signed short koef04052[] = {31,-88, -46,-61, -90, 31, -67, 90};
    signed short koef04053[] = { 4, 54,  73,-38,  88,-90,  38,-46};
    signed short koef06072[] ={-13, 90,  82, 13,  61,-88, -46,-31};
    signed short koef06073[] ={-88, 82,  -4, 46,  85,-73,  54,-61};
    signed short koef08092[] = {-4,-90, -90, 38,  22, 67,  85,-78};
    signed short koef08093[] ={-38,-22, -78, 90,  54,-31,  67,-73};
    signed short koef10112[] = {22, 85,  67,-78, -85, 13,  13, 61};
    signed short koef10113[] = {73,-90, -82, 54,   4, 22,  78,-82};
    signed short koef12132[] ={-38,-78, -22, 90,  73,-82, -90, 54}; 
    signed short koef12133[] = {67,-13, -13,-31, -46, 67,  85,-88};
    signed short koef14152[] = {54, 67, -31,-73,   4, 78,  22,-82};
    signed short koef14153[] ={-46, 85,  67,-88, -82, 90,  90,-90};

    unsigned int repos[32] = 
    {0, 16,  8, 17, 4, 18,  9, 19, 
    2, 20, 10, 21, 5, 22, 11, 23, 
    1, 24, 12, 25, 6, 26, 13, 27,
    3, 28, 14, 29, 7, 30, 15, 31};
}

#endif