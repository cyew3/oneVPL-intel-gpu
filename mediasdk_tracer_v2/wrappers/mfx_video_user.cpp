/* ****************************************************************************** *\

Copyright (C) 2012-2015 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_video_user.cpp

\* ****************************************************************************** */

#include <exception>
#include <iostream>

#include "../loggers/timer.h"
#include "../tracer/functions_table.h"
#include "mfx_structures.h"

mfxStatus MFXVideoUSER_Register(mfxSession session, mfxU32 type, const mfxPlugin *par)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoUSER_Register(mfxSession session=" + ToString(session) + ", mfxU32 type=" + ToString(type) + ", mfxPlugin *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoUSER_Register_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        Log::WriteLog(context.dump_mfxU32("type", type));
        if (par) Log::WriteLog(context.dump("par", *par));

        Timer t;
        mfxStatus status = (*(fMFXVideoUSER_Register) proc)(session, type, par);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoUSER_Register called");

        Log::WriteLog(context.dump("session", session));
        Log::WriteLog(context.dump_mfxU32("type", type));
        if (par) Log::WriteLog(context.dump("par", *par));

        Log::WriteLog("function: MFXVideoUSER_Register(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoUSER_Unregister(mfxSession session, mfxU32 type)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoUSER_Unregister(mfxSession session=" + ToString(session) + ", mfxU32 type=" + ToString(type) + ", mfxPlugin *par=" + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoUSER_Unregister_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        Log::WriteLog(context.dump_mfxU32("type", type));

        Timer t;
        mfxStatus status = (*(fMFXVideoUSER_Unregister) proc) (session, type);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoUSER_Unregister called");

        Log::WriteLog(context.dump("session", session));
        Log::WriteLog(context.dump_mfxU32("type", type));

        Log::WriteLog("function: MFXVideoUSER_Unregister(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoUSER_ProcessFrameAsync(mfxSession session, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxSyncPoint *syncp)
{
    try {
        if (Log::GetLogLevel() >= LOG_LEVEL_FULL) // call with logging
        {
            DumpContext context;
            context.context = DUMPCONTEXT_MFX;
            TracerSyncPoint sp;
            if (syncp) {
                sp.syncPoint = (*syncp);
            }
            else {
                sp.syncPoint = NULL;
            }

            Log::WriteLog("function: MFXVideoUSER_ProcessFrameAsync(mfxSession session=, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxSyncPoint *syncp) +");
            mfxLoader *loader = (mfxLoader*) session;

            if (!loader) return MFX_ERR_INVALID_HANDLE;

            mfxFunctionPointer proc = loader->table[eMFXVideoUSER_ProcessFrameAsync_tracer];
            if (!proc) return MFX_ERR_INVALID_HANDLE;

            session = loader->session;
            Log::WriteLog(context.dump("session", session));
            Log::WriteLog(context.dump_mfxHDL("in", in));
            Log::WriteLog(context.dump_mfxU32("in_num", in_num));
            Log::WriteLog(context.dump_mfxHDL("out", out));
            Log::WriteLog(context.dump_mfxU32("out_num", out_num));
            Log::WriteLog(context.dump("syncp", sp.syncPoint));

            Timer t;
            mfxStatus status = (*(fMFXVideoUSER_ProcessFrameAsync) proc) (session, in, in_num, out, out_num, syncp);
            std::string elapsed = TimeToString(t.GetTime());
            if (syncp) {
                sp.syncPoint = (*syncp);
            }
            else {
                sp.syncPoint = NULL;
            }
            Log::WriteLog(">> MFXVideoUSER_ProcessFrameAsync called");

            Log::WriteLog(context.dump("session", session));
            Log::WriteLog(context.dump_mfxHDL("in", in));
            Log::WriteLog(context.dump_mfxU32("in_num", in_num));
            Log::WriteLog(context.dump_mfxHDL("out", out));
            Log::WriteLog(context.dump_mfxU32("out_num", out_num));
            Log::WriteLog(context.dump("syncp", sp.syncPoint));

            Log::WriteLog("function: MFXVideoUSER_ProcessFrameAsync(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");

            return status;
        }
        else // call without logging
        {
            DumpContext context;
            context.context = DUMPCONTEXT_MFX;

            mfxLoader *loader = (mfxLoader*) session;

            if (!loader) return MFX_ERR_INVALID_HANDLE;

            mfxFunctionPointer proc = loader->table[eMFXVideoUSER_ProcessFrameAsync_tracer];
            if (!proc) return MFX_ERR_INVALID_HANDLE;

            session = loader->session;
            
            mfxStatus status = (*(fMFXVideoUSER_ProcessFrameAsync) proc) (session, in, in_num, out, out_num, syncp);

            return status;
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoUSER_GetPlugin(mfxSession session, mfxU32 type, mfxPlugin *par)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoUSER_GetPlugin(mfxSession session=" + ToString(session) + ", mfxU32 type=" + ToString(type) + ", mfxPlugin *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoUSER_GetPlugin_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        Log::WriteLog(context.dump_mfxU32("type", type));
        if (par) Log::WriteLog(context.dump("par", *par));

        Timer t;
        mfxStatus status = (*(fMFXVideoUSER_GetPlugin) proc)(session, type, par);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoUSER_GetPlugin called");

        Log::WriteLog(context.dump("session", session));
        Log::WriteLog(context.dump_mfxU32("type", type));
        if (par) Log::WriteLog(context.dump("par", *par));

        Log::WriteLog("function: MFXVideoUSER_GetPlugin(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus  MFXAudioUSER_Register(mfxSession session, mfxU32 type, const mfxPlugin *par) { return MFX_ERR_NONE;}
mfxStatus  MFXAudioUSER_Unregister(mfxSession session, mfxU32 type) {return MFX_ERR_NONE;}
