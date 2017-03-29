//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2017 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include <vector>

#include "umc_jpeg_ddi.h"
#include "umc_svc_ddi.h"
#include "umc_vp8_ddi.h"
#include "umc_vp9_ddi.h"
#include "umc_mvc_ddi.h"
#define DEFINE_GUID_(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

/* The same guids w/o prefix 's' should be in dxva.lib */
DEFINE_GUID_(sDXVA2_ModeMPEG2_IDCT,        0xbf22ad00,0x03ea,0x4690,0x80,0x77,0x47,0x33,0x46,0x20,0x9b,0x7e);
DEFINE_GUID_(sDXVA2_ModeMPEG2_VLD,         0xee27417f,0x5e28,0x4e65,0xbe,0xea,0x1d,0x26,0xb5,0x08,0xad,0xc9);
DEFINE_GUID_(sDXVA2_ModeH264_MoComp_NoFGT, 0x1b81be64,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID_(sDXVA2_ModeH264_VLD_NoFGT,    0x1b81be68,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);

DEFINE_GUID_(sDXVA2_ModeVC1_MoComp,        0x1b81beA1,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID_(sDXVA2_ModeVC1_VLD,           0x1b81beA3,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID_(sDXVA2_ModeMPEG4_VLD,         0x8ccf025a,0xbacf,0x4e44,0x81,0x16,0x55,0x21,0xd9,0xb3,0x94,0x07);

DEFINE_GUID_(sDXVA2_Intel_ModeVC1_D,       0xBCC5DB6D,0xA2B6,0x4AF0,0xAC, 0xE4,0xAD,0xB1,0xF7,0x87,0xBC,0x89);
DEFINE_GUID_(sDXVA2_Intel_IVB_ModeJPEG_VLD_NoFGT,    0x91cd2d6e,0x897b,0x4fa1,0xb0,0xd7,0x51,0xdc,0x88,0x01,0x0e,0x0a);
DEFINE_GUID_(sDXVA_Intel_ModeVP8_VLD, 0x442b942a, 0xb4d9, 0x4940, 0xbc, 0x45, 0xa8, 0x82, 0xe5, 0xf9, 0x19, 0xf3);
DEFINE_GUID_(sDXVA_Intel_ModeVP9_VLD, 0x76988a52, 0xdf13, 0x419a, 0x8e, 0x64, 0xff, 0xcf, 0x4a, 0x33, 0x6c, 0xf5);

// Intel defined GUID for AVC Elementary Stream Decoding
// {C528916C-C0AF-4645-8CB2-372B6D4ADC2A}
DEFINE_GUID(DXVA_Intel_Decode_Elementary_Stream_AVC, 0xc528916c, 0xc0af, 0x4645, 0x8c, 0xb2, 0x37, 0x2b, 0x6d, 0x4a, 0xdc, 0x2a);

// Intel defined GUID for HEVC Elementary Stream Decoding
// {07CFAFFB-5A2E-4B99-B62A-E4CA53B6D5AA}
DEFINE_GUID(DXVA_Intel_Decode_Elementary_Stream_HEVC, 0x7cfaffb, 0x5a2e, 0x4b99, 0xb6, 0x2a, 0xe4, 0xca, 0x53, 0xb6, 0xd5, 0xaa);

/* Intel VP9 VLD GUIDs (VP9 decoding DDI version 0.88) */
// {76988A52-DF13-419A-8E64-FFCF4A336CF5}
DEFINE_GUID(DXVA_Intel_ModeVP9_Profile0_VLD, 
0x76988a52, 0xdf13, 0x419a, 0x8e, 0x64, 0xff, 0xcf, 0x4a, 0x33, 0x6c, 0xf5);

// {80A3A7BD-89D8-4497-A2B8-2126AF7E6EB8}
DEFINE_GUID(DXVA_Intel_ModeVP9_Profile2_10bit_VLD, 
0x80a3a7bd, 0x89d8, 0x4497, 0xa2, 0xb8, 0x21, 0x26, 0xaf, 0x7e, 0x6e, 0xb8);

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
// {68A21C7B-D58F-4e74-9993-E4B8172B19A0}
DEFINE_GUID(DXVA_Intel_ModeVP9_Profile1_YUV444_VLD, 
0x68a21c7b, 0xd58f, 0x4e74, 0x99, 0x93, 0xe4, 0xb8, 0x17, 0x2b, 0x19, 0xa0);

// {1D5C4D76-B55A-4430-904C-3383A7AE3B16}
DEFINE_GUID(DXVA_Intel_ModeVP9_Profile3_YUV444_10bit_VLD, 
0x1d5c4d76, 0xb55a, 0x4430, 0x90, 0x4c, 0x33, 0x83, 0xa7, 0xae, 0x3b, 0x16);
#endif //PRE_SI_TARGET_PLATFORM_GEN11

DEFINE_GUID(DXVA_ModeVP9_VLD_10bit_Profile2_private_copy,
0xa4c749ef, 0x6ecf, 0x48aa, 0x84, 0x48, 0x50, 0xa7, 0xa1, 0x16, 0x5f, 0xf7);

DEFINE_GUID_(sDXVA_Intel_ModeH264_VLD_MVC, 0xe296bf50, 0x8808, 0x4ff8, 0x92, 0xd4, 0xf1, 0xee, 0x79, 0x9f, 0xc3, 0x3c);

DEFINE_GUID_(sDXVA2_Intel_ModeVC1_D_Super,       0xE07EC519,0xE651,0x4cd6,0xAC, 0x84,0x13,0x70,0xCC,0xEE,0xC8,0x51);

DEFINE_GUID_(sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_Baseline,          0x9b8175d4, 0xd670, 0x4cf2, 0xa9, 0xf0, 0xfa, 0x56, 0xdf, 0x71, 0xa1, 0xae);
DEFINE_GUID_(sDXVA_ModeH264_VLD_SVC_Scalable_Baseline,                      0xc30700c4, 0xe384, 0x43e0, 0xb9, 0x82, 0x2d, 0x89, 0xee, 0x7f, 0x77, 0xc4);
DEFINE_GUID_(sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_High,              0x8efa5926, 0xbd9e, 0x4b04, 0x8b, 0x72, 0x8f, 0x97, 0x7d, 0xc4, 0x4c, 0x36);
DEFINE_GUID_(sDXVA_ModeH264_VLD_SVC_Scalable_High,                          0x728012c9, 0x66a8, 0x422f, 0x97, 0xe9, 0xb5, 0xe3, 0x9b, 0x51, 0xc0, 0x53);

DEFINE_GUID_(sDXVA_ModeH264_VLD_Multiview_NoFGT,                  0x705b9d82, 0x76cf, 0x49d6, 0xb7, 0xe6, 0xac, 0x88, 0x72, 0xdb, 0x01, 0x3c);
DEFINE_GUID_(sDXVA_ModeH264_VLD_Stereo_NoFGT,                     0xf9aaccbb, 0xc2b6, 0x4cfc, 0x87, 0x79, 0x57, 0x07, 0xb1, 0x76, 0x05, 0x52);
DEFINE_GUID_(sDXVA_ModeH264_VLD_Stereo_Progressive_NoFGT,         0xd79be8da, 0x0cf1, 0x4c81, 0xb8, 0x2a, 0x69, 0xa4, 0xe2, 0x36, 0xf4, 0x3d);

DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_MainProfile,  0x8c56eb1e, 0x2b47, 0x466f, 0x8d, 0x33, 0x7d, 0xbc, 0xd6, 0x3f, 0x3d, 0xf2);
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_Main10Profile, 0x75fc75f7, 0xc589, 0x4a07, 0xa2, 0x5b, 0x72, 0xe0, 0x3b, 0x03, 0x83, 0xb3);

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
// {E484DCB8-CAC9-4859-99F5-5C0D45069089}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_Main422_10Profile, 
0xe484dcb8, 0xcac9, 0x4859, 0x99, 0xf5, 0x5c, 0xd, 0x45, 0x6, 0x90, 0x89);

// {6A6A81BA-912A-485D-B57F-CCD2D37B8D94}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_Main444_10Profile, 
0x6a6a81ba, 0x912a, 0x485d, 0xb5, 0x7f, 0xcc, 0xd2, 0xd3, 0x7b, 0x8d, 0x94);
#endif //PRE_SI_TARGET_PLATFORM_GEN11

struct GuidProfile
{
    Ipp32s      profile;
    GUID        guid;

    static const GuidProfile * GetGuidProfiles();
    static const GuidProfile * GetGuidProfile(size_t k);
    static size_t GetGuidProfileSize();
    static bool IsIntelCustomGUID(const GUID & guid);
    static bool IsMVCGUID(const GUID & guid);
    static bool isShortFormat(bool isHEVCGUID, Ipp32u configBitstreamRaw);

    bool IsIntelCustomGUID() const
    { return IsIntelCustomGUID(guid); }
};

enum eUMC_DirectX_Status
{
    E_FRAME_LOCKED = 0xC0262111
};

namespace UMC
{

///////////////////////////////////////////////////////////////////////////////////
class DXVA2Accelerator : public VideoAccelerator
{
    DYNAMIC_CAST_DECL(DXVA2Accelerator, VideoAccelerator);

public:
    DXVA2Accelerator();
    virtual ~DXVA2Accelerator();

    virtual Status Init(VideoAcceleratorParams *pParams);
    virtual Status CloseDirectXDecoder();

    virtual Status Close(void);

    virtual Status BeginFrame(Ipp32s index);
    virtual void* GetCompBuffer(Ipp32s buffer_type, UMCVACompBuffer **buf = NULL, Ipp32s size = -1, Ipp32s index = -1);
    virtual Status Execute();
    virtual Status ExecuteExtensionBuffer(void * buffer);
    virtual Status ExecuteStatusReportBuffer(void * buffer, Ipp32s size);
    virtual Status SyncTask(Ipp32s index, void * error = NULL) { index; error; return UMC_ERR_UNSUPPORTED;}
    virtual Status QueryTaskStatus(Ipp32s , void *, void * ) { return UMC_ERR_UNSUPPORTED;}
    virtual Status ReleaseBuffer(Ipp32s type);
    virtual Status EndFrame(void * handle = 0);

    virtual bool IsIntelCustomGUID() const;

    void SetDeviceManager(IDirect3DDeviceManager9 *dm)
    {
        m_bIsExtManager = true;
        m_pDirect3DDeviceManager9=dm;
    };
    void GetVideoDecoder(void **handle)
    {
        *handle = m_pDXVAVideoDecoder;
    };

protected:

    Status FindConfiguration(VideoStreamInfo *pVideoInfo);

    HRESULT CreateDecoder(IDirectXVideoDecoderService   *m_pDecoderService,
                          GUID                          guid,
                          DXVA2_VideoDesc               *pVideoDesc,
                          DXVA2_ConfigPictureDecode     *pDecoderConfig,
                          int                           cNumberSurfaces);

    BOOL                    m_bInitilized;
    GUID                    m_guidDecoder;
    DXVA2_ConfigPictureDecode m_Config;
    DXVA2_VideoDesc         m_videoDesc;
    UMCVACompBuffer         m_pCompBuffer[MAX_BUFFER_TYPES];

    IDirect3DDeviceManager9 *m_pDirect3DDeviceManager9;
    IDirectXVideoDecoderService *m_pDecoderService;
    IDirectXVideoDecoder    *m_pDXVAVideoDecoder;
    HANDLE                  m_hDevice;

protected:
    bool    m_bIsExtManager;

    std::vector<Ipp32s>  m_bufferOrder;
};

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x) { x->Release(); x = NULL; }
#endif

#define UMC_CHECK_HRESULT(hr, DESCRIPTION)  \
{                                           \
    vm_trace_x(hr);                         \
    /*assert(SUCCEEDED(hr));*/              \
    if (FAILED(hr)) return UMC_ERR_FAILED;  \
}

#define CHECK_HR(hr)  UMC_CHECK_HRESULT(hr, #hr)

template <typename T>
bool CheckDXVAConfig(Ipp32s profile_flags, T *config, ProtectedVA * protectedVA)
{
    Ipp32s const profile = 
        (profile_flags & (VA_ENTRY_POINT | VA_CODEC));

    if (protectedVA)
    {
        if (config->guidConfigBitstreamEncryption != protectedVA->GetEncryptionGUID())
            return false;
    }
    else
    {
        /*if (pConfig->guidConfigBitstreamEncryption != DXVA_NoEncrypt ||
            pConfig->guidConfigMBcontrolEncryption != DXVA_NoEncrypt ||
            pConfig->guidConfigResidDiffEncryption != DXVA_NoEncrypt)
            return false;*/
    }

    bool res = false;
    switch(profile)
    {
    case JPEG_VLD:
    case VP8_VLD:
    case VP9_VLD:
    case VP9_10_VLD:
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    case VP9_VLD_422:
    case VP9_VLD_444:
    case VP9_10_VLD_422:
    case VP9_10_VLD_444:
#endif //PRE_SI_TARGET_PLATFORM_GEN11
        res = true;
        break;
    case MPEG2_VLD:
    case VC1_VLD:
        res = (1 == config->ConfigBitstreamRaw);
        break;

    case H264_VLD:
    case H264_VLD_MVC:
        if (profile_flags & VA_LONG_SLICE_MODE)
            res = (1 == config->ConfigBitstreamRaw);
        else if (profile_flags & VA_SHORT_SLICE_MODE)
            res = (2 == config->ConfigBitstreamRaw);
        else
            res = (1 == config->ConfigBitstreamRaw || 2 == config->ConfigBitstreamRaw);
        break;
    case H264_VLD_MVC_MULTIVIEW:
    case H264_VLD_MVC_STEREO:
    case H264_VLD_MVC_STEREO_PROG:
    case H264_VLD_SVC_BASELINE:
    case H264_VLD_SVC_HIGH:
        if (profile_flags & VA_LONG_SLICE_MODE)
            res = (1 == config->ConfigBitstreamRaw) || (3 == config->ConfigBitstreamRaw) || (5 == config->ConfigBitstreamRaw);
        else if (profile_flags & VA_SHORT_SLICE_MODE)
            res = (2 == config->ConfigBitstreamRaw) || (4 == config->ConfigBitstreamRaw) || (6 == config->ConfigBitstreamRaw);
        else
            res = (1 == config->ConfigBitstreamRaw || 2 == config->ConfigBitstreamRaw) || (3 == config->ConfigBitstreamRaw || 4 == config->ConfigBitstreamRaw) ||
             (5 == config->ConfigBitstreamRaw || 6 == config->ConfigBitstreamRaw);
        break;

    case H265_VLD:
    case H265_10_VLD:
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    case H265_VLD    | VA_PROFILE_REXT:
    case H265_10_VLD | VA_PROFILE_REXT:
    case H265_VLD_422:
    case H265_VLD_444:
    case H265_10_VLD_422:
    case H265_10_VLD_444:
#endif //PRE_SI_TARGET_PLATFORM_GEN11
        if (profile_flags & VA_LONG_SLICE_MODE)
            res = (2 == config->ConfigBitstreamRaw || 3 == config->ConfigBitstreamRaw);
        else if (profile_flags & VA_SHORT_SLICE_MODE)
            res = (1 == config->ConfigBitstreamRaw);
        else
            res = (1 == config->ConfigBitstreamRaw || 2 == config->ConfigBitstreamRaw || 3 == config->ConfigBitstreamRaw);
        break;

    default:
        break;
    }

    return res;
};

} //namespace UMC

#endif // UMC_VA_DXVA
