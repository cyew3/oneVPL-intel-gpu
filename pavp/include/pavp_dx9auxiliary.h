/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __PAVP_DX9AUXILIARY__
#define __PAVP_DX9AUXILIARY__

#include <d3d9.h>
#include <dxva2api.h>

#include "pavpsdk_pavpsession.h"
#include "intel_pavp_core_api.h"

/**@ingroup PAVPSession 
@{
*/

/// Verify if OS version and operation mode doesn't prohibit PAVP through Intel Auxiliary Device.
bool PAVPAuxiliary9_isPossible();

/** Creates Intel auxiliary device to be used for PAVP session commands.
@param[in] pD3DDevice9 Pointer to IDirect3DDevice9 interface.
@param[out] ppAuxDevice Pointer to IDirectXVideoDecoder interface pointer
    to recieve Intel auxiliary device created.
*/
HRESULT PAVPAuxiliary9_create(IDirect3DDevice9 *pD3DDevice9, IDirectXVideoDecoder** ppAuxDevice);

/** Query Intel auxiliary device PAVP_QUERY_CAPS capabilities.
@param[in] aux Intel auxiliary device. Use PAVPAuxiliary9_create to create it.
@param[out] pCaps Pointer top capabilities structure. Filled in with capabilities 
    on function return.
*/
HRESULT PAVPAuxiliary9_getPavpCaps(IDirectXVideoDecoder* aux, PAVP_QUERY_CAPS *pCaps);

/** Query Intel auxiliary device PAVP_QUERY_CAPS2 capabilities.
@param[in] aux Intel auxiliary device. Use PAVPAuxiliary9_create to create it.
@param[out] pCaps Pointer top capabilities structure. Filled in with capabilities 
    on function return.
*/
HRESULT PAVPAuxiliary9_getPavpCaps2(IDirectXVideoDecoder* aux, PAVP_QUERY_CAPS2 *pCaps);

/** Initialize Intel auxiliary device.
Initialize Intel auxiliary device for given key exchange and session type by
calling AUXDEV_CREATE_ACCEL_SERVICE function. Verify if requested 
configuration is supported via AUXDEV_QUERY_ACCEL_CAPS function.
@param[in] aux Intel auxiliary device. Use PAVPAuxiliary9_create to create it.
@param[in] keyExchange Key exchange to initialize for.
@param[in] sessionType Session type to initialize for.
*/
HRESULT PAVPAuxiliary9_initialize(
    IDirectXVideoDecoder* aux, 
    PAVP_KEY_EXCHANGE_MASK keyExchange,
    PAVP_SESSION_TYPE sessionType);

/** CCPDevice for DirectX Intel Auxiliary Device interface.
@ingroup PAVPSession
*/
class CCPDeviceAuxiliary9: public CCPDevice
{
public:
    /** Create device for Intel Auxiliary Device and key exchange function ID.
    @param[in] aux Intel auxiliary device. Use PAVPAuxiliary9_create to create it.
    @param[in] PAVPKeyExchangeFuncId Key exchange function to use. Example values
        are PAVP_KEY_EXCHANGE or PAVP_KEY_EXCHANGE_HEAVY.
    @param[in] sessionType PAVP session type Intel auxiliary device was 
        initialized for.
    */
    CCPDeviceAuxiliary9(
        IDirectXVideoDecoder* aux, 
        uint32 PAVPKeyExchangeFuncId, 
        PAVP_SESSION_TYPE sessionType);
    virtual ~CCPDeviceAuxiliary9();

    virtual PavpEpidStatus ExecuteKeyExchange(
        const void *dataIn,
        uint32 dizeIn,
        void *dataOut,
        uint32 sizeOut);

    virtual PavpEpidStatus ExecuteFunc(
        uint32 funcID,
        const void *dataIn,
        uint32 sizeIn,
        void *dataOut,
        uint32 sizeOut);

    /// Get Intel auxiliary device object was created for.
    IDirectXVideoDecoder* GetAuxPtr() { return m_aux; };
    /// Get key exchange fucntion in use.
    uint32 GetPAVPKeyExchangeFuncId() { return m_PAVPKeyExchangeFuncId; };
    PAVP_SESSION_TYPE GetSessionType() { return m_sessionType; };
private:
    IDirectXVideoDecoder* m_aux;
    uint32 m_PAVPKeyExchangeFuncId;
    PAVP_SESSION_TYPE m_sessionType;

    PAVPSDK_DISALLOW_COPY_AND_ASSIGN(CCPDeviceAuxiliary9);
};

/** @} */ //@ingroup PAVPSession 

#endif//__PAVP_DX9AUXILIARY__