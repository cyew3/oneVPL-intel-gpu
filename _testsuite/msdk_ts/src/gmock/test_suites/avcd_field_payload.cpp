/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"

namespace avcd_field_payload
{

class TestSuite : tsVideoDecoder, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_AVC) { m_surf_processor = this; }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxU8 myData[1024];
        mfxPayload myPayload;
        myPayload.Data = myData;

        bool myFlag = true;

        while (1)
        {
            myPayload.BufSize = sizeof(myData);
            myPayload.NumBit = sizeof(myData)*8;
            mfxU64 myTime;

            GetPayload(m_session, &myTime, &myPayload);
            if (myPayload.NumBit == 0)
                break;

            if (myPayload.NumBit >= 12*8) {
                if (( myFlag && myData[12] != 0xfc) ||
                    (!myFlag && myData[12] != 0xfd))
                {
                    g_tsLog << "ERROR: out of order Payload data\n";
                    return MFX_ERR_ABORTED;
                }

                myFlag = !myFlag;
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
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/
     "Issue: VCSD100022649. Test checks that payloads are returned in order of top/bottom fields",
     MFX_ERR_NONE, "forBehaviorTest/customer_issues/avc_1080i_CC.264", 692}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
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

TS_REG_TEST_SUITE_CLASS(avcd_field_payload);
}
