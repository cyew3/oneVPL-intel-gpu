/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef _SIGMA_CYPTO_UTILS_H
#define _SIGMA_CYPTO_UTILS_H

#include "pavpsdk_defs.h"
#include "ippcp.h"

#include "bignumber.h"
#include "ecpf.h"
#include "epid_types.h"
#include "cdg_status_codes.h"


class SigmaCryptoUtils {
public:
    SigmaCryptoUtils();
    ~SigmaCryptoUtils();

    // Initialize the generator
    CdgStatus Init(const uint32 *seed, uint32 seedBits);

    CdgStatus MsgSign3p(const Ipp8u* PrivKey3p, int PrivKey3pLen, const Ipp8u* Msg, int MsgLen, Ipp8u* Signature, int SignatureLen);
    CdgStatus VerifyCertPchSigIntel(Ipp8u* PubCertPch, int PubCertPchLen, CdgResult* VerifRes);
    CdgStatus DeriveSkMk(Ipp8u* Ga, int GaLen, Ipp8u* Gb, int GbLen, Ipp8u* Sk, int SkLen, Ipp8u* Mk, int MkLen);
private:
    bool                    _initialized;

    EPIDParameterCertificateBlob    m_EpidParamsCert;
    IppsPRNGState*            m_PrngCtx;
    int                       m_PrngCtxSize;
    // G3 elliptic curve used for SIGMA and EC-DSA operations
    G3ECPrimeField            m_G3;
    G3Element*                m_IntelVerifKey;

    // Initialize/delete PRNG context for EC-DSA key generation
    void InitPrngCtx(const uint32 *seed, uint32 seedBits);
    void FreePrngCtx();

    // EC-DSA keys serialization/deserialization
    CdgStatus EcDsaPrivKeySerialize(BigNum& EcDsaPrivKey, Ipp8u* SerialPrivKey, int PrivKeyLen);
    CdgStatus EcDsaPrivKeyDeserialize(BigNum& EcDsaPrivKey, Ipp8u* SerialPrivKey, int PrivKeyLen);
    CdgStatus EcDsaPubKeySerialize(G3Element& EcDsaPubKey, Ipp8u* SerialPubKey, int PubKeyLen);
    CdgStatus EcDsaPubKeyDeserialize(G3Element& EcDsaPubKey, Ipp8u* SerialPubKey, int PubKeyLen);

    // EC-DSA sign/verify
    CdgStatus EcDsaSign(const BigNum& EcDsaPrivKey, const Ipp8u* Msg, int MsgLen, Ipp8u* Signature, int SignatureLen);
    CdgStatus EcDsaVerify(G3Element& EcDsaPubKey, Ipp8u* Msg, int MsgLen, Ipp8u* Signature, int SignatureLen, CdgResult* VerifRes);

private:
    PAVPSDK_DISALLOW_COPY_AND_ASSIGN(SigmaCryptoUtils);
};

#endif