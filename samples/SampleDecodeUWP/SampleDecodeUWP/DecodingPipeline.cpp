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

#include "pch.h"
#include "DecodingPipeline.h"
#include "sample_defs.h"
#include "MSDKHandle.h"
#include "PluginsManager.h"

#define WAIT_INTERVAL 20000
#define BUFFERED_FRAMES_NUM 5

CDecodingPipeline::CDecodingPipeline()
    : pluginsManager(session)
{
    fileSource = nullptr;
    rendererPanel = nullptr;
    IsHWLib = true;
    pipelineStatus = PS_NONE;
    codecID = MFX_CODEC_AVC;
    AsyncDepth = 4;
    OnPipelineStatusChanged = NULL;
    impl= 0;
    isDecodingEnding = false;

    MSDK_ZERO_MEMORY(decoderParams);
}

bool CDecodingPipeline::Init()
{
    return InitSession() >= MFX_ERR_NONE;
}

bool CDecodingPipeline::OnStart()
{
    // Loading file
    reader.Close();
    reader.Init(fileSource);

    // Creating decoder
    if (!pDecoder)
    {
        pDecoder = new(std::nothrow) MFXVideoDECODE(session);
    }
    if (!pDecoder)
    {
        return false;
    }
    // Decoding bitstream header to get params
    int i = 0;
    mfxStatus sts = MFX_ERR_MORE_BITSTREAM;

    //--- Decoding Stream Header 
    MSDK_ZERO_MEMORY(decoderParams);
    decoderParams.mfx.CodecId = codecID;
    decoderParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    decoderParams.AsyncDepth = AsyncDepth;

    reader.ReadNextFrame();
    sts = LoadPluginsAndDecodeHeader(&reader.BitStream, codecID);
    if (sts != MFX_ERR_NONE)
    {
        MSDK_PRINT_RET_MSG(sts, "Error reading stream header");
        return false;
    }

    //--- Setting rendering framerate
    double frameRate = decoderParams.mfx.FrameInfo.FrameRateExtD ? ((double)decoderParams.mfx.FrameInfo.FrameRateExtN) / decoderParams.mfx.FrameInfo.FrameRateExtD : 0; 
    if (frameRate < 10)
    {
        //--- Some streams have corrupted framerate, set default value in this case
        frameRate = 30;
    }
    rendererPanel->SetFrameRate(frameRate);

    //--- Querying for number of surfaces required for the decoding process
    mfxFrameAllocRequest req;
    MSDK_ZERO_MEMORY(req);
    sts = pDecoder->QueryIOSurf(&decoderParams, &req);
    MSDK_CHECK_STATUS_BOOL(sts, "Decoder QueryIOSurf failed");

    //--- Allocating surfaces and putting them into surfaces pool
    req.NumFrameMin = req.NumFrameSuggested = req.NumFrameSuggested + BUFFERED_FRAMES_NUM; // One more surface for rendering
    mfxFrameAllocResponse resp;
    MSDK_ZERO_MEMORY(resp);
    sts = allocator.AllocFrames(&req, &resp);
    MSDK_CHECK_STATUS_BOOL(sts, "Cannot allocate frames");
    surfacesPool.Create(resp.mids, resp.NumFrameActual, 0, resp.MemType, &allocator, req.Info);

    //--- Initializing decoder
    sts = pDecoder->Init(&decoderParams);
    MSDK_CHECK_STATUS_BOOL(sts, "Cannot initialize decoder");

    pipelineStatus = PS_PLAYING;
    if (OnPipelineStatusChanged)
    {
        OnPipelineStatusChanged(PS_PLAYING);
    }
    isDecodingEnding = false;

    return true;
}

bool CDecodingPipeline::RunOnce()
{
    //--- Do nothing in case of pause
    if (pipelineStatus == PS_PAUSED)
    {
        return true;
    }

    mfxStatus sts = MFX_ERR_NONE;
    //--- Keep AsyncDepth frames in the queue for best performance
    while (decodingSurfaces.size() < AsyncDepth)
    {
        mfxFrameSurface1* pOutSurface = NULL;
        mfxSyncPoint syncPoint = NULL;

        mfxFrameSurface1* pFreeSurface = surfacesPool.GetFreeSurface();
        MSDK_CHECK_POINTER(pFreeSurface, false);

        //--- Deocding one frame
        bool shouldRepeat = true;
        while (shouldRepeat)
        {
            sts = pDecoder->DecodeFrameAsync(isDecodingEnding ? NULL : &reader.BitStream,
                pFreeSurface, &pOutSurface, &syncPoint);

            switch(sts)
            { 
            case MFX_ERR_MORE_DATA:
                if (reader.BitStream.DataLength > (mfxU32)(reader.BitStream.MaxLength*0.9))
                {
                    //--- If bitstream is full of data, but we still have an error - expand buffer
                    reader.ExpandBitstream(reader.BitStream.MaxLength);
                }
                if (isDecodingEnding)
                {
                    //--- Buffered frames are taken, time to close thread
                    return false;
                }
                else if (reader.ReadNextFrame() == MFX_ERR_MORE_DATA)
                {
                    //--- If we can't read more data from file - finish decoding, clean up buffers.
                    isDecodingEnding = true;
                }
                break;
            case MFX_ERR_DEVICE_FAILED:
            case MFX_WRN_DEVICE_BUSY:
                // If device failed, sleep and try again
                Sleep(10);
                break;
            case MFX_ERR_MORE_SURFACE:
                pFreeSurface = surfacesPool.GetFreeSurface();
                MSDK_CHECK_POINTER(pFreeSurface, false);
                break;
            default:
                shouldRepeat = false;
                break;
            }
        }

        MSDK_CHECK_STATUS_BOOL(sts, "DecodeFrameAsync failed");

        //--- Putting "decoding request" into queue (at this moment surface does not contain decoded data yet, it's just an empty container)
        if (pOutSurface)
        {
            CMfxFrameSurfaceExt* pOutSurfExt = surfacesPool.GetExtSurface(pOutSurface);
            pOutSurfExt->LinkedSyncPoint = syncPoint;
            pOutSurfExt->UserLock = true;
            decodingSurfaces.push_back(pOutSurfExt);
        }
    }
    
    return SyncOneSurface()>=MFX_ERR_NONE;
}

mfxStatus CDecodingPipeline::SyncOneSurface()
{
    if (decodingSurfaces.size())
    {
        //--- Synchronizing surface to finish decoding and get actual frame data
        CMfxFrameSurfaceExt* pFrontSurface = decodingSurfaces.front();
        mfxStatus sts = session.SyncOperation(pFrontSurface->LinkedSyncPoint, WAIT_INTERVAL);
        MSDK_CHECK_STATUS(sts, "SyncOperation failed");
        decodingSurfaces.front()->LinkedSyncPoint = NULL;

        //--- Pushing surface to the output queue
        if (rendererPanel)
        {
            rendererPanel->EnqueueSurface(ref new SampleDecodeUWP::CMSDKHandle(pFrontSurface));
        }

        decodingSurfaces.pop_front();
    }
    return MFX_ERR_NONE;
}

void CDecodingPipeline::OnClose()
{
    //--- Syncing the rest of surfaces in the queue
    while (decodingSurfaces.size() && SyncOneSurface() >= MFX_ERR_NONE);
    //--- Just in case of error, remove userlock from the rest surfaces in decodingSurfaces - to avoid hangs
    for (auto surf : decodingSurfaces)
    {
        surf->UserLock = false;
    }

    // Wait for all surfaces to be unlocked (from UserLock)
    while (surfacesPool.GetUserLockedCount())
    {
        Sleep(10);
    }
    mfxFrameAllocResponse resp = surfacesPool.GenerateAllocResponse();
    mfxStatus sts=allocator.FreeFrames(&resp);
    surfacesPool.Clear();
    pDecoder->Close();
    decodingSurfaces.clear();
    reader.Close();
    pluginsManager.UnloadAllPlugins();

    pipelineStatus = PS_STOPPED;
    if (OnPipelineStatusChanged)
    {
        OnPipelineStatusChanged(PS_STOPPED);
    }
}

mfxStatus CDecodingPipeline::InitSession()
{
    mfxInitParam initPar;
    mfxVersion version;     // real API version with which library is initialized
    mfxStatus sts = MFX_ERR_NONE;

    MSDK_ZERO_MEMORY(initPar);
    
    // we set version to 1.0 and later we will query actual version of the library which will got leaded
    initPar.Version.Major = 1;
    initPar.Version.Minor = 0;

    initPar.GPUCopy = true;

    bool needInitExtPar = false;

    //--- Init session
    if (IsHWLib)
    {
        // try searching on all display adapters
        initPar.Implementation = MFX_IMPL_HARDWARE_ANY;
        initPar.Implementation |= MFX_IMPL_VIA_D3D11;

        // Library should pick first available compatible adapter during InitEx call with MFX_IMPL_HARDWARE_ANY
        sts = session.InitEx(initPar);
    }
    else
    {
        initPar.Implementation = MFX_IMPL_SOFTWARE;
        sts = session.InitEx(initPar);
    }

    MSDK_CHECK_STATUS(sts, "m_mfxSession.Init failed");

    //--- Query library version  and implementation
    sts = session.QueryVersion(&version); // get real API version of the loaded library
    MSDK_CHECK_STATUS(sts, "m_mfxSession.QueryVersion failed");

    sts = session.QueryIMPL(&impl); // get actual library implementation
    MSDK_CHECK_STATUS(sts, "m_mfxSession.QueryIMPL failed");

    //--- Initialize D3D11 device
    sts=dev.Init(0);
    MSDK_CHECK_STATUS(sts, "D3D11 Device Init failed");
    
    //--- Initialize D3D11 allocator
    mfxHDL hdl = NULL;
    sts = dev.GetHandle(MFX_HANDLE_D3D11_DEVICE, &hdl);
    MSDK_CHECK_STATUS(sts, "D3D11 Device GetHandle failed");
    session.SetHandle(MFX_HANDLE_D3D11_DEVICE, hdl);
    MSDK_CHECK_STATUS(sts, "SetHandle failed");

    D3D11AllocatorParams params;
    params.pDevice = reinterpret_cast<ID3D11Device*>(hdl);
    sts = allocator.Init(&params);
    MSDK_CHECK_STATUS(sts, "Allocator Init failed");
    sts = session.SetFrameAllocator(&allocator);
    MSDK_CHECK_STATUS(sts, "SetFrameAllocator failed");

    return MFX_ERR_NONE;
}

void CDecodingPipeline::Play()
{
    rendererPanel->SetPlay(true);
    if (!IsRunning())
    {
        //--- Currently pipeline is stopped, let's start playing
        Start();
    }
    else
    {
        //--- Currently pipeline is playing, but possibly paused. Let's resume playing
        pipelineStatus = PS_PLAYING;
        if (OnPipelineStatusChanged)
        {
            OnPipelineStatusChanged(PS_PLAYING);
        }
    }
}

void CDecodingPipeline::Stop()
{
    CThread11::Stop();
}

void CDecodingPipeline::Pause()
{
    pipelineStatus = PS_PAUSED;
    rendererPanel->SetPlay(false);
    if (OnPipelineStatusChanged)
    {
        OnPipelineStatusChanged(PS_PAUSED);
    }
}

void CDecodingPipeline::Load(Windows::Storage::StorageFile^ file)
{
    Stop();
    fileSource = file;
}

mfxStatus CDecodingPipeline::LoadPluginsAndDecodeHeader(mfxBitstream* pBS, mfxU32 codecID)
{
    mfxStatus sts = MFX_ERR_UNKNOWN;
    mfxPluginUID guid;

    //--- Load plugins if required (hardware version first, then software if hardware won't work)
    mfxStatus stsPlugin = pluginsManager.LoadVideoPlugin(codecID, EPluginSpecification(PD_DECODE | PD_HARDWARE), &guid);
    if (stsPlugin != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
    {
        if (stsPlugin >= MFX_ERR_NONE)
        {
            // Querying for correct parameters
            sts = pDecoder->DecodeHeader(pBS, &decoderParams);

            if (sts<MFX_ERR_NONE)
            {
                msdk_printf(MSDK_STRING("WARNING: HW plugin (%s) seems to be incompatible with current platform, so unloading it."), CPluginsManager::GUID2String(guid).c_str());
                pluginsManager.UnloadPluginByGUID(guid);
            }
        }
        else
        {
            msdk_printf(MSDK_STRING("ERROR: Requested HW (%s) plugin cannot be loaded, so we'll try with SW plugin."), CPluginsManager::GUID2String(guid).c_str());
        }
    }

    if (stsPlugin != MFX_ERR_NONE || sts != MFX_ERR_NONE)
    {
        stsPlugin = pluginsManager.LoadVideoPlugin(codecID, EPluginSpecification(PD_DECODE | PD_SOFTWARE), &guid);
        if (stsPlugin >= MFX_ERR_NONE)
        {
            // Querying for correct parameters
            sts = pDecoder->DecodeHeader(pBS, &decoderParams);
            if (sts<MFX_ERR_NONE)
            {
                if (stsPlugin != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                {
                    //--- Unload plugin only if it was actually loaded
                    msdk_printf(MSDK_STRING("ERROR: SW plugin (%s) might be incompatible with current platform, so unloading it."), CPluginsManager::GUID2String(guid).c_str());
                    pluginsManager.UnloadPluginByGUID(guid);
                }
            }
        }
    }

    return stsPlugin >= MFX_ERR_NONE ? sts : stsPlugin;
}
