#if defined(_WIN32) || defined(_WIN64)

#define MFX_DISPATCHER_LOG 1

#include <windows.h>
#include <string.h>
#include <iostream>
#include "tracer.h"
#include "mfx_dispatcher.h"
#include "mfxstructures.h"
#include "mfx_dispatcher_log.h"
#include "../tools/shared_mem_server.h"


//dump messages from dispatcher
class DispatcherLogRecorder : public IMsgHandler
{
    static DispatcherLogRecorder m_Instance;
public:
    static DispatcherLogRecorder& get(){return m_Instance;}
    virtual void Write(int level, int opcode, const char * msg, va_list argptr);
};


class msdk_analyzer_sink : public IMsgHandler
{
    
public:
    msdk_analyzer_sink()
    {
        DispatchLog::get().AttachSink(DL_SINK_IMsgHandler, this);
        DispatchLog::get().DetachSink(DL_SINK_PRINTF, NULL);
    }
    ~msdk_analyzer_sink()
    {
        DispatchLog::get().DetachSink(DL_SINK_IMsgHandler, this);
       
    }

    virtual void Write(int level, int /*opcode*/, const char * msg, va_list argptr)
    {
        char message_formated[1024];
        if ((msg) && (argptr) && (level != DL_LOADED_LIBRARY))
        {
            vsprintf_s(message_formated, sizeof(message_formated)/sizeof(message_formated[0]), msg, argptr);

            //todo: eliminate this
            if (message_formated[strlen(message_formated)-1] == '\n')
            {
                message_formated[strlen(message_formated)-1] = 0;
            }
            Log::WriteLog(message_formated);
        }
    }
   
};

static char* g_mfxlib = NULL;
bool is_loaded;



BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    try{
       
        std::string slib = "";
        switch( fdwReason ){
            case DLL_PROCESS_ATTACH:
                tracer_init();
                slib = Config::GetParam("core", "lib");
                g_mfxlib = new char[slib.length()];
                strcpy(g_mfxlib, slib.c_str());
                if (!strcmp(g_mfxlib, "none"))
                {
                    Log::useGUI = true;
                    if (!is_loaded)
                    {
                        run_shared_memory_server();
                    }
                                        
                }
                Log::WriteLog(std::string("function: DLLMain() DLL_PROCESS_ATTACH +"));
                Log::WriteLog("mfx_tracer: lib=" + std::string(g_mfxlib));
                Log::WriteLog(std::string("function: DLLMain() DLL_PROCESS_ATTACH - \n\n"));
                break;
            case DLL_THREAD_ATTACH:

                break;
            case DLL_THREAD_DETACH:

                break;
            case DLL_PROCESS_DETACH:
                Log::WriteLog(std::string("function: DLLMain() DLL_PROCESS_DETACH +"));
                //delete [] g_mfxlib;
                Log::WriteLog(std::string("function: DLLMain() DLL_PROCESS_DETACH - \n\n"));
                if (is_loaded)
                {
                   stop_shared_memory_server();
                }
                break;
        }
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
    return TRUE;
}


mfxStatus MFXInit(mfxIMPL impl, mfxVersion *ver, mfxSession *session)
{
    if (is_loaded)
    {
        return MFX_ERR_ABORTED;
    }
    try{
        
        is_loaded = true;

        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog(std::string("function: MFXInit(mfxIMPL impl=" + GetmfxIMPL(impl) + ", mfxVersion *ver=" + ToString(ver) + ", mfxSession *session=" + ToString(session) + ") +"));
        if (!session) {
            Log::WriteLog(context.dump("ver", *ver));
            Log::WriteLog(context.dump("session", *session));
            Log::WriteLog(context.dump_mfxStatus("status", MFX_ERR_NULL_PTR));
            return MFX_ERR_NULL_PTR;
        }

        mfxLoader* loader = (mfxLoader*)calloc(1, sizeof(mfxLoader));
        if (!loader) {
            Log::WriteLog(context.dump("ver", *ver));
            Log::WriteLog(context.dump("session", *session));
            Log::WriteLog(context.dump_mfxStatus("status", MFX_ERR_MEMORY_ALLOC));
            return MFX_ERR_MEMORY_ALLOC;
        }

        Log::WriteLog("LoadLibrary: " + std::string(g_mfxlib));
        HINSTANCE h_mfxdll = NULL;
        h_mfxdll = LoadLibrary(g_mfxlib);
        if (h_mfxdll == NULL){
            //dispatcher
            mfxInitParam par = {};
            mfxStatus sts;
             par.Implementation = impl;
            if (ver)
            {
                par.Version = *ver;
            }
            else
            {
                par.Version.Major = DEFAULT_API_VERSION_MAJOR;
                par.Version.Minor = DEFAULT_API_VERSION_MINOR;
            }
            par.ExternalThreads = 0;

            msdk_analyzer_sink sink;
            sts = _MFXInitEx(par, session);
            if (sts != MFX_ERR_NONE)
            {
                Log::WriteLog(context.dump("ver", *ver));
                Log::WriteLog(context.dump("session", *session));
                Log::WriteLog(context.dump_mfxStatus("status", MFX_ERR_NOT_FOUND));
                return MFX_ERR_NOT_FOUND;
            }
            char libModuleName[MAX_PATH];

            GetModuleFileName((HMODULE)(*(MFX_DISP_HANDLE**)(session))->hModule, libModuleName, MAX_PATH);
            if (GetLastError() != 0) 
            {
                Log::WriteLog("GetModuleFileName() reported  error! \n");
                
                return MFX_ERR_NOT_FOUND;
            }
            g_mfxlib = new char[MAX_PATH];
            strcpy(g_mfxlib, libModuleName);
            h_mfxdll = LoadLibrary(g_mfxlib);
        }
               
        loader->dlhandle = h_mfxdll;
        
        /* Loading functions table */
        int i;
        mfxFunctionPointer proc;
        for (i = 0; i < eFunctionsNum; ++i) {
            proc = (mfxFunctionPointer)GetProcAddress((HINSTANCE)loader->dlhandle, g_mfxFuncTable[i].name);
            if (!proc){
                Sleep(100);
                proc = (mfxFunctionPointer)GetProcAddress((HINSTANCE)loader->dlhandle, g_mfxFuncTable[i].name);
            }
            //if (!proc) break;
            if(proc) loader->table[i] = proc;
        }
        if (i < eFunctionsNum) {
            Log::WriteLog(ToString(eFunctionsNum));
            Log::WriteLog(ToString(i));
            Log::WriteLog("FreeLibrary: MFX_ERR_NOT_FOUND");
            FreeLibrary(h_mfxdll);
            free(loader);
            Log::WriteLog(context.dump("ver", *ver));
            Log::WriteLog(context.dump("session", *session));
            Log::WriteLog(context.dump_mfxStatus("status", MFX_ERR_NOT_FOUND));
            return MFX_ERR_NOT_FOUND;
        }
    
        Log::WriteLog(context.dump("impl", impl));
        Log::WriteLog(context.dump("ver", *ver));
        Log::WriteLog(context.dump("session", loader->session));
        /* Initializing loaded library */
        Timer t;
        mfxStatus mfx_res = (*(MFXInitPointer)loader->table[eMFXInit])(impl, ver, &(loader->session));
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXInit called");
        if (MFX_ERR_NONE != mfx_res) {
            FreeLibrary(h_mfxdll);
            free(loader);
            Log::WriteLog(context.dump("ver", *ver));
            Log::WriteLog(context.dump("session", *session));
            Log::WriteLog(context.dump_mfxStatus("status", mfx_res));
            return mfx_res;
        }
        *session = (mfxSession)loader;
        Log::WriteLog(context.dump("impl", impl));
        Log::WriteLog(context.dump("ver", *ver));
        Log::WriteLog(context.dump("session", loader->session));
        Log::WriteLog(std::string("function: MFXInit(" + elapsed + ", " + context.dump_mfxStatus("status", mfx_res) + ") - \n\n"));
        return MFX_ERR_NONE;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }

}

mfxStatus MFXClose(mfxSession session)
{
    try{
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXClose(mfxSession session=" + context.toString<mfxSession>(session) + ") +");
        mfxLoader* loader = (mfxLoader*)session;

        if (!loader){
            Log::WriteLog(context.dump("session", session));
            Log::WriteLog(context.dump_mfxStatus("status", MFX_ERR_INVALID_HANDLE));
            return MFX_ERR_INVALID_HANDLE;
        }
        Log::WriteLog(context.dump("session", session));
        Timer t;
        mfxStatus mfx_res = (*(MFXClosePointer)loader->table[eMFXClose])(loader->session);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXClose called");

        FreeLibrary((HINSTANCE)loader->dlhandle);
        free(loader);

        Log::WriteLog(context.dump("session", session));
        Log::WriteLog("function: MFXClose(" + elapsed + ", " + context.dump_mfxStatus("status", mfx_res) + ") - \n\n");
        return mfx_res;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
    return MFX_ERR_NOT_FOUND;
}

mfxStatus MFXInitEx(mfxInitParam par, mfxSession *session)
{
    try{
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog(std::string("function: MFXInitEx(mfxInitParam par= " + context.dump("par", par) + ", mfxSession *session=" + ToString(session) + ") +"));
        if (!session) {
            Log::WriteLog(context.dump("par", par));
            Log::WriteLog(context.dump("session", *session));
            Log::WriteLog(context.dump_mfxStatus("status", MFX_ERR_NULL_PTR));
            return MFX_ERR_NULL_PTR;
        }

        mfxLoader* loader = (mfxLoader*)calloc(1, sizeof(mfxLoader));
        if (!loader) {
            Log::WriteLog(context.dump("par", par));
            Log::WriteLog(context.dump("session", *session));
            Log::WriteLog(context.dump_mfxStatus("status", MFX_ERR_MEMORY_ALLOC));
            return MFX_ERR_MEMORY_ALLOC;
        }
        loader->dlhandle = LoadLibrary(g_mfxlib);
        if (!loader->dlhandle){
            Log::WriteLog(context.dump("par", par));
            Log::WriteLog(context.dump("session", *session));
            Log::WriteLog(context.dump_mfxStatus("status", MFX_ERR_NOT_FOUND));
            return MFX_ERR_NOT_FOUND;
        }
        /* Loading functions table */
        int i;
        mfxFunctionPointer proc;
        for (i = 0; i < eFunctionsNum; ++i) {
            proc = (mfxFunctionPointer)GetProcAddress((HINSTANCE)loader->dlhandle, g_mfxFuncTable[i].name);
            if (!proc){
                Sleep(100);
                proc = (mfxFunctionPointer)GetProcAddress((HINSTANCE)loader->dlhandle, g_mfxFuncTable[i].name);
            }
            if (!proc) break;
            loader->table[i] = proc;
        }
        if (i < eFunctionsNum) {
            FreeLibrary((HINSTANCE)loader->dlhandle);
            free(loader);
            Log::WriteLog(context.dump("par", par));
            Log::WriteLog(context.dump("session", *session));
            Log::WriteLog(context.dump_mfxStatus("status", MFX_ERR_NOT_FOUND));
            return MFX_ERR_NOT_FOUND;
        }

        Log::WriteLog(context.dump("par", par));
        Log::WriteLog(context.dump("session", loader->session));
        /* Initializing loaded library */
        Timer t;
        mfxStatus mfx_res = (*(MFXInitExPointer)loader->table[eMFXInitEx])(par, &(loader->session));
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXInitEx called");
        if (MFX_ERR_NONE != mfx_res) {
            FreeLibrary((HINSTANCE)loader->dlhandle);
            free(loader);
            Log::WriteLog(context.dump("par", par));
            Log::WriteLog(context.dump("session", *session));
            Log::WriteLog(context.dump_mfxStatus("status", mfx_res));
            return mfx_res;
        }
        *session = (mfxSession)loader;
        Log::WriteLog(context.dump("par", par));
        Log::WriteLog(context.dump("session", loader->session));
        Log::WriteLog(std::string("function: MFXInit(" + elapsed + ", " + context.dump_mfxStatus("status", mfx_res) + ") - \n\n"));
        return MFX_ERR_NONE;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}


#endif // #if defined(_WIN32) || defined(_WIN64)


