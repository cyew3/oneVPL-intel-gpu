/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: api.h

\* ****************************************************************************** */

#ifndef PA_PFC_H
#define PA_PFC_H

#include "../common.h"

// Deinterlace mode
#define INTL_MODE_NONE                0
#define INTL_MODE_CERTAIN            1
#define INTL_MODE_SMART_CHECK        2

// Deinterlace detection results
#define INTL_STRONG_INTERLACE        (2)
#define INTL_WEAK_INTERLACE            (1)
#define INTL_NO_DETECTION            (0)
#define INTL_WEAK_PROGRESSIVE        (-1)
#define INTL_STRONG_PROGRESSIVE        (-2)

// line removal modes
#define INTL_LINE_REMOVE_MEDIAN        0
#define INTL_LINE_REMOVE_AVG        1
#define INTL_LINE_REMOVE_DOWN        2
#define INTL_LINE_REMOVE_UP            3

#define MEDIAN_3(a,b,c)    ((a > b) ?                \
    ((b > c) ? (b) : ((a > c) ? (c) : (a))) :    \
    ((a > c) ? (a) : ((b > c) ? (c) : (b))))

#define MABS(v) (tmp=(v),((tmp)^(tmp>>31)))
#define ABS(a)  (((a) < 0) ? (-(a)) : (a))
#define MSQUARED(v) ((v) * (v))

#define MOVEMENT_THRESH (400)
#define SKIP_FACTOR (4)

//int Deinterlace(unsigned char *frame, unsigned char *prev_frame, int pels, int lines, unsigned int *mode, unsigned int *detection_measure);

#define INVTELE_RESULT_DROP_FRAME    0
#define INVTELE_RESULT_FRAME_OK        1

int InvTelecine(unsigned char *frame, unsigned char *prev_frame, int pels, int lines);

void ReSample(Frame *frmOut, Frame *frmIn);

void Filter_CCIR601(Plane *plaOut, Plane *plaIn, int *data);

#endif