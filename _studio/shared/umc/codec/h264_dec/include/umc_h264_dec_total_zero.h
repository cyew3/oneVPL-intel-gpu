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

#ifndef __UMC_H264_DEC_TOTAL_ZEROS_H__
#define __UMC_H264_DEC_TOTAL_ZEROS_H__

namespace UMC
{

//Table 9 7 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 1
/*
0    1

2    010
1    011

4    0010
3    0011

6    0001 0
5    0001 1

8    0000 10
7    0000 11

10    0000 010
9    0000 011

12    0000 0010
11    0000 0011

15    0000 0000 1
14    0000 0001 0
13    0000 0001 1
*/

static
Ipp32s total_zeros_map_1[] =
{
9, /* max bits */
1,  /* total subtables */
9,/* subtable sizes */

1, /* 1-bit codes */
0x0001, 0,

0, /* 2-bit codes */

2, /* 3-bit codes */
0x0002, 2, 0x0003, 1,

2, /* 4-bit codes */
0x0002, 4, 0x0003, 3,

2, /* 5-bit codes */
0x0002, 6, 0x0003, 5,

2, /* 6-bit codes */
0x0002, 8, 0x0003, 7,

2, /* 7-bit codes */
0x0002, 10, 0x0003, 9,

2, /* 8-bit codes */
0x0002, 12, 0x0003, 11,

3, /* 9-bit codes */
0x0001, 15, 0x0002, 14, 0x0003, 13,

-1
};

//Table 9 7 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 2

/*
4    011
3    100
2    101
1    110
0    111

8    0010
7    0011
6    0100
5    0101

10    0001 0
9    0001 1

14    0000 00
13    0000 01
12    0000 10
11    0000 11
*/

static
Ipp32s total_zeros_map_2[] =
{
6, /* max bits */
1,  /* total subtables */
6,/* subtable sizes */

0, /* 1-bit codes */
0, /* 2-bit codes */

5, /* 3-bit codes */
0x0003, 4, 0x0004, 3, 0x0005, 2, 0x0006, 1,
0x0007, 0,

4, /* 4-bit codes */
0x0002, 8, 0x0003, 7, 0x0004, 6, 0x0005, 5,

2, /* 5-bit codes */
0x0002, 10, 0x0003, 9,

4, /* 6-bit codes */
0x0000, 14, 0x0001, 13, 0x0002, 12, 0x0003, 11,

-1
};

//Table 9 7 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 3

/*
7    011
6    100
3    101
2    110
1    111

8    0010
5    0011
4    0100
0    0101

12    0000 1
10    0001 0
9    0001 1

13    0000 00
11    0000 01
*/

static
Ipp32s total_zeros_map_3[] =
{
6, /* max bits */
1,  /* total subtables */
6,/* subtable sizes */

0, /* 1-bit codes */
0, /* 2-bit codes */

5, /* 3-bit codes */
0x0003, 7, 0x0004, 6, 0x0005, 3, 0x0006, 2,
0x0007, 1,

4, /* 4-bit codes */
0x0002, 8, 0x0003, 5, 0x0004, 4, 0x0005, 0,

3, /* 5-bit codes */
0x0001, 12, 0x0002, 10, 0x0003, 9,

2, /* 6-bit codes */
0x0000, 13, 0x0001, 11,

-1
};

//Table 9 7 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 4
/*
8  011
6  100
5  101
4  110
1  111

9  0010
7  0011
3  0100
2  0101

12 0000 0
11 0000 1
10 0001 0
0  0001 1
*/

static
Ipp32s total_zeros_map_4[] =
{
5, /* max bits */
1,  /* total subtables */
5,/* subtable sizes */

0, /* 1-bit codes */
0, /* 2-bit codes */

5, /* 3-bit codes */
0x0003, 8, 0x0004, 6, 0x0005, 5, 0x0006, 4,
0x0007, 1,

4, /* 4-bit codes */
0x0002, 9, 0x0003, 7, 0x0004, 3, 0x0005, 2,

4, /* 5-bit codes */
0x0000, 12, 0x0001, 11, 0x0002, 10, 0x0003, 0,

-1
};

//Table 9 7 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 5
/*
7    011
6    100
5    101
4    110
3    111

10    0001
8    0010
2    0011
1    0100
0    0101

11    0000 0
9    0000 1
*/

static
Ipp32s total_zeros_map_5[] =
{
5, /* max bits */
1,  /* total subtables */
5,/* subtable sizes */

0, /* 1-bit codes */
0, /* 2-bit codes */

5, /* 3-bit codes */
0x0003, 7, 0x0004, 6, 0x0005, 5, 0x0006, 4,
0x0007, 3,

5, /* 4-bit codes */
0x0001, 10, 0x0002, 8, 0x0003, 2, 0x0004, 1,
0x0005, 0,

2, /* 5-bit codes */
0x0000, 11, 0x0001, 9,

-1
};

//Table 9 7 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 6
/*
9    001
7    010
6    011
5    100
4    101
3    110
2    111

8    0001

1    0000 1

10    0000 00
0    0000 01
*/

static
Ipp32s total_zeros_map_6[] =
{
6, /* max bits */
1,  /* total subtables */
6,/* subtable sizes */

0, /* 1-bit codes */
0, /* 2-bit codes */

7, /* 3-bit codes */
0x0001, 9, 0x0002, 7, 0x0003, 6, 0x0004, 5,
0x0005, 4, 0x0006, 3, 0x0007, 2,

1, /* 4-bit codes */
0x0001, 8,

1, /* 5-bit codes */
0x0001, 1,

2, /* 6-bit codes */
0x0000, 10, 0x0001, 0,

-1
};

//Table 9 7 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 7
/*
5    11

8    001
6    010
4    011
3    100
2    101

7    0001

1    0000 1

9    0000 00
0    0000 01

*/

static
Ipp32s total_zeros_map_7[] =
{
6, /* max bits */
1,  /* total subtables */
6,/* subtable sizes */

0, /* 1-bit codes */

1, /* 2-bit codes */
0x0003, 5,

5, /* 3-bit codes */
0x0001, 8, 0x0002, 6, 0x0003, 4, 0x0004, 3,
0x0005, 2,

1, /* 4-bit codes */
0x0001, 7,

1, /* 5-bit codes */
0x0001, 1,

2, /* 6-bit codes */
0x0000, 9, 0x0001, 0,

-1
};

//Table 9-8 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 8
/*
5    10
4    11

7    001
6    010
3    011

1    0001

2    0000 1

8    0000 00
0    0000 01

*/

static
Ipp32s total_zeros_map_8[] =
{
6, /* max bits */
1,  /* total subtables */
6,/* subtable sizes */

0, /* 1-bit codes */

2, /* 2-bit codes */
0x0002, 5,0x0003, 4,

3, /* 3-bit codes */
0x0001, 7, 0x0002, 6, 0x0003, 3,

1, /* 4-bit codes */
0x0001, 1,

1, /* 5-bit codes */
0x0001, 2,

2, /* 6-bit codes */
0x0000, 8, 0x0001, 0,

-1
};

//Table 9-8 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 9
/*
6    01
4    10
3    11

5    001

2    0001

7    0000 1

1    0000 00
0    0000 01
*/

static
Ipp32s total_zeros_map_9[] =
{
6, /* max bits */
1,  /* total subtables */
6,/* subtable sizes */

0, /* 1-bit codes */

3, /* 2-bit codes */
0x0001, 6, 0x0002, 4, 0x0003, 3,

1, /* 3-bit codes */
0x0001, 5,

1, /* 4-bit codes */
0x0001, 2,

1, /* 5-bit codes */
0x0001, 7,

2, /* 6-bit codes */
0x0000, 1, 0x0001, 0,

-1
};

//Table 9-8 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 10
/*
5    01
4    10
3    11

2    001

6    0001

1    0000 0
0    0000 1
*/

static
Ipp32s total_zeros_map_10[] =
{
5, /* max bits */
1,  /* total subtables */
5,/* subtable sizes */

0, /* 1-bit codes */

3, /* 2-bit codes */
0x0001, 5, 0x0002, 4, 0x0003, 3,

1, /* 3-bit codes */
0x0001, 2,

1, /* 4-bit codes */
0x0001, 6,

2, /* 5-bit codes */
0x0000, 1, 0x0001, 0,

-1
};

//Table 9-8 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 11
/*

4    1

2    001
3    010
5    011

0    0000
1    0001
*/

static
Ipp32s total_zeros_map_11[] =
{
4, /* max bits */
1,  /* total subtables */
4,/* subtable sizes */

1, /* 1-bit codes */
0x0001, 4,

0, /* 2-bit codes */

3, /* 3-bit codes */
0x0001, 2, 0x0002, 3, 0x0003, 5,

2, /* 4-bit codes */
0x0000, 0, 0x0001, 1,


-1
};

//Table 9-8 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 12
/*
3    1

2    01

4    001

0    0000
1    0001
*/

static
Ipp32s total_zeros_map_12[] =
{
4, /* max bits */
1,  /* total subtables */
4,/* subtable sizes */

1, /* 1-bit codes */
0x0001, 3,

1, /* 2-bit codes */
0x0001, 2,

1, /* 3-bit codes */
0x0001, 4,

2, /* 4-bit codes */
0x0000, 0, 0x0001, 1,


-1
};


//Table 9-8 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 13
/*
2    1

3    01

0    000
1    001

*/

static
Ipp32s total_zeros_map_13[] =
{
3, /* max bits */
1,  /* total subtables */
3,/* subtable sizes */

1, /* 1-bit codes */
0x0001, 2,

1, /* 2-bit codes */
0x0001, 3,

2, /* 3-bit codes */
0x0000, 0, 0x0001, 1,

-1
};

//Table 9-8 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 14
/*
2    1

0    00
1    01

*/

static
Ipp32s total_zeros_map_14[] =
{
2, /* max bits */
1,  /* total subtables */
2,/* subtable sizes */

1, /* 1-bit codes */
0x0001, 2,

2, /* 2-bit codes */
0x0000, 0, 0x0001, 1,

-1
};

//Table 9-8 - total_zeros tables for 4x4 blocks with
//TotalCoeff( coeff_token ) == 15
/*
0    0
1    1

*/

static
Ipp32s total_zeros_map_15[] =
{
1, /* max bits */
1,  /* total subtables */
1,/* subtable sizes */

2, /* 1-bit codes */
0x0000, 0, 0x0001, 1,

-1
};

//Table 9-9 - total_zeros tables for chroma DC 2x2 blocks
//TotalCoeff( coeff_token ) == 1
/*
0    1
1    01
2    001
3    000
*/

static
Ipp32s total_zeros_map_cr1[] =
{
3, /* max bits */
1, /* total subtables */
3,/* subtable sizes */

1, /* 1-bit codes */
0x0001, 0,

1, /* 2-bit codes */
0x0001, 1,

2, /* 3-bit codes */
0x0000, 3, 0x0001, 2,

-1
};

//Table 9-9 - total_zeros tables for chroma DC 2x2 blocks
//TotalCoeff( coeff_token ) == 2
/*
0    1
1    01
2    00
*/

static
Ipp32s total_zeros_map_cr2[] =
{
2, /* max bits */
1, /* total subtables */
2,/* subtable sizes */

1, /* 1-bit codes */
0x0001, 0,

2, /* 2-bit codes */
0x0000, 2, 0x0001, 1,

-1
};

//Table 9-9 - total_zeros tables for chroma DC 2x2 blocks
//TotalCoeff( coeff_token ) == 3
/*
0    1
1    0
*/

static
Ipp32s total_zeros_map_cr3[] =
{
1, /* max bits */
1, /* total subtables */
1,/* subtable sizes */

2, /* 1-bit codes */
0x0001, 0, 0x0000, 1,

-1
};

static
Ipp32s total_zeros_map_cr422_1[] =
{
    5, /* max bits */
    1, /* total subtables */
    5,/* subtable sizes */

    1, /* 1-bit codes */
    0x0001, 0,

    0, /* 2-bit codes */
    2, /* 3-bit codes */
    0x0002, 1,0x0003, 2,
    3, /* 4-bit codes */
    0x0001, 5,
    0x0002, 3,0x0003, 4,
    2, /* 5-bit codes */
    0x0000, 7,0x0001, 6,

    -1
};

static
Ipp32s total_zeros_map_cr422_2[] =
{
    3, /* max bits */
    1, /* total subtables */
    3,/* subtable sizes */

    0, /* 1-bit codes */

    1, /* 2-bit codes */
    0x0001, 1,
    6, /* 3-bit codes */
    0x0000, 0,0x0001, 2,
    0x0004, 3,0x0005, 4,
    0x0006, 5,0x0007, 6,

    -1
};

static
Ipp32s total_zeros_map_cr422_3[] =
{
    3, /* max bits */
    1, /* total subtables */
    3,/* subtable sizes */

    0, /* 1-bit codes */

    2, /* 2-bit codes */
    0x0001, 2,0x0002, 3,
    4, /* 3-bit codes */
    0x0000, 0,0x0001, 1,
    0x0006, 4,0x0007, 5,

    -1
};

static
Ipp32s total_zeros_map_cr422_4[] =
{
    3, /* max bits */
    1, /* total subtables */
    3,/* subtable sizes */

    0, /* 1-bit codes */

    3, /* 2-bit codes */
    0x0000, 1,0x0001, 2,
    0x0002, 3,
    2, /* 3-bit codes */
    0x0006, 0,0x0007, 4,

    -1
};

static
Ipp32s total_zeros_map_cr422_5[] =
{
    2, /* max bits */
    1, /* total subtables */
    2,/* subtable sizes */

    0, /* 1-bit codes */

    4, /* 2-bit codes */
    0x0000, 0,0x0001, 1,
    0x0002, 2,0x0003, 3,


    -1
};

static
Ipp32s total_zeros_map_cr422_6[] =
{
    2, /* max bits */
    1, /* total subtables */
    2,/* subtable sizes */

    1, /* 1-bit codes */
    0x0001, 2,

    2, /* 2-bit codes */
    0x0000, 0, 0x0001, 1,

    -1
};

static
Ipp32s total_zeros_map_cr422_7[] =
{
    1, /* max bits */
    1, /* total subtables */
    1,/* subtable sizes */

    2, /* 1-bit codes */
    0x0000, 0, 0x0001, 1,

    -1
};

} // namespace UMC

#endif //__UMC_H264_DEC_TOTAL_ZEROS_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
