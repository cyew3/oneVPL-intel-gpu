/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2005-2006 Intel Corporation. All Rights Reserved.
//
//              Intel(R) Integrated Performance Primitives
//                  Cryptographic Primitives (ippcp)
//
//  Purpose:
//    Define ippCP variant
//
//  Created: 30-Mar-2005 10:59
//
//  Author(s): Sergey Kirillov
*/
#if !defined(_CP_VARIANT_H)
#define _CP_VARIANT_H

/*
// do not change definitions below!
*/

#define _xUSE_NN_VERSION_          /* use NN  version of RSA, DSA, DH, PRNG, MontExp */
#define _USE_NN_MONTMUL_           /* use NN  version of Montgomery multiplication */
#define _USE_XSC_MONTMUL_          /* use XSC version of Montgomery multiplication */
#define _USE_EMT_MONTMUL_          /* use EMT version of Montgomery multiplication */
#define _USE_ERNIE_CBA_MITIGATION_ /* use mitigation of CB attack (ftom Ernie) */

#endif /* _CP_VARIANT_H */
