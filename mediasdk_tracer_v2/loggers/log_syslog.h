#ifdef linux
#ifndef LOGSYSLOG_H_
#define LOGSYSLOG_H_

#include "ilog.h"

class LogSyslog : public ILog
{
public:
    LogSyslog();
    virtual ~LogSyslog();
    virtual void WriteLog(const std::string &log);
};

#endif //LOGSYSLOG_H_
#endif