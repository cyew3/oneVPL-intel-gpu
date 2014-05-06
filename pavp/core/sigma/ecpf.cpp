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
#include "ecpf.h"

#define G3_ELEMENT_BIT_SIZE    256

/*
 *    Elliptic curve G3
 */

G3ECPrimeField::G3ECPrimeField() {
    IppStatus Status;
    int EcpfCtxSize;

    /*
     *    Create the G3 EC prime field
     */
    // Get the curve context size
    Status = ippsECCPGetSize(G3_ELEMENT_BIT_SIZE, &EcpfCtxSize);
    assert(Status == ippStsNoErr);

    // Allocate the context buffer
    EcpfCtx = (IppsECCPState*)(new Ipp8u[EcpfCtxSize]);

    // Initialize the context
    Status = ippsECCPInit(G3_ELEMENT_BIT_SIZE, EcpfCtx);
    assert(Status == ippStsNoErr);

    // Set up the EC as a standard NIST P-256 curve
    Status = ippsECCPSetStd(IppECCPStd256r1, EcpfCtx);
    assert(Status == ippStsNoErr);

    // Retrieve parameters of the curve
    BigNum p(0, 8);
    BigNum A(0, 8);
    BigNum B(0, 8);
    BigNum gX(0, 8);
    BigNum gY(0, 8);
    BigNum OrderMod(0, 8);
    int Cofactor;

    Status = ippsECCPGet(BN(p), BN(A), BN(B), BN(gX), BN(gY), BN(OrderMod), &Cofactor, EcpfCtx);
    assert(Status == ippStsNoErr);

    // Create the generator element
    g = new G3Element(this, gX, gY);
    // Create the order prime field
    Order = new G3OrderPrimeField(OrderMod);
}

G3ECPrimeField::~G3ECPrimeField() {
    // Destroy the group generator
    PAVPSDK_SAFE_DELETE(g);

    // Destroy the order prime field
    PAVPSDK_SAFE_DELETE(Order);

    // Deallocate the context buffer
    PAVPSDK_SAFE_DELETE_ARRAY(EcpfCtx);
}

bool G3ECPrimeField::Exp(const G3Element& G, const BigNum& a, const G3Element& Ga) const {
    IppStatus Status;

    // Perform scalar multiplication Ga = a*G
    Status = ippsECCPMulPointScalar((IppsECCPPointState*)G, BN(a), (IppsECCPPointState*)Ga, EcpfCtx);
    if(Status != ippStsNoErr) {
        return false;
    }

    return true;
}

bool G3ECPrimeField::Exp(const G3Element& G, const G3OrderElement& a, const G3Element& Ga) const {
    return Exp(G, *a.Data, Ga);
}


/*
 *    Point on the elliptic curve G3
 */

G3Element::G3Element(G3Element& Elem) {
    IppStatus Status;

    InitPoint(Elem.G3);

    // Copy the coordinates
    BigNum X(0, 8);
    BigNum Y(0, 8);

    // Copy X and Y from the source point
    Status = ippsECCPGetPoint(BN(X), BN(Y), Elem.EcpfPointCtx, (IppsECCPState*)(*G3));
    assert(Status == ippStsNoErr);
    Status = ippsECCPSetPoint(BN(X), BN(Y), EcpfPointCtx, (IppsECCPState*)(*G3));
    assert(Status == ippStsNoErr);
}

G3Element::G3Element(G3ECPrimeField* G3Field) {
    IppStatus Status;

    InitPoint(G3Field);

    // Set the point at infinity by default
    Status = ippsECCPSetPointAtInfinity(EcpfPointCtx, (IppsECCPState*)(*G3));
    assert(Status == ippStsNoErr);
}

G3Element::G3Element(G3ECPrimeField* G3Field, BigNum& X, BigNum& Y) {
    IppStatus Status;

    InitPoint(G3Field);

    // Set the point coordinates
    Status = ippsECCPSetPoint(BN(X), BN(Y), EcpfPointCtx, (IppsECCPState*)(*G3));
    assert(Status == ippStsNoErr);
}

G3Element::~G3Element() {
    // Deallocate the context buffer
    delete[] (Ipp8u*)EcpfPointCtx;
}

G3Element& G3Element::operator = (const G3Element& Elem) {
    IppStatus Status;

    if(this != &Elem) {
        assert(G3 == Elem.G3);
        
        BigNum X(0, 8);
        BigNum Y(0, 8);

        // Copy X and Y from the source point
        Status = ippsECCPGetPoint(BN(X), BN(Y), Elem.EcpfPointCtx, (IppsECCPState*)(*G3));
        assert(Status == ippStsNoErr);
        Status = ippsECCPSetPoint(BN(X), BN(Y), EcpfPointCtx, (IppsECCPState*)(*G3));
        assert(Status == ippStsNoErr);
    }

    return *this;
}

void G3Element::InitPoint(G3ECPrimeField* G3Field) {
    IppStatus Status;
    int EcpfPointCtxSize;

    // Set the pointer to the G3 field
    G3 = G3Field;

    /*
     *    Create a point in G3
     */
    // Get the point context size
    Status = ippsECCPPointGetSize(G3_ELEMENT_BIT_SIZE, &EcpfPointCtxSize);
    assert(Status == ippStsNoErr);

    // Allocate the context buffer
    EcpfPointCtx = (IppsECCPPointState*)(new Ipp8u[EcpfPointCtxSize]);

    // Initialize the context
    Status = ippsECCPPointInit(G3_ELEMENT_BIT_SIZE, EcpfPointCtx);
    assert(Status == ippStsNoErr);
}

bool G3Element::GetCoords(BigNum& X, BigNum& Y) {
    IppStatus Status;

    // Retrieve the point coordinates
    Status = ippsECCPGetPoint(BN(X), BN(Y), EcpfPointCtx, (IppsECCPState*)(*G3));
    if(Status != ippStsNoErr) {
        return false;
    }
    
    return true;
}