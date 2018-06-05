/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"

namespace avcd_payload_size
{

class TestSuite : tsVideoDecoder, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_AVC) { m_surf_processor = this; }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;
    unsigned int payload_size_ref = 0;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        while (1)
        {
            mfxU8 myData[1024];

            mfxPayload myPayload;
            myPayload.Data = myData;
            myPayload.BufSize = sizeof(myData);
            myPayload.NumBit = sizeof(myData)*8;

            mfxU64 myTime;
            mfxStatus sts;

            sts = GetPayload(m_session, &myTime, &myPayload);

            if (sts != MFX_ERR_NONE)
            {
                g_tsLog << "ERROR: payload is not received\n";
                return MFX_ERR_ABORTED;
            }

            if (myPayload.NumBit == 0)
            {
                g_tsLog << "ERROR: closed caption SEI not found\n";
                return MFX_ERR_ABORTED;
            }

            if (myPayload.Type == 4)
            {
                mfxU32 payload_size_actual = myPayload.NumBit/8;
                EXPECT_EQ(payload_size_actual, payload_size_ref) << "ERROR: payload has wrong size\n";
                break;
            }
        }
        return MFX_ERR_NONE;
    }

private:
    struct tc_struct
    {
        std::string desc;
        mfxStatus sts;
        std::string stream;
        mfxU32 nframes;
        mfxU32 payload_size;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/
     /* 67 = 57 + 10, where 57 is size of clean CC data and 10 is:
        - itu_t_t35_country_code: 0xB5
        - itu_t_t35_provider_code: 0x00, 0x31
        - user_identifier (DVB1_data): 0x47, 0x41, 0x39, 0x34
        - user_data_type_code: 0x00 to 0x02 - reserved */
      "Issue: MDP-31344. Test checks, that decoder correctly outputs SEI, containing closed caption.",
      MFX_ERR_NONE, "forBehaviorTest/customer_issues/SeiPictTiming.h264", 1378, 67}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    payload_size_ref = tc.payload_size;
    g_tsLog << "Test description: " << tc.desc << "\n";

    MFXInit();

    const char* sname = g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 100000);
    m_bs_processor = &reader;

    Init();
    AllocSurfaces();

    g_tsStatus.expect(tc.sts);
    DecodeFrames(tc.nframes);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avcd_payload_size);
}
