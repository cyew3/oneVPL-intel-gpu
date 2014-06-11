#ifndef TIMER_H__
#define TIMER_H__

#include <string>
#include "../dumps/dump.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <sys/time.h>
#include <ctime>
#endif

using namespace std;

class Timer
{
protected:
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER start;
#else
    struct timeval start;
#endif

public:
    Timer()
    {
        Restart();
    }

    static string GetTimeStamp(){
        string timestamp = "";
        #if defined(_WIN32) || defined(_WIN64)
        SYSTEMTIME st;
        GetLocalTime(&st);
        timestamp = ToString(st.wYear) + "-" + ToString(st.wMonth) + "-" + ToString(st.wDay) + " " +
                    ToString(st.wHour) + ":" + ToString(st.wMinute) + ":" + ToString(st.wSecond);

        #else
        time_t t = time(0);
        struct tm * now = localtime(&t);
        timestamp = ToString((now->tm_year + 1900)) + '-' + ToString(now->tm_mon + 1) + '-' + ToString(now->tm_mday) + " " +
                       ToString(now->tm_hour) + ":" + ToString(now->tm_min) + ":" + ToString(now->tm_sec);
        #endif

        return timestamp;
    };

    double GetTime()
    {
#ifdef WIN32
        LARGE_INTEGER tick, ticksPerSecond;
        QueryPerformanceFrequency(&ticksPerSecond);
        QueryPerformanceCounter(&tick);
        return (((double)tick.QuadPart - (double)start.QuadPart) / (double)ticksPerSecond.QuadPart) * 1000;
#else
        struct timeval now;
        gettimeofday(&now, NULL);
        return ((now.tv_sec - start.tv_sec) + (now.tv_usec - start.tv_usec)/1000000.0) * 1000;
#endif
    }

    void Restart()
    {
#if defined(_WIN32) || defined(_WIN64)
        QueryPerformanceCounter(&start);
#else
        gettimeofday(&start, NULL);
#endif
    }
};
#endif //TIMER_H__