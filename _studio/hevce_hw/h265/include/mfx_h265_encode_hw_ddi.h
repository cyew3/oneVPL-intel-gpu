//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "mfx_config.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h264_encode_struct_vaapi.h"
#include "mfxplugin++.h"

#include "mfx_h265_encode_hw_utils.h"
#include "mfx_h265_encode_hw_bs.h"

#include <memory>
#include <vector>

//#define HEADER_PACKING_TEST
#if defined(_DEBUG) && defined(_WIN32)
    #define DDI_TRACE
#endif

#define D3DDDIFMT_NV12 (D3DDDIFORMAT)(MFX_MAKEFOURCC('N', 'V', '1', '2'))
#define D3DDDIFMT_YU12 (D3DDDIFORMAT)(MFX_MAKEFOURCC('Y', 'U', '1', '2'))

namespace MfxHwH265Encode
{

// GUIDs from DDI for HEVC Encoder spec 0.956
static const GUID DXVA2_Intel_Encode_HEVC_Main =
{ 0x28566328, 0xf041, 0x4466, { 0x8b, 0x14, 0x8f, 0x58, 0x31, 0xe7, 0x8f, 0x8b } };

#ifndef OPEN_SOURCE
static const GUID DXVA2_Intel_Encode_HEVC_Main10 =
{ 0x6b4a94db, 0x54fe, 0x4ae1, { 0x9b, 0xe4, 0x7a, 0x7d, 0xad, 0x00, 0x46, 0x00 } };

#ifdef PRE_SI_TARGET_PLATFORM_GEN10
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main =
{ 0xb8b28e0c, 0xecab, 0x4217, { 0x8c, 0x82, 0xea, 0xaa, 0x97, 0x55, 0xaa, 0xf0 } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main10 =
{ 0x8732ecfd, 0x9747, 0x4897, { 0xb4, 0x2a, 0xe5, 0x34, 0xf9, 0xff, 0x2b, 0x7a } };
#endif  //PRE_SI_TARGET_PLATFORM_GEN10

#ifdef PRE_SI_TARGET_PLATFORM_GEN11
static const GUID DXVA2_Intel_Encode_HEVC_Main422 =
{ 0x056a6e36, 0xf3a8, 0x4d00, { 0x96, 0x63, 0x7e, 0x94, 0x30, 0x35, 0x8b, 0xf9 } };
static const GUID DXVA2_Intel_Encode_HEVC_Main422_10 =
{ 0xe139b5ca, 0x47b2, 0x40e1, { 0xaf, 0x1c, 0xad, 0x71, 0xa6, 0x7a, 0x18, 0x36 } };
static const GUID DXVA2_Intel_Encode_HEVC_Main444 =
{ 0x5415a68c, 0x231e, 0x46f4, { 0x87, 0x8b, 0x5e, 0x9a, 0x22, 0xe9, 0x67, 0xe9 } };
static const GUID DXVA2_Intel_Encode_HEVC_Main444_10 =
{ 0x161be912, 0x44c2, 0x49c0, { 0xb6, 0x1e, 0xd9, 0x46, 0x85, 0x2b, 0x32, 0xa1 } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main422 =
{ 0xcee393ab, 0x1030, 0x4f7b, { 0x8d, 0xbc, 0x55, 0x62, 0x9c, 0x72, 0xf1, 0x7e } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main422_10 =
{ 0x580da148, 0xe4bf, 0x49b1, { 0x94, 0x3b, 0x42, 0x14, 0xab, 0x05, 0xa6, 0xff } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main444 =
{ 0x87b2ae39, 0xc9a5, 0x4c53, { 0x86, 0xb8, 0xa5, 0x2d, 0x7e, 0xdb, 0xa4, 0x88 } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main444_10 =
{ 0x10e19ac8, 0xbf39, 0x4443, { 0xbe, 0xc3, 0x1b, 0x0c, 0xbf, 0xe4, 0xc7, 0xaa } };
#endif
#endif // #ifndef OPEN_SOURCE

GUID GetGUID(MfxVideoParam const & par);

#ifndef OPEN_SOURCE
const GUID GuidTable[2][2][3] = 
{
    // LowPower = OFF
    {
        // BitDepthLuma = 8
        {
            /*420*/ DXVA2_Intel_Encode_HEVC_Main,
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            /*422*/ DXVA2_Intel_Encode_HEVC_Main422,
            /*444*/ DXVA2_Intel_Encode_HEVC_Main444
#endif
        },
        // BitDepthLuma = 10
        {
            /*420*/ DXVA2_Intel_Encode_HEVC_Main10,
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            /*422*/ DXVA2_Intel_Encode_HEVC_Main422_10,
            /*444*/ DXVA2_Intel_Encode_HEVC_Main444_10
#endif
        }
    },
    // LowPower = ON
    
#if defined(PRE_SI_TARGET_PLATFORM_GEN10)
    {
        // BitDepthLuma = 8
        {
            /*420*/ DXVA2_Intel_LowpowerEncode_HEVC_Main,
    #if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            /*422*/ DXVA2_Intel_LowpowerEncode_HEVC_Main422,
            /*444*/ DXVA2_Intel_LowpowerEncode_HEVC_Main444
    #endif
        },
        // BitDepthLuma = 10
        {
            /*420*/ DXVA2_Intel_LowpowerEncode_HEVC_Main10,
    #if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            /*422*/ DXVA2_Intel_LowpowerEncode_HEVC_Main422_10,
            /*444*/ DXVA2_Intel_LowpowerEncode_HEVC_Main444_10
    #endif
        }
    }
#endif
};
#endif

mfxStatus HardcodeCaps(ENCODE_CAPS_HEVC& caps, MFXCoreInterface* pCore, GUID guid);

class DriverEncoder;

typedef enum tagENCODER_TYPE
{
    ENCODER_DEFAULT = 0,
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    ENCODER_REXT
#endif //defined(PRE_SI_TARGET_PLATFORM_GEN11)
} ENCODER_TYPE;

DriverEncoder* CreatePlatformH265Encoder(MFXCoreInterface* core, ENCODER_TYPE type = ENCODER_DEFAULT);
mfxStatus QueryHwCaps(MFXCoreInterface* core, GUID guid, ENCODE_CAPS_HEVC & caps);
mfxStatus CheckHeaders(MfxVideoParam const & par, ENCODE_CAPS_HEVC const & caps);

#if MFX_EXTBUFF_CU_QP_ENABLE
mfxStatus FillCUQPDataDDI(Task& task, MfxVideoParam &par, MFXCoreInterface& core, mfxFrameInfo &CUQPFrameInfo);
#endif

enum
{
    MAX_DDI_BUFFERS = 4, //sps, pps, slice, bs
};

class DriverEncoder
{
public:

    virtual ~DriverEncoder(){}

    virtual
    mfxStatus CreateAuxilliaryDevice(
                    MFXCoreInterface * core,
                    GUID        guid,
                    mfxU32      width,
                    mfxU32      height) = 0;

    virtual
    mfxStatus CreateAccelerationService(
        MfxVideoParam const & par) = 0;

    virtual
    mfxStatus Reset(
        MfxVideoParam const & par, bool bResetBRC) = 0;

    virtual
    mfxStatus Register(
        mfxFrameAllocResponse & response,
        D3DDDIFORMAT            type) = 0;

    virtual
    mfxStatus Execute(
        Task const &task,
        mfxHDL surface) = 0;

    virtual
    mfxStatus QueryCompBufferInfo(
        D3DDDIFORMAT           type,
        mfxFrameAllocRequest & request) = 0;

    virtual
    mfxStatus QueryEncodeCaps(
        ENCODE_CAPS_HEVC & caps) = 0;


    virtual
    mfxStatus QueryStatus(
        Task & task ) = 0;

    virtual
    mfxStatus Destroy() = 0;

    virtual
    ENCODE_PACKEDHEADER_DATA* PackHeader(Task const & task, mfxU32 nut) = 0;

    // Functions below are used in HEVC FEI encoder

    virtual mfxStatus PreSubmitExtraStage(Task const & task)
    {
        task;
        return MFX_ERR_NONE;
    }

    virtual mfxStatus PostQueryExtraStage()
    {
        return MFX_ERR_NONE;
    }
};

class DDIHeaderPacker
{
public:
    DDIHeaderPacker();
    ~DDIHeaderPacker();

    void Reset(MfxVideoParam const & par);

    ENCODE_PACKEDHEADER_DATA* PackHeader(Task const & task, mfxU32 nut);
    ENCODE_PACKEDHEADER_DATA* PackSliceHeader(Task const & task,
        mfxU32 id,
        mfxU32* qpd_offset,
        bool dyn_slice_size = false,
        mfxU32* sao_offset = 0,
        mfxU16* ssh_start_len = 0,
        mfxU32* ssh_offset = 0,
        mfxU32* pwt_offset = 0,
        mfxU32* pwt_length = 0);
    ENCODE_PACKEDHEADER_DATA* PackSkippedSlice(Task const & task, mfxU32 id, mfxU32* qpd_offset);

    inline mfxU32 MaxPackedHeaders() { return (mfxU32)m_buf.size(); }

private:
    std::vector<ENCODE_PACKEDHEADER_DATA>           m_buf;
    std::vector<ENCODE_PACKEDHEADER_DATA>::iterator m_cur;
    HeaderPacker m_packer;

    void NewHeader();
};

#if defined(_WIN32) || defined(_WIN64)

inline mfxU32 FeedbackSize(ENCODE_QUERY_STATUS_PARAM_TYPE func, mfxU32 maxSlices)
{
    if (func == QUERY_STATUS_PARAM_FRAME)
        return sizeof(ENCODE_QUERY_STATUS_PARAMS);
    if (func == QUERY_STATUS_PARAM_SLICE)
        return sizeof(ENCODE_QUERY_STATUS_PARAMS) + sizeof(UINT) * 4 + sizeof(USHORT) * maxSlices;
    assert(!"unknown query function");
    return sizeof(ENCODE_QUERY_STATUS_PARAMS);
}

class FeedbackStorage
{
public:
    FeedbackStorage()
        :m_size(0)
    {
    }

    void Reset(size_t cacheSize, mfxU32 feedbackSize)
    {
        m_size = feedbackSize;
        m_buf.resize(m_size * cacheSize);
    }

    inline ENCODE_QUERY_STATUS_PARAMS& operator[] (size_t i) const
    {
        return *(ENCODE_QUERY_STATUS_PARAMS*)&m_buf[i * m_size];
    }

    inline size_t size() const
    {
        return (m_buf.size() / m_size);
    }

    inline void copy(size_t dstIdx, FeedbackStorage const & src, size_t srcIdx)
    {
        CopyN(&m_buf[dstIdx * m_size], &src.m_buf[srcIdx * src.m_size], Min(m_size, src.m_size));
    }

    inline mfxU32 feedback_size()
    {
        return m_size;
    }

private:
    std::vector<mfxU8> m_buf;
    mfxU32 m_size;
};

class CachedFeedback
{
public:
    typedef ENCODE_QUERY_STATUS_PARAMS Feedback;

    void Reset(mfxU32 cacheSize, mfxU32 feedbackSize = sizeof(Feedback));

    mfxStatus Update(FeedbackStorage const & update);

    const Feedback * Hit(mfxU32 feedbackNumber) const;

    mfxStatus Remove(mfxU32 feedbackNumber);

private:
    FeedbackStorage m_cache;
};

void FillSpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC & sps);

void FillPpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & caps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps);

void FillPpsBuffer(
    Task const & task,
    ENCODE_CAPS_HEVC const & caps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps,
    std::vector<ENCODE_RECT> & dirtyRects);

void FillSliceBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & sps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice);

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice);

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)

void FillSpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT & sps);

void FillPpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & caps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT & pps);

void FillSliceBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT const & sps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC_REXT> & slice);

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC_REXT> & slice);

#endif //defined(PRE_SI_TARGET_PLATFORM_GEN11)

#endif //defined(_WIN32) || defined(_WIN64)

}; // namespace MfxHwH265Encode
#endif
