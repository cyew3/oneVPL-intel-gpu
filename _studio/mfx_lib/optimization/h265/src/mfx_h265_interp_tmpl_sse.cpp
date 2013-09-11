/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include <immintrin.h>
#include "ipp.h"

#include "mfx_h265_optimization.h"

#ifdef MFX_EMULATE_SSSE3
#include "mfx_ssse3_emulation.h"
#endif

#ifndef OPT_INTERP_PMUL

using namespace UMC_HEVC_DECODER;

/* NOTE: In debug mode compiler attempts to load data with MOVNTDQA while data is
+only 8-byte aligned, but PMOVZX does not require 16-byte alignment. */
#ifdef NDEBUG
#define MM_LOAD_EPI64(x) (*(__m128i*)x)
#else
#define MM_LOAD_EPI64(x) _mm_loadl_epi64( (const __m128i*)x )
#endif

namespace MFX_HEVC_PP
{
    //=====================================================

    // general multiplication by const form
    template <int c_coeff> class t_interp_mulw
    { 
    public: 
        static __m128i H265_FORCEINLINE func( __m128i v_src ) { return _mm_mullo_epi16( v_src, _mm_set1_epi16( c_coeff ) ); } }
    ;

    template <int c_coeff> class t_interp_muld
    { 
    public:
        static __m128i H265_FORCEINLINE func( __m128i v_src, __m128i& v_ret_hi )
        {
            __m128i v_coeff = _mm_set1_epi16( c_coeff );

            __m128i v_chunk = _mm_mullo_epi16( v_src, v_coeff );
            __m128i v_chunk_h = _mm_mulhi_epi16( v_src, v_coeff );

            v_ret_hi = _mm_unpackhi_epi16( v_chunk, v_chunk_h );
            return ( _mm_unpacklo_epi16( v_chunk, v_chunk_h ) );
        }
    };

    template <int c_coeff> class t_interp_addmulw 
    { public: static __m128i H265_FORCEINLINE func( __m128i v_acc, __m128i v_src ) { return _mm_adds_epi16( v_acc, t_interp_mulw< c_coeff >::func( v_src ) ); } };

    template <int c_coeff> class t_interp_addmuld
    {
    public:
        static void H265_FORCEINLINE func( __m128i v_src, __m128i& v_acc1, __m128i& v_acc2 )
        {
            __m128i v_hi, v_lo = t_interp_muld< c_coeff >::func( v_src, v_hi );
            v_acc2 = _mm_add_epi32( v_acc2, v_hi );
            v_acc1 = _mm_add_epi32( v_acc1, v_lo );
        } 
    };

    //=================================================================================================
    // specializations for particular constants that are faster to compute without doing multiply - who said metaprogramming is too complex??

    // pass-through accumulators for zero 
    template <> class t_interp_addmulw< 0 > 
    { public: static __m128i H265_FORCEINLINE func( __m128i v_acc, __m128i v_src ) { return v_acc; } };
    template <> class t_interp_addmuld< 0 >
    { public: static void H265_FORCEINLINE func( __m128i v_src, __m128i& v_acc1, __m128i& v_acc2 ) {} };

    template <> class t_interp_mulw< 1 >
    { public: static __m128i H265_FORCEINLINE func( __m128i v_src ) { return v_src; } };
    template <> class t_interp_muld< 1 >
    { public: static __m128i H265_FORCEINLINE func( __m128i v_src, __m128i& v_ret_hi ) { v_ret_hi = _mm_cvtepi16_epi32( _mm_unpackhi_epi64( v_src, v_src ) ); return ( _mm_cvtepi16_epi32( v_src ) ); } };

    template <> class t_interp_mulw< -1 >
    { public: static __m128i H265_FORCEINLINE func( __m128i v_src ) { return _mm_sub_epi16( _mm_setzero_si128(), v_src ); } };
    template <> class t_interp_muld< -1 >
    { public: static __m128i H265_FORCEINLINE func( __m128i v_src, __m128i& v_ret_hi ) { return t_interp_muld< 1 >::func( t_interp_mulw< -1 >::func( v_src ), v_ret_hi ); } };
    // although the above does not properly work for -32768 (0x8000) value (proper unpack-sign-extend first with two subtracts from 0 is slower) it should be OK as 0x8000 cannot be in the source

    template <> class t_interp_mulw< 2 >
    { public: static __m128i H265_FORCEINLINE func( __m128i v_src ) { return _mm_slli_epi16( v_src, 1 ); } };
    template <> class t_interp_muld< 2 >
    { public: static __m128i H265_FORCEINLINE func( __m128i v_src, __m128i& v_ret_hi ) { return t_interp_muld< 1 >::func( t_interp_mulw< 2 >::func( v_src ), v_ret_hi ); } };

    template <> class t_interp_mulw< -2 >
    { public: static __m128i H265_FORCEINLINE func( __m128i v_src ) { return t_interp_mulw< -1 >::func( t_interp_mulw< 2 >::func( v_src ) ); } };

    template <> class t_interp_mulw< 4 >
    { public: static __m128i H265_FORCEINLINE func( __m128i v_src ) { return _mm_slli_epi16( v_src, 2 ); } };

    template <> class t_interp_mulw< -4 >
    { public: static __m128i H265_FORCEINLINE func( __m128i v_src ) { return t_interp_mulw< -1 >::func( t_interp_mulw< 4 >::func( v_src ) ); } };

    template <> class t_interp_mulw< 16 >
    { public: static __m128i H265_FORCEINLINE func( __m128i v_src ) { return _mm_slli_epi16( v_src, 4 ); } };


    //=================================================================================================
    // 4-tap and 8-tap filter implemntation using filter constants as template params where general case 
    // is to multiply by constant but for some trivial constants specializations are provided
    // to get the result faster

    static __m128i H265_FORCEINLINE _mm_interp_load( const Ipp8u* pSrc )  { return _mm_cvtepu8_epi16( MM_LOAD_EPI64( pSrc ) ); }
    static __m128i H265_FORCEINLINE _mm_interp_load( const Ipp16s* pSrc ) { return _mm_loadu_si128( (const __m128i *)pSrc ); }

    template <int c1, int c2, int c3, int c4, typename t_src >
    static __m128i H265_FORCEINLINE _mm_interp_4tap_kernel( const t_src* pSrc, Ipp32s accum_pitch, __m128i& )
    {
        __m128i v_acc1 = t_interp_mulw< c1 >::func( _mm_interp_load( pSrc ) );
        v_acc1 = t_interp_addmulw< c2 >::func( v_acc1, _mm_interp_load( pSrc + accum_pitch   ) );
        v_acc1 = t_interp_addmulw< c3 >::func( v_acc1, _mm_interp_load( pSrc + accum_pitch*2 ) );
        v_acc1 = t_interp_addmulw< c4 >::func( v_acc1, _mm_interp_load( pSrc + accum_pitch*3 ) );
        return ( v_acc1 );
    }

    template <int c1, int c2, int c3, int c4 >
    static __m128i H265_FORCEINLINE _mm_interp_4tap_kernel( const Ipp16s* pSrc, Ipp32s accum_pitch, __m128i& v_ret_acc2 )
    {
        __m128i v_acc1_lo = _mm_setzero_si128(), v_acc1_hi = _mm_setzero_si128();
        t_interp_addmuld< c1 >::func( _mm_interp_load( pSrc                 ), v_acc1_lo, v_acc1_hi );
        t_interp_addmuld< c2 >::func( _mm_interp_load( pSrc + accum_pitch   ), v_acc1_lo, v_acc1_hi );
        t_interp_addmuld< c3 >::func( _mm_interp_load( pSrc + accum_pitch*2 ), v_acc1_lo, v_acc1_hi );
        t_interp_addmuld< c4 >::func( _mm_interp_load( pSrc + accum_pitch*3 ), v_acc1_lo, v_acc1_hi );
        v_ret_acc2 = v_acc1_hi;
        return ( v_acc1_lo );
    }

    template <int c1, int c2, int c3, int c4, int c5, int c6, int c7, int c8 >
    static __m128i H265_FORCEINLINE _mm_interp_8tap_kernel( const Ipp16s* pSrc, Ipp32s accum_pitch, __m128i& v_ret_acc2 )
    {
        __m128i v_acc1_hi, v_acc1 = _mm_interp_4tap_kernel<c1, c2, c3, c4 >( pSrc, accum_pitch, v_acc1_hi );
        __m128i v_acc2_hi, v_acc2 = _mm_interp_4tap_kernel<c5, c6, c7, c8 >( pSrc + accum_pitch*4, accum_pitch, v_acc2_hi );
        v_ret_acc2 = _mm_add_epi32( v_acc1_hi, v_acc2_hi );
        return ( _mm_add_epi32( v_acc1, v_acc2 ) );
    }

    template <int c1, int c2, int c3, int c4, int c5, int c6, int c7, int c8 >
    static __m128i H265_FORCEINLINE _mm_interp_8tap_kernel( const Ipp8u* pSrc, Ipp32s accum_pitch, __m128i& )
    {
        __m128i v_unused1, v_acc1 = _mm_interp_4tap_kernel<c1, c2, c3, c4 >( pSrc, accum_pitch, v_unused1 );
        __m128i v_unused2, v_acc2 = _mm_interp_4tap_kernel<c5, c6, c7, c8 >( pSrc + accum_pitch*4, accum_pitch, v_unused2 );
        return ( _mm_adds_epi16( v_acc1, v_acc2 ) );
    }

    //=================================================================================================
    // partioal specialization for __m128i; 
    // NOTE: always reads a block with a width extended to a multiple of 8
    template
        < 
        UMC_HEVC_DECODER::EnumTextType c_plane_type,
        typename     t_src, 
        typename     t_dst,
        int          tab_index
        >
    class t_InterpKernel_intrin< __m128i, c_plane_type, t_src, t_dst, tab_index >
    {
        typedef __m128i t_vec;

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
            MFX_HEVC_PP::EnumAddAverageType eAddAverage,
            const void* in_pSrc2,
            int   in_Src2Pitch // in samples
            )
        {
            const int c_tap = (c_plane_type == TEXT_LUMA) ? 8 : 4;

            t_vec v_offset = _mm_cvtsi32_si128( sizeof(t_src)==2 ? offset : (offset << 16) | offset );
            v_offset = _mm_shuffle_epi32( v_offset, 0 ); // broadcast
            in_Src2Pitch *= (eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_BUF ? 2 : 1);

            for (int i, j = 0; j < height; ++j) 
            {
                t_dst* pDst_ = pDst;
                const Ipp8u* pSrc2 = (const Ipp8u*)in_pSrc2;
                t_vec  v_acc;

                _mm_prefetch( (const char*)(pSrc + in_SrcPitch), _MM_HINT_NTA ); 

                for (i = 0; i < width; i += 8, pDst_ += 8 )
                {
                    t_vec v_acc2 = _mm_setzero_si128(); v_acc = _mm_setzero_si128();
                    const t_src*  pSrc_ = pSrc + i;

                    if ( c_plane_type == TEXT_LUMA )   // resolved at compile time
                    {
                        if ( tab_index == 1 )   // resolved at compile time
                            v_acc = _mm_interp_8tap_kernel< -1,   4, -10,  58,  17,  -5,   1,   0 >( pSrc_, accum_pitch, v_acc2 );
                        else if ( tab_index == 2 )
                            v_acc = _mm_interp_8tap_kernel< -1,   4, -11,  40,  40, -11,   4,  -1 >( pSrc_, accum_pitch, v_acc2 );
                        else
                            v_acc = _mm_interp_8tap_kernel<  0,   1,  -5,  17,  58, -10,   4,  -1 >( pSrc_, accum_pitch, v_acc2 );
                    }
                    else // ( c_plane_type == TEXT_CHROMA  )
                    {
                        if ( tab_index == 1 )
                            v_acc = _mm_interp_4tap_kernel< -2, 58, 10, -2 >( pSrc_, accum_pitch, v_acc2 );
                        else if ( tab_index == 2 )
                            v_acc = _mm_interp_4tap_kernel< -4, 54, 16, -2 >( pSrc_, accum_pitch, v_acc2 );
                        else if ( tab_index == 3 )
                            v_acc = _mm_interp_4tap_kernel< -6, 46, 28, -4 >( pSrc_, accum_pitch, v_acc2 );
                        else if ( tab_index == 4 )
                            v_acc = _mm_interp_4tap_kernel< -4, 36, 36, -4 >( pSrc_, accum_pitch, v_acc2 );
                        else if ( tab_index == 5 )
                            v_acc = _mm_interp_4tap_kernel< -4, 28, 46, -6 >( pSrc_, accum_pitch, v_acc2 );
                        else if ( tab_index == 6 )
                            v_acc = _mm_interp_4tap_kernel< -2, 16, 54, -4 >( pSrc_, accum_pitch, v_acc2 );
                        else
                            v_acc = _mm_interp_4tap_kernel< -2, 10, 58, -2 >( pSrc_, accum_pitch, v_acc2 );
                    }

                    if ( sizeof(t_src) == 1 ) // resolved at compile time
                    {
                        if ( offset ) // cmp/jmp is nearly free, branch prediction removes 1-instruction from critical dep chain
                            v_acc = _mm_add_epi16( v_acc, v_offset ); 

                        if ( shift == 6 )
                            v_acc = _mm_srai_epi16( v_acc, 6 );
                        else
                            VM_ASSERT(shift == 0);
                    }
                    else // 16-bit src, 32-bit accum
                    {
                        if ( offset ) {
                            v_acc  = _mm_add_epi32( v_acc,  v_offset );
                            v_acc2 = _mm_add_epi32( v_acc2, v_offset );
                        }

                        if ( shift == 6 ) {
                            v_acc  = _mm_srai_epi32( v_acc, 6 );
                            v_acc2 = _mm_srai_epi32( v_acc2, 6 );
                        } 
                        else if ( shift == 12 ) {
                            v_acc  = _mm_srai_epi32( v_acc, 12 );
                            v_acc2 = _mm_srai_epi32( v_acc2, 12 );
                        }
                        else
                            VM_ASSERT(shift == 0);

                        v_acc = _mm_packs_epi32( v_acc, v_acc2 );
                    }

                    if ( eAddAverage != MFX_HEVC_PP::AVERAGE_NO )
                    {
                        if ( eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_PIC ) {
                            v_acc2 = _mm_cvtepu8_epi16( MM_LOAD_EPI64(pSrc2) );
                            pSrc2 += 8;
                            v_acc2 = _mm_slli_epi16( v_acc2, 6 );
                        }
                        else {
                            v_acc2 = _mm_loadu_si128( (const t_vec*)pSrc2 ); pSrc2 += 16;
                        }

                        v_acc2 = _mm_adds_epi16( v_acc2, _mm_set1_epi16( 1<<6 ) );
                        v_acc = _mm_adds_epi16( v_acc, v_acc2 );
                        v_acc = _mm_srai_epi16( v_acc, 7 );
                    }

                    if ( sizeof( t_dst ) == 1 )
                        v_acc = _mm_packus_epi16(v_acc, v_acc);

                    if ( i + 8 > width )
                        break;

                    if ( sizeof(t_dst) == 1 ) // 8-bit dest, check resolved at compile time
                        _mm_storel_epi64( (t_vec*)pDst_, v_acc );
                    else
                        _mm_storeu_si128( (t_vec*)pDst_, v_acc );
                }

                int rem = (width & 7) * sizeof(t_dst);
                if ( rem )
                {
                    if (rem > 7) {
                        rem -= 8;
                        _mm_storel_epi64( (t_vec*)pDst_, v_acc );
                        v_acc = _mm_srli_si128( v_acc, 8 );
                        pDst_ = (t_dst*)(8 + (Ipp8u*)pDst_);
                    }
                    if (rem > 3) {
                        rem -= 4;
                        *(Ipp32u*)(pDst_) = _mm_cvtsi128_si32( v_acc );
                        v_acc = _mm_srli_si128( v_acc, 4 );
                        pDst_ = (t_dst*)(4 + (Ipp8u*)pDst_);
                    }
                    if (rem > 1)
                        *(Ipp16u*)(pDst_) = (Ipp16u)_mm_cvtsi128_si32( v_acc );
                }

                pSrc += in_SrcPitch;
                pDst += in_DstPitch;
                in_pSrc2 = (const Ipp8u*)in_pSrc2 + in_Src2Pitch;
            }
        }
    };

    //=====================================================
    // explicit instantiation of used forms, otherwise won't be found at link

    // LUMA
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp8u, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp8u, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp8u, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp8u, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp8u, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp8u, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp8u, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp8u, 8 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp16s, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp16s, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp16s, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp16s, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp16s, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp16s, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp16s, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp16s, 8 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp16s, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp16s, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp16s, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp16s, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp16s, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp16s, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp16s, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp16s, 8 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp8u, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp8u, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp8u, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp8u, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp8u, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp8u, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp8u, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp8u, 8 >;

    // interleaved U/V
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 8 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 8 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 8 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 8 >;

    // plane U
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp8u, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp8u, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp8u, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp8u, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp8u, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp8u, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp8u, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp8u, 8 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp16s, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp16s, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp16s, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp16s, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp16s, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp16s, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp16s, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp8u, Ipp16s, 8 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp16s, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp16s, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp16s, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp16s, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp16s, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp16s, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp16s, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp16s, 8 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp8u, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp8u, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp8u, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp8u, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp8u, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp8u, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp8u, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_U, Ipp16s, Ipp8u, 8 >;

    // plane V
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp8u, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp8u, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp8u, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp8u, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp8u, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp8u, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp8u, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp8u, 8 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp16s, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp16s, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp16s, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp16s, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp16s, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp16s, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp16s, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp8u, Ipp16s, 8 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp16s, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp16s, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp16s, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp16s, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp16s, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp16s, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp16s, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp16s, 8 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp8u, 1 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp8u, 2 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp8u, 3 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp8u, 4 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp8u, 5 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp8u, 6 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp8u, 7 >;
    template t_InterpKernel_intrin< __m128i, TEXT_CHROMA_V, Ipp16s, Ipp8u, 8 >;

} // end namespace MFX_HEVC_PP

#endif //#ifndef OPT_INTERP_PMUL
#endif //#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
