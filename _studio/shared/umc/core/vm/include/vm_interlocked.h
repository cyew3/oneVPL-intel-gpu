//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __VM_INTERLOCKED_H__
#define __VM_INTERLOCKED_H__

#include "vm_types.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* Thread-safe 16-bit variable incrementing */
uint16_t vm_interlocked_inc16(volatile uint16_t *pVariable);

/* Thread-safe 16-bit variable decrementing */
uint16_t vm_interlocked_dec16(volatile uint16_t *pVariable);

/* Thread-safe 32-bit variable incrementing */
uint32_t vm_interlocked_inc32(volatile uint32_t *pVariable);

/* Thread-safe 32-bit variable decrementing */
uint32_t vm_interlocked_dec32(volatile uint32_t *pVariable);

/* Thread-safe 32-bit variable comparing and storing */
uint32_t vm_interlocked_cas32(volatile uint32_t *pVariable, uint32_t with, uint32_t cmp);

/* Thread-safe 32-bit variable exchange */
uint32_t vm_interlocked_xchg32(volatile uint32_t *pVariable, uint32_t val);

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif /* #ifndef __VM_INTERLOCKED_H__ */
