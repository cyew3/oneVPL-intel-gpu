#ifndef LOGGERCONSOLE_H_
#define LOGGERCONSOLE_H_

#include "ilog.h"

class LogConsole : public ILog
{
public:
    LogConsole();
    virtual ~LogConsole();
    virtual void WriteLog(const std::string &log);
};

#endif //LOGGERCONSOLE_H_