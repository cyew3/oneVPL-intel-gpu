#ifndef LOG_LOGCAT_H_
#define LOG_LOGCAT_H_

#if defined(ANDROID)

#include "ilog.h"

class LogLogcat : public ILog
{
public:
    LogLogcat();
    virtual ~LogLogcat();
    virtual void WriteLog(const std::string &log);
};

#endif //#if defined(ANDROID)

#endif //LOG_LOGCAT_H_
