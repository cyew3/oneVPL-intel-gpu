/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
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

#pragma once

#include <cmath>
#include "mfxvideo++.h"

//sample bitrate control class
//based on per frame QP settings available via mfxEncodeCtrl structure
//this class also maintains IPPP gop structure using input GopPicSize value
class SampleBRC
    : public IBRC
{
    mfxF64 m_fps;
    mfxU32 m_targetFrameSize;
    int    m_QP;
    mfxU32 m_nFramesAfterReset;
    mfxU32 m_nFrames;
    mfxU64 m_BsSizeAfterReset;
    mfxU16 m_targetKbps;
    mfxU16 m_nGopSize;

public:
    SampleBRC()
    {}

    mfxStatus Init(mfxVideoParam *pvParams)
    {
        m_fps               = (mfxF64)pvParams->mfx.FrameInfo.FrameRateExtN / (mfxF64)pvParams->mfx.FrameInfo.FrameRateExtD;
        m_targetKbps        = pvParams->mfx.TargetKbps;
        m_targetFrameSize   = (mfxU32)(m_targetKbps * 1024.0 / m_fps / 8);
        m_nGopSize          = pvParams->mfx.GopPicSize;
        m_QP                = 20;//just constant
        m_BsSizeAfterReset  = 0;
        m_nFrames           = 0;
        m_nFramesAfterReset = 0;

        return MFX_ERR_NONE;
    }

    void Update(mfxBitstream *pbs)
    {
        mfxU32 encodedFrameSize = pbs->DataLength;

        m_BsSizeAfterReset += encodedFrameSize;
        m_nFrames++;
        m_nFramesAfterReset ++;

        mfxU32 AverageFrameSizeSoFar = (mfxU32)((mfxF64)m_BsSizeAfterReset / m_nFramesAfterReset);

        if (encodedFrameSize > m_targetFrameSize)
            m_QP++;

        if (encodedFrameSize < m_targetFrameSize)
            m_QP--;


        if (encodedFrameSize > 2*m_targetFrameSize)
            m_QP += 2;

        if (encodedFrameSize < m_targetFrameSize/2)
            m_QP -= 2;


        if (AverageFrameSizeSoFar > m_targetFrameSize)
            m_QP++;

        if (AverageFrameSizeSoFar < m_targetFrameSize)
            m_QP--;

        if (AverageFrameSizeSoFar > 1.3 * m_targetFrameSize)
            m_QP++;

        if (AverageFrameSizeSoFar < 0.8 * m_targetFrameSize)
            m_QP--;

        if (m_QP < 1) m_QP = 1;
    }

    mfxStatus Reset(mfxVideoParam *pvParams)
    {
        m_targetKbps      = pvParams->mfx.TargetKbps;
        m_targetFrameSize = (mfxU32)(m_targetKbps * 1024 / m_fps / 8);
        m_QP              = (mfxU16)(m_QP * pow(((mfxF64)m_BsSizeAfterReset / m_nFramesAfterReset / m_targetFrameSize), 0.3));
        if (m_QP < 1)
            m_QP = 1;

        m_BsSizeAfterReset  = 0;
        m_nFramesAfterReset = 0;

        return MFX_ERR_NONE;
    }

    //current target bitrate
    mfxU16 GetCurrentBitrate()
    {
        return m_targetKbps;
    }

    mfxU16 GetFrameQP()
    {
        return (mfxU16)m_QP;
    }

    mfxU16 GetFrameType()
    {
        if (!(m_nFrames % m_nGopSize))
        {
            return MFX_FRAMETYPE_I;
        }
        return MFX_FRAMETYPE_P;
    }
};