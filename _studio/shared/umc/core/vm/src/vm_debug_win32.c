//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#if _MSC_VER >= 1600
#define _CRT_NON_CONFORMING_SWPRINTFS
#endif

#include <stdio.h>
#include <stdarg.h>
#include <vm_file.h>
#include <vm_debug.h>

#include <umc_structures.h>

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
# include <tchar.h>
# if defined(_WIN32_WCE)
# include <Kfuncs.h>
# endif
#elif defined LINUX32
# include <time.h>
# include <sys/types.h>
# include <unistd.h>
# include <syslog.h>
#endif


#pragma warning(disable:4047)
#pragma warning(disable:4024)
#pragma warning(disable:4133)

#if defined(UMC_VA) && ( defined(LINUX32) || defined(LINUX64) )
  #define UMC_VA_LINUX
#endif

/* here is line for global enable/disable debug trace
// and specify trace info */
#if defined(_DEBUG) && defined(UMC_VA_LINUX)
  #define VM_DEBUG (VM_DEBUG_LOG_ALL|VM_DEBUG_SHOW_FILELINE|VM_DEBUG_SHOW_THIS|VM_DEBUG_SHOW_FUNCTION|VM_DEBUG_FILE)
#endif

#ifdef VM_DEBUG

#define VM_ONE_FILE_OPEN
#define VM_DEBUG_LINE   1024

#ifdef LINUX32
#   define _tfopen fopen
#   define _T(param) param
#endif
static vm_debug_level vm_DebugLevel = (vm_debug_level) (((vm_debug_level) VM_DEBUG) & VM_DEBUG_ALL);
static vm_debug_output vm_DebugOutput = (vm_debug_output) (((vm_debug_output) VM_DEBUG) &~ VM_DEBUG_ALL);
#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
    static vm_char vm_LogFileName[_MAX_LEN] = VM_STRING("\\vm_debug_log.txt");
#elif defined(LINUX32)
    static vm_char vm_LogFileName[_MAX_LEN] = VM_STRING("vm_debug_log.txt");
#endif
static FILE *vm_LogFile = NULL;
/*
static vm_mutex mDebugMutex;
static Ipp32s mDebugMutexIsValid = 1; */

vm_debug_level vm_debug_setlevel(vm_debug_level new_level) {
    vm_debug_level old_level = vm_DebugLevel;
    vm_DebugLevel = new_level;
    return old_level;
}

vm_debug_output vm_debug_setoutput(vm_debug_output new_output) {
    vm_debug_output old_output = vm_DebugOutput;
    vm_DebugOutput = new_output;
    return old_output;
}

void vm_debug_setfile(vm_char *file, Ipp32s truncate)
{
    if (!file)
    {
        file = VM_STRING("");
    }

    if (!vm_string_strcmp(vm_LogFileName, file))
    {
        return;
    }

    if (vm_LogFile)
    {
        fclose(vm_LogFile);
        vm_LogFile = NULL;
    }

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
    vm_string_strcpy_s(vm_LogFileName, sizeof(vm_LogFileName) / sizeof(vm_LogFileName[0]), file);
#elif defined(LINUX32)
    vm_string_strcpy(vm_LogFileName, file);
#endif
    if (*vm_LogFileName)
    {
        vm_LogFile = _tfopen(vm_LogFileName, (truncate) ? VM_STRING("w") : VM_STRING("a"));
    }
}

void vm_debug_message(const vm_char *format,...)
{
    vm_char line[VM_DEBUG_LINE];
    va_list args;

    va_start(args, format);
    vm_string_vsprintf(line, format, args);
    va_end(args);
#if ( defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE) ) && !defined(WIN_TRESHOLD_MOBILE)
    MessageBox(GetActiveWindow(), line, VM_STRING("MESSAGE"), MB_OK);
#elif defined(LINUX32)
    fprintf(stderr, line);
#endif
}

void vm_debug_trace_ex(Ipp32s level,
                       const void *ptr_this,
                       const vm_char *func_name,
                       const vm_char *file_name,
                       Ipp32s num_line,
                       const vm_char *format,
                       ...)
{
    va_list args;
    vm_char tmp[VM_DEBUG_LINE] = {0,};
    const vm_char *p;

    if (level && !(level & vm_DebugLevel)) {
        return;
    }
/*
    if (vm_string_strstr(file_name, "mpeg2_mux") && level == VM_DEBUG_PROGRESS) return; */

    /* print filename and line */
    if (vm_DebugLevel & VM_DEBUG_SHOW_FILELINE) {
        int showMask;

        if (vm_DebugLevel &~ VM_DEBUG_SHOW_DIRECTORY) {
            p = vm_string_strrchr(file_name, '\\');
            if (p != NULL) file_name = p + 1;
            p = vm_string_strrchr(file_name, '/');
            if (p != NULL) file_name = p + 1;
        }
        vm_string_sprintf(tmp, VM_STRING("%s(%i), "), file_name, num_line);

        showMask = vm_DebugLevel & VM_DEBUG_SHOW_ALL;
        if (!(showMask &~ (VM_DEBUG_SHOW_FILELINE | VM_DEBUG_SHOW_FUNCTION))) {
          vm_char *pc;
          /* in case of fileline&function only expand to 35 symbols */
          pc = tmp + vm_string_strlen(tmp);
          while (pc < tmp + 32) *pc++ = ' ';
          *pc = 0;
        }
    }

    /* print time and PID */
#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
    if (vm_DebugLevel & VM_DEBUG_SHOW_TIME) {
        SYSTEMTIME SystemTime;
        GetLocalTime(&SystemTime);

        vm_string_sprintf(tmp + vm_string_strlen(tmp),
                          VM_STRING("%u/%u/%u %u:%u:%u:%u, "),
                          SystemTime.wDay,
                          SystemTime.wMonth,
                          SystemTime.wYear,
                          SystemTime.wHour,
                          SystemTime.wMinute,
                          SystemTime.wSecond,
                          SystemTime.wMilliseconds);
    }
    if (vm_DebugLevel & VM_DEBUG_SHOW_PID) {
        DWORD curProcId = GetCurrentProcessId();
        vm_string_sprintf(tmp + vm_string_strlen(tmp), VM_STRING("PID %i, "), curProcId);
    }
#elif defined(LINUX32)
    if (vm_DebugLevel & VM_DEBUG_SHOW_TIME) {
        time_t timep;
        struct tm *ptm;

        time(&timep);
        ptm = localtime((const time_t*) &timep);
        vm_string_sprintf(tmp + vm_string_strlen(tmp),
                          VM_STRING("%s%u/%u/%u %u:%u:%u, "),
                          ptm->tm_mday, 1+(ptm->tm_mon), 1900+(ptm->tm_year),
                          ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    }
    if (vm_DebugLevel & VM_DEBUG_SHOW_PID) {
        vm_string_sprintf(tmp + vm_string_strlen(tmp), VM_STRING("%sPID %i, "), getpid());
    }
#endif

    /* print 'this' pointer if needed */
    if ((vm_DebugLevel & VM_DEBUG_SHOW_THIS) && ptr_this) {
        vm_string_sprintf(tmp + vm_string_strlen(tmp), VM_STRING("%08x, "), ptr_this);
    }

    /* print func_name if needed */
    if (func_name && (vm_DebugLevel & VM_DEBUG_SHOW_FUNCTION)) {
      p = vm_string_strstr(func_name, VM_STRING("::"));
      if (p) p += 2; else p = func_name;
      vm_string_sprintf(tmp + vm_string_strlen(tmp), VM_STRING("%s(): "), p);
    }

    if (vm_DebugLevel & VM_DEBUG_SHOW_LEVEL) {
      vm_string_sprintf(tmp + vm_string_strlen(tmp), VM_STRING("Level%d, "), level);
    }

    va_start(args, format);
    vm_string_vsprintf(tmp + vm_string_strlen(tmp), format, args);
    va_end(args);

    /* remove trailing \n */
    {
      vm_char *pc;
      pc = tmp + vm_string_strlen(tmp) - 1;
      if (*pc == 0xD || *pc == 0xA) *pc-- = 0;
      if (*pc == 0xD || *pc == 0xA) *pc-- = 0;
    }

    vm_string_strcat(tmp, VM_STRING("\n"));

    if (vm_DebugOutput & VM_DEBUG_FILE) {
        FILE *pFile = NULL;
/*
        if (mDebugMutexIsValid) {
            vm_mutex_set_invalid(&mDebugMutex);
            res = vm_mutex_init(&mDebugMutex);
            if (res != VM_OK) {
                vm_debug_trace(VM_DEBUG_ALL, VM_STRING("Failed to init debug mutex !!!"));
                return;
            }
            mDebugMutexIsValid = 0;
        }
        res = vm_mutex_lock(&mDebugMutex);
        if (res != VM_OK) {
            vm_debug_trace(VM_DEBUG_ALL, VM_STRING("Failed to lock debug mutex !!!"));
            return;
        }
*/
        if (!vm_LogFile) {
          pFile = _tfopen(vm_LogFileName, VM_STRING("w"));
          vm_LogFile = pFile;
        } else {
#ifdef VM_ONE_FILE_OPEN
          pFile = vm_LogFile;
#else
          pFile = fopen(vm_LogFileName, VM_STRING("a"));
#endif
        }
        if (pFile) {
          // file is opened in the same mode as tmp has. There's no need to
          // convert ASCII<->UNICODE
#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
          vm_string_fprintf_s(pFile, VM_STRING("%s"), tmp);
#elif defined(LINUX32) || defined(LINUX64)
          fprintf(pFile, "%s", tmp);
#endif

#ifdef VM_ONE_FILE_OPEN
          fflush(pFile);
#else
          fclose(pFile);
#endif
        }
/*
        res = vm_mutex_unlock(&mDebugMutex);
        if (res != VM_OK) {
            vm_debug_trace(VM_DEBUG_ALL, VM_STRING("Failed to unlock debug mutex !!!"));
            return;
        }
*/
    }
    if (vm_DebugOutput & VM_DEBUG_CONSOLE) {
        vm_string_printf(VM_STRING("%s"), tmp);
    }
    if (vm_DebugOutput == VM_DEBUG_AUTO) {
#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
        OutputDebugString(tmp);
#elif defined(LINUX32)
        syslog("%s", tmp);
#endif
    }
}

#else /*  !VM_DEBUG */

vm_debug_level vm_debug_setlevel(vm_debug_level level)
{
    return level;
}

vm_debug_output vm_debug_setoutput(vm_debug_output output)
{
    return output;
}

void vm_debug_setfile(vm_char *file, Ipp32s truncate)
{
    // touch unreferenced parameters
    file = file;
    truncate = truncate;
}

void vm_debug_message(const vm_char *format, ...)
{
    // touch unreferenced parameters
    format = format;
}

#endif /* VM_DEBUG */

static struct {
    int code;
    vm_char *string;
}
ErrorStringTable[] =
{
    { 0,                    VM_STRING("No any errors") },
    /* VM */
    { VM_OPERATION_FAILED,  VM_STRING("General operation fault") },
    { VM_NOT_INITIALIZED,   VM_STRING("VM_NOT_INITIALIZED") },
    { VM_TIMEOUT,           VM_STRING("VM_TIMEOUT") },
    { VM_NOT_ENOUGH_DATA,   VM_STRING("VM_NOT_ENOUGH_DATA") },
    { VM_NULL_PTR,          VM_STRING("VM_NULL_PTR") },
    { VM_SO_CANT_LOAD,      VM_STRING("VM_SO_CANT_LOAD") },
    { VM_SO_INVALID_HANDLE, VM_STRING("VM_SO_INVALID_HANDLE") },
    { VM_SO_CANT_GET_ADDR,  VM_STRING("VM_SO_CANT_GET_ADDR") },
    /* UMC */
    { -899,                 VM_STRING("UMC_ERR_INIT: Failed to initialize codec") },
    { -897,                 VM_STRING("UMC_ERR_SYNC: Required suncronization code was not found") },
    { -896,                 VM_STRING("UMC_ERR_NOT_ENOUGH_BUFFER: Buffer size is not enough") },
    { -895,                 VM_STRING("UMC_ERR_END_OF_STREAM: End of stream") },
    { -894,                 VM_STRING("UMC_ERR_OPEN_FAILED: Device/file open error") },
    { -883,                 VM_STRING("UMC_ERR_ALLOC: Failed to allocate memory") },
    { -882,                 VM_STRING("UMC_ERR_INVALID_STREAM: Invalid stream") },
    { -879,                 VM_STRING("UMC_ERR_UNSUPPORTED: Unsupported") },
    { -878,                 VM_STRING("UMC_ERR_NOT_IMPLEMENTED: Not implemented yet") },
    { -876,                 VM_STRING("UMC_ERR_INVALID_PARAMS: Incorrect parameters") },
    { 1,                    VM_STRING("UMC_WRN_INVALID_STREAM") },
    { 2,                    VM_STRING("UMC_WRN_REPOSITION_INPROGRESS") },
    { 3,                    VM_STRING("UMC_WRN_INFO_NOT_READY") },
    /* Win */
    { 0x80004001L,          VM_STRING("E_NOTIMPL: Not implemented") },
    { 0x80004002L,          VM_STRING("E_NOINTERFACE: No such interface supported") },
    { 0x80004003L,          VM_STRING("E_POINTER: Invalid pointer") },
    { 0x80004004L,          VM_STRING("E_ABORT: Operation aborted") },
    { 0x80004005L,          VM_STRING("E_FAIL: Unspecified error") },
    { 0x8000FFFFL,          VM_STRING("E_UNEXPECTED: Catastrophic failure") },
    { 0x80070005L,          VM_STRING("E_ACCESSDENIED: General access denied error") },
    { 0x80070006L,          VM_STRING("E_HANDLE: Invalid handle") },
    { 0x8007000EL,          VM_STRING("E_OUTOFMEMORY: Ran out of memory") },
    { 0x80070057L,          VM_STRING("E_INVALIDARG: One or more arguments are invalid") },
    /* DirectShow */
    { 0x8004020BL,          VM_STRING("VFW_E_RUNTIME_ERROR: A run-time error occurred") },
    { 0x80040210L,          VM_STRING("VFW_E_BUFFERS_OUTSTANDING: One or more buffers are still active") },
    { 0x80040211L,          VM_STRING("VFW_E_NOT_COMMITTED: Cannot allocate a sample when the allocator is not active") },
    { 0x80040227L,          VM_STRING("VFW_E_WRONG_STATE: The operation could not be performed because the filter is in the wrong state") },
};

Ipp32s vm_trace_hresult_func(Ipp32s hr, vm_char *mess, void *pthis, vm_char *func, vm_char *file, Ipp32u line)
{
    /* touch unreferenced parameter */
    pthis = pthis;

#ifdef VM_DEBUG
    {
        int level;
        vm_char *pString;
        if (!hr)
        {
            level = VM_DEBUG_MACROS;
            pString = VM_STRING("No any errors");
        }
        else
        {
            int i;
            level = VM_DEBUG_ERROR;
            pString = VM_STRING("Unknown error");
            for (i = 0; i < sizeof(ErrorStringTable)/sizeof(ErrorStringTable[0]); i++)
            {
                if (hr == (Ipp32s) ErrorStringTable[i].code)
                {
                    pString = ErrorStringTable[i].string;
                }
            }
        }
        vm_debug_trace_ex(level, NULL, func, file, line, _T("%s = 0x%x = %s"), mess, hr, pString);
    }
#else
    // touch unreferenced parameters
    hr = hr;
    mess = mess;
    func = func;
    file = file;
    line = line;
#endif

    return hr;
}

#if defined(LINUX32)
void vm_debug_trace_ex(Ipp32s level, const vm_char *func_name, const vm_char *file_name,
                       Ipp32s num_line, const vm_char *format, ...)
{}
#endif

//#endif
/* EOF */
