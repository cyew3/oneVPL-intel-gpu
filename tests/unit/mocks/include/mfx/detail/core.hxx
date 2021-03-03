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

namespace mocks { namespace mfx
{
    namespace api
    { 
        struct core
            : public VideoCORE
        {
            mfxStatus GetHandle(mfxHandleType, mfxHDL*) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus SetHandle(mfxHandleType, mfxHDL) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus SetBufferAllocator(mfxBufferAllocator*) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus SetFrameAllocator(mfxFrameAllocator*) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            mfxStatus AllocBuffer(mfxU32, mfxU16, mfxMemId*) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus LockBuffer(mfxMemId, mfxU8**) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus UnlockBuffer(mfxMemId) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus FreeBuffer(mfxMemId) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            mfxStatus CheckHandle() override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus GetFrameHDL(mfxMemId, mfxHDL*, bool) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            mfxStatus AllocFrames(mfxFrameAllocRequest*, mfxFrameAllocResponse*, bool) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus AllocFrames(mfxFrameAllocRequest*, mfxFrameAllocResponse*, mfxFrameSurface1**, mfxU32) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus LockFrame(mfxMemId, mfxFrameData*) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus UnlockFrame(mfxMemId, mfxFrameData*) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus FreeFrames(mfxFrameAllocResponse*, bool) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            mfxStatus LockExternalFrame(mfxMemId, mfxFrameData*, bool) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus GetExternalFrameHDL(mfxMemId, mfxHDL*, bool) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus UnlockExternalFrame(mfxMemId, mfxFrameData*, bool) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            mfxMemId MapIdx(mfxMemId) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxFrameSurface1* GetNativeSurface(mfxFrameSurface1*, bool = true) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxFrameSurface1* GetOpaqSurface(mfxMemId, bool = true) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            mfxStatus IncreaseReference(mfxFrameData*, bool) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus DecreaseReference(mfxFrameData*, bool) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus IncreasePureReference(mfxU16&) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus DecreasePureReference(mfxU16&) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            void  GetVA(mfxHDL*, mfxU16) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus CreateVA(mfxVideoParam*, mfxFrameAllocRequest*, mfxFrameAllocResponse*, UMC::FrameAllocator*) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxU32 GetAdapterNumber() override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            void  GetVideoProcessing(mfxHDL*)
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus CreateVideoProcessing(mfxVideoParam*) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            eMFXPlatform GetPlatformType() override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxU32 GetNumWorkingThreads() override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            void INeedMoreThreadsInside(const void*)
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            mfxStatus DoFastCopy(mfxFrameSurface1*, mfxFrameSurface1*) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus DoFastCopyExtended(mfxFrameSurface1*, mfxFrameSurface1*) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus DoFastCopyWrapper(mfxFrameSurface1*, mfxU16, mfxFrameSurface1*, mfxU16) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            bool IsFastCopyEnabled() override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            bool IsExternalFrameAllocator() const override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            eMFXHWType GetHWType()
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            bool SetCoreId(mfxU32) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            eMFXVAType   GetVAType() const override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            mfxStatus CopyFrame(mfxFrameSurface1*, mfxFrameSurface1*) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus CopyBuffer(mfxU8*, mfxU32, mfxFrameSurface1*) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxStatus CopyFrameEx(mfxFrameSurface1*, mfxU16, mfxFrameSurface1*, mfxU16) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            mfxStatus IsGuidSupported(const GUID, mfxVideoParam*, bool) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            bool CheckOpaqueRequest(mfxFrameAllocRequest*, mfxFrameSurface1**, mfxU32, bool) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            bool IsOpaqSurfacesAlreadyMapped(mfxFrameSurface1**, mfxU32, mfxFrameAllocResponse*, bool) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            void* QueryCoreInterface(const MFX_GUID&) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
            mfxSession GetSession() override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            void SetWrapper(void*) override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            mfxU16 GetAutoAsyncDepth() override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }

            bool IsCompatibleForOpaq() override
            { throw std::system_error(E_NOTIMPL, std::system_category()); }
        };
    }

} }
