/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

/* ****************************************************************************** *\
1. The test will create SEI message for mfxPayload;
2. Attach those info of mfxPayload into mfxEncodeCtrl in the specific frames;
3. In condition of progressive and filed pictures;
4. Insert messages to I, P, B frames;
5. In the output bitstream, below info should be checked:
     a. Right payload type and size
     b. Right number of the messages
     c. Right place of the inserted messages
\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#define BS_LEN 1024*1024*10

namespace fei_encode_sei_payload
{

class TestSuite : tsVideoEncoder, tsSurfaceProcessor, tsBitstreamProcessor, tsParserH264AU
{
public:
    static const unsigned int n_cases;

    TestSuite()
        : tsVideoEncoder(MFX_FEI_FUNCTION_ENCODE, MFX_CODEC_AVC, true)
        , tsParserH264AU(BS_H264_INIT_MODE_DEFAULT)
        , m_nframes(10)
        , m_frm_in(0)
        , m_frm_out(0)
        , m_noiser(10)
        , m_offset(0)
        , m_writer("debug.h264")
    {
        m_filler = this;
        m_bs_processor = this;
    }
    ~TestSuite() {}

    int RunTest(unsigned int id);

private:
    std::map<mfxU32, std::vector<mfxPayload*>> m_per_frame_sei;
    mfxU32 m_nframes;
    mfxU32 m_frm_in;
    mfxU32 m_frm_out;
    tsNoiseFiller m_noiser;
    mfxU32 m_offset;
    tsBitstreamWriter m_writer;
    mfxU32 m_field;
    mfxU8 *m_pCompleteBS;

    mfxStatus ProcessSurface(mfxFrameSurface1&);
    mfxStatus ProcessBitstream(mfxBitstream &bs, mfxU32 nFrames);
    void sei_message_size_and_type(std::vector<mfxU8>& data, mfxU16 type, mfxU32 size);
    void create_sei_message(mfxPayload* pl, mfxU16 type, mfxU32 size);

    enum {
        MFX_PAR = 1,
        EXT_COD,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 sum_group_of_msg; //suppose max to 6
        struct
        {
            mfxU16 nf; //which frame the sei will be attached
            mfxU16 num_payloads;
            mfxU16 payload_types[6];
        } per_frm_sei[6];
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& pSurf)
{
    if (m_frm_in > (m_nframes - 1))
        return MFX_ERR_NONE;

    m_noiser.ProcessSurface(pSurf);

    if (m_per_frame_sei.find(m_frm_in) != m_per_frame_sei.end()) { // if current frame has the sei msg, find it out and attach it.
        m_ctrl.Payload = &(m_per_frame_sei[m_frm_in])[0];
        m_ctrl.NumPayload = (mfxU16)m_per_frame_sei[m_frm_in].size();
    } else {
        m_ctrl.Payload = NULL;
        m_ctrl.NumPayload = 0;
    }

    m_frm_in++;
    return MFX_ERR_NONE;
}

mfxStatus TestSuite::ProcessBitstream(mfxBitstream &bs, mfxU32 nFrames)
{

    if((m_offset + bs.DataLength) > BS_LEN){
        g_tsLog << "ERROR: Too much bitsteam\n";
        return MFX_ERR_ABORTED;
    }

    memcpy(m_pCompleteBS + m_offset, bs.Data + bs.DataOffset, bs.DataLength);
    m_offset += bs.DataLength;
    m_frm_out++;

    //won't write output for every case, just one for debug
    mfxU32 tmp;
    tmp = bs.DataLength;
    m_writer.ProcessBitstream(bs, nFrames);
    bs.DataLength = tmp;

    if(m_frm_out != m_nframes) {
        bs.DataLength = 0;
        return MFX_ERR_NONE;
    } else { //The last frame
        bs.DataLength = m_offset;
        bs.DataOffset = 0;
        memcpy(bs.Data, m_pCompleteBS, m_offset);
        SetBuffer(bs);

        bool bff = (m_field == MFX_PICSTRUCT_FIELD_BFF);
        mfxU8 frm_field = (m_field == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2;
        mfxU8 fieldId = 0;
        mfxU8 i = 0;

        struct payload {
            mfxU16 Type;
            mfxU16 BufSize;
        };

        std::vector<payload> found_pl;
        BS_H264_au au;
        Bs32u frm_count = 0;
        Bs32s last_poc_even = 0;
        Bs32s gop_count = -1;
        Bs16u gop_size = m_par.mfx.GopPicSize ? m_par.mfx.GopPicSize : 0xffff;
        while(1) {
            au = ParseOrDie();
            if(m_sts == BS_ERR_MORE_DATA) { //end of bs buffer
                return MFX_ERR_NONE;
            }
            for (Bs32u au_ind = 0; au_ind < au.NumUnits; au_ind++) {
                auto nalu = &au.NALU[au_ind];

                if((nalu->nal_unit_type == 0x01 || nalu->nal_unit_type == 0x05)) {
                    slice_header& s = *nalu->slice_hdr;
                    if(m_field == MFX_PICSTRUCT_FIELD_BFF) {
                        fieldId = s.bottom_field_flag ? 0 : 1;
                    }
                    if(m_field == MFX_PICSTRUCT_FIELD_TFF) {
                        fieldId = s.bottom_field_flag ? 1 : 0;
                    }

                    if(s.PicOrderCnt == 0)
                        ++gop_count;
                    if(last_poc_even != (s.PicOrderCnt / 2)) {
                        last_poc_even = (s.PicOrderCnt / 2);
                        frm_count = last_poc_even + gop_count * gop_size;
                    }
                    if((m_per_frame_sei.find(frm_count) != m_per_frame_sei.end())) {
                        //For field pictures, odd payloads are associated with the first field and even payloads are associated with the second field.
                        mfxU32 num_sei = 0;
                        for (auto p : found_pl) {
                            //compare with the preset sei message of the frm_count frame
                            std::vector<mfxPayload*>::iterator i_exp_p =  m_per_frame_sei[frm_count].begin();
                            if(frm_field > 1) {
                                i_exp_p += fieldId;
                            }
                            for (i_exp_p; i_exp_p < m_per_frame_sei[frm_count].end(); i_exp_p += frm_field) {
                                //  recalc real size, because exp_p->BufSize = SEI size + header_size(type + size)
                                mfxU16 size = (*i_exp_p)->BufSize;
                                std::vector<mfxU8> data;
                                sei_message_size_and_type(data, (*i_exp_p)->Type, (*i_exp_p)->BufSize);
                                size -= (mfxU16)data.size();

                                // whether the payload match one of it in the messages group
                                if (p.BufSize == size && p.Type == (*i_exp_p)->Type) {
                                    num_sei++;
                                }
                            }
                            g_tsLog << "The SEI in the " << frm_count << " frame: "  << "Type " << p.Type << " Size " << p.BufSize << "\n";
                        }
                        //only with the correct type and buffer size of each payload in the bitstream, the number of messages could be correct.
                        EXPECT_EQ((frm_field > 1) ? m_per_frame_sei[frm_count].size()/2 : m_per_frame_sei[frm_count].size(), num_sei)
                                << "ERROR: incorrect number of SEI messages, some lost. \n";
                    }

                }

                if(nalu->nal_unit_type == 0x06) { //SEI
                    found_pl.clear();
                    for (Bs16u sei_ind = 0; sei_ind < nalu->SEI->numMessages; ++sei_ind) {
                        payload p = {};
                        p.BufSize = nalu->SEI->message[sei_ind].payloadSize;
                        p.Type = nalu->SEI->message[sei_ind].payloadType;
                        found_pl.push_back(p);
                    }
                }
            }
        }
    }
    bs.DataLength = 0;
    return MFX_ERR_NONE;
}

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*01*/ MFX_ERR_NONE, 1,
            {{0, 1, {5}}}},

    {/*02*/ MFX_ERR_NONE, 1,
            {{0, 2, {5, 5}}}}, //with 2 extra SEI inserted, the parser return the BS_ERR_WRONG_UNITS_ORDER

    {/*03*/ MFX_ERR_NONE, 2,
            {{0, 1, {5}},{3, 2, {5, 5}}}},

    {/*04*/ MFX_ERR_NONE, 2,
            {{0, 2, {5, 5}},{3, 2, {5, 5}}}},

    {/*05*/ MFX_ERR_NONE, 3,
            {{0, 1, {5}},{3, 1, {5}},{7, 3, {5, 5, 5}}}},

    {/*06*/ MFX_ERR_NONE, 2,
            {{0, 1, {5}},{4, 2, {5, 5}}}, //the parser can't parse 2 type of 5 SEI inserted in IDR(5th frame in this case)
            {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 4},
            {EXT_COD, &tsStruct::mfxExtCodingOption.SingleSeiNalUnit, MFX_CODINGOPTION_ON}}},

    {/*07*/ MFX_ERR_NONE, 2,
            {{0, 1, {5}},{4, 1, {5}}},
            {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 4}}},

    {/*08*/ MFX_ERR_NONE, 2,
            {{0, 1, {5}},{3, 2, {5, 5}}},
            {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 4}}},

    {/*09*/ MFX_ERR_NONE, 2,
            {{0, 1, {5}},{5, 2, {5, 5}}},
            {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 4}}},

    {/*10*/ MFX_ERR_NONE, 2,
            {{0, 1, {5}},{3, 2, {5, 5}}},
            {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 4},
             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},

    {/*11*/ MFX_ERR_NONE, 2,
            {{0, 1, {5}},{3, 2, {5, 5}}},
            {{EXT_COD, &tsStruct::mfxExtCodingOption.SingleSeiNalUnit, MFX_CODINGOPTION_OFF}}},

    {/*12*/ MFX_ERR_NONE, 2,
            {{0, 1, {5}},{3, 2, {5, 5}}},
            {{EXT_COD, &tsStruct::mfxExtCodingOption.SingleSeiNalUnit, MFX_CODINGOPTION_ON}}},

    {/*13*/ MFX_ERR_NONE, 1,
            {{0, 2, {5, 5}}},
            {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF}}},

    {/*14*/ MFX_ERR_NONE, 1,
            {{0, 2, {5, 5}}},
            {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF}}},

    {/*15*/ MFX_ERR_NONE, 2,
            {{0, 2, {5, 5}},{3, 2, {5, 5}}},
            {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
              {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 4}}},

    {/*16*/ MFX_ERR_NONE, 2,
            {{0, 2, {5, 5}},{3, 2, {5, 5}}},
            {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
              {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 4}}},

    {/*17*/ MFX_ERR_NONE, 2,
            {{0, 2, {5, 5}},{4, 2, {5, 5}}},
            {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
              {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 4}}},

    {/*18*/ MFX_ERR_NONE, 2,
            {{0, 2, {5, 5}},{3, 2, {5, 5}}},
            {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
              {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 4},
              {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

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

    pl->BufSize = mfxU16(B + size);
    pl->Data    = new mfxU8[pl->BufSize];
    pl->NumBit  = pl->BufSize * 8;
    pl->Type    = mfxU16(type);

    memcpy(pl->Data, &data[0], pl->BufSize);
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    srand(0);
    const tc_struct& tc = test_case[id];

    m_par.AsyncDepth = 1;

    for (mfxU16 i = 0; i < tc.sum_group_of_msg; i++) {
        for (mfxU16 p_ind = 0; p_ind < tc.per_frm_sei[i].num_payloads; p_ind++) {
            mfxPayload* p = new mfxPayload;
            auto par = tc.per_frm_sei[i];
            //currently only insert user data type to try, which buffer size will be 17 according to the spec
            create_sei_message(p, par.payload_types[p_ind], 17);
            m_per_frame_sei[par.nf].push_back(p);
        }
    }

    SETPARS(m_pPar, MFX_PAR);

    mfxExtCodingOption& cod = m_par;
    SETPARS(&cod, EXT_COD);

    m_field = m_pPar->mfx.FrameInfo.PicStruct;
    mfxExtFeiEncFrameCtrl in_efc[2];
    std::vector<mfxExtBuffer*> in_buffs;

    for (mfxU32 fieldId = 0; fieldId < ((m_field == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2); fieldId++) {
        in_efc[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
        in_efc[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);
        in_efc[fieldId].SearchPath = 57;
        in_efc[fieldId].LenSP = 57;
        in_efc[fieldId].MultiPredL0 = 0;
        in_efc[fieldId].MultiPredL1 = 0;
        in_efc[fieldId].SubPelMode = 3;
        in_efc[fieldId].InterSAD = 2;
        in_efc[fieldId].IntraSAD = 2;
        in_efc[fieldId].DistortionType = 2;
        in_efc[fieldId].RepartitionCheckEnable = 0;
        in_efc[fieldId].AdaptiveSearch = 1;
        in_efc[fieldId].MVPredictor = 0;
        in_efc[fieldId].NumMVPredictors[0] = 0;
        in_efc[fieldId].PerMBQp = 0;
        in_efc[fieldId].PerMBInput = 0;
        in_efc[fieldId].MBSizeCtrl = 0;
        in_efc[fieldId].SearchWindow = 2;
        in_buffs.push_back((mfxExtBuffer*)&in_efc[fieldId]);
    }
    m_ctrl.ExtParam = &in_buffs[0];
    m_ctrl.NumExtParam = (mfxU16)in_buffs.size();

    MFXInit();

    Init();
    AllocBitstream(BS_LEN);
    m_pCompleteBS = new mfxU8[BS_LEN];

    g_tsStatus.expect(tc.sts);
    EncodeFrames(m_nframes, true);

    delete[] m_pCompleteBS;
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

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_sei_payload);
}
