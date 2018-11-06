#ifndef THREAD_INFO_H_
#define THREAD_INFO_H_

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <unistd.h>
#include <sys/syscall.h>
#endif

class ThreadInfo
{
public:
    static long GetThreadId()
    {
        #if defined(_WIN32) || defined(_WIN64)
            return (long)GetCurrentThreadId();
        #else
            return (long)syscall(SYS_gettid);
        #endif
        return -1;
    };

    static long GetProcessId()
    {
        #if defined(_WIN32) || defined(_WIN64)
            return (long)GetCurrentProcessId();
        #else
            return (long)getpid();
        #endif
        return -1;
    }
};

#endif //THREAD_INFO_H_