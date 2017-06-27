/******************************************************************************\
Copyright (c) 2017, Intel Corporation
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

#ifndef __SAMPLE_HEVC_FEI_DEFS_H__
#define __SAMPLE_HEVC_FEI_DEFS_H__

#include "sample_defs.h"

struct sInputParams
{
    msdk_char  strSrcFile[MSDK_MAX_FILENAME_LEN];
    msdk_char  strDstFile[MSDK_MAX_FILENAME_LEN];

    bool bENCODE;
    mfxU32 ColorFormat;
    mfxU16 nPicStruct;
    mfxU8  QP;
    mfxU16 nWidth;          // source picture width
    mfxU16 nHeight;         // source picture height
    mfxF64 dFrameRate;
    mfxU32 nNumFrames;
    mfxU16 nNumSlices;
    mfxU16 nRefDist;        // number of frames to next I,P
    mfxU16 nGopSize;        // number of frames to next I
    mfxU16 nIdrInterval;    // distance between IDR frames in GOPs
    mfxU16 BRefType;        // B-pyramid ON/OFF/UNKNOWN (default, let MSDK lib to decide)
    mfxU16 nNumRef;         // number of reference frames (DPB size)

    sInputParams()
        : bENCODE(false)
        , ColorFormat(MFX_FOURCC_I420)
        , nPicStruct(MFX_PICSTRUCT_PROGRESSIVE)
        , QP(26)
        , nWidth(0)
        , nHeight(0)
        , dFrameRate(30.0)
        , nNumFrames(0)
        , nNumSlices(1)
        , nRefDist(1)
        , nGopSize(1)
        , nIdrInterval(0xffff)         // Infinite IDR interval
        , BRefType(MFX_B_REF_UNKNOWN)  // MSDK library make decision itself about enabling B-pyramid or not
        , nNumRef(1)
    {
        MSDK_ZERO_MEMORY(strSrcFile);
        MSDK_ZERO_MEMORY(strDstFile);
    }

};

template <typename T, typename T1>
// `T1& lower` is bound below which `T& val` is considered invalid
T tune(const T& val, const T1& lower, const T1& default_val)
{
    if (val <= lower)
        return default_val;
    return val;
}

#endif // #define __SAMPLE_HEVC_FEI_DEFS_H__
