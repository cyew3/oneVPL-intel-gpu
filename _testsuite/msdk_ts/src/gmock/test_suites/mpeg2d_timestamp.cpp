#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_bitstream.h"
#include "ts_parser.h"
#include <vector>

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace mpeg2d_timestamp
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_MPEG2) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 10;

    enum
    {
        READ_BY_FRAME  = 0x001,
        COMPLETE_FRAME = 0x010,
        ALWAYS_READ    = 0x100
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair 
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] = 
{
    // READ_BY_FRAME: read whole frame and only one at a time. Do not not overwrite data until DataLength = 0

    // ALWAYS_READ: read frames for all statuses from DecodeFrameAsync except ERR_MORE_SURFACE
    {/*00*/ MFX_ERR_NONE, TestSuite::ALWAYS_READ},
    {/*00*/ MFX_ERR_NONE, TestSuite::ALWAYS_READ|TestSuite::COMPLETE_FRAME},
    {/*00*/ MFX_ERR_NONE, TestSuite::ALWAYS_READ|TestSuite::COMPLETE_FRAME|TestSuite::READ_BY_FRAME},

    // common pipeline
    {/*00*/ MFX_ERR_NONE, TestSuite::COMPLETE_FRAME},
    {/*00*/ MFX_ERR_NONE, TestSuite::COMPLETE_FRAME|TestSuite::READ_BY_FRAME},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);


class FrameReader : public tsBitstreamProcessor, public tsReader, public tsParserMPEG2
{
public:
    bool    m_eos;
    bool    m_first_pic_header;
    bool    m_read_by_frame;
    bool    m_complete_frame;
    mfxU8*  m_buf;
    mfxU32  m_buf_size;
    mfxU64  offset;
    int     in_pts;

    FrameReader(const char* fname, bool read_by_frame, bool complete_frame)
        : tsParserMPEG2(BS_MPEG2_INIT_MODE_MB)
        , tsReader(fname)
        , offset(0)
        , m_buf_size(1024*1024)
        , m_first_pic_header(false)
        , m_eos(false)
        , m_read_by_frame(read_by_frame)
        , m_complete_frame(complete_frame)
        , in_pts(1)
    {
        tsParserMPEG2::open(fname);
        tsParserMPEG2::set_trace_level(0/*BS_MPEG2_TRACE_LEVEL_START_CODE*/);
        m_buf = new mfxU8[m_buf_size];
        m_buf[0] = 0;
    };
    ~FrameReader() { delete[] m_buf; }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if(m_eos)
        {
            return MFX_ERR_MORE_DATA;
        }

        UnitType* hdr = 0;

        mfxU32 size = 0;
        if (m_read_by_frame)
        {
            if (bs.DataLength != 0)  // do not overwrite existing data
            {
                return MFX_ERR_NONE;
            }

            BSErr err = BS_ERR_NONE;
            do {
                mfxU64 curr = get_offset();
                err = parse_next_unit();

                hdr = (UnitType*)get_header();
                if ((BS_ERR_MORE_DATA == err) ||
                    (hdr && hdr->start_code == 0x00000100)) // picture header
                {
                    if (!m_first_pic_header)
                    {
                        m_first_pic_header = true;
                        continue;
                    }
                    size = (mfxU32)(curr - offset)+1;
                    offset += size;
                    if (err == BS_ERR_MORE_DATA)
                    {
                        hdr = 0;
                    }
                    break;
                }
            } while(hdr);

            bs.MaxLength = m_buf_size;
            bs.Data      = m_buf;
            bs.DataLength = 1;
            bs.DataOffset = 0;

            bs.DataLength += Read(bs.Data + 1, size);

            m_eos = !hdr;
        }
        else
        {
            if(bs.DataLength + bs.DataOffset > m_buf_size)
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }

            bs.Data      = m_buf;
            bs.MaxLength = m_buf_size;

            if(bs.DataLength && bs.DataOffset)
            {
                memmove(bs.Data, bs.Data + bs.DataOffset, bs.DataLength);
            }
            bs.DataOffset = 0;

            bs.DataLength += Read(bs.Data + bs.DataLength, bs.MaxLength - bs.DataLength);

            m_eos = bs.DataLength != bs.MaxLength;
        }
        bs.TimeStamp = in_pts++;
        if (m_complete_frame)
        {
            bs.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
        }

        return MFX_ERR_NONE;
    }
};

class spCheckTimeStamp : public tsSurfaceProcessor
{
public:
    std::vector<mfxU32> expected_timestamps;
    spCheckTimeStamp()
    {
        mfxU32 expected[] = {1,3,4,5,6,2,8,9,10,11,7,13,14,15,16,12};
        expected_timestamps = std::vector<mfxU32>(expected, expected + sizeof(expected) / sizeof(expected[0]));
    }
    ~spCheckTimeStamp() {}

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (expected_timestamps.empty())
        {
            g_tsLog << "ERROR: expected_timestamps is empty but there is more surface\n";
            EXPECT_FALSE (expected_timestamps.empty());
            return MFX_ERR_NONE;
        }
        
        if (expected_timestamps[0] != s.Data.TimeStamp)
        {
            g_tsLog << "ERROR: frame timestamp=" << s.Data.TimeStamp <<
                       " is not equal to requested=" << expected_timestamps[0] << "\n";
        }
        EXPECT_EQ(expected_timestamps[0], s.Data.TimeStamp);
        expected_timestamps.erase(expected_timestamps.begin());
        return MFX_ERR_NONE;
    }
};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    m_par_set = true;
    const tc_struct& tc = test_case[id];

    mfxU32 nframes = 16;  // nframes in bit_stream2.m2v == 16
    FrameReader reader(g_tsStreamPool.Get("conformance/mpeg2/streams/bit_stream2.m2v"),
                       !!(tc.mode & READ_BY_FRAME),
                       !!(tc.mode & COMPLETE_FRAME));
    g_tsStreamPool.Reg();
    m_bs_processor = &reader;

    spCheckTimeStamp sp;
    m_surf_processor = &sp;
    //Uncomment for debug
    //tsSurfaceWriter writer("output.yuv");
    //m_surf_processor = &writer;

    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
    }
    m_par_set = false;

    if (tc.mode & ALWAYS_READ)
    {
        m_par.AsyncDepth = 1;
        Init();

        mfxU32 decoded = 0;
        mfxU32 submitted = 0;
        mfxU32 async = TS_MAX(1, m_par.AsyncDepth);
        mfxStatus res = MFX_ERR_NONE;

        async = TS_MIN(nframes, async - 1);

        SetPar4_DecodeFrameAsync();

        while(decoded < nframes)
        {
            res = DecodeFrameAsync(m_session, m_pBitstream, m_pSurf = GetSurface(), &m_pSurfOut, m_pSyncPoint);

            if(g_tsStatus.get() == 0)
            {
                m_surf_out.insert( std::make_pair(*m_pSyncPoint, m_pSurfOut) );
                if(m_pSurfOut)
                {
                    m_pSurfOut->Data.Locked ++;
                }
            }

            // read next frame on each iteration
            if (res != MFX_ERR_MORE_SURFACE && m_pBitstream)
            {
                reader.ProcessBitstream(*m_pBitstream, 1);
            }

            if(MFX_ERR_MORE_SURFACE == res || MFX_ERR_MORE_DATA == res || res > 0)
            {
                if (reader.m_eos && MFX_ERR_MORE_DATA == res)
                {
                    for (mfxU16 i = 0; i < m_par.AsyncDepth + 1; i++)
                    {
                        res = DecodeFrameAsync(m_session, 0, m_pSurf = GetSurface(), &m_pSurfOut, m_pSyncPoint);
                        if (MFX_ERR_NONE == res)
                        {
                            submitted++;
                            m_surf_out.insert( std::make_pair(*m_pSyncPoint, m_pSurfOut) );
                            if(m_pSurfOut)
                            {
                                m_pSurfOut->Data.Locked ++;
                            }
                        }
                    }
                    while(m_surf_out.size()) SyncOperation();
                    decoded += submitted;
                    break;
                }
                continue;
            }

            if(res < 0) g_tsStatus.check();

            if(++submitted >= async)
            {
                while(m_surf_out.size()) SyncOperation();
                decoded += submitted;
                submitted = 0;
                async = TS_MIN(async, (nframes - decoded));
            }
        }

        g_tsLog << decoded << " FRAMES DECODED\nframes";
    }
    else
    {
        DecodeFrames(nframes);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(mpeg2d_timestamp);

}
