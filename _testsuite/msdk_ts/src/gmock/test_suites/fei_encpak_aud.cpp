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

#include "ts_encpak.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include "ts_fei_warning.h"
#include "avc2_struct.h"

namespace fei_encpak_aud
{

class TestSuite
{
public:
    tsVideoENCPAK encpak;

    TestSuite() : encpak(MFX_FEI_FUNCTION_ENC, MFX_FEI_FUNCTION_PAK, MFX_CODEC_AVC, true) {} // fix function id
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    struct tc_struct
    {
        mfxStatus sts_init;
        struct f_pair
        {
            mfxU16 picStruct;
            mfxU16 aud_delimiter;
        } set_par;
    };

private:

    static const tc_struct test_case[];

};


const TestSuite::tc_struct TestSuite::test_case[] =
{
    // AUDelimiter cases
    {/*00*/ MFX_ERR_NONE, {MFX_PICSTRUCT_PROGRESSIVE, MFX_CODINGOPTION_ON}},
    {/*01*/ MFX_ERR_NONE, {MFX_PICSTRUCT_FIELD_TFF, MFX_CODINGOPTION_ON}},
    {/*02*/ MFX_ERR_NONE, {MFX_PICSTRUCT_FIELD_BFF, MFX_CODINGOPTION_ON}},
    {/*03*/ MFX_ERR_NONE, {MFX_PICSTRUCT_PROGRESSIVE, MFX_CODINGOPTION_OFF}},
    {/*04*/ MFX_ERR_NONE, {MFX_PICSTRUCT_FIELD_TFF, MFX_CODINGOPTION_OFF}},
    {/*05*/ MFX_ERR_NONE, {MFX_PICSTRUCT_FIELD_BFF, MFX_CODINGOPTION_OFF}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class ProcessSetAUD : public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserH264AU
{
    mfxU32 m_nFields;
    mfxU32 m_nSurf;
    mfxU32 m_nFrame;
    mfxVideoParam& m_par;
    const TestSuite::tc_struct& m_tc;

public:
    ProcessSetAUD(mfxVideoParam& par, const TestSuite::tc_struct& tc/*, const char* fname*/)
        : tsBitstreamProcessor(), tsParserH264AU(), m_nSurf(0), m_nFrame(0), m_par(par), m_tc(tc)
    {
        m_nFields = m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
        set_trace_level(BS_H264_TRACE_LEVEL_SLICE);
    }

    ~ProcessSetAUD() {}

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        s.Data.FrameOrder = m_nSurf;
        m_nSurf++;

        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        using namespace BS_AVC2; // for NALU type
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

        while (nFrames--) {
            bool AUDPresent = false;

            UnitType& au = ParseOrDie();
            for (UnitType::nalu_param* nalu = au.NALU; nalu < au.NALU + au.NumUnits; nalu ++)
            {
                if (nalu->nal_unit_type == BS_AVC2::AUD_NUT)
                {
                    byte frtype = nalu->AUD->primary_pic_type << 1;
                    bool bcheckedAudPar;
                    mfxU32 goporder = m_nFrame % m_par.mfx.GopPicSize;

                    if (goporder == 0)
                    {
                        bcheckedAudPar = (frtype + 1) & MFX_FRAMETYPE_I;
                    }
                    else if (goporder % m_par.mfx.GopRefDist == 0)
                    {
                        bcheckedAudPar = frtype & MFX_FRAMETYPE_P;
                    }
                    else
                    {
                        bcheckedAudPar = frtype & MFX_FRAMETYPE_B;
                    }

                    if (bcheckedAudPar)
                    {
                        AUDPresent = true;
                        break;
                    }
                }
            }
            if (m_tc.set_par.aud_delimiter == MFX_CODINGOPTION_OFF)
            {
                EXPECT_FALSE(AUDPresent);
            }
            else
            {
                EXPECT_TRUE(AUDPresent);
            }

            m_nFrame++;
        }

        bs.DataOffset = 0;
        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }
};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_FEI_SUPPORT();

    const tc_struct& tc = test_case[id];

    const mfxU16 n_frames = 10;
    mfxStatus sts;

    encpak.enc.m_par.mfx.FrameInfo.PicStruct = tc.set_par.picStruct;
    encpak.enc.m_pPar->mfx.GopPicSize = 30;
    encpak.enc.m_pPar->mfx.GopRefDist  = 1;
    encpak.enc.m_pPar->mfx.NumRefFrame = 2;
    encpak.enc.m_pPar->mfx.TargetUsage = 4;
    encpak.enc.m_pPar->AsyncDepth = 1;
    encpak.enc.m_pPar->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    encpak.pak.m_par.mfx = encpak.enc.m_par.mfx;
    encpak.pak.m_par.AsyncDepth = encpak.enc.m_par.AsyncDepth;
    encpak.pak.m_par.IOPattern = encpak.enc.m_par.IOPattern;

    mfxU16 num_fields = encpak.enc.m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;

    ProcessSetAUD pd(encpak.enc.m_par, tc);

    encpak.m_filler = &pd;
    encpak.m_bs_processor = &pd;

    encpak.PrepareInitBuffers();

    mfxExtCodingOption* extOpt = new mfxExtCodingOption;
    memset(extOpt, 0, sizeof(mfxExtCodingOption));
    extOpt->Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    extOpt->Header.BufferSz = sizeof(mfxExtCodingOption);
    extOpt->AUDelimiter = tc.set_par.aud_delimiter;
    encpak.pak.initbuf.push_back(&extOpt->Header);

    g_tsStatus.disable(); // it is checked many times inside, resetting expected to err_none

    sts = encpak.Init();

    if (sts != MFX_ERR_NONE) {
        g_tsStatus.expect(tc.sts_init); // if init fails check if it is expected
    }

    if (sts == MFX_ERR_NONE)
        sts = encpak.AllocBitstream((encpak.enc.m_par.mfx.FrameInfo.Width * encpak.enc.m_par.mfx.FrameInfo.Height) * 16 * n_frames + 1024*1024);


    mfxU32 count;
    mfxU32 async = TS_MAX(1, encpak.enc.m_par.AsyncDepth);
    async = TS_MIN(n_frames, async - 1);

    for ( count = 0; count < n_frames && sts == MFX_ERR_NONE; count++) {
        for (mfxU32 field = 0; field < num_fields && sts == MFX_ERR_NONE; field++) {
            encpak.PrepareFrameBuffers(field);

            sts = encpak.EncodeFrame(field);

            g_tsStatus.expect(tc.sts_init); // if fails check if it is expected
            if (sts != MFX_ERR_NONE)
                break;
        }
    }

    g_tsLog << count << " FRAMES Encoded\n";

    g_tsStatus.enable();
    g_tsStatus.check(sts);

    if (extOpt)
    {
        delete extOpt;
        extOpt = NULL;
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encpak_aud);
}

