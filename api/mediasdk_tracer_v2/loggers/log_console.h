#ifndef LOG_CONSOLE_H_
#define LOG_CONSOLE_H_

#include "ilog.h"

class LogConsole : public ILog
{
public:
    LogConsole();
    virtual ~LogConsole();
    virtual void WriteLog(const std::string &log);
};

#endif //LOG_CONSOLE_H_
