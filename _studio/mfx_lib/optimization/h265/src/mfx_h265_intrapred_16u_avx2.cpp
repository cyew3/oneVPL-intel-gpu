//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_AVX2) && defined(MFX_TARGET_OPTIMIZATION_AVX2)

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

namespace MFX_HEVC_PP
{

    static const Ipp32s intraPredAngleTable[] = {
        0,   0,  32,  26,  21,  17, 13,  9,  5,  2,  0, -2, -5, -9, -13, -17, -21,
        -26, -32, -26, -21, -17, -13, -9, -5, -2,  0,  2,  5,  9, 13,  17,  21,  26, 32
    };

    //
    // Encoder version:
    //  computes all 33 modes in one call, returned in pels[33][]
    //  for mode < 18, output is returned in transposed form
    //

    ALIGN_DECL(32) static const Ipp16s tab_frac[32][16] = {
        { 32, 0,32, 0,32, 0,32, 0,32, 0,32, 0,32, 0,32, 0 },
        { 31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1 },
        { 30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2 },
        { 29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3 },
        { 28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4 },
        { 27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5 },
        { 26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6 },
        { 25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7 },
        { 24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8 },
        { 23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9 },
        { 22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10 },
        { 21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11 },
        { 20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12 },
        { 19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13 },
        { 18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14 },
        { 17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15 },
        { 16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16 },
        { 15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17 },
        { 14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18 },
        { 13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19 },
        { 12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20 },
        { 11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21 },
        { 10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22 },
        {  9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23 },
        {  8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24 },
        {  7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25 },
        {  6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26 },
        {  5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27 },
        {  4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28 },
        {  3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29 },
        {  2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30 },
        {  1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31 },
    };

    ALIGN_DECL(32) static const Ipp8s tab_shuf4x4[][16] = {
        {  0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8, 9 },
        {  2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8, 9, 8, 9,10,11 },
        {  4, 5, 6, 7, 6, 7, 8, 9, 8, 9,10,11,10,11,12,13 },
        {  6, 7, 8, 9, 8, 9,10,11,10,11,12,13,12,13,14,15 },
    };

    ALIGN_DECL(32) static const Ipp8s tab_left4x4[2][16] = {
        {  2, 3, 4, 5, 6, 7, 8, 9, 4, 5, 6, 7, 8, 9,10,11 },
        {  6, 7, 8, 9,10,11,12,13, 8, 9,10,11,12,13,14,15 },
    };

ALIGN_DECL(32) static const Ipp8s proj_4x4[][4][16] = {
	{ { -1, -1, -1, -1, -1, -1,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, }, { -1, -1, -1, -1, -1, -1, -1, -1,  2,  3,  4,  5,  6,  7,  8,  9, }, { -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, }, { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, } },
	{ { -1, -1, -1, -1, -1, -1,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, }, { -1, -1, -1, -1, -1, -1, -1, -1,  2,  3,  4,  5,  6,  7,  8,  9, }, { -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, }, { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, } },
	{ { -1, -1, -1, -1,  8,  9,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, }, { -1, -1, -1, -1, -1, -1, -1, -1,  2,  3,  4,  5,  6,  7,  8,  9, }, { -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, }, { -1, -1, -1, -1,  8,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, } },
	{ { -1, -1, -1, -1,  4,  5,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, }, { -1, -1, -1, -1, -1, -1, -1, -1,  2,  3,  4,  5,  6,  7,  8,  9, }, { -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, }, { -1, -1, -1, -1,  4,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, } },
	{ { -1, -1,  8,  9,  4,  5,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, }, { -1, -1, -1, -1, -1, -1, -1, -1,  2,  3,  4,  5,  6,  7,  8,  9, }, { -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, }, { -1, -1,  8,  9,  4,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, } },
	{ { -1, -1,  6,  7,  4,  5,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, }, { -1, -1, -1, -1, -1, -1, -1, -1,  2,  3,  4,  5,  6,  7,  8,  9, }, { -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, }, { -1, -1,  6,  7,  4,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, } },
	{ {  8,  9,  4,  5,  2,  3,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, }, { -1, -1, -1, -1, -1, -1, -1, -1,  2,  3,  4,  5,  6,  7,  8,  9, }, { -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, }, {  8,  9,  4,  5,  2,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, } },
};

ALIGN_DECL(32) static const Ipp8s proj_8x8[][16] = {
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,},
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, 11, -1, -1,},
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 12, 13,  6,  7, -1, -1,},
        { -1, -1, -1, -1, -1, -1, -1, -1, 12, 13,  8,  9,  2,  3, -1, -1,},
        { -1, -1, -1, -1, -1, -1, 14, 15, 10, 11,  6,  7,  2,  3, -1, -1,},
        { -1, -1, -1, -1, 14, 15, 10, 11,  8,  9,  4,  5,  2,  3, -1, -1,},
        { -1, -1, 12, 13, 10, 11,  8,  9,  6,  7,  2,  3,  0,  1, -1, -1,},
};

ALIGN_DECL(32) static const Ipp8s proj_16x16[][3][16] = {
	{ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }, { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  1, }, { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, } },
	{ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }, { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  12,  13,  0,  1, }, { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  10,  11, -1, -1, -1, -1, } },
	{ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }, { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  14,  15,  8,  9,  0,  1, }, { -1, -1, -1, -1, -1, -1,  12,  13,  6,  7, -1, -1, -1, -1, -1, -1, } },
	{ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }, { -1, -1, -1, -1, -1, -1, -1, -1,  14,  15,  10,  11,  4,  5,  0,  1, }, { -1, -1,  14,  15,  8,  9,  4,  5, -1, -1, -1, -1, -1, -1, -1, -1, } },
	{ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  14,  15, }, { -1, -1, -1, -1, -1, -1, -1, -1,  12,  13,  8,  9,  4,  5,  0,  1, }, {  10,  11,  6,  7,  2,  3,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, } },
	{ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  14,  15,  12,  13,  8,  9, }, { -1, -1, -1, -1, -1, -1,  12,  13,  10,  11,  6,  7,  4,  5,  0,  1, }, {  6,  7,  2,  3,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, } },
	{ { -1, -1, -1, -1, -1, -1,  14,  15,  12,  13,  8,  9,  6,  7,  4,  5, }, { -1, -1,  14,  15,  12,  13,  10,  11,  8,  9,  4,  5,  2,  3,  0,  1, }, {  2,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, } },
};

ALIGN_DECL(32) static const Ipp8s proj_32x32[][4][16] = {
	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,} },
	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,} },
	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,} },

	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,} },
	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 12, 13,  0,  1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, 11, -1, -1, -1, -1,} },
	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1,  6,  7, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1,  4,  5, -1, -1, -1, -1, -1, -1, -1, -1,} },

	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,} },
	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 14, 15,  8,  9,  0,  1,}, {  -1, -1, -1, -1, -1, -1, 12, 13,  6,  7, -1, -1, -1, -1, -1, -1,} },
	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  8,  9,}, {  -1, -1, 10, 11,  4,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {   2,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,} },

	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,} },
	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, 14, 15, 10, 11,  4,  5,  0,  1,}, {  -1, -1, 14, 15,  8,  9,  4,  5, -1, -1, -1, -1, -1, -1, -1, -1,} },
	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 12, 13,  8,  9,}, {  -1, -1, -1, -1, -1, -1, 12, 13,  6,  7,  2,  3, -1, -1, -1, -1,}, {   2,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,} },

	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 12, 13,} },
	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 14, 15,}, {  -1, -1, -1, -1, -1, -1, -1, -1, 12, 13,  8,  9,  4,  5,  0,  1,}, {  10, 11,  6,  7,  2,  3,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1,} },
	{ {  -1, -1, -1, -1, -1, -1, 14, 15, 10, 11,  6,  7,  2,  3, -1, -1,}, {   8,  9,  4,  5,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,} },

	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, 12, 13, 10, 11,  6,  7,  4,  5,  0,  1,} },
	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 14, 15, 12, 13,  8,  9,}, {  -1, -1, -1, -1, -1, -1, 12, 13, 10, 11,  6,  7,  4,  5,  0,  1,}, {   6,  7,  2,  3,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,} },
	{ {  14, 15, 10, 11,  8,  9,  4,  5,  2,  3, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,} },

	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 14, 15, 12, 13,}, {  -1, -1, -1, -1, -1, -1, -1, -1, 14, 15, 12, 13, 10, 11,  8,  9,}, {   8,  9,  6,  7,  4,  5,  2,  3, -1, -1, -1, -1, -1, -1, -1, -1,} },
	{ {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, 14, 15, 12, 13,  8,  9,  6,  7,  4,  5,}, {  -1, -1, 14, 15, 12, 13, 10, 11,  8,  9,  4,  5,  2,  3,  0,  1,}, {   2,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,} },
	{ {   4,  5,  2,  3,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,}, {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,} },

};

    #define STEP(angle) {   \
        pos += angle;       \
        iIdx = pos >> 5;    \
        iFact = pos & 31;   \
    }

    #define MUX4(dst, src, idx) \
    switch ((idx+4) % 4) { \
        case 0: dst##a = src##0##a; dst##b = src##0##b; break; \
        case 1: dst##a = src##1##a; dst##b = src##1##b; break; \
        case 2: dst##a = src##2##a; dst##b = src##2##b; break; \
        case 3: dst##a = src##3##a; dst##b = src##3##b; break; \
    }

    template <int mode>
    static inline void PredAngle_4x4(Ipp16u *pSrc1, Ipp16u* /*pSrc2*/, Ipp16u *pDst1, Ipp16u *pDst2)
    {
        Ipp32s iIdx;
        Ipp32s iFact;
        Ipp32s pos = 0;
        Ipp32s angle = intraPredAngleTable[mode];

        __m128i r0a, r0b, r1a, r1b, r2a, r2b, r3a, r3b;
        __m128i s0, s1, s2, s3;
        __m128i t0a, t0b, t1a, t1b, t2a, t2b, t3a, t3b;
        __m128i f0, f1, f2, f3;

        r0a = _mm_load_si128((__m128i *)&pSrc1[0]);
        r0b = _mm_load_si128((__m128i *)&pSrc1[8]);

        t0a = _mm_shuffle_epi8(r0a, *(__m128i *)tab_shuf4x4[0]);
        t0b = _mm_shuffle_epi8(r0b, *(__m128i *)tab_shuf4x4[0]);

        t1a = _mm_shuffle_epi8(r0a, *(__m128i *)tab_shuf4x4[1]);
        t1b = _mm_shuffle_epi8(r0b, *(__m128i *)tab_shuf4x4[1]);

        t2a = _mm_shuffle_epi8(r0a, *(__m128i *)tab_shuf4x4[2]);
        t2b = _mm_shuffle_epi8(r0b, *(__m128i *)tab_shuf4x4[2]);

        t3a = _mm_shuffle_epi8(r0a, *(__m128i *)tab_shuf4x4[3]);
        t3b = _mm_shuffle_epi8(r0b, *(__m128i *)tab_shuf4x4[3]);

        // MUX4: r0 = t_iIdx
        STEP(angle);
        MUX4(r0, t, iIdx);
        f0 = _mm_load_si128((__m128i *)tab_frac[iFact]);

        STEP(angle);
        MUX4(r1, t, iIdx);
        f1 = _mm_load_si128((__m128i *)tab_frac[iFact]);

        STEP(angle);
        MUX4(r2, t, iIdx);
        f2 = _mm_load_si128((__m128i *)tab_frac[iFact]);

        STEP(angle);
        MUX4(r3, t, iIdx);
        f3 = _mm_load_si128((__m128i *)tab_frac[iFact]);

        r0a = _mm_madd_epi16(r0a, f0);
        r0b = _mm_madd_epi16(r0b, f0);
        r1a = _mm_madd_epi16(r1a, f1);
        r1b = _mm_madd_epi16(r1b, f1);
        r2a = _mm_madd_epi16(r2a, f2);
        r2b = _mm_madd_epi16(r2b, f2);
        r3a = _mm_madd_epi16(r3a, f3);
        r3b = _mm_madd_epi16(r3b, f3);

        /* pack 32 to 16 here */
        // f0 = [32,0], [31,1], etc. so should not overflow with up to 10-bit inputs
        // output = 0x3ff*a + 0x3ff*(32-a)
        s0 = _mm_packus_epi32(r0a, r1a);
        s1 = _mm_packus_epi32(r2a, r3a);
        s2 = _mm_packus_epi32(r0b, r1b);
        s3 = _mm_packus_epi32(r2b, r3b);

        /* round */
        s0 = _mm_add_epi16(s0, _mm_set1_epi16(16));
        s1 = _mm_add_epi16(s1, _mm_set1_epi16(16));
        s2 = _mm_add_epi16(s2, _mm_set1_epi16(16));
        s3 = _mm_add_epi16(s3, _mm_set1_epi16(16));

        /* shift */
        s0 = _mm_srai_epi16(s0, 5);
        s1 = _mm_srai_epi16(s1, 5);
        s2 = _mm_srai_epi16(s2, 5);
        s3 = _mm_srai_epi16(s3, 5);

        _mm_store_si128((__m128i *)&(pDst1[0]), s0);
        _mm_store_si128((__m128i *)&(pDst1[8]), s1);
        _mm_store_si128((__m128i *)&(pDst2[0]), s2);
        _mm_store_si128((__m128i *)&(pDst2[8]), s3);
    }

    #define MUX8(dst, src, idx) \
    switch ((idx+8) % 8) { \
        case 0: dst = src##0; break; \
        case 1: dst = src##1; break; \
        case 2: dst = src##2; break; \
        case 3: dst = src##3; break; \
        case 4: dst = src##4; break; \
        case 5: dst = src##5; break; \
        case 6: dst = src##6; break; \
        case 7: dst = src##7; break; \
    }

    #define PRED8(j) { \
        STEP(angle); \
        MUX8(r0, t, iIdx); \
        f0 = _mm256_load_si256((__m256i *)tab_frac[iFact]); \
         \
        STEP(angle); \
        MUX8(r1, t, iIdx); \
        f1 = _mm256_load_si256((__m256i *)tab_frac[iFact]); \
         \
        r0 = _mm256_madd_epi16(r0, f0); \
        r1 = _mm256_madd_epi16(r1, f1); \
        r0 = _mm256_packus_epi32(r0, r1); \
        r0 = _mm256_permute4x64_epi64(r0, 0xd8); \
        r0 = _mm256_add_epi16(r0, _mm256_set1_epi16(16)); \
        r0 = _mm256_srai_epi16(r0, 5); \
        _mm256_store_si256((__m256i *)&(pDst[j*16+0]), r0); \
    }

    template <int mode>
    static inline void PredAngle_8x8(Ipp16u *pSrc1, Ipp16u *pSrc2, Ipp16u *pDst1, Ipp16u *pDst2)
    {
        Ipp32s i, iIdx;
        Ipp32s iFact, pos, angle;
        Ipp16u *pSrc, *pDst;

        __m128i r0a, r0b, r1a, r1b;
        __m128i t0a, t0b;

        __m256i r0, r1, f0, f1;
        __m256i t0, t1, t2, t3, t4, t5, t6, t7;

        for (i = 0; i < 2; i++) {
            pos = 0;
            angle = intraPredAngleTable[mode];
            pSrc = (i == 0 ? pSrc1 : pSrc2);
            pDst = (i == 0 ? pDst1 : pDst2);

            t0a = _mm_load_si128((__m128i *)&pSrc[0]);             // [0 1 2 3 4 5 6 7]
            t0b = _mm_load_si128((__m128i *)&pSrc[8]);             // [8 9 A B C D E F]

            r0a = _mm_unpacklo_epi16(t0a, _mm_srli_si128(t0a, 2));          // [0 1 1 2 2 3 3 4]
            r0b = _mm_unpackhi_epi16(t0a, _mm_alignr_epi8(t0b, t0a, 2*1));  // [4 5 5 6 6 7 7 8]
            r1a = _mm_unpacklo_epi16(t0b, _mm_srli_si128(t0b, 2));          // [8 9 9 A A B B C]
            r1b = _mm_unpackhi_epi16(t0b, _mm_srli_si128(t0b, 2));          // [C D D E E F F -]

            t0 = _mm256_permute2x128_si256(mm256(r0a), mm256(r0b), 0x20);   // [0 1 1 2 2 3 3 4 | 4 5 5 6 6 7 7 8]
            t4 = _mm256_permute2x128_si256(mm256(r0b), mm256(r1a), 0x20);   // [4 5 5 6 6 7 7 8 | 8 9 9 A A B B C]
            r0 = _mm256_permute2x128_si256(mm256(r1a), mm256(r1b), 0x20);   // [8 9 9 A A B B C | C D D E E F F -]

            t1 = _mm256_alignr_epi8(t4, t0,  4);   // [1 2 2 3 3 4 4 5 | 5 6 6 7 7 8 8 9]
            t2 = _mm256_alignr_epi8(t4, t0,  8);   // [2 3 3 4 4 5 5 6 | 6 7 7 8 8 9 9 A]
            t3 = _mm256_alignr_epi8(t4, t0, 12);   // [3 4 4 5 5 6 6 7 | 7 8 8 9 9 A A B]

            t5 = _mm256_alignr_epi8(r0, t4,  4);   // [5 6 6 7 7 8 8 9 | 9 A A B B C C D]
            t6 = _mm256_alignr_epi8(r0, t4,  8);   // [6 7 7 8 8 9 9 A | A B B C C D D E]
            t7 = _mm256_alignr_epi8(r0, t4, 12);   // [7 8 8 9 9 A A B | B C C D D E E F]

            PRED8(0);
            PRED8(1);
            PRED8(2);
            PRED8(3);
        }
    }

    #define PRED16(j) { \
        pos += angle; \
        iIdx = pos >> 5; \
        iFact = pos & 31; \
        iIdx = ((iIdx+16) % 16); \
        \
        f0 = _mm256_load_si256((__m256i *)tab_frac[iFact]); \
            \
        t0 = _mm256_loadu_si256((__m256i *)&pSrc[iIdx+0]); \
        t1 = _mm256_loadu_si256((__m256i *)&pSrc[iIdx+1]); \
            \
        r0 = _mm256_unpacklo_epi16(t0, t1); \
        r1 = _mm256_unpackhi_epi16(t0, t1); \
            \
        r0 = _mm256_madd_epi16(r0, f0); \
        r1 = _mm256_madd_epi16(r1, f0); \
            \
        r0 = _mm256_packus_epi32(r0, r1); \
        r0 = _mm256_add_epi16(r0, _mm256_set1_epi16(16)); \
        r0 = _mm256_srai_epi16(r0, 5); \
            \
        _mm256_store_si256((__m256i *)&pDst[j*16+0], r0); \
    }

    template <int mode>
    static inline void PredAngle_16x16(Ipp16u *pSrc1, Ipp16u *pSrc2, Ipp16u *pDst1, Ipp16u *pDst2)
    {
        Ipp32s i, iIdx;
        Ipp32s iFact, pos, angle;
        Ipp16u *pSrc, *pDst;

        __m256i r0, r1, f0, t0, t1;

        for (i = 0; i < 2; i++) {
            pos = 0;
            angle = intraPredAngleTable[mode];
            pSrc = (i == 0 ? pSrc1 : pSrc2);
            pDst = (i == 0 ? pDst1 : pDst2);

            PRED16(0);
            PRED16(1);
            PRED16(2);
            PRED16(3);
            PRED16(4);
            PRED16(5);
            PRED16(6);
            PRED16(7);
            PRED16(8);
            PRED16(9);
            PRED16(10);
            PRED16(11);
            PRED16(12);
            PRED16(13);
            PRED16(14);
            PRED16(15);
        }
    }

    #define PRED32(j) { \
        pos += angle; \
        iIdx = pos >> 5; \
        iFact = pos & 31; \
        iIdx = ((iIdx+32) % 32); \
            \
        f0 = _mm256_load_si256((__m256i *)tab_frac[iFact]); \
            \
        t0 = _mm256_loadu_si256((__m256i *)&pSrc[iIdx+0]); \
        t1 = _mm256_loadu_si256((__m256i *)&pSrc[iIdx+1]); \
        t2 = _mm256_loadu_si256((__m256i *)&pSrc[iIdx+16]); \
        t3 = _mm256_loadu_si256((__m256i *)&pSrc[iIdx+17]); \
            \
        r0 = _mm256_unpacklo_epi16(t0, t1); \
        r1 = _mm256_unpackhi_epi16(t0, t1); \
        r2 = _mm256_unpacklo_epi16(t2, t3); \
        r3 = _mm256_unpackhi_epi16(t2, t3); \
            \
        r0 = _mm256_madd_epi16(r0, f0); \
        r1 = _mm256_madd_epi16(r1, f0); \
        r2 = _mm256_madd_epi16(r2, f0); \
        r3 = _mm256_madd_epi16(r3, f0); \
            \
        r0 = _mm256_packus_epi32(r0, r1); \
        r0 = _mm256_add_epi16(r0, _mm256_set1_epi16(16)); \
        r0 = _mm256_srai_epi16(r0, 5); \
            \
        r2 = _mm256_packus_epi32(r2, r3); \
        r2 = _mm256_add_epi16(r2, _mm256_set1_epi16(16)); \
        r2 = _mm256_srai_epi16(r2, 5); \
            \
        _mm256_store_si256((__m256i *)&pDst[j*32+ 0], r0); \
        _mm256_store_si256((__m256i *)&pDst[j*32+16], r2); \
    }

    template <int mode>
    static inline void PredAngle_32x32(Ipp16u *pSrc1, Ipp16u *pSrc2, Ipp16u *pDst1, Ipp16u *pDst2)
    {
        Ipp32s i, iIdx;
        Ipp32s iFact, pos, angle;
        Ipp16u *pSrc, *pDst;

        __m256i f0, r0, r1, r2, r3, t0, t1, t2, t3;

        for (i = 0; i < 2; i++) {
            pos = 0;
            angle = intraPredAngleTable[mode];
            pSrc = (i == 0 ? pSrc1 : pSrc2);
            pDst = (i == 0 ? pDst1 : pDst2);

            PRED32(0);
            PRED32(1);
            PRED32(2);
            PRED32(3);
            PRED32(4);
            PRED32(5);
            PRED32(6);
            PRED32(7);
            PRED32(8);
            PRED32(9);
            PRED32(10);
            PRED32(11);
            PRED32(12);
            PRED32(13);
            PRED32(14);
            PRED32(15);
            PRED32(16);
            PRED32(17);
            PRED32(18);
            PRED32(19);
            PRED32(20);
            PRED32(21);
            PRED32(22);
            PRED32(23);
            PRED32(24);
            PRED32(25);
            PRED32(26);
            PRED32(27);
            PRED32(28);
            PRED32(29);
            PRED32(30);
            PRED32(31);
        }
    }

    template <int width, int mode>
    static inline void PredAngle(Ipp16u *pSrc1, Ipp16u *pSrc2, Ipp16u *pDst1, Ipp16u *pDst2)
    {
        switch (width)
        {
        case 4:
            PredAngle_4x4<mode>(pSrc1, pSrc2, pDst1, pDst2);
            break;
        case 8:
            PredAngle_8x8<mode>(pSrc1, pSrc2, pDst1, pDst2);
            break;
        case 16:
            PredAngle_16x16<mode>(pSrc1, pSrc2, pDst1, pDst2);
            break;
        case 32:
            PredAngle_32x32<mode>(pSrc1, pSrc2, pDst1, pDst2);
            break;
        }
    }

    //
    // mode = 2,34 (shift left)
    //
    template <int width>
    static inline void PredAngle2(Ipp16u *pSrc1, Ipp16u *pSrc2, Ipp16u *pDst1, Ipp16u *pDst2)
    {
        switch (width)
        {
        case 4:
            {
                __m128i r0 = _mm_load_si128((__m128i *)&pSrc1[0]);
                __m128i s0 = _mm_load_si128((__m128i *)&pSrc1[8]);
                __m128i r1 = r0, s1 = s0;

                r0 = _mm_shuffle_epi8(r0, _mm_load_si128((__m128i *)tab_left4x4[0]));
                r1 = _mm_shuffle_epi8(r1, _mm_load_si128((__m128i *)tab_left4x4[1]));
                s0 = _mm_shuffle_epi8(s0, _mm_load_si128((__m128i *)tab_left4x4[0]));
                s1 = _mm_shuffle_epi8(s1, _mm_load_si128((__m128i *)tab_left4x4[1]));

                /* mode 2 = [r0 | r1], mode 34 = [s0 | s1] */
                _mm_store_si128((__m128i*)&pDst1[0], r0);
                _mm_store_si128((__m128i*)&pDst1[8], r1);
                _mm_store_si128((__m128i*)&pDst2[0], s0);
                _mm_store_si128((__m128i*)&pDst2[8], s1);
            }
            break;
        case 8:
            {
                __m128i r0 = _mm_load_si128((__m128i *)&pSrc1[0]);
                __m128i r1 = _mm_load_si128((__m128i *)&pSrc1[8]);

                __m128i t0 = _mm_alignr_epi8(r1, r0,  2);
                __m128i t1 = _mm_alignr_epi8(r1, r0,  4);
                __m128i t2 = _mm_alignr_epi8(r1, r0,  6);
                __m128i t3 = _mm_alignr_epi8(r1, r0,  8);
                __m128i t4 = _mm_alignr_epi8(r1, r0, 10);
                __m128i t5 = _mm_alignr_epi8(r1, r0, 12);
                __m128i t6 = _mm_alignr_epi8(r1, r0, 14);
                __m128i t7 = r1;

                _mm_store_si128((__m128i*)&pDst1[ 0], t0);
                _mm_store_si128((__m128i*)&pDst1[ 8], t1);
                _mm_store_si128((__m128i*)&pDst1[16], t2);
                _mm_store_si128((__m128i*)&pDst1[24], t3);
                _mm_store_si128((__m128i*)&pDst1[32], t4);
                _mm_store_si128((__m128i*)&pDst1[40], t5);
                _mm_store_si128((__m128i*)&pDst1[48], t6);
                _mm_store_si128((__m128i*)&pDst1[56], t7);

                r0 = _mm_load_si128((__m128i *)&pSrc2[0]);
                r1 = _mm_load_si128((__m128i *)&pSrc2[8]);

                t0 = _mm_alignr_epi8(r1, r0,  2);
                t1 = _mm_alignr_epi8(r1, r0,  4);
                t2 = _mm_alignr_epi8(r1, r0,  6);
                t3 = _mm_alignr_epi8(r1, r0,  8);
                t4 = _mm_alignr_epi8(r1, r0, 10);
                t5 = _mm_alignr_epi8(r1, r0, 12);
                t6 = _mm_alignr_epi8(r1, r0, 14);
                t7 = r1;

                _mm_store_si128((__m128i*)&pDst2[ 0], t0);
                _mm_store_si128((__m128i*)&pDst2[ 8], t1);
                _mm_store_si128((__m128i*)&pDst2[16], t2);
                _mm_store_si128((__m128i*)&pDst2[24], t3);
                _mm_store_si128((__m128i*)&pDst2[32], t4);
                _mm_store_si128((__m128i*)&pDst2[40], t5);
                _mm_store_si128((__m128i*)&pDst2[48], t6);
                _mm_store_si128((__m128i*)&pDst2[56], t7);
            }
            break;
        case 16:
            {
                Ipp32s i;
                Ipp16u *pSrc, *pDst;

                for (i = 0; i < 2; i++) {
                    pSrc = (i == 0 ? pSrc1 : pSrc2);
                    pDst = (i == 0 ? pDst1 : pDst2);

                    __m128i r0 = _mm_load_si128((__m128i *)&pSrc[ 0]);
                    __m128i r1 = _mm_load_si128((__m128i *)&pSrc[ 8]);
                    __m128i r2 = _mm_load_si128((__m128i *)&pSrc[16]);
                    __m128i r3 = _mm_load_si128((__m128i *)&pSrc[24]);

                    _mm_store_si128((__m128i*)&pDst[ 0*16+0], _mm_alignr_epi8(r1, r0,  2));   _mm_store_si128((__m128i*)&pDst[ 0*16+8], _mm_alignr_epi8(r2, r1,  2));
                    _mm_store_si128((__m128i*)&pDst[ 1*16+0], _mm_alignr_epi8(r1, r0,  4));   _mm_store_si128((__m128i*)&pDst[ 1*16+8], _mm_alignr_epi8(r2, r1,  4));
                    _mm_store_si128((__m128i*)&pDst[ 2*16+0], _mm_alignr_epi8(r1, r0,  6));   _mm_store_si128((__m128i*)&pDst[ 2*16+8], _mm_alignr_epi8(r2, r1,  6));
                    _mm_store_si128((__m128i*)&pDst[ 3*16+0], _mm_alignr_epi8(r1, r0,  8));   _mm_store_si128((__m128i*)&pDst[ 3*16+8], _mm_alignr_epi8(r2, r1,  8));
                    _mm_store_si128((__m128i*)&pDst[ 4*16+0], _mm_alignr_epi8(r1, r0, 10));   _mm_store_si128((__m128i*)&pDst[ 4*16+8], _mm_alignr_epi8(r2, r1, 10));
                    _mm_store_si128((__m128i*)&pDst[ 5*16+0], _mm_alignr_epi8(r1, r0, 12));   _mm_store_si128((__m128i*)&pDst[ 5*16+8], _mm_alignr_epi8(r2, r1, 12));
                    _mm_store_si128((__m128i*)&pDst[ 6*16+0], _mm_alignr_epi8(r1, r0, 14));   _mm_store_si128((__m128i*)&pDst[ 6*16+8], _mm_alignr_epi8(r2, r1, 14));
                    _mm_store_si128((__m128i*)&pDst[ 7*16+0], r1);                            _mm_store_si128((__m128i*)&pDst[ 7*16+8], r2);

                    _mm_store_si128((__m128i*)&pDst[ 8*16+0], _mm_alignr_epi8(r2, r1,  2));   _mm_store_si128((__m128i*)&pDst[ 8*16+8], _mm_alignr_epi8(r3, r2,  2));
                    _mm_store_si128((__m128i*)&pDst[ 9*16+0], _mm_alignr_epi8(r2, r1,  4));   _mm_store_si128((__m128i*)&pDst[ 9*16+8], _mm_alignr_epi8(r3, r2,  4));
                    _mm_store_si128((__m128i*)&pDst[10*16+0], _mm_alignr_epi8(r2, r1,  6));   _mm_store_si128((__m128i*)&pDst[10*16+8], _mm_alignr_epi8(r3, r2,  6));
                    _mm_store_si128((__m128i*)&pDst[11*16+0], _mm_alignr_epi8(r2, r1,  8));   _mm_store_si128((__m128i*)&pDst[11*16+8], _mm_alignr_epi8(r3, r2,  8));
                    _mm_store_si128((__m128i*)&pDst[12*16+0], _mm_alignr_epi8(r2, r1, 10));   _mm_store_si128((__m128i*)&pDst[12*16+8], _mm_alignr_epi8(r3, r2, 10));
                    _mm_store_si128((__m128i*)&pDst[13*16+0], _mm_alignr_epi8(r2, r1, 12));   _mm_store_si128((__m128i*)&pDst[13*16+8], _mm_alignr_epi8(r3, r2, 12));
                    _mm_store_si128((__m128i*)&pDst[14*16+0], _mm_alignr_epi8(r2, r1, 14));   _mm_store_si128((__m128i*)&pDst[14*16+8], _mm_alignr_epi8(r3, r2, 14));
                    _mm_store_si128((__m128i*)&pDst[15*16+0], r2);                            _mm_store_si128((__m128i*)&pDst[15*16+8], r3);
                }
            }
            break;
        case 32:
            {
                Ipp32s i;
                Ipp16u *pSrc, *pDst;

                for (i = 0; i < 2; i++) {
                    pSrc = (i == 0 ? pSrc1 : pSrc2);
                    pDst = (i == 0 ? pDst1 : pDst2);

                    __m128i r0 = _mm_load_si128((__m128i *)&pSrc[ 0]);
                    __m128i r1 = _mm_load_si128((__m128i *)&pSrc[ 8]);
                    __m128i r2 = _mm_load_si128((__m128i *)&pSrc[16]);
                    __m128i r3 = _mm_load_si128((__m128i *)&pSrc[24]);
                    __m128i r4 = _mm_load_si128((__m128i *)&pSrc[32]);
                    __m128i r5 = _mm_load_si128((__m128i *)&pSrc[40]);
                    __m128i r6 = _mm_load_si128((__m128i *)&pSrc[48]);
                    __m128i r7 = _mm_load_si128((__m128i *)&pSrc[56]);

                    _mm_store_si128((__m128i*)&pDst[ 0*32+0], _mm_alignr_epi8(r1, r0,  2));   _mm_store_si128((__m128i*)&pDst[ 0*32+8], _mm_alignr_epi8(r2, r1,  2));   _mm_store_si128((__m128i*)&pDst[ 0*32+16], _mm_alignr_epi8(r3, r2,  2));   _mm_store_si128((__m128i*)&pDst[ 0*32+24], _mm_alignr_epi8(r4, r3,  2));
                    _mm_store_si128((__m128i*)&pDst[ 1*32+0], _mm_alignr_epi8(r1, r0,  4));   _mm_store_si128((__m128i*)&pDst[ 1*32+8], _mm_alignr_epi8(r2, r1,  4));   _mm_store_si128((__m128i*)&pDst[ 1*32+16], _mm_alignr_epi8(r3, r2,  4));   _mm_store_si128((__m128i*)&pDst[ 1*32+24], _mm_alignr_epi8(r4, r3,  4));
                    _mm_store_si128((__m128i*)&pDst[ 2*32+0], _mm_alignr_epi8(r1, r0,  6));   _mm_store_si128((__m128i*)&pDst[ 2*32+8], _mm_alignr_epi8(r2, r1,  6));   _mm_store_si128((__m128i*)&pDst[ 2*32+16], _mm_alignr_epi8(r3, r2,  6));   _mm_store_si128((__m128i*)&pDst[ 2*32+24], _mm_alignr_epi8(r4, r3,  6));
                    _mm_store_si128((__m128i*)&pDst[ 3*32+0], _mm_alignr_epi8(r1, r0,  8));   _mm_store_si128((__m128i*)&pDst[ 3*32+8], _mm_alignr_epi8(r2, r1,  8));   _mm_store_si128((__m128i*)&pDst[ 3*32+16], _mm_alignr_epi8(r3, r2,  8));   _mm_store_si128((__m128i*)&pDst[ 3*32+24], _mm_alignr_epi8(r4, r3,  8));
                    _mm_store_si128((__m128i*)&pDst[ 4*32+0], _mm_alignr_epi8(r1, r0, 10));   _mm_store_si128((__m128i*)&pDst[ 4*32+8], _mm_alignr_epi8(r2, r1, 10));   _mm_store_si128((__m128i*)&pDst[ 4*32+16], _mm_alignr_epi8(r3, r2, 10));   _mm_store_si128((__m128i*)&pDst[ 4*32+24], _mm_alignr_epi8(r4, r3, 10));
                    _mm_store_si128((__m128i*)&pDst[ 5*32+0], _mm_alignr_epi8(r1, r0, 12));   _mm_store_si128((__m128i*)&pDst[ 5*32+8], _mm_alignr_epi8(r2, r1, 12));   _mm_store_si128((__m128i*)&pDst[ 5*32+16], _mm_alignr_epi8(r3, r2, 12));   _mm_store_si128((__m128i*)&pDst[ 5*32+24], _mm_alignr_epi8(r4, r3, 12));
                    _mm_store_si128((__m128i*)&pDst[ 6*32+0], _mm_alignr_epi8(r1, r0, 14));   _mm_store_si128((__m128i*)&pDst[ 6*32+8], _mm_alignr_epi8(r2, r1, 14));   _mm_store_si128((__m128i*)&pDst[ 6*32+16], _mm_alignr_epi8(r3, r2, 14));   _mm_store_si128((__m128i*)&pDst[ 6*32+24], _mm_alignr_epi8(r4, r3, 14));
                    _mm_store_si128((__m128i*)&pDst[ 7*32+0], r1);                            _mm_store_si128((__m128i*)&pDst[ 7*32+8], r2);                            _mm_store_si128((__m128i*)&pDst[ 7*32+16], r3);                            _mm_store_si128((__m128i*)&pDst[ 7*32+24], r4);

                    _mm_store_si128((__m128i*)&pDst[ 8*32+0], _mm_alignr_epi8(r2, r1,  2));   _mm_store_si128((__m128i*)&pDst[ 8*32+8], _mm_alignr_epi8(r3, r2,  2));   _mm_store_si128((__m128i*)&pDst[ 8*32+16], _mm_alignr_epi8(r4, r3,  2));   _mm_store_si128((__m128i*)&pDst[ 8*32+24], _mm_alignr_epi8(r5, r4,  2));
                    _mm_store_si128((__m128i*)&pDst[ 9*32+0], _mm_alignr_epi8(r2, r1,  4));   _mm_store_si128((__m128i*)&pDst[ 9*32+8], _mm_alignr_epi8(r3, r2,  4));   _mm_store_si128((__m128i*)&pDst[ 9*32+16], _mm_alignr_epi8(r4, r3,  4));   _mm_store_si128((__m128i*)&pDst[ 9*32+24], _mm_alignr_epi8(r5, r4,  4));
                    _mm_store_si128((__m128i*)&pDst[10*32+0], _mm_alignr_epi8(r2, r1,  6));   _mm_store_si128((__m128i*)&pDst[10*32+8], _mm_alignr_epi8(r3, r2,  6));   _mm_store_si128((__m128i*)&pDst[10*32+16], _mm_alignr_epi8(r4, r3,  6));   _mm_store_si128((__m128i*)&pDst[10*32+24], _mm_alignr_epi8(r5, r4,  6));
                    _mm_store_si128((__m128i*)&pDst[11*32+0], _mm_alignr_epi8(r2, r1,  8));   _mm_store_si128((__m128i*)&pDst[11*32+8], _mm_alignr_epi8(r3, r2,  8));   _mm_store_si128((__m128i*)&pDst[11*32+16], _mm_alignr_epi8(r4, r3,  8));   _mm_store_si128((__m128i*)&pDst[11*32+24], _mm_alignr_epi8(r5, r4,  8));
                    _mm_store_si128((__m128i*)&pDst[12*32+0], _mm_alignr_epi8(r2, r1, 10));   _mm_store_si128((__m128i*)&pDst[12*32+8], _mm_alignr_epi8(r3, r2, 10));   _mm_store_si128((__m128i*)&pDst[12*32+16], _mm_alignr_epi8(r4, r3, 10));   _mm_store_si128((__m128i*)&pDst[12*32+24], _mm_alignr_epi8(r5, r4, 10));
                    _mm_store_si128((__m128i*)&pDst[13*32+0], _mm_alignr_epi8(r2, r1, 12));   _mm_store_si128((__m128i*)&pDst[13*32+8], _mm_alignr_epi8(r3, r2, 12));   _mm_store_si128((__m128i*)&pDst[13*32+16], _mm_alignr_epi8(r4, r3, 12));   _mm_store_si128((__m128i*)&pDst[13*32+24], _mm_alignr_epi8(r5, r4, 12));
                    _mm_store_si128((__m128i*)&pDst[14*32+0], _mm_alignr_epi8(r2, r1, 14));   _mm_store_si128((__m128i*)&pDst[14*32+8], _mm_alignr_epi8(r3, r2, 14));   _mm_store_si128((__m128i*)&pDst[14*32+16], _mm_alignr_epi8(r4, r3, 14));   _mm_store_si128((__m128i*)&pDst[14*32+24], _mm_alignr_epi8(r5, r4, 14));
                    _mm_store_si128((__m128i*)&pDst[15*32+0], r2);                            _mm_store_si128((__m128i*)&pDst[15*32+8], r3);                            _mm_store_si128((__m128i*)&pDst[15*32+16], r4);                            _mm_store_si128((__m128i*)&pDst[15*32+24], r5);

                    _mm_store_si128((__m128i*)&pDst[16*32+0], _mm_alignr_epi8(r3, r2,  2));   _mm_store_si128((__m128i*)&pDst[16*32+8], _mm_alignr_epi8(r4, r3,  2));   _mm_store_si128((__m128i*)&pDst[16*32+16], _mm_alignr_epi8(r5, r4,  2));   _mm_store_si128((__m128i*)&pDst[16*32+24], _mm_alignr_epi8(r6, r5,  2));
                    _mm_store_si128((__m128i*)&pDst[17*32+0], _mm_alignr_epi8(r3, r2,  4));   _mm_store_si128((__m128i*)&pDst[17*32+8], _mm_alignr_epi8(r4, r3,  4));   _mm_store_si128((__m128i*)&pDst[17*32+16], _mm_alignr_epi8(r5, r4,  4));   _mm_store_si128((__m128i*)&pDst[17*32+24], _mm_alignr_epi8(r6, r5,  4));
                    _mm_store_si128((__m128i*)&pDst[18*32+0], _mm_alignr_epi8(r3, r2,  6));   _mm_store_si128((__m128i*)&pDst[18*32+8], _mm_alignr_epi8(r4, r3,  6));   _mm_store_si128((__m128i*)&pDst[18*32+16], _mm_alignr_epi8(r5, r4,  6));   _mm_store_si128((__m128i*)&pDst[18*32+24], _mm_alignr_epi8(r6, r5,  6));
                    _mm_store_si128((__m128i*)&pDst[19*32+0], _mm_alignr_epi8(r3, r2,  8));   _mm_store_si128((__m128i*)&pDst[19*32+8], _mm_alignr_epi8(r4, r3,  8));   _mm_store_si128((__m128i*)&pDst[19*32+16], _mm_alignr_epi8(r5, r4,  8));   _mm_store_si128((__m128i*)&pDst[19*32+24], _mm_alignr_epi8(r6, r5,  8));
                    _mm_store_si128((__m128i*)&pDst[20*32+0], _mm_alignr_epi8(r3, r2, 10));   _mm_store_si128((__m128i*)&pDst[20*32+8], _mm_alignr_epi8(r4, r3, 10));   _mm_store_si128((__m128i*)&pDst[20*32+16], _mm_alignr_epi8(r5, r4, 10));   _mm_store_si128((__m128i*)&pDst[20*32+24], _mm_alignr_epi8(r6, r5, 10));
                    _mm_store_si128((__m128i*)&pDst[21*32+0], _mm_alignr_epi8(r3, r2, 12));   _mm_store_si128((__m128i*)&pDst[21*32+8], _mm_alignr_epi8(r4, r3, 12));   _mm_store_si128((__m128i*)&pDst[21*32+16], _mm_alignr_epi8(r5, r4, 12));   _mm_store_si128((__m128i*)&pDst[21*32+24], _mm_alignr_epi8(r6, r5, 12));
                    _mm_store_si128((__m128i*)&pDst[22*32+0], _mm_alignr_epi8(r3, r2, 14));   _mm_store_si128((__m128i*)&pDst[22*32+8], _mm_alignr_epi8(r4, r3, 14));   _mm_store_si128((__m128i*)&pDst[22*32+16], _mm_alignr_epi8(r5, r4, 14));   _mm_store_si128((__m128i*)&pDst[22*32+24], _mm_alignr_epi8(r6, r5, 14));
                    _mm_store_si128((__m128i*)&pDst[23*32+0], r3);                            _mm_store_si128((__m128i*)&pDst[23*32+8], r4);                            _mm_store_si128((__m128i*)&pDst[23*32+16], r5);                            _mm_store_si128((__m128i*)&pDst[23*32+24], r6);

                    _mm_store_si128((__m128i*)&pDst[24*32+0], _mm_alignr_epi8(r4, r3,  2));   _mm_store_si128((__m128i*)&pDst[24*32+8], _mm_alignr_epi8(r5, r4,  2));   _mm_store_si128((__m128i*)&pDst[24*32+16], _mm_alignr_epi8(r6, r5,  2));   _mm_store_si128((__m128i*)&pDst[24*32+24], _mm_alignr_epi8(r7, r6,  2));
                    _mm_store_si128((__m128i*)&pDst[25*32+0], _mm_alignr_epi8(r4, r3,  4));   _mm_store_si128((__m128i*)&pDst[25*32+8], _mm_alignr_epi8(r5, r4,  4));   _mm_store_si128((__m128i*)&pDst[25*32+16], _mm_alignr_epi8(r6, r5,  4));   _mm_store_si128((__m128i*)&pDst[25*32+24], _mm_alignr_epi8(r7, r6,  4));
                    _mm_store_si128((__m128i*)&pDst[26*32+0], _mm_alignr_epi8(r4, r3,  6));   _mm_store_si128((__m128i*)&pDst[26*32+8], _mm_alignr_epi8(r5, r4,  6));   _mm_store_si128((__m128i*)&pDst[26*32+16], _mm_alignr_epi8(r6, r5,  6));   _mm_store_si128((__m128i*)&pDst[26*32+24], _mm_alignr_epi8(r7, r6,  6));
                    _mm_store_si128((__m128i*)&pDst[27*32+0], _mm_alignr_epi8(r4, r3,  8));   _mm_store_si128((__m128i*)&pDst[27*32+8], _mm_alignr_epi8(r5, r4,  8));   _mm_store_si128((__m128i*)&pDst[27*32+16], _mm_alignr_epi8(r6, r5,  8));   _mm_store_si128((__m128i*)&pDst[27*32+24], _mm_alignr_epi8(r7, r6,  8));
                    _mm_store_si128((__m128i*)&pDst[28*32+0], _mm_alignr_epi8(r4, r3, 10));   _mm_store_si128((__m128i*)&pDst[28*32+8], _mm_alignr_epi8(r5, r4, 10));   _mm_store_si128((__m128i*)&pDst[28*32+16], _mm_alignr_epi8(r6, r5, 10));   _mm_store_si128((__m128i*)&pDst[28*32+24], _mm_alignr_epi8(r7, r6, 10));
                    _mm_store_si128((__m128i*)&pDst[29*32+0], _mm_alignr_epi8(r4, r3, 12));   _mm_store_si128((__m128i*)&pDst[29*32+8], _mm_alignr_epi8(r5, r4, 12));   _mm_store_si128((__m128i*)&pDst[29*32+16], _mm_alignr_epi8(r6, r5, 12));   _mm_store_si128((__m128i*)&pDst[29*32+24], _mm_alignr_epi8(r7, r6, 12));
                    _mm_store_si128((__m128i*)&pDst[30*32+0], _mm_alignr_epi8(r4, r3, 14));   _mm_store_si128((__m128i*)&pDst[30*32+8], _mm_alignr_epi8(r5, r4, 14));   _mm_store_si128((__m128i*)&pDst[30*32+16], _mm_alignr_epi8(r6, r5, 14));   _mm_store_si128((__m128i*)&pDst[30*32+24], _mm_alignr_epi8(r7, r6, 14));
                    _mm_store_si128((__m128i*)&pDst[31*32+0], r4);                            _mm_store_si128((__m128i*)&pDst[31*32+8], r5);                            _mm_store_si128((__m128i*)&pDst[31*32+16], r6);                            _mm_store_si128((__m128i*)&pDst[31*32+24], r7);
                }
            }
            break;
        }
    }

    template <int bitDepth>
    static inline __m128i BoundaryFilter(__m128i a, __m128i b, __m128i c)
    {
        b = _mm_sub_epi16(b, c);
        b = _mm_srai_epi16(b, 1);
        a = _mm_add_epi16(a, b);
        a = _mm_max_epi16(a, _mm_set1_epi16(0));
        a = _mm_min_epi16(a, _mm_set1_epi16((1 << bitDepth)-1));

        return a;
    }

    //
    // mode 10,26 (copy)
    // Src is unaligned, Dst is aligned
    template <int width, int bitDepth>
    static inline void PredAngle10(Ipp16u *pSrc1, Ipp16u *pSrc2, Ipp16u *pDst1, Ipp16u *pDst2)
    {
        switch (width)
        {
        case 4:
            {
                __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[1]);
                __m128i r1 = _mm_loadu_si128((__m128i *)&pSrc1[9]);
                __m128i s0;

                __m128i t0 = _mm_set1_epi16(pSrc1[9]);
                __m128i t1 = _mm_set1_epi16(pSrc1[1]);
                __m128i u0 = _mm_set1_epi16(pSrc1[0]);

                t0 = BoundaryFilter<bitDepth>(t0, r0, u0);
                t1 = BoundaryFilter<bitDepth>(t1, r1, u0);

                s0 = _mm_unpacklo_epi64(r1, r1);    // [8 9 10 11 8 9 10 11]
                r0 = _mm_unpacklo_epi64(r0, r0);    // [0 1 2 3 0 1 2 3]

                _mm_store_si128((__m128i *)&pDst1[0], s0);
                _mm_store_si128((__m128i *)&pDst1[8], s0);

                _mm_store_si128((__m128i *)&pDst2[0], r0);
                _mm_store_si128((__m128i *)&pDst2[8], r0);

                pDst1[0*4] = (Ipp16u)_mm_extract_epi16(t0, 0);
                pDst1[1*4] = (Ipp16u)_mm_extract_epi16(t0, 1);
                pDst1[2*4] = (Ipp16u)_mm_extract_epi16(t0, 2);
                pDst1[3*4] = (Ipp16u)_mm_extract_epi16(t0, 3);

                pDst2[0*4] = (Ipp16u)_mm_extract_epi16(t1, 0);
                pDst2[1*4] = (Ipp16u)_mm_extract_epi16(t1, 1);
                pDst2[2*4] = (Ipp16u)_mm_extract_epi16(t1, 2);
                pDst2[3*4] = (Ipp16u)_mm_extract_epi16(t1, 3);
            }
            break;
        case 8:
            {
                __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[1]);
                __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[1]);

                __m128i t0 = _mm_set1_epi16(pSrc2[1]);
                __m128i t1 = _mm_set1_epi16(pSrc1[1]);
                __m128i u0 = _mm_set1_epi16(pSrc1[0]);

                t0 = BoundaryFilter<bitDepth>(t0, r0, u0);
                t1 = BoundaryFilter<bitDepth>(t1, s0, u0);

                for (int i = 0; i < 8; i++)
                {
                    _mm_store_si128((__m128i *)&pDst1[i*8], s0);
                    _mm_store_si128((__m128i *)&pDst2[i*8], r0);
                }

                pDst1[0*8] = (Ipp16u)_mm_extract_epi16(t0, 0);
                pDst1[1*8] = (Ipp16u)_mm_extract_epi16(t0, 1);
                pDst1[2*8] = (Ipp16u)_mm_extract_epi16(t0, 2);
                pDst1[3*8] = (Ipp16u)_mm_extract_epi16(t0, 3);
                pDst1[4*8] = (Ipp16u)_mm_extract_epi16(t0, 4);
                pDst1[5*8] = (Ipp16u)_mm_extract_epi16(t0, 5);
                pDst1[6*8] = (Ipp16u)_mm_extract_epi16(t0, 6);
                pDst1[7*8] = (Ipp16u)_mm_extract_epi16(t0, 7);

                pDst2[0*8] = (Ipp16u)_mm_extract_epi16(t1, 0);
                pDst2[1*8] = (Ipp16u)_mm_extract_epi16(t1, 1);
                pDst2[2*8] = (Ipp16u)_mm_extract_epi16(t1, 2);
                pDst2[3*8] = (Ipp16u)_mm_extract_epi16(t1, 3);
                pDst2[4*8] = (Ipp16u)_mm_extract_epi16(t1, 4);
                pDst2[5*8] = (Ipp16u)_mm_extract_epi16(t1, 5);
                pDst2[6*8] = (Ipp16u)_mm_extract_epi16(t1, 6);
                pDst2[7*8] = (Ipp16u)_mm_extract_epi16(t1, 7);
            }
            break;
        case 16:
            {
                __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[1]);
                __m128i r1 = _mm_loadu_si128((__m128i *)&pSrc1[9]);
                __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[1]);
                __m128i s1 = _mm_loadu_si128((__m128i *)&pSrc2[9]);

                __m128i t0 = _mm_set1_epi16(pSrc2[1]);
                __m128i t1 = _mm_set1_epi16(pSrc1[1]);
                __m128i u0 = _mm_set1_epi16(pSrc1[0]);
                __m128i t2, t3;

                t3 = BoundaryFilter<bitDepth>(t1, s1, u0);
                t2 = BoundaryFilter<bitDepth>(t1, s0, u0);
                t1 = BoundaryFilter<bitDepth>(t0, r1, u0);
                t0 = BoundaryFilter<bitDepth>(t0, r0, u0);

                for (int i = 0; i < 16; i++)
                {
                    _mm_store_si128((__m128i *)&pDst1[i*16+0], s0);
                    _mm_store_si128((__m128i *)&pDst1[i*16+8], s1);

                    _mm_store_si128((__m128i *)&pDst2[i*16+0], r0);
                    _mm_store_si128((__m128i *)&pDst2[i*16+8], r1);
                }

                pDst1[0*16]  = (Ipp16u)_mm_extract_epi16(t0, 0);
                pDst1[1*16]  = (Ipp16u)_mm_extract_epi16(t0, 1);
                pDst1[2*16]  = (Ipp16u)_mm_extract_epi16(t0, 2);
                pDst1[3*16]  = (Ipp16u)_mm_extract_epi16(t0, 3);
                pDst1[4*16]  = (Ipp16u)_mm_extract_epi16(t0, 4);
                pDst1[5*16]  = (Ipp16u)_mm_extract_epi16(t0, 5);
                pDst1[6*16]  = (Ipp16u)_mm_extract_epi16(t0, 6);
                pDst1[7*16]  = (Ipp16u)_mm_extract_epi16(t0, 7);
                pDst1[8*16]  = (Ipp16u)_mm_extract_epi16(t1, 0);
                pDst1[9*16]  = (Ipp16u)_mm_extract_epi16(t1, 1);
                pDst1[10*16] = (Ipp16u)_mm_extract_epi16(t1, 2);
                pDst1[11*16] = (Ipp16u)_mm_extract_epi16(t1, 3);
                pDst1[12*16] = (Ipp16u)_mm_extract_epi16(t1, 4);
                pDst1[13*16] = (Ipp16u)_mm_extract_epi16(t1, 5);
                pDst1[14*16] = (Ipp16u)_mm_extract_epi16(t1, 6);
                pDst1[15*16] = (Ipp16u)_mm_extract_epi16(t1, 7);

                pDst2[0*16]  = (Ipp16u)_mm_extract_epi16(t2, 0);
                pDst2[1*16]  = (Ipp16u)_mm_extract_epi16(t2, 1);
                pDst2[2*16]  = (Ipp16u)_mm_extract_epi16(t2, 2);
                pDst2[3*16]  = (Ipp16u)_mm_extract_epi16(t2, 3);
                pDst2[4*16]  = (Ipp16u)_mm_extract_epi16(t2, 4);
                pDst2[5*16]  = (Ipp16u)_mm_extract_epi16(t2, 5);
                pDst2[6*16]  = (Ipp16u)_mm_extract_epi16(t2, 6);
                pDst2[7*16]  = (Ipp16u)_mm_extract_epi16(t2, 7);
                pDst2[8*16]  = (Ipp16u)_mm_extract_epi16(t3, 0);
                pDst2[9*16]  = (Ipp16u)_mm_extract_epi16(t3, 1);
                pDst2[10*16] = (Ipp16u)_mm_extract_epi16(t3, 2);
                pDst2[11*16] = (Ipp16u)_mm_extract_epi16(t3, 3);
                pDst2[12*16] = (Ipp16u)_mm_extract_epi16(t3, 4);
                pDst2[13*16] = (Ipp16u)_mm_extract_epi16(t3, 5);
                pDst2[14*16] = (Ipp16u)_mm_extract_epi16(t3, 6);
                pDst2[15*16] = (Ipp16u)_mm_extract_epi16(t3, 7);
            }
            break;
        case 32:
            {
                __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[ 1]);
                __m128i r1 = _mm_loadu_si128((__m128i *)&pSrc1[ 9]);
                __m128i r2 = _mm_loadu_si128((__m128i *)&pSrc1[17]);
                __m128i r3 = _mm_loadu_si128((__m128i *)&pSrc1[25]);

                __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[ 1]);
                __m128i s1 = _mm_loadu_si128((__m128i *)&pSrc2[ 9]);
                __m128i s2 = _mm_loadu_si128((__m128i *)&pSrc2[17]);
                __m128i s3 = _mm_loadu_si128((__m128i *)&pSrc2[25]);

                for (int i = 0; i < 32; i++)
                {
                    _mm_store_si128((__m128i *)&pDst1[i*32+ 0], s0);
                    _mm_store_si128((__m128i *)&pDst1[i*32+ 8], s1);
                    _mm_store_si128((__m128i *)&pDst1[i*32+16], s2);
                    _mm_store_si128((__m128i *)&pDst1[i*32+24], s3);

                    _mm_store_si128((__m128i *)&pDst2[i*32+ 0], r0);
                    _mm_store_si128((__m128i *)&pDst2[i*32+ 8], r1);
                    _mm_store_si128((__m128i *)&pDst2[i*32+16], r2);
                    _mm_store_si128((__m128i *)&pDst2[i*32+24], r3);
                }
            }
            break;
        }
    }

    // Src is unaligned, Dst is aligned
    template <int width>
    static void CopyLine(Ipp16u *pSrc1, Ipp16u *pSrc2, Ipp16u *pDst1, Ipp16u *pDst2)
    {
        switch (width) {
        case 4:
            {
                __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[1]); // load [1-8]
                __m128i r1 = _mm_loadu_si128((__m128i *)&pSrc1[9]); // load [9-16]

                _mm_store_si128((__m128i *)&pDst1[0], r1);          // store [9-16]
                _mm_store_si128((__m128i *)&pDst1[8], r0);          // store [1-8]
            }
            break;
        case 8:
            {
                __m256i r0, r1;

                r0 = _mm256_loadu_si256((__m256i *)&pSrc1[1]);
                r1 = _mm256_loadu_si256((__m256i *)&pSrc2[1]);

                _mm256_store_si256((__m256i *)&pDst2[0], r0);
                _mm256_store_si256((__m256i *)&pDst1[0], r1);
            }
            break;
        case 16:
            {
                __m256i r0, r1, r2, r3;

                r0 = _mm256_loadu_si256((__m256i *)&pSrc1[ 1]);
                r1 = _mm256_loadu_si256((__m256i *)&pSrc1[17]);
                r2 = _mm256_loadu_si256((__m256i *)&pSrc2[ 1]);
                r3 = _mm256_loadu_si256((__m256i *)&pSrc2[17]);

                _mm256_store_si256((__m256i *)&pDst2[ 0], r0);
                _mm256_store_si256((__m256i *)&pDst2[16], r1);
                _mm256_store_si256((__m256i *)&pDst1[ 0], r2);
                _mm256_store_si256((__m256i *)&pDst1[16], r3);
            }
            break;
        case 32:
            {
                __m256i r0, r1, r2, r3, r4, r5, r6, r7;

                r0 = _mm256_loadu_si256((__m256i *)&pSrc1[ 1]);
                r1 = _mm256_loadu_si256((__m256i *)&pSrc1[17]);
                r2 = _mm256_loadu_si256((__m256i *)&pSrc1[33]);
                r3 = _mm256_loadu_si256((__m256i *)&pSrc1[49]);

                r4 = _mm256_loadu_si256((__m256i *)&pSrc2[ 1]);
                r5 = _mm256_loadu_si256((__m256i *)&pSrc2[17]);
                r6 = _mm256_loadu_si256((__m256i *)&pSrc2[33]);
                r7 = _mm256_loadu_si256((__m256i *)&pSrc2[49]);

                _mm256_store_si256((__m256i *)&pDst2[ 0], r0);
                _mm256_store_si256((__m256i *)&pDst2[16], r1);
                _mm256_store_si256((__m256i *)&pDst2[32], r2);
                _mm256_store_si256((__m256i *)&pDst2[48], r3);

                _mm256_store_si256((__m256i *)&pDst1[ 0], r4);
                _mm256_store_si256((__m256i *)&pDst1[16], r5);
                _mm256_store_si256((__m256i *)&pDst1[32], r6);
                _mm256_store_si256((__m256i *)&pDst1[48], r7);
            }
            break;
        }
    }

    // Src is unaligned, Dst is aligned
    template <int width, int mode>
    static void ProjLine(Ipp16u *pSrc1, Ipp16u *pSrc2, Ipp16u *pDst1, Ipp16u *pDst2)
    {
        switch (width) {
        case 4:
            {

                __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[0]);
                __m128i r1 = _mm_loadu_si128((__m128i *)&pSrc1[8]);

                __m128i t0 = _mm_shuffle_epi8(r0, _mm_load_si128((__m128i *)proj_4x4[mode-11][0]));
                __m128i t1 = _mm_shuffle_epi8(r1, _mm_load_si128((__m128i *)proj_4x4[mode-11][1]));
                t0 = _mm_or_si128(t0, t1);
                _mm_store_si128((__m128i *)&pDst1[0], t0);

                r0 = _mm_shuffle_epi8(r0, _mm_load_si128((__m128i *)proj_4x4[mode-11][2]));
                r1 = _mm_shuffle_epi8(r1, _mm_load_si128((__m128i *)proj_4x4[mode-11][3]));
                r0 = _mm_or_si128(r0, r1);
                _mm_store_si128((__m128i *)&pDst1[8], r0);
            }
            break;
        case 8:
            {
                __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[0]);
                __m128i r1 = _mm_loadu_si128((__m128i *)&pSrc1[8]);
                __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[0]);
                __m128i s1 = _mm_loadu_si128((__m128i *)&pSrc2[8]);

                r1 = _mm_alignr_epi8(r1, r0, 2);    // [1 2 3 4 5 6 7 8]
                s1 = _mm_alignr_epi8(s1, s0, 2);    // [1 2 3 4 5 6 7 8]

                r0 = _mm_shuffle_epi8(r1, _mm_load_si128((__m128i *)proj_8x8[mode-11]));
                s0 = _mm_shuffle_epi8(s1, _mm_load_si128((__m128i *)proj_8x8[mode-11]));

                r0 = _mm_insert_epi16(r0, pSrc1[0], 7);  // pDst1[7] = pSrc1[0];
                s0 = _mm_insert_epi16(s0, pSrc1[0], 7);  // pDst2[7] = pSrc1[0];

                _mm_store_si128((__m128i *)&pDst1[0], r0);
                _mm_store_si128((__m128i *)&pDst1[8], s1);
                _mm_store_si128((__m128i *)&pDst2[0], s0);
                _mm_store_si128((__m128i *)&pDst2[8], r1);
            }
            break;
        case 16:
            {
                __m128i r0a = _mm_loadu_si128((__m128i *)&pSrc1[0]);
                __m128i r0b = _mm_loadu_si128((__m128i *)&pSrc1[8]);
                __m128i t0a = _mm_shuffle_epi8(r0b, _mm_load_si128((__m128i *)proj_16x16[mode-11][0]));
                __m128i t0b = _mm_or_si128( _mm_shuffle_epi8(r0a, _mm_load_si128((__m128i *)proj_16x16[mode-11][1])), _mm_shuffle_epi8(r0b, _mm_load_si128((__m128i *)proj_16x16[mode-11][2])) );

                __m128i s0a = _mm_loadu_si128((__m128i *)&pSrc2[0]);
                __m128i s0b = _mm_loadu_si128((__m128i *)&pSrc2[8]);
                __m128i t1a = _mm_shuffle_epi8(s0b, _mm_load_si128((__m128i *)proj_16x16[mode-11][0]));
                __m128i t1b = _mm_or_si128( _mm_shuffle_epi8(s0a, _mm_load_si128((__m128i *)proj_16x16[mode-11][1])), _mm_shuffle_epi8(s0b, _mm_load_si128((__m128i *)proj_16x16[mode-11][2])) );
                t1b = _mm_insert_epi16(t1b, pSrc1[0], 7); // pDst2[15] = pSrc1[0];

                _mm_store_si128((__m128i *)&pDst1[0], t0a);
                _mm_store_si128((__m128i *)&pDst1[8], t0b);
                _mm_store_si128((__m128i *)&pDst2[0], t1a);
                _mm_store_si128((__m128i *)&pDst2[8], t1b);

                r0a = _mm_alignr_epi8(r0b, r0a, 2);
                r0b = _mm_srli_si128(r0b, 2);
                r0b = _mm_insert_epi16(r0b, pSrc1[16], 7);
                s0a = _mm_alignr_epi8(s0b, s0a, 2);
                s0b = _mm_srli_si128(s0b, 2);
                s0b = _mm_insert_epi16(s0b, pSrc2[16], 7);

                _mm_store_si128((__m128i *)&pDst1[16], s0a);
                _mm_store_si128((__m128i *)&pDst1[24], s0b);
                _mm_store_si128((__m128i *)&pDst2[16], r0a);
                _mm_store_si128((__m128i *)&pDst2[24], r0b);
            }
            break;
        case 32:
            {
                int mOff = 3*(mode - 11);

                __m128i r0a = _mm_loadu_si128((__m128i *)&pSrc1[ 0]);
                __m128i r0b = _mm_loadu_si128((__m128i *)&pSrc1[ 8]);
                __m128i r1a = _mm_loadu_si128((__m128i *)&pSrc1[16]);
                __m128i r1b = _mm_loadu_si128((__m128i *)&pSrc1[24]);

                __m128i t0a, t0b, t1a, t1b;

                t0a = _mm_shuffle_epi8(r1b, _mm_load_si128((__m128i *)proj_32x32[mOff+0][1]));
                t0b = _mm_or_si128( _mm_shuffle_epi8(r1a, _mm_load_si128((__m128i *)proj_32x32[mOff+0][2])), _mm_shuffle_epi8(r1b, _mm_load_si128((__m128i *)proj_32x32[mOff+0][3])) );
                _mm_store_si128((__m128i *)&pDst1[0], t0a);
                _mm_store_si128((__m128i *)&pDst1[8], t0b);

                t0a = _mm_shuffle_epi8(r0b, _mm_load_si128((__m128i *)proj_32x32[mOff+1][1]));
                t0b = _mm_or_si128( _mm_shuffle_epi8(r0a, _mm_load_si128((__m128i *)proj_32x32[mOff+1][2])), _mm_shuffle_epi8(r0b, _mm_load_si128((__m128i *)proj_32x32[mOff+1][3])) );

                t1a = _mm_or_si128( _mm_shuffle_epi8(r1a, _mm_load_si128((__m128i *)proj_32x32[mOff+2][0])), _mm_shuffle_epi8(r1b, _mm_load_si128((__m128i *)proj_32x32[mOff+2][1])) );
                t1b = _mm_or_si128( _mm_shuffle_epi8(r1a, _mm_load_si128((__m128i *)proj_32x32[mOff+2][2])), _mm_shuffle_epi8(r1b, _mm_load_si128((__m128i *)proj_32x32[mOff+2][3])) );

                t1a = _mm_or_si128(t0a, t1a);
                t1b = _mm_or_si128(t0b, t1b);
                _mm_store_si128((__m128i *)&pDst1[16], t1a);
                _mm_store_si128((__m128i *)&pDst1[24], t1b);

                r0a = _mm_alignr_epi8(r0b, r0a, 2);
                r0b = _mm_alignr_epi8(r1a, r0b, 2);
                r1a = _mm_alignr_epi8(r1b, r1a, 2);
                r1b = _mm_srli_si128(r1b, 2);
                r1b = _mm_insert_epi16(r1b, pSrc1[32], 7);

                _mm_store_si128((__m128i *)&pDst2[32], r0a);
                _mm_store_si128((__m128i *)&pDst2[40], r0b);
                _mm_store_si128((__m128i *)&pDst2[48], r1a);
                _mm_store_si128((__m128i *)&pDst2[56], r1b);

                __m128i s0a = _mm_loadu_si128((__m128i *)&pSrc2[ 0]);
                __m128i s0b = _mm_loadu_si128((__m128i *)&pSrc2[ 8]);
                __m128i s1a = _mm_loadu_si128((__m128i *)&pSrc2[16]);
                __m128i s1b = _mm_loadu_si128((__m128i *)&pSrc2[24]);

                t0a = _mm_shuffle_epi8(s1b, _mm_load_si128((__m128i *)proj_32x32[mOff+0][1]));
                t0b = _mm_or_si128( _mm_shuffle_epi8(s1a, _mm_load_si128((__m128i *)proj_32x32[mOff+0][2])), _mm_shuffle_epi8(s1b, _mm_load_si128((__m128i *)proj_32x32[mOff+0][3])) );
                _mm_store_si128((__m128i *)&pDst2[0], t0a);
                _mm_store_si128((__m128i *)&pDst2[8], t0b);

                t0a = _mm_shuffle_epi8(s0b, _mm_load_si128((__m128i *)proj_32x32[mOff+1][1]));
                t0b = _mm_or_si128( _mm_shuffle_epi8(s0a, _mm_load_si128((__m128i *)proj_32x32[mOff+1][2])), _mm_shuffle_epi8(s0b, _mm_load_si128((__m128i *)proj_32x32[mOff+1][3])) );

                t1a = _mm_or_si128( _mm_shuffle_epi8(s1a, _mm_load_si128((__m128i *)proj_32x32[mOff+2][0])), _mm_shuffle_epi8(s1b, _mm_load_si128((__m128i *)proj_32x32[mOff+2][1])) );
                t1b = _mm_or_si128( _mm_shuffle_epi8(s1a, _mm_load_si128((__m128i *)proj_32x32[mOff+2][2])), _mm_shuffle_epi8(s1b, _mm_load_si128((__m128i *)proj_32x32[mOff+2][3])) );

                t1a = _mm_or_si128(t0a, t1a);
                t1b = _mm_or_si128(t0b, t1b);
                t1b = _mm_insert_epi16(t1b, pSrc1[0], 7); // pDst2[31] = pSrc1[0];
                _mm_store_si128((__m128i *)&pDst2[16], t1a);
                _mm_store_si128((__m128i *)&pDst2[24], t1b);

                s0a = _mm_alignr_epi8(s0b, s0a, 2);
                s0b = _mm_alignr_epi8(s1a, s0b, 2);
                s1a = _mm_alignr_epi8(s1b, s1a, 2);
                s1b = _mm_srli_si128(s1b, 2);
                s1b = _mm_insert_epi16(s1b, pSrc2[32], 7);

                _mm_store_si128((__m128i *)&pDst1[32], s0a);
                _mm_store_si128((__m128i *)&pDst1[40], s0b);
                _mm_store_si128((__m128i *)&pDst1[48], s1a);
                _mm_store_si128((__m128i *)&pDst1[56], s1b);
            }
            break;
        }
    }

    //
    // mode 18 (shift right)
    // Src is unaligned, Dst is aligned
    template <int width>
    static inline void PredAngle18(Ipp16u *pSrc1, Ipp16u *pSrc2, Ipp16u *pDst2)
    {
        /* use common kernel for mode 18 */
        MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_NoTranspose_16u)(18, pSrc1, pDst2, width, width);
        return;
    }

    template <typename PixType, int bitDepth>
    static void PredictIntra_Ang_All_4x4_Even(PixType* PredPel, PixType* /*FiltPel*/, PixType* pels)
    {
        ALIGN_DECL(64) PixType ref1[16];  // input for mode < 18
        ALIGN_DECL(64) PixType ref2[16];  // input for mode > 18

        PixType *PredPel2 = PredPel + 2 * 4;
        //Ipp16u *FiltPel2 = FiltPel + 2 * 4;
        PixType (*buf)[4*4] = (PixType(*)[4*4])pels;

        // unfiltered
        CopyLine<4>(PredPel, PredPel2, ref1, ref2);
        PredAngle2<4>(ref1, ref2, buf[2-2], buf[34-2]);
        PredAngle<4,4>(ref1, ref2, buf[4-2], buf[32-2]);
        PredAngle<4,6>(ref1, ref2, buf[6-2], buf[30-2]);
        PredAngle<4,8>(ref1, ref2, buf[8-2], buf[28-2]);

        PredAngle10<4, bitDepth>(PredPel, PredPel2, buf[10-2], buf[26-2]);

        ProjLine<4,12>(PredPel, PredPel2, ref1, ref2);
        PredAngle<4,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
        ProjLine<4,14>(PredPel, PredPel2, ref1, ref2);
        PredAngle<4,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
        ProjLine<4,16>(PredPel, PredPel2, ref1, ref2);
        PredAngle<4,16>(ref1, ref2, buf[16-2], buf[36-16-2]);
        PredAngle18<4>(PredPel, PredPel2, buf[18-2]);
    }

    template <typename PixType, int bitDepth>
    static void PredictIntra_Ang_All_8x8_Even(PixType* PredPel, PixType* FiltPel, PixType* pels)
    {
        ALIGN_DECL(64) PixType ref1[16];  // input for mode < 18
        ALIGN_DECL(64) PixType ref2[16];  // input for mode > 18

        PixType *PredPel2 = PredPel + 2 * 8;
        PixType *FiltPel2 = FiltPel + 2 * 8;
        PixType (*buf)[8*8] = (PixType(*)[8*8])pels;

        // filtered
        CopyLine<8>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle2<8>(ref1, ref2, buf[2-2], buf[34-2]);

        // unfiltered
        CopyLine<8>(PredPel, PredPel2, ref1, ref2);
        PredAngle<8,4>(ref1, ref2, buf[4-2], buf[32-2]);
        PredAngle<8,6>(ref1, ref2, buf[6-2], buf[30-2]);
        PredAngle<8,8>(ref1, ref2, buf[8-2], buf[28-2]);

        PredAngle10<8, bitDepth>(PredPel, PredPel2, buf[10-2], buf[26-2]);

        ProjLine<8,12>(PredPel, PredPel2, ref1, ref2);
        PredAngle<8,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
        ProjLine<8,14>(PredPel, PredPel2, ref1, ref2);
        PredAngle<8,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
        ProjLine<8,16>(PredPel, PredPel2, ref1, ref2);
        PredAngle<8,16>(ref1, ref2, buf[16-2], buf[36-16-2]);

        // filtered
        PredAngle18<8>(FiltPel, FiltPel2, buf[18-2]);
    }

    template <typename PixType, int bitDepth>
    static void PredictIntra_Ang_All_16x16_Even(PixType* PredPel, PixType* FiltPel, PixType* pels)
    {
        ALIGN_DECL(64) PixType ref1[32];  // input for mode < 18
        ALIGN_DECL(64) PixType ref2[32];  // input for mode > 18

        PixType *PredPel2 = PredPel + 2 * 16;
        PixType *FiltPel2 = FiltPel + 2 * 16;
        PixType (*buf)[16*16] = (PixType(*)[16*16])pels;

        // filtered
        CopyLine<16>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle2<16>(ref1, ref2, buf[2-2], buf[34-2]);
        PredAngle<16,4>(ref1, ref2, buf[4-2], buf[32-2]);
        PredAngle<16,6>(ref1, ref2, buf[6-2], buf[30-2]);
        PredAngle<16,8>(ref1, ref2, buf[8-2], buf[28-2]);

        // unfiltered
        CopyLine<16>(PredPel, PredPel2, ref1, ref2);

        PredAngle10<16, bitDepth>(PredPel, PredPel2, buf[10-2], buf[26-2]);

        // filtered
        ProjLine<16,12>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<16,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
        ProjLine<16,14>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<16,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
        ProjLine<16,16>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<16,16>(ref1, ref2, buf[16-2], buf[36-16-2]);

        PredAngle18<16>(FiltPel, FiltPel2, buf[18-2]);
    }

    template <typename PixType, int bitDepth>
    static void PredictIntra_Ang_All_32x32_Even(PixType* PredPel, PixType* FiltPel, PixType* pels)
    {
        ALIGN_DECL(64) PixType ref1[64];  // input for mode < 18
        ALIGN_DECL(64) PixType ref2[64];  // input for mode > 18

        PixType *PredPel2 = PredPel + 2 * 32;
        PixType *FiltPel2 = FiltPel + 2 * 32;
        PixType (*buf)[32*32] = (PixType(*)[32*32])pels;

        // filtered
        CopyLine<32>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle2<32>(ref1, ref2, buf[2-2], buf[34-2]);
        PredAngle<32,4>(ref1, ref2, buf[4-2], buf[32-2]);
        PredAngle<32,6>(ref1, ref2, buf[6-2], buf[30-2]);
        PredAngle<32,8>(ref1, ref2, buf[8-2], buf[28-2]);

        // unfiltered
        PredAngle10<32, bitDepth>(PredPel, PredPel2, buf[10-2], buf[26-2]);

        // filtered
        ProjLine<32,12>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<32,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
        ProjLine<32,14>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<32,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
        ProjLine<32,16>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<32,16>(ref1, ref2, buf[16-2], buf[36-16-2]);

        PredAngle18<32>(FiltPel, FiltPel2, buf[18-2]);
    }

    template <typename PixType, int bitDepth>
    static void PredictIntra_Ang_All_4x4(PixType* PredPel, PixType* /*FiltPel*/, PixType* pels)
    {
        ALIGN_DECL(64) PixType ref1[16];  // input for mode < 18
        ALIGN_DECL(64) PixType ref2[16];  // input for mode > 18

        PixType *PredPel2 = PredPel + 2 * 4;
        //PixType *FiltPel2 = FiltPel + 2 * 4;
        PixType (*buf)[4*4] = (PixType(*)[4*4])pels;

        // unfiltered
        CopyLine<4>(PredPel, PredPel2, ref1, ref2);
        PredAngle2<4>(ref1, ref2, buf[2-2], buf[34-2]);
        PredAngle<4,3>(ref1, ref2, buf[3-2], buf[33-2]);
        PredAngle<4,4>(ref1, ref2, buf[4-2], buf[32-2]);
        PredAngle<4,5>(ref1, ref2, buf[5-2], buf[31-2]);
        PredAngle<4,6>(ref1, ref2, buf[6-2], buf[30-2]);
        PredAngle<4,7>(ref1, ref2, buf[7-2], buf[29-2]);
        PredAngle<4,8>(ref1, ref2, buf[8-2], buf[28-2]);
        PredAngle<4,9>(ref1, ref2, buf[9-2], buf[27-2]);

        PredAngle10<4, bitDepth>(PredPel, PredPel2, buf[10-2], buf[26-2]);

        ProjLine<4,11>(PredPel, PredPel2, ref1, ref2);
        PredAngle<4,11>(ref1, ref2, buf[11-2], buf[36-11-2]);
        ProjLine<4,12>(PredPel, PredPel2, ref1, ref2);
        PredAngle<4,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
        ProjLine<4,13>(PredPel, PredPel2, ref1, ref2);
        PredAngle<4,13>(ref1, ref2, buf[13-2], buf[36-13-2]);
        ProjLine<4,14>(PredPel, PredPel2, ref1, ref2);
        PredAngle<4,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
        ProjLine<4,15>(PredPel, PredPel2, ref1, ref2);
        PredAngle<4,15>(ref1, ref2, buf[15-2], buf[36-15-2]);
        ProjLine<4,16>(PredPel, PredPel2, ref1, ref2);
        PredAngle<4,16>(ref1, ref2, buf[16-2], buf[36-16-2]);
        ProjLine<4,17>(PredPel, PredPel2, ref1, ref2);
        PredAngle<4,17>(ref1, ref2, buf[17-2], buf[36-17-2]);
        PredAngle18<4>(PredPel, PredPel2, buf[18-2]);
    }

    template <typename PixType, int bitDepth>
    static void PredictIntra_Ang_All_8x8(PixType* PredPel, PixType* FiltPel, PixType* pels)
    {
        ALIGN_DECL(64) PixType ref1[16];  // input for mode < 18
        ALIGN_DECL(64) PixType ref2[16];  // input for mode > 18

        PixType *PredPel2 = PredPel + 2 * 8;
        PixType *FiltPel2 = FiltPel + 2 * 8;
        PixType (*buf)[8*8] = (PixType(*)[8*8])pels;

        // filtered
        CopyLine<8>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle2<8>(ref1, ref2, buf[2-2], buf[34-2]);

        // unfiltered
        CopyLine<8>(PredPel, PredPel2, ref1, ref2);
        PredAngle<8,3>(ref1, ref2, buf[3-2], buf[33-2]);
        PredAngle<8,4>(ref1, ref2, buf[4-2], buf[32-2]);
        PredAngle<8,5>(ref1, ref2, buf[5-2], buf[31-2]);
        PredAngle<8,6>(ref1, ref2, buf[6-2], buf[30-2]);
        PredAngle<8,7>(ref1, ref2, buf[7-2], buf[29-2]);
        PredAngle<8,8>(ref1, ref2, buf[8-2], buf[28-2]);
        PredAngle<8,9>(ref1, ref2, buf[9-2], buf[27-2]);

        PredAngle10<8, bitDepth>(PredPel, PredPel2, buf[10-2], buf[26-2]);

        ProjLine<8,11>(PredPel, PredPel2, ref1, ref2);
        PredAngle<8,11>(ref1, ref2, buf[11-2], buf[36-11-2]);
        ProjLine<8,12>(PredPel, PredPel2, ref1, ref2);
        PredAngle<8,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
        ProjLine<8,13>(PredPel, PredPel2, ref1, ref2);
        PredAngle<8,13>(ref1, ref2, buf[13-2], buf[36-13-2]);
        ProjLine<8,14>(PredPel, PredPel2, ref1, ref2);
        PredAngle<8,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
        ProjLine<8,15>(PredPel, PredPel2, ref1, ref2);
        PredAngle<8,15>(ref1, ref2, buf[15-2], buf[36-15-2]);
        ProjLine<8,16>(PredPel, PredPel2, ref1, ref2);
        PredAngle<8,16>(ref1, ref2, buf[16-2], buf[36-16-2]);
        ProjLine<8,17>(PredPel, PredPel2, ref1, ref2);
        PredAngle<8,17>(ref1, ref2, buf[17-2], buf[36-17-2]);

        // filtered
        PredAngle18<8>(FiltPel, FiltPel2, buf[18-2]);
    }

    template <typename PixType, int bitDepth>
    static void PredictIntra_Ang_All_16x16(PixType* PredPel, PixType* FiltPel, PixType* pels)
    {
        ALIGN_DECL(64) PixType ref1[32];  // input for mode < 18
        ALIGN_DECL(64) PixType ref2[32];  // input for mode > 18

        PixType *PredPel2 = PredPel + 2 * 16;
        PixType *FiltPel2 = FiltPel + 2 * 16;
        PixType (*buf)[16*16] = (PixType(*)[16*16])pels;

        // filtered
        CopyLine<16>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle2<16>(ref1, ref2, buf[2-2], buf[34-2]);
        PredAngle<16,3>(ref1, ref2, buf[3-2], buf[33-2]);
        PredAngle<16,4>(ref1, ref2, buf[4-2], buf[32-2]);
        PredAngle<16,5>(ref1, ref2, buf[5-2], buf[31-2]);
        PredAngle<16,6>(ref1, ref2, buf[6-2], buf[30-2]);
        PredAngle<16,7>(ref1, ref2, buf[7-2], buf[29-2]);
        PredAngle<16,8>(ref1, ref2, buf[8-2], buf[28-2]);

        // unfiltered
        CopyLine<16>(PredPel, PredPel2, ref1, ref2);
        PredAngle<16,9>(ref1, ref2, buf[9-2], buf[27-2]);

        PredAngle10<16, bitDepth>(PredPel, PredPel2, buf[10-2], buf[26-2]);

        ProjLine<16,11>(PredPel, PredPel2, ref1, ref2);
        PredAngle<16,11>(ref1, ref2, buf[11-2], buf[36-11-2]);

        // filtered
        ProjLine<16,12>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<16,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
        ProjLine<16,13>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<16,13>(ref1, ref2, buf[13-2], buf[36-13-2]);
        ProjLine<16,14>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<16,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
        ProjLine<16,15>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<16,15>(ref1, ref2, buf[15-2], buf[36-15-2]);
        ProjLine<16,16>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<16,16>(ref1, ref2, buf[16-2], buf[36-16-2]);
        ProjLine<16,17>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<16,17>(ref1, ref2, buf[17-2], buf[36-17-2]);
        PredAngle18<16>(FiltPel, FiltPel2, buf[18-2]);
    }

    template <typename PixType, int bitDepth>
    static void PredictIntra_Ang_All_32x32(PixType* PredPel, PixType* FiltPel, PixType* pels)
    {
        ALIGN_DECL(64) PixType ref1[64];  // input for mode < 18
        ALIGN_DECL(64) PixType ref2[64];  // input for mode > 18

        PixType *PredPel2 = PredPel + 2 * 32;
        PixType *FiltPel2 = FiltPel + 2 * 32;
        PixType (*buf)[32*32] = (PixType(*)[32*32])pels;

        // filtered
        CopyLine<32>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle2<32>(ref1, ref2, buf[2-2], buf[34-2]);
        PredAngle<32,3>(ref1, ref2, buf[3-2], buf[33-2]);
        PredAngle<32,4>(ref1, ref2, buf[4-2], buf[32-2]);
        PredAngle<32,5>(ref1, ref2, buf[5-2], buf[31-2]);
        PredAngle<32,6>(ref1, ref2, buf[6-2], buf[30-2]);
        PredAngle<32,7>(ref1, ref2, buf[7-2], buf[29-2]);
        PredAngle<32,8>(ref1, ref2, buf[8-2], buf[28-2]);
        PredAngle<32,9>(ref1, ref2, buf[9-2], buf[27-2]);

        // unfiltered
        PredAngle10<32, bitDepth>(PredPel, PredPel2, buf[10-2], buf[26-2]);

        // filtered
        ProjLine<32,11>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<32,11>(ref1, ref2, buf[11-2], buf[36-11-2]);
        ProjLine<32,12>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<32,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
        ProjLine<32,13>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<32,13>(ref1, ref2, buf[13-2], buf[36-13-2]);
        ProjLine<32,14>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<32,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
        ProjLine<32,15>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<32,15>(ref1, ref2, buf[15-2], buf[36-15-2]);
        ProjLine<32,16>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<32,16>(ref1, ref2, buf[16-2], buf[36-16-2]);
        ProjLine<32,17>(FiltPel, FiltPel2, ref1, ref2);
        PredAngle<32,17>(ref1, ref2, buf[17-2], buf[36-17-2]);

        PredAngle18<32>(FiltPel, FiltPel2, buf[18-2]);
    }

    //
    // pels should be aligned, and large enough to hold pels[33][32*32]
    //

    void MAKE_NAME(h265_PredictIntra_Ang_All_Even_16u)(
        Ipp16u* PredPel,
        Ipp16u* FiltPel,
        Ipp16u* pels,
        Ipp32s width,
        Ipp32s bitDepth)
    {
        _mm256_zeroupper();

        if (bitDepth == 10) {
            switch (width) {
            case 4:  PredictIntra_Ang_All_4x4_Even  <Ipp16u, 10>(PredPel, FiltPel, pels);    break;
            case 8:  PredictIntra_Ang_All_8x8_Even  <Ipp16u, 10>(PredPel, FiltPel, pels);    break;
            case 16: PredictIntra_Ang_All_16x16_Even<Ipp16u, 10>(PredPel, FiltPel, pels);    break;
            case 32: PredictIntra_Ang_All_32x32_Even<Ipp16u, 10>(PredPel, FiltPel, pels);    break;
            }
        } else if (bitDepth == 9) {
            switch (width) {
            case 4:  PredictIntra_Ang_All_4x4_Even  <Ipp16u,  9>(PredPel, FiltPel, pels);    break;
            case 8:  PredictIntra_Ang_All_8x8_Even  <Ipp16u,  9>(PredPel, FiltPel, pels);    break;
            case 16: PredictIntra_Ang_All_16x16_Even<Ipp16u,  9>(PredPel, FiltPel, pels);    break;
            case 32: PredictIntra_Ang_All_32x32_Even<Ipp16u,  9>(PredPel, FiltPel, pels);    break;
            }
        } else if (bitDepth == 11) {
            switch (width) {
            case 4:  PredictIntra_Ang_All_4x4_Even  <Ipp16u,  11>(PredPel, FiltPel, pels);    break;
            case 8:  PredictIntra_Ang_All_8x8_Even  <Ipp16u,  11>(PredPel, FiltPel, pels);    break;
            case 16: PredictIntra_Ang_All_16x16_Even<Ipp16u,  11>(PredPel, FiltPel, pels);    break;
            case 32: PredictIntra_Ang_All_32x32_Even<Ipp16u,  11>(PredPel, FiltPel, pels);    break;
            }
        } else if (bitDepth == 12) {
            switch (width) {
            case 4:  PredictIntra_Ang_All_4x4_Even  <Ipp16u,  12>(PredPel, FiltPel, pels);    break;
            case 8:  PredictIntra_Ang_All_8x8_Even  <Ipp16u,  12>(PredPel, FiltPel, pels);    break;
            case 16: PredictIntra_Ang_All_16x16_Even<Ipp16u,  12>(PredPel, FiltPel, pels);    break;
            case 32: PredictIntra_Ang_All_32x32_Even<Ipp16u,  12>(PredPel, FiltPel, pels);    break;
            }
        }

    }

    void MAKE_NAME(h265_PredictIntra_Ang_All_16u)(
        Ipp16u* PredPel,
        Ipp16u* FiltPel,
        Ipp16u* pels,
        Ipp32s width,
        Ipp32s bitDepth)
    {
        _mm256_zeroupper();

        if (bitDepth == 10) {
            switch (width) {
            case 4:  PredictIntra_Ang_All_4x4  <Ipp16u, 10>(PredPel, FiltPel, pels);    break;
            case 8:  PredictIntra_Ang_All_8x8  <Ipp16u, 10>(PredPel, FiltPel, pels);    break;
            case 16: PredictIntra_Ang_All_16x16<Ipp16u, 10>(PredPel, FiltPel, pels);    break;
            case 32: PredictIntra_Ang_All_32x32<Ipp16u, 10>(PredPel, FiltPel, pels);    break;
            }
        } else if (bitDepth == 9) {
            switch (width) {
            case 4:  PredictIntra_Ang_All_4x4  <Ipp16u,  9>(PredPel, FiltPel, pels);    break;
            case 8:  PredictIntra_Ang_All_8x8  <Ipp16u,  9>(PredPel, FiltPel, pels);    break;
            case 16: PredictIntra_Ang_All_16x16<Ipp16u,  9>(PredPel, FiltPel, pels);    break;
            case 32: PredictIntra_Ang_All_32x32<Ipp16u,  9>(PredPel, FiltPel, pels);    break;
            }
        } else if (bitDepth == 11) {
            switch (width) {
            case 4:  PredictIntra_Ang_All_4x4  <Ipp16u,  11>(PredPel, FiltPel, pels);    break;
            case 8:  PredictIntra_Ang_All_8x8  <Ipp16u,  11>(PredPel, FiltPel, pels);    break;
            case 16: PredictIntra_Ang_All_16x16<Ipp16u,  11>(PredPel, FiltPel, pels);    break;
            case 32: PredictIntra_Ang_All_32x32<Ipp16u,  11>(PredPel, FiltPel, pels);    break;
            }
        } else if (bitDepth == 12) {
            switch (width) {
            case 4:  PredictIntra_Ang_All_4x4  <Ipp16u,  12>(PredPel, FiltPel, pels);    break;
            case 8:  PredictIntra_Ang_All_8x8  <Ipp16u,  12>(PredPel, FiltPel, pels);    break;
            case 16: PredictIntra_Ang_All_16x16<Ipp16u,  12>(PredPel, FiltPel, pels);    break;
            case 32: PredictIntra_Ang_All_32x32<Ipp16u,  12>(PredPel, FiltPel, pels);    break;
            }
        }
    }

}; // namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
