/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2016 Intel Corporation. All Rights Reserved.

File Name: slice_size_reporting.cpp

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_struct.h"

namespace slice_size_reporting
{

class BitstreamChecker : public tsBitstreamProcessor
{
    private:
        mfxU32 max_slice_size;
        mfxU32 max_nslices;

    public:
        BitstreamChecker(mfxU32 size, mfxU32 num)
        {
            max_slice_size = size;
            max_nslices = num;
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};

mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    mfxExtEncodedSlicesInfo *buff = NULL;

    for(mfxU16 i = 0; i < bs.NumExtParam; i++)
    {
        if (bs.ExtParam[i]->BufferId == MFX_EXTBUFF_ENCODED_SLICES_INFO)
            buff = (mfxExtEncodedSlicesInfo*)bs.ExtParam[i];
    }
    if (buff && buff->SliceSize) {
        if (buff->SliceSize[0] == 0) {
            g_tsLog << "ERROR: Reported slice size is wrong!\nExpected max slice_size = " << max_slice_size << "\nReal: slice_size[0] = " << buff->SliceSize[0] << "\n";
            return MFX_ERR_ABORTED;
        }
        mfxU32 i;
        for (i = 0; i < max_nslices; i++) {
            if ((buff->SliceSize[i] == 0) && (i > 0))
                break;  // real num slices can be less
            if (buff->SliceSize[i] > max_slice_size) {
                g_tsLog << "ERROR: Reported slice size is wrong!\nExpected max slice_size = " << max_slice_size << "\nReal: slice_size[" << i << "] = " << buff->SliceSize[i] << "\n";
                return MFX_ERR_ABORTED;
            }
        }
        g_tsLog << "PASSED: " << i << " slices of size from 0 to " << max_slice_size << " were generated\n";

        return MFX_ERR_NONE;
    }
    else
    {
        g_tsLog << "ERROR: Buffer ENCODED_SLICES_INFO not found!\n";
        return MFX_ERR_ABORTED;
    }
    return MFX_ERR_NONE;
}

    enum
    {
        MFXINIT = 1,
        MFXQUERY = 2,
        MFXENCODE = 3
    };

    enum
    {
        NOCO2 = 1,
        NOCO2MAXSLICESIZE = 2
    };

    struct tc_struct
    {
        mfxStatus exp_sts;
        mfxU32 func;
        mfxU32 mode;
    };

static const tc_struct test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, MFXQUERY, 0},
    {/* 1*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFXQUERY, NOCO2},
    {/* 2*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFXQUERY, NOCO2MAXSLICESIZE},
    {/* 3*/ MFX_ERR_NONE, MFXINIT, 0},
    {/* 4*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFXINIT, NOCO2},
    {/* 5*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFXINIT, NOCO2MAXSLICESIZE},
    {/* 6*/ MFX_ERR_NONE, MFXENCODE, 0}
};

const unsigned int n_cases = sizeof(test_case)/sizeof(tc_struct);
const unsigned int lcu_size = 64;

int RunTest (mfxU32 codecId, unsigned int id)
{
    TS_START;

    //int encoder
    tsVideoEncoder enc(codecId);

    enc.Init();
    enc.GetVideoParam();
    enc.Close();
    // now mfxSession exists but Encoder is closed

    mfxU32 nslices = ((enc.m_par.mfx.FrameInfo.Width + (lcu_size - 1)) / lcu_size) * ((enc.m_par.mfx.FrameInfo.Height + (lcu_size - 1)) / lcu_size);

    //set input stream
    const char* stream = g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12");
    g_tsStreamPool.Reg();
    tsRawReader reader = tsRawReader(stream, enc.m_par.mfx.FrameInfo);
    enc.m_filler = (tsSurfaceProcessor*)(&reader);

    int exp_slice_size[2] = {1000, 1000};

    mfxExtEncodedSlicesInfo SlicesInfo = {0};
    mfxU16 SSize[16] = {0};
    SlicesInfo.Header.BufferId = MFX_EXTBUFF_ENCODED_SLICES_INFO;
    SlicesInfo.Header.BufferSz = sizeof(mfxExtEncodedSlicesInfo);
    SlicesInfo.SliceSize = SSize;
    SlicesInfo.NumSliceSizeAlloc = sizeof(SlicesInfo.SliceSize);

    mfxExtCodingOption2 CO2 = {0};
    CO2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    CO2.Header.BufferSz = sizeof(mfxExtCodingOption2);
    CO2.MaxSliceSize = 1000;

    mfxExtBuffer *pBuf[2];
    pBuf[0] = (mfxExtBuffer*)&SlicesInfo;
    pBuf[1] = (mfxExtBuffer*)&CO2;
    enc.m_par.ExtParam = pBuf;
    enc.m_par.NumExtParam = 2;
    
    const tc_struct& tc = test_case[id];
    g_tsStatus.expect(tc.exp_sts);

    if (tc.func == MFXQUERY) {
        if (tc.mode == NOCO2) {
            pBuf[1] = 0;
            enc.m_par.NumExtParam--;
        }
        if (tc.mode == NOCO2MAXSLICESIZE) {
            CO2.MaxSliceSize = 0;
        }
        enc.Query();
    } else  if (tc.func == MFXINIT) {
        if (tc.mode == NOCO2) {
            pBuf[1] = 0;
            enc.m_par.NumExtParam--;
        }
        if (tc.mode == NOCO2MAXSLICESIZE) {
            CO2.MaxSliceSize = 0;
        }
        enc.Init();
    } else {

        //set bs checker
        BitstreamChecker bs_check(CO2.MaxSliceSize, nslices);
        enc.m_bs_processor = &bs_check;

        //default pipeline
        enc.Init();

        enc.m_bitstream.ExtParam = pBuf;    //set ext buff in bs
        enc.m_bitstream.NumExtParam = 2;

        enc.EncodeFrames(1);
    }

    TS_END;

    return 0;
}

int AVCTest     (unsigned int id) { return RunTest(MFX_CODEC_AVC, id); }
int MPEG2Test   (unsigned int id) { return RunTest(MFX_CODEC_MPEG2, id); }
int HEVCTest    (unsigned int id) { return RunTest(MFX_CODEC_HEVC, id); }

TS_REG_TEST_SUITE(hevce_slice_size_report, HEVCTest, n_cases);
TS_REG_TEST_SUITE(avce_slice_size_report, AVCTest, n_cases);
TS_REG_TEST_SUITE(mpeg2e_slice_size_report, MPEG2Test, n_cases);
}