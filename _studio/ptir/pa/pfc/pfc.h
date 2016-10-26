//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#ifndef PTIR_PICFFRMTCONV_H
#define PTIR_PICFFRMTCONV_H

#include "../api.h"
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIAN_3(a,b,c)    ((a > b) ?            \
    ((b > c) ? (b) : ((a > c) ? (c) : (a))) :    \
    ((a > c) ? (a) : ((b > c) ? (c) : (b))))

#define MABS(v) (tmp=(v),((tmp)^(tmp>>31)))
#define ABS(a)  (((a) < 0) ? (-(a)) : (a))
#define MSQUARED(v) ((v) * (v))


void ReSample(Frame *frmOut, Frame *frmIn);

void Filter_CCIR601(Plane *plaOut, Plane *plaIn, int *data);
#ifdef __cplusplus
}
#endif 
#endif