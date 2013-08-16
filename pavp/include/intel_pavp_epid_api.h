/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef _INTEL_PAVP_EPID_API_H
#define _INTEL_PAVP_EPID_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "oscl_platform_def.h"
#include "intel_pavp_types.h"
#include "intel_pavp_error.h"

#define ECDSA_PRIVKEY_LEN              32
#define ECDSA_PUBKEY_LEN               64
#define ECDSA_SIGNATURE_LEN            64
#define EPID_PARAM_LEN                 876        // EPID cryptosystem context length
#define EPID_CERT_LEN                  392        // EPID certificate length
#define EPID_PUBKEY_LEN                328        // EPID cert length - EC-DSA sig length
#define EPID_SIG_LEN                   569        // EPID signature length
#define SIGMA_PUBLIC_KEY_LEN           64
#define SIGMA_SESSION_KEY_LEN          16
#define SIGMA_MAC_KEY_LEN              16
#define SIGMA_MAC_LEN                  32
#define PAVP_EPID_STREAM_KEY_LEN       16

// PAVP EPID Commands:
#define CMD_GET_HW_ECC_PUBKEY           0x00000003
#define CMD_EXCHG_HW_APP_CERT           0x00000004
#define CMD_CLOSE_SIGMA_SESSION         0x00000005
#define CMD_GET_STREAM_KEY              0x00000006
#define CMD_INV_STREAM_KEY              0x00000007
// PAVP_EPID_API_VERISON 2.0:
#define CMD_GET_PCH_CAPABILITIES        0x00000009
#define CMD_GET_STREAM_KEY_EX           0x0000000e
#define CMD_GET_HANDLE                  0x00000010

typedef unsigned char EcDsaPrivKey[ECDSA_PRIVKEY_LEN];
typedef unsigned char EcDsaPubKey[ECDSA_PUBKEY_LEN];
typedef unsigned char EcDsaSig[ECDSA_SIGNATURE_LEN];
typedef unsigned char EpidSig[EPID_SIG_LEN];
typedef unsigned char SigmaPubKey[SIGMA_PUBLIC_KEY_LEN];
typedef unsigned char SigmaSessionKey[SIGMA_SESSION_KEY_LEN];
typedef unsigned char SigmaMacKey[SIGMA_MAC_KEY_LEN];
typedef unsigned char HMAC[SIGMA_MAC_LEN];
typedef unsigned char StreamKey[PAVP_EPID_STREAM_KEY_LEN];
typedef uint32 PAVPStreamId;
typedef uint32 PAVPSessId;

typedef enum 
{
    PAVP_VIDEO_PATH = 0,
    PAVP_AUDIO_PATH
} PAVPPathId;

#pragma pack(push)
#pragma pack(1)

/*
**    3rd Party Certificate
*/
// Certificate Type Values for the 3p Signed CertificateType Field
/// Application's certificate
#define PAVP_EPID_PUBCERT3P_TYPE_APP         0x00000000
/// Server's certificate
#define PAVP_EPID_PUBCERT3P_TYPE_SERVER      0x00000001

// Certificate Type Values for the Intel Signed IntelSignedCertificateType Field (2.0 certificate only)
/// PAVP
#define PAVP_EPID_PUBCERT3P_TYPE_PAVP        0x00000000
/// Media Vault
#define PAVP_EPID_PUBCERT3P_TYPE_MV1_0       0x00000001

// Issuer id: Intel
#define PAVP_EPID_PUBCERT3P_ISSUER_ID        0x00000000

typedef struct tagSignBy3p 
{
    unsigned int    CertificateType;
    unsigned char   TimeValidStart[8];
    unsigned char   TimeValidEnd[8];
    unsigned int    Id3p;
    unsigned int    IssuerId;
    EcDsaPubKey     PubKey3p;
} _SignBy3p;

// PAVP 1.5 certificate structure
typedef struct _Cert3p 
{
    // 3rd Party signed part
    _SignBy3p           SignBy3p;
    EcDsaSig            Sign3p;
    // Intel signed part
    struct _SignByIntel 
    {
        unsigned char   TimeValidStart[8];
        unsigned char   TimeValidEnd[8];
        EcDsaPubKey PubKeyVerify3p;
    } SignByIntel;
    EcDsaSig            SignIntel;
} Cert3p;

// PAVP 2.0 certificate structure
typedef struct _Cert3p20
{
    // 3rd Party signed part
    _SignBy3p            SignBy3p;
    EcDsaSig                Sign3p;
    // Intel signed part
    struct _SignByIntel20
   {
        unsigned short       IntelSignedVersion;
        unsigned char        TimeValidStart[8];
        unsigned char        TimeValidEnd[8];
        unsigned short       IntelSignedCertificateType;
        EcDsaPubKey          PubKeyVerify3p;
    } SignByIntel20;
    EcDsaSig                SignIntel;
} Cert3p20;

/*
**    EPID Certificate
*/
typedef struct _EpidCert 
{
    unsigned char   PubKeyEpid[EPID_PUBKEY_LEN];
    EcDsaSig        SignIntel;
} EpidCert;

/*
**    Input/output message buffer common header
*/
typedef struct _PAVPCmdHdr 
{
    uint32    ApiVersion;
    uint32    CommandId;
    uint32    PavpCmdStatus;
    uint32    BufferLength;
} PAVPCmdHdr;

/*
**    Command: CMD_GET_PCH_CAPABILITIES
*/

typedef struct _PAVP_PCH_Capabilities
{
    PAVP_ENCRYPTION_TYPE    AudioMode;
    PAVP_COUNTER_TYPE       AudioCounterType;
    PAVP_ENDIANNESS_TYPE    AudioCounterEndiannessType;
    unsigned int            PAVPVersion;    
    uint8                   Time[8];
} PAVP_PCH_Capabilities;
/*
**    InBuffer:
*/
typedef struct _GetCapsInBuff
{
    PAVPCmdHdr  Header;
} GetCapsInBuff;
/*
**    OutBuffer:
*/
typedef struct _GetCapsOutBuff
{
    PAVPCmdHdr              Header;
    PAVP_PCH_Capabilities   Caps;
} GetCapsOutBuff;

/*
**    Command: CMD_GET_HW_ECC_PUBKEY
**
**    InBuffer:
*/
typedef struct _GetHwEccPubKeyInBuff 
{
    PAVPCmdHdr    Header;
} GetHwEccPubKeyInBuff;
/*
**    OutBuffer:
*/
typedef struct _GetHwEccPubKeyOutBuff 
{
    PAVPCmdHdr    Header;
    PAVPSessId    SigmaSessionId;
    SigmaPubKey   Ga;
} GetHwEccPubKeyOutBuff;

/*
**    Command: CMD_EXCHG_HW_APP_CERT
**
**    InBuffer (PAVP 1.5):
*/
typedef struct _HwAppExchgCertsInBuff 
{
    PAVPCmdHdr    Header;
    PAVPSessId    SigmaSessionId;
    SigmaPubKey   Gb;
    Cert3p        Certificate3p;
    HMAC          Certificate3pHmac;
    EcDsaSig      EcDsaSigGaGb;
} HwAppExchgCertsInBuff;
/*
**    InBuffer (PAVP 2.0)
*/
typedef struct _HwAppExchgCertsInBuff20 {
   PAVPCmdHdr     Header;
   PAVPSessId     SigmaSessionId;
   SigmaPubKey    Gb;
   Cert3p20       Certificate3p;
   HMAC           Certificate3pHmac;
   EcDsaSig       EcDsaSigGaGb;
} HwAppExchgCertsInBuff20;
/*
**    OutBuffer:
*/
typedef struct _HwAppExchgCertsOutBuff 
{
    PAVPCmdHdr  Header;
    PAVPSessId  SigmaSessionId;
    EpidCert    CertificatePch;
    HMAC        CertificatePchHmac;
    EpidSig     EpidSigGaGb;
} HwAppExchgCertsOutBuff;

/*
**    Command: CMD_CLOSE_SIGMA_SESSION
**
**    InBuffer:
*/
typedef struct _CloseSigmaSessionInBuff 
{
    PAVPCmdHdr  Header;
    PAVPSessId  SigmaSessionId;
} CloseSigmaSessionInBuff;
/*
**    OutBuffer:
*/
typedef struct _CloseSigmaSessionOutBuff
{
    PAVPCmdHdr  Header;
    PAVPSessId  SigmaSessionId;
} CloseSigmaSessionOutBuff;

/*
**    Command: CMD_GET_STREAM_KEY
**
**    InBuffer:
*/
typedef struct _GetStreamKeyInBuff
{
    PAVPCmdHdr      Header;
    PAVPSessId      SigmaSessionId;
    PAVPStreamId    StreamId;
    PAVPPathId      MediaPathId;
} GetStreamKeyInBuff;
/*
**    OutBuffer:
*/
typedef struct _GetStreamKeyOutBuff
{
    PAVPCmdHdr   Header;
    PAVPSessId   SigmaSessionId;
    StreamKey    WrappedStreamKey;
    HMAC         WrappedStreamKeyHmac;
} GetStreamKeyOutBuff;

typedef struct _GetStreamKeyExInBuff
{
    PAVPCmdHdr  Header;
    PAVPSessId  SigmaSessionId;
} GetStreamKeyExInBuff;

typedef struct _GetStreamKeyExOutBuff
{
    PAVPCmdHdr  Header;
    PAVPSessId  SigmaSessionId;
    StreamKey   WrappedStreamDKey;
    StreamKey   WrappedStreamEKey;
    HMAC        WrappedStreamKeyHmac;
} GetStreamKeyExOutBuff;

/*
**    Command: CMD_INV_STREAM_KEY
**
**    InBuffer:
*/
typedef struct _InvStreamKeyInBuff
{
    PAVPCmdHdr      Header;
    PAVPSessId      SigmaSessionId;
    PAVPStreamId    StreamId;
    PAVPPathId      MediaPathId;
} InvStreamKeyInBuff;
/*
**    OutBuffer:
*/
typedef struct _InvStreamKeyOutBuff
{
    PAVPCmdHdr  Header;
    PAVPSessId  SigmaSessionId;
} InvStreamKeyOutBuff;


#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // _INTEL_PAVP_EPID_API_H
