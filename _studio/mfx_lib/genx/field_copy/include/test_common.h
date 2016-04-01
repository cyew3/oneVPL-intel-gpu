/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

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

const int WIDTH  = 416;
const int HEIGHT = 256; // must be multiple of 32 for cases where FIELD2TFF, FIELD2BFF, TFF2FIELD, BFF2FIELD
const char YUV_NAME_W[] = "./test_data/inwhite_1fr_di.yuv";
const char YUV_NAME_B[] = "./test_data/inblack_1fr_di.yuv";
const char YUV_NAME_G[] = "./test_data/ingrey_1fr_di.yuv";

