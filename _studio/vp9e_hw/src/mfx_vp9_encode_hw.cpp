/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#include "mfxvideo++int.h"
#include "mfx_plugin_module.h"
#include "mfx_vp9_encode_hw.h"
#include "mfx_vp9_encode_hw_par.h"
#include "mfx_vp9_encode_hw_ddi.h"
#include "mfx_vp9_encode_hw_utils.h"
#include "ippi.h"
#include "ipps.h"

#if defined (AS_VP9E_PLUGIN)
namespace MfxHwVP9Encode
{

Plugin::Plugin(bool CreateByDispatcher)
    :m_createdByDispatcher(CreateByDispatcher)
    ,m_adapter(this)
{
    m_pmfxCore = 0;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_PARALLEL;
    m_PluginParam.MaxThreadNum = 1;
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = MFX_PLUGINID_VP9E_HW;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_ENCODE;
    m_PluginParam.CodecId = MFX_CODEC_VP9;
    m_PluginParam.PluginVersion = 1;

#if 0 // commented out from initial commit
    memset(&m_mfxpar, 0, sizeof(mfxVideoParam));
#endif // commented out from initial commit
}

mfxStatus Plugin::PluginInit(mfxCoreInterface * pCore)
{
    if (!pCore)
        return MFX_ERR_NULL_PTR;

    m_pmfxCore = pCore;
    m_initialized = false;

    return MFX_ERR_NONE;
}

mfxStatus Plugin::PluginClose()
{
    m_initialized = false;

    if (m_createdByDispatcher) {
        delete this;
    }

    return MFX_ERR_NONE;
}

mfxStatus Plugin::GetPluginParam(mfxPluginParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}

mfxStatus Plugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);

    ENCODE_CAPS_VP9 caps = {};
    MFX_CHECK(MFX_ERR_NONE == QueryHwCaps(m_pmfxCore, caps), MFX_ERR_UNSUPPORTED);

    return   (in == 0) ? SetSupportedParameters(out):
        CheckParameters(in,out);
}

mfxStatus Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    out;
    MFX_CHECK_NULL_PTR2(par,in);

    MFX_CHECK(CheckPattern(par->IOPattern), MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(CheckFrameSize(par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height),MFX_ERR_INVALID_VIDEO_PARAM);

    in->Type = mfxU16((par->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)? MFX_MEMTYPE_SYS_EXT:MFX_MEMTYPE_D3D_EXT) ;

    in->NumFrameMin = par->AsyncDepth;
    in->NumFrameSuggested = in->NumFrameMin;

    in->Info = par->mfx.FrameInfo;
    return MFX_ERR_NONE;
}

mfxStatus Plugin::Init(mfxVideoParam *par)
{
    if (m_initialized == true)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    mfxStatus sts  = MFX_ERR_NONE;
    mfxStatus sts1 = MFX_ERR_NONE; // to save warnings ater parameters checking

    m_video = *par;

    m_pTaskManager = new TaskManagerVmePlusPak;

    mfxExtCodingOptionVP9* pExtOpt    = GetExtBuffer(m_video);
    {
        mfxExtOpaqueSurfaceAlloc   * pExtOpaque = GetExtBuffer(m_video);

        MFX_CHECK(CheckFrameSize(par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height),MFX_ERR_INVALID_VIDEO_PARAM);

        sts1 = CheckParametersAndSetDefault(par,&m_video, pExtOpt, pExtOpaque, true ,false);
        MFX_CHECK(sts1 >=0, sts1);
    }

    m_ddi.reset(CreatePlatformVp9Encoder());
    MFX_CHECK(m_ddi.get() != 0, MFX_ERR_UNSUPPORTED);

    sts = m_ddi->CreateAuxilliaryDevice(m_pmfxCore, DXVA2_Intel_LowpowerEncode_VP9_Profile0,
    //sts = m_ddi->CreateAuxilliaryDevice(m_pmfxCore, DXVA2_Intel_Encode_VP9_Profile0,
        m_video.mfx.FrameInfo.Width, m_video.mfx.FrameInfo.Height);
    MFX_CHECK(sts == MFX_ERR_NONE, MFX_ERR_UNSUPPORTED);

    ENCODE_CAPS_VP9 caps = {};
    sts = m_ddi->QueryEncodeCaps(caps);
    if (sts != MFX_ERR_NONE)
        return MFX_ERR_UNSUPPORTED;

    sts = CheckVideoParam(m_video, caps);
    MFX_CHECK(sts>=0,sts);

    sts = m_ddi->CreateAccelerationService(m_video);
    MFX_CHECK_STS(sts);

    mfxFrameAllocRequest reqOutBs     = {};
#if 0 // segmentation support is disabled
    mfxFrameAllocRequest reqSegMap = {};
#endif // segmentation support is disabled

    // on Linux we should allocate recon surfaces first, and then create encoding context and use it for allocation of other buffers
    // initialize task manager, including allocation of recon surfaces chain
    sts = m_pTaskManager->Init(m_pmfxCore, &m_video, m_ddi->GetReconSurfFourCC());
    MFX_CHECK_STS(sts);

    sts = m_ddi->Register(m_pTaskManager->GetRecFramesForReg(), D3DDDIFMT_NV12);
    MFX_CHECK_STS(sts);

    sts = m_ddi->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_BITSTREAMDATA, reqOutBs, m_video.mfx.FrameInfo.Width, m_video.mfx.FrameInfo.Height);
    MFX_CHECK_STS(sts);

    reqOutBs.NumFrameMin = reqOutBs.NumFrameSuggested = (mfxU16)CalcNumSurfRecon(m_video);
#if 0 // segmentation support is disabled
    sts = m_ddi->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_SEGMENTMAP, reqSegMap, m_video.mfx.FrameInfo.Width, m_video.mfx.FrameInfo.Height);
    if (sts == MFX_ERR_NONE && pExtOpt->EnableMultipleSegments == MFX_CODINGOPTION_ON)
        reqSegMap.NumFrameMin = reqSegMap.NumFrameSuggested = (mfxU16)CalcNumSurfRecon(m_video);
#endif // segmentation support is disabled

    sts = m_pTaskManager->AllocInternalResources(
          m_pmfxCore
        , reqOutBs
#if 0 // segmentation support is disabled
        , reqSegMap
#endif // segmentation support is disabled
        );
    MFX_CHECK_STS(sts);
    m_maxBsSize = reqOutBs.Info.Width * reqOutBs.Info.Height;

    sts = m_ddi->Register(m_pTaskManager->GetMBDataFramesForReg(), D3DDDIFMT_INTELENCODE_BITSTREAMDATA);
    MFX_CHECK_STS(sts);
#if 0 // segmentation support is disabled
    if (reqSegMap.NumFrameMin)
    {
        sts = m_ddi->Register(m_pTaskManager->GetSegMapFramesForReg(), D3DDDIFMT_INTELENCODE_SEGMENTMAP);
        MFX_CHECK_STS(sts);
    }
#endif // segmentation support is disabled

    m_bStartIVFSequence = true;

    m_initialized = true;

    return sts1;
}

mfxStatus Plugin::Reset(mfxVideoParam *par)
{
    if (m_initialized == false)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
    par;
#if 0 // commented out from initial commit
       mfxStatus sts  = MFX_ERR_NONE;
        mfxStatus sts1 = MFX_ERR_NONE;

        //printf("HybridPakDDIImpl::Reset\n");

        MFX_CHECK_NULL_PTR1(par);
        MFX_CHECK(par->IOPattern == m_video.IOPattern, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        VP9MfxParam parBeforeReset = m_video;
        VP9MfxParam parAfterReset = *par;

        {
            mfxExtCodingOptionVP9*       pExtOpt = GetExtBuffer(parAfterReset);
            mfxExtOpaqueSurfaceAlloc*    pExtOpaque = GetExtBuffer(parAfterReset);

            sts1 = CheckParametersAndSetDefault(par,&parAfterReset, pExtOpt, pExtOpaque,true,true);
            MFX_CHECK(sts1>=0, sts1);
        }

        MFX_CHECK(parBeforeReset.AsyncDepth == parAfterReset.AsyncDepth
            && parBeforeReset.mfx.RateControlMethod == parAfterReset.mfx.RateControlMethod,
            MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        m_video = parAfterReset;

        sts = m_pTaskManager->Reset(&m_video);
        MFX_CHECK_STS(sts);

        sts = m_ddi->Reset(m_video);
        MFX_CHECK_STS(sts);

        if (IsResetOfPipelineRequired(parBeforeReset, parAfterReset))
        {
            sts = m_BSE->Reset(m_video);
        }
        MFX_CHECK_STS(sts);

        m_bStartIVFSequence = false;

        return sts1;
#endif // commented out from initial commit

        return MFX_ERR_UNSUPPORTED;
}

mfxStatus Plugin::EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task)
{
    if (m_initialized == false)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    Task* pTask = 0;
    mfxStatus sts  = MFX_ERR_NONE;

    mfxStatus checkSts = CheckEncodeFrameParam(
        m_video,
        ctrl,
        surface,
        bs,
        true);

    MFX_CHECK(checkSts >= MFX_ERR_NONE, checkSts);

    {
        UMC::AutomaticUMCMutex guard(m_taskMutex);
        sts = m_pTaskManager->InitTask(surface,bs,pTask);
        MFX_CHECK_STS(sts);
        if (ctrl)
            pTask->m_ctrl = *ctrl;

    }

    *task = (mfxThreadTask*)pTask;

    return checkSts;
}

mfxStatus Plugin::Execute(mfxThreadTask task, mfxU32 , mfxU32 )
{
    if (m_initialized == false)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    Task* pTask = (Task*)task;
    MFX_CHECK(pTask->m_status == TASK_INITIALIZED || pTask->m_status == TASK_SUBMITTED, MFX_ERR_UNDEFINED_BEHAVIOR);

    if (pTask->m_status == TASK_INITIALIZED)
    {
        VP9_LOG("\n (VP9_LOG) Frame %d Plugin::SubmitFrame +", pTask->m_frameOrder);
        mfxStatus sts = MFX_ERR_NONE;
        {
            UMC::AutomaticUMCMutex guard(m_taskMutex);
            if (MFX_ERR_NONE != m_pTaskManager->CheckHybridDependencies(*pTask))
              return MFX_TASK_BUSY;
        }
        sFrameParams        frameParams={0};
        mfxFrameSurface1    *pSurface=0;
        bool                bExternalSurface = true;

        mfxHDL surfaceHDL = 0;
        mfxHDL *pSurfaceHdl = (mfxHDL *)&surfaceHDL;

        {
            UMC::AutomaticUMCMutex guard(m_taskMutex);
            sts = SetFramesParams(m_video, pTask->m_ctrl.FrameType, pTask->m_frameOrder, &frameParams);
            MFX_CHECK_STS(sts);
            sts = m_pTaskManager->SubmitTask(&m_video, pTask, &frameParams);
            MFX_CHECK_STS(sts);
        }

        sts = pTask->GetInputSurface(pSurface, bExternalSurface);
        MFX_CHECK_STS(sts);

        sts = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, pSurface->Data.MemId, pSurfaceHdl);
        MFX_CHECK_STS(sts);

        MFX_CHECK(surfaceHDL != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
        sts = m_ddi->Execute(*pTask, surfaceHDL);
        MFX_CHECK_STS(sts);

        {
            UMC::AutomaticUMCMutex guard(m_taskMutex);
            m_pTaskManager->RememberSubmittedTask(*pTask);
        }

        VP9_LOG("\n (VP9_LOG) Frame %d Plugin::SubmitFrame -", pTask->m_frameOrder);
        return MFX_TASK_WORKING;
    }
    else
    {
        VP9_LOG("\n (VP9_LOG) Frame %d Plugin::QueryFrame +", pTask->m_frameOrder);
        mfxStatus sts = MFX_ERR_NONE;

        {
            UMC::AutomaticUMCMutex guard(m_taskMutex);
            if (MFX_ERR_NONE != m_pTaskManager->CheckHybridDependencies(*pTask))
              return MFX_TASK_BUSY;
        }

        if ((sts = m_ddi->QueryStatus(*pTask) )== MFX_WRN_DEVICE_BUSY)
        {
            return MFX_TASK_WORKING;
        }

#if 0 // rest of hybrid-specific code
        MFX_CHECK_STS(sts);
        MFX_CHECK_STS(m_ddi->QueryMBLayout(mbdataLayout, mbcoeffLayout));
#endif // rest of hybrid-specific code

        {
#if 0 // rest of hybrid-specific code
            mfxFrameData mbData = { 0 };
            mfxExtFrameSupplementalInfo frmInfo;
            mfxExtBuffer *pfrmInfo = 0;
            if (m_video.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
            {
                pfrmInfo = (mfxExtBuffer*)&frmInfo;
                frmInfo.Header.BufferId = MFX_EXTBUFF_FRAME_SUPPLEMENTAL_INFO;
                frmInfo.Header.BufferSz = sizeof(mfxExtFrameSupplementalInfo);

                mbData.ExtParam = (mfxExtBuffer**)&pfrmInfo;
                mbData.NumExtParam = 1;
            }

            if (m_video.mfx.RateControlMethod != MFX_RATECONTROL_CQP && frmInfo.Info)
            {
                sts = m_ddi->ParseBrcStatusUpdate(*pTask, frmInfo.Info);
                MFX_CHECK_STS(sts);
            }

            sts = m_BSE->SetNextFrame(0, 0, pTask->m_sFrameParams,pTask->m_frameOrder);
            MFX_CHECK_STS(sts);
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL_VTUNE, "VP9::Pack BS");

            m_BSE->RunBSP(bInsertIVF,
                    bInsertSH,
                    pTask->m_pBitsteam,
                    mbData.Y,
                    pTask->ddi_frames.m_pMBCoeff_hw->pSurface->Data.Y,
                    mbdataLayout,
                    m_pmfxCore);
#endif // rest of hybrid-specific code
        }

        {
            UMC::AutomaticUMCMutex guard(m_taskMutex);
            m_pTaskManager->RememberEncodedTask(*pTask);
        }

        sts = UpdateBitstream(*pTask);
        MFX_CHECK_STS(sts);

        sts = pTask->CompleteTask();
        MFX_CHECK_STS(sts);

        {
            UMC::AutomaticUMCMutex guard(m_taskMutex);
            pTask->FreeTask();
        }

        VP9_LOG("\n (VP9_LOG) Frame %d Plugin::QueryFrame -", pTask->m_frameOrder);
        return MFX_TASK_DONE;
    }
}

mfxStatus Plugin::FreeResources(mfxThreadTask task, mfxStatus )
{
    task; return MFX_ERR_NONE;
}

mfxStatus Plugin::Close()
{
    if (m_initialized == false)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (m_pTaskManager)
    {
        delete m_pTaskManager;
        m_pTaskManager = 0;
    }

    return MFX_ERR_NONE;
}

mfxStatus Plugin::GetVideoParam(mfxVideoParam *par)
{
    if (m_initialized == false)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_NULL_PTR1(par);
    return MfxHwVP9Encode::GetVideoParam(par, &m_video);
}

inline void UpdatePictureHeader(unsigned int frameLen, unsigned int frameNum, unsigned char *pPictureHeader)
{
    mfxU32 ivf_frame_header[3] = {frameLen, frameNum << 1, 0x00000000};
    memcpy(pPictureHeader, ivf_frame_header, sizeof (ivf_frame_header));
};

mfxStatus Plugin::UpdateBitstream(
    Task & task)
{
    mfxFrameData bitstream = {};

    FrameLocker lock(m_pmfxCore, bitstream, task.m_pOutBs->pSurface->Data.MemId);
    if (bitstream.Y == 0)
        return MFX_ERR_LOCK_MEMORY;

    mfxU32   bsSizeToCopy  = task.m_bsDataLength;
    mfxU32   bsSizeAvail   = task.m_pBitsteam->MaxLength - task.m_pBitsteam->DataOffset - task.m_pBitsteam->DataLength;
    mfxU8 *  bsData        = task.m_pBitsteam->Data + task.m_pBitsteam->DataOffset + task.m_pBitsteam->DataLength;

    assert(bsSizeToCopy <= bsSizeAvail);

    if (bsSizeToCopy > bsSizeAvail)
    {
        bsSizeToCopy = bsSizeAvail;
    }

    // Avoid segfaults on very high bitrates
    if (bsSizeToCopy > m_maxBsSize)
    {
        lock.Unlock();
        return MFX_ERR_DEVICE_FAILED;
    }

    // Copy compressed picture from d3d surface to buffer in system memory
    if (bsSizeToCopy)
    {
        IppiSize roi = {(Ipp32s)bsSizeToCopy,1};
        ippiCopyManaged_8u_C1R(bitstream.Y, bitstream.Pitch, bsData, bsSizeToCopy, roi, IPP_NONTEMPORAL_LOAD);
    }

    mfxU8 * pIVFPicHeader = InsertSeqHeader(task) ? bsData + IVF_SEQ_HEADER_SIZE_BYTES : bsData;
    UpdatePictureHeader(bsSizeToCopy - IVF_PIC_HEADER_SIZE_BYTES - (InsertSeqHeader(task) ? IVF_SEQ_HEADER_SIZE_BYTES : 0), (mfxU32)task.m_frameOrder, pIVFPicHeader);

    task.m_pBitsteam->DataLength += bsSizeToCopy;

    // Update bitstream fields
    task.m_pBitsteam->TimeStamp = task.m_timeStamp;
    task.m_pBitsteam->FrameType = mfxU16(task.m_sFrameParams.bIntra ? MFX_FRAMETYPE_I : MFX_FRAMETYPE_P);
    task.m_pBitsteam->PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    return MFX_ERR_NONE;
}


} // MfxHwVP9Encode

#endif // AS_VP9E_PLUGIN
