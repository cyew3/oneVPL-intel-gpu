/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __MFX_MSG_H
#define __MFX_MSG_H

#include <ippdefs.h>

#include <stdio.h>
#include <windows.h>

extern "C"
Ipp64u freq;

inline
void Msg(Ipp32u threadNum, const char *pMsg, Ipp64u time, Ipp64u lasting)
{
    char cStr[256];
    Ipp32s timeSec, timeMSec, us;

    timeSec = (Ipp32u) ((time / freq) % 60);
    timeMSec = (Ipp32u) (((time * 1000) / freq) % 1000);
    us = (Ipp32u) ((lasting * 1000000) / freq);
    sprintf_s(cStr, sizeof(cStr),
              "[% 4u] %3u.%03u % 30s % 6u us\n",
              threadNum, timeSec, timeMSec, pMsg, us);
    OutputDebugStringA(cStr);

} // void Msg(Ipp32u threadNum, const char *pMsg, Ipp64u time)

#endif // __MFX_MSG_H
