/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __OSCL_PLATFORM_DEF_H__
#define __OSCL_PLATFORM_DEF_H__


#include "string.h"

/*-------------------------------------------------------------------------------------
|   Size of C type in different platform
| Type        win32    win64    ubuntu32    ubuntu64    android-32
| bool          1        1         1            1            1
| char          1        1         1            1            1
| short         2        2         2            2            2
| short int     2        2         2            2            2
| int           4        4         4            4            4
| long          4        4         4            8            4
| long int      4        4         4            8            4
| long long     8        8         8            8            8
| float         4        4         4            4            4
| double        8        8         8            8            8
| long double   8        8         12           16           12
| void          -        -         1            1            1
| wchar_t       2        2         4            4            4
| void*         4        8         4            8            4
\------------------------------------------------------------------------------------*/
typedef char                int8;
typedef unsigned char       uint8, uchar, bool8;
typedef short               int16; 
typedef unsigned short      uint16;
typedef int                 int32, bool32;
typedef unsigned int        uint32;
typedef long long           int64;
typedef unsigned long long  uint64;
typedef float               float32;
typedef double              double64, DOUBLE;

#endif   // __OSCL_PLATFORM_DEF_H__
