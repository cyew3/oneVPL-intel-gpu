#if !defined(_WIN32) && !defined(_WIN64) && !defined(ANDROID)

#include "log_syslog.h"

LogSyslog::LogSyslog()
{
}


LogSyslog::~LogSyslog()
{
}

void LogSyslog::WriteLog(const std::string &log)
{

}

#endif // #if !defined(_WIN32) && !defined(_WIN64) && !defined(ANDROID)
