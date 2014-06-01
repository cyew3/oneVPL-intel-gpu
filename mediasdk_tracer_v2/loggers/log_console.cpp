#include <iostream>

#include "log_console.h"

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
