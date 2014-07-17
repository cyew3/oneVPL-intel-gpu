//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "utest_pch.h"

#include "itransform.h"

using namespace ::testing;
#if defined(_WIN32) || defined(_WIN64)

class TransformVVppInited : public Transform<MFXVideoVPP> {
public:
    TransformVVppInited(PipelineFactory& factory, MFXVideoSessionExt & session, int TimeToWait) :
        Transform<MFXVideoVPP>(factory, session, TimeToWait)
    {
        m_bInited = 1;
    }
};

struct TransformTestVVPP  : public ::testing::Test {
    std::auto_ptr<ITransform> transform_vv;
    SamplePtr sample1, sample2, sample3;

    mfxFrameSurface1 surf1, surf2, surf3;
    std::auto_ptr<MockMFXVideoVPP> vpp;

    std::auto_ptr<MockMFXFrameAllocator> allocator;
    mfxFrameAllocRequest allocRequest;
    mfxFrameAllocResponse response;
    mfxIMPL impl;
    MockPipelineFactory factory, factory2;
    MockMFXVideoSession vSession, vSession2;
    MockTransform nextTransform;
    mfxVideoParam vParam;
public:

    TransformTestVVPP() :
        impl(MFX_IMPL_HARDWARE_ANY)
        , vSession(factory)
        , vSession2(factory2){
        vpp.reset(new MockMFXVideoVPP());

        allocator.reset(new MockMFXFrameAllocator());
        EXPECT_CALL(factory, CreateVideoVPP(_)).WillOnce(Return(vpp.get()));
        transform_vv.reset(new Transform<MFXVideoVPP>(factory, vSession, 10));

        sample1.reset(new MockSample());
        sample2.reset(new MockSample());
        sample3.reset(new MockSample());

        memset(&vParam, 0, sizeof(vParam));
        vParam.AsyncDepth = 2;
        vParam.mfx.BufferSizeInKB = 1000;

        //EXPECT_CALL(nextTransform, GetNumSurfaces(_, _)).WillOnce(DoAll(SetArgReferee<1>(allocRequest), Return()));
        transform_vv->Configure(MFXAVParams(vParam), &nextTransform);

        allocRequest.NumFrameMin = 2;
        allocRequest.NumFrameSuggested = 2;

        response.NumFrameActual = 2;
        response.mids = new mfxMemId[response.NumFrameActual];
        EXPECT_CALL(*(MockSample*)sample1.get(), GetSurface()).WillRepeatedly(ReturnRef(surf1));
    }
    ~TransformTestVVPP() {
        delete[] response.mids;
    }
    void VPPInit(int e) {
        if (e < 0) {
            EXPECT_CALL(*vpp, Init(_)).WillOnce(Return(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
        } else if (e > 0) {
            EXPECT_CALL(*vpp, Init(_)).WillOnce(Return(MFX_WRN_PARTIAL_ACCELERATION));
        } else {
            EXPECT_CALL(*vpp, Init(_)).WillOnce(Return(MFX_ERR_NONE));
        }
    }
    void VPPQueryIOSurf(int e) {
        if (e < 0) {
            EXPECT_CALL(*vpp, QueryIOSurf(_, _)).WillOnce(Return(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
        } else if (e > 0) {
            EXPECT_CALL(*vpp, QueryIOSurf(_, _)).WillOnce(Return(MFX_WRN_PARTIAL_ACCELERATION));
        } else {
            EXPECT_CALL(*vpp, QueryIOSurf(_, _)).WillOnce(Return(MFX_ERR_NONE));
        }
    }

    void GetFrameAllocator(int e) {
        if (e < 0) {
            EXPECT_CALL(vSession, GetFrameAllocator(_)).WillOnce(DoAll(SetArgReferee<0>(allocator.get()), Return(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)));
        } else if (e > 0) {
            EXPECT_CALL(vSession, GetFrameAllocator(_)).WillOnce(DoAll(SetArgReferee<0>(allocator.get()), Return(MFX_WRN_PARTIAL_ACCELERATION)));
        } else {
            EXPECT_CALL(vSession, GetFrameAllocator(_)).WillOnce(DoAll(SetArgReferee<0>(allocator.get()), Return(MFX_ERR_NONE)));
        }
    }

    void AllocFrames(int e) {
        if (e < 0) {
            EXPECT_CALL(*allocator.get(), AllocFrames(_, _)).WillOnce(DoAll(SetArgPointee<1>(response), Return(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)));
        } else if (e > 0) {
            EXPECT_CALL(*allocator.get(), AllocFrames(_, _)).WillOnce(DoAll(SetArgPointee<1>(response), Return(MFX_ERR_UNDEFINED_BEHAVIOR)));
        } else {
            EXPECT_CALL(*allocator.get(), AllocFrames(_, _)).WillOnce(DoAll(SetArgPointee<1>(response), Return(MFX_ERR_NONE)));
        }
    }
    void CompleteSuccessInit() {
        VPPInit(0);
        VPPQueryIOSurf(0);
        GetFrameAllocator(0);
        AllocFrames(0);
    }

};
#if 0
TEST_F(TransformTestVVPP, PutSample_VPPInitError) {
    VPPInit(-1);
    EXPECT_THROW(transform_vv->PutSample(sample1), VPPInitError);
    vpp.release();
}

TEST_F(TransformTestVVPP, PutSample_GetFrameAllocatorError) {
    VPPInit(0);
    VPPQueryIOSurf(0);
    GetFrameAllocator(-1);
    EXPECT_THROW(transform_vv->PutSample(sample1), VPPGetFrameAllocatorError);
    vpp.release();
}


TEST_F(TransformTestVVPP, GetSample_Without_PutSample_AllocError) {
    VPPInit(0);
    VPPQueryIOSurf(0);
    GetFrameAllocator(0);
    AllocFrames(-1);
    EXPECT_THROW(transform_vv->PutSample(sample1), VPPAllocError);
    vpp.release();
}

TEST_F(TransformTestVVPP, GetSample_NotInitedError) {
    EXPECT_EQ(false, transform_vv->GetSample(sample1));
    vpp.release();
}

TEST_F(TransformTestVVPP, GetSample_RunVPP_VPPOneTime_NoError) {
    CompleteSuccessInit();
    EXPECT_NO_THROW(transform_vv->PutSample(sample1));

    EXPECT_CALL(*vpp, RunFrameVPPAsync(_, _, _, _)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_EQ(true, transform_vv->GetSample(sample2));
    vpp.release();
}

TEST_F(TransformTestVVPP, GetSample_RunVPP_DeviseBusy) {
    CompleteSuccessInit();
    EXPECT_NO_THROW(transform_vv->PutSample(sample1));
    Sequence s1;
    EXPECT_CALL(*vpp, RunFrameVPPAsync(_, _, _, _)).InSequence(s1).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)0), Return(MFX_WRN_DEVICE_BUSY)));
    EXPECT_CALL(*vpp, RunFrameVPPAsync(_, _, _, _)).InSequence(s1).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)1), Return(MFX_ERR_NONE)));
    EXPECT_EQ(true, transform_vv->GetSample(sample2));
    vpp.release();
}

TEST_F(TransformTestVVPP, GetSample_RunVPP_MoreSurface) {
    CompleteSuccessInit();
    EXPECT_NO_THROW(transform_vv->PutSample(sample1));

    EXPECT_CALL(*vpp, RunFrameVPPAsync(_, _, _, _)).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)1), Return(MFX_ERR_MORE_SURFACE)));
    EXPECT_EQ(true, transform_vv->GetSample(sample2));

    EXPECT_CALL(*vpp, RunFrameVPPAsync(_, _, _, _)).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)1), Return(MFX_ERR_NONE)));
    EXPECT_EQ(true, transform_vv->GetSample(sample2));

    EXPECT_EQ(false, transform_vv->GetSample(sample2));
    vpp.release();
}

TEST_F(TransformTestVVPP, GetSample_RunVPP_AnyOtherError) {
    CompleteSuccessInit();
    EXPECT_NO_THROW(transform_vv->PutSample(sample1));

    EXPECT_CALL(*vpp, RunFrameVPPAsync(_, _, _, _)).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)1), Return((mfxStatus)-10000)));
    EXPECT_THROW(transform_vv->GetSample(sample2), VPPRunFrameVPPAsyncError);
    vpp.release();
}

TEST_F(TransformTestVVPP, GetSample_RunVPP_WarningNoSyncPoint) {
    CompleteSuccessInit();
    EXPECT_NO_THROW(transform_vv->PutSample(sample1));

    EXPECT_CALL(*vpp, RunFrameVPPAsync(_, _, _, _)).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)0), Return((mfxStatus)10000)));
    EXPECT_THROW(transform_vv->GetSample(sample2), VPPRunFrameVPPAsyncError);
    vpp.release();
}

TEST_F(TransformTestVVPP, GetSample_RunVPP_WarningWithSyncPoint) {
    CompleteSuccessInit();
    EXPECT_NO_THROW(transform_vv->PutSample(sample1));

    EXPECT_CALL(*vpp, RunFrameVPPAsync(_, _, _, _)).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)1), Return((mfxStatus)10000)));
    EXPECT_EQ(true, transform_vv->GetSample(sample2));
    vpp.release();
}

TEST_F(TransformTestVVPP, GetNumSurfacesThrowError) {
    EXPECT_CALL(*vpp, QueryIOSurf(_, _)).WillOnce(Return(MFX_ERR_NOT_INITIALIZED));
    EXPECT_THROW(transform_vv->GetNumSurfaces(vParam, allocRequest), VPPQueryIOSurfError);
    vpp.release();
}

TEST_F(TransformTestVVPP, GetNumSurfacesErr_None) {
    EXPECT_CALL(*vpp, QueryIOSurf(_, _)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_NO_THROW(transform_vv->GetNumSurfaces(vParam, allocRequest));
    vpp.release();
}

TEST_F(TransformTestVVPP, GetNumSurfacesWarning) {
    EXPECT_CALL(*vpp, QueryIOSurf(_, _)).WillOnce(Return(MFX_WRN_VIDEO_PARAM_CHANGED));
    EXPECT_NO_THROW(transform_vv->GetNumSurfaces(vParam, allocRequest));
    vpp.release();
}
#endif

#endif // #if defined(_WIN32) || defined(_WIN64)
