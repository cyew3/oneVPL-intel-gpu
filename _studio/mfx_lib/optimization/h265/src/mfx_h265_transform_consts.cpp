//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

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
extern ALIGNED_SSE2 const signed int rounder_8[4]   = {8, 8, 8, 8};
extern ALIGNED_SSE2 const signed int rounder_16[4]   = {16, 16, 16, 16};
extern ALIGNED_SSE2 const signed int rounder_32[4]   = {32, 32, 32, 32};
extern ALIGNED_SSE2 const unsigned char kfh_shuff[] = {14,15,12,13,10,11,8,9,6,7,4,5,2,3,0,1};

extern ALIGNED_SSE2 const signed short kfh_0000[8]  = {90, 90,  90, 82,  88, 67,  85, 46};
extern ALIGNED_SSE2 const signed short kfh_0001[8]  = {82, 22,  78, -4,  73,-31,  67,-54};
extern ALIGNED_SSE2 const signed short kfh_0002[8]  = {88, 85,  67, 46,  31,-13, -13,-67};
extern ALIGNED_SSE2 const signed short kfh_0003[8]  ={-54,-90, -82,-73, -90,-22, -78, 38};
extern ALIGNED_SSE2 const signed short kfh_0004[8]  = {82, 78,  22, -4, -54,-82, -90,-73};
extern ALIGNED_SSE2 const signed short kfh_0005[8]  ={-61, 13,  13, 85,  78, 67,  85,-22};
extern ALIGNED_SSE2 const signed short kfh_0006[8]  = {73, 67, -31,-54, -90,-78, -22, 38};
extern ALIGNED_SSE2 const signed short kfh_0007[8]  = {78, 85,  67,-22, -38,-90, -90,  4};
extern ALIGNED_SSE2 const signed short kfh_0008[8]  = {61, 54, -73,-85, -46, -4,  82, 88};
extern ALIGNED_SSE2 const signed short kfh_0009[8]  = {31,-46, -88,-61, -13, 82,  90, 13};
extern ALIGNED_SSE2 const signed short kfh_0010[8]  = {46, 38, -90,-88,  38, 73,  54, -4};
extern ALIGNED_SSE2 const signed short kfh_0011[8]  ={-90,-67,  31, 90,  61,-46, -88,-31};
extern ALIGNED_SSE2 const signed short kfh_0012[8]  = {31, 22, -78,-61,  90, 85, -61,-90};
extern ALIGNED_SSE2 const signed short kfh_0013[8]  = { 4, 73,  54,-38, -88, -4,  82, 46};
extern ALIGNED_SSE2 const signed short kfh_0014[8]  = {13,  4, -38,-13,  61, 22, -78,-31};
extern ALIGNED_SSE2 const signed short kfh_0015[8]  = {88, 38, -90,-46,  85, 54, -73,-61};

extern ALIGNED_SSE2 const signed short kfh_0100[8]  = {61,-73,  54,-85,  46,-90,  38,-88};
extern ALIGNED_SSE2 const signed short kfh_0101[8]  = {31,-78,  22,-61,  13,-38,   4,-13};
extern ALIGNED_SSE2 const signed short kfh_0102[8]  ={-46, 82,  -4, 88,  38, 54,  73, -4};
extern ALIGNED_SSE2 const signed short kfh_0103[8]  = {90,-61,  85,-90,  61,-78,  22,-31};
extern ALIGNED_SSE2 const signed short kfh_0104[8]  = {31,-88, -46,-61, -90, 31, -67, 90};
extern ALIGNED_SSE2 const signed short kfh_0105[8]  = { 4, 54,  73,-38,  88,-90,  38,-46};
extern ALIGNED_SSE2 const signed short kfh_0106[8]  ={-13, 90,  82, 13,  61,-88, -46,-31};
extern ALIGNED_SSE2 const signed short kfh_0107[8]  ={-88, 82,  -4, 46,  85,-73,  54,-61};
extern ALIGNED_SSE2 const signed short kfh_0108[8]  ={ -4,-90, -90, 38,  22, 67,  85,-78};
extern ALIGNED_SSE2 const signed short kfh_0109[8]  ={-38,-22, -78, 90,  54,-31,  67,-73};
extern ALIGNED_SSE2 const signed short kfh_0110[8]  = {22, 85,  67,-78, -85, 13,  13, 61};
extern ALIGNED_SSE2 const signed short kfh_0111[8]  = {73,-90, -82, 54,   4, 22,  78,-82};
extern ALIGNED_SSE2 const signed short kfh_0112[8]  ={-38,-78, -22, 90,  73,-82, -90, 54};
extern ALIGNED_SSE2 const signed short kfh_0113[8]  = {67,-13, -13,-31, -46, 67,  85,-88};
extern ALIGNED_SSE2 const signed short kfh_0114[8]  = {54, 67, -31,-73,   4, 78,  22,-82};
extern ALIGNED_SSE2 const signed short kfh_0115[8]  ={-46, 85,  67,-88, -82, 90,  90,-90};

extern ALIGNED_SSE2 const signed short kfh_0200[8]  = {90, 87,  87, 57,  80,  9,  70,-43};
extern ALIGNED_SSE2 const signed short kfh_0201[8]  = {57,-80,  43,-90,  25,-70,   9,-25};
extern ALIGNED_SSE2 const signed short kfh_0202[8]  = {80, 70,   9,-43, -70,-87, -87,  9};
extern ALIGNED_SSE2 const signed short kfh_0203[8]  ={-25, 90,  57, 25,  90,-80,  43,-57};
extern ALIGNED_SSE2 const signed short kfh_0204[8]  = {57, 43, -80,-90, -25, 57,  90, 25};
extern ALIGNED_SSE2 const signed short kfh_0205[8]  ={ -9,-87, -87, 70,  43,  9,  70,-80};
extern ALIGNED_SSE2 const signed short kfh_0206[8]  = {25,  9, -70,-25,  90, 43, -80,-57};
extern ALIGNED_SSE2 const signed short kfh_0207[8]  = {43, 70,   9,-80, -57, 87,  87,-90};

extern ALIGNED_SSE2 const signed short kfh_0300[8]  = {89, 75,  75,-18,  50,-89,  18,-50};
extern ALIGNED_SSE2 const signed short kfh_0301[8]  = {50, 18, -89,-50,  18, 75,  75,-89};
extern ALIGNED_SSE2 const signed short kfh_0400[8]  = {64, 64,  64,-64,  83, 36,  36,-83};

// koefs for vertical 1-D transform
extern ALIGNED_SSE2 const signed int rounder_1k[]  = {1024, 1024, 1024, 1024};

extern ALIGNED_SSE2 const signed short kfv_0000[8]  = {90, 90,  67, 46, -54,-82, -22, 38};
extern ALIGNED_SSE2 const signed short kfv_0001[8]  = {82, 22, -82,-73,  78, 67, -90,  4};
extern ALIGNED_SSE2 const signed short kfv_0002[8]  = {88, 85,  22, -4, -90,-78,  85, 46};
extern ALIGNED_SSE2 const signed short kfv_0003[8]  ={-54,-90,  13, 85, -38,-90,  67,-54};
extern ALIGNED_SSE2 const signed short kfv_0004[8]  = {82, 78, -31,-54,  88, 67, -13,-67};
extern ALIGNED_SSE2 const signed short kfv_0005[8]  ={-61, 13,  67,-22,  73,-31, -78, 38};
extern ALIGNED_SSE2 const signed short kfv_0006[8]  = {73, 67,  90, 82,  31,-13, -90,-73};
extern ALIGNED_SSE2 const signed short kfv_0007[8]  = {78, 85,  78, -4, -90,-22,  85,-22};
extern ALIGNED_SSE2 const signed short kfv_0008[8]  = {61, 54, -90,-88,  90, 85, -78,-31};
extern ALIGNED_SSE2 const signed short kfv_0009[8]  = {31,-46,  31, 90, -88, -4, -73,-61};
extern ALIGNED_SSE2 const signed short kfv_0010[8]  = {46, 38, -78,-61,  61, 22,  82, 88};
extern ALIGNED_SSE2 const signed short kfv_0011[8]  ={-90,-67,  54,-38,  85, 54,  90, 13};
extern ALIGNED_SSE2 const signed short kfv_0012[8]  = {31, 22, -38,-13, -46, -4,  54, -4};
extern ALIGNED_SSE2 const signed short kfv_0013[8]  = { 4, 73, -90,-46, -13, 82, -88,-31};
extern ALIGNED_SSE2 const signed short kfv_0014[8]  = {13,  4, -73,-85,  38, 73, -61,-90};
extern ALIGNED_SSE2 const signed short kfv_0015[8]  = {88, 38, -88,-61,  61,-46,  82, 46};

extern ALIGNED_SSE2 const signed short kfv_0100[8]  = {61,-73,  -4, 88, -90, 31, -46,-31};
extern ALIGNED_SSE2 const signed short kfv_0101[8]  = {31,-78,  85,-90,  88,-90,  54,-61};
extern ALIGNED_SSE2 const signed short kfv_0102[8]  ={-46, 82, -46,-61,  61,-88,  38,-88};
extern ALIGNED_SSE2 const signed short kfv_0103[8]  = {90,-61,  73,-38,  85,-73,   4,-13};
extern ALIGNED_SSE2 const signed short kfv_0104[8]  = {31,-88,  82, 13,  46,-90,  73, -4};
extern ALIGNED_SSE2 const signed short kfv_0105[8]  = { 4, 54,  -4, 46,  13,-38,  22,-31};
extern ALIGNED_SSE2 const signed short kfv_0106[8]  ={-13, 90,  54,-85,  38, 54, -67, 90};
extern ALIGNED_SSE2 const signed short kfv_0107[8]  ={-88, 82,  22,-61,  61,-78,  38,-46};
extern ALIGNED_SSE2 const signed short kfv_0108[8]  ={ -4,-90,  67,-78,  73,-82,  22,-82};
extern ALIGNED_SSE2 const signed short kfv_0109[8]  ={-38,-22, -82, 54, -46, 67,  90,-90};
extern ALIGNED_SSE2 const signed short kfv_0110[8]  = {22, 85, -22, 90,   4, 78,  85,-78};
extern ALIGNED_SSE2 const signed short kfv_0111[8]  = {73,-90, -13,-31, -82, 90,  67,-73};
extern ALIGNED_SSE2 const signed short kfv_0112[8]  ={-38,-78, -31,-73,  22, 67,  13, 61};
extern ALIGNED_SSE2 const signed short kfv_0113[8]  = {67,-13,  67,-88,  54,-31,  78,-82};
extern ALIGNED_SSE2 const signed short kfv_0114[8]  = {54, 67, -90, 38, -85, 13, -90, 54};
extern ALIGNED_SSE2 const signed short kfv_0115[8]  ={-46, 85, -78, 90,   4, 22,  85,-88};

extern ALIGNED_SSE2 const signed int kfv_0200[8] = {90,  57, -70,   9};
extern ALIGNED_SSE2 const signed int kfv_0201[8] = {57, -90,  90, -57};
extern ALIGNED_SSE2 const signed int kfv_0202[8] = {57, -90,  90, -57};
extern ALIGNED_SSE2 const signed int kfv_0203[8] ={ -9,  70, -57, -90};
extern ALIGNED_SSE2 const signed int kfv_0204[8] = {87,   9, -87,  70};
extern ALIGNED_SSE2 const signed int kfv_0205[8] = {43, -70,  43,  90};
extern ALIGNED_SSE2 const signed int kfv_0206[8] ={-80,  57, -80,   9};
extern ALIGNED_SSE2 const signed int kfv_0207[8] ={-87,   9,  87,  70};
extern ALIGNED_SSE2 const signed int kfv_0208[8] = {80, -43,  80, -43};
extern ALIGNED_SSE2 const signed int kfv_0209[8] = {25, -25, -25,  25};
extern ALIGNED_SSE2 const signed int kfv_0210[8] ={-25,  25,  25, -25};
extern ALIGNED_SSE2 const signed int kfv_0211[8] = {43, -80,  43, -80};
extern ALIGNED_SSE2 const signed int kfv_0212[8] = {70,  87,   9, -87};
extern ALIGNED_SSE2 const signed int kfv_0213[8] = { 9, -80,  57, -80};
extern ALIGNED_SSE2 const signed int kfv_0214[8] = {90,  43, -70,  43};
extern ALIGNED_SSE2 const signed int kfv_0215[8] = {70, -87,   9,  87};

extern ALIGNED_SSE2 const signed int kfv_0300[] = {64,  36, -64, -36};
extern ALIGNED_SSE2 const signed int kfv_0301[] = {89, -18,  18, -89};
extern ALIGNED_SSE2 const signed int kfv_0302[] = {64, -36,  64,  36};
extern ALIGNED_SSE2 const signed int kfv_0303[] = {75, -89,  75,  18};
extern ALIGNED_SSE2 const signed int kfv_0304[] = {64, -83,  64, -83};
extern ALIGNED_SSE2 const signed int kfv_0305[] = {50, -50,  50, -50};
extern ALIGNED_SSE2 const signed int kfv_0306[] = {64,  83, -64,  83};
extern ALIGNED_SSE2 const signed int kfv_0307[] = {18,  75, -89,  75};

    // 32-bits constants
    extern const signed int rounder_64[8]   = {64, 64, 0, 0, 64, 64, 0, 0};
    extern const signed int rounder_2048[8] = {2048, 2048, 0, 0, 2048, 2048, 0, 0};
    extern const signed int rounder_1024[8] = {1024, 1024, 0, 0, 1024, 1024, 0, 0};
    extern const signed int rounder_512[8] = {512, 512, 0, 0, 512, 512, 0, 0};
    extern const signed int rounder_256[8] = {256, 256, 0, 0, 256, 256, 0, 0};
    extern const signed int rounder_128[8] = {128, 128, 0, 0, 128, 128, 0, 0};

    // 16-bits constants
    extern const signed short koef0000[8]  = {64, 64,  64,-64,  83, 36,  36,-83};
    extern const signed short koef0001[8]  = {89, 75,  75,-18,  50,-89,  18,-50};
    extern const signed short koef0002[8]  = {50, 18, -89,-50,  18, 75,  75,-89};
    extern const signed short koef0003[8]  = {90, 87,  87, 57,  80,  9,  70,-43};
    extern const signed short koef0004[8]  = {57,-80,  43,-90,  25,-70,   9,-25};
    extern const signed short koef0005[8]  = {80, 70,   9,-43, -70,-87, -87,  9};
    extern const signed short koef0006[8]  ={-25, 90,  57, 25,  90,-80,  43,-57};
    extern const signed short koef0007[8]  = {57, 43, -80,-90, -25, 57,  90, 25};
    extern const signed short koef0008[8]  = {-9,-87, -87, 70,  43,  9,  70,-80};
    extern const signed short koef0009[8]  = {25,  9, -70,-25,  90, 43, -80,-57};
    extern const signed short koef0010[8]  = {43, 70,   9,-80, -57, 87,  87,-90};
    extern const signed short koef00010[8] = {90, 90,  90, 82,  88, 67,  85, 46};
    extern const signed short koef02030[8] = {88, 85,  67, 46,  31,-13, -13,-67};
    extern const signed short koef04050[8] = {82, 78,  22, -4, -54,-82, -90,-73};
    extern const signed short koef06070[8] = {73, 67, -31,-54, -90,-78, -22, 38};
    extern const signed short koef08090[8] = {61, 54, -73,-85, -46, -4,  82, 88};
    extern const signed short koef10110[8] = {46, 38, -90,-88,  38, 73,  54, -4};
    extern const signed short koef12130[8] = {31, 22, -78,-61,  90, 85, -61,-90};
    extern const signed short koef14150[8] = {13,  4, -38,-13,  61, 22, -78,-31};
    extern const signed short koef00011[8] = {82, 22,  78, -4,  73,-31,  67,-54};
    extern const signed short koef02031[8] ={-54,-90, -82,-73, -90,-22, -78, 38};
    extern const signed short koef04051[8] ={-61, 13,  13, 85,  78, 67,  85,-22};
    extern const signed short koef06071[8] = {78, 85,  67,-22, -38,-90, -90,  4};
    extern const signed short koef08091[8] = {31,-46, -88,-61, -13, 82,  90, 13};
    extern const signed short koef10111[8] ={-90,-67,  31, 90,  61,-46, -88,-31};
    extern const signed short koef12131[8] = { 4, 73,  54,-38, -88, -4,  82, 46};
    extern const signed short koef14151[8] = {88, 38, -90,-46,  85, 54, -73,-61};
    extern const signed short koef00012[8] = {61,-73,  54,-85,  46,-90,  38,-88};
    extern const signed short koef00013[8] = {31,-78,  22,-61,  13,-38,   4,-13};
    extern const signed short koef02032[8] ={-46, 82,  -4, 88,  38, 54,  73, -4};
    extern const signed short koef02033[8] = {90,-61,  85,-90,  61,-78,  22,-31};
    extern const signed short koef04052[8] = {31,-88, -46,-61, -90, 31, -67, 90};
    extern const signed short koef04053[8] = { 4, 54,  73,-38,  88,-90,  38,-46};
    extern const signed short koef06072[8] ={-13, 90,  82, 13,  61,-88, -46,-31};
    extern const signed short koef06073[8] ={-88, 82,  -4, 46,  85,-73,  54,-61};
    extern const signed short koef08092[8] = {-4,-90, -90, 38,  22, 67,  85,-78};
    extern const signed short koef08093[8] ={-38,-22, -78, 90,  54,-31,  67,-73};
    extern const signed short koef10112[8] = {22, 85,  67,-78, -85, 13,  13, 61};
    extern const signed short koef10113[8] = {73,-90, -82, 54,   4, 22,  78,-82};
    extern const signed short koef12132[8] ={-38,-78, -22, 90,  73,-82, -90, 54}; 
    extern const signed short koef12133[8] = {67,-13, -13,-31, -46, 67,  85,-88};
    extern const signed short koef14152[8] = {54, 67, -31,-73,   4, 78,  22,-82};
    extern const signed short koef14153[8] ={-46, 85,  67,-88, -82, 90,  90,-90};

    extern const unsigned int repos[32] = 
    {0, 16,  8, 17, 4, 18,  9, 19, 
    2, 20, 10, 21, 5, 22, 11, 23, 
    1, 24, 12, 25, 6, 26, 13, 27,
    3, 28, 14, 29, 7, 30, 15, 31};
}

#endif