/* ////////////////////////// "dependencesp.h" /////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2005-2007 Intel Corporation. All Rights Reserved.
//
//
//  Purpose:    for XScale platforms building
//
//  Author(s):
//
//  Created:    6-Jul-2005 10:55
//  Changed:   30-Mar-2007  - separated functions by domains
//
*/

#ifndef __DEPENDENCESP_H__
#define __DEPENDENCESP_H__

#if (_IPP_ARCH == _IPP_ARCH_XSC)

#ifndef _CORE

#ifndef __IPPS_H__
  #include "ipps.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsZero
//  Purpose:    set elements of the vector to zero of corresponding type
//  Parameters:
//    pDst       pointer to the destination vector
//    len        length of the vectors
//  Returns:
//    ippStsNullPtrErr        pointer to the vector is NULL
//    ippStsSizeErr           length of the vectors is less or equal zero
//    ippStsNoErr             otherwise
*/
IPPAPI ( IppStatus, ippsZero_32f,( Ipp32f* pDst, int len ))

/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsSet
//  Purpose:    set elements of the destination vector to the value
//  Parameters:
//    val        value to set the elements of the vector
//    pDst       pointer to the destination vector
//    len        length of the vectors
//  Returns:
//    ippStsNullPtrErr        pointer to the vector is NULL
//    ippStsSizeErr           length of the vector is less or equal zero
//    ippStsNoErr             otherwise
*/
IPPAPI ( IppStatus, ippsSet_32f,( Ipp32f val, Ipp32f* pDst, int len ))

/* /////////////////////////////////////////////////////////////////////////
// Name:                ippsRandUniformInitAlloc_32f
// Purpose:             Allocate and initialization parameters for the generator
//                      of noise with uniform distribution.
// Parameters:
//    pRandUniState     A pointer to the structure containing parameters for the
//                      generator of noise.
//    low               The lower bounds of the uniform distributions range.
//    high              The upper bounds of the uniform distributions range.
//    seed              The seed value used by the pseudo-random number generation
//                      algorithm.
//
// Returns:
//    ippStsNullPtrErr  pRandUniState==NULL
//    ippMemAllocErr    Can not allocate random uniform state
//    ippStsNoErr       No errors
//
*/
IPPAPI(IppStatus, ippsRandUniformInitAlloc_32f, (IppsRandUniState_32f** pRandUniState,
                                   Ipp32f low, Ipp32f high, unsigned int seed))

/* /////////////////////////////////////////////////////////////////////////
// Name:                     ippsRandUniformFree_32f
// Purpose:                  Close random uniform state
// Parameters:
//    pRandUniState          Pointer to the random uniform state
//
// Returns:
//    ippStsNullPtrErr       pState==NULL
//    ippStsContextMatchErr  pState->idCtx != idCtxRandUni
//    ippStsNoErr,           No errors
*/
IPPAPI (IppStatus, ippsRandUniformFree_32f, (IppsRandUniState_32f* pRandUniState))

/* /////////////////////////////////////////////////////////////////////////
// Name:                     ippsRandUniform_32f
// Purpose:                  Makes pseudo-random samples with a uniform distribution
// Parameters:
//    pDst                   The pointer to vector
//    len                    Vector's length
//    pRandUniState          A pointer to the structure containing parameters for the
//                           generator of noise
// Returns:
//    ippStsNullPtrErr       pRandUniState==NULL
//    ippStsContextMatchErr  pState->idCtx != idCtxRandUni
//    ippStsNoErr            No errors
*/
IPPAPI(IppStatus, ippsRandUniform_32f, (Ipp32f* pDst, int len, IppsRandUniState_32f* pRandUniState))

/* /////////////////////////////////////////////////////////////////////////////
//                  Arithmetic functions
///////////////////////////////////////////////////////////////////////////// */
/* ////////////////////////////////////////////////////////////////////////////
//  Names:       ippsAdd, ippsSub, ippsMul
//  Purpose:    add, subtract and multiply operations upon every element of
//              the source vector
//  Parameters:
//    pSrc                 pointer to the source vector
//    pSrcDst              pointer to the source/destination vector
//    pSrc1                pointer to the first source vector
//    pSrc2                pointer to the second source vector
//    pDst                 pointer to the destination vector
//    len                  length of the vectors
//    scaleFactor          scale factor value
//  Returns:
//    ippStsNullPtrErr     pointer(s) to the data is NULL
//    ippStsSizeErr        length of the vectors is less or equal zero
//    ippStsNoErr          otherwise
//  Note:
//    AddC(X,v,Y)    :  Y[n] = X[n] + v
//    SubC(X,v,Y)    :  Y[n] = X[n] - v
//    Mul(X,Y)       :  Y[n] = X[n] * Y[n]
//    MulC(X,v,Y)    :  Y[n] = X[n] * v
//    SubC(X,v,Y)    :  Y[n] = X[n] - v
//    SubCRev(X,v,Y) :  Y[n] = v - X[n]
//    Sub(X,Y)       :  Y[n] = Y[n] - X[n]
//    Sub(X,Y,Z)     :  Z[n] = Y[n] - X[n]
*/
IPPAPI(IppStatus, ippsAddC_32f_I,     (Ipp32f  val, Ipp32f*  pSrcDst, int len))
IPPAPI(IppStatus, ippsSubC_32f_I,     (Ipp32f  val, Ipp32f*  pSrcDst, int len))
IPPAPI(IppStatus, ippsMul_32f_I,  (const Ipp32f*  pSrc,
       Ipp32f*  pSrcDst, int len))
IPPAPI(IppStatus, ippsMulC_32f_I,     (Ipp32f  val, Ipp32f*  pSrcDst, int len))
IPPAPI(IppStatus, ippsSub_64f_I,  (const Ipp64f*  pSrc, Ipp64f*  pSrcDst, int len))
IPPAPI(IppStatus, ippsMulC_64f_I, (Ipp64f  val, Ipp64f*  pSrcDst, int len))

/* ////////////////////////////////////////////////////////////////////////////
//  Name:       ippsDiv
//  Purpose:    divide every element of the source vector by the scalar value
//              or by corresponding element of the second source vector
//  Parameters:
//    val               the divisor value
//    pSrc              pointer to the divisor source vector
//    pSrc1             pointer to the divisor source vector
//    pSrc2             pointer to the dividend source vector
//    pDst              pointer to the destination vector
//    pSrcDst           pointer to the source/destination vector
//    len               vector's length, number of items
//    scaleFactor       scale factor parameter value
//  Returns:
//    ippStsNullPtrErr     pointer(s) to the data vector is NULL
//    ippStsSizeErr        length of the vector is less or equal zero
//    ippStsDivByZeroErr   the scalar divisor value is zero
//    ippStsDivByZero      Warning status if an element of divisor vector is
//                      zero. If the dividend is zero then result is
//                      NaN, if the dividend is not zero then result
//                      is Infinity with correspondent sign. The
//                      execution is not aborted. For the integer operation
//                      zero instead of NaN and the corresponding bound
//                      values instead of Infinity
//    ippStsNoErr          otherwise
//  Note:
//    DivC(v,X,Y)  :    Y[n] = X[n] / v
//    DivC(v,X)    :    X[n] = X[n] / v
//    Div(X,Y)     :    Y[n] = Y[n] / X[n]
//    Div(X,Y,Z)   :    Z[n] = Y[n] / X[n]
*/
IPPAPI(IppStatus, ippsDiv_32f, (const Ipp32f* pSrc1, const Ipp32f* pSrc2,
       Ipp32f* pDst, int len))
IPPAPI(IppStatus, ippsDivC_64f_I, (Ipp64f val, Ipp64f* pSrcDst, int len))

/* /////////////////////////////////////////////////////////////////////////////
//  Names:      ippsSqr
//  Purpose:    compute square value for every element of the source vector
//  Parameters:
//    pSrcDst          pointer to the source/destination vector
//    pSrc             pointer to the input vector
//    pDst             pointer to the output vector
//    len              length of the vectors
//   scaleFactor       scale factor value
//  Returns:
//    ippStsNullPtrErr    pointer(s) the source data NULL
//    ippStsSizeErr       length of the vectors is less or equal zero
//    ippStsNoErr         otherwise
*/
IPPAPI(IppStatus,ippsSqr_32f_I,(Ipp32f* pSrcDst, int len))
IPPAPI(IppStatus,ippsSqr_32f,(const Ipp32f* pSrc, Ipp32f* pDst, int len))

/* /////////////////////////////////////////////////////////////////////////////
//  Names:      ippsSqrt
//  Purpose:    compute square root value for every element of the source vector
//  Parameters:
//   pSrc                 pointer to the source vector
//   pDst                 pointer to the destination vector
//   pSrcDst              pointer to the source/destination vector
//   len                  length of the vector(s), number of items
//   scaleFactor          scale factor value
//  Returns:
//   ippStsNullPtrErr        pointer to vector is NULL
//   ippStsSizeErr           length of the vector is less or equal zero
//   ippStsSqrtNegArg        negative value in real sequence
//   ippStsNoErr             otherwise
*/
IPPAPI(IppStatus,ippsSqrt_32f_I,(Ipp32f* pSrcDst,int len))
IPPAPI(IppStatus,ippsSqrt_32f,(const Ipp32f* pSrc,Ipp32f* pDst,int len))

/* /////////////////////////////////////////////////////////////////////////////
//                  Statistic functions
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:            ippsNorm
//  Purpose:         calculate norm of vector
//     Inf   - calculate C-norm of vector:  n = MAX |src1|
//     L1    - calculate L1-norm of vector: n = SUM |src1|
//     L2    - calculate L2-norm of vector: n = SQRT(SUM |src1|^2)
//     L2Sqr - calculate L2-norm of vector: n = SUM |src1|^2
//  Parameters:
//    pSrc           source data pointer
//    len            length of vector
//    pNorm          pointer to result
//    scaleFactor    scale factor value
//  Returns:
//    ippStsNoErr       otherwise
//    ippStsNullPtrErr  Some of pointers to input or output data are NULL
//    ippStsSizeErr     The length of vector is less or equal zero
//  Notes:
*/
IPPAPI(IppStatus, ippsNorm_L2_32f, (const Ipp32f* pSrc, int len, Ipp32f* pNorm))

/* /////////////////////////////////////////////////////////////////////////////
//  Names:      ippsMean
//  Purpose:    compute average value of all elements of the source vector
//  Parameters:
//   pSrc                pointer to the source vector
//   pMean               pointer to the result
//   len                 length of the source vector
//   scaleFactor         scale factor value
//  Returns:
//   ippStsNullPtrErr       pointer(s) to the vector or the result is NULL
//   ippStsSizeErr          length of the vector is less or equal 0
//   ippStsNoErr            otherwise
*/
IPPAPI(IppStatus,ippsMean_32f, (const Ipp32f*  pSrc,int len,Ipp32f*  pMean,
       IppHintAlgorithm hint))

/* /////////////////////////////////////////////////////////////////////////////
//                  Dot Product Functions
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsDotProd
//  Purpose:    compute Dot Product value
//  Parameters:
//     pSrc1               pointer to the source vector
//     pSrc2               pointer to the another source vector
//     len                 vector's length, number of items
//     pDp                 pointer to the result
//     scaleFactor         scale factor value
//  Returns:
//     ippStsNullPtrErr       pointer(s) pSrc pDst is NULL
//     ippStsSizeErr          length of the vectors is less or equal 0
//     ippStsNoErr            otherwise
//  Notes:
//     the functions don't conjugate one of the source vectors
*/

IPPAPI(IppStatus, ippsDotProd_32f, (const Ipp32f* pSrc1,
       const Ipp32f* pSrc2, int len, Ipp32f* pDp))

/* /////////////////////////////////////////////////////////////////////////////
//                  Windowing functions
//  Note: to create the window coefficients you have to make two calls
//        Set(1,x,n) and Win(x,n)
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Names:            ippsWinBartlett, ippsWinHamming
//  Parameters:
//   pSrcDst          pointer to the vector
//   len              length of the vector, window size
//  Returns:
//   ippStsNullPtrErr    pointer to the vector is NULL
//   ippStsSizeErr       length of the vector is less 3
//   ippStsNoErr         otherwise
*/
IPPAPI(IppStatus, ippsWinBartlett_32f_I, (Ipp32f* pSrcDst, int len))
IPPAPI(IppStatus, ippsWinHamming_32f_I, (Ipp32f* pSrcDst, int len))

/* /////////////////////////////////////////////////////////////////////////////
//                  FFT Context Functions
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsFFTInitAlloc_C, ippsFFTInitAlloc_R
//  Purpose:    create and initialize of FFT context
//  Parameters:
//     order    - base-2 logarithm of the number of samples in FFT
//     flag     - normalization flag
//     hint     - code specific use hints
//     pFFTSpec - where write pointer to new context
//  Returns:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pFFTSpec == NULL
//     ippStsFftOrderErr      bad the order value
//     ippStsFftFlagErr       bad the normalization flag value
//     ippStsMemAllocErr      memory allocation error
*/
IPPAPI (IppStatus, ippsFFTInitAlloc_R_32f,
                   ( IppsFFTSpec_R_32f** pFFTSpec,
                     int order, int flag, IppHintAlgorithm hint ))
IPPAPI (IppStatus, ippsFFTInitAlloc_C_32fc,
                   ( IppsFFTSpec_C_32fc** pFFTSpec,
                     int order, int flag, IppHintAlgorithm hint ))

/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsFFTFree_C, ippsFFTFree_R
//  Purpose:    delete FFT context
//  Parameters:
//     pFFTSpec - pointer to FFT context to be deleted
//  Returns:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pFFTSpec == NULL
//     ippStsContextMatchErr  bad context identifier
*/
IPPAPI (IppStatus, ippsFFTFree_R_32f,    ( IppsFFTSpec_R_32f*  pFFTSpec ))
IPPAPI (IppStatus, ippsFFTFree_C_32fc,   ( IppsFFTSpec_C_32fc* pFFTSpec ))

/* /////////////////////////////////////////////////////////////////////////////
//                  FFT Buffer Size
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsFFTGetBufSize_C, ippsFFTGetBufSize_R
//  Purpose:    get size of the FFT work buffer (on bytes)
//  Parameters:
//     pFFTSpec - pointer to the FFT structure
//     pSize    - Pointer to the FFT work buffer size value
//  Returns:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pFFTSpec == NULL or pSize == NULL
//     ippStsContextMatchErr  bad context identifier
*/
IPPAPI (IppStatus, ippsFFTGetBufSize_R_32f,
                   ( const IppsFFTSpec_R_32f*  pFFTSpec, int* pSize ))
IPPAPI (IppStatus, ippsFFTGetBufSize_C_32fc,
                   ( const IppsFFTSpec_C_32fc* pFFTSpec, int* pSize ))

/* /////////////////////////////////////////////////////////////////////////////
//                  FFT Complex Transforms
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsFFTFwd_CToC, ippsFFTInv_CToC
//  Purpose:    compute forward and inverse FFT of the complex signal
//  Parameters:
//     pFFTSpec - pointer to FFT context
//     pSrc     - pointer to source complex signal
//     pDst     - pointer to destination complex signal
//     pSrcRe   - pointer to real      part of source signal
//     pSrcIm   - pointer to imaginary part of source signal
//     pDstRe   - pointer to real      part of destination signal
//     pDstIm   - pointer to imaginary part of destination signal
//     pSrcDSt  - pointer to complex signal
//     pSrcDstRe- pointer to real      part of signal
//     pSrcDstIm- pointer to imaginary part of signal
//     pBuffer  - pointer to work buffer
//     scaleFactor
//              - scale factor for output result
//  Returns:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pFFTSpec == NULL or
//                            pSrc == NULL or pDst == NULL or
//                            pSrcRe == NULL or pSrcIm == NULL or
//                            pDstRe == NULL or pDstIm == NULL or
//     ippStsContextMatchErr  bad context identifier
//     ippStsMemAllocErr      memory allocation error
*/
IPPAPI (IppStatus, ippsFFTFwd_CToC_32fc,
                   ( const Ipp32fc* pSrc, Ipp32fc* pDst,
                     const IppsFFTSpec_C_32fc* pFFTSpec, Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippsFFTInv_CToC_32fc,
                   ( const Ipp32fc* pSrc, Ipp32fc* pDst,
                     const IppsFFTSpec_C_32fc* pFFTSpec, Ipp8u* pBuffer ))

/* /////////////////////////////////////////////////////////////////////////////
//                  FFT Real Packed Transforms
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsFFTFwd_RToPerm, ippsFFTFwd_RToPack, ippsFFTFwd_RToCCS
//              ippsFFTInv_PermToR, ippsFFTInv_PackToR, ippsFFTInv_CCSToR
//  Purpose:    compute forward and inverse FFT of real signal
//              using Perm, Pack or Ccs packed format
//  Parameters:
//     pFFTSpec - pointer to FFT context
//     pSrc     - pointer to source signal
//     pDst     - pointer to destination signal
//     pSrcDst  - pointer to signal
//     pBuffer  - pointer to work buffer
//     scaleFactor
//              - scale factor for output result
//  Returns:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pFFTSpec == NULL or
//                            pSrc == NULL or pDst == NULL
//     ippStsContextMatchErr  bad context identifier
//     ippStsMemAllocErr      memory allocation error
*/
IPPAPI (IppStatus, ippsFFTFwd_RToPack_32f,
                   ( const Ipp32f* pSrc, Ipp32f* pDst,
                     const IppsFFTSpec_R_32f* pFFTSpec, Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippsFFTInv_PackToR_32f,
                   ( const Ipp32f* pSrc, Ipp32f* pDst,
                     const IppsFFTSpec_R_32f* pFFTSpec, Ipp8u* pBuffer ))

/* /////////////////////////////////////////////////////////////////////////////
//                  DFT Context Functions
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsDFTInitAlloc_C, ippsDFTInitAlloc_R
//  Purpose:    create and initialize of DFT context
//  Parameters:
//     length   - number of samples in DFT
//     flag     - normalization flag
//     hint     - code specific use hints
//     pDFTSpec - where write pointer to new context
//  Returns:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pDFTSpec == NULL
//     ippStsSizeErr          bad the length value
//     ippStsFlagErr       bad the normalization flag value
//     ippStsMemAllocErr      memory allocation error
*/
IPPAPI (IppStatus, ippsDFTInitAlloc_R_32f,
                   ( IppsDFTSpec_R_32f** pDFTSpec,
                     int length, int flag, IppHintAlgorithm hint ))
IPPAPI (IppStatus, ippsDFTInitAlloc_C_32fc,
                   ( IppsDFTSpec_C_32fc** pDFTSpec,
                     int length, int flag, IppHintAlgorithm hint ))

/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsDFTFree_C, ippsDFTFree_R
//  Purpose:    delete DFT context
//  Parameters:
//     pDFTSpec - pointer to DFT context to be deleted
//  Returns:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pDFTSpec == NULL
//     ippStsContextMatchErr  bad context identifier
*/
IPPAPI (IppStatus, ippsDFTFree_R_32f,  ( IppsDFTSpec_R_32f*  pDFTSpec ))
IPPAPI (IppStatus, ippsDFTFree_C_32fc, ( IppsDFTSpec_C_32fc* pDFTSpec ))

/* /////////////////////////////////////////////////////////////////////////////
//                  DFT Buffer Size
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsDFTGetBufSize_C, ippsDFTGetBufSize_R
//  Purpose:    get size of the DFT work buffer (on bytes)
//  Parameters:
//     pDFTSpec - pointer to DFT context
//     pSize     - where write size of buffer
//  Returns:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pDFTSpec == NULL or pSize == NULL
//     ippStsContextMatchErr  bad context identifier
*/
IPPAPI (IppStatus, ippsDFTGetBufSize_R_32f,
                   ( const IppsDFTSpec_R_32f*  pDFTSpec, int* pSize ))
IPPAPI (IppStatus, ippsDFTGetBufSize_C_32fc,
                   ( const IppsDFTSpec_C_32fc* pDFTSpec, int* pSize ))

/* /////////////////////////////////////////////////////////////////////////////
//                  DFT Complex Transforms
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsDFTFwd_CToC, ippsDFTInv_CToC
//  Purpose:    compute forward and inverse DFT of the complex signal
//  Parameters:
//     pDFTSpec - pointer to DFT context
//     pSrc     - pointer to source complex signal
//     pDst     - pointer to destination complex signal
//     pSrcRe   - pointer to real      part of source signal
//     pSrcIm   - pointer to imaginary part of source signal
//     pDstRe   - pointer to real      part of destination signal
//     pDstIm   - pointer to imaginary part of destination signal
//     pBuffer  - pointer to work buffer
//     scaleFactor
//              - scale factor for output result
//  Returns:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pDFTSpec == NULL or
//                            pSrc == NULL or pDst == NULL or
//                            pSrcRe == NULL or pSrcIm == NULL or
//                            pDstRe == NULL or pDstIm == NULL or
//     ippStsContextMatchErr  bad context identifier
//     ippStsMemAllocErr      memory allocation error
*/
IPPAPI (IppStatus, ippsDFTFwd_CToC_32fc,
                   ( const Ipp32fc* pSrc, Ipp32fc* pDst,
                     const IppsDFTSpec_C_32fc* pDFTSpec, Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippsDFTInv_CToC_32fc,
                   ( const Ipp32fc* pSrc, Ipp32fc* pDst,
                     const IppsDFTSpec_C_32fc* pDFTSpec, Ipp8u* pBuffer ))

/* /////////////////////////////////////////////////////////////////////////////
//                  DFT Real Packed Transforms
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsDFTFwd_RToPerm, ippsDFTFwd_RToPack, ippsDFTFwd_RToCCS
//              ippsDFTInv_PermToR, ippsDFTInv_PackToR, ippsDFTInv_CCSToR
//  Purpose:    compute forward and inverse DFT of real signal
//              using Perm, Pack or Ccs packed format
//  Parameters:
//     pDFTSpec - pointer to DFT context
//     pSrc     - pointer to source signal
//     pDst     - pointer to destination signal
//     pBuffer  - pointer to work buffer
//     scaleFactor
//              - scale factor for output result
//  Returns:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pDFTSpec == NULL or
//                            pSrc == NULL or pDst == NULL
//     ippStsContextMatchErr  bad context identifier
//     ippStsMemAllocErr      memory allocation error
*/
IPPAPI (IppStatus, ippsDFTFwd_RToPack_32f,
                   ( const Ipp32f* pSrc, Ipp32f* pDst,
                     const IppsDFTSpec_R_32f* pDFTSpec, Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippsDFTInv_PackToR_32f,
                   ( const Ipp32f* pSrc, Ipp32f* pDst,
                     const IppsDFTSpec_R_32f* pDFTSpec, Ipp8u* pBuffer ))

/* /////////////////////////////////////////////////////////////////////////////
//                  DCT Context Functions
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//                  DCT Get Size Functions
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsDCTFwdGetSize, ippsDCTInvGetSize
//  Purpose:    get sizes of the DCTSpec and buffers (on bytes)
//  Arguments:
//     len             - number of samples in DCT
//     hint            - code specific use hints
//     pSpecSize       - where write size of DCTSpec
//     pSpecBufferSize - where write size of buffer for DCTInit functions
//     pBufferSize     - where write size of buffer for DCT calculation
//  Return:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pSpecSize == NULL or pSpecBufferSize == NULL or
//                            pBufferSize == NULL
//     ippStsSizeErr          bad the len value
*/
IPPAPI (IppStatus, ippsDCTFwdGetSize_32f,
                   ( int len, IppHintAlgorithm hint,
                     int* pSpecSize, int* pSpecBufferSize, int* pBufferSize ))
IPPAPI (IppStatus, ippsDCTInvGetSize_32f,
                   ( int len, IppHintAlgorithm hint,
                     int* pSpecSize, int* pSpecBufferSize, int* pBufferSize ))

/* /////////////////////////////////////////////////////////////////////////////
//                  DCT Context Functions
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsDCTFwdInit, ippsDCTInvInit
//  Purpose:    initialize of DCT context
//  Arguments:
//     len         - number of samples in DCT
//     hint        - code specific use hints
//     ppDCTSpec   - where write pointer to new context
//     pSpec       - pointer to area for DCTSpec
//     pSpecBuffer - pointer to work buffer
//  Return:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       ppDCTSpec == NULL or
//                            pSpec == NULL or pMemInit == NULL
//     ippStsSizeErr          bad the len value
*/
IPPAPI (IppStatus, ippsDCTFwdInit_32f,
                   ( IppsDCTFwdSpec_32f** ppDCTSpec,
                     int len, IppHintAlgorithm hint,
                     Ipp8u* pSpec, Ipp8u* pSpecBuffer ))
IPPAPI (IppStatus, ippsDCTInvInit_32f,
                   ( IppsDCTInvSpec_32f** ppDCTSpec,
                     int len, IppHintAlgorithm hint,
                     Ipp8u* pSpec, Ipp8u* pSpecBuffer ))

/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsDCTFwdInitAlloc, ippsDCTInvInitAlloc
//  Purpose:    create and initialize of DCT context
//  Arguments:
//     len       - number of samples in DCT
//     hint      - code specific use hints
//     ppDCTSpec - where write pointer to new context
//  Return:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       ppDCTSpec == NULL
//     ippStsSizeErr          bad the len value
//     ippStsMemAllocErr      memory allocation error
*/
IPPAPI (IppStatus, ippsDCTFwdInitAlloc_32f,
                   ( IppsDCTFwdSpec_32f** ppDCTSpec,
                     int len, IppHintAlgorithm hint ))
IPPAPI (IppStatus, ippsDCTInvInitAlloc_32f,
                   ( IppsDCTInvSpec_32f** ppDCTSpec,
                     int len, IppHintAlgorithm hint ))

/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsDCTFwdFree, ippsDCTInvFree
//  Purpose:    delete DCT context
//  Arguments:
//     pDCTSpec - pointer to DCT context to be deleted
//  Return:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pDCTSpec == NULL
//     ippStsContextMatchErr  bad context identifier
*/
IPPAPI (IppStatus, ippsDCTFwdFree_32f, ( IppsDCTFwdSpec_32f*  pDCTSpec ))
IPPAPI (IppStatus, ippsDCTInvFree_32f, ( IppsDCTInvSpec_32f*  pDCTSpec ))

/* /////////////////////////////////////////////////////////////////////////////
//                  DCT Buffer Size
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsDCTFwdGetBufSize, ippsDCTInvGetBufSize
//  Purpose:    get size of the DCT work buffer (on bytes)
//  Arguments:
//     pDCTSpec    - pointer to the DCT structure
//     pBufferSize - pointer to the DCT work buffer size value
//  Return:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pDCTSpec == NULL or pSize == NULL
//     ippStsContextMatchErr  bad context identifier
*/
IPPAPI (IppStatus, ippsDCTFwdGetBufSize_32f,
                   ( const IppsDCTFwdSpec_32f* pDCTSpec, int* pBufferSize ))
IPPAPI (IppStatus, ippsDCTInvGetBufSize_32f,
                   ( const IppsDCTInvSpec_32f* pDCTSpec, int* pBufferSize ))

/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsDCTFwd, ippsDCTInv
//  Purpose:    compute forward and inverse DCT of signal
//  Arguments:
//     pDCTSpec - pointer to DCT context
//     pSrc     - pointer to source signal
//     pDst     - pointer to destination signal
//     pSrcDst  - pointer to signal
//     pBuffer  - pointer to work buffer
//     scaleFactor
//              - scale factor for output result
//  Return:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pDCTSpec == NULL or
//                            pSrc == NULL or pDst == NULL or pSrcDst == NULL
//     ippStsContextMatchErr  bad context identifier
//     ippStsMemAllocErr      memory allocation error
*/
IPPAPI (IppStatus, ippsDCTFwd_32f,
                   ( const Ipp32f* pSrc, Ipp32f* pDst,
                     const IppsDCTFwdSpec_32f* pDCTSpec, Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippsDCTInv_32f,
                   ( const Ipp32f* pSrc, Ipp32f* pDst,
                     const IppsDCTInvSpec_32f* pDCTSpec, Ipp8u* pBuffer ))

/* /////////////////////////////////////////////////////////////////////////////
//  Names:      ippsAutoCorr_32f
//  Purpose:    Calculate the autocorrelation
//  Parameters:
//     pSrc   - pointer to the source vector
//     srcLen - source vector length
//     pDst   - pointer to the auto-correlation result vector
//     dstLen - length of auto-correlation
//  Returns:
//   ippStsNoErr       otherwise
//   ippStsNullPtrErr  either pSrc or(and) pDst are NULL
//   ippStsSizeErr     vector's length is not positive
*/
IPPAPI(IppStatus, ippsAutoCorr_32f, ( const Ipp32f* pSrc, int srcLen, Ipp32f* pDst, int dstLen ))

/* /////////////////////////////////////////////////////////////////////////////
//  Names:       ippsConj
//  Purpose:     complex conjugate data vector
//  Parameters:
//    pSrc               pointer to the input vector
//    pDst               pointer to the output vector
//    len                length of the vectors
//  Return:
//    ippStsNullPtrErr      pointer(s) to the data is NULL
//    ippStsSizeErr         length of the vectors is less or equal zero
//    ippStsNoErr           otherwise
//  Notes:
//    the ConjFlip version conjugates and stores result in reverse order
*/
IPPAPI ( IppStatus, ippsConj_32fc_I, ( Ipp32fc* pSrcDst, int len ))
IPPAPI ( IppStatus, ippsConj_64fc_I, ( Ipp64fc* pSrcDst, int len ))

/* /////////////////////////////////////////////////////////////////////////////
//  Name:              ippsDurbin_*
//  Purpose:           Performs Durbin's recursion on an input vector
//                     of autocorrelations.
//  Parameters:
//    pSrc             Pointer to the input vector [ len+1].
//    pDst             Pointer to the output LPC coefficients vector [len].
//    len              Length of the input and output vectors.
//    pErr             Pointer to the residual prediction error.
//    scaleFactor
//  Return:
//    ippStsNoErr      Indicates no error.
//    ippStsNullPtrErr Indicates an error when the pSrc or  pDst or pErr
//                     pointer is null.
//   ippStsSizeErr     Indicates an error when length less than or equal to 0
//   ippStsMemAllocErr memory allocation error
//   ippStsNoOperation indicates no solution to the LPC problem.
//
*/
IPPAPI(IppStatus, ippsDurbin_32f,(const Ipp32f *pSrc, Ipp32f *pDst, int lenDst, Ipp32f* pErr))

/* /////////////////////////////////////////////////////////////////////////////
//  Names:      ippsMax_64f, ippsMin_64f
//  Purpose:    find maximum/minimum value among all elements of the source vector
//  Parameters:
//   pSrc                 pointer to the source vector
//   pMax,pMin            pointer to the result
//   len                  length of the vector
//  Return:
//   ippStsNullPtrErr        pointer(s) to the vector or the result is NULL
//   ippStsSizeErr           length of the vector is less or equal 0
//   ippStsNoErr             otherwise
*/
IPPAPI(IppStatus,ippsMax_64f,(const Ipp64f* pSrc,int len,Ipp64f* pMin))
IPPAPI(IppStatus,ippsMin_64f,(const Ipp64f* pSrc,int len,Ipp64f* pMin))

/* /////////////////////////////////////////////////////////////////////////////
//  Names:       ippsMinIndx_32f
//
//  Purpose:    find element with min value and return the value and the index
//  Parameters:
//   pSrc           pointer to the input vector
//   len            length of the vector
//   pMin           address to place min value found
//   pIndx          address to place index found, may be NULL
//  Return:
//   ippStsNullPtrErr        pointer(s) to the data is NULL
//   ippStsSizeErr           length of the vector is less or equal zero
//   ippStsNoErr             otherwise
*/
IPPAPI(IppStatus, ippsMinIndx_32f, (const Ipp32f* pSrc, int len, Ipp32f* pMin, int* pIndx))


#ifdef __cplusplus
}
#endif

#endif /* _CORE */

#endif

#endif /* __DEPENDENCESP_H__ */
