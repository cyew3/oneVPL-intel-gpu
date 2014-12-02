//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
#pragma once

#include "mfx_h265_encode_hw_ddi.h"


namespace MfxHwH265Encode
{

class D3D9Encoder : public DriverEncoder, DDIHeaderPacker
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

    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC         m_sps;
    ENCODE_SET_PICTURE_PARAMETERS_HEVC          m_pps;
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC>   m_slice;
    std::vector<ENCODE_COMP_BUFFER_INFO>        m_compBufInfo;
    std::vector<D3DDDIFORMAT>                   m_uncompBufInfo;
    std::vector<ENCODE_QUERY_STATUS_PARAMS>     m_feedbackUpdate;
    CachedFeedback                              m_feedbackCached;
};

}; // namespace MfxHwH265Encode