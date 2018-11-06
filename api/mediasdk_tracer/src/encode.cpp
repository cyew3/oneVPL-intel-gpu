/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2012 Intel Corporation. All Rights Reserved.

File Name: encode.cpp

\* ****************************************************************************** */
#include "configuration.h"
#include "msdka_import.h"
#include "msdka_serial.h"
#include "etw_event.h"

mfxStatus MFXVideoENCODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out) {
    ExtendedBufferOverride ebo1(in),ebo2(out);
    StatusOverride so;

    EDIT_CONFIGURATION(TEXT("encode.Query"),{
        if (in) if (!_tcsncmp(line2,TEXT(".in"),3)) scan_mfxVideoParam(line2+3,ENCODE,in,&ebo1);
        if (!_tcsncmp(line2,TEXT(".out"),4)) scan_mfxVideoParam(line2+4,ENCODE,out,&ebo2);
        scan_mfxStatus(line2,&so);
    });

    //in parameters might be changed if shared extended buffers used
    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("encode.Query.in"),ENCODE,in);
    })

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoENCODE_Query,(((AnalyzerSession *)session)->session,in,out)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("encode.Query.out"),ENCODE,out,NULL != in);
        dump_mfxStatus(fd, level, TEXT("encode.Query"),sts);
    });

    return sts;
}

mfxStatus MFXVideoENCODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request) {
    ExtendedBufferOverride ebo(par);
    StatusOverride so;

    //::MessageBoxA(0,"MFXVideoENCODE_QueryIOSurf", "msdk_analyzer", MB_OK);

    EDIT_CONFIGURATION(TEXT("encode.QueryIOSurf.par"),{
        scan_mfxVideoParam(line2,ENCODE,par,&ebo);
        scan_mfxStatus(line2,&so);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoENCODE_QueryIOSurf,(((AnalyzerSession *)session)->session, par, request)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("encode.QueryIOSurf.par"),ENCODE,par);
        dump_mfxFrameAllocRequest(fd, level, TEXT("encode.QueryIOSurf.request"),request);
        dump_mfxStatus(fd, level, TEXT("encode.QueryIOSurf"),sts);
    });

    return sts;
}

mfxStatus MFXVideoENCODE_Init(mfxSession session, mfxVideoParam *par) {
    StackIncreaser si;
    ExtendedBufferOverride ebo(par);
    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;

    EDIT_CONFIGURATION(TEXT("encode.Init"),{
        if (!_tcsncmp(line2,TEXT(".par"),4)) scan_mfxVideoParam(line2+4, ENCODE, par,&ebo);
        scan_mfxStatus(line2, &so);
    });

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("encode.Init.par"),ENCODE,par);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoENCODE_Init,(as->session, par)));

    /* initialize performance counters */
    as->encode.async_exec_time=0;
    as->encode.async_exec_rate=0;
    as->encode.frames=0;
    as->encode.frames_sync=0;
    as->encode.last_async_tick=0;
    as->encode.exec_time=0;

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("encode.Init"),sts);
    });

    //number of allocated surfaces
    AnalyzerFrameAllocator *pAfa = (AnalyzerFrameAllocator *)as->pAlloc ;

    if (pAfa)
    {
        mfxFrameAllocRequest  request;
        mfxFrameAllocResponse resp;
        mfxIMPL               impl;

        mfxStatus sts2 = MFX_CALL(MFXVideoENCODE_QueryIOSurf,(as->session, par, &request));

        sts2 = MFX_CALL(MFXQueryIMPL,(as->session, &impl));

        if (impl != MFX_IMPL_SOFTWARE)
        {
            request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
        }
        else
        {
            request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY;
        }

        request.Info = par->mfx.FrameInfo;

        sts2 = as->pAlloc->Alloc(as->pAlloc->pthis, &request, &resp);

        RECORD_CONFIGURATION({
            dump_format_wprefix(fd, level, 0, TEXT("encode.NumExternalFrames=%d"), resp.NumFrameActual);
        });
        pAfa->encode.nSurfacesAllocated = resp.NumFrameActual;
    }

    return sts;
}

mfxStatus MFXVideoENCODE_Reset(mfxSession session, mfxVideoParam *par) {
    ExtendedBufferOverride ebo(par);
    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;

    EDIT_CONFIGURATION(TEXT("encode.Reset.par"),{
        scan_mfxVideoParam(line2, ENCODE, par, &ebo);
        scan_mfxStatus(line2,&so);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoENCODE_Reset,(as->session, par)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("encode.Reset.par"),ENCODE,par);
        dump_mfxStatus(fd, level, TEXT("encode.Reset"),sts);
    });
    return sts;
}

mfxStatus MFXVideoENCODE_Close(mfxSession session) {
    AnalyzerSession *as=(AnalyzerSession *)session;
    mfxStatus sts=MFX_CALL(MFXVideoENCODE_Close,(as->session));

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("encode.Close"),sts);
    });

    /* record performance data */
    RECORD_CONFIGURATION2(as->encode.frames, MSDKA_LEVEL_GLOBAL, {
        LARGE_INTEGER cr;
        QueryPerformanceFrequency(&cr);

        /* encoding performance */
        dump_format_wprefix(fd, level, 1,TEXT("encode.perf.frames="),TEXT("%I64u"),as->encode.frames);
        dump_format_wprefix(fd, level, 1,TEXT("encode.perf.async_exec_time="),TEXT("%g (s/f)"),(double)as->encode.async_exec_time/(double)cr.QuadPart/(double)as->encode.frames);
        dump_format_wprefix(fd, level, 1,TEXT("encode.perf.async_exec_rate="),TEXT("%g (s/f)"),(double)as->encode.async_exec_rate/(double)cr.QuadPart/(double)as->encode.frames);
        if (as->encode.frames_sync) {
            dump_format_wprefix(fd, level, 1,TEXT("encode.perf.frames_sync="),TEXT("%I64u"),as->encode.frames_sync);
            dump_format_wprefix(fd, level, 1,TEXT("encode.perf.exec_time="),TEXT("%g (s/f)"),(double)as->encode.exec_time/(double)as->encode.frames_sync/(double)cr.QuadPart);
        }
    });

    return sts;
}

mfxStatus MFXVideoENCODE_GetVideoParam(mfxSession session, mfxVideoParam *par) {
    /* temporarily work around. Submit a bug to GetVideoParam */
    ExtendedBufferOverride ebo(par);
    StatusOverride so;

    EDIT_CONFIGURATION(TEXT("encode.Init.par"),{
        scan_mfxVideoParam(line2, ENCODE, par,&ebo);
        scan_mfxStatus(line2,&so);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoENCODE_GetVideoParam,(((AnalyzerSession *)session)->session,par)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("encode.GetVideoParam.par"),ENCODE,par);
        dump_mfxStatus(fd, level, TEXT("encode.GetVideoParam"),sts);
    });

    return sts;
}

mfxStatus MFXVideoENCODE_GetEncodeStat(mfxSession session, mfxEncodeStat *stat) {
    AnalyzerSession *as=(AnalyzerSession *)session;
    mfxStatus sts=MFX_CALL(MFXVideoENCODE_GetEncodeStat,(as->session,stat));

    RECORD_CONFIGURATION({
        dump_mfxEncodeStat(fd, level, TEXT("encode.GetEncodeStat.stat"),stat);
    });

    return sts;
}

StatusMap sm_encode;

mfxStatus MFXVideoENCODE_EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp) {
    MPA_TRACE_FUNCTION();

    if (gc.debug_view) OutputDebugString(TEXT("SDK: EncodeFrameAsync"));
    

    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;
    gc.RegisterSurface(ENCODE,RunAsync_Input,surface);

    AnalyzerFrameAllocator * pAlloc = (AnalyzerFrameAllocator*)as->pAlloc ;
    if (pAlloc)
    {
        RECORD_CONFIGURATION_PER_FRAME({
            pAlloc->decode.RegisterSurface(surface);
            //dump_format_wprefix(fd, level, 0, TEXT("decode.surfaces:Locked/Used/Allocated=%d/%d/%d"), pAlloc->decode.GetNumLockedSurfaces(), pAlloc->decode.surfaces.size(), pAlloc->decode.nSurfacesAllocated);
            dump_format_wprefix(fd, level, 0, TEXT("encode.surfaces:Locked/Used/Allocated=%d/%d/%d"), pAlloc->encode.GetNumLockedSurfaces(), pAlloc->encode.surfaces.size(), pAlloc->encode.nSurfacesAllocated);
        });
    }


    EDIT_CONFIGURATION_PER_FRAME(TEXT("encode.EncodeFrameAsync"),{
        if (!_tcsncmp(line2,TEXT(".surface"),8)) scan_mfxFrameSurface1(line2+8,surface);
        scan_mfxStatus(line2,&so);
    });

    RECORD_CONFIGURATION_PER_FRAME({
        dump_mfxEncodeCtrl(fd, level, TEXT("encode.EncodeFrameAsync.ctrl"),ctrl);
        dump_mfxFrameSurface1(fd, level, TEXT("encode.EncodeFrameAsync.surface"),surface);
        dump_mfxBitstream(fd, level, TEXT("encode.EncodeFrameAsync.bs(in)"),bs);
    });

    RECORD_RAW_BITS_IF_FLAG(ENCODE|DIR_INPUT|FORCE_NO_SYNC,as->encode.frames,{
        dump_mfxFrameSurface1Raw(fd2, surface, as->pAlloc );
    });

    /* replace syncpoint for performance measurement */
    AnalyzerSyncPoint *sp = new AnalyzerSyncPoint(ENCODE);
    if (!sp) return MFX_ERR_MEMORY_ALLOC;
    *syncp=(mfxSyncPoint)sp;
    sp->output.bitstream=bs;
    sp->input.frame=surface;

    RECORD_CONFIGURATION_PER_FRAME({
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("encode.EncodeFrameAsync.syncp(in)=0x"),TEXT("%p"), sp->sync_point));
    });

    LARGE_INTEGER ts1 = {0}, ts2 = {0};
    mfxStatus sts = MFX_ERR_NONE;

    for (int nTimeout = gc.encode.handle_device_busy;
        !gc.encode.handle_device_busy || nTimeout > 0;
        nTimeout-=5)
    {
        QueryPerformanceCounter(&ts1);
        if (!gc.encode.stub_mode || NULL == as->encode.first_frame.Data){
            sts=so.Override(MFX_CALL(MFXVideoENCODE_EncodeFrameAsync,(as->session, ctrl, surface, bs, &sp->sync_point)));
        } else {
            //non buffering mode
            if (NULL != surface) {
                // do nothing since syncpoint will be ready automatically
                sp->copy_data_from_stub = true;
                RECORD_CONFIGURATION_PER_FRAME({
                    dump_format_wprefix(fd, level, 1,TEXT("encode.EncodeFrameAsync:::NO MEDIASDK CALL, STUB BITSTREAM WILL BE COPIED"),TEXT(""));
                });
                //stub syncpoint generator
                sp->sync_point = (mfxSyncPoint)0x1;
            } else {
                sts = MFX_ERR_MORE_DATA;
            }
        }
        QueryPerformanceCounter(&ts2);

        if (gc.encode.handle_device_busy && sts == MFX_WRN_DEVICE_BUSY)
        {
            sm_encode.CountInternal(sts);
            Sleep(5);
            continue;
        }
        break;
    }

    sm_encode.Count(sts);

    RECORD_CONFIGURATION_PER_FRAME({
        dump_mfxFrameSurface1(fd, level, TEXT("encode.EncodeFrameAsync.surface(out)"),surface);
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("encode.EncodeFrameAsync.syncp(out)=0x"),TEXT("%p"), sp->sync_point));
        if (sts!=MFX_ERR_NONE) dump_mfxBitstream(fd, level, TEXT("encode.EncodeFrameAsync.bs(out)"),bs);
        dump_mfxStatus(fd, level, TEXT("encode.EncodeFrameAsync"),sts);
    });

    if (sts==MFX_ERR_NONE) {
        sp->start_tick=(mfxI64)ts1.QuadPart;
        as->encode.frames++;
        as->encode.async_exec_time+=(ts2.QuadPart-ts1.QuadPart);
        if (as->encode.last_async_tick)
            as->encode.async_exec_rate+=(ts1.QuadPart-as->encode.last_async_tick);
        as->encode.last_async_tick=ts1.QuadPart;
    }

    if (!sp->sync_point) {
        delete sp;
        *syncp=0;
    }

    if (gc.encode.sync && *syncp && sts==MFX_ERR_NONE)
        /*status shouldn't be changed sts=*/MFXVideoCORE_SyncOperation(session,*syncp,gc.encode.sync);
    return sts;
}