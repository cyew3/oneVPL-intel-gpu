#include "log_file.h"
#include "../config/config.h"
#include "../dumps/dump.h"

LogFile::LogFile()
{
    std::string strproc_id = ToString(ThreadInfo::GetProcessId());
    std::string file_log = Config::GetParam("core", "log");
    if(!file_log.empty())
        _file_path = std::string(file_log);
    else
        _file_path = std::string("mfxtracer.log");

    if (!Log::useGUI)
    {
        strproc_id = std::string("_") + strproc_id;
        unsigned int pos = _file_path.rfind(".");
        if (pos == std::string::npos)
            _file_path.insert(_file_path.length(), strproc_id);
        else if((_file_path.length() - pos) > std::string(".log").length())
            _file_path.insert(_file_path.length(), strproc_id);
        else
            _file_path.insert(pos, strproc_id);
    }
}

LogFile::~LogFile()
{
    if(_file.is_open())
        _file.close();
}

void LogFile::WriteLog(const std::string &log)
{
    if(!_file.is_open())
        _file.open(_file_path.c_str(), std::ios_base::app);

   
        std::stringstream str_stream;
        str_stream << log;
        std::string spase = "";
        while(true) {
            spase = "";
            std::string logstr;
            getline(str_stream, logstr);
            if(log.find("function:") == std::string::npos && log.find(">>") == std::string::npos) spase = "    ";
            std::stringstream pre_out;
            if(logstr.length() > 2) pre_out << ThreadInfo::GetThreadId() << " " << Timer::GetTimeStamp() << " " << spase << logstr << std::endl;
            else pre_out << logstr << "\n";
            _file << pre_out.str();
            if(str_stream.eof())
                break;
        }
        _file.flush();
    
}

void LogFile::SetFilePath(std::string file_path)
{
    if(_file.is_open())
        _file.close();

    _file_path = file_path;
}
