#ifdef _WIN32 || _WIN64
#ifndef LOGETWEVENTS_H_
#define LOGETWEVENTS_H_

#include "ilog.h"

class LogEtwEvents : public ILog
{
public:
    LogEtwEvents();
    virtual ~LogEtwEvents();
    virtual void WriteLog(const std::string &log);
};

#endif //LOGETWEVENTS_H_
#endif