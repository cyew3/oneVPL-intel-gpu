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
    _file << log << "\n";
}

void LogFile::SetFilePath(std::string file_path)
{
    if(_file.is_open())
        _file.close();

    _file_path = file_path;
}