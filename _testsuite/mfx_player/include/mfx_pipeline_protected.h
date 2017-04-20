/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "mfx_pipeline_defs.h"

#ifdef D3D_SURFACES_SUPPORT
    #include <d3d9.h>
    #include <dxva2api.h>
#endif
//#if MFX_D3D11_SUPPORT
//    #include <d3d11.h>
//#endif
#include "mfx_ipipeline.h"
#include "mfx_renders.h"
#include "mfx_ihw_device.h"
#include "test_statistics.h"
#include "mfx_loop_decoder.h"
#include "mfx_commands.h"
#include "mfx_bitsream_buffer.h"
#include "mfx_cmd_option_processor.h"
#include "mfx_component_params.h"
//#if defined(_WIN32)
//    #include "mfx_vpp_plugin.h"
//#endif
#include "mfx_commands_initializer.h"
#include "mfx_ifactory.h"
#include "mfx_bitstream2.h"
#include "mfx_bitstream_reader_encryptor.h"
#include "mfx_ibitstream_reader.h"
#include "mfx_shared_ptr.h"
#include "app_defs.h"

#include "mfx_pavp.h"

#include "mfxpcp.h"
#include "pavp_video_lib.h"

#include "pavp_sample_common_dx9.h" //SAuthChannel9
#include "pavp_sample_common_dx11.h" //SAuthChannel11

#include "pavp_video_lib_dx9.h"
#include "pavp_video_lib_dx11.h"
#include "intel_pavp_gpucp_api.h"
#include "win8ui_video_lib.h"



#include <atlbase.h>

extern const GUID DXVA2_Intel_Encode_AVC;
extern const GUID DXVA2_Intel_Encode_MPEG2;

mfxU64 SwapEndian(mfxU64 val);
mfxU64 AdjustCtr(mfxU64 counter, mfxU16 type);
bool RandomizeIVCtr(mfxAES128CipherCounter &iv_ctr, mfxU16 type);
mfxStatus CPAVPVideo_Encrypt(mfxHDL pthis, mfxU8 *src, mfxU8 *dst, mfxU32 nbytes, 
    mfxAES128CipherCounter *cipherCounter);
mfxStatus CPAVPVideo_Decrypt(mfxHDL pthis, mfxU8 *src, mfxU8 *dst, mfxU32 nbytes, 
    const mfxAES128CipherCounter& cipherCounter);

class DeviceLocker
{
public:
    DeviceLocker(IDirect3DDeviceManager9* manager)
        : m_manager(manager)
        , m_handle(0)
        , m_device(0)
        , m_deviceEx(0)
    {
        if (manager)
        {
            HRESULT hr = manager->OpenDeviceHandle(&m_handle);
            if (FAILED(hr))
            {
                m_manager = 0;
                return;
            }

            hr = m_manager->LockDevice(m_handle, &m_device, true);
            if (FAILED(hr))
            {
                m_manager->CloseDeviceHandle(m_handle);
                m_manager = 0;
                return;
            }

            hr = m_device->QueryInterface(
                __uuidof(IDirect3DDevice9Ex),
                (void **)&m_deviceEx);
            if (FAILED(hr))
            {
                m_device = 0;
                m_manager->UnlockDevice(m_handle, true);
                m_manager->CloseDeviceHandle(m_handle);
                m_manager = 0;
                return;
            }
        }        
    }

    ~DeviceLocker()
    {
        if (m_manager && m_handle)
        {
            m_deviceEx = 0;
            m_device = 0;
            m_manager->UnlockDevice(m_handle, true);
            m_manager->CloseDeviceHandle(m_handle);
        }
    }

    operator IDirect3DDevice9*()
    {
        return m_device;
    }

    operator IDirect3DDevice9Ex*()
    {
        return m_deviceEx;
    }

    bool operator !() const
    {
        return 0 == m_device;
    }

protected:
    CComPtr<IDirect3DDeviceManager9> m_manager;
    CComPtr<IDirect3DDevice9> m_device;
    CComPtr<IDirect3DDevice9Ex> m_deviceEx;
    HANDLE m_handle;
};

extern const char* pavp_functions[];
extern const mfxU32 pavp_functions_count;

template <class T>
class MFXProtectedPipeline: public T
{
public:
    MFXProtectedPipeline<T>(IMFXPipelineFactory *pFactory)
        :T(pFactory)
        , m_cpDevice(NULL)
        , m_pavpSession(NULL)
        , m_pavpVideo(NULL)
        , m_authChannel9(NULL)
        , m_authChannel11(NULL)
    {
    };
    ~MFXProtectedPipeline<T>()
    {
        SO_CALL(SAuthChannel11_Destroy, m_pavpdll, 0, (m_authChannel11));
        SO_CALL(SAuthChannel9_Destroy, m_pavpdll, 0, (m_authChannel9));
        SAFE_DELETE(m_pavpVideo);
        SAFE_DELETE(m_pavpSession);
        SAFE_DELETE(m_cpDevice);
    }
protected:
    mfxStatus CreatePAVPSessions()
    {
        if (0 == m_inParams.Protected)
            return MFX_ERR_UNKNOWN;

        mfxStatus sts = MFX_ERR_NONE;
        PavpEpidStatus PavpStatus;

        mfxU16 msdkCounterType = 0;
        HRESULT hr = S_OK;

        m_inParams.encType = PAVP_ENCRYPTION_AES128_CTR;

        GUID profile = GUID_NULL;
        mfxU32 CodecId = getOutputCodecId();
        if (0 != CodecId)
            profile = MFX_CODEC_MPEG2 == CodecId ? 
                DXVA2_Intel_Encode_MPEG2 : GUID_INTEL_TRANSCODE_SESSION;
        else
            MFX_CHECK_STS(GetDecoderProfile(m_components[eDEC].m_pSession, &profile));

        if (m_components[eDEC].m_bufType == MFX_BUF_HW_DX11)
        {
    #if MFX_D3D11_SUPPORT
            if (IS_PROTECTION_GPUCP_ANY(m_inParams.Protected))
            {
                ID3D11Device *pD3D11Device = NULL;
                sts = m_pHWDevice->GetHandle(MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&pD3D11Device);
                CHECK_RESULT(sts, MFX_ERR_NONE, sts);

                GUID cryptoType = GUID_NULL;
                if(m_inParams.ctr_dec_type != 0)
                {
                    switch(m_inParams.ctr_dec_type)
                    {
                    case MFX_PAVP_CTR_TYPE_A:
                        cryptoType = D3DCRYPTOTYPE_INTEL_AES128_CTR;
                        break;
                    case MFX_PAVP_CTR_TYPE_C:
                        cryptoType = D3D11_CRYPTO_TYPE_AES128_CTR;
                        break;
                    default:
                        PRINT_RET_MSG(MFX_ERR_UNSUPPORTED);
                        return MFX_ERR_UNSUPPORTED;
                    }
                }



                GUID keyExchangeType = GUID_NULL;
                ID3D11CryptoSession *cs = NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        
                if (MFX_PROTECTION_GPUCP_AES128_CTR == m_inParams.Protected)
                {
                    cryptoType = D3D11_CRYPTO_TYPE_AES128_CTR;
                    keyExchangeType = D3D11_KEY_EXCHANGE_RSAES_OAEP;
                    hr = SO_CALL(CCPDeviceD3D11_create4, m_pavpdll, E_FAIL, (
                        pD3D11Device, 
                        &cryptoType,
                        &profile,
                        &keyExchangeType,
                        &m_cpDevice));
                    if (FAILED(hr))
                    {
                        PRINT_RET_MSG(hr);
                        return MFX_ERR_DEVICE_FAILED;
                    }
                    cs = dynamic_cast<CCPDeviceD3D11Base*>(m_cpDevice)->GetCryptoSessionPtr();
                }
                else if (MFX_PROTECTION_GPUCP_PAVP == m_inParams.Protected)
                {
                    hr = SO_CALL(CCPDeviceD3D11_create, m_pavpdll, E_FAIL, (
                        pD3D11Device,
                        profile, 
                        (PAVP_ENCRYPTION_TYPE)m_inParams.encType,
                        ((cpImplSW == m_inParams.cpImpl || 
                            cpImplUnknown == m_inParams.cpImpl) ? D3D11_CONTENT_PROTECTION_CAPS_SOFTWARE : 0) |
                        ((cpImplHW == m_inParams.cpImpl || 
                            cpImplUnknown == m_inParams.cpImpl) ? D3D11_CONTENT_PROTECTION_CAPS_HARDWARE : 0),
                        &m_cpDevice,
                        &cryptoType));
                    if (FAILED(hr))
                    {
                        PRINT_RET_MSG(hr);
                        return MFX_ERR_DEVICE_FAILED;
                    }
                    cs = dynamic_cast<CCPDeviceD3D11Base*>(m_cpDevice)->GetCryptoSessionPtr();
                    keyExchangeType = dynamic_cast<CCPDeviceD3D11Base*>(m_cpDevice)->GetKeyExchangeGUID();
                }
                else
                {
                    PRINT_RET_MSG(MFX_ERR_UNSUPPORTED);
                    return MFX_ERR_UNSUPPORTED;
                }
                
/*                hr = CCPDeviceD3D11_create(
                    pD3D11Device,
                    profile, 
                    (PAVP_ENCRYPTION_TYPE)m_inParams.encType,
                    D3D11_CONTENT_PROTECTION_CAPS_SOFTWARE | D3D11_CONTENT_PROTECTION_CAPS_HARDWARE,
                    &m_cpDevice,
                    &cryptoType);
                if (FAILED(hr))
                {
                    PRINT_RET_MSG(hr);
                    return MFX_ERR_DEVICE_FAILED;
                }
*/
                if (D3DCRYPTOTYPE_INTEL_AES128_CTR == cryptoType)
                    msdkCounterType = MFX_PAVP_CTR_TYPE_A;
                else if (D3D11_CRYPTO_TYPE_AES128_CTR == cryptoType)
                    msdkCounterType = MFX_PAVP_CTR_TYPE_C;
                else
                {
                    PRINT_RET_MSG(MFX_ERR_UNSUPPORTED);
                    return MFX_ERR_UNSUPPORTED;
                }

                hr = SO_CALL(SAuthChannel11_Create, m_pavpdll, E_FAIL, (&m_authChannel11, m_cpDevice));
                if (FAILED(hr))
                {
                    vm_string_printf(VM_STRING("Failed to create authenticated channel (hr=0x%08x)\n"), hr);
                    return MFX_ERR_DEVICE_FAILED;
                } 
            }
    #else
            PRINT_RET_MSG(MFX_ERR_UNSUPPORTED);
            return MFX_ERR_UNSUPPORTED;
    #endif
        }
        else
        {
            IDirect3DDeviceManager9 *pd3dDeviceManager = NULL;
            MFX_CHECK_STS(m_pHWDevice->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&pd3dDeviceManager));
            DeviceLocker deviceEx(pd3dDeviceManager);

            sts = MFX_ERR_NONE;
            for (;;)
            {

                if (MFX_PROTECTION_GPUCP_PAVP == m_inParams.Protected)
                {
                    HANDLE cs9Handle = 0;

                    GUID cryptoType = GUID_NULL;
                    if(m_inParams.ctr_dec_type != 0)
                    {
                        if(MFX_PAVP_CTR_TYPE_A == m_inParams.ctr_dec_type)
                            cryptoType = D3DCRYPTOTYPE_INTEL_AES128_CTR;
                        else if(MFX_PAVP_CTR_TYPE_C == m_inParams.ctr_dec_type)
                            cryptoType = D3DCRYPTOTYPE_AES128_CTR;
                        else
                        {
                            PRINT_RET_MSG(MFX_ERR_UNSUPPORTED);
                            sts = MFX_ERR_UNSUPPORTED;
                            break;
                        }
                    }

                    hr = SO_CALL(CCPDeviceCryptoSession9_create, m_pavpdll, E_FAIL, (
                        deviceEx,
                        profile, 
                        (PAVP_ENCRYPTION_TYPE)m_inParams.encType,
                        ((cpImplSW == m_inParams.cpImpl || 
                            cpImplUnknown == m_inParams.cpImpl) ? D3DCPCAPS_SOFTWARE : 0) |
                        ((cpImplHW == m_inParams.cpImpl || 
                            cpImplUnknown == m_inParams.cpImpl) ? D3DCPCAPS_HARDWARE : 0),
                        &(m_cpDevice),
                        &cs9Handle,
                        &cryptoType));
                    if (FAILED(hr))
                    {
                        vm_string_printf(VM_STRING("Failed to create CCPDeviceCryptoSession9 (hr=0x%08x)\n"), hr);
                        sts = MFX_ERR_DEVICE_FAILED;
                        break;
                    }

                    // cryptoType can be adjusted. Get the counter type so MSDK be in sync.
                    if (D3DCRYPTOTYPE_INTEL_AES128_CTR == cryptoType)
                        msdkCounterType = MFX_PAVP_CTR_TYPE_A;
                    else if (D3DCRYPTOTYPE_AES128_CTR == cryptoType)
                        msdkCounterType = MFX_PAVP_CTR_TYPE_C;
                    else
                    {
                        PRINT_RET_MSG(MFX_ERR_UNSUPPORTED);
                        sts = MFX_ERR_UNSUPPORTED;
                        break;
                    }

                    hr = SO_CALL(SAuthChannel9_Create, m_pavpdll, E_FAIL, (&m_authChannel9, m_cpDevice, deviceEx));//pd3d9DeviceEx);
                    if (FAILED(hr))
                    {
                        vm_string_printf(VM_STRING("Failed to create authenticated channel (hr=0x%08x)\n"), hr);
                        sts = MFX_ERR_DEVICE_FAILED;
                        break;
                    } 
                }
                
                if (MFX_PROTECTION_PAVP == m_inParams.Protected)
                {
                    hr = SO_CALL(CCPDeviceAuxiliary9_create, m_pavpdll, E_FAIL, (
                        deviceEx,//pd3d9DeviceEx,
                        PAVP_SESSION_TYPE_TRANSCODE,
                        ((cpImplSW == m_inParams.cpImpl || 
                            cpImplUnknown == m_inParams.cpImpl) ? PAVP_MEMORY_PROTECTION_LITE : 0) |
                        ((cpImplHW == m_inParams.cpImpl || 
                            cpImplUnknown == m_inParams.cpImpl) ? PAVP_MEMORY_PROTECTION_DYNAMIC : 0),
                        &(m_cpDevice)));
                    if (FAILED(hr))
                    {
                        vm_string_printf(VM_STRING("Failed to create CCPDeviceCryptoSession9 (hr=0x%08x)\n"), hr);
                        sts = MFX_ERR_DEVICE_FAILED;
                        break;
                    }
                }
                break;
            }

            CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        if (NULL == m_cpDevice)
        {
            vm_string_printf(VM_STRING("No interface exists to open PAVP session.\n"));
            return MFX_ERR_DEVICE_FAILED;
        }
        
        if (IS_PROTECTION_PAVP_ANY(m_inParams.Protected))
        {
            SYSTEMTIME seed;
            GetSystemTime(&seed);
            m_pavpSession = SO_CALL(CreateCSIGMASession, m_pavpdll, NULL, (
                m_cpDevice, 
                SO_DATA(Test_v10_ApiVersion, m_pavpdll, NULL), 
                SO_DATA(Test_v10_Cert3p, m_pavpdll, NULL), SO_DATA(Test_v10_Cert3pSize, m_pavpdll, NULL), 
                NULL, 0,
                SO_DATA(Test_v10_DsaPrivateKey, m_pavpdll, NULL),
                (uint32*)&seed, sizeof(seed)));
            if (NULL == m_pavpSession)
            {
                vm_string_printf(VM_STRING("Failed to open SIGMA session\n"));
                return MFX_ERR_DEVICE_FAILED;
            }

            vm_string_printf(VM_STRING("SIGMA session created succesfully\n"));

        /*        memset (&(m_inParams.extDecPavpOption), 0, sizeof(mfxExtPAVPOption));
            memset (&(m_inParams.extEncPavpOption), 0, sizeof(mfxExtPAVPOption));
            m_inParams.extDecPavpOption.Header.BufferId = 
                m_inParams.extEncPavpOption.Header.BufferId = MFX_EXTBUFF_PAVP_OPTION;
            m_inParams.extDecPavpOption.Header.BufferSz = 
                m_inParams.extEncPavpOption.Header.BufferSz = sizeof(mfxExtPAVPOption);
            m_inParams.extEncPavpOption.EncryptionType = (mfxU16)m_inParams.encType;
        */
        }
        mfxU16 protection = 0;
    #if MFX_D3D11_SUPPORT
        if (NULL != dynamic_cast<CCPDeviceD3D11*>(m_cpDevice))
        {
            protection = MFX_PROTECTION_GPUCP_PAVP;

            PAVP_ENCRYPTION_MODE encEncryptionMode;
            PAVP_ENCRYPTION_MODE *mode = NULL;
            if (MFX_ENC_RENDER == m_RenderType)
            {
                encEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_AES128_CTR;
                encEncryptionMode.eCounterMode = (PAVP_COUNTER_TYPE)msdkCounterType;
                mode = &encEncryptionMode;
            }
            m_pavpVideo = SO_CALL(CreateCPAVPVideo_D3D11, m_pavpdll, NULL, (dynamic_cast<CCPDeviceD3D11*>(m_cpDevice),
                m_pavpSession, mode));
            if (NULL == m_pavpVideo)
                return MFX_ERR_UNSUPPORTED;
        }
        else if (NULL != dynamic_cast<CCPDeviceD3D11Base*>(m_cpDevice))
        {
            m_pavpVideo = SO_CALL(CreateCVideoProtection_CS11, m_pavpdll, NULL, (dynamic_cast<CCPDeviceD3D11Base*>(m_cpDevice)));
        }
        else
    #endif //MFX_D3D11_SUPPORT
             if (NULL != dynamic_cast<CCPDeviceCryptoSession9*>(m_cpDevice))
        {
            protection = MFX_PROTECTION_GPUCP_PAVP;

            PAVP_ENCRYPTION_MODE encEncryptionMode;
            PAVP_ENCRYPTION_MODE *mode = NULL;
            if (MFX_ENC_RENDER == m_RenderType)
            {
                encEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_AES128_CTR;
                encEncryptionMode.eCounterMode = (PAVP_COUNTER_TYPE)msdkCounterType;
                mode = &encEncryptionMode;
            }
            m_pavpVideo = SO_CALL(CreateCPAVPVideo_CryptoSession9, m_pavpdll, NULL, (dynamic_cast<CCPDeviceCryptoSession9*>(m_cpDevice),
                m_pavpSession, mode));
            if (NULL == m_pavpVideo)
                return MFX_ERR_UNSUPPORTED;
        }
        else if (NULL != dynamic_cast<CCPDeviceAuxiliary9*>(m_cpDevice))
        {
            protection = MFX_PROTECTION_PAVP;

            //workaround: asomsiko should fix it
            msdkCounterType = MFX_PAVP_CTR_TYPE_A;

            PAVP_ENCRYPTION_MODE encEncryptionMode;
            PAVP_ENCRYPTION_MODE *mode = NULL;
            if (MFX_ENC_RENDER == m_RenderType)
            {
                encEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_AES128_CTR;
                encEncryptionMode.eCounterMode = (PAVP_COUNTER_TYPE)msdkCounterType;
                mode = &encEncryptionMode;
            }
            m_pavpVideo = SO_CALL(CreateCPAVPVideo_Auxiliary9, m_pavpdll, NULL, (dynamic_cast<CCPDeviceAuxiliary9*>(m_cpDevice),
                m_pavpSession, NULL, mode));
            if (NULL == m_pavpVideo)
                return MFX_ERR_UNSUPPORTED;
        }
        else
        {
            vm_string_printf(VM_STRING("Unknown CCPDevice)\n"));
            return MFX_ERR_DEVICE_FAILED;
        }

    /*        m_inParams.extEncPavpOption.CounterType = msdkCounterType;
        m_inParams.decParams_Protected = protection;
        m_inParams.encParams_Protected = protection;

        // PAVP ext parameters for decoders are only required for axiliary interface
        m_inParams.extDecPavpOption.CounterType = 
            m_inParams.extEncPavpOption.CounterType;
        m_inParams.extDecPavpOption.EncryptionType = 
            m_inParams.extEncPavpOption.EncryptionType;
        m_inParams.decExtParams = &(m_inParams.extDecPavpOption.Header);

        // PAVP ext parameters for encoders are always required
        m_inParams.encExtParams = &(m_inParams.extEncPavpOption.Header);
    */
        
        if (MFX_ENC_RENDER == m_RenderType)
        {
            m_components[eREN].m_extParams.push_back(new mfxExtPAVPOption());
            MFXExtBufferPtr<mfxExtPAVPOption> pExtPAVPOption(m_components[eREN].m_extParams);
            pExtPAVPOption->CounterIncrement = 0; // default will be used
            pExtPAVPOption->EncryptionType = MFX_PAVP_AES128_CTR;
            pExtPAVPOption->CounterType = 0 != m_inParams.ctr_enc_type ? m_inParams.ctr_enc_type : msdkCounterType;
            RandomizeIVCtr(pExtPAVPOption->CipherCounter, pExtPAVPOption->CounterType);
            if (m_inParams.startctr_enc_set)
                pExtPAVPOption->CipherCounter.Count = AdjustCtr(SwapEndian(m_inParams.startctr_enc), pExtPAVPOption->CounterType);
            m_components[eREN].AssignExtBuffers();
        }

        mfxAES128CipherCounter iv_ctr = {0};
        RandomizeIVCtr(iv_ctr, msdkCounterType);
        if (m_inParams.startctr_dec_set)
            iv_ctr.Count = AdjustCtr(SwapEndian(m_inParams.startctr_dec), msdkCounterType);


        if (MFX_PROTECTION_GPUCP_AES128_CTR == m_inParams.Protected)
        {
            PavpStatus = dynamic_cast<CVideoProtection_CS11*>(m_pavpVideo)->Init((void*)&iv_ctr, sizeof(iv_ctr)); 
            if (PAVP_EPID_FAILURE(PavpStatus))
            {
                vm_string_printf(VM_STRING("Failed to Init video pipeline ptotection (sts=0x%08x)\n"), PavpStatus);
                return MFX_ERR_DEVICE_FAILED;
            }
        }
        else
        {
            m_components[eDEC].m_extParams.push_back(new mfxExtPAVPOption());
            MFXExtBufferPtr<mfxExtPAVPOption> pExtPAVPOption(m_components[eDEC].m_extParams);
            pExtPAVPOption->EncryptionType = m_inParams.encType;
            pExtPAVPOption->CounterType = msdkCounterType;
            m_components[eDEC].AssignExtBuffers();

            PavpStatus = dynamic_cast<CPAVPVideo*>(m_pavpVideo)->Init(0, &iv_ctr, sizeof (iv_ctr)); 
            if (PAVP_EPID_FAILURE(PavpStatus))
            {
                vm_string_printf(VM_STRING("Failed to Init video pipeline protection (sts=0x%08x)\n"), PavpStatus);
                return MFX_ERR_DEVICE_FAILED;
            }
        }

        return sts;
    }


    // Request decoder GUID from video decoder handle. Decoder GUID is required 
    // to create crypto session.
    mfxStatus GetDecoderProfile(IVideoSession *session, GUID *profile)
    {
        HRESULT hr = S_OK;

        if (NULL == profile)
            return MFX_ERR_NULL_PTR;
        
        *profile = GUID_NULL;

        IUnknown *videoDecoder = NULL;
        if (NULL != session)
            session->GetHandle((mfxHandleType)MFX_HANDLE_VIDEO_DECODER, (mfxHDL*)&videoDecoder);
        if (NULL != videoDecoder)
        {

    #if MFX_D3D11_SUPPORT
            if (GUID_NULL == *profile)
            {
                ID3D11VideoDecoder *videoDecoder_dx11 = NULL;
                D3D11_VIDEO_DECODER_DESC videoDesc;
                D3D11_VIDEO_DECODER_CONFIG config;
                hr = videoDecoder->QueryInterface(
                    IID_ID3D11VideoDecoder,
                    (void **)&videoDecoder_dx11);
                if (SUCCEEDED(hr) && NULL != videoDecoder_dx11)
                    if (SUCCEEDED(videoDecoder_dx11->GetCreationParameters(&videoDesc, &config)))
                        *profile = videoDesc.Guid;
                SAFE_RELEASE(videoDecoder_dx11);
            }
    #endif//MFX_D3D11_SUPPORT
            if (GUID_NULL == *profile)
            {
                IDirectXVideoDecoder *videoDecoder_dx9 = NULL;
                hr = videoDecoder->QueryInterface(
                    IID_IDirectXVideoDecoder,
                    (void **)&videoDecoder_dx9);
                if (SUCCEEDED(hr) && NULL != videoDecoder_dx9)
                    videoDecoder_dx9->GetCreationParameters(profile, NULL, NULL, NULL, NULL);
                SAFE_RELEASE(videoDecoder_dx9);
            }
        }

        // In case decoder GUID was failed to query make an assumption based on 
        // known platfrom capabilities.
        if (GUID_NULL == *profile)
        {
            switch (m_components[eDEC].m_params.mfx.CodecId)
            {
            case MFX_CODEC_MPEG2:
                *profile = DXVA2_ModeMPEG2_VLD;
                break;
            case MFX_CODEC_AVC:
                // asomsiko: for IronLake use DXVA2_Intel_EagleLake_ModeH264_VLD_NoFGT
                *profile = DXVA2_ModeH264_VLD_NoFGT;
                break;
            case MFX_CODEC_VC1:
                *profile = DXVA2_ModeVC1_VLD;
                break;
            default:
                // asomsiko: driver likely ignore profile for decoding unless it is not GUID_NULL.
                // So it might be an option to put some valid GUID here.
                return MFX_ERR_UNSUPPORTED; 
            }
        }
        return MFX_ERR_NONE;
    }

    virtual mfxStatus DecodeHeader()
    {
        MFX_CHECK_STS(T::DecodeHeader());

        MFX_CHECK_STS(CreatePAVPSessions());
        for(size_t i = 0; i < m_components.size(); i++)
            m_components[i].m_params.Protected = m_inParams.Protected;
        // reset file to the beginning
        m_pSpl->Close();
        m_bitstreamBuf.DataOffset = 0;
        m_bitstreamBuf.DataLength = 0;
        pcpEncryptor encryptor = {m_pavpVideo, CPAVPVideo_Encrypt, CPAVPVideo_Decrypt};
        MFX_CHECK_STS(m_pSpl->Init(m_inParams.strSrcFile));
        MFX_CHECK_STS(m_bitstreamReaderEncryptor->SetEncryptor(encryptor));

        return MFX_ERR_NONE;
    }

    virtual mfxStatus InitYUVSource()
    {
        MFX_CHECK_STS(T::InitYUVSource());

        HANDLE DXVA2DecodeHandle;
        
        MFX_CHECK_STS(m_components[eDEC].m_pSession->GetHandle((mfxHandleType)MFX_HANDLE_DECODER_DRIVER_HANDLE, &DXVA2DecodeHandle));
        if (NULL != m_authChannel9)
        {
            HRESULT hr = SO_CALL(SAuthChannel9_ConfigureCryptoSession, m_pavpdll, E_FAIL, (
                m_authChannel9, 
                dynamic_cast<CCPDeviceCryptoSession9*>(m_cpDevice)->GetCryptoSessionHandle(), 
                DXVA2DecodeHandle));
            if (FAILED(hr))
            {
                vm_string_printf(VM_STRING("Failed to associate decoder handle to authenticated channel (hr=0x%08x)\n"), hr);
                return MFX_ERR_DEVICE_FAILED;
            } 
        }
#if MFX_D3D11_SUPPORT
        else if (NULL != m_authChannel11)
        {

            HANDLE cryptoSessionHandle = NULL;
            dynamic_cast<CCPDeviceD3D11Base*>(m_cpDevice)->GetCryptoSessionPtr()->GetCryptoSessionHandle(&cryptoSessionHandle);
            HRESULT hr = SO_CALL(SAuthChannel11_ConfigureCryptoSession, m_pavpdll, E_FAIL, (
                m_authChannel11, 
                cryptoSessionHandle, 
                DXVA2DecodeHandle));
            if (FAILED(hr))
            {
                vm_string_printf(VM_STRING("Failed to associate decoder handle to authenticated channel (hr=0x%08x)\n"), hr);
                return MFX_ERR_DEVICE_FAILED;
            } 
        }
        else
        {
            vm_string_printf(VM_STRING("Expect authenticated channel to be created\n"));
            return MFX_ERR_DEVICE_FAILED;
        } 
#endif //MFX_D3D11_SUPPORT

        return MFX_ERR_NONE;
    };

    virtual mfxStatus CreateSplitter()
    {
        MFX_CHECK_STS(T::CreateSplitter());

        pcpEncryptor encryptor = {NULL, CPAVPVideo_Encrypt, CPAVPVideo_Decrypt};

        m_bitstreamReaderEncryptor = new BitstreamReaderEncryptor(
            encryptor, m_pSpl, &m_pavpdll, &m_inParams.BitstreamReaderEncryptor_params);
        MFX_CHECK_POINTER(m_bitstreamReaderEncryptor);
        MFX_CHECK_STS(m_bitstreamReaderEncryptor->PostInit());
        m_pSpl = m_bitstreamReaderEncryptor;
//        pSpl.release();

        return MFX_ERR_NONE;
    };

    virtual mfxStatus  ProcessCommandInternal(vm_char ** &argv, mfxI32 argc, bool bReportError = true)
    {
        MFX_CHECK_SET_ERR(NULL != argv, PE_OPTION, MFX_ERR_NULL_PTR);
        MFX_CHECK_SET_ERR(0 != argc, PE_OPTION, MFX_ERR_UNKNOWN);

        vm_char **     argvEnd = argv + argc;
        mfxStatus      mfxres;

        //print mode;
        if (m_OptProc.GetPrint())
        {
            vm_string_printf(VM_STRING("  Protection specific parameters:\n"));
        }

        for (; argv < argvEnd; argv++)
        {
            int nPattern = 0;
            if (0 != (nPattern = m_OptProc.Check(argv[0], VM_STRING("-protected(:pavp|:gpucp_pavp|:gpucp_aes128_ctr)")
                , VM_STRING("Configures for protected content processing simulation. Defines mode for mfxVideParam::Protected"))))
            {
                switch(nPattern)
                {
                    case 1 : m_inParams.Protected = MFX_PROTECTION_PAVP; break;
                    case 2 : m_inParams.Protected = MFX_PROTECTION_GPUCP_PAVP; break;
                    case 3 : m_inParams.Protected = MFX_PROTECTION_GPUCP_AES128_CTR; break;
                }
                m_inParams.bCompleteFrame = true;
            }
            else if (0 != (nPattern = m_OptProc.Check(argv[0], VM_STRING("-protection(:sw|:hw)")
                , VM_STRING("Choose SOFTWARE or HARDWARE protection."))))
            {
                switch(nPattern)
                {
                    case 1 : m_inParams.cpImpl = cpImplSW; break;
                    case 2 : m_inParams.cpImpl = cpImplHW; break;
                }
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-pavplibpath")
                , VM_STRING("Path to pavp library. Default is mfx_pavp.dll"), OPT_FILENAME))
            {
                MFX_CHECK(1 + argv != argvEnd);
                MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.strPAVPLibPath, MFX_ARRAY_SIZE(m_inParams.strPAVPLibPath), argv[1]));
                argv++;
            }
            else if (0 != (nPattern = m_OptProc.Check(argv[0], VM_STRING("-ctr_dec(:A|:B|:C)")
                , VM_STRING("Counter type to be used for decoder"))))
            {
                switch(nPattern)
                {
                    case 1 : m_inParams.ctr_dec_type = MFX_PAVP_CTR_TYPE_A; break;
                    case 2 : m_inParams.ctr_dec_type = MFX_PAVP_CTR_TYPE_B; break;
                    case 3 : m_inParams.ctr_dec_type = MFX_PAVP_CTR_TYPE_C; break;
                }
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-startctr_dec")
                , VM_STRING("Start counter value used for decoder"), OPT_UINT_64))
            {
                MFX_CHECK(1 + argv != argvEnd);
                m_inParams.startctr_dec_set = true;
                lexical_cast<mfxU64>(argv[1], m_inParams.startctr_dec);
                argv++;
            }
            else if (0 != (nPattern = m_OptProc.Check(argv[0], VM_STRING("-preparation(:fast|:full_encr)")
                , VM_STRING("Bitstream preparation mode. \"fast\" will skip non-key frames from encryption. \"fill_encr\" encrypt all frames."))))
            {
                switch(nPattern)
                {
                    case 1 : m_inParams.BitstreamReaderEncryptor_params.isFullEncrypt = false; break;
                    case 2 : m_inParams.BitstreamReaderEncryptor_params.isFullEncrypt = true; break;
                }
            }
            else if (0 != (nPattern = m_OptProc.Check(argv[0], VM_STRING("-ctr_enc(:A|:B|:C)")
                , VM_STRING("Counter type to be used for encoder"))))
            {
                switch(nPattern)
                {
                    case 1 : m_inParams.ctr_enc_type = MFX_PAVP_CTR_TYPE_A; break;
                    case 2 : m_inParams.ctr_enc_type = MFX_PAVP_CTR_TYPE_B; break;
                    case 3 : m_inParams.ctr_enc_type = MFX_PAVP_CTR_TYPE_C; break;
                }
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-startctr_enc")
                , VM_STRING("Start counter value used for encoder"), OPT_UINT_64))
            {
                MFX_CHECK(1 + argv != argvEnd);
                m_inParams.startctr_enc_set = true;
                lexical_cast<mfxU64>(argv[1], m_inParams.startctr_enc);
                argv++;
            }
            
            else //if (MFX_ERR_UNSUPPORTED == (mfxres = ProcessOption(argv, argvEnd)))
            {
                //check with base pipeline parser
                vm_char **myArgv = argv;
                if (MFX_ERR_UNSUPPORTED != (mfxres = T::ProcessCommandInternal( argv
                                                                               , (mfxI32)(argvEnd - argv)
                                                                               , false)))
                {
                    MFX_CHECK_STS(mfxres);
                }else
                {
                    //check whether base pipeline process at least one option
                    if (myArgv == argv)
                    {
                        MFX_TRACE_AT_EXIT_IF( MFX_ERR_UNSUPPORTED
                            , !bReportError
                            , PE_OPTION
                            , (VM_STRING("ERROR: Unknown option: %s\n"), argv[0]));
                    }
                    //decreasing pointer because baseclass parser points to first unknown option
                    argv--;
                }
            }/*else
            {
                MFX_CHECK_STS(mfxres);
            }*/
        }

        if (0 != m_inParams.Protected)
        {
            vm_char str[MAX_PATH+1024];
            _stprintf_p(str, sizeof(str)/sizeof(str[0]), VM_STRING("Loading mfx_pavp DLL as \"%s\""), m_inParams.strPAVPLibPath);
            OutputDebugString(str);

            if (!m_pavpdll.load(m_inParams.strPAVPLibPath, pavp_functions, pavp_functions_count))
            {
                MFX_TRACE_AT_EXIT_IF(MFX_ERR_UNSUPPORTED,
                    0,
                    PE_CHECK_PARAMS,
                    (VM_STRING("Failed to load \"%s\"\n"), m_inParams.strPAVPLibPath));
            }
        }


        return MFX_ERR_NONE;
    }

    virtual mfxU32 getOutputCodecId() = 0;
    BitstreamReaderEncryptor *m_bitstreamReaderEncryptor;
    CCPDevice *m_cpDevice;
    CPAVPSession *m_pavpSession;
    CVideoProtection *m_pavpVideo;
    SAuthChannel9 *m_authChannel9;
#if MFX_D3D11_SUPPORT
    SAuthChannel11 *m_authChannel11;
#endif//MFX_D3D11_SUPPORT
    SOProxy m_pavpdll;
};
