/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#if !defined(_WIN32) && !defined(_WIN64)

#include "mfx_trace_utils.h"
#include <sstream>
#include <iomanip>

#ifdef MFX_TRACE_ENABLE
extern "C"
{

#include <stdlib.h>
#include "vm_sys_info.h"

/*------------------------------------------------------------------------------*/

FILE* mfx_trace_open_conf_file(const char* name)
{
    FILE* file = NULL;
    char file_name[MAX_PATH] = {0};

    if (getenv("HOME"))
    {
        snprintf(file_name, MAX_PATH-1, "%s/.%s", getenv("HOME"), name);
        file = fopen(file_name, "r");
    }
    else
    {
        snprintf(file_name, MAX_PATH-1, "%s/%s", MFX_TRACE_CONFIG_PATH, name);
        file = fopen(file_name, "r");
    }
    return file;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 mfx_trace_get_value_pos(FILE* file,
                                    const char* pName,
                                    char* line, mfxTraceU32 line_size,
                                    char** value_pos)
{
    char *str = NULL, *p = NULL;
    mfxTraceU32 n = 0;
    bool bFound = false;

    if (!file || ! pName || !value_pos) return 1;
    while (NULL != (str = fgets(line, line_size-1,  file)))
    {
        n = strnlen_s(str, line_size-1);
        if ((n > 0) && (str[n-1] == '\n')) str[n-1] = '\0';
        for(; strchr(" \t", *str) && (*str); str++);
        n = strnlen_s(pName, 256);
        if (!strncmp(str, pName, n))
        {
            str += n;
            if (!strchr(" =\t", *str)) continue;
            for(; strchr(" =\t", *str) && (*str); str++);
            bFound = true;
            *value_pos = str;
            break;
        }
    }
    return (bFound)? 0: 1;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 mfx_trace_get_conf_dword(FILE* file,
                                     const char* pName,
                                     mfxTraceU32* pValue)
{
    char line[MAX_PATH] = {0}, *value_pos = NULL;
    
    if (!mfx_trace_get_value_pos(file, pName, line, sizeof(line), &value_pos))
    {
        if (!strncmp(value_pos, "0x", 2))
        {
            value_pos += 2;
            std::stringstream ss(value_pos);
            std::string s;
            ss >> std::setw(8) >> s;
            std::stringstream(s) >> std::hex >> *pValue;
        }
        else
        {
            *pValue = atoi(value_pos);
        }
        return 0;
    }
    return 1;
}


mfxTraceU32 mfx_trace_get_conf_string(FILE* file,
                                      const char* pName,
                                      mfxTraceChar* pValue, mfxTraceU32 cValueMaxSize)
{
    char line[MAX_PATH] = {0}, *value_pos = NULL;
    
    if (!mfx_trace_get_value_pos(file, pName, line, sizeof(line), &value_pos))
    {
        strncpy(pValue, value_pos, cValueMaxSize-1);
        return 0;
    }
    return 1;
}

} // extern "C"
#endif // #ifdef MFX_TRACE_ENABLE

#endif // #if !defined(_WIN32) && !defined(_WIN64)
