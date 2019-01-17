/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2019 Intel Corporation. All Rights Reserved.

File Name: hevce_slice_size_report.cpp

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_struct.h"

namespace hevce_slice_size_report
{
class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
    ~TestSuite() { }

    struct tc_struct
    {
        mfxStatus exp_sts;
        mfxU32 func;
        mfxU32 mode;
    };

    template<mfxU32 fourcc>
    int RunTest_Subtype(const unsigned int id);
    int RunTest(tc_struct tc, unsigned int fourcc_id);

    static const unsigned int n_cases;

private:

    enum
    {
        MFXINIT = 1,
        MFXQUERY = 2,
        MFXENCODE = 3
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    { MFX_ERR_NONE, MFXQUERY, 0 },
    { MFX_ERR_NONE, MFXINIT, 0 },
    { MFX_ERR_NONE, MFXENCODE, 0 }
};


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

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

struct streamDesc
{
    mfxU16 w;
    mfxU16 h;
    mfxU32 FourCC;
    mfxU16 ChromaFormat;
    mfxU16 BitDepthLuma;
    mfxU16 Shift;
    mfxU16 CodecProfile;
    const char *name;

};
const streamDesc streams[2][4] = {
    {
        {1920,1088,MFX_FOURCC_NV12, MFX_CHROMAFORMAT_YUV420, 8 , 0, MFX_PROFILE_HEVC_MAIN,   "forBehaviorTest/Kimono1_1920x1088_30_content_1920x1080.yuv" },                //NV12 nosim
        {1920,1088,MFX_FOURCC_P010, MFX_CHROMAFORMAT_YUV420, 10, 1, MFX_PROFILE_HEVC_MAIN10, "forBehaviorTest/Kimono1_1920x1088_24_p010_shifted_30_content_1920x1080.yuv"}, //P010 nosim
        {1920,1088,MFX_FOURCC_AYUV, MFX_CHROMAFORMAT_YUV444, 8 , 0, MFX_PROFILE_HEVC_REXT,   "forBehaviorTest/Kimono1_1920x1088_24_ayuv_30_content_1920x1080.yuv"},         //AYUV nosim
        {1920,1088,MFX_FOURCC_Y410, MFX_CHROMAFORMAT_YUV444, 10, 0, MFX_PROFILE_HEVC_REXT,   "forBehaviorTest/Kimono1_1920x1088_24_y410_30_content_1920x1080.yuv"},         //Y410 nosim
    },
    {
        {352, 288, MFX_FOURCC_NV12, MFX_CHROMAFORMAT_YUV420, 8 , 0, MFX_PROFILE_HEVC_MAIN,   "forBehaviorTest/foreman_cif.nv12"},                       //NV12 sim
        {352, 288, MFX_FOURCC_P010, MFX_CHROMAFORMAT_YUV420, 10, 1, MFX_PROFILE_HEVC_MAIN10, "forBehaviorTest/Kimono1_352x288_24_p010_shifted_50.yuv"}, //P010 sim
        {352, 288, MFX_FOURCC_AYUV, MFX_CHROMAFORMAT_YUV444, 8 , 0, MFX_PROFILE_HEVC_REXT,   "forBehaviorTest/Kimono1_352x288_24_ayuv_50.yuv"},         //AYUV sim
        {352, 288, MFX_FOURCC_Y410, MFX_CHROMAFORMAT_YUV444, 10, 0, MFX_PROFILE_HEVC_REXT,   "forBehaviorTest/Kimono1_352x288_24_y410_50.yuv"},         //Y410 sim
    }
};

const streamDesc& getStreamDesc(const unsigned int id, bool sim)
{
    switch (id)
    {
    case MFX_FOURCC_NV12: return streams[sim][0];
    case MFX_FOURCC_P010: return streams[sim][1];
    case MFX_FOURCC_AYUV: return streams[sim][2];
    case MFX_FOURCC_Y410: return streams[sim][3];
    }
}

template<mfxU32 fourcc>
int TestSuite::RunTest_Subtype(const unsigned int id)
{
    const tc_struct& tc = test_case[id];
    return RunTest(tc, fourcc);
}

int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
{
    TS_START;

    MFXInit();

    ENCODE_CAPS_HEVC caps = {};
    mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);
    mfxStatus sts = GetCaps(&caps, &capSize);

    const unsigned int lcu_size = (caps.LCUSizeSupported & 4) ? 64 : ((caps.LCUSizeSupported & 2) ? 32 : 16);

    if (!caps.SliceByteSizeCtrl || !caps.SliceLevelReportSupport || (sts == MFX_ERR_UNSUPPORTED)) {
        g_tsLog << "\n\nWARNING: SliceByteSizeCtrl or SliceLevelReportSupport is not supported in this platform. Test is skipped.\n\n\n";
        throw tsSKIP;
    }
    g_tsStatus.check(sts);

    const streamDesc& desc = getStreamDesc(fourcc_id, g_tsConfig.sim);
    const char* stream                 = g_tsStreamPool.Get(desc.name);
    m_par.mfx.FrameInfo.Width          = m_par.mfx.FrameInfo.CropW = desc.w;
    m_par.mfx.FrameInfo.Height         = m_par.mfx.FrameInfo.CropH = desc.h;
    m_par.mfx.FrameInfo.FourCC         = desc.FourCC;
    m_par.mfx.FrameInfo.ChromaFormat   = desc.ChromaFormat;
    m_par.mfx.FrameInfo.BitDepthLuma   = desc.BitDepthLuma;
    m_par.mfx.FrameInfo.BitDepthChroma = desc.BitDepthLuma;
    m_par.mfx.FrameInfo.Shift          = desc.Shift;
    m_par.mfx.CodecProfile             = desc.CodecProfile;

    mfxU32 maxslices = ((m_par.mfx.FrameInfo.Width + (lcu_size - 1)) / lcu_size) * ((m_par.mfx.FrameInfo.Height + (lcu_size - 1)) / lcu_size);

    g_tsStreamPool.Reg();
    tsRawReader reader = tsRawReader(stream, m_par.mfx.FrameInfo);
    m_filler = (tsSurfaceProcessor*)(&reader);

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
    m_par.ExtParam = pBuf;
    m_par.NumExtParam = 2;

    g_tsStatus.expect(tc.exp_sts);

    if (tc.func == MFXQUERY) {
        Query();
    } else  if (tc.func == MFXINIT) {
        Init();
    } else {

        //set bs checker
        g_tsLog << "Set CO2.MaxSliceSize = " << CO2.MaxSliceSize << "\n";
        g_tsLog << "Set maxslices = " << maxslices << "\n";

        BitstreamChecker bs_check(CO2.MaxSliceSize, maxslices);
        m_bs_processor = &bs_check;

        if (g_tsConfig.sim)
            m_pPar->AsyncDepth = 1;
        else
            m_pPar->AsyncDepth = 4;
        g_tsStatus.check(Init());

        mfxExtEncodedUnitsInfo UnitInfo = { 0 };
        std::vector <mfxEncodedUnitInfo> SliceInfo;
        UnitInfo.Header.BufferId = MFX_EXTBUFF_ENCODED_UNITS_INFO;
        UnitInfo.Header.BufferSz = sizeof(mfxExtEncodedUnitsInfo);

        SliceInfo.resize(maxslices);
        UnitInfo.UnitInfo = &SliceInfo[0];
        UnitInfo.NumUnitsAlloc = (mfxU16)SliceInfo.size();

        mfxExtBuffer* bsExtBuf = (mfxExtBuffer*)&UnitInfo;
        m_bitstream.ExtParam = &bsExtBuf;    //set ext buff in bs
        m_bitstream.NumExtParam = 1;

        EncodeFrames(1);
    }

    TS_END;

    return 0;
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_slice_size_report, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_slice_size_report, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_slice_size_report, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_slice_size_report, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
}