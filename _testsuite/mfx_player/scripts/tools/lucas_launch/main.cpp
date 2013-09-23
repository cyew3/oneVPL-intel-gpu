
#include "vm_shared_object.h"
#include <iostream>

#ifdef WIN32
    #include "windows.h"
#endif

#define LUCAS_DLL
#include "lucas.h"

typedef  int (*LucasMain)(int argc, wchar_t * argv[], lucas::TFMWrapperCtx *ctx);

using namespace std;

#include <string>
#include <sstream>

inline std::wstring convert_s2w(const std::string &str)
{
    std::wstring  sout;
    sout.reserve(str.length() + 1);

    for (std::string::const_iterator it = str.begin(); it != str.end(); it ++)
    {
        sout.push_back(*it);
    }
    return sout;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cout<<"usage: " << *argv << " mfx_player.dll|mfx_transcoder.dll\n";
        return 0;
    }

#ifdef WIN32
    std::wstring wdll_name = convert_s2w(argv[1]);
#else
    std::string wdll_name = argv[1];
#endif

    std::string dll_name = argv[1];

    vm_so_handle hdl = vm_so_load((vm_char*)wdll_name.c_str());

    if (!hdl)
    {
        std::cout<<"Cannot open module" << dll_name << std::endl;
        return 0;
    }
    vm_so_func  fnc = vm_so_get_addr(hdl, "LucasMain");
    if (!fnc)
    {
        std::cout<<"Cannot vm_so_get_addr LucasMain in module" << dll_name << std::endl;
        return 0;
    }
    LucasMain lm = (LucasMain)fnc;
    //invoking lucas_main
    wchar_t param1[] = {L"-protected"};
    wchar_t *param[] = {param1};

    lucas::TFMWrapperCtx lucas_ctx;
    std::cout<<"invoking LucasMain " << std::endl;
    lm(1, param, &lucas_ctx);
}