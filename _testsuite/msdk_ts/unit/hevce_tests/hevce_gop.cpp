//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2015 - 2016 Intel Corporation. All Rights Reserved.
//

#include "gtest/gtest.h"

#define MFX_ENABLE_H265_VIDEO_ENCODE
#include <vector>
#include "mfx_h265_frame.h"
#include "mfx_h265_enc.h"

using H265Enc::FrameData;
using H265Enc::Frame;
using H265Enc::FrameIter;
using H265Enc::FrameList;
using H265Enc::H265VideoParam;

void ReorderBiFrames(FrameIter inBegin, FrameIter inEnd, FrameList &in, FrameList &out, Ipp32s layer = 1);
void ReorderBiFramesLongGop(FrameIter inBegin, FrameIter inEnd, FrameList &in, FrameList &out);
void ReorderFrames(FrameList &input, FrameList &reordered, const H265VideoParam &param, Ipp32s endOfStream);

class GopTest : public ::testing::Test {
protected:
    GopTest() {
        for (int i = 0; i < 20; i++) {
            Frame *task = new Frame;
            task->m_frameOrder = i;
            //task->m_origin = new H265Frame;
            inInit.push_back(task);
        }

        Frame *task = new Frame;
        task->m_frameOrder = 100;
        outInit.push_back(task);

        in = inInit;
        out = outInit;
    }

    ~GopTest() {
        for (auto task: inInit) {
            delete task->m_origin;
            delete task;
        }
        for (auto task: outInit) {
            delete task;
        }
    }

    void SetupQueues(const char *types) {
        in = inInit;
        out.clear();
        auto iter = in.begin();
        for (int frameOrder = 0; *types; types++, frameOrder++, iter++) {
            assert(iter != in.end());
            assert(*types == 'I' || *types == 'i' || *types == 'p' || *types == 'b');
            (*iter)->m_frameOrder = frameOrder;
            //(*iter)->m_origin = new H265Frame;
            (*iter)->m_picCodeType = (*types == 'p') ? MFX_FRAMETYPE_P : (*types == 'b') ? MFX_FRAMETYPE_B : MFX_FRAMETYPE_I;
            (*iter)->m_isIdrPic = (*types == 'I');
        }
        in.erase(iter, in.end());
    }

    FrameList inInit;
    FrameList outInit;
    FrameList in;
    FrameList out;
};

TEST_F(GopTest, ReorderBiFrames) {
    { // 0B
        in = inInit;
        out = outInit;
        ReorderBiFrames(std::next(in.begin(), 5), std::next(in.begin(), 5), in, out);
        EXPECT_EQ(inInit, in);
        EXPECT_EQ(outInit, out);
    }
    { // 1B, in the beginning
        in = inInit;
        out = outInit;
        ReorderBiFrames(in.begin(), std::next(in.begin(), 1), in, out);
        EXPECT_EQ(20 - 1, in.size());
        EXPECT_EQ(1 + 1, out.size());
        EXPECT_EQ(true, std::equal(in.begin(), in.end(), std::next(inInit.begin(), 1)));
        auto iter = out.begin();
        EXPECT_EQ((*iter++)->m_frameOrder, 100); // check that existed task is untouched
        EXPECT_EQ((*iter)->m_frameOrder, 0);
        EXPECT_EQ((*iter)->m_pyramidLayer, 1);
    }
    { // 5B, in the beginning
        in = inInit;
        out = outInit;
        ReorderBiFrames(in.begin(), std::next(in.begin(), 5), in, out);
        // check sizes
        EXPECT_EQ(20 - 5, in.size());
        EXPECT_EQ(1 + 5, out.size());
        // check tasks remained in input queue
        EXPECT_EQ(true, std::equal(in.begin(), in.end(), std::next(inInit.begin(), 5)));
        // check order of tasks in output queue
        auto iter = out.begin();
        EXPECT_EQ(100, (*iter++)->m_frameOrder);
        EXPECT_EQ(2,   (*iter++)->m_frameOrder);
        EXPECT_EQ(0,   (*iter++)->m_frameOrder);
        EXPECT_EQ(1,   (*iter++)->m_frameOrder);
        EXPECT_EQ(3,   (*iter++)->m_frameOrder);
        EXPECT_EQ(4,   (*iter++)->m_frameOrder);
        iter = std::next(out.begin());
        EXPECT_EQ(1,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(2,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(3,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(2,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(3,   (*iter++)->m_pyramidLayer);
    }
    { // 6B, in the end
        in = inInit;
        out = outInit;
        ReorderBiFrames(std::prev(in.end(), 6), in.end(), in, out);
        // check sizes
        EXPECT_EQ(20 - 6, in.size());
        EXPECT_EQ(1 + 6, out.size());
        // check tasks remained in input queue
        EXPECT_EQ(true, std::equal(in.begin(), in.end(), inInit.begin()));
        // check order of tasks in output queue
        auto iter = out.begin();
        EXPECT_EQ(100, (*iter++)->m_frameOrder);
        EXPECT_EQ(16,  (*iter++)->m_frameOrder);
        EXPECT_EQ(14,  (*iter++)->m_frameOrder);
        EXPECT_EQ(15,  (*iter++)->m_frameOrder);
        EXPECT_EQ(18,  (*iter++)->m_frameOrder);
        EXPECT_EQ(17,  (*iter++)->m_frameOrder);
        EXPECT_EQ(19,  (*iter++)->m_frameOrder);
        iter = std::next(out.begin());
        EXPECT_EQ(1,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(2,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(3,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(2,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(3,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(3,   (*iter++)->m_pyramidLayer);
    }
    { // 7B, in the middle, initial layer != 1
        in = inInit;
        out = outInit;
        ReorderBiFrames(std::next(in.begin(), 2), std::next(in.begin(), 9), in, out, 5);
        // check sizes
        EXPECT_EQ(20 - 7, in.size());
        EXPECT_EQ(1 + 7, out.size());
        // check tasks remained in input queue
        EXPECT_EQ(true, std::equal(std::next(in.begin(), 0), std::next(in.begin(), 2), std::next(inInit.begin(), 0)));
        EXPECT_EQ(true, std::equal(std::next(in.begin(), 2), std::next(in.begin(), 4), std::next(inInit.begin(), 9)));
        // check order of tasks in output queue
        auto iter = out.begin();
        EXPECT_EQ(100, (*iter++)->m_frameOrder);
        EXPECT_EQ(5,   (*iter++)->m_frameOrder);
        EXPECT_EQ(3,   (*iter++)->m_frameOrder);
        EXPECT_EQ(2,   (*iter++)->m_frameOrder);
        EXPECT_EQ(4,   (*iter++)->m_frameOrder);
        EXPECT_EQ(7,   (*iter++)->m_frameOrder);
        EXPECT_EQ(6,   (*iter++)->m_frameOrder);
        EXPECT_EQ(8,   (*iter++)->m_frameOrder);
        iter = std::next(out.begin());
        EXPECT_EQ(5,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(6,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(7,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(7,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(6,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(7,   (*iter++)->m_pyramidLayer);
        EXPECT_EQ(7,   (*iter++)->m_pyramidLayer);
    }
}

TEST_F(GopTest, ReorderBiFramesLongGop) {
    ReorderBiFramesLongGop(in.begin(), std::next(in.begin(), 15), in, out);
    // check sizes
    EXPECT_EQ(20 - 15, in.size());
    EXPECT_EQ(1 + 15, out.size());
    // check order of tasks in output queue
    auto iter = out.begin();
    EXPECT_EQ(100, (*iter++)->m_frameOrder);
    EXPECT_EQ(3,   (*iter++)->m_frameOrder);
    EXPECT_EQ(1,   (*iter++)->m_frameOrder);
    EXPECT_EQ(0,   (*iter++)->m_frameOrder);
    EXPECT_EQ(2,   (*iter++)->m_frameOrder);
    EXPECT_EQ(7,   (*iter++)->m_frameOrder);
    EXPECT_EQ(5,   (*iter++)->m_frameOrder);
    EXPECT_EQ(4,   (*iter++)->m_frameOrder);
    EXPECT_EQ(6,   (*iter++)->m_frameOrder);
    EXPECT_EQ(11,  (*iter++)->m_frameOrder);
    EXPECT_EQ(9,   (*iter++)->m_frameOrder);
    EXPECT_EQ(8,   (*iter++)->m_frameOrder);
    EXPECT_EQ(10,  (*iter++)->m_frameOrder);
    EXPECT_EQ(13,  (*iter++)->m_frameOrder);
    EXPECT_EQ(12,  (*iter++)->m_frameOrder);
    EXPECT_EQ(14,  (*iter++)->m_frameOrder);
    iter = std::next(out.begin());
    EXPECT_EQ(2,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(3,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(4,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(4,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(2,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(3,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(4,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(4,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(2,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(3,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(4,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(4,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(3,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(4,   (*iter++)->m_pyramidLayer);
    EXPECT_EQ(4,   (*iter++)->m_pyramidLayer);
}

TEST_F(GopTest, ReorderFrames) {
    H265VideoParam param;
    // Idr P I bb P bBb P bbbb Idr BbBBb Idr bbbbbb I<strict/closed> bBbBbBb I<strict/open> bbbbbbbb I<non-strict/closed> bBBbBbBBb I<non-strict/open> bbbbbbbbbb <non-eos>

    param.GopClosedFlag = 0;
    param.GopStrictFlag = 0;
    param.BiPyramidLayers = 1;
    param.longGop = 0;
    SetupQueues("Ipibbp"); // Idr P I bb P
    ReorderFrames(in, out, param, false);
    EXPECT_EQ(0, out.back()->m_frameOrder);
    EXPECT_EQ(0, out.back()->m_biFramesInMiniGop);
    ReorderFrames(in, out, param, false);
    EXPECT_EQ(1, out.back()->m_frameOrder);
    EXPECT_EQ(0, out.back()->m_biFramesInMiniGop);
    ReorderFrames(in, out, param, false);
    EXPECT_EQ(2, out.back()->m_frameOrder);
    EXPECT_EQ(0, out.back()->m_biFramesInMiniGop);
    ReorderFrames(in, out, param, false);
    auto iter = std::next(out.begin(), 3);
    EXPECT_EQ(5, (*iter++)->m_frameOrder);
    EXPECT_EQ(3, (*iter++)->m_frameOrder);
    EXPECT_EQ(4, (*iter++)->m_frameOrder);
    iter = std::next(out.begin(), 3);
    EXPECT_EQ(2, (*iter++)->m_biFramesInMiniGop);
    EXPECT_EQ(2, (*iter++)->m_biFramesInMiniGop);
    EXPECT_EQ(2, (*iter++)->m_biFramesInMiniGop);

    param.BiPyramidLayers = 4;
    SetupQueues("bbbp"); // bBb P
    ReorderFrames(in, out, param, false);
    iter = out.begin();
    EXPECT_EQ(3, (*iter++)->m_frameOrder);
    EXPECT_EQ(1, (*iter++)->m_frameOrder); 
    EXPECT_EQ(0, (*iter++)->m_frameOrder);
    EXPECT_EQ(2, (*iter++)->m_frameOrder);
    for (auto task: out)
        EXPECT_EQ(3, task->m_biFramesInMiniGop);

    param.BiPyramidLayers = 1;
    SetupQueues("bbbbI"); // bbbb Idr
    ReorderFrames(in, out, param, false);
    iter = out.begin();
    EXPECT_EQ(MFX_FRAMETYPE_P, (*iter)->m_picCodeType);
    EXPECT_EQ(3, (*iter++)->m_frameOrder);
    EXPECT_EQ(0, (*iter++)->m_frameOrder); 
    EXPECT_EQ(1, (*iter++)->m_frameOrder);
    EXPECT_EQ(2, (*iter++)->m_frameOrder);
    for (auto task: out)
        EXPECT_EQ(3, task->m_biFramesInMiniGop);
    ReorderFrames(in, out, param, false);
    EXPECT_EQ(4, out.back()->m_frameOrder);

    param.BiPyramidLayers = 4;
    SetupQueues("bbbbbI"); // BbBBb Idr
    ReorderFrames(in, out, param, false);
    iter = out.begin();
    EXPECT_EQ(MFX_FRAMETYPE_P, (*iter)->m_picCodeType);
    EXPECT_EQ(4, (*iter++)->m_frameOrder);
    EXPECT_EQ(1, (*iter++)->m_frameOrder); 
    EXPECT_EQ(0, (*iter++)->m_frameOrder);
    EXPECT_EQ(2, (*iter++)->m_frameOrder);
    EXPECT_EQ(3, (*iter++)->m_frameOrder);
    for (auto task: out)
        EXPECT_EQ(4, task->m_biFramesInMiniGop);
    ReorderFrames(in, out, param, false);
    EXPECT_EQ(5, out.back()->m_frameOrder);

    param.GopClosedFlag = 1;
    param.GopStrictFlag = 1;
    param.BiPyramidLayers = 1;
    SetupQueues("bbbbbbi"); // bbbbbb I<strict/closed>
    ReorderFrames(in, out, param, false);
    iter = out.begin();
    EXPECT_EQ(0, (*iter++)->m_frameOrder);
    EXPECT_EQ(1, (*iter++)->m_frameOrder); 
    EXPECT_EQ(2, (*iter++)->m_frameOrder);
    EXPECT_EQ(3, (*iter++)->m_frameOrder);
    EXPECT_EQ(4, (*iter++)->m_frameOrder);
    EXPECT_EQ(5, (*iter++)->m_frameOrder);
    for (auto task: out)
        EXPECT_EQ(6, task->m_biFramesInMiniGop);
    ReorderFrames(in, out, param, false);
    EXPECT_EQ(6, out.back()->m_frameOrder);

    param.GopClosedFlag = 0;
    param.BiPyramidLayers = 4;
    SetupQueues("bbbbbbbi"); // bBbBbBb I<strict/open>
    ReorderFrames(in, out, param, false);
    iter = out.begin();
    EXPECT_EQ(7, (*iter++)->m_frameOrder);
    EXPECT_EQ(3, (*iter++)->m_frameOrder); 
    EXPECT_EQ(1, (*iter++)->m_frameOrder);
    EXPECT_EQ(0, (*iter++)->m_frameOrder);
    EXPECT_EQ(2, (*iter++)->m_frameOrder);
    EXPECT_EQ(5, (*iter++)->m_frameOrder);
    EXPECT_EQ(4, (*iter++)->m_frameOrder);
    EXPECT_EQ(6, (*iter++)->m_frameOrder);
    for (auto task: out)
        EXPECT_EQ(7, task->m_biFramesInMiniGop);

    param.GopStrictFlag = 0;
    param.GopClosedFlag = 1;
    param.BiPyramidLayers = 1;
    SetupQueues("bbbbbbbbi"); // bbbbbbbb I<non-strict/closed>
    ReorderFrames(in, out, param, false);
    iter = out.begin();
    EXPECT_EQ(MFX_FRAMETYPE_P, (*iter)->m_picCodeType);
    EXPECT_EQ(7, (*iter++)->m_frameOrder);
    EXPECT_EQ(0, (*iter++)->m_frameOrder); 
    EXPECT_EQ(1, (*iter++)->m_frameOrder);
    EXPECT_EQ(2, (*iter++)->m_frameOrder);
    EXPECT_EQ(3, (*iter++)->m_frameOrder);
    EXPECT_EQ(4, (*iter++)->m_frameOrder);
    EXPECT_EQ(5, (*iter++)->m_frameOrder);
    EXPECT_EQ(6, (*iter++)->m_frameOrder);
    for (auto task: out)
        EXPECT_EQ(7, task->m_biFramesInMiniGop);
    ReorderFrames(in, out, param, false);
    EXPECT_EQ(8, out.back()->m_frameOrder);

    param.GopStrictFlag = 0;
    param.GopClosedFlag = 0;
    param.BiPyramidLayers = 4;
    SetupQueues("bbbbbbbbbi"); // bBBbBbBBb I<non-strict/open>
    ReorderFrames(in, out, param, false);
    iter = out.begin();
    EXPECT_EQ(9, (*iter++)->m_frameOrder);
    EXPECT_EQ(4, (*iter++)->m_frameOrder); 
    EXPECT_EQ(1, (*iter++)->m_frameOrder);
    EXPECT_EQ(0, (*iter++)->m_frameOrder);
    EXPECT_EQ(2, (*iter++)->m_frameOrder);
    EXPECT_EQ(3, (*iter++)->m_frameOrder);
    EXPECT_EQ(6, (*iter++)->m_frameOrder);
    EXPECT_EQ(5, (*iter++)->m_frameOrder);
    EXPECT_EQ(7, (*iter++)->m_frameOrder);
    EXPECT_EQ(8, (*iter++)->m_frameOrder);
    for (auto task: out)
        EXPECT_EQ(9, task->m_biFramesInMiniGop);

    param.BiPyramidLayers = 1;
    SetupQueues("bbbbbbbbbb"); // bbbbbbbbbb <non-eos>
    ReorderFrames(in, out, param, false);
    EXPECT_EQ(0, out.size());

    param.GopStrictFlag = 1;
    param.BiPyramidLayers = 1;
    SetupQueues("bbbbbbbbbbb"); // bbbbbbbbbbb <eos/strict>
    ReorderFrames(in, out, param, true);
    iter = out.begin();
    EXPECT_EQ(0, (*iter++)->m_frameOrder);
    EXPECT_EQ(1, (*iter++)->m_frameOrder);
    EXPECT_EQ(2, (*iter++)->m_frameOrder);
    EXPECT_EQ(3, (*iter++)->m_frameOrder);
    EXPECT_EQ(4, (*iter++)->m_frameOrder);
    EXPECT_EQ(5, (*iter++)->m_frameOrder);
    EXPECT_EQ(6, (*iter++)->m_frameOrder);
    EXPECT_EQ(7, (*iter++)->m_frameOrder);
    EXPECT_EQ(8, (*iter++)->m_frameOrder);
    EXPECT_EQ(9, (*iter++)->m_frameOrder);
    EXPECT_EQ(10,(*iter++)->m_frameOrder);
    for (auto task: out)
        EXPECT_EQ(11, task->m_biFramesInMiniGop);

    param.GopStrictFlag = 0;
    param.BiPyramidLayers = 4;
    SetupQueues("bbbbbbbbbbbb"); // BBBbBbBBbBbb <eos/non-strict>
    ReorderFrames(in, out, param, true);
    iter = out.begin();
    EXPECT_EQ(MFX_FRAMETYPE_P, (*iter)->m_picCodeType);
    EXPECT_EQ(11,(*iter++)->m_frameOrder);
    EXPECT_EQ(5, (*iter++)->m_frameOrder);
    EXPECT_EQ(2, (*iter++)->m_frameOrder);
    EXPECT_EQ(0, (*iter++)->m_frameOrder);
    EXPECT_EQ(1, (*iter++)->m_frameOrder);
    EXPECT_EQ(3, (*iter++)->m_frameOrder);
    EXPECT_EQ(4, (*iter++)->m_frameOrder);
    EXPECT_EQ(8, (*iter++)->m_frameOrder);
    EXPECT_EQ(6, (*iter++)->m_frameOrder);
    EXPECT_EQ(7, (*iter++)->m_frameOrder);
    EXPECT_EQ(9, (*iter++)->m_frameOrder);
    EXPECT_EQ(10,(*iter++)->m_frameOrder);
    for (auto task: out)
        EXPECT_EQ(11, task->m_biFramesInMiniGop);

    param.GopClosedFlag = 0;
    param.GopStrictFlag = 0;
    param.BiPyramidLayers = 5;
    param.longGop = 1;
    SetupQueues("bbbbbbbbbbbbbbbp"); // bBbBbBbBbBbBbBb<long> P
    ReorderFrames(in, out, param, false);
    iter = out.begin();
    EXPECT_EQ(15, (*iter++)->m_frameOrder);
    EXPECT_EQ(3, (*iter++)->m_frameOrder);
    EXPECT_EQ(1, (*iter++)->m_frameOrder);
    EXPECT_EQ(0, (*iter++)->m_frameOrder);
    EXPECT_EQ(2, (*iter++)->m_frameOrder);
    EXPECT_EQ(7, (*iter++)->m_frameOrder);
    EXPECT_EQ(5, (*iter++)->m_frameOrder);
    EXPECT_EQ(4, (*iter++)->m_frameOrder);
    EXPECT_EQ(6, (*iter++)->m_frameOrder);
    EXPECT_EQ(11,(*iter++)->m_frameOrder);
    EXPECT_EQ(9, (*iter++)->m_frameOrder);
    EXPECT_EQ(8, (*iter++)->m_frameOrder);
    EXPECT_EQ(10,(*iter++)->m_frameOrder);
    EXPECT_EQ(13,(*iter++)->m_frameOrder);
    EXPECT_EQ(12,(*iter++)->m_frameOrder);
    EXPECT_EQ(14,(*iter++)->m_frameOrder);
    for (auto task: out)
        EXPECT_EQ(15, task->m_biFramesInMiniGop);

    param.GopClosedFlag = 0;
    param.GopStrictFlag = 0;
    param.BiPyramidLayers = 5;
    param.longGop = 1;
    SetupQueues("bbbbbbbbbbbbbbp"); // not enough B for long gop
    ReorderFrames(in, out, param, false);
    iter = out.begin();
    EXPECT_EQ(14, (*iter++)->m_frameOrder);
    EXPECT_EQ(6, (*iter++)->m_frameOrder);
    EXPECT_EQ(2, (*iter++)->m_frameOrder);
    EXPECT_EQ(0, (*iter++)->m_frameOrder);
    EXPECT_EQ(1, (*iter++)->m_frameOrder);
    EXPECT_EQ(4, (*iter++)->m_frameOrder);
    EXPECT_EQ(3, (*iter++)->m_frameOrder);
    EXPECT_EQ(5, (*iter++)->m_frameOrder);
    EXPECT_EQ(10,(*iter++)->m_frameOrder);
    EXPECT_EQ(8, (*iter++)->m_frameOrder);
    EXPECT_EQ(7, (*iter++)->m_frameOrder);
    EXPECT_EQ(9, (*iter++)->m_frameOrder);
    EXPECT_EQ(12,(*iter++)->m_frameOrder);
    EXPECT_EQ(11,(*iter++)->m_frameOrder);
    EXPECT_EQ(13,(*iter++)->m_frameOrder);
    for (auto task: out)
        EXPECT_EQ(14, task->m_biFramesInMiniGop);

    param.GopClosedFlag = 0;
    param.GopStrictFlag = 0;
    param.BiPyramidLayers = 1;
    param.longGop = 1;
    SetupQueues("bbbbbbbbbbbbbbbp"); // pyramid disabled
    ReorderFrames(in, out, param, false);
    iter = out.begin();
    EXPECT_EQ(15, (*iter++)->m_frameOrder);
    EXPECT_EQ(0, (*iter++)->m_frameOrder);
    EXPECT_EQ(1, (*iter++)->m_frameOrder);
    EXPECT_EQ(2, (*iter++)->m_frameOrder);
    EXPECT_EQ(3, (*iter++)->m_frameOrder);
    EXPECT_EQ(4, (*iter++)->m_frameOrder);
    EXPECT_EQ(5, (*iter++)->m_frameOrder);
    EXPECT_EQ(6, (*iter++)->m_frameOrder);
    EXPECT_EQ(7, (*iter++)->m_frameOrder);
    EXPECT_EQ(8, (*iter++)->m_frameOrder);
    EXPECT_EQ(9, (*iter++)->m_frameOrder);
    EXPECT_EQ(10,(*iter++)->m_frameOrder);
    EXPECT_EQ(11,(*iter++)->m_frameOrder);
    EXPECT_EQ(12,(*iter++)->m_frameOrder);
    EXPECT_EQ(13,(*iter++)->m_frameOrder);
    EXPECT_EQ(14,(*iter++)->m_frameOrder);
    for (auto task: out)
        EXPECT_EQ(15, task->m_biFramesInMiniGop);

    param.GopClosedFlag = 0;
    param.GopStrictFlag = 0;
    param.BiPyramidLayers = 6;
    param.longGop = 1;
    SetupQueues("bbbbbbbbbbbbbbbbp"); // too many frames for long pyramid
    ReorderFrames(in, out, param, false);
    iter = out.begin();
    EXPECT_EQ(16, (*iter++)->m_frameOrder);
    EXPECT_EQ(7, (*iter++)->m_frameOrder);
    EXPECT_EQ(3, (*iter++)->m_frameOrder);
    EXPECT_EQ(1, (*iter++)->m_frameOrder);
    EXPECT_EQ(0, (*iter++)->m_frameOrder);
    EXPECT_EQ(2, (*iter++)->m_frameOrder);
    EXPECT_EQ(5, (*iter++)->m_frameOrder);
    EXPECT_EQ(4, (*iter++)->m_frameOrder);
    EXPECT_EQ(6, (*iter++)->m_frameOrder);
    EXPECT_EQ(11,(*iter++)->m_frameOrder);
    EXPECT_EQ(9, (*iter++)->m_frameOrder);
    EXPECT_EQ(8, (*iter++)->m_frameOrder);
    EXPECT_EQ(10,(*iter++)->m_frameOrder);
    EXPECT_EQ(13,(*iter++)->m_frameOrder);
    EXPECT_EQ(12,(*iter++)->m_frameOrder);
    EXPECT_EQ(14,(*iter++)->m_frameOrder);
    EXPECT_EQ(15,(*iter++)->m_frameOrder);
    for (auto task: out)
        EXPECT_EQ(16, task->m_biFramesInMiniGop);
}
