//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#include "enc_scd.h"
#include "mfxenc.h"
#include "mfx_utils.h"
#include "vm_time.h"

namespace MfxEncSCD
{

MFXEncPlugin* Plugin::Create()
{
    return new Plugin(false);
}

mfxStatus Plugin::CreateByDispatcher(const mfxPluginUID& guid, mfxPlugin* mfxPlg)
{
    if (memcmp(&guid, &MFX_PLUGINID_ENC_SCD, sizeof(mfxPluginUID)))
        return MFX_ERR_NOT_FOUND;

    Plugin* tmp_pplg = 0;

    try
    {
        tmp_pplg = new Plugin(false);
    }
    catch (std::bad_alloc&)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    catch (...)
    {
        delete tmp_pplg;
        return MFX_ERR_UNKNOWN;
    }

    *mfxPlg = tmp_pplg->m_adapter;
    tmp_pplg->m_createdByDispatcher = true;

    return MFX_ERR_NONE;
}

Plugin::Plugin(bool CreateByDispatcher)
    : m_createdByDispatcher(CreateByDispatcher)
    , m_adapter(this)
    , m_bInit(false)
{
}

Plugin::~Plugin()
{
    Close();
}

mfxStatus Plugin::PluginInit(mfxCoreInterface *core)
{
    MFX_CHECK_NULL_PTR1(core);

    (MFXCoreInterface&)(*this) = *core;

    return MFX_ERR_NONE;
}

mfxStatus Plugin::PluginClose()
{
    if (m_createdByDispatcher)
    {
        Release();
        m_createdByDispatcher = false;
    }
    return MFX_ERR_NONE;
}

mfxStatus Plugin::GetPluginParam(mfxPluginParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    par->PluginUID          = MFX_PLUGINID_ENC_SCD;
    par->PluginVersion      = 1;
    par->ThreadPolicy       = MFX_THREADPOLICY_SERIAL;
    par->MaxThreadNum       = ENC_SCD_MAX_THREADS;
    par->APIVersion.Major   = MFX_VERSION_MAJOR;
    par->APIVersion.Minor   = MFX_VERSION_MINOR;
    par->Type               = MFX_PLUGINTYPE_VIDEO_ENC;
    par->CodecId            = 0;

    return MFX_ERR_NONE;
}

mfxStatus Plugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);
    mfxU32 changed = 0;
    mfxU32 invalid = 0;

    if (!in)
    {
        memset(out, 0, sizeof(*out));

        out->IOPattern  = 1;
        out->AsyncDepth = 1;

        out->mfx.FrameInfo.BitDepthLuma    = 1;
        out->mfx.FrameInfo.BitDepthChroma  = 1;
        out->mfx.FrameInfo.FourCC          = 1;
        out->mfx.FrameInfo.Width           = 1;
        out->mfx.FrameInfo.Height          = 1;
        out->mfx.FrameInfo.CropX           = 1;
        out->mfx.FrameInfo.CropY           = 1;
        out->mfx.FrameInfo.CropW           = 1;
        out->mfx.FrameInfo.CropH           = 1;
        //out->mfx.FrameInfo.FrameRateExtN   = 1;
        //out->mfx.FrameInfo.FrameRateExtD   = 1;
        //out->mfx.FrameInfo.AspectRatioW    = 1;
        //out->mfx.FrameInfo.AspectRatioH    = 1;
        out->mfx.FrameInfo.ChromaFormat    = 1;
        out->mfx.FrameInfo.PicStruct       = 1;

        return MFX_ERR_NONE;
    }

    auto& par = *out;
    par = *in;

    invalid += CheckOption<mfxU16>(par.IOPattern
        , MFX_IOPATTERN_IN_OPAQUE_MEMORY
        , MFX_IOPATTERN_IN_SYSTEM_MEMORY
        , MFX_IOPATTERN_IN_VIDEO_MEMORY
        , 0) || (par.IOPattern == 0);

    invalid += CheckOption(par.mfx.FrameInfo.BitDepthLuma, 8, 0);
    invalid += CheckOption(par.mfx.FrameInfo.BitDepthChroma, 8, 0);

    invalid += CheckOption(par.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12);
    invalid += CheckOption(par.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420);

    changed += CheckOption(par.mfx.FrameInfo.Width, (par.mfx.FrameInfo.Width + 15) & ~15u);
    changed += CheckOption(par.mfx.FrameInfo.Height, (par.mfx.FrameInfo.Height + 15) & ~15u);

    changed += CheckMax(par.mfx.FrameInfo.CropX, par.mfx.FrameInfo.Width - 2);
    changed += CheckMax(par.mfx.FrameInfo.CropY, par.mfx.FrameInfo.Height - 2);
    changed += CheckMax(par.mfx.FrameInfo.CropW, par.mfx.FrameInfo.Width - par.mfx.FrameInfo.CropX);
    changed += CheckMax(par.mfx.FrameInfo.CropH, par.mfx.FrameInfo.Height - par.mfx.FrameInfo.CropY);

    changed += CheckOption<mfxU16>(par.mfx.FrameInfo.PicStruct
        , MFX_PICSTRUCT_PROGRESSIVE
        , MFX_PICSTRUCT_FIELD_TFF
        , MFX_PICSTRUCT_FIELD_BFF
        , MFX_PICSTRUCT_FIELD_SINGLE
        , MFX_PICSTRUCT_FIELD_TOP
        , MFX_PICSTRUCT_FIELD_BOTTOM
        , 0);

    if (invalid)
        return MFX_ERR_UNSUPPORTED;

    if (changed)
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

    return MFX_ERR_NONE;
}

void SetDefaults(mfxVideoParam& par)
{
    if (!par.AsyncDepth)
        par.AsyncDepth = 5;

    if (!par.mfx.FrameInfo.BitDepthLuma)
        par.mfx.FrameInfo.BitDepthLuma = 8;

    if (!par.mfx.FrameInfo.BitDepthChroma)
        par.mfx.FrameInfo.BitDepthChroma = par.mfx.FrameInfo.BitDepthLuma;
}

mfxStatus Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *)
{
    MFX_CHECK_NULL_PTR2(par, in);

    mfxVideoParam tmp;

    switch (par->IOPattern & (MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_OPAQUE_MEMORY))
    {
    case MFX_IOPATTERN_IN_SYSTEM_MEMORY:
        in->Type = MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME;
        break;
    case MFX_IOPATTERN_IN_VIDEO_MEMORY:
        in->Type = MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME;
        break;
    case MFX_IOPATTERN_IN_OPAQUE_MEMORY:
        in->Type = MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_OPAQUE_FRAME;
        break;
    default: return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    Query(par, &tmp);
    SetDefaults(tmp);

    in->Info = tmp.mfx.FrameInfo;
    in->NumFrameMin = tmp.AsyncDepth;
    in->NumFrameSuggested = in->NumFrameMin;

    return MFX_ERR_NONE;
}

mfxStatus Plugin::Init(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(!m_bInit, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxStatus sts = MFX_ERR_NONE, qsts;
    mfxCoreParam corePar = {};
    mfxVideoParam tmp;
    mfxExtOpaqueSurfaceAlloc* pOSA = 0;
    AutoClose<Plugin> close(this);

    qsts = Query(par, &tmp);
    if (qsts < 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    SetDefaults(tmp);

    m_vpar = tmp;
    m_vpar.NumExtParam = 0;
    m_vpar.ExtParam = 0;
    m_vpar_init = m_vpar;

    sts = GetCoreParam(&corePar);
    MFX_CHECK_STS(sts);

    memset(&m_osa, 0, sizeof(m_osa));

    for (mfxU16 i = 0; i < par->NumExtParam; i++)
    {
        if (par->ExtParam[i]->BufferId == MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION)
        {
            pOSA = (mfxExtOpaqueSurfaceAlloc*)par->ExtParam[i];
            break;
        }
    }


    if (m_vpar.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        if (!pOSA)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        sts = MapOpaqueSurface(pOSA->In.NumSurface, pOSA->In.Type, pOSA->In.Surfaces);
        MFX_CHECK_STS(sts);

        m_osa.In = pOSA->In;
    }

    if (isSysIn())
    {
        mfxFrameAllocRequest request = {};
        request.Info = m_vpar.mfx.FrameInfo;
        request.Type = MFX_MEMTYPE_FROM_ENC| MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME;
        request.NumFrameMin = request.NumFrameSuggested = m_vpar.AsyncDepth;

        sts = InternalSurfaces::Alloc(FrameAllocator(), request);
        MFX_CHECK_STS(sts);
    }

    TaskManager::Reset(m_vpar.AsyncDepth);

    mfxHandleType hdlType =
          (corePar.Impl & MFX_IMPL_VIA_D3D9) ? MFX_HANDLE_D3D9_DEVICE_MANAGER
        : (corePar.Impl & MFX_IMPL_VIA_D3D11) ? MFX_HANDLE_D3D11_DEVICE
        : MFX_HANDLE_VA_DISPLAY;
    mfxHDL hdl;
    mfxPlatform platform = {};

    sts = QueryPlatform(&platform);
    MFX_CHECK_STS(sts);

    sts = GetHandle(hdlType, &hdl);
    if (sts == MFX_ERR_NOT_FOUND)
        sts = CreateAccelerationDevice(hdlType, &hdl);
    MFX_CHECK_STS(sts);

    CmDevice* pCmDevice = 0;
    sts = m_core.GetHandle(m_core.pthis, MFX_HANDLE_CM_DEVICE, (mfxHDL*)&pCmDevice);
    MFX_CHECK_STS(sts);

    sts = SCD::Init(
        par->mfx.FrameInfo.CropW,
        par->mfx.FrameInfo.CropH,
        par->mfx.FrameInfo.Width,
        (par->mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_SINGLE)) ? MFX_PICSTRUCT_PROGRESSIVE : par->mfx.FrameInfo.PicStruct,
        pCmDevice);
    MFX_CHECK_STS(sts);

    sts = SetGoPSize(Immediate_GoP);
    MFX_CHECK_STS(sts);

    m_bInit = true;

    if (qsts >= 0)
        close.Cancel();

    return qsts;
}

mfxStatus Plugin::Reset(mfxVideoParam *par)
{
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(par);

    mfxStatus /*sts = MFX_ERR_NONE,*/ qsts;
    mfxVideoParam tmp;

    qsts = Query(par, &tmp);
    if (qsts < 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    SetDefaults(tmp);

#define RESET_CHECK_EQ(par) MFX_CHECK(tmp.par == m_vpar.par, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    RESET_CHECK_EQ(IOPattern);
    //RESET_CHECK_EQ(AsyncDepth);
    RESET_CHECK_EQ(mfx.FrameInfo.BitDepthLuma);
    RESET_CHECK_EQ(mfx.FrameInfo.BitDepthChroma);
    RESET_CHECK_EQ(mfx.FrameInfo.FourCC);
    RESET_CHECK_EQ(mfx.FrameInfo.Width);
    RESET_CHECK_EQ(mfx.FrameInfo.Height);
    RESET_CHECK_EQ(mfx.FrameInfo.CropX);
    RESET_CHECK_EQ(mfx.FrameInfo.CropY);
    RESET_CHECK_EQ(mfx.FrameInfo.CropW);
    RESET_CHECK_EQ(mfx.FrameInfo.CropH);
    RESET_CHECK_EQ(mfx.FrameInfo.ChromaFormat);
    //RESET_CHECK_EQ(mfx.FrameInfo.PicStruct);
#undef RESET_CHECK_EQ

    //sync tasks and try to reset
    MFX_CHECK(TaskManager::Reset(tmp.AsyncDepth), MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    m_vpar = tmp;
    m_vpar.NumExtParam = 0;
    m_vpar.ExtParam = 0;

    return qsts;
}

mfxStatus Plugin::Close()
{
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);

    TaskManager::Close();
    SCD::Close();
    InternalSurfaces::Free();

    if (m_vpar.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        if (m_osa.In.NumSurface)
            UnmapOpaqueSurface(m_osa.In.NumSurface, m_osa.In.Type, m_osa.In.Surfaces);

        m_osa.In.NumSurface = 0;
    }

    m_bInit = false;

    return MFX_ERR_NONE;
}

mfxStatus Plugin::GetVideoParam(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(par);

    mfxVideoParam tmp = m_vpar;
    tmp.NumExtParam = par->NumExtParam;
    tmp.ExtParam = par->ExtParam;
    *par = tmp;

    return sts;
}

mfxStatus Plugin::EncFrameSubmit(mfxENCInput *in, mfxENCOutput *out, mfxThreadTask *ttask)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU16 thID = 0;
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);

    if (!in || !in->InSurface)
        return MFX_ERR_MORE_DATA;
    if (!out || !ttask)
        return MFX_ERR_NULL_PTR;

    Task* pTask = New();
    if (!pTask)
        return MFX_WRN_DEVICE_BUSY;

    for (mfxU16 i = 0; i < out->NumExtParam; i++)
    {
        if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_SCENE_CHANGE)
        {
            pTask->m_pResult = (mfxExtSceneChange*)out->ExtParam[i];
            break;
        }
    }

    pTask->m_surfIn = in->InSurface;

    if (m_vpar.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        sts = GetRealSurface(in->InSurface, &pTask->m_surfRealIn);
        MFX_CHECK_STS(sts);
        if (!pTask->m_surfRealIn)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    else
        pTask->m_surfRealIn = in->InSurface;

    if (isSysIn())
    {
        if (!InternalSurfaces::Pop(pTask->m_surfNative))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        pTask->m_stages[thID] |= COPY_IN_TO_INTERNAL;
        pTask->m_stages[thID] |= DO_SCD;
    }
    else //if (!isSysIn())
    {
        pTask->m_surfNative = *pTask->m_surfRealIn;
        pTask->m_stages[thID] |= DO_SCD;
    }

    IncreaseReference(&in->InSurface->Data);

    *ttask = pTask;

    return sts;
}

mfxStatus Plugin::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 /*uid_a*/)
{
    Task* pTask = (Task*)task;

    if (!pTask->m_stages[uid_p])
        return MFX_TASK_DONE;

    mfxStatus sts = MFX_TASK_DONE;
    mfxFrameSurface1 In = *pTask->m_surfRealIn;
    mfxFrameSurface1 Native = pTask->m_surfNative;

    if (!In.Data.MemType)
        In.Data.MemType = isSysIn() ? MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

    if (m_vpar.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        Native.Data.MemType |= MFX_MEMTYPE_OPAQUE_FRAME;

    if (pTask->m_stages[uid_p] & COPY_IN_TO_INTERNAL)
    {
        sts = CopyFrame(&Native, &In);
        MFX_CHECK_STS(sts);
        pTask->m_stages[uid_p] &= ~COPY_IN_TO_INTERNAL;
    }

    if (pTask->m_stages[uid_p] & DO_SCD)
    {
        mfxU16 PicStruct = In.Info.PicStruct;
        mfxU32 shot[2] = {};
        mfxU32 last[2] = {};

        if (!PicStruct)
            PicStruct = m_vpar.mfx.FrameInfo.PicStruct;

        sts = GetFrameHandle(&Native.Data, &Native.Data.MemId);
        MFX_CHECK_STS(sts);

        sts = SCD::MapFrame(&Native);
        MFX_CHECK_STS(sts);

        if (PicStruct & MFX_PICSTRUCT_FIELD_TFF)
            SCD::SetParityTFF();
        else if (PicStruct & MFX_PICSTRUCT_FIELD_BFF)
            SCD::SetParityBFF();
        else //if (PicStruct & (MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_SINGLE))
            SCD::SetProgressiveOp();

        SCD::ProcessField();
        shot[0] = SCD::Get_frame_shot_Decision();
        last[0] = SCD::Get_frame_last_in_scene();

        if (PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))
        {
            SCD::ProcessField();
            shot[1] = SCD::Get_frame_shot_Decision();
            last[1] = SCD::Get_frame_last_in_scene();
        }
        else
        {
            shot[1] = shot[0];
            last[1] = last[0];
        }

        /*printf("\n%d : shot{%d %d} last{%d %d}", pTask->m_surfIn->Data.FrameOrder, shot[0], shot[1], last[0], last[1]);
        fflush(stdout);*/

        if (pTask->m_pResult)
        {
            pTask->m_pResult->Type = MFX_SCENE_NO_CHANGE;

            if (shot[0] || shot[1])
                pTask->m_pResult->Type |= MFX_SCENE_START;

            if (last[0] || last[1])
                pTask->m_pResult->Type |= MFX_SCENE_END;
        }

        pTask->m_stages[uid_p] &= ~DO_SCD;
    }

    return sts;
}

mfxStatus Plugin::FreeResources(mfxThreadTask task, mfxStatus)
{
    Task* pTask = (Task*)task;

    if (pTask)
    {
        DecreaseReference(&pTask->m_surfIn->Data);
        if (isSysIn())
            InternalSurfaces::Push(pTask->m_surfNative);

        Ready(pTask);
    }

    return MFX_ERR_NONE;
}

}
