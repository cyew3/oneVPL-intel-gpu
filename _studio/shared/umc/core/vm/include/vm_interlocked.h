//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2011 Intel Corporation. All Rights Reserved.
//

#ifndef __VM_INTERLOCKED_H__
#define __VM_INTERLOCKED_H__

#include "vm_types.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* Thread-safe 16-bit variable incrementing */
Ipp16u vm_interlocked_inc16(volatile Ipp16u *pVariable);

/* Thread-safe 16-bit variable decrementing */
Ipp16u vm_interlocked_dec16(volatile Ipp16u *pVariable);

/* Thread-safe 32-bit variable incrementing */
Ipp32u vm_interlocked_inc32(volatile Ipp32u *pVariable);

/* Thread-safe 32-bit variable decrementing */
Ipp32u vm_interlocked_dec32(volatile Ipp32u *pVariable);

/* Thread-safe 32-bit variable comparing and storing */
Ipp32u vm_interlocked_cas32(volatile Ipp32u *pVariable, Ipp32u with, Ipp32u cmp);

/* Thread-safe 32-bit variable exchange */
Ipp32u vm_interlocked_xchg32(volatile Ipp32u *pVariable, Ipp32u val);

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif /* #ifndef __VM_INTERLOCKED_H__ */
