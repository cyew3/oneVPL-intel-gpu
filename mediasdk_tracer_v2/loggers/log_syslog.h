#ifndef LOG_SYSLOG_H_
#define LOG_SYSLOG_H_

#if !defined(_WIN32) && !defined(_WIN64) && !defined(ANDROID)

#include "ilog.h"

class LogSyslog : public ILog
{
public:
    LogSyslog();
    virtual ~LogSyslog();
    virtual void WriteLog(const std::string &log);
};

#endif // #if !defined(_WIN32) && !defined(_WIN64) && !defined(ANDROID)
#endif // LOGSYSLOG_H_
