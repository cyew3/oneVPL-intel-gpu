// Copyright (c) 2006-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "umc_va_base.h"
#ifdef MFX_ENABLE_SLICE_MODE_CFG
#include "atlbase.h"
#endif // MFX_ENABLE_SLICE_MODE_CFG

#ifdef UMC_VA_DXVA

#include <vector>
#include <atlbase.h>
#include <sdkddkver.h>

#include "umc_jpeg_ddi.h"
#include "umc_svc_ddi.h"
#include "umc_vp8_ddi.h"
#include "umc_vp9_ddi.h"
#include "umc_mvc_ddi.h"
#include "umc_hevc_ddi.h"

#include "mfx_ext_ddi.h"

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
#include "umc_win_event_cache.h"
#endif

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

// {68A21C7B-D58F-4e74-9993-E4B8172B19A0}
DEFINE_GUID(DXVA_Intel_ModeVP9_Profile1_YUV444_VLD,
0x68a21c7b, 0xd58f, 0x4e74, 0x99, 0x93, 0xe4, 0xb8, 0x17, 0x2b, 0x19, 0xa0);

// {CA44AFC5-E1D0-42e6-9154-B127186D4D40}
DEFINE_GUID(DXVA_Intel_ModeAV1_VLD,
    0xca44afc5, 0xe1d0, 0x42e6, 0x91, 0x54, 0xb1, 0x27, 0x18, 0x6d, 0x4d, 0x40);

// {F9A16190-3FB4-4DC5-9846-C8751F83D6D7}
DEFINE_GUID(DXVA_Intel_ModeAV1_VLD_420_10b,
    0xf9a16190, 0x3fb4, 0x4dc5, 0x98, 0x46, 0xc8, 0x75, 0x1f, 0x83, 0xd6, 0xd7);

#if !defined(NTDDI_WIN10_VB) || (WDK_NTDDI_VERSION < NTDDI_WIN10_VB)
// {B8BE4CCB-CF53-46BA-8D59-D6B8A6DA5D2A}
DEFINE_GUID(DXVA_ModeAV1_VLD_Profile0,
    0xb8be4ccb, 0xcf53, 0x46ba, 0x8d, 0x59, 0xd6, 0xb8, 0xa6, 0xda, 0x5d, 0x2a);
#endif

// {1D5C4D76-B55A-4430-904C-3383A7AE3B16}
DEFINE_GUID(DXVA_Intel_ModeVP9_Profile3_YUV444_10bit_VLD,
0x1d5c4d76, 0xb55a, 0x4430, 0x90, 0x4c, 0x33, 0x83, 0xa7, 0xae, 0x3b, 0x16);

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

// {E484DCB8-CAC9-4859-99F5-5C0D45069089}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_Main422_10Profile,
    0xe484dcb8, 0xcac9, 0x4859, 0x99, 0xf5, 0x5c, 0xd, 0x45, 0x6, 0x90, 0x89);

// {6A6A81BA-912A-485D-B57F-CCD2D37B8D94}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_Main444_10Profile,
    0x6a6a81ba, 0x912a, 0x485d, 0xb5, 0x7f, 0xcc, 0xd2, 0xd3, 0x7b, 0x8d, 0x94);

// {8FF8A3AA-C456-4132-B6EF-69D9DD72571D}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_Main12Profile,
    0x8ff8a3aa, 0xc456, 0x4132, 0xb6, 0xef, 0x69, 0xd9, 0xdd, 0x72, 0x57, 0x1d);

// {C23DD857-874B-423C-B6E0-82CEAA9B118A}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_Main422_12Profile,
    0xc23dd857, 0x874b, 0x423c, 0xb6, 0xe0, 0x82, 0xce, 0xaa, 0x9b, 0x11, 0x8a);

// {5B08E35D-0C66-4C51-A6F1-89D00CB2C197}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_Main444_12Profile,
    0x5b08e35d, 0xc66, 0x4c51, 0xa6, 0xf1, 0x89, 0xd0, 0xc, 0xb2, 0xc1, 0x97);

// {0E4BC693-5D2C-4936-B125-AEFE32B16D8A}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_SCC_Main_Profile,
    0xe4bc693, 0x5d2c, 0x4936, 0xb1, 0x25, 0xae, 0xfe, 0x32, 0xb1, 0x6d, 0x8a);

// {2F08B5B1-DBC2-4d48-883A-4E7B8174CFF6}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_SCC_Main_10Profile,
    0x2f08b5b1, 0xdbc2, 0x4d48, 0x88, 0x3a, 0x4e, 0x7b, 0x81, 0x74, 0xcf, 0xf6);

// {5467807A-295D-445d-BD2E-CBA8C2457C3D}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_SCC_Main444_Profile,
    0x5467807a, 0x295d, 0x445d, 0xbd, 0x2e, 0xcb, 0xa8, 0xc2, 0x45, 0x7c, 0x3d);

// {AE0D4E15-2360-40a8-BF82-028E6A0DD827}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_SCC_Main444_10Profile,
    0xae0d4e15, 0x2360, 0x40a8, 0xbf, 0x82, 0x2, 0x8e, 0x6a, 0xd, 0xd8, 0x27);

// {A7F759DD-5F54-4d7f-8291-42E883C546FE}
DEFINE_GUID(DXVA_Intel_ModeVP9_Profile2_YUV420_12bit_VLD,
    0xa7f759dd, 0x5f54, 0x4d7f, 0x82, 0x91, 0x42, 0xe8, 0x83, 0xc5, 0x46, 0xfe);

// {F34FA92F-DC79-474c-B0DB-B7BD4522DF77}
DEFINE_GUID(DXVA_Intel_ModeVP9_Profile3_YUV444_12bit_VLD,
    0xf34fa92f, 0xdc79, 0x474c, 0xb0, 0xdb, 0xb7, 0xbd, 0x45, 0x22, 0xdf, 0x77);

struct GuidProfile
{
    int32_t      profile;
    GUID        guid;

    static const GuidProfile * GetGuidProfiles();
    static const GuidProfile * GetGuidProfile(size_t k);
    static size_t GetGuidProfileSize();
    static bool IsIntelCustomGUID(const GUID & guid);
    static bool IsMVCGUID(const GUID & guid);
    static bool isShortFormat(bool isHEVCGUID, uint32_t configBitstreamRaw);

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

class DXAccelerator : public VideoAccelerator
{
    DYNAMIC_CAST_DECL(DXAccelerator, VideoAccelerator);

public:

    DXAccelerator();
    virtual ~DXAccelerator();

    Status Close() override;

    void* GetCompBuffer(int32_t type, UMCVACompBuffer **buf = nullptr, int32_t size = -1, int32_t index = -1);
    Status ReleaseBuffer(int32_t type);

    virtual Status BeginFrame(int32_t  index) = 0; // Begin decoding for specified index
    // Begin decoding for specified index, keep in mind fieldId to sync correctly on task in next SyncTask call.
    Status BeginFrame(int32_t  index, uint32_t fieldId) override;

    Status SyncTask(int32_t index, void * error = nullptr) override;
    Status ExecuteStatusReportBuffer(void * buffer, int32_t size) override;
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE)
    virtual Status RegisterGpuEvent(GPU_SYNC_EVENT_HANDLE&);
    void SetGlobalHwEvent(HANDLE *globalHwEvent)
    {
        m_EventsMap.SetGlobalHwEvent(globalHwEvent);
    }
#endif

private:

    virtual Status GetCompBufferInternal(UMCVACompBuffer*) = 0;
    virtual Status ReleaseBufferInternal(UMCVACompBuffer*) = 0;

protected:

    UMCVACompBuffer* FindBuffer(int32_t type);

protected:

    UMCVACompBuffer          m_pCompBuffer[MAX_BUFFER_TYPES];
    std::vector<int32_t>     m_bufferOrder;

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
    UMC::EventCache m_EventsMap;
#endif
};

class DXVA2AcceleratorParams
    : public VideoAcceleratorParams
{

public:

    DYNAMIC_CAST_DECL(DXVA2AcceleratorParams, VideoAcceleratorParams)

    DXVA2AcceleratorParams()
        : m_device_manager(nullptr)
    {}

    IDirect3DDeviceManager9* m_device_manager;
};

class DXVA2Accelerator : public DXAccelerator
{
    DYNAMIC_CAST_DECL(DXVA2Accelerator, DXAccelerator);

public:

    DXVA2Accelerator();
    virtual ~DXVA2Accelerator();

    Status Init(VideoAcceleratorParams *pParams) override;
    Status Close() override;

    Status BeginFrame(int32_t index) override;
    Status Execute() override;
    Status ExecuteExtensionBuffer(void * buffer) override;
    Status QueryTaskStatus(int32_t , void *, void * ) override { return UMC_ERR_UNSUPPORTED;}
    Status EndFrame(void * handle = 0) override;

    bool IsIntelCustomGUID() const override;

    void GetVideoDecoder(void **handle)
    {
        *handle = m_pDXVAVideoDecoder;
    };

    Status ExecuteExtension(int, ExtensionData const&) override;

protected:

    void CloseDirectXDecoder();
    Status FindConfiguration(VideoStreamInfo const*);

private:

    Status GetCompBufferInternal(UMCVACompBuffer*) override;
    Status ReleaseBufferInternal(UMCVACompBuffer*) override;

protected:

    BOOL                    m_bInitilized;
    GUID                    m_guidDecoder;
    DXVA2_ConfigPictureDecode m_Config;
    DXVA2_VideoDesc         m_videoDesc;

    CComPtr<IDirect3DDeviceManager9>     m_pDirect3DDeviceManager9;
    CComPtr<IDirectXVideoDecoderService> m_pDecoderService;
    CComPtr<IDirectXVideoDecoder>        m_pDXVAVideoDecoder;

    HANDLE                  m_hDevice;
};

#define UMC_CHECK_HRESULT(hr, DESCRIPTION)  \
{                                           \
    vm_trace_x(hr);                         \
    /*assert(SUCCEEDED(hr));*/              \
    if (FAILED(hr)) return UMC_ERR_FAILED;  \
}

#define CHECK_HR(hr)  UMC_CHECK_HRESULT(hr, #hr)

template <typename T>
bool CheckDXVAConfig(int32_t profile_flags, T const* config, ProtectedVA * protectedVA)
{
    int32_t const profile =
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

#ifdef MFX_ENABLE_SLICE_MODE_CFG
    DWORD sliceModeSwitch = 1;
    CRegKey key;
    LONG lRes = ERROR_SUCCESS;
    lRes = key.Open(HKEY_CURRENT_USER, _T("Software\\Intel\\MediaSDK"), KEY_READ);
    if (lRes == ERROR_SUCCESS)
    {
        key.QueryDWORDValue(_T("SLICE_MODE_ENABLE"), sliceModeSwitch);
    }
#endif // MFX_ENABLE_SLICE_MODE_CFG	
	
    switch(profile)
    {
    case JPEG_VLD:
    case VP8_VLD:
    case VP9_VLD:
    case VP9_10_VLD:
    case VP9_VLD_422:
    case VP9_VLD_444:
    case VP9_10_VLD_422:
    case VP9_10_VLD_444:
    case VP9_12_VLD_420:
    case VP9_12_VLD_444:
#if defined(MFX_ENABLE_AV1_VIDEO_DECODE)
    case AV1_VLD:
    case AV1_10_VLD:
#endif
        res = true;
        break;
    case MPEG2_VLD:
    case VC1_VLD:
        res = (1 == config->ConfigBitstreamRaw);
        break;

    case H264_VLD:
    case H264_VLD_MVC:
        if (profile_flags & VA_LONG_SLICE_MODE)
            res = (H264_LONG_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw);
        else if (profile_flags & VA_SHORT_SLICE_MODE)
            res = (H264_SHORT_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw);
        else
            res = (H264_LONG_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw || H264_SHORT_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw);
        break;
    case H264_VLD_MVC_MULTIVIEW:
    case H264_VLD_MVC_STEREO:
    case H264_VLD_MVC_STEREO_PROG:
    case H264_VLD_SVC_BASELINE:
    case H264_VLD_SVC_HIGH:
        if (profile_flags & VA_LONG_SLICE_MODE)
            res = (H264_LONG_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw) || (H264_MVC_LONG_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw) || (H264_SVC_LONG_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw);
        else if (profile_flags & VA_SHORT_SLICE_MODE)
            res = (H264_SHORT_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw)|| (H264_MVC_SHORT_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw) || (H264_SVC_SHORT_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw);
        else
            res = (H264_LONG_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw || H264_SHORT_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw)||
                  (H264_MVC_LONG_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw || H264_MVC_SHORT_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw) ||
                  (H264_SVC_LONG_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw || H264_SVC_SHORT_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw);
        break;

    case H265_VLD:
    case H265_10_VLD:
    case H265_VLD    | VA_PROFILE_REXT:
    case H265_10_VLD | VA_PROFILE_REXT:
    case H265_VLD_422:
    case H265_VLD_444:
    case H265_10_VLD_422:
    case H265_10_VLD_444:
    case H265_12_VLD_420:
    case H265_12_VLD_422:
    case H265_12_VLD_444:
    case H265_VLD_SCC:
    case H265_10_VLD_SCC:
    case H265_VLD_444_SCC:
    case H265_10_VLD_444_SCC:
        if ((profile_flags & VA_PROFILE_REXT) || (profile_flags & VA_PROFILE_SCC))
#ifdef MFX_ENABLE_SLICE_MODE_CFG
            if (sliceModeSwitch)
            {
                res = (HEVC_SHORT_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw);
            }
            else
            {
                res = (HEVC_LONG_FORMAT_SLICE_NON_REXT_DATA == config->ConfigBitstreamRaw || HEVC_LONG_FORMAT_SLICE_REXT_DATA == config->ConfigBitstreamRaw);
            }
#else
            res = (HEVC_SHORT_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw);
#endif
        else
        if (profile_flags & VA_LONG_SLICE_MODE)
            res = (HEVC_LONG_FORMAT_SLICE_NON_REXT_DATA == config->ConfigBitstreamRaw || HEVC_LONG_FORMAT_SLICE_REXT_DATA == config->ConfigBitstreamRaw);
        else if (profile_flags & VA_SHORT_SLICE_MODE)
            res = (HEVC_SHORT_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw);
        else
            res = (HEVC_SHORT_FORMAT_SLICE_DATA == config->ConfigBitstreamRaw || HEVC_LONG_FORMAT_SLICE_NON_REXT_DATA == config->ConfigBitstreamRaw || HEVC_LONG_FORMAT_SLICE_REXT_DATA == config->ConfigBitstreamRaw);
        break;

    default:
        break;
    }

    return res;
};

} //namespace UMC

#endif // UMC_VA_DXVA
