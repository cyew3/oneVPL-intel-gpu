/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __ATOMIC_DEFS_H__
#define __ATOMIC_DEFS_H__

#include "mfxdefs.h"

/* Thread-safe 16-bit variable incrementing */
mfxU16 msdk_atomic_inc16(volatile mfxU16 *pVariable);

/* Thread-safe 16-bit variable decrementing */
mfxU16 msdk_atomic_dec16(volatile mfxU16 *pVariable);

#endif // #ifndef __ATOMIC_DEFS_H__
