// Copyright (c) 2003-2018 Intel Corporation
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

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)

#include <windows.h>
#include "vm_mmap.h"

/* Set the mmap handle an invalid value */
void vm_mmap_set_invalid(vm_mmap *handle)
{
    /* check error(s) */
    if (NULL == handle)
        return;

    handle->fd_file     = NULL;
    handle->address     = NULL;
    handle->fd_map      = NULL;
    handle->fAccessAttr = 0;

} /* void vm_mmap_set_invalid(vm_mmap *handle) */

/* Verify if the mmap handle is valid */
int32_t vm_mmap_is_valid(vm_mmap *handle)
{
    /* check error(s) */
    if (NULL == handle)
        return 0;

    return (int32_t)(NULL != handle->fd_map);

} /* int32_t vm_mmap_is_valid(vm_mmap *handle) */

/* Map a file into system meory, return the file size */
unsigned long long vm_mmap_create(vm_mmap *handle, vm_char *file, int32_t fileAccessAttr)
{
    LARGE_INTEGER sizet;

    /* check error(s) */
    if (NULL == handle)
        return 0;

    handle->fd_map = NULL;
    handle->address = NULL;

    if (FLAG_ATTRIBUTE_READ & fileAccessAttr)
    {
#ifdef _WIN32_WCE
        handle->fd_file = CreateFileForMapping(file,
                                               GENERIC_READ,
                                               FILE_SHARE_READ,
                                               NULL,
                                               OPEN_EXISTING,
                                               FILE_ATTRIBUTE_NORMAL,
                                               NULL);
#else /* !_WIN32_WCE */
        handle->fd_file = CreateFile(file,
                                     GENERIC_READ,
                                     FILE_SHARE_READ,
                                     NULL,
                                     OPEN_EXISTING,
                                     FILE_ATTRIBUTE_READONLY|FILE_FLAG_RANDOM_ACCESS,
                                     0);
#endif /* _WIN32_WCE */

        if (INVALID_HANDLE_VALUE == handle->fd_file)
            return 0;

        sizet.LowPart = GetFileSize(handle->fd_file, (LPDWORD) &sizet.HighPart);
        handle->fd_map = CreateFileMapping(handle->fd_file,
                                           NULL,
                                           PAGE_READONLY,
                                           sizet.HighPart,
                                           sizet.LowPart,
                                           0);
    }
    else
    {
#ifdef _WIN32_WCE
        handle->fd_file = CreateFileForMapping(file,
                                               GENERIC_WRITE|GENERIC_READ,
                                               FILE_SHARE_WRITE,
                                               NULL,
                                               CREATE_ALWAYS,
                                               FILE_ATTRIBUTE_NORMAL,
                                               NULL);
#else /* !_WIN32_WCE */
        handle->fd_file = CreateFile(file,
                                     GENERIC_WRITE|GENERIC_READ,
                                     FILE_SHARE_WRITE,
                                     NULL,
                                     CREATE_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL,
                                     0);
#endif /* _WIN32_WCE */

        if (INVALID_HANDLE_VALUE == handle->fd_file)
            return 0;

        sizet.LowPart  = 0;
        sizet.HighPart = 1;
        handle->fd_map = CreateFileMapping(handle->fd_file,
                                           NULL,
                                           PAGE_READWRITE,
                                           0,
                                           sizet.LowPart,
                                           0);
        sizet.LowPart  = 1024 * 1024 * 1024;
        sizet.HighPart = 0;

        while (!handle->fd_map)
        {
            uint32_t error;
            handle->fd_map = CreateFileMapping(handle->fd_file,
                                               NULL,
                                               PAGE_READWRITE,
                                               0,
                                               sizet.LowPart,
                                               0);
            error = GetLastError();

            if (ERROR_NOT_ENOUGH_MEMORY == error || ERROR_DISK_FULL == error)
                sizet.LowPart /= 2;
            else
                break;
        }
    }

    handle->fAccessAttr = fileAccessAttr;

    if (FLAG_ATTRIBUTE_READ & fileAccessAttr || 16*1024*1024 <= sizet.LowPart)
    {
        if (handle->fd_map)
            return (unsigned long long) sizet.QuadPart;
    }

    vm_mmap_close(handle);
    return 0;

} /* unsigned long long vm_mmap_create(vm_mmap *handle, vm_char *file, int32_t fileAccessAttr) */

/* Obtain the a view of the mapped file */
void *vm_mmap_set_view(vm_mmap *handle, unsigned long long *offset, unsigned long long *sizet)
{
    LARGE_INTEGER t_offset;
    LARGE_INTEGER t_sizet;
    SYSTEM_INFO info;
    unsigned long long pagesize;
    unsigned long long edge;

    /* check error(s) */
    if (NULL == handle)
        return NULL;

    if (handle->address)
        UnmapViewOfFile(handle->address);

    GetSystemInfo(&info);
    pagesize = info.dwAllocationGranularity;

    edge = (*sizet) + (*offset);
    t_offset.QuadPart =
    (*offset) = (unsigned long long)((*offset) / pagesize) * pagesize;

    t_sizet.QuadPart = (*sizet) = edge - (*offset);

    if (FLAG_ATTRIBUTE_READ & handle->fAccessAttr)
    {
        handle->address = MapViewOfFile(handle->fd_map,
                                        FILE_MAP_READ,
                                        t_offset.HighPart,
                                        t_offset.LowPart,
                                        t_sizet.LowPart);
    }
    else
    {
        handle->address = MapViewOfFile(handle->fd_map,
                                        FILE_MAP_WRITE,
                                        t_offset.HighPart,
                                        t_offset.LowPart,
                                        t_sizet.LowPart);
    }
    return handle->address;

} /* void *vm_mmap_set_view(vm_mmap *handle, unsigned long long *offset, unsigned long long *sizet) */

/* Remove the mmap */
void vm_mmap_close(vm_mmap *handle)
{
    /* check error(s) */
    if (NULL == handle)
        return;

    if (handle->address)
    {
        UnmapViewOfFile(handle->address);
        handle->address = NULL;
    }

    if (handle->fd_map)
    {
        CloseHandle(handle->fd_map);
        handle->fd_map = NULL;
    }

    if (INVALID_HANDLE_VALUE != handle->fd_file)
    {
        CloseHandle(handle->fd_file);
        handle->fd_file = INVALID_HANDLE_VALUE;
    }
} /* void vm_mmap_close(vm_mmap *handle) */

/*  Return page size*/
uint32_t vm_mmap_get_page_size(void)
{
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    return info.dwPageSize;

} /* uint32_t vm_mmap_get_page_size(void) */

uint32_t vm_mmap_get_alloc_granularity(void)
{
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    return info.dwAllocationGranularity;

} /* uint32_t vm_mmap_get_alloc_granularity(void) */

void vm_mmap_unmap(vm_mmap *handle)
{
    /* check error(s) */
    if (NULL == handle)
        return;

    if (handle->address)
    {
        UnmapViewOfFile(handle->address);
        handle->address = NULL;
    }
} /* void vm_mmap_unmap(vm_mmap *handle) */

#endif /* defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE) */
