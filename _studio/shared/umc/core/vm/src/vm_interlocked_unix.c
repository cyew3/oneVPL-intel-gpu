//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2018 Intel Corporation. All Rights Reserved.
//

#if defined(LINUX32) || defined(__APPLE__)

#include <vm_interlocked.h>

static uint16_t vm_interlocked_add16(volatile uint16_t *pVariable, uint16_t value_to_add)
{
    asm volatile ("lock; xaddw %0,%1"
                  : "=r" (value_to_add), "=m" (*pVariable)
                  : "0" (value_to_add), "m" (*pVariable)
                  : "memory", "cc");
    return value_to_add;
}

static uint32_t vm_interlocked_add32(volatile uint32_t *pVariable, uint32_t value_to_add)
{
    asm volatile ("lock; xaddl %0,%1"
                  : "=r" (value_to_add), "=m" (*pVariable)
                  : "0" (value_to_add), "m" (*pVariable)
                  : "memory", "cc");
    return value_to_add;
}

uint16_t vm_interlocked_inc16(volatile uint16_t *pVariable)
{
    return vm_interlocked_add16(pVariable, 1) + 1;
} /* uint16_t vm_interlocked_inc16(uint16_t *pVariable) */

uint16_t vm_interlocked_dec16(volatile uint16_t *pVariable)
{
    return vm_interlocked_add16(pVariable, (uint16_t)-1) - 1;
} /* uint16_t vm_interlocked_dec16(uint16_t *pVariable) */

uint32_t vm_interlocked_inc32(volatile uint32_t *pVariable)
{
    return vm_interlocked_add32(pVariable, 1) + 1;
} /* uint32_t vm_interlocked_inc32(uint32_t *pVariable) */

uint32_t vm_interlocked_dec32(volatile uint32_t *pVariable)
{
    return vm_interlocked_add32(pVariable, (uint32_t)-1) - 1;
} /* uint32_t vm_interlocked_dec32(uint32_t *pVariable) */

uint32_t vm_interlocked_cas32(volatile uint32_t *pVariable, uint32_t value_to_exchange, uint32_t value_to_compare)
{
    uint32_t previous_value;

    asm volatile ("lock; cmpxchgl %1,%2"
                  : "=a" (previous_value)
                  : "r" (value_to_exchange), "m" (*pVariable), "0" (value_to_compare)
                  : "memory", "cc");
    return previous_value;
} /* uint32_t vm_interlocked_cas32(volatile uint32_t *pVariable, uint32_t value_to_exchange, uint32_t value_to_compare) */

uint32_t vm_interlocked_xchg32(volatile uint32_t *pVariable, uint32_t value)
{
    uint32_t previous_value = value;

    asm volatile ("lock; xchgl %0,%1"
                  : "=r" (previous_value), "+m" (*pVariable)
                  : "0" (previous_value));
    return previous_value;
} /* uint32_t vm_interlocked_xchg32(volatile uint32_t *pVariable, uint32_t value); */

#else
# pragma warning( disable: 4206 )
#endif /* #if defined(LINUX32) || defined(__APPLE__) */
