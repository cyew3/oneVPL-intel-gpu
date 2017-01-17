//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016-2017 Intel Corporation. All Rights Reserved.
//

#pragma once

#if defined (_WIN32) || defined (_WIN64)

#include "auxiliary_device.h"
#include "encoding_ddi.h"
#include "mfx_vp9_encode_hw_ddi.h"
#include <atlbase.h>

#if defined (PRE_SI_TARGET_PLATFORM_GEN10)

namespace MfxHwVP9Encode
{
#if defined (MFX_VA_WIN)

class D3D11Encoder : public DriverEncoder
{
public:
    D3D11Encoder();

    virtual
    ~D3D11Encoder();

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
    mfxStatus QueryStatus(
        Task & task);

    virtual
        mfxU32 GetReconSurfFourCC();

    virtual
    mfxStatus Destroy();

private:
    D3D11Encoder(const D3D11Encoder&);
    D3D11Encoder& operator=(const D3D11Encoder&);

    GUID m_guid;

    CComPtr<ID3D11DeviceContext>                m_pContext;
    CComPtr<ID3D11VideoDecoder>                 m_pVDecoder;
    CComQIPtr<ID3D11VideoDevice>                m_pVDevice;
    CComQIPtr<ID3D11VideoContext>               m_pVContext;


    mfxCoreInterface*  m_pmfxCore;
    ENCODE_CAPS_VP9    m_caps;

    ENCODE_SET_SEQUENCE_PARAMETERS_VP9 m_sps;
    ENCODE_SET_PICTURE_PARAMETERS_VP9  m_pps;
    std::vector<ENCODE_QUERY_STATUS_PARAMS>     m_feedbackUpdate;
    CachedFeedback                              m_feedbackCached;

    std::vector<mfxHDLPair>                     m_reconQueue;
    std::vector<mfxHDLPair>                     m_bsQueue;

    bool                                        m_infoQueried;
    std::vector<ENCODE_COMP_BUFFER_INFO>        m_compBufInfo;
    std::vector<D3DDDIFORMAT>                   m_uncompBufInfo; // isn't used. Required just to query buffer info from driver.

    std::vector<mfxU8> m_frameHeaderBuf;
    ENCODE_PACKEDHEADER_DATA m_descForFrameHeader;

    VP9SeqLevelParam m_seqParam;

    mfxU32 m_width;
    mfxU32 m_height;

    UMC::Mutex m_guard;
};
#endif // (MFX_VA_LINUX)
} // MfxHwVP9Encode

#endif // (_WIN32) || (_WIN64)

#endif // PRE_SI_TARGET_PLATFORM_GEN10
