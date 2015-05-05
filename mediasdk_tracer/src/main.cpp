/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2015 Intel Corporation. All Rights Reserved.

File Name: main.cpp

\* ****************************************************************************** */
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "configuration.h"
#include "msdka_serial.h"

#include "msdka_import.h"

//function table

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    return_value (* p##func_name) formal_param_list;

#include "msdka_exposed_functions_list.h"


//external exposes
extern mfxStatus MFXInit(mfxIMPL impl, mfxVersion *pVer, mfxSession *session);
extern mfxStatus MFXClose(mfxSession session);


static HMODULE module;

void getinfo(TCHAR * filename, TCHAR *pFileVersion, int filevLen,  TCHAR *pProductVersion, int prodvLen, int &dll_size)
{
    BOOL bResult = FALSE;
    DWORD size;
    DWORD dummy;

    unsigned int len;
    size = GetFileVersionInfoSize( filename, &dummy );
    if( size == 0 )
    {
        return ;
    }
    TCHAR * pBuffer = new TCHAR[ size ];
    VS_FIXEDFILEINFO* data = NULL;
    if( NULL == pBuffer)
    {
        return ;
    }
    bResult = GetFileVersionInfo( filename, 0, size, (void*)pBuffer );
    if( !bResult )
    {
        delete[] pBuffer;
        return ;
    }
    bResult = VerQueryValue( pBuffer, _T("\\StringFileInfo\\040904b0\\FileVersion"), (void**)&data, &len );
    if( !bResult || data == NULL)
    {
        delete[] pBuffer;
        return ;
    }
    ::_tcscpy_s(pFileVersion, filevLen, (TCHAR*)data);

    bResult = VerQueryValue( pBuffer, _T("\\StringFileInfo\\040904b0\\ProductVersion"), (void**)&data, &len );
    if( !bResult || data == NULL)
    {
        delete[] pBuffer;
        return ;
    }

    ::_tcscpy_s(pProductVersion, prodvLen, (TCHAR*)data);

    struct __stat64 stat;
    if( -1 == _tstat64(filename, &stat))
    {
        dll_size = 0;
    }else
    {
        dll_size = (int)stat.st_size;
    }

    delete[] pBuffer;
}

class msdk_analyzer_sink : public IMsgHandler
{
    std::basic_string<TCHAR> m_libPath;
public:
    msdk_analyzer_sink()
    {
        //library notifier
        DispatchLog::get().AttachSink(DL_SINK_IMsgHandler, this);
        //attaching logger
        DispatchLog::get().AttachSink(DL_SINK_IMsgHandler, &DispatcherLogRecorder::get());

        DispatchLog::get().DetachSink(DL_SINK_PRINTF, NULL);
    }
    ~msdk_analyzer_sink()
    {
        DispatchLog::get().DetachSink(DL_SINK_IMsgHandler, this);
        DispatchLog::get().DetachSink(DL_SINK_IMsgHandler, &DispatcherLogRecorder::get());
    }

    virtual void Write(int level, int /*opcode*/, const char * msg, va_list argptr)
    {
        HMODULE libModule;
        if (level == DL_LOADED_LIBRARY)
        {
            char str[32];
            vsprintf_s(str, sizeof(str)/sizeof(str[0]), msg, argptr);
            if (1 == sscanf_s(str, "%p", &libModule))
            {
                TCHAR libPath [2048];
                GetModuleFileName(libModule,libPath, sizeof(libPath)/sizeof(libPath[0]));
                m_libPath = libPath;
            }
        }
    }
    std::basic_string<TCHAR>& GetPath(){return m_libPath;}
};

mfxStatus MFXInit(mfxIMPL impl, mfxVersion *pVer, mfxSession *session) {

    //application might not zero session pointer
    if (session != 0)
    {
        *session = 0;
    }

    std::auto_ptr<AnalyzerSession> as(new AnalyzerSession());
    if (!as.get()) return MFX_ERR_MEMORY_ALLOC;
    //memset not required: see initialisation of POD types   
    //memset(as->get(),0,sizeof(*as));

    mfxStatus sts;

    EDIT_CONFIGURATION(TEXT("MFXInit"),{
        scan_mfxIMPL(line2,&impl);
        scan_mfxVersion(line2,pVer);
    });
    

    if (gc.instant_init || !gc.is_gui_present)
    {
        if (session != 0)
        {
            *session=(mfxSession)as.release();
        }
        return gc.is_gui_present ? MFX_ERR_NONE : MFX_ERR_UNSUPPORTED;
    }

    //special case to override library version used in testing of old applications 
    mfxVersion version_local;

    if (0 != gc.use_version.Version )
    {
        if (!pVer)
            pVer = &version_local;
        
        *pVer = gc.use_version;
    }

    if (gc.use_dispatcher)
    {
        StackIncreaser si;

        //if dispatcher already find this library in subsequent call to mfxinit
        if (si.get()!=1)
        {
            return MFX_ERR_UNSUPPORTED;
        }

        
        msdk_analyzer_sink sink;

        //library will be loaded using dispatcher
        sts = MFXInit(impl, pVer, &as->session);

        _tcscpy_s(gc.sdk_dll, sizeof(gc.sdk_dll) / sizeof(gc.sdk_dll[0]), sink.GetPath().c_str());

    }else
    {
        RECORD_CONFIGURATION({
            dump_format_wprefix(fd, level, 1,TEXT("SDK DLL="),TEXT("%s"),gc.sdk_dll);
            if (gc.edit_configuration || gc.edit_configuration_per_frame)
                dump_format_wprefix(fd, level, 1,TEXT("CFG File="),TEXT("%s"),gc.cfg_file);
        });
      
        module=LoadLibrary(gc.sdk_dll);

      if (!module) {
            RECORD_CONFIGURATION({
            dump_format_wprefix(fd, level, 1,TEXT("Failed to load "),TEXT("%s"), gc.sdk_dll);
            });
            return MFX_ERR_UNSUPPORTED;
        }

      //removing endless recursion here
      if (NULL != GetProcAddress(module, "msdk_analyzer_dll"))
      {
          RECORD_CONFIGURATION({
              dump_format_wprefix(fd, level, 0,TEXT("ERROR: msdk_analyzer detected at %s instead of Mediasdk native library, need to correct SDK DLL key\n"), gc.sdk_dll);
          });
          return MFX_ERR_UNSUPPORTED;  
      }

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
        p##func_name=(return_value (*) formal_param_list)GetProcAddress(module,#func_name); \
        if (!p##func_name && pVer != NULL && FUNCTION_VERSION_MAJOR <= pVer->Major && FUNCTION_VERSION_MINOR <=pVer->Minor) { \
            RECORD_CONFIGURATION({    \
                dump_format_wprefix(fd, level, 1,TEXT("Missing function "),TEXT("%s"), #func_name);    \
            });    \
        }

#include "mfx_exposed_functions_list.h"

        mfxStatus (*pMFXInit)(mfxIMPL,mfxVersion*,mfxSession*);
        pMFXInit=(mfxStatus (*)(mfxIMPL,mfxVersion*,mfxSession*))GetProcAddress(module,"MFXInit");
        if (!pMFXInit) {
            RECORD_CONFIGURATION({
                dump_format_wprefix(fd, level, 1,TEXT("Failed to load "),TEXT("%s, missing MFXInit"), gc.sdk_dll);
            });
            return MFX_ERR_UNSUPPORTED;
        }

        sts=(*pMFXInit)(impl,pVer,&as->session);
    }

    TCHAR pFileV[256] = {_T('\0')}, pProductV[256] = {_T('\0')};
    int dll_size;
    getinfo(gc.sdk_dll, pFileV, 256, pProductV, 256, dll_size);

    RECORD_CONFIGURATION({
        dump_format_wprefix(fd, level, 1,TEXT("SDK DLL File Version="),TEXT("%s"), pFileV);
        dump_format_wprefix(fd, level, 1,TEXT("SDK DLL Product Version="),TEXT("%s"), pProductV);
        dump_format_wprefix(fd, level, 1,TEXT("SDK DLL File Size="),TEXT("%d"), dll_size );

        dump_mfxIMPL(fd, level,  TEXT("MFXInit"), impl);
        dump_mfxVersion(fd, level,  TEXT("MFXInit"), pVer);
        dump_mfxStatus(fd, level,  TEXT("MFXInit"), sts);
    });

    LARGE_INTEGER ts1;
    QueryPerformanceCounter(&ts1);
    as->init_tick=ts1.QuadPart;

    if (NULL != session)
    {
        *session=(mfxSession)as.release();
    }

    return sts;
}

mfxStatus MFXClose(mfxSession session) {
    AnalyzerSession *as=(AnalyzerSession *)session;

    LARGE_INTEGER ts1;
    QueryPerformanceCounter(&ts1);

    mfxStatus sts = MFX_ERR_NONE;

    if (gc.use_dispatcher)
    {
        sts = MFXClose(as->session);
    }else
    {
        mfxStatus (*pMFXClose)(mfxSession);
        pMFXClose=(mfxStatus (*)(mfxSession))GetProcAddress(module,"MFXClose");
    
        if (pMFXClose) sts=(*pMFXClose)(as->session);
    }

    /* record transcoding performance data */
    mfxU64 frames=as->decode.frames;
    if (as->vpp.frames>frames) frames=as->vpp.frames;
    if (as->encode.frames>frames) frames=as->encode.frames;
    RECORD_CONFIGURATION2((as->decode.frames+as->vpp.frames+as->encode.frames>frames), MSDKA_LEVEL_GLOBAL,
    {
        LARGE_INTEGER cr;
        QueryPerformanceFrequency(&cr);

        dump_format_wprefix(fd, level, 1,TEXT("transcode.perf.frames="),TEXT("%I64u"),frames);
        dump_format_wprefix(fd, level, 1,TEXT("transcode.perf.exec_time="),TEXT("%g (s/f)"),((double)ts1.QuadPart-(double)as->init_tick)/(double)frames/(double)cr.QuadPart);
    });

    delete as;
    if (module) FreeLibrary(module);
    //DUE to dispatcher issue we cant return status directly it could be not MFX_ERR_NONE, and it will cause dispatcher crash
    return MFX_ERR_NONE;
}

mfxStatus MFXQueryIMPL(mfxSession session, mfxIMPL *impl) {
    mfxStatus sts = MFX_CALL(MFXQueryIMPL,(((AnalyzerSession *)session)->session, impl));

    EDIT_CONFIGURATION(TEXT("MFXQueryIMPL"),{
        scan_mfxIMPL(line2,impl);
    });

    RECORD_CONFIGURATION({
        dump_mfxIMPL(fd, level,  TEXT("MFXQueryIMPL"), *impl);
    });
    return sts;
}

mfxStatus MFXQueryVersion(mfxSession session, mfxVersion *version) {
    mfxStatus sts = MFX_ERR_NONE;
    if (_tcslen(gc.sdk_dll) != 0)
        sts = MFX_CALL(MFXQueryVersion,(((AnalyzerSession *)session)->session, version));
    else
        sts = MFX_CALL(MFXQueryVersion,(session, version));

    RECORD_CONFIGURATION({
        dump_mfxVersion(fd, level, TEXT("MFXQueryVersion"), version);
    });
    return sts;
}

mfxStatus MFXCloneSession(mfxSession session, mfxSession *clone)
{
    AnalyzerSession *as = new AnalyzerSession();
    if (!as) return MFX_ERR_MEMORY_ALLOC;

    *clone = (mfxSession)as;

    mfxStatus sts=MFX_CALL(MFXCloneSession,(((AnalyzerSession *)session)->session, &as->session));

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("MFXCloneSession"),sts);
    });
    if (MFX_ERR_NONE != sts)
    {
        delete as;
        *clone = NULL;
    }
    return sts;
}

mfxStatus MFXJoinSession(mfxSession session, mfxSession child)
{
    mfxStatus sts=MFX_CALL(MFXJoinSession,(((AnalyzerSession *)session)->session, ((AnalyzerSession *)child)->session));

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("MFXJoinSession"),sts);
    });
    return sts;
}

mfxStatus MFXDisjoinSession(mfxSession session) {
    mfxStatus sts=MFX_CALL(MFXDisjoinSession,(((AnalyzerSession *)session)->session));

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("MFXDisjoinSession"),sts);
    });
    return sts;
}


mfxStatus MFXSetPriority(mfxSession session, mfxPriority priority) {
    EDIT_CONFIGURATION(TEXT("MFXSetPriority"),{
        scan_mfxPriority(line2,&priority);
    });
    mfxStatus sts=MFX_CALL(MFXSetPriority,(((AnalyzerSession *)session)->session, priority));

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("MFXSetPriority"), sts);
    });
    return sts;
}

mfxStatus MFXGetPriority(mfxSession session, mfxPriority *priority) {
    mfxStatus sts=MFX_CALL(MFXGetPriority,(((AnalyzerSession *)session)->session, priority));

    EDIT_CONFIGURATION(TEXT("MFXGetPriority"),{
        scan_mfxPriority(line2,priority);
    });

    RECORD_CONFIGURATION({
        dump_mfxPriority(fd, level, TEXT("MFXGetPriority"),*priority);
        dump_mfxStatus(fd, level, TEXT("MFXGetPriority"), sts);
    });
    return sts;
}

#if 0
mfxStatus MFXGetLogMessage(mfxSession session, char *msg, mfxU32 size) {
    mfxStatus sts=MFX_CALL(MFXGetLogMessage,(((AnalyzerSession *)session)->session, msg, size));

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("MFXGetLogMessage"),sts);
    });
    return sts;
}
#endif

#pragma warning(disable: 4100)

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved) 
{
    if (ul_reason_for_call==DLL_PROCESS_ATTACH) {
        gc.ReadRegistrySetting(HKEY_CURRENT_USER);

        RECORD_CONFIGURATION({
            TCHAR path[1024];
            TCHAR date[64];
            TCHAR time[64];
            _tstrtime_s(time, 64);
            _tstrdate_s(date, 64);
            dump_format_wprefix(fd, level, 0,TEXT(""));
            dump_format_wprefix(fd, level, 0,TEXT("=====%s %s===="),time,date);
            GetModuleFileName(hModule, path, sizeof(path)/sizeof(path[0]));
            dump_format_wprefix(fd, level, 1,TEXT("SDK_ANALYZER loaded at: "),TEXT("%s"), path);
            HANDLE curProcHandle = OpenProcess(0,0,GetCurrentProcessId());
            GetModuleFileName((HMODULE)curProcHandle, path, sizeof(path)/sizeof(path[0]));
            dump_format_wprefix(fd, level, 1,TEXT("PROCESS: "),TEXT("%s"), GetCommandLine());
            CloseHandle(curProcHandle);
        });

        gc.ValidateRegistry(HKEY_CURRENT_USER);
        gc.ReadConfiguration();
        gc.DetectGUIPresence();

    }else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
            extern StatusMap sm_encode, sm_decode, sm_vpp, sm_encode_sync, sm_decode_sync, sm_vpp_sync;

            RECORD_CONFIGURATION({
                dump_format_wprefix(fd, level, 1,TEXT("=====mfxSTATUS MAP====="),TEXT(""));
                dump_StatusMap(fd, level,  TEXT("DecodeFrameAsync"), &sm_decode);
                dump_StatusMap(fd, level,  TEXT("SyncOperation(D)"), &sm_decode_sync);
                dump_StatusMap(fd, level,  TEXT("EncodeFrameAsync"), &sm_encode);
                dump_StatusMap(fd, level,  TEXT("SyncOperation(E)"), &sm_encode_sync);
                dump_StatusMap(fd, level,  TEXT("RunFrameVPPAsync"), &sm_vpp);
                dump_StatusMap(fd, level,  TEXT("SyncOperation(V)"), &sm_vpp_sync);
                

                //TODO : it is possible to check whether dec+vpp surfaces pool intersect with vpp+encode pool, 
                //in general case they could have intersections, but in most cases they shouldn't
                dump_format_wprefix(fd, level,  1, TEXT("====Unique Surfaces===="),TEXT(""));
                dump_format_wprefix(fd, level,  1, TEXT("DecodeFrameAsync      : "), TEXT("%d"), gc.GetSurfacesCount(DECODE, RunAsync_Output));
                dump_format_wprefix(fd, level,  1, TEXT("SyncOperation(D)      : "), TEXT("%d"), gc.GetSurfacesCount(DECODE, SyncOperation_Output));
                dump_format_wprefix(fd, level,  1, TEXT("RunFrameVPPAsync(IN)  : "), TEXT("%d"), gc.GetSurfacesCount(VPP, RunAsync_Input));
                dump_format_wprefix(fd, level,  1, TEXT("RunFrameVPPAsync(OUT) : "), TEXT("%d"), gc.GetSurfacesCount(VPP, RunAsync_Output));
                dump_format_wprefix(fd, level,  1, TEXT("SyncOperation(VPP_IN) : "), TEXT("%d"), gc.GetSurfacesCount(VPP, SyncOperation_Input));
                dump_format_wprefix(fd, level,  1, TEXT("SyncOperation(VPP_OUT): "), TEXT("%d"), gc.GetSurfacesCount(VPP, SyncOperation_Output));
                dump_format_wprefix(fd, level,  1, TEXT("EncodeFrameAsync      : "), TEXT("%d"), gc.GetSurfacesCount(ENCODE, RunAsync_Input));
                dump_format_wprefix(fd, level,  1, TEXT("SyncOperation(E)      : "), TEXT("%d"), gc.GetSurfacesCount(ENCODE, SyncOperation_Input));
                dump_format_wprefix(fd, level,  1, TEXT("======================="),TEXT(""));

                TCHAR path[1024];
                GetModuleFileName(hModule, path, sizeof(path)/sizeof(path[0]));
                dump_format_wprefix(fd, level,  1,TEXT("SDK_ANALYZER unloaded: "),TEXT("%s"), path);
        });
    }
    return TRUE;
}

