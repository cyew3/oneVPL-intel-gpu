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

//=========================================================
//   configuration macros
//=========================================================

// [1] choose only one arch
#define MFX_TARGET_OPTIMIZATION_SSE4
//#define MFX_TARGET_OPTIMIZATION_AVX2
//#define MFX_TARGET_OPTIMIZATION_PX // ref C or IPP based, not supported yet

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

namespace MFX_HEVC_COMMON
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
        Ipp8u strength;
        Ipp8u qp;
        Ipp8u deblockP;
        Ipp8u deblockQ;
        Ipp8s tcOffset;
        Ipp8s betaOffset;
        Ipp32s uiPartQ; // debug
    };

};

namespace MFX_HEVC_COMMON
{
    /* transform Inv */
    void h265_DST4x4Inv_16sT  (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT4x4Inv_16sT  (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT8x8Inv_16sT  (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT16x16Inv_16sT(void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT32x32Inv_16sT(void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);

    /* interpolation, version from Jon/Ken */
    void Interp_S8_NoAvg(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height, int shift, short offset, int dir, int plane);

    void Interp_S16_NoAvg(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height, int shift, short offset, int dir, int plane);

    void Interp_S8_WithAvg(const unsigned char* pSrc, unsigned int srcPitch, unsigned char *pDst, unsigned int dstPitch, void *pvAvg, unsigned int avgPitch, int avgMode, 
        int tab_index, int width, int height, int shift, short offset, int dir, int plane);

    void Interp_S16_WithAvg(const short* pSrc, unsigned int srcPitch, unsigned char *pDst, unsigned int dstPitch, void *pvAvg, unsigned int avgPitch, int avgMode, 
        int tab_index, int width, int height, int shift, short offset, int dir, int plane);

    /* Deblocking, "_I" means Implace operation */
    void h265_FilterEdgeLuma_8u_I(
        H265EdgeData *edge, 
        Ipp8u *srcDst, 
        Ipp32s srcDstStride, 
        Ipp32s dir);

    void h265_FilterEdgeChroma_Plane_8u_I(
        H265EdgeData *edge, 
        Ipp8u *srcDst, 
        Ipp32s srcDstStride, 
        Ipp32s chromaQpOffset, 
        Ipp32s dir, 
        Ipp32s chromaQp);

    void h265_FilterEdgeChroma_Interleaved_8u_I(
        H265EdgeData *edge, 
        Ipp8u *srcDst, 
        Ipp32s srcDstStride,
        Ipp32s chromaCbQpOffset, 
        Ipp32s chromaCrQpOffset, 
        Ipp32s dir,
        Ipp32s chromaQpCb,
        Ipp32s chromaQpCr);
};

namespace MFX_HEVC_ENCODER
{
    /* transform Fwd */
    void H265_FASTCALL h265_DST4x4Fwd_16s  (const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT4x4Fwd_16s  (const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT8x8Fwd_16s  (const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT16x16Fwd_16s(const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT32x32Fwd_16s(const short *H265_RESTRICT src, short *H265_RESTRICT dst);

    /* IPP based version SAD */
    typedef IppStatus (__STDCALL *SADfunc8u)(
        const Ipp8u* pSrcCur,
        Ipp32s srcCurStep,
        const Ipp8u* pSrcRef,
        Ipp32s srcRefStep,
        Ipp32s* pDst,
        Ipp32s mcType);

    extern SADfunc8u* SAD_8u[17];

    IppStatus h265_SAD_MxN_special_IPP_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY, int* sad);

    /* special version SAD from Nablet */
    typedef int (__STDCALL *SADfunc8u_special)(
        const unsigned char *image,  const unsigned char *block, int img_stride);

    extern SADfunc8u_special* SAD_8u_special[17];
    int h265_SAD_MxN_special_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY);

    /* Quantization Fwd */
    void h265_QuantFwd_16s(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift);
    Ipp32s h265_QuantFwd_SBH_16s(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scale, int offset, int shift);

};

namespace MFX_HEVC_DECODER
{
};

//=================================================================================================

namespace MFX_HEVC_COMMON
{

    template < UMC_HEVC_DECODER::EnumTextType plane_type, typename t_src, typename t_dst >
    void H265_FORCEINLINE Interpolate( 
        MFX_HEVC_COMMON::EnumInterpType interp_type,
        const t_src* in_pSrc,
        Ipp32u in_SrcPitch, // in samples
        t_dst* H265_RESTRICT in_pDst,
        Ipp32u in_DstPitch, // in samples
        Ipp32s tab_index,
        Ipp32s width,
        Ipp32s height,
        Ipp32s shift,
        Ipp16s offset,
        MFX_HEVC_COMMON::EnumAddAverageType eAddAverage = MFX_HEVC_COMMON::AVERAGE_NO,
        const void* in_pSrc2 = NULL,
        int   in_Src2Pitch = 0 ); // in samples

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
            int    in_SrcPitch, // in samples
            int    in_DstPitch, // in samples
            int    width,
            int    height,
            int    accum_pitch,
            int    shift,
            int    offset,
            MFX_HEVC_COMMON::EnumAddAverageType eAddAverage = MFX_HEVC_COMMON::AVERAGE_NO,
            const void* in_pSrc2 = NULL,
            int   in_Src2Pitch = 0 // in samples
            );
    };


    template < typename t_vec, UMC_HEVC_DECODER::EnumTextType plane_type, typename t_src, typename t_dst >
    void t_InterpKernel_dispatch(
        t_dst* H265_RESTRICT pDst, 
        const t_src* pSrc, 
        int    in_SrcPitch, // in samples
        int    in_DstPitch, // in samples
        int    tab_index,
        int    width,
        int    height,
        int    accum_pitch,
        int    shift,
        int    offset,
        MFX_HEVC_COMMON::EnumAddAverageType eAddAverage,
        const void* in_pSrc2,
        int   in_Src2Pitch // in samples
        )
    {
        if ( tab_index == 1 )
            t_InterpKernel_intrin< t_vec, plane_type, t_src, t_dst, 1 >::func( pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
        else if ( tab_index == 2 )
            t_InterpKernel_intrin< t_vec, plane_type, t_src, t_dst, 2 >::func( pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
        else if ( tab_index == 3 )
            t_InterpKernel_intrin< t_vec, plane_type, t_src, t_dst, 3 >::func( pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
        else if ( tab_index == 4 )
            t_InterpKernel_intrin< t_vec, plane_type, t_src, t_dst, 4 >::func( pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
        else if ( tab_index == 5 )
            t_InterpKernel_intrin< t_vec, plane_type, t_src, t_dst, 5 >::func( pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
        else if ( tab_index == 6 )
            t_InterpKernel_intrin< t_vec, plane_type, t_src, t_dst, 6 >::func( pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
        else if ( tab_index == 7 )
            t_InterpKernel_intrin< t_vec, plane_type, t_src, t_dst, 7 >::func( pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
    }
#endif // #ifndef OPT_INTERP_PMUL
    //=================================================================================================    

    template < UMC_HEVC_DECODER::EnumTextType plane_type, typename t_src, typename t_dst >
    void H265_FORCEINLINE Interpolate(
        MFX_HEVC_COMMON::EnumInterpType interp_type,
        const t_src* in_pSrc,
        Ipp32u in_SrcPitch, // in samples
        t_dst* H265_RESTRICT in_pDst,
        Ipp32u in_DstPitch, // in samples
        Ipp32s tab_index,
        Ipp32s width,
        Ipp32s height,
        Ipp32s shift,
        Ipp16s offset,
        MFX_HEVC_COMMON::EnumAddAverageType eAddAverage,
        const void* in_pSrc2,
        int    in_Src2Pitch ) // in samples
    {
        Ipp32s accum_pitch = ((interp_type == MFX_HEVC_COMMON::INTERP_HOR) ? (plane_type == UMC_HEVC_DECODER::TEXT_CHROMA ? 2 : 1) : in_SrcPitch);

        const t_src* pSrc = in_pSrc - (((( plane_type == UMC_HEVC_DECODER::TEXT_LUMA) ? 8 : 4) >> 1) - 1) * accum_pitch;

        width <<= int(plane_type == UMC_HEVC_DECODER::TEXT_CHROMA);

#ifdef TIME_INTERP
    startTime = __rdtsc();
#endif

#if defined OPT_INTERP_PMUL
    if (sizeof(t_src) == 1) {
        if (sizeof(t_dst) == 1)
            Interp_S8_WithAvg((unsigned char *)pSrc, in_SrcPitch, (unsigned char *)in_pDst, in_DstPitch, (void *)in_pSrc2, in_Src2Pitch, eAddAverage, tab_index, width, height, shift, offset, interp_type, plane_type);
        else
            Interp_S8_NoAvg((unsigned char *)pSrc, in_SrcPitch, (short *)in_pDst, in_DstPitch, tab_index, width, height, shift, offset, interp_type, plane_type);
    }

    /* only used for vertical filter */
    if (sizeof(t_src) == 2) {
        if (sizeof(t_dst) == 1)
            Interp_S16_WithAvg((short *)pSrc, in_SrcPitch, (unsigned char *)in_pDst, in_DstPitch, (void *)in_pSrc2, in_Src2Pitch, eAddAverage, tab_index, width, height, shift, offset, INTERP_VER, plane_type);
        else
            Interp_S16_NoAvg((short *)pSrc, in_SrcPitch, (short *)in_pDst, in_DstPitch, tab_index, width, height, shift, offset, INTERP_VER, plane_type);
    }
#else
#ifdef __INTEL_COMPILER
        if ( _may_i_use_cpu_feature( _FEATURE_AVX2 ) && (width > 8) )
            t_InterpKernel_dispatch< __m256i, plane_type >( in_pDst, pSrc, in_SrcPitch, in_DstPitch, tab_index, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
        else
#endif // __INTEL_COMPILER
            t_InterpKernel_dispatch< __m128i, plane_type >( in_pDst, pSrc, in_SrcPitch, in_DstPitch, tab_index, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
#endif /* OPT_INTERP_PMUL */

#ifdef TIME_INTERP
    endTime = __rdtsc();
    if (endTime > startTime) {
        totalCycles[sizeof(t_src)-1][sizeof(t_dst)-1] += (endTime - startTime);
        totalPixels[sizeof(t_src)-1][sizeof(t_dst)-1] += (width * height);

        totalCyclesAll += (endTime - startTime);
        totalPixelsAll += (width * height);
    }

#endif

    }

} // namespace MFX_HEVC_COMMON

//=================================================================================================

#endif // __MFX_H265_OPTIMIZATION_H__
#endif // defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
