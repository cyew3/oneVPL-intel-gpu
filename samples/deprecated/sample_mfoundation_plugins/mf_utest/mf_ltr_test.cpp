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

#include "mf_ltr.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
using ::testing::AtLeast;

class MockPatternGenerator : public FrameTypeGenerator
{
public:
    MOCK_CONST_METHOD0( CurrentFrameType, mfxU16 ());
    MOCK_METHOD0( Next, void ());
    MOCK_METHOD4( Init, void (mfxU16 GopOptFlag,
                              mfxU16 GopPicSize,
                              mfxU16 GopRefDist,
                              mfxU16 IdrInterval));
};

TEST(LTR, GOP_PATTERN_GENERATOR_ONLY_IDR_FRAMES)
{
    FrameTypeGenerator gen;
    gen.Init(MFX_GOP_CLOSED, 1, 0, 0);

    EXPECT_EQ(MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
    gen.Next();
    EXPECT_EQ(MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
    gen.Next();
    EXPECT_EQ(MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
}

TEST(LTR, GOP_PATTERN_GENERATOR_I_FRAMES_PLUS_IDR)
{
    FrameTypeGenerator gen;
    gen.Init(MFX_GOP_CLOSED, 0, 0, 3);

    EXPECT_EQ(MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
    gen.Next();
    EXPECT_EQ(MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
    gen.Next();
    EXPECT_EQ(MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
    gen.Next();
    EXPECT_EQ(MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
    gen.Next();
    EXPECT_EQ(MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
}

TEST(LTR, GOP_PATTERN_GENERATOR_I_AND_P_Frames)
{
    FrameTypeGenerator gen;
    gen.Init(MFX_GOP_CLOSED, 3, 0, 0);

    EXPECT_EQ(MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
    gen.Next();
    EXPECT_EQ(MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
    gen.Next();
    EXPECT_EQ(MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
    gen.Next();
    EXPECT_EQ(MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
}

TEST(LTR, GOP_PATTERN_GENERATOR_IBP)
{
    FrameTypeGenerator gen;
    gen.Init(MFX_GOP_STRICT, 4, 2, 0);

    EXPECT_EQ(MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
    gen.Next();
    EXPECT_EQ(MFX_FRAMETYPE_B | MFX_FRAMETYPE_xB , gen.CurrentFrameType());
    gen.Next();
    EXPECT_EQ(MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
    gen.Next();
    EXPECT_EQ(MFX_FRAMETYPE_B | MFX_FRAMETYPE_xB , gen.CurrentFrameType());
    gen.Next();
    EXPECT_EQ(MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF , gen.CurrentFrameType());
}

TEST(LTR, GOP_PATTERN_GENERATOR_NO_INIT)
{
    FrameTypeGenerator gen;
    EXPECT_EQ(MFX_FRAMETYPE_IDR , gen.CurrentFrameType());
}

/*******************************************************************************************/
using testing::Return;

TEST(LTR, BufferControl_SpecValid )
{
    MockPatternGenerator *my_pattern = new MockPatternGenerator;

    MFLongTermReference ltr(NULL, my_pattern);

    EXPECT_EQ(S_OK, ltr.SetBufferControl(MAKE_ULONG(1, 16)));
    EXPECT_EQ(E_INVALIDARG, ltr.SetBufferControl(MAKE_ULONG(0, 6)));
    EXPECT_EQ(E_INVALIDARG, ltr.SetBufferControl(MAKE_ULONG(11, 7)));
    EXPECT_EQ(E_INVALIDARG, ltr.SetBufferControl(MAKE_ULONG(0, 17)));
    ULONG val;
    EXPECT_EQ(S_OK, ltr.GetBufferControl(val));
    EXPECT_EQ(MAKE_ULONG(1, 16), val);
}

#define NBITS1(n) (0xFFFFFFFF >> (32 - n))

TEST(LTR, UseFrame_SpecValid )
{
    MockPatternGenerator *my_pattern = new MockPatternGenerator;
    mfxU16 idr_return_type = MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
    mfxU16 p_return_type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;

    EXPECT_CALL(*my_pattern, CurrentFrameType())
        .WillOnce(Return(idr_return_type))
        .WillRepeatedly(Return(p_return_type));

    MFLongTermReference ltr(NULL, my_pattern);

    EXPECT_EQ(S_OK, ltr.SetBufferControl(MAKE_ULONG(1, 10)));
    ltr.SetNumRefFrame(12);

    mfxEncodeCtrl encodeCtrl = {0};

    //need to populate ltrs prior calling to use certain pattern
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();

    EXPECT_EQ(E_INVALIDARG, ltr.UseFrame(MAKE_ULONG(2, 5)));
    EXPECT_EQ(E_INVALIDARG, ltr.UseFrame(MAKE_ULONG(0, 0)));

    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(1, 5)));
    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(0, NBITS1(1))));
    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(0, NBITS1(2))));
    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(0, NBITS1(3))));
    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(0, NBITS1(4))));
    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(0, NBITS1(5))));
    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(0, NBITS1(6))));
    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(0, NBITS1(7))));
    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(0, NBITS1(8))));

    //frame not yet used as ltr, call should report an error
    EXPECT_EQ(E_INVALIDARG, ltr.UseFrame(MAKE_ULONG(0, ((ULONG)1) << 9)));
}

TEST(LTR, MArkFrame_SpecValid )
{
    MockPatternGenerator *my_pattern = new MockPatternGenerator;
    mfxU16 idr_return_type = MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
    mfxU16 p_return_type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;

    EXPECT_CALL(*my_pattern, CurrentFrameType())
        .WillOnce(Return(idr_return_type))
        .WillRepeatedly(Return(p_return_type));

    MFLongTermReference ltr(NULL, my_pattern);

    EXPECT_EQ(S_OK, ltr.SetBufferControl(MAKE_ULONG(1, 10)));
    ltr.SetNumRefFrame(12);

    mfxEncodeCtrl encodeCtrl = {0};

    EXPECT_EQ(E_INVALIDARG, ltr.MarkFrame(16));

    //need to populate ltrs prior calling to use certain pattern
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();


    EXPECT_EQ(S_OK, ltr.MarkFrame(0));
    EXPECT_EQ(S_OK, ltr.MarkFrame(1));
    EXPECT_EQ(S_OK, ltr.MarkFrame(2));
    EXPECT_EQ(S_OK, ltr.MarkFrame(3));
    EXPECT_EQ(E_INVALIDARG, ltr.MarkFrame(11));
}

#define EXPECT_FRAME(array_name, idx, order)\
    EXPECT_EQ(MFX_PICSTRUCT_PROGRESSIVE, ref_list->array_name[idx].PicStruct);\
    EXPECT_EQ(order, ref_list->array_name[idx].FrameOrder);

#define UNEXPECT_FRAME(array_name, idx)\
    EXPECT_EQ(MFX_PICSTRUCT_UNKNOWN, ref_list->array_name[idx].PicStruct);


TEST(LTR, BufferControl_BehaviorValid )
{
    MockPatternGenerator *my_pattern = new MockPatternGenerator;

    mfxU16 idr_return_type = MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
    mfxU16 p_return_type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;

    EXPECT_CALL(*my_pattern, CurrentFrameType())
        .WillOnce(Return(idr_return_type))
        .WillRepeatedly(Return(p_return_type));

    MFLongTermReference ltr(NULL, my_pattern);
    EXPECT_EQ(S_OK, ltr.SetBufferControl(MAKE_ULONG(1, 3)));
    ltr.SetNumRefFrame(5);

    mfxExtAVCRefListCtrl *ref_list;
    mfxEncodeCtrl encodeCtrl = {0};

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));

    EXPECT_EQ(1, encodeCtrl.NumExtParam);
    EXPECT_NE((mfxExtBuffer**)NULL, encodeCtrl.ExtParam);
    EXPECT_EQ((mfxU32)MFX_EXTBUFF_AVC_REFLIST_CTRL, encodeCtrl.ExtParam[0]->BufferId);

    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);

    //first frame 1ltr
    EXPECT_FRAME(LongTermRefList, 0, 0);
    UNEXPECT_FRAME(LongTermRefList, 1);

    //second frame 2 ltrs
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));

    EXPECT_EQ(1, encodeCtrl.NumExtParam);
    EXPECT_NE((mfxExtBuffer**)NULL, encodeCtrl.ExtParam);
    EXPECT_EQ((mfxU32)MFX_EXTBUFF_AVC_REFLIST_CTRL, encodeCtrl.ExtParam[0]->BufferId);

    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);

    EXPECT_FRAME(LongTermRefList, 0, 1);
    UNEXPECT_FRAME(LongTermRefList, 1);

    //3rd frame - 3 ltrs
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));

    EXPECT_EQ(1, encodeCtrl.NumExtParam);
    EXPECT_NE((mfxExtBuffer**)NULL, encodeCtrl.ExtParam);
    EXPECT_EQ((mfxU32)MFX_EXTBUFF_AVC_REFLIST_CTRL, encodeCtrl.ExtParam[0]->BufferId);

    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_FRAME(LongTermRefList, 0, 2);
    UNEXPECT_FRAME(LongTermRefList, 1);

    //4rd frame - no new ltrs
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));

    EXPECT_EQ(1, encodeCtrl.NumExtParam);
    EXPECT_NE((mfxExtBuffer**)NULL, encodeCtrl.ExtParam);
    EXPECT_EQ((mfxU32)MFX_EXTBUFF_AVC_REFLIST_CTRL, encodeCtrl.ExtParam[0]->BufferId);

    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);

    UNEXPECT_FRAME(LongTermRefList, 0);

    //5th frame - again only 3 ltrs, 5ref
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));

    EXPECT_EQ(1, encodeCtrl.NumExtParam);
    EXPECT_NE((mfxExtBuffer**)NULL, encodeCtrl.ExtParam);
    EXPECT_EQ((mfxU32)MFX_EXTBUFF_AVC_REFLIST_CTRL, encodeCtrl.ExtParam[0]->BufferId);

    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);

    UNEXPECT_FRAME(LongTermRefList, 0);
}

TEST(LTR, BufferControl_BehaviorValid_RepopulateLTRs_after_secondIDR )
{
    MockPatternGenerator *my_pattern = new MockPatternGenerator;

    mfxU16 idr_return_type = MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
    mfxU16 p_return_type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;

    EXPECT_CALL(*my_pattern, CurrentFrameType())
        .WillOnce(Return(idr_return_type))
        .WillOnce(Return(p_return_type))
        .WillOnce(Return(p_return_type))
        .WillOnce(Return(idr_return_type))
        .WillOnce(Return(p_return_type))
        .WillOnce(Return(p_return_type));

    MFLongTermReference ltr(NULL, my_pattern);
    EXPECT_EQ(S_OK, ltr.SetBufferControl(MAKE_ULONG(1, 2)));
    ltr.SetNumRefFrame(5);

    mfxExtAVCRefListCtrl *ref_list;
    mfxEncodeCtrl encodeCtrl = {0};

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);

    //first frame 1ltr
    EXPECT_FRAME(LongTermRefList, 0, 0);
    UNEXPECT_FRAME(LongTermRefList, 1);

    //second frame 2 ltrs
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);

    EXPECT_FRAME(LongTermRefList, 0, 1);
    UNEXPECT_FRAME(LongTermRefList, 1);

    //3rd frame - 2 ltrs
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));

    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    UNEXPECT_FRAME(LongTermRefList, 0);

    //4rd frame - IDR - 3 frame order is LTR
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_FRAME(LongTermRefList, 0, 3);
    UNEXPECT_FRAME(LongTermRefList, 1);

    //5th frame - 4th frame is LTR
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_FRAME(LongTermRefList, 0, 4);
    UNEXPECT_FRAME(LongTermRefList, 1);

    //6th frame - no new LTRs
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    UNEXPECT_FRAME(LongTermRefList, 0);
}



TEST(LTR, BufferControl_TrusUntiltWithoutUse_SpecValid)
{
    MockPatternGenerator *my_pattern = new MockPatternGenerator;

    mfxU16 idr_return_type = MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
    mfxU16 p_return_type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;

    EXPECT_CALL(*my_pattern, CurrentFrameType())
        .WillOnce(Return(idr_return_type))
        .WillRepeatedly(Return(p_return_type));

    MFLongTermReference ltr(NULL, my_pattern);
    EXPECT_EQ(S_OK, ltr.SetBufferControl(MAKE_ULONG(MF_LTR_BC_MODE_TRUST_UNTIL, 3)));
    ltr.SetNumRefFrame(5);

    mfxExtAVCRefListCtrl *ref_list;
    mfxEncodeCtrl encodeCtrl = {0};

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_EQ(0, ref_list->NumRefIdxL0Active);
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_EQ(0, ref_list->NumRefIdxL0Active);
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_EQ(0, ref_list->NumRefIdxL0Active);
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_EQ(0, ref_list->NumRefIdxL0Active); // why EXPECT_EQ(not EXPECT_NE): we don't have any short-term's yet
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_EQ(0, ref_list->NumRefIdxL0Active);
}

TEST(LTR, BufferControl_TrustUntilWithUse_SpecValid)
{
    MockPatternGenerator *my_pattern = new MockPatternGenerator;

    mfxU16 idr_return_type = MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
    mfxU16 p_return_type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;

    EXPECT_CALL(*my_pattern, CurrentFrameType())
        .WillOnce(Return(idr_return_type))
        .WillRepeatedly(Return(p_return_type));

    MFLongTermReference ltr(NULL, my_pattern);
    EXPECT_EQ(S_OK, ltr.SetBufferControl(MAKE_ULONG(MF_LTR_BC_MODE_TRUST_UNTIL, 3)));
    ltr.SetNumRefFrame(5);

    mfxExtAVCRefListCtrl *ref_list;
    mfxEncodeCtrl encodeCtrl = {0};

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(0, NBITS1(1))));

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_EQ(1, ref_list->NumRefIdxL0Active);
    EXPECT_FRAME(PreferredRefList, 0, 0);
    UNEXPECT_FRAME(PreferredRefList, 1);
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(0, NBITS1(2))));

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_EQ(2, ref_list->NumRefIdxL0Active);
    EXPECT_FRAME(PreferredRefList, 0, 0);
    EXPECT_FRAME(PreferredRefList, 1, 1);
    UNEXPECT_FRAME(PreferredRefList, 2);
}

TEST(LTR, ForbiddenBFrames)
{
    MockPatternGenerator *my_pattern = new MockPatternGenerator;

    mfxU16 idr_return_type = MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
    mfxU16 p_return_type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
    mfxU16 b_return_type = MFX_FRAMETYPE_B;

    EXPECT_CALL(*my_pattern, CurrentFrameType())
        .WillOnce(Return(idr_return_type))
        .WillOnce(Return(b_return_type))
        .WillRepeatedly(Return(p_return_type));

    MFLongTermReference ltr(NULL, my_pattern);
    EXPECT_EQ(S_OK, ltr.SetBufferControl(MAKE_ULONG(MF_LTR_BC_MODE_TRUST_UNTIL, 3)));
    ltr.SetNumRefFrame(5);

    mfxEncodeCtrl encodeCtrl = {0};

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(E_FAIL, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
}

TEST(LTR, ForceFrameType)
{
    MockPatternGenerator *my_pattern = new MockPatternGenerator;

    mfxU16 idr_return_type = MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
    mfxU16 p_return_type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;

    EXPECT_CALL(*my_pattern, CurrentFrameType())
        .WillOnce(Return(idr_return_type))
        .WillRepeatedly(Return(p_return_type));

    MFLongTermReference ltr(NULL, my_pattern);
    EXPECT_EQ(S_OK, ltr.SetBufferControl(MAKE_ULONG(MF_LTR_BC_MODE_TRUST_UNTIL, 3)));
    ltr.SetNumRefFrame(5);

    mfxEncodeCtrl encodeCtrl = {0};
    mfxExtAVCRefListCtrl *ref_list;

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    ltr.ForceFrameType(MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF);

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_FRAME(LongTermRefList, 0, 3);
    UNEXPECT_FRAME(LongTermRefList, 1);
}

TEST(LTR, UseFrameWithConstrationsInTrustMode)
{
    MockPatternGenerator *my_pattern = new MockPatternGenerator;
    mfxU16 idr_return_type = MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
    mfxU16 p_return_type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;

    EXPECT_CALL(*my_pattern, CurrentFrameType())
        .WillOnce(Return(idr_return_type))
        .WillRepeatedly(Return(p_return_type));

    MFLongTermReference ltr(NULL, my_pattern);

    EXPECT_EQ(S_OK, ltr.SetBufferControl(MAKE_ULONG(1, 3)));
    ltr.SetNumRefFrame(5);

    mfxEncodeCtrl encodeCtrl = {0};
    mfxExtAVCRefListCtrl *ref_list;

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(E_INVALIDARG, ltr.UseFrame(MAKE_ULONG(2, 5)));
    EXPECT_EQ(E_INVALIDARG, ltr.UseFrame(MAKE_ULONG(0, 0)));
    EXPECT_EQ(E_INVALIDARG, ltr.UseFrame(MAKE_ULONG(1, 0)));

    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(1, 5))); // 00101

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_EQ(2, ref_list->NumRefIdxL0Active);
    UNEXPECT_FRAME(LongTermRefList, 0);
    EXPECT_FRAME(PreferredRefList, 0, 0);
    EXPECT_FRAME(PreferredRefList, 1, 2);
    UNEXPECT_FRAME(PreferredRefList, 2);
    EXPECT_FRAME(RejectedRefList, 0, 3);
    EXPECT_FRAME(RejectedRefList, 1, 4);
    UNEXPECT_FRAME(RejectedRefList, 2);
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(E_INVALIDARG, ltr.UseFrame(MAKE_ULONG(1, 13))); // 01101
    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(1, 3))); // 00011

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_EQ(2, ref_list->NumRefIdxL0Active);
    UNEXPECT_FRAME(LongTermRefList, 0);
    EXPECT_FRAME(PreferredRefList, 0, 0);
    EXPECT_FRAME(PreferredRefList, 1, 1);
    UNEXPECT_FRAME(PreferredRefList, 2);
    EXPECT_FRAME(RejectedRefList, 0, 5); // short-term
    UNEXPECT_FRAME(RejectedRefList, 1);
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_EQ(2, ref_list->NumRefIdxL0Active);
    UNEXPECT_FRAME(LongTermRefList, 0);
    EXPECT_FRAME(PreferredRefList, 0, 0);
    EXPECT_FRAME(PreferredRefList, 1, 1);
    UNEXPECT_FRAME(PreferredRefList, 2);
    UNEXPECT_FRAME(RejectedRefList, 0);
}

TEST(LTR, UseFrameWithConstrationsInDontTrustMode)
{
    MockPatternGenerator *my_pattern = new MockPatternGenerator;
    mfxU16 idr_return_type = MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
    mfxU16 p_return_type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;

    EXPECT_CALL(*my_pattern, CurrentFrameType())
        .WillOnce(Return(idr_return_type))
        .WillRepeatedly(Return(p_return_type));

    MFLongTermReference ltr(NULL, my_pattern);

    EXPECT_EQ(S_OK, ltr.SetBufferControl(MAKE_ULONG(1, 4)));
    ltr.SetNumRefFrame(6);

    mfxEncodeCtrl encodeCtrl = {0};
    mfxExtAVCRefListCtrl *ref_list;

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(E_INVALIDARG, ltr.UseFrame(MAKE_ULONG(2, 5)));
    EXPECT_EQ(E_INVALIDARG, ltr.UseFrame(MAKE_ULONG(0, 0)));
    EXPECT_EQ(E_INVALIDARG, ltr.UseFrame(MAKE_ULONG(1, 0)));

    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(1, 10))); // 1010

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_EQ(2, ref_list->NumRefIdxL0Active);
    EXPECT_FRAME(RejectedRefList, 0, 4);
    UNEXPECT_FRAME(RejectedRefList, 1);
    UNEXPECT_FRAME(LongTermRefList, 0);
    EXPECT_FRAME(PreferredRefList, 0, 1);
    EXPECT_FRAME(PreferredRefList, 1, 3);
    UNEXPECT_FRAME(PreferredRefList, 2);

    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(E_INVALIDARG, ltr.UseFrame(MAKE_ULONG(1, 23))); // 10111
    EXPECT_EQ(S_OK, ltr.UseFrame(MAKE_ULONG(1, 14))); // 1110

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_EQ(3, ref_list->NumRefIdxL0Active);
    UNEXPECT_FRAME(LongTermRefList, 0);
    EXPECT_FRAME(PreferredRefList, 0, 1);
    EXPECT_FRAME(PreferredRefList, 1, 2);
    EXPECT_FRAME(PreferredRefList, 2, 3);
    UNEXPECT_FRAME(PreferredRefList, 3);
    EXPECT_FRAME(RejectedRefList, 0, 5); // short-term
    UNEXPECT_FRAME(RejectedRefList, 1);  // ltr
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    EXPECT_EQ(3, ref_list->NumRefIdxL0Active);
    UNEXPECT_FRAME(LongTermRefList, 0);
    EXPECT_FRAME(PreferredRefList, 0, 1);
    EXPECT_FRAME(PreferredRefList, 1, 2);
    EXPECT_FRAME(PreferredRefList, 2, 3);
    UNEXPECT_FRAME(PreferredRefList, 3);
    UNEXPECT_FRAME(RejectedRefList, 0);
}

TEST(LTR, MarkFrame_BehaviorValid)
{
    MockPatternGenerator *my_pattern = new MockPatternGenerator;
    mfxU16 idr_return_type = MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
    mfxU16 p_return_type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;

    EXPECT_CALL(*my_pattern, CurrentFrameType())
        .WillOnce(Return(idr_return_type))
        .WillRepeatedly(Return(p_return_type));

    MFLongTermReference ltr(NULL, my_pattern);

    EXPECT_EQ(S_OK, ltr.SetBufferControl(MAKE_ULONG(1, 2)));
    ltr.SetNumRefFrame(3);

    mfxEncodeCtrl encodeCtrl = {0};
    mfxExtAVCRefListCtrl *ref_list;

    EXPECT_EQ(E_INVALIDARG, ltr.MarkFrame(2));

    //need to populate ltrs prior calling to use certain pattern
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.MarkFrame(0));

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    UNEXPECT_FRAME(RejectedRefList, 0);
    UNEXPECT_FRAME(RejectedRefList, 1);
    EXPECT_FRAME(LongTermRefList, 0, 1);
    UNEXPECT_FRAME(LongTermRefList, 1);

    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);
    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.MarkFrame(0));

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    UNEXPECT_FRAME(RejectedRefList, 0);
    UNEXPECT_FRAME(RejectedRefList, 1);
    EXPECT_FRAME(LongTermRefList, 0, 4);
    UNEXPECT_FRAME(PreferredRefList, 1);

    ltr.IncrementFrameOrder();
    Zero(encodeCtrl);

    EXPECT_EQ(S_OK, ltr.MarkFrame(1));

    EXPECT_EQ(S_OK, ltr.GenerateFrameEncodeCtrlExtParam(encodeCtrl));
    ref_list = reinterpret_cast<mfxExtAVCRefListCtrl*>(encodeCtrl.ExtParam[0]);
    UNEXPECT_FRAME(RejectedRefList, 0);
    UNEXPECT_FRAME(RejectedRefList, 1);
    EXPECT_FRAME(LongTermRefList, 0, 5);
    UNEXPECT_FRAME(PreferredRefList, 1);
}

