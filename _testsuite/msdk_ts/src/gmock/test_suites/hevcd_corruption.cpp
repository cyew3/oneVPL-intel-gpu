/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2017 Intel Corporation. All Rights Reserved.
//
//
*/

#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

#define TEST_NAME hevcd_corruption
#define NSTREAMS 5

namespace TEST_NAME
{

/* Corrupted in mfxFrameData */
enum {
    MINOR     = MFX_CORRUPTION_MINOR              ,
    MAJOR     = MFX_CORRUPTION_MAJOR              ,
    ABSENT_TF = MFX_CORRUPTION_ABSENT_TOP_FIELD   ,
    ABSENT_BF = MFX_CORRUPTION_ABSENT_BOTTOM_FIELD,
    REF_FRAME = MFX_CORRUPTION_REFERENCE_FRAME    ,
    REF_LIST  = MFX_CORRUPTION_REFERENCE_LIST     
};

class TestSuite : public tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_HEVC) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    struct f_pair
    {
        const  tsStruct::Field* f;
        mfxU64 v;
    };

private:
    struct tc_struct
    {
        struct 
        {
            mfxU32       n;
            const char*  name;
            mfxU8 flags[32];
        } stream;
    };

    static const tc_struct test_case[];
};

struct mfxCorrupted {
    mfxU16 v;
    mfxCorrupted(mfxU16 value){v = value;};
};
std::ostream& operator<< (std::ostream &out, const mfxCorrupted &Corrupted) {
    if(Corrupted.v == 0        ) out << "0";
    if(Corrupted.v == MINOR    ) out << MINOR     << "(MFX_CORRUPTION_MINOR) ";
    if(Corrupted.v == MAJOR    ) out << MAJOR     << "(MFX_CORRUPTION_MAJOR) ";
    if(Corrupted.v == ABSENT_TF) out << ABSENT_TF << "(MFX_CORRUPTION_ABSENT_TOP_FIELD) ";
    if(Corrupted.v == ABSENT_BF) out << ABSENT_BF << "(MFX_CORRUPTION_ABSENT_BOTTOM_FIELD) ";
    if(Corrupted.v == REF_FRAME) out << REF_FRAME << "(MFX_CORRUPTION_REFERENCE_FRAME) ";
    if(Corrupted.v == REF_LIST ) out << REF_LIST  << "(MFX_CORRUPTION_REFERENCE_LIST)";
    return out;
}

class Verifier : public tsSurfaceProcessor
{
public:
    const tsExtBufType<mfxVideoParam>& init_par;
    const mfxSession m_session;
    mfxU32 count;
    std::list <mfxU16> expected_flag;
    mfxU16 frame;

    Verifier(const mfxU8* flags, const mfxU16 N, 
             const tsExtBufType<mfxVideoParam>& par, mfxU64 start = 0, 
             mfxU64 step = 0, const mfxSession _session = 0)
        : tsSurfaceProcessor()
        , init_par(par)
        , m_session(_session)
        , count(0)
        , expected_flag(flags, flags + N)
        , frame(0)
    { }

    ~Verifier() {}

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        const mfxU16 Corrupted = s.Data.Corrupted;
        if (expected_flag.empty())
            return MFX_ERR_NONE;
        const mfxU16 Expected = expected_flag.front();
        expected_flag.pop_front();
        EXPECT_EQ(Expected, Corrupted) << "ERROR: per-frame reported corrupted value != expected\n" << 
           "Frame# " << frame << " Corrupted value: " << mfxCorrupted(Corrupted) << "; Expected: " << mfxCorrupted(Expected) << "\n";

        count++;

#define CHECK_FIELD_EXP_ACT(EXPECTED, ACTUAL, FIELD) \
do {                                                 \
    if(ACTUAL.FIELD && EXPECTED.FIELD){              \
        EXPECT_EQ(EXPECTED.FIELD, ACTUAL.FIELD);     \
    }                                                \
} while (0,0)

#define CHECK_FIELD(FIELD)    CHECK_FIELD_EXP_ACT(init_par.mfx.FrameInfo, s.Info, FIELD)
        CHECK_FIELD(BitDepthLuma  );
        CHECK_FIELD(BitDepthChroma);
        CHECK_FIELD(Shift         );
        CHECK_FIELD(FourCC        );
        CHECK_FIELD(Width         );
        CHECK_FIELD(Height        );
        CHECK_FIELD(CropX         );
        CHECK_FIELD(CropY         );
        CHECK_FIELD(CropW         );
        CHECK_FIELD(CropH         );
        CHECK_FIELD(FrameRateExtN );
        CHECK_FIELD(FrameRateExtD );
        CHECK_FIELD(AspectRatioW  );
        CHECK_FIELD(AspectRatioH  );
        CHECK_FIELD(PicStruct     );
        CHECK_FIELD(ChromaFormat  );
#undef CHECK_FIELD

        if(m_session)
        {
            mfxVideoParam GetVideoParamPar = {};

            TRACE_FUNC2(MFXVideoDECODE_GetVideoParam, m_session, &GetVideoParamPar);
            g_tsStatus.expect(MFX_ERR_NONE);
            g_tsStatus.check( MFXVideoDECODE_GetVideoParam(m_session, &GetVideoParamPar) );

#define CHECK_FIELD(FIELD)    CHECK_FIELD_EXP_ACT(GetVideoParamPar.mfx.FrameInfo, s.Info, FIELD)
            CHECK_FIELD(BitDepthLuma  );
            CHECK_FIELD(BitDepthChroma);
            CHECK_FIELD(Shift         );
            CHECK_FIELD(FourCC        );
            CHECK_FIELD(Width         );
            CHECK_FIELD(Height        );
            CHECK_FIELD(CropX         );
            CHECK_FIELD(CropY         );
            CHECK_FIELD(CropW         );
            CHECK_FIELD(CropH         );
            CHECK_FIELD(FrameRateExtN );
            CHECK_FIELD(FrameRateExtD );
            CHECK_FIELD(AspectRatioW  );
            CHECK_FIELD(AspectRatioH  );
            CHECK_FIELD(PicStruct     );
            CHECK_FIELD(ChromaFormat  );
#undef CHECK_FIELD
#undef CHECK_FIELD_EXP_ACT

            tsExtBufType<mfxVideoParam> par;
            mfxExtCodingOptionSPSPPS&   spspps = par;

            mfxU8 spsBuf[512] = {0};
            mfxU8 ppsBuf[512] = {0};
            spspps.SPSBuffer = spsBuf;
            spspps.PPSBuffer = ppsBuf;
            spspps.SPSBufSize = sizeof(spsBuf);
            spspps.PPSBufSize = sizeof(ppsBuf);

            TRACE_FUNC2(MFXVideoDECODE_GetVideoParam, m_session, &par);
            g_tsStatus.expect(MFX_ERR_NONE);
            g_tsStatus.check( MFXVideoDECODE_GetVideoParam(m_session, &par) );

            mfxDecodeStat stat = {};

            TRACE_FUNC2(MFXVideoDECODE_GetDecodeStat, m_session, &stat);
            g_tsStatus.expect(MFX_ERR_NONE);
            g_tsStatus.check( MFXVideoDECODE_GetDecodeStat(m_session, &stat) );

            EXPECT_EQ(count, stat.NumFrame) << "ERROR: number of decoded frames reported by GetDecodeStat != actual\n";
        }

        frame++;
        return MFX_ERR_NONE;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    //manually corrupted streams with corruptions in slice data
    {/*00*/ {12, "forBehaviorTest/corrupted/hevc/AMP_F_Hisilicon_3_I.bit" , {MAJOR,REF_FRAME,REF_FRAME,REF_FRAME,REF_FRAME,REF_FRAME,
                                                                             REF_FRAME,REF_FRAME,REF_FRAME,REF_FRAME,REF_FRAME,REF_FRAME,REF_FRAME},},},
    {/*01*/ {6, "forBehaviorTest/corrupted/hevc/AMP_F_Hisilicon_3_firstB.bit" , {0,MAJOR,REF_FRAME,REF_FRAME,REF_FRAME,REF_FRAME},},},
    {/*02*/ {6, "forBehaviorTest/corrupted/hevc/AMP_F_Hisilicon_3_lastB.bit" , {0,0,0,0,0,MAJOR},},},

    // NOTE - Gen9 HW does not set the corrupt flags correctly unless the error occurs in the last slice (reset after each slice).
    //   Because the errors in this bistream are in slice 1 of 4, the corrupt flags will all be 0 for SKL/BXT/KBL/CFL.
    //   See MDP-17699, MDP-27066, MDP-32230, HSD-1208699222
    //
    //   When/if this is fixed, this test is expected to fail and the expected values for these flags will need to be updated as follows:
    //   {0, REF_FRAME, MAJOR, REF_FRAME, 0, 0, 0, 0, 0, REF_FRAME, REF_FRAME, REF_FRAME, MAJOR}
    //   This bitream has errors in frame 2 (ref for 1,3) and in frame 12 (ref for 9,10,11)
    {/*03*/ {13, "forBehaviorTest/corrupted/hevc/4K_BodyPainting_10M_1B3P.265" , {0,0,0,0,0,0,0,0,0,0,0,0,0},},},

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    const char* sname = g_tsStreamPool.Get(tc.stream.name);
    g_tsStreamPool.Reg();

    tsBitstreamReader reader(sname, 100000);
    m_bs_processor = &reader;

    Init();

    SetPar4_DecodeFrameAsync();

    Verifier v(tc.stream.flags, tc.stream.n, m_par);
    m_surf_processor = &v;

    DecodeFrames(tc.stream.n);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(TEST_NAME);
#undef TEST_NAME

}