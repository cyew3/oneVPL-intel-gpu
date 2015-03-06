/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#include "pipeline_manager.h"
#include <iostream>
#include <string>

#ifdef UNICODE
    #include <clocale>
    #include <locale>
    #pragma warning(disable: 4996)
#endif

#ifdef MFX_D3D11_SUPPORT
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#endif

namespace {
    msdk_string cvt_string2msdk(const std::string & lhs)
    {
    #ifndef UNICODE
        return lhs;
    #else
        std::locale const loc("");
        std::vector<wchar_t> buffer(lhs.size() + 1);
        std::use_facet<std::ctype<wchar_t> >(loc).widen(&lhs.at(0), &lhs.at(0) + lhs.size(), &buffer[0]);
        return msdk_string(&buffer[0], &buffer[lhs.size()]);
    #endif
    }
}

#if defined(_WIN32) || defined(_WIN64)
int _tmain(int nParams, TCHAR ** sparams)
#else
int main(int nParams, char ** sparams)
#endif
{
    try
    {
        msdk_stringstream ss;
        for (int i = 1; i < nParams; i++) {
            ss<<sparams[i];
            if (i + 1 != nParams)
                ss<<MSDK_CHAR(' ');
        }
        PipelineFactory factory;
        std::auto_ptr<CmdLineParser> parser(factory.CreateCmdLineParser());
        if (nParams == 1 || !parser->Parse(ss.str())) {
            parser->PrintHelp(msdk_cout);
            return 1;
        }

        PipelineManager mgr(factory);
        mgr.Build(*parser);
        mgr.Run();

        return 0;
    }
    catch (KnownException & /*e*/) {
        //TRACEs should be generated for that exception
        MSDK_TRACE_ERROR(MSDK_STRING("Exiting..."));
    }
    catch (std::exception & e) {
        MSDK_TRACE_ERROR(MSDK_STRING("exception: ") << cvt_string2msdk(e.what()));
    }
    catch(...) {
        MSDK_TRACE_ERROR(MSDK_STRING("Unknown exception "));
    }
    return 1;
}
