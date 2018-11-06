/*******************************************************************************

Copyright (C) 2017-2018 Intel Corporation.  All rights reserved.

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

File Name: intel_api_factory.cpp

*******************************************************************************/

#include "pch.h"
#include "intel_api_factory.h"

namespace {

    #include <DelayImp.h>

    void SehHandler(PEXCEPTION_POINTERS pExcPointers) {

        if (pExcPointers->ExceptionRecord->ExceptionCode == (ERROR_SEVERITY_ERROR | (FACILITY_VISUALCPP << 16) | ERROR_MOD_NOT_FOUND)) {
            const auto & pDelayLoadInfo = *PDelayLoadInfo(pExcPointers->ExceptionRecord->ExceptionInformation[0]);
            OutputDebugStringA(pDelayLoadInfo.szDll);
        }
    }

    HRESULT HandleSeh()
    {
        HRESULT hr = E_FAIL;

        __try {
            throw;
        }
        __except (SehHandler(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {

            auto code = GetExceptionCode();

            switch (code) {
            case EXCEPTION_ACCESS_VIOLATION:
                OutputDebugStringA(": Access violation");
                hr = E_ACCESSDENIED;
                break;
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
                OutputDebugStringA(": Integer division by zero");
                hr = STATUS_INTEGER_DIVIDE_BY_ZERO;
                break;
            case EXCEPTION_STACK_OVERFLOW:
                OutputDebugStringA(": Stack overflow");
                hr = STATUS_STACK_OVERFLOW;
                break;
            case (ERROR_SEVERITY_ERROR | (FACILITY_VISUALCPP << 16) | ERROR_MOD_NOT_FOUND):
                OutputDebugStringA(": Module not found: ");
                hr = HRESULT_FROM_WIN32(ERROR_DELAY_LOAD_FAILED);

                break;
            default:
                OutputDebugStringA(": Operation Aborted");
            }
        }

        return hr;
    }

    #include "mfx_engine_loader.h"
}


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    HRESULT APIENTRY InitialiseMediaSession(HANDLE* handle, LPVOID lpParam, LPVOID lpReserved)
    {
        HRESULT hr = E_NOTIMPL;
        (void)lpReserved;

        try {

            if (nullptr == lpParam) {
                return E_INVALIDARG;
            }

            mfxSession session = nullptr;
            const mfxInitParam& par = *(mfxInitParam*)(lpParam);
            mfxStatus err = InitMediaSDKSession(par, &session);

            if (MFX_ERR_NONE != err) {
                return  HRESULT_FROM_WIN32(err);
            }

            if (nullptr == session) {
                return E_FAIL;
            }

            *handle = reinterpret_cast<HANDLE>(session);
            hr = S_OK;

        }
        catch (std::bad_alloc& mem) {

            OutputDebugStringA(mem.what());
            hr = HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
        }
        catch (std::exception& last) {

            OutputDebugStringA(last.what());
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
        catch (...) {

            hr = HandleSeh();
        }
        return hr;
    }

    HRESULT APIENTRY DisposeMediaSession(const HANDLE handle)
    {
        HRESULT hr = E_NOTIMPL;

        if (nullptr == handle) {
            return E_UNEXPECTED;
        }

        mfxStatus err = DisposeMediaSDKSession(reinterpret_cast<mfxSession>(handle));
        hr = (MFX_ERR_NONE == err) ? S_OK : HRESULT_FROM_WIN32(err);

        return hr;
    }

#ifdef __cplusplus
}
#endif /* __cplusplus */
