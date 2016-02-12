#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

#define MFX_VIDEO_EDIT_SP_BUF_SIZE  1024

namespace avcd_decode_header
{

class TestSuite : tsVideoDecoder, tsSurfaceProcessor, public BS_H264_parser
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_AVC)  { }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus FillSPExtBuf(mfxBitstream *pBs, mfxExtCodingOptionSPSPPS *pExtOpt, mfxU8 *pBuf, bool search_first)
    {
        mfxU8 *pStart = pBs->Data + pBs->DataOffset;
        mfxU8 *pEnd = pBs->Data + pBs->DataOffset + pBs->DataLength;
        bool pps_is_found = false;
        bool sps_is_found = false;
        mfxU8 *pStartSps = NULL, *pStartPps = NULL;
        mfxU32 sps_len = 0, pps_len = 0;

        while(pStart < pEnd - 4)
        {
            if ((pStart[0] == 0x00) && (pStart[1] == 0x00) && ((pStart[2] == 0x01) || ((pStart[2] == 0x00) && (pStart[3] == 0x01))))
            {
                if (sps_is_found && !sps_len)
                {
                    sps_len = pStart - pStartSps;
                    sps_is_found = false;
                }
                if (pps_is_found && !pps_len)
                {
                    pps_len = pStart - pStartPps;
                    if(search_first) break;
                }
            }
            if ((pStart[0] == 0x00) && (pStart[1] == 0x00) &&
                (pStart[2] == 0x00) && (pStart[3] == 0x01) && ((pStart[4] & 0x1F) == 0x07))
            {
                sps_is_found = true;
                pStartSps = pStart;
                sps_len = 0;
                pStart += 4;
            }
            if ((pStart[0] == 0x00) && (pStart[1] == 0x00) &&
                (pStart[2] == 0x00) && (pStart[3] == 0x01) && ((pStart[4] & 0x1F) == 0x08))
            {
                pps_is_found = true;
                pStartPps = pStart;
                pps_len = 0;
                pStart += 4;
            }

            pStart++;
        }

        if (sps_is_found && !sps_len)
        {
            sps_len = pEnd - pStartSps;
        }

        if (pps_is_found && !pps_len)
        {
            pps_len = pEnd - pStartPps;
        }

        //if (!(sps_len && pps_len))
          //  return MFX_ERR_MORE_DATA;

        //if (sps_len + pps_len > MFX_VIDEO_EDIT_SP_BUF_SIZE)
          //  return MFX_ERR_NOT_ENOUGH_BUFFER;

        /* start code should be skipped in current implementation */
        /* start code prevention bytes should be skipped in current implementation */
        if (sps_len)
        {
            if (sps_len > (MFX_VIDEO_EDIT_SP_BUF_SIZE / 2) )
                return MFX_ERR_NOT_ENOUGH_BUFFER;

            pExtOpt->SPSBuffer = pBuf;
            memcpy(pExtOpt->SPSBuffer, pStartSps, sps_len);//sps_len - 4;
            pExtOpt->SPSBufSize = sps_len;
            for (; pExtOpt->SPSBufSize && !pExtOpt->SPSBuffer[pExtOpt->SPSBufSize - 1]; pExtOpt->SPSBufSize--);
        }

        if (pps_len)
        {
            if (pps_len > (MFX_VIDEO_EDIT_SP_BUF_SIZE / 2) )
                return MFX_ERR_NOT_ENOUGH_BUFFER;

            pExtOpt->PPSBuffer = pBuf + (MFX_VIDEO_EDIT_SP_BUF_SIZE / 2);
            memcpy(pExtOpt->PPSBuffer, pStartPps, pps_len);//pps_len - 4;
            pExtOpt->PPSBufSize = pps_len;
            for (; pExtOpt->PPSBufSize && !pExtOpt->PPSBuffer[pExtOpt->PPSBufSize - 1]; pExtOpt->PPSBufSize--);
        }

        return MFX_ERR_NONE;
    }

    mfxStatus CheckVideoSignalInfo(mfxExtVideoSignalInfo* vsi,mfxBitstream *pBs, mfxVideoParam* pPar)
    {
        mfxExtVideoSignalInfo vsi_default;

        bool encoder_flag = (pPar->IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
        ||(pPar->IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY)
        ||(pPar->IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY);

        if(!pPar || !pBs) return MFX_ERR_NULL_PTR;

        if(!vsi && !encoder_flag) return MFX_ERR_INVALID_VIDEO_PARAM;

        bool vsi_found = false;
        bool new_sps = false;

        vsi_default.VideoFormat                 = 5;
        vsi_default.VideoFullRange              = 0;
        vsi_default.ColourDescriptionPresent    = 0;
        vsi_default.ColourPrimaries             = 2;
        vsi_default.TransferCharacteristics     = 2;
        vsi_default.MatrixCoefficients          = 2;

        bool sps_found = false;

        BS_H264_header* pHdr = NULL;

        if (pBs->Data)
            set_buffer(pBs->Data + pBs->DataOffset, pBs->DataLength);

        while (1)
        {
            BSErr sts = parse_next_unit();

            if (sts == BS_ERR_NOT_IMPLEMENTED) continue;
            if (sts == BS_ERR_MORE_DATA) break;

            pHdr = (BS_H264_header*)get_header();
            if(pHdr->nal_unit_type == 7) new_sps = true;
        }

        if (pHdr->SPS)
        {
            sps_found = true;

            if(pHdr->SPS->vui_parameters_present_flag ? pHdr->SPS->vui->video_signal_type_present_flag : false)
            {
                g_tsLog << "Video signal information found \n";
                vsi_found = true;
                if(!vsi)
                {
                    if(new_sps)
                        TS_FAIL_TEST("Stream contains video signal information when there is no VideoSignalInfo in mfxVideoParam \n", MFX_ERR_NONE);
                } else
                {
                    EXPECT_EQ(vsi->VideoFormat, pHdr->SPS->vui->video_format) <<
                        "ERROR: vsi->VideoFormat is not equal to pHdr->SPS->vui->video_format \n";

                    EXPECT_EQ(vsi->VideoFullRange, pHdr->SPS->vui->video_full_range_flag) <<
                        "ERROR: vsi->VideoFullRange is not equal to pHdr->SPS->vui->video_full_range_flag \n";

                    EXPECT_EQ(vsi->ColourDescriptionPresent, pHdr->SPS->vui->colour_description_present_flag) <<
                        "ERROR: vsi->ColourDescriptionPresent is not equal to pHdr->SPS->vui->colour_description_present_flag \n";

                    if(pHdr->SPS->vui->colour_description_present_flag)
                    {
                        EXPECT_EQ(vsi->ColourPrimaries, pHdr->SPS->vui->colour_primaries) <<
                            "ERROR: vsi->ColourPrimaries is not equal to pHdr->SPS->vui->colour_primaries \n";

                        EXPECT_EQ(vsi->TransferCharacteristics, pHdr->SPS->vui->transfer_characteristics) <<
                            "ERROR: vsi->TransferCharacteristics is not equal to pHdr->SPS->vui->transfer_characteristics \n";

                        EXPECT_EQ(vsi->MatrixCoefficients, pHdr->SPS->vui->matrix_coefficients) <<
                            "ERROR: vsi->MatrixCoefficients is not equal to pHdr->SPS->vui->matrix_coefficients \n";
                    }
                    else if (!encoder_flag)
                    {
                        EXPECT_EQ(vsi->ColourPrimaries, vsi_default.ColourPrimaries) <<
                            "ERROR: vsi->ColourPrimaries is not equal to default \n";

                        EXPECT_EQ(vsi->TransferCharacteristics, vsi_default.TransferCharacteristics) <<
                            "ERROR: vsi->TransferCharacteristics is not equal to default \n";

                        EXPECT_EQ(vsi->MatrixCoefficients, vsi_default.MatrixCoefficients) <<
                            "ERROR: vsi->MatrixCoefficients is not equal to default \n";
                    }
                }
            }
            else
            {
                g_tsLog << "Stream contains no video signal information";

                if(!(!vsi || encoder_flag))
                {
                    g_tsLog << "Check default values...";

                    EXPECT_EQ(vsi->VideoFormat, vsi_default.VideoFormat) <<
                        "ERROR: vsi->VideoFormat is not equal to default \n";

                    EXPECT_EQ(vsi->ColourDescriptionPresent, vsi_default.ColourDescriptionPresent) <<
                        "ERROR: vsi->ColourDescriptionPresent is not equal to default \n";

                    EXPECT_EQ(vsi->ColourPrimaries, vsi_default.ColourPrimaries) <<
                        "ERROR: vsi->ColourPrimaries is not equal to default \n";

                    EXPECT_EQ(vsi->TransferCharacteristics, vsi_default.TransferCharacteristics) <<
                        "ERROR: vsi->TransferCharacteristics is not equal to default \n";

                    EXPECT_EQ(vsi->MatrixCoefficients, vsi_default.MatrixCoefficients) <<
                        "ERROR: vsi->MatrixCoefficients is not equal to default \n";
                }
            }
        }

        if (!sps_found)
            TS_FAIL_TEST("SPS is not found \n", MFX_ERR_NONE);

        if(encoder_flag && vsi && 0 != memcmp(vsi, &vsi_default, sizeof(mfxExtVideoSignalInfo)) && false == vsi_found )
            TS_FAIL_TEST("Stream contain no video signal information when mfxExtVideoSignalInfo present and != default \n", MFX_ERR_NONE);

        return MFX_ERR_NONE;
    }

private:

    enum
    {
        NULL_PAR = 1,
        NULL_SESSION,
        NULL_BS,
        PAR_ACCEL,
        EXT_CODING_OPTION_SPSPPS,
        EXT_VSI,
        NOT_ENOUGH_BS,
        OFFSET_TOO_LARGE,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        std::string stream;
        mfxU16 in_width;
        mfxU16 in_height;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0, "forBehaviorTest/foreman_cif.h264", 352, 288},
    {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION, "forBehaviorTest/foreman_cif.h264", 352, 288},
    {/*02*/ MFX_ERR_NULL_PTR, NULL_PAR, "forBehaviorTest/foreman_cif.h264", 352, 288},
    {/*03*/ MFX_ERR_NULL_PTR, NULL_BS, "forBehaviorTest/foreman_cif.h264", 352, 288},
    {/*04*/ MFX_ERR_NONE, PAR_ACCEL, "forBehaviorTest/par_accel_avc_5K.264", 352, 288},
    {/*05*/ MFX_ERR_NONE, EXT_CODING_OPTION_SPSPPS, "forBehaviorTest/foreman_cif.h264", 352, 288},
    {/*06*/ MFX_ERR_NONE, EXT_VSI, "forBehaviorTest/foreman_cif_vsi_6f.h264", 352, 288},
    {/*07*/ MFX_ERR_NONE, EXT_VSI, "forBehaviorTest/foreman_cif.h264", 352, 288}, //no video signal information in stream
    {/*08*/ MFX_ERR_MORE_DATA, NOT_ENOUGH_BS, "forBehaviorTest/foreman_cif.h264", 352, 288},
    {/*09*/ MFX_ERR_UNDEFINED_BEHAVIOR, OFFSET_TOO_LARGE, "forBehaviorTest/par_accel_avc_5K.264", 352, 288},
    {/*10*/ MFX_ERR_NONE, PAR_ACCEL, "conformance/h264/baseline_ext/fm2_sva_c.264", 176, 144},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    const tc_struct& tc = test_case[id];

    MFXInit();
    if(m_uid)
        Load();

    const char* sname = g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 1024);
    m_bs_processor = &reader;

    tsExtBufType<mfxBitstream> bs;
    mfxBitstream* pBs = &bs;

    if(m_bs_processor)
    {
        pBs = m_bs_processor->ProcessBitstream(bs);
    }

    if (tc.mode == NOT_ENOUGH_BS)
    {
        m_pBitstream->Data = new mfxU8[1024];
        memcpy(m_pBitstream->Data, pBs->Data, 1024);
        memset(m_pBitstream->Data, 0, m_pBitstream->DataLength);
    }
    else if (tc.mode == OFFSET_TOO_LARGE)
    {
        m_pBitstream->Data = new mfxU8[1024];
        memcpy(m_pBitstream->Data, pBs->Data, 1024);
        memset(m_pBitstream->Data, 0, m_pBitstream->DataLength);
        m_pBitstream->DataOffset = m_pBitstream->DataLength+1;
    }
    else
    {   /* 10 zero bytes ahead of file data */
        m_pBitstream->Data = new mfxU8[1024+10];

        memset(m_pBitstream->Data, 0xFF, 10);
        memcpy(&(m_pBitstream->Data[10]), pBs->Data, 1024);

        m_pBitstream->DataOffset = 0;
        m_pBitstream->DataLength = 1024+10;
        m_pBitstream->MaxLength = 1024+10;
    }

    mfxSession m_session_tmp = m_session;
    mfxU16 NumExtParam = 0;
    mfxExtCodingOptionSPSPPS ref_spspps = {0,};


    if (tc.mode == NULL_PAR)
        m_pPar = 0;
    else if (tc.mode == NULL_SESSION)
        m_session = 0;
    else if (tc.mode == NULL_BS)
        m_pBitstream = 0;
    else
    {
        // set values that should not be changed in DecodeHeader
        m_par.Protected = m_par.IOPattern = m_par.mfx.NumThread = m_par.AsyncDepth = 0xAA;
        if (m_par.NumExtParam == 0) m_par.NumExtParam = 0xAA;

        if (tc.mode == EXT_CODING_OPTION_SPSPPS)
        {
            m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION_SPSPPS, sizeof(mfxExtCodingOptionSPSPPS));
            mfxExtCodingOptionSPSPPS* spspps = (mfxExtCodingOptionSPSPPS*)m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION_SPSPPS);

            mfxU8 spbuf[1024];

            spspps->SPSBuffer = spbuf;
            spspps->SPSBufSize = (MFX_VIDEO_EDIT_SP_BUF_SIZE>>1);

            spspps->PPSBuffer = spbuf+(MFX_VIDEO_EDIT_SP_BUF_SIZE>>1);
            spspps->PPSBufSize = (MFX_VIDEO_EDIT_SP_BUF_SIZE>>1);

            mfxU8 ref_spbuf[1024];

            FillSPExtBuf(m_pBitstream, &ref_spspps, ref_spbuf, true);
        }
        else if (tc.mode == EXT_VSI)
            m_par.AddExtBuffer(MFX_EXTBUFF_VIDEO_SIGNAL_INFO, sizeof(mfxExtVideoSignalInfo));

        NumExtParam = m_par.NumExtParam;
    }

    g_tsStatus.expect(tc.sts);
    mfxStatus sts = DecodeHeader(m_session, m_pBitstream, m_pPar);


    if (tc.mode == PAR_ACCEL)
    {
        if (g_tsImpl == MFX_IMPL_SOFTWARE)
        {
            g_tsStatus.expect(MFX_ERR_NONE);
            g_tsLog << "Expected status changed to MFX_ERR_NONE \n";
        }
    }
    else if (sts == MFX_ERR_NONE)
    {
        EXPECT_EQ(MFX_FOURCC_NV12, m_pPar->mfx.FrameInfo.FourCC) <<
            "ERROR: pPar->mfx.FrameInfo.FourCC is not equal to MFX_FOURCC_NV12 as expected \n";
        if ((m_pPar->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) &&
                (m_pPar->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN))
            TS_FAIL_TEST("m_pPar->mfx.FrameInfo.PicStruct is not equal to\n\
                               either MFX_PICSTRUCT_PROGRESSIVE or MFX_PICSTRUCT_UNKNOWN!!!\n", MFX_ERR_NONE);

        EXPECT_EQ(0xAA, m_pPar->Protected) <<
            "ERROR: m_pPar->Protected should not be changed in DecodeHeader \n";

        EXPECT_EQ(0xAA, m_pPar->IOPattern) <<
            "ERROR: m_pPar->IOPattern should not be changed in DecodeHeader \n";

        EXPECT_EQ(0xAA, m_pPar->mfx.NumThread) <<
            "ERROR: m_pPar->mfx.NumThread should not be changed in DecodeHeader \n";

        EXPECT_EQ(0xAA, m_pPar->AsyncDepth) <<
            "ERROR: m_pPar->AsyncDepth should not be changed in DecodeHeader \n";

        EXPECT_EQ(0xAA, m_pPar->mfx.NumThread) <<
            "ERROR: m_pPar->Protected should not be changed in DecodeHeader \n";

        EXPECT_EQ(NumExtParam, m_pPar->NumExtParam) <<
            "ERROR: m_pPar->NumExtParam should not be changed in DecodeHeader \n";

        EXPECT_EQ(tc.in_width, m_pPar->mfx.FrameInfo.Width) <<
            "ERROR: Width after DecodeHeader is not the same as in stream \n";

        EXPECT_EQ(tc.in_height, m_pPar->mfx.FrameInfo.Height) <<
            "ERROR: Height after DecodeHeader is not the same as in stream \n";

        // DataOffset check
        {
            mfxBitstream *pbs = &m_bitstream;
            mfxU8 *pd = pbs->Data + pbs->DataOffset;

            if ( !((pd[0] == 0x00) && (pd[1] == 0x00) && (pd[2] == 0x00) && (pd[3] == 0x01) && ((pd[4] & 0x1f) == 0x7)) &&
                !((pd[0] == 0x00) && (pd[1] == 0x00) && (pd[2] == 0x00) && (pd[3] == 0x01) && ((pd[4] & 0x1f) == 0x9)) )
            {
                mfxU32 i = 0;

                g_tsLog << "Initial BS kept 10 leading 0xFF bytes before video data \n";
                g_tsLog << "pbs->MaxLength = " << pbs->MaxLength << " \n";
                g_tsLog << "The output DataOffset = " << pbs->DataOffset << " \n\n";

                for (; i < pbs->DataOffset; i++)
                {
                    g_tsLog << "bs->Data[" << i << "] = 0x";
                    g_tsLog << (int)pbs->Data[i] << " \n";
                }

                for (; i < pbs->DataOffset + 10; i++)
                {
                    g_tsLog << "***bs->Data[" << i << "] = 0x";
                    g_tsLog << (int)pbs->Data[i] << " \n";
                }

                g_tsLog << "***bs->Data[" << i << "] = 0x";
                g_tsLog << (int)pbs->Data[i] << " \n";

                TS_FAIL_TEST("Bitstream does not point to SPS start code \n", MFX_ERR_NONE);
            }

            slice_header* f = NULL;
            set_buffer(m_pBitstream->Data+m_pBitstream->DataOffset, m_pBitstream->DataLength);
            BSErr bs_sts = BS_ERR_NONE;
            BS_H264_header* hdr = NULL;

            while(1)
            {
                bs_sts = parse_next_unit();
                if(bs_sts == BS_ERR_NOT_IMPLEMENTED) continue;
                if(bs_sts == BS_ERR_MORE_DATA) break;
                hdr = (BS_H264_header*)get_header();
                if((hdr->nal_unit_type == 1 || hdr->nal_unit_type == 5 || hdr->nal_unit_type == 20)
                    && hdr->slice_hdr->first_mb_in_slice == 0)
                {
                    f = hdr->slice_hdr;
                    break;
                }
            }

            EXPECT_EQ(!!m_pPar->mfx.SliceGroupsPresent, !!f->pps_active->num_slice_groups_minus1) <<
                "ERROR: SliceGroupsPresent flag is wrong \n";

        }

        // Check ext buffers
        mfxExtCodingOptionSPSPPS* spspps = (mfxExtCodingOptionSPSPPS*)m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION_SPSPPS);
        if (spspps != 0)
        {
            EXPECT_EQ(ref_spspps.SPSBufSize, spspps->SPSBufSize) <<
                "ERROR: SPSBufSize is wrong \n";

            EXPECT_EQ(0, memcmp(spspps->SPSBuffer, ref_spspps.SPSBuffer, ref_spspps.SPSBufSize) ) <<
                "ERROR: DecodeHeader didn't copy SPS from stream to mfxExtCodingOptionSPSPPS \n";

            EXPECT_EQ(ref_spspps.PPSBufSize, spspps->PPSBufSize) <<
                "ERROR: PPSBufSize is wrong \n";

            EXPECT_EQ(0, memcmp(spspps->PPSBuffer, ref_spspps.PPSBuffer, ref_spspps.PPSBufSize) ) <<
                "ERROR: DecodeHeader didn't copy PPS from stream to mfxExtCodingOptionSPSPPS \n";
        }

        mfxExtVideoSignalInfo* vsi = (mfxExtVideoSignalInfo*)m_par.GetExtBuffer(MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
        if (vsi != 0)
        {
            g_tsStatus.expect(MFX_ERR_NONE);
            mfxStatus vsi_sts = CheckVideoSignalInfo(vsi, &m_bitstream, m_pPar);
            g_tsStatus.check(vsi_sts);
        }
    }

    if (tc.mode == NULL_SESSION)
        m_session = m_session_tmp;

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avcd_decode_header);

}

// Legacy test: behavior_h264_decode_suite, b_AVC_DECODE_DecodeHeader