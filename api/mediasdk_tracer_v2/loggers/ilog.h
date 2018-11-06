#ifndef ILOG_H_
#define ILOG_H_

#include <string>
#include "timer.h"
#include "thread_info.h"



class ILog
{
public:
    virtual ~ILog(){}
    virtual void WriteLog(const std::string &log) = 0;
};

#endif //ILOG_H_
