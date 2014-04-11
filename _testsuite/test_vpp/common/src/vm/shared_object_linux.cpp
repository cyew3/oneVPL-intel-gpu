/* ****************************************************************************** *\

 INTEL CORPORATION PROPRIETARY INFORMATION
 This software is supplied under the terms of a license agreement or nondisclosure
 agreement with Intel Corporation and may not be copied or disclosed except in
 accordance with the terms of that agreement
 Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if !defined(_WIN32) && !defined(_WIN64)

#include "vm/so_defs.h"
#include <dlfcn.h>

msdk_so_handle msdk_so_load(const msdk_char *file_name)
{
    if (!file_name) return NULL;
    return (msdk_so_handle) dlopen(file_name, RTLD_LAZY);
}

msdk_func_pointer msdk_so_get_addr(msdk_so_handle handle, const char *func_name)
{
    if (!handle) return NULL;
    return (msdk_func_pointer)dlsym(handle, func_name);
}

void msdk_so_free(msdk_so_handle handle)
{
    if (!handle) return;
    dlclose(handle);
}

#endif // #if !defined(_WIN32) && !defined(_WIN64)
