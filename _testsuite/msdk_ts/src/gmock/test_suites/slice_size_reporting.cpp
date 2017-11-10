/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2017 Intel Corporation. All Rights Reserved.

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
    mfxExtEncodedUnitsInfo *buff = NULL;

    for(mfxU16 i = 0; i < bs.NumExtParam; i++)
    {
        if (bs.ExtParam[i]->BufferId == MFX_EXTBUFF_ENCODED_UNITS_INFO)
            buff = (mfxExtEncodedUnitsInfo*)bs.ExtParam[i];
    }
    if (buff && buff->UnitInfo) {
        g_tsLog << "buff.NumUnitsAlloc = " << buff->NumUnitsAlloc << "\n";
        g_tsLog << "buff.NumUnitsEncoded = " << buff->NumUnitsEncoded << "\n";
        for (mfxU32 i = 0; i < buff->NumUnitsEncoded; i++) {
            g_tsLog << "buff.SliceSize[" << i << "] = " << buff->UnitInfo[i].Size << "\n";
        }
        g_tsLog << "bs.DataLength = " << bs.DataLength << "\n";
        if (buff->NumUnitsEncoded > buff->NumUnitsAlloc) {
            g_tsLog << "ERROR: NumUnitsEncoded is wrong!\nIt exceeds NumUnitsAlloc = " << buff->NumUnitsAlloc << "\n";
            return MFX_ERR_ABORTED;
        }
        if (buff->UnitInfo[0].Size == 0) {
            g_tsLog << "ERROR: Reported slice size is wrong!\nExpected max slice_size <= " << max_slice_size << "\nReal: UnitInfo[0].Size = " << buff->UnitInfo[0].Size << "\n";
            return MFX_ERR_ABORTED;
        }
        mfxU32 i, framesize = 0;
        for (i = 0; i < buff->NumUnitsEncoded; i++) {
            framesize += buff->UnitInfo[i].Size;
            if (buff->UnitInfo[i].Size > max_slice_size) {
                g_tsLog << "ERROR: Reported slice size is wrong!\nExpected max slice_size <= " << max_slice_size << "\nReal: UnitInfo[" << i << "].Size = " << buff->UnitInfo[i].Size << "\n";
                return MFX_ERR_ABORTED;
            }
        }
        if (framesize != bs.DataLength) {
            g_tsLog << "ERROR: bs.DataLength is not equal to the sum of slice sizes! (see data above)\n";
            return MFX_ERR_ABORTED;
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

    struct tc_struct
    {
        mfxStatus exp_sts;
        mfxU32 func;
        mfxU32 mode;
    };


// correct statuses. Change when feature will be developed

static const tc_struct test_case[] =
{
    { MFX_ERR_NONE, MFXQUERY, 0},
    { MFX_ERR_NONE, MFXINIT, 0},
    { MFX_ERR_NONE, MFXENCODE, 0}
};

const unsigned int n_cases = sizeof(test_case)/sizeof(tc_struct);
const unsigned int lcu_size = 64;

int RunTest (mfxU32 codecId, unsigned int id)
{
    TS_START;

    //int encoder
    tsVideoEncoder enc(codecId);
    enc.Init();

    ENCODE_CAPS_HEVC caps = {};
    mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);
    g_tsStatus.check(enc.GetCaps(&caps, &capSize));

    enc.GetVideoParam();
    enc.Close();
    // now mfxSession exists but Encoder is closed

    if (!caps.SliceByteSizeCtrl || !caps.SliceLevelReportSupport) {
        g_tsLog << "\n\nWARNING: SliceByteSizeCtrl or SliceLevelReportSupport is not supported in this platform. Test is skipped.\n\n\n";
        throw tsSKIP;
    }

    mfxU32 maxslices = ((enc.m_par.mfx.FrameInfo.Width + (lcu_size - 1)) / lcu_size) * ((enc.m_par.mfx.FrameInfo.Height + (lcu_size - 1)) / lcu_size);

    //set input stream
    const char* stream = g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12");
    g_tsStreamPool.Reg();
    tsRawReader reader = tsRawReader(stream, enc.m_par.mfx.FrameInfo);
    enc.m_filler = (tsSurfaceProcessor*)(&reader);

    mfxExtCodingOption2 CO2 = {0};
    CO2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    CO2.Header.BufferSz = sizeof(mfxExtCodingOption2);
    CO2.MaxSliceSize = 1000;  // enable Dynamic Slice Size

    mfxExtCodingOption3 CO3 = {0};
    CO3.Header.BufferId = MFX_EXTBUFF_CODING_OPTION3;
    CO3.Header.BufferSz = sizeof(mfxExtCodingOption3);
    CO3.EncodedUnitsInfo = MFX_CODINGOPTION_ON;  // enable Reporting

    mfxExtBuffer *pBuf[2];
    pBuf[0] = (mfxExtBuffer*)&CO3;
    pBuf[1] = (mfxExtBuffer*)&CO2;
    enc.m_par.ExtParam = pBuf;
    enc.m_par.NumExtParam = 2;
    
    const tc_struct& tc = test_case[id];
    g_tsStatus.expect(tc.exp_sts);

    if (tc.func == MFXQUERY) {
        enc.Query();
    } else  if (tc.func == MFXINIT) {
        enc.Init();
    } else {

        //set bs checker
        g_tsLog << "Set CO2.MaxSliceSize = " << CO2.MaxSliceSize << "\n";
        g_tsLog << "Set maxslices = " << maxslices << "\n";

        BitstreamChecker bs_check(CO2.MaxSliceSize, maxslices);
        enc.m_bs_processor = &bs_check;

        enc.m_pPar->AsyncDepth = 1;     // sergo: WA for CNL
        g_tsStatus.check(enc.Init());

        mfxExtEncodedUnitsInfo UnitInfo = { 0 };
        std::vector <mfxEncodedUnitInfo> SliceInfo;
        UnitInfo.Header.BufferId = MFX_EXTBUFF_ENCODED_UNITS_INFO;
        UnitInfo.Header.BufferSz = sizeof(mfxExtEncodedUnitsInfo);

        SliceInfo.resize(maxslices);
        UnitInfo.UnitInfo = &SliceInfo[0];
        UnitInfo.NumUnitsAlloc = (mfxU16)SliceInfo.size();

        mfxExtBuffer* bsExtBuf = (mfxExtBuffer*)&UnitInfo;
        enc.m_bitstream.ExtParam = &bsExtBuf;    //set ext buff in bs
        enc.m_bitstream.NumExtParam = 1;

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