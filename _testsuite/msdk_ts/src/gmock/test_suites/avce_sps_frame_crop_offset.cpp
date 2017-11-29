/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace avce_sps_frame_crop_offset
{
    class TestSuite : tsVideoEncoder, tsBitstreamProcessor, tsParserH264AU
    {
    public:
        static const unsigned int n_cases;
        static const unsigned int n_frames = 5;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_AVC)
            , m_nframes(n_frames)
        {
            m_bs_processor = this;
            tsParserH264AU::set_trace_level(0);
        }
        ~TestSuite() {}

        int RunTest(unsigned int id);

    private:
        mfxU32 m_nframes;
        mfxStatus ProcessBitstream(mfxBitstream &bs, mfxU32 nFrames);
        mfxU16 initWidth;
        mfxU16 initHeight;
        struct tc_struct
        {
            mfxU32 fourcc;
            mfxU32 chromaFormat;
            mfxU16 width;
            mfxU16 height;
        };
        static const tc_struct test_case[];
    };

    mfxStatus TestSuite::ProcessBitstream(mfxBitstream &bs, mfxU32 nFrames)
    {
        SetBuffer(bs);

        auto& au = ParseOrDie();

        for (UnitType::nalu_param* nalu = au.NALU; nalu < au.NALU + au.NumUnits; nalu++)
        {
            if (nalu->nal_unit_type == 0x07) //SPS
            {
                Bs32u SubWidthC[3] = { 2, 2, 1 };
                Bs32u SubHeightC[3] = { 2, 1, 1 };

                Bs32u CropUnitX, CropUnitY;                
                Bs32u ChromaArrayType = nalu->SPS->separate_colour_plane_flag ? 0 : nalu->SPS->chroma_format_idc;
                
                Bs32u frame_crop_left_offset = nalu->SPS->frame_cropping_flag ? nalu->SPS->frame_crop_left_offset : 0;
                Bs32u frame_crop_right_offset = nalu->SPS->frame_cropping_flag ? nalu->SPS->frame_crop_right_offset : 0;
                Bs32u frame_crop_top_offset = nalu->SPS->frame_cropping_flag ? nalu->SPS->frame_crop_top_offset : 0;
                Bs32u frame_crop_bottom_offset = nalu->SPS->frame_cropping_flag ? nalu->SPS->frame_crop_bottom_offset : 0;

                if (ChromaArrayType)
                {
                    CropUnitX = SubWidthC[ChromaArrayType - 1];
                    CropUnitY = SubHeightC[ChromaArrayType - 1] *(2 - nalu->SPS->frame_mbs_only_flag);
                }
                else
                {
                    CropUnitX = 1;
                    CropUnitY = 2 - nalu->SPS->frame_mbs_only_flag;
                }

                Bs32u calculatedWidth = ((nalu->SPS->pic_width_in_mbs_minus1 + 1) * 16) - frame_crop_left_offset*CropUnitX - frame_crop_right_offset*CropUnitX;
                Bs32u calculatedHeight = ((2 - nalu->SPS->frame_mbs_only_flag)*(nalu->SPS->pic_height_in_map_units_minus1 + 1) * 16)
                                         - (frame_crop_top_offset * CropUnitY) - (frame_crop_bottom_offset * CropUnitY);

                EXPECT_EQ(initWidth, calculatedWidth)
                    << "calculatedWidth is wrong: " << calculatedWidth << " Should be: " << initWidth;

                EXPECT_EQ(initHeight, calculatedHeight)
                    << "calculatedHeight is wrong: " << calculatedHeight << " Should be: " << initHeight;

            }
        }

        return MFX_ERR_NONE;
    }

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_FOURCC_RGB4, MFX_CHROMAFORMAT_YUV444, 1620, 1080},
        {/*01*/ MFX_FOURCC_RGB4, MFX_CHROMAFORMAT_YUV444, 768,  1366},
        {/*02*/ MFX_FOURCC_NV12, MFX_CHROMAFORMAT_YUV420, 1620, 1080},
        {/*03*/ MFX_FOURCC_NV12, MFX_CHROMAFORMAT_YUV420, 768,  1366},
        
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);
    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];

        if (g_tsOSFamily != MFX_OS_FAMILY_WINDOWS && tc.fourcc == MFX_FOURCC_RGB4) {
            g_tsLog << "[ SKIPPED ] This test is only for windows platform\n";
            return 0;
        }

        m_par.mfx.FrameInfo.FourCC = tc.fourcc;
        m_par.mfx.FrameInfo.ChromaFormat = tc.chromaFormat;
        m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.mfx.FrameInfo.Width = (mfxU16)(((tc.width + 15) >> 4) << 4);
        m_par.mfx.FrameInfo.CropW = initWidth = tc.width;
        m_par.mfx.FrameInfo.Height = (mfxU16)(((tc.height + 15) >> 4) << 4);
        m_par.mfx.FrameInfo.CropH = initHeight = tc.height;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
        set_brc_params(&m_par);

        MFXInit();
        g_tsStatus.expect(MFX_ERR_NONE);
        Query();

        AllocBitstream(1024 * 1024 * m_nframes);
        Init();
        EncodeFrames(m_nframes);

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(avce_sps_frame_crop_offset);
}