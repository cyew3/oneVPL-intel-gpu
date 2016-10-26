//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2009 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_PAR_FILE_UTIL_H__
#define __UMC_PAR_FILE_UTIL_H__

#include "vm_types.h"
#include "vm_file.h"

namespace UMC
{
  vm_char* umc_file_fgets_ascii(vm_char* str, int nchar, vm_file* fd);
} // namespace UMC

#endif /* __UMC_PAR_FILE_UTIL_H__ */
