//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2012 Intel Corporation. All Rights Reserved.
//

#if defined(LINUX32) || defined(__APPLE__)

#include <vm_interlocked.h>

static Ipp16u vm_interlocked_add16(volatile Ipp16u *pVariable, Ipp16u value_to_add)
{
    asm volatile ("lock; xaddw %0,%1"
                  : "=r" (value_to_add), "=m" (*pVariable)
                  : "0" (value_to_add), "m" (*pVariable)
                  : "memory", "cc");
    return value_to_add;
}

static Ipp32u vm_interlocked_add32(volatile Ipp32u *pVariable, Ipp32u value_to_add)
{
    asm volatile ("lock; xaddl %0,%1"
                  : "=r" (value_to_add), "=m" (*pVariable)
                  : "0" (value_to_add), "m" (*pVariable)
                  : "memory", "cc");
    return value_to_add;
}

Ipp16u vm_interlocked_inc16(volatile Ipp16u *pVariable)
{
    return vm_interlocked_add16(pVariable, 1) + 1;
} /* Ipp16u vm_interlocked_inc16(Ipp16u *pVariable) */

Ipp16u vm_interlocked_dec16(volatile Ipp16u *pVariable)
{
    return vm_interlocked_add16(pVariable, (Ipp16u)-1) - 1;
} /* Ipp16u vm_interlocked_dec16(Ipp16u *pVariable) */

Ipp32u vm_interlocked_inc32(volatile Ipp32u *pVariable)
{
    return vm_interlocked_add32(pVariable, 1) + 1;
} /* Ipp32u vm_interlocked_inc32(Ipp32u *pVariable) */

Ipp32u vm_interlocked_dec32(volatile Ipp32u *pVariable)
{
    return vm_interlocked_add32(pVariable, (Ipp32u)-1) - 1;
} /* Ipp32u vm_interlocked_dec32(Ipp32u *pVariable) */

Ipp32u vm_interlocked_cas32(volatile Ipp32u *pVariable, Ipp32u value_to_exchange, Ipp32u value_to_compare)
{
    Ipp32u previous_value;

    asm volatile ("lock; cmpxchgl %1,%2"
                  : "=a" (previous_value)
                  : "r" (value_to_exchange), "m" (*pVariable), "0" (value_to_compare)
                  : "memory", "cc");
    return previous_value;
} /* Ipp32u vm_interlocked_cas32(volatile Ipp32u *pVariable, Ipp32u value_to_exchange, Ipp32u value_to_compare) */

Ipp32u vm_interlocked_xchg32(volatile Ipp32u *pVariable, Ipp32u value)
{
    Ipp32u previous_value = value;

    asm volatile ("lock; xchgl %0,%1"
                  : "=r" (previous_value), "+m" (*pVariable)
                  : "0" (previous_value));
    return previous_value;
} /* Ipp32u vm_interlocked_xchg32(volatile Ipp32u *pVariable, Ipp32u value); */

#else
# pragma warning( disable: 4206 )
#endif /* #if defined(LINUX32) || defined(__APPLE__) */
