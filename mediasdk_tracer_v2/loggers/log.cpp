#include "log.h"

Log* Log::_sing_log = 0;

Log::Log()
{
    _log_level = LOG_LEVEL_DEFAULT;
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_CONSOLE, new LogConsole()));
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_FILE, new LogFile()));
#ifdef linux
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_SYSLOG, new LogSyslog()));
#elif _WIN32 || _WIN64
    _logmap.insert(std::pair<eLogType,ILog*>(LOG_ETW, new LogEtwEvents()));
#endif

    _log = _logmap[LOG_CONSOLE];
}

Log::~Log()
{
    for(std::map<eLogType,ILog*>::iterator it=_logmap.begin(); it!=_logmap.end(); ++it){
        delete it->second;
    }
    _logmap.clear();
}

void Log::SetLogType(eLogType type)
{
    if(!_sing_log)
        _sing_log = new Log();

    _sing_log->_log = _sing_log->_logmap[type];
}

void Log::SetLogLevel(eLogLevel level)
{
    _sing_log->_log_level = level;
}

eLogLevel Log::GetLogLevel()
{
    return _sing_log->_log_level;
}

void Log::WriteLog(const std::string &log)
{
    if(!_sing_log)
        _sing_log = new Log();

    if(_sing_log->_log_level == LOG_LEVEL_DEFAULT){
        _sing_log->_log->WriteLog(log);
    }
    else if(_sing_log->_log_level == LOG_LEVEL_FULL){
        _sing_log->_log->WriteLog(log);
    }
    else if(_sing_log->_log_level == LOG_LEVEL_SHORT){
        if(log.find("function:") != std::string::npos /*&& log.find("-") != std::string::npos*/)
            _sing_log->_log->WriteLog(log /*.substr(0, log.size()-2)*/);
    }
}
