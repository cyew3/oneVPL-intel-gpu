/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2012 Intel Corporation. All Rights Reserved.

File Name: configuration.h

\* ****************************************************************************** */
#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__
#include <windows.h>
#include <mfxstructures.h>
#include <tchar.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "msdka_defs.h"
#include "msdka_structures.h"

struct GlobalConfiguration {
    bool  use_dispatcher;
    bool  instant_init;//used to just indicates that sdk_analyzer loaded
    bool  is_gui_present;
    bool  run_without_gui;
    bool  is_metro_app;

    int level;

    mfxVersion use_version;

    DWORD record_configuration;
    DWORD record_configuration_per_frame;
    DWORD record_pointers;
    DWORD record_thread_id;
    
    DWORD edit_configuration;
    DWORD edit_configuration_per_frame;
    struct 
    {
        DWORD skip_sync_operation;
        DWORD sync;
        DWORD handle_device_busy;
        /// dont call async functions, - maximum performance possible
        /// instead return firstly created frame in all next calls, 
        /// WARNING: work for encoder only
        DWORD stub_mode;
    }decode,vpp,encode;

    DWORD debug_view;
    DWORD tls_idx_threadinfo;
    enum {
        ASYNC_NONE,
        ASYNC,
        ASYNC_ETW,
        ASYNC_SHARED_MEMORY
    }async_dump;
    
    DWORD record_raw_bits_start;
    DWORD record_raw_bits_end;
    DWORD record_raw_bits_module;

    enum {
        TIMESTAMP_NONE,
        TIMESTAMP_LOCAL,
        TIMESTAMP_UTC,
        TIMESTAMP_RELATIVE
    } record_timestamp_mode;
    LARGE_INTEGER qpc_start;
    LARGE_INTEGER qpc_freq;

    TCHAR sdk_dll[1024];
    TCHAR cfg_file[1024];
    TCHAR raw_file[1024];
    TCHAR log_file[1024];

    LinkedString *edit_configuration_lines;

    GlobalConfiguration(void);
    ~GlobalConfiguration(void);

    void ReadRegistrySetting(HKEY hkey);
    //probes for all known options, aims to safe time if some options are handy - edited
    static void ValidateRegistry(HKEY hkey);
    void ReadConfiguration(void);
    void DetectGUIPresence(void);
    void CreateMetroSpecificObjects(void);
    static bool IsMetroProcess(void);

    //fast dump support
    struct TLSData
    {
        LARGE_INTEGER timestamp;
        int           level;
        TCHAR       * prefix[5];
        std::basic_string<TCHAR> str;
        bool operator() (const TLSData & d1,
                         const TLSData & d2)
        {
            return d1.timestamp.QuadPart < d2.timestamp.QuadPart;
        }
    };
    
    class TLSDataList : public std::list<TLSData>
    {
    public:
        void push_back(const TLSData& _Val);
    };

    struct ThreadData
    {
        TLSDataList messages;
        int                thread_id;
        int                nStackSize; //saves stack 
        ThreadData()
            : nStackSize()
        {}
    };
    typedef union HANDLE64{
        __int64 hdl64;
        HANDLE hdl;
    };

    struct {
        HANDLE64 BufferReadyEvent;
        HANDLE64 DataReadyEvent;
        HANDLE64 BufferMapping;
    } MetroHandles;

    CRITICAL_SECTION threadinfo_access;
    std::vector<ThreadData> threads_info;
    DebugBuffer *m_sharedBuffer;

    ThreadData & GetThreadData();

    void WriteLogData();
    
    
    //automatic async level detection support
    std::map<std::pair<int, int>, std::set<void*> > m_surfacesData;

    CRITICAL_SECTION surfacesdata_access;
    void RegisterSurface(int component, int component_entry, mfxFrameSurface1 *pSurface);
    int  GetSurfacesCount(int component, int component_entry);
};

extern GlobalConfiguration gc;

#endif
