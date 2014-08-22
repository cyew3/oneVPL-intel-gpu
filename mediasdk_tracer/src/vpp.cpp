/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2014 Intel Corporation. All Rights Reserved.

File Name: vpp.cpp

\* ****************************************************************************** */
#include "configuration.h"
#include "msdka_import.h"
#include "msdka_serial.h"

mfxStatus MFXVideoVPP_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out) {
    ExtendedBufferOverride ebo1(in), ebo2(out);
    StatusOverride so;

    EDIT_CONFIGURATION(TEXT("vpp.Query"),{
        if (in) if (!_tcsncmp(line2,TEXT(".in"),3)) scan_mfxVideoParam(line2+3,VPP,in, &ebo1);
        if (!_tcsncmp(line2,TEXT(".out"),4)) scan_mfxVideoParam(line2+4,VPP,out, &ebo2);
        scan_mfxStatus(line2,&so);
    });


    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("vpp.Query.in"),VPP,in);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoVPP_Query,(((AnalyzerSession *)session)->session, in, out)));

    
    RECORD_CONFIGURATION({
        //due to spec numextparams value == 1 if in is zero but validity of extparams buffer donot preserv, we cannot print it
        dump_mfxVideoParam(fd, level, TEXT("vpp.Query.out"),VPP,out, NULL != in);
        dump_mfxStatus(fd, level, TEXT("vpp.Query"),sts);
    });

    return sts;
}

mfxStatus MFXVideoVPP_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request) {
    ExtendedBufferOverride ebo(par);
    StatusOverride so;

    EDIT_CONFIGURATION(TEXT("vpp.QueryIOSurf.par"),{
        scan_mfxVideoParam(line2,VPP,par,&ebo);
        scan_mfxStatus(line2,&so);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoVPP_QueryIOSurf,(((AnalyzerSession *)session)->session, par, request)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("vpp.QueryIOSurf.par"),VPP,par);
        dump_mfxFrameAllocRequest(fd, level, TEXT("vpp.QueryIOSurf.request[0]"),request);
        dump_mfxFrameAllocRequest(fd, level, TEXT("vpp.QueryIOSurf.request[1]"),request+1);
        dump_mfxStatus(fd, level, TEXT("vpp.QueryIOSurf"),sts);
    });

    return sts;
}

mfxStatus MFXVideoVPP_Init(mfxSession session, mfxVideoParam *par) {
    StackIncreaser si;
    ExtendedBufferOverride ebo(par);
    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;

    EDIT_CONFIGURATION(TEXT("vpp.Init"),{
        if (!_tcsncmp(line2,TEXT(".par"),4)) scan_mfxVideoParam(line2+4, VPP, par,&ebo);
        scan_mfxStatus(line2,&so);
    });

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("vpp.Init.par"),VPP,par);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoVPP_Init,(as->session, par)));

    /* initialize performance counters */
    as->vpp.async_exec_time=0;
    as->vpp.async_exec_rate=0;
    as->vpp.frames=0;
    as->vpp.frames_sync=0;
    as->vpp.last_async_tick=0;
    as->vpp.exec_time=0;

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("vpp.Init"),sts);
    });

    return sts;
}

mfxStatus MFXVideoVPP_Reset(mfxSession session, mfxVideoParam *par) {
    ExtendedBufferOverride ebo(par);
    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;

    EDIT_CONFIGURATION(TEXT("vpp.Reset.par"),{
        scan_mfxVideoParam(line2, VPP, par,&ebo);
        scan_mfxStatus(line2,&so);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoVPP_Reset,(as->session, par)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("vpp.Reset.par"),VPP,par);
        dump_mfxStatus(fd, level, TEXT("vpp.Reset"),sts);
    });

    return sts;
}

mfxStatus MFXVideoVPP_Close(mfxSession session) {
    AnalyzerSession *as=(AnalyzerSession *)session;
    mfxStatus sts=MFX_CALL(MFXVideoVPP_Close,(as->session));

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("vpp.Close"),sts);
    });

    /* record performance data */
    RECORD_CONFIGURATION2(as->vpp.frames, MSDKA_LEVEL_GLOBAL, {
        LARGE_INTEGER cr;
        QueryPerformanceFrequency(&cr);

        dump_format_wprefix(fd, level, 1,TEXT("vpp.perf.frames="),TEXT("%I64u"),as->vpp.frames);
        dump_format_wprefix(fd, level, 1,TEXT("vpp.perf.async_exec_time="),TEXT("%g (s/f)"),(double)as->vpp.async_exec_time/(double)cr.QuadPart/(double)as->vpp.frames);
        dump_format_wprefix(fd, level, 1,TEXT("vpp.perf.async_exec_rate="),TEXT("%g (s/f)"),(double)as->vpp.async_exec_rate/(double)cr.QuadPart/(double)as->vpp.frames);
        if (as->vpp.frames_sync) {
            dump_format_wprefix(fd, level, 1,TEXT("vpp.perf.frames_sync="),TEXT("%I64u"),as->vpp.frames_sync);
            dump_format_wprefix(fd, level, 1,TEXT("vpp.perf.exec_time="),TEXT("%g (s/f)"),(double)as->vpp.exec_time/(double)cr.QuadPart/(double)as->vpp.frames_sync);
        }
    });

    return sts;
}

mfxStatus MFXVideoVPP_GetVideoParam(mfxSession session, mfxVideoParam *par) {
    /* temporarily work around. Submit a bug to GetVideoParam */
    ExtendedBufferOverride ebo(par);
    StatusOverride so;

    EDIT_CONFIGURATION(TEXT("vpp.Init.par"),{
        scan_mfxVideoParam(line2, VPP, par, &ebo);
        scan_mfxStatus(line2,&so);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoVPP_GetVideoParam,(((AnalyzerSession *)session)->session, par)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("vpp.GetVideoParam.par"),VPP,par);
        dump_mfxStatus(fd, level, TEXT("vpp.GetVideoParam"),sts);
    });

    return sts;
}

mfxStatus MFXVideoVPP_GetVPPStat(mfxSession session, mfxVPPStat *stat) {
    mfxStatus sts=MFX_CALL(MFXVideoVPP_GetVPPStat,(((AnalyzerSession *)session)->session, stat));

    RECORD_CONFIGURATION({
        dump_mfxVPPStat(fd, level, TEXT("vpp.GetVPPStat.stat"),stat);
    });

    return sts;
}

StatusMap sm_vpp;

mfxStatus MFXVideoVPP_RunFrameVPPAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp) {
    if (gc.debug_view) OutputDebugString(TEXT("SDK: RunFrameVPPAsync"));

    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;
    if (!as) return MFX_ERR_NULL_PTR;

    gc.RegisterSurface(VPP,RunAsync_Input, in);
    gc.RegisterSurface(VPP,RunAsync_Output, out);

    AnalyzerFrameAllocator * pAlloc = (AnalyzerFrameAllocator*)as->pAlloc ;
    if (pAlloc)
    {
        RECORD_CONFIGURATION_PER_FRAME({
            pAlloc->decode.RegisterSurface(in);
            pAlloc->encode.RegisterSurface(out);

            dump_format_wprefix(fd, level, 0, TEXT("decode.surfaces:Locked/Used/Allocated=%d/%d/%d"), pAlloc->decode.GetNumLockedSurfaces(), pAlloc->decode.surfaces.size(), pAlloc->decode.nSurfacesAllocated);
            dump_format_wprefix(fd, level, 0, TEXT("encode.surfaces:Locked/Used/Allocated=%d/%d/%d"), pAlloc->encode.GetNumLockedSurfaces(), pAlloc->encode.surfaces.size(), pAlloc->encode.nSurfacesAllocated);
        });
    }


    EDIT_CONFIGURATION_PER_FRAME(TEXT("vpp.RunFrameVPPAsync"),{
        if (in) if (!_tcsncmp(line2,TEXT(".in"),3)) scan_mfxFrameSurface1(line2+3,in);
        if (!_tcsncmp(line2,TEXT(".out"),4)) scan_mfxFrameSurface1(line2+4,out);
        scan_mfxStatus(line2,&so);
    });

    RECORD_RAW_BITS_IF_FLAG(VPP|DIR_INPUT|FORCE_NO_SYNC,as->vpp.frames,{
        dump_mfxFrameSurface1Raw(fd2, in, as->pAlloc);
    });

    AnalyzerSyncPoint *sp=new AnalyzerSyncPoint(VPP);
    if (!sp) return MFX_ERR_MEMORY_ALLOC;
    *syncp=(mfxSyncPoint)sp;
    sp->output.out=out;
    sp->output.aux=aux;
    sp->input.frame=in;

    LARGE_INTEGER ts1 = {0}, ts2 = {0};
    mfxStatus sts = MFX_ERR_NONE;

    for (int nTimeout = gc.vpp.handle_device_busy;
        !gc.vpp.handle_device_busy || nTimeout > 0;
        nTimeout-=5)
    {
        QueryPerformanceCounter(&ts1);
        sts=so.Override(MFX_CALL(MFXVideoVPP_RunFrameVPPAsync,(as->session, in, out, aux, &sp->sync_point)));
        QueryPerformanceCounter(&ts2);

        if (gc.vpp.handle_device_busy && sts == MFX_WRN_DEVICE_BUSY)
        {
            sm_vpp.CountInternal(sts);
            Sleep(5);
            continue;
        }
        break;
    }

    sm_vpp.Count(sts);

    RECORD_CONFIGURATION_PER_FRAME({
        dump_mfxFrameSurface1(fd, level, TEXT("vpp.RunFrameVPPAsync.in"),in);
        dump_mfxFrameSurface1(fd, level, TEXT("vpp.RunFrameVPPAsync.out"),out);
        dump_mfxStatus(fd, level, TEXT("vpp.RunFrameVPPAsync"),sts);
    });

    if (sts==MFX_ERR_NONE) {
        sp->start_tick=(mfxI64)ts1.QuadPart;
        as->vpp.frames++;
        as->vpp.async_exec_time+=(ts2.QuadPart-ts1.QuadPart);
        if (as->vpp.last_async_tick)
            as->vpp.async_exec_rate+=(ts1.QuadPart-as->vpp.last_async_tick);
        as->vpp.last_async_tick=ts1.QuadPart;
    }

    if (!sp->sync_point) {
        delete sp;
        *syncp=0;
    }

    if (gc.vpp.sync && *syncp && sts==MFX_ERR_NONE) /*status shouldn't be changed sts=*/MFXVideoCORE_SyncOperation(session,*syncp,gc.vpp.sync);

    return sts;
}

mfxStatus MFXVideoVPP_RunFrameVPPAsyncEx(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp) {
    if (gc.debug_view) OutputDebugString(TEXT("SDK: MFXVideoVPP_RunFrameVPPAsyncEx"));

    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;
    if (!as) return MFX_ERR_NULL_PTR;

    AnalyzerSyncPoint *sp=new AnalyzerSyncPoint(VPPEX);
    if (!sp) return MFX_ERR_MEMORY_ALLOC;
    *syncp=(mfxSyncPoint)sp;
    sp->output.ppOut=surface_out;
    sp->output.work=surface_work;
    sp->input.frame=in;

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoVPP_RunFrameVPPAsyncEx,(as->session, in, surface_work, surface_out, &sp->sync_point)));

    RECORD_CONFIGURATION_PER_FRAME({
        dump_mfxFrameSurface1(fd, level, TEXT("vpp.MFXVideoVPP_RunFrameVPPAsyncEx.in"),in);
        dump_mfxFrameSurface1(fd, level, TEXT("vpp.MFXVideoVPP_RunFrameVPPAsyncEx.surface_out"),*surface_out);
        dump_mfxStatus(fd, level, TEXT("vpp.MFXVideoVPP_RunFrameVPPAsyncEx"),sts);
    });

    if (!sp->sync_point) {
        delete sp;
        *syncp=0;
    }

    if (gc.vpp.sync && *syncp && sts==MFX_ERR_NONE) /*status shouldn't be changed sts=*/MFXVideoCORE_SyncOperation(session,*syncp,gc.vpp.sync);

    return sts;
}
