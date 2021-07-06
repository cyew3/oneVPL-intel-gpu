// Copyright (c) 2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "libmfx_core.h"
#include "mfx_error.h"

namespace mocks { namespace mfx
{
    namespace api
    { 
        struct core
            : public VideoCORE
        {
            mfxStatus GetHandle(mfxHandleType, mfxHDL*) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus SetHandle(mfxHandleType, mfxHDL) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus SetBufferAllocator(mfxBufferAllocator*) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus SetFrameAllocator(mfxFrameAllocator*) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            mfxStatus AllocBuffer(mfxU32, mfxU16, mfxMemId*) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus LockBuffer(mfxMemId, mfxU8**) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus UnlockBuffer(mfxMemId) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus FreeBuffer(mfxMemId) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            mfxStatus CheckHandle() override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus GetFrameHDL(mfxMemId, mfxHDL*, bool) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            mfxStatus AllocFrames(mfxFrameAllocRequest*, mfxFrameAllocResponse*, bool) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
#if defined (MFX_ENABLE_OPAQUE_MEMORY)
            mfxStatus AllocFrames(mfxFrameAllocRequest*, mfxFrameAllocResponse*, mfxFrameSurface1**, mfxU32) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
#endif
            mfxStatus LockFrame(mfxMemId, mfxFrameData*) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus UnlockFrame(mfxMemId, mfxFrameData*) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus FreeFrames(mfxFrameAllocResponse*, bool) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            mfxStatus LockExternalFrame(mfxMemId, mfxFrameData*, bool) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus GetExternalFrameHDL(mfxMemId, mfxHDL*, bool) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus UnlockExternalFrame(mfxMemId, mfxFrameData*, bool) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            mfxMemId MapIdx(mfxMemId) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxFrameSurface1* GetNativeSurface(mfxFrameSurface1*, bool = true) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            mfxStatus IncreaseReference(mfxFrameData*, bool) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus DecreaseReference(mfxFrameData*, bool) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus IncreasePureReference(mfxU16&) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus DecreasePureReference(mfxU16&) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            void  GetVA(mfxHDL*, mfxU16) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus CreateVA(mfxVideoParam*, mfxFrameAllocRequest*, mfxFrameAllocResponse*, UMC::FrameAllocator*) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxU32 GetAdapterNumber() override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            void  GetVideoProcessing(mfxHDL*)
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus CreateVideoProcessing(mfxVideoParam*) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            eMFXPlatform GetPlatformType() override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxU32 GetNumWorkingThreads() override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            void INeedMoreThreadsInside(const void*)
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            mfxStatus DoFastCopy(mfxFrameSurface1*, mfxFrameSurface1*) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus DoFastCopyExtended(mfxFrameSurface1*, mfxFrameSurface1*) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus DoFastCopyWrapper(mfxFrameSurface1*, mfxU16, mfxFrameSurface1*, mfxU16) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            bool IsFastCopyEnabled() override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            bool IsExternalFrameAllocator() const override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            eMFXHWType GetHWType()
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            bool SetCoreId(mfxU32) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            eMFXVAType GetVAType() const override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            mfxStatus CopyFrame(mfxFrameSurface1*, mfxFrameSurface1*) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus CopyBuffer(mfxU8*, mfxU32, mfxFrameSurface1*) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxStatus CopyFrameEx(mfxFrameSurface1*, mfxU16, mfxFrameSurface1*, mfxU16) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            mfxStatus IsGuidSupported(const GUID, mfxVideoParam*, bool) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            bool CheckOpaqueRequest(mfxFrameAllocRequest*, mfxFrameSurface1**, mfxU32, bool) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            bool IsOpaqSurfacesAlreadyMapped(mfxFrameSurface1**, mfxU32, mfxFrameAllocResponse*, bool) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            void* QueryCoreInterface(const MFX_GUID&) override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            mfxSession GetSession() override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            mfxU16 GetAutoAsyncDepth() override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

            bool IsCompatibleForOpaq() override
            { throw std::system_error(::mfx::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
        };
    }

} }
