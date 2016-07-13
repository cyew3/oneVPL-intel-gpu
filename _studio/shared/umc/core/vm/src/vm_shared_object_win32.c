/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2016 Intel Corporation. All Rights Reserved.
//
*/

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include "vm_types.h"
#include "vm_shared_object.h"

vm_so_handle vm_so_load(const vm_char *so_file_name)
{
    //mfx_trace_get_reg_string
    HKEY hkey;
    vm_so_handle handle = NULL;
    TCHAR path[MAX_PATH] = _T("");
    DWORD size = MAX_PATH;

    /* check error(s) */
    if (NULL == so_file_name)
        return NULL;

    handle = ((vm_so_handle)LoadLibraryExW(so_file_name, NULL, 0));

    /* try load from DriverStore */
    /* WARNING, this is temporary solution */
    if (handle == NULL)
    {
        if( RegOpenKeyEx( HKEY_LOCAL_MACHINE,
        _T("SOFTWARE\\Intel\\MDF"),
        0,
        KEY_READ,
        &hkey) == ERROR_SUCCESS)
        {
            if (ERROR_SUCCESS == RegQueryValueEx(hkey, _T("DriverStorePath"), 0, NULL, (LPBYTE)path, &size ))
                {
                    wcscat(path, _T("\\"));
                    wcscat(path, so_file_name);
                    handle = ((vm_so_handle)LoadLibraryExW(path, NULL, 0));
                }
        }
        RegCloseKey(hkey);
    }

    return handle;

} /* vm_so_handle vm_so_load(vm_char *so_file_name) */

vm_so_func vm_so_get_addr(vm_so_handle so_handle, const char *so_func_name)
{
    /* check error(s) */
    if (NULL == so_handle)
        return NULL;

    return (vm_so_func)GetProcAddress((HMODULE)so_handle, (LPCSTR)so_func_name);

} /* void *vm_so_get_addr(vm_so_handle so_handle, const vm_char *so_func_name) */

void vm_so_free(vm_so_handle so_handle)
{
    /* check error(s) */
    if (NULL == so_handle)
        return;

    FreeLibrary((HMODULE)so_handle);

} /* void vm_so_free(vm_so_handle so_handle) */

#endif /* defined(_WIN32) || defined(_WIN64) */
