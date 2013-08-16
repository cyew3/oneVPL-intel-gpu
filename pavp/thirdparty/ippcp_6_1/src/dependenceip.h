/* ////////////////////////// "dependenceip.h" /////////////////////////////////
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

#ifndef __DEPENDENCEIP_H__
#define __DEPENDENCEIP_H__

#if (_IPP_ARCH == _IPP_ARCH_XSC)

#ifndef _CORE

#ifndef __IPPI_H__
  #include "ippi.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippiSet
//  Purpose:    Sets pixels in the image buffer to a constant value
//  Returns:
//    ippStsNullPtrErr  One of pointers is NULL
//    ippStsSizeErr     roiSize has a field with negative or zero value
//    ippStsStepErr     dstStep or maskStep has zero or negative value
//    ippStsNoErr       OK
//  Parameters:
//    value      Constant value assigned to each pixel in the image buffer
//    pDst       Pointer to the destination image buffer
//    dstStep    Step in bytes through the destination image buffer
//    roiSize    Size of the ROI
//    pMask      Pointer to the mask image buffer
//    maskStep   Step in bytes through the mask image buffer
*/
IPPAPI ( IppStatus, ippiSet_32f_C1R,
                    ( Ipp32f value, Ipp32f* pDst, int dstStep,
                      IppiSize roiSize ))
IPPAPI ( IppStatus, ippiSet_32f_C3R,
                    ( const Ipp32f value[3], Ipp32f* pDst, int dstStep,
                      IppiSize roiSize ))
IPPAPI ( IppStatus, ippiSet_32f_AC4R,
                    ( const Ipp32f value[3], Ipp32f* pDst, int dstStep,
                      IppiSize roiSize ))

/* ////////////////////////////////////////////////////////////////////////////
//  Name:       ippiCopy
//  Purpose:  copy pixel values from the source image to the destination  image
//  Returns:
//    ippStsNullPtrErr  One of the pointers is NULL
//    ippStsSizeErr     roiSize has a field with zero or negative value
//    ippStsNoErr       OK
//  Parameters:
//    pSrc              Pointer  to the source image buffer
//    srcStep           Step in bytes through the source image buffer
//    pDst              Pointer to the  destination image buffer
//    dstStep           Step in bytes through the destination image buffer
//    roiSize           Size of the ROI
//    pMask             Pointer to the mask image buffer
//    maskStep          Step in bytes through the mask image buffer
*/
IPPAPI ( IppStatus, ippiCopy_32f_C1R,
                    ( const Ipp32f* pSrc, int srcStep,
                      Ipp32f* pDst, int dstStep,IppiSize roiSize ))
IPPAPI ( IppStatus, ippiCopy_32f_C3R,
                    ( const Ipp32f* pSrc, int srcStep,
                      Ipp32f* pDst, int dstStep,IppiSize roiSize ))
IPPAPI ( IppStatus, ippiCopy_32f_AC4R,
                    ( const Ipp32f* pSrc, int srcStep,
                      Ipp32f* pDst, int dstStep,IppiSize roiSize ))

/* ///////////////////////////////////////////////////////////////////////////////////////
//  Name: ippiAddC_32f_C1IR, ippiAddC_32f_C3IR, ippiAddC_32f_C4IR, ippiAddC_32f_AC4IR,
//        ippiSubC_32f_C1IR, ippiSubC_32f_C3IR, ippiSubC_32f_C4IR, ippiSubC_32f_AC4IR,
//        ippiMulC_32f_C1IR, ippiMulC_32f_C3IR, ippiMulC_32f_C4IR, ippiMulC_32f_AC4IR
//  Purpose:    Adds, subtracts, or multiplies pixel values of an image
//              and a constant, and places the results in the same image.
//  Returns:
//    ippStsNoErr              OK
//    ippStsNullPtrErr         Pointer is NULL
//    ippStsSizeErr            Width or height of an image is less than or equal to zero
//  Parameters:
//    value                    The constant value for the specified operation
//    pSrcDst                  Pointer to the image
//    srcDstStep               Step through the image
//    roiSize                  Size of the ROI
*/
IPPAPI(IppStatus, ippiAddC_32f_C1IR,  (Ipp32f value, Ipp32f* pSrcDst, int srcDstStep,
                                       IppiSize roiSize))
IPPAPI(IppStatus, ippiAddC_32f_C3IR,  (const Ipp32f value[3], Ipp32f* pSrcDst, int srcDstStep,
                                       IppiSize roiSize))
IPPAPI(IppStatus, ippiAddC_32f_C4IR,  (const Ipp32f value[4], Ipp32f* pSrcDst, int srcDstStep,
                                       IppiSize roiSize))
IPPAPI(IppStatus, ippiAddC_32f_AC4IR, (const Ipp32f value[3], Ipp32f* pSrcDst, int srcDstStep,
                                       IppiSize roiSize))
IPPAPI(IppStatus, ippiSubC_32f_C1IR,  (Ipp32f value, Ipp32f* pSrcDst, int srcDstStep,
                                       IppiSize roiSize))
IPPAPI(IppStatus, ippiSubC_32f_C3IR,  (const Ipp32f value[3], Ipp32f* pSrcDst, int srcDstStep,
                                       IppiSize roiSize))
IPPAPI(IppStatus, ippiSubC_32f_C4IR,  (const Ipp32f value[4], Ipp32f* pSrcDst, int srcDstStep,
                                       IppiSize roiSize))
IPPAPI(IppStatus, ippiSubC_32f_AC4IR, (const Ipp32f value[3], Ipp32f* pSrcDst, int srcDstStep,
                                       IppiSize roiSize))
IPPAPI(IppStatus, ippiMulC_32f_C1IR,  (Ipp32f value, Ipp32f* pSrcDst, int srcDstStep,
                                       IppiSize roiSize))
IPPAPI(IppStatus, ippiMulC_32f_C3IR,  (const Ipp32f value[3], Ipp32f* pSrcDst, int srcDstStep,
                                       IppiSize roiSize))
IPPAPI(IppStatus, ippiMulC_32f_C4IR,  (const Ipp32f value[4], Ipp32f* pSrcDst, int srcDstStep,
                                       IppiSize roiSize))
IPPAPI(IppStatus, ippiMulC_32f_AC4IR, (const Ipp32f value[3], Ipp32f* pSrcDst, int srcDstStep,
                                       IppiSize roiSize))

/* ////////////////////////////////////////////////////////////////////////////////////
//  Name: ippiAdd_32f_C1IR, ippiAdd_32f_C3IR, ippiAdd_32f_C4IR, ippiAdd_32f_AC4IR,
//        ippiSub_32f_C1IR, ippiSub_32f_C3IR, ippiSub_32f_C4IR, ippiSub_32f_AC4IR,
//        ippiMul_32f_C1IR, ippiMul_32f_C3IR, ippiMul_32f_C4IR, ippiMul_32f_AC4IR
//  Purpose:    Adds, subtracts, or multiplies pixel values of two source images
//              and places the results in the first image.
//  Returns:
//    ippStsNoErr              OK
//    ippStsNullPtrErr         One of the pointers is NULL
//    ippStsSizeErr            Width or height of images is less than or equal to zero
//  Parameters:
//    pSrc                     Pointer to the second source image
//    srcStep                  Step through the second source image
//    pSrcDst                  Pointer to the  first source/destination image
//    srcDstStep               Step through the first source/destination image
//    roiSize                  Size of the ROI
*/
IPPAPI(IppStatus, ippiAdd_32f_C1IR,  (const Ipp32f* pSrc, int srcStep, Ipp32f* pSrcDst,
                                      int srcDstStep, IppiSize roiSize))
IPPAPI(IppStatus, ippiAdd_32f_C3IR,  (const Ipp32f* pSrc, int srcStep, Ipp32f* pSrcDst,
                                      int srcDstStep, IppiSize roiSize))
IPPAPI(IppStatus, ippiAdd_32f_C4IR,  (const Ipp32f* pSrc, int srcStep, Ipp32f* pSrcDst,
                                      int srcDstStep, IppiSize roiSize))
IPPAPI(IppStatus, ippiAdd_32f_AC4IR, (const Ipp32f* pSrc, int srcStep, Ipp32f* pSrcDst,
                                      int srcDstStep, IppiSize roiSize))
IPPAPI(IppStatus, ippiSub_32f_C1IR,  (const Ipp32f* pSrc, int srcStep, Ipp32f* pSrcDst,
                                      int srcDstStep, IppiSize roiSize))
IPPAPI(IppStatus, ippiSub_32f_C3IR,  (const Ipp32f* pSrc, int srcStep, Ipp32f* pSrcDst,
                                      int srcDstStep, IppiSize roiSize))
IPPAPI(IppStatus, ippiSub_32f_C4IR,  (const Ipp32f* pSrc, int srcStep, Ipp32f* pSrcDst,
                                      int srcDstStep, IppiSize roiSize))
IPPAPI(IppStatus, ippiSub_32f_AC4IR, (const Ipp32f* pSrc, int srcStep, Ipp32f* pSrcDst,
                                      int srcDstStep, IppiSize roiSize))

/* ////////////////////////////////////////////////////////////////////////////
//  Name:       ippiDiv_32f_C1IR, ippiDiv_32f_C3IR ippiDiv_32f_C4IR ippiDiv_32f_AC4IR
//  Purpose:    Divides pixel values of an image by pixel values of
//              another image and places the results in the dividend source
//              image.
//  Returns:
//    ippStsNoErr              OK
//    ippStsNullPtrErr         One of the pointers is NULL
//    ippStsSizeErr            roiSize has a field with zero or negative value
//    ippStsStepErr            At least one step value is less than or equal to zero
//    ippStsDivByZero          A warning that a divisor value is zero, the function
//                             execution is continued.
//                             If a dividend is equal to zero, then the result is NAN_32F;
//                             if it is greater than zero, then the result is INF_32F,
//                             if it is less than zero, then the result is INF_NEG_32F
//  Parameters:
//    pSrc                     Pointer to the divisor source image
//    srcStep                  Step through the divisor source image
//    pSrcDst                  Pointer to the dividend source/destination image
//    srcDstStep               Step through the dividend source/destination image
//    roiSize                  Size of the ROI
*/
IPPAPI(IppStatus, ippiDiv_32f_C1IR,    (const Ipp32f* pSrc, int srcStep,
                                              Ipp32f* pSrcDst, int srcDstStep, IppiSize roiSize))
IPPAPI(IppStatus, ippiDiv_32f_C3IR,    (const Ipp32f* pSrc, int srcStep,
                                              Ipp32f* pSrcDst, int srcDstStep, IppiSize roiSize))
IPPAPI(IppStatus, ippiDiv_32f_C4IR,    (const Ipp32f* pSrc, int srcStep,
                                              Ipp32f* pSrcDst, int srcDstStep, IppiSize roiSize))
IPPAPI(IppStatus, ippiDiv_32f_AC4IR,    (const Ipp32f* pSrc, int srcStep,
                                              Ipp32f* pSrcDst, int srcDstStep, IppiSize roiSize))

/* ////////////////////////////////////////////////////////////////////////////
//  Name:       ippiConvert
//  Purpose:    Converts pixel values of an image from one bit depth to another
//  Returns:
//    ippStsNullPtrErr      One of the pointers is NULL
//    ippStsSizeErr         roiSize has a field with zero or negative value
//    ippStsStepErr         srcStep or dstStep has zero or negative value
//    ippStsNoErr           OK
//  Parameters:
//    pSrc                  Pointer  to the source image
//    srcStep               Step through the source image
//    pDst                  Pointer to the  destination image
//    dstStep               Step in bytes through the destination image
//    roiSize               Size of the ROI
//    roundMode             Rounding mode, ippRndZero or ippRndNear
*/
IPPAPI ( IppStatus, ippiConvert_32f8u_C1R,
        ( const Ipp32f* pSrc, int srcStep, Ipp8u* pDst, int dstStep,
          IppiSize roiSize, IppRoundMode roundMode ))
IPPAPI ( IppStatus, ippiConvert_32f8u_C3R,
        ( const Ipp32f* pSrc, int srcStep, Ipp8u* pDst, int dstStep,
          IppiSize roiSize, IppRoundMode roundMode ))
IPPAPI ( IppStatus, ippiConvert_32f8u_C4R,
        ( const Ipp32f* pSrc, int srcStep, Ipp8u* pDst, int dstStep,
          IppiSize roiSize, IppRoundMode roundMode ))
IPPAPI ( IppStatus, ippiConvert_32f8u_AC4R,
        ( const Ipp32f* pSrc, int srcStep, Ipp8u* pDst, int dstStep,
          IppiSize roiSize, IppRoundMode roundMode ))
IPPAPI ( IppStatus, ippiConvert_32f16s_C1R,
        ( const Ipp32f* pSrc, int srcStep, Ipp16s* pDst, int dstStep,
          IppiSize roiSize, IppRoundMode roundMode ))
IPPAPI ( IppStatus, ippiConvert_32f16s_C3R,
        ( const Ipp32f* pSrc, int srcStep, Ipp16s* pDst, int dstStep,
          IppiSize roiSize, IppRoundMode roundMode ))
IPPAPI ( IppStatus, ippiConvert_32f16s_AC4R,
        ( const Ipp32f* pSrc, int srcStep, Ipp16s* pDst, int dstStep,
          IppiSize roiSize, IppRoundMode roundMode ))
IPPAPI ( IppStatus, ippiConvert_8u32f_C1R,
        (const Ipp8u* pSrc, int srcStep, Ipp32f* pDst, int dstStep,
         IppiSize roiSize ))
IPPAPI ( IppStatus, ippiConvert_8s32f_C1R,
        (const Ipp8s* pSrc, int srcStep, Ipp32f* pDst, int dstStep,
         IppiSize roiSize ))
IPPAPI ( IppStatus, ippiConvert_16s32f_C1R,
        (const Ipp16s* pSrc, int srcStep, Ipp32f* pDst, int dstStep,
         IppiSize roiSize ))

/* /////////////////////////////////////////////////////////////////////////////
//  Name:           ippiNorm_L2
//  Purpose:        computes the L2-norm of pixel values of the image: n = SQRT(SUM |src1|^2)
//  Returns:        IppStatus
//    ippStsNoErr        OK
//    ippStsNullPtrErr   One of the pointers is NULL
//    ippStsSizeErr      roiSize has a field with zero or negative value
//  Parameters:
//    pSrc        Pointer to the source image.
//    srcStep     Step through the source image
//    roiSize     Size of the source ROI.
//    pValue      Pointer to the computed norm (one-channel data)
//    value       Array of the computed norms for each channel (multi-channel data)
//    hint        Option to specify the computation algorithm
*/
IPPAPI(IppStatus, ippiNorm_L2_32f_C1R, (const Ipp32f* pSrc, int srcStep,
                                      IppiSize roiSize, Ipp64f* pValue, IppHintAlgorithm hint))
IPPAPI(IppStatus, ippiNorm_L2_32f_C3R, (const Ipp32f* pSrc, int srcStep,
                                      IppiSize roiSize, Ipp64f value[3], IppHintAlgorithm hint))
IPPAPI(IppStatus, ippiNorm_L2_32f_AC4R, (const Ipp32f* pSrc, int srcStep,
                                       IppiSize roiSize, Ipp64f value[3], IppHintAlgorithm hint))
IPPAPI(IppStatus, ippiNorm_L2_32f_C4R, (const Ipp32f* pSrc, int srcStep,
                                       IppiSize roiSize, Ipp64f value[4], IppHintAlgorithm hint))

/* /////////////////////////////////////////////////////////////////////////////
//  Name:           ippiMean
//  Purpose:        computes the mean of image pixel values
//  Returns:        IppStatus
//    ippStsNoErr        OK
//    ippStsNullPtrErr   One of the pointers is NULL
//    ippStsSizeErr      roiSize has a field with zero or negative value.
//    ippStsStepErr      srcStep is less than or equal to zero
//  Parameters:
//    pSrc        Pointer to the source image.
//    srcStep     Step in bytes through the source image
//    roiSize     Size of the source ROI.
//    pMean       Pointer to the result (one-channel data)
//    mean        Array containing the results (multi-channel data)
//    hint        Option to select the algorithmic implementation of the function
*/
IPPAPI(IppStatus, ippiMean_32f_C1R, (const Ipp32f* pSrc, int srcStep,
                                    IppiSize roiSize, Ipp64f* pMean, IppHintAlgorithm hint))
IPPAPI(IppStatus, ippiMean_32f_C3R, (const Ipp32f* pSrc, int srcStep,
                                    IppiSize roiSize, Ipp64f mean[3], IppHintAlgorithm hint))
IPPAPI(IppStatus, ippiMean_32f_C4R, (const Ipp32f* pSrc, int srcStep,
                                     IppiSize roiSize, Ipp64f mean[4], IppHintAlgorithm hint))
IPPAPI(IppStatus, ippiMean_32f_AC4R, (const Ipp32f* pSrc, int srcStep,
                                     IppiSize roiSize, Ipp64f mean[3], IppHintAlgorithm hint))

/* /////////////////////////////////////////////////////////////////////////////
//  Name:           ippiMinMax
//  Purpose:        computes the minimum and maximum of image pixel value
//  Returns:        IppStatus
//    ippStsNoErr        OK
//    ippStsNullPtrErr   One of the pointers is NULL
//    ippStsSizeErr      roiSize has a field with zero or negative value
//  Parameters:
//    pSrc        Pointer to the source image
//    srcStep     Step in bytes through the source image
//    roiSize     Size of the source image ROI.
//    pMin, pMax  Pointers to the results (C1)
//    min, max    Arrays containing the results (C3, AC4, C4)
*/
IPPAPI(IppStatus, ippiMinMax_32f_C1R, (const Ipp32f* pSrc, int srcStep, IppiSize roiSize, Ipp32f* pMin, Ipp32f* pMax))
IPPAPI(IppStatus, ippiMinMax_32f_AC4R, (const Ipp32f* pSrc, int srcStep, IppiSize roiSize, Ipp32f min[3], Ipp32f max[3]))


/* /////////////////////////////////////////////////////////////////////////////
//                  FFT Context Functions
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippiFFTInitAlloc
//  Purpose:    Creates and initializes the FFT context structure
//  Parameters:
//     orderX     Base-2 logarithm of the number of samples in FFT (width)
//     orderY     Base-2 logarithm of the number of samples in FFT (height)
//     flag       Flag to choose the results normalization factors
//     hint       Option to select the algorithmic implementation of the transform
//                function
//     pFFTSpec   Pointer to the pointer to the FFT context structure
//  Returns:
//     ippStsNoErr            No errors
//     ippStsNullPtrErr       pFFTSpec == NULL
//     ippStsFftOrderErr      FFT order value is illegal
//     ippStsFFTFlagErr       flag has an illegal value
//     ippStsMemAllocErr      Memory allocation fails
*/
IPPAPI (IppStatus, ippiFFTInitAlloc_R_32f,
                   ( IppiFFTSpec_R_32f** pFFTSpec,
                     int orderX, int orderY, int flag, IppHintAlgorithm hint ))

/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippiFFTFree
//  Purpose:    Deallocates memory used by the FFT context structure
//  Parameters:
//     pFFTSpec  Pointer to the FFT context structure
//  Returns:
//     ippStsNoErr            No errors
//     ippStsNullPtrErr       pFFTSpec == NULL
//     ippStsContextMatchErr  Invalid context structure
*/
IPPAPI (IppStatus, ippiFFTFree_R_32f, ( IppiFFTSpec_R_32f*  pFFTSpec ))

/* /////////////////////////////////////////////////////////////////////////////
//                  FFT Buffer Size
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippiFFTGetBufSize
//  Purpose:    Computes the size of an external FFT work buffer (in bytes)
//  Parameters:
//     pFFTSpec  Pointer to the FFT context structure
//     pSize      Pointer to the size of the external buffer
//  Returns:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pFFTSpec == NULL or pSize == NULL
//     ippStsContextMatchErr  bad context identifier
*/
IPPAPI (IppStatus, ippiFFTGetBufSize_R_32f,
                   ( const IppiFFTSpec_R_32f* pFFTSpec, int* pSize ))

/* /////////////////////////////////////////////////////////////////////////////
//                  FFT Transforms
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippiFFTFwd, ippiFFTInv
//  Purpose:    Performs forward or inverse FFT of an image
//  Parameters:
//     pFFTSpec   Pointer to the FFT context structure
//     pSrc       Pointer to the source image
//     srcStep    Step through the source image
//     pDst       Pointer to the destination image
//     dstStep    Step through the destination image
//     pBuffer    Pointer to the external work buffer
//  Returns:
//     ippStsNoErr            No errors
//     ippStsNullPtrErr       pFFTSpec == NULL, or
//                            pSrc == NULL, or pDst == NULL
//     ippStsStepErr          srcStep or dstStep value is zero or negative
//     ippStsContextMatchErr  Invalid context structure
//     ippStsMemAllocErr      Memory allocation error
*/

IPPAPI (IppStatus, ippiFFTFwd_RToPack_32f_C1R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiFFTSpec_R_32f* pFFTSpec,
                     Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippiFFTFwd_RToPack_32f_C3R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiFFTSpec_R_32f* pFFTSpec,
                     Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippiFFTFwd_RToPack_32f_C4R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiFFTSpec_R_32f* pFFTSpec,
                     Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippiFFTFwd_RToPack_32f_AC4R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiFFTSpec_R_32f* pFFTSpec,
                     Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippiFFTInv_PackToR_32f_C1R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiFFTSpec_R_32f* pFFTSpec,
                     Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippiFFTInv_PackToR_32f_C3R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiFFTSpec_R_32f* pFFTSpec,
                     Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippiFFTInv_PackToR_32f_C4R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiFFTSpec_R_32f* pFFTSpec,
                     Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippiFFTInv_PackToR_32f_AC4R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiFFTSpec_R_32f* pFFTSpec,
                     Ipp8u* pBuffer ))

/* /////////////////////////////////////////////////////////////////////////////
//                  DFT Context Functions
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippiDFTInitAlloc
//  Purpose:      Creates and initializes the DFT context structure
//  Parameters:
//     roiSize    Size of the ROI
//     flag       Flag to choose the results normalization factors
//     hint       Option to select the algorithmic implementation of the transform
//                function
//     pDFTSpec   Pointer to the pointer to the DFT context structure
//  Returns:
//     ippStsNoErr            No errors
//     ippStsNullPtrErr       pDFTSpec == NULL
//     ippStsSizeErr          roiSize has a field with zero or negative value
//     ippStsFFTFlagErr       Illegal value of the flag
//     ippStsMemAllocErr      Memory allocation error
*/
IPPAPI (IppStatus, ippiDFTInitAlloc_R_32f,
                   ( IppiDFTSpec_R_32f** pDFTSpec,
                     IppiSize roiSize, int flag, IppHintAlgorithm hint ))

/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippiDFTFree
//  Purpose:    Deallocates memory used by the DFT context structure
//  Parameters:
//     pDFTSpec       Pointer to the DFT context structure
//  Returns:
//     ippStsNoErr            No errors
//     ippStsNullPtrErr       pDFTSpec == NULL
//     ippStsContextMatchErr  Invalid context structure
*/
IPPAPI (IppStatus, ippiDFTFree_R_32f, ( IppiDFTSpec_R_32f*  pDFTSpec ))

/* /////////////////////////////////////////////////////////////////////////////
//                  DFT Buffer Size
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippiDFTGetBufSize
//  Purpose:    Computes the size of the external DFT work buffer (in bytes)
//  Parameters:
//     pDFTSpec   Pointer to the DFT context structure
//     pSize      Pointer to the size of the buffer
//  Returns:
//     ippStsNoErr            no errors
//     ippStsNullPtrErr       pDFTSpec == NULL or pSize == NULL
//     ippStsContextMatchErr  Invalid context structure
*/
IPPAPI (IppStatus, ippiDFTGetBufSize_R_32f,
                   ( const IppiDFTSpec_R_32f* pDFTSpec, int* pSize ))

/* /////////////////////////////////////////////////////////////////////////////
//                  DFT Transforms
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippiDFTFwd, ippiDFTInv
//  Purpose:    Performs forward or inverse DFT of an image
//  Parameters:
//     pDFTSpec    Pointer to the DFT context structure
//     pSrc        Pointer to source image
//     srcStep     Step through the source image
//     pDst        Pointer to the destination image
//     dstStep     Step through the destination image
//     pBuffer     Pointer to the external work buffer
//  Returns:
//     ippStsNoErr            No errors
//     ippStsNullPtrErr       pDFTSpec == NULL, or
//                            pSrc == NULL, or pDst == NULL
//     ippStsStepErr          srcStep or dstStep value is zero or negative
//     ippStsContextMatchErr  Invalid context structure
//     ippStsMemAllocErr      Memory allocation error
*/
IPPAPI (IppStatus, ippiDFTFwd_RToPack_32f_C1R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiDFTSpec_R_32f* pDFTSpec,
                     Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippiDFTFwd_RToPack_32f_C3R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiDFTSpec_R_32f* pDFTSpec,
                     Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippiDFTFwd_RToPack_32f_C4R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiDFTSpec_R_32f* pDFTSpec,
                     Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippiDFTInv_PackToR_32f_C1R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiDFTSpec_R_32f* pDFTSpec,
                     Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippiDFTInv_PackToR_32f_C3R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiDFTSpec_R_32f* pDFTSpec,
                     Ipp8u* pBuffer ))
IPPAPI (IppStatus, ippiDFTInv_PackToR_32f_C4R,
                   ( const Ipp32f* pSrc, int srcStep,
                     Ipp32f* pDst, int dstStep,
                     const IppiDFTSpec_R_32f* pDFTSpec,
                     Ipp8u* pBuffer ))

/* /////////////////////////////////////////////////////////////////////////////
//                  DCT Transforms
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:      ippiDCT8x8Fwd_32f_C1, ippiDCT8x8Fwd_32f_C1I
//             ippiDCT8x8Inv_32f_C1, ippiDCT8x8Inv_32f_C1I
//  Purpose:   Performs forward or inverse DCT in the 8x8 buffer for 32f data
//
//  Parameters:
//     pSrc       Pointer to the source buffer
//     pDst       Pointer to the destination buffer
//     pSrcDst    Pointer to the source and destination buffer (in-place operations)
//  Returns:
//     ippStsNoErr         No errors
//     ippStsNullPtrErr    One of the pointers is NULL
*/

IPPAPI (IppStatus, ippiDCT8x8Fwd_32f_C1,  ( const Ipp32f* pSrc, Ipp32f* pDst ))
IPPAPI (IppStatus, ippiDCT8x8Inv_32f_C1,  ( const Ipp32f* pSrc, Ipp32f* pDst ))

IPPAPI (IppStatus, ippiDCT8x8Fwd_32f_C1I, ( Ipp32f* pSrcDst ))
IPPAPI (IppStatus, ippiDCT8x8Inv_32f_C1I, ( Ipp32f* pSrcDst ))

/* /////////////////////////////////////////////////////////////////////////////
//              Vector Multiplication of Images in RCPack2D Format
///////////////////////////////////////////////////////////////////////////// */
/*  Name:               ippiMulPack
//  Purpose:            Multiplies pixel values of two images in RCPack2D format
//  Returns:
//      ippStsNoErr       No errors
//      ippStsNullPtrErr  One of the pointers is NULL
//      ippStsStepErr     One of the step values is zero or negative
//      ippStsSizeErr     The roiSize has a field with negative or zero value
//  Parameters:
//      pSrc            Pointer to the source image for in-place operation
//      pSrcDst         Pointer to the source/destination image for in-place operation
//      srcStep         Step through the source image for in-place operation
//      srcDstStep      Step through the source/destination image for in-place operation
//      pSrc1           Pointer to the first source image
//      src1Step        Step through the first source image
//      pSrc2           Pointer to the second source image
//      src1Step        Step through the second source image
//      pDst            Pointer to the destination image
//      dstStep         Step through the destination image
//      roiSize         Size of the source and destination ROI
//      scaleFactor     Scale factor
*/
IPPAPI(IppStatus, ippiMulPack_32f_C1IR, (const Ipp32f* pSrc, int srcStep, Ipp32f* pSrcDst, int srcDstStep, IppiSize roiSize))
IPPAPI(IppStatus, ippiMulPack_32f_C3IR, (const Ipp32f* pSrc, int srcStep, Ipp32f* pSrcDst, int srcDstStep, IppiSize roiSize))
IPPAPI(IppStatus, ippiMulPack_32f_C4IR, (const Ipp32f* pSrc, int srcStep, Ipp32f* pSrcDst, int srcDstStep, IppiSize roiSize))
IPPAPI(IppStatus, ippiMulPack_32f_AC4IR, (const Ipp32f* pSrc, int srcStep, Ipp32f* pSrcDst, int srcDstStep, IppiSize roiSize))

/* ////////////////////////////////////////////////////////////////////////////
//  Names:      ippiFilter32f_16s_C1R
//              ippiFilter32f_16s_C3R
//              ippiFilter32f_16s_C4R
//              ippiFilter32f_16s_AC4R
//  Purpose:    Filters an image that consists of integer data with use of
//              the rectangular kernel of floating-point values.
//  Returns:
//   ippStsNoErr       OK
//   ippStsNullPtrErr  One of the pointers is NULL
//   ippStsSizeErr     dstRoiSize or kernelSize has a field with zero or negative value
//  Parameters:
//      pSrc            Pointer to the source buffer
//      srcStep         Step in bytes through the source image buffer
//      pDst            Pointer to the destination buffer
//      dstStep         Step in bytes through the destination image buffer
//      dstRoiSize      Size of the source and destination ROI in pixels
//      pKernel         Pointer to the kernel values ( 32f kernel )
//      kernelSize      Size of the rectangular kernel in pixels.
//      anchor          Anchor cell specifying the rectangular kernel alignment
//                      with respect to the position of the input pixel
*/
IPPAPI( IppStatus, ippiFilter32f_16s_C1R, ( const Ipp16s* pSrc, int srcStep,
        Ipp16s* pDst, int dstStep, IppiSize dstRoiSize, const Ipp32f* pKernel,
        IppiSize kernelSize, IppiPoint anchor ))
IPPAPI( IppStatus, ippiFilter32f_16s_C3R, ( const Ipp16s* pSrc, int srcStep,
        Ipp16s* pDst, int dstStep, IppiSize dstRoiSize, const Ipp32f* pKernel,
        IppiSize kernelSize, IppiPoint anchor ))
IPPAPI( IppStatus, ippiFilter32f_16s_C4R, ( const Ipp16s* pSrc, int srcStep,
        Ipp16s* pDst, int dstStep, IppiSize dstRoiSize, const Ipp32f* pKernel,
        IppiSize kernelSize, IppiPoint anchor ))
IPPAPI( IppStatus, ippiFilter32f_16s_AC4R, ( const Ipp16s* pSrc, int srcStep,
        Ipp16s* pDst, int dstStep, IppiSize dstRoiSize, const Ipp32f* pKernel,
        IppiSize kernelSize, IppiPoint anchor ))

/* ////////////////////////////////////////////////////////////////////////////
//  Names:      ippiFilterColumn32f_16s_C1R
//              ippiFilterColumn32f_16s_C3R
//              ippiFilterColumn32f_16s_C4R
//              ippiFilterColumn32f_16s_AC4R
//  Purpose:    Filters an image using a spatial 32f kernel consisting of a
//              single column
//  Returns:
//   ippStsNoErr       OK
//   ippStsNullPtrErr  Some of pointers to pSrc, pDst or pKernel are NULL
//   ippStsSizeErr     dstRoiSize has a field with zero or negative value, or
//                     kernelSize value is zero or negative
//  Parameters:
//      pSrc        Pointer to the source buffer
//      srcStep     Step in bytes through the source image buffer
//      pDst        Pointer to the destination buffer
//      dstStep     Step in bytes through the destination image buffer
//      dstRoiSize  Size of the source and destination ROI in pixels
//      pKernel     Pointer to the column kernel values ( 32f kernel )
//      kernelSize  Size of the column kernel in pixels.
//      yAnchor     Anchor cell specifying the kernel vertical alignment with
//                  respect to the position of the input pixel
*/
IPPAPI( IppStatus, ippiFilterColumn32f_16s_C1R, ( const Ipp16s* pSrc,
        int srcStep, Ipp16s* pDst, int dstStep, IppiSize dstRoiSize,
        const Ipp32f* pKernel, int kernelSize, int yAnchor ))
IPPAPI( IppStatus, ippiFilterColumn32f_16s_C3R, ( const Ipp16s* pSrc,
        int srcStep, Ipp16s* pDst, int dstStep, IppiSize dstRoiSize,
        const Ipp32f* pKernel, int kernelSize, int yAnchor ))
IPPAPI( IppStatus, ippiFilterColumn32f_16s_C4R, ( const Ipp16s* pSrc,
        int srcStep, Ipp16s* pDst, int dstStep, IppiSize dstRoiSize,
        const Ipp32f* pKernel, int kernelSize, int yAnchor ))
IPPAPI( IppStatus, ippiFilterColumn32f_16s_AC4R, ( const Ipp16s* pSrc,
        int srcStep, Ipp16s* pDst, int dstStep, IppiSize dstRoiSize,
        const Ipp32f* pKernel, int kernelSize, int yAnchor ))

/* ////////////////////////////////////////////////////////////////////////////
//  Names:      ippiFilterRow32f_16s_C1R
//              ippiFilterRow32f_16s_C3R
//              ippiFilterRow32f_16s_C4R
//              ippiFilterRow32f_16s_AC4R
//  Purpose:   Filters an image using a spatial 32f kernel consisting of a
//             single row
//  Returns:
//   ippStsNoErr       OK
//   ippStsNullPtrErr  One of the pointers is NULL
//   ippStsSizeErr     dstRoiSize has a field with zero or negative value, or
//                     kernelSize value is zero or negative
//  Parameters:
//      pSrc        Pointer to the source buffer;
//      srcStep     Step in bytes through the source image buffer;
//      pDst        Pointer to the destination buffer;
//      dstStep     Step in bytes through the destination image buffer;
//      dstRoiSize  Size of the source and destination ROI in pixels;
//      pKernel     Pointer to the row kernel values ( 32f kernel );
//      kernelSize  Size of the row kernel in pixels;
//      xAnchor     Anchor cell specifying the kernel horizontal alignment with
//                  respect to the position of the input pixel.
*/
IPPAPI( IppStatus, ippiFilterRow32f_16s_C1R, ( const Ipp16s* pSrc, int srcStep,
        Ipp16s* pDst, int dstStep, IppiSize dstRoiSize, const Ipp32f* pKernel,
        int kernelSize, int xAnchor ))
IPPAPI( IppStatus, ippiFilterRow32f_16s_C3R, ( const Ipp16s* pSrc, int srcStep,
        Ipp16s* pDst, int dstStep, IppiSize dstRoiSize, const Ipp32f* pKernel,
        int kernelSize, int xAnchor ))
IPPAPI( IppStatus, ippiFilterRow32f_16s_C4R, ( const Ipp16s* pSrc, int srcStep,
        Ipp16s* pDst, int dstStep, IppiSize dstRoiSize, const Ipp32f* pKernel,
        int kernelSize, int xAnchor ))
IPPAPI( IppStatus, ippiFilterRow32f_16s_AC4R, ( const Ipp16s* pSrc, int srcStep,
        Ipp16s* pDst, int dstStep, IppiSize dstRoiSize, const Ipp32f* pKernel,
        int kernelSize, int xAnchor ))

/* ////////////////////////////////////////////////////////////////////////////
//  Names:      ippiSqr_32f_C1IR
//              ippiSqr_32f_C3IR
//              ippiSqr_32f_AC4IR
//              ippiSqr_32f_C4IR
//  Purpose:    squares pixel values of an image and
//              places results in the destination image;
//              for in-place flavors - in  the same image
//  Returns:
//   ippStsNoErr       OK
//   ippStsNullPtrErr  One of the pointers is NULL
//   ippStsSizeErr     The roiSize has a field with negative or zero value
//  Parameters:
//   pSrc       pointer to the source image
//   srcStep    step through the source image
//   pDst       pointer to the destination image
//   dstStep    step through the destination image
//   pSrcDst    pointer to the source/destination image (for in-place function)
//   srcDstStep step through the source/destination image (for in-place function)
//   roiSize    size of the ROI
//   scaleFactor scale factor
*/
IPPAPI(IppStatus,ippiSqr_32f_C1IR, (Ipp32f* pSrcDst, int srcDstStep,
       IppiSize roiSize))
IPPAPI(IppStatus,ippiSqr_32f_C3IR, (Ipp32f* pSrcDst, int srcDstStep,
       IppiSize roiSize))
IPPAPI(IppStatus,ippiSqr_32f_C4IR,(Ipp32f* pSrcDst, int srcDstStep,
       IppiSize roiSize))
IPPAPI(IppStatus,ippiSqr_32f_AC4IR,(Ipp32f* pSrcDst, int srcDstStep,
       IppiSize roiSize))

/* ////////////////////////////////////////////////////////////////////////////
//  Names:      ippiSqrt_32f_C1IR
//              ippiSqrt_32f_C3IR
//              ippiSqrt_32f_AC4IR
//              ippiSqrt_32f_C4IR
//  Purpose:    computes square roots of pixel values of a source image and
//              places results in the destination image;
//              for in-place flavors - in the same image
//  Returns:
//   ippStsNoErr       OK
//   ippStsNullPtrErr  One of pointers is NULL
//   ippStsSizeErr     The roiSize has a field with negative or zero value
//   ippStsSqrtNegArg  Source image pixel has a negative value
//
//  Parameters:
//   pSrc       pointer to the source image
//   srcStep    step through the source image
//   pDst       pointer to the destination image
//   dstStep    step through the destination image
//   pSrcDst    pointer to the source/destination image (for in-place function)
//   srcDstStep step through the source/destination image (for in-place function)
//   roiSize    size of the ROI
//   scaleFactor scale factor
*/
IPPAPI(IppStatus,ippiSqrt_32f_C1IR, (Ipp32f* pSrcDst, int srcDstStep,
       IppiSize roiSize))
IPPAPI(IppStatus,ippiSqrt_32f_C3IR, (Ipp32f* pSrcDst, int srcDstStep,
       IppiSize roiSize))
IPPAPI(IppStatus,ippiSqrt_32f_C4IR,(Ipp32f* pSrcDst, int srcDstStep,
       IppiSize roiSize))
IPPAPI(IppStatus,ippiSqrt_32f_AC4IR,(Ipp32f* pSrcDst, int srcDstStep,
       IppiSize roiSize))

/* ////////////////////////////////////////////////////////////////////////////
//  Names:      ippiThreshold_LTVal_32f_C1IR
//              ippiThreshold_LTVal_32f_C3IR
//              ippiThreshold_LTVal_32f_C4IR
//              ippiThreshold_LTVal_32f_AC4IR
//  Purpose:  Performs thresholding of pixel values: pixels that are
//            less than threshold, are set to a specified value
//  Returns:
//   ippStsNoErr       OK
//   ippStsNullPtrErr  One of the pointers is NULL
//   ippStsSizeErr     roiSize has a field with zero or negative value
//   ippStsStepErr     One of the step values is zero or negative
//
//  Parameters:
//   pSrc       Pointer to the source image
//   srcStep    Step through the source image
//   pDst       Pointer to the destination image
//   dstStep    Step through the destination image
//   pSrcDst    Pointer to the source/destination image (in-place flavors)
//   srcDstStep Step through the source/destination image (in-place flavors)
//   roiSize    Size of the ROI
//   threshold  Threshold level value (array of values for multi-channel data)
//   value      The output value (array or values for multi-channel data)
*/
IPPAPI(IppStatus,ippiThreshold_LTVal_32f_C1IR,(Ipp32f* pSrcDst, int srcDstStep,
       IppiSize roiSize, Ipp32f threshold, Ipp32f value))
IPPAPI(IppStatus,ippiThreshold_LTVal_32f_C3IR,(Ipp32f* pSrcDst, int srcDstStep,
       IppiSize roiSize, const Ipp32f threshold[3], const Ipp32f value[3]))
IPPAPI(IppStatus,ippiThreshold_LTVal_32f_C4IR,(Ipp32f* pSrcDst, int srcDstStep,
       IppiSize roiSize, const Ipp32f threshold[4], const Ipp32f value[4]))
IPPAPI(IppStatus,ippiThreshold_LTVal_32f_AC4IR,(Ipp32f* pSrcDst, int srcDstStep,
       IppiSize roiSize, const Ipp32f threshold[3], const Ipp32f value[3]))
IPPAPI(IppStatus,ippiThreshold_LT_32f_C1IR,(Ipp32f* pSrcDst, int srcDstStep,
       IppiSize roiSize, Ipp32f threshold))


#ifdef __cplusplus
}
#endif

#endif /* _CORE */

#endif

#endif /* __DEPENDENCEIP_H__ */
