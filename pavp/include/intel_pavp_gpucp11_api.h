/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef _INTEL_PAVP_GPUCP11_API_H
#define _INTEL_PAVP_GPUCP11_API_H

#ifdef __cplusplus
extern "C" {
#endif


// DX11 GUIDs:

// {BFB8C6C3-E088-4547-86AA-C6D95C769317}
static const GUID D3D11_AUTHENTICATED_QUERY_PAVP_SESSION_STATUS = 
{ 0xbfb8c6c3, 0xe088, 0x4547, { 0x86, 0xaa, 0xc6, 0xd9, 0x5c, 0x76, 0x93, 0x17 } };

// {849D003F-9AF4-47b4-B550-4E3F5F271846}
static const GUID D3D11_AUTHENTICATED_QUERY_INTEL_WIDI_STATUS = 
{ 0x849d003f, 0x9af4, 0x47b4, { 0xb5, 0x50, 0x4e, 0x3f, 0x5f, 0x27, 0x18, 0x46 } };


#pragma pack(push)
#pragma pack(8)

// DX11 Structs:

typedef struct tagD3D11_AUTHENTICATED_QUERY_PAVP_STATUS_INPUT
{
    D3D11_AUTHENTICATED_QUERY_INPUT     Parameters;
    BOOL                                bSkipOmac;
} D3D11_AUTHENTICATED_QUERY_PAVP_STATUS_INPUT;

typedef struct tagD3D11_AUTHENTICATED_QUERY_PAVP_STATUS_OUTPUT
{
    D3D11_AUTHENTICATED_QUERY_OUTPUT    Output;
    BOOL                                bSessionAlive;
} D3D11_AUTHENTICATED_QUERY_PAVP_STATUS_OUTPUT;

typedef struct tagD3D11_AUTHENTICATED_QUERY_WIDI_STATUS_OUTPUT
{
    D3D11_AUTHENTICATED_QUERY_OUTPUT    Output;
    UINT                                WidiVersion;
    UINT                                HardwareID;
    BOOL                                bWidiDisplayActive;
    UINT                                NumActiveWirelessDisplays;
    UINT                                Reserved;
} D3D11_AUTHENTICATED_QUERY_WIDI_STATUS_OUTPUT;

#pragma pack(pop)


#ifdef __cplusplus
}
#endif

#endif//_INTEL_PAVP_GPUCP11_API_H