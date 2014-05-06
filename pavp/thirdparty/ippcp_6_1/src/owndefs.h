/* //////////////////////////////// "owndefs.h" ////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 1999-2009 Intel Corporation. All Rights Reserved.
//
//
//
//  Author(s): Alexey Korchuganov
//             Anatoly Pluzhnikov
//             Igor Astakhov
//             Dmitry Kozhaev
//
//  Created: 27-Jul-1999 20:27
//
*/
#ifndef __OWNDEFS_H__
#define __OWNDEFS_H__

#if defined( IMALLOC )

#include <malloc.h>
#include "i_malloc.h"

extern void *ipp_malloc( size_t size );
extern void *ipp_calloc( size_t num, size_t size );
extern void *ipp_realloc( void* memblock, size_t size );
extern void ipp_free( void* memblock );

#define malloc  i_malloc
#define calloc  i_calloc
#define realloc i_realloc
#define free    i_free

#endif /* IMALLOC */

#include "ippdefs.h"

#if defined(__ICC) || defined(__ECC) || defined( __ICL ) || defined ( __ECL ) || defined(_MSC_VER)
  #define __INLINE static __inline
#elif defined( __GNUC__ )
  #define __INLINE static __inline__
#else
  #define __INLINE static
#endif

#if defined(__ICC) || defined(__ECC) || defined( __ICL ) || defined ( __ECL )
 #define __RESTRICT restrict
#elif !defined( __RESTRICT )
 #define __RESTRICT
#endif

#if defined( IPP_W32DLL )
  #if defined( _MSC_VER ) || defined( __ICL ) || defined ( __ECL )
    #define IPPFUN(type,name,arg) __declspec(dllexport) type __STDCALL name arg
  #else
    #define IPPFUN(type,name,arg)                extern type __STDCALL name arg
  #endif
#else
  #define   IPPFUN(type,name,arg)                extern type __STDCALL name arg
#endif

#if !defined ( _IA64) && (defined ( _WIN64 ) || defined( linux64 ))
#define _IA64
#endif

//structure represeting 128 bit unsigned integer type

typedef struct{
  Ipp64u low;
  Ipp64u high;
}Ipp128u;

#define _IPP_PX 0
#define _IPP_M6 1
#define _IPP_A6 2
#define _IPP_W7 4
#define _IPP_T7 8
#define _IPP_V8 16
#define _IPP_P8 32
#define _IPP_G9 64

#define _IPPXSC_PX 0
#define _IPPXSC_S1 1
#define _IPPXSC_S2 2
#define _IPPXSC_C2 4

#define _IPPLRB_PX 0
#define _IPPLRB_B1 1

#define _IPP64_PX  _IPP_PX
#define _IPP64_I7 64

#define _IPP32E_PX _IPP_PX
#define _IPP32E_M7 32
#define _IPP32E_U8 64
#define _IPP32E_Y8 128
#define _IPP32E_E9 256

#define _IPPLP32_PX _IPP_PX
#define _IPPLP32_S8 1

#define _IPPLP64_PX _IPP_PX
#define _IPPLP64_N8 1

#if defined(__ICC) || defined(__ECC) || defined( __ICL ) || defined ( __ECL ) || (_MSC_VER >= 1300)
    #define __ALIGN8  __declspec (align(8))
    #define __ALIGN16 __declspec (align(16))
#if !defined( OSX32 )
    #define __ALIGN32 __declspec (align(32))
#else
    #define __ALIGN32 __declspec (align(16))
#endif
    #define __ALIGN64 __declspec (align(64))
#else
    #define __ALIGN8
    #define __ALIGN16
    #define __ALIGN32
    #define __ALIGN64
#endif

#if defined ( _M6 )
  #define _IPP    _IPP_M6
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _A6 )
  #define _IPP    _IPP_A6
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _W7 )
  #define _IPP    _IPP_W7
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _T7 )
  #define _IPP    _IPP_T7
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _V8 )
  #define _IPP    _IPP_V8
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _P8 )
  #define _IPP    _IPP_P8
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _G9 )
  #define _IPP    _IPP_G9
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _M7 )
  #define _IPP    _IPP_PX
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_M7
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _U8 )
  #define _IPP    _IPP_PX
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_U8
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _Y8 )
  #define _IPP    _IPP_PX
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_Y8
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _E9 )
  #define _IPP    _IPP_PX
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_E9
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _I7 )
  #define _IPP    _IPP_PX
  #define _IPP64  _IPP64_I7
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _B1 )
  #define _IPP    _IPP_PX
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_B1
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _S8 )
  #define _IPP    _IPP_V8
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_S8
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _N8 )
  #define _IPP    _IPP_PX
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_U8
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_N8

#elif defined( _StrongARM )
  #define _IPP    _IPP_PX
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_S1
  #define _PX    /* This string must be removed after migation period */
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _XScale )
  #define _IPP    _IPP_PX
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_S2
  #define _PX    /* This string must be removed after migation period */
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#elif defined( _XScale_Concan )
  #define _IPP    _IPP_PX
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_C2
  #define _PX    /* This string must be removed after migation period */
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#else
  #define _IPP    _IPP_PX
  #define _IPP64  _IPP64_PX
  #define _IPP32E _IPP32E_PX
  #define _IPPXSC _IPPXSC_PX
  #define _IPPLRB _IPPLRB_PX
  #define _IPPLP32 _IPPLP32_PX
  #define _IPPLP64 _IPPLP64_PX

#endif


#define _IPP_ARCH_IA32    1
#define _IPP_ARCH_IA64    2
#define _IPP_ARCH_EM64T   4
#define _IPP_ARCH_XSC     8
#define _IPP_ARCH_LRB     16
#define _IPP_ARCH_LP32    32
#define _IPP_ARCH_LP64    64

#if defined ( _ARCH_IA32 )
  #define _IPP_ARCH    _IPP_ARCH_IA32

#elif defined( _ARCH_IA64 )
  #define _IPP_ARCH    _IPP_ARCH_IA64

#elif defined( _ARCH_EM64T )
  #define _IPP_ARCH    _IPP_ARCH_EM64T

#elif defined( _ARCH_XSC )
  #define _IPP_ARCH    _IPP_ARCH_XSC

#elif defined( _ARCH_LRB )
  #define _IPP_ARCH    _IPP_ARCH_LRB

#elif defined( _ARCH_LP32 )
  #define _IPP_ARCH    _IPP_ARCH_LP32

#elif defined( _ARCH_LP64 )
  #define _IPP_ARCH    _IPP_ARCH_LP64

#else
  #if defined(_M_AMD64) || defined(__x86_64) || defined(__x86_64__)
    #define _IPP_ARCH    _IPP_ARCH_EM64T

  #elif defined(_M_IA64) || defined(__ia64) || defined(__ia64__)
    #define _IPP_ARCH    _IPP_ARCH_IA64

  #elif defined(_M_ARM) || defined(_ARM_) || defined(ARM) || defined(__arm__)
    #define _IPP_ARCH    _IPP_ARCH_XSC

  #else
    #define _IPP_ARCH    _IPP_ARCH_IA32

  #endif
#endif

#if ((_IPP_ARCH == _IPP_ARCH_IA32) || (_IPP_ARCH == _IPP_ARCH_XSC) || (_IPP_ARCH == _IPP_ARCH_LP32))
__INLINE
Ipp32s IPP_INT_PTR( const void* ptr )  {
    union {
        void*   Ptr;
        Ipp32s  Int;
    } dd;
    dd.Ptr = (void*)ptr;
    return dd.Int;
}

__INLINE
Ipp32u IPP_UINT_PTR( const void* ptr )  {
    union {
        void*   Ptr;
        Ipp32u  Int;
    } dd;
    dd.Ptr = (void*)ptr;
    return dd.Int;
}
#elif ((_IPP_ARCH == _IPP_ARCH_IA64) || (_IPP_ARCH == _IPP_ARCH_EM64T) || (_IPP_ARCH == _IPP_ARCH_LRB) || (_IPP_ARCH == _IPP_ARCH_LP64))
__INLINE
Ipp64s IPP_INT_PTR( const void* ptr )  {
    union {
        void*   Ptr;
        Ipp64s  Int;
    } dd;
    dd.Ptr = (void*)ptr;
    return dd.Int;
}

__INLINE
Ipp64u IPP_UINT_PTR( const void* ptr )  {
    union {
        void*    Ptr;
        Ipp64u   Int;
    } dd;
    dd.Ptr = (void*)ptr;
    return dd.Int;
}
#else
  #define IPP_INT_PTR( ptr )  ( (long)(ptr) )
  #define IPP_UINT_PTR( ptr ) ( (unsigned long)(ptr) )
#endif

#define IPP_ALIGN_TYPE(type, align) ((align)/sizeof(type)-1)
#define IPP_BYTES_TO_ALIGN(ptr, align) ((-(IPP_INT_PTR(ptr)&((align)-1)))&((align)-1))
#define IPP_ALIGNED_PTR(ptr, align) (void*)( (unsigned char*)(ptr) + (IPP_BYTES_TO_ALIGN( ptr, align )) )

#if (_IPP_ARCH == _IPP_ARCH_LRB)
#define IPP_MALLOC_ALIGNED_BYTES  64
#else
#define IPP_MALLOC_ALIGNED_BYTES  32
#endif

#define IPP_MALLOC_ALIGNED_8BYTES   8
#define IPP_MALLOC_ALIGNED_16BYTES 16
#define IPP_MALLOC_ALIGNED_32BYTES 32

#define IPP_ALIGNED_ARRAY(align,arrtype,arrname,arrlength)\
 char arrname##AlignedArrBuff[sizeof(arrtype)*(arrlength)+IPP_ALIGN_TYPE(char, align)];\
 arrtype *arrname = (arrtype*)IPP_ALIGNED_PTR(arrname##AlignedArrBuff,align)

#if defined( __cplusplus )
extern "C" {
#endif

/* /////////////////////////////////////////////////////////////////////////////

           IPP Context Identification

  /////////////////////////////////////////////////////////////////////////// */

#define IPP_CONTEXT( a, b, c, d) \
            (int)(((unsigned)(a) << 24) | ((unsigned)(b) << 16) | \
            ((unsigned)(c) << 8) | (unsigned)(d))

typedef enum {
    idCtxUnknown = 0,
    idCtxFFT_C_16sc,
    idCtxFFT_C_16s,
    idCtxFFT_R_16s,
    idCtxFFT_C_32fc,
    idCtxFFT_C_32f,
    idCtxFFT_R_32f,
    idCtxFFT_C_64fc,
    idCtxFFT_C_64f,
    idCtxFFT_R_64f,
    idCtxDFT_C_16sc,
    idCtxDFT_C_16s,
    idCtxDFT_R_16s,
    idCtxDFT_C_32fc,
    idCtxDFT_C_32f,
    idCtxDFT_R_32f,
    idCtxDFT_C_64fc,
    idCtxDFT_C_64f,
    idCtxDFT_R_64f,
    idCtxDCTFwd_16s,
    idCtxDCTInv_16s,
    idCtxDCTFwd_32f,
    idCtxDCTInv_32f,
    idCtxDCTFwd_64f,
    idCtxDCTInv_64f,
    idCtxFFT2D_C_32fc,
    idCtxFFT2D_R_32f,
    idCtxDFT2D_C_32fc,
    idCtxDFT2D_R_32f,
    idCtxFFT2D_R_32s,
    idCtxDFT2D_R_32s,
    idCtxDCT2DFwd_32f,
    idCtxDCT2DInv_32f,
    idCtxMoment64f,
    idCtxMoment64s,
    idCtxRandUni_8u,
    idCtxRandUni_16s,
    idCtxRandUni_32f,
    idCtxRandGauss_8u,
    idCtxRandGauss_16s,
    idCtxRandGauss_32f,
    idCtxWTFwd_32f,
    idCtxWTFwd_8u32f,
    idCtxWTFwd_8s32f,
    idCtxWTFwd_16u32f,
    idCtxWTFwd_16s32f,
    idCtxWTFwd2D_32f_C1R,
    idCtxWTInv2D_32f_C1R,
    idCtxWTFwd2D_32f_C3R,
    idCtxWTInv2D_32f_C3R,
    idCtxWTInv_32f,
    idCtxWTInv_32f8u,
    idCtxWTInv_32f8s,
    idCtxWTInv_32f16u,
    idCtxWTInv_32f16s,
    idCtxMDCTFwd_32f,
    idCtxMDCTInv_32f,
    idCtxMDCTFwd_16s,
    idCtxFIRBlock_32f,
    idCtxFDP_32f,
    idCtxRLMS_32f       = IPP_CONTEXT( 'L', 'M', 'S', '1'),
    idCtxRLMS32f_16s    = IPP_CONTEXT( 'L', 'M', 'S', 0 ),
    idCtxIIRAR_32f      = IPP_CONTEXT( 'I', 'I', '0', '1'),
    idCtxIIRBQ_32f      = IPP_CONTEXT( 'I', 'I', '0', '2'),
    idCtxIIRAR_32fc     = IPP_CONTEXT( 'I', 'I', '0', '3'),
    idCtxIIRBQ_32fc     = IPP_CONTEXT( 'I', 'I', '0', '4'),
    idCtxIIRAR32f_16s   = IPP_CONTEXT( 'I', 'I', '0', '5'),
    idCtxIIRBQ32f_16s   = IPP_CONTEXT( 'I', 'I', '0', '6'),
    idCtxIIRAR32fc_16sc = IPP_CONTEXT( 'I', 'I', '0', '7'),
    idCtxIIRBQ32fc_16sc = IPP_CONTEXT( 'I', 'I', '0', '8'),
    idCtxIIRAR32s_16s   = IPP_CONTEXT( 'I', 'I', '0', '9'),
    idCtxIIRBQ32s_16s   = IPP_CONTEXT( 'I', 'I', '1', '0'),
    idCtxIIRAR32sc_16sc = IPP_CONTEXT( 'I', 'I', '1', '1'),
    idCtxIIRBQ32sc_16sc = IPP_CONTEXT( 'I', 'I', '1', '2'),
    idCtxIIRAR_64f      = IPP_CONTEXT( 'I', 'I', '1', '3'),
    idCtxIIRBQ_64f      = IPP_CONTEXT( 'I', 'I', '1', '4'),
    idCtxIIRAR_64fc     = IPP_CONTEXT( 'I', 'I', '1', '5'),
    idCtxIIRBQ_64fc     = IPP_CONTEXT( 'I', 'I', '1', '6'),
    idCtxIIRAR64f_32f   = IPP_CONTEXT( 'I', 'I', '1', '7'),
    idCtxIIRBQ64f_32f   = IPP_CONTEXT( 'I', 'I', '1', '8'),
    idCtxIIRAR64fc_32fc = IPP_CONTEXT( 'I', 'I', '1', '9'),
    idCtxIIRBQ64fc_32fc = IPP_CONTEXT( 'I', 'I', '2', '0'),
    idCtxIIRAR64f_32s   = IPP_CONTEXT( 'I', 'I', '2', '1'),
    idCtxIIRBQ64f_32s   = IPP_CONTEXT( 'I', 'I', '2', '2'),
    idCtxIIRAR64fc_32sc = IPP_CONTEXT( 'I', 'I', '2', '3'),
    idCtxIIRBQ64fc_32sc = IPP_CONTEXT( 'I', 'I', '2', '4'),
    idCtxIIRAR64f_16s   = IPP_CONTEXT( 'I', 'I', '2', '5'),
    idCtxIIRBQ64f_16s   = IPP_CONTEXT( 'I', 'I', '2', '6'),
    idCtxIIRAR64fc_16sc = IPP_CONTEXT( 'I', 'I', '2', '7'),
    idCtxIIRBQ64fc_16sc = IPP_CONTEXT( 'I', 'I', '2', '8'),
    idCtxIIRBQDF1_32f   = IPP_CONTEXT( 'I', 'I', '2', '9'),
    idCtxIIRBQDF164f_32s= IPP_CONTEXT( 'I', 'I', '3', '0'),
    idCtxFIRSR_32f      = IPP_CONTEXT( 'F', 'I', '0', '1'),
    idCtxFIRSR_32fc     = IPP_CONTEXT( 'F', 'I', '0', '2'),
    idCtxFIRMR_32f      = IPP_CONTEXT( 'F', 'I', '0', '3'),
    idCtxFIRMR_32fc     = IPP_CONTEXT( 'F', 'I', '0', '4'),
    idCtxFIRSR32f_16s   = IPP_CONTEXT( 'F', 'I', '0', '5'),
    idCtxFIRSR32fc_16sc = IPP_CONTEXT( 'F', 'I', '0', '6'),
    idCtxFIRMR32f_16s   = IPP_CONTEXT( 'F', 'I', '0', '7'),
    idCtxFIRMR32fc_16sc = IPP_CONTEXT( 'F', 'I', '0', '8'),
    idCtxFIRSR32s_16s   = IPP_CONTEXT( 'F', 'I', '0', '9'),
    idCtxFIRSR32sc_16sc = IPP_CONTEXT( 'F', 'I', '1', '0'),
    idCtxFIRMR32s_16s   = IPP_CONTEXT( 'F', 'I', '1', '1'),
    idCtxFIRMR32sc_16sc = IPP_CONTEXT( 'F', 'I', '1', '2'),
    idCtxFIRSR_64f      = IPP_CONTEXT( 'F', 'I', '1', '3'),
    idCtxFIRSR_64fc     = IPP_CONTEXT( 'F', 'I', '1', '4'),
    idCtxFIRMR_64f      = IPP_CONTEXT( 'F', 'I', '1', '5'),
    idCtxFIRMR_64fc     = IPP_CONTEXT( 'F', 'I', '1', '6'),
    idCtxFIRSR64f_32f   = IPP_CONTEXT( 'F', 'I', '1', '7'),
    idCtxFIRSR64fc_32fc = IPP_CONTEXT( 'F', 'I', '1', '8'),
    idCtxFIRMR64f_32f   = IPP_CONTEXT( 'F', 'I', '1', '9'),
    idCtxFIRMR64fc_32fc = IPP_CONTEXT( 'F', 'I', '2', '0'),
    idCtxFIRSR64f_32s   = IPP_CONTEXT( 'F', 'I', '2', '1'),
    idCtxFIRSR64fc_32sc = IPP_CONTEXT( 'F', 'I', '2', '2'),
    idCtxFIRMR64f_32s   = IPP_CONTEXT( 'F', 'I', '2', '3'),
    idCtxFIRMR64fc_32sc = IPP_CONTEXT( 'F', 'I', '2', '4'),
    idCtxFIRSR64f_16s   = IPP_CONTEXT( 'F', 'I', '2', '5'),
    idCtxFIRSR64fc_16sc = IPP_CONTEXT( 'F', 'I', '2', '6'),
    idCtxFIRMR64f_16s   = IPP_CONTEXT( 'F', 'I', '2', '7'),
    idCtxFIRMR64fc_16sc = IPP_CONTEXT( 'F', 'I', '2', '8'),
    idCtxFIRSR_16s      = IPP_CONTEXT( 'F', 'I', '2', '9'),
    idCtxFIRMR_16s      = IPP_CONTEXT( 'F', 'I', '3', '0'),
    idCtxFIRSRStream_16s= IPP_CONTEXT( 'F', 'I', '3', '1'),
    idCtxFIRMRStream_16s= IPP_CONTEXT( 'F', 'I', '3', '2'),
    idCtxFIRSRStream_32f= IPP_CONTEXT( 'F', 'I', '3', '3'),
    idCtxFIRMRStream_32f= IPP_CONTEXT( 'F', 'I', '3', '4'),
    idCtxRLMS32s_16s    = IPP_CONTEXT( 'L', 'M', 'S', 'R'),
    idCtxCLMS32s_16s    = IPP_CONTEXT( 'L', 'M', 'S', 'C'),
    idCtxEncode_JPEG2K,
    idCtxDES            = IPP_CONTEXT( ' ', 'D', 'E', 'S'),
    idCtxBlowfish       = IPP_CONTEXT( ' ', ' ', 'B', 'F'),
    idCtxRijndael       = IPP_CONTEXT( ' ', 'R', 'I', 'J'),
    idCtxTwofish        = IPP_CONTEXT( ' ', ' ', 'T', 'F'),
    idCtxARCFOUR        = IPP_CONTEXT( ' ', 'R', 'C', '4'),
    idCtxRC564          = IPP_CONTEXT( 'R', 'C', '5', '1'),
    idCtxRC5128         = IPP_CONTEXT( 'R', 'C', '5', '2'),
    idCtxSHA1           = IPP_CONTEXT( 'S', 'H', 'S', '1'),
    idCtxSHA224         = IPP_CONTEXT( 'S', 'H', 'S', '3'),
    idCtxSHA256         = IPP_CONTEXT( 'S', 'H', 'S', '2'),
    idCtxSHA384         = IPP_CONTEXT( 'S', 'H', 'S', '4'),
    idCtxSHA512         = IPP_CONTEXT( 'S', 'H', 'S', '5'),
    idCtxMD5            = IPP_CONTEXT( ' ', 'M', 'D', '5'),
    idCtxHMAC           = IPP_CONTEXT( 'H', 'M', 'A', 'C'),
    idCtxDAA            = IPP_CONTEXT( ' ', 'D', 'A', 'A'),
    idCtxBigNum         = IPP_CONTEXT( 'B', 'I', 'G', 'N'),
    idCtxMontgomery     = IPP_CONTEXT( 'M', 'O', 'N', 'T'),
    idCtxPrimeNumber    = IPP_CONTEXT( 'P', 'R', 'I', 'M'),
    idCtxPRNG           = IPP_CONTEXT( 'P', 'R', 'N', 'G'),
    idCtxRSA            = IPP_CONTEXT( ' ', 'R', 'S', 'A'),
    idCtxDSA            = IPP_CONTEXT( ' ', 'D', 'S', 'A'),
    idCtxECCP           = IPP_CONTEXT( ' ', 'E', 'C', 'P'),
    idCtxECCB           = IPP_CONTEXT( ' ', 'E', 'C', 'B'),
    idCtxECCPPoint      = IPP_CONTEXT( 'P', 'E', 'C', 'P'),
    idCtxECCBPoint      = IPP_CONTEXT( 'P', 'E', 'C', 'B'),
    idCtxDH             = IPP_CONTEXT( ' ', ' ', 'D', 'H'),
    idCtxDLP            = IPP_CONTEXT( ' ', 'D', 'L', 'P'),
    idCtxCMAC           = IPP_CONTEXT( 'C', 'M', 'A', 'C'),
    idCtxRFFT2_8u,
    idCtxHilbert_32f32fc,
    idCtxHilbert_16s32fc,
    idCtxHilbert_16s16sc,
    idCtxTone_16s,
    idCtxTriangle_16s,
    idCtxDFTOutOrd_C_32fc,
    idCtxDFTOutOrd_C_64fc,
    idCtxFFT_C_32sc,
    idCtxFFT_C_32s,
    idCtxFFT_R_32s,
    idCtxFFT_R_16s32s,
    idCtxDecodeProgr_JPEG2K,
    idCtxWarp_MPEG4,
    idCtxQuantInvIntra_MPEG4,
    idCtxQuantInvInter_MPEG4,
    idCtxQuantIntra_MPEG4,
    idCtxQuantInter_MPEG4,
    idCtxAnalysisFilter_SBR_C_32f32fc,
    idCtxAnalysisFilter_SBR_C_32f,
    idCtxAnalysisFilter_SBR_R_32f,
    idCtxSynthesisFilter_SBR_C_32fc32f,
    idCtxSynthesisFilter_SBR_C_32f,
    idCtxSynthesisFilter_SBR_R_32f,
    idCtxSynthesisDownFilter_SBR_C_32fc32f,
    idCtxSynthesisDownFilter_SBR_C_32f,
    idCtxSynthesisDownFilter_SBR_R_32f,
    idCtxVLCEncode,
    idCtxVLCDecode,
    idCtxAnalysisFilter_SBR_C_32s32sc,
    idCtxAnalysisFilter_SBR_R_32s,
    idCtxSynthesisFilter_SBR_C_32sc32s,
    idCtxSynthesisFilter_SBR_R_32s,
    idCtxSynthesisDownFilter_SBR_C_32sc32s,
    idCtxSynthesisDownFilter_SBR_R_32s,
    idCtxSynthesisFilter_PQMF_MP3_32f,
    idCtxAnalysisFilter_PQMF_MP3_32f,
    idCtxResampleRow,
    idCtxAnalysisFilter_SBR_Enc_C_32f32fc,
    idCtxSynthesisFilter_DTS_32f,
    idCtxFilterBilateralGauss_8u,
    idCtxFilterBilateralGaussFast_8u,
    idCtxBGF,
    idCtxPolyGF,
    idCtxRSenc,
    idCtxRSdec,
    idCtxSnow3g        = IPP_CONTEXT( 'S', 'n', 'o', 'w'),
    idCtxSnow3gF8,
    idCtxSnow3gF9,
    idCtxKasumi        = IPP_CONTEXT( 'K', 'a', 's', 'u'),
    idCtxKasumiF8,
    idCtxKasumiF9,
    idCtxResizeHannFilter_8u,
    idCtxResizeLanczosFilter_8u,
    idCtxAESXCBC,
    idCtxAESCCM,
    idCtxAESGCM,
    idCtxMsgCatalog,
    idCtxGFP,
    idCtxGFPE,
    idCtxGFPX,
    idCtxGFPXE,
    idCtxGFPXQX,
    idCtxGFPXQXE,
    idCtxGFPEC,
    idCtxGFPPoint,
    idCtxGFPXEC,
    idCtxGFPXECPoint,
    idCtxPairing
} IppCtxId;




/* /////////////////////////////////////////////////////////////////////////////
           Helpers
  /////////////////////////////////////////////////////////////////////////// */

#define IPP_NOERROR_RET()  return ippStsNoErr
#define IPP_ERROR_RET( ErrCode )  return (ErrCode)

#ifdef _IPP_DEBUG

    #define IPP_BADARG_RET( expr, ErrCode )\
                {if (expr) { IPP_ERROR_RET( ErrCode ); }}

#else

    #define IPP_BADARG_RET( expr, ErrCode )

#endif


    #define IPP_BAD_SIZE_RET( n )\
                IPP_BADARG_RET( (n)<=0, ippStsSizeErr )

    #define IPP_BAD_STEP_RET( n )\
                IPP_BADARG_RET( (n)<=0, ippStsStepErr )

    #define IPP_BAD_PTR1_RET( ptr )\
                IPP_BADARG_RET( NULL==(ptr), ippStsNullPtrErr )

    #define IPP_BAD_PTR2_RET( ptr1, ptr2 )\
                IPP_BADARG_RET(((NULL==(ptr1))||(NULL==(ptr2))), ippStsNullPtrErr)

    #define IPP_BAD_PTR3_RET( ptr1, ptr2, ptr3 )\
                IPP_BADARG_RET(((NULL==(ptr1))||(NULL==(ptr2))||(NULL==(ptr3))),\
                                                         ippStsNullPtrErr)

    #define IPP_BAD_PTR4_RET( ptr1, ptr2, ptr3, ptr4 )\
                {IPP_BAD_PTR2_RET( ptr1, ptr2 ); IPP_BAD_PTR2_RET( ptr3, ptr4 )}


/* ////////////////////////////////////////////////////////////////////////// */
/*                              internal messages                             */

#define MSG_LOAD_DLL_ERR (-9700) /* Error at loading of %s library */
#define MSG_NO_DLL       (-9701) /* No DLLs were found in the Waterfall procedure */
#define MSG_NO_SHARED    (-9702) /* No shared libraries were found in the Waterfall procedure */

/* ////////////////////////////////////////////////////////////////////////// */


typedef union { /* double precision */
    Ipp64s  hex;
    Ipp64f   fp;
} IppFP_64f;

typedef union { /* single precision */
    Ipp32s  hex;
    Ipp32f   fp;
} IppFP_32f;


extern const IppFP_32f ippConstantOfNAN_32f;
extern const IppFP_64f ippConstantOfNAN_64f;

extern const IppFP_32f ippConstantOfINF_32f;
extern const IppFP_64f ippConstantOfINF_64f;
extern const IppFP_32f ippConstantOfINF_NEG_32f;
extern const IppFP_64f ippConstantOfINF_NEG_64f;

#define NAN_32F      (ippConstantOfNAN_32f.fp)
#define NAN_64F      (ippConstantOfNAN_64f.fp)
#define INF_32F      (ippConstantOfINF_32f.fp)
#define INF_64F      (ippConstantOfINF_64f.fp)
#define INF_NEG_32F  (ippConstantOfINF_NEG_32f.fp)
#define INF_NEG_64F  (ippConstantOfINF_NEG_64f.fp)

/* ////////////////////////////////////////////////////////////////////////// */

typedef enum {
    ippac =  0,
    ippcc =  1,
    ippch =  2,
    ippcp =  3,
    ippcv =  4,
    ippdc =  5,
    ippdi =  6,
    ippgen = 7,
    ippi =   8,
    ippj =   9,
    ippm =  10,
    ippr =  11,
    ipps =  12,
    ippsc = 13,
    ippsr = 14,
    ippvc = 15,
    ippvm = 16,
    ippnomore
} IppDomain;

int __CDECL ownGetNumThreads( void );
int __CDECL ownGetFeature( Ipp64u MaskOfFeature );
IppStatus __STDCALL ippIsCpuEnabled ( IppCpuType cpu );

#ifdef _IPP_DYNAMIC
typedef IppStatus (__STDCALL *DYN_RELOAD)( IppCpuType );
void __CDECL ownRegisterLib( IppDomain, DYN_RELOAD );
void __CDECL ownUnregisterLib( IppDomain );
#endif

/*     the number of threads available for any ipp function that uses OMP;     */
/* at the ippxx.dll loading time is equal to the number of logical processors, */
/*  and can be changed ONLY externally by library user to any desired number   */
/*               by means of ippSetNumThreads() function                       */
#define IPP_GET_NUM_THREADS() ( ownGetNumThreads() )
#define IPP_OMP_NUM_THREADS() num_threads( IPP_GET_NUM_THREADS() )
#define IPP_OMP_LIMIT_MAX_NUM_THREADS(n)  num_threads( IPP_MIN(IPP_GET_NUM_THREADS(),(n)))


/* ////////////////////////////////////////////////////////////////////////// */

/* Define NULL pointer value */
#ifndef NULL
#ifdef  __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

#define UNREFERENCED_PARAMETER(p) (p)=(p)

#if defined( _IPP_MARK_LIBRARY )
static char G[] = {73, 80, 80, 71, 101, 110, 117, 105, 110, 101, 243, 193, 210, 207, 215};
#endif


#define STR2(x)           #x
#define STR(x)       STR2(x)
#define MESSAGE( desc )\
     message(__FILE__ "(" STR(__LINE__) "):" #desc)

/*
// endian definition
*/
#define IPP_LITTLE_ENDIAN  (0)
#define IPP_BIG_ENDIAN     (1)

#if defined( _IPP_LE )
   #define IPP_ENDIAN IPP_LITTLE_ENDIAN

#elif defined( _IPP_BE )
   #define IPP_ENDIAN IPP_BIG_ENDIAN

#else
   #if defined( __ARMEB__ )
     #define IPP_ENDIAN IPP_BIG_ENDIAN

   #else
     #define IPP_ENDIAN IPP_LITTLE_ENDIAN

   #endif
#endif


/* ////////////////////////////////////////////////////////////////////////// */

/* intrinsics */
#if (_IPPXSC >= _IPPXSC_C2)
    #if defined(__INTEL_COMPILER)
        #include "mmintrin.h"
    #endif
#elif (_IPP64 >= _IPP64_I7)
    #if defined(__INTEL_COMPILER)
        #define _SSE2_INTEGER_FOR_ITANIUM
        #include "emmintrin.h"
    #endif
#elif (_IPP >= _IPP_A6) || (_IPP32E >= _IPP32E_M7)
    #if defined(__INTEL_COMPILER) || (_MSC_VER >= 1300)
        #if (_IPP == _IPP_A6)
            #include "xmmintrin.h"
        #elif (_IPP == _IPP_W7)
            #if defined(__INTEL_COMPILER)
              #include "emmintrin.h"
            #else
              #undef _W7
              #include "emmintrin.h"
              #define _W7
            #endif
            #define _mm_loadu _mm_loadu_si128
        #elif (_IPP == _IPP_T7) || (_IPP32E == _IPP32E_M7)
            #if defined(__INTEL_COMPILER)
                #include "pmmintrin.h"
                #define _mm_loadu _mm_lddqu_si128
            #elif (_MSC_FULL_VER >= 140050110)
                #include "intrin.h"
                #define _mm_loadu _mm_lddqu_si128
            #elif (_MSC_FULL_VER < 140050110)
                #include "emmintrin.h"
                #define _mm_loadu _mm_loadu_si128
            #endif
        #elif (_IPP == _IPP_V8) || (_IPP32E == _IPP32E_U8)
            #if defined(__INTEL_COMPILER)
                #include "tmmintrin.h"
                #define _mm_loadu _mm_lddqu_si128
            #elif (_MSC_FULL_VER >= 140050110)
                #include "intrin.h"
                #define _mm_loadu _mm_lddqu_si128
            #elif (_MSC_FULL_VER < 140050110)
                #include "emmintrin.h"
                #define _mm_loadu _mm_loadu_si128
            #endif
        #elif (_IPP >= _IPP_P8) || (_IPP32E >= _IPP32E_Y8)
            #if defined(__INTEL_COMPILER)
                #include "smmintrin.h"
                #define _mm_loadu _mm_lddqu_si128
            #elif (_MSC_FULL_VER >= 140050110)
                #include "intrin.h"
                #define _mm_loadu _mm_lddqu_si128
            #elif (_MSC_FULL_VER < 140050110)
                #include "emmintrin.h"
                #define _mm_loadu _mm_loadu_si128
            #endif
        /* It's for SNB and IVB now, then it will be separated */
        #elif (_IPP >= _IPP_G9) || (_IPP32E >= _IPP32E_E9)
            #if defined(__INTEL_COMPILER)
                #include "fmmintrin.h"
            #endif
        #endif
    #endif
#elif (_IPPLP32 >= _IPPLP32_S8) || (_IPPLP64 >= _IPPLP64_N8)
    #if defined(__INTEL_COMPILER)
        #include "tmmintrin.h"
        #define _mm_loadu _mm_lddqu_si128
    #elif (_MSC_FULL_VER >= 140050110)
        #include "intrin.h"
        #define _mm_loadu _mm_lddqu_si128
    #elif (_MSC_FULL_VER < 140050110)
        #include "emmintrin.h"
        #define _mm_loadu _mm_loadu_si128
    #endif
#endif


/* ////////////////////////////////////////////////////////////////////////// */

/* header depended on the architecture */

#ifndef __DEPENDENCESP_H__
  #include "dependencesp.h"
#endif

#ifndef __DEPENDENCEIP_H__
  #include "dependenceip.h"
#endif


#if defined( __cplusplus )
}
#endif

#endif /* __OWNDEFS_H__ */
/* //////////////////////// End of file "owndefs.h" ///////////////////////// */
