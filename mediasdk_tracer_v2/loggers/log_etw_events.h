#ifndef LOGETWEVENTS_H_
#define LOGETWEVENTS_H_

#if defined(_WIN32) || defined(_WIN64)

#include "ilog.h"

class LogEtwEvents : public ILog
{
public:
    LogEtwEvents();
    virtual ~LogEtwEvents();
    virtual void WriteLog(const std::string &log);
};

#endif // #if defined(_WIN32) || defined(_WIN64)

#endif // LOGETWEVENTS_H_
