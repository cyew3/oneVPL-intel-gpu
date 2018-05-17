/******************************************************************************\
Copyright (c) 2018, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#ifndef __BS_DEF_H
#define __BS_DEF_H

//#undef __BS_TRACE__
//#define  __BS_TRACE__

typedef unsigned char    byte;
typedef unsigned char    Bs8u;
typedef unsigned short   Bs16u;
typedef unsigned int     Bs32u;

typedef signed char      Bs8s;
typedef signed short     Bs16s;
typedef signed int       Bs32s;

#ifdef __GNUC__
 typedef unsigned long long Bs64u;
 typedef signed long long   Bs64s;
#else
 typedef unsigned __int64 Bs64u;
 typedef signed __int64   Bs64s;
#endif
 
 #if defined( _WIN32 ) || defined ( _WIN64 )
  #define __STDCALL  __stdcall
  #define __CDECL    __cdecl
  #define __INT64    __int64
  #define __UINT64    unsigned __int64
#else
  #define __STDCALL
  #define __CDECL
  #define __INT64    long long
  #define __UINT64    unsigned long long
#endif

typedef enum {
    BS_ERR_NONE                 =  0,
    BS_ERR_UNKNOWN              = -1,
    BS_ERR_WRONG_UNITS_ORDER    = -2,
    BS_ERR_MORE_DATA            = -3,
    BS_ERR_INVALID_PARAMS       = -4,
    BS_ERR_MEM_ALLOC            = -5,
    BS_ERR_NOT_IMPLEMENTED      = -6,
    BS_ERR_NOT_ENOUGH_BUFFER    = -7,
    BS_ERR_BAD_HANDLE           = -8,
    BS_ERR_INCOMPLETE_DATA      = -9
} BSErr;

#define BS_MIN(x, y) ( ((x) < (y)) ? (x) : (y) )
#define BS_MAX(x, y) ( ((x) > (y)) ? (x) : (y) )
#define BS_ABS(x)    ( ((x) > 0) ? (x) : (-(x)) )

#endif //__BS_DEF_H