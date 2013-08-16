/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __PAVP_SAMPLE_COMMON_DX9__
#define __PAVP_SAMPLE_COMMON_DX9__

#include <d3d9.h>
#include "pavp_sample_common.h"
#include "pavp_dx9.h"
#include "pavp_dx9auxiliary.h"
#include "intel_pavp_core_api.h" // PAVP_SESSION_TYPE

/** Creates IDirect3D9Ex and IDirect3DDevice9Ex
@param[out] pd3d9 Pointer to receive IDirect3D9Ex interface pointer.
@param[out] ppd3d9DeviceEx Pointer to receive IDirect3DDevice9Ex interface pointer.
*/
HRESULT    CreateD3D9AndDeviceEx(IDirect3D9Ex ** pd3d9, IDirect3DDevice9Ex **ppd3d9DeviceEx);


/** Create CCPDeviceCryptoSession9 from IDirect3DDevice9Ex.
IDirect3DCryptoSession9 interface created with best avaliable 
capabilities. Counter type and encryption algorithm crypto session was created 
for aer returned through cryptoType argument. CCPDeviceCryptoSession9 object is 
created using new operator and returned through hwDevice pointer. Caller is 
responsible to free memory. 

Current implementation supports EPID based key exchange only.

@param pd3d9Device Pointer to IDirect3DDevice9Ex to be used to create 
    IDirect3DCryptoSession9 interace 
@param[in] profile Pass decoder profile GUID or GUID denoting transcoding session.
@param[in] encryptionType Encryption type used for input content.
@param[in] capsMask Ensures D3DCONTENTPROTECTIONCAPS::Caps contain at least one
    match with the capsMask.
@param[out] hwDevice Pointer to recieve pointer to the interface can execute 
    commands on HW. Caller is responsible to call delete operator to cleanup memory.
@param[out] cs9Handle Pointer to the HANDLE to receive crypto session handle. 
    The handle must be configured with D3DAUTHENTICATEDCONFIGURE_CRYPTOSESSION 
    to authenticated channel before beign used for video processing. 
@param[in,out] cryptoType Crypto type GUID to be used in 
    IDirect3DDevice9Video::CreateCryptoSession call. Pass GUID_NULL for 
    auto detection. On output crypto type GUID was used. Example values are
    D3DCRYPTOTYPE_AES128_CTR and D3DCRYPTOTYPE_INTEL_AES128_CTR.
*/
extern "C" HRESULT CCPDeviceCryptoSession9_create(
    IDirect3DDevice9Ex* pd3d9Device, 
    GUID profile,
    PAVP_ENCRYPTION_TYPE encryptionType,
    DWORD capsMask, 
    CCPDevice** hwDevice, 
    HANDLE *cs9Handle,
    GUID *cryptoType);

/** Create CCPDeviceAuxiliary9 object from IDirect3DDevice9Ex.
Intel auxiliary interface created and initialized for best avaliable 
capabilities. CCPDeviceAuxiliary9 object is created using new operator and 
returned through hwDevice pointer. Caller is responsible to free memory. 

Current implementation supports EPID based key exchange only.

@param pd3d9Device Pointer to IDirect3DDevice9Ex to be used to create 
    IDirect3DCryptoSession9 interace 
@param[in] sessionType Session type to initialize for.
@param[in] AvailableMemoryProtectionMask Pass combination of 
    PAVP_MEMORY_PROTECTION_MASK values. Function will ensures 
    PAVP_QUERY_CAPS::AvailableMemoryProtection contain at least one match with 
    the AvailableMemoryProtectionMask.
@param[out] hwDevice Pointer to recieve pointer to the interface can execute 
    commands on HW. Caller is responsible to call delete operator to cleanup memory.
*/
extern "C" HRESULT CCPDeviceAuxiliary9_create(
    IDirect3DDevice9Ex* pd3d9Device,
    PAVP_SESSION_TYPE sessionType,
    UINT AvailableMemoryProtectionMask, 
    CCPDevice** hwDevice);


/**
@defgroup AuthenticatedChannel9 DX9 Authenticated channel session sample implementation.

Authenticated channel is part of GPU-CP content protection.

@ingroup PAVPSession
*/

/** @ingroup AuthenticatedChannel9
@{
*/

/** Authenticated channel helper context.
*/
typedef struct _SAuthChannel9{
    IDirect3DAuthenticatedChannel9* authChannel; ///< Authenticated channel interface pointer.
    HANDLE authChannelHandle; ///< Authenticated channel handle.
    UINT uiConfigSequence; ///< Current config sequence number.
    UINT uiQuerySequence; ///< Current query sequence number.
    BYTE key[16]; ///< Session key.
} SAuthChannel9;

/** Create and initialize authenticated channel helper context.
Create authenticated channel interface and perform key exchange.
@param[in] ctx SAuthChannel9 context.
@param[in] pd3d9Device Pointer to IDirect3DDevice9Ex interface to create 
    authenticated channel for.
@param[in] channelType Authenticated channel type. Must match crypto session 
    implementation associated with.
*/
HRESULT SAuthChannel9_Create4(
    SAuthChannel9 **ctx,
    IDirect3DDevice9Ex *pd3d9Device, 
    D3DAUTHENTICATEDCHANNELTYPE channelType);

/** Create and initialize authenticated channel helper context.
Create authenticated channel interface and perform key exchange.
@param[in] ctx SAuthChannel9 context.
@param[in] hwDevice Pointer to CCPDeviceCryptoSession9 to be used indirectly to create 
    IDirect3DAuthenticatedChannel9 interface 
@param[in] pd3d9Device Pointer to IDirect3DDevice9Ex interface to create 
    authenticated channel for.
*/
extern "C" HRESULT SAuthChannel9_Create(
    SAuthChannel9 **ctx,
    CCPDevice* hwDevice,
    IDirect3DDevice9Ex *pd3d9Device);

/** Associate authenticated channel, crypto session and decoder device.
@param[in] ctx SAuthChannel9 context.
@param[in] cryptoSessionHandle Crypto session handle to associate.
@param[in] DXVA2DecodeHandle Decoder handle to associate.
*/
extern "C" HRESULT SAuthChannel9_ConfigureCryptoSession(
    SAuthChannel9 *ctx,
    HANDLE cryptoSessionHandle,
    HANDLE DXVA2DecodeHandle);

/** Destroy Authenticated channel and cleanup secret data.
@param[in] ctx SAuthChannel9 context.
*/
extern "C" void SAuthChannel9_Destroy(SAuthChannel9 *ctx);

/** @} */ //@ingroup AuthenticatedChannel9

#endif//__PAVP_SAMPLE_COMMON_DX9__