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
#if defined(_WIN32) || defined(_WIN64)
    LOG_ETW,
#elif defined(ANDROID)
    LOG_LOGCAT,
#else
    LOG_SYSLOG,
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
