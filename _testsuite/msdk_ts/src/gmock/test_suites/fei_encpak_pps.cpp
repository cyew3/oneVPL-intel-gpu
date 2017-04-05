/******************************************************************************* *\

Copyright (C) 2016-2017 Intel Corporation.  All rights reserved.

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

namespace fei_encpak_ppsid
{

// when pps is provided
enum
{
    NEVER  = 0, // never provided
    INIT   = 1, // only init
    FRAME  = 2, // no on init but with frame
    ALWAYS = 3, // both on init and with frame
    ENC    = 4,
    PAK    = 8,
    NORMAL = ALWAYS | ENC | PAK
};

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
        mfxStatus sts;
        mfxU32 mode;
        struct f_par
        {
            mfxU16    SPSId;
            mfxU16    PPSId;

            mfxU16    PictureType;
            mfxU16    FrameType;
            mfxU16    PicInitQP;
            mfxU16    NumRefIdxL0Active;
            mfxU16    NumRefIdxL1Active;
            mfxI16    ChromaQPIndexOffset;
            mfxI16    SecondChromaQPIndexOffset;
            mfxU16    Transform8x8ModeFlag;
            // dpb later
        } par;
    };

private:

    static const tc_struct test_case[];

};

// NOTE: it is only preliminary version with only few successive cases

const TestSuite::tc_struct TestSuite::test_case[] =
{
 /*00*/ {MFX_ERR_NONE, NORMAL, {0, 0, MFX_FRAMETYPE_I, MFX_PICTYPE_FRAME, 26, 0,0, 0,0, 1}},
 /*01*/ {MFX_ERR_NONE, NORMAL, {0, 0, MFX_FRAMETYPE_I, MFX_PICTYPE_FRAME, 26, 0,0, 0,0, 0}}, // should be unsupported
 /*02*/ {MFX_ERR_NONE, NORMAL, {0, 0, MFX_FRAMETYPE_I, MFX_PICTYPE_FRAME, 26, 1,0, 0,0, 1}},
// /*00*/ {MFX_ERR_NONE, NORMAL, {0, 0, MFX_FRAMETYPE_I, MFX_PICTYPE_FRAME, 26, 0,1, 0,0, 1}}, // craches
// /*00*/ {MFX_ERR_INVALID_VIDEO_PARAM, NORMAL, {0, 0, MFX_FRAMETYPE_I, MFX_PICTYPE_FRAME, 26, 33,0, 0,0, 1}}, // crashes
// /*00*/ {MFX_ERR_INVALID_VIDEO_PARAM, NORMAL, {0, 0, MFX_FRAMETYPE_I, MFX_PICTYPE_FRAME, 26, 2,33, 0,0, 1}},

 // /*01*/ {MFX_ERR_NONE, NORMAL, {0, 1, 4}},
// /*02*/ {MFX_ERR_NONE, NORMAL, {0, 0, 5}},
// /*03*/ {MFX_ERR_UNDEFINED_BEHAVIOR, NORMAL, {1, 0, 4}},
// /*04*/ {MFX_ERR_INVALID_VIDEO_PARAM, NORMAL, {0, 0, 1}}, // 0 0 1 fails
// /*05*/ {MFX_ERR_NONE, NEVER | ENC, {0, 0, 4}},
// /*06*/ {MFX_ERR_UNDEFINED_BEHAVIOR, FRAME | ENC, {0, 0, 4}},
// /*07*/ {MFX_ERR_UNDEFINED_BEHAVIOR, ALWAYS | ENC, {0, 0, 4}},
// /*08*/ {MFX_ERR_NONE, NEVER | PAK, {0, 0, 4}},
// /*09*/ {MFX_ERR_UNDEFINED_BEHAVIOR, FRAME | PAK, {0, 0, 4}},
// /*10*/ {MFX_ERR_UNDEFINED_BEHAVIOR, ALWAYS | PAK, {0, 0, 4}},
};

// use 3 times - progressive, tff, bff
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct) * 3;
mfxStatus LoadTC(std::vector<mfxExtBuffer*>& encbuf, std::vector<mfxExtBuffer*>& pakbuf, const TestSuite::tc_struct& tc, mfxU32 field)
{
    mfxExtFeiPPS* ppsE = (mfxExtFeiPPS*)GetExtFeiBuffer(encbuf, MFX_EXTBUFF_FEI_PPS, field);
    mfxExtFeiPPS* ppsP = (mfxExtFeiPPS*)GetExtFeiBuffer(pakbuf, MFX_EXTBUFF_FEI_PPS, field);
    if (!ppsE || !ppsP)
    	return MFX_ERR_NOT_FOUND;
    ppsE->SPSId = ppsP->SPSId = tc.par.SPSId;
    ppsE->PPSId = ppsP->PPSId = tc.par.PPSId;
    ppsE->PicInitQP = ppsP->PicInitQP = tc.par.PicInitQP;
    ppsE->NumRefIdxL0Active = ppsP->NumRefIdxL0Active = std::min(tc.par.NumRefIdxL0Active, ppsP->NumRefIdxL0Active);
    ppsE->NumRefIdxL1Active = ppsP->NumRefIdxL1Active = std::min(tc.par.NumRefIdxL1Active, ppsP->NumRefIdxL1Active);
    ppsE->ChromaQPIndexOffset = ppsP->ChromaQPIndexOffset = tc.par.ChromaQPIndexOffset;
    ppsE->SecondChromaQPIndexOffset = ppsP->SecondChromaQPIndexOffset = tc.par.SecondChromaQPIndexOffset;
    ppsE->Transform8x8ModeFlag = ppsP->Transform8x8ModeFlag = tc.par.Transform8x8ModeFlag;

    return MFX_ERR_NONE;
}


#define VERIFY_FIELD(var, expected, name) { \
    g_tsLog << name << " = " << var << " (expected = " << expected << ")\n"; \
    if ( var != expected) { \
        g_tsLog << "ERROR: " << name <<  " in stream != expected\n"; \
        err++; \
    } \
}

class ProcessSetPPS : public tsSurfaceProcessor, public tsBitstreamWriter, public tsParserH264AU
{
    mfxU32 m_nFields;
    mfxU32 m_nSurf;
    mfxU32 m_nFrame;
    mfxVideoParam& m_par;
    //mfxU32 m_mode;
    std::vector <mfxExtBuffer*> m_buffs; // unused

public:
    TestSuite::tc_struct m_tc; // some fields differs with original tc (numRef*)

    ProcessSetPPS(mfxVideoParam& par, const TestSuite::tc_struct& tc, const char* fname)
        : tsBitstreamWriter(fname), tsParserH264AU(), m_nSurf(0), m_nFrame(0), m_par(par), m_tc(tc)
    {
        m_nFields = m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
        set_trace_level(BS_H264_TRACE_LEVEL_PPS);
    }

    ~ProcessSetPPS()
    {
        //ReleaseExtBufs (m_buffs, m_nFields);
    }

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

        mfxU16 err = 0;

        while (nFrames--) {
            for (mfxU16 field = 0; field < 1/*m_nFields*/; field++) { // now one field per call
                UnitType& au = ParseOrDie();
                for (Bs32u i = 0; i < au.NumUnits; i++)
                {
                    if (au.NALU[i].nal_unit_type != PPS_NUT)
                        continue; // todo: check if missed pps succeeds

                    VERIFY_FIELD(au.NALU[i].PPS->seq_parameter_set_id, m_tc.par.SPSId, "SPSid")
                    VERIFY_FIELD(au.NALU[i].PPS->pic_parameter_set_id, m_tc.par.PPSId, "PPSId")
                    //VERIFY_FIELD(au.NALU[i].PPS->, m_tc.par.PictureType, "PictureType")
                    //VERIFY_FIELD(au.NALU[i].PPS->, m_tc.par.FrameType, "FrameType")
                    VERIFY_FIELD(au.NALU[i].PPS->pic_init_qp_minus26+26, m_tc.par.PicInitQP, "PicInitQP")
                    VERIFY_FIELD(au.NALU[i].PPS->num_ref_idx_l0_default_active_minus1+1,
                    		(std::max)(mfxU16(1), m_tc.par.NumRefIdxL0Active), "NumRefIdxL0Active")
                    VERIFY_FIELD(au.NALU[i].PPS->num_ref_idx_l1_default_active_minus1+1, (std::max)(mfxU16(1), m_tc.par.NumRefIdxL1Active), "NumRefIdxL1Active")
                    VERIFY_FIELD(au.NALU[i].PPS->chroma_qp_index_offset, m_tc.par.ChromaQPIndexOffset, "ChromaQPIndexOffset")
                    VERIFY_FIELD(au.NALU[i].PPS->second_chroma_qp_index_offset, m_tc.par.SecondChromaQPIndexOffset, "SecondChromaQPIndexOffset")
                    VERIFY_FIELD((mfxU8)au.NALU[i].PPS->transform_8x8_mode_flag, m_tc.par.Transform8x8ModeFlag, "Transform8x8ModeFlag")

                    g_tsLog << "\n";
                }
            }
            m_nFrame++;
        }

        bs.DataOffset = 0;
        bs.DataLength = 0;

        if (err)
            return MFX_ERR_ABORTED;

        return MFX_ERR_NONE;
    }
};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_FEI_SUPPORT();

    const int table_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);
    const tc_struct& tc = test_case[id % table_cases];
    srand(id);

    const mfxU16 n_frames = 10;
    mfxU16 picStruct;
    mfxStatus sts;

    switch(id / table_cases) {
    case 0:
    default:
        picStruct = MFX_PICSTRUCT_PROGRESSIVE;
        break;
    case 1:
        picStruct = MFX_PICSTRUCT_FIELD_TFF;
        break;
    case 2:
        picStruct = MFX_PICSTRUCT_FIELD_BFF;
        break;
    }

    encpak.enc.m_par.mfx.FrameInfo.PicStruct = picStruct;
    encpak.enc.m_pPar->mfx.GopPicSize = 30;
    encpak.enc.m_pPar->mfx.GopRefDist  = 1; // to exclude
    encpak.enc.m_pPar->mfx.NumRefFrame = 2; // frame reordering
    encpak.enc.m_pPar->mfx.TargetUsage = 4;
    encpak.enc.m_pPar->mfx.EncodedOrder = 1;
    encpak.enc.m_pPar->AsyncDepth = 1;
    encpak.enc.m_pPar->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    encpak.pak.m_par.mfx = encpak.enc.m_par.mfx;
    encpak.pak.m_par.AsyncDepth = encpak.enc.m_par.AsyncDepth;
    encpak.pak.m_par.IOPattern = encpak.enc.m_par.IOPattern;

    mfxU16 num_fields = encpak.enc.m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;

    ProcessSetPPS pd(encpak.enc.m_par, tc, "bs.out");

    encpak.m_filler = &pd;
    encpak.m_bs_processor = &pd;

    encpak.PrepareInitBuffers();
    LoadTC(encpak.enc.initbuf, encpak.pak.initbuf, tc, 0);

//// mods for Init to be here
    mfxExtFeiPPS* ppsE = (mfxExtFeiPPS*)GetExtFeiBuffer(encpak.enc.initbuf, MFX_EXTBUFF_FEI_PPS);
    mfxExtFeiPPS* ppsP = (mfxExtFeiPPS*)GetExtFeiBuffer(encpak.pak.initbuf, MFX_EXTBUFF_FEI_PPS);
    ppsE->PictureType = ppsP->PictureType = tc.par.PictureType;
    ppsE->FrameType = ppsP->FrameType = tc.par.FrameType;

    if ((tc.mode & (ENC|INIT)) == ENC) // no pps on init for enc
        ExcludeExtBufferPtr(encpak.enc.initbuf, &ppsE->Header);
    if ((tc.mode & (PAK|INIT)) == PAK) // no pps on init for pak
        ExcludeExtBufferPtr(encpak.pak.initbuf, &ppsP->Header);

    g_tsStatus.disable(); // it is checked many times inside, resetting expected to err_none

    sts = encpak.Init();

    if (sts != MFX_ERR_NONE) {
        g_tsStatus.expect(tc.sts); // if init fails check if it is expected
    }

    if (sts == MFX_ERR_NONE)
        sts = encpak.AllocBitstream((encpak.enc.m_par.mfx.FrameInfo.Width * encpak.enc.m_par.mfx.FrameInfo.Height) * 16 * n_frames + 1024*1024);


    mfxU32 count;
    mfxU32 async = TS_MAX(1, encpak.enc.m_par.AsyncDepth);
    async = TS_MIN(n_frames, async - 1);

    for ( count = 0; count < n_frames && sts == MFX_ERR_NONE; count++) {
        for (mfxU32 field = 0; field < num_fields && sts == MFX_ERR_NONE; field++) {
            encpak.PrepareFrameBuffers(field);
            LoadTC(encpak.enc.inbuf, encpak.pak.inbuf, tc, field);

            // tmp: to align fields
            if (num_fields == 2) {
                LoadTC(encpak.enc.inbuf, encpak.pak.inbuf, tc, 1 ^ field);
            }
            // to modify pps etc
            mfxExtFeiPPS * fppsE = (mfxExtFeiPPS *)GetExtFeiBuffer(encpak.enc.inbuf, MFX_EXTBUFF_FEI_PPS, field);
            mfxExtFeiSliceHeader * fsliceE = (mfxExtFeiSliceHeader *)GetExtFeiBuffer(encpak.enc.inbuf, MFX_EXTBUFF_FEI_SLICE, field);
            for (mfxI32 s=0; s<fsliceE->NumSlice; s++) {
                fsliceE->Slice[s].PPSId = fppsE->PPSId;
                fsliceE->Slice[s].NumRefIdxL0Active = fppsE->NumRefIdxL0Active;
                fsliceE->Slice[s].NumRefIdxL1Active = fppsE->NumRefIdxL1Active;
            }

            mfxExtFeiPPS * fppsP = (mfxExtFeiPPS *)GetExtFeiBuffer(encpak.pak.inbuf, MFX_EXTBUFF_FEI_PPS, field);
            mfxExtFeiSliceHeader * fsliceP = (mfxExtFeiSliceHeader *)GetExtFeiBuffer(encpak.pak.inbuf, MFX_EXTBUFF_FEI_SLICE, field);
            for (mfxI32 s=0; s<fsliceP->NumSlice; s++) {
                fsliceP->Slice[s].PPSId = fppsP->PPSId;
                fsliceP->Slice[s].NumRefIdxL0Active = fppsP->NumRefIdxL0Active;
                fsliceP->Slice[s].NumRefIdxL1Active = fppsP->NumRefIdxL1Active;
            }

            // values in input tc are not always valid and can be changed in PrepareFrameBuffers()
            pd.m_tc.par.NumRefIdxL0Active = fppsE->NumRefIdxL0Active;
            pd.m_tc.par.NumRefIdxL1Active = fppsE->NumRefIdxL1Active;

            // remove both fields'?
            if ((tc.mode & (ENC|FRAME)) == ENC) // no pps on frame for enc
                ExcludeExtBufferPtr(encpak.enc.inbuf, &ppsE->Header);
            if ((tc.mode & (PAK|FRAME)) == PAK) // no pps on frame for pak
                ExcludeExtBufferPtr(encpak.pak.inbuf, &ppsP->Header);

            sts = encpak.EncodeFrame(field);

            g_tsStatus.expect(tc.sts); // if init fails check if it is expected
            if (sts != MFX_ERR_NONE)
                break;
        }

    }

    g_tsLog << count << " FRAMES Encoded\n";

    g_tsStatus.enable();
    int ret = g_tsStatus.check(sts);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encpak_pps);
};
