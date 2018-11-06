#include "tracer.h"

void tracer_init()
{
#ifdef ANDROID // temporary hardcode for Android
    Log::SetLogLevel(LOG_LEVEL_DEFAULT);
    Log::SetLogType(LOG_LOGCAT);
#else
    std::string type = Config::GetParam("core", "type");
    if (type == std::string("console")) {
        Log::SetLogType(LOG_CONSOLE);
    } else if (type == std::string("file")) {
        Log::SetLogType(LOG_FILE);
    } else {
        // TODO: what to do with incorrect setting?
        Log::SetLogType(LOG_CONSOLE);
    }

    std::string log_level = Config::GetParam("core", "level");
    if(log_level == std::string("default")){
         Log::SetLogLevel(LOG_LEVEL_DEFAULT);
    }
    else if(log_level == std::string("short")){
        Log::SetLogLevel(LOG_LEVEL_SHORT);
    }
    else if(log_level == std::string("full")){
        Log::SetLogLevel(LOG_LEVEL_FULL);
    }
    else{
        // TODO
        Log::SetLogLevel(LOG_LEVEL_FULL);
    }
#endif
}