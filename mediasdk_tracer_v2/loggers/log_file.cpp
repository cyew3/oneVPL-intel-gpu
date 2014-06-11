#include "log_file.h"

LogFile::LogFile()
{
    _file_path = std::string("trace.log");
}

LogFile::~LogFile()
{
    if(_file.is_open())
        _file.close();
}

void LogFile::WriteLog(const std::string &log)
{
    if(!_file.is_open())
        _file.open(_file_path.c_str());

    std::stringstream str_stream;
    str_stream << log;
    std::string spase = "";
    while(true) {
        spase = "";
        std::string logstr;
        getline(str_stream, logstr);
        if(log.find("function:") == std::string::npos && log.find(">>") == std::string::npos) spase = "    ";
        if(logstr.length() > 2) _file << ThreadInfo::GetThreadId() << " " << Timer::GetTimeStamp() << " " << spase << logstr << std::endl;
        else _file << logstr << "\n";
        if(str_stream.eof())
            break;
    }
}

void LogFile::SetFilePath(std::string file_path)
{
    if(_file.is_open())
        _file.close();

    _file_path = file_path;
}
