//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.
//
#pragma once

#if defined(_WIN32) || defined(_WIN64)

#include "auxiliary_device.h"
#include "mfx_h265_encode_hw_ddi.h"
#include "mfx_h265_encode_hw_ddi_trace.h"

namespace MfxHwH265Encode
{

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

void FillSpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC & sps);

void FillPpsBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps);

void FillPpsBuffer(
    Task const & task,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps);

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

class D3D9Encoder : public DriverEncoder, DDIHeaderPacker, DDITracer
{
public:
    D3D9Encoder();

    virtual
    ~D3D9Encoder();

    virtual
    mfxStatus CreateAuxilliaryDevice(
        MFXCoreInterface * core,
        GUID        guid,
        mfxU32      width,
        mfxU32      height);

    virtual
    mfxStatus CreateAccelerationService(
        MfxVideoParam const & par);

    virtual
    mfxStatus Reset(
        MfxVideoParam const & par);

    virtual
    mfxStatus Register(
        mfxFrameAllocResponse & response,
        D3DDDIFORMAT            type);

    virtual
    mfxStatus Execute(
        Task const &task,
        mfxHDL surface);

    virtual
    mfxStatus QueryCompBufferInfo(
        D3DDDIFORMAT           type,
        mfxFrameAllocRequest & request);

    virtual
    mfxStatus QueryEncodeCaps(
        ENCODE_CAPS_HEVC & caps);


    virtual
    mfxStatus QueryStatus(
        Task & task);

    virtual
    mfxStatus Destroy();

    virtual
    ENCODE_PACKEDHEADER_DATA* PackHeader(Task const & task, mfxU32 nut)
    {
        return DDIHeaderPacker::PackHeader(task, nut);
    }

private:
    MFXCoreInterface*              m_core;
    std::auto_ptr<AuxiliaryDevice> m_auxDevice;

    GUID                 m_guid;
    mfxU32               m_width;
    mfxU32               m_height;
    ENCODE_CAPS_HEVC     m_caps;
    ENCODE_ENC_CTRL_CAPS m_capsQuery;
    ENCODE_ENC_CTRL_CAPS m_capsGet;
    bool                 m_infoQueried;
    bool                 m_widi;
    mfxU32               m_maxSlices;

    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC         m_sps;
    ENCODE_SET_PICTURE_PARAMETERS_HEVC          m_pps;
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC>   m_slice;
    std::vector<ENCODE_COMP_BUFFER_INFO>        m_compBufInfo;
    std::vector<D3DDDIFORMAT>                   m_uncompBufInfo;
    std::vector<ENCODE_COMPBUFFERDESC>          m_cbd;
    FeedbackStorage                             m_feedbackUpdate;
    CachedFeedback                              m_feedbackCached;
};

}; // namespace MfxHwH265Encode

#endif // #if defined(_WIN32) || defined(_WIN64)