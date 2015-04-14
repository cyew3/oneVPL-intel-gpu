#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace mpeg2e_headers_order
{
class TestSuite : public tsVideoEncoder, public tsBitstreamProcessor, public BS_MPEG2_parser
{
private:
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_MPEG2)
        , BS_MPEG2_parser()
        , mode(0)
    {
        srand(0);

        m_bs_processor = this;
    }

    ~TestSuite() {}

    enum
    {
        VSI_BUF = 1,
        VSI_BUF_ON      = 1 << 1,
        VSI_BUF_OFF     = 1 << 2
    };

    enum
    {
        MFXPAR = 1
    };

    mfxU32 mode;

    static const unsigned int n_cases;
    int RunTest(unsigned int id);
    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };
    static const tc_struct test_case[];

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxExtVideoSignalInfo* vsi = (mfxExtVideoSignalInfo*)m_par.GetExtBuffer(MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
        if (mode == VSI_BUF && vsi == 0)
        {
            g_tsLog << "ERROR: Mode is VSI_BUF. But buffer is empty."<< "\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }

        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

        while (1)
        {
            BSErr sts = parse_next_unit();

            if (sts == BS_ERR_NOT_IMPLEMENTED) continue;
            if (sts) break;

            BS_MPEG2_header* pHdr = (BS_MPEG2_header*)get_header();

            // expected orders: seq_hdr, seq_ext, seq_display_ext (for VSI_BUF mode), pic_hdr, pic_coding_ext
            if (pHdr->seq_hdr != 0)
            {
                if (pHdr->seq_ext != 0)
                {
                    if (mode == VSI_BUF)
                    {
                        if (pHdr->seq_displ_ext != 0)
                        {
                            if (pHdr->pic_hdr == 0 && pHdr->pic_coding_ext != 0)
                            {
                                TS_FAIL_TEST("Failed. Expected header: pic_hdr. Current is pic_coding_ext.", MFX_ERR_ABORTED)
                            }

                            // check VideoFormat, ColourDescriptionPresent, ColourPrimaries,
                            // TransferCharacteristics, MatrixCoefficients
                            EXPECT_EQ(pHdr->seq_displ_ext->video_format , vsi->VideoFormat)
                                      << "ERROR: Video format in stream != video format in ExtBuffer";TS_CHECK_MFX;
                            EXPECT_EQ(pHdr->seq_displ_ext->colour_description , vsi->ColourDescriptionPresent)
                                      << "ERROR: Colour description in stream != colour description in ExtBuffer";TS_CHECK_MFX;
                            EXPECT_EQ(pHdr->seq_displ_ext->colour_primaries , vsi->ColourPrimaries)
                                      << "ERROR: Colour primaries in stream != colour primaries in ExtBuffer";TS_CHECK_MFX;
                            EXPECT_EQ(pHdr->seq_displ_ext->transfer_characteristics , vsi->TransferCharacteristics)
                                      << "ERROR: Transfer characteristics in stream != transfer characteristics in ExtBuffer";TS_CHECK_MFX;
                            EXPECT_EQ(pHdr->seq_displ_ext->matrix_coefficients , vsi->MatrixCoefficients)
                                      << "ERROR: Matrix coefficients in stream != matrix coefficients in ExtBuffer";
                        }
                        else if (pHdr->pic_hdr != 0 || pHdr->pic_coding_ext != 0)
                        {
                            if (pHdr->pic_hdr != 0)
                            {
                                TS_FAIL_TEST("Failed. Expected header: seq_display_ext. Current is pic_hdr.", MFX_ERR_ABORTED)
                            }

                            if (pHdr->pic_coding_ext != 0)
                            {
                                TS_FAIL_TEST("Failed. Expected header: seq_display_ext. Current is pic_coding_ext.", MFX_ERR_ABORTED)
                            }
                        }
                    }
                    else
                    {
                        if (pHdr->seq_displ_ext != 0)
                        {
                            TS_FAIL_TEST("Failed. Expected header: pic_hdr. Current is seq_display_ext. Mode isn't VSI_BUF.", MFX_ERR_ABORTED)
                        }
                        if (pHdr->pic_hdr == 0 && pHdr->pic_coding_ext != 0)
                        {
                            TS_FAIL_TEST("Failed. Expected header: pic_hdr. Current is pic_coding_ext.", MFX_ERR_ABORTED)
                        }
                    }
                }
                else if (pHdr->seq_displ_ext != 0 || pHdr->pic_hdr != 0 || pHdr->pic_coding_ext != 0)
                {
                    if (pHdr->seq_displ_ext != 0)
                    {
                        if (mode == VSI_BUF)
                        {
                            TS_FAIL_TEST("Failed. Expected header: seq_ext. Current is seq_display_ext.", MFX_ERR_ABORTED)
                        }
                        else
                        {
                            TS_FAIL_TEST("Failed. Expected header: seq_ext. Current is seq_display_ext. Mode isn't VSI_BUF.", MFX_ERR_ABORTED)
                        }
                    }
                    if (pHdr->pic_hdr != 0)
                    {
                        TS_FAIL_TEST("Failed. Expected header: seq_ext. Current is pic_hdr.", MFX_ERR_ABORTED)
                    }
                    if (pHdr->pic_coding_ext != 0)
                    {
                        TS_FAIL_TEST("Failed. Expected header: seq_ext. Current is pic_coding_ext.", MFX_ERR_ABORTED)
                    }
                }
            }
            else if (pHdr->seq_ext != 0 || pHdr->seq_displ_ext != 0 || pHdr->pic_hdr != 0 || pHdr->pic_coding_ext != 0)
            {
                if (pHdr->seq_ext != 0)
                {
                    TS_FAIL_TEST("Failed. Expected header: seq_hdr. Current is seq_ext.", MFX_ERR_ABORTED)
                }
                if (pHdr->seq_displ_ext != 0)
                {
                    if (mode == VSI_BUF)
                    {
                        TS_FAIL_TEST("Failed. Expected header: seq_hdr. Current is seq_display_ext.", MFX_ERR_ABORTED)
                    }
                    else
                    {
                        TS_FAIL_TEST("Failed. Expected header: seq_hdr. Current is seq_display_ext. Mode isn't VSI_BUF.", MFX_ERR_ABORTED)
                    }
                }
                if (pHdr->pic_hdr != 0)
                {
                    TS_FAIL_TEST("Failed. Expected header: seq_hdr. Current is pic_hdr.", MFX_ERR_ABORTED)
                }
                if (pHdr->pic_coding_ext != 0)
                {
                    TS_FAIL_TEST("Failed. Expected header: seq_hdr. Current is pic_coding_ext.", MFX_ERR_ABORTED)
                }
            }
        }
        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }
};
#define mfx_PicStruct tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct
#define mfx_Async tsStruct::mfxVideoParam.AsyncDepth
const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0, { MFXPAR, &mfx_Async, 1}},
    {/*01*/ MFX_ERR_NONE, 0, {{ MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF }, { MFXPAR, &mfx_Async, 1}} },
    {/*02*/ MFX_ERR_NONE, 0, {{ MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF }, { MFXPAR, &mfx_Async, 1}} },
    {/*03*/ MFX_ERR_NONE, VSI_BUF , { MFXPAR, &mfx_Async, 1}},
    {/*04*/ MFX_ERR_NONE, VSI_BUF, {{ MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF }, { MFXPAR, &mfx_Async, 1}} },
    {/*05*/ MFX_ERR_NONE, VSI_BUF, {{ MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF },{ MFXPAR, &mfx_Async, 1}} },

    {/*06*/ MFX_ERR_NONE, 0, { MFXPAR, &mfx_Async, 5}},
    {/*07*/ MFX_ERR_NONE, 0, {{ MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF }, { MFXPAR, &mfx_Async, 5}} },
    {/*08*/ MFX_ERR_NONE, 0, {{ MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF }, { MFXPAR, &mfx_Async, 5}} },
    {/*09*/ MFX_ERR_NONE, VSI_BUF , { MFXPAR, &mfx_Async, 5}},
    {/*10*/ MFX_ERR_NONE, VSI_BUF, {{ MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF }, { MFXPAR, &mfx_Async, 5}} },
    {/*11*/ MFX_ERR_NONE, VSI_BUF, {{ MFXPAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF },{ MFXPAR, &mfx_Async, 5}} },

    {/*12*/ MFX_ERR_NONE, VSI_BUF_ON, { MFXPAR, &mfx_Async, 5}},
    {/*13*/ MFX_ERR_NONE, VSI_BUF_OFF, { MFXPAR, &mfx_Async, 5}},
};
#undef mfx_PicStruct
#undef mfx_Async

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];

    MFXInit();

    m_filler = new tsNoiseFiller;
    AllocBitstream(1024*1024*1024);

    m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    SETPARS(m_pPar, MFXPAR);

    // in MPEG2 only interlace frame supported in HW
    if (m_par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
    {
        mfxExtCodingOption& co = m_par;
        co.FramePicture = MFX_CODINGOPTION_ON;
    }

    if (tc.mode == VSI_BUF || tc.mode == VSI_BUF_ON )
    {
        mode = 1;
        mfxExtVideoSignalInfo& vsi = m_par;
        vsi.VideoFormat = 5;
        vsi.VideoFullRange = 0;
        vsi.ColourDescriptionPresent = 1;
        vsi.ColourPrimaries = 4;
        vsi.TransferCharacteristics = 4;
        vsi.MatrixCoefficients = 4;
    }

    Init();

    g_tsStatus.expect(tc.sts);

    mfxU32 nf = 3;
    EncodeFrames(nf);

    if (tc.mode == VSI_BUF_ON || tc.mode == VSI_BUF_OFF)
    {
        reset();    // reset parser
        if (tc.mode == VSI_BUF_ON)
        {
            m_par.NumExtParam = 0;
            m_par.ExtParam = 0;
            mode = 0;
        }
        else
        {
            mode = 1;
            mfxExtVideoSignalInfo& vsi = m_par;
            vsi.VideoFormat = 5;
            vsi.VideoFullRange = 0;
            vsi.ColourDescriptionPresent = 1;
            vsi.ColourPrimaries = 1;
            vsi.TransferCharacteristics = 1;
            vsi.MatrixCoefficients = 1;
        }
        Reset();
        EncodeFrames(nf);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(mpeg2e_headers_order);
};
