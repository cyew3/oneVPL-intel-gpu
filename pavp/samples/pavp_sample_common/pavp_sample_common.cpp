/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#include "pavpsdk_defs.h"

#include <tchar.h>
#include <stdio.h>
#include <windows.h>
#include "pavp_sample_common.h"
#include "intel_pavp_core_api.h"


#define DO_VALUE_STRING_PAIR(val) {(val), _T(#val)}
#define _DO_VALUE_STRING_PAIR(val) {(pavpsdk_##val), _T(#val)}


GUID pavpsdk_D3DCRYPTOTYPE_AES128_CTR = {0x9b6bd711, 0x4f74, 0x41c9, {0x9e, 0x7b, 0xb, 0xe2, 0xd7, 0xd9, 0x3b, 0x4f}};
GUID pavpsdk_DXVA2_ModeH264_E = {0x1b81be68, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}};
GUID pavpsdk_D3D11_CRYPTO_TYPE_AES128_CTR = {0x9b6bd711, 0x4f74, 0x41c9, {0x9e, 0x7b, 0xb, 0xe2, 0xd7, 0xd9, 0x3b, 0x4f}};
GUID pavpsdk_GUID_NULL = {0,0,0,{0,0,0,0,0,0,0,0}};

// intel_pavp_gpucp_api.h
GUID pavpsdk_D3DKEYEXCHANGE_CANTIGA = 
{ 0xb15afe59, 0x70bf, 0x4063, { 0xa7, 0x33, 0xea, 0xe1, 0xa2, 0xe7, 0x82, 0x42 } };
GUID pavpsdk_D3DKEYEXCHANGE_EAGLELAKE  = 
{ 0xa75334c7, 0x80c, 0x4539, { 0x8e, 0x2d, 0xe6, 0xa0, 0xd2, 0xb1, 0xf, 0xf0 } };
GUID pavpsdk_D3DKEYEXCHANGE_EPID = 
{ 0xD13D3283, 0x9154, 0x43a2, { 0x90, 0x3A, 0xF7, 0xE0, 0xF9, 0x2D, 0x0A, 0xB5 } };
GUID pavpsdk_D3DKEYEXCHANGE_HW_PROTECT = 
{ 0x45f55235, 0x1d9f, 0x4c9e, { 0x8c, 0x5, 0x80, 0xa9, 0x6f, 0x1f, 0xd9, 0xad } };
GUID pavpsdk_D3DCRYPTOTYPE_INTEL_AES128_CTR  =
{ 0xe29faf83, 0xaabf, 0x48c8, { 0xa3, 0xab, 0x85, 0xfe, 0xdd, 0x5e, 0x60, 0xC4 } };
GUID pavpsdk_D3DCRYPTOTYPE_INTEL_AES128_CBC = 
{ 0xda3538fa, 0xb5cb, 0x4d58, { 0x86, 0xc3, 0xbd, 0x6c, 0x0, 0xf7, 0x58, 0xa3 } };
GUID pavpsdk_D3DCRYPTOTYPE_INTEL_DECE_AES128_CTR  =
{ 0x8DD4D67F, 0xABCE, 0x4D9B, { 0xB7, 0xC8, 0x2C, 0xCF, 0xB9, 0x41, 0x8C, 0x7B } };
GUID pavpsdk_GUID_INTEL_TRANSCODE_SESSION = 
{ 0xefcf2910, 0xd0f0, 0x4e49, { 0x8d, 0x3c, 0x13, 0xdb, 0x6c, 0x68, 0x1b, 0x3b } };
GUID pavpsdk_GUID_INTEL_PROPRIETARY_FUNCTION = 
{ 0xe8c7e79f, 0x10b7, 0x43d6, { 0xa1, 0x77, 0xdf, 0xa3, 0x9c, 0x6b, 0x25, 0x25 } };
GUID pavpsdk_INTEL_QUERY_WIDI_STATUS = 
{ 0xe4588ff4, 0x0055, 0x4986, { 0x9d, 0x08, 0xef, 0xec, 0xbf, 0x86, 0x42, 0x12 } };
GUID pavpsdk_D3DAUTHENTICATEDQUERY_PAVP_SESSION_STATUS    = 
{ 0xfee9dd76, 0x1673, 0x4598, {0x8e, 0x58, 0x81, 0x0b, 0x6f, 0x6a, 0xe5, 0x73 } };
GUID pavpsdk_INTEL_CONFIG_WIDI_SETFRAMERATE = 
{ 0xd9e6c700, 0xdc2d, 0x4812, { 0xbf, 0xde, 0x0f, 0x18, 0x32, 0xf4, 0x78, 0x7f } };

struct SGUID2string{
    GUID val; 
    TCHAR *str;
} guids[] = {
    // codec profiles for crypto session
    _DO_VALUE_STRING_PAIR(DXVA2_ModeH264_E),
    _DO_VALUE_STRING_PAIR(GUID_INTEL_TRANSCODE_SESSION),

    // Key exchange types for crypto session
    _DO_VALUE_STRING_PAIR(D3DKEYEXCHANGE_EPID),
    _DO_VALUE_STRING_PAIR(D3DKEYEXCHANGE_HW_PROTECT),

    // crypto types for crypto session
    _DO_VALUE_STRING_PAIR(D3DCRYPTOTYPE_INTEL_AES128_CTR),
    _DO_VALUE_STRING_PAIR(D3DCRYPTOTYPE_INTEL_AES128_CBC),
    _DO_VALUE_STRING_PAIR(D3DCRYPTOTYPE_INTEL_DECE_AES128_CTR),
    _DO_VALUE_STRING_PAIR(D3DCRYPTOTYPE_AES128_CTR),
    _DO_VALUE_STRING_PAIR(D3D11_CRYPTO_TYPE_AES128_CTR),

    _DO_VALUE_STRING_PAIR(GUID_NULL),
    {0,0}
};

#define pavpsdk_PAVP_SESSION_TYPE_DECODE        1
#define pavpsdk_PAVP_SESSION_TYPE_TRANSCODE     2

#define pavpsdk_PAVP_KEY_EXCHANGE_DEFAULT       0
#define pavpsdk_PAVP_KEY_EXCHANGE_CANTIGA       1
#define pavpsdk_PAVP_KEY_EXCHANGE_EAGLELAKE     2
#define pavpsdk_PAVP_KEY_EXCHANGE_HEAVY         4
#define pavpsdk_PAVP_KEY_EXCHANGE_IRONLAKE      8
#define pavpsdk_PAVP_KEY_EXCHANGE_DAA           16


struct SNumber2string{
    uint32 val; 
    TCHAR *str;
} numbers[] = {
    //PAVP_SESSION_TYPE
    _DO_VALUE_STRING_PAIR(PAVP_SESSION_TYPE_DECODE),
    _DO_VALUE_STRING_PAIR(PAVP_SESSION_TYPE_TRANSCODE),

    //PAVP_KEY_EXCHANGE_MASK
    _DO_VALUE_STRING_PAIR(PAVP_KEY_EXCHANGE_CANTIGA),
    _DO_VALUE_STRING_PAIR(PAVP_KEY_EXCHANGE_EAGLELAKE),
    _DO_VALUE_STRING_PAIR(PAVP_KEY_EXCHANGE_IRONLAKE),
    _DO_VALUE_STRING_PAIR(PAVP_KEY_EXCHANGE_DAA),

    {0,0}
};


void PAVPSDK_printGUID(GUID val)
{
    for (int i = 0; NULL != guids[i].str; i++)
        if (val == guids[i].val)
        {
            _tprintf(guids[i].str);
            return;
        }
    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("undefined")));
}


void PAVPSDK_printNumber(uint64 val)
{
    for (int i = 0; NULL != numbers[i].str; i++)
        if (val == numbers[i].val)
        {
            _tprintf(numbers[i].str);
            return;
        }
    PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("%I64d undefined"), val));
}
