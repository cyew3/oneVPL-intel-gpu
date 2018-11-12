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

#ifndef __SYS_INFO_H__
#define __SYS_INFO_H__

#include <stdio.h>
#include "umc_structures.h"
#include "vm_time.h"
#include "vm_types.h"

namespace UMC
{

typedef struct sSystemInfo
{
    uint32_t num_proc;                                           // (uint32_t) number of processor(s)
    uint32_t cpu_freq;                                           // (uint32_t) CPU frequency
    vm_char os_name[_MAX_LEN];                                 // (vm_char []) OS name
    vm_char proc_name[_MAX_LEN];                               // (vm_char []) processor's name
    vm_char computer_name[_MAX_LEN];                           // (vm_char []) computer's name
    vm_char user_name[_MAX_LEN];                               // (vm_char []) user's name
    vm_char video_card[_MAX_LEN];                              // (vm_char []) video adapter's name
    vm_char program_path[_MAX_LEN];                            // (vm_char []) program path
    vm_char program_name[_MAX_LEN];                            // (vm_char []) program name
    vm_char description[_MAX_LEN];
    uint32_t phys_mem;
} sSystemInfo;

#if ! (defined(_WIN32_WCE) || defined(__linux) || defined(__APPLE__))
/* obtain  */

typedef struct _VIRTUAL_MACHINE_COUNTERS {
    size_t        size1;
    size_t        size2;
    uint32_t        size3;
    size_t        size4;
    size_t        size5;
    size_t        size6;
    size_t        size7;
    size_t        size8;
    size_t        size9;
    size_t        size10;
    size_t        size11;
} VIRTUAL_MACHINE_COUNTERS;

typedef struct _TASK_ID {
    uint32_t        ProcessID;
    uint32_t        ThreadID;
} TASK_ID;

typedef struct _ST {
    unsigned long long        TimeOfKernel;
    unsigned long long        TimeOfUser;
    unsigned long long        TimeOfCreation;
    uint32_t        TimeOfWaiting;
    void          *BaseAddress;
    TASK_ID       TaskID;
    int32_t        Priority;
    int32_t        StartPriority;
    uint32_t        CSCount;
    int32_t        State;
    int32_t        ReasonOfWaiting;
} ST;

typedef struct _WSTRING {
    uint16_t        Len;
    uint16_t        MaxLen;
    wchar_t       *Ptr;
} WSTRING;

typedef struct _SYSTEM_PROCESSES {
    uint32_t             OffsetOfNextEntry;
    uint32_t             Reserved1;
    uint32_t             Reserved2[6];
    unsigned long long             Reserved3;
    unsigned long long             APP_Time;
    unsigned long long             OS_Time;
    WSTRING            Application_Name;
    int32_t             Start_priority;
    uint32_t             PID;
    uint32_t             Reserved4;
    uint32_t             Reserved5;
    uint32_t             Reserved6[2];
    VIRTUAL_MACHINE_COUNTERS       Reserved7;
#if _WIN32_WINNT >= 0x500
    IO_COUNTERS        Reserved8;
#endif
    ST                 Reserved9[1];
} SYSTEM_PROCESSES, * PSYSTEM_PROCESSES;

typedef int32_t    COUNTERS_STATUS;
#define IF_SUCCESS(Status) ((COUNTERS_STATUS)(Status) >= 0)
#define NEXT_AVAILABLE     ((COUNTERS_STATUS)0xC0000004L)

#endif // if !(defined(_WIN32_WCE) || defined(__linux) || defined(__APPLE__))

typedef uint32_t (*FuncGetMemUsage)();

class SysInfo
{
public:
    // Default constructor
    SysInfo(vm_char *pProcessName = NULL);
    // Destructor
    virtual ~SysInfo(void);

    // Get system information
    sSystemInfo *GetSysInfo(void);

    // CPU usage
    double GetCpuUsage(void);
    double GetAvgCpuUsage(void) {return avg_cpuusage;};
    double GetMaxCpuUsage(void) {return max_cpuusage;};
    void CpuUsageRelease(void);

    // Memory usage
    void   SetFuncGetMemUsage(FuncGetMemUsage pFunc) { m_FuncGetMemUsage = pFunc; }
    double GetMemUsage(void);
    double GetAvgMemUsage(void);
    double GetMaxMemUsage(void);
    void   ResetMemUsage(void);

protected:
    // system info struct
    sSystemInfo m_sSystemInfo;

    // CPU usage
    vm_tick user_time;
    vm_tick total_time;
    vm_tick user_time_start;
    vm_tick total_time_start;
    double last_cpuusage;
    double max_cpuusage;
    double avg_cpuusage;

    // memory usage
    FuncGetMemUsage m_FuncGetMemUsage;
    double  m_MemoryMax;
    double  m_MemorySum;
    int32_t  m_MemoryCount;
};

} // namespace UMC

#endif // __SYS_INFO_H__
