/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_singleton.h"
#include "ippcp.h"
#include "ipps.h"
#include "mfx_shared_ptr.h"

class IRandom : public EnableSharedFromThis<IRandom>
{
public:
    virtual void srand(mfxU32 seed) = 0;
    virtual mfxU32  rand()          = 0;
    virtual mfxU32  rand_max()      = 0;
};

class DefaultRandom 
    : public MFXNTSSingleton<DefaultRandom>
    , public IRandom
{
public:
    virtual void srand(mfxU32 seed)
    {
        ::srand(seed);
    }
    virtual mfxU32 rand()
    {
        return ::rand();
    }
    virtual mfxU32 rand_max()
    {
        return RAND_MAX;
    }
};

class IPPBasedRandom 
    : public IRandom
{
    IppsPRNGState *m_pRNGCtx;
    IppsBigNumState* m_pBN;
    //size in bits for seed value
    static const int BitSize = sizeof(mfxU32) * 8;
public:
    IPPBasedRandom() 
        : m_pRNGCtx()
        , m_pBN()
    {
        int ctxSize = 0;
        MFX_CHECK_AND_THROW(ippStsNoErr == ippsPRNGGetSize(&ctxSize));
        MFX_CHECK_AND_THROW(NULL != (m_pRNGCtx = (IppsPRNGState *)ippsMalloc_8u(ctxSize)));
        MFX_CHECK_AND_THROW(ippStsNoErr == ippsPRNGInit(BitSize, m_pRNGCtx));

        MFX_CHECK_AND_THROW(ippStsNoErr == ippsBigNumGetSize(BitSize, &ctxSize));
        // allocate the Big Number context
        MFX_CHECK_AND_THROW(NULL != (m_pBN = (IppsBigNumState*) ippsMalloc_8u(ctxSize)));
        MFX_CHECK_AND_THROW(ippStsNoErr == ippsBigNumInit(BitSize, m_pBN));
    }
    virtual ~IPPBasedRandom()
    {
        ippsFree (m_pRNGCtx);
        ippsFree (m_pBN);
    }
    virtual void srand(mfxU32 seed)
    {
        MFX_CHECK_AND_THROW(ippStsNoErr == ippsSet_BN(IppsBigNumPOS, BitSize, (Ipp32u*)&seed, m_pBN));
        MFX_CHECK_AND_THROW(ippStsNoErr == ippsPRNGSetSeed(m_pBN, m_pRNGCtx));
    }
    virtual mfxU32 rand()
    {
        mfxU32 rand_value = 0;
        MFX_CHECK_AND_THROW(ippStsNoErr == ippsPRNGen((Ipp32u*)&rand_value, BitSize, m_pRNGCtx));
        return rand_value ;
    }
    virtual mfxU32 rand_max()
    {
        return (std::numeric_limits<mfxU32>::max)();
    }
};