#if defined(ANDROID)

#include <iostream>
#include <sstream>
#include <android/log.h>
#include "log_logcat.h"

LogLogcat::LogLogcat()
{
}

LogLogcat::~LogLogcat()
{
}

void LogLogcat::WriteLog(const std::string &log)
{

    std::stringstream str_stream;
    str_stream << log;
    std::string spase = "";
    while(true) {
        spase = "";
        std::string logstr;
        std::stringstream finalstr;
        getline(str_stream, logstr);
        if(log.find("function:") == std::string::npos && log.find(">>") == std::string::npos) spase = "    ";
        if(logstr.length() > 2) {
            finalstr << ThreadInfo::GetThreadId() << " " << Timer::GetTimeStamp() << " " << spase << logstr << "\n";
        } else {
            finalstr << logstr << "\n";
        }
        __android_log_print(ANDROID_LOG_DEBUG, "mfxtracer", "%s", finalstr.str().c_str());
        if(str_stream.eof())
            break;
    }
}

#endif //#if defined(ANDROID)
