//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2018 Intel Corporation. All Rights Reserved.
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

uint16_t vm_interlocked_inc16(volatile uint16_t *pVariable)
{
    return _InterlockedIncrement16((volatile short*) pVariable);
} /* uint16_t vm_interlocked_inc16(volatile uint32_t *pVariable) */

uint16_t vm_interlocked_dec16(volatile uint16_t *pVariable)
{
    return _InterlockedDecrement16((volatile short*) pVariable);
} /* uint16_t vm_interlocked_dec16(volatile uint32_t *pVariable) */

uint32_t vm_interlocked_inc32(volatile uint32_t *pVariable)
{
    return InterlockedIncrement((LPLONG) pVariable);
} /* uint32_t vm_interlocked_inc32(volatile uint32_t *pVariable) */

uint32_t vm_interlocked_dec32(volatile uint32_t *pVariable)
{
    return InterlockedDecrement((LPLONG) pVariable);
} /* uint32_t vm_interlocked_dec32(volatile uint32_t *pVariable) */

uint32_t vm_interlocked_cas32(volatile uint32_t *pVariable, uint32_t value_to_exchange, uint32_t value_to_compare)
{
    return InterlockedCompareExchange((LPLONG) pVariable, value_to_exchange, value_to_compare);
} /* uint32_t vm_interlocked_cas32(volatile uint32_t *pVariable, uint32_t value_to_exchange, uint32_t value_to_compare) */

uint32_t vm_interlocked_xchg32(volatile uint32_t *pVariable, uint32_t value)
{
    return InterlockedExchange((LPLONG) pVariable, value);
} /* uint32_t vm_interlocked_xchg32(volatile uint32_t *pVariable, uint32_t value); */

#endif /* #if defined(_WIN32) || defined(_WIN64) */
