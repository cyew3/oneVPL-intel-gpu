#ifndef LOGGER_H_
#define LOGGER_H_

#include <map>
#include "log_console.h"
#include "log_file.h"
#include "log_etw_events.h"
#include "log_syslog.h"

enum eLogType{
    LOG_FILE,
    LOG_CONSOLE,
#ifdef linux
    LOG_SYSLOG,
#elif _WIN32 || _WIN64
    LOG_ETW
#endif
};

class Log
{
public:
    static void WriteLog(const std::string &log);
    static void SetLogType(eLogType type);
    static void SetFilePath(std::string file_path);
private:
    Log();
    ~Log();
    static Log *_sing_log;
    ILog *_log;
    std::map<eLogType, ILog*> _logmap;
};

#endif //LOGGER_H_
