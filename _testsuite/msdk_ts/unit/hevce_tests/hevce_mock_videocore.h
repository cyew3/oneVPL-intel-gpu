#pragma once

#include "gmock/gmock.h"

#include "mfxvideo++int.h"

namespace ApiTestCommon {
    struct ParamSet;

    class MockVideoCORE : public VideoCORE {
    public:
        MOCK_METHOD2(GetHandle, mfxStatus (mfxHandleType type, mfxHDL *handle));
        MOCK_METHOD2(SetHandle, mfxStatus (mfxHandleType type, mfxHDL handle));
        MOCK_METHOD1(SetBufferAllocator, mfxStatus (mfxBufferAllocator *allocator));
        MOCK_METHOD1(SetFrameAllocator, mfxStatus (mfxFrameAllocator *allocator));
        MOCK_METHOD3(AllocBuffer, mfxStatus (mfxU32 nbytes, mfxU16 type, mfxMemId *mid));
        MOCK_METHOD2(LockBuffer, mfxStatus (mfxMemId mid, mfxU8 **ptr));
        MOCK_METHOD1(UnlockBuffer, mfxStatus (mfxMemId mid));
        MOCK_METHOD1(FreeBuffer, mfxStatus (mfxMemId mid));
        MOCK_METHOD0(CheckHandle, mfxStatus ());
        MOCK_METHOD3(GetFrameHDL, mfxStatus (mfxMemId mid, mfxHDL *handle, bool ExtendedSearch));
        MOCK_METHOD3(AllocFrames, mfxStatus (mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool isNeedCopy));
        MOCK_METHOD4(AllocFrames, mfxStatus (mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxFrameSurface1 **pOpaqueSurface, mfxU32 NumOpaqueSurface));
        MOCK_METHOD2(LockFrame, mfxStatus (mfxMemId mid, mfxFrameData *ptr));
        MOCK_METHOD2(UnlockFrame, mfxStatus (mfxMemId mid, mfxFrameData *ptrs));
        MOCK_METHOD2(FreeFrames, mfxStatus (mfxFrameAllocResponse *response, bool ExtendedSearch));
        MOCK_METHOD3(LockExternalFrame, mfxStatus (mfxMemId mid, mfxFrameData *ptr, bool ExtendedSearch));
        MOCK_METHOD3(GetExternalFrameHDL, mfxStatus (mfxMemId mid, mfxHDL *handle, bool ExtendedSearch));
        MOCK_METHOD3(UnlockExternalFrame, mfxStatus (mfxMemId mid, mfxFrameData *ptr, bool ExtendedSearch));
        MOCK_METHOD1(MapIdx, mfxMemId (mfxMemId mid));
        MOCK_METHOD2(GetNativeSurface, mfxFrameSurface1* (mfxFrameSurface1 *pOpqSurface, bool ExtendedSearch));
        MOCK_METHOD2(GetOpaqSurface, mfxFrameSurface1* (mfxMemId mid, bool ExtendedSearch));
        MOCK_METHOD2(IncreaseReference, mfxStatus (mfxFrameData *ptr, bool ExtendedSearch));
        MOCK_METHOD2(DecreaseReference, mfxStatus (mfxFrameData *ptr, bool ExtendedSearch));
        MOCK_METHOD1(IncreasePureReference, mfxStatus (mfxU16 &));
        MOCK_METHOD1(DecreasePureReference, mfxStatus (mfxU16 &));
        MOCK_METHOD2(GetVA, void (mfxHDL* phdl, mfxU16 type));
        MOCK_METHOD4(CreateVA, mfxStatus (mfxVideoParam * , mfxFrameAllocRequest *, mfxFrameAllocResponse *, UMC::FrameAllocator *));
        MOCK_METHOD0(GetAdapterNumber, mfxU32 (void));
        MOCK_METHOD1(GetVideoProcessing, void (mfxHDL* phdl));
        MOCK_METHOD1(CreateVideoProcessing, mfxStatus (mfxVideoParam *));
        MOCK_METHOD0(GetPlatformType, eMFXPlatform ());
        MOCK_METHOD0(GetNumWorkingThreads, mfxU32 (void));
        MOCK_METHOD1(INeedMoreThreadsInside, void (const void *pComponent));
        MOCK_METHOD2(DoFastCopy, mfxStatus (mfxFrameSurface1 *dst, mfxFrameSurface1 *src));
        MOCK_METHOD2(DoFastCopyExtended, mfxStatus (mfxFrameSurface1 *dst, mfxFrameSurface1 *src));
        MOCK_METHOD4(DoFastCopyWrapper, mfxStatus (mfxFrameSurface1 *dst, mfxU16 dstMemType, mfxFrameSurface1 *src, mfxU16 srcMemType));
        MOCK_METHOD0(IsFastCopyEnabled, bool (void));
        MOCK_CONST_METHOD0(IsExternalFrameAllocator, bool (void));
        MOCK_METHOD0(GetHWType, eMFXHWType ());
        MOCK_METHOD1(SetCoreId, bool (mfxU32 Id));
        MOCK_CONST_METHOD0(GetVAType, eMFXVAType ());
        MOCK_METHOD2(CopyFrame, mfxStatus (mfxFrameSurface1 *dst, mfxFrameSurface1 *src));
        MOCK_METHOD3(CopyBuffer, mfxStatus (mfxU8 *dst, mfxU32 dst_size, mfxFrameSurface1 *src));
        MOCK_METHOD4(CopyFrameEx, mfxStatus (mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType));
        MOCK_METHOD3(IsGuidSupported, mfxStatus (const GUID guid, mfxVideoParam *par, bool isEncoder));
        MOCK_METHOD4(CheckOpaqueRequest, bool (mfxFrameAllocRequest *request, mfxFrameSurface1 **pOpaqueSurface, mfxU32 NumOpaqueSurface, bool ExtendedSearch));
        MOCK_METHOD4(IsOpaqSurfacesAlreadyMapped, bool (mfxFrameSurface1 **pOpaqueSurface, mfxU32 NumOpaqueSurface, mfxFrameAllocResponse *response, bool ExtendedSearch));
        MOCK_METHOD1(QueryCoreInterface, void* (const MFX_GUID &guid));
        MOCK_METHOD0(GetSession, mfxSession ());
        MOCK_METHOD1(SetWrapper, void (void* pWrp));
        MOCK_METHOD0(GetAutoAsyncDepth, mfxU16 ());
        MOCK_METHOD0(IsCompatibleForOpaq, bool ());

        MockVideoCORE();
        void SetParamSet(const ParamSet &param);
        mfxStatus AllocFramesImpl1(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool);
        mfxStatus AllocFramesImpl2(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxFrameSurface1 **pOpaqueSurface, mfxU32 NumOpaqueSurface);
        mfxStatus FreeFramesImpl(mfxFrameAllocResponse *response, bool);

        const ParamSet *paramset;
        mfxMemId memIds[10];
    };
};
