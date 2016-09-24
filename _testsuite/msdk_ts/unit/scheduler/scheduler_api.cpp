/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "gtest/gtest.h"

#include "mfx_scheduler_core.h"
#include "libmfx_core_interface.h"

#define REPEATS_NUM 10

class ApiTest : public ::testing::Test {
protected:
    ApiTest() {
        core_ = new mfxSchedulerCore;
    }

    ~ApiTest() {
        core_->Release();
    }

    mfxSchedulerCore* core_;
};

TEST_F(ApiTest, GetNumRef) {
  EXPECT_EQ(1, core_->GetNumRef());
}

TEST_F(ApiTest, Query) {
    struct {
        const MFX_GUID guid;
        bool supported;
    } guid_table[] =
    {
        { MFXIScheduler_GUID, true },
        { MFXIScheduler2_GUID, true },
        { MFXIVideoCORE_GUID, false },
    };

    for (size_t i=0; i < sizeof(guid_table)/sizeof(guid_table[0]); ++i) {
        MFXIUnknown* unk = (MFXIUnknown*)core_->QueryInterface(guid_table[i].guid);
        if (guid_table[i].supported) {
          EXPECT_EQ(2, core_->GetNumRef());
        } else {
          EXPECT_EQ(1, core_->GetNumRef());
        }
        if (unk) unk->Release();
    }
}

TEST_F(ApiTest, InitReset) {
    mfxStatus sts;

    for (int i=0; i < REPEATS_NUM; ++i) {
        sts = core_->Initialize(NULL);
        EXPECT_EQ(MFX_ERR_NONE, sts);
    }
    for (int i=0; i < REPEATS_NUM; ++i) {
        sts = core_->Initialize(NULL);
        EXPECT_EQ(MFX_ERR_NONE, sts);
        sts = core_->Reset();
        EXPECT_EQ(MFX_ERR_NONE, sts);
    }
}

TEST_F(ApiTest, AddNullTask) {
  mfxStatus sts;
  MFX_TASK task;
  mfxSyncPoint syncp;

  memset(&task, 0, sizeof(task));

  sts = core_->AddTask(task, NULL);
  EXPECT_EQ(MFX_ERR_NOT_INITIALIZED, sts);

  sts = core_->Initialize(NULL);
  EXPECT_EQ(MFX_ERR_NONE, sts);

  sts = core_->AddTask(task, NULL);
  EXPECT_EQ(MFX_ERR_NULL_PTR, sts);

  sts = core_->AddTask(task, &syncp);
  EXPECT_EQ(MFX_ERR_NULL_PTR, sts);

  sts = core_->Reset();
  EXPECT_EQ(MFX_ERR_NONE, sts);
}

TEST_F(ApiTest, DoWork) {
  mfxStatus sts;

  sts = core_->DoWork();
  EXPECT_EQ(MFX_ERR_UNSUPPORTED, sts);

  sts = core_->Initialize(NULL);
  EXPECT_EQ(MFX_ERR_NONE, sts);

  sts = core_->DoWork();
  EXPECT_EQ(MFX_ERR_UNSUPPORTED, sts);

  sts = core_->Reset();
  EXPECT_EQ(MFX_ERR_NONE, sts);
}
