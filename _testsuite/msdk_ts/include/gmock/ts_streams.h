#pragma once

#include<string>
#include <map>

class tsStreamPool
{
private:
    typedef std::map<std::string, std::string> PoolType;
    PoolType m_pool;
    std::string m_dir;
    bool m_query_mode;
    bool m_reg;
    static const char valid_delimiter   = '/';
    static const char invalid_delimiter = '\\';

public:

    tsStreamPool();
    ~tsStreamPool();

    void Init(bool query_mode);
    void Close();

    const char* Get(std::string name, std::string path = "");
    void Reg();

};
