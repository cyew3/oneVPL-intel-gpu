/******************************************************************************* *\

Copyright (C) 2016 Intel Corporation.  All rights reserved.

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

#include "ts_preenc.h"
#include "ts_struct.h"

namespace fei_preenc_memory_init
{
    class TestSuite : tsVideoPreENC
    {
        public:
        TestSuite() : tsVideoPreENC(MFX_FEI_FUNCTION_PREENC, MFX_CODEC_AVC, true) {}
        ~TestSuite() {}
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

        private:
        enum
        {
            NULL_SESSION             = 0x1,
            NULL_PARAMS              = 0x2,
            MFX_MEMTYPE_FROM_INVALID = 0x3,
        };


        struct tc_struct
        {
            mfxStatus sts_alloc;
            mfxStatus sts_init;
            mfxU32 mode;
            struct f_pair
            {
                mfxU16 type;
                mfxU16 num_frame_min;
                mfxU16 num_frame_suggested;
            } set_par;
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, MFX_ERR_INVALID_HANDLE, NULL_SESSION, {MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_ENC, 4, 4}},        
        {/*00*/ MFX_ERR_MEMORY_ALLOC, MFX_ERR_NONE, NULL_PARAMS},
        // Memory type cases
        {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_ENC, 4, 4}},
        {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_PAK, 4, 4}},
        {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_ENCODE, 4, 4}},
        {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_DECODE, 4, 4}},
        {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPIN, 4, 4}},
        {/*06*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT, 4, 4}},
        {/*07*/ MFX_ERR_UNSUPPORTED, MFX_ERR_NONE, 0, {MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_INVALID, 4, 4}},
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];
        m_impl = MFX_IMPL_HARDWARE;
        m_par.IOPattern =  MFX_IOPATTERN_IN_VIDEO_MEMORY;
        m_pPar->AsyncDepth = 1; //limitation for FEI (from sample_fei)

        if (!(tc.mode & NULL_SESSION))
        {
            MFXInit();
        }
  
        if (!(tc.mode & NULL_SESSION))
        {
            SetFrameAllocator(m_session, m_pVAHandle);
        }

        // set up parameters for case
        mfxFrameAllocRequest preenc_request;
        MSDK_ZERO_MEMORY(preenc_request);
        MSDK_MEMCPY_VAR(preenc_request.Info, &(m_pPar->mfx.FrameInfo), sizeof(mfxFrameInfo));
        preenc_request.AllocId = (mfxU64)&m_session & 0xffffffff;
        preenc_request.Type = tc.set_par.type;
        preenc_request.NumFrameMin = tc.set_par.num_frame_min;
        preenc_request.NumFrameSuggested = tc.set_par.num_frame_suggested;

        if (tc.mode & NULL_PARAMS)
        {
            m_pRequest = 0;
        }
        else
        {
            m_request = preenc_request;
        }

        //Create extended buffer to Init FEI
        mfxExtFeiParam preenc_init;
        MSDK_ZERO_MEMORY(preenc_init);
        preenc_init.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        preenc_init.Header.BufferSz = sizeof (mfxExtFeiParam);
        preenc_init.Func = MFX_FEI_FUNCTION_PREENC;
        preenc_init.SingleFieldProcessing = MFX_CODINGOPTION_ON;

        mfxExtBuffer* buf[1];
        buf[0] = (mfxExtBuffer*)&preenc_init;

        m_par.NumExtParam = 1;
        m_par.ExtParam    = buf;
        m_par.AllocId     = (mfxU64)&m_session & 0xffffffff;

        /*******************Alloc() and Init() test**********************/
        g_tsStatus.expect(tc.sts_alloc);
        m_default = false;
        AllocSurfaces();
        m_default = true;
        g_tsStatus.expect(tc.sts_init);
        Init(m_session, m_pPar);

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(fei_preenc_memory_init);
}
