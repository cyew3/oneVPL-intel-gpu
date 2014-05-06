/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __PAVP_DX9__
#define __PAVP_DX9__

#include <d3d9.h>
#include "intel_pavp_types.h"
#include "pavpsdk_pavpsession.h"


/**@ingroup PAVPSession 
@{
*/

/// Verify if OS version and operation mode doesn't prohibit PAVP through IDirect3DCryptoSession9 interface.
bool PAVPCryptoSession9_isPossible();

/** Find crypto type GUID to be used in IDirect3DDevice9Video::CreateCryptoSession call.
@param[in] p3DDevice9Video Pointer to IDirect3DDevice9Video interface.
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
GUID PAVPCryptoSession9_findCryptoType(
    IDirect3DDevice9Video *p3DDevice9Video, 
    GUID profile, 
    PAVP_ENCRYPTION_TYPE encryptionType,
    GUID keyExchangeType,
    DWORD capsMask,
    D3DCONTENTPROTECTIONCAPS *caps);

/** CCPDevice for IDirect3DCryptoSession9 interface.
@ingroup PAVPSession
*/
class CCPDeviceCryptoSession9: public CCPDevice
{
public:
    /** Create device for IDirect3DCryptoSession9 interface pointer and key exchange GUID.
    @param[in] cs9 Pointer to IDirect3DCryptoSession9 interface.
    @param[in] cs9Handle IDirect3DCryptoSession9 object handle it was created with.
    @param[in] PAVPKeyExchangeGUID Key exchange GUID to use. Example values
        are D3DKEYEXCHANGE_EPID or D3DKEYEXCHANGE_HW_PROTECT.
    @param[in] cryptoType Crypto type supported. Example value is
        D3DCRYPTOTYPE_AES128_CTR.
    @param[in] profile Profile supported. Can be decoder profile GUID or 
        GUID denoting transcoding session.
    */
    CCPDeviceCryptoSession9(
        IDirect3DCryptoSession9* cs9, 
        HANDLE cs9Handle,
        GUID PAVPKeyExchangeGUID,
        GUID cryptoType,
        GUID profile,
        const D3DCONTENTPROTECTIONCAPS *caps);
    virtual ~CCPDeviceCryptoSession9();

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

    /// Get IDirect3DCryptoSession9 interface pointer object was created for.
    IDirect3DCryptoSession9* GetCryptoSessionPtr() { return m_cs9; };
    /// Get IDirect3DCryptoSession9 object handle it was created with.
    HANDLE GetCryptoSessionHandle() { return m_cs9Handle; };
    /// Get key exchange GUID in use.
    GUID GetPAVPKeyExchangeGUID() { return m_PAVPKeyExchangeGUID; };
    /// Get cryptoType GUID supported.
    GUID GetCryptoType() { return m_cryptoType; };
    /// Get profile supported.
    GUID GetProfile() { return m_profile; };
    void GetContentProtectionCaps(D3DCONTENTPROTECTIONCAPS *caps);
private:
    IDirect3DCryptoSession9* m_cs9;
    HANDLE m_cs9Handle;
    GUID m_PAVPKeyExchangeGUID;
    GUID m_cryptoType;
    GUID m_profile;
    D3DCONTENTPROTECTIONCAPS m_caps;

    PAVPSDK_DISALLOW_COPY_AND_ASSIGN(CCPDeviceCryptoSession9);
};

/** @} */ //@ingroup PAVPSession 

#endif//__PAVP_DX9__