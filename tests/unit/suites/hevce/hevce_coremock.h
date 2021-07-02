// Copyright (c) 2020-2021 Intel Corporation
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

#include "mfx_common.h"

namespace hevce { namespace tests
{
class CoreMock
    : public VideoCORE
{
public:
    CoreMock(eMFXHWType hw = MFX_HW_UNKNOWN) : m_hw(hw) {}

    eMFXHWType m_hw;

    virtual mfxStatus GetHandle(mfxHandleType, mfxHDL*) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus SetHandle(mfxHandleType, mfxHDL) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus SetBufferAllocator(mfxBufferAllocator*) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus SetFrameAllocator(mfxFrameAllocator*) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus AllocBuffer(mfxU32, mfxU16, mfxMemId*) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus LockBuffer(mfxMemId, mfxU8**) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus UnlockBuffer(mfxMemId) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus FreeBuffer(mfxMemId) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus CheckHandle() override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus GetFrameHDL(mfxMemId, mfxHDL*, bool) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest*, mfxFrameAllocResponse*, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  LockFrame(mfxMemId, mfxFrameData*) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  UnlockFrame(mfxMemId, mfxFrameData*) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  FreeFrames(mfxFrameAllocResponse*, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  LockExternalFrame(mfxMemId, mfxFrameData*, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  GetExternalFrameHDL(mfxMemId, mfxHDL*, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  UnlockExternalFrame(mfxMemId, mfxFrameData*, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxMemId MapIdx(mfxMemId) override { return nullptr; }
    virtual mfxFrameSurface1* GetNativeSurface(mfxFrameSurface1*, bool = true) override { return nullptr; }
    virtual mfxStatus  IncreaseReference(mfxFrameData*, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  DecreaseReference(mfxFrameData*, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus IncreasePureReference(mfxU16&) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus DecreasePureReference(mfxU16&) override { return MFX_ERR_UNSUPPORTED; }
    virtual void  GetVA(mfxHDL*, mfxU16) override {  }
    virtual mfxStatus CreateVA(mfxVideoParam*, mfxFrameAllocRequest*, mfxFrameAllocResponse*, UMC::FrameAllocator*) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxU32 GetAdapterNumber(void) override { return 0; }
    virtual void  GetVideoProcessing(mfxHDL*) {}
    virtual mfxStatus CreateVideoProcessing(mfxVideoParam*) override { return MFX_ERR_UNSUPPORTED; }
    virtual eMFXPlatform GetPlatformType() override { return MFX_PLATFORM_HARDWARE; }
    virtual mfxU32 GetNumWorkingThreads(void) override { return 1; }
    virtual void INeedMoreThreadsInside(const void*) {}
    virtual mfxStatus DoFastCopy(mfxFrameSurface1*, mfxFrameSurface1*) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1*, mfxFrameSurface1*) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1*, mfxU16, mfxFrameSurface1*, mfxU16) override { return MFX_ERR_UNSUPPORTED; }
    virtual bool IsFastCopyEnabled(void) override { return false; }
    virtual bool IsExternalFrameAllocator(void) const override { return false; }

    virtual eMFXHWType   GetHWType() { return m_hw; };

    virtual bool         SetCoreId(mfxU32) override { return false; }
    virtual eMFXVAType   GetVAType() const override
    {
#if defined(MFX_VA_WIN)
        return MFX_HW_D3D11;
#elif defined(MFX_VA_LINUX)
        return MFX_HW_VAAPI;
#endif
    }

    virtual mfxStatus CopyFrame(mfxFrameSurface1*, mfxFrameSurface1*) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus CopyBuffer(mfxU8*, mfxU32, mfxFrameSurface1*) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus CopyFrameEx(mfxFrameSurface1*, mfxU16, mfxFrameSurface1*, mfxU16) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus IsGuidSupported(const GUID, mfxVideoParam*, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual bool CheckOpaqueRequest(mfxFrameAllocRequest*, mfxFrameSurface1**, mfxU32, bool) override { return false; }
    virtual bool IsOpaqSurfacesAlreadyMapped(mfxFrameSurface1**, mfxU32, mfxFrameAllocResponse*, bool) override { return false; }
    virtual void* QueryCoreInterface(const MFX_GUID&) override { return nullptr; };
    virtual mfxSession GetSession() override { return nullptr; }
    virtual void SetWrapper(void*) override {}
    virtual mfxU16 GetAutoAsyncDepth() override { return 0; }
    virtual bool IsCompatibleForOpaq() override { return false; }
};
}}