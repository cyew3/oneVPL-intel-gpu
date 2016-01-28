/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2006-2014 Intel Corporation. All Rights Reserved.
*/

#ifdef _DEBUG
#define VM_DEBUG
#endif

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include "umc_va_dxva2.h"
#include "umc_mf_decl.h"
#include "vm_time.h"
#include "mfx_trace.h"

#include "umc_va_dxva2_protected.h"
#include "umc_va_video_processing.h"

#include "umc_frame_allocator.h"

#define OVERFLOW_CHECK_VALUE    0x11

using namespace UMC;

/////////////////////////////////////////////////

namespace UMC
{
    DXVA2Accelerator *CreateDXVA2Accelerator()
    {
        MFX_LTRACE_S(MFX_TRACE_LEVEL_DXVA, "CreateDXVA2Accelerator()");
        return (new DXVA2Accelerator);
    }
}

extern void DeleteVideoAccelerator(void* acc)
{
    if(acc!=NULL){
        delete (DXVA2Accelerator*)acc;
    }
}

template <typename T>
class auto_deleter_stdcall_void
{
public:

    typedef void (__stdcall * deleter_func)(T *);

    auto_deleter_stdcall_void(T * ptr, deleter_func func)
    {
        m_ptr = ptr;
        m_func = func;
    }

    ~auto_deleter_stdcall_void()
    {
        if (m_ptr)
            m_func(m_ptr);
    }

private:
    T * m_ptr;
    deleter_func m_func;
};

DXVA2Accelerator::DXVA2Accelerator():
    m_pDirect3DDeviceManager9(NULL),
    m_pDecoderService(NULL),
    m_pDXVAVideoDecoder(NULL),
    m_hDevice(INVALID_HANDLE_VALUE),
    m_bInitilized(FALSE),
    m_guidDecoder(GUID_NULL),
    m_bIsExtManager(false)
{
    memset(&m_videoDesc, 0, sizeof(m_videoDesc));
}

//////////////////////////////////////////////////////////////


Status DXVA2Accelerator::CloseDirectXDecoder()
{
    SAFE_RELEASE(m_pDXVAVideoDecoder);
    m_bInitilized = FALSE;
    SAFE_RELEASE(m_pDecoderService);
    if (m_pDirect3DDeviceManager9 && m_hDevice != INVALID_HANDLE_VALUE)
    {
        m_pDirect3DDeviceManager9->CloseDeviceHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
    UMC_RETURN(UMC_OK);
}

DXVA2Accelerator::~DXVA2Accelerator()
{
    CloseDirectXDecoder();
    if (!m_bIsExtManager)
        SAFE_RELEASE(m_pDirect3DDeviceManager9);
}

//////////////////////////////////////////////////////////////

Status DXVA2Accelerator::BeginFrame(Ipp32s index)
{
    IDirect3DSurface9* surface;
    Status sts = m_allocator->GetFrameHandle(index, &surface);
    if (sts != UMC_OK)
        return sts;

    HRESULT hr = S_OK;
    for (Ipp32u i = 0; i < 200; i++)
    {
        hr = m_pDXVAVideoDecoder->BeginFrame(surface, NULL);
        
        if (E_PENDING != hr) 
            break;

        // sleep some time
        vm_time_sleep(5);
    }

    if (FAILED(hr))
    {
        return UMC::UMC_ERR_DEVICE_FAILED;
    }

    return UMC::UMC_OK;
}

//////////////////////////////////////////////////////////////

Status DXVA2Accelerator::EndFrame(void * handle)
{
    for (int i = 0; i < MAX_BUFFER_TYPES; i++)
    {
        m_pCompBuffer[i].SetBufferPointer(NULL, 0);
    }

    HRESULT hr = m_pDXVAVideoDecoder->EndFrame((HANDLE*)handle);

    if (FAILED(hr))
        return UMC::UMC_ERR_DEVICE_FAILED;

    CHECK_HR(hr);
    return UMC::UMC_OK;
}

//////////////////////////////////////////////////////////////

void* DXVA2Accelerator::GetCompBuffer(Ipp32s buffer_type, UMCVACompBuffer **buf, Ipp32s /*size*/, Ipp32s /*index*/)
{
    UMC_CHECK(buffer_type >= 0, NULL);
    UMC_CHECK(buffer_type < MAX_BUFFER_TYPES, NULL);
    UMCVACompBuffer *pCompBuffer = &m_pCompBuffer[buffer_type];

    if (!pCompBuffer->GetPtr())
    {
        void *pBuffer = NULL;
        UINT uBufferSize = 0;
        HRESULT hr = m_pDXVAVideoDecoder->GetBuffer(buffer_type - 1, &pBuffer, &uBufferSize);
        if (FAILED(hr))
        {
            vm_trace_x(hr);
            VM_ASSERT(SUCCEEDED(hr));
            return NULL;
        }
        pCompBuffer->type = buffer_type;
        pCompBuffer->SetBufferPointer((Ipp8u*)pBuffer, uBufferSize);
        pCompBuffer->SetDataSize(0);

#if defined(DEBUG) || defined(_DEBUG)
        ((Ipp8u*)pBuffer)[uBufferSize - 1] = OVERFLOW_CHECK_VALUE;
#endif

        m_bufferOrder.push_back(buffer_type);
    }

    if (buf)
    {
        *buf = pCompBuffer;
    }

    return pCompBuffer->GetPtr();
}

//////////////////////////////////////////////////////////////

Status DXVA2Accelerator::ReleaseAllBuffers()
{
    for (Ipp32u j = 0; j < m_bufferOrder.size(); ++j)
    {
        Status s = ReleaseBuffer(m_bufferOrder[j]); 
        if (s != UMC_OK)
            return s;
    }
    return UMC_OK;
}

Status DXVA2Accelerator::ReleaseBuffer(Ipp32s type)
{
    HRESULT hr = S_OK;
    UMCVACompBuffer *pCompBuffer = &m_pCompBuffer[type];

    hr = m_pDXVAVideoDecoder->ReleaseBuffer(type - 1);
    CHECK_HR(hr);

#if defined(DEBUG) || defined(_DEBUG)
    if (type == DXVA_SLICE_CONTROL_BUFFER ||
        type == DXVA_BITSTREAM_DATA_BUFFER ||
        type == DXVA_MACROBLOCK_CONTROL_BUFFER ||
        type == DXVA_RESIDUAL_DIFFERENCE_BUFFER)
    {
        Ipp8u *pBuffer = (Ipp8u*)pCompBuffer->GetPtr();
        Ipp32s buffer_size = pCompBuffer->GetBufferSize();
        if (pBuffer[buffer_size - 1] != OVERFLOW_CHECK_VALUE)
        {
            vm_debug_trace4(VM_DEBUG_ERROR, VM_STRING("Buffer%d overflow!!! %x[%d] = %x\n"), type, pBuffer, buffer_size, pBuffer[buffer_size - 1]);
        }
        //VM_ASSERT(pBuffer[buffer_size - 1] == OVERFLOW_CHECK_VALUE);
    }
#endif

    pCompBuffer->SetBufferPointer(NULL, 0);

    return UMC_OK;
}

//////////////////////////////////////////////////////////////

#if 0
static int frame_n = 0;
static void dumpFile(void *pData, int size, int n)
{
    return;
    char file_name[256];
    FILE *f;
    if (frame_n > 30) return;
    sprintf(file_name, "\\x_frame%02d_%d.bin", frame_n, n);
    f = fopen(file_name, "wb");
    fwrite(pData, 1, size, f);
    fclose(f);
}
#endif

Status DXVA2Accelerator::Execute()
{
    DXVA2_DecodeBufferDesc pBufferDesc[MAX_BUFFER_TYPES];
    DXVA2_DecodeExecuteParams pExecuteParams[1];
    int n = 0;

    ZeroMemory(pBufferDesc, sizeof(pBufferDesc));

    for (Ipp32u j = 0; j < m_bufferOrder.size(); ++j)
    {
        Ipp32u i = m_bufferOrder[j];

        UMCVACompBuffer *pCompBuffer = &m_pCompBuffer[i];
        if (!pCompBuffer->GetPtr()) continue;
        if (!pCompBuffer->GetBufferSize()) continue;

        int FirstMb = 0, NumOfMB = 0;

        if (pCompBuffer->FirstMb > 0) FirstMb = pCompBuffer->FirstMb;
        if (pCompBuffer->NumOfMB > 0) NumOfMB = pCompBuffer->NumOfMB;

        pBufferDesc[n].CompressedBufferType = i - 1;
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

        //dumpFile(pCompBuffer->GetPtr(), pCompBuffer->GetDataSize(), n);

        ReleaseBuffer(i);
    }

    pExecuteParams->NumCompBuffers = n;
    pExecuteParams->pCompressedBuffers = pBufferDesc;
    pExecuteParams->pExtensionData = NULL;

    m_bufferOrder.clear();

    //dumpFile(pBufferDesc, n*sizeof(DXVA2_DecodeBufferDesc), 0);
    //frame_n++;
   
    vm_trace_i(pExecuteParams->NumCompBuffers);

    HRESULT hr = E_FRAME_LOCKED;
    mfxU32 counter = 0;
    while (E_FRAME_LOCKED == hr && counter++ < 4) // 20 ms should be enough
    {
        //MFX_AUTO_LTRACE_WITHID(MFX_TRACE_LEVEL_DXVA, "DXVA2_DecodeExecute");
        hr = m_pDXVAVideoDecoder->Execute(pExecuteParams);

        if (hr != 0)
        {
            MFX_LTRACE_I(MFX_TRACE_LEVEL_DXVA, hr);
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
    DXVA2_DecodeExecuteParams executeParams;
    HRESULT hr;

    executeParams.NumCompBuffers = 0;
    executeParams.pCompressedBuffers = 0;
    executeParams.pExtensionData = (DXVA2_DecodeExtensionData *)buffer;

    hr = m_pDXVAVideoDecoder->Execute(&executeParams);
    vm_trace_x(hr);

    if (FAILED(hr))
        return UMC::UMC_ERR_DEVICE_FAILED;

    CHECK_HR(hr);
    return UMC_OK;
}
Status DXVA2Accelerator::ExecuteStatusReportBuffer(void * buffer, Ipp32s size)
{
    DXVA2_DecodeExecuteParams executeParams;
    DXVA2_DecodeExtensionData extensionData;
    HRESULT hr;
    
    memset(&executeParams, 0, sizeof(DXVA2_DecodeExecuteParams));
    memset(&extensionData, 0, sizeof(DXVA2_DecodeExtensionData));

    executeParams.NumCompBuffers = 0;
    executeParams.pCompressedBuffers = 0;
    executeParams.pExtensionData = &extensionData;
    extensionData.Function = 7;
    extensionData.pPrivateOutputData = (PVOID)buffer;
    extensionData.PrivateOutputDataSize = size;

    hr = m_pDXVAVideoDecoder->Execute(&executeParams);
    vm_trace_x(hr);

    if (FAILED(hr))
        return UMC::UMC_ERR_DEVICE_FAILED;

    CHECK_HR(hr);
    return UMC_OK;
}

//////////////////////////////////////////////////////////////
static const GuidProfile guidProfiles[] =
{
    { MPEG2_VLD,        sDXVA2_ModeMPEG2_VLD },
    { H264_VLD,         sDXVA2_ModeH264_VLD_NoFGT },

    { H264_VLD,  sDXVA2_Intel_EagleLake_ModeH264_VLD_NoFGT },

    { VC1_MC,    sDXVA2_ModeVC1_MoComp },
    { VC1_VLD,   sDXVA2_Intel_ModeVC1_D_Super},

    { JPEG_VLD,  sDXVA2_Intel_IVB_ModeJPEG_VLD_NoFGT},
    { VP8_VLD,   sDXVA_Intel_ModeVP8_VLD},
    { VP9_VLD,   sDXVA_Intel_ModeVP9_VLD},

    { VA_H264 | VA_VLD | VA_PROFILE_SVC_HIGH,     sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_High },
    { VA_H264 | VA_VLD | VA_PROFILE_SVC_BASELINE, sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_Baseline },

    { VA_H264 | VA_VLD | VA_PROFILE_MVC,     sDXVA_Intel_ModeH264_VLD_MVC },

    { VA_H264 | VA_VLD | VA_PROFILE_MVC_MV,            sDXVA_ModeH264_VLD_Multiview_NoFGT },
    { VA_H264 | VA_VLD | VA_PROFILE_MVC_STEREO,        sDXVA_ModeH264_VLD_Stereo_NoFGT },
    { VA_H264 | VA_VLD | VA_PROFILE_MVC_STEREO_PROG,   sDXVA_ModeH264_VLD_Stereo_Progressive_NoFGT },

    { VA_H264 | VA_VLD | VA_PROFILE_MVC_MV | VA_LONG_SLICE_MODE,            sDXVA2_ModeH264_VLD_NoFGT },
    { VA_H264 | VA_VLD | VA_PROFILE_MVC_STEREO | VA_LONG_SLICE_MODE,        sDXVA2_ModeH264_VLD_NoFGT },
    { VA_H264 | VA_VLD | VA_PROFILE_MVC_STEREO_PROG | VA_LONG_SLICE_MODE,   sDXVA2_ModeH264_VLD_NoFGT },

    { VA_H265 | VA_VLD, DXVA_ModeHEVC_VLD_Main }, // MS
    { VA_H265 | VA_VLD | VA_LONG_SLICE_MODE, DXVA_Intel_ModeHEVC_VLD_MainProfile },

    { VA_H265 | VA_VLD | VA_PROFILE_10, DXVA_ModeHEVC_VLD_Main10  }, // MS
    { VA_H265 | VA_VLD | VA_PROFILE_10 | VA_LONG_SLICE_MODE, DXVA_Intel_ModeHEVC_VLD_Main10Profile },

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

bool GuidProfile::isShortFormat(bool isHEVCGUID, Ipp32u configBitstreamRaw)
{
    if (isHEVCGUID)
    {
        if (2 == configBitstreamRaw)
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
    return (guid == sDXVA2_Intel_ModeVC1_D_Super || guid == sDXVA2_Intel_EagleLake_ModeH264_VLD_NoFGT || guid == sDXVA_Intel_ModeH264_VLD_MVC || guid == DXVA_Intel_ModeHEVC_VLD_MainProfile ||
        guid == DXVA_Intel_ModeHEVC_VLD_Main10Profile);
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
Status ConvertVideoInfo2DVXA2Desc(VideoStreamInfo   *pVideoInfo,
                                  DXVA2_VideoDesc   *pVideoDesc)
{
    int width  = pVideoInfo->clip_info.width;
    int height = pVideoInfo->clip_info.height;

    // Fill in the SampleFormat.
    DXVA2_ExtendedFormat sf;
    sf.SampleFormat = DXVA2_SampleUnknown; //DXVA2_SampleProgressiveFrame;//DXVA2_SampleUnknown ;
    sf.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Unknown;
    sf.NominalRange = DXVA2_NominalRange_Unknown; //DXVA2_NominalRange_Normal;
    sf.VideoTransferMatrix = DXVA2_VideoTransferMatrix_Unknown;
    sf.VideoLighting = DXVA2_VideoLighting_Unknown;
    sf.VideoPrimaries = DXVA2_VideoPrimaries_Unknown;
    sf.VideoTransferFunction = DXVA2_VideoTransFunc_Unknown;

    // Fill in the video description.

    pVideoDesc->SampleWidth  = width;
    pVideoDesc->SampleHeight = height;

    pVideoDesc->SampleFormat = sf;
    //pVideoDesc->Format = pFormats[iFormat];
    pVideoDesc->InputSampleFreq.Numerator = 0; //(int)frame_rate;
    pVideoDesc->InputSampleFreq.Denominator = 1;
    pVideoDesc->OutputFrameFreq.Numerator = 0; //(int)frame_rate;
    pVideoDesc->OutputFrameFreq.Denominator = 1;
    pVideoDesc->UABProtectionLevel = 1; //FALSE;
    pVideoDesc->Reserved = 0;

    return S_OK;
}

Status DXVA2Accelerator::FindConfiguration(VideoStreamInfo *pVideoInfo)
{
    HRESULT hr = S_OK;
    UINT    cDecoderGuids = 0;
    GUID    *pDecoderGuids = NULL; // [cDecoderGuids] must be cleaned up at the end

    // Close decoder if opened
    CloseDirectXDecoder();
    bool bInitilized = false;

    UMC_CHECK_PTR(pVideoInfo);

    ConvertVideoInfo2DVXA2Desc(pVideoInfo, &m_videoDesc);

    // Two options to get the video decoder service.
    if (m_pDirect3DDeviceManager9)
    {
        // Open a new device handle.
        hr = m_pDirect3DDeviceManager9->OpenDeviceHandle(&m_hDevice);
        if (SUCCEEDED(hr)) // Get the video decoder service.
        {
            hr = m_pDirect3DDeviceManager9->GetVideoService(m_hDevice, __uuidof(IDirectXVideoDecoderService), (void**)&m_pDecoderService);
        }
    }
    else
    {
        return E_FAIL;
    }

    // Get the decoder GUIDs.
    if (SUCCEEDED(hr))
    {
        HRESULT hr2 = m_pDecoderService->GetDecoderDeviceGuids(&cDecoderGuids, &pDecoderGuids);
        if (FAILED(hr2))
        {
            cDecoderGuids = 0;
        }
    }

    auto_deleter_stdcall_void<VOID> automatic2(pDecoderGuids, &CoTaskMemFree);

    if (SUCCEEDED(hr))
    {
        for (Ipp32u k = 0; k < GuidProfile::GetGuidProfileSize(); k++)
        {
            if ((m_Profile & (VA_ENTRY_POINT | VA_CODEC))!= (GuidProfile::GetGuidProfile(k)->profile & (VA_ENTRY_POINT | VA_CODEC)))
                continue;

            m_guidDecoder = GuidProfile::GetGuidProfile(k)->guid;
            vm_trace_GUID(m_guidDecoder);

            if (cDecoderGuids)
            { // Look for the decoder GUID we want.
                UINT iGuid;
                for (iGuid = 0; iGuid < cDecoderGuids; iGuid++)
                {
                    if (pDecoderGuids[iGuid] == m_guidDecoder)
                        break;
                }
                if (iGuid >= cDecoderGuids)
                {
                    continue;
                }
                MFX_LTRACE_S(MFX_TRACE_LEVEL_PARAMS, "DXVA GUID found");
            }

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Find DXVA2 decode configuration");
            MFX_LTRACE_I(MFX_TRACE_LEVEL_PARAMS, k);

            D3DFORMAT *pFormats = NULL; // size = cFormats
            UINT cFormats = 0;

            // Find the valid render target formats for this decoder GUID.
            hr = m_pDecoderService->GetDecoderRenderTargets(
                m_guidDecoder,
                &cFormats,
                &pFormats
                );
            if (FAILED(hr))
            {
                MFX_LTRACE_S(MFX_TRACE_LEVEL_PARAMS, "GetDecoderRenderTargets failed");
                continue;
            }

            auto_deleter_stdcall_void<VOID> automatic0(pFormats, &CoTaskMemFree);

            // Look for a format that matches our output format.
            for (UINT iFormat = 0; iFormat < cFormats;  iFormat++)
            {
                DXVA2_ConfigPictureDecode *pConfig = NULL; // size = cConfigurations
                UINT cConfigurations = 0;

                m_videoDesc.Format = pFormats[iFormat];

                // Get the available configurations.
                hr = m_pDecoderService->GetDecoderConfigurations(
                    m_guidDecoder,
                    &m_videoDesc,
                    NULL, // Reserved.
                    &cConfigurations,
                    &pConfig
                    );

                auto_deleter_stdcall_void<VOID> automatic1(pConfig, &CoTaskMemFree);

                bool isHEVCGUID = (m_Profile & 0xf) == VA_H265;
                
                // Find a supported configuration.
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
    HRESULT hr = S_OK;

    if (m_bInitilized)
    {
        UMC_RETURN(UMC_OK);
    }

    m_allocator = pParams->m_allocator;

    if (!m_allocator)
        return UMC_ERR_NULL_PTR;

    if (IS_PROTECTION_ANY(pParams->m_protectedVA))
    {
        m_protectedVA = new UMC::ProtectedVA((mfxU16)pParams->m_protectedVA);
    }

    UMC_CALL(FindConfiguration(pParams->m_pVideoStreamInfo));

    // Number of surfaces
    Ipp32u numberSurfaces = pParams->m_iNumberSurfaces;

    // check surface width&height
    if (pParams->m_pVideoStreamInfo)
    {
        UMC_CHECK(m_videoDesc.SampleWidth  >= (UINT)(pParams->m_pVideoStreamInfo->clip_info.width), UMC_ERR_INVALID_PARAMS);
        UMC_CHECK(m_videoDesc.SampleHeight >= (UINT)(pParams->m_pVideoStreamInfo->clip_info.height), UMC_ERR_INVALID_PARAMS);
    }

    vm_trace_fourcc(m_videoDesc.Format);

    // Create DXVA decoder
    if (SUCCEEDED(hr))
    {
        hr = m_pDecoderService->CreateVideoDecoder(
            m_guidDecoder,
            &m_videoDesc,
            &m_Config,
            (IDirect3DSurface9**)&pParams->m_surf[0],
            numberSurfaces,
            &m_pDXVAVideoDecoder);
    }

    if (FAILED(hr))
    {
        UMC_RETURN(UMC_ERR_INIT);
    }

    m_isUseStatuReport = m_HWPlatform != VA_HW_LAKE;
    m_bInitilized = TRUE;
    UMC_RETURN(UMC_OK);
}

Status DXVA2Accelerator::Close()
{
    delete m_protectedVA;
    m_protectedVA = 0;

    delete m_videoProcessingVA;
    m_videoProcessingVA = 0;

    m_bInitilized = FALSE;
    return VideoAccelerator::Close();
}

#endif // UMC_VA_DXVA
