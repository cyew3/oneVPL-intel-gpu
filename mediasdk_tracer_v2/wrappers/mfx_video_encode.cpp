#include <exception>
#include <iostream>

#include "../loggers/timer.h"
#include "../tracer/functions_table.h"
#include "mfx_structures.h"

// ENCODE interface functions
mfxStatus MFXVideoENCODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    try{
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoENCODE_Query(mfxSession session=" + ToString(session) + ", mfxVideoParam *in=" + ToString(in) + ", mfxVideoParam *out=" + ToString(out) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Query_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if(in) Log::WriteLog(context.dump/*_mfxVideoParam*/("in", *in));
        if(out) Log::WriteLog(context.dump("out", *out));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_Query) proc) (session, in, out);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_Query called");
        Log::WriteLog(context.dump("session", session));
        if(in) Log::WriteLog(context.dump/*_mfxVideoParam*/("in", *in));
        if(out) Log::WriteLog(context.dump("out", *out));
        Log::WriteLog("function: MFXVideoENCODE_Query(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    try{
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoENCODE_QueryIOSurf(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ", mfxFrameAllocRequest *request=" + ToString(request) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_QueryIOSurf_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if(par) Log::WriteLog(context.dump("par", *par));
        if(request) Log::WriteLog(context.dump("request", *request));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_QueryIOSurf) proc) (session, par, request);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_QueryIOSurf called");
        Log::WriteLog(context.dump("session", session));
        if(par) Log::WriteLog(context.dump("par", *par));
        if(request) Log::WriteLog(context.dump("request", *request));
        Log::WriteLog("function: MFXVideoENCODE_QueryIOSurf(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_Init(mfxSession session, mfxVideoParam *par)
{
    try{
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoENCODE_Init(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Init_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if(par) Log::WriteLog(context.dump("par", *par));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_Init) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_Init called");
        Log::WriteLog(context.dump("session", session));
        if(par) Log::WriteLog(context.dump("par", *par));
        Log::WriteLog("function: MFXVideoENCODE_Init(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_Reset(mfxSession session, mfxVideoParam *par)
{
    try{
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoENCODE_Reset(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");

        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Reset_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if(par) Log::WriteLog(context.dump("par", *par));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_Reset) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_Reset called");
        Log::WriteLog(context.dump("session", session));
        if(par) Log::WriteLog(context.dump("par", *par));
        Log::WriteLog("function: MFXVideoENCODE_Reset(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_Close(mfxSession session)
{
    try{
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoENCODE_Close(mfxSession session=" + ToString(session) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Close_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_Close) proc) (session);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_Close called");
        Log::WriteLog(context.dump("session", session));
        Log::WriteLog("function: MFXVideoENCODE_Close(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    try{
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoENCODE_GetVideoParam(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_GetVideoParam_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if(par) Log::WriteLog(context.dump("par", *par));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_GetVideoParam) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_GetVideoParam called");
        Log::WriteLog(context.dump("session", session));
        if(par) Log::WriteLog(context.dump("par", *par));
        Log::WriteLog("function: MFXVideoENCODE_GetVideoParam(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_GetEncodeStat(mfxSession session, mfxEncodeStat *stat)
{
    try{
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoENCODE_GetEncodeStat(mfxSession session=" + ToString(session) + ", mfxEncodeStat *stat=" + ToString(stat) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_GetEncodeStat_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if(stat) Log::WriteLog(context.dump("stat", *stat));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_GetEncodeStat) proc) (session, stat);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_GetEncodeStat called");
        Log::WriteLog(context.dump("session", session));
        if(stat) Log::WriteLog(context.dump("stat", *stat));
        Log::WriteLog("function: MFXVideoENCODE_GetEncodeStat(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
{
    try{
        if (Log::GetLogLevel() >= LOG_LEVEL_FULL)
        {
            DumpContext context;
            context.context = DUMPCONTEXT_MFX;
            TracerSyncPoint * sp = new TracerSyncPoint();
            sp->syncPoint = (*syncp);
            sp->component = ENCODE;
            Log::WriteLog("function: MFXVideoENCODE_EncodeFrameAsync(mfxSession session=" + ToString(session) + ", mfxEncodeCtrl *ctrl=" + ToString(ctrl) + ", mfxFrameSurface1 *surface=" + ToString(surface) + ", mfxBitstream *bs=" + ToString(bs) + ", mfxSyncPoint *syncp=" + ToString(syncp) + ") +");
            mfxLoader *loader = (mfxLoader*) session;

            if (!loader) return MFX_ERR_INVALID_HANDLE;

            mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_EncodeFrameAsync_tracer];
            if (!proc) return MFX_ERR_INVALID_HANDLE;

            session = loader->session;
            Log::WriteLog(context.dump("session", session));
            if(ctrl) Log::WriteLog(context.dump("ctrl", *ctrl));
            if(surface) Log::WriteLog(context.dump("surface", *surface));
            if(bs) Log::WriteLog(context.dump("bs", *bs));
            if(syncp) Log::WriteLog(context.dump("syncp", *syncp));
        
            sp->timer.Restart();
            Timer t;
            mfxStatus status = (*(fMFXVideoENCODE_EncodeFrameAsync) proc) (session, ctrl, surface, bs, &sp->syncPoint);
            std::string elapsed = TimeToString(t.GetTime());

            *syncp = (mfxSyncPoint)sp;
            if (!sp->syncPoint) {
                delete sp;
                *syncp=NULL;
            }
        
            Log::WriteLog(">> MFXVideoENCODE_EncodeFrameAsync called");
            Log::WriteLog(context.dump("session", session));
            if(ctrl) Log::WriteLog(context.dump("ctrl", *ctrl));
            if(surface) Log::WriteLog(context.dump("surface", *surface));
            if(bs) Log::WriteLog(context.dump("bs", *bs));
            Log::WriteLog(context.dump("syncp", sp->syncPoint));
            Log::WriteLog("function: MFXVideoENCODE_EncodeFrameAsync(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
            
            return status;
        }
        else // call without loging
        {
            DumpContext context;
            context.context = DUMPCONTEXT_MFX;
            TracerSyncPoint * sp = new TracerSyncPoint();
            sp->syncPoint = (*syncp);
            sp->component = ENCODE;
            mfxLoader *loader = (mfxLoader*) session;

            if (!loader) return MFX_ERR_INVALID_HANDLE;

            mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_EncodeFrameAsync_tracer];
            if (!proc) return MFX_ERR_INVALID_HANDLE;

            session = loader->session;
            
            mfxStatus status = (*(fMFXVideoENCODE_EncodeFrameAsync) proc) (session, ctrl, surface, bs, &sp->syncPoint);
            
            *syncp = (mfxSyncPoint)sp;
            if (!sp->syncPoint) {
                delete sp;
                *syncp=NULL;
            }
                    
            return status;
        }
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}
