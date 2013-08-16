/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 Intel Corporation. All Rights Reserved.
//
*/
#include <windows.h>

#include "sigma_crypto_utils.h"
#include "intel_pavp_epid_api.h"
#include "le2be_macros.h"
#include "epid_errors.h"
#include "epid_macros.h"
#include "epid_verifier.h"


using namespace std;

const uint8 EpidParamsCertData[] = { 
0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x89, 0x57, 0x3f, 0x17, 0x47, 0x30, 0x8c, 0x43, 0xd5, 0xee, 
0x41, 0x97, 0x96, 0x19, 0x72, 0xbb, 0x86, 0x88, 0xed, 0x4b, 0xef, 0x04, 0xab, 0xae, 0xc3, 0x8e, 
0xec, 0x51, 0xc3, 0xd3, 0x09, 0xf9, 0x24, 0xe5, 0xd9, 0xbc, 0x67, 0x7f, 0x81, 0x0d, 0xf0, 0x25, 
0x58, 0xf7, 0x53, 0x13, 0xa9, 0x8a, 0xa6, 0x10, 0x47, 0x65, 0x5d, 0x73, 0x9e, 0xf1, 0x94, 0xeb, 
0x05, 0xb1, 0xa7, 0x11, 0x00, 0x00, 0x12, 0x97, 0x05, 0x53, 0xd7, 0xc8, 0x81, 0xf7, 0x78, 0xc2, 
0x2c, 0x37, 0xb6, 0xc0, 0x16, 0x3e, 0x68, 0x24, 0x3a, 0x84, 0x78, 0x1c, 0x0a, 0xdf, 0x9b, 0xb3, 
0xed, 0x21, 0xc4, 0x46, 0xe5, 0xa7, 0xa3, 0x92, 0x00, 0x3a, 0x2e, 0x39, 0x0e, 0x10, 0xd8, 0xac, 
0x47, 0xcb, 0x29, 0xc8, 0xf1, 0x2c, 0x7f, 0x11, 0x99, 0x2a, 0x18, 0xb7, 0xef, 0x73, 0x48, 0xa6, 
0xbe, 0x70, 0xa6, 0x8b, 0x97, 0x34, 0x8a, 0xb1, 0x02, 0x16, 0x7a, 0x61, 0x53, 0xdd, 0xf6, 0xe2, 
0x89, 0x15, 0xa0, 0x94, 0xf1, 0xb5, 0xdc, 0x65, 0x21, 0x15, 0x62, 0xe1, 0x7d, 0xc5, 0x43, 0x89, 
0xee, 0xb4, 0xef, 0xc8, 0xa0, 0x8e, 0x34, 0x0f, 0x04, 0x82, 0x27, 0xe1, 0xeb, 0x98, 0x64, 0xc2, 
0x8d, 0x8f, 0xdd, 0x0e, 0x82, 0x40, 0xae, 0xd4, 0x31, 0x63, 0xd6, 0x46, 0x32, 0x16, 0x85, 0x7a, 
0xb7, 0x18, 0x68, 0xb8, 0x17, 0x02, 0x81, 0xa6, 0x06, 0x20, 0x76, 0xe8, 0x54, 0x54, 0x53, 0xb4, 
0xa9, 0xd8, 0x44, 0x4b, 0xaa, 0xfb, 0x1c, 0xfd, 0xae, 0x15, 0xca, 0x29, 0x79, 0xa6, 0x24, 0xa4, 
0x0a, 0xf6, 0x1e, 0xac, 0xed, 0xfb, 0x10, 0x41, 0x08, 0x66, 0xa7, 0x67, 0x36, 0x6e, 0x62, 0x71, 
0xb7, 0xa6, 0x52, 0x94, 0x8f, 0xfb, 0x25, 0x9e, 0xe6, 0x4f, 0x25, 0xe5, 0x26, 0x9a, 0x2b, 0x6e, 
0x7e, 0xf8, 0xa6, 0x39, 0xae, 0x46, 0xaa, 0x24, 0x00, 0x03, 0xdf, 0xfc, 0xbe, 0x2f, 0x5c, 0x2e, 
0x45, 0x49, 0x7a, 0x2a, 0x91, 0xba, 0xd1, 0x3e, 0x01, 0xec, 0x5f, 0xc2, 0x15, 0x14, 0x10, 0xb3, 
0x28, 0x5e, 0x56, 0xcc, 0x26, 0x51, 0x24, 0x93, 0x0e, 0x6c, 0x99, 0x96, 0x38, 0xe0, 0x7d, 0x68, 
0x8c, 0xb7, 0x97, 0x23, 0xf4, 0xac, 0x4d, 0xbc, 0x5e, 0x01, 0x15, 0xff, 0x45, 0x60, 0x08, 0x13, 
0xcd, 0x59, 0xd7, 0x73, 0xb0, 0x0c, 0x20, 0x5e, 0xab, 0xaa, 0x24, 0x31, 0xe2, 0x2a, 0xa2, 0x53, 
0x8a, 0xf7, 0x86, 0xd5, 0x19, 0x78, 0xc5, 0x55, 0x9c, 0x08, 0xb7, 0xe2, 0xf4, 0xd0, 0x37, 0x74, 
0x93, 0x56, 0x62, 0x7b, 0x95, 0xcc, 0x2c, 0xb0, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84, 
0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 
0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x5a, 0xc6, 0x35, 0xd8, 
0xaa, 0x3a, 0x93, 0xe7, 0xb3, 0xeb, 0xbd, 0x55, 0x76, 0x98, 0x86, 0xbc, 0x65, 0x1d, 0x06, 0xb0, 
0xcc, 0x53, 0xb0, 0xf6, 0x3b, 0xce, 0x3c, 0x3e, 0x27, 0xd2, 0x60, 0x4b, 0x07, 0x78, 0x3b, 0x0d, 
0xfe, 0x4a, 0xa3, 0x19, 0x49, 0xb0, 0xce, 0xaf, 0x3f, 0x74, 0x0f, 0x32, 0x16, 0x0c, 0x8b, 0x46, 
0x94, 0x5b, 0xa5, 0xb0, 0xe4, 0x8a, 0xda, 0xd8, 0x88, 0x32, 0x90, 0x53, 0x08, 0xf7, 0xa2, 0xaa, 
0xba, 0x62, 0xb3, 0xfe, 0x29, 0x80, 0xc9, 0x5b, 0x63, 0x53, 0xc8, 0x24, 0x3c, 0x7c, 0x1f, 0x4c, 
0xda, 0xcd, 0xe5, 0x5f, 0xa2, 0x36, 0x93, 0x04, 0x3c, 0x3a, 0xbc, 0x2e, 0x02, 0x10, 0x9a, 0xf4, 
0x06, 0x32, 0x30, 0x89, 0xcb, 0x95, 0xe9, 0x55, 0x0e, 0x9d, 0xaf, 0x0e, 0x98, 0xcd, 0xca, 0xdc, 
0xb1, 0xff, 0xfc, 0xd1, 0x45, 0x66, 0xbb, 0x86, 0x46, 0x1e, 0x8c, 0x30, 0x04, 0x78, 0x53, 0xe1, 
0x3f, 0x96, 0xc5, 0xe4, 0x15, 0x23, 0x7b, 0x1f, 0x3f, 0x2c, 0xd3, 0x95, 0x40, 0xbc, 0x7a, 0x31, 
0x1f, 0x14, 0x38, 0x9e, 0x1a, 0xa5, 0xd6, 0x63, 0x10, 0x91, 0xe4, 0xd3, 0x00, 0xb4, 0x02, 0xbc, 
0x47, 0xfa, 0xa6, 0x29, 0x82, 0x0b, 0xb1, 0xd5, 0xff, 0xf2, 0xe6, 0xb0, 0xc6, 0xae, 0xe8, 0x7b, 
0x91, 0xd9, 0xee, 0x66, 0x07, 0x1f, 0xfd, 0xa2, 0xe7, 0x02, 0x66, 0xdd, 0x05, 0x2e, 0xf8, 0xc6, 
0xc1, 0x6a, 0xef, 0x3c, 0xc1, 0x95, 0xf6, 0x26, 0xce, 0x5e, 0x55, 0xd1, 0x64, 0x13, 0x28, 0xb1, 
0x18, 0x57, 0xd8, 0x1b, 0x84, 0xfa, 0xec, 0x7e, 0x5d, 0x99, 0x06, 0x49, 0x05, 0x73, 0x35, 0xa9, 
0xa7, 0xf2, 0xa1, 0x92, 0x5f, 0x3e, 0x7c, 0xdf, 0xac, 0xfe, 0x0f, 0xf5, 0x08, 0xd0, 0x3c, 0xae, 
0xcd, 0x58, 0x00, 0x5f, 0xd0, 0x84, 0x7e, 0xea, 0x63, 0x57, 0xfe, 0xc6, 0x01, 0x56, 0xda, 0xf3, 
0x72, 0x61, 0xda, 0xc6, 0x93, 0xb0, 0xac, 0xef, 0xaa, 0xd4, 0x51, 0x6d, 0xca, 0x71, 0x1e, 0x06, 
0x73, 0xea, 0x83, 0xb2, 0xb1, 0x99, 0x4a, 0x4d, 0x4a, 0x0d, 0x35, 0x07, 0x6b, 0x17, 0xd1, 0xf2, 
0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 
0x2d, 0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96, 0x4f, 0xe3, 0x42, 0xe2, 
0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33, 0x57, 
0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5, 0x93, 0xca, 0x4a, 0x6a, 
0x3e, 0xe3, 0x16, 0xef, 0x63, 0x8b, 0xfb, 0x84, 0xf7, 0xff, 0xae, 0x27, 0xbe, 0xaf, 0x88, 0xcd, 
0xdf, 0x63, 0x0a, 0xe4, 0x29, 0x96, 0x12, 0x32, 0xe7, 0xf4, 0xc7, 0x36, 0xdd, 0xf8, 0x71, 0x21, 
0xf5, 0x73, 0x9e, 0x3d, 0x9f, 0xbf, 0xa1, 0x14, 0x90, 0xa7, 0x67, 0x1b, 0x24, 0xdf, 0xde, 0x96, 
0x83, 0x05, 0x83, 0x6d, 0x6e, 0x34, 0xf4, 0xa1, 0xd0, 0xb8, 0x36, 0x5d
};

//Intel's public key to verify signatures from the DCP signing facility
const uint8 INTEL_ECDSA_PUBKEY[ECDSA_PUBLIC_KEY_SIZE] = {
    0x7b, 0xe2, 0xc7, 0xc7, 0xe6, 0xc4, 0xb4, 0x3c, 
    0x02, 0x8b, 0x67, 0xcc, 0xd0, 0x1e, 0x80, 0xc2,
    0x35, 0x2d, 0x1a, 0x4e, 0x4a, 0x81, 0x6e, 0x06, 
    0x00, 0x4c, 0xb0, 0xa4, 0x2e, 0x65, 0xbd, 0x27,
    0x52, 0x07, 0x71, 0x62, 0xcb, 0xb9, 0x3c, 0x48, 
    0x42, 0xb2, 0x2a, 0xc7, 0x84, 0xa6, 0xca, 0x12,
    0x0f, 0xc7, 0x9f, 0x6c, 0x9f, 0xbd, 0x3f, 0x38, 
    0xd5, 0xc6, 0x1b, 0x44, 0x16, 0x3e, 0xc8, 0xae
};

SigmaCryptoUtils::SigmaCryptoUtils() {
    _initialized = false;

    // Initialize PRNG context for EC-DSA key generation

    m_IntelVerifKey    = new G3Element(&m_G3);

}

SigmaCryptoUtils::~SigmaCryptoUtils() {
    // Free the PRNG context
    FreePrngCtx();

    delete m_IntelVerifKey;

}

CdgStatus SigmaCryptoUtils::Init(const uint32 *seed, uint32 seedBits) {
    // >>>> SAFEID SIK MIGRATION <<<<
    CdgStatus    CdgRes;
    unsigned int ParamsSize = sizeof(m_EpidParamsCert);

    InitPrngCtx(seed, seedBits);

       // Read the crypto context data from file
    if(sizeof(m_EpidParamsCert) != sizeof(EpidParamsCertData)) 
        return CdgStsCryptCtxInitErr;
    memcpy(&m_EpidParamsCert, EpidParamsCertData, sizeof(m_EpidParamsCert));

    Ipp8u SerialIvk[ECDSA_PUBKEY_LEN];

    memcpy(SerialIvk, INTEL_ECDSA_PUBKEY,  ECDSA_PUBKEY_LEN);

    CdgRes = EcDsaPubKeyDeserialize(*m_IntelVerifKey, SerialIvk, ECDSA_PUBKEY_LEN);
    if(CdgRes != CdgStsOk) return CdgRes;

    _initialized = true;

    return CdgStsOk;
}

CdgStatus SigmaCryptoUtils::MsgSign3p(const Ipp8u* PrivKey3p, int PrivKey3pLen, const Ipp8u* Msg, int MsgLen, Ipp8u* Signature, int SignatureLen) {
    CdgStatus    Status;
    BigNum        SigningPrivKey3p(0, 8);
    Ipp8u        SerialSigningPrivKey3p[ECDSA_PRIVKEY_LEN] = {0};

    // Check if the library is initialized
    if(!_initialized) return CdgStsNotInit;

    // Watch for null pointers
    if((PrivKey3p == NULL) || (Msg == NULL) || (Signature == NULL)) {
        return CdgStsBadPtr;
    }

    //Verify the length of the private key
    if(PrivKey3pLen < ECDSA_PRIVKEY_LEN) {
        return CdgStsBuffTooSmall;
    }

    // Create a copy of the private key to prevent swapping endianess in the original buffer
    memcpy(SerialSigningPrivKey3p, PrivKey3p, ECDSA_PRIVKEY_LEN);
    // Deserialize the private key
    Status = EcDsaPrivKeyDeserialize(SigningPrivKey3p, SerialSigningPrivKey3p, ECDSA_PRIVKEY_LEN);
    if(Status != CdgStsOk) return Status;

    // Sign the message
    return EcDsaSign(SigningPrivKey3p, Msg, MsgLen, Signature, SignatureLen);
}

CdgStatus SigmaCryptoUtils::VerifyCertPchSigIntel(Ipp8u* PubCertPch, int PubCertPchLen, CdgResult* VerifRes) {
    // Check if the library is initialized
    if(!_initialized) return CdgStsNotInit;

    // Watch for null pointers
    if((PubCertPch == NULL) || (VerifRes == NULL)) {
        return CdgStsBadPtr;
    }

    // Verify the length of the certificate buffer
    if(PubCertPchLen < sizeof(EpidCert)) {
        return CdgStsBuffTooSmall;
    }

    // Pointer to the certificate
    EpidCert* CertificatePch = (EpidCert*)PubCertPch;

    return EcDsaVerify(*m_IntelVerifKey,
                       PubCertPch,
                       (PubCertPchLen - ECDSA_SIGNATURE_LEN),
                       (Ipp8u*)(&(CertificatePch->SignIntel)),
                       ECDSA_SIGNATURE_LEN,
                       VerifRes);
}

CdgStatus SigmaCryptoUtils::DeriveSkMk(Ipp8u* Ga, int GaLen, Ipp8u* Gb, int GbLen, Ipp8u* Sk, int SkLen, Ipp8u* Mk, int MkLen) {
    IppStatus    Status;
    bool        Result;
    Ipp8u        EcpfXBytes[32];
    int            EcpfXLen = 32;
    Ipp8u        EcpfYBytes[32];
    int            EcpfYLen = 32;

    // Watch for null pointers
    if((Ga == NULL) || (Gb == NULL) || (Sk == NULL) || (Mk == NULL)) {
        return CdgStsBadPtr;
    }

    // Verify the length of the Ga, Gb, Sk and Mk buffers
    if(GaLen < 64 || GbLen < 64 || SkLen < 16 || MkLen < 16) {
        return CdgStsBuffTooSmall;
    }

    // g is the generator of m_G3
    G3Element g(*m_G3.g);

    // Deserialize g^a swapping the coordinate endianess
    Ipp8u SerialGa[64];
    memcpy(SerialGa, Ga, 64);
    SwapEndian_32B((Ipp32u*)(SerialGa     ));
    SwapEndian_32B((Ipp32u*)(SerialGa + 32));

    BigNum g_aX((Ipp32u*)(SerialGa     ), 8);
    BigNum g_aY((Ipp32u*)(SerialGa + 32), 8);
    G3Element g_a(&m_G3, g_aX, g_aY);

    // Compute g^b
    G3OrderElement b(m_G3.Order);
    G3Element g_b(&m_G3);

    m_G3.Order->GetRandomElement(b, m_PrngCtx);            // Generate random b
    m_G3.Exp(g, b, g_b);                                // Compute g^b

    // Serialize g^b swapping endianess of each coordinate
    BigNum g_bX(0, 8), g_bY(0, 8);

    Result = g_b.GetCoords(g_bX, g_bY);
    if(!Result) return CdgStsSerErr;

    Result = g_bX.GetValue(EcpfXBytes, EcpfXLen);
    if(!Result) return CdgStsSerErr;
    SwapEndian_32B(EcpfXBytes);

    Result = g_bY.GetValue(EcpfYBytes, EcpfYLen);
    if(!Result) return CdgStsSerErr;
    SwapEndian_32B(EcpfYBytes);

    memcpy((Gb           ), EcpfXBytes, EcpfXLen);
    memcpy((Gb + EcpfXLen), EcpfYBytes, EcpfYLen);

    // Compute shared key g^ab
    G3Element g_ab(&m_G3);
    m_G3.Exp(g_a, b, g_ab);                            // g^ab = (g^a)^b

    // Serialize g^ab (no endianess swapping - this is not returned but used for futher computing)
    Ipp8u Gab[64];
    BigNum g_abX(0, 8), g_abY(0, 8);

    Result = g_ab.GetCoords(g_abX, g_abY);
    if(!Result) return CdgStsSerErr;

    Result = g_abX.GetValue(EcpfXBytes, EcpfXLen);
    if(!Result) return CdgStsSerErr;

    Result = g_abY.GetValue(EcpfYBytes, EcpfYLen);
    if(!Result) return CdgStsSerErr;

    memcpy((Gab           ), EcpfXBytes, EcpfXLen);
    memcpy((Gab + EcpfXLen), EcpfYBytes, EcpfYLen);

    // Compute SHA256(g^ab)
    Ipp8u GabSha256[32];
    Status = ippsSHA256MessageDigest(Gab, 64, GabSha256);
    if(Status != ippStsNoErr) {
        return CdgStsHashErr;
    }

    // Derive SK and MK from SHA256(g^ab)
    memcpy(Sk, (GabSha256     ), 16);            // SK: bits   0-127
    memcpy(Mk, (GabSha256 + 16), 16);            // MK: bits 128-255

    return CdgStsOk;
}

// Private methods ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
CdgStatus SigmaCryptoUtils::EcDsaSign(const BigNum& EcDsaPrivKey, const Ipp8u* Msg, int MsgLen, Ipp8u* Signature, int SignatureLen) {
    IppStatus Status;
    // Message digest
    Ipp8u MsgDigest[32];
    //Signature
    BigNum SignX(0, 8);
    BigNum SignY(0, 8);
    // Ephemeral key pair
    BigNum        ephPriv(0, 8);
    G3Element    ephPub(&m_G3);
    // Signature serialization variables
    bool    Result;
    Ipp8u    SigBytes[32];
    int        SigLen = 32;

    // Verify signature length
    if(SignatureLen < ECDSA_SIGNATURE_LEN) {
        return CdgStsBuffTooSmall;
    }

    // Compute digest of the message
    Status = ippsSHA256MessageDigest(Msg, MsgLen, MsgDigest);
    if(Status != ippStsNoErr) {
        return CdgStsHashErr;
    }
    SwapEndian_32B(MsgDigest);
    BigNum BnMsgDigest((Ipp32u*)MsgDigest, 8);

    // Generate and set ephemeral key pair
    Status = ippsECCPGenKeyPair(BN(ephPriv), (IppsECCPPointState*)ephPub, ECC(ephPub), (IppBitSupplier)ippsPRNGen, m_PrngCtx);
    if(Status != ippStsNoErr) {
        return CdgStsKeyPairGenErr;
    }
    Status = ippsECCPSetKeyPair(BN(ephPriv), (IppsECCPPointState*)ephPub, ippFalse, ECC(ephPub));
    if(Status != ippStsNoErr) {
        return CdgStsKeyPairSetErr;
    }
    // Sign the digest
    Status = ippsECCPSignDSA(BN(BnMsgDigest), BN(EcDsaPrivKey), BN(SignX), BN(SignY), (IppsECCPState*)m_G3);
    if(Status != ippStsNoErr) {
        return CdgStsSignErr;
    }

    // Serialize the signature swapping the endianess of each coordinate
    Result = SignX.GetValue(SigBytes, SigLen);
    if(!Result) return CdgStsSerErr;
    SwapEndian_32B(SigBytes);
    memcpy(Signature, SigBytes, SigLen);
    
    Result = SignY.GetValue(SigBytes, SigLen);
    if(!Result) return CdgStsSerErr;
    SwapEndian_32B(SigBytes);
    memcpy((Signature + 32), SigBytes, SigLen);

    return CdgStsOk;
}

CdgStatus SigmaCryptoUtils::EcDsaVerify(G3Element& EcDsaPubKey, Ipp8u* Msg, int MsgLen, Ipp8u* Signature, int SignatureLen, CdgResult* VerifRes) {
    IppStatus Status;
    IppECResult Res;
    // Temporary buffer for swapping the signature coordinates endianess
    Ipp8u SerialSig[ECDSA_SIGNATURE_LEN];
    // Message digest
    Ipp8u MsgDigest[32];

    // Verify signature length
    if(SignatureLen < ECDSA_SIGNATURE_LEN) {
        return CdgStsBuffTooSmall;
    }

    // Copy the signature to the temporary buffer and swap the endianess of its coordinates
    memcpy(SerialSig, Signature, ECDSA_SIGNATURE_LEN);
    SwapEndian_32B((SerialSig     ));
    SwapEndian_32B((SerialSig + 32));

    // Compute digest of the message
    Status = ippsSHA256MessageDigest(Msg, MsgLen, MsgDigest);
    if(Status != ippStsNoErr) {
        return CdgStsHashErr;
    }
    SwapEndian_32B(MsgDigest); 
    BigNum BnCertDigest((Ipp32u*)MsgDigest, 8);

    // Deserialize the signature
    BigNum SignXDes((Ipp32u*)SerialSig,            8);
    BigNum SignYDes((Ipp32u*)(SerialSig + 32),    8);

    // Set the public key in the Ecc context
    Status = ippsECCPSetKeyPair(0, (IppsECCPPointState*)EcDsaPubKey, ippTrue, ECC(EcDsaPubKey));
    if(Status != ippStsNoErr) {
        return CdgStsKeyPairSetErr;
    }

    // Verify the signature
    Status = ippsECCPVerifyDSA(BN(BnCertDigest), BN(SignXDes), BN(SignYDes), &Res, ECC(EcDsaPubKey));
    if(Status != ippStsNoErr) {
        return CdgStsVerifErr;
    }

    if(Res == ippECValid) {
        *VerifRes = CdgValid;
    }
    else {
        *VerifRes = CdgInvalid;
    }

    return CdgStsOk;
}

CdgStatus SigmaCryptoUtils::EcDsaPrivKeyDeserialize(BigNum& EcDsaPrivKey, Ipp8u* SerialPrivKey, int PrivKeyLen) {
    // Verify private key length
    if(PrivKeyLen < ECDSA_PRIVKEY_LEN) {
        return CdgStsBuffTooSmall;
    }

    // Deserialize the key swapping endianess
    SwapEndian_32B(SerialPrivKey);
    BigNum DeserPrivKey((Ipp32u*)SerialPrivKey, (ECDSA_PRIVKEY_LEN / 4));
    EcDsaPrivKey = DeserPrivKey;

    return CdgStsOk;
}

CdgStatus SigmaCryptoUtils::EcDsaPubKeySerialize(G3Element& EcDsaPubKey, Ipp8u* SerialPubKey, int PubKeyLen) {
    bool    Result;
    Ipp8u    PkXBytes[32];
    int        PkXLen = 32;
    Ipp8u    PkYBytes[32];
    int        PkYLen = 32;
    BigNum PkX(0, 8), PkY(0, 8);

    // Verify the lenght of the key buffer
    if(PubKeyLen < ECDSA_PUBKEY_LEN) {
        return CdgStsBuffTooSmall;
    }

    // Retrieve the coordinates
    Result = EcDsaPubKey.GetCoords(PkX, PkY);
    if(!Result) return CdgStsSerErr;

    // Serialize each coordinate swapping endianess
    Result = PkX.GetValue(PkXBytes, PkXLen);
    if(!Result) return CdgStsSerErr;
    SwapEndian_32B(PkXBytes);

    Result = PkY.GetValue(PkYBytes, PkYLen);
    if(!Result) return CdgStsSerErr;
    SwapEndian_32B(PkYBytes);

    memcpy(SerialPubKey, PkXBytes, PkXLen);
    memcpy((SerialPubKey + PkXLen), PkYBytes, PkYLen);

    return CdgStsOk;
}

CdgStatus SigmaCryptoUtils::EcDsaPubKeyDeserialize(G3Element& EcDsaPubKey, Ipp8u* SerialPubKey, int PubKeyLen) {
    // Verify public key length
    if(PubKeyLen < ECDSA_PUBKEY_LEN) {
        return CdgStsBuffTooSmall;
    }

    // Deserialize the key coordinates swapping endianess for each of them
    SwapEndian_32B(SerialPubKey);
    BigNum PkX((Ipp32u*)(SerialPubKey),                                ((ECDSA_PUBKEY_LEN / 2) / 4));
    SwapEndian_32B((SerialPubKey + (ECDSA_PUBKEY_LEN / 2)));
    BigNum PkY((Ipp32u*)(SerialPubKey + (ECDSA_PUBKEY_LEN / 2)),    ((ECDSA_PUBKEY_LEN / 2) / 4));
    G3Element DeserPubKey(&m_G3, PkX, PkY);

    EcDsaPubKey = DeserPubKey;

    return CdgStsOk;
}

void SigmaCryptoUtils::InitPrngCtx(const uint32 *seed, uint32 seedBits) {
    // Get context buffer size
    ippsPRNGGetSize(&m_PrngCtxSize);

    // Allocate the context buffer
    m_PrngCtx = (IppsPRNGState*)(new Ipp8u[m_PrngCtxSize]);

    // Initialize the context
    ippsPRNGInit(256, m_PrngCtx);
    BigNum bnSeed( seed, seedBits );
    ippsPRNGSetSeed(BN(bnSeed), m_PrngCtx);
}

void SigmaCryptoUtils::FreePrngCtx() {
    SecureZeroMemory(m_PrngCtx, m_PrngCtxSize);
    if(m_PrngCtx) {
        // Deallocate the context buffer
        delete[] (Ipp8u*)m_PrngCtx;
        m_PrngCtx = NULL;
    }
}