// Copyright (c) 2009-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
