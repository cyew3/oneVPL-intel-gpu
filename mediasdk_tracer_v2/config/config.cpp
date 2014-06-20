#include <algorithm>
#include <ctype.h>
#include <iostream>
#include <sstream>

#include "config.h"

Config* Config::conf = 0;

Config::Config()
{
    _file_path = std::string(getenv("HOME")) + "/.mfxtracer";

    if(!_file.is_open()){
        _file.open(_file_path.c_str(), std::ifstream::binary);
    }
    Init();
}

Config::~Config()
{
    if(_file.is_open())
        _file.close();

    //delete ini;
}

void Config::Init()
{
    //delete tabs and spaces
    //delete comments
    //parse sections
    //parse params in sections

    if(_file.is_open()){
        //TODO parse
        std::string curent_section;
        std::map<std::string, std::string> section_params;
        while(true) {
            std::string inistr;
            getline(_file, inistr);

            //delete tabs and spaces
            inistr.erase(std::remove_if(inistr.begin(), inistr.end(), &::isspace), inistr.end());

            //delete comments
            std::basic_string <char>::size_type n1 = inistr.find("#");
            std::basic_string <char>::size_type n2 = inistr.find(";");

            if(n1 != std::string::npos && n2 != std::string::npos)
                n1>=n2 ? inistr.erase(n1, inistr.length()) : inistr.erase(n2, inistr.length());
            else if (n1 != std::string::npos)
                   inistr.erase(n1, inistr.length());
            else if(n2 != std::string::npos)
                inistr.erase(n2, inistr.length());


            std::basic_string <char>::size_type s1 = inistr.find("[");
            std::basic_string <char>::size_type s2 = inistr.find("]");
            if(s1 != std::string::npos && s2 != std::string::npos) {
                inistr.erase(s1, s1+1);
                inistr.erase(s2-1, s2);
                curent_section = inistr;
                section_params.clear();

                ini.insert(std::pair<std::string, std::map<std::string, std::string> >(inistr, section_params));

                continue;
            }

            if(!curent_section.empty()){
                std::vector<std::string> key_val = split(inistr, '=');
                if(key_val.size() == 2){
                    ini[curent_section].insert(std::pair<std::string, std::string>(key_val[0], key_val[1]));
                }
            }

            if(_file.eof())
                break;
        }

    }
    else {
        //TODO add init default config
    }
}

std::vector<std::string> &Config::split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> Config::split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

std::string Config::GetParam(std::string section, std::string key)
{
    if(!conf)
        conf = new Config();

    return conf->ini[section][key];
}
