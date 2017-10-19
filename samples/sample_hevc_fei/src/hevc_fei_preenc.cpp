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

IPreENC::IPreENC(MfxVideoParamsWrapper& preenc_pars)
    : m_videoParams(preenc_pars)
{
}

mfxStatus IPreENC::PreInit()
{
    mfxStatus sts = ResetExtBuffers(m_videoParams);
    return sts;
}

MfxVideoParamsWrapper IPreENC::GetVideoParam()
{
    return m_videoParams;
}

mfxStatus IPreENC::UpdateFrameInfo(mfxFrameInfo& info)
{
    m_videoParams.mfx.FrameInfo = info;

    return MFX_ERR_NONE;
}

mfxStatus IPreENC::ResetExtBuffers(const MfxVideoParamsWrapper & videoParams)
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

    MSDK_CHECK_NOT_EQUAL(m_videoParams.AsyncDepth, 1, MFX_ERR_UNSUPPORTED);

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

/**********************************************************************************/

FEI_Preenc::FEI_Preenc(MFXVideoSession* session, MfxVideoParamsWrapper& preenc_pars,
    const msdk_char* mvoutFile, const msdk_char* mbstatoutFile)
    : IPreENC(preenc_pars)
    , m_pmfxSession(session)
    , m_mfxPREENC(*m_pmfxSession)
{
    if (0 != msdk_strlen(mvoutFile))
    {
        m_pFile_MV_out.reset(new FileHandler(mvoutFile, MSDK_STRING("wb")));
    }

    if (0 != msdk_strlen(mbstatoutFile))
    {
        m_pFile_MBstat_out.reset(new FileHandler(mbstatoutFile, MSDK_STRING("wb")));
    }

    /* Default value for I-frames */
    for (size_t i = 0; i < 16; i++)
    {
        for (size_t j = 0; j < 2; j++)
        {
            m_default_MVMB.MV[i][j].x = (mfxI16)0x8000;
            m_default_MVMB.MV[i][j].y = (mfxI16)0x8000;
        }
    }
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
    mfxStatus sts = MFX_ERR_NONE;

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
    MSDK_CHECK_POINTER(request, MFX_ERR_NULL_PTR);

    // PreENC works with raw references.
    // So it needs to have a NumRefFrame number of frames.
    MSDK_MEMCPY_VAR(request->Info, &m_videoParams.mfx.FrameInfo, sizeof(mfxFrameInfo));
    request->NumFrameMin =
    request->NumFrameSuggested = m_videoParams.mfx.NumRefFrame + 1;

    return MFX_ERR_NONE;
}

mfxStatus FEI_Preenc::PreInit()
{
    mfxStatus sts = IPreENC::PreInit();
    MSDK_CHECK_STATUS(sts, "FEI PreENC ResetExtBuffers failed");

    /* The sample is dedicated to hevc encode where FIELD_SINGLE PicStruct is possible but
     * AVC PreENC doesn't know this PicStruct. So we replace FIELD_SINGLE with PROGRESSIVE */
    if (MFX_PICSTRUCT_FIELD_SINGLE == m_videoParams.mfx.FrameInfo.PicStruct)
        m_videoParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    mfxExtFeiPreEncCtrl* ctrl = m_syncp.second.first.AddExtBuffer<mfxExtFeiPreEncCtrl>();
    MSDK_CHECK_POINTER(ctrl, MFX_ERR_NULL_PTR);

    ctrl->PictureType = MFX_PICTYPE_FRAME;
    ctrl->RefPictureType[0] = ctrl->PictureType;
    ctrl->RefPictureType[1] = ctrl->PictureType;
    ctrl->DownsampleInput = MFX_CODINGOPTION_ON;  // Default is ON
    ctrl->DownsampleReference[0] = MFX_CODINGOPTION_OFF; // In sample_fei PreENC works only in encoded order
    ctrl->DownsampleReference[1] = MFX_CODINGOPTION_OFF; // so all references would be already downsampled

    ctrl->Qp             = m_videoParams.mfx.QPI;
    ctrl->LenSP          = 57;    // default value from AVC PreENC initialization
    ctrl->SearchPath     = 0;     // exhaustive (full search)
    ctrl->SearchWindow   = 5;     // 48x40 (48 SUs)
    ctrl->SubMBPartMask  = 0x00;  // all enabled
    ctrl->SubPelMode     = 0x03;  // quarter-pixel
    ctrl->IntraSAD       = 0x02;  // Haar transform
    ctrl->InterSAD       = 0x02;  // Haar transform
    ctrl->AdaptiveSearch = 0;     // default value from AVC PreENC initialization
    ctrl->MVPredictor    = 0;
    ctrl->MBQp           = 0;
    ctrl->FTEnable       = 0;     // default value from AVC PreENC initialization
    ctrl->IntraPartMask  = 0x00;  // all enabled
    ctrl->RefHeight      = 32;    // default value from AVC PreENC initialization
    ctrl->RefWidth       = 32;    // default value from AVC PreENC initialization
    ctrl->DisableMVOutput         = 1;
    ctrl->DisableStatisticsOutput = 1;
    ctrl->Enable8x8Stat           = 0; // default value from AVC PreENC initialization

    return sts;
}

mfxStatus FEI_Preenc::DumpResult(HevcTask* task)
{
    MSDK_CHECK_POINTER(task, MFX_ERR_NULL_PTR);

    if (m_pFile_MBstat_out.get())
    {
        for (std::list<PreENCOutput>::iterator it = task->m_preEncOutput.begin(); it != task->m_preEncOutput.end(); ++it)
        {
            mfxStatus sts = m_pFile_MBstat_out->Write(it->m_mb->MB, sizeof(it->m_mb->MB[0]) * it->m_mb->NumMBAlloc, 1);
            MSDK_CHECK_STATUS(sts, "Write MB stat output to file failed in DumpResult");
        }
    }

    if (m_pFile_MV_out.get())
    {
        if (task->m_frameType & MFX_FRAMETYPE_I)
        {
            // count number of MB 16x16, as PreENC works as AVC
            mfxU32 numMB = (MSDK_ALIGN16(task->m_surf->Info.Width) * MSDK_ALIGN16(task->m_surf->Info.Height)) >> 8;
            if (*task->m_ds_surf)
            {
                numMB = (MSDK_ALIGN16((*task->m_ds_surf)->Info.Width) * MSDK_ALIGN16((*task->m_ds_surf)->Info.Height)) >> 8;
            }

            for (mfxU32 k = 0; k < numMB; k++)
            {
                mfxStatus sts = m_pFile_MV_out->Write(&m_default_MVMB, sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB), 1);
                MSDK_CHECK_STATUS(sts, "Write MV output to file failed in DumpResult");
            }
        }
        else
        {
            for (std::list<PreENCOutput>::iterator it = task->m_preEncOutput.begin(); it != task->m_preEncOutput.end(); ++it)
            {
                mfxStatus sts = m_pFile_MV_out->Write(it->m_mv->MB, sizeof(it->m_mv->MB[0]) * it->m_mv->NumMBAlloc, 1);
                MSDK_CHECK_STATUS(sts, "Write MV output to file failed in DumpResult");
            }
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus FEI_Preenc::PreEncFrame(HevcTask * task)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts =  PreEncMultiFrames(task);
    MSDK_CHECK_STATUS(sts, "PreENC: PreEncMultiFrames failed");

    //drop output data to output file
    sts = DumpResult(task);
    MSDK_CHECK_STATUS(sts, "PreENC: DumpResult failed");

    return sts;
}

mfxStatus FEI_Preenc::PreEncOneFrame(HevcTask & currTask, const RefIdxPair & refFramesIdx, const bool bDownsampleInput)
{
    mfxStatus sts;
    HevcDpbArray & DPB = currTask.m_dpb[TASK_DPB_ACTIVE];

    mfxENCInputWrap & in = m_syncp.second.first;

    in.InSurface  = *currTask.m_ds_surf ? *currTask.m_ds_surf : currTask.m_surf;
    in.NumFrameL0 = (refFramesIdx.RefL0 != IDX_INVALID);
    in.NumFrameL1 = (refFramesIdx.RefL1 != IDX_INVALID);

    mfxExtFeiPreEncCtrl* ctrl = in.GetExtBuffer<mfxExtFeiPreEncCtrl>();
    MSDK_CHECK_POINTER(ctrl, MFX_ERR_NULL_PTR);

    ctrl->PictureType     = MFX_PICTYPE_FRAME;
    ctrl->DownsampleInput = bDownsampleInput ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
    ctrl->RefPictureType[0] = ctrl->RefPictureType[1] = MFX_PICTYPE_FRAME;
    ctrl->DownsampleReference[0] = ctrl->DownsampleReference[1] = MFX_CODINGOPTION_OFF;

    ctrl->RefFrame[0] = NULL;
    ctrl->RefFrame[1] = NULL;
    if (refFramesIdx.RefL0 != IDX_INVALID)
    {
        if (*currTask.m_ds_surf)
            ctrl->RefFrame[0] = *DPB[refFramesIdx.RefL0].m_ds_surf;
        else
            ctrl->RefFrame[0] = DPB[refFramesIdx.RefL0].m_surf;
    }
    if (refFramesIdx.RefL1 != IDX_INVALID)
    {
        if (*currTask.m_ds_surf)
            ctrl->RefFrame[1] = *DPB[refFramesIdx.RefL1].m_ds_surf;
        else
            ctrl->RefFrame[1] = DPB[refFramesIdx.RefL1].m_surf;
    }

    // disable MV output for I frames / if no reference frames provided
    ctrl->DisableMVOutput = (currTask.m_frameType & MFX_FRAMETYPE_I) || (IDX_INVALID == refFramesIdx.RefL0 && IDX_INVALID == refFramesIdx.RefL1);
    // enable only if mbstat dump is required
    ctrl->DisableStatisticsOutput = m_pFile_MBstat_out.get() ? 0 : 1;

    mfxENCOutputWrap & out = m_syncp.second.second;

    PreENCOutput stat;
    MSDK_ZERO_MEMORY(stat);
    if (!ctrl->DisableMVOutput)
    {
        mfxExtFeiPreEncMV * mv = out.AddExtBuffer<mfxExtFeiPreEncMV>();
        MSDK_CHECK_POINTER(mv, MFX_ERR_NULL_PTR);

        mfxExtFeiPreEncMVExtended * ext_mv = AcquireResource(m_mvs);
        MSDK_CHECK_POINTER_SAFE(ext_mv, MFX_ERR_MEMORY_ALLOC, msdk_printf(MSDK_STRING("ERROR: No free buffer in mfxExtFeiPreEncMVExtended\n")));

        mfxExtFeiPreEncMV & free_mv = *ext_mv;
        Copy(*mv, free_mv);

        stat.m_mv = ext_mv;
        stat.m_refIdxPair = refFramesIdx;
    }

    if (!ctrl->DisableStatisticsOutput)
    {
        mfxExtFeiPreEncMBStat* mb = out.AddExtBuffer<mfxExtFeiPreEncMBStat>();
        MSDK_CHECK_POINTER(mb, MFX_ERR_NULL_PTR);

        mfxExtFeiPreEncMBStatExtended * ext_mb = AcquireResource(m_mbs);
        MSDK_CHECK_POINTER_SAFE(ext_mb, MFX_ERR_MEMORY_ALLOC, msdk_printf(MSDK_STRING("ERROR: No free buffer in mfxExtFeiPreEncMBStatExtended\n")));

        mfxExtFeiPreEncMBStat & free_mb = *ext_mb;
        Copy(*mb, free_mb);

        stat.m_mb = ext_mb;
    }

    if (stat.m_mb || stat.m_mv)
        currTask.m_preEncOutput.push_back(stat);

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
    mfxU8 const (&RPL)[2][MAX_DPB_SIZE] = task.m_refPicList;

    bool bDownsampleInput = true;
    for (size_t idxL0 = 0, idxL1 = 0;
         idxL0 < task.m_numRefActive[0] || idxL1 < task.m_numRefActive[1] // Iterate thru L0/L1 frames
         || idxL0 < !!(task.m_frameType & MFX_FRAMETYPE_I); // tricky: use idxL0 for 1 iteration for I-frame
         ++idxL0, ++idxL1)
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

/**************************************************************************************************/

PreencDownsampler::PreencDownsampler(IPreENC* preenc, mfxU16 ds_factor, MFXVideoSession* parent_session,
    MFXFrameAllocator* allocator)
    : m_preenc(preenc)
    , m_parentSession(parent_session)
    , m_ds_factor(ds_factor)
    , m_DsSurfacePool(allocator)
{
    if (!m_parentSession)
        throw mfxError(MFX_ERR_NOT_INITIALIZED, "Not defined parent session in PreencDownsampler");
    if (!m_preenc.get())
        throw mfxError(MFX_ERR_NOT_INITIALIZED, "Not defined PreENC in PreencDownsampler");
}

PreencDownsampler::~PreencDownsampler()
{
    m_VPP.reset();
}

mfxStatus PreencDownsampler::Init()
{
    mfxStatus sts = m_VPP->Init();
    MSDK_CHECK_STATUS(sts, "VPP DS Init failed");

    sts = m_preenc->Init();
    MSDK_CHECK_STATUS(sts, "PreENC Init failed");

    return sts;
}

mfxStatus PreencDownsampler::QueryIOSurf(mfxFrameAllocRequest* request)
{
    MSDK_CHECK_POINTER(request, MFX_ERR_NULL_PTR);

    mfxStatus sts = m_preenc->QueryIOSurf(request);
    MSDK_CHECK_STATUS(sts, "PreENC QueryIOSurf failed");

    // reorder works before VPP DS and it locks full resolution surface even if it's not required,
    // so return VPP input + PreEnc requests instead of only VPP input request
    // to avoid lack full resolution surfaces in pipeline
    mfxFrameAllocRequest vpp_request[2];
    Zero(&vpp_request[0], 2);
    sts = m_VPP->QueryIOSurf(vpp_request);
    MSDK_CHECK_STATUS(sts, "VPP DS QueryIOSurf failed");

    request->Info = vpp_request[0].Info;
    request->NumFrameMin += request->NumFrameSuggested += vpp_request[0].NumFrameSuggested;
    request->Type |= vpp_request[0].Type;

    return sts;
}

mfxStatus PreencDownsampler::PreInit()
{
    mfxStatus sts = MFX_ERR_NONE;

    // create session for VPP DS
    {
        sts = m_vpp_session.Init(MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_VAAPI, NULL);
        MSDK_CHECK_STATUS(sts, "Init mfxSession for VPP DS failed");

        mfxHDL hdl = NULL;
        sts = m_parentSession->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
        MSDK_CHECK_STATUS(sts, "PreencDownsampler: m_parentSession->GetHandle failed");

        sts = m_vpp_session.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
        MSDK_CHECK_STATUS(sts, "SetHandle mfxSession for VPP DS failed");

        sts = m_parentSession->JoinSession(m_vpp_session);
        MSDK_CHECK_STATUS(sts, "PreencDownsampler: m_parentSession->JoinSession failed");

        MFXFrameAllocator * allocator = m_DsSurfacePool.GetAllocator();
        MSDK_CHECK_POINTER(allocator, MFX_ERR_NOT_INITIALIZED);

        sts = m_vpp_session.SetFrameAllocator(allocator);
        MSDK_CHECK_STATUS(sts, "SetFrameAllocator mfxSession for VPP DS failed");
    }

    // Fill params and create VPP instance
    {
        MfxVideoParamsWrapper par = CreateVppDSParams(m_preenc->GetVideoParam().mfx.FrameInfo);

        m_VPP.reset(new MFX_VPP(&m_vpp_session, par, &m_DsSurfacePool));
        MSDK_CHECK_POINTER(m_VPP.get(), MFX_ERR_NOT_INITIALIZED);

        sts = m_VPP->PreInit();
        MSDK_CHECK_STATUS(sts, "VPP DS PreInit failed");
    }

    mfxFrameInfo vpp_out = m_VPP->GetOutFrameInfo();
    sts = m_preenc->UpdateFrameInfo(vpp_out);
    MSDK_CHECK_STATUS(sts, "PreENC UpdateFrameInfo failed");

    sts = m_preenc->PreInit();
    MSDK_CHECK_STATUS(sts, "PreENC PreInit failed");


    // alloc frames for VPP downsampler and PreENC
    {
        mfxFrameAllocRequest alloc_request;
        MSDK_ZERO_MEMORY(alloc_request);

        // get preenc request
        sts = m_preenc->QueryIOSurf(&alloc_request);
        MSDK_CHECK_STATUS(sts, "FEI PreENC QueryIOSurf failed");

        // get VPP request
        mfxFrameAllocRequest vpp_request[2];
        Zero(&vpp_request[0], 2);
        sts = m_VPP->QueryIOSurf(vpp_request);
        MSDK_CHECK_STATUS(sts, "VPP DS QueryIOSurf failed");

        alloc_request.Type |= vpp_request[1].Type;
        alloc_request.NumFrameSuggested += vpp_request[1].NumFrameSuggested;
        alloc_request.NumFrameMin = alloc_request.NumFrameSuggested;
        sts = m_DsSurfacePool.AllocSurfaces(alloc_request);
        MSDK_CHECK_STATUS(sts, "Alloc DS surfaces failed");
    }

    return sts;
}

MfxVideoParamsWrapper PreencDownsampler::GetVideoParam()
{
    return m_preenc->GetVideoParam();
}

mfxStatus PreencDownsampler::PreEncFrame(HevcTask * task)
{
    mfxStatus sts = DownSampleFrame(*task);
    MSDK_CHECK_STATUS(sts, "DownSampleFrame failed");

    sts = m_preenc->PreEncFrame(task);

    return sts;
}

mfxStatus PreencDownsampler::DownSampleFrame(HevcTask & task)
{
    mfxFrameSurface1* pOutSurf = NULL;
    mfxStatus sts = m_VPP->ProcessFrame(task.m_surf, pOutSurf);
    MSDK_CHECK_STATUS(sts, "VPP Downsampling failed");

    *task.m_ds_surf = pOutSurf;
    // HACK: lock ds surf here, but unlock in reorder
    msdk_atomic_inc16((volatile mfxU16*)&pOutSurf->Data.Locked);

    return MFX_ERR_NONE;
}

MfxVideoParamsWrapper PreencDownsampler::CreateVppDSParams(mfxFrameInfo in_fi)
{
    MfxVideoParamsWrapper pars;
    pars.AsyncDepth = 1;
    pars.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    MSDK_MEMCPY_VAR(pars.vpp.In, &in_fi, sizeof(mfxFrameInfo));
    MSDK_MEMCPY_VAR(pars.vpp.Out, &pars.vpp.In, sizeof(mfxFrameInfo));

    pars.vpp.Out.CropX = pars.vpp.Out.CropY = 0;
    pars.vpp.Out.CropW = pars.vpp.Out.Width = MSDK_ALIGN16(pars.vpp.In.Width / m_ds_factor);
    pars.vpp.Out.CropH = pars.vpp.Out.Height =
        (MFX_PICSTRUCT_PROGRESSIVE & pars.vpp.Out.PicStruct)
        ? MSDK_ALIGN16(pars.vpp.In.Height / m_ds_factor)
        : MSDK_ALIGN32(pars.vpp.In.Height / m_ds_factor);

    return pars;
}
