/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <mutex>
#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace vp9e_temporal_scalability
{
#define VP9E_MAX_TEMPORAL_LAYERS (4)
#define VP9E_DEFAULT_BITRATE (200)
#define VP9E_PSNR_THRESHOLD (20.0)
#define VP9E_MAX_FRAMES_IN_TEST_SEQUENCE (256)

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
        mfxU32 m_SourceWidth;
        mfxU32 m_SourceHeight;
        std::map<mfxU32, mfxU8 *> m_source_frames_y;
        std::map<mfxU32, mfxU32> m_encoded_frame_sizes;
        std::mutex  m_frames_storage_mtx;
        mfxI8 m_LayerNestingLevel[VP9E_MAX_FRAMES_IN_TEST_SEQUENCE];
        mfxI8 m_LayerToCheck;
        mfxU32 m_SourceFrameCount = 0;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_VP9)
            , m_RuntimeSettingsCtrl(0)
            , temporal_layers_ext_params(nullptr)
            , m_SourceWidth(0)
            , m_SourceHeight(0)
            , m_LayerToCheck(-1)
            , m_SourceFrameCount(0)
        {
            memset(m_LayerNestingLevel, -1, VP9E_MAX_FRAMES_IN_TEST_SEQUENCE);
        }
        ~TestSuite() {}
        int RunTest(unsigned int id);
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

        // query: incorrect 'FrameRateScale' for the 1st layer (should be bigger than for 0-layer)
        {/*01*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_QUERY,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE/2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // query: incorrect bitrate for the 1st layer (less than for 0-layer)
        {/*02*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_QUERY,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
            }
        },

        // query: too many layers - error
        {/*03*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_QUERY,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 6},
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE / 5 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].FrameRateScale, 3 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].TargetKbps, VP9E_DEFAULT_BITRATE / 4},
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[3].FrameRateScale, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[3].TargetKbps, VP9E_DEFAULT_BITRATE / 3 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[4].FrameRateScale, 5 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[4].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[5].FrameRateScale, 6 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[5].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // init: too many layers - error
        {/*04*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_INIT,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 6 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE / 5 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].FrameRateScale, 3 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].TargetKbps, VP9E_DEFAULT_BITRATE / 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[3].FrameRateScale, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[3].TargetKbps, VP9E_DEFAULT_BITRATE / 3 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[4].FrameRateScale, 5 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[4].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[5].FrameRateScale, 6 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[5].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // init + get_v_param + encode: correct params with 2 temporal layers (only mandatory params are set)
        {/*05*/ MFX_ERR_NONE, CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, (mfxU16)(VP9E_DEFAULT_BITRATE*1.5) },
            }
        },

        // init + get_v_param + encode: correct params with 2 temporal layers (some optional params are set)
        {/*06*/ MFX_ERR_NONE, CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE/2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // all routines: TargetKbps for 0-layer is not set - Error
        {/*07*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_INIT | CHECK_GET_V_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, 0 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, (mfxU16)(VP9E_DEFAULT_BITRATE*1.5) },
            }
        },

        // all routines: FrameRateScale for 0-layer is not set - Error
        {/*08*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_INIT | CHECK_GET_V_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 0 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, (mfxU16)(VP9E_DEFAULT_BITRATE*1.5) },
            }
        },

        // all routines: TargetKbps for 1st layer is less than for the 0st layer - Error
        {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_INIT | CHECK_GET_V_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, (mfxU16)(VP9E_DEFAULT_BITRATE*0.9) },
            }
        },

        // all routines: FrameRateScale for 2nd layer is wrong - Error
        {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_INIT | CHECK_GET_V_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, (mfxU16)(VP9E_DEFAULT_BITRATE*1.5) },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].TargetKbps, (mfxU16)(VP9E_DEFAULT_BITRATE*2) },
            }
        },

        // all routines: corner cases for the layers' values - should work
        {/*11*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 0xffff },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, 0xffff },
            }
        },

        // all routines: CQP-mode - should work
        {/*12*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, (mfxU16)(VP9E_DEFAULT_BITRATE*1.5) },
            }
        },

        // all routines: correct VBR-mode - should work
        {/*13*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE*2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, (mfxU16)(VP9E_DEFAULT_BITRATE*1.5) },
            }
        },

        // all routines: VBR-mode with incorrect MaxKbps - error
        {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, VP9E_DEFAULT_BITRATE / 2},
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, (mfxU16)(VP9E_DEFAULT_BITRATE*1.5) },
            }
        },

        // correct params: too small bitrates
        {/*15*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 10 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE / 5 },
            }
        },

        // correct params: too big bitrates
        {/*16*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, (0xffff) / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, 0xffff },
            }
        },

        // correct params: layers' bitrates should be updated with multiplier
        {/*17*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_GET_V_PARAM | CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: decode zero layer from a stream of 2 layers (TU=1)
        {/*18*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: decode zero layer from a stream of 2 layers (TU=4)
        {/*19*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: decode zero layer from a stream of 2 layers (TU=7)
        {/*20*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: decode zero layer from a stream of 2 layers (NumRefFrame=1)
        {/*21*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: decode zero layer from a stream of 2 layers (NumRefFrame=2)
        {/*22*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params: decode zero layer from a stream of 2 layers (NumRefFrame=3)
        {/*23*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // correct params, but 'NumRefFrame' is not set (should use default value)
        {/*24*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, (mfxU16)(VP9E_DEFAULT_BITRATE*1.5) },
            }
        },

        // correct params, CQP-mode
        {/*25*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
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
        {/*26*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_FIRST_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, (mfxU16)(VP9E_DEFAULT_BITRATE*1.5) },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].FrameRateScale, 4 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[2].TargetKbps, (mfxU16)(VP9E_DEFAULT_BITRATE*2) },
            }
        },

        // correct params: big RateScale
        {/*27*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 5 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, (mfxU16)(VP9E_DEFAULT_BITRATE*1.5) },
            }
        },

        // Temporal Scalability is set on frame level - unsupported, check error status after EncodeFrameAsync()
        {/*28*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_TEMP_SCAL_ON_FRAME,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // Cases below include 2 parts of encoding - it is expected the second part to have better quality (= bigger size of the encoded stream)

        // bitrate is being increased on reset
        {/*29*/ MFX_ERR_NONE, CHECK_RESET | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // Temporal Scalability is being switched off on reset
        {/*30*/ MFX_ERR_NONE, CHECK_RESET | CHECK_SWITCH_OFF_TEMP_SCAL | CHECK_ZERO_LAYER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, VP9E_DEFAULT_BITRATE * 10 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].FrameRateScale, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[0].TargetKbps, VP9E_DEFAULT_BITRATE / 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].FrameRateScale, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9TemporalLayers.Layer[1].TargetKbps, VP9E_DEFAULT_BITRATE },
            }
        },

        // Temporal Scalability is being activated on reset
        {/*31*/ MFX_ERR_NONE, CHECK_RESET | CHECK_SWITCH_ON_TEMP_SCAL | CHECK_ZERO_LAYER | CHECK_PSNR,
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
        bool m_CheckPsnr;
    public:
        BitstreamChecker(TestSuite *testPtr, bool check_psnr = false);
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
        mfxU32 m_AllFramesSize;
        mfxU32 m_DecodedFramesCount;
        bool m_DecoderInited;
        mfxU32 m_ChunkCount;
    };

    BitstreamChecker::BitstreamChecker(TestSuite *testPtr, bool check_psnr)
        : tsVideoDecoder(MFX_CODEC_VP9)
        , m_TestPtr(testPtr)
        , m_CheckPsnr(check_psnr)
        , m_AllFramesSize(0)
        , m_DecodedFramesCount(0)
        , m_DecoderInited(false)
        , m_ChunkCount(0)
    {}

    mfxF64 PSNR_Y_8bit(mfxU8 *ref, mfxU8 *src, mfxU32 length)
    {
        mfxF64 max = (1 << 8) - 1;
        mfxI32 diff = 0;
        mfxU64 dist = 0;

        for (mfxU32 y = 0; y < length; y++)
        {
            diff = ref[y] - src[y];
            dist += (diff * diff);
        }

        return (10. * log10(max * max * ((mfxF64)length / dist)));
    }

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
                                // Let's check second frame
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
                                ADD_FAILURE() << "ERROR: Invalid superframe index [headers in the beginning and in the end do not match]"; throw tsFAIL;
                            }
                        }
                        else
                        {
                            ADD_FAILURE() << "ERROR: Index size calculated as " << index_size << " bytes, but whole chunk is only " << bs.DataLength << " bytes"; throw tsFAIL;
                        }
                    }
                    else
                    {
                        ADD_FAILURE() << "ERROR: Superframe index is not found in the end of the chunk"; throw tsFAIL;
                    }

                    if (hdr.uh.show_frame != 0)
                    {
                        ADD_FAILURE() << "ERROR: first frame in the superframe expected show_frame=false, but it is not"; throw tsFAIL;
                    }
                }
                else
                {
                    // for other frames superframe syntax is not expected
                    const mfxU8 superframe_header = bs.Data[bs.DataLength - 1];
                    const mfxU32 superframe_marker = superframe_header & 0xe0;

                    if (superframe_marker == 0xc0)
                    {
                        ADD_FAILURE() << "ERROR: superframe index marker is found at a frame in the middle of the layer sequence"; throw tsFAIL;
                    }
                }
            }

            // do the decoder initialisation on the first encoded frame
            if (m_DecodedFramesCount == 0 && m_CheckPsnr)
            {
                const mfxU32 headers_shift = 12/*header*/ + (m_DecodedFramesCount == 0 ? 32/*ivf_header*/ : 0);
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
                    // This is a workaround for the decoder (there is an issue with NumFrameSuggested and decoding superframes)
                    m_request.NumFrameMin = 5;
                    m_request.NumFrameSuggested = 10;

                    mfxStatus alloc_status = tsSurfacePool::AllocSurfaces(m_request, !m_use_memid);
                    if (alloc_status >= 0)
                    {
                        m_DecoderInited = true;
                    }
                    else
                    {
                        g_tsLog << "WARNING: Could not allocate surfaces for the decoder, status " << alloc_status;
                    }
                }
                else
                {
                    g_tsLog << "WARNING: Could not inilialize the decoder, Init() returned " << init_status;
                }
            }

            if (m_DecoderInited)
            {
                if (m_TestPtr->m_LayerToCheck != -1 && m_TestPtr->m_LayerNestingLevel[m_ChunkCount] > m_TestPtr->m_LayerToCheck)
                {
                    g_tsLog << "INFO: Frame " << m_ChunkCount << " is not sending to the decoder cause layer " << m_TestPtr->m_LayerToCheck << " is being checked\n";
                }
                else
                {
                    const mfxU32 headers_shift = 12/*header*/ + (m_DecodedFramesCount == 0 ? 32/*ivf_header*/ : 0);
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
                        SyncOperation();

                        mfxU8 *ptr_decoded = new mfxU8[m_TestPtr->m_SourceWidth*m_TestPtr->m_SourceHeight];

                        for (mfxU32 i = 0; i < m_TestPtr->m_SourceHeight; i++)
                        {
                            memcpy(ptr_decoded + i*m_TestPtr->m_SourceWidth, m_pSurf->Data.Y + i*m_pSurf->Data.Pitch, m_TestPtr->m_SourceWidth);
                        }

                        m_TestPtr->m_frames_storage_mtx.lock();
                        mfxF64 psnr = PSNR_Y_8bit(m_TestPtr->m_source_frames_y[m_ChunkCount], ptr_decoded, m_TestPtr->m_SourceWidth*m_TestPtr->m_SourceHeight);

                        // old source frames are not needed anymore
                        for (mfxU32 i = 0; i <= m_ChunkCount; i++)
                        {
                            if (m_TestPtr->m_source_frames_y[m_ChunkCount])
                            {
                                delete[]m_TestPtr->m_source_frames_y[m_ChunkCount];
                                m_TestPtr->m_source_frames_y[m_ChunkCount] = 0;
                            }
                        }

                        m_TestPtr->m_frames_storage_mtx.unlock();

                        if (psnr < VP9E_PSNR_THRESHOLD)
                        {
                            ADD_FAILURE() << "ERROR: Y-PSNR of the decoded frame is too low: " << psnr << ", threshold is " << VP9E_PSNR_THRESHOLD; throw tsFAIL;
                        }
                        else
                        {
                            g_tsLog << "INFO: Y-PSNR for the decoded frame is " << psnr << "\n";
                        }
                    }

                    m_DecodedFramesCount++;
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

            // save source y-surface for checking PSNR if necessary
            if (flags & CHECK_PSNR)
            {
                mfxU8 *ptr = new mfxU8[m_SourceWidth*m_SourceHeight];
                for (mfxU32 i = 0; i < m_SourceHeight; i++)
                {
                    memcpy(ptr + i*m_SourceWidth, m_pSurf->Data.Y + i*m_pSurf->Data.Pitch, m_SourceWidth);
                }

                m_frames_storage_mtx.lock();
                m_source_frames_y[m_SourceFrameCount] = ptr;
                m_frames_storage_mtx.unlock();
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
            // let's match each encoded frame with the layer it is nested
            mfxU32 counter = 0;
            while (true)
            {
                m_LayerNestingLevel[counter++] = 0;
                if (counter >= VP9E_MAX_FRAMES_IN_TEST_SEQUENCE)
                {
                    break;
                }
                for (mfxU32 i = 1; i < VP9E_MAX_TEMPORAL_LAYERS; i++)
                {
                    if (temporal_layers_ext_params->Layer[i].FrameRateScale)
                    {
                        for (mfxU16 m = 0; m < temporal_layers_ext_params->Layer[i].FrameRateScale - temporal_layers_ext_params->Layer[i - 1].FrameRateScale; m++)
                        {
                            m_LayerNestingLevel[counter++] = i;
                            if (counter >= VP9E_MAX_FRAMES_IN_TEST_SEQUENCE)
                            {
                                break;
                            }
                        }
                    }
                }
                if (counter >= VP9E_MAX_FRAMES_IN_TEST_SEQUENCE)
                {
                    break;
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

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];

        const char* stream = g_tsStreamPool.Get("YUV/salesman_176x144_449.yuv");

        g_tsStreamPool.Reg();
        tsRawReader *reader = nullptr;

        const mfxU32 default_frames_number = 10;

        MFXInit();
        Load();

        //set default params
        m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = m_SourceWidth = 176;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = m_SourceHeight = 144;
        m_par.mfx.InitialDelayInKB = m_par.mfx.TargetKbps = m_par.mfx.MaxKbps = 0;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;

        BitstreamChecker bs_checker(this, tc.type & CHECK_PSNR ? true : false);
        m_bs_processor = &bs_checker;

        SETPARS(m_par, MFX_PAR);

        if (tc.type & CHECK_TEMP_SCAL_ON_FRAME)
        {
            m_RuntimeSettingsCtrl = 1;
        }

        InitAndSetAllocator();

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            if (g_tsHWtype < MFX_HW_CNL) // unsupported on platform less CNL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                return 0;
            }
        }  else {
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

            TRACE_FUNC3(MFXVideoENCODE_Query, m_session, m_pPar, &par_query_out);
            mfxStatus query_result_status = MFXVideoENCODE_Query(m_session, m_pPar, &par_query_out);

            if (!TemporalLayersParamsChecker("Query", m_par, par_query_out))
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
            reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
            m_filler = reader;

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
                EXPECT_EQ(!!temporal_layers_ext_params, true);
                temporal_layers_ext_params->Layer[0].FrameRateScale = 0;
                temporal_layers_ext_params->Layer[1].FrameRateScale = 0;
                temporal_layers_ext_params->Layer[2].FrameRateScale = 0;
                temporal_layers_ext_params->Layer[3].FrameRateScale = 0;
            }
            else if (tc.type & CHECK_SWITCH_ON_TEMP_SCAL)
            {
                m_par.mfx.TargetKbps = 0;

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

    TS_REG_TEST_SUITE_CLASS(vp9e_temporal_scalability);
};
