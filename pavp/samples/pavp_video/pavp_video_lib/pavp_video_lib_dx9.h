/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __PAVP_VIDEO_LIB_DX9__
#define __PAVP_VIDEO_LIB_DX9__

#include <d3d9.h>

#include "pavp_video_lib.h"
#include "pavp_sample_common_dx9.h"

/**
@ingroup avUsageCase
@{
*/

/** Intel Auxiliary Interface video processing pipeline protection.
@ingroup avUsageCase
*/
class CPAVPVideo_Auxiliary9: public CPAVPVideo
{
public:
    /** Create video processing pipeline protection through CCPDeviceAuxiliary9 device with PAVP session opened.
    @param[in] cpDevice CP Device to create video processing pipeline protection.
    @param[in] session PAVP session to be used for protecting pipeline.
    */
    CPAVPVideo_Auxiliary9(
        CCPDeviceAuxiliary9 *cpDevice, 
        CPAVPSession *session);

    /** A simple way to set desired or autodetect encryptor and decryptor mode of operations.
    Must be called before init.
    @param[in] encryptorMode Encryptor mode of operation or NULL to autodetect.
    @param[in] decryptorMode Decryptor mode of operation or NULL to autodetect.
    */
    PavpEpidStatus PreInit(
        const PAVP_ENCRYPTION_MODE *encryptorMode,
        const PAVP_ENCRYPTION_MODE *decryptorMode);

    virtual PavpEpidStatus Init(
        uint32 streamId, 
        const void *encryptorConfig, 
        uint32 encryptorConfigSize);

    virtual PavpEpidStatus Close();

    /* * Get encryptor counter type.
    @param[out] ctrType Pointer value will hold encryptor counter type on output.
    */
//    PavpEpidStatus GetEncryptorCounterType(PAVP_COUNTER_TYPE *ctrType);

private:
    uint32 m_streamId;
    CPAVPSession *m_session;
    CCPDeviceAuxiliary9 *m_aux;
    PAVP_COUNTER_TYPE m_ctrType;

    PAVPSDK_DISALLOW_COPY_AND_ASSIGN(CPAVPVideo_Auxiliary9);
};

/// Notify the driver of the protected content display position.
PavpEpidStatus CCPDevice_SetWindowPosition(CCPDevice *cpDevice, HWND hwnd);

/** IDirect3DCryptoSession9 interface video processing pipeline protection.
@ingroup avUsageCase
*/
class CPAVPVideo_CryptoSession9: public CPAVPVideo
{
public:
    /** Create video processing pipeline protection through CCPDeviceCryptoSession9 device with PAVP session opened.
    @param[in] cpDevice CP Device to create video processing pipeline protection.
    @param[in] session PAVP session to be used for protecting pipeline.
    */
    CPAVPVideo_CryptoSession9(CCPDeviceCryptoSession9 *cpDevice, CPAVPSession *session);

    /** A simple way to set desired or autodetect decryptor mode of operations.
    Must be called before init.
    @param[in] decryptorMode Decryptor mode of operation or NULL to autodetect.
    */
    PavpEpidStatus PreInit(const PAVP_ENCRYPTION_MODE *decryptorMode);

    virtual PavpEpidStatus Init(
        uint32 streamId, 
        const void *encryptorConfig, 
        uint32 encryptorConfigSize);

    /// Utilize IDirect3DCryptoSession9::StartSessionKeyRefresh FinishSessionKeyRefresh for encryption key refresh.
    virtual PavpEpidStatus EncryptionKeyRefresh();
    virtual PavpEpidStatus Close();
protected:
    virtual PavpEpidStatus getVideoTranscodingStreamKeys(
        uint8 *decodeStreamKey, 
        uint8 *encodeStreamKey);

    virtual PavpEpidStatus getDecodingStreamKey(
        uint32 streamId, 
        uint8 *streamKey);

    uint32 m_streamId;
    CPAVPSession *m_session;
    CCPDeviceCryptoSession9 *m_cs9Device;
    D3DCONTENTPROTECTIONCAPS m_contentProtectionCaps;

private:
    PAVPSDK_DISALLOW_COPY_AND_ASSIGN(CPAVPVideo_CryptoSession9);
};

/** @} */ //@ingroup avUsageCase

#endif//__PAVP_VIDEO_LIB_DX9__