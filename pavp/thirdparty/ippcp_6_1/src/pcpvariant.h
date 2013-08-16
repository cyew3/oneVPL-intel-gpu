/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2005-2008 Intel Corporation. All Rights Reserved.
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
// Define _IPP_TRS if "special" build is requested
//
// "Special" means:
//    - no global/static variable
//    - no inderect call
// at least for defined list of functions
*/
#if defined(_IPP_TRS)
   #if !defined(_PX)
   #error _IPP_TRS definition and CPU specisic are incompatible! PX only is acceptable
   #endif
#endif

/*
// do not change definitions below!
*/

#if (_IPP64==_IPP64_I7)
   #define _xUSE_NN_VERSION_           /* not use NN  version of RSA, DSA, DH, PRNG, MontExp */
   #define _xUSE_NN_MONTMUL_           /* not use NN  version of Montgomery multiplication */
   #define _xUSE_XSC_MONTMUL_          /* not use XSC version of Montgomery multiplication */
   #define _xUSE_EMT_MONTMUL_          /* not use EMT version of Montgomery multiplication */
   #define _xUSE_SQR_                  /* not use special implementaton of sqr */
   #define _xUSE_KARATSUBA_            /* not use Karatsuba method for multiplication */
   #define _xUSE_ERNIE_CBA_MITIGATION_ /* not use (Ernie) mitigation of CB attack */
   #define _USE_GRES_CBA_MITIGATION_   /*     use (Gres)  mitigation of CB attack */
   #define _USE_WINDOW_EXP_            /*     use (Gres)  window exponentiation */
#else
   // ~ 5.0 and above
   #define _xUSE_NN_VERSION_           /* not use NN  version of RSA, DSA, DH, PRNG, MontExp */
   #define _xUSE_NN_MONTMUL_           /* not use NN  version of Montgomery multiplication */
   #define _xUSE_XSC_MONTMUL_          /* not use XSC version of Montgomery multiplication */
   #define _xUSE_EMT_MONTMUL_          /* not use EMT version of Montgomery multiplication */
   #define _xUSE_SQR_                  /* not use special implementaton of sqr */
   #define _USE_KARATSUBA_             /*     use Karatsuba method for multiplication */
   #define _xUSE_ERNIE_CBA_MITIGATION_ /* not use (Ernie) mitigation of CB attack */
   #define _USE_GRES_CBA_MITIGATION_   /*     use (Gres)  mitigation of CB attack */
   #define _USE_WINDOW_EXP_            /*     use (Gres)  window exponentiation */

   // ~ v4.1
 //#define _xUSE_NN_VERSION_           /* not use NN  version of RSA, DSA, DH, PRNG, MontExp */
 //#define _USE_NN_MONTMUL_            /* not use NN  version of Montgomery multiplication */
 //#define _USE_XSC_MONTMUL_           /* not use XSC version of Montgomery multiplication */
 //#define _USE_EMT_MONTMUL_           /* not use EMT version of Montgomery multiplication */
 //#define _xUSE_SQR_                  /* not use special implementaton of sqr */
 //#define _xUSE_KARATSUBA_            /*     use Karatsuba method for multiplication */
 //#define _xUSE_ERNIE_CBA_MITIGATION_ /*     use (Ernie) mitigation of CB attack */
 //#define _xUSE_GRES_CBA_MITIGATION_  /* not use (Gres)  mitigation of CB attack */
 //#define _xUSE_WINDOW_EXP_           /* not use (Gres)  window exponentiation */
#endif


#if defined ( _OPENMP )

#define DEFAULT_CPU_NUM    (8)

#define     BF_MIN_BLK_PER_THREAD (32)
#define     TF_MIN_BLK_PER_THREAD (16)

#define    DES_MIN_BLK_PER_THREAD (32)
#define   TDES_MIN_BLK_PER_THREAD (16)

#define  RC5_64_MIN_BLK_PER_THREAD (16)
#define RC5_128_MIN_BLK_PER_THREAD (32)

#define RIJ128_MIN_BLK_PER_THREAD (32)
#define RIJ192_MIN_BLK_PER_THREAD (16)
#define RIJ256_MIN_BLK_PER_THREAD (16)

#endif

#endif /* _CP_VARIANT_H */
