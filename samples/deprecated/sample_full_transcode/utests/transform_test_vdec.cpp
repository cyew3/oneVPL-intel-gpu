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
#include "utest_pch.h"
#include "itransform.h"

struct TransformTest  : public ::testing::Test {
    std::auto_ptr<ITransform> transform_vd;
    SamplePtr sample_in;
    SamplePtr sample_out;
    std::auto_ptr<MockMFXVideoDECODE> decode;
    std::auto_ptr<MockSysMemFrameAllocator> allocator;
    std::auto_ptr<BaseFrameAllocator> allocatorBase;
    mfxIMPL impl;
    MockPipelineFactory factory;
    MockMFXVideoSession vSession;
    MockTransform nextTransform;
    mfxBitstream bitstream;
    mfxFrameAllocRequest request;
    mfxFrameAllocResponse response;
    mfxAllocatorParams AllocatorParams;
    std::auto_ptr<MockCHWDevice> device;

    MockSamplePool& pool;
public:
    TransformTest() :
        impl(MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_D3D11)
        , pool(*new MockSamplePool())
        , vSession(*new PipelineFactory()) {
            decode.reset(new MockMFXVideoDECODE());
            device.reset(new MockCHWDevice());
            EXPECT_CALL(factory, CreateSamplePool(_)).WillRepeatedly(Return(&pool));
            EXPECT_CALL(factory, CreateVideoDecoder(_)).WillOnce(Return(decode.get()));

            sample_in.reset(new MockSample());
            sample_out.reset(new MockSample());
            allocator.reset(new MockSysMemFrameAllocator());

            allocatorBase.reset(allocator.get());
            response.NumFrameActual = 5;
            response.mids = new mfxMemId[5];

            transform_vd.reset(new Transform<MFXVideoDECODE>(factory, vSession, 10));

            mfxVideoParam vParam = {0};

            request.NumFrameSuggested = request.NumFrameMin = 2;
            transform_vd->Configure(*new MFXAVParams(vParam), &nextTransform);
    }
    ~TransformTest() {
        MSDK_SAFE_DELETE(response.mids);
    }

    void DecodeInit(int e) {
        if (e < 0) {
            EXPECT_CALL(*decode, Init(_)).WillOnce(Return(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
        } else if (e > 0) {
            EXPECT_CALL(*decode, Init(_)).WillOnce(Return(MFX_WRN_PARTIAL_ACCELERATION));
        } else {
            EXPECT_CALL(*decode, Init(_)).WillOnce(Return(MFX_ERR_NONE));
        }
    }
    void DecodeQueryIOSurf(int e) {
        if (e < 0) {
            EXPECT_CALL(*decode, QueryIOSurf(_, _)).WillOnce(Return(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
        } else if (e > 0) {
            EXPECT_CALL(*decode, QueryIOSurf(_, _)).WillOnce(Return(MFX_WRN_PARTIAL_ACCELERATION));
        } else {
            EXPECT_CALL(*decode, QueryIOSurf(_, _)).WillOnce(Return(MFX_ERR_NONE));
        }
    }

    void GetFrameAllocator(int e) {
        if (e < 0) {
//            EXPECT_CALL(vSession, GetFrameAllocator(_)).WillOnce(DoAll(SetArgReferee<0>(allocator.get()), Return(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)));
        } else if (e > 0) {
    //        EXPECT_CALL(vSession, GetFrameAllocator(_)).WillOnce(DoAll(SetArgReferee<0>(allocator.get()), Return(MFX_WRN_PARTIAL_ACCELERATION)));
        } else {
            EXPECT_CALL(vSession, GetAllocator()).WillOnce(ReturnRef(allocatorBase));
        }
    }

    void SetFrameAllocator(int e) {
        if (e < 0) {
            EXPECT_CALL(vSession, SetFrameAllocator(_)).WillOnce(Return(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
        } else if (e > 0) {
            EXPECT_CALL(vSession, SetFrameAllocator(_)).WillOnce(Return(MFX_WRN_PARTIAL_ACCELERATION));
        } else {
            EXPECT_CALL(vSession, SetFrameAllocator(_)).WillOnce(Return(MFX_ERR_NONE));
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
    void DecodeHeader(int e) {
        if (e < 0) {
            EXPECT_CALL(*decode.get(), DecodeHeader(_, _)).WillOnce(Return(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
        } else if (e > 0) {
            EXPECT_CALL(*decode.get(), DecodeHeader(_, _)).WillOnce(Return(MFX_ERR_UNDEFINED_BEHAVIOR));
        } else {
            EXPECT_CALL(*decode.get(), DecodeHeader(_, _)).WillOnce(Return(MFX_ERR_NONE));
        }
    }
    void CreateFrameAllocator(int e) {
        if (e < 0) {
            EXPECT_CALL(factory, CreateFrameAllocator(_)).WillOnce(Return((BaseFrameAllocator *)0));
        } else {
            EXPECT_CALL(factory, CreateFrameAllocator(_)).WillOnce(Return((BaseFrameAllocator *)allocator.get()));
        }
    }
    void QueryImpl(int e) {
        if (e < 0) {
            EXPECT_CALL(vSession, QueryIMPL(_)).WillOnce(DoAll(SetArgPointee<0>(impl), Return(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)));
        } else if (e > 0) {
            EXPECT_CALL(vSession, QueryIMPL(_)).WillOnce(DoAll(SetArgPointee<0>(impl), Return(MFX_ERR_UNDEFINED_BEHAVIOR)));
        } else {
            EXPECT_CALL(vSession, QueryIMPL(_)).WillOnce(DoAll(SetArgPointee<0>(impl), Return(MFX_ERR_NONE)));
        }
    }
    void SetHandle(int e) {
        if (e < 0) {
            EXPECT_CALL(vSession, SetHandle(_, _)).WillOnce(Return(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
        } else if (e > 0) {
            EXPECT_CALL(vSession, SetHandle(_, _)).WillOnce(Return(MFX_ERR_UNDEFINED_BEHAVIOR));
        } else {
            EXPECT_CALL(vSession, SetHandle(_, _)).WillOnce(Return(MFX_ERR_NONE));
        }
    }
    void CreateAllocatorParam(int e) {
        if (e < 0) {
            EXPECT_CALL(factory, CreateAllocatorParam(_, _)).WillOnce(Return((mfxAllocatorParams *)0));
        } else {
            EXPECT_CALL(factory, CreateAllocatorParam(_, _)).WillOnce(Return(&AllocatorParams));
        }
    }
    void CreateHardwareDevice(int e) {
        if (e < 0) {
            EXPECT_CALL(factory, CreateHardwareDevice(_)).WillOnce(Return((CHWDevice*)0));
        } else {
            EXPECT_CALL(factory, CreateHardwareDevice(_)).WillOnce(Return(device.get()));
        }
    }
    void DeviceInit(int e) {
        if (e < 0) {
            EXPECT_CALL(*device.get(), Init(_, _, _)).WillOnce(Return(MFX_ERR_NULL_PTR));
        } else {
            EXPECT_CALL(*device.get(), Init(_, _, _)).WillOnce(Return(MFX_ERR_NONE));
        }
    }
    void AllocatorInit(int e) {
        if (e < 0) {
            EXPECT_CALL(*allocator, Init(_)).WillOnce(Return(MFX_ERR_NULL_PTR));
        } else {
            EXPECT_CALL(*allocator, Init(_)).WillOnce(Return(MFX_ERR_NONE));
        }
    }
    void DeviceGetHandle(int e) {
        if (e < 0) {
            EXPECT_CALL(*device.get(), GetHandle(_, _)).WillOnce(Return(MFX_ERR_NULL_PTR));
        } else {
            EXPECT_CALL(*device.get(), GetHandle(_, _)).WillOnce(Return(MFX_ERR_NONE));
        }
    }
    void NextTransformGetNumSurfaces() {
            //EXPECT_CALL(nextTransform, GetNumSurfaces(_, _)).WillOnce(DoAll(SetArgReferee<1>(request), Return()));
    }
    void DecodeFrameAsync(mfxStatus sts, mfxSyncPoint sp, int nTimes) {
        EXPECT_CALL(*decode.get(), DecodeFrameAsync(_, _, _, _)).Times(nTimes).WillRepeatedly(DoAll(SetArgPointee<3>(sp),Return(sts)));
    }
    void CompleteSuccessInit() {
        CreateHardwareDevice(0);
        AllocatorInit(0);
        DeviceGetHandle(0);
        DeviceInit(0);
        DecodeInit(0);
        AllocFrames(0);
        DecodeQueryIOSurf(0);
        SetFrameAllocator(0);
        DecodeHeader(0);
        CreateAllocatorParam(0);
        CreateFrameAllocator(0);
        QueryImpl(0);
        SetHandle(0);
        NextTransformGetNumSurfaces();
    }

    void ReleaseAll() {
        device.release();
        decode.release();
        allocatorBase.release();
        allocator.release();
    }
};

TEST_F(TransformTest, VDec_Init_DecodeHeader_failed) {
    DecodeHeader(-1);

    EXPECT_CALL(*(MockSample*)sample_in.get(), GetBitstream()).WillOnce(ReturnRef(bitstream));
    EXPECT_THROW(transform_vd->PutSample(sample_in), DecodeHeaderError);
    ReleaseAll();
}

TEST_F(TransformTest, VDec_Init_QueryIOSurf_failed) {
    DecodeHeader(0);
    GetFrameAllocator(0);

    DecodeQueryIOSurf(-1);
    EXPECT_CALL(*(MockSample*)sample_in.get(), GetBitstream()).WillOnce(ReturnRef(bitstream));
    EXPECT_THROW(transform_vd->PutSample(sample_in), DecodeQueryIOSurfaceError);
    ReleaseAll();
}

TEST_F(TransformTest, VDec_putSample_Init_Success) {
    DecodeInit(0);
    DecodeHeader(0);

    request.NumFrameSuggested = request.NumFrameMin = 3;

    EXPECT_CALL(*decode.get(), QueryIOSurf(_, _)).WillOnce(DoAll(SetArgPointee<1>(request), Return(MFX_ERR_NONE)));
    EXPECT_CALL(*(MockSample*)sample_in.get(), GetBitstream()).WillOnce(ReturnRef(bitstream));
    EXPECT_CALL(*(MockSample*)sample_in.get(), GetTrackID()).WillRepeatedly(Return(0));

    EXPECT_CALL(pool, RegisterSample(_)).WillRepeatedly(Return());
    EXPECT_CALL(pool, FindFreeSample()).WillRepeatedly(ReturnRef(*sample_in));
    EXPECT_CALL(pool, LockSample(_)).WillRepeatedly(Return((sample_out.get())));
    EXPECT_CALL(*(MockSample*)sample_out.get(), GetBitstream()).WillRepeatedly(ReturnRef(bitstream));

    //QueryImpl(0);
    //CreateFrameAllocator(0);
    //CreateHardwareDevice(0);
    AllocatorInit(0);
    DeviceGetHandle(0);
    //SetFrameAllocator(0);
    //CreateAllocatorParam(0);
    //SetHandle(0);
    GetFrameAllocator(0);
    DeviceInit(0);
    NextTransformGetNumSurfaces();
    mfxFrameAllocRequest frameAllocRequestFinal  = {};

    EXPECT_CALL(*allocator, AllocFrames(_, _)).WillOnce(DoAll(SaveArgPointee<0>(&frameAllocRequestFinal), SetArgPointee<1>(response), Return(MFX_ERR_NONE)));

    EXPECT_NO_THROW(transform_vd->PutSample(sample_in));
    //EXPECT_EQ(3 + 3, frameAllocRequestFinal.NumFrameSuggested);

    ReleaseAll();
}

TEST_F(TransformTest, DISABLED_VDec_putSample_Init_DecodeHeaderError) {
    DecodeHeader(-1);
    EXPECT_CALL(*(MockSample*)sample_in.get(), GetBitstream()).WillOnce(ReturnRef(bitstream));
    EXPECT_THROW(transform_vd->PutSample(sample_in), DecodeHeaderError);
    ReleaseAll();
}

TEST_F(TransformTest, DISABLED_VDec_putSample_Init_DecodeInitError) {
    CreateHardwareDevice(0);
    AllocatorInit(0);
    DeviceGetHandle(0);
    DeviceInit(0);
    DecodeHeader(0);
    SetFrameAllocator(0);
    CreateAllocatorParam(0);
    CreateFrameAllocator(0);
    QueryImpl(0);
    SetHandle(0);
    DecodeQueryIOSurf(0);
    DecodeInit(-1);
    EXPECT_CALL(*(MockSample*)sample_in.get(), GetBitstream()).WillOnce(ReturnRef(bitstream));
    EXPECT_THROW(transform_vd->PutSample(sample_in), DecodeInitError);
    ReleaseAll();
}


TEST_F(TransformTest, DISABLED_VDec_putSample_DecodeFrameAsync_Device_Busy) {
    CompleteSuccessInit();
    EXPECT_CALL(*(MockSample*)sample_in.get(), GetBitstream()).WillRepeatedly(ReturnRef(bitstream));
    EXPECT_CALL(*(MockSample*)sample_in.get(), GetMetaData(_, _)).WillRepeatedly(Return(false));
    EXPECT_CALL(*(MockSample*)sample_in.get(), GetTrackID()).WillRepeatedly(Return(0));

    EXPECT_CALL(pool, RegisterSample(_)).WillRepeatedly(Return());
    EXPECT_CALL(pool, FindFreeSample()).WillRepeatedly(ReturnRef(*sample_in));
    EXPECT_CALL(pool, LockSample(_)).WillRepeatedly(Return((sample_out.get())));
//    EXPECT_CALL(*(MockSample*)sample_out.get(), GetFreeSurface()).WillRepeatedly(ReturnRef(surface));

    transform_vd->PutSample(sample_in);
    DecodeFrameAsync(MFX_WRN_DEVICE_BUSY, (mfxSyncPoint)0, 2);
    EXPECT_THROW(transform_vd->GetSample(sample_in), DecodeTimeoutError);
    ReleaseAll();
}

TEST_F(TransformTest, DISABLED_VDec_putSample_DecodeFrameAsync_EOS) {
    CompleteSuccessInit();
    EXPECT_CALL(*(MockSample*)sample_in.get(), GetBitstream()).WillRepeatedly(ReturnRef(bitstream));
    EXPECT_CALL(*(MockSample*)sample_in.get(), GetMetaData(META_EOS, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*(MockSample*)sample_in.get(), GetTrackID()).WillRepeatedly(Return(0));
    transform_vd->PutSample(sample_in);
    EXPECT_CALL(*decode.get(), DecodeFrameAsync(0, _, _, _)).Times(1).WillRepeatedly(DoAll(SetArgPointee<3>((mfxSyncPoint)1),Return(MFX_ERR_MORE_DATA)));
    EXPECT_EQ(true, transform_vd->GetSample(sample_in));
    EXPECT_CALL(*decode.get(), DecodeFrameAsync(0, _, _, _)).Times(1).WillRepeatedly(DoAll(SetArgPointee<3>((mfxSyncPoint)1),Return(MFX_ERR_MORE_DATA)));
    EXPECT_EQ(false, transform_vd->GetSample(sample_in));
    ReleaseAll();
}

TEST_F(TransformTest, DISABLED_VDec_putSample_DecodeFrameAsync_More_Data) {
    CompleteSuccessInit();
    EXPECT_CALL(*(MockSample*)sample_in.get(), GetBitstream()).WillRepeatedly(ReturnRef(bitstream));
    transform_vd->PutSample(sample_in);
    DecodeFrameAsync(MFX_ERR_MORE_DATA, (mfxSyncPoint)0, 1);
    EXPECT_EQ(false, transform_vd->GetSample(sample_in));
    ReleaseAll();
}
