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

namespace fei_encpak_mmco
{

// mmco types
enum
{
    MMCO_0  = 0,
    MMCO_1  = 1, // exclude ST
    MMCO_2  = 2,
    MMCO_3  = 3,
    MMCO_4  = 4,
    MMCO_5  = 5, // remove all
    MMCO_6  = 6,
};

#define MAX_MMCO_IN_CASE 15

class TestSuite
{
public:
    tsVideoENCPAK encpak;

    TestSuite() : encpak(MFX_FEI_FUNCTION_ENC, MFX_FEI_FUNCTION_PAK, MFX_CODEC_AVC, true) {} // fix function id
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    struct mmco_par
    {
        mfxU16    frameNum;
        mfxU16    mmco;
        mfxU16    idxST; // diff with current (without minus1)
        mfxU16    idxLT;
    };
    struct tc_struct
    {
        mfxStatus sts;
        mfxU16 mode;
        mfxU16 gopRefDist;
        struct mmco_par mmco_set[MAX_MMCO_IN_CASE];
    };

private:

    static const tc_struct test_case[];

};

// NOTE: it is only preliminary version with only few successive cases
// MMC0_5 (clear all) shouldn't be tested - it is not used in msdk

const TestSuite::tc_struct TestSuite::test_case[] =
{
 /*00*/ {MFX_ERR_NONE, 0+0, 1, {{2, MMCO_1, 1, 0}, {6, MMCO_0, 0, 0,}, {0}} },
};

// use 3 times - progressive, tff, bff
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct) * 1; //3;
mfxStatus LoadTC(std::vector<mfxExtBuffer*>& encbuf, std::vector<mfxExtBuffer*>& pakbuf, const TestSuite::tc_struct& tc, mfxU32 frame, mfxU32 field)
{
    mfxExtFeiPPS* ppsE = (mfxExtFeiPPS*)GetExtFeiBuffer(encbuf, MFX_EXTBUFF_FEI_PPS, field);
    mfxExtFeiPPS* ppsP = (mfxExtFeiPPS*)GetExtFeiBuffer(pakbuf, MFX_EXTBUFF_FEI_PPS, field);
    if (!ppsE || !ppsP)
        return MFX_ERR_NOT_FOUND;
    for (int op = 0; tc.mmco_set[op].mmco != 0; op++) {
    	if (tc.mmco_set[op].frameNum != frame)
    		continue;
    	switch (tc.mmco_set[op].mmco) {
    	case MMCO_1:
			{
				mfxI32 target = frame - tc.mmco_set[op].idxST;
				for (int d = 0; d<16; d++) {
					if (ppsE->DpbAfter[d].FrameNumWrap == target) {
						// remove with shift left
						for ( ; d+1 < 16 && ppsE->DpbAfter[d+1].Index != 0xffff; d++)
							ppsE->DpbAfter[d] = ppsP->DpbAfter[d] = ppsE->DpbAfter[d+1];
						ppsE->DpbAfter[d].Index = ppsP->DpbAfter[d].Index = 0xffff;
						break;
					}
				}
			}
            break;
    	case MMCO_5:
    		// remove all ST
    		for (int d = 0; d<16; d++)
    			ppsE->DpbAfter[d].Index = ppsP->DpbAfter[d].Index = 0xffff;
    		// and LT: ...
    		break;
    	default:
    		return MFX_ERR_ABORTED; // or what?
    	}
    }

    return MFX_ERR_NONE;
}


#define VERIFY_FIELD(var, expected, name) { \
    g_tsLog << name << " = " << var << " (expected = " << expected << ")\n"; \
    if ( var != expected) { \
        g_tsLog << "ERROR: " << name <<  " in stream != expected\n"; \
        err++; \
    } \
}

class ProcessSetMMCO : public tsSurfaceProcessor, public tsBitstreamWriter, public tsParserH264AU
{
    mfxU32 m_nFields;
    mfxU32 m_nSurf;
    mfxU32 m_nFrame;
    mfxVideoParam& m_par;
    //mfxU32 m_mode;
    std::vector <mfxExtBuffer*> m_buffs; // unused

public:
    TestSuite::tc_struct m_tc; // some fields differs with original tc (numRef*)

    ProcessSetMMCO(mfxVideoParam& par, const TestSuite::tc_struct& tc, const char* fname)
        : tsBitstreamWriter(fname), tsParserH264AU(), m_nSurf(0), m_nFrame(0), m_par(par), m_tc(tc)
    {
        m_nFields = m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
        set_trace_level(BS_H264_TRACE_LEVEL_REF_LIST);
    }

    ~ProcessSetMMCO()
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
                    if (au.NALU[i].nal_unit_type != SLICE_IDR && au.NALU[i].nal_unit_type != SLICE_NONIDR)
                        continue; // todo: check if missed pps succeeds
                    dec_ref_pic_marking_params* marking = au.NALU[i].slice_hdr->dec_ref_pic_marking;
                    int numExpected = 0, numFound = 0;
            		for (int d = 0; d<MAX_MMCO_IN_CASE; numExpected += (m_tc.mmco_set[d].frameNum == m_nFrame), d++);

                    while (marking) {
                    	numFound += (marking->memory_management_control_operation != 0);
                    	bool matched = false;

                    	switch (marking->memory_management_control_operation) {
                        case MMCO_0:
                        	matched = true;
                        	break;
                        case MMCO_1:
							{
								mfxU32 target = m_nFrame - (marking->difference_of_pic_nums_minus1+1);
								for (int d = 0; d<MAX_MMCO_IN_CASE && !matched; d++) {
									if (m_tc.mmco_set[d].frameNum != m_nFrame || m_tc.mmco_set[d].mmco != MMCO_1)
										continue;
									matched = (m_tc.mmco_set[d].idxST == target);
								}
							}
                            break;
                        case MMCO_5:
							{
								for (int d = 0; d<MAX_MMCO_IN_CASE; d++) {
									if (m_tc.mmco_set[d].frameNum != m_nFrame)
										continue;
									matched = (m_tc.mmco_set[d].mmco == MMCO_5);
									break;
								}
							}
                            break;
                        default:
                        	;
                        }
                        VERIFY_FIELD(matched, true, "mmco_" << marking->memory_management_control_operation << " matched ")
                        marking = marking->next;
                    }
                    VERIFY_FIELD(numFound, numExpected, "Number of nz mmco in " << m_nFrame << " frame ")

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
    encpak.enc.m_pPar->mfx.GopRefDist  = tc.gopRefDist;
    encpak.enc.m_pPar->mfx.NumRefFrame = 4;
    encpak.enc.m_pPar->mfx.TargetUsage = 4;
    encpak.enc.m_pPar->mfx.EncodedOrder = 1;
    encpak.enc.m_pPar->AsyncDepth = 1;
    encpak.enc.m_pPar->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    encpak.pak.m_par.mfx = encpak.enc.m_par.mfx;
    encpak.pak.m_par.AsyncDepth = encpak.enc.m_par.AsyncDepth;
    encpak.pak.m_par.IOPattern = encpak.enc.m_par.IOPattern;

    mfxU16 num_fields = encpak.enc.m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;

    ProcessSetMMCO pd(encpak.enc.m_par, tc, "bs.out");

    encpak.m_filler = &pd;
    encpak.m_bs_processor = &pd;

    encpak.PrepareInitBuffers();
    //LoadTC(encpak.enc.initbuf, encpak.pak.initbuf, tc, 0);

//// mods for Init to be here
    mfxExtFeiPPS* ppsE = (mfxExtFeiPPS*)GetExtFeiBuffer(encpak.enc.initbuf, MFX_EXTBUFF_FEI_PPS);
    mfxExtFeiPPS* ppsP = (mfxExtFeiPPS*)GetExtFeiBuffer(encpak.pak.initbuf, MFX_EXTBUFF_FEI_PPS);
//    ppsE->PictureType = ppsP->PictureType = tc.par.PictureType;
//    ppsE->FrameType = ppsP->FrameType = tc.par.FrameType;

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
            LoadTC(encpak.enc.inbuf, encpak.pak.inbuf, tc, count, field);

            // tmp: to align fields
            if (num_fields == 2) {
                LoadTC(encpak.enc.inbuf, encpak.pak.inbuf, tc, count, 1 ^ field);
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

//            pd.m_tc.par.SPSId                = fppsE->SPSId;

            sts = encpak.EncodeFrame(field);

            g_tsStatus.expect(tc.sts); // if fails check if it is expected
            if (sts != MFX_ERR_NONE)
                break;
        }

    }

    g_tsLog << count << " FRAMES Encoded\n";

    g_tsStatus.enable();
    int ret = g_tsStatus.check(/*sts*/);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encpak_mmco);
};
