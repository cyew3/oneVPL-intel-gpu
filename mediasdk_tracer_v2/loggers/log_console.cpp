#include "log_console.h"
#include <iostream>

LogConsole::LogConsole()
{
}


LogConsole::~LogConsole()
{
}

void LogConsole::WriteLog(const std::string &log)
{
    std::cout << log << "\n";
}
