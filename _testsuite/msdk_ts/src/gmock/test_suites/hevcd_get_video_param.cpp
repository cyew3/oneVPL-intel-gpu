/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_parser.h"

namespace hevcd_get_video_param
{

inline BS_HEVC::NALU* GetNalu(BS_HEVC::AU& au, mfxU32 type)
{
    for(mfxU32 i = 0; i < au.NumUnits; i ++)
        if(type == au.nalu[i]->nal_unit_type)
            return au.nalu[i];
    return 0;
}

class BsRepackerHelper
{
private:
    friend class BsRepacker;

    BsRepackerHelper()
      : m_bs_int()
    { }

    mfxBitstream m_bs_int;
};

class BsRepacker : public BsRepackerHelper, public tsBitstreamReader, public tsParserHEVC
{
private:
    static const mfxU32 virtual_buffer_size = 5000;
    static const mfxU32 max_repack_buf_size = 256;
    std::string m_original_stream;
    mfxU32 m_numFrames;
public:
    UnitType* m_pAU;
    BS_HEVC::NALU m_sps;
    BS_HEVC::NALU m_pps;

    BsRepacker()
        : BsRepackerHelper()
        , tsBitstreamReader(m_bs_int, virtual_buffer_size)
        , tsParserHEVC()
        , m_original_stream("")
        , m_numFrames(0)
        , m_pAU(0)
        , m_sps()
    { }

    void Init(const char* f, mfxU32 n_frames)
    {
        BS_HEVC::NALU* p = 0;

        m_original_stream = f;
        m_numFrames = n_frames;

        if(!m_numFrames)
            return;

        open(f);

        set_trace_level(0);
        for(mfxU32 i = 0; i < m_numFrames; i ++)
        {
            m_pAU = &ParseOrDie();
            if((p = GetNalu(*m_pAU, BS_HEVC::SPS_NUT)))
                m_sps = *p;
        }

        m_bs_int.MaxLength = (mfxU32)get_offset() + max_repack_buf_size;
        m_bs_int.Data = new mfxU8[m_bs_int.MaxLength];
    }

    ~BsRepacker()
    {
        if(m_bs_int.Data)
            delete [] m_bs_int.Data;
    }

    void Repack()
    {
        tsReader r(m_original_stream.c_str());
        BS_HEVC::NALU* p = 0;
        mfxU32 part0 = 0;
        mfxU32 skip = 0;
        mfxU32 part1 = mfxU32(m_pAU->nalu[m_pAU->NumUnits-1]->StartOffset + m_pAU->nalu[m_pAU->NumUnits-1]->NumBytesInNalUnit);
        mfxU32 rsz = 0;

        if(!m_numFrames)
            return;

        for(mfxU32 i = 0; i < m_pAU->NumUnits; i ++)
        {
            if(m_pAU->nalu[i]->unit == m_sps.unit)
            {
                part0 = i;
                skip = m_pAU->nalu[i]->NumBytesInNalUnit;
                break;
            }

            if(    m_pAU->nalu[i]->nal_unit_type == BS_HEVC::PPS_NUT
                || m_pAU->nalu[i]->nal_unit_type == BS_HEVC::PREFIX_SEI_NUT
                || m_pAU->nalu[i] == m_pAU->pic->slice[0])
            {
                part0 = i;
                break;
            }
        }
        part0 = (mfxU32)m_pAU->nalu[part0]->StartOffset;
        part1 -= (part0 + skip);

        m_bs_int.DataLength += r.Read(m_bs_int.Data + m_bs_int.DataLength, part0);

        rsz = m_bs_int.MaxLength - m_bs_int.DataLength;
        BS_HEVC_Pack(m_bs_int.Data + m_bs_int.DataLength, rsz, &m_sps);
        m_bs_int.DataLength += rsz;

        r.Read(m_bs_int.Data + m_bs_int.DataLength, skip);
        m_bs_int.DataLength += r.Read(m_bs_int.Data + m_bs_int.DataLength, part1 );

        reset();
        SetBuffer(m_bs_int);
        set_trace_level(BS_HEVC::TRACE_LEVEL_SPS);

        for(mfxU32 i = 0; i < m_numFrames; i ++)
        {
            m_pAU = &ParseOrDie();
            if((p = GetNalu(*m_pAU, BS_HEVC::SPS_NUT)))
                m_sps = *p;
            if((p = GetNalu(*m_pAU, BS_HEVC::PPS_NUT)))
                m_pps = *p;
        }

        *(tsReader*)this = m_bs_int;
    }
};

class TestSuite : public tsVideoDecoder
{
public:
    TestSuite()
        : tsVideoDecoder(MFX_CODEC_HEVC)
        , m_stream()
    {
        m_bs_processor = &m_stream;
    }
    ~TestSuite() {}
    int RunTest(unsigned int id, const char* sname);
    static const unsigned int n_cases;

private:
    static const char path[];
    static const mfxU32 sp_buf_size      = 256;
    static const mfxU32 max_num_ctrl     = 3;
    static const mfxU32 max_num_ctrl_par = 8;
    mfxU8 m_sp_buf[sp_buf_size];
    BsRepacker m_stream;

    enum {
          DO_NOTHING = 0
        , CLOSE_SES
        , CLOSE_DEC
        , ZERO_PAR
        , ATTACH_EXT_BUF
        , start_REPACK
        , REPACK_CROPS_CW = start_REPACK
        , REPACK_CROPS_DW
        , REPACK_AR
        , REPACK_VSI
        , REPACK_FS
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 n_frames;
        struct tctrl{
            mfxU32 type;
            mfxU32 par[max_num_ctrl_par];
        } ctrl[max_num_ctrl];
    };
    static const tc_struct test_case[];

    void apply_par(const tc_struct& p, bool repack_only)
    {
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        {
            const tc_struct::tctrl& c = p.ctrl[i];

            if(    (repack_only && c.type < start_REPACK)
                || (!repack_only && c.type >= start_REPACK))
                continue;

            switch(c.type)
            {
            case CLOSE_SES:
                if(tsSession::m_initialized)
                {
                    Close();
                    MFXClose();
                    m_session = 0;
                }
                break;
            case CLOSE_DEC:
                if(tsVideoDecoder::m_initialized)
                    Close();
                break;
            case ZERO_PAR:
                m_pPar = 0;
                break;
            case ATTACH_EXT_BUF:
                for(mfxU32 j = 1; j <= c.par[0] * 2; j += 2)
                {
                    m_par.AddExtBuffer(c.par[j], c.par[j + 1]);

                    if(c.par[j] == MFX_EXTBUFF_CODING_OPTION_SPSPPS)
                    {
                        mfxExtCodingOptionSPSPPS& eb = m_par;
                        eb.SPSBuffer = m_sp_buf;
                        eb.SPSBufSize = eb.PPSBufSize = sp_buf_size/2;
                        eb.PPSBuffer  = eb.SPSBuffer + eb.SPSBufSize;
                    }
                }
                break;
            case REPACK_CROPS_CW:
            {
                BS_HEVC::SPS& sps = *m_stream.m_sps.sps;
                sps.conformance_window_flag = 1;
                sps.conf_win_left_offset    = c.par[0];
                sps.conf_win_right_offset   = c.par[1];
                sps.conf_win_top_offset     = c.par[2];
                sps.conf_win_bottom_offset  = c.par[3];
                break;
            }
            case REPACK_CROPS_DW:
            {
                BS_HEVC::SPS& sps = *m_stream.m_sps.sps;
                sps.vui_parameters_present_flag = 1;
                sps.vui.default_display_window_flag = 1;
                sps.vui.def_disp_win_left_offset    = c.par[0];
                sps.vui.def_disp_win_right_offset   = c.par[1];
                sps.vui.def_disp_win_top_offset     = c.par[2];
                sps.vui.def_disp_win_bottom_offset  = c.par[3];
                break;
            }
            case REPACK_AR      :
            {
                BS_HEVC::SPS& sps = *m_stream.m_sps.sps;
                sps.vui_parameters_present_flag         = 1;
                sps.vui.aspect_ratio_info_present_flag  = 1;
                sps.vui.aspect_ratio_idc                = c.par[0];
                sps.vui.sar_width                       = c.par[1];
                sps.vui.sar_height                      = c.par[2];
                break;
            }
            case REPACK_VSI     :
            {
                BS_HEVC::SPS& sps = *m_stream.m_sps.sps;
                sps.vui_parameters_present_flag         = 1;
                sps.vui.video_signal_type_present_flag  = 1;
                sps.vui.video_format                    = c.par[0];
                sps.vui.video_full_range_flag           = c.par[1];
                sps.vui.colour_description_present_flag = c.par[2];
                sps.vui.colour_primaries                = c.par[3];
                sps.vui.transfer_characteristics        = c.par[4];
                sps.vui.matrix_coeffs                   = c.par[5];
                break;
            }
            case REPACK_FS      :
            {
                BS_HEVC::SPS& sps = *m_stream.m_sps.sps;
                sps.vui_parameters_present_flag         = 1;
                sps.vui.field_seq_flag                  = c.par[0];
                break;
            }
            default: break;
            }
        }
    }
};


const char TestSuite::path[] = "conformance/hevc/itu/";

#define EXT_BUF(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, 0},
    {/* 1*/ MFX_ERR_NONE, 1},
    {/* 2*/ MFX_ERR_NULL_PTR, 0, {ZERO_PAR}},
    {/* 3*/ MFX_ERR_INVALID_HANDLE, 0, {CLOSE_SES}},
    {/* 4*/ MFX_ERR_NOT_INITIALIZED, 0, {CLOSE_DEC}},
    {/* 5*/ MFX_ERR_NONE, 1, {ATTACH_EXT_BUF, {2, EXT_BUF(mfxExtCodingOptionSPSPPS), EXT_BUF(mfxExtVideoSignalInfo)}}},
    {/* 6*/ MFX_ERR_NONE, 10, {ATTACH_EXT_BUF, {2, EXT_BUF(mfxExtCodingOptionSPSPPS), EXT_BUF(mfxExtVideoSignalInfo)}}},
    {/* 7*/ MFX_ERR_NONE, 3, {REPACK_CROPS_CW, {2, 4, 5, 3}}},
    {/* 8*/ MFX_ERR_NONE, 3, {REPACK_CROPS_DW, {5, 2, 7, 16}}},
    {/* 9*/ MFX_ERR_NONE, 3, {REPACK_CROPS_DW, {0, 0, 16, 16}}},
    {/*10*/ MFX_ERR_NONE, 3, {REPACK_CROPS_DW, {48, 48, 0, 0}}},
    {/*11*/ MFX_ERR_NONE, 3, {{REPACK_CROPS_CW, {2, 4, 5, 3}}, {REPACK_CROPS_DW, {5, 2, 7, 16}} }},
    {/*12*/ MFX_ERR_NONE, 3, {REPACK_AR, {13}}},
    {/*13*/ MFX_ERR_NONE, 3, {REPACK_AR, {255, 4, 3}}},
    {/*14*/ MFX_ERR_NONE, 3, {REPACK_AR, {255, 16, 9}}},
    {/*15*/ MFX_ERR_NONE, 3, {{REPACK_VSI, {4,1,1,3,3,3}}, {ATTACH_EXT_BUF, {1, EXT_BUF(mfxExtVideoSignalInfo)}}}},
    {/*16*/ MFX_ERR_NONE, 3, {REPACK_FS, {1}}},
};
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

const mfxU16 SubWidthC  [] = {1, 2, 2, 1, 1};
const mfxU16 SubHeightC [] = {1, 2, 1, 1, 1};
const mfxU16 ARbyIdc[][2] = {
    { 1, 1}, { 1, 1}, {12,11}, {10,11}, {16, 11}, { 40,33}, {24,11}, {20, 11},
    {32,11}, {80,33}, {18,11}, {15,11}, {64, 33}, {160,99}, { 4, 3}, { 3,  2}, { 2, 1}
};
inline mfxU16 GetARW(BS_HEVC::VUI& vui){return vui.aspect_ratio_idc == 255 ? vui.sar_width  : ARbyIdc[vui.aspect_ratio_idc][0];}
inline mfxU16 GetARH(BS_HEVC::VUI& vui){return vui.aspect_ratio_idc == 255 ? vui.sar_height : ARbyIdc[vui.aspect_ratio_idc][1];}

int TestSuite::RunTest(unsigned int id, const char* sname)
{
    TS_START;
    tc_struct tc = test_case[id];
    
    m_stream.Init(sname, TS_MAX(1, tc.n_frames));
    apply_par(tc, true);
    m_stream.Repack();

    Init();
    DecodeFrames(tc.n_frames);

    apply_par(tc, false);

    g_tsStatus.expect(tc.sts);

    if(m_pPar)
    {
        memset(m_pPar, 0xF0, sizeof(mfxVideoParam));
        m_pPar->mfx.CodecId = MFX_CODEC_HEVC;
        m_par.RefreshBuffers();
    }

    GetVideoParam(m_session, m_pPar);

    if(g_tsStatus.get() >= 0 && m_stream.m_pAU)
    {
        BS_HEVC::NALU& sps_nalu = m_stream.m_sps;
        BS_HEVC::NALU& pps_nalu = m_stream.m_pps;
        BS_HEVC::SPS&  sps = *sps_nalu.sps;
        mfxU16 cf = sps.chroma_format_idc;
        mfxFrameInfo& fi = m_par.mfx.FrameInfo;

        double FrameRate = fi.FrameRateExtD ? (fi.FrameRateExtN / fi.FrameRateExtD) : .0;
        double expectedFrameRate = sps.vui.num_units_in_tick ? (sps.vui.time_scale / sps.vui.num_units_in_tick) : 30.;

        EXPECT_EQ((mfxU16)sps.ptl.general.profile_idc,  m_par.mfx.CodecProfile);
        EXPECT_EQ((mfxU16)sps.ptl.general.level_idc/3,  m_par.mfx.CodecLevel);
        EXPECT_EQ(expectedFrameRate, FrameRate);

        mfxU16 const expected_width =
            ((sps.pic_width_in_luma_samples + 16 - 1) & ~(16 - 1));
        EXPECT_EQ(expected_width, m_par.mfx.FrameInfo.Width);

        mfxU16 const expected_height =
            ((sps.pic_height_in_luma_samples + 16 - 1) & ~(16 - 1));
        EXPECT_EQ(expected_height, m_par.mfx.FrameInfo.Height);

        EXPECT_EQ((mfxU16)sps.chroma_format_idc, m_par.mfx.FrameInfo.ChromaFormat);
        EXPECT_EQ(GetARW(sps.vui), m_par.mfx.FrameInfo.AspectRatioW);
        EXPECT_EQ(GetARH(sps.vui), m_par.mfx.FrameInfo.AspectRatioH);

        EXPECT_EQ(SubWidthC[cf] * (sps.conf_win_left_offset + sps.vui.def_disp_win_left_offset), m_par.mfx.FrameInfo.CropX);
        EXPECT_EQ(SubHeightC[cf] * (sps.conf_win_top_offset + sps.vui.def_disp_win_top_offset), m_par.mfx.FrameInfo.CropY);
        EXPECT_EQ(sps.pic_width_in_luma_samples - SubWidthC[cf] *
            (  (sps.conf_win_left_offset + sps.vui.def_disp_win_left_offset)
             + (sps.conf_win_right_offset + sps.vui.def_disp_win_right_offset))
            , m_par.mfx.FrameInfo.CropW);
        EXPECT_EQ(sps.pic_height_in_luma_samples - SubHeightC[cf] *
            (  (sps.conf_win_top_offset + sps.vui.def_disp_win_top_offset)
             + (sps.conf_win_bottom_offset + sps.vui.def_disp_win_bottom_offset))
            , m_par.mfx.FrameInfo.CropH);

        EXPECT_EQ((mfxU16)!sps.vui.field_seq_flag, m_par.mfx.FrameInfo.PicStruct);
        if (m_par.mfx.FrameInfo.ChromaFormat == 1)
        {
            if (m_par.mfx.FrameInfo.BitDepthLuma == 8)
            {
                EXPECT_EQ(MFX_FOURCC_NV12, m_par.mfx.FrameInfo.FourCC);
            }
            else if (m_par.mfx.FrameInfo.BitDepthLuma == 10)
            {
                EXPECT_EQ(MFX_FOURCC_P010, m_par.mfx.FrameInfo.FourCC);
            }
            else
            {
                ADD_FAILURE() << "FAILED: Non supported bitdepth = " << m_par.mfx.FrameInfo.BitDepthLuma << " for chroma_format = "
                    << m_par.mfx.FrameInfo.ChromaFormat;
            }
        }
        else if (m_par.mfx.FrameInfo.ChromaFormat == 2)
        {
            if (m_par.mfx.FrameInfo.BitDepthLuma == 8)
            {
                EXPECT_EQ(MFX_FOURCC_YUY2, m_par.mfx.FrameInfo.FourCC);
            }
            else if (m_par.mfx.FrameInfo.BitDepthLuma == 10)
            {
                EXPECT_EQ(MFX_FOURCC_Y216, m_par.mfx.FrameInfo.FourCC);
            }
            else
            {
                ADD_FAILURE() << "FAILED: Non supported bitdepth = " << m_par.mfx.FrameInfo.BitDepthLuma << " for chroma_format = "
                    << m_par.mfx.FrameInfo.ChromaFormat;
            }
        }
        else if (m_par.mfx.FrameInfo.ChromaFormat == 3)
        {
            if (m_par.mfx.FrameInfo.BitDepthLuma == 8)
            {
                EXPECT_EQ(MFX_FOURCC_AYUV, m_par.mfx.FrameInfo.FourCC);
            }
            else if (m_par.mfx.FrameInfo.BitDepthLuma == 10)
            {
                EXPECT_EQ(MFX_FOURCC_Y410, m_par.mfx.FrameInfo.FourCC);
            }
            else
            {
                ADD_FAILURE() << "FAILED: Non supported bitdepth = " << m_par.mfx.FrameInfo.BitDepthLuma << " for chroma_format = "
                    << m_par.mfx.FrameInfo.ChromaFormat;
            }
        }

        if((mfxExtCodingOptionSPSPPS*)m_par)
        {
            mfxExtCodingOptionSPSPPS& ecoSP = m_par;

            EXPECT_EQ(sps_nalu.NumBytesInNalUnit, ecoSP.SPSBufSize);
            EXPECT_EQ(0, memcmp(ecoSP.SPSBuffer, m_stream.Data + sps_nalu.StartOffset, ecoSP.SPSBufSize));
            EXPECT_EQ(pps_nalu.NumBytesInNalUnit, ecoSP.PPSBufSize);
            EXPECT_EQ(0, memcmp(ecoSP.PPSBuffer, m_stream.Data + pps_nalu.StartOffset, ecoSP.PPSBufSize));

            g_tsLog << "STREAM SPS: " << hexstr(m_stream.Data + sps_nalu.StartOffset, sps_nalu.NumBytesInNalUnit) << "\n";
            g_tsLog << "BUFFER SPS: " << hexstr(ecoSP.SPSBuffer, ecoSP.SPSBufSize) << "\n";
            g_tsLog << "STREAM PPS: " << hexstr(m_stream.Data + pps_nalu.StartOffset, pps_nalu.NumBytesInNalUnit) << "\n";
            g_tsLog << "BUFFER PPS: " << hexstr(ecoSP.PPSBuffer, ecoSP.PPSBufSize) << "\n";
        }

        if((mfxExtVideoSignalInfo*)m_par)
        {
            mfxExtVideoSignalInfo& vsi = m_par;

            if(sps.vui.video_signal_type_present_flag)
            {
                EXPECT_EQ((mfxU16)sps.vui.video_format, vsi.VideoFormat);
                EXPECT_EQ((mfxU16)sps.vui.video_full_range_flag, vsi.VideoFullRange);
            }
            else
            {
                EXPECT_EQ(5, vsi.VideoFormat);
                EXPECT_EQ(0, vsi.VideoFullRange);
            }

            EXPECT_EQ(sps.vui.colour_description_present_flag, vsi.ColourDescriptionPresent);

            if((mfxU16)sps.vui.colour_description_present_flag)
            {
                EXPECT_EQ((mfxU16)sps.vui.colour_primaries, vsi.ColourPrimaries);
                EXPECT_EQ((mfxU16)sps.vui.transfer_characteristics, vsi.TransferCharacteristics);
                EXPECT_EQ((mfxU16)sps.vui.matrix_coeffs, vsi.MatrixCoefficients);
            }
            else
            {
                EXPECT_EQ(2, vsi.ColourPrimaries);
                EXPECT_EQ(2, vsi.TransferCharacteristics);
                EXPECT_EQ(2, vsi.MatrixCoefficients);
            }
        }
    }

    TS_END;
    return 0;
}

template <unsigned fourcc>
char const* query_stream(unsigned int, std::integral_constant<unsigned, fourcc>);

/* 8 bit */
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_NV12>)
{ return "conformance/hevc/itu/DBLK_A_SONY_3.bit"; }
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_YUY2>)
{ return "conformance/hevc/422format/inter_422_8.bin"; }
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_AYUV>)
{ return "<TODO>"; }

/* 10 bit */
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P010>)
{ return "conformance/hevc/10bit/WP_A_MAIN10_Toshiba_3.bit"; }
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y216>)
{ return "conformance/hevc/10bit/inter_422_10.bin"; }
char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y410>)
{ return "conformance/hevc/StressBitstreamEncode/rext444_10b/Stress_HEVC_Rext444_10bHT62_432x240_30fps_302_inter_stress_2.2.hevc"; }

template <unsigned fourcc>
struct TestSuiteEx
    : public TestSuite
{
    static
    int RunTest(unsigned int id)
    {
        const char* sname =
            g_tsStreamPool.Get(query_stream(id, std::integral_constant<unsigned, fourcc>()));
        g_tsStreamPool.Reg();

        TestSuite suite;
        return suite.RunTest(id, sname);
    }
};

TS_REG_TEST_SUITE(hevcd_get_video_param,     TestSuiteEx<MFX_FOURCC_NV12>::RunTest, TestSuiteEx<MFX_FOURCC_NV12>::n_cases);
TS_REG_TEST_SUITE(hevcd_422_get_video_param, TestSuiteEx<MFX_FOURCC_YUY2>::RunTest, TestSuiteEx<MFX_FOURCC_YUY2>::n_cases);
//TS_REG_TEST_SUITE(hevcd_444_get_video_param, TestSuiteEx<MFX_FOURCC_AYUV>::RunTest, TestSuiteEx<MFX_FOURCC_AYUV>::n_cases);

TS_REG_TEST_SUITE(hevc10d_get_video_param,     TestSuiteEx<MFX_FOURCC_P010>::RunTest, TestSuiteEx<MFX_FOURCC_P010>::n_cases);
TS_REG_TEST_SUITE(hevc10d_422_get_video_param, TestSuiteEx<MFX_FOURCC_Y216>::RunTest, TestSuiteEx<MFX_FOURCC_Y216>::n_cases);
TS_REG_TEST_SUITE(hevc10d_444_get_video_param, TestSuiteEx<MFX_FOURCC_Y410>::RunTest, TestSuiteEx<MFX_FOURCC_Y410>::n_cases);

}