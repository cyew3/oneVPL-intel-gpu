/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation    and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"

#ifdef PAVP_BUILD

#include "mfx_pipeline_protected.h"

const GUID DXVA2_Intel_Encode_AVC = 
{0x97688186, 0x56A8, 0x4094, {0xB5, 0x43, 0xFC, 0x9D, 0xAA, 0xA4, 0x9F, 0x4B}};
const GUID DXVA2_Intel_Encode_MPEG2 = 
{0xC346E8A3, 0xCBED, 0x4d27, {0x87, 0xCC, 0xA7, 0x0E, 0xB4, 0xDC, 0x8C, 0x27}};

mfxU64 SwapEndian(mfxU64 val)
{
    return
       ((val << 56) & 0xff00000000000000) | ((val << 40) & 0x00ff000000000000) |
       ((val << 24) & 0x0000ff0000000000) | ((val <<  8) & 0x000000ff00000000) |
       ((val >>  8) & 0x00000000ff000000) | ((val >> 24) & 0x0000000000ff0000) |
       ((val >> 40) & 0x000000000000ff00) | ((val >> 56) & 0x00000000000000ff);
}

mfxU64 AdjustCtr(mfxU64 counter, mfxU16 type)
{
    if (MFX_PAVP_CTR_TYPE_A == type)
        return counter & ~(mfxU64)0x00000000FFFFFFFF; // counter is 32 bits in big endian
    return counter;
}

bool RandomizeIVCtr(mfxAES128CipherCounter &iv_ctr, mfxU16 type)
{
    if (0 != BCryptGenRandom(NULL, (BYTE*)&iv_ctr, sizeof(iv_ctr), BCRYPT_USE_SYSTEM_PREFERRED_RNG))
        return false;
    if (MFX_PAVP_CTR_TYPE_A == type
        || MFX_PAVP_CTR_TYPE_B == type)
        iv_ctr.IV = 0;
    iv_ctr.Count = AdjustCtr(iv_ctr.Count, type);
    return true;
}


mfxStatus CPAVPVideo_Encrypt(mfxHDL pthis, mfxU8 *src, mfxU8 *dst, mfxU32 nbytes, 
    mfxAES128CipherCounter *cipherCounter)
{
    if (NULL == pthis)
        memset(dst, 0xff, nbytes);
    else
    {
        PavpEpidStatus sts = ((CPAVPVideo*)pthis)->Encrypt(src, dst, nbytes, cipherCounter, sizeof(mfxAES128CipherCounter));
        if (PAVP_STATUS_REFRESH_REQUIRED_ERROR == sts)
            return MFX_WRN_OUT_OF_RANGE;
        if (PAVP_EPID_FAILURE(sts))
            return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}

mfxStatus CPAVPVideo_Decrypt(mfxHDL pthis, mfxU8 *src, mfxU8 *dst, mfxU32 nbytes, 
    const mfxAES128CipherCounter& cipherCounter)
{
    if (NULL == pthis)
        memset(dst, 0xff, nbytes);
    else
    {
        PavpEpidStatus sts = ((CPAVPVideo*)pthis)->Decrypt(src, dst, nbytes, &cipherCounter, sizeof(cipherCounter));
        if (PAVP_STATUS_REFRESH_REQUIRED_ERROR == sts)
            return MFX_WRN_OUT_OF_RANGE;
        if (PAVP_EPID_FAILURE(sts))
            return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}

const char* pavp_functions[] = {
    ("PCPVideoDecode_Init"),
    ("PCPVideoDecode_SetEncryptor"),
    ("PCPVideoDecode_Close"),
    ("PCPVideoDecode_PrepareNextFrame"),
    
    ("CCPDeviceD3D11_create"),
    ("CCPDeviceD3D11_create4"),
("CCPDeviceD3D11_NegotiateStreamKey"),
    ("SAuthChannel11_Create"),
    ("SAuthChannel11_ConfigureCryptoSession"),
("SAuthChannel11_Destroy"),

    ("CCPDeviceCryptoSession9_create"),
    ("CCPDeviceAuxiliary9_create"),
    ("SAuthChannel9_Create"),
    ("SAuthChannel9_ConfigureCryptoSession"),
("SAuthChannel9_Destroy"),

    ("Test_v10_Cert3p"),
    ("Test_v10_Cert3pSize"),
    ("Test_v10_ApiVersion"),
    ("Test_v10_DsaPrivateKey"),

    ("CreateCSIGMASession"),
    ("CreateCVideoProtection_CS11"),
    ("CreateCPAVPVideo_D3D11"),
    ("CreateCPAVPVideo_CryptoSession9"),
    ("CreateCPAVPVideo_Auxiliary9"),
    ("mfxPAVPDestroy")
};

const mfxU32 pavp_functions_count = sizeof(pavp_functions)/sizeof(pavp_functions[0]);

#endif//PAVP_BUILD