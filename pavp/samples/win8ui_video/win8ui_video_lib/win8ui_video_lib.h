/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __WIN8UI_VIDEO_LIB__
#define __WIN8UI_VIDEO_LIB__


#include <d3d11.h>
#include "pavp_video_lib.h"
#include "pavp_dx11.h"

extern "C" HRESULT CCPDeviceD3D11_create4(
    ID3D11Device* device, 
    const GUID *cryptoType, //can be null
    const GUID *profile,
    const GUID *keyExchangeType,
    CCPDevice** hwDevice);

extern "C" HRESULT CCPDeviceD3D11_NegotiateStreamKey(
    CCPDeviceD3D11Base *cpDevice,
    const uint8 *streamKey, 
    uint32 streamKeySize);

/** ID3D11CryptoSession interface video processing pipeline protection.
@ingroup avUsageCase
*/
class CVideoProtection_CS11: public CVideoProtection
  {
public:
    /** Create video processing pipeline protection through CCPDeviceD3D11 device with PAVP session opened.
    @param[in] cpDevice CP Device to create video processing pipeline protection.
    @param[in] session PAVP session to be used for protecting pipeline.
    */
    CVideoProtection_CS11(CCPDeviceD3D11Base *cpDevice);
    virtual ~CVideoProtection_CS11();

    virtual PavpEpidStatus Init(
        const void *encryptorConfig, 
        uint32 encryptorConfigSize);

    virtual PavpEpidStatus Encrypt(
        const uint8* src, 
        uint8* dst, 
        uint32 size, 
        void *encryptorConfig, 
        uint32 encryptorConfigSize);

    /// Utilize ID3D11VideoContext  StartSessionKeyRefresh and FinishSessionKeyRefresh for encryption key refresh.
    virtual PavpEpidStatus EncryptionKeyRefresh();
    virtual PavpEpidStatus Close();
private:
    CCPDeviceD3D11Base *m_d3d11CpDevice;
    D3D11_VIDEO_CONTENT_PROTECTION_CAPS m_contentProtectionCaps;

    CStreamEncryptor *m_encryptor;          ///< Encryptor object.
    uint8 m_encryptorBlockSize;             ///< Encryption alogithm block size.
    uint8 m_encryptionKey[16];              ///< Encryption key. @todo: wrap into class
    uint8 m_encryptionKeyBitSize;           ///< Encryption key bit size. @todo: wrap into class

    PAVPSDK_DISALLOW_COPY_AND_ASSIGN(CVideoProtection_CS11);
};

/** @} */ //@ingroup avUsageCase

#endif//__WIN8UI_VIDEO_LIB__