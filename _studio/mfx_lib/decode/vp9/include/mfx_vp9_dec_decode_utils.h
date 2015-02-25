/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW)

#ifndef _MFX_VP9_DECODE_UTILS_H_
#define _MFX_VP9_DECODE_UTILS_H_

#include <assert.h>
#include "umc_frame_constructor.h"
#include "umc_frame_allocator.h"

namespace MfxVP9Decode
{

    class InputBitstream
    {
    public:
        InputBitstream(
            mfxU8 const * buf,
            size_t        size);

        InputBitstream(
            mfxU8 const * buf,
            mfxU8 const * bufEnd);

        mfxU32 NumBitsRead() const;
        mfxU32 NumBitsLeft() const;

        mfxU32 GetBit();
        mfxU32 GetBits(mfxU32 nbits);
        mfxU32 GetUe();
        mfxI32 GetSe();

    private:
        mfxU8 const * m_buf;
        mfxU8 const * m_ptr;
        mfxU8 const * m_bufEnd;
        mfxU32        m_bitOff;
    };

    static const mfxU8 VP9_FRAME_MARKER = 0x2;

    typedef enum
    {
        KEY_FRAME = 0,
        INTER_FRAME = 1
    } VP9_FRAME_TYPE;

    typedef enum
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

    typedef enum
    {
        NONE = -1,
        INTRA_FRAME = 0,
        LAST_FRAME = 1,
        GOLDEN_FRAME = 2,
        ALTREF_FRAME = 3,
        MAX_REF_FRAMES = 4
    } MV_REFERENCE_FRAME;

    typedef enum
    {
        EIGHTTAP = 0,
        EIGHTTAP_SMOOTH = 1,
        EIGHTTAP_SHARP = 2,
        BILINEAR = 3,
        SWITCHABLE = 4
    } INTERP_FILTER;

    static const mfxU8 VP9_MAX_NUM_OF_SEGMENTS = 8;
    static const mfxU8 VP9_NUM_OF_SEGMENT_TREE_PROBS = VP9_MAX_NUM_OF_SEGMENTS - 1;
    static const mfxU8 VP9_NUM_OF_PREDICTION_PROBS = 3;

    // Segment level features.
    typedef enum
    {
        SEG_LVL_ALT_Q = 0,               // Use alternate Quantizer ....
        SEG_LVL_ALT_LF = 1,              // Use alternate loop filter value...
        SEG_LVL_REF_FRAME = 2,           // Optional Segment reference frame
        SEG_LVL_SKIP = 3,                // Optional Segment (0,0) + skip mode
        SEG_LVL_MAX = 4                  // Number of features supported
    } SEG_LVL_FEATURES;

    typedef struct
    {
        mfxU8 enabled;
        mfxU8 updateMap;
        mfxU8 updateData;
        mfxU8 absDelta;
        mfxU8 temporalUpdate;

        mfxU8 treeProbs[VP9_NUM_OF_SEGMENT_TREE_PROBS];
        mfxU8 predProbs[VP9_NUM_OF_PREDICTION_PROBS];

        mfxI32 featureData[VP9_MAX_NUM_OF_SEGMENTS][SEG_LVL_MAX];
        mfxU32 featureMask[VP9_MAX_NUM_OF_SEGMENTS];

    } VP9Segmentation;

    static const mfxU8 MAX_REF_LF_DELTAS = 4;
    static const mfxU8 MAX_MODE_LF_DELTAS = 2;

    typedef struct
    {
        mfxI32 filterLevel;

        mfxI32 sharpnessLevel;
        mfxI32 lastSharpnessLevel;

        mfxU8 modeRefDeltaEnabled;
        mfxU8 modeRefDeltaUpdate;

        // 0 = Intra, Last, GF, ARF
        mfxI8 refDeltas[MAX_REF_LF_DELTAS];

        // 0 = ZERO_MV, MV
        mfxI8 modeDeltas[MAX_MODE_LF_DELTAS];
    } Loopfilter;

    typedef struct
    {
        mfxU8 level[VP9_MAX_NUM_OF_SEGMENTS][MAX_REF_FRAMES][MAX_MODE_LF_DELTAS];
    } LoopFilterInfo;


    static const mfxU8 REFS_PER_FRAME = 3;
    static const mfxU8 REF_FRAMES_LOG2 = 3;
    static const mfxU8 NUM_REF_FRAMES = 1 << REF_FRAMES_LOG2;

    static const mfxU8 FRAME_CONTEXTS_LOG2 = 2;

    static const mfxU8 QINDEX_BITS = 8;

    static const mfxU8 VP9_MAX_PROB = 255;

    static const mfxU8 MINQ = 0;
    static const mfxU8 MAXQ = 255;
    static const mfxU16 QINDEX_RANGE = MAXQ - MINQ + 1;

    typedef struct {
        mfxU32 width;
        mfxU32 height;
    } SizeOfFrame;

    typedef struct
    {
        VM_ALIGN16_DECL(mfxI16) yDequant[QINDEX_RANGE][8];
        VM_ALIGN16_DECL(mfxI16) uvDequant[QINDEX_RANGE][8];

        COLOR_SPACE color_space;

        mfxU32 width;
        mfxU32 height;

        mfxU32 displayWidth;
        mfxU32 displayHeight;

        mfxI32 subsamplingX;
        mfxI32 subsamplingY;

        // RefCntBuffer frame_bufs[FRAME_BUFFERS];

        UMC::FrameMemID currFrame;

        mfxI32 activeRefIdx[REFS_PER_FRAME];
        UMC::FrameMemID ref_frame_map[NUM_REF_FRAMES]; /* maps fb_idx to reference slot */

        SizeOfFrame sizesOfRefFrame[NUM_REF_FRAMES];

        VP9_FRAME_TYPE frameType;

        mfxU32 show_existing_frame;
        mfxU32 frame_to_show;

        mfxU32 showFrame;
        mfxU32 lastShowFrame;

        mfxU32 intraOnly;

        mfxU32 allowHighPrecisionMv;

        mfxU32 resetFrameContext;

        mfxU8 refreshFrameFlags;

        mfxI32 baseQIndex;
        mfxI32 y_dc_delta_q;
        mfxI32 uv_dc_delta_q;
        mfxI32 uv_ac_delta_q;

        mfxU32 lossless;

        INTERP_FILTER interpFilter;

        LoopFilterInfo lf_info;

        mfxU32 refreshFrameContext;

        mfxU32 refFrameSignBias[MAX_REF_FRAMES];

        Loopfilter lf;
        VP9Segmentation segmentation;

        mfxU32 frameContextIdx;
        // FRAME_COUNTS counts; // ?????????????

        mfxU32 currentVideoFrame;
        mfxU32 profile;
        mfxU32 bit_depth;

        mfxU32 errorResilientMode;
        mfxU32 frameParallelDecodingMode;

        mfxU32 log2TileColumns;
        mfxU32 log2TileRows;

        mfxU32 frameHeaderLength; // in bytes
        mfxU32 firstPartitionSize;
        mfxU32 frameDataSize;

        mfxU32 frameCountInBS;
        mfxU32 currFrameInBS;

    } VP9FrameInfo;


    static const mfxU8 SEGMENT_DELTADATA = 0;
    static const mfxU8 SEGMENT_ABSDATA = 1;
    static const mfxU8 MAX_LOOP_FILTER = 63;
    static const mfxU8 SEG_FEATURE_DATA_SIGNED[SEG_LVL_MAX] = { 1, 1, 0, 0 };
    static const mfxU8 SEG_FEATURE_DATA_MAX[SEG_LVL_MAX] = { MAXQ, MAX_LOOP_FILTER, 3, 0 };

    inline mfxI32 GetMostSignificantBit(mfxU32 n)
    {
        mfxI32 log = 0;
        mfxU32 value = n;

        for (mfxI32 i = 4; i >= 0; --i)
        {
            const mfxI32 shift = (1 << i);
            const mfxU32 x = value >> shift;
            if (x != 0)
            {
                value = x;
                log += shift;
            }
        }
        return log;
    }

    static inline mfxI32 GetUnsignedBits(mfxU32 numValues)
    {
        return numValues > 0 ? GetMostSignificantBit(numValues) + 1 : 0;
    }

    static const mfxU8 MIN_TILE_WIDTH_B64 = 4;
    static const mfxU8 MAX_TILE_WIDTH_B64 = 64;
    static const mfxU8 MI_SIZE_LOG2 = 3;
    static const mfxU8 MI_BLOCK_SIZE_LOG2 = 6 - MI_SIZE_LOG2; // 64 = 2^6

    #define ALIGN_POWER_OF_TWO(value, n) \
        (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

    inline mfxI32 clamp(mfxI32 value, mfxI32 low, mfxI32 high)
    {
        return value < low ? low : (value > high ? high : value);
    }

    void GetTileNBits(const mfxI32 miCols, mfxI32 & minLog2TileCols, mfxI32 & maxLog2TileCols);

    void GetDisplaySize(InputBitstream *pBs, VP9FrameInfo & info);

    void GetFrameSize(InputBitstream *pBs, VP9FrameInfo & info);

    mfxStatus GetBitDepthAndColorSpace(InputBitstream *pBs, VP9FrameInfo & info);

    void GetFrameSizeWithRefs(InputBitstream *pBs, VP9FrameInfo & info);

    ///////////////////////////////////////////////////////////

    void SetupLoopFilter(InputBitstream *pBs, Loopfilter & loopfilter);

    void ClearAllSegFeatures(VP9Segmentation & seg);

    bool IsSegFeatureActive(VP9Segmentation const & seg,
                                   mfxU8 segmentId,
                                   SEG_LVL_FEATURES featureId);

    void EnableSegFeature(VP9Segmentation & seg,
                                 mfxU8 segmentId,
                                 SEG_LVL_FEATURES featureId);

    mfxU8 IsSegFeatureSigned(SEG_LVL_FEATURES featureId);

    void SetSegData(VP9Segmentation & seg, mfxU8 segmentId,
                           SEG_LVL_FEATURES featureId, mfxI32 seg_data);

    mfxI32 GetSegData(VP9Segmentation const & seg,
                             mfxI32 segmentId,
                             SEG_LVL_FEATURES featureId);


    ////////////////////////////////////////////////////////

    mfxI16 vp9_dc_quant(mfxI32 qindex, mfxI32 delta);

    mfxI16 vp9_ac_quant(mfxI32 qindex, mfxI32 delta);

    void InitDequantizer(VP9FrameInfo & info);

    mfxI32 GetQIndex(VP9Segmentation const & seg, mfxU8 segmentId, mfxI32 baseQIndex);

    ////////////////////////////////////////////////

    void SetupPastIndependence(VP9FrameInfo & info);

    void ParseSuperFrameIndex(const mfxU8 *data, size_t data_sz,
                              mfxU32 sizes[8], mfxU32 *count);

    inline mfxU8 ReadMarker(const mfxU8 *data)
    {
        return *data;
    }

}; // namespace MfxVP9Decode

#endif // _MFX_VP9_DECODE_UTILS_H_
#endif // MFX_ENABLE_VP9_VIDEO_DECODE || MFX_ENABLE_VP9_VIDEO_DECODE_HW
