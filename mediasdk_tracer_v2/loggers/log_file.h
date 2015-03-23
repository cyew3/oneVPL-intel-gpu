#ifndef LOG_FILE_H_
#define LOG_FILE_H_

#include "ilog.h"
#include "log.h"
#include <fstream>

class LogFile : public ILog
{
public:
    LogFile();
    virtual ~LogFile();
    virtual void WriteLog(const std::string &log);
    void SetFilePath(std::string file_path);
private:
    std::string _file_path;
    std::ofstream _file;
};

#endif //LOG_FILE_H_
