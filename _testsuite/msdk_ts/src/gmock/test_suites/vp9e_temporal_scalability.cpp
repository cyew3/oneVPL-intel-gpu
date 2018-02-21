/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace vp9e_temporal_scalability
{
#define VP9E_MAX_TEMPORAL_LAYERS (4)
#define VP9E_DEFAULT_BITRATE (500)
#define VP9E_PSNR_THRESHOLD (20.0)
#define VP9E_MAX_FRAMES_IN_TEST_SEQUENCE (256)

#define IVF_SEQ_HEADER_SIZE_BYTES (32)
#define IVF_PIC_HEADER_SIZE_BYTES (12)

    enum
    {
        NONE,
        MFX_PAR,
        CDO2_PAR,
    };

    enum
    {
        CHECK_QUERY = 0x1,
        CHECK_INIT  = 0x1 << 1,
        CHECK_ENCODE = 0x1 << 2,
        CHECK_GET_V_PARAM = 0x1 << 3,
        CHECK_RESET = 0x1 << 4,
        CHECK_ZERO_LAYER = 0x1 << 5,
        CHECK_FIRST_LAYER = 0x1 << 6,
        CHECK_SWITCH_OFF_TEMP_SCAL = 0x1 << 7,
        CHECK_SWITCH_ON_TEMP_SCAL = 0x1 << 8,
        CHECK_TEMP_SCAL_ON_FRAME = 0x1 << 9,
        CHECK_PSNR = 0x1 << 10,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 type;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder, BS_VP9_parser
    {
    private:
        void EncodingCycle(const tc_struct& tc, const mfxU32& frames_count);
        mfxStatus EncodeFrames(mfxU32 n, mfxU32 flags = 0);
        mfxStatus EnumerateFramesWithLayers(mfxU32 type = 0);
        static const tc_struct test_case[];
        mfxI16 m_RuntimeSettingsCtrl;

    public:
        static const unsigned int n_cases;
        mfxExtVP9TemporalLayers *temporal_layers_ext_params;
        std::map<mfxU32, mfxU32> m_encoded_frame_sizes;
        mfxI8 m_LayerNestingLevel[VP9E_MAX_FRAMES_IN_TEST_SEQUENCE];
        mfxI8 m_LayerToCheck;
        mfxU32 m_SourceFrameCount = 0;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_VP9)
            , m_RuntimeSettingsCtrl(0)
            , temporal_layers_ext_params(nullptr)
            , m_LayerToCheck(-1)
            , m_SourceFrameCount(0)
        {
            memset(m_LayerNestingLevel, -1, VP9E_MAX_FRAMES_IN_TEST_SEQUENCE);
        }
        ~TestSuite() {}

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(const tc_struct& tc, unsigned int fourcc_id);
    };

    const tc_struct TestSuite::test_case[] =
    {
        // query: correct params with 2 temporal layers
        {/*00*/ MFX_ERR_NONE, CHECK_QUERY,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE/2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // query: incorrect 'FrameRateScale' for the 1st layer is wrong
        {/*01*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_QUERY,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE/2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // query: incorrect bitrate for the 1st layer is wrong
        {/*02*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_QUERY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
            }
        },

        // query: too many layers - error
        {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 6},
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE / 5 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].FrameRateScale, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].TargetKbps, VP9E_DEFAULT_BITRATE / 4},
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[3].FrameRateScale, 8 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[3].TargetKbps, VP9E_DEFAULT_BITRATE / 3 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[4].FrameRateScale, 16 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[4].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[5].FrameRateScale, 32 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[5].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // init: too many layers - error
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_INIT,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 6 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE / 5 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].FrameRateScale, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].TargetKbps, VP9E_DEFAULT_BITRATE / 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[3].FrameRateScale, 8 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[3].TargetKbps, VP9E_DEFAULT_BITRATE / 3 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[4].FrameRateScale, 16 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[4].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[5].FrameRateScale, 32 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[5].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // init + get_v_param + encode: correct params with 2 temporal layers (only mandatory params are set)
        {/*05*/ MFX_ERR_NONE, CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // init + get_v_param + encode: correct params with 2 temporal layers, but TargetKbps is not set - warning
        {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // init + get_v_param + encode: correct params with 2 temporal layers (+ non-mandatory MaxKbps)
        {/*07*/ MFX_ERR_NONE, CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,    VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // init + get_v_param + encode: TargetKbps is less than MaxLayerLevel->TargetKbps
        {/*08*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // all routines: TargetKbps for 0-layer is not set - Error
        {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_INIT | CHECK_GET_V_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, 0 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // all routines: FrameRateScale for 0-layer is not set - Error
        {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_INIT | CHECK_GET_V_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 0 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // all routines: TargetKbps for 1st layer is less than for the 0st layer - Error
        {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_INIT | CHECK_GET_V_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
            }
        },

        // all routines: FrameRateScale for 2nd layer is wrong - Error
        {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_INIT | CHECK_GET_V_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // all routines: corner cases for the layers' values - should work
        {/*13*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 0xffff },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 0xffff },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, 0xffff },
            }
        },

        // all routines: CQP-mode - should work
        {/*14*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 50 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, 0 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, 0 },
            }
        },

        // all routines: correct VBR-mode - should work
        {/*15*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE * 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // all routines: VBR-mode with incorrect MaxKbps - maxKbps should be corrected
        {/*16*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE / 2},
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: too small bitrates (global TargetKbps should be corrected)
        {/*17*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY | CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 10 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE / 5 },
            }
        },

        // correct params: too big bitrates
        {/*18*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 0xffff },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, (0xffff) / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, 0xffff },
            }
        },

        // correct params: layers' bitrates should be updated with multiplier
        {/*19*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_GET_V_PARAM | CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: decode zero layer from a stream of 2 layers (TU=1)
        {/*20*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: decode zero layer from a stream of 2 layers (TU=4)
        {/*21*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: decode zero layer from a stream of 2 layers (TU=7)
        {/*22*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: decode zero layer from a stream of 2 layers (NumRefFrame=1)
        {/*23*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: decode zero layer from a stream of 2 layers (NumRefFrame=2)
        {/*24*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: decode zero layer from a stream of 2 layers (NumRefFrame=3)
        {/*25*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params, but 'NumRefFrame' is not set (should use default value)
        {/*26*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE/2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params, CQP-mode
        {/*27*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 100 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 110 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, 0 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, 0 },
            }
        },

        // correct params: check decoding of 1st layer from a stream of 3 layers
        {/*28*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_FIRST_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].FrameRateScale, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // query: 2nd layer has wrong FrameRateScale
        {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_QUERY,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].FrameRateScale, 3 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // init: 2nd layer has wrong FrameRateScale
        {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_INIT,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].FrameRateScale, 3 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: big RateScale
        {/*31*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 5 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // Temporal Scalability is set on frame level - unsupported, check error status after EncodeFrameAsync()
        {/*32*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_TEMP_SCAL_ON_FRAME,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // Cases below include 2 parts of encoding - it is expected the second part to have better quality (= bigger size of the encoded stream)

        // bitrate is being increased on reset
        {/*33*/ MFX_ERR_NONE, CHECK_RESET | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // Temporal Scalability is being switched off on reset
        {/*34*/ MFX_ERR_NONE, CHECK_RESET | CHECK_SWITCH_OFF_TEMP_SCAL | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // Temporal Scalability is being activated on reset
        {/*35*/ MFX_ERR_NONE, CHECK_RESET | CHECK_SWITCH_ON_TEMP_SCAL | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE / 5 },
            }
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(test_case) / sizeof(tc_struct);

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserVP9, public tsVideoDecoder
    {
    private:
        TestSuite *m_TestPtr;
        std::map<mfxU32, mfxFrameSurface1*>* m_pInputSurfaces;
        bool m_CheckPsnr;
    public:
        BitstreamChecker(TestSuite *testPtr, std::map<mfxU32, mfxFrameSurface1*>* pSurfaces, bool check_psnr = false);
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
        mfxU32 m_AllFramesSize;
        mfxU32 m_DecodedFramesCount;
        bool m_DecoderInited;
        mfxU32 m_ChunkCount;
    };

    BitstreamChecker::BitstreamChecker(TestSuite *testPtr, std::map<mfxU32, mfxFrameSurface1*>* pSurfaces, bool check_psnr)
        : tsVideoDecoder(MFX_CODEC_VP9)
        , m_TestPtr(testPtr)
        , m_pInputSurfaces(pSurfaces)
        , m_CheckPsnr(check_psnr)
        , m_AllFramesSize(0)
        , m_DecodedFramesCount(0)
        , m_DecoderInited(false)
        , m_ChunkCount(0)
    {}

    mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        SetBuffer(bs);
/*
        // Dump encoded stream to file
        const int encoded_size = bs.DataLength;
        static FILE *fp_vp9 = fopen("vp9e_encoded_temporal_scalability.vp9", "wb");
        fwrite(bs.Data, encoded_size, 1, fp_vp9);
        fflush(fp_vp9);
*/
        m_TestPtr->m_encoded_frame_sizes[m_ChunkCount] = bs.DataLength;

        while (checked++ < nFrames)
        {
            tsParserVP9::UnitType& hdr = ParseOrDie();

            mfxU32 index_size = 0;

            if (m_TestPtr)
            {
                mfxExtVP9TemporalLayers *temporal_layers = m_TestPtr->temporal_layers_ext_params;
                if (temporal_layers && (m_TestPtr->m_LayerNestingLevel[m_ChunkCount] != m_TestPtr->m_LayerNestingLevel[m_ChunkCount + 1]))
                {
                    // it is expected that the last frame in the end of each layer sequence is superframe:
                    // [sub-frame with encoded data and show_frame=false] + [dummy sub-frame with show_frame=true] + [superframe index - see VP9 standard]

                    // let's start with checking superframe index correctness
                    const mfxU8 superframe_header = bs.Data[bs.DataLength - 1];
                    const mfxU32 superframe_marker = superframe_header & 0xe0;
                    const mfxU32 frames_in_superframe = (superframe_header & 0x7) + 1;
                    const mfxU32 bytes_per_framesize = ((superframe_header >> 3) & 3) + 1;
                    index_size = 2 + frames_in_superframe*bytes_per_framesize;

                    if (superframe_marker == 0xc0)
                    {
                        if (bs.DataLength > index_size)
                        {
                            if (bs.Data[bs.DataLength - index_size] == superframe_header)
                            {
                                // Let's check the second frame inside the superframe
                                const unsigned char frame_header = bs.Data[bs.DataLength - index_size - 1];

                                const unsigned char header_marker = (frame_header >> 6) & 0x2;
                                const unsigned char header_profile = (frame_header >> 4) & 0x2;
                                const unsigned char header_show_existing_frame = (frame_header >> 3) & 0x1;

                                if (header_marker != 0x2)
                                {
                                    ADD_FAILURE() << "ERROR: Superframe: 2nd frame: Unc. header: Marker =" << header_marker
                                        << ", expected 2"; throw tsFAIL;
                                }
                                if (header_profile != 0x0)
                                {
                                    ADD_FAILURE() << "ERROR: Superframe: 2nd frame: Unc. header: Profile =" << header_profile
                                        << ", expected 0"; throw tsFAIL;
                                }
                                if (header_show_existing_frame != 0x1)
                                {
                                    ADD_FAILURE() << "ERROR: Superframe: 2nd frame: Unc. header: show_existing_frame =" << header_show_existing_frame
                                        << ", expected 1"; throw tsFAIL;
                                }
                            }
                            else
                            {
                                ADD_FAILURE() << "ERROR: Invalid superframe index [headers in the beginning and in the end do not match] for chunk #" << m_ChunkCount;
                                throw tsFAIL;
                            }
                        }
                        else
                        {
                            ADD_FAILURE() << "ERROR: Index size calculated as " << index_size << " bytes, but whole chunk #" << m_ChunkCount
                                << " is only " << bs.DataLength << " bytes"; throw tsFAIL;
                        }
                    }
                    else
                    {
                        ADD_FAILURE() << "ERROR: Superframe index is not found in the end of the chunk #" << m_ChunkCount; throw tsFAIL;
                    }

                    if (hdr.uh.show_frame != 0)
                    {
                        ADD_FAILURE() << "ERROR: first frame in the superframe expected be with flag show_frame=false, but it is not"; throw tsFAIL;
                    }
                }
                else
                {
                    // for other frames superframe syntax is not expected
                    const mfxU8 superframe_header = bs.Data[bs.DataLength - 1];
                    const mfxU32 superframe_marker = superframe_header & 0xe0;

                    if (superframe_marker == 0xc0)
                    {
                        if (temporal_layers == nullptr)
                        {
                            ADD_FAILURE() << "ERROR: superframe index marker is found at a frame with disabled temporal scalability"; throw tsFAIL;
                        }
                        else
                        {
                            ADD_FAILURE() << "ERROR: superframe index marker is found at a frame in the middle of the layer sequence"; throw tsFAIL;
                        }
                    }
                }
            }

            // do the decoder initialization on the first encoded frame
            if (m_DecodedFramesCount == 0 && m_CheckPsnr)
            {
                const mfxU32 headers_shift = IVF_PIC_HEADER_SIZE_BYTES + (m_DecodedFramesCount == 0 ? IVF_SEQ_HEADER_SIZE_BYTES : 0);
                m_pBitstream->Data = bs.Data + headers_shift;
                m_pBitstream->DataOffset = 0;
                m_pBitstream->DataLength = bs.DataLength - headers_shift;
                m_pBitstream->MaxLength = bs.MaxLength;

                m_pPar->AsyncDepth = 1;

                mfxStatus decode_header_status = DecodeHeader();

                mfxStatus init_status = Init();
                m_par_set = true;
                if (init_status >= 0)
                {
                    if (m_default && !m_request.NumFrameMin)
                    {
                        QueryIOSurf();
                    }

                    mfxStatus alloc_status = tsSurfacePool::AllocSurfaces(m_request, !m_use_memid);
                    if (alloc_status >= 0)
                    {
                        m_DecoderInited = true;
                    }
                    else
                    {
                        g_tsLog << "ERROR: Could not allocate surfaces for the decoder, status " << alloc_status;
                        throw tsFAIL;
                    }
                }
                else
                {
                    g_tsLog << "ERROR: Could not inilialize the decoder, Init() returned " << init_status;
                    throw tsFAIL;
                }
            }

            if (m_DecoderInited)
            {
                if (m_TestPtr->m_LayerToCheck != -1 && m_TestPtr->m_LayerNestingLevel[m_ChunkCount] > m_TestPtr->m_LayerToCheck)
                {
                    g_tsLog << "INFO: Frame " << m_ChunkCount << " is not sending to the decoder because layer " << (mfxI32)m_TestPtr->m_LayerToCheck
                        << " is being checked\n";
                }
                else
                {
                    g_tsLog << "INFO: Decoding frame " << m_ChunkCount << ", checked layer is " << (mfxI32)m_TestPtr->m_LayerToCheck << "\n";

                    const mfxU32 headers_shift = IVF_PIC_HEADER_SIZE_BYTES + (m_DecodedFramesCount == 0 ? IVF_SEQ_HEADER_SIZE_BYTES : 0);
                    m_pBitstream->Data = bs.Data + headers_shift;
                    m_pBitstream->DataOffset = 0;
                    m_pBitstream->DataLength = bs.DataLength - headers_shift;
                    m_pBitstream->MaxLength = bs.MaxLength;
                    mfxStatus decode_status = DecodeFrameAsync();
                    if (decode_status == MFX_WRN_DEVICE_BUSY)
                    {
                        decode_status = DecodeFrameAsync();
                    }

                    if (decode_status >= 0)
                    {
                        mfxStatus sync_status = SyncOperation();
                        if (sync_status >= 0)
                        {
                            mfxU32 w = m_pPar->mfx.FrameInfo.Width;
                            mfxU32 h = m_pPar->mfx.FrameInfo.Height;

                            mfxFrameSurface1* pInputSurface = (*m_pInputSurfaces)[hdr.FrameOrder];
                            tsFrame src = tsFrame(*pInputSurface);
                            tsFrame res = tsFrame(*m_pSurf);
                            src.m_info.CropX = src.m_info.CropY = res.m_info.CropX = res.m_info.CropY = 0;
                            src.m_info.CropW = res.m_info.CropW = w;
                            src.m_info.CropH = res.m_info.CropH = h;

                            const mfxF64 psnrY = PSNR(src, res, 0);
                            const mfxF64 psnrU = PSNR(src, res, 1);
                            const mfxF64 psnrV = PSNR(src, res, 2);

                            const mfxF64 minPsnr = VP9E_PSNR_THRESHOLD;

                            g_tsLog << "INFO: frame[" << hdr.FrameOrder << "]: PSNR_Y=" << psnrY << " PSNR_U="
                                << psnrU << " PSNR_V=" << psnrV << " size=" << bs.DataLength << "\n";

                            if (psnrY < minPsnr)
                            {
                                ADD_FAILURE() << "ERROR: PSNR_Y of frame " << hdr.FrameOrder << " is equal to " << psnrY << " and lower than threshold: " << minPsnr;
                                throw tsFAIL;
                            }
                            if (psnrU < minPsnr)
                            {
                                ADD_FAILURE() << "ERROR: PSNR_U of frame " << hdr.FrameOrder << " is equal to " << psnrU << " and lower than threshold: " << minPsnr;
                                throw tsFAIL;
                            }
                            if (psnrV < minPsnr)
                            {
                                ADD_FAILURE() << "ERROR: PSNR_V of frame " << hdr.FrameOrder << " is equal to " << psnrV << " and lower than threshold: " << minPsnr;
                                throw tsFAIL;
                            }
                        }
                    }
                    else
                    {
                        g_tsLog << "ERROR: DecodeFrameAsync() failed with the status " << decode_status; throw tsFAIL;
                    }

                    m_DecodedFramesCount++;
                }
            }

            if (m_CheckPsnr)
            {
                // now delete stored source frame regardless of whether it was used was checking on not
                mfxFrameSurface1* pInputSurface = (*m_pInputSurfaces)[hdr.FrameOrder];
                if (pInputSurface)
                {
                    pInputSurface->Data.Locked--;
                    m_pInputSurfaces->erase(hdr.FrameOrder);
                }
                else
                {
                    ADD_FAILURE() << "ERROR: Received encoded frame with unexpected hdr.FrameOrder=" << hdr.FrameOrder;
                    throw tsFAIL;
                }
            }

            m_AllFramesSize += bs.DataLength;
            bs.DataLength = bs.DataOffset = 0;
        }

        bs.DataLength = 0;

        m_ChunkCount++;

        return MFX_ERR_NONE;
    }

    bool TemporalLayersParamsChecker(std::string caller_method_name, tsExtBufType<mfxVideoParam> &input, tsExtBufType<mfxVideoParam> &output)
    {
        mfxExtVP9TemporalLayers *temporal_layers_in = reinterpret_cast <mfxExtVP9TemporalLayers*>(input.GetExtBuffer(MFX_EXTBUFF_VP9_TEMPORAL_LAYERS));
        mfxExtVP9TemporalLayers *temporal_layers_out = reinterpret_cast <mfxExtVP9TemporalLayers*>(output.GetExtBuffer(MFX_EXTBUFF_VP9_TEMPORAL_LAYERS));

        if (temporal_layers_in != nullptr && temporal_layers_out == nullptr)
        {
            ADD_FAILURE() << "ERROR: " << caller_method_name << " does not have mfxExtVP9TemporalLayers in the output while input has one"; return false;
        }

        if (temporal_layers_in)
        {
            for (mfxU32 i = 0; i < VP9E_MAX_TEMPORAL_LAYERS; i++)
            {
                if (temporal_layers_in->Layer[i].FrameRateScale && !temporal_layers_out->Layer[i].FrameRateScale)
                {
                    ADD_FAILURE() << "ERROR: " << caller_method_name << ": Layer[" << i << "] FrameRateScale_in=" << temporal_layers_in->Layer[i].FrameRateScale
                        << " FrameRateScale_out=" << temporal_layers_out->Layer[i].FrameRateScale; return false;
                }

                if (temporal_layers_in->Layer[i].TargetKbps && !temporal_layers_out->Layer[i].TargetKbps)
                {
                    ADD_FAILURE() << "ERROR: " << caller_method_name << ": Layer[" << i << "] TargetKbps_in=" << temporal_layers_in->Layer[i].TargetKbps
                        << " TargetKbps_out=" << temporal_layers_out->Layer[i].TargetKbps; return false;
                }

                if (input.mfx.BRCParamMultiplier > 0)
                {
                    if (temporal_layers_in->Layer[i].TargetKbps * input.mfx.BRCParamMultiplier != temporal_layers_out->Layer[i].TargetKbps * output.mfx.BRCParamMultiplier)
                    {
                        ADD_FAILURE() << "ERROR: BRCParamMultiplier did not apply to TargetKbps for Layer-" << i; return false;
                    }
                }
            }
        }

        return true;
    }

    mfxStatus TestSuite::EncodeFrames(mfxU32 n, mfxU32 flags)
    {
        mfxU32 encoded = 0;
        mfxU32 submitted = 0;
        mfxU32 async = TS_MAX(1, m_par.AsyncDepth);
        mfxSyncPoint sp = nullptr;

        async = TS_MIN(n, async - 1);

        while (encoded < n)
        {
            if ( m_RuntimeSettingsCtrl )
            {
                m_ctrl.AddExtBuffer(MFX_EXTBUFF_VP9_TEMPORAL_LAYERS, sizeof(mfxExtVP9TemporalLayers));
                mfxExtVP9TemporalLayers *temporal_layers_ext_params_ctrl = reinterpret_cast <mfxExtVP9TemporalLayers*>(m_ctrl.GetExtBuffer(MFX_EXTBUFF_VP9_TEMPORAL_LAYERS));
                EXPECT_EQ(!!temporal_layers_ext_params_ctrl, true);
                temporal_layers_ext_params_ctrl->Layer[0].FrameRateScale = 1;
                temporal_layers_ext_params_ctrl->Layer[0].TargetKbps = VP9E_DEFAULT_BITRATE;
                temporal_layers_ext_params_ctrl->Layer[1].FrameRateScale = 2;
                temporal_layers_ext_params_ctrl->Layer[1].TargetKbps = VP9E_DEFAULT_BITRATE*2;
            }

            mfxStatus encode_status = EncodeFrameAsync();

            if (m_RuntimeSettingsCtrl && encode_status == MFX_ERR_UNSUPPORTED)
            {
                // Temporal Scalability is not supported on the frame level params
                return MFX_ERR_NONE;
            }

            submitted++;
            m_SourceFrameCount++;

            if (MFX_ERR_MORE_DATA == encode_status)
            {
                if (!m_pSurf)
                {
                    if (submitted)
                    {
                        encoded += submitted;
                        SyncOperation(sp);
                    }
                    break;
                }
                continue;
            }

            g_tsStatus.check(); TS_CHECK_MFX;
            sp = m_syncpoint;

            if (submitted >= async)
            {
                SyncOperation(); TS_CHECK_MFX;

                encoded += submitted;
                submitted = 0;
                async = TS_MIN(async, (n - encoded));
            }
        }

        g_tsLog << encoded << " FRAMES ENCODED\n";

        if (encoded != n)
            return MFX_ERR_UNKNOWN;

        return g_tsStatus.get();
    }

    mfxStatus TestSuite::EnumerateFramesWithLayers(mfxU32 type)
    {
        temporal_layers_ext_params = reinterpret_cast <mfxExtVP9TemporalLayers*>(m_par.GetExtBuffer(MFX_EXTBUFF_VP9_TEMPORAL_LAYERS));
        if (temporal_layers_ext_params)
        {
            // let's match each encoded frame with the layer it is neste

            if (temporal_layers_ext_params->Layer[0].FrameRateScale != 1)
            {
                // do not make calculations for wrong params
                return MFX_ERR_NONE;
            }

            mfxU16 section_length = 0;
            mfxU16 layers_count = 0;
            for (mfxU16 i = 0; i < VP9E_MAX_TEMPORAL_LAYERS; i++)
            {
                if (temporal_layers_ext_params->Layer[i].FrameRateScale != 0)
                {
                    section_length = temporal_layers_ext_params->Layer[i].FrameRateScale;
                    layers_count++;

                    if (i > 1 && temporal_layers_ext_params->Layer[i].FrameRateScale < temporal_layers_ext_params->Layer[i - 1].FrameRateScale * 2)
                    {
                        // do not make calculations for wrong params
                        return MFX_ERR_NONE;
                    }
                }
                else
                {
                    break;
                }
            }
            mfxU16 *section_template = new mfxU16[section_length];

            for (mfxU32 i = 0; i < section_length; i++)
            {
                section_template[i] = layers_count;
            }

            for (mfxU32 i = layers_count - 1; i > 1; i--)
            {
                mfxU16 ratio = temporal_layers_ext_params->Layer[i].FrameRateScale / temporal_layers_ext_params->Layer[i-1].FrameRateScale;
                mfxU16 ratio_counter = ratio;
                for (mfxU32 j = 0; j < section_length; j++)
                {
                    if (section_template[j] == i + 1)
                    {
                        if (ratio_counter == ratio)
                        {
                            section_template[j] = i;
                        }
                        ratio_counter--;
                        if (ratio_counter == 0)
                        {
                            ratio_counter = ratio;
                        }
                    }
                }
            }

            section_template[0] = 1;

            mfxU32 counter = 0;
            for (mfxU32 i = 0; i < VP9E_MAX_FRAMES_IN_TEST_SEQUENCE; i++)
            {
                m_LayerNestingLevel[i] = section_template[counter++] - 1;
                if (counter == section_length)
                {
                    counter = 0;
                }
            }

            if (type & CHECK_ZERO_LAYER)
            {
                m_LayerToCheck = 0;
            }
            else if (type & CHECK_FIRST_LAYER)
            {
                m_LayerToCheck = 1;
            }
        }

        return MFX_ERR_NONE;
    }

    class SurfaceFeeder : public tsRawReader
    {
        std::map<mfxU32, mfxFrameSurface1*>* m_pInputSurfaces;
        mfxU32 m_frameOrder;

        tsSurfacePool surfaceStorage;
    public:
        SurfaceFeeder(
            std::map<mfxU32, mfxFrameSurface1*>* pInSurfaces,
            const mfxVideoParam& par,
            const char* fname,
            mfxU32 n_frames = 0xFFFFFFFF)
            : tsRawReader(fname, par.mfx.FrameInfo, n_frames)
            , m_pInputSurfaces(pInSurfaces)
            , m_frameOrder(0)
        {
            if (m_pInputSurfaces)
            {
                const mfxU32 numInternalSurfaces = par.AsyncDepth ? par.AsyncDepth : 5;
                mfxFrameAllocRequest req = {};
                req.Info = par.mfx.FrameInfo;
                req.NumFrameMin = req.NumFrameSuggested = numInternalSurfaces;
                req.Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
                surfaceStorage.AllocSurfaces(req);
            }
        };

        ~SurfaceFeeder()
        {
            if (m_pInputSurfaces)
            {
                surfaceStorage.FreeSurfaces();
            }
        }

        mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa);
    };

    mfxFrameSurface1* SurfaceFeeder::ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
    {
        mfxFrameSurface1* pSurf = tsSurfaceProcessor::ProcessSurface(ps, pfa);
        if (m_pInputSurfaces)
        {
            mfxFrameSurface1* pStorageSurf = surfaceStorage.GetSurface();
            msdk_atomic_inc16(&pStorageSurf->Data.Locked);
            tsFrame in = tsFrame(*pSurf);
            tsFrame store = tsFrame(*pStorageSurf);
            store = in;
            (*m_pInputSurfaces)[m_frameOrder] = pStorageSurf;
        }
        m_frameOrder++;

        return pSurf;
    }

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(const tc_struct& tc, unsigned int fourcc_id)
    {
        TS_START;

        std::map<mfxU32, mfxFrameSurface1*> inputSurfaces;
        char* stream = nullptr;

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"));

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 352;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 288;
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

            stream = const_cast<char *>(g_tsStreamPool.Get("YUV10bit420_ms/Kimono1_176x144_24_p010_shifted.yuv"));

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            stream = const_cast<char *>(g_tsStreamPool.Get("YUV8bit444/Kimono1_176x144_24_ayuv.yuv"));

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

            stream = const_cast<char *>(g_tsStreamPool.Get("YUV10bit444/Kimono1_176x144_24_y410.yuv"));

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        g_tsStreamPool.Reg();
        tsRawReader *reader = nullptr;

        const mfxU32 default_frames_number = 10;

        MFXInit();
        Load();

        //set default params
        m_par.mfx.InitialDelayInKB = m_par.mfx.TargetKbps = m_par.mfx.MaxKbps = 0;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;

        BitstreamChecker bs_checker(this, &inputSurfaces, tc.type & CHECK_PSNR ? true : false);
        m_bs_processor = &bs_checker;

        SETPARS(m_par, MFX_PAR);

        if (tc.type & CHECK_TEMP_SCAL_ON_FRAME)
        {
            m_RuntimeSettingsCtrl = 1;
        }

        InitAndSetAllocator();

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL(NV12) and ICL(P010, AYUV, Y410)
            if ((fourcc_id == MFX_FOURCC_NV12 && g_tsHWtype < MFX_HW_CNL)
                || ((fourcc_id == MFX_FOURCC_P010 || fourcc_id == MFX_FOURCC_AYUV
                    || fourcc_id == MFX_FOURCC_Y410) && g_tsHWtype < MFX_HW_ICL))
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
        }
        else {
            g_tsLog << "WARNING: loading encoder from plugin failed!\n";
            return 0;
        }

        EnumerateFramesWithLayers(tc.type);

        // QUERY SECTION
        if (tc.type & CHECK_QUERY)
        {
            mfxStatus query_expect_status = MFX_ERR_NONE;
            if (tc.sts < 0)
            {
                query_expect_status = MFX_ERR_UNSUPPORTED;
            }
            else if(tc.sts > 0)
            {
                query_expect_status = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }

            g_tsStatus.expect(query_expect_status);
            tsExtBufType<mfxVideoParam> par_query_out = m_par;

            TS_TRACE(m_pPar);
            mfxStatus query_result_status = MFXVideoENCODE_Query(m_session, m_pPar, &par_query_out);
            TS_TRACE(&par_query_out);

            if (tc.sts == MFX_ERR_NONE && !TemporalLayersParamsChecker("Query", m_par, par_query_out))
            {
                ADD_FAILURE() << "ERROR: Query() parameters check failed!"; throw tsFAIL;
            }

            g_tsLog << "Query() returned with status " << query_result_status << ", expected status " << query_expect_status << "\n";
            g_tsStatus.check(query_result_status);
        }

        // INIT SECTION
        if (tc.type & CHECK_INIT || tc.type & CHECK_GET_V_PARAM || tc.type & CHECK_ENCODE || tc.type & CHECK_RESET)
        {
            g_tsStatus.expect(tc.sts);
            TRACE_FUNC2(MFXVideoENCODE_Init, m_session, m_pPar);
            mfxStatus init_result_status = MFXVideoENCODE_Init(m_session, m_pPar);
            if (init_result_status >= MFX_ERR_NONE)
            {
                m_initialized = true;
            }
            g_tsLog << "Init() returned with status " << init_result_status << ", expected status " << tc.sts << "\n";
            g_tsStatus.check(init_result_status);
        }

        // GET_VIDEO_PARAM SECTION
        if (tc.type & CHECK_GET_V_PARAM && m_initialized)
        {
            g_tsStatus.expect(MFX_ERR_NONE);

            tsExtBufType<mfxVideoParam> out_par = m_par;
            mfxStatus getvparam_result_status = GetVideoParam(m_session, &out_par);

            if (!TemporalLayersParamsChecker("GetVideoParam", m_par, out_par))
            {
                ADD_FAILURE() << "ERROR: GetVideoParam() check parameters failed!"; throw tsFAIL;
            }

            g_tsLog << "GetVideoParam() returned with status " << getvparam_result_status << ", expected status MFX_ERR_NONE\n";
            g_tsStatus.check(getvparam_result_status);
        }

        g_tsStatus.expect(MFX_ERR_NONE);

        mfxU32 frames_size = 0;

        // ENCODE SECTION
        if ((tc.type & CHECK_ENCODE || tc.type & CHECK_RESET) && m_initialized)
        {
            //set reader
            if (reader == nullptr)
            {
                if (tc.type & CHECK_PSNR)
                {
                    reader = new SurfaceFeeder(&inputSurfaces, m_par, stream);
                }
                else
                {
                    reader = new tsRawReader(stream, m_par.mfx.FrameInfo);
                }
                reader->m_disable_shift_hack = true;  // this hack adds shift if fi.Shift != 0!!! Need to disable it.
                m_filler = reader;
            }

            if (tc.sts >= MFX_ERR_NONE)
            {
                TestSuite::EncodeFrames(default_frames_number, tc.type);

                g_tsStatus.check(DrainEncodedBitstream());
                TS_CHECK_MFX;
                frames_size = bs_checker.m_AllFramesSize;
            }
        }

        // RESET SECTION
        if ((tc.type & CHECK_RESET) && m_initialized)
        {
            temporal_layers_ext_params = reinterpret_cast <mfxExtVP9TemporalLayers*>(m_par.GetExtBuffer(MFX_EXTBUFF_VP9_TEMPORAL_LAYERS));
            if (tc.type & CHECK_SWITCH_OFF_TEMP_SCAL)
            {
                m_par.mfx.TargetKbps = m_par.mfx.MaxKbps = VP9E_DEFAULT_BITRATE * 10;

                EXPECT_EQ(!!temporal_layers_ext_params, true);
                temporal_layers_ext_params->Layer[0].FrameRateScale = 0;
                temporal_layers_ext_params->Layer[1].FrameRateScale = 0;
                temporal_layers_ext_params->Layer[2].FrameRateScale = 0;
                temporal_layers_ext_params->Layer[3].FrameRateScale = 0;

                // this is to disable super-frame headers check
                temporal_layers_ext_params = nullptr;

                m_LayerToCheck = -1;
            }
            else if (tc.type & CHECK_SWITCH_ON_TEMP_SCAL)
            {
                m_par.mfx.TargetKbps = m_par.mfx.MaxKbps = VP9E_DEFAULT_BITRATE * 5;

                m_par.AddExtBuffer(MFX_EXTBUFF_VP9_TEMPORAL_LAYERS, sizeof(mfxExtVP9TemporalLayers));
                mfxExtVP9TemporalLayers *temporal_layers_ext_params_ctrl = reinterpret_cast <mfxExtVP9TemporalLayers*>(m_par.GetExtBuffer(MFX_EXTBUFF_VP9_TEMPORAL_LAYERS));
                EXPECT_EQ(!!temporal_layers_ext_params_ctrl, true);
                temporal_layers_ext_params_ctrl->Layer[0].FrameRateScale = 1;
                temporal_layers_ext_params_ctrl->Layer[0].TargetKbps = VP9E_DEFAULT_BITRATE * 3;
                temporal_layers_ext_params_ctrl->Layer[1].FrameRateScale = 2;
                temporal_layers_ext_params_ctrl->Layer[1].TargetKbps = VP9E_DEFAULT_BITRATE * 5;

                EnumerateFramesWithLayers(tc.type);
            }
            else
            {
                EXPECT_EQ(!!temporal_layers_ext_params, true);
                temporal_layers_ext_params->Layer[0].TargetKbps *= 10;
                temporal_layers_ext_params->Layer[1].TargetKbps *= 10;
                m_par.mfx.TargetKbps = m_par.mfx.MaxKbps = temporal_layers_ext_params->Layer[1].TargetKbps;
            }


            g_tsStatus.expect(tc.sts);
            mfxStatus reset_status = Reset(m_session, m_pPar);
            g_tsLog << "Reset() returned with status " << reset_status << ", expected status " << tc.sts << MFX_ERR_NONE << "\n";
        }

        // SECOND ENCODE SECTION
        if ((tc.type & CHECK_RESET) && m_initialized)
        {
            reader->ResetFile();
            bs_checker.m_AllFramesSize = 0;

            TestSuite::EncodeFrames(default_frames_number, tc.type);
            g_tsStatus.check(DrainEncodedBitstream());

            mfxU32 frames_size_after_reset = bs_checker.m_AllFramesSize;

            for (mfxU32 i = 0; i < m_encoded_frame_sizes.size(); i++)
            {
                g_tsLog << "Frame[ " << i << "].encoded_size=" << m_encoded_frame_sizes[i] << "\n";
            }

            g_tsLog << "Stream size of the first part = " << frames_size << " bytes, the second part = " << frames_size_after_reset << " bytes\n";
            if (frames_size_after_reset < frames_size*1.5)
            {
                ADD_FAILURE() << "ERROR: Stream size of the second encoding part expected to be significantly bigger due to better quality, but it is not";
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }
        }

        if (m_initialized)
        {
            g_tsStatus.expect(MFX_ERR_NONE);
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_temporal_scalability,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_temporal_scalability, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_8b_444_ayuv_temporal_scalability,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_444_y410_temporal_scalability, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
};
