/******************************************************************************* *\

Copyright (C) 2019 Intel Corporation.  All rights reserved.

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

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

/*
Description:
Test verifies that HEVC encoder can generate panic mode skip frames.
Currently MediasDK doesn't whether BRC panic mode has occurred.
So the test simply hardcodes aggressive HRD BRC param which have to cause at least one skip frame.
*/
namespace hevce_brc_panic_mode
{

enum
{
    MFX_PAR = 1,
    EXT_COD3
};

struct tc_struct
{
    mfxStatus   queryStatus;
    struct f_pair
    {
        mfxU32 ext_type;
        const  tsStruct::Field* f;
        mfxU32 v;
    } set_par[MAX_NPARS];
};

class TestSuite :
    public tsVideoEncoder,
    public tsSurfaceProcessor,
    public tsBitstreamProcessor,
    public tsParserHEVC2
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC)
        , tsParserHEVC2(BS_HEVC2::PARSE_SSD)
#ifdef MANUAL_DEBUG_MODE
        , m_tsBsWriter("test.h265")
#endif
    {
        m_filler = this;
        m_bs_processor = this;

        m_pPar->mfx.FrameInfo.Width        = m_pPar->mfx.FrameInfo.CropW = 352;
        m_pPar->mfx.FrameInfo.Height       = m_pPar->mfx.FrameInfo.CropH = 288;
        m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_pPar->mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        m_pPar->mfx.FrameInfo.PicStruct    = MFX_PICSTRUCT_PROGRESSIVE;
        m_pPar->mfx.GopRefDist             = 2;
        m_pPar->mfx.GopPicSize             = 1000;

        m_pPar->mfx.BufferSizeInKB   = 5;
        m_pPar->mfx.InitialDelayInKB = 5;
        m_pPar->mfx.TargetKbps       = 10;
        m_pPar->mfx.MaxKbps          = 10;

        m_reader.reset(new tsRawReader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"),
                                   m_pPar->mfx.FrameInfo));
        g_tsStreamPool.Reg();
    }

private:
    const mfxU32 m_framesNumber = 10;

#ifdef MANUAL_DEBUG_MODE
    tsBitstreamWriter m_tsBsWriter;
#endif

    mfxStatus ProcessSurface(mfxFrameSurface1& surf) override;

    // Checks that all processed CUs have approriate size
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames) override;

    std::unique_ptr <tsRawReader> m_reader;

    std::vector<uint32_t> m_panicModeSkipFrames; // contains POCs of panic mode skip frames

public:
     int RunTest(unsigned int id);

    static const unsigned int n_cases;

    static const tc_struct test_case[];
};

// TODO: uncomment BRCPanicMode when support appears in HEVCe
const tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE,
        {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            /*{ EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_ON }*/} },
    {/*01*/ MFX_ERR_NONE,
        {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            /*{ EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_ON }*/} },
    {/*02*/ MFX_ERR_NONE,
        {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
            /*{ EXT_COD3, &tsStruct::mfxExtCodingOption3.BRCPanicMode, MFX_CODINGOPTION_OFF }*/} },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(tc_struct);

mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& s)
{
    return m_reader->ProcessSurface(s);
}

mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

    mfxStatus sts = MFX_ERR_NONE;

#ifdef MANUAL_DEBUG_MODE
    sts = m_tsBsWriter.ProcessBitstream(bs, nFrames);
#endif

    while (nFrames--)
    {
        auto& hdr = ParseOrDie();

        for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
        {
            if (!pNALU)
            {
                g_tsLog << "ERROR: ProcessBitstream: Some NAL unit is undefined" << "\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }

            if (!IsHEVCSlice(pNALU->nal_unit_type))
            {
                continue;
            }

            auto& slice = *pNALU->slice;
            for (auto pCTU = slice.ctu; pCTU; pCTU = pCTU->Next)
            {
                for (auto pCU = pCTU->Cu; pCU; pCU = pCU->Next)
                {
                    if (pCU->PredMode != BS_HEVC2::MODE_SKIP)
                    {
                        bs.DataLength = 0;
                        return MFX_ERR_NONE;
                    }
                }
            }
            m_panicModeSkipFrames.push_back(slice.POC);
        }
    }

    bs.DataLength = 0;
    return MFX_ERR_NONE;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    if ((g_tsImpl != MFX_IMPL_HARDWARE) || (g_tsHWtype < MFX_HW_TGL))
    {
        g_tsLog << "SKIPPED for this platform\n";
        return 0;
    }

    const tc_struct& tc = test_case[id];

    MFXInit();

    SETPARS(&m_par, MFX_PAR);
    mfxExtCodingOption3 &co3 = m_par;
    SETPARS(&co3,   EXT_COD3);

    EncodeFrames(m_framesNumber);

    std::for_each(m_panicModeSkipFrames.begin(), m_panicModeSkipFrames.end(), [](uint32_t poc) { g_tsLog << "Frame POC " << poc << " is skip frame\n"; } );
    EXPECT_NE(m_panicModeSkipFrames.size(), 0) << " ERROR: Expected at least one frame to be encoded as skip";

    Close();

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_brc_panic_mode);
};
