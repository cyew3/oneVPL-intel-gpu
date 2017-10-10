//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#include "vpp_scd.h"
#include "mfx_utils.h"
#include "vm_time.h"

namespace MfxVppSCD
{

MFXVPPPlugin* Plugin::Create()
{
    return new Plugin(false);
}

mfxStatus Plugin::CreateByDispatcher(const mfxPluginUID& guid, mfxPlugin* mfxPlg)
{
    if (memcmp(&guid, &MFX_PLUGINID_VPP_SCD, sizeof(mfxPluginUID)))
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

    par->PluginUID          = MFX_PLUGINID_VPP_SCD;
    par->PluginVersion      = 1;
    par->ThreadPolicy       = MFX_THREADPOLICY_SERIAL;
    par->MaxThreadNum       = VPP_SCD_MAX_THREADS;
    par->APIVersion.Major   = MFX_VERSION_MAJOR;
    par->APIVersion.Minor   = MFX_VERSION_MINOR;
    par->Type               = MFX_PLUGINTYPE_VIDEO_VPP;
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

        out->vpp.In.BitDepthLuma    = 1;
        out->vpp.In.BitDepthChroma  = 1;
        out->vpp.In.FourCC          = 1;
        out->vpp.In.Width           = 1;
        out->vpp.In.Height          = 1;
        out->vpp.In.CropX           = 1;
        out->vpp.In.CropY           = 1;
        out->vpp.In.CropW           = 1;
        out->vpp.In.CropH           = 1;
        //out->vpp.In.FrameRateExtN   = 1;
        //out->vpp.In.FrameRateExtD   = 1;
        //out->vpp.In.AspectRatioW    = 1;
        //out->vpp.In.AspectRatioH    = 1;
        out->vpp.In.ChromaFormat    = 1;
        out->vpp.In.PicStruct       = 1;
        out->vpp.Out = out->vpp.In;

        return MFX_ERR_NONE;
    }

    auto& par = *out;
    par = *in;

    invalid += CheckOption<mfxU16>(par.IOPattern
        , MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY
        , MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY
        , MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY
        , MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY
        , MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY
        , MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY
        , MFX_IOPATTERN_IN_VIDEO_MEMORY  | MFX_IOPATTERN_OUT_OPAQUE_MEMORY
        , MFX_IOPATTERN_IN_VIDEO_MEMORY  | MFX_IOPATTERN_OUT_SYSTEM_MEMORY
        , MFX_IOPATTERN_IN_VIDEO_MEMORY  | MFX_IOPATTERN_OUT_VIDEO_MEMORY
        , 0) || (par.IOPattern == 0);

    invalid += CheckOption(par.vpp.In.BitDepthLuma, 8, 0);
    invalid += CheckOption(par.vpp.In.BitDepthChroma, 8, 0);

    invalid += CheckOption(par.vpp.In.FourCC, MFX_FOURCC_NV12);
    invalid += CheckOption(par.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV420);

    changed += CheckOption(par.vpp.In.Width, (par.vpp.In.Width + 15) & ~15u);
    changed += CheckOption(par.vpp.In.Height, (par.vpp.In.Height + 15) & ~15u);

    changed += CheckMax(par.vpp.In.CropX, par.vpp.In.Width - 2);
    changed += CheckMax(par.vpp.In.CropY, par.vpp.In.Height - 2);
    changed += CheckMax(par.vpp.In.CropW, par.vpp.In.Width - par.vpp.In.CropX);
    changed += CheckMax(par.vpp.In.CropH, par.vpp.In.Height - par.vpp.In.CropY);

    changed += CheckOption<mfxU16>(par.vpp.In.PicStruct
        , MFX_PICSTRUCT_PROGRESSIVE
        , MFX_PICSTRUCT_FIELD_TFF
        , MFX_PICSTRUCT_FIELD_BFF
        , MFX_PICSTRUCT_FIELD_SINGLE
        , MFX_PICSTRUCT_FIELD_TOP
        , MFX_PICSTRUCT_FIELD_BOTTOM
        , 0);

    changed += CheckOption(par.vpp.Out.BitDepthLuma, par.vpp.In.BitDepthLuma, 0);
    changed += CheckOption(par.vpp.Out.BitDepthChroma, par.vpp.In.BitDepthChroma, 0);
    changed += CheckOption(par.vpp.Out.FourCC, par.vpp.In.FourCC, 0);
    changed += CheckOption(par.vpp.Out.ChromaFormat, par.vpp.In.ChromaFormat, 0);
    changed += CheckOption(par.vpp.Out.Width, par.vpp.In.Width, 0);
    changed += CheckOption(par.vpp.Out.Height, par.vpp.In.Height, 0);
    changed += CheckOption(par.vpp.Out.CropX, par.vpp.In.CropX, 0);
    changed += CheckOption(par.vpp.Out.CropY, par.vpp.In.CropY, 0);
    changed += CheckOption(par.vpp.Out.CropW, par.vpp.In.CropW, 0);
    changed += CheckOption(par.vpp.Out.CropH, par.vpp.In.CropH, 0);
    changed += CheckOption(par.vpp.Out.PicStruct, par.vpp.In.PicStruct, 0);

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

    if (!par.vpp.In.BitDepthLuma)
        par.vpp.In.BitDepthLuma = 8;

    if (!par.vpp.In.BitDepthChroma)
        par.vpp.In.BitDepthChroma = par.vpp.In.BitDepthLuma;

    if (!par.vpp.Out.BitDepthLuma)
        par.vpp.Out.BitDepthLuma = par.vpp.In.BitDepthLuma;

    if (!par.vpp.Out.BitDepthChroma)
        par.vpp.Out.BitDepthChroma = par.vpp.In.BitDepthChroma;

    if (!par.vpp.Out.FourCC)
        par.vpp.Out.FourCC = par.vpp.In.FourCC;

    if (!par.vpp.Out.ChromaFormat)
        par.vpp.Out.ChromaFormat = par.vpp.In.ChromaFormat;

    if (!par.vpp.Out.Width)
        par.vpp.Out.Width = par.vpp.In.Width;

    if (!par.vpp.Out.Height)
        par.vpp.Out.Height = par.vpp.In.Height;

    if (!par.vpp.Out.CropX)
        par.vpp.Out.CropX = par.vpp.In.CropX;

    if (!par.vpp.Out.CropY)
        par.vpp.Out.CropY = par.vpp.In.CropY;

    if (!par.vpp.Out.CropW)
        par.vpp.Out.CropW = par.vpp.In.CropW;

    if (!par.vpp.Out.CropH)
        par.vpp.Out.CropH = par.vpp.In.CropH;

    if (!par.vpp.Out.PicStruct)
        par.vpp.Out.PicStruct = par.vpp.In.PicStruct;
}

mfxStatus Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    MFX_CHECK_NULL_PTR3(par, in, out);

    mfxVideoParam tmp;

    switch (par->IOPattern & (MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_OPAQUE_MEMORY))
    {
    case MFX_IOPATTERN_IN_SYSTEM_MEMORY:
        in->Type = MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME;
        break;
    case MFX_IOPATTERN_IN_VIDEO_MEMORY:
        in->Type = MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME;
        break;
    case MFX_IOPATTERN_IN_OPAQUE_MEMORY:
        in->Type = MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_OPAQUE_FRAME;
        break;
    default: return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    Query(par, &tmp);
    SetDefaults(tmp);

    in->Info = tmp.vpp.In;
    in->NumFrameMin = tmp.AsyncDepth;
    in->NumFrameSuggested = in->NumFrameMin;

    *out = *in;

    switch (par->IOPattern & (MFX_IOPATTERN_OUT_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
    {
    case MFX_IOPATTERN_OUT_SYSTEM_MEMORY:
        in->Type = MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME;
        break;
    case MFX_IOPATTERN_OUT_VIDEO_MEMORY:
        in->Type = MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME;
        break;
    case MFX_IOPATTERN_OUT_OPAQUE_MEMORY:
        in->Type = MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_OPAQUE_FRAME;
        break;
    default: return MFX_ERR_INVALID_VIDEO_PARAM;
    }

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

    if (m_vpar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        if (!pOSA)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        if (pOSA->Out.NumSurface)
        {
            sts = MapOpaqueSurface(pOSA->Out.NumSurface, pOSA->Out.Type, pOSA->Out.Surfaces);
            MFX_CHECK_STS(sts);

            m_osa.Out = pOSA->Out;
        }
    }

    if (isSysIn() && isSysOut())
    {
        mfxFrameAllocRequest request = {};
        request.Info = m_vpar.vpp.In;
        request.Type = MFX_MEMTYPE_FROM_VPPOUT| MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME;
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
        par->vpp.In.CropW,
        par->vpp.In.CropH,
        par->vpp.In.Width,
        (par->vpp.In.PicStruct & (MFX_PICSTRUCT_FIELD_SINGLE)) ? MFX_PICSTRUCT_PROGRESSIVE : par->vpp.In.PicStruct,
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
    RESET_CHECK_EQ(vpp.In.BitDepthLuma);
    RESET_CHECK_EQ(vpp.In.BitDepthChroma);
    RESET_CHECK_EQ(vpp.In.FourCC);
    RESET_CHECK_EQ(vpp.In.Width);
    RESET_CHECK_EQ(vpp.In.Height);
    RESET_CHECK_EQ(vpp.In.CropX);
    RESET_CHECK_EQ(vpp.In.CropY);
    RESET_CHECK_EQ(vpp.In.CropW);
    RESET_CHECK_EQ(vpp.In.CropH);
    RESET_CHECK_EQ(vpp.In.ChromaFormat);
    //RESET_CHECK_EQ(vpp.In.PicStruct);
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
        if (m_osa.Out.NumSurface)
            UnmapOpaqueSurface(m_osa.Out.NumSurface, m_osa.Out.Type, m_osa.Out.Surfaces);

        m_osa.In.NumSurface = 0;
        m_osa.Out.NumSurface = 0;
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

mfxStatus Plugin::VPPFrameSubmit(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *, mfxThreadTask *ttask)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU16 thID = 0;
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);

    if (!in)
        return MFX_ERR_MORE_DATA;
    if (!out || !ttask)
        return MFX_ERR_NULL_PTR;

    Task* pTask = New();
    if (!pTask)
        return MFX_WRN_DEVICE_BUSY;

    pTask->m_surfIn = in;
    pTask->m_surfOut = out;

    if (m_vpar.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        sts = GetRealSurface(in, &pTask->m_surfRealIn);
        MFX_CHECK_STS(sts);
        if (!pTask->m_surfRealIn)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    else
        pTask->m_surfRealIn = in;

    if (m_vpar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        sts = GetRealSurface(out, &pTask->m_surfRealOut);
        MFX_CHECK_STS(sts);
        if (!pTask->m_surfRealOut)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    else
        pTask->m_surfRealOut = out;

    if (isSysIn() && isSysOut())
    {
        if (!InternalSurfaces::Pop(pTask->m_surfNative))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        pTask->m_stages[thID] |= COPY_IN_TO_INTERNAL;
        pTask->m_stages[thID] |= DO_SCD;

#if defined(MFX_ENABLE_VPP_SCD_PARALLEL_COPY)
        thID = (thID + 1) % VPP_SCD_MAX_THREADS;
#endif
        if (pTask->m_surfRealIn != pTask->m_surfRealOut)
            pTask->m_stages[thID] |= COPY_IN_TO_OUT;
    }
    else if (isSysIn() && !isSysOut())
    {
        pTask->m_surfNative = *out;
        pTask->m_stages[thID] |= COPY_IN_TO_INTERNAL;
        pTask->m_stages[thID] |= DO_SCD;
    }
    else if (!isSysIn() && isSysOut())
    {
        pTask->m_surfNative = *in;
        pTask->m_stages[thID] |= DO_SCD;

#if defined(MFX_ENABLE_VPP_SCD_PARALLEL_COPY)
        thID = (thID + 1) % VPP_SCD_MAX_THREADS;
#endif
        pTask->m_stages[thID] |= COPY_IN_TO_OUT;
    }
    else if (!isSysIn() && !isSysOut())
    {
        pTask->m_surfNative = *in;
        pTask->m_stages[thID] |= DO_SCD;

#if defined(MFX_ENABLE_VPP_SCD_PARALLEL_COPY)
        thID = (thID + 1) % VPP_SCD_MAX_THREADS;
#endif
        if (pTask->m_surfRealIn != pTask->m_surfRealOut)
            pTask->m_stages[thID] |= COPY_IN_TO_OUT;
    }


    IncreaseReference(&in->Data);
    IncreaseReference(&out->Data);

    *ttask = pTask;

    return sts;
}

mfxStatus Plugin::VPPFrameSubmitEx(mfxFrameSurface1 *in, mfxFrameSurface1 *work, mfxFrameSurface1 **out, mfxThreadTask *task)
{
    if (!out)
        return MFX_ERR_NULL_PTR;

    mfxStatus sts = VPPFrameSubmit(in, work, 0, task);
    if (sts >= 0)
        *out = work;

    return sts;
}

mfxStatus Plugin::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 /*uid_a*/)
{
    Task* pTask = (Task*)task;

    if (!pTask->m_stages[uid_p])
        return MFX_TASK_DONE;

    mfxStatus sts = MFX_TASK_DONE;
    mfxFrameSurface1 In = *pTask->m_surfRealIn;
    mfxFrameSurface1 Out = *pTask->m_surfRealOut;
    mfxFrameSurface1 Native = pTask->m_surfNative;

    if (!In.Data.MemType)
        In.Data.MemType = isSysIn() ? MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;
    if (!Out.Data.MemType)
        Out.Data.MemType = isSysOut() ? MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

    if (pTask->m_stages[uid_p] & COPY_IN_TO_INTERNAL)
    {
        sts = CopyFrame(&Native, &In);
        MFX_CHECK_STS(sts);
        pTask->m_stages[uid_p] &= ~COPY_IN_TO_INTERNAL;
    }

    if (pTask->m_stages[uid_p] & COPY_IN_TO_OUT)
    {
        sts = CopyFrame(&Out, &In);
        MFX_CHECK_STS(sts);
        pTask->m_stages[uid_p] &= ~COPY_IN_TO_OUT;
    }

    if (pTask->m_stages[uid_p] & COPY_INTERNAL_TO_OUT)
    {
        sts = CopyFrame(&Native, &In);
        MFX_CHECK_STS(sts);
        pTask->m_stages[uid_p] &= ~COPY_INTERNAL_TO_OUT;
    }

    if (pTask->m_stages[uid_p] & DO_SCD)
    {
        mfxU16 PicStruct = pTask->m_surfIn->Info.PicStruct;
        mfxU32 shot[2] = {};
        mfxU32 last[2] = {};

        if (!PicStruct)
            PicStruct = m_vpar.vpp.In.PicStruct;

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

        mfxExtSceneChange* pSC = 0;
        for (mfxU16 i = 0; i < pTask->m_surfOut->Data.NumExtParam; i++)
        {
            if (pTask->m_surfOut->Data.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_SCENE_CHANGE)
            {
                pSC = (mfxExtSceneChange*)pTask->m_surfOut->Data.ExtParam[i];
                break;
            }
        }

        if (pSC)
        {
            pSC->Type = MFX_SCENE_NO_CHANGE;

            if (shot[0] || shot[1])
                pSC->Type |= MFX_SCENE_START;

            if (last[0] || last[1])
                pSC->Type |= MFX_SCENE_END;
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
        DecreaseReference(&pTask->m_surfOut->Data);
        if (isSysIn() && isSysOut())
            InternalSurfaces::Push(pTask->m_surfNative);

        Ready(pTask);
    }

    return MFX_ERR_NONE;
}

}
