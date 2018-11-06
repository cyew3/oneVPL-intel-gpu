/* ****************************************************************************** *\

Copyright (C) 2018 Intel Corporation.  All rights reserved.

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

File Name: mfx_dxloader.h

\* ****************************************************************************** */
#pragma once

#ifndef __MFX_DXLOADER_H
#define __MFX_DXLOADER_H

#if !defined(OPEN_SOURCE) && defined(MEDIASDK_DX_LOADER)
#include <d3d11.h>
#include <cstdint>
#include <mfxdefs.h>

namespace MFX
{
    static const GUID MediaSDK_GUID =
    { 0xb69c20e0, 0x2508, 0x8790,{ 0x03, 0x05, 0x87, 0x54, 0x99, 0xe0, 0xa2, 0xd0 } };

    class MfxDxLoader
    {
        ID3D11Device        *pDevice;
        ID3D11DeviceContext *pContext;
        ID3D11VideoDevice   *pVDevice;
        ID3D11VideoContext  *pVContext;
        ID3D11VideoDecoder  *pDecoder;
        IDXGIAdapter1       *pAdapter;
        GUID                decGUID;

        mfxStatus FindAdapter(const mfxU32 adapterNum);
        mfxStatus InitDevice(void);
        mfxStatus InitDecoder(void);
        void ReleaseDevice(void);
        bool isMediaSDKGUID(void) { return memcmp(&decGUID, &MediaSDK_GUID, sizeof(GUID)) == 0; }
        MfxDxLoader() :
            pDevice(NULL)
            , pContext(NULL)
            , pVDevice(NULL)
            , pVContext(NULL)
            , pDecoder(NULL)
            , pAdapter(NULL)
        {
            memset(&decGUID, 0, sizeof(GUID));
        }

    public:
        ~MfxDxLoader()
        {
            ReleaseDevice();
        }
        mfxStatus Init(const mfxU32 adapterNum);
        mfxStatus GetEntryPoints(void* table, mfxU32 *tableSize);
        mfxStatus GetEntryPoint(void** entrypoint, const LPCSTR entryName);
        bool IsInitialized(void) { return pDecoder != NULL; }

        /* Not correct singleton, need a map or smthng like this */
        static MfxDxLoader* GetMfxDxLoader();
    };
}
#endif // !defined(OPEN_SOURCE) && defined(MEDIASDK_DX_LOADER)

#endif // !defined(__MFX_DXLOADER_H)
