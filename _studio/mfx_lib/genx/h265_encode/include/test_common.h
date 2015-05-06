/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "mfxdefs.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define CLIPVAL(VAL, MINVAL, MAXVAL) MAX(MINVAL, MIN(MAXVAL, VAL))
#define CHECK_CM_ERR(ERR) if ((ERR) != CM_SUCCESS) return FAILED;
#define CHECK_ERR(ERR) if ((ERR) != PASSED) return (ERR);
#define DIVUP(a, b) ((a+b-1)/b)

#define BORDER          4
#define WIDTHB (WIDTH + BORDER*2)
#define HEIGHTB (HEIGHT + BORDER*2)

enum { PASSED, FAILED };

//const mfxI32 WIDTH  = 416;
//const mfxI32 HEIGHT = 240;
//const mfxI8 YUV_NAME[] = "./test_data/basketball_pass_416x240p_2.yuv";

const mfxI32 WIDTH  = 1920;
const mfxI32 HEIGHT = 1080;
const mfxI8 YUV_NAME[] = "C:/yuv/1080p/basketball_drive_1920x1080p_501.yuv";
//const mfxI8 YUV_NAME[] = "C:/yuv/JCTVC-G1200/1080p/Kimono1_1920x1080p_240_24.yuv";  //Kimono is used to catch failure in TestRefineMeP32x32
//const mfxI8 YUV_NAME[] = "C:/yuv/1080p/BasketballDrive_1920x1080p_500_50.yuv";

