//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016-2018 Intel Corporation. All Rights Reserved.
//

#pragma once

#if defined (_WIN32) || defined (_WIN64)

#include "auxiliary_device.h"
#include "encoding_ddi.h"
#include "mfx_vp9_encode_hw_ddi.h"

#if defined (PRE_SI_TARGET_PLATFORM_GEN10)

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

enum
{
    BLOCK_16x16 = 0,
    BLOCK_32x32 = 1,
    BLOCK_64x64 = 2,
    BLOCK_8x8 = 4
};

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
    USHORT              SrcFrameHeightMinus1;
    USHORT              SrcFrameWidthMinus1;
    USHORT              DstFrameHeightMinus1;
    USHORT              DstFrameWidthMinus1;

    UCHAR               CurrOriginalPic;
    UCHAR               CurrReconstructedPic;
    UCHAR               RefFrameList[8];

    union
    {
        struct
        {
            UINT    frame_type                   : 1;
            UINT    show_frame                   : 1;
            UINT    error_resilient_mode         : 1;
            UINT    intra_only                   : 1;
            UINT    allow_high_precision_mv      : 1;
            UINT    mcomp_filter_type            : 3;
            UINT    frame_parallel_decoding_mode : 1;
            UINT    segmentation_enabled         : 1;
            UINT    segmentation_temporal_update : 1;
            UINT    segmentation_update_map      : 1;
            UINT    reset_frame_context          : 2;
            UINT    refresh_frame_context        : 1;
            UINT    frame_context_idx            : 2;
            UINT    LosslessFlag                 : 1;
            UINT    comp_prediction_mode         : 2;
            UINT    super_frame                  : 1;
            UINT    seg_id_block_size            : 2;
            UINT    segmentation_update_data     : 1;

            UINT    reserved                     : 8;
        } fields;

        UINT value;
    } PicFlags;

    union
    {
        struct
        {
            UINT    LastRefIdx                  : 3;
            UINT    LastRefSignBias             : 1;
            UINT    GoldenRefIdx                : 3;
            UINT    GoldenRefSignBias           : 1;
            UINT    AltRefIdx                   : 3;
            UINT    AltRefSignBias              : 1;

            UINT    ref_frame_ctrl_l0           : 3;
            UINT    ref_frame_ctrl_l1           : 3;

            UINT    refresh_frame_flags         : 8;
            UINT    reserved2                   : 6;
        } fields;

        UINT value;
    } RefFlags;

    UCHAR           LumaACQIndex;
    CHAR            LumaDCQIndexDelta;
    CHAR            ChromaACQIndexDelta;
    CHAR            ChromaDCQIndexDelta;

    UCHAR           filter_level;
    UCHAR           sharpness_level;

    CHAR            LFRefDelta[4];
    CHAR            LFModeDelta[2];

    USHORT          BitOffsetForLFRefDelta;
    USHORT          BitOffsetForLFModeDelta;
    USHORT          BitOffsetForLFLevel;
    USHORT          BitOffsetForQIndex;
    USHORT          BitOffsetForFirstPartitionSize;
    USHORT          BitOffsetForSegmentation;
    USHORT          BitSizeForSegmentation;

    UCHAR           log2_tile_rows;
    UCHAR           log2_tile_columns;

    UCHAR           temporal_id;

    UINT            StatusReportFeedbackNumber;

    // Skip Frames
    UCHAR           SkipFrameFlag;      // [0..2]
    UCHAR           NumSkipFrames;
    UINT            SizeSkipFrames;
} ENCODE_SET_PICTURE_PARAMETERS_VP9;

typedef struct _DXVA_Intel_Seg_VP9
{
    union
    {
        struct
        {
            UCHAR SegmentReferenceEnabled : 1; // [0..1]
            UCHAR SegmentReference : 2;        // [0..3]
            UCHAR SegmentSkipped : 1; // [0..1]
            UCHAR ReservedField3 : 4;          // [0]
        } fields;
        UCHAR value;
    } SegmentFlags;

    CHAR  SegmentLFLevelDelta; // [-63..63]
    SHORT SegmentQIndexDelta;  // [-255..255]
} DXVA_Intel_Seg_VP9;

typedef struct tagENCODE_MBSEGMENTMAP_PARAMETERS
{
    DXVA_Intel_Seg_VP9 SegData[8];
} ENCODE_SEGMENT_PARAMETERS;

void FillSpsBuffer(
    VP9MfxVideoParam const & par,
    ENCODE_CAPS_VP9 const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_VP9 & sps,
    Task const & task,
    mfxU16 maxWidth,
    mfxU16 maxHeight);

void FillPpsBuffer(
    VP9MfxVideoParam const & par,
    Task const & task,
    ENCODE_SET_PICTURE_PARAMETERS_VP9 & pps,
    BitOffsets const &offsets,
    mfxU8 &original_pic_index);

mfxStatus FillSegmentMap(Task const & task,
    mfxCoreInterface* m_pmfxCore);

void FillSegmentParam(Task const & task,
    ENCODE_SEGMENT_PARAMETERS & segPar);

void HardcodeCaps(
    ENCODE_CAPS_VP9& caps,
    mfxCoreInterface* pCore);

void PrintDdiToLog(
    ENCODE_CAPS_VP9 const &caps);

void PrintDdiToLogOnce(ENCODE_CAPS_VP9 const &caps);

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
        VP9MfxVideoParam const & par);

    virtual
    mfxStatus Reset(
        VP9MfxVideoParam const & par);

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
    mfxStatus QueryPlatform(
        mfxPlatform& platform);

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
    ENCODE_CAPS_VP9    m_caps;
    mfxPlatform        m_platform;

    ENCODE_SET_SEQUENCE_PARAMETERS_VP9 m_sps;
    ENCODE_SET_PICTURE_PARAMETERS_VP9  m_pps;
    ENCODE_SEGMENT_PARAMETERS          m_seg;
    std::vector<ENCODE_QUERY_STATUS_PARAMS>     m_feedbackUpdate;
    CachedFeedback                              m_feedbackCached;

    std::vector<mfxU8> m_frameHeaderBuf;
    ENCODE_PACKEDHEADER_DATA m_descForFrameHeader;

    VP9SeqLevelParam m_seqParam;

    mfxU32 m_width;
    mfxU32 m_height;

    mfxU8 m_CurrOriginalPicIndex;
    mfxU8 m_MaxTaskCount;

    UMC::Mutex m_guard;
};
#endif // (MFX_VA_LINUX)
} // MfxHwVP9Encode

#endif // (_WIN32) || (_WIN64)

#endif // PRE_SI_TARGET_PLATFORM_GEN10
