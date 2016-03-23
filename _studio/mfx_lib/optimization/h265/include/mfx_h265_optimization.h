//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#ifndef __MFX_H265_OPTIMIZATION_H__
#define __MFX_H265_OPTIMIZATION_H__

#include "ipps.h"
#include <immintrin.h>

#pragma warning(disable: 4127)

#if defined(_WIN32) || defined(_WIN64)
  #define H265_FORCEINLINE __forceinline
  #define H265_NONLINE __declspec(noinline)
  #define H265_FASTCALL __fastcall
  #define ALIGN_DECL(X) __declspec(align(X))
#else
  #define H265_FORCEINLINE __attribute__((always_inline))
  #define H265_NONLINE __attribute__((noinline))
  #define H265_FASTCALL
  #define ALIGN_DECL(X) __attribute__ ((aligned(X)))
#endif

#ifdef __INTEL_COMPILER
# define H265_RESTRICT __restrict
#elif defined _MSC_VER
# if _MSC_VER >= 1400
#  define H265_RESTRICT __restrict
# else
#  define H265_RESTRICT
# endif
#else
# define H265_RESTRICT
#endif

/* NOTE: In debug mode compiler attempts to load data with MOVNTDQA while data is
+only 8-byte aligned, but PMOVZX does not require 16-byte alignment.
+cannot substitute in SSSE3 emulation mode as PUNPCK used instead of PMOVZX requires alignment
+ICC14 fixed problems with reg-mem forms for PMOVZX/SX with _mm_loadl_epi64() intrinsic and broke old workaround */
#if (defined( NDEBUG ) && !defined(MFX_EMULATE_SSSE3)) && !(defined( __INTEL_COMPILER ) && (__INTEL_COMPILER >= 1400 ))
#define MM_LOAD_EPI64(x) (*(const __m128i*)(x))
#else
#define MM_LOAD_EPI64(x) _mm_loadl_epi64( (const __m128i*)(x) )
#endif

//=========================================================
//   configuration macros
//=========================================================
// [1] choose only one arch

#if defined(ANDROID)
    #ifndef __MFX_ANDROID_DEFS_H__
        # error "mfx_android_defs.h is not included"
    #endif
#else
    //#define MFX_TARGET_OPTIMIZATION_SSSE3
    //#define MFX_TARGET_OPTIMIZATION_SSE4
    //#define MFX_TARGET_OPTIMIZATION_AVX2
    //#define MFX_TARGET_OPTIMIZATION_PX // ref C or IPP based
    //#define MFX_TARGET_OPTIMIZATION_ATOM // BYT = ATOM + SSE4.2
    #define MFX_TARGET_OPTIMIZATION_AUTO // work via dispatcher. decoder/encoder must call InitDispatcher() firstly
#endif

// [2] to enable alternative interpolation optimization (Ken/Jon)
// IVB demonstrates ~ 15% perf incr _vs_ tmpl version.
#define OPT_INTERP_PMUL

//=========================================================

// data types shared btw decoder/encoder
namespace UMC_HEVC_DECODER
{
    // texture component type
    enum EnumTextType
    {
        TEXT_LUMA,            ///< luma
        TEXT_CHROMA,          ///< chroma (U+V)
        TEXT_CHROMA_U,        ///< chroma U
        TEXT_CHROMA_V,        ///< chroma V
        TEXT_ALL,             ///< Y+U+V
        TEXT_NONE = 15
    };
};

namespace MFX_HEVC_PP
{
    enum EnumAddAverageType
    {
        AVERAGE_NO = 0,
        AVERAGE_FROM_PIC,
        AVERAGE_FROM_BUF
    };

    enum EnumInterpType
    {
        INTERP_HOR = 0,
        INTERP_VER
    };

    struct H265EdgeData
    {
        Ipp8u deblockP  : 1;
        Ipp8u deblockQ  : 1;
        Ipp8s strength  : 3;

        Ipp8s qp;
        Ipp8s tcOffset;
        Ipp8s betaOffset;
    }; // sizeof - 4 bytes

    enum SGUBorderID
    {
        SGU_L = 0,
        SGU_T,
        SGU_R,
        SGU_B,
        SGU_TL,
        SGU_TR,
        SGU_BL,
        SGU_BR,
        NUM_SGU_BORDER
    };

    union CTBBorders
    {
        struct
        {
            Ipp8u m_left         : 1;
            Ipp8u m_top          : 1;
            Ipp8u m_right        : 1;
            Ipp8u m_bottom       : 1;
            Ipp8u m_top_left     : 1;
            Ipp8u m_top_right    : 1;
            Ipp8u m_bottom_left  : 1;
            Ipp8u m_bottom_right : 1;
        };
        Ipp8u data;
    };

    struct SaoCtuStatistics //data structure for SAO statistics
    {
        static const int MAX_NUM_SAO_CLASSES = 32;

        Ipp64s diff[MAX_NUM_SAO_CLASSES];
        Ipp64s count[MAX_NUM_SAO_CLASSES];

 //       SaoCtuStatistics(){}
 //       ~SaoCtuStatistics(){}

        void Reset()
        {
            memset(diff, 0, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
            memset(count, 0, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
        }

	//private:
 //       //const 
 //       SaoCtuStatistics& operator=(const SaoCtuStatistics& src)
 //       {
 //           ippsCopy_8u((Ipp8u*)src.diff,  (Ipp8u*)diff,  sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
 //           ippsCopy_8u((Ipp8u*)src.count, (Ipp8u*)count, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);

 //           return *this;
 //       }

 //       const SaoCtuStatistics& operator+= (const SaoCtuStatistics& src)
 //       {
 //           for(int i=0; i< MAX_NUM_SAO_CLASSES; i++)
 //           {
 //               diff[i] += src.diff[i];
 //               count[i] += src.count[i];
 //           }
 //           return *this;
 //       }

    };
    /* ******************************************************** */
    /*                    Data Types for Dispatcher             */
    /* ******************************************************** */

    // [PTR.Sad]
    #define SAD_PARAMETERS_LIST_SPECIAL const unsigned char *image,  const unsigned char *block, int img_stride
    #define SAD_PARAMETERS_LIST_GENERAL const unsigned char *image,  const unsigned char *block, int img_stride, int block_stride

    typedef  int (H265_FASTCALL *PTR_SAD_MxN_8u)(SAD_PARAMETERS_LIST_SPECIAL);
    typedef  int (H265_FASTCALL *PTR_SAD_MxN_General_8u)(SAD_PARAMETERS_LIST_GENERAL);

    typedef  void (H265_FASTCALL *PTR_SAD_MxN_x3_8u)(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, Ipp32s ref_stride, Ipp32s width, Ipp32s height, Ipp32s sads[3]);
    typedef  void (H265_FASTCALL *PTR_SAD_MxN_x4_8u)(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s width, Ipp32s height, Ipp32s sads[4]);

    typedef  int (H265_FASTCALL *PTR_SAD_MxN_general_16s)(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s ref_stride, Ipp32s width, Ipp32s height);
    typedef  int (H265_FASTCALL *PTR_SAD_MxN_16s)(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s width, Ipp32s height);

    // [PTR.SATD]
    typedef  Ipp32s (H265_FASTCALL *PTR_SATD_8u)     (const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep);
    typedef  void   (H265_FASTCALL *PTR_SATD_Pair_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair);

    typedef  Ipp32s (H265_FASTCALL *PTR_SATD_16u)     (const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep);
    typedef  void   (H265_FASTCALL *PTR_SATD_Pair_16u)(const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep, Ipp32s* satdPair);

    // [PTR.TransformInv]
    typedef void (* PTR_TransformInv_16sT) (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);

    // [PTR.TransformFwd]
    typedef void (H265_FASTCALL *PTR_TransformFwd_16s)(const short *H265_RESTRICT src, Ipp32s srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);

    // [PTR.QuantFwd]
    typedef void   (H265_FASTCALL *PTR_QuantFwd_16s)    (const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift);
    typedef Ipp32s (H265_FASTCALL *PTR_QuantFwd_SBH_16s)(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scale, int offset, int shift);
    typedef void   (H265_FASTCALL *PTR_Quant_zCost_16s) (const Ipp16s* pSrc, Ipp32u* qLevels, Ipp64s* zlCosts, Ipp32s len, Ipp32s qScale, Ipp32s qoffset, Ipp32s qbits, Ipp32s rdScale0);
    typedef void   (H265_FASTCALL *PTR_QuantInv_16s)    (const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift);

    // [PTR.deblocking]
    typedef Ipp32s (* PTR_FilterEdgeLuma_8u_I)(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir);
    typedef void (* PTR_FilterEdgeChromaInterleaved_8u_I)(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr);
    typedef void (* PTR_FilterEdgeChromaPlane_8u_I)(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp);

    typedef Ipp32s (* PTR_FilterEdgeLuma_16u_I)(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32u bit_depth);
    typedef void (* PTR_FilterEdgeChromaInterleaved_16u_I)(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr, Ipp32u bit_depth);
    typedef void (* PTR_FilterEdgeChromaPlane_16u_I)(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp, Ipp32u bit_depth);

    // [PTR.SAO]
    #define SAOCU_ORG_PARAMETERS_LIST Ipp8u* pRec, Ipp32s stride, Ipp32s saoType, Ipp8u* tmpL, Ipp8u* tmpU, Ipp32u maxCUWidth, Ipp32u maxCUHeight, \
            Ipp32s picWidth, Ipp32s picHeight, Ipp32s* pOffsetEo, Ipp8u* pOffsetBo, Ipp8u* pClipTable, Ipp32u CUPelX, Ipp32u CUPelY 

    #define SAOCU_PARAMETERS_LIST Ipp8u* pRec, Ipp32s stride, Ipp32s saoType, Ipp8u* tmpL, Ipp8u* tmpU, Ipp32u maxCUWidth, Ipp32u maxCUHeight, \
    Ipp32s picWidth, Ipp32s picHeight, Ipp32s* pOffsetEo, Ipp8u* pOffsetBo, Ipp8u* pClipTable, Ipp32u CUPelX, Ipp32u CUPelY, CTBBorders pbBorderAvail

    #define SAOCU_ORG_PARAMETERS_LIST_U16 Ipp16u* pRec, Ipp32s stride, Ipp32s saoType, Ipp16u* tmpL, Ipp16u* tmpU, Ipp32u maxCUWidth, Ipp32u maxCUHeight, \
            Ipp32s picWidth, Ipp32s picHeight, Ipp32s* pOffsetEo, Ipp16u* pOffsetBo, Ipp16u* pClipTable, Ipp32u CUPelX, Ipp32u CUPelY

    #define SAOCU_PARAMETERS_LIST_U16 Ipp16u* pRec, Ipp32s stride, Ipp32s saoType, Ipp16u* tmpL, Ipp16u* tmpU, Ipp32u maxCUWidth, Ipp32u maxCUHeight, \
    Ipp32s picWidth, Ipp32s picHeight, Ipp32s* pOffsetEo, Ipp16u* pOffsetBo, Ipp16u* pClipTable, Ipp32u CUPelX, Ipp32u CUPelY, CTBBorders pbBorderAvail

    typedef void (* PTR_ProcessSaoCuOrg_Luma_8u)( SAOCU_ORG_PARAMETERS_LIST );

    typedef void (* PTR_ProcessSaoCu_Luma_8u)( SAOCU_PARAMETERS_LIST );

    typedef void (* PTR_ProcessSaoCuOrg_Luma_16u)( SAOCU_ORG_PARAMETERS_LIST_U16 );

    typedef void (* PTR_ProcessSaoCu_Luma_16u)( SAOCU_PARAMETERS_LIST_U16 );

    // [PTR.SAO :: Encode primitivies]
    #define SAOCU_ENCODE_PARAMETERS_LIST int compIdx, const Ipp8u* recBlk, int recStride, const Ipp8u* orgBlk, int orgStride, int width, \
        int height, int shift,  const MFX_HEVC_PP::CTBBorders& borders, int numSaoModes, MFX_HEVC_PP::SaoCtuStatistics* statsDataTypes

    #define SAOCU_ENCODE_PARAMETERS_LIST_16U int compIdx, const Ipp16u* recBlk, int recStride, const Ipp16u* orgBlk, int orgStride, int width, \
        int height, int shift,  const MFX_HEVC_PP::CTBBorders& borders, int numSaoModes, MFX_HEVC_PP::SaoCtuStatistics* statsDataTypes

    #define SAOCU_ENCODE_PARAMETERS_LIST_CALL compIdx, recBlk, recStride, orgBlk, orgStride, width, \
        height, shift,  borders, numSaoModes,statsDataTypes

    #define SAOCU_ENCODE_PARAMETERS_LIST_16U_CALL compIdx, recBlk, recStride, orgBlk, orgStride, width, \
        height, shift,  borders, numSaoModes,statsDataTypes

    typedef void(* PTR_GetCtuStatistics_8u)( SAOCU_ENCODE_PARAMETERS_LIST );

    typedef void(* PTR_GetCtuStatistics_16u)( SAOCU_ENCODE_PARAMETERS_LIST_16U );

    // [PTR.IntraPredict]
    typedef void (* PTR_PredictIntra_Ang_8u)(
        Ipp32s mode,
        Ipp8u* PredPel,
        Ipp8u* pels,
        Ipp32s pitch,
        Ipp32s width);

    typedef void (* PTR_PredictIntra_Ang_All_8u)(
        Ipp8u* PredPel,
        Ipp8u* FiltPel,
        Ipp8u* pels,
        Ipp32s width);

    typedef void (* PTR_PredictIntra_Ang_16u)(
        Ipp32s mode,
        Ipp16u* PredPel,
        Ipp16u* pels,
        Ipp32s pitch,
        Ipp32s width);

    typedef void (* PTR_PredictIntra_Ang_All_16u)(
        Ipp16u* PredPel,
        Ipp16u* FiltPel,
        Ipp16u* pels,
        Ipp32s width,
        Ipp32s bitDepth);

    typedef void (* PTR_FilterPredictPels_8u)(Ipp8u* PredPel, Ipp32s width);
    typedef void (* PTR_FilterPredictPels_Bilinear_8u)(Ipp8u* pSrcDst, int width, int topLeft, int bottomLeft, int topRight);
    typedef void (* PTR_PredictIntra_Planar_8u)(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);
    typedef void (* PTR_PredictIntra_Planar_ChromaNV12_8u)(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);

    typedef void (* PTR_FilterPredictPels_16s)(Ipp16s* PredPel, Ipp32s width);
    typedef void (* PTR_FilterPredictPels_Bilinear_16s)(Ipp16s* pSrcDst, int width, int topLeft, int bottomLeft, int topRight);
    typedef void (* PTR_PredictIntra_Planar_16s)(Ipp16s* PredPel, Ipp16s* pels, Ipp32s pitch, Ipp32s width);

    typedef void (* PTR_AnalyzeGradient_8u)(const Ipp8u* src, Ipp32s pitch, Ipp16u* hist4, Ipp16u* hist8, Ipp32s width, Ipp32s height);
    typedef void (* PTR_AnalyzeGradient_16u)(const Ipp16u* src, Ipp32s pitch, Ipp32u* hist4, Ipp32u* hist8, Ipp32s width, Ipp32s height);

    typedef void (* PTR_ComputeRsCs_8u)(const Ipp8u *ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height);
    typedef void (* PTR_ComputeRsCs_16u)(const Ipp16u *ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height);
    typedef void (* PTR_AddClipNv12UV_8u)(Ipp8u *dstNv12, Ipp32s pitchDst, const Ipp8u *src1Nv12, Ipp32s pitchSrc1, 
                                            const Ipp16s *src2Yv12U, const Ipp16s *src2Yv12V, Ipp32s pitchSrc2, Ipp32s size);

    typedef Ipp32s (* PTR_DiffDc_8u) (const Ipp8u  *src, Ipp32s pitchSrc, const Ipp8u  *pred, Ipp32s pitchPred, Ipp32s width);
    typedef Ipp32s (* PTR_DiffDc_16u)(const Ipp16u *src, Ipp32s pitchSrc, const Ipp16u *pred, Ipp32s pitchPred, Ipp32s width);

    //-----------------------------------------------------
    // aya: approach from Ken/Jon
    // [PTR.Interpolation]
#define INTERP_S8_D16_PARAMETERS_LIST const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, \
        int width, int height, int shift, short offset

#define INTERP_S16_D16_PARAMETERS_LIST const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, \
        int width, int height, int shift, short offset

#define INTERP_AVG_NONE_PARAMETERS_LIST short *pSrc, unsigned int srcPitch, unsigned char *pDst, unsigned int dstPitch, int width, int height
#define INTERP_AVG_PIC_PARAMETERS_LIST  short *pSrc, unsigned int srcPitch, unsigned char *pAvg, unsigned int avgPitch, unsigned char *pDst, unsigned int dstPitch, int width, int height
#define INTERP_AVG_BUF_PARAMETERS_LIST  short *pSrc, unsigned int srcPitch, short *pAvg, unsigned int avgPitch, unsigned char *pDst, unsigned int dstPitch, int width, int height

#define INTERP_AVG_NONE_PARAMETERS_LIST_U16 short *pSrc, unsigned int srcPitch, Ipp16u *pDst, unsigned int dstPitch, int width, int height, unsigned bit_depth
#define INTERP_AVG_PIC_PARAMETERS_LIST_U16  short *pSrc, unsigned int srcPitch, Ipp16u *pAvg, unsigned int avgPitch, Ipp16u *pDst, unsigned int dstPitch, int width, int height, unsigned bit_depth
#define INTERP_AVG_BUF_PARAMETERS_LIST_U16  short *pSrc, unsigned int srcPitch, short *pAvg, unsigned int avgPitch, Ipp16u *pDst, unsigned int dstPitch, int width, int height, unsigned bit_depth

    typedef void (* PTR_Interp_s8_d16)(INTERP_S8_D16_PARAMETERS_LIST);
    typedef void (* PTR_Interp_s8_d16_Ext)(INTERP_S8_D16_PARAMETERS_LIST, int plane);
    typedef void (* PTR_Interp_s16_d16)(INTERP_S16_D16_PARAMETERS_LIST);
    typedef void (* PTR_Interp_s16_d16_Ext)(INTERP_S16_D16_PARAMETERS_LIST, int plane);
    typedef void (* PTR_InterpPack_d8)(const short*, int, unsigned char*, int, int, int, int);
    typedef void (* PTR_InterpPack_d16)(const short*, int, unsigned short*, int, int, int, int);
    
    // Convert w/ rshift
    typedef void (* PTR_ConvertShiftR)(const short*, int, unsigned char*, int, int, int, int);

    // average
    typedef void (* PTR_AverageModeN)(INTERP_AVG_NONE_PARAMETERS_LIST);
    typedef void (* PTR_AverageModeP)(INTERP_AVG_PIC_PARAMETERS_LIST);
    typedef void (* PTR_AverageModeB)(INTERP_AVG_BUF_PARAMETERS_LIST);

    typedef void (* PTR_AverageModeN_U16)(INTERP_AVG_NONE_PARAMETERS_LIST_U16);
    typedef void (* PTR_AverageModeP_U16)(INTERP_AVG_PIC_PARAMETERS_LIST_U16);
    typedef void (* PTR_AverageModeB_U16)(INTERP_AVG_BUF_PARAMETERS_LIST_U16);
    //-----------------------------------------------------

    // [PTR.WeightedPred]
    typedef void (* PTR_CopyWeighted_S16U8)(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round);
    typedef void (* PTR_CopyWeightedBidi_S16U8)(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round);
    typedef void (* PTR_CopyWeighted_S16U16)(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma);
    typedef void (* PTR_CopyWeightedBidi_S16U16)(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma);

    /* ******************************************************** */
    /*                    Interface Wrapper                     */
    /* ******************************************************** */
#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    #if defined(MFX_MAKENAME_PX)
        #define MAKE_NAME( func ) func ## _px
    #elif defined (MFX_MAKENAME_SSE4)
        #define MAKE_NAME( func ) func ## _sse
    #elif defined (MFX_MAKENAME_SSSE3)
        #define MAKE_NAME( func ) func ## _ssse3
    #elif defined (MFX_MAKENAME_AVX2)
        #define MAKE_NAME( func ) func ## _avx2
    #elif defined (MFX_MAKENAME_ATOM)
        #define MAKE_NAME( func ) func ## _atom
    #endif
#else
    #define MAKE_NAME( func ) func
#endif

#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    #define HEVCPP_API(type_ptr, type_ret, name, arg) type_ptr name
#else
    #define HEVCPP_API(type_ptr, type_ret, name, arg) type_ret name arg
#endif


    /* ******************************************************** */
    /*            Dispatcher Context (Interfaces)               */
    /* ******************************************************** */

    template <typename PixType>
    void CopySat(const Ipp16s *src, Ipp32s pitchSrc, PixType *dst, Ipp32s pitchDst, Ipp32s width, Ipp32s height, Ipp32s bitDepth);

#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    struct Dispatcher
    {
//#endif
        // [SAD.special]
        PTR_SAD_MxN_8u  h265_SAD_4x4_8u;
        PTR_SAD_MxN_8u  h265_SAD_4x8_8u;
        PTR_SAD_MxN_8u  h265_SAD_4x16_8u;

        PTR_SAD_MxN_8u  h265_SAD_8x4_8u;
        PTR_SAD_MxN_8u  h265_SAD_8x8_8u;
        PTR_SAD_MxN_8u  h265_SAD_8x16_8u;
        PTR_SAD_MxN_8u  h265_SAD_8x32_8u;

        PTR_SAD_MxN_8u  h265_SAD_12x16_8u;

        PTR_SAD_MxN_8u  h265_SAD_16x4_8u;
        PTR_SAD_MxN_8u  h265_SAD_16x8_8u;
        PTR_SAD_MxN_8u  h265_SAD_16x12_8u;
        PTR_SAD_MxN_8u  h265_SAD_16x16_8u;
        PTR_SAD_MxN_8u  h265_SAD_16x32_8u;
        PTR_SAD_MxN_8u  h265_SAD_16x64_8u;

        PTR_SAD_MxN_8u  h265_SAD_24x32_8u;

        PTR_SAD_MxN_8u  h265_SAD_32x8_8u;
        PTR_SAD_MxN_8u  h265_SAD_32x16_8u;
        PTR_SAD_MxN_8u  h265_SAD_32x24_8u;
        PTR_SAD_MxN_8u  h265_SAD_32x32_8u;
        PTR_SAD_MxN_8u  h265_SAD_32x64_8u;

        PTR_SAD_MxN_8u  h265_SAD_48x64_8u;

        PTR_SAD_MxN_8u  h265_SAD_64x16_8u;
        PTR_SAD_MxN_8u  h265_SAD_64x32_8u;
        PTR_SAD_MxN_8u  h265_SAD_64x48_8u;
        PTR_SAD_MxN_8u  h265_SAD_64x64_8u;

        // [SAD.general]
        PTR_SAD_MxN_General_8u  h265_SAD_4x4_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_4x8_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_4x16_general_8u;

        PTR_SAD_MxN_General_8u  h265_SAD_8x4_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_8x8_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_8x16_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_8x32_general_8u;

        PTR_SAD_MxN_General_8u  h265_SAD_12x16_general_8u;

        PTR_SAD_MxN_General_8u  h265_SAD_16x4_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_16x8_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_16x12_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_16x16_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_16x32_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_16x64_general_8u;

        PTR_SAD_MxN_General_8u  h265_SAD_24x32_general_8u;

        PTR_SAD_MxN_General_8u  h265_SAD_32x8_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_32x16_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_32x24_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_32x32_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_32x64_general_8u;

        PTR_SAD_MxN_General_8u  h265_SAD_48x64_general_8u;

        PTR_SAD_MxN_General_8u  h265_SAD_64x16_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_64x32_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_64x48_general_8u;
        PTR_SAD_MxN_General_8u  h265_SAD_64x64_general_8u;

        // [SAD.multiple] 
        PTR_SAD_MxN_x3_8u       h265_SAD_MxN_x3_8u;
        PTR_SAD_MxN_x4_8u       h265_SAD_MxN_x4_8u;

        PTR_SAD_MxN_general_16s h265_SAD_MxN_general_16s;
        PTR_SAD_MxN_16s         h265_SAD_MxN_16s;
#endif

        // [Transform Inv]
        HEVCPP_API( PTR_TransformInv_16sT, void, h265_DST4x4Inv_16sT, (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth) );
        HEVCPP_API( PTR_TransformInv_16sT, void, h265_DCT4x4Inv_16sT, (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth) );
        HEVCPP_API( PTR_TransformInv_16sT, void, h265_DCT8x8Inv_16sT, (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth) );
        HEVCPP_API( PTR_TransformInv_16sT, void, h265_DCT16x16Inv_16sT, (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth) );
        HEVCPP_API( PTR_TransformInv_16sT, void, h265_DCT32x32Inv_16sT, (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth) );

        // [transform.forward]
        HEVCPP_API( PTR_TransformFwd_16s, void H265_FASTCALL, h265_DST4x4Fwd_16s, (const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth) );
        HEVCPP_API( PTR_TransformFwd_16s, void H265_FASTCALL, h265_DCT4x4Fwd_16s, (const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth) );
        HEVCPP_API( PTR_TransformFwd_16s, void H265_FASTCALL, h265_DCT8x8Fwd_16s, (const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth) );
        HEVCPP_API( PTR_TransformFwd_16s, void H265_FASTCALL, h265_DCT16x16Fwd_16s, (const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth) );
        HEVCPP_API( PTR_TransformFwd_16s, void H265_FASTCALL, h265_DCT32x32Fwd_16s, (const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth) );
        
        // quantization
        HEVCPP_API( PTR_QuantFwd_16s,     void   H265_FASTCALL, h265_QuantFwd_16s,     (const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift) );
        HEVCPP_API( PTR_QuantFwd_SBH_16s, Ipp32s H265_FASTCALL, h265_QuantFwd_SBH_16s, (const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scale, int offset, int shift) );
        HEVCPP_API( PTR_Quant_zCost_16s,  void   H265_FASTCALL, h265_Quant_zCost_16s,  (const Ipp16s* pSrc, Ipp32u* qLevels, Ipp64s* zlCosts, Ipp32s len, Ipp32s qScale, Ipp32s qoffset, Ipp32s qbits, Ipp32s rdScale0) );
        HEVCPP_API( PTR_QuantInv_16s,     void   H265_FASTCALL, h265_QuantInv_16s,     (const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift) );
        

        // [deblocking]
        HEVCPP_API( PTR_FilterEdgeLuma_8u_I, Ipp32s, h265_FilterEdgeLuma_8u_I, (H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir) );
        HEVCPP_API( PTR_FilterEdgeChromaInterleaved_8u_I, void, h265_FilterEdgeChroma_Interleaved_8u_I, (H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr) );
        HEVCPP_API( PTR_FilterEdgeChromaPlane_8u_I, void, h265_FilterEdgeChroma_Plane_8u_I, (H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp) );

        HEVCPP_API( PTR_FilterEdgeLuma_16u_I, Ipp32s, h265_FilterEdgeLuma_16u_I, (H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32u bit_depth) );
        HEVCPP_API( PTR_FilterEdgeChromaInterleaved_16u_I, void, h265_FilterEdgeChroma_Interleaved_16u_I, (H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr, Ipp32u bit_depth) );
        HEVCPP_API( PTR_FilterEdgeChromaPlane_16u_I, void, h265_FilterEdgeChroma_Plane_16u_I, (H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp, Ipp32u bit_depth) );

        // [SAO]
        HEVCPP_API( PTR_ProcessSaoCuOrg_Luma_8u, void, h265_ProcessSaoCuOrg_Luma_8u, (SAOCU_ORG_PARAMETERS_LIST));
        HEVCPP_API( PTR_ProcessSaoCu_Luma_8u, void, h265_ProcessSaoCu_Luma_8u, (SAOCU_PARAMETERS_LIST));

        HEVCPP_API( PTR_GetCtuStatistics_8u, void, h265_GetCtuStatistics_8u, (SAOCU_ENCODE_PARAMETERS_LIST));
        HEVCPP_API( PTR_GetCtuStatistics_16u, void, h265_GetCtuStatistics_16u, (SAOCU_ENCODE_PARAMETERS_LIST_16U));

        HEVCPP_API( PTR_ProcessSaoCuOrg_Luma_16u, void, h265_ProcessSaoCuOrg_Luma_16u, (SAOCU_ORG_PARAMETERS_LIST_U16));
        HEVCPP_API( PTR_ProcessSaoCu_Luma_16u, void, h265_ProcessSaoCu_Luma_16u, (SAOCU_PARAMETERS_LIST_U16));

        // [INTRA Predict]
        HEVCPP_API( PTR_PredictIntra_Ang_8u, void, h265_PredictIntra_Ang_8u,
            (Ipp32s mode,
            Ipp8u* PredPel,
            Ipp8u* pels,
            Ipp32s pitch,
            Ipp32s width));
        HEVCPP_API( PTR_PredictIntra_Ang_All_8u, void, h265_PredictIntra_Ang_All_8u,
            (Ipp8u* PredPel,
            Ipp8u* FiltPel,
            Ipp8u* pels,
            Ipp32s width));
        HEVCPP_API( PTR_PredictIntra_Ang_8u, void, h265_PredictIntra_Ang_NoTranspose_8u,
            (Ipp32s mode,
            Ipp8u* PredPel,
            Ipp8u* pels,
            Ipp32s pitch,
            Ipp32s width));
        HEVCPP_API( PTR_PredictIntra_Ang_All_8u, void, h265_PredictIntra_Ang_All_Even_8u,
            (Ipp8u* PredPel,
            Ipp8u* FiltPel,
            Ipp8u* pels,
            Ipp32s width));

        HEVCPP_API( PTR_PredictIntra_Ang_16u, void, h265_PredictIntra_Ang_16u,
            (Ipp32s mode,
            Ipp16u* PredPel,
            Ipp16u* pels,
            Ipp32s pitch,
            Ipp32s width));
        HEVCPP_API( PTR_PredictIntra_Ang_16u, void, h265_PredictIntra_Ang_NoTranspose_16u,
            (Ipp32s mode,
            Ipp16u* PredPel,
            Ipp16u* pels,
            Ipp32s pitch,
            Ipp32s width));
        HEVCPP_API( PTR_PredictIntra_Ang_All_16u, void, h265_PredictIntra_Ang_All_16u,
            (Ipp16u* PredPel,
            Ipp16u* FiltPel,
            Ipp16u* pels,
            Ipp32s width,
            Ipp32s bitDepth));
        HEVCPP_API( PTR_PredictIntra_Ang_All_16u, void, h265_PredictIntra_Ang_All_Even_16u,
            (Ipp16u* PredPel,
            Ipp16u* FiltPel,
            Ipp16u* pels,
            Ipp32s width,
            Ipp32s bitDepth));

        HEVCPP_API( PTR_FilterPredictPels_8u,              void, h265_FilterPredictPels_8u,              (Ipp8u* PredPel, Ipp32s width) );
        HEVCPP_API( PTR_FilterPredictPels_Bilinear_8u,     void, h265_FilterPredictPels_Bilinear_8u,     (Ipp8u* pSrcDst, int width, int topLeft, int bottomLeft, int topRight) );
        HEVCPP_API( PTR_PredictIntra_Planar_8u,            void, h265_PredictIntra_Planar_8u,            (Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width) );
        HEVCPP_API( PTR_PredictIntra_Planar_ChromaNV12_8u, void, h265_PredictIntra_Planar_ChromaNV12_8u, (Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width) );

        HEVCPP_API( PTR_FilterPredictPels_16s,          void, h265_FilterPredictPels_16s,          (Ipp16s* PredPel, Ipp32s width) );
        HEVCPP_API( PTR_FilterPredictPels_Bilinear_16s, void, h265_FilterPredictPels_Bilinear_16s, (Ipp16s* pSrcDst, int width, int topLeft, int bottomLeft, int topRight) );
        HEVCPP_API( PTR_PredictIntra_Planar_16s,        void, h265_PredictIntra_Planar_16s,        (Ipp16s* PredPel, Ipp16s* pels, Ipp32s pitch, Ipp32s width) );
        
        HEVCPP_API( PTR_AnalyzeGradient_8u,  void, h265_AnalyzeGradient_8u,  (const Ipp8u *src, Ipp32s pitch, Ipp16u *hist4, Ipp16u *hist8, Ipp32s width, Ipp32s height) );
        HEVCPP_API( PTR_AnalyzeGradient_16u, void, h265_AnalyzeGradient_16u, (const Ipp16u *src, Ipp32s pitch, Ipp32u *hist4, Ipp32u *hist8, Ipp32s width, Ipp32s height) );

        HEVCPP_API( PTR_ComputeRsCs_8u,  void, h265_ComputeRsCs_8u,  (const Ipp8u *ySrc,  Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height) );
        HEVCPP_API( PTR_ComputeRsCs_16u, void, h265_ComputeRsCs_16u, (const Ipp16u *ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height) );
        HEVCPP_API( PTR_AddClipNv12UV_8u,void, h265_AddClipNv12UV_8u,(Ipp8u *dstNv12, Ipp32s pitchDst, const Ipp8u *src1Nv12, Ipp32s pitchSrc1, const CoeffsType *src2Yv12U, const CoeffsType *src2Yv12V, Ipp32s pitchSrc2, Ipp32s size) );

        HEVCPP_API( PTR_DiffDc_8u,  Ipp32s, h265_DiffDc_8u,  (const Ipp8u  *src, Ipp32s pitchSrc, const Ipp8u  *pred, Ipp32s pitchPred, Ipp32s width));
        HEVCPP_API( PTR_DiffDc_16u, Ipp32s, h265_DiffDc_16u, (const Ipp16u *src, Ipp32s pitchSrc, const Ipp16u *pred, Ipp32s pitchPred, Ipp32s width));

        // [Interpolation]
        HEVCPP_API( PTR_Interp_s8_d16, void, h265_InterpLuma_s8_d16_H,   ( INTERP_S8_D16_PARAMETERS_LIST));
        HEVCPP_API( PTR_Interp_s8_d16_Ext, void, h265_InterpChroma_s8_d16_H, ( INTERP_S8_D16_PARAMETERS_LIST, int plane));
        HEVCPP_API( PTR_Interp_s8_d16, void, h265_InterpLuma_s8_d16_V,   ( INTERP_S8_D16_PARAMETERS_LIST));
        HEVCPP_API( PTR_Interp_s8_d16, void, h265_InterpChroma_s8_d16_V, ( INTERP_S8_D16_PARAMETERS_LIST));
        HEVCPP_API( PTR_Interp_s16_d16, void, h265_InterpLuma_s16_d16_V,   ( INTERP_S16_D16_PARAMETERS_LIST));
        HEVCPP_API( PTR_Interp_s16_d16, void, h265_InterpChroma_s16_d16_V, ( INTERP_S16_D16_PARAMETERS_LIST));
        HEVCPP_API( PTR_Interp_s16_d16, void, h265_InterpLuma_s16_d16_H,   ( INTERP_S16_D16_PARAMETERS_LIST));
        HEVCPP_API( PTR_Interp_s16_d16_Ext, void, h265_InterpChroma_s16_d16_H, ( INTERP_S16_D16_PARAMETERS_LIST, int plane));
        // 4-tap filter -1,5,5,-1
        HEVCPP_API( PTR_Interp_s8_d16, void, h265_InterpLumaFast_s8_d16_H,   ( INTERP_S8_D16_PARAMETERS_LIST));
        HEVCPP_API( PTR_Interp_s8_d16, void, h265_InterpLumaFast_s8_d16_V,   ( INTERP_S8_D16_PARAMETERS_LIST));
        HEVCPP_API( PTR_Interp_s16_d16, void, h265_InterpLumaFast_s16_d16_V,   ( INTERP_S16_D16_PARAMETERS_LIST));
        HEVCPP_API( PTR_Interp_s16_d16, void, h265_InterpLumaFast_s16_d16_H,   ( INTERP_S16_D16_PARAMETERS_LIST));
        // pack intermediate interpolation pels s16 to u8/u16 [ dst = Saturate( (src + 32) >> 6, 0, (1 << bitDepth) - 1 ) ]
        HEVCPP_API( PTR_InterpPack_d8, void, h265_InterpLumaPack_d8, (const short*, int, unsigned char*, int, int, int, int));
        HEVCPP_API( PTR_InterpPack_d16, void, h265_InterpLumaPack_d16, (const short*, int, unsigned short*, int, int, int, int));

        // [Convert w/ rshift]
        HEVCPP_API( PTR_ConvertShiftR, void, h265_ConvertShiftR, (const short*, int, unsigned char*, int, int, int, int));

        HEVCPP_API( PTR_AverageModeN, void, h265_AverageModeN, (INTERP_AVG_NONE_PARAMETERS_LIST));
        HEVCPP_API( PTR_AverageModeP, void, h265_AverageModeP, (INTERP_AVG_PIC_PARAMETERS_LIST));
        HEVCPP_API( PTR_AverageModeB, void, h265_AverageModeB, (INTERP_AVG_BUF_PARAMETERS_LIST));

        HEVCPP_API( PTR_AverageModeN_U16, void, h265_AverageModeN_U16, (INTERP_AVG_NONE_PARAMETERS_LIST_U16));
        HEVCPP_API( PTR_AverageModeP_U16, void, h265_AverageModeP_U16, (INTERP_AVG_PIC_PARAMETERS_LIST_U16));
        HEVCPP_API( PTR_AverageModeB_U16, void, h265_AverageModeB_U16, (INTERP_AVG_BUF_PARAMETERS_LIST_U16));

        // [WeightedPred]
        HEVCPP_API( PTR_CopyWeighted_S16U8, void, h265_CopyWeighted_S16U8, (Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round) );
        HEVCPP_API( PTR_CopyWeightedBidi_S16U8, void, h265_CopyWeightedBidi_S16U8, (Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round) );
        HEVCPP_API( PTR_CopyWeighted_S16U16, void, h265_CopyWeighted_S16U16, (Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma) );
        HEVCPP_API( PTR_CopyWeightedBidi_S16U16, void, h265_CopyWeightedBidi_S16U16, (Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma) );

        Ipp32s (H265_FASTCALL * h265_SSE_8u) (const Ipp8u*,Ipp32s,const Ipp8u*,Ipp32s,Ipp32s,Ipp32s,Ipp32s);
        Ipp32s (H265_FASTCALL * h265_SSE_16u)(const Ipp16u*,Ipp32s,const Ipp16u*,Ipp32s,Ipp32s,Ipp32s,Ipp32s);

        void (H265_FASTCALL* h265_Diff_8u) (const Ipp8u*,Ipp32s,const Ipp8u*,Ipp32s,Ipp16s*,Ipp32s,Ipp32s);
        void (H265_FASTCALL* h265_Diff_16u)(const Ipp16u*,Ipp32s,const Ipp16u*,Ipp32s,Ipp16s*,Ipp32s,Ipp32s);
        void (H265_FASTCALL* h265_DiffNv12_8u) (const Ipp8u*,Ipp32s,const Ipp8u*,Ipp32s,Ipp16s*,Ipp32s,Ipp16s*,Ipp32s,Ipp32s,Ipp32s);
        void (H265_FASTCALL* h265_DiffNv12_16u)(const Ipp16u*,Ipp32s,const Ipp16u*,Ipp32s,Ipp16s*,Ipp32s,Ipp16s*,Ipp32s,Ipp32s,Ipp32s);

        void (H265_FASTCALL* h265_SplitChromaCtb_8u)(const Ipp8u*,Ipp32s,Ipp8u*,Ipp32s,Ipp8u*,Ipp32s,Ipp32s,Ipp32s);
        void (H265_FASTCALL* h265_SplitChromaCtb_16u)(const Ipp16u*,Ipp32s,Ipp16u*,Ipp32s,Ipp16u*,Ipp32s,Ipp32s,Ipp32s);

        void (H265_FASTCALL* h265_ImageDiffHistogram_8u)(Ipp8u*, Ipp8u*, Ipp32s, Ipp32s, Ipp32s, Ipp32s histogram[5], Ipp64s*, Ipp64s*);

        void (H265_FASTCALL* h265_SearchBestBlock8x8_8u)(Ipp8u*, Ipp8u*, Ipp32s, Ipp32s, Ipp32s, Ipp32u*, Ipp32s*, Ipp32s*);
        void (H265_FASTCALL* h265_ComputeRsCsDiff)(Ipp32f* pRs0, Ipp32f* pCs0, Ipp32f* pRs1, Ipp32f* pCs1, Ipp32s len, Ipp32f* pRsDiff, Ipp32f* pCsDiff);
        void (H265_FASTCALL* h265_ComputeRsCs4x4_8u)(const Ipp8u* pSrc, Ipp32s srcPitch, Ipp32s wblocks, Ipp32s hblocks, Ipp32f* pRs, Ipp32f* pCs);
        // [SATD]
        HEVCPP_API( PTR_SATD_8u, Ipp32s, h265_SATD_4x4_8u, (const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep));
        HEVCPP_API( PTR_SATD_8u, Ipp32s, h265_SATD_8x8_8u, (const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep));

        HEVCPP_API( PTR_SATD_Pair_8u, void, h265_SATD_4x4_Pair_8u, (const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair));
        HEVCPP_API( PTR_SATD_Pair_8u, void, h265_SATD_8x8_Pair_8u, (const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair));

        HEVCPP_API( PTR_SATD_16u, Ipp32s, h265_SATD_4x4_16u, (const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep));
        HEVCPP_API( PTR_SATD_16u, Ipp32s, h265_SATD_8x8_16u, (const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep));

        HEVCPP_API( PTR_SATD_Pair_16u, void, h265_SATD_4x4_Pair_16u, (const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep, Ipp32s* satdPair));
        HEVCPP_API( PTR_SATD_Pair_16u, void, h265_SATD_8x8_Pair_16u, (const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep, Ipp32s* satdPair));

#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
        bool           isInited;

    };
    extern Dispatcher g_dispatcher;
#endif

    int h265_SAD_MxN_special_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY);
    int h265_SAD_MxN_general_8u(const unsigned char *image,  int stride_img, const unsigned char *ref, int stride_ref, int SizeX, int SizeY);

    // LIST OF NON-OPTIMIZED (and NON-DISPATCHERED) FUNCTIONS:

    /* Inverse Quantization */
    void h265_QuantInv_ScaleList_LShift_16s(const Ipp16s* pSrc, const Ipp16s* pScaleList, Ipp16s* pDst, int len, int shift);
    void h265_QuantInv_ScaleList_RShift_16s(const Ipp16s* pSrc, const Ipp16s* pScaleList, Ipp16s* pDst, int len, int offset, int shift);

    /* Intra prediction (aya: encoder version, decoder under redesign yet) Luma is OK, Chroma - plane/interleave for encoder/decoder accordingly */
    void h265_PredictIntra_Ver_8u(
        Ipp8u* PredPel,
        Ipp8u* pels,
        Ipp32s pitch,
        Ipp32s width,
        Ipp32s bit_depth,
        Ipp32s isLuma);

    void h265_PredictIntra_Hor_8u(
        Ipp8u* PredPel,
        Ipp8u* pels,
        Ipp32s pitch,
        Ipp32s width,
        Ipp32s bit_depth,
        Ipp32s isLuma);

    void h265_PredictIntra_DC_8u(
        Ipp8u* PredPel,
        Ipp8u* pels,
        Ipp32s pitch,
        Ipp32s width,
        Ipp32s isLuma);

    void h265_PredictIntra_Planar_ChromaNV12_8u(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize);
    void h265_PredictIntra_DC_ChromaNV12_8u(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize);
    void h265_PredictIntra_Ang_ChromaNV12_8u(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize, Ipp32u dirMode);
    void h265_PredictIntra_Hor_ChromaNV12_8u(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize);
    void h265_PredictIntra_Ver_ChromaNV12_8u(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize);

    /* Predict Pels */
    void h265_GetPredPelsLuma_8u(Ipp8u* pSrc, Ipp8u* PredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf);
    void h265_GetPredPelsChromaNV12_8u(Ipp8u* pSrc, Ipp8u* pPredPel, Ipp8u isChroma422, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf);

    /* interpolation, version from Jon/Ken */
    void Interp_NoAvg(
        const unsigned char* pSrc, unsigned int srcPitch, 
        short *pDst, unsigned int dstPitch, 
        int tab_index, 
        int width, int height, 
        int shift, short offset, 
        int dir, int plane, int isFast);

    void Interp_NoAvg(
        const short* pSrc, unsigned int srcPitch, 
        short *pDst, unsigned int dstPitch, 
        int tab_index, 
        int width, int height, 
        int shift, short offset, 
        int dir, int plane, int isFast);

    void Interp_WithAvg(
        const unsigned char* pSrc, unsigned int srcPitch, 
        unsigned char *pDst, unsigned int dstPitch, 
        void *pvAvg, unsigned int avgPitch, 
        int avgMode, 
        int tab_index, 
        int width, int height, 
        int shift, short offset, 
        int dir, int plane, unsigned bit_depth, int isFast);

    void Interp_WithAvg(
        const short* pSrc, unsigned int srcPitch, 
        unsigned char *pDst, unsigned int dstPitch, 
        void *pvAvg, unsigned int avgPitch, 
        int avgMode, 
        int tab_index, 
        int width, int height, 
        int shift, short offset, 
        int dir, int plane, unsigned bit_depth, int isFast);

    /* ******************************************************** */
    /*                    MAKE_NAME                             */
    /* ******************************************************** */
#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    #define NAME(func) g_dispatcher.func
#else
    #define NAME(func) func
#endif

    enum { CPU_FEAT_AUTO = 0, CPU_FEAT_PX, CPU_FEAT_SSE4, CPU_FEAT_SSE4_ATOM, CPU_FEAT_SSSE3, CPU_FEAT_AVX2, CPU_FEAT_COUNT };

    IppStatus InitDispatcher(Ipp32s cpuFeature = CPU_FEAT_AUTO);

    /* ******************************************************** */
    /*                    INTERPOLATION TEMPLATE                */
    /* ******************************************************** */

    //=================================================================================================    
#ifndef OPT_INTERP_PMUL
    // general template for Interpolate kernel
    template
        < 
        typename     t_vec,
        UMC_HEVC_DECODER::EnumTextType c_plane_type,
        typename     t_src, 
        typename     t_dst,
        int        tab_index
        >
    class t_InterpKernel_intrin
    {
    public:
        static void func(
            t_dst* H265_RESTRICT pDst, 
            const t_src* pSrc, 
            Ipp32u  in_SrcPitch, // in samples
            Ipp32u  in_DstPitch, // in samples
            Ipp32u  width,
            Ipp32u  height,
            Ipp32u  accum_pitch,
            Ipp32u  shift,
            Ipp32u  offset,
            Ipp32s isFast,
            MFX_HEVC_PP::EnumAddAverageType eAddAverage = MFX_HEVC_PP::AVERAGE_NO,
            const  void* in_pSrc2 = NULL,
            Ipp32u in_Src2Pitch = 0 // in samples
            );
    };

    static Ipp32u s_cpuIdInfoRegs[4];
    static Ipp64u s_featuresMask;
    static int s_is_AVX2_available = ( ippGetCpuFeatures( &s_featuresMask, s_cpuIdInfoRegs), ((s_featuresMask & ippCPUID_AVX2) != 0) ); // means AVX2 + BMI_I + BMI_II to prevent issues with BMI
//    static int s_is_AVX2_available = _may_i_use_cpu_feature( _FEATURE_AVX2 | _FEATURE_BMI | _FEATURE_LZCNT | _FEATURE_MOVBE );
#endif // #ifndef OPT_INTERP_PMUL
    //=================================================================================================    

    template<typename T, bool value1>
    struct is_unsigned_base
    {
        static const bool value = value1;
    };

    template<typename T>
    struct is_unsigned
    {
        static const bool value = false;
    };

    template<>
    struct is_unsigned<unsigned char> : is_unsigned_base<unsigned char, true>
    {
    };

    template<>
    struct is_unsigned<unsigned short> : is_unsigned_base<unsigned short, true>
    {
    };

    template < UMC_HEVC_DECODER::EnumTextType plane_type, typename t_src, typename t_dst >
    void inline H265_FORCEINLINE Interpolate(
        MFX_HEVC_PP::EnumInterpType interp_type,
        const t_src* in_pSrc,
        Ipp32u in_SrcPitch, // in samples
        t_dst* in_pDst,
        Ipp32u in_DstPitch, // in samples
        Ipp32u tab_index,
        Ipp32u width,
        Ipp32u height,
        Ipp32u shift,
        Ipp16s offset,
        Ipp32u bit_depth,
        Ipp32s isFast,
        MFX_HEVC_PP::EnumAddAverageType eAddAverage = MFX_HEVC_PP::AVERAGE_NO,
        const void* in_pSrc2 = NULL,
        int    in_Src2Pitch = 0) // in samples
    {
        Ipp32s accum_pitch = ((interp_type == MFX_HEVC_PP::INTERP_HOR) ? (plane_type == UMC_HEVC_DECODER::TEXT_CHROMA ? 2 : 1) : in_SrcPitch);

        const t_src* pSrc = in_pSrc - ((( ( (plane_type == UMC_HEVC_DECODER::TEXT_LUMA) && (isFast == 0) ) ? 8 : 4) >> 1) - 1) * accum_pitch;

        width <<= int(plane_type == UMC_HEVC_DECODER::TEXT_CHROMA);


#if defined OPT_INTERP_PMUL

    if (is_unsigned<t_dst>::value)
    {
        Interp_WithAvg(pSrc, in_SrcPitch, in_pDst, in_DstPitch, (void *)in_pSrc2, in_Src2Pitch, eAddAverage, tab_index, width, height, shift, (short)offset, interp_type, plane_type, bit_depth, isFast);
    }
    else
        Interp_NoAvg(pSrc, in_SrcPitch, (short *)in_pDst, in_DstPitch, tab_index, width, height, shift, (short)offset, interp_type, plane_type, isFast);

#else
        typedef void (*t_disp_func)(
            t_dst* H265_RESTRICT pDst, 
            const t_src* pSrc, 
            Ipp32u  in_SrcPitch, // in samples
            Ipp32u  in_DstPitch, // in samples
            Ipp32u  width,
            Ipp32u  height,
            Ipp32u  accum_pitch,
            Ipp32u  shift,
            Ipp32u  offset,
            MFX_HEVC_PP::EnumAddAverageType eAddAverage,
            const void* in_pSrc2,
            Ipp32u in_Src2Pitch );

        static t_disp_func s_disp_tbl_luma_m128[ 3 ] = {
            &t_InterpKernel_intrin< __m128i, UMC_HEVC_DECODER::TEXT_LUMA, t_src, t_dst, 1 >::func,
            &t_InterpKernel_intrin< __m128i, UMC_HEVC_DECODER::TEXT_LUMA, t_src, t_dst, 2 >::func,
            &t_InterpKernel_intrin< __m128i, UMC_HEVC_DECODER::TEXT_LUMA, t_src, t_dst, 3 >::func
        };
        
        static t_disp_func s_disp_tbl_luma_m256[ 3 ] = {
            &t_InterpKernel_intrin< __m256i, UMC_HEVC_DECODER::TEXT_LUMA, t_src, t_dst, 1 >::func,
            &t_InterpKernel_intrin< __m256i, UMC_HEVC_DECODER::TEXT_LUMA, t_src, t_dst, 2 >::func,
            &t_InterpKernel_intrin< __m256i, UMC_HEVC_DECODER::TEXT_LUMA, t_src, t_dst, 3 >::func
        };
        
        static t_disp_func s_disp_tbl_chroma_m128[ 7 ] = {
            &t_InterpKernel_intrin< __m128i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 1 >::func,
            &t_InterpKernel_intrin< __m128i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 2 >::func,
            &t_InterpKernel_intrin< __m128i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 3 >::func,
            &t_InterpKernel_intrin< __m128i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 4 >::func,
            &t_InterpKernel_intrin< __m128i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 5 >::func,
            &t_InterpKernel_intrin< __m128i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 6 >::func,
            &t_InterpKernel_intrin< __m128i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 7 >::func
        };
        
        static t_disp_func s_disp_tbl_chroma_m256[ 7 ] = {
            &t_InterpKernel_intrin< __m256i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 1 >::func,
            &t_InterpKernel_intrin< __m256i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 2 >::func,
            &t_InterpKernel_intrin< __m256i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 3 >::func,
            &t_InterpKernel_intrin< __m256i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 4 >::func,
            &t_InterpKernel_intrin< __m256i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 5 >::func,
            &t_InterpKernel_intrin< __m256i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 6 >::func,
            &t_InterpKernel_intrin< __m256i, UMC_HEVC_DECODER::TEXT_CHROMA, t_src, t_dst, 7 >::func
        };

        if (s_is_AVX2_available && width > 8)
        {
            if (plane_type == UMC_HEVC_DECODER::TEXT_LUMA)
                (s_disp_tbl_luma_m256[tab_index - 1])( in_pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
            else // TEXT_CHROMA
                (s_disp_tbl_chroma_m256[tab_index - 1])( in_pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
        }
        else
        {
            if (plane_type == UMC_HEVC_DECODER::TEXT_LUMA)
                (s_disp_tbl_luma_m128[tab_index - 1])( in_pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
            else // TEXT_CHROMA
                (s_disp_tbl_chroma_m128[tab_index - 1])( in_pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
        }
#endif /* OPT_INTERP_PMUL */
    }


    void h265_PredictIntra_Ver_16u(
        Ipp16u* PredPel,
        Ipp16u* pels,
        Ipp32s pitch,
        Ipp32s width,
        Ipp32s bit_depth,
        Ipp32s isLuma);

    void h265_PredictIntra_Hor_16u(
        Ipp16u* PredPel,
        Ipp16u* pels,
        Ipp32s pitch,
        Ipp32s width,
        Ipp32s bit_depth,
        Ipp32s isLuma);

    void h265_PredictIntra_DC_16u(
        Ipp16u* PredPel,
        Ipp16u* pels,
        Ipp32s pitch,
        Ipp32s width,
        Ipp32s isLuma);

    void h265_PredictIntra_Planar_16u(
        Ipp16u* PredPel,
        Ipp16u* pels,
        Ipp32s pitch,
        Ipp32s width);

    void h265_PredictIntra_Ang_NoTranspose_16u_px(Ipp32s mode, Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width);

    void h265_PredictIntra_Planar_ChromaNV12_16u(Ipp16u* PredPel, Ipp16u* pDst, Ipp32s dstStride, Ipp32s blkSize);
    void h265_PredictIntra_DC_ChromaNV12_16u(Ipp16u* PredPel, Ipp16u* pDst, Ipp32s dstStride, Ipp32s blkSize);
    void h265_PredictIntra_Ang_ChromaNV12_16u(Ipp16u* PredPel, Ipp16u* pDst, Ipp32s dstStride, Ipp32s blkSize, Ipp32u dirMode);
    void h265_PredictIntra_Hor_ChromaNV12_16u(Ipp16u* PredPel, Ipp16u* pDst, Ipp32s dstStride, Ipp32s blkSize);
    void h265_PredictIntra_Ver_ChromaNV12_16u(Ipp16u* PredPel, Ipp16u* pDst, Ipp32s dstStride, Ipp32s blkSize);

    void h265_FilterPredictPels_16u(Ipp16u* PredPel, Ipp32s width);
    void h265_FilterPredictPels_Bilinear_16u(Ipp16u* pSrcDst, int width, int topLeft, int bottomLeft, int topRight);

    void h265_GetPredPelsLuma_16u(Ipp16u* pSrc, Ipp16u* PredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf, Ipp32u bit_depth);
    void h265_GetPredPelsChromaNV12_16u(Ipp16u* pSrc, Ipp16u* pPredPel, Ipp8u isChroma422, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf, Ipp32u bit_depth);

    void h265_GetCtuStatistics_16u_px(SAOCU_ENCODE_PARAMETERS_LIST_16U);

    /* interpolation, version from Jon/Ken */
    void Interp_NoAvg(
        const Ipp16u* pSrc, unsigned int srcPitch, 
        short *pDst,  unsigned int dstPitch, 
        int tab_index, 
        int width, int height, 
        int shift, short offset, 
        int dir, int plane, int isFast);

    void Interp_WithAvg(
        const Ipp16u* pSrc,  unsigned int srcPitch, 
        Ipp16u *pDst,  unsigned int dstPitch, 
        void *pvAvg,  unsigned int avgPitch, 
        int avgMode, 
        int tab_index, 
        int width, int height, 
        int shift,  short offset, 
        int dir,  int plane, unsigned bit_depth, int isFast);

    void Interp_WithAvg(
        const short* pSrc, unsigned int srcPitch, 
        Ipp16u *pDst,unsigned int dstPitch, 
        void *pvAvg, unsigned int avgPitch, 
        int avgMode, 
        int tab_index, 
        int width, int height, 
        int shift, short offset, 
        int dir, int plane, unsigned bit_depth, int isFast);

    inline void Interp_WithAvg(
        const short* , unsigned int , 
        short *, unsigned int , 
        void *, unsigned int , 
        int , 
        int , 
        int , int , 
        int , short , 
        int , int , unsigned, int )
    {
        // fake function
    }

    inline void Interp_WithAvg(
        const Ipp8u* , unsigned int , 
        short *, unsigned int , 
        void *, unsigned int , 
        int , 
        int , 
        int , int , 
        int , short , 
        int , int , unsigned, int )
    {
        // fake function
    }

    inline void Interp_WithAvg(
        const Ipp16u* , unsigned int , 
        short *, unsigned int , 
        void *, unsigned int , 
        int , 
        int , 
        int , int , 
        int , short , 
        int , int , unsigned, int )
    {
        // fake function
    }

    void h265_AverageModeN_px(short *pSrc, unsigned int srcPitch, Ipp16u *pDst, unsigned int dstPitch, int width, int height, unsigned bit_depth);

    void h265_AverageModeP_px(short *pSrc, unsigned int srcPitch, Ipp16u *pAvg, unsigned int avgPitch, Ipp16u *pDst, unsigned int dstPitch, int width, int height, unsigned bit_depth);

    void h265_AverageModeB_px(short *pSrc, unsigned int srcPitch, short *pAvg, unsigned int avgPitch, Ipp16u *pDst, unsigned int dstPitch, int width, int height, unsigned bit_depth);

    inline void h265_ProcessSaoCuOrg_Luma(Ipp8u* pRec, Ipp32s stride, Ipp32s saoType, Ipp8u* tmpL, Ipp8u* tmpU, Ipp32u maxCUWidth, Ipp32u maxCUHeight,
            Ipp32s picWidth, Ipp32s picHeight, Ipp32s* pOffsetEo, Ipp8u* pOffsetBo, Ipp8u* pClipTable, Ipp32u CUPelX, Ipp32u CUPelY)
    {
        MFX_HEVC_PP::NAME(h265_ProcessSaoCuOrg_Luma_8u)(pRec, stride, saoType, tmpL, tmpU, maxCUWidth, maxCUHeight,
            picWidth, picHeight, pOffsetEo, pOffsetBo, pClipTable, CUPelX, CUPelY);
    }

    inline void h265_ProcessSaoCuOrg_Luma(Ipp16u* pRec, Ipp32s stride, Ipp32s saoType, Ipp16u* tmpL, Ipp16u* tmpU, Ipp32u maxCUWidth, Ipp32u maxCUHeight,
            Ipp32s picWidth, Ipp32s picHeight, Ipp32s* pOffsetEo, Ipp16u* pOffsetBo, Ipp16u* pClipTable, Ipp32u CUPelX, Ipp32u CUPelY)
    {
        MFX_HEVC_PP::NAME(h265_ProcessSaoCuOrg_Luma_16u)(pRec, stride, saoType, tmpL, tmpU, maxCUWidth, maxCUHeight,
            picWidth, picHeight, pOffsetEo, pOffsetBo, pClipTable, CUPelX, CUPelY);
    }

    inline void h265_ProcessSaoCu_Luma(Ipp8u* pRec, Ipp32s stride, Ipp32s saoType, Ipp8u* tmpL, Ipp8u* tmpU, Ipp32u maxCUWidth, Ipp32u maxCUHeight,
        Ipp32s picWidth, Ipp32s picHeight, Ipp32s* pOffsetEo, Ipp8u* pOffsetBo, Ipp8u* pClipTable, Ipp32u CUPelX, Ipp32u CUPelY, CTBBorders pbBorderAvail)
    {
        MFX_HEVC_PP::NAME(h265_ProcessSaoCu_Luma_8u)(pRec, stride, saoType, tmpL, tmpU, maxCUWidth, maxCUHeight,
            picWidth, picHeight, pOffsetEo, pOffsetBo, pClipTable, CUPelX, CUPelY, pbBorderAvail);
    }

    inline void h265_ProcessSaoCu_Luma(Ipp16u* pRec, Ipp32s stride, Ipp32s saoType, Ipp16u* tmpL, Ipp16u* tmpU, Ipp32u maxCUWidth, Ipp32u maxCUHeight,
        Ipp32s picWidth, Ipp32s picHeight, Ipp32s* pOffsetEo, Ipp16u* pOffsetBo, Ipp16u* pClipTable, Ipp32u CUPelX, Ipp32u CUPelY, CTBBorders pbBorderAvail)
    {
        MFX_HEVC_PP::NAME(h265_ProcessSaoCu_Luma_16u)(pRec, stride, saoType, tmpL, tmpU, maxCUWidth, maxCUHeight,
            picWidth, picHeight, pOffsetEo, pOffsetBo, pClipTable, CUPelX, CUPelY, pbBorderAvail);
    }

    static inline Ipp32s h265_FilterEdgeLuma_I(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32u/* bit_depth*/)
    {
        return MFX_HEVC_PP::NAME(h265_FilterEdgeLuma_8u_I)(edge, srcDst, srcDstStride, dir);
    }

    static inline Ipp32s h265_FilterEdgeLuma_I(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32u bit_depth)
    {
        return MFX_HEVC_PP::NAME(h265_FilterEdgeLuma_16u_I)(edge, srcDst, srcDstStride, dir, bit_depth);
    }

    static inline void h265_FilterEdgeChroma_Interleaved_I(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr, Ipp32u/* bit_depth*/)
    {
        MFX_HEVC_PP::NAME(h265_FilterEdgeChroma_Interleaved_8u_I)(edge, srcDst, srcDstStride, dir, chromaQpCb, chromaQpCr);
    }

    static inline void h265_FilterEdgeChroma_Interleaved_I(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr, Ipp32u bit_depth)
    {
        MFX_HEVC_PP::NAME(h265_FilterEdgeChroma_Interleaved_16u_I)(edge, srcDst, srcDstStride, dir, chromaQpCb, chromaQpCr, bit_depth);
    }

    static inline void h265_FilterEdgeChroma_Plane_I(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp, Ipp32u/* bit_depth*/)
    {
        MFX_HEVC_PP::NAME(h265_FilterEdgeChroma_Plane_8u_I)(edge, srcDst, srcDstStride, dir, chromaQp);
    }

    static inline void h265_FilterEdgeChroma_Plane_I(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp, Ipp32u bit_depth)
    {
        MFX_HEVC_PP::NAME(h265_FilterEdgeChroma_Plane_16u_I)(edge, srcDst, srcDstStride, dir, chromaQp, bit_depth);
    }

    inline void h265_GetPredPelsLuma(Ipp16u* pSrc, Ipp16u* PredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf, Ipp32u bit_depth)
    {
        h265_GetPredPelsLuma_16u(pSrc, PredPel, blkSize, srcPitch, tpIf, lfIf, tlIf, bit_depth);
    }

    inline void h265_GetPredPelsLuma(Ipp8u* pSrc, Ipp8u* PredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf, Ipp32u )
    {
        h265_GetPredPelsLuma_8u(pSrc, PredPel, blkSize, srcPitch, tpIf, lfIf, tlIf);
    }

    inline void h265_GetPredPelsChromaNV12(Ipp16u* pSrc, Ipp16u* PredPel, Ipp8u isChroma422, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf, Ipp32u bit_depth)
    {
        h265_GetPredPelsChromaNV12_16u(pSrc, PredPel, isChroma422, blkSize, srcPitch, tpIf, lfIf, tlIf, bit_depth);
    }

    inline void h265_GetPredPelsChromaNV12(Ipp8u* pSrc, Ipp8u* PredPel, Ipp8u isChroma422, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf, Ipp32u )
    {
        h265_GetPredPelsChromaNV12_8u(pSrc, PredPel, isChroma422, blkSize, srcPitch, tpIf, lfIf, tlIf);
    }

    static inline void h265_FilterPredictPels(Ipp8u* PredPel, Ipp32s width)
    {
        MFX_HEVC_PP::NAME(h265_FilterPredictPels_8u)(PredPel, width);
    }

    static inline void h265_FilterPredictPels(Ipp16u* PredPel, Ipp32s width)
    {
        MFX_HEVC_PP::NAME(h265_FilterPredictPels_16s)((Ipp16s*)PredPel, width);
    }

    static inline void h265_FilterPredictPels_Bilinear(Ipp8u* pSrcDst, int width, int topLeft, int bottomLeft, int topRight)
    {
        MFX_HEVC_PP::NAME(h265_FilterPredictPels_Bilinear_8u)(pSrcDst, width, topLeft, bottomLeft, topRight);
    }

    static inline void h265_FilterPredictPels_Bilinear(Ipp16u* pSrcDst, int width, int topLeft, int bottomLeft, int topRight)
    {
        MFX_HEVC_PP::NAME(h265_FilterPredictPels_Bilinear_16s)((Ipp16s *)pSrcDst, width, topLeft, bottomLeft, topRight);
    }

    static inline void h265_PredictIntra_Planar(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width)
    {
        MFX_HEVC_PP::NAME(h265_PredictIntra_Planar_8u)(PredPel, pels, pitch, width);
    }

    static inline void h265_PredictIntra_Planar(Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width)
    {
        MFX_HEVC_PP::NAME(h265_PredictIntra_Planar_16s)((Ipp16s *)PredPel, (Ipp16s *)pels, pitch, width);
    }

    static inline void h265_PredictIntra_Ang(Ipp32s mode, Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width)
    {
        MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_8u)(mode, PredPel, pels, pitch, width);
    }

    static inline void h265_PredictIntra_Ang(Ipp32s mode, Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width)
    {
        MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_16u)(mode, PredPel, pels, pitch, width);
    }
        
    static inline void h265_PredictIntra_DC(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width, Ipp32s isLuma)
    {
        h265_PredictIntra_DC_8u(PredPel, pels, pitch, width, isLuma);
    }

    static inline void h265_PredictIntra_DC(Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width, Ipp32s isLuma)
    {
        h265_PredictIntra_DC_16u(PredPel, pels, pitch, width, isLuma);
    }

    static inline void h265_PredictIntra_Ver(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width, Ipp32s bit_depth, Ipp32s isLuma)
    {
        h265_PredictIntra_Ver_8u(PredPel, pels, pitch, width, bit_depth, isLuma);
    }

    static inline void h265_PredictIntra_Ver(Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width, Ipp32s bit_depth, Ipp32s isLuma)
    {
        h265_PredictIntra_Ver_16u(PredPel, pels, pitch, width, bit_depth, isLuma);
    }

    static inline void h265_PredictIntra_Hor(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width, Ipp32s bit_depth, Ipp32s isLuma)
    {
        h265_PredictIntra_Hor_8u(PredPel, pels, pitch, width, bit_depth, isLuma);
    }

    static inline void h265_PredictIntra_Hor(Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width, Ipp32s bit_depth, Ipp32s isLuma)
    {
        h265_PredictIntra_Hor_16u(PredPel, pels, pitch, width, bit_depth, isLuma);
    }

    static inline void h265_PredictIntra_Planar_ChromaNV12(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize)
    {
        MFX_HEVC_PP::NAME(h265_PredictIntra_Planar_ChromaNV12_8u)(PredPel, pDst, dstStride, blkSize);
    }

    static inline void h265_PredictIntra_Planar_ChromaNV12(Ipp16u* PredPel, Ipp16u* pDst, Ipp32s dstStride, Ipp32s blkSize)
    {
        h265_PredictIntra_Planar_ChromaNV12_16u(PredPel, pDst, dstStride, blkSize);
    }

    static inline void h265_PredictIntra_DC_ChromaNV12(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize)
    {
        h265_PredictIntra_DC_ChromaNV12_8u(PredPel, pDst, dstStride, blkSize);
    }

    static inline void h265_PredictIntra_DC_ChromaNV12(Ipp16u* PredPel, Ipp16u* pDst, Ipp32s dstStride, Ipp32s blkSize)
    {
        h265_PredictIntra_DC_ChromaNV12_16u(PredPel, pDst, dstStride, blkSize);
    }

    static inline void h265_PredictIntra_Ang_ChromaNV12(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize, Ipp32u dirMode)
    {
        h265_PredictIntra_Ang_ChromaNV12_8u(PredPel, pDst, dstStride, blkSize, dirMode);
    }

    static inline void h265_PredictIntra_Ang_ChromaNV12(Ipp16u* PredPel, Ipp16u* pDst, Ipp32s dstStride, Ipp32s blkSize, Ipp32u dirMode)
    {
        h265_PredictIntra_Ang_ChromaNV12_16u(PredPel, pDst, dstStride, blkSize, dirMode);
    }

    static inline void h265_PredictIntra_Hor_ChromaNV12(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize)
    {
        h265_PredictIntra_Hor_ChromaNV12_8u(PredPel, pDst, dstStride, blkSize);
    }

    static inline void h265_PredictIntra_Hor_ChromaNV12(Ipp16u* PredPel, Ipp16u* pDst, Ipp32s dstStride, Ipp32s blkSize)
    {
        h265_PredictIntra_Hor_ChromaNV12_16u(PredPel, pDst, dstStride, blkSize);
    }

    static inline void h265_PredictIntra_Ver_ChromaNV12(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize)
    {
        h265_PredictIntra_Ver_ChromaNV12_8u(PredPel, pDst, dstStride, blkSize);
    }

    static inline void h265_PredictIntra_Ver_ChromaNV12(Ipp16u* PredPel, Ipp16u* pDst, Ipp32s dstStride, Ipp32s blkSize)
    {
        h265_PredictIntra_Ver_ChromaNV12_16u(PredPel, pDst, dstStride, blkSize);
    }

    static inline void h265_PredictIntra_Ang_All(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels, Ipp32s width, Ipp32u/* bit_depth*/)
    {
        MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_All_8u)(PredPel, FiltPel, pels, width);
    }

    static inline void h265_PredictIntra_Ang_All(Ipp16u* PredPel, Ipp16u* FiltPel, Ipp16u* pels, Ipp32s width, Ipp32u bit_depth)
    {
        MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_All_16u)(PredPel, FiltPel, pels, width, bit_depth);
    }

    static inline void h265_PredictIntra_Ang_NoTranspose(Ipp32s mode, Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width)
    {
        MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_NoTranspose_8u)(mode, PredPel, pels, pitch, width);
    }

    static inline void h265_PredictIntra_Ang_NoTranspose(Ipp32s mode, Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width)
    {
        MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_NoTranspose_16u)(mode, PredPel, pels, pitch, width);
    }

    static inline void h265_PredictIntra_Ang_All_Even(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels, Ipp32s width, Ipp32s /*bitDepth*/)
    {
        MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_All_Even_8u)(PredPel, FiltPel, pels, width);
    }

    static inline void h265_PredictIntra_Ang_All_Even(Ipp16u* PredPel, Ipp16u* FiltPel, Ipp16u* pels, Ipp32s width, Ipp32s bitDepth)
    {
        MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_All_Even_16u)(PredPel, FiltPel, pels, width, bitDepth);
    }

    static inline void h265_AnalyzeGradient(const Ipp8u *src, Ipp32s pitch, Ipp16u *hist4, Ipp16u *hist8, Ipp32s width, Ipp32s height)
    {
        MFX_HEVC_PP::NAME(h265_AnalyzeGradient_8u)(src, pitch, hist4, hist8, width, height);
    }

    static inline void h265_AnalyzeGradient(const Ipp16u *src, Ipp32s pitch, Ipp32u *hist4, Ipp32u *hist8, Ipp32s width, Ipp32s height)
    {
        MFX_HEVC_PP::NAME(h265_AnalyzeGradient_16u)(src, pitch, hist4, hist8, width, height);
    }

    static inline void h265_ComputeRsCs(const Ipp8u *ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height) 
    {
        MFX_HEVC_PP::NAME(h265_ComputeRsCs_8u)(ySrc, pitchSrc, lcuRs, lcuCs, pitchRsCs, width, height);
    }

    static inline void h265_ComputeRsCs(const Ipp16u *ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height) 
    {
        MFX_HEVC_PP::NAME(h265_ComputeRsCs_16u)(ySrc, pitchSrc, lcuRs, lcuCs, pitchRsCs, width, height);
    }

    static inline void h265_ComputeRsCsDiff(Ipp32f* pRs0, Ipp32f* pCs0, Ipp32f* pRs1, Ipp32f* pCs1, Ipp32s len, Ipp32f* pRsDiff, Ipp32f* pCsDiff) 
    {
        MFX_HEVC_PP::NAME(h265_ComputeRsCsDiff)(pRs0, pCs0, pRs1, pCs1, len, pRsDiff, pCsDiff);
    }
    static inline void h265_GetCtuStatistics(SAOCU_ENCODE_PARAMETERS_LIST)
    {
        MFX_HEVC_PP::NAME(h265_GetCtuStatistics_8u)(SAOCU_ENCODE_PARAMETERS_LIST_CALL);
    }

    static inline void h265_GetCtuStatistics(SAOCU_ENCODE_PARAMETERS_LIST_16U)
    {
        //h265_GetCtuStatistics_16u_px(SAOCU_ENCODE_PARAMETERS_LIST_CALL);
        MFX_HEVC_PP::NAME(h265_GetCtuStatistics_16u)(SAOCU_ENCODE_PARAMETERS_LIST_16U_CALL);
    }

    static inline void h265_InterpLumaPack(const Ipp16s *src, Ipp32s pitchSrc, Ipp8u *dst, Ipp32s pitchDst, Ipp32s width, Ipp32s height, Ipp32s bitDepth)
    {
        MFX_HEVC_PP::NAME(h265_InterpLumaPack_d8)(src, pitchSrc, dst, pitchDst, width, height, bitDepth);
    }

    static inline void h265_InterpLumaPack(const Ipp16s *src, Ipp32s pitchSrc, Ipp16u *dst, Ipp32s pitchDst, Ipp32s width, Ipp32s height, Ipp32s bitDepth)
    {
        MFX_HEVC_PP::NAME(h265_InterpLumaPack_d16)(src, pitchSrc, dst, pitchDst, width, height, bitDepth);
    }

    static inline void h265_ConvertShiftR(const Ipp16s *src, Ipp32s pitchSrc, Ipp8u *dst, Ipp32s pitchDst, Ipp32s width, Ipp32s height, Ipp32s rshift)
    {
        MFX_HEVC_PP::NAME(h265_ConvertShiftR)(src, pitchSrc, dst, pitchDst, width, height, rshift);
    }

    static inline Ipp32s h265_Sse(const Ipp8u *src1, Ipp32s pitchSrc1, const Ipp8u *src2, Ipp32s pitchSrc2, Ipp32s width, Ipp32s height, Ipp32s shift)
    {
        return MFX_HEVC_PP::NAME(h265_SSE_8u)(src1, pitchSrc1, src2, pitchSrc2, width, height, shift);
    }

    static inline Ipp32s h265_Sse(const Ipp16u *src1, Ipp32s pitchSrc1, const Ipp16u *src2, Ipp32s pitchSrc2, Ipp32s width, Ipp32s height, Ipp32s shift)
    {
        return MFX_HEVC_PP::NAME(h265_SSE_16u)(src1, pitchSrc1, src2, pitchSrc2, width, height, shift);
    }

    static inline void h265_DiffNv12(const Ipp8u *src, Ipp32s pitchSrc, const Ipp8u *pred, Ipp32s pitchPred, Ipp16s *diff1, Ipp32s pitchDiff1, Ipp16s *diff2, Ipp32s pitchDiff2, Ipp32s width, Ipp32s height)
    {
        return MFX_HEVC_PP::NAME(h265_DiffNv12_8u)(src, pitchSrc, pred, pitchPred, diff1, pitchDiff1, diff2, pitchDiff2, width, height);
    }

    static inline void h265_DiffNv12(const Ipp16u *src, Ipp32s pitchSrc, const Ipp16u *pred, Ipp32s pitchPred, Ipp16s *diff1, Ipp32s pitchDiff1, Ipp16s *diff2, Ipp32s pitchDiff2, Ipp32s width, Ipp32s height)
    {
        return MFX_HEVC_PP::NAME(h265_DiffNv12_16u)(src, pitchSrc, pred, pitchPred, diff1, pitchDiff1, diff2, pitchDiff2, width, height);
    }

    static inline void h265_Diff(const Ipp8u *src, Ipp32s pitchSrc, const Ipp8u *pred, Ipp32s pitchPred, Ipp16s *diff, Ipp32s pitchDiff, Ipp32s size)
    {
        return MFX_HEVC_PP::NAME(h265_Diff_8u)(src, pitchSrc, pred, pitchPred, diff, pitchDiff, size);
    }

    static inline void h265_Diff(const Ipp16u *src, Ipp32s pitchSrc, const Ipp16u *pred, Ipp32s pitchPred, Ipp16s *diff, Ipp32s pitchDiff, Ipp32s size)
    {
        return MFX_HEVC_PP::NAME(h265_Diff_16u)(src, pitchSrc, pred, pitchPred, diff, pitchDiff, size);
    }

    static inline void h265_SplitChromaCtb(const Ipp8u *nv12, Ipp32s pitchNv12, Ipp8u *u, Ipp32s pitchU, Ipp8u *v, Ipp32s pitchV, Ipp32s width, Ipp32s height)
    {
        return MFX_HEVC_PP::NAME(h265_SplitChromaCtb_8u)(nv12, pitchNv12, u, pitchU, v, pitchV, width, height);
    }

    static inline void h265_SplitChromaCtb(const Ipp16u *nv12, Ipp32s pitchNv12, Ipp16u *u, Ipp32s pitchU, Ipp16u *v, Ipp32s pitchV, Ipp32s width, Ipp32s height)
    {
        return MFX_HEVC_PP::NAME(h265_SplitChromaCtb_16u)(nv12, pitchNv12, u, pitchU, v, pitchV, width, height);
    }

    static inline void h265_ImageDiffHistogram(Ipp8u* pSrc, Ipp8u* pRef, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s histogram[5], Ipp64s *pSrcDC, Ipp64s *pRefDC)
    {
        return MFX_HEVC_PP::NAME(h265_ImageDiffHistogram_8u)(pSrc, pRef, pitch, width, height, histogram, pSrcDC, pRefDC);
    }

    static inline void h265_SearchBestBlock8x8(Ipp8u *pSrc, Ipp8u *pRef, Ipp32s pitch, Ipp32s xrange, Ipp32s yrange, Ipp32u *bestSAD, Ipp32s *bestX, Ipp32s *bestY) 
    {
        return MFX_HEVC_PP::NAME(h265_SearchBestBlock8x8_8u)(pSrc, pRef, pitch, xrange, yrange, bestSAD, bestX, bestY);
    }

    static inline void h265_ComputeRsCs4x4(const Ipp8u* pSrc, Ipp32s srcPitch, Ipp32s wblocks, Ipp32s hblocks, Ipp32f* pRs, Ipp32f* pCs)
    {
        return MFX_HEVC_PP::NAME(h265_ComputeRsCs4x4_8u)(pSrc, srcPitch, wblocks, hblocks, pRs, pCs);
    }

    static inline Ipp32s h265_DiffDc(const Ipp8u* pSrc, Ipp32s srcPitch, const Ipp8u* pPred, Ipp32s predPitch, Ipp32s size)
    {
        return MFX_HEVC_PP::NAME(h265_DiffDc_8u)(pSrc, srcPitch, pPred, predPitch, size);
    }
    static inline Ipp32s h265_DiffDc(const Ipp16u* pSrc, Ipp32s srcPitch, const Ipp16u* pPred, Ipp32s predPitch, Ipp32s size)
    {
        return MFX_HEVC_PP::NAME(h265_DiffDc_16u)(pSrc, srcPitch, pPred, predPitch, size);
    }
} // namespace MFX_HEVC_PP

#endif // __MFX_H265_OPTIMIZATION_H__
#endif // defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
