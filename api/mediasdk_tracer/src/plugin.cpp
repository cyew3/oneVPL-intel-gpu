/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2011 Intel Corporation. All Rights Reserved.

File Name: plugin.cpp

\* ****************************************************************************** */
#include "configuration.h"
#include "msdka_import.h"
#include "msdka_serial.h"

mfxStatus MFXVideoUSER_Register(mfxSession session, mfxU32 type, const mfxPlugin *par) {
    AnalyzerSession *as=(AnalyzerSession *)session;
    mfxStatus sts=MFX_CALL(MFXVideoUSER_Register,(as->session, type, par));

    RECORD_CONFIGURATION_PER_FRAME({
        dump_format_wprefix(fd, level, 1,TEXT("MFXVideoUSER_Register.type="),TEXT("%d"),type);
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("MFXVideoUSER_Register.par=0x"),TEXT("%p"),par));
        dump_mfxStatus(fd, level, TEXT("MFXVideoUSER_Register"),sts);
    });

    return sts;
}

mfxStatus MFXVideoUSER_Unregister(mfxSession session, mfxU32 type) {
    AnalyzerSession *as=(AnalyzerSession *)session;
    mfxStatus sts=MFX_CALL(MFXVideoUSER_Unregister,(as->session, type));

    RECORD_CONFIGURATION_PER_FRAME({
        dump_format_wprefix(fd, level, 1,TEXT("MFXVideoUSER_Unregister.type="),TEXT("%d"),type);
        dump_mfxStatus(fd, level, TEXT("MFXVideoUSER_Unregister"),sts);
    });

    return sts;
}

mfxStatus MFXVideoUSER_ProcessFrameAsync(mfxSession session, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxSyncPoint *syncp) {
    AnalyzerSession *as=(AnalyzerSession *)session;
    mfxStatus sts=MFX_CALL(MFXVideoUSER_ProcessFrameAsync,(as->session, in, in_num, out, out_num, syncp));

    RECORD_CONFIGURATION_PER_FRAME({
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("MFXVideoUSER_ProcessFrameAsync.in=0x"),TEXT("%p"),in));
        dump_format_wprefix(fd, level, 1,TEXT("MFXVideoUSER_ProcessFrameAsync.in_num="),TEXT("%d"),in_num);
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("MFXVideoUSER_ProcessFrameAsync.out=0x"),TEXT("%p"),out));
        dump_format_wprefix(fd, level, 1,TEXT("MFXVideoUSER_ProcessFrameAsync.out_num="),TEXT("%d"),out_num);
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("MFXVideoUSER_ProcessFrameAsync.syncp=0x"),TEXT("%p"),*syncp));
        dump_mfxStatus(fd, level, TEXT("MFXVideoUSER_ProcessFrameAsync"),sts);
    });

    return sts;
}

