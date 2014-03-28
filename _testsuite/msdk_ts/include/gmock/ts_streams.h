#pragma once

#include<string>
#include <map>

class tsStreamPool
{
private:
    std::map<std::string, std::string> m_pool;
    std::string m_dir;
    bool m_query_mode;
    bool m_reg;
    static const char valid_delimeter   = '/';
    static const char invalid_delimeter = '\\';

public:

    tsStreamPool();
    ~tsStreamPool();

    void Init(bool query_mode);
    void Close();

    const char* Get(std::string name, std::string path = "");
    void Reg();

};
