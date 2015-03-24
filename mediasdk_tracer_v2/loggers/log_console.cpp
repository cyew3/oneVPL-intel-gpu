#include <iostream>
#include <sstream>
#include "log_console.h"

LogConsole::LogConsole()
{
}

LogConsole::~LogConsole()
{
}

void LogConsole::WriteLog(const std::string &log)
{

    
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
            std::cout << pre_out.str();
            if(str_stream.eof())
                break;
        }
    
}
