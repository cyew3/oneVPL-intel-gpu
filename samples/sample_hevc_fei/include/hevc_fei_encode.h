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

#ifndef __SAMPLE_FEI_ENCODE_H__
#define __SAMPLE_FEI_ENCODE_H__

#include <vector>
#include "sample_hevc_fei_defs.h"
#include "ref_list_manager.h"

class FEI_Encode
{
public:
    FEI_Encode(MFXVideoSession* session, const mfxFrameInfo& frameInfo, const sInputParams& encParams);
    ~FEI_Encode();

    mfxStatus Init();
    mfxStatus Query();
    mfxStatus Reset(mfxVideoParam& par);
    mfxStatus QueryIOSurf(mfxFrameAllocRequest* request);
    mfxStatus GetVideoParam(mfxVideoParam& par);
    mfxFrameInfo* GetFrameInfo();
    mfxStatus EncodeFrame(mfxFrameSurface1* pSurf);
    mfxStatus SetCtrlParams(const HevcTask& task); // for encoded order

private:
    MFXVideoSession*    m_pmfxSession;
    MFXVideoENCODE      m_mfxENCODE;

    MfxVideoParamsWrapper m_videoParams;
    mfxEncodeCtrl         m_encodeCtrl;
    mfxBitstream          m_bitstream;
    mfxSyncPoint          m_syncPoint;

    std::string          m_dstFileName;
    CSmplBitstreamWriter m_FileWriter; // bitstream writer

    mfxStatus SetEncodeParameters(const sInputParams& encParams);
    mfxStatus SyncOperation();
    mfxStatus AllocateSufficientBuffer();

    // forbid copy constructor and operator
    FEI_Encode(const FEI_Encode& encode);
    FEI_Encode& operator=(const FEI_Encode& encode);
};

#endif // __SAMPLE_FEI_ENCODE_H__
