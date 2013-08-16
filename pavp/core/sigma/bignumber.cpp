/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#include "pavpsdk_defs.h"

#include <cstring>
#include "bignumber.h"

BigNum::BigNum(const Ipp32u* Data, int DwSize, IppsBigNumSGN Sgn) {
    IppStatus Status;
    int BnCtxSize;

    // Get the context size
    Status = ippsBigNumGetSize(DwSize, &BnCtxSize);
    assert(Status == ippStsNoErr);

    // Allocate the context buffer
    BnCtx = (IppsBigNumState*)(new Ipp8u[BnCtxSize]);

    // Initialize the context
    Status = ippsBigNumInit(DwSize, BnCtx);
    assert(Status == ippStsNoErr);

    // Set the BN value if any initialization data was supplied
    if(Data) {
        Status = ippsSet_BN(Sgn, DwSize, Data, BnCtx);
        assert(Status == ippStsNoErr);
    }
    else {
        // Set only the sign
        ((Ipp32u*)BnCtx)[1] = Sgn;
    }
}

BigNum::~BigNum() {
    // Deallocate the context buffer
    PAVPSDK_SAFE_DELETE_ARRAY(BnCtx);

}
BigNum::BigNum(const BigNum& Bn) {
    *this = Bn;
}

BigNum& BigNum::operator = (const BigNum& Bn) {
    IppStatus Status;

    if(this != &Bn) {
        int DataDwSize;
        IppsBigNumSGN Sgn;
        Ipp32u* Data;

        // Get the data buffer size
        Status = ippsGetSize_BN(Bn.BnCtx, &DataDwSize);
        assert(Status == ippStsNoErr);

        // Allocate the data buffer
        Data = new Ipp32u[DataDwSize];

        // Get actual sgn, data length and data from the copied Bn
        Status = ippsGet_BN(&Sgn, &DataDwSize, Data, Bn.BnCtx);
        assert(Status == ippStsNoErr);

        // Set set the parameters copied
        Status = ippsSet_BN(Sgn, DataDwSize, Data, BnCtx);
        assert(Status == ippStsNoErr);

        // Deallocate the data buffer
        delete[] Data;
    }

    return *this;
}

bool BigNum::GetValue(Ipp8u* Value, int Size) {
    // Watch for null pointers
    if(!Value) {
        return false;
    }

    IppStatus Status;
    int DataDwSize;
    IppsBigNumSGN Sgn;
    Ipp32u* Data;
    
    // Get the size of data
    Status = ippsGetSize_BN(BnCtx, &DataDwSize);
    assert(Status == ippStsNoErr);
    
    // Allocate the buffer for the data
    Data = new Ipp32u[DataDwSize];

    // Get the value
    Status = ippsGet_BN(&Sgn, &DataDwSize, Data, BnCtx);
    assert(Status == ippStsNoErr);

    // Verify the specified output buffer is big enough
    if(Size < (int)(DataDwSize * sizeof(Ipp32u))) {
        delete[] Data;
        return false;
    }

    // Copy the data
    memcpy(Value, (Ipp8u*)Data, (DataDwSize * sizeof(Ipp32u)));

    // Deallocate the data buffer
    delete[] Data;

    return true;
}