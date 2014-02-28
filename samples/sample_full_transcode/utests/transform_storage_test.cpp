//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "utest_pch.h"
#include "transform_storage.h"

class TransformStorageTest : public Test {
public:
    TransformStorageTest() :
    mk_trans1(*new MockTransform()),
    mk_trans2(*new MockTransform()),
    mk_trans3(*new MockTransform()){}
 
protected:
    class ConfigureHolder {
    public:
        void OnConfigure(MFXAVParams param, ITransform *t) {
            m_configparam.reset(new MFXAVParams(param));
            m_configTransform = t;
        }

        const mfxVideoParam & ConfigParamVideo () {
            return m_configparam->GetVideoParam();
        }
        const mfxAudioParam & ConfigParamAudio() {
            return m_configparam->GetAudioParam();
        }
        ITransform * ConfigTransform() {
            return m_configTransform;
        }
    protected:
        std::auto_ptr<MFXAVParams> m_configparam;
        ITransform *m_configTransform;
    };

    ConfigureHolder c1, c2;
    TransformStorage storage;
    MockTransform &mk_trans1;
    MockTransform &mk_trans2;
    MockTransform &mk_trans3;
};


TEST_F(TransformStorageTest, empty_if_not_added) {
    
    EXPECT_EQ(true, storage.empty(0));
}

TEST_F(TransformStorageTest, not_empty_if_added) {

    storage.RegisterTransform(TransformConfigDesc(0, mfxVideoParam()),instant_auto_ptr2<ITransform>( new MockTransform()) );

    EXPECT_EQ(false, storage.empty(0));
    EXPECT_EQ(true, storage.empty(1));
}

TEST_F(TransformStorageTest, 2_tracks) {

    storage.RegisterTransform(TransformConfigDesc(0, mfxVideoParam()),instant_auto_ptr<ITransform>(&mk_trans1));
    storage.RegisterTransform(TransformConfigDesc(0, mfxVideoParam()),instant_auto_ptr<ITransform>( &mk_trans2));
    storage.RegisterTransform(TransformConfigDesc(1, mfxVideoParam()),instant_auto_ptr<ITransform>( &mk_trans3));

    TransformStorage::iterator i = storage.begin(0);
    i++;
    i++;
    EXPECT_EQ(storage.end(0), i);

    TransformStorage::iterator i1 = storage.begin(1);
    i1++;
    EXPECT_EQ(storage.end(1), i1);
}

TEST_F(TransformStorageTest, 1_track_1_transform_expect_register_with_NULL_neighbor_on_resolve) {
    EXPECT_CALL(mk_trans1, Configure(_, NULL)).WillOnce(Invoke(&c1, &ConfigureHolder::OnConfigure));

    mfxVideoParam vp = {};
    vp.mfx.CodecId = MFX_CODEC_AVC;
    storage.RegisterTransform(TransformConfigDesc(0, vp),instant_auto_ptr<ITransform>(&mk_trans1));

    EXPECT_NO_THROW(storage.Resolve());
    EXPECT_EQ(MFX_CODEC_AVC, c1.ConfigParamVideo().mfx.CodecId);
}

TEST_F(TransformStorageTest, 2_tracks_1_transform_expect_register_with_NULL_neighbor_on_resolve) {

    EXPECT_CALL(mk_trans1, Configure(_, NULL)).WillOnce(Invoke(&c1, &ConfigureHolder::OnConfigure));
    EXPECT_CALL(mk_trans2, Configure(_, NULL)).WillOnce(Invoke(&c2, &ConfigureHolder::OnConfigure));

    mfxVideoParam vp = {};
    vp.mfx.CodecId = MFX_CODEC_AVC;
    storage.RegisterTransform(TransformConfigDesc(0, vp),instant_auto_ptr<ITransform>(&mk_trans1));
    
    vp.mfx.CodecId = MFX_CODEC_HEVC;
    storage.RegisterTransform(TransformConfigDesc(1, vp),instant_auto_ptr<ITransform>(&mk_trans2));

    EXPECT_NO_THROW(storage.Resolve());
    EXPECT_EQ(MFX_CODEC_AVC, c1.ConfigParamVideo().mfx.CodecId);
    EXPECT_EQ(MFX_CODEC_HEVC, c2.ConfigParamVideo().mfx.CodecId);
}

TEST_F(TransformStorageTest, 1_track_2_transforms_expect_register_with_pointer_to_neighbor_on_resolve) {
    
    EXPECT_CALL(mk_trans1, Configure(_, &mk_trans2)).WillOnce(Invoke(&c1, &ConfigureHolder::OnConfigure));
    EXPECT_CALL(mk_trans2, Configure(_, NULL)).WillOnce(Invoke(&c2, &ConfigureHolder::OnConfigure));

    mfxVideoParam vp = {};
    vp.mfx.CodecId = MFX_CODEC_AVC;
    storage.RegisterTransform(TransformConfigDesc(0, vp),instant_auto_ptr<ITransform>(&mk_trans1));

    vp.mfx.CodecId = MFX_CODEC_HEVC;
    storage.RegisterTransform(TransformConfigDesc(0, vp),instant_auto_ptr<ITransform>(&mk_trans2));

    EXPECT_NO_THROW(storage.Resolve());

    EXPECT_EQ(MFX_CODEC_AVC, c1.ConfigParamVideo().mfx.CodecId);
    EXPECT_EQ(MFX_CODEC_HEVC, c2.ConfigParamVideo().mfx.CodecId);
}

TEST_F(TransformStorageTest, 2_tracks_video_and_audio_expect_register_with_proper_codecid) {

    EXPECT_CALL(mk_trans1, Configure(_, NULL)).WillOnce(Invoke(&c1, &ConfigureHolder::OnConfigure));
    EXPECT_CALL(mk_trans2, Configure(_, NULL)).WillOnce(Invoke(&c2, &ConfigureHolder::OnConfigure));

    mfxVideoParam vp = {};
    vp.mfx.CodecId = MFX_CODEC_AVC;
    storage.RegisterTransform(TransformConfigDesc(0, vp),instant_auto_ptr<ITransform>(&mk_trans1));

    mfxAudioParam ap = {};
    ap.mfx.CodecId = MFX_CODEC_AAC;
    storage.RegisterTransform(TransformConfigDesc(1, ap),instant_auto_ptr<ITransform>(&mk_trans2));

    EXPECT_NO_THROW(storage.Resolve());
    EXPECT_EQ(MFX_CODEC_AVC, c1.ConfigParamVideo().mfx.CodecId);
    EXPECT_EQ(MFX_CODEC_AAC, c2.ConfigParamAudio().mfx.CodecId);
}