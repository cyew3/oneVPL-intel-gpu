/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#if defined (_WIN32) || defined (_WIN64)

#include "auxiliary_device.h"
#include "encoding_ddi.h"
#include "mfx_vp9_encode_hw_ddi.h"

#if defined (AS_VP9E_PLUGIN)

namespace MfxHwVP9Encode
{
#if defined (MFX_VA_WIN)

class AuxiliaryDevice : public ::AuxiliaryDevice
{
public:
    using ::AuxiliaryDevice::Execute;

    template <typename T, typename U>
    HRESULT Execute(mfxU32 func, T& in, U& out)
    {
        return ::AuxiliaryDevice::Execute(func, &in, sizeof(in), &out, sizeof(out));
    }

    template <typename T>
    HRESULT Execute(mfxU32 func, T& in, void*)
    {
        return ::AuxiliaryDevice::Execute(func, &in, sizeof(in), 0, 0);
    }

    template <typename U>
    HRESULT Execute(mfxU32 func, void*, U& out)
    {
        return ::AuxiliaryDevice::Execute(func, 0, 0, &out, sizeof(out));
    }
};

typedef struct tagFRAMERATE
{
    UINT Numerator;
    UINT Denominator;
} FRAMERATE;

typedef enum tagENCODE_INPUT_COLORSPACE
{
    eColorSpace_P709  = 0,
    eColorSpace_P601  = 1,
    eColorSpace_P2020 = 2
} ENCODE_INPUT_COLORSPACE;

typedef enum tagENCODE_FRAMESIZE_TOLERANCE
{
    eFrameSizeTol_Normal        = 0,
    eFrameSizeTol_Low           = 1,    // maps to "sliding window"
    eFrameSizeTol_Extremely_Low = 2    // maps to "low delay"
} ENCODE_FRAMESIZE_TOLERANCE;

#ifdef DDI_VER_0960
// DDI v0.960
typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_VP9
{
    USHORT wMaxFrameWidth;
    USHORT wMaxFrameHeight;
    USHORT GopPicSize;
    UCHAR  TargetUsage;
    UCHAR  RateControlMethod;
    UINT   TargetBitRate[8];    // One per temporal layer
    UINT   MaxBitRate;
    UINT   MinBitRate;
    ULONG  InitVBVBufferFullnessInBit;
    ULONG  VBVBufferSizeInBit;
    ULONG  OptimalVBVBufferLevelInBit;
    ULONG  UpperVBVBufferLevelThresholdInBit;
    ULONG  LowerVBVBufferLevelThresholdInBit;

    union
    {
        struct
        {
            UINT bResetBRC               : 1;
            UINT bNoFrameHeaderInsertion : 1;
            UINT bUseRawReconRef         : 1;
            UINT MBBRC                   : 4;
            UINT EnableDynamicScaling    : 1;
            UINT SourceFormat            : 2;    //[0..3]
            UINT SourceBitDepth          : 2;     //[0..3]
            UINT EncodedFormat           : 2;     //[0..2]
            UINT EncodedBitDepth         : 2;     //[0..2]
            UINT DisplayFormatSwizzle    : 1;
            UINT bReserved               : 15;
        } fields;
        UINT value;
    } SeqFlags;

    UINT      UserMaxFrameSize;
    USHORT    reserved2;
    USHORT    reserved3;
    FRAMERATE FrameRate[8]; // One per temporal layer
    UCHAR     NumTemporalLayersMinus1;
    UCHAR     ICQQualityFactor;            // [0..255]

    ENCODE_INPUT_COLORSPACE        InputColorSpace;
    ENCODE_SCENARIO                ScenarioInfo;
    ENCODE_CONTENT                ContentInfo;
    ENCODE_FRAMESIZE_TOLERANCE         FrameSizeTolerance;

} ENCODE_SET_SEQUENCE_PARAMETERS_VP9;

typedef struct tagENCODE_SET_PICTURE_PARAMETERS_VP9
{
    USHORT SrcFrameHeightMinus1;       // [0..2^16-1]
    USHORT SrcFrameWidthMinus1;        // [0..2^16-1]
    USHORT DstFrameHeightMinus1;       // [0..2^16-1]
    USHORT DstFrameWidthMinus1;        // [0..2^16-1]

    UCHAR  CurrOriginalPic;            // [0..127]
    UCHAR  CurrReconstructedPic;       // [0..11]
    UCHAR  RefFrameList [8];           // [0..11, 0xFF]

    union
    {
        struct
        {
            UINT frame_type                : 1;    // [0..1]
            UINT show_frame               : 1;    // [0..1]
            UINT error_resilient_mode          : 1;    // [0..1]
            UINT intra_only                 : 1;    // [0..1]
            UINT allow_high_precision_mv         : 1;     // [0..1]
            UINT mcomp_filter_type             : 3;     // [0]
            UINT frame_parallel_decoding_mode    : 1;     // [0..1]
            UINT segmentation_enabled        : 1;      // [1]
            UINT segmentation_temporal_update    : 1;      // [1]
            UINT segmentation_update_map        : 1;      // [1]
            UINT reset_frame_context            : 2;      // [0..3]
            UINT refresh_frame_context        : 1;      // [0..1]
            UINT frame_context_idx             : 2;      // [0..3]
            UINT LosslessFlag                 : 1;      // [0..1]
            UINT comp_prediction_mode        : 2;     // [0..2]
            UINT SuperFrame                : 1;     // [0..1]
            UINT SegIdBlockSize            : 2;    // [0..3]
            UINT ReservedField                : 9;
        }fields;
        UINT value;
    } PicFlags;

    union
    {
        struct
        {
            UINT LastRefIdx                 : 3;    // [0..7]
            UINT LastRefSignBias             : 1;     // [0..1]
            UINT GoldenRefIdx                 : 3;     // [0..7]
            UINT GoldenRefSignBias : 1;     // [0..1]
            UINT AltRefIdx : 3;     // [0..7]
            UINT AltRefSignBias : 1;     // [0..1]

            UINT ref_frame_ctrl_l0 : 3;
            UINT ref_frame_ctrl_l1 : 3;

            UINT refresh_frame_flags : 8;
            UINT ReservedField : 6;
        }            fields;
        UINT value;
    } RefFlags;

    UCHAR       LumaACQIndex;                // [1..255]
    CHAR        LumaDCQIndexDelta;            // [-15..15]
    CHAR        ChromaACQIndexDelta;            // [-15..15]
    CHAR        ChromaDCQIndexDelta;            // [-15..15]

    UCHAR        MinLumaACQIndex;                // [1..255]
    UCHAR        MaxLumaACQIndex;                // [1..255]

    UCHAR        filter_level;                    // [0..63]
    UCHAR        sharpness_level;                // [0..7]

    CHAR        LFRefDelta[4];                // [-63..63]
    CHAR        LFModeDelta[2];                // [-63..63]

    USHORT    BitOffsetForLFRefDelta;
    USHORT    BitOffsetForLFModeDelta;
    USHORT    BitOffsetForLFLevel;
    USHORT    BitOffsetForQIndex;
    USHORT    BitOffsetForFirstPartitionSize;
    USHORT    BitOffsetForSegmentation;
    USHORT    BitSizeForSegmentation;

    UCHAR        log2_tile_rows;                // [0..2]
    UCHAR        log2_tile_columns;                // [0..5]

    UCHAR        temporal_id;

    UINT        StatusReportFeedbackNumber;

    // Skip Frames
    UCHAR        SkipFrameFlag;                // [0..2]
    UCHAR        NumSkipFrames;
    UINT        SizeSkipFrames;

} ENCODE_SET_PICTURE_PARAMETERS_VP9;
#else
// DDI v0.94
typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_VP9
{
    USHORT        wMaxFrameWidth;
    USHORT        wMaxFrameHeight;

    USHORT      GopPicSize;

    UCHAR       TargetUsage;
    UCHAR       RateControlMethod;
    UINT          TargetBitRate[8];    // One per temporal layer
    UINT          MaxBitRate;
    UINT        MinBitRate;
    ULONG         InitVBVBufferFullnessInBit;
    ULONG        VBVBufferSizeInBit;
    ULONG       OptimalVBVBufferLevelInBit;
    ULONG       UpperVBVBufferLevelThresholdInBit;
    ULONG       LowerVBVBufferLevelThresholdInBit;

    union
    {
        struct
        {
            DWORD        bResetBRC : 1;
            DWORD       bNoFrameHeaderInsertion : 1;
            DWORD       bUseRawReconRef : 1;
            DWORD       MBBRC : 4;
            DWORD       EnableDynamicScaling : 1;
            DWORD       SourceFormat : 2;
            DWORD       SourceBitDepth : 2;
            DWORD       EncodedFormat : 2;
            DWORD       EncodedBitDepth : 2;
            DWORD       DisplayFormatSwizzle : 1;
            DWORD       bReserved : 15;
        }     fields;

        UINT    value;
    } SeqFlags;

    UINT    UserMaxFrameSize;
    USHORT  reserved2;
    USHORT  reserved3;
    FRAMERATE   FrameRate[8];
    UCHAR       NumTemporalLayersMinus1;
} ENCODE_SET_SEQUENCE_PARAMETERS_VP9;

typedef struct tagENCODE_SET_PICTURE_PARAMETERS_VP9
{
    USHORT        SrcFrameHeightMinus1;        // [0..2^16-1]
    USHORT        SrcFrameWidthMinus1;        // [0..2^16-1]
    USHORT        DstFrameHeightMinus1;        // [0..2^16-1]
    USHORT        DstFrameWidthMinus1;        // [0..2^16-1]

    UCHAR            CurrOriginalPic;            // [0..127]
    UCHAR            CurrReconstructedPic;        // [0..127]
    UCHAR            RefFrameList[8];               // [0..127, 0xFF]

    union
    {
        struct
        {
            UINT        frame_type : 1;    // [0..1]
            UINT        show_frame : 1;    // [0..1]
            UINT         error_resilient_mode : 1;    // [0..1]
            UINT          intra_only : 1;    // [0..1]

            UINT         allow_high_precision_mv : 1;     // [0..1]
            UINT         mcomp_filter_type : 3;     // [0..7]
            UINT         frame_parallel_decoding_mode : 1;     // [0..1]
            UINT         segmentation_enabled : 1;      // [1]
            UINT         segmentation_temporal_update : 1;      // [1]
            UINT         segmentation_update_map : 1;      // [1]
            UINT         reset_frame_context : 2;      // [0..3]
            UINT         refresh_frame_context : 1;      // [0..1]
            UINT         frame_context_idx : 2;      // [0..3]
            UINT         LosslessFlag : 1;      // [0..1]
            UINT         comp_prediction_mode : 2;     // [0..2]
            UINT        super_frame : 1;    //[0..1]
            UINT        SegIdBlockSize : 2;    //[0..3]
            UINT        ReservedField : 9;
        }            fields;
        UINT        value;
    } PicFlags;

    union
    {
        struct
        {
            UINT        LastRefIdx : 3;    // [0..7]
            UINT         LastRefSignBias : 1;     // [0..1]
            UINT         GoldenRefIdx : 3;     // [0..7]
            UINT         GoldenRefSignBias : 1;     // [0..1]
            UINT         AltRefIdx : 3;     // [0..7]
            UINT         AltRefSignBias : 1;     // [0..1]

            UINT        ref_frame_ctrl_l0 : 3;
            UINT        ref_frame_ctrl_l1 : 3;

            UINT        refresh_frame_flags : 8;
            UINT        reserved2 : 6;

        }            fields;
        UINT        value;
    } RefFlags;

    UCHAR        LumaACQIndex;                // [1..255]
    CHAR        LumaDCQIndexDelta;            // [-15..15]
    CHAR        ChromaACQIndexDelta;            // [-15..15]
    CHAR        ChromaDCQIndexDelta;            // [-15..15]

    UCHAR        filter_level;                    // [0..63]
    UCHAR        sharpness_level;                // [0..7]

    CHAR        LFRefDelta[4];                // [-63..63]
    CHAR        LFModeDelta[2];                // [-63..63]

    USHORT    BitOffsetForLFRefDelta;
    USHORT    BitOffsetForLFModeDelta;
    USHORT    BitOffsetForLFLevel;
    USHORT    BitOffsetForQIndex;
    USHORT    BitOffsetForFirstPartitionSize;
    USHORT    BitOffsetForSegmentation;
    USHORT  BitSizeForSegmentation;

    UCHAR        log2_tile_rows;                // [0..2]
    UCHAR        log2_tile_columns;                // [0..5]

    UCHAR        temporal_id;

    UINT        StatusReportFeedbackNumber;

    // Skip Frames
    UCHAR        SkipFrameFlag;                // [0..2]
    UCHAR        NumSkipFrames;
    UINT        SizeSkipFrames;
} ENCODE_SET_PICTURE_PARAMETERS_VP9;
#endif

void FillSpsBuffer(
    VP9MfxParam const & par,
    ENCODE_CAPS_VP9 const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_VP9 & sps);

void FillPpsBuffer(
    VP9MfxParam const & par,
    Task const & task,
    ENCODE_SET_PICTURE_PARAMETERS_VP9 & pps);

class CachedFeedback
{
public:
    typedef ENCODE_QUERY_STATUS_PARAMS Feedback;
    typedef std::vector<Feedback> FeedbackStorage;

    void Reset(mfxU32 cacheSize);

    void Update(FeedbackStorage const & update);

    const Feedback * Hit(mfxU32 feedbackNumber) const;

    void Remove(mfxU32 feedbackNumber);

private:
    FeedbackStorage m_cache;
}; // class CachedFeedback

typedef enum {
  KEY_FRAME = 0,
  INTER_FRAME = 1,
  FRAME_TYPES,
} FRAME_TYPE;

typedef enum {
  INTRA_FRAME = 0,
  LAST_FRAME = 1,
  GOLDEN_FRAME = 2,
  ALTREF_FRAME = 3,
  MAX_REF_FRAMES = 4
} MV_REFERENCE_FRAME;

#define MAX_REF_LF_DELTAS 4
#define MAX_MODE_LF_DELTAS 2
#define SEG_LVL_MAX 4

/*!\brief List of supported color spaces */
typedef enum vpx_color_space {
  VPX_CS_UNKNOWN = 0,   /**< Unknown */
  VPX_CS_BT_601 = 1,    /**< BT.601 */
  VPX_CS_BT_709 = 2,    /**< BT.709 */
  VPX_CS_SMPTE_170 = 3, /**< SMPTE.170 */
  VPX_CS_SMPTE_240 = 4, /**< SMPTE.240 */
  VPX_CS_BT_2020 = 5,   /**< BT.2020 */
  VPX_CS_RESERVED = 6,  /**< Reserved */
  VPX_CS_SRGB = 7       /**< sRGB */
} vpx_color_space_t;    /**< alias for enum vpx_color_space */

typedef enum vpx_bit_depth {
    VPX_BITS_8  =  8,  /**<  8 bits */
    VPX_BITS_10 = 10,  /**< 10 bits */
    VPX_BITS_12 = 12,  /**< 12 bits */
} vpx_bit_depth_t;

typedef uint8_t INTERP_FILTER;

typedef enum BITSTREAM_PROFILE {
  PROFILE_0,
  PROFILE_1,
  PROFILE_2,
  PROFILE_3,
  MAX_PROFILES
} BITSTREAM_PROFILE;

struct loopfilter {
  int filter_level;
  int last_filt_level;

  int sharpness_level;
  int last_sharpness_level;

  uint8_t mode_ref_delta_enabled;
  uint8_t mode_ref_delta_update;

  // 0 = Intra, Last, GF, ARF
  signed char ref_deltas[MAX_REF_LF_DELTAS];
  signed char last_ref_deltas[MAX_REF_LF_DELTAS];

  // 0 = ZERO_MV, MV
  signed char mode_deltas[MAX_MODE_LF_DELTAS];
  signed char last_mode_deltas[MAX_MODE_LF_DELTAS];
};

struct segmentation {
  uint8_t enabled;
  uint8_t update_map;
  uint8_t update_data;
  uint8_t abs_delta;
  uint8_t temporal_update;

  int16_t feature_data[MAX_SEGMENTS][SEG_LVL_MAX];
  uint32_t feature_mask[MAX_SEGMENTS];
  int aq_av_offset;
};

typedef int vpx_color_range_t;
#define REF_FRAMES_LOG2 3
#define REF_FRAMES (1 << REF_FRAMES_LOG2)
#define SWITCHABLE 4 /* should be the last one */
typedef struct VP9Common {
  vpx_color_space_t color_space;
  vpx_color_range_t color_range;
  int width;
  int height;
  int render_width;
  int render_height;
  int last_width;
  int last_height;

  // TODO(jkoleszar): this implies chroma ss right now, but could vary per
  // plane. Revisit as part of the future change to YV12_BUFFER_CONFIG to
  // support additional planes.
  int subsampling_x;
  int subsampling_y;

  int ref_frame_map[REF_FRAMES]; /* maps fb_idx to reference slot */

  // Prepare ref_frame_map for the next frame.
  // Only used in frame parallel decode.
  int next_ref_frame_map[REF_FRAMES];

  FRAME_TYPE frame_type;

  int show_frame;
  int last_show_frame;
  int show_existing_frame;

  // Flag signaling that the frame is encoded using only INTRA modes.
  uint8_t intra_only;
  uint8_t last_intra_only;

  int allow_high_precision_mv;

  // Flag signaling that the frame context should be reset to default values.
  // 0 or 1 implies don't reset, 2 reset just the context specified in the
  // frame header, 3 reset all contexts.
  int reset_frame_context;

  // MBs, mb_rows/cols is in 16-pixel units; mi_rows/cols is in
  // MODE_INFO (8-pixel) units.
  int MBs;
  int mb_rows, mi_rows;
  int mb_cols, mi_cols;
  int mi_stride;

  int base_qindex;
  int y_dc_delta_q;
  int uv_dc_delta_q;
  int uv_ac_delta_q;
  int16_t y_dequant[MAX_SEGMENTS][2];
  int16_t uv_dequant[MAX_SEGMENTS][2];

  // Whether to use previous frame's motion vectors for prediction.
  int use_prev_frame_mvs;

  INTERP_FILTER interp_filter;

  int refresh_frame_context; /* Two state 0 = NO, 1 = YES */

  int ref_frame_sign_bias[MAX_REF_FRAMES]; /* Two state 0, 1 */

  struct loopfilter lf;
  struct segmentation seg;

  unsigned int frame_context_idx; /* Context to use/update */

  BITSTREAM_PROFILE profile;

  // VPX_BITS_8 in profile 0 or 1, VPX_BITS_10 or VPX_BITS_12 in profile 2 or 3.
  vpx_bit_depth_t bit_depth;

  int error_resilient_mode;
  int frame_parallel_decoding_mode;

  int log2_tile_cols, log2_tile_rows;
  int byte_alignment;
  int skip_loop_filter;
} VP9_COMMON;
typedef struct VP9_COMP {
  VP9_COMMON common;

  int scaled_ref_idx[MAX_REF_FRAMES];
  int lst_fb_idx;
  int gld_fb_idx;
  int alt_fb_idx;

  int refresh_last_frame;
  int refresh_golden_frame;
  int refresh_alt_ref_frame;

  unsigned char refresh_frames_mask;

  double framerate;

  int interp_filter_selected[MAX_REF_FRAMES][SWITCHABLE];

  int initial_width;
  int initial_height;
  int initial_mbs;  // Number of MBs in the full-size frame; to be used to
                    // normalize the firstpass stats. This will differ from the
                    // number of MBs in the current frame when the frame is
                    // scaled.

  int frame_flags;

  int resize_pending;
  int resize_state;
  int external_resize;
  int resize_scale_num;
  int resize_scale_den;
  int resize_avg_qp;
  int resize_buffer_underflow;
  int resize_count;

  int use_skin_detection;

  int target_level;

} VP9_COMP;

class D3D9Encoder : public DriverEncoder
{
public:
    D3D9Encoder();

    virtual
    ~D3D9Encoder();

    virtual
    mfxStatus CreateAuxilliaryDevice(
        mfxCoreInterface* core,
        GUID       guid,
        mfxU32     width,
        mfxU32     height);

    virtual
    mfxStatus CreateAccelerationService(
        VP9MfxParam const & par);

    virtual
    mfxStatus Reset(
        VP9MfxParam const & par);

    // empty  for Lin
    virtual
    mfxStatus Register(
        mfxMemId memId,
        D3DDDIFORMAT type);

    // 2 -> 1
    virtual
    mfxStatus Register(
        mfxFrameAllocResponse& response,
        D3DDDIFORMAT type);

    // (mfxExecuteBuffers& data)
    virtual
    mfxStatus Execute(
        Task const &task,
        mfxHDL surface);

    // recomendation from HW
    virtual
    mfxStatus QueryCompBufferInfo(
        D3DDDIFORMAT type,
        mfxFrameAllocRequest& request,
        mfxU32 frameWidth,
        mfxU32 frameHeight);

    virtual
    mfxStatus QueryEncodeCaps(
        ENCODE_CAPS_VP9& caps);

    virtual
    mfxStatus QueryStatus(
        Task & task);

    virtual
        mfxU32 GetReconSurfFourCC();

    virtual
    mfxStatus Destroy();

private:
    D3D9Encoder(const D3D9Encoder&); // no implementation
    D3D9Encoder& operator=(const D3D9Encoder&); // no implementation

    std::auto_ptr<AuxiliaryDevice> m_auxDevice;
    GUID m_guid;

    mfxCoreInterface*  m_pmfxCore;
    VP9MfxParam        m_video;
    ENCODE_CAPS_VP9    m_caps;

    ENCODE_SET_SEQUENCE_PARAMETERS_VP9 m_sps;
    ENCODE_SET_PICTURE_PARAMETERS_VP9  m_pps;
    std::vector<ENCODE_QUERY_STATUS_PARAMS>     m_feedbackUpdate;
    CachedFeedback                              m_feedbackCached;

    std::vector<mfxU8> m_uncompressedHeaderBuf;
    VP9_COMP m_libvpxBasedVP9Param;
    ENCODE_PACKEDHEADER_DATA m_descForFrameHeader;


    mfxU32 m_width;
    mfxU32 m_height;

    UMC::Mutex m_guard;
};
#endif // (MFX_VA_LINUX)
} // MfxHwVP9Encode

#endif // (_WIN32) || (_WIN64)

#endif // AS_VP9E_PLUGIN
