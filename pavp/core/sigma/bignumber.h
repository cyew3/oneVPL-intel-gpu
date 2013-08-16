/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef _BIGNUMBER_H
#define _BIGNUMBER_H

#include <cassert>

#include "ippcp.h"

class BigNum {
public:
    BigNum(const Ipp32u* Data = 0, int DwSize = 1, IppsBigNumSGN Sgn = IppsBigNumPOS);
    BigNum(const BigNum& Bn);
    ~BigNum();

    // Conversion to IppsBigNumState
    friend IppsBigNumState* BN(const BigNum& Bn) { return Bn.BnCtx; }
    operator IppsBigNumState* () const { return BnCtx; }

    // Arithmetic operators
    BigNum& operator = (const BigNum& Bn);

    // Retrieve the value
    bool GetValue(Ipp8u* Value, int Size);

protected:
    // Big number context pointer
    IppsBigNumState* BnCtx;
};

#endif