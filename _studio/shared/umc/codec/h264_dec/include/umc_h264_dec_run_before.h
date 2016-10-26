//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2012 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_DEC_RUN_BEFORE_H
#define __UMC_H264_DEC_RUN_BEFORE_H

namespace UMC
{

// Table 9-10 - Tables for run_before
// zerosLeft = 1
/*
0 1
1 0
*/

static
Ipp32s run_before_map_1[] =
{
1, /* max bits */
1,  /* total subtables */
1,/* subtable sizes */

2, /* 1-bit codes */
0x0001, 0, 0x0000, 1,

-1
};

// Table 9-10 - Tables for run_before
// zerosLeft = 2
/*
0    1

1    01
2    00
*/

static
Ipp32s run_before_map_2[] =
{
2, /* max bits */
1,  /* total subtables */
2,/* subtable sizes */

1, /* 1-bit codes */
0x0001, 0,

2, /* 2-bit codes */
0x0001, 1, 0x0000, 2,

-1
};

// Table 9-10 - Tables for run_before
// zerosLeft = 3
/*
3    00
2    01
1    10
0    11
*/

static
Ipp32s run_before_map_3[] =
{
2, /* max bits */
1,  /* total subtables */
2,/* subtable sizes */

0, /* 1-bit codes */

4, /* 2-bit codes */
0x0000, 3, 0x0001, 2, 0x0002, 1, 0x0003, 0,

-1
};

// Table 9-10 - Tables for run_before
// zerosLeft = 4
/*
2    01
1    10
0    11

4    000
3    001
*/

static
Ipp32s run_before_map_4[] =
{
3, /* max bits */
1,  /* total subtables */
3,/* subtable sizes */

0, /* 1-bit codes */

3, /* 2-bit codes */
0x0001, 2, 0x0002, 1, 0x0003, 0,

2, /* 3-bit codes */
0x0000, 4, 0x0001, 3,

-1
};

// Table 9-10 - Tables for run_before
// zerosLeft = 5
/*
1    10
0    11

5    000
4    001
3    010
2    011
*/

static
Ipp32s run_before_map_5[] =
{
3, /* max bits */
1,  /* total subtables */
3,/* subtable sizes */

0, /* 1-bit codes */

2, /* 2-bit codes */
0x0002, 1, 0x0003, 0,

4, /* 3-bit codes */
0x0000, 5, 0x0001, 4, 0x0002, 3, 0x0003, 2,

-1
};

// Table 9-10 - Tables for run_before
// zerosLeft = 6
/*
0    11

1    000
2    001
4    010
3    011
6    100
5    101

*/

static
Ipp32s run_before_map_6[] =
{
3, /* max bits */
1,  /* total subtables */
3,/* subtable sizes */

0, /* 1-bit codes */

1, /* 2-bit codes */
0x0003, 0,

6, /* 3-bit codes */
0x0000, 1, 0x0001, 2, 0x0002, 4, 0x0003, 3,
0x0004, 6, 0x0005, 5,

-1
};

// Table 9-10 - Tables for run_before
// zerosLeft > 6
/*
6    001
5    010
4    011
3    100
2    101
1    110
0    111

7    0001

8    0000 1

9    0000 01

10    0000 001

11    0000 0001

12    0000 0000 1

13    0000 0000 01

14    0000 0000 001

*/

static
Ipp32s run_before_map_6p[] =
{
11, /* max bits */
1,  /* total subtables */
11,/* subtable sizes */

0, /* 1-bit codes */
0, /* 2-bit codes */

7, /* 3-bit codes */
0x0001, 6, 0x0002, 5, 0x0003, 4, 0x0004, 3,
0x0005, 2, 0x0006, 1, 0x0007, 0,

1, /* 4-bit codes */
0x0001, 7,

1, /* 5-bit codes */
0x0001, 8,

1, /* 6-bit codes */
0x0001, 9,

1, /* 7-bit codes */
0x0001, 10,

1, /* 8-bit codes */
0x0001, 11,

1, /* 9-bit codes */
0x0001, 12,

1, /* 10-bit codes */
0x0001, 13,

1, /* 11-bit codes */
0x0001, 14,

-1
};

} // namespace UMC

#endif //__UMC_H264_DEC_RUN_BEFORE_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
