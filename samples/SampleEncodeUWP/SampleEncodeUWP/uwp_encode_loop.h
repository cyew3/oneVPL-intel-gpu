/******************************************************************************\
Copyright (c) 2005-2018, Intel Corporation
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

#ifndef __PIPELINE_UWP_ENCODE_H__
#define __PIPELINE_UWP_ENCODE_H__

#include "base_uwp_allocator.h"

#include "mfxvideo++.h"

#include <vector>
#include <memory>

using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace concurrency;
using namespace Platform;
using namespace Windows::UI::Popups;
using namespace Windows::Foundation;

enum {
    MVC_DISABLED          = 0x0,
    MVC_ENABLED           = 0x1,
    MVC_VIEWOUTPUT        = 0x2,    // 2 output bitstreams
};

enum MemType {
    SYSTEM_MEMORY = 0x00,
    D3D9_MEMORY   = 0x01,
    D3D11_MEMORY  = 0x02,
};

class CEncoder
{
    friend class std::_Ref_count_obj<CEncoder>;

private:
    CEncoder();

public:

    static std::shared_ptr<CEncoder> Instantiate(mfxVideoParam &pParams);
    ~CEncoder();

    mfxStatus ProceedFrame(const mfxU8* bufferSrc, mfxU32 buffersize, mfxSyncPoint& EncSyncP);
    void Close();
    mfxU32 GetPictureSize();
    void CancelEncoding();

    IAsyncActionWithProgress<double>^ ReadAndEncodeAsync(StorageFile^ storageSource, StorageFile^ storageSink);
protected:


    Concurrency::cancellation_token_source _CTS;

   
    MFXVideoSession m_mfxSession;
    std::unique_ptr<MFXVideoENCODE> m_pmfxENC;

    mfxVideoParam m_mfxEncParams;

    mfxBitstream m_mfxBS;

    std::unique_ptr<MFXFrameAllocator> m_pMFXAllocator;
    std::unique_ptr<mfxAllocatorParams> m_pmfxAllocatorParams;

    mfxU64 m_nEncSurfaces;
    std::unique_ptr<mfxFrameSurface1[]> m_pEncSurfaces; // frames array for encoder input (vpp output)
    mfxFrameAllocResponse m_EncResponse;  // memory allocation response for encoder

    // external parameters for each component are stored in a vector
    std::vector<mfxExtBuffer*> m_EncExtParams;

    mfxU32 m_nFramesRead;

    mfxEncodeCtrl m_encCtrl;

    mfxStatus SetEncParams(mfxVideoParam& pParams);

    mfxStatus CreateAllocator();
    void DeleteAllocator();
    void DeleteFrames();

    mfxStatus AllocEncoderInputFrames();
    mfxStatus AllocEncoderOutputBitream();

    mfxU32 nFramesProcessed = 0;
    mfxU16 nEncSurfIdx = 0;     // index of free surface for encoder input (vpp output)
};

#endif // __PIPELINE_UWP_ENCODE_H__

