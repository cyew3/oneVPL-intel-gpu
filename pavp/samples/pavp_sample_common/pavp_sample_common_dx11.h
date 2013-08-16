/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __PAVP_SAMPLE_COMMON_DX11__
#define __PAVP_SAMPLE_COMMON_DX11__


#include <d3d11.h>
#include "pavp_sample_common.h"
#include "pavp_dx11.h"

/** Create CCPDeviceD3D11 from ID3D11Device.
ID3D11CryptoSession interface created with best avaliable 
capabilities. Counter type and encryption algorithm crypto session was created 
for are returned through cryptoType argument. CCPDeviceCryptoSession9 object is 
created using new operator and returned through hwDevice pointer. Caller is 
responsible to free memory. 

Current implementation supports EPID based key exchange only.

@param[in] device Pointer to ID3D11Device to be used indirectly to create 
    ID3D11CryptoSession interface 
@param[in] profile Pass decoder profile GUID or GUID denoting transcoding session.
@param[in] encryptionType Encryption type used for input content.
@param[in] capsMask Ensures D3DCONTENTPROTECTIONCAPS::Caps contain at least one
    match with the capsMask.
@param[out] hwDevice Pointer to recieve pointer to the interface can execute 
    commands on HW. Caller is responsible to call delete operator to cleanup memory.
@param[in,out] cryptoType Crypto type GUID to be used in 
    IDirect3DDevice9Video::CreateCryptoSession call. Pass GUID_NULL for 
    auto detection. On output crypto type GUID was used. Example values are
    D3DCRYPTOTYPE_AES128_CTR and D3DCRYPTOTYPE_INTEL_AES128_CTR.
*/
extern "C" HRESULT CCPDeviceD3D11_create(
    ID3D11Device* device, 
    GUID profile,
    PAVP_ENCRYPTION_TYPE encryptionType,
    DWORD capsMask, 
    CCPDevice** hwDevice, 
    GUID *cryptoType);


/**
@defgroup AuthenticatedChannel11 DX11 Authenticated channel session sample implementation.

Authenticated channel is part of GPU-CP content protection.

@ingroup PAVPSession
*/

/** @ingroup AuthenticatedChannel11
@{
*/

/** Authenticated channel helper context.
*/
typedef struct _SAuthChannel11{
    ID3D11AuthenticatedChannel* authChannel; ///< Authenticated channel interface pointer.
    ID3D11VideoContext* videoContext; ///< Video context authenticated channel is for.
    UINT uiConfigSequence; ///< Current config sequence number.
    UINT uiQuerySequence; ///< Current query sequence number.
    BYTE key[16]; ///< Session key.
} SAuthChannel11;

/** Create and initialize authenticated channel helper context.
Create authenticated channel interface and perform key exchange.
@param[in] ctx SAuthChannel11 context.
@param[in] device Pointer to ID3D11Device to be used indirectly to create 
    ID3D11AuthenticatedChannel interface 
@param[in] channelType Authenticated channel type. Must match crypto session 
    implementation associated with.
*/
HRESULT SAuthChannel11_Create4(
    SAuthChannel11 **ctx,
    ID3D11Device* device, 
    D3D11_AUTHENTICATED_CHANNEL_TYPE channelType);

/** Create and initialize authenticated channel helper context.
Create authenticated channel interface and perform key exchange.
@param[in] ctx SAuthChannel11 context.
@param[in] hwDevice Pointer to CCPDeviceD3D11 to be used indirectly to create 
    ID3D11AuthenticatedChannel interface 
*/
extern "C" HRESULT SAuthChannel11_Create(
    SAuthChannel11 **ctx,
    CCPDevice* hwDevice);

/** Associate authenticated channel, crypto session and decoder device.
@param[in] ctx SAuthChannel11 context.
@param[in] cryptoSessionHandle Crypto session handle to associate.
@param[in] decoderHandle Decoder handle to associate.
*/
extern "C" HRESULT SAuthChannel11_ConfigureCryptoSession(
    SAuthChannel11 *ctx,
    HANDLE cryptoSessionHandle,
    HANDLE decoderHandle);

/** Destroy Authenticated channel and cleanup secret data.
@param[in] ctx SAuthChannel11 context.
*/
extern "C" void SAuthChannel11_Destroy(SAuthChannel11 *ctx);

/** @} */ //@ingroup AuthenticatedChannel11

#endif//__PAVP_SAMPLE_COMMON_DX11__