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

#include "fei_utils.h"
#include "hevc_fei_preenc.h"

FEI_Preenc::FEI_Preenc(MFXVideoSession * session, const mfxFrameInfo& frameInfo)
    : m_pmfxSession(session)
    , m_mfxPREENC(*m_pmfxSession)
{
    MSDK_MEMCPY_VAR(m_videoParams.mfx.FrameInfo, &frameInfo, sizeof(mfxFrameInfo));
}

FEI_Preenc::~FEI_Preenc()
{
    m_mfxPREENC.Close();

    for (size_t i = 0; i < m_mvs.size(); ++i)
    {
        delete[] m_mvs[i].MB;
    }
    for (size_t i = 0; i < m_mbs.size(); ++i)
    {
        delete[] m_mbs[i].MB;
    }

    m_pmfxSession = NULL;
}

mfxStatus FEI_Preenc::Init()
{
    mfxStatus sts = ResetExtBuffers(m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI PreENC ResetExtBuffers failed");

    sts = m_mfxPREENC.Init(&m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI PreENC Init failed");
    MSDK_CHECK_WRN(sts, "FEI PreENC Init");

    // just in case, call GetVideoParam to get current Encoder state
    sts = m_mfxPREENC.GetVideoParam(&m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI PreENC GetVideoParam failed");

    return sts;
}

mfxStatus FEI_Preenc::Reset(mfxU16 width, mfxU16 height)
{
    if (width && height)
    {
        m_videoParams.mfx.FrameInfo.Width = width;
        m_videoParams.mfx.FrameInfo.Height = height;
    }

    mfxStatus sts = m_mfxPREENC.Reset(&m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI PreENC Reset failed");
    MSDK_CHECK_WRN(sts, "FEI PreENC Reset");

    // just in case, call GetVideoParam to get current Encoder state
    sts = m_mfxPREENC.GetVideoParam(&m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI PreENC GetVideoParam failed");

    return sts;
}

mfxStatus FEI_Preenc::QueryIOSurf(mfxFrameAllocRequest* request)
{
    MSDK_CHECK_NOT_EQUAL(m_videoParams.AsyncDepth , 1, MFX_ERR_UNSUPPORTED);
    // PreENC works with raw references.
    // So it needs to have a NumRefFrame number of frames.
    MSDK_MEMCPY_VAR(request->Info, &m_videoParams.mfx.FrameInfo, sizeof(mfxFrameInfo));
    request->NumFrameMin =
    request->NumFrameSuggested = m_videoParams.mfx.NumRefFrame + 1;


    return MFX_ERR_NONE;
}

mfxStatus FEI_Preenc::SetParameters(const sInputParams& params)
{
    mfxStatus sts = MFX_ERR_NONE;

    // default settings
    m_videoParams.mfx.CodecId           = MFX_CODEC_AVC; // not MFX_CODEC_HEVC until PreENC is changed for HEVC
    m_videoParams.mfx.RateControlMethod = MFX_RATECONTROL_CQP; // For now FEI work with RATECONTROL_CQP only
    m_videoParams.mfx.TargetUsage       = 0; // FEI doesn't have support of
    m_videoParams.mfx.TargetKbps        = 0; // these features
    m_videoParams.AsyncDepth            = 1; // inherited limitation from AVC FEI
    m_videoParams.IOPattern             = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    // user defined settings
    m_videoParams.mfx.QPI = m_videoParams.mfx.QPP = m_videoParams.mfx.QPB = params.QP;
    m_videoParams.mfx.GopRefDist   = params.nRefDist;
    m_videoParams.mfx.GopPicSize   = params.nGopSize;
    m_videoParams.mfx.GopOptFlag   = params.nGopOptFlag;
    m_videoParams.mfx.IdrInterval  = params.nIdrInterval;
    m_videoParams.mfx.NumRefFrame  = params.nNumRef;
    m_videoParams.mfx.NumSlice     = params.nNumSlices;
    m_videoParams.mfx.EncodedOrder = params.bEncodedOrder;

    /* Create extension buffer to Init FEI PREENC */
    mfxExtFeiParam* pExtBufInit = m_videoParams.AddExtBuffer<mfxExtFeiParam>();
    MSDK_CHECK_POINTER(pExtBufInit, MFX_ERR_NULL_PTR);

    pExtBufInit->Func = MFX_FEI_FUNCTION_PREENC;

    // configure B-pyramid settings
    mfxExtCodingOption2* pCO2 = m_videoParams.AddExtBuffer<mfxExtCodingOption2>();
    MSDK_CHECK_POINTER(pCO2, MFX_ERR_NULL_PTR);

    pCO2->BRefType = params.BRefType;

    mfxExtCodingOption3* pCO3 = m_videoParams.AddExtBuffer<mfxExtCodingOption3>();
    MSDK_CHECK_POINTER(pCO3, MFX_ERR_NULL_PTR);

    pCO3->GPB = params.GPB;
    pCO3->NumRefActiveP[0]   = params.NumRefActiveP;
    pCO3->NumRefActiveBL0[0] = params.NumRefActiveBL0;
    pCO3->NumRefActiveBL1[0] = params.NumRefActiveBL1;

    return sts;
}

const MfxVideoParamsWrapper& FEI_Preenc::GetVideoParam()
{
    return m_videoParams;
}

mfxFrameInfo * FEI_Preenc::GetFrameInfo()
{
    return &m_videoParams.mfx.FrameInfo;
}

mfxStatus FEI_Preenc::ResetExtBuffers(const MfxVideoParamsWrapper & videoParams)
{
    for (size_t i = 0; i < m_mvs.size(); ++i)
    {
        delete[] m_mvs[i].MB;
        m_mvs[i].MB = 0;
        m_mvs[i].NumMBAlloc = 0;
    }
    for (size_t i = 0; i < m_mbs.size(); ++i)
    {
        delete[] m_mbs[i].MB;
        m_mbs[i].MB = 0;
        m_mbs[i].NumMBAlloc = 0;
    }

    MSDK_CHECK_NOT_EQUAL(m_videoParams.AsyncDepth , 1, MFX_ERR_UNSUPPORTED);

    mfxU32 nMB = videoParams.mfx.FrameInfo.Width * videoParams.mfx.FrameInfo.Height >> 8;
    mfxU8 nBuffers = videoParams.mfx.NumRefFrame;

    m_mvs.resize(nBuffers);
    m_mbs.resize(nBuffers);
    for (size_t i = 0; i < nBuffers; ++i)
    {
        MSDK_ZERO_MEMORY(m_mvs[i]);
        m_mvs[i].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV;
        m_mvs[i].Header.BufferSz = sizeof(mfxExtFeiPreEncMV);
        m_mvs[i].MB = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB[nMB];
        m_mvs[i].NumMBAlloc = nMB;

        MSDK_ZERO_MEMORY(m_mbs[i]);
        m_mbs[i].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MB;
        m_mbs[i].Header.BufferSz = sizeof(mfxExtFeiPreEncMBStat);
        m_mbs[i].MB = new mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB[nMB];
        m_mbs[i].NumMBAlloc = nMB;
    }
    return MFX_ERR_NONE;
}

mfxStatus FEI_Preenc::PreEncFrame(HevcTask * task)
{
    return PreEncMultiFrames(task);
}

mfxStatus FEI_Preenc::PreEncOneFrame(HevcTask & currTask, const RefIdxPair & refFramesIdx, const bool bDownsampleInput)
{
    mfxStatus sts;
    HevcDpbArray & DPB = currTask.m_dpb[TASK_DPB_ACTIVE];

    mfxENCInputWrap & in = m_syncp.second.first;

    in.InSurface  = currTask.m_surf;
    in.NumFrameL0 = (refFramesIdx.RefL0 != IDX_INVALID);
    in.NumFrameL1 = (refFramesIdx.RefL1 != IDX_INVALID);

    mfxExtFeiPreEncCtrl* ctrl = in.AddExtBuffer<mfxExtFeiPreEncCtrl>();
    MSDK_CHECK_POINTER(ctrl, MFX_ERR_NULL_PTR);

    ctrl->PictureType     = MFX_PICTYPE_FRAME;
    ctrl->DownsampleInput = bDownsampleInput ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
    ctrl->RefPictureType[0] = ctrl->RefPictureType[1] = MFX_PICTYPE_FRAME;
    ctrl->DownsampleReference[0] = ctrl->DownsampleReference[1] = MFX_CODINGOPTION_OFF;

    ctrl->RefFrame[0] = refFramesIdx.RefL0 != IDX_INVALID ? DPB[refFramesIdx.RefL0].m_surf : NULL;
    ctrl->RefFrame[1] = refFramesIdx.RefL1 != IDX_INVALID ? DPB[refFramesIdx.RefL1].m_surf : NULL;

    mfxENCOutputWrap & out = m_syncp.second.second;

    if (refFramesIdx.RefL0 != IDX_INVALID || refFramesIdx.RefL1 != IDX_INVALID)
    {
        mfxExtFeiPreEncMV * mv = out.AddExtBuffer<mfxExtFeiPreEncMV>();
        MSDK_CHECK_POINTER(mv, MFX_ERR_NULL_PTR);

        mfxExtFeiPreEncMVExtended * ext_mv = AcquireResource(m_mvs);
        MSDK_CHECK_POINTER_SAFE(ext_mv, MFX_ERR_MEMORY_ALLOC, msdk_printf(MSDK_STRING("ERROR: No free buffer in mfxExtFeiPreEncMVExtended\n")));

        mfxExtFeiPreEncMV & free_mv = *ext_mv;
        Copy(*mv, free_mv);

        mfxExtFeiPreEncMBStat* mb = out.AddExtBuffer<mfxExtFeiPreEncMBStat>();
        MSDK_CHECK_POINTER(mb, MFX_ERR_NULL_PTR);

        mfxExtFeiPreEncMBStatExtended * ext_mb = AcquireResource(m_mbs);
        MSDK_CHECK_POINTER_SAFE(ext_mb, MFX_ERR_MEMORY_ALLOC, msdk_printf(MSDK_STRING("ERROR: No free buffer in mfxExtFeiPreEncMBStatExtended\n")));

        mfxExtFeiPreEncMBStat & free_mb = *ext_mb;
        Copy(*mb, free_mb);

        PreENCOutput stat;
        stat.m_mv = ext_mv;
        stat.m_mb = ext_mb;
        stat.m_refIdxPair = refFramesIdx;
        currTask.m_preEncOutput.push_back(stat);
    }

    mfxSyncPoint & syncp = m_syncp.first;
    sts = m_mfxPREENC.ProcessFrameAsync(&in, &out, &syncp);
    MSDK_CHECK_STATUS(sts, "FEI PreEnc ProcessFrameAsync failed");
    MSDK_CHECK_POINTER(syncp, MFX_ERR_UNDEFINED_BEHAVIOR)

    sts = m_pmfxSession->SyncOperation(syncp, MSDK_WAIT_INTERVAL);
    MSDK_CHECK_STATUS(sts, "FEI PreEnc SyncOperation failed");

    return sts;
}

mfxStatus FEI_Preenc::PreEncMultiFrames(HevcTask* pTask)
{
    mfxStatus sts = MFX_ERR_NONE;
    HevcTask & currFrame = *pTask;
    HevcTask & task = *pTask;
    HevcDpbArray & DPB = task.m_dpb[TASK_DPB_ACTIVE];
    mfxU8 const (&RPL)[2][MAX_DPB_SIZE] = task.m_refPicList;

    bool bDownsampleInput = true;
    // Iterate thru L0/L1 frames
    for (size_t idxL0 = 0, idxL1 = 0; idxL0 < task.m_numRefActive[0] || idxL1 < task.m_numRefActive[1]; ++idxL0, ++idxL1)
    {
        RefIdxPair refFrames = {IDX_INVALID, IDX_INVALID};

        if (RPL[0][idxL0] < 15)
            refFrames.RefL0 = RPL[0][idxL0];

        if (RPL[1][idxL1] < 15)
            refFrames.RefL1 = RPL[1][idxL1];

        mfxStatus sts = PreEncOneFrame(currFrame, refFrames, bDownsampleInput);
        MSDK_CHECK_STATUS(sts, "FEI PreEncOneFrame failed");

        // If input surface is not changed between PreENC calls
        // an application can avoid redundant downsampling on driver side.
        bDownsampleInput = false;
    }

    return sts;
}
