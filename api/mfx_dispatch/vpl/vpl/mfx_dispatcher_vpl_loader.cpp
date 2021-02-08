/*############################################################################
  # Copyright (C) 2020 Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#include <algorithm>

#include "vpl/mfx_dispatcher_vpl.h"

// new functions for API >= 2.0
static const VPLFunctionDesc FunctionDesc2[NumVPLFunctions] = {
    { "MFXQueryImplsDescription", { { 0, 2 } } },
    { "MFXReleaseImplDescription", { { 0, 2 } } },
    { "MFXMemory_GetSurfaceForVPP", { { 0, 2 } } },
    { "MFXMemory_GetSurfaceForEncode", { { 0, 2 } } },
    { "MFXMemory_GetSurfaceForDecode", { { 0, 2 } } },
    { "MFXInitialize", { { 0, 2 } } },

    { "MFXMemory_GetSurfaceForVPPOut", { { 1, 2 } } },
    { "MFXVideoDECODE_VPP_Init", { { 1, 2 } } },
    { "MFXVideoDECODE_VPP_DecodeFrameAsync", { { 1, 2 } } },
    { "MFXVideoDECODE_VPP_Reset", { { 1, 2 } } },
    { "MFXVideoDECODE_VPP_GetChannelParam", { { 1, 2 } } },
    { "MFXVideoVPP_ProcessFrameAsync", { { 1, 2 } } },
};

// implementation of loader context (mfxLoader)
// each loader instance will build a list of valid runtimes and allow
// application to create sessions with them
LoaderCtxVPL::LoaderCtxVPL()
        : m_libInfoList(),
          m_implInfoList(),
          m_configCtxList(),
          m_userSearchDirs(),
          m_packageSearchDirs(),
          m_pathSearchDirs(),
          m_vplPackageDir(),
          m_specialConfig(),
          m_implIdxNext(0) {
    return;
}

LoaderCtxVPL::~LoaderCtxVPL() {
    return;
}

// creates ordered list of user-specified directories to search
mfxU32 LoaderCtxVPL::ParseEnvSearchPaths(const CHAR_TYPE* envVarName,
                                         std::list<STRING_TYPE>& searchDirs) {
    searchDirs.clear();

#if defined(_WIN32) || defined(_WIN64)
    DWORD err;
    CHAR_TYPE envVar[MAX_VPL_SEARCH_PATH] = { L"" };
    err = GetEnvironmentVariableW(envVarName, envVar, MAX_VPL_SEARCH_PATH);
    if (!err)
        return 0; // environment variable not defined

    // parse env variable into individual directories
    std::wstringstream envPath((CHAR_TYPE*)envVar);
    STRING_TYPE s;
    while (std::getline(envPath, s, L';')) {
        searchDirs.push_back(s);
    }
#else
    CHAR_TYPE* envVar = getenv(envVarName);
    if (!envVar)
        return 0; // environment variable not defined

    // parse env variable into individual directories
    std::stringstream envPath((CHAR_TYPE*)envVar);
    STRING_TYPE s;
    while (std::getline(envPath, s, ':')) {
        searchDirs.push_back(s);
    }
#endif

    return (mfxU32)searchDirs.size();
}

#define NUM_LIB_PREFIXES 2

mfxStatus LoaderCtxVPL::SearchDirForLibs(STRING_TYPE searchDir,
                                         std::list<LibInfo*>& libInfoList,
                                         mfxU32 priority) {
    // okay to call with empty searchDir
    if (searchDir.empty())
        return MFX_ERR_NONE;

#if defined(_WIN32) || defined(_WIN64)
    HANDLE hTestFile = nullptr;
    WIN32_FIND_DATAW testFileData;
    DWORD err;
    STRING_TYPE testFileName[NUM_LIB_PREFIXES] = { searchDir + MAKE_STRING("/libvpl*.dll"),
                                                   searchDir + MAKE_STRING("/libmfx*.dll") };

    CHAR_TYPE currDir[MAX_VPL_SEARCH_PATH] = L"";
    if (GetCurrentDirectoryW(MAX_VPL_SEARCH_PATH, currDir))
        SetCurrentDirectoryW(searchDir.c_str());

    // iterate over all candidate files in directory
    for (mfxU32 i = 0; i < NUM_LIB_PREFIXES; i++) {
        hTestFile = FindFirstFileW(testFileName[i].c_str(), &testFileData);
        if (hTestFile != INVALID_HANDLE_VALUE) {
            do {
                wchar_t libNameFull[MAX_VPL_SEARCH_PATH];
                wchar_t* libNameBase;

                // special case: do not include dispatcher itself (libmfx.dll, libvpl.dll)
                if (wcsstr(testFileData.cFileName, L"libmfx.dll") ||
                    wcsstr(testFileData.cFileName, L"libvpl.dll"))
                    continue;

                err = GetFullPathNameW(testFileData.cFileName,
                                       MAX_VPL_SEARCH_PATH,
                                       libNameFull,
                                       &libNameBase);
                // unknown error - skip it and move on to next file
                if (!err)
                    continue;

                // skip duplicates
                auto libFound =
                    std::find_if(libInfoList.begin(), libInfoList.end(), [&](LibInfo* li) {
                        return (li->libNameFull == libNameFull);
                    });
                if (libFound != libInfoList.end())
                    continue;

                LibInfo* libInfo = new LibInfo;
                if (!libInfo)
                    return MFX_ERR_MEMORY_ALLOC;

                libInfo->libNameFull = libNameFull;
                libInfo->libPriority = priority;

                // add to list
                libInfoList.push_back(libInfo);
            } while (FindNextFileW(hTestFile, &testFileData));

            FindClose(hTestFile);
        }
    }

    // restore current directory
    if (currDir[0])
        SetCurrentDirectoryW(currDir);

#else
    DIR* pSearchDir;
    struct dirent* currFile;

    pSearchDir = opendir(searchDir.c_str());
    if (pSearchDir) {
        while (1) {
            currFile = readdir(pSearchDir);
            if (!currFile)
                break;

            // save files with ".so" (including .so.1, etc.)
            if (strstr(currFile->d_name, ".so")) {
                // library names must begin with "libvpl*" or "libmfx*"
                if ((strstr(currFile->d_name, "libvpl") != currFile->d_name) &&
                    (strstr(currFile->d_name, "libmfx") != currFile->d_name))
                    continue;

                // special case: do not include dispatcher itself (libmfx.so*, libvpl.so*) or tracer library
                if (strstr(currFile->d_name, "libmfx.so") ||
                    strstr(currFile->d_name, "libvpl.so") ||
                    strstr(currFile->d_name, "libmfx-tracer"))
                    continue;

                char filePathC[MAX_VPL_SEARCH_PATH];

                // get full path to found library
                snprintf(filePathC,
                         MAX_VPL_SEARCH_PATH,
                         "%s/%s",
                         searchDir.c_str(),
                         currFile->d_name);
                char* fullPath = realpath(filePathC, NULL);

                // unknown error - skip it and move on to next file
                if (!fullPath)
                    continue;

                // skip duplicates
                auto libFound =
                    std::find_if(libInfoList.begin(), libInfoList.end(), [&](LibInfo* li) {
                        return (li->libNameFull == fullPath);
                    });
                if (libFound != libInfoList.end())
                    continue;

                LibInfo* libInfo = new LibInfo;
                if (!libInfo)
                    return MFX_ERR_MEMORY_ALLOC;

                libInfo->libNameFull = fullPath;
                libInfo->libPriority = priority;
                free(fullPath);

                // add to list
                libInfoList.push_back(libInfo);
            }
        }
        closedir(pSearchDir);
    }
#endif

    return MFX_ERR_NONE;
}

// search for implementations of oneAPI Video Processing Library (oneVPL)
//   according to the rules in the spec
mfxStatus LoaderCtxVPL::BuildListOfCandidateLibs() {
    mfxStatus sts = MFX_ERR_NONE;

    STRING_TYPE emptyPath; // default construction = empty
    std::list<STRING_TYPE>::iterator it;

    // first priority: user-defined directories in environment variable
    ParseEnvSearchPaths(ENV_ONEVPL_SEARCH_PATH, m_userSearchDirs);
    it = m_userSearchDirs.begin();
    while (it != m_userSearchDirs.end()) {
        STRING_TYPE nextDir = (*it);
        sts                 = SearchDirForLibs(nextDir, m_libInfoList, LIB_PRIORITY_USER_DEFINED);
        it++;
    }

    // second priority: oneVPL package
    ParseEnvSearchPaths(ENV_ONEVPL_PACKAGE_PATH, m_packageSearchDirs);
    it = m_packageSearchDirs.begin();
    while (it != m_packageSearchDirs.end()) {
        STRING_TYPE nextDir = (*it);
        sts                 = SearchDirForLibs(nextDir, m_libInfoList, LIB_PRIORITY_VPL_PACKAGE);
        it++;
    }

    // third priority: OS-specific PATH / LD_LIBRARY_PATH
    ParseEnvSearchPaths(ENV_OS_PATH, m_pathSearchDirs);
    it = m_pathSearchDirs.begin();
    while (it != m_pathSearchDirs.end()) {
        STRING_TYPE nextDir = (*it);
        sts                 = SearchDirForLibs(nextDir, m_libInfoList, LIB_PRIORITY_OS_PATH);
        it++;
    }

    // fourth priority: default system folders (current location for now)
    m_vplPackageDir = MAKE_STRING("./");
    sts             = SearchDirForLibs(m_vplPackageDir, m_libInfoList, LIB_PRIORITY_SYS_DEFAULT);

    // fifth priority: standalone MSDK/driver installation
    sts = SearchDirForLibs(emptyPath, m_libInfoList, LIB_PRIORITY_MSDK_PACKAGE);

    return sts;
}

// return number of valid libraries found
mfxU32 LoaderCtxVPL::CheckValidLibraries() {
    // load all libraries
    std::list<LibInfo*>::iterator it = m_libInfoList.begin();
    while (it != m_libInfoList.end()) {
        mfxU32 i         = 0;
        LibInfo* libInfo = (*it);

#if defined(_WIN32) || defined(_WIN64)
        // load DLL
        libInfo->hModuleVPL = MFX::mfx_dll_load(libInfo->libNameFull.c_str());

        // load video functions: pointers to exposed functions
        if (libInfo->hModuleVPL) {
            for (i = 0; i < NumVPLFunctions; i += 1) {
                VPLFunctionPtr pProc =
                    (VPLFunctionPtr)MFX::mfx_dll_get_addr(libInfo->hModuleVPL,
                                                          FunctionDesc2[i].pName);
                if (pProc)
                    libInfo->vplFuncTable[i] = pProc;
            }
        }
#else
        // Linux
        libInfo->hModuleVPL = dlopen(libInfo->libNameFull.c_str(), RTLD_LOCAL | RTLD_NOW);

        if (libInfo->hModuleVPL) {
            for (i = 0; i < NumVPLFunctions; i += 1) {
                VPLFunctionPtr pProc =
                    (VPLFunctionPtr)dlsym(libInfo->hModuleVPL, FunctionDesc2[i].pName);

                if (pProc)
                    libInfo->vplFuncTable[i] = pProc;
            }
        }
#endif
        // all runtime libraries with API >= 2.0 must export MFXInitialize()
        // validation of additional functions vs. API version takes place
        //   during UpdateValidImplList() since the minimum API version requested
        //   by application is not known yet (use SetConfigFilterProperty)
        if (libInfo->vplFuncTable[IdxMFXInitialize]) {
            it++;
        }
        else {
            // required function is missing from DLL, or DLL failed to load
            // remove this library from the list of options
            UnloadLibrary(libInfo);
            delete libInfo;
            it = m_libInfoList.erase(it);
        }
    }

    // number of valid oneVPL libs
    return (mfxU32)m_libInfoList.size();
}

// unload single runtime
mfxStatus LoaderCtxVPL::UnloadLibrary(LibInfo* libInfo) {
    if (libInfo && libInfo->hModuleVPL) {
#if defined(_WIN32) || defined(_WIN64)
        MFX::mfx_dll_free(libInfo->hModuleVPL);
#else
        dlclose(libInfo->hModuleVPL);
#endif
        return MFX_ERR_NONE;
    }
    else {
        return MFX_ERR_INVALID_HANDLE;
    }
}

// iterate over all implementation runtimes
// unload DLL's and free memory
mfxStatus LoaderCtxVPL::UnloadAllLibraries() {
    std::list<ImplInfo*>::iterator it2 = m_implInfoList.begin();
    while (it2 != m_implInfoList.end()) {
        ImplInfo* implInfo = (*it2);

        if (implInfo) {
            // call MFXReleaseImplDescription() for this implementation if it
            //   was never called by application
            if (implInfo->implDesc) {
                LibInfo* libInfo     = implInfo->libInfo;
                VPLFunctionPtr pFunc = libInfo->vplFuncTable[IdxMFXReleaseImplDescription];
                (*(mfxStatus(MFX_CDECL*)(mfxHDL))pFunc)(implInfo->implDesc);
            }

            delete implInfo;
        }
        it2++;
    }

    std::list<LibInfo*>::iterator it = m_libInfoList.begin();
    while (it != m_libInfoList.end()) {
        LibInfo* libInfo = (*it);

        if (libInfo) {
            UnloadLibrary(libInfo);
            delete libInfo;
        }
        it++;
    }

    return MFX_ERR_NONE;
}

// query capabilities of all valid libraries
//   and add to list for future calls to EnumImplementations()
//   as well as filtering by functionality
// assume MFX_IMPLCAPS_IMPLDESCSTRUCTURE is the only format supported
mfxStatus LoaderCtxVPL::QueryLibraryCaps() {
    std::list<LibInfo*>::iterator it = m_libInfoList.begin();
    while (it != m_libInfoList.end()) {
        mfxU32 num_impls = 0;
        LibInfo* libInfo = (*it);

        VPLFunctionPtr pFunc = libInfo->vplFuncTable[IdxMFXQueryImplsDescription];

        // call MFXQueryImplsDescription() for this implementation
        // return handle to description in requested format
        mfxHDL* hImpl;
        hImpl = (*(mfxHDL * (MFX_CDECL*)(mfxImplCapsDeliveryFormat, mfxU32*))
                     pFunc)(MFX_IMPLCAPS_IMPLDESCSTRUCTURE, &num_impls);

        if (!hImpl) {
            // the required function is implemented incorrectly
            // remove this library from the list of valid libraries
            UnloadLibrary(libInfo);
            delete libInfo;
            it = m_libInfoList.erase(it);
            continue;
        }

        for (mfxU32 i = 0; i < num_impls; i++) {
            ImplInfo* implInfo = new ImplInfo;
            if (!implInfo)
                return MFX_ERR_MEMORY_ALLOC;

            // library which contains this implementation
            implInfo->libInfo = libInfo;

            // implementation descriptor returned from runtime
            implInfo->implDesc           = hImpl[i];
            mfxImplDescription* implDesc = reinterpret_cast<mfxImplDescription*>(hImpl[i]);

            // fill out mfxInitializationParam for use in CreateSession (MFXInitialize path)
            memset(&(implInfo->vplParam), 0, sizeof(mfxInitializationParam));

            // default mode for this impl
            // this may be changed later by MFXSetConfigFilterProperty(AccelerationMode)
            implInfo->vplParam.AccelerationMode = implDesc->AccelerationMode;

            implInfo->version = implDesc->ApiVersion;

            // save local index for this library
            implInfo->libImplIdx = i;

            // assign unique index to each implementation
            implInfo->vplImplIdx = m_implIdxNext++;

            // initially all libraries have a valid, sequential value (>= 0)
            // list of valid libraries is updated with every call to MFXSetConfigFilterProperty()
            //   (see UpdateValidImplList)
            // libraries that do not support all the required props get a value of -1, and
            //   indexing of the valid libs is recalculated from 0,1,...
            implInfo->validImplIdx = implInfo->vplImplIdx;

            // add implementation to overall list
            m_implInfoList.push_back(implInfo);
        }
        it++;
    }

    return m_implInfoList.empty() ? MFX_ERR_UNSUPPORTED : MFX_ERR_NONE;
}

// query implementation i
mfxStatus LoaderCtxVPL::QueryImpl(mfxU32 idx, mfxImplCapsDeliveryFormat format, mfxHDL* idesc) {
    if (format != MFX_IMPLCAPS_IMPLDESCSTRUCTURE)
        return MFX_ERR_UNSUPPORTED;

    std::list<ImplInfo*>::iterator it = m_implInfoList.begin();
    while (it != m_implInfoList.end()) {
        ImplInfo* implInfo = (*it);

        if (implInfo->validImplIdx == (mfxI32)idx) {
            *idesc = implInfo->implDesc;
            return MFX_ERR_NONE;
        }
        it++;
    }

    // invalid idx
    return MFX_ERR_NOT_FOUND;
}

mfxStatus LoaderCtxVPL::ReleaseImpl(mfxHDL idesc) {
    mfxStatus sts = MFX_ERR_NONE;

    if (idesc == nullptr)
        return MFX_ERR_NULL_PTR;

    // all we get from the application is a handle to the descriptor,
    //   not the implementation associated with it, so we search
    //   through the full list until we find a match
    std::list<ImplInfo*>::iterator it = m_implInfoList.begin();
    while (it != m_implInfoList.end()) {
        ImplInfo* implInfo = (*it);

        if (implInfo->implDesc == idesc) {
            LibInfo* libInfo = implInfo->libInfo;

            VPLFunctionPtr pFunc = libInfo->vplFuncTable[IdxMFXReleaseImplDescription];

            // call MFXReleaseImplDescription() for this implementation
            sts = (*(mfxStatus(MFX_CDECL*)(mfxHDL))pFunc)(implInfo->implDesc);

            implInfo->implDesc = nullptr; // no longer valid

            return sts;
        }
        it++;
    }

    // did not find a matching handle - should not happen
    return MFX_ERR_INVALID_HANDLE;
}

mfxStatus LoaderCtxVPL::UpdateValidImplList(void) {
    mfxStatus sts = MFX_ERR_NONE;

    mfxI32 validImplIdx = 0;

    // iterate over all libraries and update list of those that
    //   meet current current set of config props
    std::list<ImplInfo*>::iterator it = m_implInfoList.begin();
    while (it != m_implInfoList.end()) {
        ImplInfo* implInfo = (*it);

        // already invalidated by previous filter
        if (implInfo->validImplIdx == -1) {
            it++;
            continue;
        }

        // TODO(JR) - if application has set mfxImplDescription.APIVersion, validate
        //   whether library exports all required functions for <= requested API

        // compare caps from this library vs. config filters
        sts = ConfigCtxVPL::ValidateConfig((mfxImplDescription*)implInfo->implDesc,
                                           m_configCtxList,
                                           &m_specialConfig);

        if (sts == MFX_ERR_NONE) {
            // library supports all required properties
            implInfo->validImplIdx = validImplIdx++;
        }
        else {
            // library does not support required props, do not include in list for
            //   MFXEnumImplementations() or MFXCreateSession()
            implInfo->validImplIdx = -1;
        }

        it++;
    }

    // re-sort valid implementations according to priority rules in spec
    PrioritizeImplList();

    return MFX_ERR_NONE;
}

// From specification section "oneVPL Session":
//
// When the dispatcher searches for the implementation, it uses the following priority rules
//  1) Hardware implementation has priority over software implementation.
//  2) General hardware implementation has priority over VSI hardware implementation.
//  3) Highest API version has higher priority over lower API version.
mfxStatus LoaderCtxVPL::PrioritizeImplList(void) {
    // stable sort - work from lowest to highest priority conditions

    // 3 - sort by API version
    m_implInfoList.sort([](const ImplInfo* impl1, const ImplInfo* impl2) {
        mfxImplDescription* implDesc1 = (mfxImplDescription*)(impl1->implDesc);
        mfxImplDescription* implDesc2 = (mfxImplDescription*)(impl2->implDesc);

        // prioritize greatest API version
        return (implDesc1->ApiVersion.Version > implDesc2->ApiVersion.Version);
    });

    // 2 - sort by general HW vs. VSI
    m_implInfoList.sort([](const ImplInfo* impl1, const ImplInfo* impl2) {
        mfxImplDescription* implDesc1 = (mfxImplDescription*)(impl1->implDesc);
        mfxImplDescription* implDesc2 = (mfxImplDescription*)(impl2->implDesc);

        // prioritize general HW accelerator over VSI (if none, i.e. SW, will be sorted in final step)
        return (implDesc1->AccelerationMode != MFX_ACCEL_MODE_VIA_HDDLUNITE &&
                implDesc2->AccelerationMode == MFX_ACCEL_MODE_VIA_HDDLUNITE);
    });

    // 1 - sort by implementation type (HW > SW)
    m_implInfoList.sort([](const ImplInfo* impl1, const ImplInfo* impl2) {
        mfxImplDescription* implDesc1 = (mfxImplDescription*)(impl1->implDesc);
        mfxImplDescription* implDesc2 = (mfxImplDescription*)(impl2->implDesc);

        // prioritize greatest Impl value (HW = 2, SW = 1)
        return (implDesc1->Impl > implDesc2->Impl);
    });

    // final pass - update index to match new priority order
    // validImplIdx will be the index associated with MFXEnumImplememntations()
    mfxI32 validImplIdx               = 0;
    std::list<ImplInfo*>::iterator it = m_implInfoList.begin();
    while (it != m_implInfoList.end()) {
        ImplInfo* implInfo = (*it);

        if (implInfo->validImplIdx >= 0) {
            implInfo->validImplIdx = validImplIdx++;
        }
        it++;
    }

    return MFX_ERR_NONE;
}

mfxStatus LoaderCtxVPL::CreateSession(mfxU32 idx, mfxSession* session) {
    mfxStatus sts = MFX_ERR_NONE;

    // find library with given implementation index
    // list of valid implementations (and associated indices) is updated
    //   every time a filter property is added/modified
    std::list<ImplInfo*>::iterator it = m_implInfoList.begin();
    while (it != m_implInfoList.end()) {
        ImplInfo* implInfo = (*it);

        if (implInfo->validImplIdx == (mfxI32)idx) {
            LibInfo* libInfo = implInfo->libInfo;

            if (sts == MFX_ERR_NONE) {
#if (MFX_VERSION >= 2002)
                // pass VendorImplID for this implementation (disambiguate if one
                //   library contains multiple implementations)
                mfxImplDescription* implDesc    = (mfxImplDescription*)(implInfo->implDesc);
                implInfo->vplParam.VendorImplID = implDesc->VendorImplID;
#endif

                // set any special parameters passed in via SetConfigProperty
                if (m_specialConfig.accelerationMode)
                    implInfo->vplParam.AccelerationMode = m_specialConfig.accelerationMode;

                // initialize this library via MFXInitialize or else fail
                //   (specify full path to library)
                sts = MFXInitEx2(implInfo->version,
                                 implInfo->vplParam,
                                 session,
                                 (CHAR_TYPE*)libInfo->libNameFull.c_str());
            }

            // optionally call MFXSetHandle() if present via SetConfigProperty
            if (sts == MFX_ERR_NONE && m_specialConfig.deviceHandleType &&
                m_specialConfig.deviceHandle) {
                sts = MFXVideoCORE_SetHandle(*session,
                                             m_specialConfig.deviceHandleType,
                                             m_specialConfig.deviceHandle);
            }

            return sts;
        }
        it++;
    }

    // invalid idx
    return MFX_ERR_NOT_FOUND;
}

ConfigCtxVPL* LoaderCtxVPL::AddConfigFilter() {
    // create new config filter context and add
    //   to list associated with this loader
    std::unique_ptr<ConfigCtxVPL> configCtx;
    try {
        configCtx.reset(new ConfigCtxVPL{});
    }
    catch (...) {
        return nullptr;
    }

    ConfigCtxVPL* config   = (ConfigCtxVPL*)(configCtx.release());
    config->m_parentLoader = this;

    m_configCtxList.push_back(config);

    return config;
}

mfxStatus LoaderCtxVPL::FreeConfigFilters() {
    std::list<ConfigCtxVPL*>::iterator it = m_configCtxList.begin();

    while (it != m_configCtxList.end()) {
        ConfigCtxVPL* config = (*it);
        if (config)
            delete config;
        it++;
    }

    return MFX_ERR_NONE;
}
