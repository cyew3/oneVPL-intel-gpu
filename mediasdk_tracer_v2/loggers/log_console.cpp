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
        if(log.find("function:") == std::string::npos) spase = "    ";
        if(logstr.length() > 2) std::cout << ThreadInfo::GetThreadId() << " " << Timer::GetTimeStamp() << " " << spase << logstr << "\n";
        else std::cout << logstr << "\n";
        if(str_stream.eof())
            break;
    }
}
