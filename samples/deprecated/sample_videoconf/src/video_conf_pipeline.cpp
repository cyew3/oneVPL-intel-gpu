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

#include "mfx_samples_config.h"

//warning removing when using copy algorithm for filling values into mediasdk reflist
#define _SCL_SECURE_NO_WARNINGS

#pragma warning(disable:4996)

#include "video_conf_pipeline.h"
#include "sample_defs.h"
#include "sysmem_allocator.h"
#include <iterator>
#include <algorithm>

#if D3D_SURFACES_SUPPORT
#include "d3d_allocator.h"
#include "d3d_device.h"
#endif

#ifdef LIBVA_SUPPORT
#include "vaapi_allocator.h"
#include "vaapi_device.h"
#endif

VideoConfPipeline::VideoConfPipeline()
{
    m_pmfxAllocatorParams = NULL;
    m_pMFXAllocator = NULL;
    m_hwdev = NULL;

    m_memType = SYSTEM_MEMORY;

    /*Initializing extendedbuffers used*/
    init_ext_buffer(m_avcRefList);
    init_ext_buffer(m_extCO);
    init_ext_buffer(m_extCO2);
    init_ext_buffer(m_temporalScale);

    //initializing own structure that allow to use mediasdk's reflist arrays in c++ like style
    RefListElement item = {
        (mfxU32)MFX_FRAMEORDER_UNKNOWN,
        (mfxU32)MFX_PICSTRUCT_PROGRESSIVE
    };

    m_ReferenceLists.resize(3);

    //prefered array
    m_ReferenceLists[0].elements.resize(MSDK_ARRAY_LEN(m_avcRefList.PreferredRefList), item);
    m_ReferenceLists[0].name = MSDK_STRING("preferred");

    m_ReferenceLists[1].elements.resize(MSDK_ARRAY_LEN(m_avcRefList.RejectedRefList), item);
    m_ReferenceLists[1].name = MSDK_STRING("rejected ");

    m_ReferenceLists[2].elements.resize(MSDK_ARRAY_LEN(m_avcRefList.LongTermRefList), item);
    m_ReferenceLists[2].name = MSDK_STRING("longterm ");


    /*Initializing mfxEncodeCtrl*/
    MSDK_ZERO_MEMORY(m_ctrl);

    //reflist control not attached by default;
    m_bAvcRefListAttached = false;
}

VideoConfPipeline::~VideoConfPipeline()
{
    Close();
}

mfxStatus VideoConfPipeline::GetFreeSurface(mfxFrameSurface1*& pOutSurface)
{
    mfxU32 SleepInterval = 10; // milliseconds

    //wait if there's no free surface
    for (mfxU32 i = 0; i < MSDK_WAIT_INTERVAL; i += SleepInterval)
    {
        for (mfxU16 j = 0; j < m_EncSurfaces.size(); j++)
        {
            if (0 == m_EncSurfaces[j].Data.Locked)
            {
                pOutSurface = &m_EncSurfaces[j];
                return MFX_ERR_NONE;
            }
        }

        MSDK_SLEEP(SleepInterval);
    }

    return MFX_ERR_UNKNOWN;
}

mfxStatus VideoConfPipeline::Run()
{
    mfxStatus sts = MFX_ERR_NONE;
    bool bPulRemaining = false;

    // main loop, preprocessing and encoding
    while (MFX_ERR_NONE <= sts || (MFX_ERR_MORE_DATA == sts && !bPulRemaining))
    {
        //applying all scenario actions for current frame
        std::vector<IAction*> actions;
        m_initParams.pActionProc->GetActionsForFrame(m_nFramesProcessed, actions);
        for (size_t j = 0; j < actions.size(); j++)
        {
            sts = actions[j]->ApplyFeedBack(this);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        mfxFrameSurface1* pSurf = NULL;

        //reading to new surface until file end
        if (!bPulRemaining)
        {
            // find free surface for encoder input
            sts = GetFreeSurface(pSurf);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            // load frame from file to surface data
            // if we share allocator with Media SDK we need to call Lock to access surface data and after we're done call Unlock
            // get YUV pointers
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
            MSDK_BREAK_ON_ERROR(sts);

            mfxStatus sts2 = m_FileReader.LoadNextFrame(pSurf);

            sts = m_pMFXAllocator->Unlock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));

            MSDK_BREAK_ON_ERROR(sts);

            //file end error
            if (MFX_ERR_MORE_DATA == sts2)
            {
                bPulRemaining = true;
                pSurf = NULL;
            }
            else
            {
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

                //surface data ready, for reference list selection it is required that frameorder for every frame specified
                pSurf->Data.FrameOrder = m_nFramesProcessed;
            }
        }

        sts = EncodeFrame(pSurf);
    }

    // MFX_ERR_MORE_DATA is the correct status to exit the loop with
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // report any errors that occurred in asynchronous part
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

mfxStatus   VideoConfPipeline::SetNumL0Active(mfxU16 nActive)
{
    msdk_printf(MSDK_STRING("Frame %4d : NumRefIdxL0Active=%d\n"), m_nFramesProcessed, nActive);
    AttachAvcRefListCtrl();
    m_avcRefList.NumRefIdxL0Active = nActive;

    return MFX_ERR_NONE;
}

mfxStatus   VideoConfPipeline::ResetPipeline(int nSourceIdx)
{
    msdk_printf(MSDK_STRING("Frame %4d : Reseting pipeline(w=%d, h=%d)\n")
            , m_nFramesProcessed
            , m_initParams.sources[nSourceIdx].nWidth
            , m_initParams.sources[nSourceIdx].nHeight);

    m_FileReader.Close();
    mfxStatus sts;
    // reseting fileReader input file reader
    sts = m_FileReader.Init( m_initParams.sources[nSourceIdx].srcFile.c_str()
                           , MFX_FOURCC_YV12
                           , 0
                           , std::vector<msdk_char*>());
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    InitFrameInfo( m_initParams.sources[nSourceIdx].nWidth
                 , m_initParams.sources[nSourceIdx].nHeight);


    //applying new resolution to existed surfaces
    for (mfxU16 j = 0; j < m_EncSurfaces.size(); j++)
    {
        MSDK_MEMCPY_VAR(m_EncSurfaces[j].Info, &m_mfxEncParams.mfx.FrameInfo, sizeof (m_mfxEncParams.mfx.FrameInfo));
    }

    /*need to complete encoding with current settings*/
    /*We donot have B frames in videoconferencing pipeline
      so pulling frames with NULL surface not necessary*/

    sts = m_encoder->Reset(&m_mfxEncParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxVideoParam actualParams;

    MSDK_ZERO_MEMORY(actualParams);
    sts = m_encoder->GetVideoParam(&actualParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //allocating data for encoded bitstream
    m_bsData.resize(actualParams.mfx.BufferSizeInKB * 1000);
    MSDK_ZERO_MEMORY(m_encodedBs);
    m_encodedBs.Data = &m_bsData.front();
    m_encodedBs.MaxLength = actualParams.mfx.BufferSizeInKB * 1000;

    return MFX_ERR_NONE;
}

mfxStatus   VideoConfPipeline::InsertKeyFrame()
{
    msdk_printf(MSDK_STRING("Frame %4d : inserting key frame\n"), m_nFramesProcessed);

    m_ctrl.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
    m_pCtrl = &m_ctrl;

    return MFX_ERR_NONE;
}

void VideoConfPipeline::AttachAvcRefListCtrl()
{
    //avcreflistctrl not initialized yet
    if (!m_bAvcRefListAttached)
    {
        RefListElement item = {
            (mfxU32)MFX_FRAMEORDER_UNKNOWN,
            (mfxU32)MFX_PICSTRUCT_PROGRESSIVE
        };
        //clearing frame lists only nSize elements are used
        for (size_t i = 0; i < m_ReferenceLists.size(); i++)
        {
            std::fill_n(m_ReferenceLists[i].elements.begin(), m_ReferenceLists[i].nSize, item);
            m_ReferenceLists[i].nSize = 0;
        }

        m_extBuffers.push_back(reinterpret_cast<mfxExtBuffer*>(&m_avcRefList));
        m_bAvcRefListAttached = true;
    }
}

mfxStatus   VideoConfPipeline::AddFrameToRefList(RefListType refList, mfxU32 nFrameOrder)
{
    AttachAvcRefListCtrl();

    //not using binders just for sample simplification
    bool bDuplicate = false;
    for (size_t i = 0; i < m_ReferenceLists[refList].elements.size(); i++)
    {
        if (m_ReferenceLists[refList].elements[i].FrameOrder == nFrameOrder)
        {
            bDuplicate = true;
            break;
        }
    }

    if (!bDuplicate)
    {
        //cannot insert one more frame into reflistcontrol array
        if (m_ReferenceLists[refList].nSize + 1 == m_ReferenceLists[refList].elements.size())
        {
            msdk_printf(MSDK_STRING("Frame %4d : Error: cannot insert new frame into %s reflist. \n")
                , m_nFramesProcessed, m_ReferenceLists[refList].name.c_str());
            return MFX_ERR_UNSUPPORTED;
        }

        m_ReferenceLists[refList].elements[m_ReferenceLists[refList].nSize++].FrameOrder = nFrameOrder;

        msdk_printf(MSDK_STRING("Frame %4u : Inserted %4u >>> %s reflist (%2u/%2lu)\n")
            , m_nFramesProcessed
            , nFrameOrder
            , m_ReferenceLists[refList].name.c_str()
            , m_ReferenceLists[refList].nSize, m_ReferenceLists[refList].elements.size());
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoConfPipeline::ScaleBitrate(double dScaleBy)
{
    mfxVideoParam currentParams;
    mfxStatus  sts = MFX_ERR_NONE;

    MSDK_ZERO_MEMORY(currentParams);

    //getting curent bitrate
    if (m_initParams.pBrc.get())
    {
        currentParams.mfx.TargetKbps = m_initParams.pBrc->GetCurrentBitrate();
    }
    else
    {
        sts = m_encoder->GetVideoParam(&currentParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    /*scaling bitrate according to action parameters*/
    currentParams.mfx.TargetKbps = (std::min)((mfxU16)(currentParams.mfx.TargetKbps * dScaleBy), (mfxU16)0xFFFF);

    if (m_initParams.pBrc.get())
    {
        //modifying brc settings
        m_initParams.pBrc->Reset(&currentParams);
    }
    else
    {
        /*need to complete encoding with current settings*/
        /*this not required since no B frames in output*/
        for (;;)
        {
            sts = EncodeFrame(NULL);
            if (sts == MFX_ERR_MORE_DATA)
            {
                break;
            }
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        //reseting encoder changing only bitrate
        sts = m_encoder->Reset(&currentParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    msdk_printf(MSDK_STRING("Frame %4d : TargetBitrate = %hu Kbps\n"), m_nFramesProcessed, currentParams.mfx.TargetKbps);

    return MFX_ERR_NONE;
}

mfxStatus VideoConfPipeline::EncodeFrame(mfxFrameSurface1 *pSurf)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxSyncPoint sp = NULL;

    //reflecting changes in own analog to ref list control to native mediasdk structure if necessary
    if (m_bAvcRefListAttached)
    {
        std::copy(m_ReferenceLists[0].elements.begin(), m_ReferenceLists[0].elements.end(), (RefListElement*)m_avcRefList.PreferredRefList);
        std::copy(m_ReferenceLists[1].elements.begin(), m_ReferenceLists[1].elements.end(), (RefListElement*)m_avcRefList.RejectedRefList);
        std::copy(m_ReferenceLists[2].elements.begin(), m_ReferenceLists[2].elements.end(), (RefListElement*)m_avcRefList.LongTermRefList);
    }

    //take current frame encoding settings
    if (m_initParams.pBrc.get())
    {
        m_pCtrl   = &m_ctrl;
        m_ctrl.QP = m_initParams.pBrc->GetFrameQP();
        msdk_printf(MSDK_STRING("Frame %4d : QP = %hu\n"), m_nFramesProcessed, m_ctrl.QP);
    }
    //
    if (!m_extBuffers.empty())
    {
        m_ctrl.NumExtParam = (mfxU16)m_extBuffers.size();
        m_ctrl.ExtParam    = &m_extBuffers.front();
        m_pCtrl            = &m_ctrl;
    }

    //latency calculating
    double dencodeTime = 0.;
    {
        CTimeInterval<> stopWatch(dencodeTime, m_initParams.bCalcLAtency);

        //actual sync mode encoding
        for (;;)
        {
            // at this point surface for encoder contains frame from file
            sts = m_encoder->EncodeFrameAsync(m_pCtrl, pSurf, &m_encodedBs, &sp);

            if (MFX_ERR_NONE < sts && !sp) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts)
                    MSDK_SLEEP(1); // wait if device is busy

                continue;
            }

            break;
        }

        //if syncpoint exists - error code doesn't matter
        sts = (NULL != sp) ? MFX_ERR_NONE : sts;

        if (NULL == sp)
            return sts;

        sts = MFXVideoCORE_SyncOperation(m_mfxSession, sp, MSDK_WAIT_INTERVAL);
    }

    //flushing to file
    if (MFX_ERR_NONE == sts)
    {
        //? indicates an invalid frametype for videoconferencing, only i, and p should be outputded from encoder
        if (m_initParams.bCalcLAtency)
        {
            std::basic_string<msdk_char> frameType(MSDK_STRING("?"));
            switch(m_encodedBs.FrameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P))
            {
            case MFX_FRAMETYPE_I :
                frameType = MSDK_STRING("I");
                    break;
            case MFX_FRAMETYPE_P :
                frameType = MSDK_STRING("P");
                    break;
            }
            msdk_printf(MSDK_STRING("Frame %4d, type=%s, latency=%5.2lf ms\n"), m_nFramesProcessed, frameType.c_str(), dencodeTime * 1000);
        }

        //updating custom brc
        if (m_initParams.pBrc.get())
        {
            m_initParams.pBrc->Update(&m_encodedBs);
        }
        //action processor maintains some repeatable actions based on gop pattern
        m_initParams.pActionProc->OnFrameEncoded(&m_encodedBs);

        sts = m_FileWriter.WriteNextFrame(&m_encodedBs);
        m_nFramesProcessed ++;

        //clearing encode ctrl
        m_ctrl.FrameType = MFX_FRAMETYPE_UNKNOWN;
        m_pCtrl = NULL;
        m_extBuffers.clear();
        m_bAvcRefListAttached = false;


    }

    return sts;
}

mfxStatus VideoConfPipeline::Init(IInitParams * pParams)
{
    mfxStatus sts = MFX_ERR_NONE;
    VideoConfParams * pVConfParams = dynamic_cast<VideoConfParams *>(pParams);
    MSDK_CHECK_POINTER(pVConfParams, MFX_ERR_NULL_PTR);

    m_initParams = *pVConfParams;

    // prepare input file reader
    sts = m_FileReader.Init(pVConfParams->sources[0].srcFile.c_str(),
                            MFX_FOURCC_YV12,
                            0,
                            std::vector<msdk_char*>());
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // prepare output file writer
    sts = m_FileWriter.Init(pVConfParams->dstFile.c_str());
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // init session
    mfxIMPL impl = pVConfParams->bUseHWLib ? MFX_IMPL_HARDWARE : MFX_IMPL_SOFTWARE;

    APIChangeFeatures features = {};
    features.IntraRefresh = (pVConfParams->nRefrType == 1);
    mfxVersion version = getMinimalRequiredVersion(features);

    if (MFX_IMPL_HARDWARE == impl)
    {
        // try searching on all display adapters
        sts = m_mfxSession.Init(MFX_IMPL_HARDWARE_ANY, &version);
    }
    else
        sts = m_mfxSession.Init(MFX_IMPL_SOFTWARE, &version);

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // set memory type
    m_memType = pVConfParams->memType;

    sts = InitMfxEncParamsLowLatency();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    InitFrameInfo(m_initParams.sources[0].nWidth, m_initParams.sources[0].nHeight);

    sts = InitTemporalScalability();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //initialize own or mediasdk's
    InitBrc();

    //init intra refresh
    InitIntraRefresh();

    // create and init frame allocator , and alloc frames
    sts = CreateAllocator();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // create encoder
    m_encoder.reset(new MFXVideoENCODE(m_mfxSession));

    sts = InitMFXComponents();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    m_nFramesProcessed = 0;
    m_pCtrl = 0;
    m_extBuffers.clear();

    return MFX_ERR_NONE;
}

void VideoConfPipeline::InitBrc()
{
    m_mfxEncParams.mfx.TargetKbps              = m_initParams.nTargetKbps; // in Kbps

    if (m_initParams.pBrc.get())
    {
        //brc uses target bitrate value, but Mediasdk required CQP mode
        m_initParams.pBrc->Init(&m_mfxEncParams);

        m_mfxEncParams.mfx.RateControlMethod   = MFX_RATECONTROL_CQP;
        //20 only as a constant, it will be a adjusted by brc
        m_mfxEncParams.mfx.QPI                 = 20;
        m_mfxEncParams.mfx.QPP                 = 20;
        m_mfxEncParams.mfx.QPB                 = 20;
    }
    else
    {
        m_mfxEncParams.mfx.TargetKbps          = m_initParams.nTargetKbps; // in Kbps
        m_mfxEncParams.mfx.RateControlMethod   = MFX_RATECONTROL_VBR; //dynamic bitrate change requires for encoder to be initialized with VBR
    }
}

void VideoConfPipeline::InitIntraRefresh()
{
    if (m_initParams.nRefrType)
    {
        m_extCO2.IntRefType = m_initParams.nRefrType;
        m_extCO2.IntRefCycleSize = m_initParams.nCycleSize;
        m_extCO2.IntRefQPDelta = m_initParams.nQPDelta;

        m_extBuffers.push_back(reinterpret_cast<mfxExtBuffer*>(&m_extCO2));

        m_mfxEncParams.mfx.NumRefFrame = 1; // limitation for the rolling I feature with HW acceleration on 4th Gen Intel Core processors
    }
}

void VideoConfPipeline::DeleteFrames()
{
    m_EncSurfaces.clear();
    // delete frames
    if (m_pMFXAllocator)
    {
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_EncResponse);
    }
}

mfxStatus VideoConfPipeline::InitMFXComponents()
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_encoder->Close();
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (!m_extBuffers.empty())
    {
        m_mfxEncParams.NumExtParam = (mfxU16)m_extBuffers.size();
        m_mfxEncParams.ExtParam    = &m_extBuffers.front();
    }

    sts = AllocFrames();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = m_encoder->Init(&m_mfxEncParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxVideoParam actualParams;

    MSDK_ZERO_MEMORY(actualParams);
    sts = m_encoder->GetVideoParam(&actualParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //saving buffersize to be reused for next Encode::Reset call
    m_mfxEncParams.mfx.BufferSizeInKB = actualParams.mfx.BufferSizeInKB;

    //allocating data for encoded bitstream
    m_bsData.resize(actualParams.mfx.BufferSizeInKB * 1000);
    MSDK_ZERO_MEMORY(m_encodedBs);
    m_encodedBs.Data = &m_bsData.front();
    m_encodedBs.MaxLength = actualParams.mfx.BufferSizeInKB * 1000;

    return MFX_ERR_NONE;
}

mfxStatus VideoConfPipeline::AllocFrames()
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest EncRequest;
    mfxU16 nEncSurfNum = 0; // number of surfaces for encoder

    MSDK_ZERO_MEMORY(EncRequest);

    // Calculate the number of surfaces for components.
    // QueryIOSurf functions tell how many surfaces are required to produce at least 1 output.
    // To achieve better performance we provide extra surfaces.
    // 1 extra surface at input allows to get 1 extra output.
    sts = m_encoder->QueryIOSurf(&m_mfxEncParams, &EncRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // The number of surfaces shared by vpp output and encode input.
    // When surfaces are shared 1 surface at first component output contains output frame that goes to next component input
    nEncSurfNum = EncRequest.NumFrameSuggested;

    MSDK_MEMCPY_VAR(EncRequest.Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
    EncRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_ENCODE;

    // add info about memory type to request
    EncRequest.Type |= (D3D9_MEMORY == m_memType) ? MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET : MFX_MEMTYPE_SYSTEM_MEMORY;

    // alloc frames for encoder
    sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &EncRequest, &m_EncResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


    // prepare mfxFrameSurface1 array for encoder
    m_EncSurfaces.resize(m_EncResponse.NumFrameActual);

    for (int i = 0; i < m_EncResponse.NumFrameActual; i++)
    {
        MSDK_ZERO_MEMORY(m_EncSurfaces[i]);
        MSDK_MEMCPY_VAR(m_EncSurfaces[i].Info, &m_mfxEncParams.mfx.FrameInfo, sizeof(mfxFrameInfo));
        m_EncSurfaces[i].Data.MemId = m_EncResponse.mids[i];

    }

    return MFX_ERR_NONE;
}


mfxStatus VideoConfPipeline::InitMfxEncParamsLowLatency()
{
    MSDK_ZERO_MEMORY(m_mfxEncParams);

    m_mfxEncParams.mfx.CodecId                 = MFX_CODEC_AVC;//video conferencing features available only for AVC
    m_mfxEncParams.mfx.GopPicSize              = gopLength;//gop length should be enough to consume bitrate
    m_mfxEncParams.mfx.GopRefDist              = 1;//Distance between I- or P- key frames (1 means no B-frames)
    m_mfxEncParams.AsyncDepth                  = 1;//internal buffering should be disabled in encode

    /*Initializing Extcoding options with msxdecframebuffering = 1 to reduce decode latency*/
    m_extCO.MaxDecFrameBuffering               = 1;
    //turn on reference picture marking repetition SEI
    m_extCO.RefPicMarkRep = (mfxU16)(m_initParams.bRPMRS ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);

    m_extBuffers.push_back(reinterpret_cast<mfxExtBuffer*>(&m_extCO));

    //num ref frames doesn't affect latency
    //while ref lists management feature (for recovery from broken frames) needs at least several frames to be in the ref list, 5 is good enough
    m_mfxEncParams.mfx.NumRefFrame             = 5;

    ConvertFrameRate(m_initParams.sources[0].dFrameRate, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtN, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtD);

    // specify memory type
    if (D3D9_MEMORY == m_memType)
    {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }
    else
    {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }

    return MFX_ERR_NONE;
}

void VideoConfPipeline::InitFrameInfo(mfxU16 nWidth, mfxU16 nHeight)
{
    // frame info parameters
    m_mfxEncParams.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
    m_mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_mfxEncParams.mfx.FrameInfo.PicStruct    = MFX_PICSTRUCT_PROGRESSIVE;

    // set frame size and crops
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_mfxEncParams.mfx.FrameInfo.Width  = MSDK_ALIGN16(nWidth);
    m_mfxEncParams.mfx.FrameInfo.Height = MSDK_ALIGN(nHeight, (MFX_PICSTRUCT_PROGRESSIVE == m_mfxEncParams.mfx.FrameInfo.PicStruct) ? 16 : 32);
    m_mfxEncParams.mfx.FrameInfo.CropX  = 0;
    m_mfxEncParams.mfx.FrameInfo.CropY  = 0;
    m_mfxEncParams.mfx.FrameInfo.CropW  = nWidth;
    m_mfxEncParams.mfx.FrameInfo.CropH  = nHeight;
}

mfxStatus VideoConfPipeline::InitTemporalScalability()
{
    if (m_initParams.nTemporalScalabilityLayers != 0)
    {
        //this should be specified by application, we use 0 as an example
        m_temporalScale.BaseLayerPID = 0;
        m_temporalScale.Layer[0].Scale = 1;
        for (int i = 1; i < m_initParams.nTemporalScalabilityLayers; i++)
        {
            m_temporalScale.Layer[i].Scale = m_temporalScale.Layer[i-1].Scale * m_initParams.nTemporalScalabilityBase;
        }
        m_extBuffers.push_back((mfxExtBuffer*)&m_temporalScale);
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoConfPipeline::CreateAllocator()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (D3D9_MEMORY == m_memType)
    {
#ifdef D3D_SURFACES_SUPPORT
        sts = CreateHWDevice();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        mfxHDL hdl = NULL;
        mfxHandleType hdl_t = MFX_HANDLE_D3D9_DEVICE_MANAGER;

        sts = m_hwdev->GetHandle(hdl_t, &hdl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // handle is needed for HW library only
        mfxIMPL impl = 0;
        m_mfxSession.QueryIMPL(&impl);
        if (impl != MFX_IMPL_SOFTWARE)
        {
            sts = m_mfxSession.SetHandle(hdl_t, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        // create D3D allocator
        m_pMFXAllocator = new D3DFrameAllocator;
        MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

        D3DAllocatorParams *pd3dAllocParams = new D3DAllocatorParams;
        MSDK_CHECK_POINTER(pd3dAllocParams, MFX_ERR_MEMORY_ALLOC);
        pd3dAllocParams->pManager = reinterpret_cast<IDirect3DDeviceManager9 *>(hdl);

        m_pmfxAllocatorParams = pd3dAllocParams;
#endif
#ifdef LIBVA_SUPPORT
        sts = CreateHWDevice();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        /* It's possible to skip failed result here and switch to SW implementation,
        but we don't process this way */

        mfxHDL hdl = NULL;
        sts = m_hwdev->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
        // provide device manager to MediaSDK
        sts = m_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // create VAAPI allocator
        m_pMFXAllocator = new vaapiFrameAllocator;
        MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

        vaapiAllocatorParams *p_vaapiAllocParams = new vaapiAllocatorParams;
        MSDK_CHECK_POINTER(p_vaapiAllocParams, MFX_ERR_MEMORY_ALLOC);

        p_vaapiAllocParams->m_dpy = (VADisplay)hdl;
        m_pmfxAllocatorParams = p_vaapiAllocParams;
#endif
    }
    else
    {
        // create system memory allocator
        m_pMFXAllocator = new SysMemFrameAllocator();
        MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);
    }

    sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // initialize memory allocator
    sts = m_pMFXAllocator->Init(m_pmfxAllocatorParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

mfxStatus VideoConfPipeline::CreateHWDevice()
{
    mfxStatus sts = MFX_ERR_NONE;
#ifdef D3D_SURFACES_SUPPORT // TOFO: rewrite using m_hwdev
    POINT point = {0, 0};
    HWND window = WindowFromPoint(point);

    m_hwdev = new CD3D9Device();

    if (NULL == m_hwdev)
        return MFX_ERR_MEMORY_ALLOC;

    sts = m_hwdev->Init(
        window,
        0,
        MSDKAdapter::GetNumber(m_mfxSession));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

#elif LIBVA_SUPPORT
    m_hwdev = CreateVAAPIDevice();
    if (NULL == m_hwdev)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    sts = m_hwdev->Init(NULL, 0, MSDKAdapter::GetNumber(m_mfxSession));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
#endif
    return MFX_ERR_NONE;
}

void VideoConfPipeline::DeleteHWDevice()
{
    MSDK_SAFE_DELETE(m_hwdev);
}


void VideoConfPipeline::DeleteAllocator()
{
    // delete allocator
    MSDK_SAFE_DELETE(m_pMFXAllocator);
    MSDK_SAFE_DELETE(m_pmfxAllocatorParams);
}


mfxStatus VideoConfPipeline::Close()
{
    msdk_printf(MSDK_STRING("Frame number: %u\n"), m_FileWriter.m_nProcessedFramesNum);

    m_encoder.reset();

    DeleteFrames();

    DeleteHWDevice();

    m_mfxSession.Close();
    m_FileWriter.Close();
    m_FileReader.Close();

    // allocator if used as external for MediaSDK must be deleted after SDK components
    DeleteAllocator();

    return MFX_ERR_NONE;
}

void VideoConfPipeline::PrintInfo()
{
    msdk_printf(MSDK_STRING("\nInput file format\t%s\n"), ColorFormatToStr(m_FileReader.m_ColorFormat));
    msdk_printf(MSDK_STRING("Output video\t\t%s\n"), CodecIdToStr(m_mfxEncParams.mfx.CodecId).c_str());

    mfxFrameInfo SrcPicInfo = m_mfxEncParams.mfx.FrameInfo;

    msdk_printf(MSDK_STRING("Resolution\t\t%dx%d\n"), SrcPicInfo.Width, SrcPicInfo.Height);
    msdk_printf(MSDK_STRING("Crop X,Y,W,H\t\t%d,%d,%d,%d\n"), SrcPicInfo.CropX, SrcPicInfo.CropY, SrcPicInfo.CropW, SrcPicInfo.CropH);

    msdk_printf(MSDK_STRING("Frame rate\t\t%.2lf\n"), SrcPicInfo.FrameRateExtN * 1.0 / SrcPicInfo.FrameRateExtD);
    msdk_printf(MSDK_STRING("Bit rate(Kbps)\t\t%d\n"), m_mfxEncParams.mfx.TargetKbps);

    const msdk_char* hwLibName;
#if D3D_SURFACES_SUPPORT
     hwLibName = MSDK_STRING("d3d");
#elif LIBVA_SUPPORT
     hwLibName = MSDK_STRING("va");
#endif

    const msdk_char* sMemType = (D3D9_MEMORY == m_memType) ? MSDK_STRING("d3d") : MSDK_STRING("system");
    msdk_printf(MSDK_STRING("Memory type\t\t%s\n"), sMemType);

    mfxIMPL impl;
    m_mfxSession.QueryIMPL(&impl);

    const msdk_char* sImpl = (MFX_IMPL_HARDWARE & impl) ? MSDK_STRING("hw") : MSDK_STRING("sw");
    msdk_printf(MSDK_STRING("Media SDK impl\t\t%s\n"), sImpl);

    mfxVersion ver;
    m_mfxSession.QueryVersion(&ver);
    msdk_printf(MSDK_STRING("Media SDK version\t%d.%d\n"), ver.Major, ver.Minor);

    msdk_printf(MSDK_STRING("\n"));
}