/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.
//
*/

#ifndef _DATATYPES_H_
#define _DATATYPES_H_

#if defined(_MCRT) || defined(_MCRT64)
#include "mcrtbasictypes.h"
#include "mcrt.h"
#endif

#ifdef __cplusplus
    extern "C" {
#endif

//TODO: investigate if can share common date types with other module, Gfx, etc.

/**************************************************************************\
 *                              D E F I N E S                               *
\**************************************************************************/

#define IN
#define OUT
#define INOUT

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL    (void *)0
#endif
#endif

/**************************************************************************\
 *                     T Y P E   D E F I N I T I O N S                      *
\**************************************************************************/
#if defined(_MCRT) || defined(_MCRT64)
    /*mcrt header file contains type definition for other types*/
    typedef float               float32;
    typedef double              double64;
    typedef void*               Handle_t;
    typedef uint32              Flag_t;
#elif defined(_WIN32) || defined(_WIN64)
    typedef char                int8;
    typedef unsigned char       uint8;
    typedef short               int16;
    typedef unsigned short      uint16;
    typedef long                int32;
    typedef unsigned long       uint32;
    typedef unsigned long long  uint64;
    typedef long long           int64;
    typedef float               float32;
    typedef double              double64;
    typedef void*               Handle_t;
    typedef unsigned long       Flag_t;
#elif defined(_FREEBSD)
    typedef char                int8;
    typedef unsigned char       uint8;
    typedef short               int16;
    typedef unsigned short      uint16;
    typedef int                 int32;
    typedef unsigned int        uint32;
    typedef long                int64;
    typedef unsigned long       uint64;
    typedef float               float32;
    typedef double              double64;
    typedef void*               Handle_t;
    typedef unsigned int        Flag_t;
#else
    #error !!! must have data type definition for this architecture
#endif

typedef enum
{
    False       = 0,
    True        = 1
} Bool_t;

typedef struct
{
    int32 x;
    int32 y;
} Point_t;

typedef struct
{
    int32 left;
    int32 top;
    int32 right;
    int32 bottom;
} Rect_t;

typedef enum
{
    RESULT_OK               = 0,
    RESULT_GENERAL_ERROR,
    RESULT_NOT_SUPPORTED,
    RESULT_BAD_PARAMETER,
    RESULT_NULL_PTR,
    RESULT_NO_RESOURCES,
    RESULT_INVALID_CALL,
    RESULT_FILE_ERROR,
    RESULT_TASK_DONE,
    RESULT_XNTASK_ERROR,
} Result_t;

#ifdef __cplusplus
    }       //extern "C"
#endif

#endif      //_DATATYPES_H_
