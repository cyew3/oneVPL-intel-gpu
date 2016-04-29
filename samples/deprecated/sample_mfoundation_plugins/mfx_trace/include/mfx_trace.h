/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

/*********************************************************************************

File: mfx_trace.h

Purpose: contains definition data for MFX tracing.

*********************************************************************************/
#ifndef __MFX_TRACE_H__
#define __MFX_TRACE_H__

#include <windows.h>
#include <tchar.h>
#include <stdarg.h>
#include <stdio.h>
#include <share.h>

/*------------------------------------------------------------------------------*/

// enables tracing
#ifndef MFX_TRACE_DISABLE
    #define MFX_TRACE_ENABLE
#endif

// registry path to trace options and parameters
#define MFX_TRACE_REG_ROOT            HKEY_CURRENT_USER
#define MFX_TRACE_REG_PARAMS_PATH     "Software\\Intel\\Intel MFX Trace"
#define MFX_TRACE_REG_CATEGORIES_PATH "Software\\Intel\\Intel MFX Trace\\Categories"
// trace registry options and parameters
#define MFX_TRACE_REG_OMODE_TYPE "MfxTrace_OutputMode"
#define MFX_TRACE_REG_CATEGORIES "MfxTrace_Categories"
#define MFX_TRACE_REG_LEVEL      "MfxTrace_Level"

// list of output modes
enum
{
    MFX_TRACE_OMODE_TRASH = 0x00,
    MFX_TRACE_OMODE_FILE  = 0x01,
    MFX_TRACE_OMODE_STAT  = 0x02,
    MFX_TRACE_OMODE_ETW   = 0x04
};

// defines default trace category
#define MFX_TRACE_CATEGORY_DEFAULT g_mfxTraceDefCategory

// defines category for the current module
#ifndef MFX_TRACE_CATEGORY
    #define MFX_TRACE_CATEGORY MFX_TRACE_CATEGORY_DEFAULT
#endif

// enumeration of the trace levels inside any category
typedef enum
{
    MFX_TRACE_LEVEL_0 = 0,
    MFX_TRACE_LEVEL_1 = 1,
    MFX_TRACE_LEVEL_2 = 2,
    MFX_TRACE_LEVEL_3 = 3,
    MFX_TRACE_LEVEL_4 = 4,
    MFX_TRACE_LEVEL_5 = 5,
    MFX_TRACE_LEVEL_6 = 6,
    MFX_TRACE_LEVEL_7 = 7,
    MFX_TRACE_LEVEL_8 = 8,
    MFX_TRACE_LEVEL_9 = 9,
    MFX_TRACE_LEVEL_10 = 10,

    MFX_TRACE_LEVEL_MAX = 0xFF
} mfxTraceLevel;

// defines default trace level
#define MFX_TRACE_LEVEL_DEFAULT MFX_TRACE_LEVEL_MAX

// defines default trace level for the current module
#ifndef MFX_TRACE_LEVEL
    #define MFX_TRACE_LEVEL MFX_TRACE_LEVEL_DEFAULT
#endif

// defines maximum length of output line
#define MFX_TRACE_MAX_LINE_LENGTH 1024

// enables logging of every task in stat mode (in ms)
//#define MFX_STAT_TICKS 65536

/*------------------------------------------------------------------------------*/

struct mfxTraceCategoryItem
{
    TCHAR  m_name[MAX_PATH];
    UINT32 m_level;
};

typedef LONGLONG mfxTraceTick;

typedef union
{
    UINT32        uint32;
    UINT64        uint64;
    mfxTraceTick  tick;
    char*         str;
    void*         ptr;
    TCHAR*        category;
    mfxTraceLevel level;
} mfxTraceHandle;

typedef struct
{
    TCHAR*        category;
    mfxTraceLevel level;
    // reserved for file dump:
    mfxTraceHandle fd1;
    mfxTraceHandle fd2;
    mfxTraceHandle fd3;
    mfxTraceHandle fd4;
    // reserved for stat dump:
    mfxTraceHandle sd1;
    mfxTraceHandle sd2;
    mfxTraceHandle sd3;
    mfxTraceHandle sd4;
    mfxTraceHandle sd5;
    mfxTraceHandle sd6;
    mfxTraceHandle sd7;
    // reserved for ETW:
    mfxTraceHandle etw1;
    mfxTraceHandle etw2;
    mfxTraceHandle etw3;
    mfxTraceHandle etw4;
#if MFX_STAT_TICKS
    mfxTraceTick ticks[MFX_STAT_TICKS];
#endif
} mfxTraceStaticHandle;

typedef struct
{
    // reserved for file dump:
    // n/a
    // reserved for stat dump:
    mfxTraceHandle sd1;
    // reserved for TAL:
    mfxTraceHandle tal1;
    mfxTraceHandle tal2;
    mfxTraceHandle tal3;
    mfxTraceHandle tal4;
} mfxTaskHandle;

/*------------------------------------------------------------------------------*/
// basic trace functions (macroses are recommended to use instead)

UINT32 MFXTrace_Init(const TCHAR *filename, UINT32 output_mode);

UINT32 MFXTrace_Close(void);

UINT32 MFXTrace_SetLevel(TCHAR* category,
                         mfxTraceLevel level);

UINT32 MFXTrace_DebugMessage(mfxTraceStaticHandle *static_handle,
                             const char *file_name, UINT32 line_num,
                             const char *function_name,
                             TCHAR* category, mfxTraceLevel level,
                             const char *message,
                             const char *format, ...);

UINT32 MFXTrace_vDebugMessage(mfxTraceStaticHandle *static_handle,
                              const char *file_name, UINT32 line_num,
                              const char *function_name,
                              TCHAR* category, mfxTraceLevel level,
                              const char *message,
                              const char *format, va_list args);

UINT32 MFXTrace_BeginTask(mfxTraceStaticHandle *static_handle,
                          const char *file_name, UINT32 line_num,
                          const char *function_name,
                          TCHAR* category, mfxTraceLevel level,
                          const char *task_name, mfxTaskHandle *task_handle);

UINT32 MFXTrace_EndTask(mfxTraceStaticHandle *static_handle,
                        mfxTaskHandle *task_handle);

/*------------------------------------------------------------------------------*/
// basic macroses

#define MFX_TRACE_PARAMS \
    &_trace_static_handle, __FILE__, __LINE__, __FUNCTION__, MFX_TRACE_CATEGORY

#ifdef MFX_TRACE_ENABLE
    #define MFX_TRACE_INIT(_filename, _output_mode) \
        MFXTrace_Init(_filename, _output_mode);

    #define MFX_TRACE_INIT_RES(_res, _filename, _output_mode) \
        _res = MFXTrace_Init(_filename, _output_mode);

    #define MFX_TRACE_CLOSE() \
        MFXTrace_Close();

    #define MFX_TRACE_CLOSE_RES(_res, _filename, _output_mode) \
        _res = MFXTrace_Close();

    #define MFX_LTRACE(_trace_all_params)                       \
    {                                                           \
        static mfxTraceStaticHandle _trace_static_handle = {0}; \
        MFXTrace_DebugMessage _trace_all_params;                \
    }
#else
    #define MFX_TRACE_INIT(_filename, _output_mode)
    #define MFX_TRACE_INIT_RES(res, _filename, _output_mode)
    #define MFX_TRACE_CLOSE()
    #define MFX_TRACE_CLOSE_RES(res, _filename, _output_mode)
    #define MFX_LTRACE(_trace_all_params)
#endif

/*------------------------------------------------------------------------------*/
// standard formats

#define MFX_TRACE_FORMAT_WS   "%S"
#define MFX_TRACE_FORMAT_P    "%p"
#define MFX_TRACE_FORMAT_I    "%d"
#define MFX_TRACE_FORMAT_X    "%x"
#define MFX_TRACE_FORMAT_D    "%d (0x%x)"
#define MFX_TRACE_FORMAT_F    "%.4f"
#define MFX_TRACE_FORMAT_GUID "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}"

/*------------------------------------------------------------------------------*/
// these macroses permit to set trace level

#define MFX_LTRACE_1(_level, _message, _format, _arg1) \
    MFX_LTRACE((MFX_TRACE_PARAMS, _level, _message, _format, _arg1))

#define MFX_LTRACE_2(_level, _message, _format, _arg1, _arg2) \
    MFX_LTRACE((MFX_TRACE_PARAMS, _level, _message, _format, _arg1, _arg2))

#define MFX_LTRACE_3(_level, _message, _format, _arg1, _arg2, _arg3) \
    MFX_LTRACE((MFX_TRACE_PARAMS, _level, _message, _format, _arg1, _arg2, _arg3))

#define MFX_LTRACE_4(_level, _message, _format, _arg1, _arg2, _arg3, _arg4) \
    MFX_LTRACE((MFX_TRACE_PARAMS, _level, _message, _format, _arg1, _arg2, _arg3, _arg4))

#define MFX_LTRACE_5(_level, _message, _format, _arg1, _arg2, _arg3, _arg4, _arg5) \
    MFX_LTRACE((MFX_TRACE_PARAMS, _level, _message, _format, _arg1, _arg2, _arg3, _arg4, _arg5))

#define MFX_LTRACE_6(_level, _message, _format, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6) \
    MFX_LTRACE((MFX_TRACE_PARAMS, _level, _message, _format, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6))

#define MFX_LTRACE_S(_level, _message) \
    MFX_LTRACE_1(_level, _message, NULL, 0)

#define MFX_LTRACE_WS(_level, _message) \
    MFX_LTRACE_1(_level, NULL, MFX_TRACE_FORMAT_WS, _message)

#define MFX_LTRACE_P(_level, _arg1) \
    MFX_LTRACE_1(_level, #_arg1 " = ", MFX_TRACE_FORMAT_P, _arg1)

#define MFX_LTRACE_I(_level, _arg1) \
    MFX_LTRACE_1(_level, #_arg1 " = ", MFX_TRACE_FORMAT_I, _arg1)

#define MFX_LTRACE_X(_level, _arg1) \
    MFX_LTRACE_1(_level, #_arg1 " = ", MFX_TRACE_FORMAT_X, _arg1)

#define MFX_LTRACE_D(_level, _arg1) \
    MFX_LTRACE_2(_level, #_arg1 " = ", MFX_TRACE_FORMAT_D, _arg1, _arg1)

#define MFX_LTRACE_F(_level, _arg1) \
    MFX_LTRACE_1(_level, #_arg1 " = ", MFX_TRACE_FORMAT_F, _arg1)

#define MFX_LTRACE_GUID(_level, _guid) \
    MFX_LTRACE((MFX_TRACE_PARAMS, _level, #_guid " = ", \
               MFX_TRACE_FORMAT_GUID, \
               (_guid).Data1, (_guid).Data2, (_guid).Data3, \
               (_guid).Data4[0], (_guid).Data4[1], (_guid).Data4[2], (_guid).Data4[3], \
               (_guid).Data4[4], (_guid).Data4[5], (_guid).Data4[6], (_guid).Data4[7]))

#ifdef MFX_TRACE_ENABLE
    #define MFX_LTRACE_BUFFER(_level, _name, _buffer, _size) \
    { \
        UINT8 *buf = (UINT8*)(_buffer); \
        UINT32 i = 0; \
        size_t len = MFX_TRACE_MAX_LINE_LENGTH; \
        char str[MFX_TRACE_MAX_LINE_LENGTH] = {0}, *p_str = str; \
        \
        for (i = 0; (i < (_size)) && (len > 0); ++i) \
        { \
            if (i < ((_size) - 1)) \
            { \
                _snprintf_s(p_str, len, len-1, "0x%02x, ", buf[i]); \
                p_str += _scprintf("0x%02x, ", buf[i]); \
            } \
            else \
            { \
                _snprintf_s(p_str, len, len-1, "0x%02x", buf[i]); \
                p_str += _scprintf("0x%02x", buf[i]); \
            } \
            len = MFX_TRACE_MAX_LINE_LENGTH - 1 - (p_str-str); \
        } \
        MFX_LTRACE_2(_level, NULL, "sizeof(" #_name ") = %d, " #_name " = '%s'", _size, str); \
    }
#else
    #define MFX_LTRACE_BUFFER(_level, _name, _buffer, _size)
#endif

/*------------------------------------------------------------------------------*/
// these macroses uses default trace level

#define MFX_TRACE1(_message, _format, _arg1) \
    MFX_LTRACE_1(MFX_TRACE_LEVEL, _message, _format, _arg1)

#define MFX_TRACE2(_message, _format, _arg1, _arg2) \
    MFX_LTRACE_2(MFX_TRACE_LEVEL, _message, _format, _arg1, _arg2)

#define MFX_TRACE_S(_arg1) \
    MFX_LTRACE_S(MFX_TRACE_LEVEL, _arg1)

#define MFX_TRACE_WS(_message) \
    MFX_LTRACE_WS(MFX_TRACE_LEVEL, _message)

#define MFX_TRACE_P(_arg1) \
    MFX_LTRACE_P(MFX_TRACE_LEVEL, _arg1)

#define MFX_TRACE_I(_arg1) \
    MFX_LTRACE_I(MFX_TRACE_LEVEL, _arg1)

#define MFX_TRACE_X(_arg1) \
    MFX_LTRACE_X(MFX_TRACE_LEVEL, _arg1)

#define MFX_TRACE_D(_arg1) \
    MFX_LTRACE_D(MFX_TRACE_LEVEL, _arg1)

#define MFX_TRACE_F(_arg1) \
    MFX_LTRACE_F(MFX_TRACE_LEVEL, _arg1)

#define MFX_TRACE_GUID(_guid) \
    MFX_LTRACE_GUID(MFX_TRACE_LEVEL, _guid)

#define MFX_TRACE_BUFFER(_name, _buffer, _size) \
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL, _name, _buffer, _size)

/*------------------------------------------------------------------------------*/

// C++ class for BeginTask/EndTask
class MFXTraceTask
{
public:
    MFXTraceTask(mfxTraceStaticHandle *static_handle,
                 const char *file_name, UINT32 line_num,
                 const char *function_name,
                 TCHAR* category, mfxTraceLevel level,
                 const char *task_name)
    {
        m_pStaticHandle = static_handle;
        MFXTrace_BeginTask(static_handle,
                           file_name, line_num,
                           function_name,
                           category, level,
                           task_name,
                           &m_TraceTaskHandle);
    }

    ~MFXTraceTask(void)
    {
        MFXTrace_EndTask(m_pStaticHandle, &m_TraceTaskHandle);
    }

private:
    mfxTraceStaticHandle* m_pStaticHandle;
    mfxTaskHandle         m_TraceTaskHandle;
};

/*------------------------------------------------------------------------------*/
// auto tracing of the functions

#ifdef MFX_TRACE_ENABLE
    #define MFX_AUTO_LTRACE(_level, _task_name)                                \
        static mfxTraceStaticHandle _trace_static_handle;                      \
        MFXTraceTask                _mfx_trace_task(MFX_TRACE_PARAMS,          \
                                                    _level, _task_name);

    #define MFX_AUTO_LTRACE_FUNC(_level) \
        MFX_AUTO_LTRACE(_level, NULL)
#else
    #define MFX_AUTO_LTRACE(_level, _task_name)
    #define MFX_AUTO_LTRACE_FUNC(_level)
#endif

#define MFX_AUTO_TRACE(_task_name) \
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL, _task_name)

#define MFX_AUTO_TRACE_FUNC() \
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL)

/*------------------------------------------------------------------------------*/
// help macroses

#define mfx_trace_fopen(FNAME, FMODE) _fsopen(FNAME, FMODE, _SH_DENYNO)
#define mfx_trace_tfopen(FNAME, FMODE) _tfsopen(FNAME, FMODE, _SH_DENYNO)

#define mfx_trace_get_time(T,S,F) ((double)(INT64)((T)-(S))/(double)(INT64)(F))

/*------------------------------------------------------------------------------*/
// help functions

mfxTraceTick mfx_trace_get_frequency(void);
mfxTraceTick mfx_trace_get_tick(void);
char* mfx_trace_vsprintf(char* str, size_t& str_size, const char* format, va_list args);
char* mfx_trace_sprintf(char* str, size_t& str_size, const char* format, ...);

UINT32 mfx_trace_get_category_index(TCHAR* category, UINT32& index);

HRESULT mfx_trace_get_reg_dword(HKEY BaseKey,
                                const TCHAR *pPath, const TCHAR* pName,
                                UINT32* pValue);
HRESULT mfx_trace_get_reg_qword(HKEY BaseKey,
                                const TCHAR *pPath, const TCHAR* pName,
                                UINT64* pValue);
HRESULT mfx_get_reg_string(HKEY BaseKey,
                           const TCHAR *pPath, const TCHAR* pName,
                           TCHAR* pValue, UINT32 cValueMaxSize);
HRESULT mfx_get_reg_mstring(HKEY BaseKey,
                            const TCHAR *pPath, const TCHAR* pName,
                            TCHAR* pValue, UINT32 cValueMaxSize);

/*------------------------------------------------------------------------------*/
// externals

extern TCHAR* g_mfxTraceDefCategory;
extern UINT32 g_mfxTraceCategoriesNum;
extern mfxTraceCategoryItem* g_mfxTraceCategoriesTable;
extern mfxTraceTick g_mfxTraceFrequency;

#endif // #ifndef __MFX_TRACE_H__
