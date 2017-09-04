//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_VP9_VIDEO_DECODER

#ifndef __UMC_VP9_DEC_DEFS_DEC_H__
#define __UMC_VP9_DEC_DEFS_DEC_H__

namespace UMC_VP9_DECODER
{
    const Ipp8u VP9_SYNC_CODE_0 = 0x49;
    const Ipp8u VP9_SYNC_CODE_1 = 0x83;
    const Ipp8u VP9_SYNC_CODE_2 = 0x42;

    const Ipp8u VP9_FRAME_MARKER = 0x2;

    typedef enum tagVP9_FRAME_TYPE
    {
        KEY_FRAME = 0,
        INTER_FRAME = 1
    } VP9_FRAME_TYPE;

    typedef enum tagCOLOR_SPACE
    {
        UNKNOWN    = 0,
        BT_601     = 1,  // YUV
        BT_709     = 2,  // YUV
        SMPTE_170  = 3,  // YUV
        SMPTE_240  = 4,  // YUV
        RESERVED_1 = 5,
        RESERVED_2 = 6,
        SRGB       = 7   // RGB
    } COLOR_SPACE;

    typedef enum tagMV_REFERENCE_FRAME
    {
        NONE = -1,
        INTRA_FRAME = 0,
        LAST_FRAME = 1,
        GOLDEN_FRAME = 2,
        ALTREF_FRAME = 3,
        MAX_REF_FRAMES = 4
    } MV_REFERENCE_FRAME;

    typedef enum tagINTERP_FILTER
    {
        EIGHTTAP = 0,
        EIGHTTAP_SMOOTH = 1,
        EIGHTTAP_SHARP = 2,
        BILINEAR = 3,
        SWITCHABLE = 4
    } INTERP_FILTER;

    const Ipp8u VP9_MAX_NUM_OF_SEGMENTS = 8;
    const Ipp8u VP9_NUM_OF_SEGMENT_TREE_PROBS = VP9_MAX_NUM_OF_SEGMENTS - 1;
    const Ipp8u VP9_NUM_OF_PREDICTION_PROBS = 3;

    // Segment level features.
    typedef enum tagSEG_LVL_FEATURES
    {
        SEG_LVL_ALT_Q = 0,               // Use alternate Quantizer ....
        SEG_LVL_ALT_LF = 1,              // Use alternate loop filter value...
        SEG_LVL_REF_FRAME = 2,           // Optional Segment reference frame
        SEG_LVL_SKIP = 3,                // Optional Segment (0,0) + skip mode
        SEG_LVL_MAX = 4                  // Number of features supported
    } SEG_LVL_FEATURES;

    const Ipp8u REFS_PER_FRAME = 3;
    const Ipp8u REF_FRAMES_LOG2 = 3;
    const Ipp8u NUM_REF_FRAMES = 1 << REF_FRAMES_LOG2;

    const Ipp8u FRAME_CONTEXTS_LOG2 = 2;

    const Ipp8u QINDEX_BITS = 8;

    const Ipp8u VP9_MAX_PROB = 255;

    typedef struct tagSizeOfFrame{
        Ipp32u width;
        Ipp32u height;
    } SizeOfFrame;

    const Ipp8u MINQ = 0;
    const Ipp8u MAXQ = 255;
    const Ipp16u QINDEX_RANGE = MAXQ - MINQ + 1;

    const Ipp8u SEGMENT_DELTADATA = 0;
    const Ipp8u SEGMENT_ABSDATA = 1;

    const Ipp8u MAX_REF_LF_DELTAS = 4;
    const Ipp8u MAX_MODE_LF_DELTAS = 2;

    const Ipp8u MAX_LOOP_FILTER = 63;

    const Ipp8u MIN_TILE_WIDTH_B64 = 4;
    const Ipp8u MAX_TILE_WIDTH_B64 = 64;
    const Ipp8u MI_SIZE_LOG2 = 3;
    const Ipp8u MI_BLOCK_SIZE_LOG2 = 6 - MI_SIZE_LOG2; // 64 = 2^6

    const Ipp8u SEG_FEATURE_DATA_SIGNED[SEG_LVL_MAX] = { 1, 1, 0, 0 };
    const Ipp8u SEG_FEATURE_DATA_MAX[SEG_LVL_MAX] = { MAXQ, MAX_LOOP_FILTER, 3, 0 };

    struct Loopfilter
    {
        Ipp32s filterLevel;

        Ipp32s sharpnessLevel;
        Ipp32s lastSharpnessLevel;

        Ipp8u modeRefDeltaEnabled;
        Ipp8u modeRefDeltaUpdate;

        // 0 = Intra, Last, GF, ARF
        Ipp8s refDeltas[MAX_REF_LF_DELTAS];

        // 0 = ZERO_MV, MV
        Ipp8s modeDeltas[MAX_MODE_LF_DELTAS];
    };

    struct LoopFilterInfo
    {
        Ipp8u level[VP9_MAX_NUM_OF_SEGMENTS][MAX_REF_FRAMES][MAX_MODE_LF_DELTAS];
    };

    struct VP9Segmentation
    {
        Ipp8u enabled;
        Ipp8u updateMap;
        Ipp8u updateData;
        Ipp8u absDelta;
        Ipp8u temporalUpdate;

        Ipp8u treeProbs[VP9_NUM_OF_SEGMENT_TREE_PROBS];
        Ipp8u predProbs[VP9_NUM_OF_PREDICTION_PROBS];

        Ipp32s featureData[VP9_MAX_NUM_OF_SEGMENTS][SEG_LVL_MAX];
        Ipp32u featureMask[VP9_MAX_NUM_OF_SEGMENTS];

    };

    class vp9_exception
    {
    public:

        vp9_exception(Ipp32s status = -1)
            : m_Status(status)
        {}

        virtual ~vp9_exception()
        {}

        Ipp32s GetStatus() const
        {
            return m_Status;
        }

    private:

        Ipp32s m_Status;
    };
} // end namespace UMC_VP9_DECODER

#endif // __UMC_VP9_DEC_DEFS_DEC_H__
#endif // UMC_ENABLE_VP9_VIDEO_DECODER
