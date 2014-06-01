#ifndef ILOG_H_
#define ILOG_H_

#include <string>

class ILog
{
public:
    virtual void WriteLog(const std::string &log) = 0;
};

#endif //ILOG_H_
