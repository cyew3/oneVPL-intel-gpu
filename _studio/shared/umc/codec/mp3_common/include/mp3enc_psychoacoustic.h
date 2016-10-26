//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __MP3ENC_PSYCHOACOUSTIC_H__
#define __MP3ENC_PSYCHOACOUSTIC_H__

#define MAX_PPT_SHORT 47
//#define MAX_PPT_LONG  63
#define MAX_PPT_LONG  72

#define CBANDS_s      43
#define CBANDS_l      63
#define HBLOCK        513
#define BLOCK         1024
#define BLOCK_S       256
#define HBLOCK_S      129
#define SBBND_L       21
#define SBBND_S       12

#define CBANDS_12     58

#define NORM_TYPE     0
#define START_TYPE    1
#define SHORT_TYPE    2
#define STOP_TYPE     3

#define NUM_UNPRED_LINES_LONG 128

#define MAX(a,b)    (((a) > (b)) ? (a) : (b))
#define MIN(a,b)    (((a) < (b)) ? (a) : (b))

#endif //__MP3ENC_PSYCHOACOUSTIC_H__
