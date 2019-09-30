// Copyright (c) 2006-2019 Intel Corporation
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

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include "umc_va_dxva2.h"
#include "umc_mf_decl.h"
#include "vm_time.h"
#include "mfx_trace.h"

#include "umc_va_dxva2_protected.h"
#include "umc_va_video_processing.h"

#include "umc_frame_allocator.h"

#include <algorithm>

#define OVERFLOW_CHECK_VALUE    0x11

using namespace UMC;

/////////////////////////////////////////////////

namespace UMC
{
    DXVA2Accelerator *CreateDXVA2Accelerator()
    {
        MFX_LTRACE_S(MFX_TRACE_LEVEL_EXTCALL, "CreateDXVA2Accelerator()");
        return (new DXVA2Accelerator);
    }
}

DXAccelerator::DXAccelerator()
{
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
    m_EventsMap.Init();
#endif
}

DXAccelerator::~DXAccelerator()
{
    Close();
}

UMC::Status DXAccelerator::Close()
{
    return VideoAccelerator::Close();
}

// Begin decoding for specified index, keep in mind fieldId to sync correctly on task in next SyncTask call.
Status DXAccelerator::BeginFrame(int32_t  index, uint32_t fieldId)
{
    (void)fieldId;

    Status sts = BeginFrame(index);
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
    if (sts != UMC_OK || !IsGPUSyncEventEnable())
        return sts;
    GPU_SYNC_EVENT_HANDLE ev{ GPU_COMPONENT_DECODE };
    ev.gpuSyncEvent = m_EventsMap.GetFreeEventAndMap( index, fieldId);

    if (ev.gpuSyncEvent == INVALID_HANDLE_VALUE)
        return UMC_ERR_INVALID_PARAMS;

    sts = RegisterGpuEvent(ev);
#ifdef  _DEBUG
    MFX_TRACE_3("RegisterGpuEvent", "index=%d, fieldId=%d event %p", index, fieldId, ev.gpuSyncEvent);
#endif
#endif
    return sts;
}

Status DXAccelerator::SyncTask(int32_t index, void * error)
{
    (void)index;
    (void)error;
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
    if (!IsGPUSyncEventEnable())
    {
        return UMC_ERR_UNSUPPORTED;
    }
    const uint32_t timeoutms = 60000; // TIMEOUT FOR DECODE OPERATION = 1 min
    const size_t MAX_FIELD_SUPPORTED = 2;

    Status sts = UMC_OK;

    HANDLE handle[MAX_FIELD_SUPPORTED] = { INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE };
    size_t count = 0;

    EventCache::MapValue events = m_EventsMap.GetEvents(index);
    if (sts != UMC_OK)
        return sts;

    if (events[0] != INVALID_HANDLE_VALUE)
    {
        handle[count]=events[0];
        count++;
    }
    if (events[1] != INVALID_HANDLE_VALUE)
    {
        handle[count] = events[1];
        count++;
    }

    if (count == 0 )
        return UMC::UMC_ERR_INVALID_PARAMS;

#ifdef  _DEBUG
    MFX_TRACE_3("WaitForMultipleObjects", "(count = %d handle[0] = %p handle[1] = %p)", (int)count, handle[0], handle[1]);
#endif
    HRESULT waitRes = WaitForMultipleObjects((DWORD)count, handle, TRUE, timeoutms);

#ifdef  _DEBUG
    MFX_TRACE_2("WaitForMultipleObjects", "index = %d HR=%d", index, waitRes);
#endif

    if (WAIT_TIMEOUT == waitRes)
    {
        return UMC::UMC_ERR_TIMEOUT;
    }
    else if (WAIT_OBJECT_0 != waitRes)
    {
        sts = UMC::UMC_ERR_DEVICE_FAILED;
    }

    (void)m_EventsMap.FreeEvents(index);

    return sts;
#else
    return UMC_ERR_UNSUPPORTED;
#endif
}

void* DXAccelerator::GetCompBuffer(int32_t type, UMCVACompBuffer **buf, int32_t /*size*/, int32_t /*index*/)
{
    UMCVACompBuffer* buffer = FindBuffer(type);
    UMC_CHECK(buffer, NULL);

    if (!buffer->GetPtr())
    {
        buffer->type = type;
        Status sts = GetCompBufferInternal(buffer);
        UMC_CHECK(sts == UMC_OK, NULL);

        m_bufferOrder.push_back(type);
    }

    uint8_t* data = reinterpret_cast<uint8_t*>(buffer->GetPtr());
    UMC_CHECK(data, NULL);

#if defined(DEBUG) || defined(_DEBUG)
    int32_t const buffer_size = buffer->GetBufferSize();
    data[buffer_size - 1] = OVERFLOW_CHECK_VALUE;
#endif

    if (buf)
    {
        *buf = buffer;
    }

    return data;
}

//////////////////////////////////////////////////////////////

Status DXAccelerator::ReleaseBuffer(int32_t type)
{
    UMCVACompBuffer* buffer = FindBuffer(type);
    if (!buffer)
        return UMC_ERR_FAILED;

#if defined(DEBUG) || defined(_DEBUG)
    if (type == DXVA_SLICE_CONTROL_BUFFER ||
        type == DXVA_BITSTREAM_DATA_BUFFER ||
        type == DXVA_MACROBLOCK_CONTROL_BUFFER ||
        type == DXVA_RESIDUAL_DIFFERENCE_BUFFER)
    {
        uint8_t const* data = reinterpret_cast<uint8_t const*>(buffer->GetPtr());
        int32_t const buffer_size = buffer->GetBufferSize();

        if (data[buffer_size - 1] != OVERFLOW_CHECK_VALUE)
        {
            vm_debug_trace4(VM_DEBUG_ERROR, VM_STRING("Buffer%d overflow!!! %x[%d] = %x\n"), type, data, buffer_size, data[buffer_size - 1]);
        }

        //VM_ASSERT(pBuffer[buffer_size - 1] == OVERFLOW_CHECK_VALUE);
    }
#endif

    return ReleaseBufferInternal(buffer);
}

Status DXAccelerator::ExecuteStatusReportBuffer(void * buffer, int32_t size)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "DXAccelerator::ExecuteStatusReportBuffer");

    UMC_CHECK_PTR(buffer);


    ExtensionData ext{};
    ext.output = std::make_pair(buffer, size);
    return ExecuteExtension(DXVA2_GET_STATUS_REPORT, ext);
}

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
Status DXAccelerator::RegisterGpuEvent(GPU_SYNC_EVENT_HANDLE &ev)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "DXAccelerator::RegisterGpuEvent");
    ExtensionData ext{};
    ext.input = std::make_pair(&ev, sizeof(ev));
    return ExecuteExtension(DXVA2_SET_GPU_TASK_EVENT_HANDLE, ext);
//#else
//    (void)ev;
//    return MFX_ERR_NONE;
//#endif
}
#endif

inline
int32_t GetBufferIndex(int32_t buffer_type)
{
    //use incoming [buffer_type] as direct index in [UMCVACompBuffer] if it less then DXVA_NUM_TYPES_COMP_BUFFERS,
    //otherwise re-index it in a way to fall in a range (DXVA_NUM_TYPES_COMP_BUFFERS, MAX_BUFFER_TYPES)
    return buffer_type > DXVA_NUM_TYPES_COMP_BUFFERS ?
        DXVA_NUM_TYPES_COMP_BUFFERS + (buffer_type % (MAX_BUFFER_TYPES - DXVA_NUM_TYPES_COMP_BUFFERS)) :
        buffer_type
        ;
}

UMCVACompBuffer* DXAccelerator::FindBuffer(int32_t type)
{
    int32_t const buffer_index = GetBufferIndex(type);

    UMC_CHECK(buffer_index >= 0, nullptr);
    UMC_CHECK(buffer_index < MAX_BUFFER_TYPES, nullptr);

    return
        &m_pCompBuffer[buffer_index];
}

DXVA2Accelerator::DXVA2Accelerator()
    : m_hDevice(INVALID_HANDLE_VALUE)
    , m_bInitilized(FALSE)
    , m_guidDecoder(GUID_NULL)
{
    memset(&m_videoDesc, 0, sizeof(m_videoDesc));
    memset(&m_Config, 0, sizeof(m_Config));
}

//////////////////////////////////////////////////////////////


void DXVA2Accelerator::CloseDirectXDecoder()
{
    m_pDXVAVideoDecoder.Release();
    m_pDecoderService.Release();

    m_bInitilized = FALSE;

    if (m_hDevice != INVALID_HANDLE_VALUE)
    {
        VM_ASSERT(m_pDirect3DDeviceManager9);

        m_pDirect3DDeviceManager9->CloseDeviceHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
}

DXVA2Accelerator::~DXVA2Accelerator()
{
    CloseDirectXDecoder();

    m_pDirect3DDeviceManager9.Release();
}

//////////////////////////////////////////////////////////////

Status DXVA2Accelerator::BeginFrame(int32_t index)
{
    IDirect3DSurface9* surface;
    Status sts = m_allocator->GetFrameHandle(index, &surface);
    if (sts != UMC_OK)
        return sts;

    HRESULT hr;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "BeginFrame");

        for (uint32_t i = 0; i < 200; i++)
        {
            hr = m_pDXVAVideoDecoder->BeginFrame(surface, nullptr);

            if (E_PENDING != hr)
                break;

            // sleep some time
            vm_time_sleep(5);
        }
    }

    return
        SUCCEEDED(hr) ? UMC_OK: UMC_ERR_DEVICE_FAILED;
}

//////////////////////////////////////////////////////////////

Status DXVA2Accelerator::EndFrame(void * handle)
{
    std::for_each(std::begin(m_bufferOrder), std::end(m_bufferOrder),
        [this](int32_t type)
        { ReleaseBuffer(type);  }
    );

    m_bufferOrder.clear();

    HRESULT hr;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "BeginFrame");
        hr = m_pDXVAVideoDecoder->EndFrame(reinterpret_cast<HANDLE*>(handle));
    }

    return
        SUCCEEDED(hr) ? UMC_OK: UMC_ERR_DEVICE_FAILED;
}

//////////////////////////////////////////////////////////////
Status DXVA2Accelerator::Execute()
{
    DXVA2_DecodeBufferDesc pBufferDesc[MAX_BUFFER_TYPES];
    DXVA2_DecodeExecuteParams executeParams{};
    int n = 0;

    ZeroMemory(pBufferDesc, sizeof(pBufferDesc));

    for (int32_t const type : m_bufferOrder)
    {
        UMCVACompBuffer const* pCompBuffer = FindBuffer(type);

        if (!pCompBuffer) continue;
        if (!pCompBuffer->GetPtr()) continue;
        if (!pCompBuffer->GetBufferSize()) continue;

        int FirstMb = 0, NumOfMB = 0;

        if (pCompBuffer->FirstMb > 0) FirstMb = pCompBuffer->FirstMb;
        if (pCompBuffer->NumOfMB > 0) NumOfMB = pCompBuffer->NumOfMB;

        pBufferDesc[n].CompressedBufferType = pCompBuffer->type - 1;
        pBufferDesc[n].NumMBsInBuffer = NumOfMB;
        pBufferDesc[n].DataSize = pCompBuffer->GetDataSize();
        pBufferDesc[n].BufferIndex = 0;
        pBufferDesc[n].DataOffset = 0;
        pBufferDesc[n].FirstMBaddress = FirstMb;
        pBufferDesc[n].Width = 0;
        pBufferDesc[n].Height = 0;
        pBufferDesc[n].Stride = 0;
        pBufferDesc[n].ReservedBits = 0;
        pBufferDesc[n].pvPVPState = pCompBuffer->GetPVPStatePtr();
        n++;

        ReleaseBuffer(type);
    }

    executeParams.NumCompBuffers = n;
    executeParams.pCompressedBuffers = pBufferDesc;
    executeParams.pExtensionData = nullptr;

    m_bufferOrder.clear();

    HRESULT hr = E_FRAME_LOCKED;
    mfxU32 counter = 0;
    while (E_FRAME_LOCKED == hr && counter++ < 4) // 20 ms should be enough
    {
        //MFX_AUTO_LTRACE_WITHID(MFX_TRACE_LEVEL_EXTCALL, "DXVA2_DecodeExecute");
        hr = m_pDXVAVideoDecoder->Execute(&executeParams);

        if (hr != 0)
        {
            MFX_LTRACE_I(MFX_TRACE_LEVEL_EXTCALL, hr);
        }

        //if (hr == E_FRAME_LOCKED)
        //{
        //    EndFrame();
        //    BeginFrame(m_BeginFrameIndex);
        //}

        if (E_FRAME_LOCKED == hr)
            Sleep(5); //lets wait 5 ms
    }

    if (hr == E_FRAME_LOCKED)
        return UMC::UMC_ERR_FRAME_LOCKED;

    if (FAILED(hr))
        return UMC::UMC_ERR_DEVICE_FAILED;

    CHECK_HR(hr);
    return UMC_OK;
}

Status DXVA2Accelerator::ExecuteExtensionBuffer(void * buffer)
{
    UMC_CHECK_PTR(buffer);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "DXVA2Accelerator::ExecuteExtensionBuffer");

    DXVA2_DecodeExecuteParams ext{};
    ext.pExtensionData = reinterpret_cast<DXVA2_DecodeExtensionData*>(buffer);

    HRESULT hr;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "Execute");
        hr = m_pDXVAVideoDecoder->Execute(&ext);
    }

    return
        SUCCEEDED(hr) ? UMC_OK: UMC_ERR_DEVICE_FAILED;
}

Status DXVA2Accelerator::ExecuteExtension(int function, ExtensionData const& data)
{
    DXVA2_DecodeExtensionData ext{ UINT(function) };
    ext.pPrivateInputData     = data.input.first;
    ext.PrivateInputDataSize  = UINT(data.input.second);
    ext.pPrivateOutputData    = data.output.first;
    ext.PrivateOutputDataSize = UINT(data.output.second);

    return ExecuteExtensionBuffer(&ext);
}

Status DXVA2Accelerator::GetCompBufferInternal(UMCVACompBuffer* buffer)
{
    VM_ASSERT(buffer);

    int32_t const type = buffer->GetType();

    void* data = NULL;
    UINT buffer_size = 0;
    HRESULT hr = m_pDXVAVideoDecoder->GetBuffer(type - 1, &data, &buffer_size);
    if (FAILED(hr))
    {
        vm_trace_x(hr);
        VM_ASSERT(SUCCEEDED(hr));

        return UMC_ERR_FAILED;
    }

    buffer->SetBufferPointer(reinterpret_cast<uint8_t*>(data), buffer_size);
    buffer->SetDataSize(0);

    return UMC_OK;
}

Status DXVA2Accelerator::ReleaseBufferInternal(UMCVACompBuffer* buffer)
{
    VM_ASSERT(buffer);

    int32_t const type = buffer->GetType();
    HRESULT hr = m_pDXVAVideoDecoder->ReleaseBuffer(type - 1);
    CHECK_HR(hr);

    buffer->SetBufferPointer(NULL, 0);

    return UMC_OK;
}

//////////////////////////////////////////////////////////////
static const GuidProfile guidProfiles[] =
{
    { MPEG2_VLD,                                                                sDXVA2_ModeMPEG2_VLD },
    { H264_VLD,                                                                 sDXVA2_ModeH264_VLD_NoFGT },

    { VC1_VLD,                                                                  sDXVA2_Intel_ModeVC1_D_Super},

    { JPEG_VLD,                                                                 sDXVA2_Intel_IVB_ModeJPEG_VLD_NoFGT},
    { VP8_VLD,                                                                  sDXVA_Intel_ModeVP8_VLD},

#if defined(NTDDI_WIN10_TH2)
    { VP9_VLD,                                                                  DXVA_ModeVP9_VLD_Profile0},
    { VP9_VLD | VA_PROFILE_10,                                                  DXVA_ModeVP9_VLD_10bit_Profile2},
#endif
    { VP9_VLD,                                                                  DXVA_Intel_ModeVP9_Profile0_VLD },
    { VP9_VLD | VA_PROFILE_10,                                                  DXVA_Intel_ModeVP9_Profile2_10bit_VLD },

    //{ VP9_VLD_422,                                                              DXVA_Intel_ModeVP9_Profile1_YUV422_VLD },
    { VP9_VLD_444,                                                              DXVA_Intel_ModeVP9_Profile1_YUV444_VLD },
    //{ VP9_10_VLD_422,                                                           DXVA_Intel_ModeVP9_Profile3_YUV422_10bit_VLD },
    { VP9_10_VLD_444,                                                           DXVA_Intel_ModeVP9_Profile3_YUV444_10bit_VLD },

#if (MFX_VERSION >= MFX_VERSION_NEXT)
    { VP9_12_VLD_420,                                                           DXVA_Intel_ModeVP9_Profile2_YUV420_12bit_VLD },
    { VP9_12_VLD_444,                                                           DXVA_Intel_ModeVP9_Profile3_YUV444_12bit_VLD },
#endif
#ifdef MFX_ENABLE_AV1_VIDEO_DECODE
    { AV1_VLD,                                                                  DXVA_Intel_ModeAV1_VLD },
    { AV1_10_VLD,                                                               DXVA_Intel_ModeAV1_VLD_420_10b },
#endif

    { H264_VLD        | VA_PROFILE_SVC_HIGH,                                    sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_High },
    { H264_VLD        | VA_PROFILE_SVC_BASELINE,                                sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_Baseline },

    { H264_VLD        | VA_PROFILE_MVC,                                         sDXVA_Intel_ModeH264_VLD_MVC },

    { H264_VLD        | VA_PROFILE_MVC_MV,                                      sDXVA_ModeH264_VLD_Multiview_NoFGT },
    { H264_VLD        | VA_PROFILE_MVC_STEREO,                                  sDXVA_ModeH264_VLD_Stereo_NoFGT },
    { H264_VLD        | VA_PROFILE_MVC_STEREO_PROG,                             sDXVA_ModeH264_VLD_Stereo_Progressive_NoFGT },

    { H264_VLD        | VA_PROFILE_MVC_MV          | VA_LONG_SLICE_MODE,        sDXVA2_ModeH264_VLD_NoFGT },
    { H264_VLD        | VA_PROFILE_MVC_STEREO      | VA_LONG_SLICE_MODE,        sDXVA2_ModeH264_VLD_NoFGT },
    { H264_VLD        | VA_PROFILE_MVC_STEREO_PROG | VA_LONG_SLICE_MODE,        sDXVA2_ModeH264_VLD_NoFGT },

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    { H264_VLD        | VA_PROFILE_WIDEVINE,                                    DXVA_Intel_Decode_Elementary_Stream_AVC },
#endif

    { H265_VLD,                                                                 DXVA_ModeHEVC_VLD_Main }, // MS
    { H265_VLD        | VA_LONG_SLICE_MODE,                                     DXVA_Intel_ModeHEVC_VLD_MainProfile },
    { H265_VLD        | VA_PROFILE_10,                                          DXVA_ModeHEVC_VLD_Main10  }, // MS
    { H265_VLD        | VA_PROFILE_10 | VA_LONG_SLICE_MODE,                     DXVA_Intel_ModeHEVC_VLD_Main10Profile },

    { H265_VLD        | VA_PROFILE_REXT | VA_LONG_SLICE_MODE,                   DXVA_Intel_ModeHEVC_VLD_Main422_10Profile },
    { H265_10_VLD     | VA_PROFILE_REXT | VA_LONG_SLICE_MODE,                   DXVA_Intel_ModeHEVC_VLD_Main422_10Profile },
    { H265_VLD_422    | VA_LONG_SLICE_MODE,                                     DXVA_Intel_ModeHEVC_VLD_Main422_10Profile },
    { H265_VLD_444    | VA_LONG_SLICE_MODE,                                     DXVA_Intel_ModeHEVC_VLD_Main444_10Profile },
    { H265_10_VLD_422 | VA_LONG_SLICE_MODE,                                     DXVA_Intel_ModeHEVC_VLD_Main422_10Profile },
    { H265_10_VLD_444 | VA_LONG_SLICE_MODE,                                     DXVA_Intel_ModeHEVC_VLD_Main444_10Profile },

#if (MFX_VERSION >= MFX_VERSION_NEXT)
    { H265_12_VLD_420     | VA_LONG_SLICE_MODE,                                 DXVA_Intel_ModeHEVC_VLD_Main12Profile },
    { H265_12_VLD_422     | VA_LONG_SLICE_MODE,                                 DXVA_Intel_ModeHEVC_VLD_Main422_12Profile },
    { H265_12_VLD_444     | VA_LONG_SLICE_MODE,                                 DXVA_Intel_ModeHEVC_VLD_Main444_12Profile },
#endif

#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    { H265_VLD_SCC        | VA_LONG_SLICE_MODE,                                 DXVA_Intel_ModeHEVC_VLD_SCC_Main_Profile },
    { H265_10_VLD_SCC     | VA_LONG_SLICE_MODE,                                 DXVA_Intel_ModeHEVC_VLD_SCC_Main_10Profile },
    { H265_VLD_444_SCC    | VA_LONG_SLICE_MODE,                                 DXVA_Intel_ModeHEVC_VLD_SCC_Main444_Profile },
    { H265_10_VLD_444_SCC | VA_LONG_SLICE_MODE,                                 DXVA_Intel_ModeHEVC_VLD_SCC_Main444_10Profile },
#endif //PRE_SI_TARGET_PLATFORM_GEN12

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    { VA_H265 | VA_VLD | VA_PROFILE_WIDEVINE,                                   DXVA_Intel_Decode_Elementary_Stream_HEVC },
#endif

};

const GuidProfile * GuidProfile::GetGuidProfiles()
{
    return guidProfiles;
}

const GuidProfile * GuidProfile::GetGuidProfile(size_t k)
{
    if (k >= GuidProfile::GetGuidProfileSize())
        return 0;

    return &guidProfiles[k];
}

size_t GuidProfile::GetGuidProfileSize()
{
    return sizeof(guidProfiles)/sizeof(guidProfiles[0]);
}

bool GuidProfile::isShortFormat(bool isHEVCGUID, uint32_t configBitstreamRaw)
{
    if (isHEVCGUID)
    {
        if (2 == configBitstreamRaw || 3 == configBitstreamRaw)
        { // prefer long mode
            return false;
        }
    }
    else
    {
        if (1 == configBitstreamRaw || 3 == configBitstreamRaw || 5 == configBitstreamRaw)
        {
            return false;
        }
    }

    return true;
}

bool GuidProfile::IsIntelCustomGUID(const GUID & guid)
{
    return
        guid == sDXVA2_Intel_ModeVC1_D_Super ||
        guid == sDXVA_Intel_ModeH264_VLD_MVC ||
        guid == DXVA_Intel_ModeHEVC_VLD_MainProfile       || guid == DXVA_Intel_ModeHEVC_VLD_Main10Profile ||
        guid == DXVA_Intel_ModeVP9_Profile0_VLD           || guid == DXVA_Intel_ModeVP9_Profile2_10bit_VLD
        || guid == DXVA_Intel_ModeHEVC_VLD_Main422_10Profile
        || guid == DXVA_Intel_ModeHEVC_VLD_Main444_10Profile
        || guid == DXVA_Intel_ModeVP9_Profile1_YUV444_VLD
        || guid == DXVA_Intel_ModeVP9_Profile3_YUV444_10bit_VLD
        || guid == DXVA_Intel_ModeHEVC_VLD_Main12Profile
        || guid == DXVA_Intel_ModeHEVC_VLD_Main422_12Profile
        || guid == DXVA_Intel_ModeHEVC_VLD_Main444_12Profile
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
        || guid == DXVA_Intel_ModeHEVC_VLD_SCC_Main_Profile
        || guid == DXVA_Intel_ModeHEVC_VLD_SCC_Main_10Profile
        || guid == DXVA_Intel_ModeHEVC_VLD_SCC_Main444_Profile
        || guid == DXVA_Intel_ModeHEVC_VLD_SCC_Main444_10Profile
#endif
        || guid == DXVA_Intel_ModeVP9_Profile2_YUV420_12bit_VLD
        || guid == DXVA_Intel_ModeVP9_Profile3_YUV444_12bit_VLD
        ;
}

bool GuidProfile::IsMVCGUID(const GUID & guid)
{
    return (guid == sDXVA_ModeH264_VLD_Multiview_NoFGT || guid == sDXVA_ModeH264_VLD_Stereo_NoFGT || guid == sDXVA_ModeH264_VLD_Stereo_Progressive_NoFGT || guid == sDXVA_Intel_ModeH264_VLD_MVC);
}

//////////////////////////////////////////////////////////////

bool DXVA2Accelerator::IsIntelCustomGUID() const
{
    return GuidProfile::IsIntelCustomGUID(m_guidDecoder);
}

//////////////////////////////////////////////////////////////
inline
Status ConvertVideoInfo2DVXA2Desc(VideoStreamInfo const* pVideoInfo, DXVA2_VideoDesc* pVideoDesc)
{
    VM_ASSERT(pVideoInfo);
    VM_ASSERT(pVideoDesc);

    // Fill in the SampleFormat.
    DXVA2_ExtendedFormat sf{};
    sf.SampleFormat           = DXVA2_SampleUnknown; //Unknown format. Default to DXVA2_SampleProgressiveFrame
    sf.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Unknown;
    sf.NominalRange           = DXVA2_NominalRange_Unknown;
    sf.VideoTransferMatrix    = DXVA2_VideoTransferMatrix_Unknown;
    sf.VideoLighting          = DXVA2_VideoLighting_Unknown;
    sf.VideoPrimaries         = DXVA2_VideoPrimaries_Unknown;
    sf.VideoTransferFunction  = DXVA2_VideoTransFunc_Unknown;

    // Fill in the video description.
    pVideoDesc->SampleWidth   = pVideoInfo->clip_info.width;
    pVideoDesc->SampleHeight  = pVideoInfo->clip_info.height;
    pVideoDesc->SampleFormat  = sf;

    pVideoDesc->InputSampleFreq.Numerator   = 0;
    pVideoDesc->InputSampleFreq.Denominator = 1;
    pVideoDesc->OutputFrameFreq.Numerator   = 0;
    pVideoDesc->OutputFrameFreq.Denominator = 1;

    pVideoDesc->UABProtectionLevel          = 1; //FALSE, video is not required to be protected;
    pVideoDesc->Reserved                    = 0;

    return UMC_OK;
}

Status DXVA2Accelerator::FindConfiguration(VideoStreamInfo const* pVideoInfo)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Find DXVA2 decode configuration");

    // Close decoder if opened
    CloseDirectXDecoder();
    bool bInitilized = false;

    ConvertVideoInfo2DVXA2Desc(pVideoInfo, &m_videoDesc);

    HRESULT hr = S_OK;
    if (!m_pDirect3DDeviceManager9)
        return UMC_ERR_FAILED;
    else
    {
        // Open a new device handle.
        hr = m_pDirect3DDeviceManager9->OpenDeviceHandle(&m_hDevice);
        if (FAILED(hr))
            return UMC_ERR_FAILED;

        hr = m_pDirect3DDeviceManager9->GetVideoService(m_hDevice, __uuidof(IDirectXVideoDecoderService), (void**)&m_pDecoderService);
        if (FAILED(hr))
            return UMC_ERR_FAILED;
    }

    if (!m_pDecoderService)
    {
        VM_ASSERT(FAILED(hr) && !"[GetVideoService] returned OK, but service is NULL");
        return UMC_ERR_FAILED;
    }

    // Get the decoder GUIDs.
    UINT cDecoderGuids = 0;
    CComHeapPtr<GUID> pDecoderGuids;
    hr = m_pDecoderService->GetDecoderDeviceGuids(&cDecoderGuids, &pDecoderGuids);
    if (FAILED(hr))
        return UMC_ERR_FAILED;

    for (uint32_t k = 0; k < GuidProfile::GetGuidProfileSize(); k++)
    {
        if ((m_Profile & (VA_ENTRY_POINT | VA_CODEC))!= (GuidProfile::GetGuidProfile(k)->profile & (VA_ENTRY_POINT | VA_CODEC)))
            continue;

#ifndef OPEN_SOURCE
        {
            if ((m_Profile & VA_PRIVATE_DDI_MODE) &&
                !GuidProfile::GetGuidProfile(k)->IsIntelCustomGUID())
                continue;
        }
#endif

        MFX_LTRACE_I(MFX_TRACE_LEVEL_PARAMS, k);

        auto const& candidate = GuidProfile::GetGuidProfile(k)->guid;
        vm_trace_GUID(candidate);

        auto i = std::find_if(pDecoderGuids.m_pData, pDecoderGuids.m_pData + cDecoderGuids,
            [&candidate](REFGUID g) { return candidate == g; }
        );

        if (i == pDecoderGuids.m_pData + cDecoderGuids)
            //Registered [GuidProfile] candidate is not in list of actually supported GUIDs
            continue;

        MFX_LTRACE_S(MFX_TRACE_LEVEL_PARAMS, "DXVA GUID found");

        UINT cFormats = 0;
        CComHeapPtr<D3DFORMAT> pFormats;
        // Find the valid render target formats for this decoder GUID.
        hr = m_pDecoderService->GetDecoderRenderTargets(candidate, &cFormats, &pFormats);
        if (FAILED(hr))
        {
            MFX_LTRACE_S(MFX_TRACE_LEVEL_PARAMS, "GetDecoderRenderTargets failed");
            continue;
        }

        // Look for a format that matches our output format.
        for (UINT iFormat = 0; iFormat < cFormats;  iFormat++)
        {
            UINT cConfigurations = 0;
            CComHeapPtr<DXVA2_ConfigPictureDecode> pConfig;

            DXVA2_VideoDesc vd = m_videoDesc;
            vd.Format = pFormats[iFormat];

            // Get the available configurations.
            hr = m_pDecoderService->GetDecoderConfigurations(candidate, &vd, NULL, &cConfigurations, &pConfig);
            if (FAILED(hr))
                continue;

            bool isHEVCGUID = (m_Profile & 0xf) == VA_H265;
            int idxConfig = -1;
            for (UINT iConfig = 0; iConfig < cConfigurations; iConfig++)
            {
                if (!CheckDXVAConfig<DXVA2_ConfigPictureDecode>(m_Profile, &pConfig[iConfig], GetProtectedVA()))
                {
                    MFX_LTRACE_S(MFX_TRACE_LEVEL_PARAMS, "CheckDXVAConfig failed");
                    continue;
                }

                MFX_LTRACE_S(MFX_TRACE_LEVEL_PARAMS, "Found DXVA configuration");
                bInitilized = true;

                if (idxConfig == -1)
                    idxConfig = iConfig;

                bool isShort = GuidProfile::isShortFormat(isHEVCGUID, pConfig[iConfig].ConfigBitstreamRaw);
                if (GuidProfile::GetGuidProfile(k)->profile & VA_LONG_SLICE_MODE)
                {
                    if (!isShort)
                    {
                        idxConfig = iConfig;
                    }
                }
                else
                {
                    if (isShort)
                        idxConfig = iConfig;
                }
            }

            m_guidDecoder = candidate;
            m_videoDesc.Format = vd.Format;

            m_bH264MVCSupport = GuidProfile::IsMVCGUID(m_guidDecoder);
            m_Config = pConfig[idxConfig];
            m_bH264ShortSlice = GuidProfile::isShortFormat(isHEVCGUID, m_Config.ConfigBitstreamRaw);
            m_H265ScalingListScanOrder = (m_Config.Config4GroupedCoefs & 0x80000000) ? 0 : 1;

            if (bInitilized)
            {
                break;
            }
        } // End of formats loop.

        if (FAILED(hr) || bInitilized)
        {
            break;
        }
    }

    if (!bInitilized)
    {
        hr = E_FAIL; // Unable to find a configuration.
    }

    if (FAILED(hr))
    {
        if (m_hDevice != INVALID_HANDLE_VALUE)
        {
            m_pDirect3DDeviceManager9->CloseDeviceHandle(m_hDevice);
            m_hDevice = INVALID_HANDLE_VALUE;
        }
    }

    return hr;
}

//////////////////////////////////////////////////////////////

Status DXVA2Accelerator::Init(VideoAcceleratorParams *pParams)
{
    UMC_CHECK_PTR(pParams);

    if (m_bInitilized)
    {
        return UMC_OK;
    }

    m_allocator = pParams->m_allocator;
    UMC_CHECK_PTR(m_allocator);

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if (IS_PROTECTION_ANY(pParams->m_protectedVA))
    {
        m_protectedVA = new UMC::ProtectedVA((mfxU16)pParams->m_protectedVA);
    }
#endif

    UMC_CHECK_PTR(pParams->m_pVideoStreamInfo);

    auto dxva_params = DynamicCast<DXVA2AcceleratorParams>(pParams);
    UMC_CHECK(dxva_params, UMC_ERR_INVALID_PARAMS);

    //[CComPtr] increments incoming reference int its assignment operator
    m_pDirect3DDeviceManager9 = dxva_params->m_device_manager;

    UMC_CALL(FindConfiguration(pParams->m_pVideoStreamInfo));
    vm_trace_fourcc(m_videoDesc.Format);

    UMC_CHECK(m_videoDesc.SampleWidth  >= UINT(pParams->m_pVideoStreamInfo->clip_info.width),  UMC_ERR_INVALID_PARAMS);
    UMC_CHECK(m_videoDesc.SampleHeight >= UINT(pParams->m_pVideoStreamInfo->clip_info.height), UMC_ERR_INVALID_PARAMS);

    // Create DXVA decoder
    HRESULT hr;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "CreateVideoDecoder");

        hr = m_pDecoderService->CreateVideoDecoder(
            m_guidDecoder,
            &m_videoDesc,
            &m_Config,
            (IDirect3DSurface9**)&pParams->m_surf[0],
            pParams->m_iNumberSurfaces,
            &m_pDXVAVideoDecoder);
    }

    if (FAILED(hr))
    {
        return UMC_ERR_INIT;
    }

    m_isUseStatuReport = true;
    m_bInitilized = TRUE;

    return UMC_OK;
}

Status DXVA2Accelerator::Close()
{
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    delete m_protectedVA;
    m_protectedVA = 0;
#endif

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    delete m_videoProcessingVA;
    m_videoProcessingVA = 0;
#endif

    m_bInitilized = FALSE;
    return DXAccelerator::Close();
}

#endif // UMC_VA_DXVA
