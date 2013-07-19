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

// choose only one arch
#define MFX_TARGET_OPTIMIZATION_SSE4
//#define MFX_TARGET_OPTIMIZATION_AVX2
//#define MFX_TARGET_OPTIMIZATION_PX // ref C or IPP based, not supported yet

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
};

namespace MFX_HEVC_ENCODER
{
    /* transform Fwd */
    void H265_FASTCALL h265_DST4x4Fwd_16s  (const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT4x4Fwd_16s  (const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT8x8Fwd_16s  (const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT16x16Fwd_16s(const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT32x32Fwd_16s(const short *H265_RESTRICT src, short *H265_RESTRICT dst);

    /* SAD */
    typedef IppStatus (__STDCALL *SADfunc8u)(
        const Ipp8u* pSrcCur,
        Ipp32s srcCurStep,
        const Ipp8u* pSrcRef,
        Ipp32s srcRefStep,
        Ipp32s* pDst,
        Ipp32s mcType);

    extern SADfunc8u* SAD_8u[17];
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

    //=================================================================================================    

    template < UMC_HEVC_DECODER::EnumTextType plane_type, typename t_src, typename t_dst >
    void Interpolate(
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

        width <<= int(plane_type == TEXT_CHROMA);

#ifdef __INTEL_COMPILER
        if ( _may_i_use_cpu_feature( _FEATURE_AVX2 ) && (width > 8) )
            t_InterpKernel_dispatch< __m256i, plane_type >( in_pDst, pSrc, in_SrcPitch, in_DstPitch, tab_index, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
        else
#endif // __INTEL_COMPILER
            t_InterpKernel_dispatch< __m128i, plane_type >( in_pDst, pSrc, in_SrcPitch, in_DstPitch, tab_index, width, height, accum_pitch, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
    }

} // namespace MFX_HEVC_COMMON

//=================================================================================================

#endif // __MFX_H265_OPTIMIZATION_H__
#endif // defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
