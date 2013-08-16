/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __PAVP_VIDEO_LIB_DX11__
#define __PAVP_VIDEO_LIB_DX11__

#include <d3d11.h>
#include "pavp_video_lib.h"
#include "pavp_sample_common_dx11.h"

/**
@ingroup avUsageCase
@{
*/

/** ID3D11CryptoSession interface video processing pipeline protection.
@ingroup avUsageCase
*/
class CPAVPVideo_D3D11: public CPAVPVideo
{
public:
    /** Create video processing pipeline protection through CCPDeviceD3D11 device with PAVP session opened.
    @param[in] cpDevice CP Device to create video processing pipeline protection.
    @param[in] session PAVP session to be used for protecting pipeline.
    */
    CPAVPVideo_D3D11(CCPDeviceD3D11 *cpDevice, CPAVPSession *session);

    /** A simple way to set desired or autodetect decryptor mode of operations.
    Must be called before init.
    @param[in] decryptorMode Decryptor mode of operation or NULL to autodetect.
    */
    PavpEpidStatus PreInit(const PAVP_ENCRYPTION_MODE *decryptorMode);

    virtual PavpEpidStatus Init(
        uint32 streamId, 
        const void *encryptorConfig, 
        uint32 encryptorConfigSize);

    /// Utilize ID3D11VideoContext  StartSessionKeyRefresh and FinishSessionKeyRefresh for encryption key refresh.
    virtual PavpEpidStatus EncryptionKeyRefresh();
    virtual PavpEpidStatus Close();
private:
    uint32 m_streamId;
    CPAVPSession *m_session;
    CCPDeviceD3D11 *m_d3d11CpDevice;
    D3D11_VIDEO_CONTENT_PROTECTION_CAPS m_contentProtectionCaps;

    PAVPSDK_DISALLOW_COPY_AND_ASSIGN(CPAVPVideo_D3D11);
};

/** @} */ //@ingroup avUsageCase

#endif//__PAVP_VIDEO_LIB_DX11__