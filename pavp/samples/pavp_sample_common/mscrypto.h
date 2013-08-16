/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __AUTH_CHANNEL_H__
#define __AUTH_CHANNEL_H__

#include <windows.h>

HRESULT ComputeOMAC(
    BYTE AesKey[ 16 ],              // Session key
    PUCHAR pb,                      // Data
    DWORD cb,                       // Size of the data
    BYTE *pTag                      // Receives the OMAC
    );

HRESULT RSAES_OAEP_Encrypt(
    BYTE *Cert,
    UINT CertLen,
    const BYTE *pbDataIn,
    DWORD cbDataIn,
    BYTE **pbDataOut,
    DWORD *cbDataOut);

#endif//__AUTH_CHANNEL_H__