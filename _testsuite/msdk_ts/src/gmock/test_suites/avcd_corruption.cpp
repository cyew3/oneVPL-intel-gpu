/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2016 Intel Corporation. All Rights Reserved.
//
//
*/

#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

#define TEST_NAME avcd_corruption
#define NSTREAMS 5

namespace TEST_NAME
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>*);

class H264BsPerFrameReader : public tsBitstreamProcessor, public tsReader, private tsParserH264AU
{
public:
    H264BsPerFrameReader(const char* fname, mfxU32 buf_size = 1920*1088*3/2)
        : tsBitstreamProcessor()
        , tsReader(fname)
        , tsParserH264AU()
        , m_buf_size(buf_size)
        , bs_eos(false)
        , m_eos(false)
        , m_buf(0)
        , curr_pos(0)
        , bs_offset(0)
        , repeats(0)
        , filename(fname)
        , pic_order_cnt_type(0)
    {
        BSErr bs_sts = tsParserH264AU::open(filename);
        EXPECT_EQ(BS_ERR_NONE, bs_sts) << "ERROR: Parser failed to open input file: " << filename << "\n";
        assert(0 != fname);

        tsParserH264AU::set_trace_level(0);

        bs_offset = BS_H264_parser::get_offset();

        m_buf = new mfxU8[m_buf_size];
        memset(m_buf,0,m_buf_size);

        m_frames_found = 0;
        m_bytes_sent   = 0;
        m_frame_num    = 0;
    };
    ~H264BsPerFrameReader()
    {
        if(m_buf)
        {
            delete[] m_buf;
            m_buf = 0;
        }
    };

    const mfxU32  m_buf_size;
    bool    bs_eos;
    bool    m_eos;
    mfxU8*  m_buf;
    mfxU64  curr_pos;
    mfxU64  bs_offset;

    mfxU32  repeats; //DecodeHeader, SetPar4_DecodeFrameAsync performs additional BitStream update call, this counter to skip this steps

    mfxU32  m_frame_num;
    mfxU32  m_frames_found;
    mfxU32  m_bytes_sent;

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if(!nFrames)
            nFrames = 1;

        //DecodeHeader(), SetPar4_DecodeFrameAsync() wrapper-functions performs
        //additional Bitstream update call, this counter to skip this steps
        if(0 == bs.DataOffset && bs.DataLength && bs.DataLength == curr_pos)
        {
            if( ++repeats < 4)
                return MFX_ERR_NONE;
        }
        repeats = 0;

        bool frame_found = false;
        //find frame(s) location in the stream
        for(mfxU32 i(0); i < nFrames; ++i)
        {
            mfxU64 offset = 0;
            mfxU64 prev_offset = bs_offset;
            bool first_mb_found = false;
            while(!frame_found && !bs_eos)
            {
                BS_H264_au* pAU = 0;
                BSErr bs_sts = BS_ERR_NONE;
                offset = BS_H264_parser::get_offset();
                if(offset == prev_offset)
                {
                    bs_sts = BS_H264_parser::parse_next_unit();
                }

                if(BS_ERR_MORE_DATA == bs_sts)
                {
                    bs_eos = true;
                    offset = BS_H264_parser::get_offset();
                    prev_offset = offset;

                    if(first_mb_found)
                    {
                        frame_found = true;
                        ++m_frames_found;
                    }
                }
                else
                {
                    EXPECT_EQ(BS_ERR_NONE, bs_sts) << "ERROR: Parser failed to parse input file: " << filename << "\n";
                    BS_H264_header * hdr = (BS_H264_header*) BS_H264_parser::get_header();

                    offset = BS_H264_parser::get_offset();

                    if(0x01 == hdr->nal_unit_type ||0x05 == hdr->nal_unit_type)
                    {
                        if(hdr->slice_hdr->first_mb_in_slice == 0)
                        {
                            if(!first_mb_found)
                            {
                                first_mb_found = true;
                                if(2 == pic_order_cnt_type)
                                    m_frame_num = m_frames_found;
                                else
                                    m_frame_num = hdr->slice_hdr->pic_order_cnt_lsb / 2; //TODO: check later
                            }
                            else
                            {
                                frame_found = true;
                                ++m_frames_found;
                                break;
                            }
                        }
                    }
                    else if(0x07 == hdr->nal_unit_type) //SPS
                    {
                        pic_order_cnt_type = hdr->SPS->pic_order_cnt_type;
                        prev_offset = offset;
                        continue;
                    }
                    else if(0x06 == hdr->nal_unit_type) //SEI
                    {
                        prev_offset = offset;
                        continue;
                    }
                    else if(0x12 == hdr->nal_unit_type ||0x20 == hdr->nal_unit_type) //Filler data || Coded slice extension goes after slice it belongs to
                    {
                        prev_offset = offset;
                        continue;
                    }
                    else if(first_mb_found)
                    {
                        frame_found = true;
                        ++m_frames_found;
                        break;
                    }
                }
                prev_offset = offset;
            }
            bs_offset = prev_offset;
        }

        //update Bitstream from file
        if(!m_eos)
        {
            if(bs.Data != m_buf)
            {
                bs.DataOffset = 0;
                bs.Data      = m_buf;
                bs.MaxLength = m_buf_size;
                bs.DataLength = 0;
            }

            if(bs.DataLength && bs.DataOffset)
            {
                memmove(bs.Data, bs.Data + bs.DataOffset, bs.DataLength);
                bs.DataOffset = 0;
            }
            if(!bs.DataLength)
            {
                bs.DataOffset = 0;
            }
            mfxU32 toRead = (mfxU32) bs_offset - (mfxU32) curr_pos;
            if(0 == toRead)
            {
                bs.DataFlag = MFX_BITSTREAM_EOS;
                m_eos = true;
                if(bs.DataLength)
                    return MFX_ERR_NONE;
                else
                    return MFX_ERR_MORE_DATA;
            }

            assert(curr_pos <= bs_offset);
            mfxU32 remaining = bs.MaxLength - bs.DataLength;
            if(toRead > remaining)
                toRead = remaining;

            mfxU32 read = Read(bs.Data + bs.DataLength, toRead);
            if(toRead != read)
                m_eos = true;

            bs.DataLength += read;

            if(m_eos)
                bs.DataFlag = MFX_BITSTREAM_EOS;
            else 
                bs.DataFlag = 0;

            curr_pos += read;

            m_bytes_sent += bs.DataLength;

            return MFX_ERR_NONE;
        }

        return MFX_ERR_MORE_DATA; 
    }
private:
    const char* filename;
    Bs32u  pic_order_cnt_type;
};

class TestSuite : public tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_AVC) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    struct f_pair
    {
        const  tsStruct::Field* f;
        mfxU64 v;
        bool   isInvalid;
    };

    enum
    {
        fDecodeFrameAsync     = 1,
        fDecodeFrameAsync_new = 1 << 2,
        fGetVideoParam        = 1 << 3,
        fDecodeHeader         = 1 << 4,
        fGetPayload           = 1 << 5,
        fGetDecodeStat        = 1 << 6
    };
    enum
    {
        eTestTable     = 1,
        eTrash,
    };

private:
    struct tc_struct
    {
        //string Descr;
        //string Tag;
        mfxU32      function;
        mfxStatus   sts;
        struct 
        {
            mfxU32      n;
            const char* name;
            mfxU8       flags[32];
            mfxU8       stream;
        } streams[NSTREAMS];
        f_pair      set_par[MAX_NPARS]; //main test parameters to be set
        callback    func;               //additional parameters that needed to be set as a dependency to main, e.g. bitrate parameters for RateControl tests
    };

    int RunTestDecodeFrameAsync(const tc_struct& tc);
    int RunTestDecodeFrameAsync_new(const tc_struct& tc);

    static const tc_struct test_case[];
};

class Reader : public H264BsPerFrameReader
{
public:
    const mfxU64 startTimeStamp;
    const mfxU64 stepTimeStamp;
    mfxU32 count;

    Reader(const char* fname, mfxU32 buf_size, mfxU64 start = 0, mfxU64 step = 0)
        : H264BsPerFrameReader(fname, buf_size),
            startTimeStamp(start), stepTimeStamp(step), count(0)
    {
    }
    ~Reader() {};

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if(MFX_WRN_VIDEO_PARAM_CHANGED == g_tsStatus.get() && bs.DataLength)
            return MFX_ERR_NONE;

        mfxStatus sts = H264BsPerFrameReader::ProcessBitstream(bs, nFrames);
        if(MFX_ERR_NONE == sts)
        {
            bs.TimeStamp = startTimeStamp + stepTimeStamp*m_frame_num;
            ++count;
        }
        return sts;
    }
};

class Verifier : public tsSurfaceProcessor
{
public:
    mfxU16 frame;
    mfxU32 StreamFrameCount;
    mfxU32 TotalFrameCount;
    const tsExtBufType<mfxVideoParam>& init_par;
    const mfxU64 startTimeStamp;
    const mfxU64 stepTimeStamp;
    const mfxSession m_session;    
    std::list <mfxU8> expected_flag;

    Verifier(const mfxU8* flags, const mfxU16 N, const tsExtBufType<mfxVideoParam>& par, mfxU64 start = 0, mfxU64 step = 0, const mfxSession _session = 0)
        : tsSurfaceProcessor()
        , frame(0)
        , StreamFrameCount(0)
        , TotalFrameCount(0)
        , init_par(par)
        , startTimeStamp(start)
        , stepTimeStamp(step)
        , m_session(_session)        
        , expected_flag(flags, flags + N)        
    {}

    ~Verifier() {}

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        const mfxU16 Corrupted = s.Data.Corrupted;
        const mfxU64 TimeStamp = s.Data.TimeStamp;

        if (expected_flag.empty())
            return MFX_ERR_NONE;
        const mfxU16 Expected = expected_flag.front();
        expected_flag.pop_front();
        EXPECT_EQ(Expected, Corrupted) << "ERROR: Frame#" << frame << " reported Corrupted value = " << Corrupted  << " is not equal to expected = " << Expected << "\n";
        if(startTimeStamp && stepTimeStamp)
        {
            const mfxU64 ExpTS = startTimeStamp + stepTimeStamp*StreamFrameCount;
            EXPECT_EQ(ExpTS,    TimeStamp) << "ERROR: Frame#" << frame << " reported TimeStamp value = " << TimeStamp  << " is not equal to expected = " << ExpTS << "\n";
        }
        StreamFrameCount++;
        TotalFrameCount++;

#define CHECK_FIELD_EXP_ACT(EXPECTED, ACTUAL, FIELD) \
do {                                                 \
    if(ACTUAL.FIELD && EXPECTED.FIELD){              \
        EXPECT_EQ(EXPECTED.FIELD, ACTUAL.FIELD);     \
    }                                                \
} while (0,0)

#define CHECK_FIELD(FIELD)    CHECK_FIELD_EXP_ACT(init_par.mfx.FrameInfo, s.Info, FIELD)
        CHECK_FIELD(BitDepthLuma  );
        CHECK_FIELD(BitDepthChroma);
        CHECK_FIELD(Shift         );
        CHECK_FIELD(FourCC        );
        CHECK_FIELD(Width         );
        CHECK_FIELD(Height        );
        CHECK_FIELD(CropX         );
        CHECK_FIELD(CropY         );
        CHECK_FIELD(CropW         );
        CHECK_FIELD(CropH         );
        CHECK_FIELD(FrameRateExtN );
        CHECK_FIELD(FrameRateExtD );
        CHECK_FIELD(AspectRatioW  );
        CHECK_FIELD(AspectRatioH  );
        CHECK_FIELD(PicStruct     );
        CHECK_FIELD(ChromaFormat  );
#undef CHECK_FIELD

        if(m_session)
        {
            mfxVideoParam GetVideoParamPar = {};

            TRACE_FUNC2(MFXVideoDECODE_GetVideoParam, m_session, &GetVideoParamPar);
            g_tsStatus.expect(MFX_ERR_NONE);
            g_tsStatus.check( MFXVideoDECODE_GetVideoParam(m_session, &GetVideoParamPar) );

#define CHECK_FIELD(FIELD)    CHECK_FIELD_EXP_ACT(GetVideoParamPar.mfx.FrameInfo, s.Info, FIELD)
            CHECK_FIELD(BitDepthLuma  );
            CHECK_FIELD(BitDepthChroma);
            CHECK_FIELD(Shift         );
            CHECK_FIELD(FourCC        );
            CHECK_FIELD(Width         );
            CHECK_FIELD(Height        );
            CHECK_FIELD(CropX         );
            CHECK_FIELD(CropY         );
            CHECK_FIELD(CropW         );
            CHECK_FIELD(CropH         );
            CHECK_FIELD(FrameRateExtN );
            CHECK_FIELD(FrameRateExtD );
            CHECK_FIELD(AspectRatioW  );
            CHECK_FIELD(AspectRatioH  );
            CHECK_FIELD(PicStruct     );
            CHECK_FIELD(ChromaFormat  );
#undef CHECK_FIELD
#undef CHECK_FIELD_EXP_ACT

            tsExtBufType<mfxVideoParam> par;
            mfxExtCodingOptionSPSPPS&   spspps = par;

            mfxU8 spsBuf[512] = {0};
            mfxU8 ppsBuf[512] = {0};
            spspps.SPSBuffer = spsBuf;
            spspps.PPSBuffer = ppsBuf;
            spspps.SPSBufSize = sizeof(spsBuf);
            spspps.PPSBufSize = sizeof(ppsBuf);

            TRACE_FUNC2(MFXVideoDECODE_GetVideoParam, m_session, &par);
            g_tsStatus.expect(MFX_ERR_NONE);
            g_tsStatus.check( MFXVideoDECODE_GetVideoParam(m_session, &par) );

            mfxDecodeStat stat = {};

            TRACE_FUNC2(MFXVideoDECODE_GetDecodeStat, m_session, &stat);
            g_tsStatus.expect(MFX_ERR_NONE);
            g_tsStatus.check( MFXVideoDECODE_GetDecodeStat(m_session, &stat) );

            EXPECT_EQ(TotalFrameCount, stat.NumFrame);
        }

        frame++;
        return MFX_ERR_NONE;
    }
};

/* Corrupted in mfxFrameData */
enum {
    MINOR     = MFX_CORRUPTION_MINOR              ,
    MAJOR     = MFX_CORRUPTION_MAJOR              ,
    ABSENT_TF = MFX_CORRUPTION_ABSENT_TOP_FIELD   ,
    ABSENT_BF = MFX_CORRUPTION_ABSENT_BOTTOM_FIELD,
    REF_FRAME = MFX_CORRUPTION_REFERENCE_FRAME    ,
    REF_LIST  = MFX_CORRUPTION_REFERENCE_LIST     
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    //legacy v30_behavior_h264d_corruption and behavior_avcd_corruption 
    {/*00*/ fDecodeFrameAsync, MFX_ERR_NONE, {6, "forBehaviorTest/corrupted/iceage_720x576_6_IPP_cr.264"   , {MAJOR,REF_FRAME,REF_FRAME,0,0,0                                                    },},},
    {/*01*/ fDecodeFrameAsync, MFX_ERR_NONE, {6, "forBehaviorTest/corrupted/iceage_720x576_6_IPP_1P_cr.264", {0,MAJOR,REF_FRAME,0,0,0                                                            },},},
    {/*02*/ fDecodeFrameAsync, MFX_ERR_NONE, {6, "forBehaviorTest/corrupted/iceage_720x576_6_IPP_2I_cr.264", {0,0,0,MAJOR,REF_FRAME,REF_FRAME                                                    },},},
    {/*03*/ fDecodeFrameAsync, MFX_ERR_NONE, {6, "forBehaviorTest/corrupted/ref_pic_mark_rep_1idr.264"     , {0,REF_LIST,REF_LIST,REF_LIST,REF_LIST,REF_LIST                                     },},},
    {/*04*/ fDecodeFrameAsync, MFX_ERR_NONE, {6, "forBehaviorTest/corrupted/ref_pic_mark_rep_all_idr.264"  , {0,REF_LIST,REF_LIST,0,0,0                                                          },},},
    {/*05*/ fDecodeFrameAsync, MFX_ERR_NONE, {6, "forBehaviorTest/corrupted/fields.264"                    , {ABSENT_BF,REF_FRAME|MAJOR,REF_FRAME|MAJOR|ABSENT_TF,REF_FRAME|MAJOR,REF_FRAME|MAJOR},},},
    //corruption minor try, in last MB of slice

    /* Driver reports significant problem (bStatus = 2) for all error. 
       The reason is that there is no clear definition for Minor or Significant (or Major) problem in DDI, 
       driver doesn't know how to map HW returned flags to Minor or Major result in bStatus. */
    {/*06*/ fDecodeFrameAsync, MFX_ERR_NONE, {6, "forBehaviorTest/corrupted/matrix_1920x1080_6i_minor_on_2_zeroes.264" , {0,/*MINOR*/MAJOR,0,0,0,0},},},
    {/*07*/ fDecodeFrameAsync, MFX_ERR_NONE, {6, "forBehaviorTest/corrupted/matrix_1920x1080_6i_minor_on_2_trash.264" ,  {0,/*MINOR*/MAJOR,0,0,0,0},},},

    //1 - decoding, 2 - trash/corruptions, 3 - decoding, 4 - trash/corruptions, 5 - decoding
    // + output fields (mfxFrameInfo and timestamps check)
    {/*08*/ fDecodeFrameAsync_new, MFX_ERR_NONE, { {6, "forBehaviorTest/corrupted/matrix_1920x1080_6i_minor_on_2_zeroes.264" , {0,/*MINOR*/MAJOR,0,0,0,0},},
                                                 {5, 0, {0}, 1},
                                                 {6, "forBehaviorTest/corrupted/matrix_1920x1080_6i_minor_on_2_zeroes.264"  , {0,/*MINOR*/MAJOR,0,0,0,0},},
                                                 {8, 0, {0}, 1},
                                                 {6, "forBehaviorTest/corrupted/matrix_1920x1080_6i_minor_on_2_zeroes.264" , {0,/*MINOR*/MAJOR,0,0,0,0},}
                                               }, },
    {/*09*/ fDecodeFrameAsync_new, MFX_ERR_NONE, { {6, "forBehaviorTest/corrupted/iceage_720x576_6_IPP_cr.264"   , {MAJOR,REF_FRAME,REF_FRAME,0,0,0 },},
                                                 {5, 0, {0}, 1},
                                                 {6, "forBehaviorTest/corrupted/iceage_720x576_6_IPP_2I_cr.264", {0,0,0,MAJOR,REF_FRAME,REF_FRAME },},
                                                 {8, 0, {0}, 1},
                                                 {6, "forBehaviorTest/corrupted/iceage_720x576_6_IPP_1P_cr.264", {0,MAJOR,REF_FRAME,0,0,0         },}
                                               }, },

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTestDecodeFrameAsync(const tc_struct& tc)
{
    const char* sname = g_tsStreamPool.Get(tc.streams[0].name);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 100000);
    m_bs_processor = &reader;

    Init();

    SetPar4_DecodeFrameAsync();

    Verifier v(tc.streams[0].flags, tc.streams[0].n, m_par);

    m_surf_processor = &v;

    DecodeFrames(tc.streams[0].n);

    return 0;
}

int TestSuite::RunTestDecodeFrameAsync_new(const tc_struct& tc)
{
    const char* sname[NSTREAMS] = { ( (tc.streams[0].name && strlen(tc.streams[0].name)) ? g_tsStreamPool.Get(tc.streams[0].name) : 0 ),
                                    ( (tc.streams[1].name && strlen(tc.streams[1].name)) ? g_tsStreamPool.Get(tc.streams[1].name) : 0 ),
                                    ( (tc.streams[2].name && strlen(tc.streams[2].name)) ? g_tsStreamPool.Get(tc.streams[2].name) : 0 ),
                                    ( (tc.streams[3].name && strlen(tc.streams[3].name)) ? g_tsStreamPool.Get(tc.streams[3].name) : 0 ),
                                    ( (tc.streams[4].name && strlen(tc.streams[4].name)) ? g_tsStreamPool.Get(tc.streams[4].name) : 0 ), };
    g_tsStreamPool.Reg();

    mfxU32 ts_start = 20;
    mfxU32 ts_step  = 33;
    mfxU32 count = 0;
    for(size_t i(0); i < NSTREAMS; ++i)
    {
        if(sname[i] && 0 != strlen( sname[i] ) )
        {
            Reader reader(sname[i], 1000000, ts_start, ts_step);
            m_bs_processor = &reader;
            m_update_bs = true;

            if(!m_initialized)
            {
                Init();
                m_bitstream.DataLength += m_bitstream.DataOffset;
                m_bitstream.DataOffset = 0;
                SetPar4_DecodeFrameAsync();
            }

            Verifier v(tc.streams[i].flags, tc.streams[i].n, m_par, ts_start, ts_step, m_session);
            v.TotalFrameCount = count;
            m_surf_processor = &v;

            reader.count = 0;
            m_bitstream.DataLength += m_bitstream.DataOffset;
            m_bitstream.DataOffset = 0;
            DecodeFrames(tc.streams[i].n);

            m_bs_processor = 0;
            m_surf_processor = 0;

            ts_start = ts_start + ts_step * reader.count;
            count = v.TotalFrameCount;
        }
        else
        {
            assert(0 != tc.streams[i].stream);
            if(tc.streams[i].stream == eTestTable)
            {
                mfxBitstream bs = {0};
                bs.Data = (mfxU8*) &test_case;
                bs.DataLength = bs.MaxLength = sizeof(test_case);
                bs.TimeStamp = 0xBAADF00D;

                m_pSurf = GetSurface();

                g_tsStatus.expect(MFX_ERR_MORE_DATA);
                DecodeFrameAsync(m_session, &bs, m_pSurf, &m_pSurfOut, &m_syncpoint);
                g_tsStatus.check();
            }
        }
    }

    return 0;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    if(0 == tc.function)
    {
        EXPECT_NE(0u, tc.function) << "ERROR: test is not implemented\n";
        throw tsFAIL;
    }

    if(fDecodeFrameAsync & tc.function)
        RunTestDecodeFrameAsync(tc);

    if(fDecodeFrameAsync_new & tc.function)
        RunTestDecodeFrameAsync_new(tc);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(TEST_NAME);
#undef TEST_NAME

}
