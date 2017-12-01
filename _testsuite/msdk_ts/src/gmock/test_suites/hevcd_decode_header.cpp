/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevcd_decode_header
{

class TestSuite : public tsVideoDecoder, public tsParserHEVC
{
public:
    TestSuite()
        : tsVideoDecoder(MFX_CODEC_HEVC)
        , pAU(0)
    {
        set_trace_level(0);
        m_bitstream.Data = m_bs_buf;
        m_bitstream.MaxLength = max_bs_size;
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const char path[];
    static const mfxU32 repack_buf_size  = 256;
    static const mfxU32 max_bs_size      = 1000;
    static const mfxU32 max_num_ctrl     = 3;
    static const mfxU32 max_num_ctrl_par = 8;
    UnitType* pAU;
    mfxU8 m_bs_buf[max_bs_size];
    mfxU8 m_sp_buf[repack_buf_size];

    enum {
          DO_NOTHING = 0
        , CLOSE
        , CHANGE_PAR
        , CHANGE_BS
        , ATTACH_EXT_BUF
        , REPACK_CROPS_CW
        , REPACK_CROPS_DW
        , REPACK_AR
        , REPACK_VSI
        , REPACK_FS
    };

    struct tc_struct
    {
        mfxStatus sts;
        std::string stream;
        struct tctrl{
            mfxU32 type;
            const tsStruct::Field* field;
            mfxU32 par[max_num_ctrl_par];
        } ctrl[max_num_ctrl];
    };
    static const tc_struct test_case[];

    void repackNALU     (BS_HEVC::NALU& nalu);
    void repackCWCrops  (mfxU32 l, mfxU32 r, mfxU32 t, mfxU32 b);
    void repackDWCrops  (mfxU32 l, mfxU32 r, mfxU32 t, mfxU32 b);
    void repackAR       (mfxU32 id, mfxU32  sw, mfxU32 sh);
    void repackVSI      (mfxU32 vf, mfxU32 vfr, mfxU32 cdpf, mfxU32 cp, mfxU32 tc, mfxU32 mc);
    void repackFieldSeq (mfxU32 fs);

    void apply_par(const tc_struct& p)
    {
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        {
            const tc_struct::tctrl& c = p.ctrl[i];

            switch(c.type)
            {
            case CLOSE:
                MFXClose(); m_session = 0;
                break;
            case CHANGE_PAR:
                if(c.field) tsStruct::set(m_pPar, *c.field, c.par[0]);
                else        m_pPar = 0;
                break;
            case CHANGE_BS:
                if(c.field) tsStruct::set(m_pBitstream, *c.field, c.par[0]);
                else        m_pBitstream = 0;
                break;
            case ATTACH_EXT_BUF:
                for(mfxU32 j = 1; j <= c.par[0] * 2; j += 2)
                {
                    m_par.AddExtBuffer(c.par[j], c.par[j + 1]);
                }
                break;
            case REPACK_CROPS_CW:
                repackCWCrops(c.par[0], c.par[1], c.par[2], c.par[3]);
                break;
            case REPACK_CROPS_DW:
                repackDWCrops(c.par[0], c.par[1], c.par[2], c.par[3]);
                break;
            case REPACK_AR:
                repackAR(c.par[0], c.par[1], c.par[2]);
                break;
            case REPACK_VSI:
                repackVSI(c.par[0], c.par[1], c.par[2], c.par[3], c.par[4], c.par[5]);
                break;
            case REPACK_FS:
                repackFieldSeq(c.par[0]);
                break;
            default: break;
            }
        }
    }
};

const mfxU16 SubWidthC  [] = {1, 2, 2, 1, 1};
const mfxU16 SubHeightC [] = {1, 2, 1, 1, 1};
const mfxU16 ARbyIdc[][2] = {
    { 0, 0}, { 1, 1}, {12,11}, {10,11}, {16, 11}, { 40,33}, {24,11}, {20, 11},
    {32,11}, {80,33}, {18,11}, {15,11}, {64, 33}, {160,99}, { 4, 3}, { 3,  2}, { 2, 1}
};
inline mfxU16 GetARW(BS_HEVC::VUI& vui){return vui.aspect_ratio_idc == 255 ? vui.sar_width  : ARbyIdc[vui.aspect_ratio_idc][0];}
inline mfxU16 GetARH(BS_HEVC::VUI& vui){return vui.aspect_ratio_idc == 255 ? vui.sar_height : ARbyIdc[vui.aspect_ratio_idc][1];}


inline BS_HEVC::NALU& GetNalu(BS_HEVC::AU& au, mfxU32 type)
{
    for(mfxU32 i = 0; i < au.NumUnits; i ++)
        if(type == au.nalu[i]->nal_unit_type)
            return *au.nalu[i];
    g_tsStatus.check(MFX_ERR_NOT_FOUND);
    return *au.nalu[0];
}

void RepackBS(mfxBitstream& bs, mfxU32 at, mfxU8* what, mfxU32 size, mfxI32 overlap)
{
    mfxU8* insert_to  = bs.Data + bs.DataOffset + at;
    mfxU8* move_from  = insert_to + size - overlap;
    mfxU8* move_to    = insert_to + size;
    mfxU32 move_sz    = bs.DataLength - (at + size - overlap);
    mfxU32 available  = bs.MaxLength - (bs.DataOffset + at + size);

    move_sz = TS_MIN(move_sz, available);

    memmove(move_to, move_from, move_sz);
    memmove(insert_to, what, size);

    bs.DataLength = at + size + move_sz;
}

void TestSuite::repackNALU(BS_HEVC::NALU& nalu)
{
    mfxU8 buf[repack_buf_size] = {};
    mfxU32 size = repack_buf_size;
    BS_HEVC::NALU& replace_nalu = GetNalu(*pAU, nalu.nal_unit_type);
    mfxU32 old_sz = nalu.NumBytesInNalUnit;


    if(BS_HEVC_Pack(buf, size, &nalu))
    {
        g_tsStatus.check(MFX_ERR_UNKNOWN);
        return;
    }

    TRACE_FUNC5(RepackBS, m_bitstream, (Bs32u)replace_nalu.StartOffset, buf, nalu.NumBytesInNalUnit, size - old_sz);
    RepackBS(m_bitstream, (Bs32u)replace_nalu.StartOffset, buf, nalu.NumBytesInNalUnit, size - old_sz);
}

void TestSuite::repackCWCrops(mfxU32 l, mfxU32 r, mfxU32 t, mfxU32 b)
{
    BS_HEVC::NALU& nalu = GetNalu(*pAU, BS_HEVC::NALU_TYPE::SPS_NUT);
    BS_HEVC::SPS& sps = *nalu.sps;

    sps.conformance_window_flag = 1;
    sps.conf_win_left_offset    = l;
    sps.conf_win_right_offset   = r;
    sps.conf_win_top_offset     = t;
    sps.conf_win_bottom_offset  = b;

    sps.vui.hrd_parameters_present_flag = 0;

    repackNALU(nalu);
}

void TestSuite::repackDWCrops(mfxU32 l, mfxU32 r, mfxU32 t, mfxU32 b)
{
    BS_HEVC::NALU& nalu = GetNalu(*pAU, BS_HEVC::NALU_TYPE::SPS_NUT);
    BS_HEVC::SPS& sps = *nalu.sps;

    sps.vui_parameters_present_flag = 1;
    sps.vui.default_display_window_flag = 1;
    sps.vui.def_disp_win_left_offset    = l;
    sps.vui.def_disp_win_right_offset   = r;
    sps.vui.def_disp_win_top_offset     = t;
    sps.vui.def_disp_win_bottom_offset  = b;

    sps.vui.hrd_parameters_present_flag = 0;

    repackNALU(nalu);
}

void TestSuite::repackAR(mfxU32 id, mfxU32  sw, mfxU32 sh)
{
    BS_HEVC::NALU& nalu = GetNalu(*pAU, BS_HEVC::NALU_TYPE::SPS_NUT);
    BS_HEVC::SPS& sps = *nalu.sps;

    sps.vui_parameters_present_flag         = 1;
    sps.vui.aspect_ratio_info_present_flag  = 1;
    sps.vui.aspect_ratio_idc                = id;
    sps.vui.sar_width                       = sw;
    sps.vui.sar_height                      = sh;

    sps.vui.hrd_parameters_present_flag = 0;

    repackNALU(nalu);
}

void TestSuite::repackVSI(mfxU32 vf, mfxU32 vfr, mfxU32 cdpf, mfxU32 cp, mfxU32 tc, mfxU32 mc)
{
    BS_HEVC::NALU& nalu = GetNalu(*pAU, BS_HEVC::NALU_TYPE::SPS_NUT);
    BS_HEVC::SPS& sps = *nalu.sps;

    sps.vui_parameters_present_flag         = 1;
    sps.vui.video_signal_type_present_flag  = 1;
    sps.vui.video_format                    = vf;
    sps.vui.video_full_range_flag           = vfr;
    sps.vui.colour_description_present_flag = cdpf;
    sps.vui.colour_primaries                = cp;
    sps.vui.transfer_characteristics        = tc;
    sps.vui.matrix_coeffs                   = mc;

    sps.vui.hrd_parameters_present_flag = 0;

    repackNALU(nalu);
}

void TestSuite::repackFieldSeq(mfxU32 fs)
{
    BS_HEVC::NALU& nalu = GetNalu(*pAU, BS_HEVC::NALU_TYPE::SPS_NUT);
    BS_HEVC::SPS& sps = *nalu.sps;

    sps.vui_parameters_present_flag         = 1;
    sps.vui.field_seq_flag                  = fs;

    sps.vui.hrd_parameters_present_flag = 0;

    repackNALU(nalu);
}

const char TestSuite::path[] = "conformance/hevc/";

#define EXT_BUF(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE,  "itu/HRD_A_Fujitsu_3.bin",},
    {/* 1*/ MFX_ERR_NONE,  "itu/HRD_A_Fujitsu_3.bin", {ATTACH_EXT_BUF, 0, {2, EXT_BUF(mfxExtCodingOptionSPSPPS), EXT_BUF(mfxExtVideoSignalInfo)}, {}, {}}},
    {/* 2*/ MFX_ERR_NONE,  "itu/DBLK_A_SONY_3.bit",},
    {/* 3*/ MFX_ERR_NONE,  "itu/DBLK_A_SONY_3.bit", {ATTACH_EXT_BUF, 0, {2, EXT_BUF(mfxExtCodingOptionSPSPPS), EXT_BUF(mfxExtVideoSignalInfo)}, {}, {}}},
    {/* 4*/ MFX_ERR_INVALID_HANDLE, "", {CLOSE}},
    {/* 5*/ MFX_ERR_NULL_PTR, "", {CHANGE_PAR}},
    {/* 6*/ MFX_ERR_NULL_PTR, "", {CHANGE_BS}},
    {/* 7*/ MFX_ERR_MORE_DATA, "itu/HRD_A_Fujitsu_3.bin", {CHANGE_BS, &tsStruct::mfxBitstream.DataLength, {16} }},
    {/* 8*/ MFX_ERR_UNDEFINED_BEHAVIOR, "itu/HRD_A_Fujitsu_3.bin", {CHANGE_BS, &tsStruct::mfxBitstream.DataLength, {max_bs_size + 1} }},
    {/* 9*/ MFX_ERR_UNDEFINED_BEHAVIOR, "itu/HRD_A_Fujitsu_3.bin", {CHANGE_BS, &tsStruct::mfxBitstream.DataOffset, {max_bs_size + 1} }},
    {/*10*/ MFX_ERR_NONE,  "itu/HRD_A_Fujitsu_3.bin", {REPACK_CROPS_CW, 0, {2, 4, 5, 3}}},
    {/*11*/ MFX_ERR_NONE,  "itu/HRD_A_Fujitsu_3.bin", {REPACK_CROPS_DW, 0, {5, 2, 7, 16}}},
    {/*12*/ MFX_ERR_NONE,  "itu/HRD_A_Fujitsu_3.bin", { REPACK_CROPS_DW, 0, { 0, 0, 16, 16}}},
    {/*13*/ MFX_ERR_NONE,  "itu/HRD_A_Fujitsu_3.bin", { REPACK_CROPS_DW, 0, { 48, 48, 0, 0}}},
    {/*14*/ MFX_ERR_NONE,  "itu/HRD_A_Fujitsu_3.bin", {{REPACK_CROPS_CW, 0, {2, 4, 5, 3}}, {REPACK_CROPS_DW, 0, {5, 2, 7, 16}} }},
    {/*15*/ MFX_ERR_NONE,  "itu/HRD_A_Fujitsu_3.bin", {REPACK_AR, 0, {13}}},
    {/*16*/ MFX_ERR_NONE,  "itu/HRD_A_Fujitsu_3.bin", {REPACK_AR, 0, {255, 4, 3}}},
    {/*17*/ MFX_ERR_NONE,  "itu/HRD_A_Fujitsu_3.bin", {REPACK_AR, 0, {255, 16, 9}}},
    {/*18*/ MFX_ERR_NONE,  "itu/HRD_A_Fujitsu_3.bin", {{REPACK_VSI, 0, {4,1,1,3,3,3}}, {ATTACH_EXT_BUF, 0, {1, EXT_BUF(mfxExtVideoSignalInfo)}}}},
    {/*19*/ MFX_ERR_NONE,  "itu/HRD_A_Fujitsu_3.bin", {REPACK_FS, 0, {1}}},
    {/*20*/ MFX_ERR_NONE,  "itu/FILLER_A_Sony_1.bit",},
    {/*21*/ MFX_ERR_NONE,  "422format/inter_422_8.bin",},
    {/*22*/ MFX_ERR_NONE,  "10bit/DBLK_A_MAIN10_VIXS_3.bit",},
    {/*23*/ MFX_ERR_NONE,  "10bit/GENERAL_10b_422_RExt_Sony_1.bit",},
    {/*24*/ MFX_ERR_NONE,  "10bit/GENERAL_10b_422_RExt_Sony_1.bit",},
    {/*25*/ MFX_ERR_NONE,  "10bit/GENERAL_8b_444_RExt_Sony_1.bit",},
    {/*26*/ MFX_ERR_NONE,  "10bit/GENERAL_10b_444_RExt_Sony_1.bit",},
    {/*27*/ MFX_ERR_NONE,  "StressBitstreamEncode/rext444_10b/Stress_HEVC_Rext444_10bHT62_432x240_30fps_302_inter_stress_2.2.hevc",},
    {/*28*/ MFX_ERR_NONE,  "12bit/400format/GENERAL_12b_400_RExt_Sony_1.bit",},
    {/*29*/ MFX_ERR_NONE,  "12bit/420format/GENERAL_12b_420_RExt_Sony_1.bit",},
    {/*30*/ MFX_ERR_NONE,  "12bit/422format/GENERAL_12b_422_RExt_Sony_1.bit",},
    {/*31*/ MFX_ERR_NONE,  "12bit/444format/GENERAL_12b_444_RExt_Sony_1.bit",},
    {/*32*/ MFX_ERR_NONE,  "scc/scc-main/020_main_palette_all_lf.hevc",},
    {/*33*/ MFX_ERR_NONE,  "scc/scc-main10/020_main10_palette_all_lf.hevc",},
    {/*34*/ MFX_ERR_NONE,  "scc/scc-main444/020_main444_palette_all_lf.hevc",},
    {/*35*/ MFX_ERR_NONE,  "scc/scc-main444_10/020_main444_10_palette_all_lf.hevc",},
};
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];
    if (tc.stream != "")
    {
        g_tsStreamPool.Get(tc.stream, path);
        g_tsStreamPool.Reg();
    }

    memset(m_pPar, 0xF0, sizeof(mfxVideoParam));
    m_pPar->mfx.CodecId = MFX_CODEC_HEVC;
    m_par.RefreshBuffers();

    if (tc.stream != "")
    {
        //set 1st 10 bytes to 0xFF and fill the rest by bitstream data
        tsReader stream(g_tsStreamPool.Get(tc.stream));
        m_bitstream.DataLength = 10;
        memset(m_bitstream.Data, 0xFF, m_bitstream.DataLength);
        m_bitstream.DataLength += stream.Read(m_bitstream.Data + m_bitstream.DataLength, m_bitstream.MaxLength - m_bitstream.DataLength);
    }
    MFXInit();
    if(m_uid)
    {
        Load();
    }

    //parse original stream
    if(tc.stream != "" && tc.sts >= 0)
    {
        SetBuffer(m_bitstream);
        pAU = &ParseOrDie();
    }

    apply_par(tc);

    //parse final stream
    if(tc.stream != "" && tc.sts >= 0)
    {
        set_trace_level(BS_HEVC::TRACE_LEVEL_SPS);
        SetBuffer(m_bitstream);
        pAU = &ParseOrDie();

    }

    if((mfxExtCodingOptionSPSPPS*)m_par)
    {
        mfxExtCodingOptionSPSPPS& eb = m_par;
        eb.SPSBuffer = m_sp_buf;
        eb.SPSBufSize = eb.PPSBufSize = repack_buf_size/2;
        eb.PPSBuffer = eb.SPSBuffer + eb.SPSBufSize;
    }

    mfxVideoParam expectedPar = m_par;

    g_tsStatus.expect(tc.sts);
    DecodeHeader(m_session, m_pBitstream, m_pPar);

    if(g_tsStatus.get() >= 0)
    {
        BS_HEVC::NALU& sps_nalu = GetNalu(*pAU, BS_HEVC::SPS_NUT);
        BS_HEVC::NALU& pps_nalu = GetNalu(*pAU, BS_HEVC::PPS_NUT);
        BS_HEVC::SPS&  sps = *sps_nalu.sps;
        BS_HEVC::PPS&  pps = *sps_nalu.pps;
        mfxU16 cf = sps.chroma_format_idc;
        mfxFrameInfo& fi = m_par.mfx.FrameInfo;

        double FrameRate = fi.FrameRateExtD ? (fi.FrameRateExtN / fi.FrameRateExtD) : .0;
        double expectedFrameRate = sps.vui.num_units_in_tick ? (sps.vui.time_scale / sps.vui.num_units_in_tick) : .0;

        EXPECT_EQ(sps_nalu.StartOffset, m_bitstream.DataOffset);

        expectedPar.mfx.FrameInfo       = m_par.mfx.FrameInfo;
        expectedPar.mfx.CodecProfile    = m_par.mfx.CodecProfile;
        expectedPar.mfx.CodecLevel      = m_par.mfx.CodecLevel;
        expectedPar.mfx.DecodedOrder    = m_par.mfx.DecodedOrder;
        expectedPar.mfx.MaxDecFrameBuffering = m_par.mfx.MaxDecFrameBuffering;

        if(memcmp(m_pPar, &expectedPar, sizeof(mfxVideoParam)))
        {
            ADD_FAILURE() << "FAILED: DecodeHeader should not change fields other than FrameInfo, CodecProfile, CodecLevel, DecodedOrder";
        }

        mfxU16 expected_width = (mfxU16)sps.pic_width_in_luma_samples;
        mfxU16 expected_height = (mfxU16)sps.pic_height_in_luma_samples;
        mfxU16 expectedPicStruct = sps.vui.field_seq_flag ? MFX_PICSTRUCT_FIELD_SINGLE : MFX_PICSTRUCT_PROGRESSIVE;

        expected_width = ( ( expected_width / 16 ) + ( (expected_width % 16) != 0 ) )* 16 ;
        expected_height = ( ( expected_height / 16 ) + ( (expected_height % 16) != 0 ) )* 16;

        EXPECT_EQ((mfxU16)sps.ptl.general.profile_idc,  m_par.mfx.CodecProfile);
        EXPECT_EQ((mfxU16)sps.ptl.general.level_idc/3,  m_par.mfx.CodecLevel & 0xFF);
        EXPECT_EQ(sps.ptl.general.tier_flag, (m_par.mfx.CodecLevel >> 8) & 0x1);
        EXPECT_EQ(expectedFrameRate, FrameRate);
        EXPECT_EQ(expected_width, m_par.mfx.FrameInfo.Width);
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

        EXPECT_EQ(expectedPicStruct, m_par.mfx.FrameInfo.PicStruct);

        EXPECT_EQ(sps.bit_depth_luma_minus8 + 8, m_par.mfx.FrameInfo.BitDepthLuma);
        EXPECT_EQ(sps.bit_depth_chroma_minus8 + 8, m_par.mfx.FrameInfo.BitDepthChroma);

        if ( (m_par.mfx.CodecProfile == MFX_PROFILE_HEVC_SCC) && (g_tsHWtype < MFX_HW_TGL) && (g_tsOSFamily == MFX_OS_FAMILY_LINUX) )
        {
            // SCC not supported before Gen12
            EXPECT_EQ(0, m_par.mfx.FrameInfo.FourCC);
        }
        else
        {
            if (m_par.mfx.FrameInfo.ChromaFormat == 0)
            {
                if (g_tsImpl != MFX_IMPL_SOFTWARE)
                    EXPECT_EQ(0, m_par.mfx.FrameInfo.FourCC);
                else
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
            }
            else if (m_par.mfx.FrameInfo.ChromaFormat == 1)
            {
                if (m_par.mfx.FrameInfo.BitDepthLuma == 8)
                {
                    EXPECT_EQ(MFX_FOURCC_NV12, m_par.mfx.FrameInfo.FourCC);
                }
                else if (m_par.mfx.FrameInfo.BitDepthLuma == 10)
                {
                    EXPECT_EQ(MFX_FOURCC_P010, m_par.mfx.FrameInfo.FourCC);
                }
                else if (m_par.mfx.FrameInfo.BitDepthLuma == 12)
                {
                    if (g_tsHWtype >= MFX_HW_TGL) //Gen12
                    {
                        EXPECT_EQ(MFX_FOURCC_P016, m_par.mfx.FrameInfo.FourCC);
                    }
                }
                else
                {
                    ADD_FAILURE() << "FAILED: Non supported bitdepth = " << m_par.mfx.FrameInfo.BitDepthLuma << " for chroma_format = "
                        << m_par.mfx.FrameInfo.ChromaFormat;
                }
            }
            else if (m_par.mfx.FrameInfo.ChromaFormat == 2)
            {
                if ((g_tsHWtype >= MFX_HW_ICL) || (g_tsImpl == MFX_IMPL_SOFTWARE))//Gen11 or SW
                {
                    if (m_par.mfx.FrameInfo.BitDepthLuma == 8)
                    {
                        EXPECT_EQ(MFX_FOURCC_YUY2, m_par.mfx.FrameInfo.FourCC);
                    }
                    else if (m_par.mfx.FrameInfo.BitDepthLuma == 10)
                    {
                        EXPECT_EQ(MFX_FOURCC_Y210, m_par.mfx.FrameInfo.FourCC);
                    }
                    else if (m_par.mfx.FrameInfo.BitDepthLuma == 12)
                    {
                        if (g_tsHWtype >= MFX_HW_TGL)//Gen12
                        {
                            EXPECT_EQ(MFX_FOURCC_Y216, m_par.mfx.FrameInfo.FourCC);
                        }
                    }
                    else
                    {
                        ADD_FAILURE() << "FAILED: Non supported bitdepth = " << m_par.mfx.FrameInfo.BitDepthLuma << " for chroma_format = "
                            << m_par.mfx.FrameInfo.ChromaFormat;
                    }
                }
             }
            else if (m_par.mfx.FrameInfo.ChromaFormat == 3)
            {
                if (g_tsHWtype >= MFX_HW_ICL)//Gen11
                {
                    if (m_par.mfx.FrameInfo.BitDepthLuma == 8)
                    {
                        EXPECT_EQ(MFX_FOURCC_AYUV, m_par.mfx.FrameInfo.FourCC);
                    }
                    else if (m_par.mfx.FrameInfo.BitDepthLuma == 10)
                    {
                        EXPECT_EQ(MFX_FOURCC_Y410, m_par.mfx.FrameInfo.FourCC);
                    }
                    else if (m_par.mfx.FrameInfo.BitDepthLuma == 12)//Gen12
                    {
                        if (g_tsHWtype >= MFX_HW_TGL)
                        {
                            EXPECT_EQ(MFX_FOURCC_Y416, m_par.mfx.FrameInfo.FourCC);
                        }
                    }
                    else
                    {
                        ADD_FAILURE() << "FAILED: Non supported bitdepth = " << m_par.mfx.FrameInfo.BitDepthLuma << " for chroma_format = "
                            << m_par.mfx.FrameInfo.ChromaFormat;
                    }
                }
            }
        }
        if((mfxExtCodingOptionSPSPPS*)m_par)
        {
            mfxExtCodingOptionSPSPPS& ecoSP = m_par;

            EXPECT_EQ(sps_nalu.NumBytesInNalUnit, ecoSP.SPSBufSize);
            EXPECT_EQ(0, memcmp(ecoSP.SPSBuffer, m_bitstream.Data + sps_nalu.StartOffset, ecoSP.SPSBufSize));
            EXPECT_EQ(pps_nalu.NumBytesInNalUnit, ecoSP.PPSBufSize);
            EXPECT_EQ(0, memcmp(ecoSP.PPSBuffer, m_bitstream.Data + pps_nalu.StartOffset, ecoSP.PPSBufSize));
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

TS_REG_TEST_SUITE_CLASS(hevcd_decode_header);
}