/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2005-2016 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#ifndef __MFX_ITT_TRACE_H__
#define __MFX_ITT_TRACE_H__

#ifdef ITT_SUPPORT
#include <ittnotify.h>
#endif

#ifdef ITT_SUPPORT
#define MFX_ITT_TASK_BEGIN(x) \
  do { \
    __itt_domain* domain = mfx_itt_get_domain(); \
    if (domain) \
      __itt_task_begin(domain, __itt_null, __itt_null, __itt_string_handle_create(x)); \
  } while(0)
#define MFX_ITT_TASK_END() \
  do { \
    __itt_domain* domain = mfx_itt_get_domain(); \
    if (domain) __itt_task_end(domain); \
  } while(0)
#else
#define MFX_ITT_TASK_BEGIN(x)
#define MFX_ITT_TASK_END()
#endif

#ifdef ITT_SUPPORT

static inline __itt_domain* mfx_itt_get_domain() {
  static __itt_domain *domain = NULL;

  if (!domain) domain = __itt_domain_create("MFX_SAMPLES");
  return domain;
}

#endif

#endif //__MFX_ITT_TRACE_H__
