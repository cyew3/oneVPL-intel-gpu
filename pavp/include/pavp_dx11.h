/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __PAVP_DX11__
#define __PAVP_DX11__

#include <d3d11.h>
#include "intel_pavp_types.h"
#include "pavpsdk_pavpsession.h"


/**@ingroup PAVPSession 
@{
*/

/** Find crypto type GUID to be used in ID3D1VideoDevice::CreateCryptoSession call.
@param[in] videoDevice Pointer to ID3D1VideoDevice interface.
@param[in] profile Pass decoder profile GUID or GUID denoting transcoding session.
@param[in] encryptionType Encryption type used for input content.
@param[in] keyExchangeType Key exchange type to find crypto type for. For 
    example D3DKEYEXCHANGE_EPID.
@param[in] capsMask Ensures D3DCONTENTPROTECTIONCAPS::Caps contain at least one
    match with the capsMask. Pass D3DCPCAPS_HARDWARE | D3DCPCAPS_SOFTWARE if 
    you need to find both PAVP Heavy and Lite configurations.
@param[out] caps If not NULL then on return will hold caps found.
@return Crypto type GUID to be used in IDirect3DDevice9Video::CreateCryptoSession 
    call or GUID_NULL if no crypto type found.
*/
GUID PAVPD3D11_findCryptoType(
    ID3D11VideoDevice *videoDevice, 
    GUID profile, 
    PAVP_ENCRYPTION_TYPE encryptionType,
    GUID keyExchangeType,
    DWORD capsMask,
    D3D11_VIDEO_CONTENT_PROTECTION_CAPS *caps);

/** CCPDevice for ID3D11CryptoSession interface.
@ingroup PAVPSession
*/
class CCPDeviceD3D11Base: public CCPDevice
{
public:
    /** Create device for ID3D11CryptoSession interface pointer and key exchange GUID.
    @param[in] cs Pointer to ID3D11CryptoSession interface created for ID3D11VideoDevice.
    @param[in] videoContext Pointer to ID3D11VideoContext queried from  ID3D11DeviceContext. 
    @param[in] keyExchangeGUID Key exchange GUID to use.
    */
    CCPDeviceD3D11Base(
        ID3D11CryptoSession* cs, 
        ID3D11VideoContext* videoContext, 
        GUID keyExchangeGUID,
        const D3D11_VIDEO_CONTENT_PROTECTION_CAPS *caps);
    virtual ~CCPDeviceD3D11Base();
 
    virtual PavpEpidStatus ExecuteKeyExchange(
        const void *dataIn,
        uint32 sizeIn,
        void *dataOut,
        uint32 sizeOut);

    virtual PavpEpidStatus ExecuteFunc(
        uint32 funcID,
        const void *dataIn,
        uint32 sizeIn,
        void *dataOut,
        uint32 sizeOut);

    /// Get ID3D11CryptoSession interface pointer object was created for.
    ID3D11CryptoSession* GetCryptoSessionPtr() { return m_cs; };
    /// Get ID3D11VideoContext interface pointer object was created for.
    ID3D11VideoContext* GetVideoContextPtr() { return m_videoContext; };
    /// Get key exchange GUID in use.
    GUID GetKeyExchangeGUID() { return m_keyExchangeGUID; };
    void GetContentProtectionCaps(D3D11_VIDEO_CONTENT_PROTECTION_CAPS *caps);
protected:
    ID3D11CryptoSession* m_cs;
    ID3D11VideoContext* m_videoContext;
    GUID m_keyExchangeGUID;
    D3D11_VIDEO_CONTENT_PROTECTION_CAPS m_caps;

    PAVPSDK_DISALLOW_COPY_AND_ASSIGN(CCPDeviceD3D11Base);
};


/** CCPDevice for ID3D11CryptoSession interface.
*/
class CCPDeviceD3D11: public CCPDeviceD3D11Base
{
public:
    /** Create device for ID3D11CryptoSession interface pointer and key exchange GUID.
    @param[in] cs Pointer to ID3D11CryptoSession interface created for ID3D11VideoDevice.
    @param[in] videoContext Pointer to ID3D11VideoContext queried from  ID3D11DeviceContext. 
    @param[in] PAVPKeyExchangeGUID Key exchange GUID to use. Example values
        are D3DKEYEXCHANGE_EPID or D3DKEYEXCHANGE_HW_PROTECT.
    */
    CCPDeviceD3D11(
        ID3D11CryptoSession* cs, 
        ID3D11VideoContext* videoContext, 
        GUID PAVPKeyExchangeGUID,
        const D3D11_VIDEO_CONTENT_PROTECTION_CAPS *caps);
 
    virtual PavpEpidStatus ExecuteKeyExchange(
        const void *dataIn,
        uint32 sizeIn,
        void *dataOut,
        uint32 sizeOut);

    virtual PavpEpidStatus ExecuteFunc(
        uint32 funcID,
        const void *dataIn,
        uint32 sizeIn,
        void *dataOut,
        uint32 sizeOut);

    GUID GetPAVPKeyExchangeGUID() { return m_keyExchangeGUID; };

    PAVPSDK_DISALLOW_COPY_AND_ASSIGN(CCPDeviceD3D11);
};


/** @} */ //@ingroup PAVPSession 

#endif//__PAVP_DX11__