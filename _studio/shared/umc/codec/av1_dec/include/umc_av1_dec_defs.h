//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#ifndef __UMC_AV1_DEC_DEFS_DEC_H__
#define __UMC_AV1_DEC_DEFS_DEC_H__

#include <stdexcept>
#include <vector>
#include "umc_vp9_dec_defs.h"
#include "umc_vp9_frame.h"

namespace UMC_AV1_DECODER
{
    using UMC_VP9_DECODER::NUM_REF_FRAMES;

    class AV1DecoderFrame;
    typedef std::vector<AV1DecoderFrame*> DPBType;

    using UMC_VP9_DECODER::NUM_REF_FRAMES;

    const Ipp8u SYNC_CODE_0 = 0x49;
    const Ipp8u SYNC_CODE_1 = 0x83;
    const Ipp8u SYNC_CODE_2 = 0x43;

    const Ipp8u FRAME_MARKER = 0x2;

    const Ipp8u MINIMAL_DATA_SIZE = 4;

    enum FRAME_TYPE
    {
        KEY_FRAME = 0,
        INTER_FRAME = 1
    };

#if UMC_AV1_DECODER_REV == 0
    const Ipp8u FRAME_ID_NUMBERS_PRESENT_FLAG = 1;
    const Ipp8u FRAME_ID_LENGTH_MINUS7        = 8;   // Allows frame id up to 2^15-1
    const Ipp8u DELTA_FRAME_ID_LENGTH_MINUS2  = 12;  // Allows frame id deltas up to 2^14-1

    const Ipp8u INTER_REFS                    = 6;
    const Ipp8u TOTAL_REFS                    = 7;

    const Ipp8u LOG2_SWITCHABLE_FILTERS       = 3;

    const Ipp8u CDEF_MAX_STRENGTHS            = 16;
#else
    const Ipp8u INTER_REFS                    = 7;
    const Ipp8u TOTAL_REFS                    = 8;

    const Ipp8u QM_LEVEL_BITS                 = 4;

    const Ipp8u LOG2_SWITCHABLE_FILTERS       = 2;

    const Ipp8u CDEF_MAX_STRENGTHS            = 8;

    const Ipp8u MAX_SB_SIZE_LOG2              = 6;
    const Ipp8u MI_SIZE_LOG2                  = 2;
    const Ipp8u MAX_MIB_SIZE_LOG2             = MAX_SB_SIZE_LOG2 - MI_SIZE_LOG2;
#endif
    const Ipp8u FRAME_CONTEXTS_LOG2           = 3;
    const Ipp8u MAX_MODE_LF_DELTAS            = 2;

    enum {
        RESET_FRAME_CONTEXT_NONE = 0,
        RESET_FRAME_CONTEXT_CURRENT = 1,
        RESET_FRAME_CONTEXT_ALL = 2
    };

    enum INTERP_FILTER{
        EIGHTTAP_REGULAR,
        EIGHTTAP_SMOOTH,
        MULTITAP_SHARP,
        BILINEAR,
        INTERP_FILTERS_ALL,
        SWITCHABLE_FILTERS = BILINEAR,
        SWITCHABLE = SWITCHABLE_FILTERS + 1, /* the last switchable one */
        EXTRA_FILTERS = INTERP_FILTERS_ALL - SWITCHABLE_FILTERS,
    };

    enum TX_MODE{
        ONLY_4X4 = 0,     // only 4x4 transform used
        ALLOW_8X8 = 1,    // allow block transform size up to 8x8
        ALLOW_16X16 = 2,  // allow block transform size up to 16x16
        ALLOW_32X32 = 3,  // allow block transform size up to 32x32
        TX_MODE_SELECT,  // transform specified for each block
        TX_MODES,
    };

    enum REFERENCE_MODE {
        SINGLE_REFERENCE = 0,
        COMPOUND_REFERENCE = 1,
        REFERENCE_MODE_SELECT = 2,
        REFERENCE_MODES = 3,
    };

    enum REFRESH_FRAME_CONTEXT_MODE {
        REFRESH_FRAME_CONTEXT_FORWARD,
        REFRESH_FRAME_CONTEXT_BACKWARD,
    };

    enum TRANSFORMATION_TYPE {
        IDENTITY = 0,      // identity transformation, 0-parameter
        TRANSLATION = 1,   // translational motion 2-parameter
        ROTZOOM = 2,       // simplified affine with rotation + zoom only, 4-parameter
        AFFINE = 3,        // affine, 6-parameter
        HORTRAPEZOID = 4,  // constrained homography, hor trapezoid, 6-parameter
        VERTRAPEZOID = 5,  // constrained homography, ver trapezoid, 6-parameter
        HOMOGRAPHY = 6,    // homography, 8-parameter
        TRANS_TYPES = 7,
    };

    enum MV_REFERENCE_FRAME
    {
        NONE = -1,
        INTRA_FRAME = 0,
        LAST_FRAME = 1,
        LAST2_FRAME = 2,
        LAST3_FRAME = 3,
        GOLDEN_FRAME = 4,
        BWDREF_FRAME = 5,
        ALTREF2_FRAME = 6,
        ALTREF_FRAME = 7,
        MAX_REF_FRAMES = 8
    };

    struct SequenceHeader
    {
        int frame_id_numbers_present_flag;
        int frame_id_length_minus7;
        int delta_frame_id_length_minus2;
    };

    struct Loopfilter
    {
        Ipp32s filterLevel;

        Ipp32s sharpnessLevel;
        Ipp32s lastSharpnessLevel;

        Ipp8u modeRefDeltaEnabled;
        Ipp8u modeRefDeltaUpdate;

        // 0 = Intra, Last, Last2, Last3, GF, Bwd, ARF
        Ipp8s refDeltas[TOTAL_REFS];

        // 0 = ZERO_MV, MV
        Ipp8s modeDeltas[MAX_MODE_LF_DELTAS];
    };

#if UMC_AV1_DECODER_REV >= 251
    struct WarpedMotionParams {
        TRANSFORMATION_TYPE wmtype;
        Ipp32s wmmat[8];
        Ipp16u alpha;
        Ipp16u beta;
        Ipp16u gamma;
        Ipp16u delta;
    };
#endif

    namespace vp92av1
    {
        struct FrameHeader : public UMC_VP9_DECODER::VP9DecoderFrame
        {};
    }

    struct FrameHeader
        : vp92av1::FrameHeader
    {
        Ipp8u  use_reference_buffer;
        Ipp32u display_frame_id;

        Ipp32u frameIdsRefFrame[NUM_REF_FRAMES];
        Ipp32s activeRefIdx[INTER_REFS];
        Ipp32u refFrameSignBias[INTER_REFS];
        Ipp32u allowScreenContentTools;
        Ipp32u loopFilterAcrossTilesEnabled;
        Ipp32u usePrevMVs;
        INTERP_FILTER interpFilter;
        TX_MODE txMode;
        Ipp32u reducedTxSetUsed;
        Ipp32u referenceMode;

        Ipp32u deltaQPresentFlag;
        Ipp32u deltaQRes;
        Ipp32u deltaLFPresentFlag;
        Ipp32u deltaLFRes;

#if UMC_AV1_DECODER_REV == 0
        Ipp32u cdefDeringDamping;
        Ipp32u cdefClpfDamping;
        Ipp32u nbCdefStrengths;
#else
        Ipp32u cdefPriDamping;
        Ipp32u cdefSecDamping;
#endif
        Ipp32u cdefStrength[CDEF_MAX_STRENGTHS];
        Ipp32u cdefUVStrength[CDEF_MAX_STRENGTHS];

#if UMC_AV1_DECODER_REV >= 251
        Ipp32u allowInterIntraCompound;
        Ipp32u allowMaskedCompound;
        Ipp32u globalMotionType;

        Ipp32u useQMatrix;
        Ipp32u minQMLevel;
        Ipp32u maxQMLevel;

        WarpedMotionParams global_motion[TOTAL_REFS];

        Ipp32u tileCols;
        Ipp32u tileRows;
#endif

        Ipp32u tileSizeBytes;

        Loopfilter lf;
    };

    inline bool IsFrameIntraOnly(FrameHeader const * fh)
    {
        return (fh->frameType == KEY_FRAME || fh->intraOnly);
    }

    inline bool IsFrameResilent(FrameHeader const * fh)
    {
        return IsFrameIntraOnly(fh) || fh->errorResilientMode;
    }

    class av1_exception
        : public std::runtime_error
    {
    public:

        av1_exception(Ipp32s /*status*/)
            : std::runtime_error("AV1 error")
        {}

        Ipp32s GetStatus() const
        {
            return UMC::UMC_OK;
        }
    };
}

#endif // __UMC_AV1_DEC_DEFS_DEC_H__
#endif // UMC_ENABLE_AV1_VIDEO_DECODER
