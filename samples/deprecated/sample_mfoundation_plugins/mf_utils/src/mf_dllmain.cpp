/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

// Note: This source file implements the class factory, plus the following
// DLL functions:
// - DllMain
// - DllCanUnloadNow
// - DllRegisterServer
// - DllUnregisterServer
// - DllGetClassObject

#include "mf_guids.h"

/*------------------------------------------------------------------------------*/

//mfxVersion g_MfxVersion = {MFX_VERSION_MINOR, MFX_VERSION_MAJOR};
#if MFX_D3D11_SUPPORT
    mfxVersion g_MfxVersion = {7, 1};
#else
    mfxVersion g_MfxVersion = {4, 1};
#endif
/*------------------------------------------------------------------------------*/

const TCHAR* REGKEY_MF_BYTESTREAM_HANDLERS = TEXT("Software\\Microsoft\\Windows Media Foundation\\ByteStreamHandlers");

/*------------------------------------------------------------------------------*/

HINSTANCE g_hInst; // DLL module handle

static ClassRegData *g_ClassFactories = NULL; // pointer to ClassRegData
static UINT32       g_numClassFactories = 0;  // number of classes to registrate
LONG                g_cRefModule = 0;         // module reference count

/*------------------------------------------------------------------------------*/

// Functions to increase and decrease reference count for a module
void DllAddRef(void);
void DllRelease(void);

/*------------------------------------------------------------------------------*/

// Functions to register and unregister the byte stream handler.
HRESULT RegisterByteStreamHandler(const GUID& guid, const TCHAR *sFileExtension, const TCHAR *sDescription);
HRESULT UnregisterByteStreamHandler(const GUID& guid, const TCHAR *sFileExtension);

/*------------------------------------------------------------------------------*/
// ClassFactory: implements the class factory for the COM objects.

class ClassFactory : public IClassFactory
{
public:
    ClassFactory(ClassRegData* pRegistrationData) :
        m_refCount(1),
        m_pRegistrationData(pRegistrationData)
    {
        DllAddRef();
    }

    static bool IsLocked(void)
    {
        return (0 != m_serverLocks);
    }

    // IUnknown methods
    virtual STDMETHODIMP_(ULONG) AddRef(void)
    {
        return InterlockedIncrement(&m_refCount);
    }

    virtual STDMETHODIMP_(ULONG) Release(void)
    {
        assert(m_refCount >= 0);
        ULONG uCount = InterlockedDecrement(&m_refCount);
        if (uCount == 0) delete this;
        // Return the temporary variable, not the member
        // variable, for thread safety.
        return uCount;
    }

    virtual STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if (NULL == ppv) return E_POINTER;
        else if (riid == IID_IUnknown)
        {
            *ppv = static_cast<IUnknown*>(this);
        }
        else if (riid == IID_IClassFactory)
        {
            *ppv = static_cast<IClassFactory*>(this);
        }
        else
        {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    virtual STDMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv)
    {
        // We do not support aggregation.
        if (NULL != pUnkOuter) return CLASS_E_NOAGGREGATION;
        if (!(m_pRegistrationData->pCreationFunction)) return E_FAIL;
        return m_pRegistrationData->pCreationFunction(riid, ppv, m_pRegistrationData);
    }

    virtual STDMETHODIMP LockServer(BOOL lock)
    {
        if (lock)
        {
            DllAddRef();
        }
        else
        {
            DllRelease();
        }

        return S_OK;
    }

protected:
    volatile long          m_refCount;    // Reference count.
    static volatile long   m_serverLocks; // Number of server locks

    ClassRegData* m_pRegistrationData; // Registration data including creation function

private:
    // avoiding possible problems by defining operator= and copy constructor
    ClassFactory(const ClassFactory&);
    ClassFactory& operator=(const ClassFactory&);

    ~ClassFactory(void)
    {
        DllRelease();
    }
};

volatile long ClassFactory::m_serverLocks = 0;

/*------------------------------------------------------------------------------*/

void DllAddRef(void)
{
    InterlockedIncrement(&g_cRefModule);
}

/*------------------------------------------------------------------------------*/

void DllRelease(void)
{
    InterlockedDecrement(&g_cRefModule);
}

/*------------------------------------------------------------------------------*/

static HRESULT RegisterAsMFT(ClassRegData *pRegData,
                             GUID guidCategory)
{
    HRESULT hr = S_OK;

    if (!pRegData) return E_POINTER;

    DWORD i = 0;
    DWORD cInputTypes  = pRegData->cInputTypes;
    DWORD cOutputTypes = pRegData->cOutputTypes;
    MFT_REGISTER_TYPE_INFO *pInputTypes  = NULL;
    MFT_REGISTER_TYPE_INFO *pOutputTypes = NULL;

    if (SUCCEEDED(hr) && cInputTypes)
    {
        SAFE_NEW_ARRAY(pInputTypes, MFT_REGISTER_TYPE_INFO, cInputTypes);
        if (!pInputTypes) hr = E_OUTOFMEMORY;
        else
        {
            for (i = 0; i < cInputTypes; ++i)
            {
                pInputTypes[i].guidMajorType = pRegData->pInputTypes[i].major_type;
                pInputTypes[i].guidSubtype   = pRegData->pInputTypes[i].subtype;
            }
        }
    }
    if (SUCCEEDED(hr) && cOutputTypes)
    {
        SAFE_NEW_ARRAY(pOutputTypes, MFT_REGISTER_TYPE_INFO, cOutputTypes);
        if (!pOutputTypes) hr = E_OUTOFMEMORY;
        else
        {
            for (i = 0; i < cOutputTypes; ++i)
            {
                pOutputTypes[i].guidMajorType = pRegData->pOutputTypes[i].major_type;
                pOutputTypes[i].guidSubtype   = pRegData->pOutputTypes[i].subtype;
            }
        }
    }
    if (SUCCEEDED(hr)) hr = MFTRegister(*(pRegData->guidClassID), guidCategory,
                                        pRegData->pPluginName,
                                        pRegData->iFlags,
                                        cInputTypes, pInputTypes,
                                        cOutputTypes, pOutputTypes,
                                        pRegData->pAttributes);
    SAFE_DELETE_ARRAY(pInputTypes);
    SAFE_DELETE_ARRAY(pOutputTypes);
    return hr;
}

/*------------------------------------------------------------------------------*/

static HRESULT RegisterAsWMPPlugin(ClassRegData *pRegData)
{
    HRESULT hr = S_OK;

    if (!pRegData) return E_POINTER;

    CComPtr<IWMPMediaPluginRegistrar> spRegistrar;
    DWORD cInputTypes = pRegData->cInputTypes, i = 0;
    DMO_PARTIAL_MEDIATYPE* mt = NULL;

    // Create the registration object
    hr = spRegistrar.CoCreateInstance(CLSID_WMPMediaPluginRegistrar, NULL, CLSCTX_INPROC_SERVER);
    if (FAILED(hr)) return hr;

    // Load friendly name and description strings

    // Describe the type of data handled by the plug-in
    mt = (DMO_PARTIAL_MEDIATYPE*)calloc(cInputTypes, sizeof(DMO_PARTIAL_MEDIATYPE));
    if (NULL == mt) return E_OUTOFMEMORY;

    for(i = 0; i < cInputTypes; ++i)
    {
        mt[i].type = pRegData->pInputTypes[i].major_type;
        mt[i].subtype = pRegData->pInputTypes[i].subtype;
    }

    // Register the plug-in with WMP
#if 0
    hr = spRegistrar->WMPRegisterPlayerPlugin(
                    pRegData->pPluginName,    // friendly name (for menus, etc)
                    pRegData->pPluginName,    // description (for Tools->Options->Plug-ins)
                    NULL,                     // path to app that uninstalls the plug-in
                    1,                        // DirectShow priority for this plug-in
                    WMP_PLUGINTYPE_DSP,       // Plug-in type
                    *(pRegData->guidClassID), // Class ID of plug-in
                    cInputTypes,              // No. media types supported by plug-in
                    mt);                      // Array of media types supported by plug-in
#endif

    // Also register for out-of-proc playback in the MF pipeline
    // We'll only do this on Windows Vista or later operating systems because
    // WMP 11 and Vista are required at a minimum.
    if (SUCCEEDED(hr) /*&& TRUE == IsVistaOrLater()*/)
    {
        hr = spRegistrar->WMPRegisterPlayerPlugin(
                        pRegData->pPluginName,        // friendly name (for menus, etc)
                        pRegData->pPluginName,        // description (for Tools->Options->Plug-ins)
                        NULL,                         // path to app that uninstalls the plug-in
                        1,                            // DirectShow priority for this plug-in
                        WMP_PLUGINTYPE_DSP_OUTOFPROC, // Plug-in type
                        *(pRegData->guidClassID),     // Class ID of plug-in
                        cInputTypes,                  // No. media types supported by plug-in
                        mt);                          // Array of media types supported by plug-in
    }
    SAFE_FREE(mt);
    if (FAILED(hr)) return hr;
    return S_OK;
}

/*------------------------------------------------------------------------------*/

BOOL myDllMain(HANDLE hModule,
               DWORD  ul_reason_for_call,
               ClassRegData *pRegistrationData,
               UINT32 numberClassRegData)
{
    g_ClassFactories = pRegistrationData;
    g_numClassFactories = numberClassRegData;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInst = (HMODULE)hModule;
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

/*------------------------------------------------------------------------------*/

STDAPI DllCanUnloadNow(void)
{
    if (!ClassFactory::IsLocked() && (0 == g_cRefModule)) return S_OK;
    return S_FALSE;
}

/*------------------------------------------------------------------------------*/

STDAPI DllRegisterServer(void)
{
    HRESULT hr = S_OK;
    UINT32 i = 0, j = 0;

    if (!g_numClassFactories || !g_ClassFactories) return E_FAIL;
    for (i = 0; i < g_numClassFactories; ++i)
    {
        ClassRegData *pRegData = &(g_ClassFactories[i]);

        if (pRegData)
        {
            if (!pRegData->guidClassID) hr = E_FAIL;
            if (SUCCEEDED(hr))
            {
                // Register as COM object
                WCHAR fileName[MAX_PATH];
                /*DWORD res = */GetModuleFileName(g_hInst, fileName, MAX_PATH);

                hr = AMovieSetupRegisterServer(
                    *(pRegData->guidClassID),
                    pRegData->pPluginName,
                    fileName);
                if (FAILED(hr)) continue;
            }
            if (SUCCEEDED(hr))
            {
                // Register as MFT
                if (pRegData->iPluginCategory & REG_AS_AUDIO_DECODER)
                {
                    hr = RegisterAsMFT(pRegData, MFT_CATEGORY_AUDIO_DECODER);
                }
                if (pRegData->iPluginCategory & REG_AS_VIDEO_DECODER)
                {
                    hr = RegisterAsMFT(pRegData, MFT_CATEGORY_VIDEO_DECODER);
                }
                if (pRegData->iPluginCategory & REG_AS_AUDIO_ENCODER)
                {
                    hr = RegisterAsMFT(pRegData, MFT_CATEGORY_AUDIO_ENCODER);
                }
                if (pRegData->iPluginCategory & REG_AS_VIDEO_ENCODER)
                {
                    hr = RegisterAsMFT(pRegData, MFT_CATEGORY_VIDEO_ENCODER);
                }
                if (pRegData->iPluginCategory & REG_AS_AUDIO_EFFECT)
                {
                    hr = RegisterAsMFT(pRegData, MFT_CATEGORY_AUDIO_EFFECT);
                }
                if (pRegData->iPluginCategory & REG_AS_VIDEO_EFFECT)
                {
                    hr = RegisterAsMFT(pRegData, MFT_CATEGORY_VIDEO_EFFECT);
                }
                if (pRegData->iPluginCategory & REG_AS_VIDEO_PROCESSOR)
                {
                    hr = RegisterAsMFT(pRegData, MFT_CATEGORY_VIDEO_PROCESSOR);
                }
            }
            // Register as bytestream handler
            if (SUCCEEDED(hr) && pRegData->pFileExtensions)
            {
                for (j = 0; j < g_ClassFactories[i].cFileExtensions; ++j)
                {
                    hr = RegisterByteStreamHandler(
                        *(pRegData->guidClassID),
                        pRegData->pFileExtensions[j],
                        pRegData->pPluginName);
                }
            }
            // Register as WMP plugin
            if (SUCCEEDED(hr) && (g_ClassFactories[i].iPluginCategory & REG_AS_WMP_PLUGIN))
            {
                 hr = RegisterAsWMPPlugin(pRegData);
            }
            if (SUCCEEDED(hr) && g_ClassFactories[i].pDllRegisterFn) hr = g_ClassFactories[i].pDllRegisterFn();
        }
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

STDAPI DllUnregisterServer(void)
{
    HRESULT hr = S_OK;
    UINT32 i = 0;

    // Unregister COM objects
    if (g_ClassFactories)
    {
        for (i = 0; i < g_numClassFactories; ++i)
        {
            if (g_ClassFactories[i].guidClassID)
            {
                hr = AMovieSetupUnregisterServer(*(g_ClassFactories[i].guidClassID));

                if (SUCCEEDED(hr) && g_ClassFactories[i].pInputTypes)
                {
                    // NOTE: that's known MSFT issue that MFTUnregister fails;
                    // calling it just in case and ignoring status
                    /*hr =*/ MFTUnregister(*(g_ClassFactories[i].guidClassID));
                }
                // TODO: it seems that there is no code here to unregister byte stream handlers
                if (SUCCEEDED(hr) && g_ClassFactories[i].pDllUnregisterFn) hr = g_ClassFactories[i].pDllUnregisterFn();
            }
        }
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv)
{
    ClassFactory *pFactory = NULL;
    HRESULT hr = CLASS_E_CLASSNOTAVAILABLE; // Default to failure
    UINT32 i = 0;

    if (g_ClassFactories)
    {
        // Find an entry in our look-up table for the specified CLSID.
        for (i = 0; i < g_numClassFactories; ++i)
        {
            if (g_ClassFactories[i].guidClassID &&
                (*(g_ClassFactories[i].guidClassID) == clsid))
            {
                // Found an entry. Create a new class factory object.
                try
                {
                    pFactory =  new ClassFactory(&(g_ClassFactories[i]));
                }
                catch(...)
                {
                    SAFE_RELEASE(pFactory);
                }

                if (pFactory) hr = S_OK;
                else hr = E_OUTOFMEMORY;
                break;
            }
        }
        if (SUCCEEDED(hr)) hr = pFactory->QueryInterface(riid, ppv);
        SAFE_RELEASE(pFactory);
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT CreateRegistryKey(HKEY hKey, LPCTSTR subkey, HKEY *phKey)
{
    assert(phKey != NULL);

    LSTATUS res = RegCreateKeyEx(hKey, subkey,   // [in] parent key, name of subkey
        0, NULL,                                 // [in] reserved, class string (can be NULL)
        REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, // [in] options, access rights
        NULL,                                    // [in] security attributes
        phKey,                                   // [out] handled of opened/created key
        NULL);                                   // [out] receives the "disposition" (is it a new or existing key)
    return __HRESULT_FROM_WIN32(res);
}

/*------------------------------------------------------------------------------*/

HRESULT RegisterByteStreamHandler(const GUID& guid, const TCHAR *sFileExtension, const TCHAR *sDescription)
{
    HRESULT hr      = S_OK;
    HKEY    hKey    = NULL;
    HKEY    hSubKey = NULL;
    OLECHAR sCLSID[CHARS_IN_GUID];
    size_t  sDescriptionSize = 0;

    // Open HKCU/<byte stream handlers>/<file extension>
    // Create {clsid} = <description> key

    hr = StringCchLength(sDescription, STRSAFE_MAX_CCH, &sDescriptionSize);

    if (SUCCEEDED(hr)) hr = StringFromGUID2(guid, sCLSID, CHARS_IN_GUID);
    if (SUCCEEDED(hr)) hr = CreateRegistryKey(HKEY_LOCAL_MACHINE, REGKEY_MF_BYTESTREAM_HANDLERS, &hKey);
    if (SUCCEEDED(hr)) hr = CreateRegistryKey(hKey, sFileExtension, &hSubKey);
    if (SUCCEEDED(hr))
    {
        hr = RegSetValueEx(hSubKey, sCLSID, 0, REG_SZ,
            (BYTE*)sDescription, static_cast<DWORD>((sDescriptionSize + 1) * sizeof(TCHAR)));
    }
    if (NULL != hSubKey) RegCloseKey(hSubKey);
    if (NULL != hKey)    RegCloseKey(hKey);

    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT UnregisterByteStreamHandler(const GUID& /*guid*/, const TCHAR *sFileExtension)
{
    HRESULT hr  = S_OK;
    LSTATUS res = ERROR_SUCCESS;
    TCHAR sTemp[MAX_PATH];

    hr = StringCchPrintf(sTemp, MAX_PATH, TEXT("%s\\%s"), REGKEY_MF_BYTESTREAM_HANDLERS, sFileExtension);

    if (SUCCEEDED(hr))
    {
        res = RegDeleteKey(HKEY_CLASSES_ROOT, sTemp);
        if (ERROR_SUCCESS == res) hr = S_OK;
        else hr = __HRESULT_FROM_WIN32(res);
    }
    return hr;
}
