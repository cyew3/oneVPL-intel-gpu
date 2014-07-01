#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include "tracer.h"

static char* g_mfxlib = NULL;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    try{
        std::string slib = "";
        switch( fdwReason ){
            case DLL_PROCESS_ATTACH:
                Log::WriteLog(std::string("function: DLLMain() DLL_PROCESS_ATTACH +"));
                tracer_init();
                slib = Config::GetParam("core", "lib");
                g_mfxlib = new char[slib.length()];
                strcpy(g_mfxlib, slib.c_str());
                Log::WriteLog("mfx_tracer: lib=" + std::string(g_mfxlib));
                Log::WriteLog(std::string("function: DLLMain() DLL_PROCESS_ATTACH - \n\n"));
                break;
            case DLL_THREAD_ATTACH:

                break;
            case DLL_THREAD_DETACH:

                break;
            case DLL_PROCESS_DETACH:
                Log::WriteLog(std::string("function: DLLMain() DLL_PROCESS_DETACH +"));
                delete [] g_mfxlib;
                Log::WriteLog(std::string("function: DLLMain() DLL_PROCESS_DETACH - \n\n"));
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
    try{
        Log::WriteLog(std::string("function: MFXInit(mfxIMPL impl=" + ToString(impl) + ", mfxVersion *ver=" + ToString(ver) + ", mfxSession *session=" + ToString(session) + ") +"));
        if (!session) {
            Log::WriteLog(dump("ver", ver));
            Log::WriteLog(dump("session", *session));
            Log::WriteLog(dump_mfxStatus("status", MFX_ERR_NULL_PTR));
            return MFX_ERR_NULL_PTR;
        }

        mfxLoader* loader = (mfxLoader*)calloc(1, sizeof(mfxLoader));
        if (!loader) {
            Log::WriteLog(dump("ver", ver));
            Log::WriteLog(dump("session", *session));
            Log::WriteLog(dump_mfxStatus("status", MFX_ERR_MEMORY_ALLOC));
            return MFX_ERR_MEMORY_ALLOC;
        }

        Log::WriteLog("LoadLibrary: " + std::string(g_mfxlib));
        HINSTANCE h_mfxdll = NULL;
        h_mfxdll = LoadLibrary(g_mfxlib);
        if (h_mfxdll == NULL){
            Log::WriteLog(dump("ver", ver));
            Log::WriteLog(dump("session", *session));
            Log::WriteLog(dump_mfxStatus("status", MFX_ERR_NOT_FOUND));
            return MFX_ERR_NOT_FOUND;
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
            Log::WriteLog(dump("ver", ver));
            Log::WriteLog(dump("session", *session));
            Log::WriteLog(dump_mfxStatus("status", MFX_ERR_NOT_FOUND));
            return MFX_ERR_NOT_FOUND;
        }

        Log::WriteLog(dump("impl", impl));
        Log::WriteLog(dump("ver", ver));
        Log::WriteLog(dump("session", loader->session));
        /* Initializing loaded library */
        Timer t;
        mfxStatus mfx_res = (*(MFXInitPointer)loader->table[eMFXInit])(impl, ver, &(loader->session));
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXInit called");
        if (MFX_ERR_NONE != mfx_res) {
            FreeLibrary(h_mfxdll);
            free(loader);
            Log::WriteLog(dump("ver", ver));
            Log::WriteLog(dump("session", *session));
            Log::WriteLog(dump_mfxStatus("status", mfx_res));
            return mfx_res;
        }
        *session = (mfxSession)loader;
        Log::WriteLog(dump("impl", impl));
        Log::WriteLog(dump("ver", ver));
        Log::WriteLog(dump("session", loader->session));
        Log::WriteLog(std::string("function: MFXInit(" + elapsed + ", " + dump_mfxStatus("status", mfx_res) + ") - \n\n"));
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
        Log::WriteLog("function: MFXClose(mfxSession session=" + ToString(session) + ") +");
        mfxLoader* loader = (mfxLoader*)session;

        if (!loader){
            Log::WriteLog(dump("session", session));
            Log::WriteLog(dump_mfxStatus("status", MFX_ERR_INVALID_HANDLE));
            return MFX_ERR_INVALID_HANDLE;
        }
        Log::WriteLog(dump("session", session));
        Timer t;
        mfxStatus mfx_res = (*(MFXClosePointer)loader->table[eMFXClose])(loader->session);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXClose called");

        FreeLibrary((HINSTANCE)loader->dlhandle);
        free(loader);

        Log::WriteLog(dump("session", session));
        Log::WriteLog("function: MFXClose(" + elapsed + ", " + dump_mfxStatus("status", mfx_res) + ") - \n\n");
        return mfx_res;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
    return MFX_ERR_NOT_FOUND;
}

#endif // #if defined(_WIN32) || defined(_WIN64)
