/* ****************************************************************************** *\

Copyright (C) 2020 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

\* ****************************************************************************** */
#include <fstream>

#include "config_manager.h"

ConfigManager *ConfigManager::manager = 0;

ConfigManager::ConfigManager(void)
{
}

ConfigManager::~ConfigManager(void)
{
    if(manager)
        delete manager;
}

map<string, map<string, string> > ConfigManager::ParceConfig(string config_path)
{
    if(!manager)
        manager = new ConfigManager();

    map<string, map<string, string> > ini;
    ifstream  _file(config_path.c_str(), ifstream::binary);
    if(_file.is_open()) {
        //TODO parse
        std::string curent_section;
        std::map<std::string, std::string> section_params;
        while(true) {
            std::string inistr;
            getline(_file, inistr);

            //delete tabs and spaces
            unsigned int firstQuote = inistr.find("\"");
            unsigned int secondQuote = inistr.find_last_of("\"");
            std::string firstPart="";
            std::string secondPart="";
            std::string quotePart="";
            if ((firstQuote != std::string::npos) && firstQuote < secondQuote)
            {
               firstPart = inistr.substr(0,firstQuote);
               if (secondQuote != std::string::npos)
               {
                  secondPart = inistr.substr(secondQuote+1);
                  quotePart = inistr.substr(firstQuote+1, secondQuote-firstQuote-1);
               }

               firstPart.erase(std::remove_if(firstPart.begin(), firstPart.end(), &::isspace),firstPart.end());
               secondPart.erase(std::remove_if(secondPart.begin(), secondPart.end(), &::isspace),secondPart.end());
               inistr = firstPart + quotePart + secondPart;
            }
            else
            {
                inistr.erase(std::remove_if(inistr.begin(), inistr.end(), &::isspace),inistr.end());
            }

            //delete comments
            basic_string <char>::size_type n1 = inistr.find("#");
            basic_string <char>::size_type n2 = inistr.find(";");

            if(n1 != string::npos && n2 != string::npos)
                n1>=n2 ? inistr.erase(n1, inistr.length()) : inistr.erase(n2, inistr.length());
            else if (n1 != string::npos)
                   inistr.erase(n1, inistr.length());
            else if(n2 != string::npos)
                inistr.erase(n2, inistr.length());


            basic_string <char>::size_type s1 = inistr.find("[");
            basic_string <char>::size_type s2 = inistr.find("]");
            if(s1 != string::npos && s2 != string::npos) {
                inistr.erase(s1, s1+1);
                inistr.erase(s2-1, s2);
                curent_section = inistr;
                section_params.clear();

                ini.insert(pair<string, map<string, string> >(inistr, section_params));

                continue;
            }

            if(!curent_section.empty()){
                vector<string> key_val = split(inistr, '=');
                if(key_val.size() == 2){
                    ini[curent_section].insert(std::pair<std::string, std::string>(key_val[0], key_val[1]));
                }
            }

            if(_file.eof())
                break;
        }
        _file.close();
    }

    return ini;
}

bool ConfigManager::CreateConfig(string config_path, map<string, map<string, string> > config)
{
    ofstream _file(config_path.c_str());
    if(_file.is_open()){
        for(map<string, map<string, string> >::iterator it = config.begin(); it != config.end(); ++it){
            _file << string("[") + (*it).first + string("]\n");

             map<string, string> section_params = (*it).second;
             for(map<string, string>::iterator itparams = section_params.begin(); itparams != section_params.end(); ++itparams){
                _file << "  " << (*itparams).first + string(" = ") + (*itparams).second + string("\n");
             }

             _file << "\n";
        }

        _file.close();
        return true;
    }
    return false;
}
