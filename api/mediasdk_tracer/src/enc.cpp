/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: enc.cpp

\* ****************************************************************************** */
#include "configuration.h"
#include "msdka_import.h"
#include "msdka_serial.h"

mfxStatus MFXVideoENC_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out) {
    ExtendedBufferOverride ebo1(in),ebo2(out);
    StatusOverride so;

    EDIT_CONFIGURATION(TEXT("enc.Query"),{
        if (in) if (!_tcsncmp(line2,TEXT(".in"),3)) scan_mfxVideoParam(line2+3,ENC,in,&ebo1);
        if (!_tcsncmp(line2,TEXT(".out"),4)) scan_mfxVideoParam(line2+4,ENC,out,&ebo2);
        scan_mfxStatus(line2,&so);
    });

    //in parameters might be changed if shared extended buffers used
    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("enc.Query.in"),ENC,in);
    })

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoENC_Query,(((AnalyzerSession *)session)->session,in,out)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("enc.Query.out"),ENC,out,NULL != in);
        dump_mfxStatus(fd, level, TEXT("enc.Query"),sts);
    });

    return sts;
}

mfxStatus MFXVideoENC_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request) {
    ExtendedBufferOverride ebo(par);
    StatusOverride so;

    EDIT_CONFIGURATION(TEXT("enc.QueryIOSurf.par"),{
        scan_mfxVideoParam(line2,ENC,par,&ebo);
        scan_mfxStatus(line2,&so);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoENC_QueryIOSurf,(((AnalyzerSession *)session)->session, par, request)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("enc.QueryIOSurf.par"),ENC,par);
        dump_mfxFrameAllocRequest(fd, level, TEXT("enc.QueryIOSurf.request"),request);
        dump_mfxStatus(fd, level, TEXT("enc.QueryIOSurf"),sts);
    });

    return sts;
}

mfxStatus MFXVideoENC_Init(mfxSession session, mfxVideoParam *par) {
    StackIncreaser si;
    ExtendedBufferOverride ebo(par);
    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;

    EDIT_CONFIGURATION(TEXT("enc.Init"),{
        if (!_tcsncmp(line2,TEXT(".par"),4)) scan_mfxVideoParam(line2+4, ENC, par,&ebo);
        scan_mfxStatus(line2, &so);
    });

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("enc.Init.par"),ENC,par);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoENC_Init,(as->session, par)));

    /* initialize performance counters */
    as->enc.async_exec_time=0;
    as->enc.async_exec_rate=0;
    as->enc.frames=0;
    as->enc.frames_sync=0;
    as->enc.last_async_tick=0;
    as->enc.exec_time=0;

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("enc.Init"),sts);
    });

    //number of allocated surfaces
    AnalyzerFrameAllocator *pAfa = (AnalyzerFrameAllocator *)as->pAlloc ;

    if (pAfa)
    {
        mfxFrameAllocRequest  request;
        mfxFrameAllocResponse resp;
        mfxIMPL               impl;

        mfxStatus sts2 = MFX_CALL(MFXVideoENC_QueryIOSurf,(as->session, par, &request));

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
            dump_format_wprefix(fd, level, 0, TEXT("enc.NumExternalFrames=%d"), resp.NumFrameActual);
        });
        pAfa->enc.nSurfacesAllocated = resp.NumFrameActual;
    }

    return sts;
}

mfxStatus MFXVideoENC_Reset(mfxSession session, mfxVideoParam *par) {
    ExtendedBufferOverride ebo(par);
    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;

    EDIT_CONFIGURATION(TEXT("enc.Reset.par"),{
        scan_mfxVideoParam(line2, ENC, par, &ebo);
        scan_mfxStatus(line2,&so);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoENC_Reset,(as->session, par)));

    RECORD_CONFIGURATION({
        dump_mfxVideoParam(fd, level, TEXT("enc.Reset.par"),ENC,par);
        dump_mfxStatus(fd, level, TEXT("enc.Reset"),sts);
    });
    return sts;
}

mfxStatus MFXVideoENC_Close(mfxSession session) {
    AnalyzerSession *as=(AnalyzerSession *)session;
    mfxStatus sts=MFX_CALL(MFXVideoENC_Close,(as->session));

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("enc.Close"),sts);
    });

    /* record performance data */
    RECORD_CONFIGURATION2(as->enc.frames, MSDKA_LEVEL_GLOBAL, {
        LARGE_INTEGER cr;
        QueryPerformanceFrequency(&cr);

        /* encoding performance */
        dump_format_wprefix(fd, level, 1,TEXT("enc.perf.frames="),TEXT("%I64u"),as->enc.frames);
        dump_format_wprefix(fd, level, 1,TEXT("enc.perf.async_exec_time="),TEXT("%g (s/f)"),(double)as->enc.async_exec_time/(double)cr.QuadPart/(double)as->enc.frames);
        dump_format_wprefix(fd, level, 1,TEXT("enc.perf.async_exec_rate="),TEXT("%g (s/f)"),(double)as->enc.async_exec_rate/(double)cr.QuadPart/(double)as->enc.frames);
        if (as->enc.frames_sync) {
            dump_format_wprefix(fd, level, 1,TEXT("enc.perf.frames_sync="),TEXT("%I64u"),as->enc.frames_sync);
            dump_format_wprefix(fd, level, 1,TEXT("enc.perf.exec_time="),TEXT("%g (s/f)"),(double)as->enc.exec_time/(double)as->enc.frames_sync/(double)cr.QuadPart);
        }
    });

    return sts;
}

mfxStatus MFXVideoENC_ProcessFrameAsync(mfxSession session, mfxENCInput *in, mfxENCOutput *out, mfxSyncPoint *syncp) {
    StatusOverride so;
    AnalyzerSession *as=(AnalyzerSession *)session;

    RECORD_CONFIGURATION_PER_FRAME({
        dump_mfxENCInput(fd, level, TEXT("enc.ProcessFrameAsync.in"),in);
        dump_mfxENCOutput(fd, level, TEXT("enc.ProcessFrameAsync.out"),out);
    });

    mfxStatus sts=so.Override(MFX_CALL(MFXVideoENC_ProcessFrameAsync,(as->session, in, out, syncp)));
    return sts;
}
