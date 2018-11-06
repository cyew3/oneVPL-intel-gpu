#ifndef CONFIGMANAGER_H_
#define CONFIGMANAGER_H_

#include <map>
#include <string>

#include "strfuncs.h"

using namespace std;

class ConfigManager
{
public:
    static map<string, map<string, string> > ParceConfig(const string config_path);
    static bool CreateConfig(const string config_path, map<string, map<string, string> > config);

private:
    static ConfigManager *manager;

    ConfigManager(void);
    ~ConfigManager(void);
};

#endif //CONFIGMANAGER_H_
