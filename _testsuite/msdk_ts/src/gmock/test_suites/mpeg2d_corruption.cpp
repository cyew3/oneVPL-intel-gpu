#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

#define TEST_NAME mpeg2d_corruption
#define NSTREAMS 5

namespace TEST_NAME
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>*);

class MPEG2BsPerFrameReader : public tsBitstreamProcessor, public tsReader, private tsParserMPEG2
{
public:
    MPEG2BsPerFrameReader(const char* fname, mfxU32 buf_size = 1920*1088*3/2)
        : filename(fname), m_eos(false), bs_eos(false), curr_pos(0), m_buf(0), m_buf_size(buf_size), repeats(0), bs_offset(0), m_first_pic_header(false),
          tsReader(fname)
    {
        BSErr bs_sts = tsParserMPEG2::open(filename);
        EXPECT_EQ(BS_ERR_NONE, bs_sts) << "ERROR: Parser failed to open input file: " << filename << "\n";
        assert(0 != fname);

        tsParserMPEG2::set_trace_level(0);

        bs_offset = tsParserMPEG2::get_offset();

        m_buf = new mfxU8[m_buf_size];
        memset(m_buf,0,m_buf_size);

        m_frames_found = 0;
        m_bytes_sent   = 0;
        m_frame_num    = 0;
    };
    ~MPEG2BsPerFrameReader()
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

    bool    m_first_pic_header;
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
            bool pic_hdr_found = false;
            while(!frame_found && !bs_eos)
            {
                BS_H264_au* pAU = 0;
                BSErr bs_sts = BS_ERR_NONE;
                offset = tsParserMPEG2::get_offset();
                if(offset == prev_offset)
                {
                    bs_sts = tsParserMPEG2::parse_next_unit();
                }

                if(BS_ERR_MORE_DATA == bs_sts)
                {
                    bs_eos = true;
                    offset = tsParserMPEG2::get_offset();
                    prev_offset = offset;

                    if(pic_hdr_found)
                    {
                        frame_found = true;
                        ++m_frames_found;
                    }
                }
                else
                {
                    EXPECT_EQ(BS_ERR_NONE, bs_sts) << "ERROR: Parser failed to parse input file: " << filename << "\n";
                    BS_MPEG2_header * hdr = (BS_MPEG2_header*) tsParserMPEG2::get_header();

                    offset = tsParserMPEG2::get_offset();

                    if(hdr->start_code == 0x00000100) // picture header
                    {
                        if(!m_first_pic_header)
                        {
                            prev_offset = offset;
                            m_first_pic_header = true;
                            continue;
                        }
                        if(!pic_hdr_found)
                        {
                            pic_hdr_found = true;
                            ++m_frame_num;
                        }
                        else
                        {
                            frame_found = true;
                            ++m_frames_found;
                            break;
                        }
                    }
                    else if(hdr->start_code == 0x000001B5 && hdr->start_code_id == (1 << 3)) //(extension header && Picture_Coding_Extension)
                    {
                        prev_offset = offset;
                        continue;
                    }
                    else if(pic_hdr_found)
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
            toRead += 6;

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
            else if(frame_found)
                bs.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;

            curr_pos += read;

            m_bytes_sent += bs.DataLength;

            return MFX_ERR_NONE;
        }

        return MFX_ERR_MORE_DATA; 
    }
private:
    const char* filename;
};

class TestSuite : public tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_MPEG2) {}
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
        fGetDecodeStat        = 1 << 6,
        noFlagCheck           = 1 << 7
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
    };

    int RunTestDecodeFrameAsync(const tc_struct& tc);
    int RunTestDecodeFrameAsync_new(const tc_struct& tc);

    static const tc_struct test_case[];
};

class Reader : public MPEG2BsPerFrameReader
{
public:
    const mfxU64 startTimeStamp;
    const mfxU64 stepTimeStamp;
    mfxU32 count;

    Reader(const char* fname, mfxU32 buf_size, mfxU64 start = 0, mfxU64 step = 0)
        : MPEG2BsPerFrameReader(fname, buf_size),
            startTimeStamp(start), stepTimeStamp(step), count(0)
    {
    }
    ~Reader() {};

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if(MFX_WRN_VIDEO_PARAM_CHANGED == g_tsStatus.get() && bs.DataLength)
            return MFX_ERR_NONE;
        if(MFX_ERR_MORE_SURFACE == g_tsStatus.get() && bs.DataLength)
        {
            if(bs.DataOffset)
                bs.DataFlag = 0;
            return MFX_ERR_NONE;
        }

        mfxStatus sts = MPEG2BsPerFrameReader::ProcessBitstream(bs, nFrames);
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
    const tsExtBufType<mfxVideoParam>& init_par;
    const mfxU64 startTimeStamp;
    const mfxU64 stepTimeStamp;
    const mfxSession m_session;
    mfxU32 count;
    std::list <mfxU8> expected_flag;
    mfxU16 frame;
    bool  noFlagCheck;
    mfxU32 expectSpsSize;

    Verifier(const mfxU8* flags, const mfxU16 N, const tsExtBufType<mfxVideoParam>& par, mfxU64 start = 0, mfxU64 step = 0, const mfxSession _session = 0)
        : expected_flag(flags, flags + N), init_par(par), frame(0), m_session(_session),
            startTimeStamp(start), stepTimeStamp(step), count(0), noFlagCheck(false), expectSpsSize(0){}
    ~Verifier() {}

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        const mfxU16 Corrupted = s.Data.Corrupted;
        const mfxU64 TimeStamp = s.Data.TimeStamp;

        if (expected_flag.empty())
            return MFX_ERR_NONE;
        const mfxU16 Expected = expected_flag.front();
        expected_flag.pop_front();
        if(!noFlagCheck)
        {
            EXPECT_EQ(Expected, Corrupted) << "ERROR: Frame#" << frame << " reported Corrupted value = " << Corrupted  << " is not equal to expected = " << Expected << "\n";
        }
        if(startTimeStamp && stepTimeStamp)
        {
            const mfxU64 ExpTS = startTimeStamp + stepTimeStamp*count;
            EXPECT_EQ(ExpTS,    TimeStamp) << "ERROR: Frame#" << frame << " reported TimeStamp value = " << TimeStamp  << " is not equal to expected = " << ExpTS << "\n";
        }
        count++;
        s.Data.Corrupted = 0;

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

            if(expectSpsSize)
            {
                EXPECT_EQ(expectSpsSize, spspps.SPSBufSize) << "ERROR: Returned by GetVideoParam SPS buffer size is wrong\n";
            }

            spspps.PPSBuffer  = 0;
            spspps.PPSBufSize = 0;
            TRACE_FUNC2(MFXVideoDECODE_GetVideoParam, m_session, &par);
            g_tsStatus.expect(MFX_ERR_NONE);
            g_tsStatus.check( MFXVideoDECODE_GetVideoParam(m_session, &par) );

            mfxU8 spsBufSmall[100] = {0};
            spspps.SPSBuffer = spsBufSmall;
            spspps.SPSBufSize = expectSpsSize ? (expectSpsSize - 1) : (spspps.SPSBufSize - 1);
            spspps.PPSBufSize = 0;
            spspps.PPSBuffer  = 0;

            TRACE_FUNC2(MFXVideoDECODE_GetVideoParam, m_session, &par);
            g_tsStatus.expect(MFX_ERR_NOT_ENOUGH_BUFFER);
            g_tsStatus.check( MFXVideoDECODE_GetVideoParam(m_session, &par) );

            mfxDecodeStat stat = {};

            TRACE_FUNC2(MFXVideoDECODE_GetDecodeStat, m_session, &stat);
            g_tsStatus.expect(MFX_ERR_NONE);
            g_tsStatus.check( MFXVideoDECODE_GetDecodeStat(m_session, &stat) );

            EXPECT_EQ(count, stat.NumFrame) << "ERROR: Returned stat.NumFrame by GetDecodeStat is wrong\n";
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
    {/*00*/ fDecodeFrameAsync, MFX_ERR_NONE, {6, "forBehaviorTest/corrupted/iceage_720x576_6_IPP_1I_cr.mp2", {MAJOR,REF_FRAME,REF_FRAME,0,0,0},},},
    {/*01*/ fDecodeFrameAsync, MFX_ERR_NONE, {6, "forBehaviorTest/corrupted/iceage_720x576_6_IPP_2I_cr.mp2", {0,0,0,MAJOR,REF_FRAME,REF_FRAME},},},

    //CQ from customer: VCSD100023973
    {/*02*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_001.m2v", {0},},},
    {/*03*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_002.m2v", {0},},},
    {/*04*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_003.m2v", {0},},},
    {/*05*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_004.m2v", {0},},},
    {/*06*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_005.m2v", {0},},},
    {/*07*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_006.m2v", {0},},},
    {/*08*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_007.m2v", {0},},},
    {/*09*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_008.m2v", {0},},},
    {/*10*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_009.m2v", {0},},},
    {/*11*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_010.m2v", {0},},},
    {/*12*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_011.m2v", {0},},},
    {/*13*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_012.m2v", {0},},},
    {/*14*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_013.m2v", {0},},},
    {/*15*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_014.m2v", {0},},},
    {/*16*/ fDecodeFrameAsync|noFlagCheck, MFX_ERR_NONE, {100, "forBehaviorTest/corrupted/cq23973/gvp_segfault_015.m2v", {0},},},

    {/*17*/ fDecodeFrameAsync_new, MFX_ERR_NONE, { {6, "forBehaviorTest/corrupted/iceage_720x576_6_IPP_1I_cr.mp2" , {MAJOR,REF_FRAME,REF_FRAME,0,0,0},},
                                                 {5, 0, {0}, 1},
                                                 {6, "forBehaviorTest/corrupted/iceage_720x576_6_IPP_1I_cr.mp2"  , {MAJOR,REF_FRAME,REF_FRAME,0,0,0},},
                                                 {8, 0, {0}, 1},
                                                 {6, "forBehaviorTest/corrupted/iceage_720x576_6_IPP_1I_cr.mp2" , {MAJOR,REF_FRAME,REF_FRAME,0,0,0},}
                                               }, },

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTestDecodeFrameAsync(const tc_struct& tc)
{
    const char* sname = g_tsStreamPool.Get(tc.streams[0].name);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 1920*1080*3);
    m_bs_processor = &reader;

    Init();

    SetPar4_DecodeFrameAsync();

    Verifier v(tc.streams[0].flags, tc.streams[0].n, m_par, 0, 0, m_session);
    if(tc.function & noFlagCheck)
        v.noFlagCheck = true;
    if( strstr(sname, "gvp_segfault") )
        v.expectSpsSize = 86;

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

    mfxU32 ts_start = 0; //20;
    mfxU32 ts_step  = 0; //33;
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

            Verifier v(tc.streams[i].flags, tc.streams[i].n, m_par, 0, ts_step, m_session);
            v.count = count;
            m_surf_processor = &v;

            reader.count = 0;
            m_bitstream.DataLength += m_bitstream.DataOffset;
            m_bitstream.DataOffset = 0;
            DecodeFrames(tc.streams[i].n);

            m_bs_processor = 0;
            m_surf_processor = 0;

            ts_start = ts_start + ts_step * reader.count;
            count = v.count;
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
        EXPECT_NE(0, tc.function) << "ERROR: test is not implemented\n";
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
