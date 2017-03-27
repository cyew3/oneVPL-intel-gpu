/******************************************************************************* *\

Copyright (C) 2017 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include "ts_pak.h"
#include "ts_struct.h"

namespace fei_pak_queryiosurf
{

    class TestSuite : tsVideoPAK
    {
      public:
        TestSuite() : tsVideoPAK(MFX_FEI_FUNCTION_PAK, MFX_CODEC_AVC, true) {}
        ~TestSuite() {}
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

      private:

        enum
        {
            MFXPAR = 1,
        };

        struct tc_struct
        {
            mfxStatus sts_queryiosurf;

            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        /*00*/ {MFX_ERR_NONE,                 {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}},
        /*01*/ {MFX_ERR_INVALID_VIDEO_PARAM,  {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}},
        /*02*/ {MFX_ERR_INVALID_VIDEO_PARAM,  {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY}},
        /*03*/ {MFX_ERR_NONE,                 {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 1}},
        /*04*/ {MFX_ERR_INVALID_VIDEO_PARAM,  {MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 4}},
        /*05*/ {MFX_ERR_NONE,                {{MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
                                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame , 2}}},
        /*06*/ {MFX_ERR_NONE,                {{MFXPAR, &tsStruct::mfxVideoParam.AsyncDepth, 1},
                                              {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame , 4}}},
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];

        MFXInit();

        SETPARS(m_pPar, MFXPAR);

        SetFrameAllocator(m_session, m_pVAHandle);

        /*******************QueryIOSurf() test**********************/
        g_tsStatus.expect(tc.sts_queryiosurf);
        QueryIOSurf();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(fei_pak_queryiosurf);
}

