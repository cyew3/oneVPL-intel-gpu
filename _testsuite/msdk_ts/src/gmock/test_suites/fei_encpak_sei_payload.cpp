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

#include <algorithm>

namespace fei_encpak_sei_payload
{

class TestSuite: tsSurfaceProcessor, tsBitstreamProcessor, tsParserH264AU
{
public:
    tsVideoENCPAK encpak;

    TestSuite()
        : tsSurfaceProcessor()
        , tsBitstreamProcessor()
        , tsParserH264AU()
        , encpak(MFX_FEI_FUNCTION_ENC, MFX_FEI_FUNCTION_PAK, MFX_CODEC_AVC, true)
        , m_nFrames(5)
        , m_frm_in(0)
        , m_frm_out(0)
    {
        encpak.m_filler = this;
        encpak.m_bs_processor = this;
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 sum_group_of_msg;
        struct
        {
            mfxU16    nf; //which frame the sei will be attached
            mfxU16    num_payloads;
            mfxU16    payload_types[6];
        } per_frm_sei[6];
        struct f_pair
        {
            mfxU32 picStruct;
            mfxU32 singleSeiNalUnit;
        } set_par;
    };

private:

    std::map<mfxU32, std::vector<mfxPayload*>> m_per_frame_sei;
    mfxU32 m_nFrames;
    mfxU32 m_frm_in;
    mfxU32 m_frm_out;
    mfxU32 m_field;

    mfxStatus ProcessSurface(mfxFrameSurface1 &);
    mfxStatus ProcessBitstream(mfxBitstream & bs,mfxU32 nFrames);
    void sei_message_size_and_type(std::vector<mfxU8>& data, mfxU16 type, mfxU32 size);
    void create_sei_message(mfxPayload* pl, mfxU16 type, mfxU32 size);


    static const tc_struct test_case[];

};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    // unsupported SEI type cases (spare picture)
    {/*00*/ MFX_ERR_INVALID_VIDEO_PARAM, 1, {{0, 1, {8}}},
        {MFX_PICSTRUCT_PROGRESSIVE, MFX_CODINGOPTION_ON}},

    //supported SEI type case (picture timing)
    {/*01*/ MFX_ERR_NONE, 1, {{0, 1, {1}}},
        {MFX_PICSTRUCT_PROGRESSIVE, MFX_CODINGOPTION_ON}},

    //supported SEI type case (buffer period)
    {/*02*/ MFX_ERR_NONE, 1, {{0, 1, {0}}},
        {MFX_PICSTRUCT_PROGRESSIVE, MFX_CODINGOPTION_ON}},

    //supported SEI type case (recovery point)
    {/*03*/ MFX_ERR_NONE, 1, {{0, 1, {6}}},
        {MFX_PICSTRUCT_PROGRESSIVE, MFX_CODINGOPTION_ON}},

    //supported SEI type case (RefPicMarRep)
    {/*04*/ MFX_ERR_NONE, 1, {{0, 1, {7}}},
        {MFX_PICSTRUCT_PROGRESSIVE, MFX_CODINGOPTION_ON}},

    //supported SEI type
    {/*05*/ MFX_ERR_NONE, 1, {{1, 4, {0, 1, 6, 7}}},
        {MFX_PICSTRUCT_PROGRESSIVE, MFX_CODINGOPTION_ON}},

    //supported SEI type, SingleSeiNalUnit = ON
    {/*06*/ MFX_ERR_NONE, 2, {{0, 1, {6}},{3, 2, {0, 1}}},
        {MFX_PICSTRUCT_PROGRESSIVE, MFX_CODINGOPTION_ON}},

    //supported SEI type, SingleSeiNalUnit = OFF
    {/*07*/ MFX_ERR_NONE, 2, {{0, 1, {6}},{3, 2, {0, 1}}},
        {MFX_PICSTRUCT_PROGRESSIVE, MFX_CODINGOPTION_OFF}},

    //supported SEI type, picstruct = MFX_PICSTRUCT_FIELD_BFF;
    {/*08*/ MFX_ERR_NONE, 1, {{0, 4, {0, 1, 6, 7}}},
        {MFX_PICSTRUCT_FIELD_BFF, MFX_CODINGOPTION_ON}},

    //supported SEI type, picstruct = MFX_PICSTRUCT_FIELD_TFF;
    {/*09*/ MFX_ERR_NONE, 1, {{0, 4, {0, 1, 6, 7}}},
        {MFX_PICSTRUCT_FIELD_TFF, MFX_CODINGOPTION_ON}},

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& s)
{
    s.Data.FrameOrder = m_frm_in;

    if (m_per_frame_sei.find(m_frm_in) != m_per_frame_sei.end()) { // if current frame has the sei msg, find it out and attach it.
        encpak.m_PAKInput->Payload    = &(m_per_frame_sei[m_frm_in])[0];
        encpak.m_PAKInput->NumPayload = (mfxU16)m_per_frame_sei[m_frm_in].size();
    } else {
        encpak.m_PAKInput->Payload    = NULL;
        encpak.m_PAKInput->NumPayload = 0;
    }

    m_frm_in++;

    return MFX_ERR_NONE;
}

mfxStatus TestSuite::ProcessBitstream(mfxBitstream &bs, mfxU32 nFrames)
{
    set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

    auto& au = ParseOrDie();
    if (m_per_frame_sei.find(m_frm_out) == m_per_frame_sei.end()) {
        m_frm_out++;
        bs.DataLength = 0;
        return MFX_ERR_NONE;
    }

    struct payload {
        mfxU16 Type;
        mfxU16 BufSize;
    };

    bool bff = (m_field == MFX_PICSTRUCT_FIELD_BFF);
    mfxU8 frm_field = (m_field == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2;
    mfxU8 fieldId = 0;

    std::vector<payload> found_pl;
    found_pl.clear();

    for (Bs32u au_ind = 0; au_ind < au.NumUnits; au_ind++)
    {
        bool bBPFound = false;
        auto nalu = &au.NALU[au_ind];

        if((nalu->nal_unit_type == BS_AVC2::SLICE_NONIDR
            || nalu->nal_unit_type == BS_AVC2::SLICE_IDR))
        {
            slice_header& s = *nalu->slice_hdr;
            if(m_field == MFX_PICSTRUCT_FIELD_BFF)
            {
                fieldId = s.bottom_field_flag ? 0 : 1;
            }
            if(m_field == MFX_PICSTRUCT_FIELD_TFF)
            {
                fieldId = s.bottom_field_flag ? 1 : 0;
            }

            /*check whether Buffer period SEI present or not*/
            std::vector<mfxPayload*>::iterator i_exp_p =  m_per_frame_sei[m_frm_out].begin();
            if(frm_field > 1)
            {
                i_exp_p += fieldId;
            }
            for (i_exp_p; i_exp_p < m_per_frame_sei[m_frm_out].end(); i_exp_p += frm_field)
            {
                if ((*i_exp_p)->Type == 0) //Buffer Period SEI Type
                {
                    bBPFound = true;
                }
            }

            if (bBPFound)
            {
                payload pl = found_pl[0];
                EXPECT_EQ(pl.Type, 0) << "ERROR: Buffer Period SEI is NOT at the beginning posotion. \n";
            }

            //For field pictures, odd payloads are associated with the first
            //field and even payloads are associated with the second field.
            mfxU32 num_sei = 0;
            for (auto p : found_pl)
            {
                //compare with the preset sei message of the frm_count frame
                std::vector<mfxPayload*>::iterator i_exp_p =  m_per_frame_sei[m_frm_out].begin();
                if(frm_field > 1)
                {
                    i_exp_p += fieldId;
                }
                for (i_exp_p; i_exp_p < m_per_frame_sei[m_frm_out].end(); i_exp_p += frm_field)
                {
                    //  recalc real size, because exp_p->BufSize = SEI size + header_size(type + size)
                    mfxU16 size = (*i_exp_p)->BufSize;
                    std::vector<mfxU8> data;
                    sei_message_size_and_type(data, (*i_exp_p)->Type, (*i_exp_p)->BufSize);
                    size -= (mfxU16)data.size();

                    // whether the payload match one of it in the messages group
                    if (p.BufSize == size && p.Type == (*i_exp_p)->Type)
                    {
                        num_sei++;
                    }
                }
                g_tsLog << "The SEI in the " << m_frm_out << " frame: "  << "Type " << p.Type << " Size " << p.BufSize << "\n";
            }
            //only with the correct type and buffer size of each payload in the bitstream, the number of messages could be correct.
            EXPECT_EQ((frm_field > 1) ? m_per_frame_sei[m_frm_out].size()/2 : m_per_frame_sei[m_frm_out].size(), num_sei)
                    << "ERROR: incorrect number of SEI messages, some lost. \n";
        }

        if(nalu->nal_unit_type == BS_AVC2::SEI_NUT)
        {
            //SEI
            for (Bs16u sei_ind = 0; sei_ind < nalu->SEI->numMessages; ++sei_ind)
            {
                payload p = {};
                p.BufSize = nalu->SEI->message[sei_ind].payloadSize;
                p.Type = nalu->SEI->message[sei_ind].payloadType;
                found_pl.push_back(p);
            }
        }
    }

    bs.DataLength = 0;
    m_frm_out++;

    return MFX_ERR_NONE;
}


void TestSuite::sei_message_size_and_type(std::vector<mfxU8>& data, mfxU16 type, mfxU32 size)
{
    size_t B = type;

    while (B > 255)
    {
        data.push_back(255);
        B -= 255;
    }
    data.push_back(mfxU8(B));

    B = size;

    while (B > 255)
    {
        data.push_back(255);
        B -= 255;
    }
    data.push_back(mfxU8(B));

    B = data.size();
}

void TestSuite::create_sei_message(mfxPayload* pl, mfxU16 type, mfxU32 size)
{
    std::vector<mfxU8> data;
    sei_message_size_and_type(data, type, size);

    size_t B = data.size();

    data.resize(B + size);

    std::fill(data.begin() + B, data.end(), 0xff);

    pl->BufSize = mfxU16(B + size);
    pl->Data    = new mfxU8[pl->BufSize];
    pl->NumBit  = pl->BufSize * 8;
    pl->Type    = mfxU16(type);

    memcpy(pl->Data, &data[0], pl->BufSize);
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_FEI_SUPPORT();

    const tc_struct& tc = test_case[id];

    //Generate SEI message
    for (mfxU16 i = 0; i < tc.sum_group_of_msg; i++) {
        for (mfxU16 p_ind = 0; p_ind < tc.per_frm_sei[i].num_payloads; p_ind++) {
            mfxPayload* p = new mfxPayload;
            auto par = tc.per_frm_sei[i];
            create_sei_message(p, par.payload_types[p_ind], (par.nf + p_ind + 1) * 6);
            m_per_frame_sei[par.nf].push_back(p);
        }
    }

    const mfxU16 n_frames = 5;
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

    m_field = encpak.enc.m_par.mfx.FrameInfo.PicStruct;
    mfxU16 num_fields = encpak.enc.m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;

    encpak.PrepareInitBuffers();

    mfxExtCodingOption* extOpt = new mfxExtCodingOption;
    memset(extOpt, 0, sizeof(mfxExtCodingOption));
    extOpt->Header.BufferId  = MFX_EXTBUFF_CODING_OPTION;
    extOpt->Header.BufferSz  = sizeof(mfxExtCodingOption);
    extOpt->SingleSeiNalUnit = tc.set_par.singleSeiNalUnit;
    encpak.pak.initbuf.push_back(&extOpt->Header);

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

            sts = encpak.EncodeFrame(field);
            g_tsStatus.expect(tc.sts); // if fails check if it is expected
            if (sts != MFX_ERR_NONE)
                break;
        }
    }

    g_tsLog << count << " FRAMES Encoded\n";

    g_tsStatus.enable();
    int ret = g_tsStatus.check(sts);

    for(std::map<mfxU32, std::vector<mfxPayload*>>::iterator i_map = m_per_frame_sei.begin(); i_map != m_per_frame_sei.end(); i_map++) {
        for(std::vector<mfxPayload*>::iterator i_pl =  i_map->second.begin(); i_pl != i_map->second.end(); i_pl++) {
            if((*i_pl) && (*i_pl)->Data) {
                delete[] (*i_pl)->Data;
                delete *i_pl;
            }
        }
        i_map->second.clear();
    }
    m_per_frame_sei.clear();

    if (extOpt)
       {
           delete extOpt;
           extOpt = NULL;
       }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encpak_sei_payload);
}
