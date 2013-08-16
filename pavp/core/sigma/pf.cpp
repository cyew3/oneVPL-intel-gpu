/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#include "pavpsdk_defs.h"

#include "pf.h"

#define G3_ORDER_BIT_SIZE    256

/*
 *    Prime field defining oredr of the G3 curve
 */

G3OrderPrimeField::G3OrderPrimeField(BigNum &Modulus) {
    // Create modulus
    _Modulus = new BigNum(0, 8);

    // Copy input data
    *_Modulus = Modulus;
}

G3OrderPrimeField::~G3OrderPrimeField() {
    // Delete the modulus
    PAVPSDK_SAFE_DELETE(_Modulus);
}

bool G3OrderPrimeField::GetRandomElement(G3OrderElement &RandElem, IppsPRNGState* PrngCtx) const {
    IppStatus Status;

    // Generate a random big number of a size G3_ORDER_BIT_SIZE + 80 bits
    int BnDwSize = ((G3_ORDER_BIT_SIZE + 80) / 8) / sizeof(Ipp32u) + 1;
    BigNum RandBn(0, BnDwSize);

    Status = ippsPRNGen_BN(BN(RandBn), (G3_ORDER_BIT_SIZE + 80), PrngCtx);
    assert(Status == ippStsNoErr);

    // Convert to the G3 order element
    G3OrderElement Temp((G3OrderPrimeField*)this, RandBn);
    RandElem = Temp;

    return true;
}


/*
 *    Element of the G3 order prime field
 */

G3OrderElement::G3OrderElement(G3OrderPrimeField* G3OrderField) {
    // Initialize the prime field pointer
    G3Order = G3OrderField;

    // Initialize the data
    // Set the element value to 0 by default
    Ipp32u ZeroData = 0;
    Data = new BigNum(&ZeroData, (G3_ORDER_BIT_SIZE / 8) / sizeof(Ipp32u));
}

G3OrderElement::G3OrderElement(G3OrderPrimeField* G3OrderField, BigNum& Value) {
    IppStatus Status;

    // Initialize the prime field pointer
    G3Order = G3OrderField;

    // Initialize the data
    Data = new BigNum(0, (G3_ORDER_BIT_SIZE / 8) / sizeof(Ipp32u));

    // Perform modulo reduction on the input value (so it is in the range of [0, Modulus - 1])
    Status = ippsMod_BN(BN(Value), BN(*(G3Order->_Modulus)), BN(*Data));
    assert(Status == ippStsNoErr);
}

G3OrderElement::~G3OrderElement() {
    if(Data) {
        // Delete the data
        delete Data;
    }
}

G3OrderElement& G3OrderElement::operator = (const G3OrderElement& Elem) {
    if(this != &Elem) {
        assert(G3Order == Elem.G3Order);

        // Copy the data
        *Data = *Elem.Data;
    }

    return *this;
}

void G3OrderElement::GetBigNum(BigNum& Bn) {
    Bn = *Data;
}