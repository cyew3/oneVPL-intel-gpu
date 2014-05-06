/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2002-2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Message block processing according to SHA1
//
// Contents:
//    SHA1 constants
//
//
//    Created: Mon 21-Oct-2002 18:33
//  Author(s): Sergey Kirillov
*/
#include "precomp.h"
#include "owncp.h"
#include "pcpshs.h"

/*
// Constants below are defined in
// Secure Hash Signature Standard (SHS) (FIPS PUB 180-2).
// ( http://csrc.nist.gov/publications/fips/fips180-2/fips180-2withchangenotice.pdf )
*/
#if (defined(_PX) && defined(_MERGED_BLD)) || (!defined(_MERGED_BLD) && !defined(_IPP_TRS))
/*const*/
Ipp32u K_SHA1[] = FIPS_180_K_SHA1;

#endif /* _MERGED_BLD */
