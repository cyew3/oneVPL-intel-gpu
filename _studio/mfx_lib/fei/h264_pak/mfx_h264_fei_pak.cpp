//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined(MFX_VA) && defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_PAK)

#include "mfx_h264_fei_pak.h"

#if defined(_DEBUG)
#define mdprintf fprintf
#else
#define mdprintf(...)
#endif

using namespace MfxHwH264Encode;
using namespace MfxH264FEIcommon;

namespace MfxEncPAK
{

static bool IsVideoParamExtBufferIdSupported(mfxU32 id)
{
    return
        id == MFX_EXTBUFF_FEI_PPS        ||
        id == MFX_EXTBUFF_FEI_SPS        ||
        id == MFX_EXTBUFF_CODING_OPTION  ||
        id == MFX_EXTBUFF_CODING_OPTION2 ||
        id == MFX_EXTBUFF_CODING_OPTION3 ||
        id == MFX_EXTBUFF_FEI_PARAM;
}

static mfxStatus CheckExtBufferId(mfxVideoParam const & par)
{
    for (mfxU32 i = 0; i < par.NumExtParam; ++i)
    {
        MFX_CHECK(par.ExtParam[i], MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(MfxEncPAK::IsVideoParamExtBufferIdSupported(par.ExtParam[i]->BufferId), MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(!MfxHwH264Encode::GetExtBuffer(
            par.ExtParam + i + 1,
            par.NumExtParam - i - 1,
            par.ExtParam[i]->BufferId), MFX_ERR_INVALID_VIDEO_PARAM);
    }

    return MFX_ERR_NONE;
}

} // namespace MfxEncPAK

bool bEnc_PAK(mfxVideoParam *par)
{
    MFX_CHECK(par, false);

    mfxExtFeiParam *pControl = NULL;

    for (mfxU16 i = 0; i < par->NumExtParam; ++i)
    {
        if (par->ExtParam[i] != 0 && par->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PARAM){ // assuming aligned buffers
            pControl = reinterpret_cast<mfxExtFeiParam *>(par->ExtParam[i]);
            break;
        }
    }

    return pControl ? (pControl->Func == MFX_FEI_FUNCTION_PAK) : false;
}


static mfxStatus AsyncRoutine(void * state, void * param, mfxU32, mfxU32)
{
    VideoPAK_PAK & impl = *(VideoPAK_PAK *)state;
    return  impl.RunFramePAK(NULL, NULL);
}



mfxStatus VideoPAK_PAK::RunFramePAK(mfxPAKInput *in, mfxPAKOutput *out)
{
    mdprintf(stderr, "VideoPAK_PAK::RunFramePAK\n");

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoPAK_PAK::RunFramePAK");

    mfxStatus sts = MFX_ERR_NONE;
    DdiTask & task = m_incoming.front();

    mfxU32 f_start = 0, fieldCount = task.m_fieldPicFlag;

    if (MFX_CODINGOPTION_ON == m_singleFieldProcessingMode)
    {
        fieldCount = f_start = m_firstFieldDone; // 0 or 1
    }

    for (mfxU32 f = f_start; f <= fieldCount; ++f)
    {
        sts = m_ddi->Execute(task.m_handleRaw.first, task, task.m_fid[f], m_sei);
        MFX_CHECK(sts == MFX_ERR_NONE, Error(sts));
    }

    if (MFX_CODINGOPTION_ON == m_singleFieldProcessingMode)
    {
        m_firstFieldDone = 1 - m_firstFieldDone;
    }

    if (0 == m_firstFieldDone)
    {
        m_prevTask = task;
    }

    return sts;
}

static mfxStatus AsyncQuery(void * state, void * param, mfxU32 /*threadNumber*/, mfxU32 /*callNumber*/)
{
    VideoPAK_PAK & impl = *(VideoPAK_PAK *)state;
    DdiTask& task = *(DdiTask *)param;
    return impl.QueryStatus(task);
}

mfxStatus VideoPAK_PAK::QueryStatus(DdiTask& task)
{
    mdprintf(stderr,"VideoPAK_PAK::QueryStatus\n");
    mfxStatus sts = MFX_ERR_NONE;

    mfxU32 f_start = 0, fieldCount = task.m_fieldPicFlag;

    if (MFX_CODINGOPTION_ON == m_singleFieldProcessingMode)
    {
        f_start = fieldCount = 1 - m_firstFieldDone;
    }

    for (mfxU32 f = f_start; f <= fieldCount; f++)
    {
        sts = m_ddi->QueryStatus(task, task.m_fid[f]);
        MFX_CHECK(sts != MFX_WRN_DEVICE_BUSY, MFX_TASK_BUSY);
        MFX_CHECK(sts == MFX_ERR_NONE, Error(sts));
    }

    mfxPAKInput  *input  = reinterpret_cast<mfxPAKInput  *>(task.m_userData[0]);
    mfxPAKOutput *output = reinterpret_cast<mfxPAKOutput *>(task.m_userData[1]);

    m_core->DecreaseReference(&input->InSurface->Data);
    m_core->DecreaseReference(&output->OutSurface->Data);

    UMC::AutomaticUMCMutex guard(m_listMutex);
    //move that task to free tasks from m_incoming
    //m_incoming
    std::list<DdiTask>::iterator it = std::find(m_incoming.begin(), m_incoming.end(), task);
    MFX_CHECK(it != m_incoming.end(), MFX_ERR_NOT_FOUND);

    m_free.splice(m_free.end(), m_incoming, it);

    ReleaseResource(m_rec, task.m_midRec);

    return MFX_ERR_NONE;
}

mfxStatus VideoPAK_PAK::Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out)
{
    mdprintf(stderr, "VideoPAK_PAK::Query\n");

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoPAK_PAK::Query");

    return MFX_ERR_NONE;
}

mfxStatus VideoPAK_PAK::QueryIOSurf(VideoCORE* , mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR2(par,request);
#if 0
    mfxExtLAControl *pControl = (mfxExtLAControl *) GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_LOOKAHEAD_CTRL);

    MFX_CHECK(pControl,                 MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(pControl->LookAheadDepth, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxU32 inPattern = par->IOPattern & MFX_IOPATTERN_IN_MASK;
    MFX_CHECK(
        inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        inPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY  ||
        inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY,
        MFX_ERR_INVALID_VIDEO_PARAM);

    if (inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        request->Type =
            MFX_MEMTYPE_EXTERNAL_FRAME |
            MFX_MEMTYPE_FROM_ENCODE |
            MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else // MFX_IOPATTERN_IN_VIDEO_MEMORY || MFX_IOPATTERN_IN_OPAQUE_MEMORY
    {
        request->Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
        request->Type |= (inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            ? MFX_MEMTYPE_OPAQUE_FRAME
            : MFX_MEMTYPE_EXTERNAL_FRAME;
    }
    request->NumFrameMin         = GetRefDist(par) + GetAsyncDeph(par) + pControl->LookAheadDepth;
    request->NumFrameSuggested   = request->NumFrameMin;
    request->Info                = par->mfx.FrameInfo;
#endif
    return MFX_ERR_NONE;
}

VideoPAK_PAK::VideoPAK_PAK(VideoCORE *core,  mfxStatus * sts)
    : m_bInit(false)
    , m_core(core)
    , m_caps()
    , m_video()
    , m_videoInit()
    , m_prevTask()
    , m_inputFrameType(0)
    , m_currentPlatform(MFX_HW_UNKNOWN)
    , m_currentVaType(MFX_HW_NO)
    , m_singleFieldProcessingMode(0)
    , m_firstFieldDone(0)
{
    *sts = MFX_ERR_NONE;
}

VideoPAK_PAK::~VideoPAK_PAK()
{
    Close();
}

mfxStatus VideoPAK_PAK::Init(mfxVideoParam *par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoPAK_PAK::Init");
    mfxStatus sts = MfxEncPAK::CheckExtBufferId(*par);
    MFX_CHECK_STS(sts);

    m_video = *par;
    //add ext buffers from par to m_video

    MfxVideoParam tmp(*par);

    sts = ReadSpsPpsHeaders(tmp);
    MFX_CHECK_STS(sts);

    sts = CheckWidthAndHeight(tmp);
    MFX_CHECK_STS(sts);

    sts = CopySpsPpsToVideoParam(tmp);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    m_ddi.reset(new VAAPIFEIPAKEncoder);

    sts = m_ddi->CreateAuxilliaryDevice(
        m_core,
        DXVA2_Intel_Encode_AVC,
        GetFrameWidth(m_video),
        GetFrameHeight(m_video));

    MFX_CHECK(sts == MFX_ERR_NONE, MFX_WRN_PARTIAL_ACCELERATION);

    sts = m_ddi->QueryEncodeCaps(m_caps);
    MFX_CHECK(sts == MFX_ERR_NONE, MFX_WRN_PARTIAL_ACCELERATION);

    m_currentPlatform = m_core->GetHWType();
    m_currentVaType   = m_core->GetVAType();

    mfxStatus spsppsSts = CopySpsPpsToVideoParam(m_video);

    mfxStatus checkStatus = CheckVideoParam(m_video, m_caps, m_core->IsExternalFrameAllocator(), m_currentPlatform, m_currentVaType);
    MFX_CHECK(checkStatus != MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION);
    MFX_CHECK(checkStatus >= MFX_ERR_NONE, checkStatus);

    if (checkStatus == MFX_ERR_NONE)
        checkStatus = spsppsSts;

    const mfxExtFeiParam* params = GetExtBuffer(m_video);

    if (MFX_CODINGOPTION_ON == params->SingleFieldProcessing)
        m_singleFieldProcessingMode = MFX_CODINGOPTION_ON;

    //raw surfaces should be created before accel service
    mfxFrameAllocRequest request = { };
    request.Info = m_video.mfx.FrameInfo;

    /* The entire recon surface pool should be passed to vaContexCreat() finction on Init() stage.
     * And only these surfaces should be passed to driver within Execute() call.
     * The size of recon pool is limited to 127 surfaces. */
    request.Type              = MFX_MEMTYPE_FROM_PAK | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME;
    request.NumFrameMin       = m_video.mfx.GopRefDist * 2 + (m_video.AsyncDepth-1) + 1 + m_video.mfx.NumRefFrame + 1;
    request.NumFrameSuggested = request.NumFrameMin;
    request.AllocId           = par->AllocId;

    //sts = m_core->AllocFrames(&request, &m_rec);
    sts = m_rec.Alloc(m_core, request, false, true);
    //sts = m_raw.Alloc(m_core, request);
    MFX_CHECK_STS(sts);
    sts = m_ddi->Register(m_rec, D3DDDIFMT_NV12);
    MFX_CHECK_STS(sts);

    m_recFrameOrder.resize(m_rec.NumFrameActual, 0xffffffff);

    sts = CheckInitExtBuffers(m_video, *par);
    MFX_CHECK_STS(sts);

    sts = m_ddi->CreateAccelerationService(m_video);
    MFX_CHECK(sts == MFX_ERR_NONE, MFX_WRN_PARTIAL_ACCELERATION);

    m_inputFrameType =
        (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY || m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            ? MFX_IOPATTERN_IN_SYSTEM_MEMORY
            : MFX_IOPATTERN_IN_VIDEO_MEMORY;


    m_free.resize(m_video.AsyncDepth);
    m_incoming.clear();

    m_videoInit = m_video;

    m_bInit = true;
    return checkStatus;
}

mfxStatus VideoPAK_PAK::Reset(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);

    sts = MfxEncPAK::CheckExtBufferId(*par);
    MFX_CHECK_STS(sts);

    MfxVideoParam newPar = *par;

    bool isIdrRequired = false;
    mfxStatus checkStatus = ProcessAndCheckNewParameters(newPar, isIdrRequired, par);
    if (checkStatus < MFX_ERR_NONE)
        return checkStatus;

    m_ddi->Reset(newPar);

    mfxExtEncoderResetOption const * extResetOpt = GetExtBuffer(newPar);

    // perform reset of encoder and start new sequence with IDR in following cases:
    // 1) change of encoding parameters require insertion of IDR
    // 2) application explicitly asked about starting new sequence
    if (isIdrRequired || IsOn(extResetOpt->StartNewSequence))
    {
        UMC::AutomaticUMCMutex guard(m_listMutex);
        m_free.splice(m_free.end(), m_incoming);

        for (DdiTaskIter i = m_free.begin(); i != m_free.end(); ++i)
        {
            if (i->m_yuv)
                m_core->DecreaseReference(&i->m_yuv->Data);
            *i = DdiTask();
        }

        m_rec.Unlock();
    }

    m_video = newPar;

    return checkStatus;

}

mfxStatus VideoPAK_PAK::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    // For buffers which are field-based
    std::map<mfxU32, mfxU32> buffers_offsets;

    for (mfxU32 i = 0; i < par->NumExtParam; ++i)
    {
        if (buffers_offsets.find(par->ExtParam[i]->BufferId) == buffers_offsets.end())
            buffers_offsets[par->ExtParam[i]->BufferId] = 0;
        else
            buffers_offsets[par->ExtParam[i]->BufferId]++;

        if (mfxExtBuffer * buf = MfxHwH264Encode::GetExtBuffer(m_video.ExtParam, m_video.NumExtParam, par->ExtParam[i]->BufferId, buffers_offsets[par->ExtParam[i]->BufferId]))
        {
            MFX_INTERNAL_CPY(par->ExtParam[i], buf, par->ExtParam[i]->BufferSz);
        }
        else
        {
            return MFX_ERR_UNSUPPORTED;
        }
    }

    mfxExtBuffer ** ExtParam = par->ExtParam;
    mfxU16       NumExtParam = par->NumExtParam;

    MFX_INTERNAL_CPY(par, &(static_cast<mfxVideoParam &>(m_video)), sizeof(mfxVideoParam));

    par->ExtParam    = ExtParam;
    par->NumExtParam = NumExtParam;

    return MFX_ERR_NONE;
}

mfxStatus VideoPAK_PAK::RunFramePAKCheck(
                    mfxPAKInput  *input,
                    mfxPAKOutput *output,
                    MFX_ENTRY_POINT pEntryPoints[],
                    mfxU32 &numEntryPoints)
{
    mdprintf(stderr, "VideoPAK_PAK::RunFramePAKCheck\n");

    // Check that all appropriate parameters passed

    MFX_CHECK(m_bInit, MFX_ERR_UNDEFINED_BEHAVIOR);

    MFX_CHECK_NULL_PTR2(input, output);
    MFX_CHECK_NULL_PTR2(input->InSurface, output->OutSurface);

#if MFX_VERSION >= 1023
    // For now PAK doesn't have output extension buffers
    MFX_CHECK(!output->NumExtParam, MFX_ERR_UNDEFINED_BEHAVIOR);
#endif // MFX_VERSION >= 1023

    mfxStatus sts = CheckRuntimeExtBuffers(input, output, m_video);
    MFX_CHECK_STS(sts);

    // For frame type detection
    PairU8 frame_type = PairU8(mfxU8(MFX_FRAMETYPE_UNKNOWN), mfxU8(MFX_FRAMETYPE_UNKNOWN));

    mfxU32 fieldMaxCount = m_video.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
    for (mfxU32 field = 0; field < fieldMaxCount; ++field)
    {
        // Additionally check ENC specific requirements for extension buffers
        mfxExtFeiEncMV     * mvout     = GetExtBufferFEI(input, field);
        mfxExtFeiPakMBCtrl * mbcodeout = GetExtBufferFEI(input, field);
        MFX_CHECK(mvout && mbcodeout, MFX_ERR_INVALID_VIDEO_PARAM);

        // Frame type detection
        mfxExtFeiPPS* extFeiPPSinRuntime = GetExtBufferFEI(input, field);

#if MFX_VERSION >= 1023
        mfxU8 type = extFeiPPSinRuntime->FrameType;
#else
        mfxU8 type = extFeiPPSinRuntime->PictureType;
#endif // MFX_VERSION >= 1023

        // MFX_FRAMETYPE_xI / P / B disallowed here
        MFX_CHECK(!(type & 0xff00), MFX_ERR_UNDEFINED_BEHAVIOR);

        switch (type & 0xf)
        {
        case MFX_FRAMETYPE_UNKNOWN:
        case MFX_FRAMETYPE_I:
        case MFX_FRAMETYPE_P:
        case MFX_FRAMETYPE_B:
            break;
        default:
            return MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
        }

        frame_type[field] = type;
    }
    if (!frame_type[1]){ frame_type[1] = frame_type[0] & ~MFX_FRAMETYPE_IDR; }

    UMC::AutomaticUMCMutex guard(m_listMutex);

    m_free.front().m_yuv         = input->InSurface;
    //m_free.front().m_ctrl      = 0;
    m_free.front().m_type        = frame_type;

    m_free.front().m_extFrameTag = input->InSurface->Data.FrameOrder;
    m_free.front().m_frameOrder  = input->InSurface->Data.FrameOrder;
    m_free.front().m_timeStamp   = input->InSurface->Data.TimeStamp;
    m_free.front().m_userData.resize(2);
    m_free.front().m_userData[0] = input;
    m_free.front().m_userData[1] = output;

    MFX_CHECK_NULL_PTR1(output->Bs);
    m_free.front().m_bs = output->Bs;

    m_incoming.splice(m_incoming.end(), m_free, m_free.begin());
    DdiTask& task = m_incoming.front();

    task.m_picStruct    = GetPicStruct(m_video, task);
    task.m_fieldPicFlag = task.m_picStruct[ENC] != MFX_PICSTRUCT_PROGRESSIVE;
    task.m_fid[0]       = task.m_picStruct[ENC] == MFX_PICSTRUCT_FIELD_BFF;
    task.m_fid[1]       = task.m_fieldPicFlag - task.m_fid[0];

    if (task.m_picStruct[ENC] == MFX_PICSTRUCT_FIELD_BFF)
    {
        std::swap(task.m_type.top, task.m_type.bot);
    }

    m_core->IncreaseReference(&input->InSurface->Data);
    m_core->IncreaseReference(&output->OutSurface->Data);

    // Configure current task
    //if (m_firstFieldDone == 0)
    {
        sts = GetNativeHandleToRawSurface(*m_core, m_video, task, task.m_handleRaw);
        MFX_CHECK(sts == MFX_ERR_NONE, Error(sts));

        sts = CopyRawSurfaceToVideoMemory(*m_core, m_video, task);
        MFX_CHECK(sts == MFX_ERR_NONE, Error(sts));

        mfxHDL handle_src, handle_rec;
        sts = m_core->GetExternalFrameHDL(output->OutSurface->Data.MemId, &handle_src);
        MFX_CHECK(sts == MFX_ERR_NONE, MFX_ERR_INVALID_HANDLE);

        mfxMemId midRec;
        mfxU32 *src_surf_id = (mfxU32 *)handle_src, *rec_surf_id, i;

        for (i = 0; i < m_rec.NumFrameActual; ++i)
        {
            midRec = AcquireResource(m_rec, i);

            sts = m_core->GetFrameHDL(m_rec.mids[i], &handle_rec);
            MFX_CHECK(MFX_ERR_NONE == sts, MFX_ERR_INVALID_HANDLE);

            rec_surf_id = (mfxU32 *)handle_rec;
            if ((*src_surf_id) == (*rec_surf_id))
            {
                task.m_idxRecon = i;
                task.m_midRec   = midRec;
                break;
            }
            else
            {
                ReleaseResource(m_rec, midRec);
            }
        }
        MFX_CHECK(task.m_idxRecon != NO_INDEX && task.m_midRec != MID_INVALID && i != m_rec.NumFrameActual, MFX_ERR_UNDEFINED_BEHAVIOR);

#if MFX_VERSION >= 1023
        ConfigureTaskFEI(task, m_prevTask, m_video, input);
#else
        ConfigureTaskFEI(task, m_prevTask, m_video, input, output, m_frameOrder_frameNum);
#endif // MFX_VERSION >= 1023

        //!!! HACK !!!
        m_recFrameOrder[task.m_idxRecon] = task.m_frameOrder;
        TEMPORAL_HACK_WITH_DPB(task.m_dpb[0],          m_rec.mids, m_recFrameOrder);
        TEMPORAL_HACK_WITH_DPB(task.m_dpb[1],          m_rec.mids, m_recFrameOrder);
        TEMPORAL_HACK_WITH_DPB(task.m_dpbPostEncoding, m_rec.mids, m_recFrameOrder);
    }

    pEntryPoints[0].pState               = this;
    pEntryPoints[0].pParam               = &m_incoming.front();
    pEntryPoints[0].pCompleteProc        = 0;
    pEntryPoints[0].pGetSubTaskProc      = 0;
    pEntryPoints[0].pCompleteSubTaskProc = 0;
    pEntryPoints[0].requiredNumThreads   = 1;
    pEntryPoints[0].pRoutineName         = "AsyncRoutine";
    pEntryPoints[0].pRoutine             = AsyncRoutine;
    pEntryPoints[1] = pEntryPoints[0];
    pEntryPoints[1].pRoutineName         = "Async Query";
    pEntryPoints[1].pRoutine             = AsyncQuery;
    pEntryPoints[1].pParam               = &m_incoming.front();

    numEntryPoints = 2;

    return MFX_ERR_NONE;
}

static mfxStatus CopyRawSurfaceToVideoMemory(VideoCORE &    core,
                                        MfxVideoParam const & video,
                                        mfxFrameSurface1 *  src_sys,
                                        mfxMemId            dst_d3d,
                                        mfxHDL&             handle)
{
    mfxExtOpaqueSurfaceAlloc const * extOpaq = GetExtBuffer(video);
    MFX_CHECK(extOpaq, MFX_ERR_NOT_FOUND);

    mfxFrameData d3dSurf = {0};

    if (video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        (video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)))
    {
        mfxFrameData sysSurf = src_sys->Data;
        d3dSurf.MemId = dst_d3d;

        FrameLocker lock2(&core, sysSurf, true);

        MFX_CHECK_NULL_PTR1(sysSurf.Y)
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "Copy input (sys->d3d)");
            MFX_CHECK_STS(CopyFrameDataBothFields(&core, d3dSurf, sysSurf, video.mfx.FrameInfo));
        }

        MFX_CHECK_STS(lock2.Unlock());
    }
    else
    {
        d3dSurf.MemId =  src_sys->Data.MemId;
    }

    if (video.IOPattern != MFX_IOPATTERN_IN_OPAQUE_MEMORY)
       MFX_CHECK_STS(core.GetExternalFrameHDL(d3dSurf.MemId, &handle))
    else
       MFX_CHECK_STS(core.GetFrameHDL(d3dSurf.MemId, &handle));

    return MFX_ERR_NONE;
}


mfxStatus VideoPAK_PAK::Close(void)
{
    MFX_CHECK(m_bInit, MFX_ERR_NONE);
    m_bInit = false;
    m_ddi->Destroy();

    m_core->FreeFrames(&m_rec);
    //m_core->FreeFrames(&m_opaqHren);

    return MFX_ERR_NONE;
}


mfxStatus VideoPAK_PAK::ProcessAndCheckNewParameters(
    MfxVideoParam & newPar,
    bool & isIdrRequired,
    mfxVideoParam const * newParIn)
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxExtEncoderResetOption * extResetOpt = GetExtBuffer(newPar);

    InheritDefaultValues(m_video, newPar, newParIn);

    mfxStatus checkStatus = CheckVideoParam(newPar, m_caps, m_core->IsExternalFrameAllocator(), m_currentPlatform, m_currentVaType);
    if (checkStatus == MFX_WRN_PARTIAL_ACCELERATION)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else if (checkStatus < MFX_ERR_NONE)
        return checkStatus;

    sts = CheckInitExtBuffers(m_video, newPar);
    MFX_CHECK_STS(sts);

    mfxExtSpsHeader const * extSpsNew = GetExtBuffer(newPar);
    mfxExtSpsHeader const * extSpsOld = GetExtBuffer(m_video);

    // check if IDR required after change of encoding parameters
    bool isSpsChanged = extSpsNew->vuiParametersPresentFlag == 0 ?
        memcmp(extSpsNew, extSpsOld, sizeof(mfxExtSpsHeader) - sizeof(VuiParameters)) != 0 :
    !Equal(*extSpsNew, *extSpsOld);

    isIdrRequired = isSpsChanged
        || newPar.mfx.GopPicSize != m_video.mfx.GopPicSize;

    if (isIdrRequired && IsOff(extResetOpt->StartNewSequence))
        return MFX_ERR_INVALID_VIDEO_PARAM; // Reset can't change parameters w/o IDR. Report an error

    mfxExtCodingOption * extOptNew = GetExtBuffer(newPar);
    mfxExtCodingOption * extOptOld = GetExtBuffer(m_video);

    MFX_CHECK(
        IsAvcProfile(newPar.mfx.CodecProfile)                                   &&
        m_video.AsyncDepth                 == newPar.AsyncDepth                 &&
        m_videoInit.mfx.GopRefDist         >= newPar.mfx.GopRefDist             &&
        m_videoInit.mfx.NumSlice           >= newPar.mfx.NumSlice               &&
        m_videoInit.mfx.NumRefFrame        >= newPar.mfx.NumRefFrame            &&
        m_video.mfx.RateControlMethod      == newPar.mfx.RateControlMethod      &&
        m_videoInit.mfx.FrameInfo.Width    >= newPar.mfx.FrameInfo.Width        &&
        m_videoInit.mfx.FrameInfo.Height   >= newPar.mfx.FrameInfo.Height       &&
        m_video.mfx.FrameInfo.ChromaFormat == newPar.mfx.FrameInfo.ChromaFormat,
        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);


    MFX_CHECK(
        IsOn(extOptOld->FieldOutput) || extOptOld->FieldOutput == extOptNew->FieldOutput,
        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    return checkStatus;
}

#endif  // defined(MFX_VA) && defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_PAK)
