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

namespace vp9e_segmentation
{
#define VP9E_MAX_SUPPORTED_SEGMENTS (8)
#define VP9E_MAX_POSITIVE_Q_INDEX_DELTA (255)
#define VP9E_MAX_NEGATIVE_Q_INDEX_DELTA (-255)
#define VP9E_MAX_POSITIVE_LF_DELTA (63)
#define VP9E_MAX_NEGATIVE_LF_DELTA (-63)
#define VP9E_MAGIC_NUM_WRONG_SIZE (11)
#define VP9E_PSNR_THRESHOLD (20.0)
#define VP9E_DEFAULT_ENCODING_SEQUENCE_COUNT (3)

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
        CHECK_RESET_WITH_SEG_BEFORE_ONLY = 0x1 << 5,
        CHECK_RESET_WITH_SEG_AFTER_ONLY = 0x1 << 6,
        CHECK_NO_BUFFER = 0x1 << 7,
        CHECK_SEGM_ON_FRAME = 0x1 << 8,
        CHECK_INVALID_SEG_INDEX_IN_MAP = 0x1 << 9,
        CHECK_PSNR = 0x1 << 10,
        CHECK_SEGM_OUT_OF_RANGE_PARAMS = 0x1 << 11,
        CHECK_SEGM_DISABLE_FOR_FRAME = 0x1 << 12,
        CHECK_RESET_NO_SEGM_EXT_BUFFER = 0x1 << 13,
        CHECK_SECOND_ENCODING_STREAM_IS_BIGGER = 0x1 << 14,
        CHECK_STRESS_TEST = 0x1 << 15,
        CHECK_STRESS_SCENARIO_1 = 0x1 << 16,
        CHECK_STRESS_SCENARIO_2 = 0x1 << 17,
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

    enum
    {
        SEGMENTATION_DISABLED = 0,
        SEGMENTATION_WITHOUT_UPDATES = 0x1,
        SEGMENTATION_MAP_UPDATE = 0x1 << 1,
        SEGMENTATION_DATA_UPDATE = 0x1 << 2,
    };
    enum
    {
        CHANGE_TYPE_NONE = 0,
        CHANGE_TYPE_INIT = 0x1,
        CHANGE_TYPE_RESET = 0x1 << 1,
        CHANGE_TYPE_RUNTIME_PARAMS = 0x1 << 2,
    };


#define VP9_SET_RESET_PARAMS(p, type, step)                                                                                  \
for(mfxU32 i = 0; i < MAX_NPARS; i++)                                                                                        \
{                                                                                                                            \
    if(m_StressTestScenario[step].reset_params[i].f && m_StressTestScenario[step].reset_params[i].ext_type == type)          \
    {                                                                                                                        \
        SetParam(p, m_StressTestScenario[step].reset_params[i].f->name, m_StressTestScenario[step].reset_params[i].f->offset,\
                          m_StressTestScenario[step].reset_params[i].f->size, m_StressTestScenario[step].reset_params[i].v); \
    }                                                                                                                        \
}

    typedef std::map</*frame_num*/mfxU32, /*params*/mfxExtVP9Segmentation> runtime_segmentation_params;

    struct test_iteration
    {
        mfxU32 encoding_count;

        struct reset_params_str
        {
            mfxU32 ext_type;
            const tsStruct::Field* f;
            mfxU32 v;
        } reset_params[MAX_NPARS];

        mfxU32 segmentation_type_expected;

        runtime_segmentation_params runtime_params;

        test_iteration()
            : encoding_count(VP9E_DEFAULT_ENCODING_SEQUENCE_COUNT)
            , segmentation_type_expected(SEGMENTATION_DISABLED)
        {}
    };

    struct FrameStatStruct : public mfxExtVP9Segmentation
    {
        mfxU32 type;
        mfxU32 encoded_size;
        double psnr;
        mfxU32 change_type;
        mfxExtVP9Segmentation segm_params;
        FrameStatStruct()
            : type(SEGMENTATION_DISABLED)
            , encoded_size(0)
            , psnr(0.0)
            , change_type(CHANGE_TYPE_NONE)
        {
            memset(&segm_params, 0, sizeof(mfxExtVP9Segmentation));
        }
    };

    class TestSuite : tsVideoEncoder, BS_VP9_parser
    {
    public:
        static const unsigned int n_cases;
        mfxExtVP9Segmentation *m_InitedSegmentExtParams;
        mfxU32 m_SourceWidth;
        mfxU32 m_SourceHeight;
        std::map<mfxU32, mfxU8 *> m_source_frames_y;
        std::mutex  m_frames_storage_mtx;
        std::map<mfxU32, FrameStatStruct> m_EncodedFramesStat;
        std::vector<test_iteration> m_StressTestScenario;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_VP9)
            , m_InitedSegmentExtParams(nullptr)
            , m_SourceWidth(0)
            , m_SourceHeight(0)
            , m_SourceFrameCount(0)
        {}
        ~TestSuite()
        {
            // let's print frames info in the very end
            PrintFramesInfo();

            if (m_InitedSegmentExtParams && m_InitedSegmentExtParams->SegmentId)
            {
                delete[]m_InitedSegmentExtParams->SegmentId;
            }
        }
        int RunTest(unsigned int id);

        mfxVideoParam const & GetEncodingParams() { return m_par; }

    private:
        void EncodingCycle(const tc_struct& tc, const mfxU32& frames_count);
        void PrintFramesInfo();
        mfxStatus EncodeFrames(mfxU32 n, mfxU32 flags = 0, const runtime_segmentation_params *runtime_params = nullptr);
        mfxStatus AllocateAndSetMap(mfxExtVP9Segmentation &segment_ext_params, mfxU32 is_check_no_buffer = 0);
        static const tc_struct test_case[];
        mfxU32 m_SourceFrameCount;
    };

    const tc_struct TestSuite::test_case[] =
    {
        // query: correct segmentation params with 32x32 block and 1 segment
        {/*00*/ MFX_ERR_NONE, CHECK_QUERY,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[0].QIndexDelta, 1 },
            }
        },

        // init + get_v_param: correct segmentation params with 32x32 block and 1 segment
        {/*01*/ MFX_ERR_NONE, CHECK_GET_V_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[0].QIndexDelta, 1 },
            }
        },

        // encode: correct segmentation params with 32x32 block and 1 segment
        {/*02*/ MFX_ERR_NONE, CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[0].QIndexDelta, 1 },
            }
        },

        // all routines: correct segmentation params with 2 segments and QIndexDelta for both segments
        {/*03*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[0].QIndexDelta, 5 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, (mfxU32)(-10) },
            }
        },

        // all routines: correct segmentation params with 2 segments and QIndexDelta for the first segment only
        {/*04*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[0].QIndexDelta, 15 },
            }
        },

        // all routines: correct segmentation params with 2 segments and QIndexDelta for the second segment only
        {/*05*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 50 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, (mfxU32)(-10) },
            }
        },

        // all routines: correct segmentation params with 2 segments and LoopFilterLevelDelta for the second segment only
        {/*06*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].LoopFilterLevelDelta, (mfxU32)(-15) },
            }
        },

        // all routines: check ReferenceFrame feature for the segment
        // currently the feature is not supported by the driver - so check warning-status
        {/*07*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].ReferenceFrame, MFX_VP9_REF_LAST },
            }
        },

        // all routines: check Skip feature for the segment
        // currently the feature is not supported by the driver - so check warning-status
        {/*08*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].FeatureEnabled, MFX_VP9_SEGMENT_FEATURE_SKIP },
            }
        },

        // all routines: correct segmentation params with values for 8 segments
        {/*09*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 8 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[0].QIndexDelta, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 5 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[2].QIndexDelta, (mfxU32)(-5) },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[3].LoopFilterLevelDelta, 10 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[4].QIndexDelta, 10 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[5].LoopFilterLevelDelta, (mfxU32)(-10) },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[6].QIndexDelta, 10 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[7].QIndexDelta, (mfxU32)(-10) },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[7].LoopFilterLevelDelta, 10 },
            }
        },

        // all routines: 64x64 block - ok
        {/*10*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_64x64 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 1 },
            }
        },

        // all routines: 8x8 block - error (not supported now)
        {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments,2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_8x8 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 1 },
            }
        },

        // all routines: 16x16 block - error (not supported now)
        {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_16x16 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 1 },
            }
        },

        // query: map-buffer is null, but other params are correct - OK-status on Query (Query doesn't check null-params)
        {/*13*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_NO_BUFFER,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 1 },
            }
        },

        // init + get_v_param: map-buffer is null, but other params are correct - error (Init checks all params)
        {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_GET_V_PARAM | CHECK_NO_BUFFER,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 1 },
            }
        },

        // query: map-buffer and NumSegmentIdAlloc have small sizes - error
        {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_QUERY,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegmentIdAlloc, VP9E_MAGIC_NUM_WRONG_SIZE }
            }
        },

        // init + get_v_param: map-buffer and NumSegmentIdAlloc have small sizes - error
        {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_GET_V_PARAM | CHECK_NO_BUFFER,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegmentIdAlloc, VP9E_MAGIC_NUM_WRONG_SIZE }
            }
        },

        // all routines: too big NumSegments - error
        {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, VP9E_MAX_SUPPORTED_SEGMENTS + 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
            }
        },

        // all routines: invalid seg. index in the map (value more than VP9E_MAX_SUPPORTED_SEGMENTS) - error
        {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_INVALID_SEG_INDEX_IN_MAP,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
            }
        },

        // query: correct 'NumSegments' passes checks
        {/*19*/ MFX_ERR_NONE, CHECK_QUERY,
            { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 1 },
        },

        // init: mandatory parameter SegmentIdBlockSize is not set - error
        {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, CHECK_INIT,
            { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 1 },
        },

        // all routines: MBBRC with segmentation - warning and MBBRC should be switched off by Init
        {/*21*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY | CHECK_GET_V_PARAM | CHECK_GET_V_PARAM,
            {
                { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON },
                { CDO2_PAR, &tsStruct::mfxExtCodingOption2.FixedFrameRate, 29 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
            }
        },

        // query: q_index and loop_filter delta-s are out of range - warning status + values should be corrected
        {/*22*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[0].QIndexDelta, VP9E_MAX_POSITIVE_Q_INDEX_DELTA + 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[0].LoopFilterLevelDelta, VP9E_MAX_POSITIVE_LF_DELTA + 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, (mfxU32)(VP9E_MAX_NEGATIVE_Q_INDEX_DELTA - 1) },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].LoopFilterLevelDelta, (mfxU32)(VP9E_MAX_NEGATIVE_LF_DELTA - 1) },
            }
        },

        // init + get_v_param: q_index and loop_filter delta-s out of range - warning status + values should be corrected
        {/*23*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_GET_V_PARAM,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[0].QIndexDelta, VP9E_MAX_POSITIVE_Q_INDEX_DELTA + 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[0].LoopFilterLevelDelta, VP9E_MAX_POSITIVE_LF_DELTA + 1 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, (mfxU32)(VP9E_MAX_NEGATIVE_Q_INDEX_DELTA - 1) },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].LoopFilterLevelDelta, (mfxU32)(VP9E_MAX_NEGATIVE_LF_DELTA - 1) },
            }
        },

        // CASES BELOW DO STREAM DECODING AND CALCULATION OF PSNR

        // all routines: base-QP + QIndexDelta give out-of-range positive value combined - expected QIndexDelta correction
        {/*24*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 200 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 200 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 56 },
            }
        },

        // all routines: base-QP + QIndexDelta give out-of-range negative value combined - expected QIndexDelta correction
        {/*25*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 50 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, (mfxU32)(-51) },
            }
        },

        // all routines: BRC=CBR with correct bitrate and correct QIndexDelta on segmentation
        {/*26*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 5 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 500 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 500 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
            }
        },

        // all routines: BRC=CBR with high bitrate and big negative QP_delta on segmentation
        {/*27*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, (mfxU32)(-100) },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 1000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 1000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
            }
        },

        // all routines: BRC=CBR with low bitrate and big positive QP_delta on segmentation
        {/*28*/ MFX_ERR_NONE, CHECK_QUERY | CHECK_INIT | CHECK_GET_V_PARAM | CHECK_ENCODE | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 100 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 50 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 50 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.BufferSizeInKB, 0 },
            }
        },

        // check runtime segmentation set with disabled global segmentation
        {/*29*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_SEGM_ON_FRAME | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 200 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 200 },
            }
        },

        // check runtime segmentation set with enabled global segmentation
        {/*30*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_SEGM_ON_FRAME | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 100 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 100 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].LoopFilterLevelDelta, 50 },
            }
        },

        // check runtime segmentation with out-of-range params (global segmentation disabled)
        //  expected MFX_WRN_INCOMPATIBLE_VIDEO_PARAM on EncodeFrameAsync()
        {/*31*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_SEGM_ON_FRAME | CHECK_SEGM_OUT_OF_RANGE_PARAMS | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 200 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 200 },
            }
        },

        // check disabling segmentation in runtime for one frame
        {/*32*/ MFX_ERR_NONE, CHECK_ENCODE | CHECK_SEGM_ON_FRAME | CHECK_SEGM_DISABLE_FOR_FRAME | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 100 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 100 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].LoopFilterLevelDelta, 50 },
            }
        },

        // check changing segmentation params on Reset
        {/*33*/ MFX_ERR_NONE, CHECK_RESET | CHECK_RESET_NO_SEGM_EXT_BUFFER | CHECK_PSNR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 150 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 150 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 20 },
            }
        },

        // CASES BELOW INCLUDE 2 ENCODING PARTS - IT IS EXPECTED THE SECOND PART TO ENCODE IN BETTER QUALITY (=> BIGGER SIZE)

        // check Reset with segmentation enabling after reset only
        {/*34*/ MFX_ERR_NONE, CHECK_RESET_WITH_SEG_AFTER_ONLY | CHECK_SECOND_ENCODING_STREAM_IS_BIGGER,
            {
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, (mfxU32)(-150) },
            }
        },

        // check Reset with segmentation enabled before reset only
        {/*35*/ MFX_ERR_NONE, CHECK_RESET_WITH_SEG_BEFORE_ONLY | CHECK_SECOND_ENCODING_STREAM_IS_BIGGER,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 10 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 10 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 240 },
            }
        },

        // check Reset with changing segmentation's params on reset
        {/*36*/ MFX_ERR_NONE, CHECK_RESET | CHECK_SECOND_ENCODING_STREAM_IS_BIGGER,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 128 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 128 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 2 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 },
                { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[1].QIndexDelta, 100 },
            }
        },

        // CASES BELOW DO SEGMENTATION STRESS-TESTING

        // Stress-test: Scenario #1 (see the script below in RunTest)
        {/*37*/ MFX_ERR_NONE, CHECK_STRESS_TEST | CHECK_STRESS_SCENARIO_1 | CHECK_PSNR, {} },

        // Stress-test: Scenario #2 (see the script below in RunTest)
        {/*38*/ MFX_ERR_NONE, CHECK_STRESS_TEST | CHECK_STRESS_SCENARIO_2 | CHECK_PSNR, {} },

    };

    const unsigned int TestSuite::n_cases = sizeof(test_case) / sizeof(tc_struct);

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserVP9, public tsVideoDecoder
    {
        TestSuite *m_TestPtr;
    public:
        BitstreamChecker(TestSuite *testPtr, bool check_psnr = false);
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
        mfxU32 m_AllFramesSize;
        mfxU32 m_DecodedFramesCount;
        bool m_DecoderInited;
    };

    BitstreamChecker::BitstreamChecker(TestSuite *testPtr, bool check_psnr)
        : tsVideoDecoder(MFX_CODEC_VP9)
        , m_TestPtr(testPtr)
        , m_AllFramesSize(0)
        , m_DecodedFramesCount(0)
        , m_DecoderInited(false)
    {
        if (check_psnr)
        {
            m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_pPar->mfx.FrameInfo.Width = m_TestPtr->m_SourceWidth;
            m_pPar->mfx.FrameInfo.Height = m_TestPtr->m_SourceHeight;
            m_pPar->AsyncDepth = 1;

            mfxStatus init_status = Init();
            m_par_set = true;
            if (init_status >= 0)
            {
                mfxStatus alloc_status = AllocSurfaces();
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
    }

    int sign_value(Bs16u sign)
    {
        if (sign == 1)
            return -1;
        else if (sign == 0)
            return 1;
        else
            return 0;
    }

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
        // Dump stream to file
        const int encoded_size = bs.DataLength;
        static FILE *fp_vp9 = fopen("vp9e_encoded_segmentation.vp9", "wb");
        fwrite(bs.Data, encoded_size, 1, fp_vp9);
        fflush(fp_vp9);
        */

        while (checked++ < nFrames)
        {
            tsParserVP9::UnitType& hdr = ParseOrDie();

            if (m_TestPtr)
            {
                m_TestPtr->m_EncodedFramesStat[m_DecodedFramesCount].encoded_size = bs.DataLength;
                if (m_TestPtr->m_EncodedFramesStat[m_DecodedFramesCount].type > 0)
                {
                    if (!hdr.uh.segm.enabled)
                    {
                        ADD_FAILURE() << "ERROR: encoded_frame[" << m_DecodedFramesCount << "] expected to have segmentation, but uncompr_hdr.segm.enabled=false";
                        throw tsFAIL;
                    }
                    if (m_TestPtr->m_EncodedFramesStat[m_DecodedFramesCount].type & SEGMENTATION_DATA_UPDATE)
                    {
                        if (hdr.uh.segm.update_data == 0)
                        {
                            ADD_FAILURE() << "ERROR: encoded_frame[" << m_DecodedFramesCount
                                << "] expected to have segmentation data update, but uncompr_hdr.segm.update_data=false"; throw tsFAIL;
                        }

                        mfxExtVP9Segmentation *segment_params = &m_TestPtr->m_EncodedFramesStat[m_DecodedFramesCount].segm_params;

                        for (mfxU16 i = 0; i < segment_params->NumSegments; ++i)
                        {
                            if (segment_params->Segment[i].QIndexDelta)
                            {
                                if (hdr.uh.segm.feature[i][0].enabled != true)
                                {
                                    ADD_FAILURE() << "ERROR: mfxExtVP9Segmentation.Segment[" << i << "].QIndexDelta=" << segment_params->Segment[i].QIndexDelta
                                        << " but uncompr_hdr.segm.feature[" << i << "][0].enabled=" << hdr.uh.segm.feature[i][0].enabled; throw tsFAIL;
                                }

                                if ((segment_params->Segment[i].QIndexDelta > VP9E_MAX_POSITIVE_Q_INDEX_DELTA
                                    || segment_params->Segment[i].QIndexDelta < VP9E_MAX_NEGATIVE_Q_INDEX_DELTA)
                                    && hdr.uh.segm.feature[i][0].data != 0)
                                {
                                    ADD_FAILURE() << "ERROR: mfxExtVP9Segmentation.Segment[" << i << "].QIndexDelta=" << segment_params->Segment[i].QIndexDelta
                                        << " (out of range) but uncompr_hdr.segm.feature[" << i << "][0].data=" << hdr.uh.segm.feature[i][0].data << " ..sign="
                                        << hdr.uh.segm.feature[i][0].sign; throw tsFAIL;
                                }
                                else if (m_TestPtr->GetEncodingParams().mfx.RateControlMethod == MFX_RATECONTROL_CQP && segment_params->Segment[i].QIndexDelta + hdr.uh.quant.base_qindex > 255)
                                {
                                    if (hdr.uh.segm.feature[i][0].data*sign_value(hdr.uh.segm.feature[i][0].sign) + hdr.uh.quant.base_qindex != 255)
                                    {
                                        ADD_FAILURE() << "ERROR: Cutting QIndexDelta wrong:  mfxExtVP9Segmentation.Segment[" << i << "].QIndexDelta=" << segment_params->Segment[i].QIndexDelta
                                            << " base_qindex" << i << "][0].data=" << hdr.uh.quant.base_qindex << " uncompr_hdr.segm.feature[" << i << "][0].data="
                                            << hdr.uh.segm.feature[i][0].data << " ..sign=" << hdr.uh.segm.feature[i][0].sign; throw tsFAIL;
                                    }
                                }
                                else if (m_TestPtr->GetEncodingParams().mfx.RateControlMethod == MFX_RATECONTROL_CQP && segment_params->Segment[i].QIndexDelta + hdr.uh.quant.base_qindex < 1)
                                {
                                    if (hdr.uh.segm.feature[i][0].data*sign_value(hdr.uh.segm.feature[i][0].sign) + hdr.uh.quant.base_qindex != 1)
                                    {
                                        ADD_FAILURE() << "ERROR: Cutting QIndexDelta wrong:  mfxExtVP9Segmentation.Segment[" << i << "].QIndexDelta=" << segment_params->Segment[i].QIndexDelta
                                            << " base_qindex" << i << "][0].data=" << hdr.uh.quant.base_qindex << " uncompr_hdr.segm.feature[" << i << "][0].data="
                                            << hdr.uh.segm.feature[i][0].data << " ..sign=" << hdr.uh.segm.feature[i][0].sign; throw tsFAIL;
                                    }
                                }
                                else if (hdr.uh.segm.feature[i][0].data * sign_value(hdr.uh.segm.feature[i][0].sign) != segment_params->Segment[i].QIndexDelta)
                                {
                                    ADD_FAILURE() << "ERROR: mfxExtVP9Segmentation.Segment[" << i << "].QIndexDelta=" << segment_params->Segment[i].QIndexDelta
                                        << " but uncompr_hdr.segm.feature[" << i << "][0].data=" << hdr.uh.segm.feature[i][0].data << " ..sign="
                                        << hdr.uh.segm.feature[i][0].sign; throw tsFAIL;
                                }
                            }

                            if (segment_params->Segment[i].LoopFilterLevelDelta)
                            {
                                if (hdr.uh.segm.feature[i][1].enabled != true)
                                {
                                    ADD_FAILURE() << "ERROR: mfxExtVP9Segmentation.Segment[" << i << "].LoopFilterLevelDelta=" << segment_params->Segment[i].LoopFilterLevelDelta
                                        << " but uncompr_hdr.segm.feature[" << i << "][1].enabled=" << hdr.uh.segm.feature[i][1].enabled; throw tsFAIL;
                                }

                                if ((segment_params->Segment[i].LoopFilterLevelDelta > VP9E_MAX_POSITIVE_LF_DELTA
                                    || segment_params->Segment[i].LoopFilterLevelDelta < VP9E_MAX_NEGATIVE_LF_DELTA)
                                    && hdr.uh.segm.feature[i][0].data != 0)
                                {
                                    ADD_FAILURE() << "ERROR: mfxExtVP9Segmentation.Segment[" << i << "].LoopFilterLevelDelta=" << segment_params->Segment[i].LoopFilterLevelDelta
                                        << " (out of range) but uncompr_hdr.segm.feature[" << i << "][0].data=" << hdr.uh.segm.feature[i][0].data << " ..sign="
                                        << hdr.uh.segm.feature[i][0].sign; throw tsFAIL;
                                }
                                else if (hdr.uh.segm.feature[i][1].data * sign_value(hdr.uh.segm.feature[i][1].sign) != segment_params->Segment[i].LoopFilterLevelDelta)
                                {
                                    ADD_FAILURE() << "ERROR: mfxExtVP9Segmentation.Segment[" << i << "].LoopFilterLevelDelta=" << segment_params->Segment[i].LoopFilterLevelDelta
                                        << " but uncompr_hdr.segm.feature[" << i << "][1].data=" << hdr.uh.segm.feature[i][1].data << " ..sign="
                                        << hdr.uh.segm.feature[i][1].sign; throw tsFAIL;
                                }
                            }

                            if (segment_params->Segment[i].FeatureEnabled & MFX_VP9_SEGMENT_FEATURE_SKIP)
                            {
                                if (hdr.uh.segm.feature[i][3].enabled == true)
                                {
                                    ADD_FAILURE() << "ERROR: mfxExtVP9Segmentation.Segment[" << i << "].ReferenceAndSkipCtrl=MFX_VP9_RASCTRL_SKIP"
                                        << " but uncompr_hdr.segm.feature[" << i << "][3].enabled=" << hdr.uh.segm.feature[i][3].enabled << "(should be desibled)"; throw tsFAIL;
                                }
                            }

                            if (segment_params->Segment[i].FeatureEnabled & MFX_VP9_SEGMENT_FEATURE_REFERENCE)
                            {
                                if (hdr.uh.segm.feature[i][2].enabled == true)
                                {
                                    ADD_FAILURE() << "ERROR: mfxExtVP9Segmentation.Segment[" << i << "].ReferenceAndSkipCtrl=" << segment_params->Segment[i].ReferenceFrame
                                        << " but uncompr_hdr.segm.feature[" << i << "][2].enabled=" << hdr.uh.segm.feature[i][3].enabled << "(should be desibled)"; throw tsFAIL;
                                }
                            }
                        }
                    }

                    if (m_TestPtr->m_EncodedFramesStat[m_DecodedFramesCount].type & SEGMENTATION_MAP_UPDATE)
                    {
                        if (hdr.uh.segm.update_map == 0)
                        {
                            ADD_FAILURE() << "ERROR: encoded_frame[" << m_DecodedFramesCount
                                << "] expected to have segmentation map update, but uncompr_hdr.segm.update_map=false"; throw tsFAIL;
                        }
                    }

                    if (m_TestPtr->m_EncodedFramesStat[m_DecodedFramesCount].type & SEGMENTATION_WITHOUT_UPDATES)
                    {
                        if (hdr.uh.segm.update_map != 0 || hdr.uh.segm.update_data != 0)
                        {
                            ADD_FAILURE() << "ERROR: encoded_frame[" << m_DecodedFramesCount
                                << "] expected do not have segmentation updates, but uncompr_hdr.segm.update_map=" << (hdr.uh.segm.update_map ? 1 : 0)
                                << ", uncompr_hdr.segm.update_data=" << (hdr.uh.segm.update_data ? 1 : 0); throw tsFAIL;
                        }
                    }
                }
                else
                {
                    if (hdr.uh.segm.enabled != 0)
                    {
                        ADD_FAILURE() << "ERROR: encoded_frame[" << m_DecodedFramesCount
                            << "] expected do not have enabled segmentation, but uncompr_hdr.segm.enabled=true"; throw tsFAIL;
                    }
                }
            }

            if (m_DecoderInited)
            {
                const mfxU32 headers_shift = 12/*header*/ + (m_DecodedFramesCount == 0 ? 32/*ivf_header*/ : 0);
                m_pBitstream->Data = bs.Data + headers_shift;
                m_pBitstream->DataOffset = 0;
                m_pBitstream->DataLength = m_pBitstream->MaxLength = bs.DataLength - headers_shift;
                mfxStatus decode_status = DecodeFrameAsync();
                if (decode_status >= 0)
                {
                    mfxStatus sync_status = SyncOperation(*m_pSyncPoint);
                    if (sync_status >= 0)
                    {
                        mfxU8 *ptr_decoded = new mfxU8[m_TestPtr->m_SourceWidth*m_TestPtr->m_SourceHeight];
                        for (mfxU32 i = 0; i < m_TestPtr->m_SourceHeight; i++)
                        {
                            memcpy(ptr_decoded + i*m_TestPtr->m_SourceWidth, m_pSurf->Data.Y + i*m_pSurf->Data.Pitch, m_TestPtr->m_SourceWidth);
                        }

                        m_TestPtr->m_frames_storage_mtx.lock();
                        mfxF64 psnr = PSNR_Y_8bit(m_TestPtr->m_source_frames_y[m_DecodedFramesCount], ptr_decoded, m_TestPtr->m_SourceWidth*m_TestPtr->m_SourceHeight);

                        m_TestPtr->m_EncodedFramesStat[m_DecodedFramesCount].psnr = psnr;

                        // source frame is not needed anymore
                        delete[]m_TestPtr->m_source_frames_y[m_DecodedFramesCount];

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
                    else
                    {
                        ADD_FAILURE() << "ERROR: DecodeFrameAsync->SyncOperation() failed with status: " << sync_status; throw tsFAIL;
                    }
                }
                else
                {
                    ADD_FAILURE() << "ERROR: DecodeFrameAsync() failed with status: " << decode_status; throw tsFAIL;
                }
            }

            m_AllFramesSize += bs.DataLength;
            bs.DataLength = bs.DataOffset = 0;

            m_DecodedFramesCount++;
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    bool SegmentationParamsChecker(std::string caller_method_name, mfxExtVP9Segmentation &input, mfxExtVP9Segmentation &output)
    {
        if (input.NumSegments > VP9E_MAX_SUPPORTED_SEGMENTS && output.NumSegments != 0)
        {
            ADD_FAILURE() << "ERROR: " << caller_method_name << ": NumSegments_in=" << input.NumSegments << " NumSegments_out=" << output.NumSegments
                << " NumSegments_expected=" << 0;
            return false;
        }
        for (mfxU16 i = 0; i < input.NumSegments; ++i)
        {
            if (input.Segment[i].QIndexDelta > VP9E_MAX_POSITIVE_Q_INDEX_DELTA && output.Segment[i].QIndexDelta != 0)
            {
                ADD_FAILURE() << "ERROR: " << caller_method_name << ": QIndexDelta_in=" << input.Segment[i].QIndexDelta << " QIndexDelta_out=" << output.Segment[i].QIndexDelta
                    << " QIndexDelta_expected=" << 0;
                return false;
            }
            if (input.Segment[i].QIndexDelta < VP9E_MAX_NEGATIVE_Q_INDEX_DELTA && output.Segment[i].QIndexDelta != 0)
            {
                ADD_FAILURE() << "ERROR: " << caller_method_name << ": QIndexDelta_in=" << input.Segment[i].QIndexDelta << " QIndexDelta_out=" << output.Segment[i].QIndexDelta
                    << " QIndexDelta_expected=" << 0;
                return false;
            }

            if (input.Segment[i].LoopFilterLevelDelta > VP9E_MAX_POSITIVE_LF_DELTA && output.Segment[i].LoopFilterLevelDelta != 0)
            {
                ADD_FAILURE() << "ERROR: " << caller_method_name << ": LoopFilterLevelDelta_in=" << input.Segment[i].LoopFilterLevelDelta << " LoopFilterLevelDelta_out="
                    << output.Segment[i].LoopFilterLevelDelta << " LoopFilterLevelDelta_expected=" << 0;
                return false;
            }
            if (input.Segment[i].LoopFilterLevelDelta < VP9E_MAX_NEGATIVE_LF_DELTA && output.Segment[i].LoopFilterLevelDelta != 0)
            {
                ADD_FAILURE() << "ERROR: " << caller_method_name << ": LoopFilterLevelDelta_in=" << input.Segment[i].LoopFilterLevelDelta << " LoopFilterLevelDelta_out="
                    << output.Segment[i].LoopFilterLevelDelta << " LoopFilterLevelDelta_expected=" << 0;
                return false;
            }
        }

        return true;
    }

    // Routine just set FetaureEnable flag for non-null values
    mfxStatus SetFeatureEnable(mfxExtVP9Segmentation &segment_ext_params)
    {
        for (mfxU32 i = 0; i < VP9E_MAX_SUPPORTED_SEGMENTS; ++i)
        {
            if (segment_ext_params.Segment[i].QIndexDelta)
                segment_ext_params.Segment[i].FeatureEnabled = MFX_VP9_SEGMENT_FEATURE_QINDEX;
            if (segment_ext_params.Segment[i].LoopFilterLevelDelta)
                segment_ext_params.Segment[i].FeatureEnabled |= MFX_VP9_SEGMENT_FEATURE_LOOP_FILTER;
            if (segment_ext_params.Segment[i].ReferenceFrame)
                segment_ext_params.Segment[i].FeatureEnabled |= MFX_VP9_SEGMENT_FEATURE_REFERENCE;
        }

        return MFX_ERR_NONE;
    }

    mfxStatus TestSuite::EncodeFrames(mfxU32 n, mfxU32 flags, const runtime_segmentation_params *runtime_params)
    {
        mfxU32 encoded = 0;
        mfxU32 submitted = 0;
        mfxU32 async = TS_MAX(1, m_par.AsyncDepth);
        mfxSyncPoint sp = nullptr;
        mfxU32 cycle_count = 0;

        async = TS_MIN(n, async - 1);

        while (encoded < n)
        {
            mfxU32 is_frame_with_out_of_range_params = false;

            //clear extBuffer in the beginning
            m_ctrl.RemoveExtBuffer(MFX_EXTBUFF_VP9_SEGMENTATION);

            if (runtime_params && runtime_params->find(cycle_count) != runtime_params->end())
            {
                m_ctrl.AddExtBuffer(MFX_EXTBUFF_VP9_SEGMENTATION, sizeof(mfxExtVP9Segmentation));
                mfxExtVP9Segmentation *segment_ext_params_ctrl = reinterpret_cast <mfxExtVP9Segmentation*>(m_ctrl.GetExtBuffer(MFX_EXTBUFF_VP9_SEGMENTATION));
                EXPECT_EQ(!!segment_ext_params_ctrl, true);

                memcpy((char *)segment_ext_params_ctrl + sizeof(mfxExtBuffer), (char *)(&runtime_params->at(cycle_count)) + sizeof(mfxExtBuffer), sizeof(mfxExtVP9Segmentation) - sizeof(mfxExtBuffer));

                m_EncodedFramesStat[m_SourceFrameCount].type = SEGMENTATION_MAP_UPDATE | SEGMENTATION_DATA_UPDATE;
                m_EncodedFramesStat[m_SourceFrameCount].segm_params = *segment_ext_params_ctrl;
                m_EncodedFramesStat[m_SourceFrameCount].change_type = CHANGE_TYPE_RUNTIME_PARAMS;
            }
            else if ( flags & CHECK_SEGM_ON_FRAME && m_SourceFrameCount == 1)
            {
                g_tsStatus.expect(MFX_ERR_NONE);

                m_ctrl.AddExtBuffer(MFX_EXTBUFF_VP9_SEGMENTATION, sizeof(mfxExtVP9Segmentation));
                mfxExtVP9Segmentation *segment_ext_params_ctrl = reinterpret_cast <mfxExtVP9Segmentation*>(m_ctrl.GetExtBuffer(MFX_EXTBUFF_VP9_SEGMENTATION));
                EXPECT_EQ(!!segment_ext_params_ctrl, true);

                if (flags & CHECK_SEGM_DISABLE_FOR_FRAME)
                {
                    segment_ext_params_ctrl->NumSegments = 0;

                    m_EncodedFramesStat[m_SourceFrameCount].type = SEGMENTATION_DISABLED;
                    m_EncodedFramesStat[m_SourceFrameCount].change_type = CHANGE_TYPE_RUNTIME_PARAMS;
                }
                else
                {
                    segment_ext_params_ctrl->NumSegments = 3;
                    segment_ext_params_ctrl->SegmentIdBlockSize = MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32;
                    AllocateAndSetMap(*segment_ext_params_ctrl);

                    if (flags & CHECK_SEGM_OUT_OF_RANGE_PARAMS)
                    {
                        segment_ext_params_ctrl->Segment[0].QIndexDelta = VP9E_MAX_POSITIVE_Q_INDEX_DELTA * 2;
                        segment_ext_params_ctrl->Segment[1].LoopFilterLevelDelta = VP9E_MAX_NEGATIVE_LF_DELTA * 2;

                        is_frame_with_out_of_range_params = true;
                    }
                    else
                    {
                        segment_ext_params_ctrl->Segment[1].QIndexDelta = -50;
                    }

                    m_EncodedFramesStat[m_SourceFrameCount].type = SEGMENTATION_MAP_UPDATE | SEGMENTATION_DATA_UPDATE;
                    m_EncodedFramesStat[m_SourceFrameCount].segm_params = *segment_ext_params_ctrl;
                    m_EncodedFramesStat[m_SourceFrameCount].change_type = CHANGE_TYPE_RUNTIME_PARAMS;
                }

                SetFeatureEnable(*segment_ext_params_ctrl);
            }
            else
            {
                if (m_EncodedFramesStat.find(m_SourceFrameCount) == m_EncodedFramesStat.end())
                {
                    if (m_InitedSegmentExtParams == nullptr)
                    {
                        m_EncodedFramesStat[m_SourceFrameCount].type = SEGMENTATION_DISABLED;
                    }
                    else
                    {
                        m_EncodedFramesStat[m_SourceFrameCount].type = SEGMENTATION_WITHOUT_UPDATES;
                        if (m_SourceFrameCount > 0)
                        {
                            if (m_EncodedFramesStat[m_SourceFrameCount - 1].change_type == CHANGE_TYPE_RUNTIME_PARAMS)
                            {
                                m_EncodedFramesStat[m_SourceFrameCount].type = SEGMENTATION_MAP_UPDATE | SEGMENTATION_DATA_UPDATE;
                            }
                        }
                        if (m_EncodedFramesStat[m_SourceFrameCount].change_type > CHANGE_TYPE_INIT)
                        {
                            m_EncodedFramesStat[m_SourceFrameCount].type = SEGMENTATION_MAP_UPDATE | SEGMENTATION_DATA_UPDATE;
                        }
                    }
                }
            }

            cycle_count++;

            mfxStatus encode_status = EncodeFrameAsync();

            mfxU8 *ptr = new mfxU8[m_SourceWidth*m_SourceHeight];
            for (mfxU32 i = 0; i < m_SourceHeight; i++)
            {
                memcpy(ptr + i*m_SourceWidth, m_pSurf->Data.Y + i*m_pSurf->Data.Pitch, m_SourceWidth);
            }
            m_frames_storage_mtx.lock();
            m_source_frames_y[m_SourceFrameCount] = ptr;
            m_frames_storage_mtx.unlock();

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
            else if (is_frame_with_out_of_range_params && flags & CHECK_SEGM_OUT_OF_RANGE_PARAMS)
            {
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
                // for out-of-range segmentation params a warning expected at this point
                if (encode_status != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                {
                    ADD_FAILURE() << "ERROR: For out-of-range segmentation params MFX_WRN_INCOMPATIBLE_VIDEO_PARAM expected on EncodeFrameAsync(), but returned  "
                        << encode_status; throw tsFAIL;
                }
                else
                {
                    // correcting values for the future check of the encoded frame
                    m_EncodedFramesStat[m_SourceFrameCount-1].segm_params.Segment[0].QIndexDelta = 0;
                    m_EncodedFramesStat[m_SourceFrameCount-1].segm_params.Segment[1].LoopFilterLevelDelta = 0;

                    encode_status = MFX_ERR_NONE;
                }
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

    mfxStatus TestSuite::AllocateAndSetMap(mfxExtVP9Segmentation &segment_ext_params, mfxU32 is_check_no_buffer)
    {
        mfxU32 seg_block = 8;
        if (segment_ext_params.SegmentIdBlockSize == MFX_VP9_SEGMENT_ID_BLOCK_SIZE_16x16)
        {
            seg_block = 16;
        }
        else if (segment_ext_params.SegmentIdBlockSize == MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32)
        {
            seg_block = 32;
        }
        else if (segment_ext_params.SegmentIdBlockSize == MFX_VP9_SEGMENT_ID_BLOCK_SIZE_64x64)
        {
            seg_block = 64;
        }

        const mfxU32 map_width_qnt = (m_par.mfx.FrameInfo.Width + (seg_block - 1)) / seg_block;
        const mfxU32 map_height_qnt = (m_par.mfx.FrameInfo.Height + (seg_block - 1)) / seg_block;

        if (segment_ext_params.NumSegmentIdAlloc != VP9E_MAGIC_NUM_WRONG_SIZE)
        {
            segment_ext_params.NumSegmentIdAlloc = map_width_qnt * map_height_qnt;
        }

        if (is_check_no_buffer)
        {
            return MFX_ERR_NONE;
        }

        segment_ext_params.SegmentId = new mfxU8[segment_ext_params.NumSegmentIdAlloc];
        memset(segment_ext_params.SegmentId, 0, segment_ext_params.NumSegmentIdAlloc);

        if (segment_ext_params.NumSegmentIdAlloc == VP9E_MAGIC_NUM_WRONG_SIZE)
        {
            return MFX_ERR_NONE;
        }

        if (segment_ext_params.NumSegments == 2)
        {
            // for two segment let the second be half of a frame to affect radically size/quality for checks
            for (mfxU32 i = 0; i < map_width_qnt * map_height_qnt * 1 / 2; ++i)
            {
                segment_ext_params.SegmentId[i] = 1;
            }
        }
        else if (segment_ext_params.NumSegments > 2)
        {
            // for many segments let them cover all frame
            const mfxU32 blocks_qnt_for_one_seg = map_width_qnt * map_height_qnt / segment_ext_params.NumSegments;
            mfxU32 seg_counter = 0;
            for (mfxU32 i = 0; i < map_width_qnt * map_height_qnt; ++i)
            {
                segment_ext_params.SegmentId[i] = seg_counter >= segment_ext_params.NumSegments ? (segment_ext_params.NumSegments - 1) : seg_counter;
                if (i!= 0 && i%blocks_qnt_for_one_seg == 0)
                {
                    seg_counter++;
                }
            }
        }

        return MFX_ERR_NONE;
    }

    void TestSuite::PrintFramesInfo()
    {
        g_tsLog << "\n INFO: FRAMES STATISTICS (encoded " << m_EncodedFramesStat.size() << " frames)\n";
        for (auto const &iter : m_EncodedFramesStat)
        {
            std::string segm_status_text = "SEGM_DISABLED";
            if (iter.second.type == 1)
            {
                segm_status_text = "SEGM_NO_UPDATE";
            }
            else if (iter.second.type & SEGMENTATION_MAP_UPDATE && iter.second.type & SEGMENTATION_DATA_UPDATE)
            {
                segm_status_text = "SEGM_MAP_DATA_UPDATE";
            }
            else if (iter.second.type & SEGMENTATION_MAP_UPDATE)
            {
                segm_status_text = "SEGM_MAP_UPDATE";
            }
            else if (iter.second.type & SEGMENTATION_DATA_UPDATE)
            {
                segm_status_text = "SEGM_DATA_UPDATE";
            }

            std::string change_type_status = "NO_CHANGE";
            if (iter.second.change_type == CHANGE_TYPE_INIT)
            {
                change_type_status = "CHANGE_BY_INIT";
            }
            else if (iter.second.change_type == CHANGE_TYPE_RESET)
            {
                change_type_status = "CHANGE_BY_RESET";
            }
            else if (iter.second.change_type == CHANGE_TYPE_RUNTIME_PARAMS)
            {
                change_type_status = "CHANGE_BY_RUNTIME_PARAMS";
            }

            g_tsLog << "encoded_frame[" << iter.first << "|" << change_type_status << "].segmentation_status=" << segm_status_text << ", size=" << iter.second.encoded_size << ", psnr=" << iter.second.psnr << "\n";
            if (iter.second.change_type > CHANGE_TYPE_NONE)
            {
                g_tsLog << "             num_segments=" << iter.second.segm_params.NumSegments << " block_size=" << iter.second.segm_params.SegmentIdBlockSize
                    << " map_ptr=" << iter.second.segm_params.SegmentId << "\n";
            }
        }
    }

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];

        const char* stream = g_tsStreamPool.Get("YUV/salesman_176x144_449.yuv");

        g_tsStreamPool.Reg();
        tsRawReader *reader = nullptr;

        mfxU32 m_EncodingSequenceCount = VP9E_DEFAULT_ENCODING_SEQUENCE_COUNT;

        MFXInit();
        Load();

        //set default params
        m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = m_SourceWidth = 176;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = m_SourceHeight = 144;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        m_par.mfx.QPI = m_par.mfx.QPP = 50;

        BitstreamChecker bs_checker(this, tc.type & CHECK_PSNR ? true : false);
        m_bs_processor = &bs_checker;

        if (!(tc.type & CHECK_RESET_WITH_SEG_AFTER_ONLY))
        {
            SETPARS(m_par, MFX_PAR);
            SETPARS(m_par, CDO2_PAR);
        }
        else
        {
            m_par.mfx.QPI = m_par.mfx.QPP = 200;
        }

        m_InitedSegmentExtParams = reinterpret_cast <mfxExtVP9Segmentation*>(m_par.GetExtBuffer(MFX_EXTBUFF_VP9_SEGMENTATION));
        if (m_InitedSegmentExtParams)
        {
            SetFeatureEnable(*m_InitedSegmentExtParams);
            AllocateAndSetMap(*m_InitedSegmentExtParams, tc.type & CHECK_NO_BUFFER ? 1 : 0);
            if (tc.type & CHECK_INVALID_SEG_INDEX_IN_MAP)
            {
                m_InitedSegmentExtParams->SegmentId[m_InitedSegmentExtParams->NumSegmentIdAlloc / 2] = VP9E_MAX_SUPPORTED_SEGMENTS;
            }

            m_EncodedFramesStat[0].type = SEGMENTATION_MAP_UPDATE | SEGMENTATION_DATA_UPDATE;
            m_EncodedFramesStat[0].segm_params = *m_InitedSegmentExtParams;
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

        if (tc.type & CHECK_STRESS_SCENARIO_1)
        {
            test_iteration cycle1;
            cycle1.encoding_count = 5;
            cycle1.reset_params[0] = { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 1 };
            cycle1.reset_params[1] = { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32 };
            cycle1.reset_params[2] = { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.Segment[0].QIndexDelta, 100 };
            cycle1.segmentation_type_expected = SEGMENTATION_MAP_UPDATE | SEGMENTATION_DATA_UPDATE;

            mfxExtVP9Segmentation cycle1_runtime_params;
            memset(&cycle1_runtime_params, 0, sizeof(mfxExtVP9Segmentation));
            cycle1_runtime_params.NumSegments = 8;
            cycle1_runtime_params.SegmentIdBlockSize = MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32;
            AllocateAndSetMap(cycle1_runtime_params);
            cycle1_runtime_params.Segment[0].QIndexDelta = 10;
            cycle1_runtime_params.Segment[2].QIndexDelta = 15;
            SetFeatureEnable(cycle1_runtime_params);
            cycle1.runtime_params[3] = cycle1_runtime_params;

            m_StressTestScenario.push_back(cycle1);

            test_iteration cycle2;
            cycle2.encoding_count = 5;
            cycle2.reset_params[1] = { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.SegmentIdBlockSize, MFX_VP9_SEGMENT_ID_BLOCK_SIZE_64x64 };
            cycle2.segmentation_type_expected = SEGMENTATION_MAP_UPDATE;
            m_StressTestScenario.push_back(cycle2);

            test_iteration cycle3;
            cycle3.encoding_count = 5;
            cycle3.reset_params[0] = { MFX_PAR, &tsStruct::mfxExtVP9Segmentation.NumSegments, 3 };
            cycle3.segmentation_type_expected = SEGMENTATION_DATA_UPDATE;
            m_StressTestScenario.push_back(cycle3);
        }
        else if (tc.type & CHECK_STRESS_SCENARIO_2)
        {
            test_iteration cycle1;
            cycle1.encoding_count = 6;
            cycle1.segmentation_type_expected = SEGMENTATION_DISABLED;

            mfxExtVP9Segmentation cycle1_runtime_params;
            memset(&cycle1_runtime_params, 0, sizeof(mfxExtVP9Segmentation));
            cycle1_runtime_params.NumSegments = 1;
            cycle1_runtime_params.SegmentIdBlockSize = MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32;
            AllocateAndSetMap(cycle1_runtime_params);
            cycle1_runtime_params.Segment[0].QIndexDelta = 10;
            SetFeatureEnable(cycle1_runtime_params);
            cycle1.runtime_params[1] = cycle1_runtime_params;

            mfxExtVP9Segmentation cycle1_runtime_params_1;
            memset(&cycle1_runtime_params_1, 0, sizeof(mfxExtVP9Segmentation));
            cycle1_runtime_params_1.NumSegments = 2;
            cycle1_runtime_params_1.SegmentIdBlockSize = MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32;
            AllocateAndSetMap(cycle1_runtime_params_1);
            cycle1_runtime_params.Segment[0].QIndexDelta = 22;
            SetFeatureEnable(cycle1_runtime_params_1);
            cycle1.runtime_params[2] = cycle1_runtime_params_1;

            cycle1.runtime_params[4] = cycle1_runtime_params_1;

            m_StressTestScenario.push_back(cycle1);
        }

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

            TS_TRACE(m_session);
            TS_TRACE(m_pPar);
            g_tsLog << "NOW CALLING QUERY()...\n MFXVideoENCODE_Query(m_session, m_pPar, &par_query_out) \n";
            mfxStatus query_result_status = MFXVideoENCODE_Query(m_session, m_pPar, &par_query_out);
            g_tsLog << "QUERY() FINISHED WITH STATUS " << query_result_status << "\n";
            TS_TRACE(&par_query_out);
            mfxExtVP9Segmentation *segment_ext_params_query = reinterpret_cast <mfxExtVP9Segmentation*>(par_query_out.GetExtBuffer(MFX_EXTBUFF_VP9_SEGMENTATION));
            if (m_InitedSegmentExtParams != nullptr && segment_ext_params_query == nullptr)
            {
                ADD_FAILURE() << "ERROR: Query() does not have mfxExtVP9Segmentation in the output while input has one";
            }

            if (m_InitedSegmentExtParams && !SegmentationParamsChecker("Query", *m_InitedSegmentExtParams, *segment_ext_params_query))
            {
                ADD_FAILURE() << "ERROR: Query() parameters check failed!";
            }

            mfxExtCodingOption2 *codopt2_ext_params_query = reinterpret_cast <mfxExtCodingOption2*>(par_query_out.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2));
            if (codopt2_ext_params_query)
            {
                if (segment_ext_params_query != 0 && codopt2_ext_params_query->MBBRC == MFX_CODINGOPTION_ON)
                {
                    ADD_FAILURE() << "ERROR: Query() left mfxExtCodingOption2.MBBRC=MFX_CODINGOPTION_ON and segmentation";
                }
            }

            g_tsLog << "Query() returned with status " << query_result_status << ", expected status " << query_expect_status << "\n";
            g_tsStatus.check(query_result_status);
        }

        // INIT SECTION
        if (tc.type & CHECK_INIT || tc.type & CHECK_GET_V_PARAM || tc.type & CHECK_ENCODE || tc.type & CHECK_RESET
            || tc.type & CHECK_RESET_WITH_SEG_BEFORE_ONLY || tc.type & CHECK_RESET_WITH_SEG_AFTER_ONLY || tc.type & CHECK_STRESS_TEST)
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
            m_EncodedFramesStat[m_SourceFrameCount].change_type = CHANGE_TYPE_INIT;
            if (m_InitedSegmentExtParams)
            {
                m_EncodedFramesStat[m_SourceFrameCount].segm_params = *m_InitedSegmentExtParams;
                m_EncodedFramesStat[m_SourceFrameCount].type = SEGMENTATION_MAP_UPDATE | SEGMENTATION_DATA_UPDATE;
            }
        }

        // GET_VIDEO_PARAM SECTION
        if (tc.type & CHECK_GET_V_PARAM && m_initialized)
        {
            g_tsStatus.expect(MFX_ERR_NONE);

            tsExtBufType<mfxVideoParam> getvpar_out_par = m_par;
            mfxStatus getvparam_result_status = GetVideoParam(m_session, &getvpar_out_par);

            mfxExtVP9Segmentation *segment_ext_params_getvparam = reinterpret_cast <mfxExtVP9Segmentation*>(getvpar_out_par.GetExtBuffer(MFX_EXTBUFF_VP9_SEGMENTATION));
            if (m_InitedSegmentExtParams != nullptr && segment_ext_params_getvparam == nullptr)
            {
                ADD_FAILURE() << "ERROR: GetVideoParam() does not have mfxExtVP9Segmentation in the output while input has one";
            }

            if (m_InitedSegmentExtParams && !SegmentationParamsChecker("GetVideoParam", *m_InitedSegmentExtParams, *segment_ext_params_getvparam))
            {
                ADD_FAILURE() << "ERROR: GetVideoParam() check parameters failed!";
            }
            g_tsLog << "GetVideoParam() returned with status " << getvparam_result_status << ", expected status MFX_ERR_NONE\n";
            g_tsStatus.check(getvparam_result_status);
        }

        g_tsStatus.expect(MFX_ERR_NONE);

        mfxU32 frames_size = 0;

        // FIRST ENCODE SECTION
        if ((tc.type & CHECK_ENCODE || tc.type & CHECK_RESET || tc.type & CHECK_RESET_WITH_SEG_BEFORE_ONLY
            || tc.type & CHECK_RESET_WITH_SEG_AFTER_ONLY || tc.type & CHECK_STRESS_TEST) && m_initialized)
        {
            //set reader
            if (reader == nullptr)
            {
                reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
                m_filler = reader;
            }

            if (tc.sts >= MFX_ERR_NONE)
            {
                TestSuite::EncodeFrames(m_EncodingSequenceCount, tc.type);

                g_tsStatus.check(DrainEncodedBitstream());
                TS_CHECK_MFX;
                frames_size = bs_checker.m_AllFramesSize;
            }
        }

        const mfxU32 launch_steps = (mfxU32)(m_StressTestScenario.size() > 0 ? m_StressTestScenario.size() : 1);
        const mfxU32 setup_steps = m_StressTestScenario.size() > 0 ? 1 : 0;
        for (mfxU32 st = 0; st < launch_steps; ++st)
        {

            // RESET SECTION
            if ((tc.type & CHECK_RESET || tc.type & CHECK_RESET_WITH_SEG_BEFORE_ONLY
                || tc.type & CHECK_RESET_WITH_SEG_AFTER_ONLY || tc.type & CHECK_STRESS_TEST) && m_initialized)
            {
                tsExtBufType<mfxVideoParam> tmp_par;
                if (tc.type & CHECK_RESET_WITH_SEG_AFTER_ONLY)
                {
                    SETPARS(m_par, MFX_PAR);
                    m_InitedSegmentExtParams = reinterpret_cast <mfxExtVP9Segmentation*>(m_par.GetExtBuffer(MFX_EXTBUFF_VP9_SEGMENTATION));
                    if (m_InitedSegmentExtParams)
                    {
                        SetFeatureEnable(*m_InitedSegmentExtParams);
                        AllocateAndSetMap(*m_InitedSegmentExtParams, tc.type & CHECK_NO_BUFFER ? 1 : 0);

                        m_EncodedFramesStat[m_SourceFrameCount].type = SEGMENTATION_MAP_UPDATE | SEGMENTATION_DATA_UPDATE;
                        m_EncodedFramesStat[m_SourceFrameCount].segm_params = *m_InitedSegmentExtParams;
                        m_EncodedFramesStat[m_SourceFrameCount].change_type = CHANGE_TYPE_RESET;
                    }
                }
                else if (tc.type & CHECK_RESET_WITH_SEG_BEFORE_ONLY)
                {
                    if (m_InitedSegmentExtParams)
                    {
                        // switching off the segmentation
                        m_InitedSegmentExtParams->NumSegments = 0;

                        for (mfxU32 i = 0; i < VP9E_DEFAULT_ENCODING_SEQUENCE_COUNT; ++i)
                        {
                            m_EncodedFramesStat[m_SourceFrameCount + i].type = SEGMENTATION_DISABLED;
                        }
                    }
                }
                else if (tc.type & CHECK_RESET_NO_SEGM_EXT_BUFFER)
                {
                    tmp_par = m_par;
                    tmp_par.RemoveExtBuffer(MFX_EXTBUFF_VP9_SEGMENTATION);
                    m_pPar = &tmp_par;
                }
                else if (tc.type & CHECK_STRESS_TEST)
                {
                    VP9_SET_RESET_PARAMS(m_par, MFX_PAR, st);

                    m_InitedSegmentExtParams = reinterpret_cast <mfxExtVP9Segmentation*>(m_par.GetExtBuffer(MFX_EXTBUFF_VP9_SEGMENTATION));
                    if (m_InitedSegmentExtParams)
                    {
                        SetFeatureEnable(*m_InitedSegmentExtParams);
                        AllocateAndSetMap(*m_InitedSegmentExtParams, tc.type & CHECK_NO_BUFFER ? 1 : 0);

                        m_EncodedFramesStat[m_SourceFrameCount].type = m_StressTestScenario[st].segmentation_type_expected;
                        m_EncodedFramesStat[m_SourceFrameCount].segm_params = *m_InitedSegmentExtParams;
                        m_EncodedFramesStat[m_SourceFrameCount].change_type = CHANGE_TYPE_RESET;
                    }
                }
                else
                {
                    // switch Q_Index from positive delta to negative delta
                    EXPECT_EQ(!!m_InitedSegmentExtParams, true);
                    m_InitedSegmentExtParams->Segment[1].QIndexDelta = -100;

                    m_EncodedFramesStat[m_SourceFrameCount].type = SEGMENTATION_DATA_UPDATE;
                    m_EncodedFramesStat[m_SourceFrameCount].change_type = CHANGE_TYPE_RESET;
                }

                g_tsStatus.expect(tc.sts);
                mfxStatus reset_status = Reset(m_session, m_pPar);
                g_tsLog << "Reset() returned with status " << reset_status << ", expected status" << tc.sts << MFX_ERR_NONE << "\n";
            }

            // SECOND ENCODE SECTION
            if ((tc.type & CHECK_RESET || tc.type & CHECK_RESET_WITH_SEG_BEFORE_ONLY
                || tc.type & CHECK_RESET_WITH_SEG_AFTER_ONLY || tc.type & CHECK_STRESS_TEST) && m_initialized)
            {
                if (setup_steps)
                {
                    m_EncodingSequenceCount = m_StressTestScenario[st].encoding_count;
                }
                else
                {
                    reader->ResetFile();
                    bs_checker.m_AllFramesSize = 0;
                }
                const runtime_segmentation_params *params_ptr = tc.type & CHECK_STRESS_TEST ? &m_StressTestScenario[st].runtime_params : nullptr;

                TestSuite::EncodeFrames(m_EncodingSequenceCount, tc.type, params_ptr);
                g_tsStatus.check(DrainEncodedBitstream());

                if (tc.type & CHECK_SECOND_ENCODING_STREAM_IS_BIGGER)
                {
                    mfxU32 frames_size_after_reset = bs_checker.m_AllFramesSize;
                    g_tsLog << "Stream size of the first part = " << frames_size << " bytes, the second part = " << frames_size_after_reset << " bytes\n";
                    if (frames_size_after_reset < frames_size*1.5)
                    {
                        ADD_FAILURE() << "ERROR: Stream size of the second encoding part expected to be significantly bigger due to better quality, but it is not";
                        return MFX_ERR_INVALID_VIDEO_PARAM;
                    }
                }
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

    TS_REG_TEST_SUITE_CLASS(vp9e_segmentation);
};
