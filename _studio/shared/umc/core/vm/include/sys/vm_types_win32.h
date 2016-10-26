//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2009 Intel Corporation. All Rights Reserved.
//

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)

//#include <windows.h>

#if defined(_SOCKET_SUPPORT)
#if defined(_WIN32_WCE)
#include <winsock.h>
#endif /* defined(_WIN32_WCE) */
#endif /* defined(_SOCKET_SUPPORT) */

//#include <stdio.h>
#include <ippcore.h>
//#include <ipps.h>


#ifdef _UNICODE
# define vm_main wmain
#else
# define vm_main _tmain
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifdef __ICL

#define VM_ALIGN_DECL(X,Y) __declspec(align(X)) Y

#else /* !__ICL */

#define VM_ALIGN_DECL(X,Y) Y

#endif /* __ICL */

#define CONST_LL(X) X
#define CONST_ULL(X) X##ULL

typedef void * vm_handle;

/* vm_condvar.h */
typedef struct vm_cond
{
    vm_handle handle;

} vm_cond;

/* vm_event.h */
typedef struct vm_event
{
    vm_handle handle;

} vm_event;

/* vm_mmap.h */
typedef struct vm_mmap
{
    vm_handle fd_file, fd_map;
    void  *address;
    Ipp32s fAccessAttr;
} vm_mmap;

/* vm_mutex.h */
typedef struct vm_mutex
{
    vm_handle handle;

} vm_mutex;

/* vm_semaphore.h */
typedef struct vm_semaphore
{
    vm_handle handle;

} vm_semaphore;

/* vm_thread.h */
typedef struct vm_thread
{
    vm_handle handle;
    vm_mutex access_mut;
    Ipp32s i_wait_count;

    Ipp32u (__stdcall * protected_func)(void *);
    void *protected_arg;

} vm_thread;

#if defined(_SOCKET_SUPPORT)
/* vm_socket.h */
#define VM_SOCKET_QUEUE 20
typedef struct vm_socket
{
    fd_set r_set, w_set;
    SOCKET chns[VM_SOCKET_QUEUE + 1];
    struct sockaddr_in sal;
    struct sockaddr_in sar;
    struct sockaddr_in peers[VM_SOCKET_QUEUE + 1];
    Ipp32s flags;

} vm_socket;
#endif /* defined(_SOCKET_SUPPORT) */

typedef struct vm_time
{
   Ipp64s start;
   Ipp64s diff;
   Ipp64s freq;

} vm_time;

struct vm_timeval
{
  Ipp32u tv_sec;
  long int tv_usec;

};

struct vm_timezone
{
  int tz_minuteswest; // This is the number of minutes west of UTC.
  int tz_dsttime;    /* If nonzero, Daylight Saving Time applies
                     during some part of the year. */
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE) */
