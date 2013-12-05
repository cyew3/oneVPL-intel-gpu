//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#ifndef __MFX_H265_OPTIMIZATION_H__
#define __MFX_H265_OPTIMIZATION_H__

#include "ipps.h"
#include <immintrin.h>

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

#if defined(ANDROID) || defined(LINUX32) || defined(LINUX64)
    #define MFX_TARGET_OPTIMIZATION_ATOM
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
        // Reserved are 5 bits in each flag byte are making sure that no writes are
        // made between sides. P side may be written from one thread and Q side from
        // another, they have to be in different bytes or race conditions happen on
        // slice or tile boundaries!!!
        Ipp8u deblockP  : 1;
        Ipp8u isIntraP  : 1;
        Ipp8u isTrCbfYP : 1;
        Ipp8u reservedP : 5;
        Ipp8u qpP;

        Ipp8u deblockQ  : 1;
        Ipp8u isIntraQ  : 1;
        Ipp8u isTrCbfYQ : 1;
        Ipp8u reservedQ : 5;
        Ipp8u qp;

        Ipp8u strength;
        Ipp8s tcOffset;
        Ipp8s betaOffset;
    };

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

        SaoCtuStatistics(){}
        ~SaoCtuStatistics(){}

        void Reset()
        {
            memset(diff, 0, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
            memset(count, 0, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
        }

        //const 
        SaoCtuStatistics& operator=(const SaoCtuStatistics& src)
        {
            ippsCopy_8u((Ipp8u*)src.diff,  (Ipp8u*)diff,  sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
            ippsCopy_8u((Ipp8u*)src.count, (Ipp8u*)count, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);

            return *this;
        }

        const SaoCtuStatistics& operator+= (const SaoCtuStatistics& src)
        {
            for(int i=0; i< MAX_NUM_SAO_CLASSES; i++)
            {
                diff[i] += src.diff[i];
                count[i] += src.count[i];
            }
            return *this;
        }

    };
    /* ******************************************************** */
    /*                    Data Types for Dispatcher             */
    /* ******************************************************** */

    // [PTR.Sad]
    #define SAD_PARAMETERS_LIST_SPECIAL const unsigned char *image,  const unsigned char *block, int img_stride
    #define SAD_PARAMETERS_LIST_GENERAL const unsigned char *image,  const unsigned char *block, int img_stride, int block_stride

    typedef  int (H265_FASTCALL *PTR_SAD_MxN_8u)(SAD_PARAMETERS_LIST_SPECIAL);
    typedef  int (H265_FASTCALL *PTR_SAD_MxN_General_8u)(SAD_PARAMETERS_LIST_GENERAL);

    // [PTR.TransformInv]
    typedef void (* PTR_TransformInv_16sT) (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);

    // [PTR.TransformFwd]
    typedef void (H265_FASTCALL *PTR_TransformFwd_16s)(const short *H265_RESTRICT src, short *H265_RESTRICT dst);

    // [PTR.QuantFwd]
    typedef void   (H265_FASTCALL *PTR_QuantFwd_16s)    (const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift);
    typedef Ipp32s (H265_FASTCALL *PTR_QuantFwd_SBH_16s)(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scale, int offset, int shift);

    // [PTR.deblocking]
    typedef Ipp32s (* PTR_FilterEdgeLuma_8u_I)(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir);
    typedef void (* PTR_FilterEdgeChromaInterleaved_8u_I)(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr);
    typedef void (* PTR_FilterEdgeChromaPlane_8u_I)(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp);

    // [PTR.SAO]
    #define SAOCU_ORG_PARAMETERS_LIST Ipp8u* pRec, Ipp32s stride, Ipp32s saoType, Ipp8u* tmpL, Ipp8u* tmpU, Ipp32u maxCUWidth, Ipp32u maxCUHeight, \
            Ipp32s picWidth, Ipp32s picHeight, Ipp32s* pOffsetEo, Ipp8u* pOffsetBo, Ipp8u* pClipTable, Ipp32u CUPelX, Ipp32u CUPelY 

    #define SAOCU_PARAMETERS_LIST Ipp8u* pRec, Ipp32s stride, Ipp32s saoType, Ipp8u* tmpL, Ipp8u* tmpU, Ipp32u maxCUWidth, Ipp32u maxCUHeight, \
    Ipp32s picWidth, Ipp32s picHeight, Ipp32s* pOffsetEo, Ipp8u* pOffsetBo, Ipp8u* pClipTable, Ipp32u CUPelX, Ipp32u CUPelY, CTBBorders pbBorderAvail

    typedef void (* PTR_ProcessSaoCuOrg_Luma_8u)( SAOCU_ORG_PARAMETERS_LIST );

    typedef void (* PTR_ProcessSaoCu_Luma_8u)( SAOCU_PARAMETERS_LIST );

    // [PTR.SAO :: Encode primitivies]
    #define SAOCU_ENCODE_PARAMETERS_LIST int compIdx, const Ipp8u* recBlk, int recStride, const Ipp8u* orgBlk, int orgStride, int width, \
        int height, int shift,  const MFX_HEVC_PP::CTBBorders& borders, MFX_HEVC_PP::SaoCtuStatistics* statsDataTypes

    typedef void(* PTR_GetCtuStatistics_8u)( SAOCU_ENCODE_PARAMETERS_LIST );

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

    typedef void (* PTR_Interp_s8_d16)(INTERP_S8_D16_PARAMETERS_LIST);
    typedef void (* PTR_Interp_s8_d16_Ext)(INTERP_S8_D16_PARAMETERS_LIST, int plane);
    typedef void (* PTR_Interp_s16_d16)(INTERP_S16_D16_PARAMETERS_LIST);

    // average
    typedef void (* PTR_AverageModeN)(INTERP_AVG_NONE_PARAMETERS_LIST);
    typedef void (* PTR_AverageModeP)(INTERP_AVG_PIC_PARAMETERS_LIST);
    typedef void (* PTR_AverageModeB)(INTERP_AVG_BUF_PARAMETERS_LIST);
    //-----------------------------------------------------

    // [PTR.WeightedPred]
    typedef void (* PTR_CopyWeighted_S16U8)(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round);
    typedef void (* PTR_CopyWeightedBidi_S16U8)(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round);

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
#endif

        // [Transform Inv]
        HEVCPP_API( PTR_TransformInv_16sT, void, h265_DST4x4Inv_16sT, (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize) );
        HEVCPP_API( PTR_TransformInv_16sT, void, h265_DCT4x4Inv_16sT, (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize) );
        HEVCPP_API( PTR_TransformInv_16sT, void, h265_DCT8x8Inv_16sT, (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize) );
        HEVCPP_API( PTR_TransformInv_16sT, void, h265_DCT16x16Inv_16sT, (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize) );
        HEVCPP_API( PTR_TransformInv_16sT, void, h265_DCT32x32Inv_16sT, (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize) );
        
        // [transform.forward]
        HEVCPP_API( PTR_TransformFwd_16s, void H265_FASTCALL, h265_DST4x4Fwd_16s, (const short *H265_RESTRICT src, short *H265_RESTRICT dst) );
        HEVCPP_API( PTR_TransformFwd_16s, void H265_FASTCALL, h265_DCT4x4Fwd_16s, (const short *H265_RESTRICT src, short *H265_RESTRICT dst) );
        HEVCPP_API( PTR_TransformFwd_16s, void H265_FASTCALL, h265_DCT8x8Fwd_16s, (const short *H265_RESTRICT src, short *H265_RESTRICT dst) );
        HEVCPP_API( PTR_TransformFwd_16s, void H265_FASTCALL, h265_DCT16x16Fwd_16s, (const short *H265_RESTRICT src, short *H265_RESTRICT dst) );
        HEVCPP_API( PTR_TransformFwd_16s, void H265_FASTCALL, h265_DCT32x32Fwd_16s, (const short *H265_RESTRICT src, short *H265_RESTRICT dst) );
        
        // forward quantization
        HEVCPP_API( PTR_QuantFwd_16s,     void H265_FASTCALL,   h265_QuantFwd_16s,     (const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift) );
        HEVCPP_API( PTR_QuantFwd_SBH_16s, Ipp32s H265_FASTCALL, h265_QuantFwd_SBH_16s, (const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scale, int offset, int shift) );

        // [deblocking]
        HEVCPP_API( PTR_FilterEdgeLuma_8u_I, Ipp32s, h265_FilterEdgeLuma_8u_I, (H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir) );
        HEVCPP_API( PTR_FilterEdgeChromaInterleaved_8u_I, void, h265_FilterEdgeChroma_Interleaved_8u_I, (H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr) );
        HEVCPP_API( PTR_FilterEdgeChromaPlane_8u_I, void, h265_FilterEdgeChroma_Plane_8u_I, (H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp) );

        // [SAO]
        HEVCPP_API( PTR_ProcessSaoCuOrg_Luma_8u, void, h265_ProcessSaoCuOrg_Luma_8u, (SAOCU_ORG_PARAMETERS_LIST));
        HEVCPP_API( PTR_ProcessSaoCu_Luma_8u, void, h265_ProcessSaoCu_Luma_8u, (SAOCU_PARAMETERS_LIST));

        HEVCPP_API( PTR_GetCtuStatistics_8u, void, h265_GetCtuStatistics_8u, (SAOCU_ENCODE_PARAMETERS_LIST));
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

        // [Interpolation]
        HEVCPP_API( PTR_Interp_s8_d16, void, h265_InterpLuma_s8_d16_H,   ( INTERP_S8_D16_PARAMETERS_LIST));
        HEVCPP_API( PTR_Interp_s8_d16_Ext, void, h265_InterpChroma_s8_d16_H, ( INTERP_S8_D16_PARAMETERS_LIST, int plane));
        HEVCPP_API( PTR_Interp_s8_d16, void, h265_InterpLuma_s8_d16_V,   ( INTERP_S8_D16_PARAMETERS_LIST));
        HEVCPP_API( PTR_Interp_s8_d16, void, h265_InterpChroma_s8_d16_V, ( INTERP_S8_D16_PARAMETERS_LIST));
        HEVCPP_API( PTR_Interp_s16_d16, void, h265_InterpLuma_s16_d16_V,   ( INTERP_S16_D16_PARAMETERS_LIST));
        HEVCPP_API( PTR_Interp_s16_d16, void, h265_InterpChroma_s16_d16_V, ( INTERP_S16_D16_PARAMETERS_LIST));

        HEVCPP_API( PTR_AverageModeN, void, h265_AverageModeN, (INTERP_AVG_NONE_PARAMETERS_LIST));
        HEVCPP_API( PTR_AverageModeP, void, h265_AverageModeP, (INTERP_AVG_PIC_PARAMETERS_LIST));
        HEVCPP_API( PTR_AverageModeB, void, h265_AverageModeB, (INTERP_AVG_BUF_PARAMETERS_LIST));

        // [WeightedPred]
        HEVCPP_API( PTR_CopyWeighted_S16U8, void, h265_CopyWeighted_S16U8, (Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round) );
        HEVCPP_API( PTR_CopyWeightedBidi_S16U8, void, h265_CopyWeightedBidi_S16U8, (Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round) );

#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
        bool           isInited;

    };
    extern Dispatcher g_dispatcher;
#endif

    int h265_SAD_MxN_special_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY);
    int h265_SAD_MxN_general_8u(const unsigned char *image,  int stride_img, const unsigned char *ref, int stride_ref, int SizeX, int SizeY);

    // LIST OF NON-OPTIMIZED (and NON-DISPATCHERED) FUNCTIONS:

    /* Inverse Quantization */
    void h265_QuantInv_16s(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift);
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

    void h265_PredictIntra_Planar_8u(
        Ipp8u* PredPel,
        Ipp8u* pels,
        Ipp32s pitch,
        Ipp32s width);

    void h265_PredictIntra_Planar_ChromaNV12_8u(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize);
    void h265_PredictIntra_DC_ChromaNV12_8u(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize);
    void h265_PredictIntra_Ang_ChromaNV12_8u(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize, Ipp32u dirMode);
    void h265_PredictIntra_Hor_ChromaNV12_8u(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize);
    void h265_PredictIntra_Ver_ChromaNV12_8u(Ipp8u* PredPel, Ipp8u* pDst, Ipp32s dstStride, Ipp32s blkSize);

    /* Pre-Filtering for INTRA prediction */
    void h265_FilterPredictPels_8u(Ipp8u* PredPel, Ipp32s width);
    void h265_FilterPredictPels_Bilinear_8u(Ipp8u* pSrcDst, int width, int topLeft, int bottomLeft, int topRight);

    /* Predict Pels */
    void h265_GetPredPelsLuma_8u(Ipp8u* pSrc, Ipp8u* PredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf);
    void h265_GetPredPelsChromaNV12_8u(Ipp8u* pSrc, Ipp8u* pPredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf);

    /* interpolation, version from Jon/Ken */
    void Interp_S8_NoAvg(
        const unsigned char* pSrc, 
        unsigned int srcPitch, 
        short *pDst, 
        unsigned int dstPitch, 
        int tab_index, 
        int width, 
        int height, 
        int shift, 
        short offset, 
        int dir, 
        int plane);

    void Interp_S16_NoAvg(
        const short* pSrc, 
        unsigned int srcPitch, 
        short *pDst, 
        unsigned int dstPitch, 
        int tab_index, 
        int width, 
        int height, 
        int shift, 
        short offset, 
        int dir, 
        int plane);

    void Interp_S8_WithAvg(
        const unsigned char* pSrc, 
        unsigned int srcPitch, 
        unsigned char *pDst, 
        unsigned int dstPitch, 
        void *pvAvg, 
        unsigned int avgPitch, 
        int avgMode, 
        int tab_index, 
        int width, 
        int height, 
        int shift, 
        short offset, 
        int dir, 
        int plane);

    void Interp_S16_WithAvg(
        const short* pSrc, 
        unsigned int srcPitch, 
        unsigned char *pDst, 
        unsigned int dstPitch, 
        void *pvAvg, 
        unsigned int avgPitch, 
        int avgMode, 
        int tab_index, 
        int width, 
        int height, 
        int shift, 
        short offset, 
        int dir, 
        int plane);

    /* ******************************************************** */
    /*                    MAKE_NAME                             */
    /* ******************************************************** */
#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    #define NAME(func) g_dispatcher. ## func
#else
    #define NAME(func) func
#endif

    IppStatus InitDispatcher( void );

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

    template < UMC_HEVC_DECODER::EnumTextType plane_type, typename t_src, typename t_dst >
    void H265_FORCEINLINE Interpolate(
        MFX_HEVC_PP::EnumInterpType interp_type,
        const t_src* in_pSrc,
        Ipp32u in_SrcPitch, // in samples
        t_dst* H265_RESTRICT in_pDst,
        Ipp32u in_DstPitch, // in samples
        Ipp32u tab_index,
        Ipp32u width,
        Ipp32u height,
        Ipp32u shift,
        Ipp16s offset,
        MFX_HEVC_PP::EnumAddAverageType eAddAverage = MFX_HEVC_PP::AVERAGE_NO,
        const void* in_pSrc2 = NULL,
        int    in_Src2Pitch = 0) // in samples
    {
        Ipp32s accum_pitch = ((interp_type == MFX_HEVC_PP::INTERP_HOR) ? (plane_type == UMC_HEVC_DECODER::TEXT_CHROMA ? 2 : 1) : in_SrcPitch);

        const t_src* pSrc = in_pSrc - (((( plane_type == UMC_HEVC_DECODER::TEXT_LUMA) ? 8 : 4) >> 1) - 1) * accum_pitch;

        width <<= int(plane_type == UMC_HEVC_DECODER::TEXT_CHROMA);


#if defined OPT_INTERP_PMUL
    if (sizeof(t_src) == 1) {
        if (sizeof(t_dst) == 1)
            Interp_S8_WithAvg((unsigned char *)pSrc, in_SrcPitch, (unsigned char *)in_pDst, in_DstPitch, (void *)in_pSrc2, in_Src2Pitch, eAddAverage, tab_index, width, height, shift, (short)offset, interp_type, plane_type);
        else
            Interp_S8_NoAvg((unsigned char *)pSrc, in_SrcPitch, (short *)in_pDst, in_DstPitch, tab_index, width, height, shift, (short)offset, interp_type, plane_type);
    }

    /* only used for vertical filter */
    if (sizeof(t_src) == 2) {
        if (sizeof(t_dst) == 1)
            Interp_S16_WithAvg((short *)pSrc, in_SrcPitch, (unsigned char *)in_pDst, in_DstPitch, (void *)in_pSrc2, in_Src2Pitch, eAddAverage, tab_index, width, height, shift, (short)offset, INTERP_VER, plane_type);
        else
            Interp_S16_NoAvg((short *)pSrc, in_SrcPitch, (short *)in_pDst, in_DstPitch, tab_index, width, height, shift, (short)offset, INTERP_VER, plane_type);
    }
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

} // namespace MFX_HEVC_PP

#endif // __MFX_H265_OPTIMIZATION_H__
#endif // defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
