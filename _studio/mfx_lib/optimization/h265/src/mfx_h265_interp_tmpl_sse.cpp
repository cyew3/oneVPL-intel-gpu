//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include <immintrin.h>
//#include "ipp.h"

#include "mfx_h265_optimization.h"

#ifdef MFX_EMULATE_SSSE3
#include "mfx_ssse3_emulation.h"
#endif

#ifndef OPT_INTERP_PMUL

using namespace UMC_HEVC_DECODER;

namespace MFX_HEVC_PP
{
    //=================================================================================================
    // general multiplication by const forms (used for coeffs without specializations below)
    template <int c_coeff, typename t_src> class t_interp_mul;
    template <int c_coeff, typename t_src> class t_interp_addmul;

    template <int c_coeff> class t_interp_mul<c_coeff, Ipp8u>
    { public: static __m128i H265_FORCEINLINE func( const Ipp8u* pSrc, __m128i& v_ret_hi ) 
        { __m128i v_src = t_interp_mul< 1, Ipp8u >::func( pSrc, v_ret_hi ); 
          return _mm_mullo_epi16( v_src, _mm_set1_epi16( c_coeff ) ); } 
    };

    template <int c_coeff> class t_interp_mul<c_coeff, Ipp16s>
    {
    public:
        static __m128i H265_FORCEINLINE func( const Ipp16s* pSrc, __m128i& v_ret_hi )
        {
            __m128i v_src = _mm_loadu_si128( (const __m128i*)pSrc );
            __m128i v_coeff = _mm_set1_epi16( c_coeff );

            __m128i v_hi = _mm_mulhi_epi16( v_src, v_coeff );
            __m128i v_lo = _mm_mullo_epi16( v_src, v_coeff );
            v_ret_hi = _mm_unpackhi_epi16( v_lo, v_hi );
            return _mm_unpacklo_epi16( v_lo, v_hi );
        }
    };

    template <bool coeff_is_positive, typename t_src> class t_interp_acc;

    template <bool coeff_is_positive> class t_interp_acc< coeff_is_positive, Ipp8u >
    { public: static __m128i H265_FORCEINLINE func_acc( __m128i v_src1, __m128i v_acc1, __m128i v_src2, __m128i& v_acc2 ) 
        { return _mm_adds_epi16( v_acc1, v_src1 ); } 
    };
    template <bool coeff_is_positive> class t_interp_acc< coeff_is_positive, Ipp16s >
    { public: static __m128i H265_FORCEINLINE func_acc( __m128i v_src1, __m128i v_acc1, __m128i v_src2, __m128i& v_acc2 ) 
        { v_acc2 = _mm_add_epi32( v_acc2, v_src2 ); return _mm_add_epi32( v_acc1, v_src1 ); };
    };
    template <> class t_interp_acc<false, Ipp8u> // coeff_is_negative
    { public: static __m128i H265_FORCEINLINE func_acc( __m128i v_src1, __m128i v_acc1, __m128i v_src2, __m128i& v_acc2 ) 
        { return _mm_subs_epi16( v_acc1, v_src1 ); } 
    };
    template <> class t_interp_acc<false, Ipp16s> // coeff_is_negative
    { public: static __m128i H265_FORCEINLINE func_acc( __m128i v_src1, __m128i v_acc1, __m128i v_src2, __m128i& v_acc2 ) 
        { v_acc2 = _mm_sub_epi32( v_acc2, v_src2 ); return ( _mm_sub_epi32( v_acc1, v_src1 ) ); };
    };

    template <int c_coeff, typename t_src> class t_interp_addmul
    { 
    public: 
        static __m128i H265_FORCEINLINE func( const t_src* pSrc, __m128i v_acc, __m128i& v_acc_hi ) 
        { 
            const int c_abs_coeff = (c_coeff > 0) ? c_coeff : -c_coeff;
            __m128i v_src2, v_src1 = t_interp_mul< c_abs_coeff, t_src>::func( pSrc, v_src2 );
            return t_interp_acc< (c_coeff > 0), t_src >::func_acc( v_src1, v_acc, v_src2, v_acc_hi );
        } 
    };

    template <int c1, int c2> class t_interp_addmul2
    {
    public:
        static __m128i H265_FORCEINLINE func( const Ipp8u* p_src1, const Ipp8u* p_src2, __m128i& )
        { 
            __m128i v1 = _mm_unpacklo_epi8( _mm_loadl_epi64( (const __m128i*)p_src1 ), _mm_loadl_epi64( (const __m128i*)p_src2 ) );
            __m128i v2 = _mm_setr_epi8( c1, c2, c1, c2, c1, c2, c1, c2,
                                        c1, c2, c1, c2, c1, c2, c1, c2 );
            return ( _mm_maddubs_epi16( v1, v2 ) );
        } 

        static __m128i H265_FORCEINLINE func( const Ipp16s* p_src1, const Ipp16s* p_src2, __m128i& v_ret_hi )
        {
            __m128i v_coeff = _mm_setr_epi16( c1, c2, c1, c2, c1, c2, c1, c2 );
            __m128i v_src1 = _mm_loadu_si128( (const __m128i*)p_src1 );
            __m128i v_src2 = _mm_loadu_si128( (const __m128i*)p_src2 );
            __m128i v_hi = _mm_unpackhi_epi16( v_src1, v_src2 );
            __m128i v_lo = _mm_unpacklo_epi16( v_src1, v_src2 );
            v_lo = _mm_madd_epi16( v_lo, v_coeff );
            v_ret_hi = _mm_madd_epi16( v_hi, v_coeff );
            return ( v_lo );
        }
    };

    //=================================================================================================
    // specializations for particular constants that are faster to compute without doing a multiply
    // pass-through accumulators for zero 
    template <typename t_src> class t_interp_addmul< 0, t_src >
    { public: static __m128i H265_FORCEINLINE func( const t_src*, __m128i v_acc1, __m128i& ) { return ( v_acc1 ); } };

    template <> class t_interp_mul< 1, Ipp8u >
    { public: static __m128i H265_FORCEINLINE func( const Ipp8u* pSrc, __m128i& )
        { return _mm_cvtepu8_epi16( _mm_loadl_epi64( (const __m128i*)pSrc ) ); }
    };
    template <> class t_interp_mul< 2, Ipp8u >
    { public: static __m128i H265_FORCEINLINE func( const Ipp8u* pSrc, __m128i& v_hi )
        { __m128i v_src = t_interp_mul< 1, Ipp8u >::func( pSrc, v_hi ); return _mm_add_epi16( v_src, v_src ); } 
    };
    template <> class t_interp_mul< 4, Ipp8u >
    { public: static __m128i H265_FORCEINLINE func( const Ipp8u* pSrc, __m128i& v_hi )
        { __m128i v_src = t_interp_mul< 1, Ipp8u >::func( pSrc, v_hi ); return _mm_slli_epi16( v_src, 2 ); }
    };

    template <> class t_interp_mul< 1, Ipp16s >
    { public: static __m128i H265_FORCEINLINE func( const Ipp16s* pSrc, __m128i& v_ret_hi ) 
        { v_ret_hi = _mm_cvtepi16_epi32( _mm_loadl_epi64( (const __m128i*)(pSrc + 4) ) ); return _mm_cvtepi16_epi32( _mm_loadl_epi64( (const __m128i*)pSrc ) ); }
    };
    template <> class t_interp_mul< 2, Ipp16s >
    { public: static __m128i H265_FORCEINLINE func( const Ipp16s* pSrc, __m128i& v_ret_hi ) 
        { __m128i v_src = t_interp_mul< 1, Ipp16s >::func( pSrc, v_ret_hi ); v_ret_hi = _mm_add_epi32( v_ret_hi, v_ret_hi ); return _mm_add_epi32( v_src, v_src ); } 
    };
    template <> class t_interp_mul< 4, Ipp16s >
    { public: static __m128i H265_FORCEINLINE func( const Ipp16s* pSrc, __m128i& v_ret_hi ) 
        { __m128i v_src = t_interp_mul< 1, Ipp16s >::func( pSrc, v_ret_hi ); v_ret_hi = _mm_slli_epi32( v_ret_hi, 2 ); return _mm_slli_epi32( v_src, 2 ); } 
    };

    //=================================================================================================
    // 4-tap and 8-tap filter implemntation using filter constants as template params where general case 
    // is to multiply by constant but for some trivial constants specializations are provided
    // to get the result faster
    template <int c1, int c2, int c3, int c4, typename t_src>
    static __m128i H265_FORCEINLINE _mm_interp_4tap_kernel( const t_src* pSrc, Ipp32u accum_pitch, __m128i& v_acc_hi )
    {
        __m128i v_acc1 = t_interp_addmul2< c2, c3 >::func( pSrc + accum_pitch*1, pSrc + accum_pitch*2, v_acc_hi );
        v_acc1 = t_interp_addmul< c1, t_src >::func( pSrc,                 v_acc1, v_acc_hi );
        v_acc1 = t_interp_addmul< c4, t_src >::func( pSrc + accum_pitch*3, v_acc1, v_acc_hi );
        return ( v_acc1 );
    }

    template <int c1, int c2, int c3, int c4, int c5, int c6, int c7, int c8, typename t_src >
    static __m128i H265_FORCEINLINE _mm_interp_8tap_kernel( const t_src* pSrc, Ipp32u accum_pitch, __m128i& v_acc_hi )
    {
        const t_src *pSrc_ = pSrc + accum_pitch*2;
        __m128i v_acc1_hi, v_acc1 = t_interp_addmul2< c3, c4 >::func( pSrc_, pSrc_ + accum_pitch, v_acc1_hi ); pSrc_ += accum_pitch*2;
        v_acc1 = t_interp_addmul< c1, t_src >::func( pSrc,  v_acc1, v_acc1_hi ); pSrc += accum_pitch;
        v_acc1 = t_interp_addmul< c2, t_src >::func( pSrc,  v_acc1, v_acc1_hi );
        __m128i v_acc2_hi, v_acc2 = t_interp_addmul2< c5, c6 >::func( pSrc_, pSrc_ + accum_pitch, v_acc2_hi ); pSrc_ += accum_pitch*2;
        v_acc2 = t_interp_addmul< c7, t_src >::func( pSrc_, v_acc2, v_acc2_hi ); pSrc_ += accum_pitch;
        v_acc2 = t_interp_addmul< c8, t_src >::func( pSrc_, v_acc2, v_acc2_hi );
        if ( sizeof( t_src ) == 1 )
            v_acc1 = _mm_adds_epi16( v_acc1, v_acc2 );
        else
            { v_acc_hi = _mm_add_epi32( v_acc1_hi, v_acc2_hi ); v_acc1 = _mm_add_epi32( v_acc1, v_acc2 ); }
        return ( v_acc1 );
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
            Ipp32u  in_SrcPitch, // in samples
            Ipp32u  in_DstPitch, // in samples
            Ipp32u  width,
            Ipp32u  height,
            Ipp32u  accum_pitch,
            Ipp32u  shift,
            Ipp32u  offset,
            MFX_HEVC_PP::EnumAddAverageType eAddAverage,
            const void* in_pSrc2,
            Ipp32u in_Src2Pitch // in samples
            )
        {
            const int c_tap = (c_plane_type == TEXT_LUMA) ? 8 : 4;

            t_vec v_offset = _mm_cvtsi32_si128( sizeof(t_src)==2 ? offset : (offset << 16) | offset );
            v_offset = _mm_shuffle_epi32( v_offset, 0 ); // broadcast
            in_Src2Pitch *= (eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_BUF ? 2 : 1);

            for (Ipp32u i, j = 0; j < height; ++j) 
            {
                t_dst* pDst_ = pDst;
                const Ipp8u* pSrc2 = (const Ipp8u*)in_pSrc2;
                t_vec  v_acc;

                _mm_prefetch( (const char*)(pSrc + in_SrcPitch), _MM_HINT_NTA ); 
                #pragma loop_count( 16 ) // suggesting compiler this is a short loop to avoid reg spill/fill
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
                        if ( shift ) {
                            v_acc = _mm_adds_epi16( v_acc, v_offset ); 
                            v_acc = _mm_sra_epi16( v_acc, _mm_cvtsi32_si128( shift ) );
                        }
                    }
                    else // 16-bit src, 32-bit accum
                    {
                        if ( shift ) {
                            v_acc  = _mm_add_epi32( v_acc,  v_offset );
                            v_acc2 = _mm_add_epi32( v_acc2, v_offset );

                            __m128i v_shift = _mm_cvtsi32_si128( shift );
                            v_acc  = _mm_sra_epi32( v_acc,  v_shift );
                            v_acc2 = _mm_sra_epi32( v_acc2, v_shift );
                        }

                        v_acc = _mm_packs_epi32( v_acc, v_acc2 );
                    }

                    if ( eAddAverage != MFX_HEVC_PP::AVERAGE_NO )
                    {
                        if ( eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_PIC ) {
                            v_acc2 = _mm_cvtepu8_epi16( _mm_loadl_epi64( (const __m128i*)pSrc2 ) );
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
                        *(Ipp32s*)(pDst_) = _mm_cvtsi128_si32( v_acc );
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
    template class t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp8u, 1 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp8u, 2 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp8u, 3 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp16s, 1 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp16s, 2 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp8u, Ipp16s, 3 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp16s, 1 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp16s, 2 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp16s, 3 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp8u, 1 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp8u, 2 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_LUMA, Ipp16s, Ipp8u, 3 >;

    // interleaved U/V
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 1 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 2 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 3 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 4 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 5 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 6 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp8u, 7 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 1 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 2 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 3 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 4 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 5 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 6 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp8u, Ipp16s, 7 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 1 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 2 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 3 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 4 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 5 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 6 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp16s, 7 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 1 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 2 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 3 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 4 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 5 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 6 >;
    template class t_InterpKernel_intrin< __m128i, TEXT_CHROMA, Ipp16s, Ipp8u, 7 >;

} // end namespace MFX_HEVC_PP

#endif //#ifndef OPT_INTERP_PMUL
#endif //#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
