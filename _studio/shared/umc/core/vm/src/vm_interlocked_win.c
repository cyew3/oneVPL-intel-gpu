//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2010 Intel Corporation. All Rights Reserved.
//

#if defined(_WIN32) || defined(_WIN64)

// Compiler specific
#define _interlockedbittestandset      fake_set
#define _interlockedbittestandreset    fake_reset
#define _interlockedbittestandset64    fake_set64
#define _interlockedbittestandreset64  fake_reset64
#include <intrin.h>
#undef _interlockedbittestandset
#undef _interlockedbittestandreset
#undef _interlockedbittestandset64
#undef _interlockedbittestandreset64

#if !defined (__ICL)
#pragma intrinsic (_InterlockedIncrement16)
#pragma intrinsic (_InterlockedDecrement16)
#endif

#include <vm_interlocked.h>
#include <windows.h>

Ipp16u vm_interlocked_inc16(volatile Ipp16u *pVariable)
{
    return _InterlockedIncrement16((volatile short*) pVariable);
} /* Ipp16u vm_interlocked_inc16(volatile Ipp32u *pVariable) */

Ipp16u vm_interlocked_dec16(volatile Ipp16u *pVariable)
{
    return _InterlockedDecrement16((volatile short*) pVariable);
} /* Ipp16u vm_interlocked_dec16(volatile Ipp32u *pVariable) */

Ipp32u vm_interlocked_inc32(volatile Ipp32u *pVariable)
{
    return InterlockedIncrement((LPLONG) pVariable);
} /* Ipp32u vm_interlocked_inc32(volatile Ipp32u *pVariable) */

Ipp32u vm_interlocked_dec32(volatile Ipp32u *pVariable)
{
    return InterlockedDecrement((LPLONG) pVariable);
} /* Ipp32u vm_interlocked_dec32(volatile Ipp32u *pVariable) */

Ipp32u vm_interlocked_cas32(volatile Ipp32u *pVariable, Ipp32u value_to_exchange, Ipp32u value_to_compare)
{
    return InterlockedCompareExchange((LPLONG) pVariable, value_to_exchange, value_to_compare);
} /* Ipp32u vm_interlocked_cas32(volatile Ipp32u *pVariable, Ipp32u value_to_exchange, Ipp32u value_to_compare) */

Ipp32u vm_interlocked_xchg32(volatile Ipp32u *pVariable, Ipp32u value)
{
    return InterlockedExchange((LPLONG) pVariable, value);
} /* Ipp32u vm_interlocked_xchg32(volatile Ipp32u *pVariable, Ipp32u value); */

#endif /* #if defined(_WIN32) || defined(_WIN64) */
