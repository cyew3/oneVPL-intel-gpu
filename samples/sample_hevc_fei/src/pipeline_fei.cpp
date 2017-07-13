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

#include "pipeline_fei.h"

CEncodingPipeline::CEncodingPipeline(sInputParams& userInput)
    : m_inParams(userInput)
    , m_impl(MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_VAAPI)
    , m_EncSurfPool()
{
}

CEncodingPipeline::~CEncodingPipeline()
{
    Close();
}

mfxStatus CEncodingPipeline::Init()
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_mfxSession.Init(m_impl, NULL);
    MSDK_CHECK_STATUS(sts, "m_mfxSession.Init failed");

    sts = LoadFEIPlugin();
    MSDK_CHECK_STATUS(sts, "LoadPlugin failed");

    // create and init frame allocator
    sts = CreateAllocator();
    MSDK_CHECK_STATUS(sts, "CreateAllocator failed");

    mfxFrameInfo frameInfo;
    MSDK_ZERO_MEMORY(frameInfo);
    sts = FillInputFrameInfo(frameInfo);
    MSDK_CHECK_STATUS(sts, "FillInputFrameInfo failed");

    m_pInputSource.reset(new YUVReader(m_inParams, frameInfo, &m_EncSurfPool));

    if (m_inParams.bENCODE)
    {
        m_pFEI_Encode.reset(new FEI_Encode(&m_mfxSession, frameInfo, m_inParams));
        // call Query to check that Encode's parameters are valid
        sts = m_pFEI_Encode->Query();
        MSDK_CHECK_STATUS(sts, "FEI ENCODE Query failed");
    }

    sts = AllocFrames();
    MSDK_CHECK_STATUS(sts, "AllocFrames failed");

    return InitComponents();
}

void CEncodingPipeline::Close()
{
    m_pPlugin.reset(); // Unload plugin by destructing object PluginLoader
    m_mfxSession.Close();
}

mfxStatus CEncodingPipeline::LoadFEIPlugin()
{
    // m_pluginGuid = msdkGetPluginUID(m_impl, MSDK_VENC, MFX_CODEC_HEVC);
    m_pluginGuid = msdkGetPluginUID(m_impl, MSDK_VENCODE, MFX_CODEC_HEVC); // use HW HEVC for debug
    MSDK_CHECK_ERROR(AreGuidsEqual(m_pluginGuid, MSDK_PLUGINGUID_NULL), true, MFX_ERR_NOT_FOUND);

    // m_pPlugin.reset(LoadPlugin(MFX_PLUGINTYPE_VIDEO_ENC, m_mfxSession, m_pluginGuid, 1));
    m_pPlugin.reset(LoadPlugin(MFX_PLUGINTYPE_VIDEO_ENCODE, m_mfxSession, m_pluginGuid, 1));
    MSDK_CHECK_POINTER(m_pPlugin.get(), MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::CreateAllocator()
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = CreateHWDevice();
    MSDK_CHECK_STATUS(sts, "CreateHWDevice failed");

    mfxHDL hdl = NULL;
    sts = m_pHWdev->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
    MSDK_CHECK_STATUS(sts, "m_pHWdev->GetHandle failed");

    // provide device manager to MediaSDK
    sts = m_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
    MSDK_CHECK_STATUS(sts, "m_mfxSession.SetHandle failed");

    /* Only video memory is supported now. So application must provide MediaSDK
    with external allocator and call SetFrameAllocator */
    // create VAAPI allocator
    m_pMFXAllocator.reset(new vaapiFrameAllocator);

    std::auto_ptr<vaapiAllocatorParams> p_vaapiAllocParams(new vaapiAllocatorParams);

    p_vaapiAllocParams->m_dpy = (VADisplay)hdl;
    m_pMFXAllocatorParams = p_vaapiAllocParams;

    // Call SetAllocator to pass external allocator to MediaSDK
    sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator.get());
    MSDK_CHECK_STATUS(sts, "m_mfxSession.SetFrameAllocator failed");

    // initialize memory allocator
    sts = m_pMFXAllocator->Init(m_pMFXAllocatorParams.get());
    MSDK_CHECK_STATUS(sts, "m_pMFXAllocator->Init failed");

    return sts;
}

mfxStatus CEncodingPipeline::CreateHWDevice()
{
    mfxStatus sts = MFX_ERR_NONE;

    m_pHWdev.reset(CreateVAAPIDevice());
    MSDK_CHECK_POINTER(m_pHWdev.get(), MFX_ERR_MEMORY_ALLOC);

    sts = m_pHWdev->Init(NULL, 0, MSDKAdapter::GetNumber(m_mfxSession));
    MSDK_CHECK_STATUS(sts, "m_pHWdev->Init failed");

    return sts;
}


// fill FrameInfo structure with user parameters taking into account that input stream sequence
// will be stored in MSDK surfaces, i.e. width/height should be aligned, FourCC within supported formats
mfxStatus CEncodingPipeline::FillInputFrameInfo(mfxFrameInfo& fi)
{
    bool isProgressive = (MFX_PICSTRUCT_PROGRESSIVE == m_inParams.nPicStruct);
    fi.FourCC       = MFX_FOURCC_NV12;
    fi.ChromaFormat = FourCCToChroma(fi.FourCC);
    fi.PicStruct    = m_inParams.nPicStruct;

    fi.CropX = 0;
    fi.CropY = 0;
    fi.CropW = m_inParams.nWidth;
    fi.CropH = m_inParams.nHeight;
    fi.Width = MSDK_ALIGN16(fi.CropW);
    fi.Height = isProgressive ? MSDK_ALIGN16(fi.CropH) : MSDK_ALIGN32(fi.CropH);

    mfxStatus sts = ConvertFrameRate(m_inParams.dFrameRate, &fi.FrameRateExtN, &fi.FrameRateExtD);
    MSDK_CHECK_STATUS(sts, "ConvertFrameRate failed");

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocFrames()
{
    mfxStatus sts = MFX_ERR_NOT_INITIALIZED;

    mfxFrameAllocRequest encRequest;
    MSDK_ZERO_MEMORY(encRequest);

    if (m_pFEI_Encode.get())
    {
        sts = m_pFEI_Encode->QueryIOSurf(&encRequest);
        MSDK_CHECK_STATUS(sts, "m_pFEI_Encode->QueryIOSurf failed");

        // prepare allocation requests
        MSDK_MEMCPY_VAR(encRequest.Info, m_pFEI_Encode->GetFrameInfo(), sizeof(mfxFrameInfo));
        encRequest.Type |= MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;

        m_EncSurfPool.SetAllocator(m_pMFXAllocator.get());
        sts = m_EncSurfPool.AllocSurfaces(encRequest);
        MSDK_CHECK_STATUS(sts, "EncSurfPool->AllocSurfaces failed");
    }

    return sts;
}

mfxStatus CEncodingPipeline::InitComponents()
{
    mfxStatus sts = MFX_ERR_NONE;

    MSDK_CHECK_POINTER(m_pInputSource.get(), MFX_ERR_UNSUPPORTED);
    sts = m_pInputSource->Init();
    MSDK_CHECK_STATUS(sts, "m_pYUVSource->Init failed");

    if (m_pFEI_Encode.get())
    {
        sts = m_pFEI_Encode->Init();
        MSDK_CHECK_STATUS(sts, "FEI ENCODE Init failed");
    }

    return sts;
}

mfxStatus CEncodingPipeline::Execute()
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxFrameSurface1* pSurf = NULL; // points to frame being processed

    mfxU32 frameCount = 0;

    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)
    {
        if (m_inParams.nNumFrames && m_inParams.nNumFrames <= frameCount) // frame encoding limit
            break;

        sts = m_pInputSource->GetOneFrame(pSurf);
        MSDK_BREAK_ON_ERROR(sts);

        frameCount++;

        if (m_pFEI_Encode.get())
        {
            sts = m_pFEI_Encode->EncodeOneFrame(pSurf);
            MSDK_BREAK_ON_ERROR(sts);
        }

    } // while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)

    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_STATUS(sts, "Frame processing failed");

    sts = DrainBufferedFrames();
    MSDK_CHECK_STATUS(sts, "Buffered frames processing failed");

    return sts;
}

mfxStatus CEncodingPipeline::DrainBufferedFrames()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_pFEI_Encode.get())
    {
        while (MFX_ERR_NONE <= sts)
        {
            sts = m_pFEI_Encode->EncodeOneFrame(NULL);
        }

        // MFX_ERR_MORE_DATA indicates that there are no more buffered frames
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
        // exit in case of other errors
        MSDK_CHECK_STATUS(sts, "EncodeOneFrame failed");
    }

    return sts;
}
