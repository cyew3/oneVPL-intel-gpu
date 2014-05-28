//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright(c) 2011-2011 Intel Corporation. All Rights Reserved.
//

#ifndef PRIVATE_QUERY_DEVICE_INCLUDE
#define PRIVATE_QUERY_DEVICE_INCLUDE

#include "auxiliary_device.h"

// Private Query Device
// {56D269F0-C210-421C-8538-BE4ECB7C0AE2}
static const GUID DXVA2_PrivateQueryDevice =
{ 0x56d269f0, 0xc210, 0x421c, { 0x85, 0x38, 0xbe, 0x4e, 0xcb, 0x7c, 0x0a, 0xe2 } };

// Structures use 64-bit alignment
#pragma pack(8)

// Private Query ID enums
typedef enum _PRIVATE_QUERY_ID
{
    QUERY_ID_DUAL_DXVA_DECODE_V1    = 1,
    QUERY_ID_DUAL_DXVA_DECODE_V2    = 2,
    QUERY_ID_SET_VPP_S3D_MODE       = 3
} PRIVATE_QUERY_ID;

// Private Query Header
typedef struct _QUERY_HEADER
{
    INT             iPrivateQueryID;    // [IN    ] Must be QUERY_ID_*
    INT             iReserved;          // [IN/OUT] Reserved, MBZ
} QUERY_HEADER, *PQUERY_HEADER;

// Stream information for Query Dual Dxva Decode v2
typedef struct _QUERY_STREAM_V2
{
    UINT            Width;                  // Video Width
    UINT            Height;                 // Video Height
    UINT            VideoFormat     : 4;    // Video Format (Codec)
    UINT            SourceFrameRate : 8;    // Frame rate
    BOOL            Interlaced      : 1;    // Interlaced
    UINT            Reserved1       : 19;   // Reserved1 - MBZ
    UINT            Reserved2;              // Reserved2 - MBZ
} QUERY_STREAM_V2, *PQUERY_STREAM_V2;


// Private Query for Dual Dxva Decode v1
typedef struct _QUERY_DUAL_DXVA_DECODE_V1
{
    QUERY_HEADER    Header;         // [IN] Header - QUERY_ID_DUAL_DXVA_DECODE_V1
    UINT            Width1;         // [IN] Width of first video stream
    UINT            Height1;        // [IN] Height of first video stream
    UINT            Width2;         // [IN] Width of second video stream
    UINT            Height2;        // [IN] Height of second video stream
    BOOL            bSupported;     // [OUT] = TRUE/FALSE
} QUERY_DUAL_DXVA_DECODE_V1, *PQUERY_DUAL_DXVA_DECODE_V1;


// Private Query for Dual Dxva Decode v2
typedef struct _QUERY_DUAL_DXVA_DECODE_V2
{
    QUERY_HEADER    Header;         // [IN] Header - QUERY_ID_DUAL_DXVA_DECODE_V2
    QUERY_STREAM_V2 Stream1;        // [IN] Stream 1 parameters
    QUERY_STREAM_V2 Stream2;        // [IN] Stream 2 parameters
    INT             Reserved[4];    // Reserved (MBZ)
    BOOL            bSupported;     // [OUT] = TRUE/FALSE
} QUERY_DUAL_DXVA_DECODE_V2, *PQUERY_DUAL_DXVA_DECODE_V2;

// Private query S3D operation
typedef enum _QUERY_S3D_MODE
{
    QUERY_S3D_MODE_NONE          = 0,
    QUERY_S3D_MODE_L             = 1,
    QUERY_S3D_MODE_R             = 2
} QUERY_S3D_MODE;

// Private Query for setting up S3D mode for VPP creation
typedef struct _QUERY_SET_VPP_S3D_MODE
{
    QUERY_HEADER        Header;     // [IN] Header - QUERY_ID_S3D
    QUERY_S3D_MODE      S3D_mode;   // [IN] Indicate L or R view
} QUERY_SET_VPP_S3D_MODE, *PQUERY_SET_VPP_S3D_MODE;

#pragma pack()

class CPrivateQueryDevice : public CIntelAuxiliaryDevice
{
private:
    BOOL    bIsPresent;

    IDirectXVideoProcessorService *m_pVPservice;

public:
    CPrivateQueryDevice (IDirect3DDevice9 *pD3DDevice9);

    HRESULT QueryDualDxvaDecode(
        UINT            Width1,
        UINT            Height1,
        UINT            Width2,
        UINT            Height2,
        BOOL *          pSupported);

    HRESULT QueryDualDxvaDecodeV2(
        QUERY_STREAM_V2 Stream1,
        QUERY_STREAM_V2 Stream2,
        BOOL *          pSupported);

    HRESULT SelectVppS3dMode(
        QUERY_S3D_MODE  S3D_mode);

    BOOL    IsPresent() { return bIsPresent; }
};


#endif // PRIVATE_QUERY_DEVICE_INCLUDE
