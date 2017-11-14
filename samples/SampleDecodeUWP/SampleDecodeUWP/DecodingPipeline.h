/******************************************************************************\
Copyright (c) 2005-2017, Intel Corporation
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
#include "mfxvideo++.h"
#include "Thread11.h"
#include "SimpleBitstreamReader.h"
#include <atomic>
#include <list>
#include "d3d11_device.h"
#include "d3d11_allocator.h"
#include "SurfacesPool.h"
#include "UWPRenderer.h"
#include "sample_defs.h"
#include "RendererPanel.h"
#include "PluginsManager.h"

class CDecodingPipeline : public CThread11
{
public:

    enum PIPELINE_STATUS
    {
        PS_NONE, // No clip or error occured
        PS_STOPPED,
        PS_PAUSED,
        PS_PLAYING,
    };

    std::function<void(PIPELINE_STATUS)> OnPipelineStatusChanged;

    CDecodingPipeline();

    bool Init();
    void Play();
    void Stop();
    void Pause();
    void Load(Windows::Storage::StorageFile^ file);
    void SetCodecID(mfxU32 codec) { codecID = codec; }
    PIPELINE_STATUS GetStatus() { return pipelineStatus; }
    bool IsHWLib;
    mfxU32 AsyncDepth;
    mfxHDL GetHWDevHdl()
    {
        mfxHDL hdl = NULL;
        return dev.GetHandle(MFX_HANDLE_D3D11_DEVICE, &hdl) >= MFX_ERR_NONE ? hdl : NULL;
    }
    void SetRendererPanel(SampleDecodeUWP::CRendererPanel^ panel) { rendererPanel = panel; }
    int GetProgressPromilleage() { return  reader.GetFileSize() ? (int)(reader.GetBytesProcessed() * 1000 / reader.GetFileSize()) : 0; }

protected:
    virtual bool OnStart() override;
    virtual bool RunOnce() override;
    virtual void OnClose() override;

private:
    mfxStatus InitSession();
    mfxStatus SyncOneSurface();

    MFXVideoSession session;
    mfxIMPL impl;

    CSimpleBitstreamReader reader;
    std::atomic<PIPELINE_STATUS> pipelineStatus;
    
    mfxVideoParam decoderParams;
    MFXVideoDECODE* pDecoder=NULL;
    mfxU32 codecID;

    CD3D11Device dev;
    D3D11FrameAllocator allocator;
    CSurfacesPool surfacesPool;
    std::list<CMfxFrameSurfaceExt*> decodingSurfaces;

    SampleDecodeUWP::CRendererPanel^ rendererPanel = nullptr;

    Windows::Storage::StorageFile^ fileSource;

    bool isDecodingEnding;

    CPluginsManager pluginsManager;

    mfxStatus CDecodingPipeline::LoadPluginsAndDecodeHeader(mfxBitstream* pBS, mfxU32 codecID);
};

