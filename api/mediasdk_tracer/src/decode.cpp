/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2013 Intel Corporation. All Rights Reserved.

File Name: decode.cpp

\* ****************************************************************************** */
#include "configuration.h"
#include "msdka_import.h"
#include "msdka_serial.h"

mfxStatus MFXVideoDECODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out) {
    ExtendedBufferOverride ebo1(in), ebo2(out);
    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;

    EDIT_CONFIGURATION(TEXT("decode.Query"),{
        if (in) if (!_tcsncmp(line2,TEXT(".in"),3)) scan_mfxVideoParam(line2+3,DECODE,in,&ebo1);
        if (!_tcsncmp(line2,TEXT(".out"),4)) scan_mfxVideoParam(line2+4,DECODE,out,&ebo2);
        scan_mfxStatus(line2,&so);
    });

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("decode.Query.in"),DECODE,in);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoDECODE_Query,(as->session,in,out)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("decode.Query.out"),DECODE,out, NULL != in);
        dump_mfxStatus(fd, level, TEXT("decode.Query"),sts);
    });

    return sts;
}

mfxStatus MFXVideoDECODE_DecodeHeader(mfxSession session, mfxBitstream *bs, mfxVideoParam *par) {
    ExtendedBufferOverride ebo(par);
    StatusOverride so;

    EDIT_CONFIGURATION(TEXT("decode.DecodeHeader"),{
        scan_mfxStatus(line2,&so);
    });

    RECORD_CONFIGURATION({
        dump_mfxBitstream(fd, level, TEXT("decode.DecodeHeader.bs(in)"), bs);
    });
    
    RECORD_RAW_BITS_UNCONSTRAINED(DECODE|DIR_HEADER, {
        dump_mfxBitstreamRaw(fd2, bs);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoDECODE_DecodeHeader,(((AnalyzerSession *)session)->session, bs, par)));

    EDIT_CONFIGURATION(TEXT("decode.DecodeHeader"),{
        if (!_tcsncmp(line2,TEXT(".par"),4)) scan_mfxVideoParam(line2+4,DECODE,par,&ebo);
    });

    RECORD_CONFIGURATION({
        dump_mfxBitstream(fd, level, TEXT("decode.DecodeHeader.bs(out)"),bs);
        dump_mfxVideoParam(fd, level, TEXT("decode.DecodeHeader.par"),DECODE,par);
        dump_mfxStatus(fd, level, TEXT("decode.DecodeHeader"),sts);
    });

    return sts;
}

mfxStatus MFXVideoDECODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request) {
    ExtendedBufferOverride ebo(par);
    StatusOverride so;

    EDIT_CONFIGURATION(TEXT("decode.QueryIOSurf.par"),{
        scan_mfxVideoParam(line2,DECODE,par,&ebo);
        scan_mfxStatus(line2,&so);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoDECODE_QueryIOSurf,(((AnalyzerSession *)session)->session, par, request)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("decode.QueryIOSurf.par"),DECODE,par);
        dump_mfxFrameAllocRequest(fd, level, TEXT("decode.QueryIOSurf.request"),request);
        dump_mfxStatus(fd, level, TEXT("decode.QueryIOSurf"),sts);
    });

    return sts;
}

mfxStatus MFXVideoDECODE_Init(mfxSession session, mfxVideoParam *par) {
    StackIncreaser si;
    ExtendedBufferOverride ebo(par);
    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;

    EDIT_CONFIGURATION(TEXT("decode.Init"),{
        if (!_tcsncmp(line2,TEXT(".par"),4)) scan_mfxVideoParam(line2+4, DECODE, par,&ebo);
        scan_mfxStatus(line2, &so);
    });

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("decode.Init.par"),DECODE,par);
    });


    mfxStatus sts=so.Override(MFX_CALL(MFXVideoDECODE_Init,(as->session, par)));

    /* initialize performance counters */
    as->decode.async_exec_time=0;
    as->decode.async_exec_rate=0;
    as->decode.frames=0;
    as->decode.frames_sync=0;
    as->decode.last_async_tick=0;
    as->decode.exec_time=0;
    //

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("decode.Init"),sts);
    });

    AnalyzerFrameAllocator *pAfa = (AnalyzerFrameAllocator *)as->pAlloc ;

    if (pAfa) {
        mfxFrameAllocRequest  request;
        mfxFrameAllocResponse resp;
        mfxIMPL               impl;

        mfxStatus sts2 = MFX_CALL(MFXVideoDECODE_QueryIOSurf,(as->session, par, &request));

        sts2 = MFX_CALL(MFXQueryIMPL,(as->session, &impl));


        //if (impl != MFX_IMPL_SOFTWARE)
        {
            request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET| MFX_MEMTYPE_SYSTEM_MEMORY;
        }

        request.Info = par->mfx.FrameInfo;

        sts2 = as->pAlloc->Alloc(as->pAlloc->pthis, &request, &resp);

        if (sts2 == MFX_ERR_NONE)
        {
            RECORD_CONFIGURATION({
                dump_format_wprefix(fd, level, 0, TEXT("decode.NumExternalFrames=%d"), resp.NumFrameActual);
            });
            pAfa->decode.nSurfacesAllocated = resp.NumFrameActual;
            sts2 = as->pAlloc->Free(as->pAlloc->pthis,&resp);
        }
    }

    return sts;
}

mfxStatus MFXVideoDECODE_Reset(mfxSession session, mfxVideoParam *par) {
    ExtendedBufferOverride ebo(par);
    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;

    EDIT_CONFIGURATION(TEXT("decode.Reset.par"),{
        scan_mfxVideoParam(line2, DECODE, par, &ebo);
        scan_mfxStatus(line2, &so);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoDECODE_Reset,(as->session, par)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("decode.Reset.par"),DECODE,par);
        dump_mfxStatus(fd, level, TEXT("decode.Reset"),sts);
    });

    return sts;
}

mfxStatus MFXVideoDECODE_Close(mfxSession session) {
    AnalyzerSession *as=(AnalyzerSession *)session;
    mfxStatus sts=MFX_CALL(MFXVideoDECODE_Close,(as->session));

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("decode.Close"),sts);
    });

    /* record performance data */
    RECORD_CONFIGURATION2(as->decode.frames, MSDKA_LEVEL_GLOBAL, {
        LARGE_INTEGER cr;
        QueryPerformanceFrequency(&cr);

        dump_format_wprefix(fd, level, 1,TEXT("decode.perf.frames="),TEXT("%I64u"),as->decode.frames);
        dump_format_wprefix(fd, level, 1,TEXT("decode.perf.async_exec_time="),TEXT("%g (s/f)"),(double)as->decode.async_exec_time/(double)cr.QuadPart/(double)as->decode.frames);
        dump_format_wprefix(fd, level, 1,TEXT("decode.perf.async_exec_rate="),TEXT("%g (s/f)"),(double)as->decode.async_exec_rate/(double)cr.QuadPart/(double)as->decode.frames);
        if (as->decode.frames_sync) {
            dump_format_wprefix(fd, level, 1,TEXT("decode.perf.frames_sync="),TEXT("%I64u"),as->decode.frames_sync);
            dump_format_wprefix(fd, level, 1,TEXT("decode.perf.exec_time="),TEXT("%g (s/f)"),(double)as->decode.exec_time/(double)as->decode.frames_sync/(double)cr.QuadPart);
        }
    });

    return sts;
}

mfxStatus MFXVideoDECODE_GetVideoParam(mfxSession session, mfxVideoParam *par) {
    /* temporarily work around. Submit a bug to GetVideoParam */
    ExtendedBufferOverride ebo(par);
    StatusOverride so;

    EDIT_CONFIGURATION(TEXT("decode.Init.par"),{
        scan_mfxVideoParam(line2, DECODE, par,&ebo);
        scan_mfxStatus(line2, &so);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoDECODE_GetVideoParam,(((AnalyzerSession *)session)->session, par)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("decode.GetVideoParam.par"),DECODE,par);
        dump_mfxStatus(fd, level, TEXT("decode.GetVideoParam"),sts);
    });

    return sts;
}

mfxStatus MFXVideoDECODE_GetDecodeStat(mfxSession session, mfxDecodeStat *stat) {
    mfxStatus sts=MFX_CALL(MFXVideoDECODE_GetDecodeStat,(((AnalyzerSession *)session)->session, stat));

    RECORD_CONFIGURATION({
        dump_mfxDecodeStat(fd, level, TEXT("decode.GetDecodeStat.stat"),stat);
    });

    return sts;
}

mfxStatus MFXVideoDECODE_SetSkipMode(mfxSession session, mfxSkipMode mode) {
    mfxStatus sts=MFX_CALL(MFXVideoDECODE_SetSkipMode,(((AnalyzerSession *)session)->session,mode));

    RECORD_CONFIGURATION({
        dump_format_wprefix(fd, level, 1,TEXT("decode.SetSkipMode.mode="),TEXT("%d"),mode);
        dump_mfxStatus(fd, level, TEXT("decode.SetSkipMode"),sts);
    });

    return sts;
}

mfxStatus MFXVideoDECODE_GetPayload(mfxSession session, mfxU64 *ts, mfxPayload *payload) {
    mfxStatus sts=MFX_CALL(MFXVideoDECODE_GetPayload,(((AnalyzerSession *)session)->session, ts, payload));
    RECORD_CONFIGURATION({
        dump_format_wprefix(fd, level, 1,TEXT("decode.GetPayload.ts="),TEXT("%IA64u"),ts);
        dump_mfxStatus(fd, level, TEXT("decode.GetPayload"),sts);
    });
    return sts;
}

StatusMap sm_decode;

mfxStatus MFXVideoDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp) {
    if (gc.debug_view) OutputDebugString(TEXT("SDK: DecodeFrameAsync"));

    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;

    EDIT_CONFIGURATION_PER_FRAME(TEXT("decode.DecodeFrameAsync"),{
        if (!_tcsncmp(line2,TEXT(".surface_work"),3)) scan_mfxFrameSurface1(line2+3,surface_work);
        scan_mfxStatus(line2,&so);
    });  

    gc.RegisterSurface(DECODE, RunAsync_Output, surface_work);

    AnalyzerFrameAllocator * pAlloc = (AnalyzerFrameAllocator*)as->pAlloc ;
    if (pAlloc)
    {
        RECORD_CONFIGURATION_PER_FRAME({
            pAlloc->decode.RegisterSurface(surface_work);
            dump_format_wprefix(fd, level, 0, TEXT("decode.surfaces:Locked/Used/Allocated=%d/%d/%d"), pAlloc->decode.GetNumLockedSurfaces(), pAlloc->decode.surfaces.size(), pAlloc->decode.nSurfacesAllocated);
            //dump_format_wprefix(fd, level, 0, TEXT("encode.surfaces:Locked/Used/Allocated=%d/%d/%d"), pAlloc->encode.GetNumLockedSurfaces(), pAlloc->encode.surfaces.size(), pAlloc->encode.nSurfacesAllocated);
        });
    }

    RECORD_CONFIGURATION_PER_FRAME({
        dump_mfxBitstream(fd, level, TEXT("decode.DecodeFrameAsync.bs(in)"),bs);
        dump_mfxFrameSurface1(fd, level, TEXT("decode.DecodeFrameAsync.surface_work"),surface_work);
    });

    AnalyzerSyncPoint *sp=new AnalyzerSyncPoint(DECODE);
    if (!sp) return MFX_ERR_MEMORY_ALLOC;
    
    mfxBitstream bs_in ={0};
    if (NULL != bs)
        bs_in = *bs;
    
    *syncp=(mfxSyncPoint)sp;
    
    LARGE_INTEGER ts1 = {0}, ts2 = {0};
    mfxStatus sts = MFX_ERR_NONE;
    
    for (int nTimeout = gc.decode.handle_device_busy;
         !gc.decode.handle_device_busy || nTimeout > 0;
          nTimeout-=5)
    {
        QueryPerformanceCounter(&ts1);
        sts=so.Override(MFX_CALL(MFXVideoDECODE_DecodeFrameAsync,(as->session, bs, surface_work, surface_out, &sp->sync_point)));
        QueryPerformanceCounter(&ts2);
        
        
        if (gc.decode.handle_device_busy && sts == MFX_WRN_DEVICE_BUSY)
        {
            sm_decode.CountInternal(sts);
            Sleep(5);
            continue;
        }
        break;
    }
    if (surface_out) sp->output.psurface_out = *surface_out;
    
    sm_decode.Count(sts);
    
    bs_in.DataLength = (bs_in.DataLength - (NULL == bs ? 0 : bs->DataLength));

    RECORD_RAW_BITS(DECODE|DIR_INPUT,as->decode.frames,{
        dump_mfxBitstreamRaw(fd2,&bs_in);
    });

    RECORD_CONFIGURATION_PER_FRAME({
        dump_mfxBitstream(fd, level, TEXT("decode.DecodeFrameAsync.bs(out)"),bs);
        if ((NULL != surface_out) && (NULL != *surface_out)){
            dump_mfxFrameSurface1(fd, level, TEXT("decode.DecodeFrameAsync.surface_out"),*surface_out);
        }
        dump_mfxStatus(fd, level, TEXT("decode.DecodeFrameAsync"),sts);
    });

    if (sts==MFX_ERR_NONE) {
        sp->start_tick=(mfxI64)ts1.QuadPart;
        as->decode.frames++;
        as->decode.async_exec_time+=(ts2.QuadPart-ts1.QuadPart);
        if (as->decode.last_async_tick)
            as->decode.async_exec_rate+=(ts1.QuadPart-as->decode.last_async_tick);
        as->decode.last_async_tick=ts1.QuadPart;
    }

    if (!sp->sync_point) {
        delete sp;
        *syncp=0;
    }

    if (gc.decode.sync && *syncp && sts==MFX_ERR_NONE) 
        MFX_CALL(MFXVideoCORE_SyncOperation,(as->session, sp->sync_point, gc.decode.sync));
        ///*status shouldn't be changed sts=*/MFXVideoCORE_SyncOperation(session,*syncp,gc.sync_decode);
    return sts;
}
