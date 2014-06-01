#ifndef CONFIG_H_
#define CONFIG_H_

#include <fstream>
#include <map>
#include <string>
#include <vector>

class Config
{
public:
    static std::string GetParam(std::string section, std::string key);

private:
    static Config *conf;
    Config();
    ~Config();

    std::string _file_path;
    std::ifstream  _file;

    std::map<std::string, std::map<std::string, std::string> > ini;

    void Init();
    std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
    std::vector<std::string> split(const std::string &s, char delim);
};

#endif //CONFIG_H_
