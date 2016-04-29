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

    storage.RegisterTransform(TransformConfigDesc(0, *new mfxVideoParam()),instant_auto_ptr2<ITransform>( new MockTransform()) );

    EXPECT_EQ(false, storage.empty(0));
    EXPECT_EQ(true, storage.empty(1));
}

TEST_F(TransformStorageTest, 2_tracks) {

    storage.RegisterTransform(TransformConfigDesc(0, *new mfxVideoParam()),instant_auto_ptr<ITransform>(&mk_trans1));
    storage.RegisterTransform(TransformConfigDesc(0, *new mfxVideoParam()),instant_auto_ptr<ITransform>( &mk_trans2));
    storage.RegisterTransform(TransformConfigDesc(1, *new mfxVideoParam()),instant_auto_ptr<ITransform>( &mk_trans3));

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
