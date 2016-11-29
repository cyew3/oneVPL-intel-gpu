/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_PIPELINE_DEFS_H
#define __MFX_PIPELINE_DEFS_H

#include<memory>
#define _ATL_DISABLE_NOTHROW_NEW

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#endif // #if defined(_WIN32) || defined(_WIN64)

#include <memory>
#include <string>
#include <sstream>
#include <list>
#include <vector>
#include <map>
#include <algorithm>

#pragma warning(disable: 4996)

#include <errno.h>
#include "mfxdefs.h"
//#include "app_defs.h"
//#include "automatic_pointer.h"
#include "mfxvideo.h"
#include "mfxcamera.h"
#include "vm_strings.h"
#include "vm_time.h"
#include "vm_file.h"
#include "trace_utils.h"
#include "mfx_pipeline_features.h"
//TODO: need to link dependencies without recompiling core
#include "mfx_conditional_linking.h"
#include "mfx_trace.h"



//////////////////////////////////////////////////////////////////////////
typedef enum {
    MFX_CONTAINER_RAW,
    MFX_CONTAINER_MKV=100,
    MFX_CONTAINER_CRMF
} mfxContainer;

enum {
    MFX_CODEC_H263 = MFX_MAKEFOURCC('H','2','6','3'),
};

//////////////////////////////////////////////////////////////////////////
#if defined(WIN32) || defined(WIN64)
#define PAVP_BUILD
#endif // defined(WIN32) || defined(WIN64) 

//////////////////////////////////////////////////////////////////////////
//custom error codes - to separate errors from MFX components from errors in pipeline level
#define MAKE_MFX_STATUS(code)               (mfxStatus)(code)
#define MFX_USER_ERR_STATUS                 MAKE_MFX_STATUS(-1000)
#define MFX_USER_WRN_STATUS                 MAKE_MFX_STATUS(1000)

//pipeline level error codes
#define PIPELINE_ERR_STOPPED                MAKE_MFX_STATUS(MFX_USER_ERR_STATUS - 1)
#define PIPELINE_ERR_FILES_SIZES_MISMATCH   MAKE_MFX_STATUS(MFX_USER_ERR_STATUS - 2)
#define PIPELINE_ERR_THRESHOLD              MAKE_MFX_STATUS(MFX_USER_ERR_STATUS - 3)
#define PIPELINE_ERROR_FILE_READ            MAKE_MFX_STATUS(MFX_USER_ERR_STATUS - 4)
#define PIPELINE_ERROR_RESET_FAILED         MAKE_MFX_STATUS(MFX_USER_ERR_STATUS - 5)


// new warning codes
#define PIPELINE_WRN_BUFFERING_UNAVAILABLE  MFX_USER_WRN_STATUS 
//indicates that state has switched, and process need to continue
#define PIPELINE_WRN_SWITCH_STATE           MAKE_MFX_STATUS(MFX_USER_WRN_STATUS + 1)



//////////////////////////////////////////////////////////////////////////
//constants

//default length of buffer for encoded stream comparison with reference
#define MAX_CACHED_BUF_LEN                  1024*1024


//no debloking hint
#define MFX_SKIP_MODE_NO_DBLK               0x22
//status reporting disabling for decoder
#define MFX_SKIP_MODE_NO_STATUS_REP         0x23

//message ids for future mark like style notifications using SendNotifyMessage()
#define MSG_BUFFERING_START  0
#define MSG_BUFFERING_FAILED 1
#define MSG_BUFFERING_FINISH 2

#define MSG_PROCESSING_START  3
#define MSG_PROCESSING_FAILED 4
#define MSG_PROCESSING_FINISH 5

//max file path that is used
#define MAX_FILE_PATH 1024
//////////////////////////////////////////////////////////////////////////
//extendend version of picstruct support 
#define PIPELINE_PICSTRUCT_NOT_SPECIFIED  -2 //picstruct not specified in command line
#define PIPELINE_PICSTRUCT_UNKNOWN        -1 //== MFX_PICSTRUCT_UNKNOWN
#define PIPELINE_PICSTRUCT_PROGRESSIVE     0 //== MFX_PICSTRUCT_PROGRESSIVE
#define PIPELINE_PICSTRUCT_FIELD_TFF       1 //== MFX_PICSTRUCT_FIELD_TFF
#define PIPELINE_PICSTRUCT_FIELD_BFF       2 //== MFX_PICSTRUCT_FIELD_BFF
#define PIPELINE_PICSTRUCT_FIELD_TFF2      3 //== MFX_PICSTRUCT_FIELD_TFF + MFX_CODINGOPTION_ON
#define PIPELINE_PICSTRUCT_FIELD_BFF2      4 //== MFX_PICSTRUCT_FIELD_BFF + MFX_CODINGOPTION_ON

//////////////////////////////////////////////////////////////////////////
//trace support

#define TRACE_NONE         0
#define TRACE_CONSOLE      1 
#define TRACE_ERR_CONSOLE  2 
#define TRACE_DEBUG        4

//pipeline has different sink connected to each providers
#define PROVIDER_INFO  0x1
#define PROVIDER_ERROR 0x2
#define PROVIDER_WRN   0x4
#define PROVIDER_ALL   0xFF

#define PIPELINE_ERR_TRACE      TRACE_CONSOLE | TRACE_DEBUG
#define PIPELINE_WRN_TRACE      TRACE_CONSOLE | TRACE_DEBUG
#define PIPELINE_INFO_TRACE     TRACE_CONSOLE
#define PipelineTrace(str)      PipelineTraceEx str
#define PipelineTraceForce(str) PipelineTraceForceEx str
#define LAST_ERR_OR_UNK()       (PE_NO_ERROR == PipelineGetLastErr() ? PE_UNKNOWN_ERROR : PipelineGetLastErr())
static mfxU16 gamma_point[64] =
{
    0,  94, 104, 114, 124, 134, 144, 154, 159, 164, 169, 174, 179, 184, 194, 199,
    204, 209, 214, 219, 224, 230, 236, 246, 256, 266, 276, 286, 296, 306, 316, 326,
    336, 346, 356, 366, 376, 386, 396, 406, 416, 426, 436, 446, 456, 466, 476, 486,
    496, 516, 526, 536, 546, 556, 566, 576, 586, 596, 606, 616, 626, 636, 646, 1023
};
static mfxU16 gamma_correct[64] =
{
    4,   4,  20,  37,  56,  75,  96, 117, 128, 140, 150, 161, 171, 180, 198, 207,
    216, 224, 232, 240, 249, 258, 268, 283, 298, 310, 329, 344, 359, 374, 389, 404,
    420, 435, 451, 466, 482, 498, 515, 531, 548, 565, 582, 599, 617, 635, 653, 671,
    690, 729, 749, 769, 790, 811, 832, 854, 876, 899, 922, 945, 969, 994, 1019,1019
};


#ifdef UNICODE
    #define WIDEN2(x) L ## x
    #define WIDEN(x) WIDEN2(x)
    //used in string formating in wprintf for example
    #define STR_TOKEN() L"%S"
    //used in wstring formating in wprintf for example
    #define WSTR_TOKEN() L"%s"
#else
    #define WIDEN(x) x
    #define STR_TOKEN() "%s"
    #define WSTR_TOKEN() "%S"
#endif

#define EVAL1(macro) macro
#define EVAL(macro) EVAL1(macro)

#define __TFILE__ WIDEN(__FILE__)
#define _TSTR(value) WIDEN(#value)

#define MFX_PP_COMMA() ,

//group of trace macro

#define MFX_CHECK_SET_ERR_EXPR(expr, expt_to_trace, err_code, ret_val)\
if (0 == (expr))\
{\
    PipelineSetLastErr(err_code,_TSTR(expt_to_trace), __TFILE__,__LINE__);\
    return ret_val;\
}

#define MFX_CHECK_SET_ERR_EXPR2(expr, expt_to_trace, expr_to_trace2, err_code, ret_val)\
    if (0 == (expr))\
{\
    PipelineSetLastErr(err_code,_TSTR(expt_to_trace), __TFILE__,__LINE__);\
    PipelineTrace(expr_to_trace2);\
    return ret_val;\
}

//check for mfx sts or set pipeline err code
#define MFX_CHECK_STS_SET_ERR(expr, err_code)\
{ \
    mfxStatus s = expr; \
    if (MFX_ERR_NONE > s || MFX_WRN_IN_EXECUTION == s || MFX_WRN_DEVICE_BUSY == s) \
    { \
        PipelineSetLastMFXErr(err_code, s, _TSTR(expr), __TFILE__,__LINE__); \
        return s; \
    } if (MFX_ERR_NONE != s)\
    {\
        PipelineTraceMFXWrn(s, _TSTR(expr), __TFILE__,__LINE__); \
    }\
}

#define MFX_CHECK_STS_NO_RETURN_TRACE_EXPR(sts, expr, bReturn)\
{ \
    bReturn = false;\
    if (MFX_ERR_NONE > sts || MFX_WRN_IN_EXECUTION == sts || MFX_WRN_DEVICE_BUSY == sts) \
    { \
        PipelineSetLastMFXErr(LAST_ERR_OR_UNK(), sts, expr, __TFILE__,__LINE__); \
        bReturn = true;\
    } else if (MFX_ERR_NONE != sts)\
    {\
        PipelineTraceMFXWrn(sts, expr, __TFILE__,__LINE__); \
    }\
}

#define MFX_CHECK_STS_TRACE_EXPR(sts, expr)\
{ \
    bool bRet;\
    MFX_CHECK_STS_NO_RETURN_TRACE_EXPR(sts, _TSTR(expr), bRet);\
    if (bRet) \
        return sts;\
}

//evaluate string expression before racing
#define MFX_CHECK_STS_TRACE_EVAL_EXPR(sts, expr)\
{ \
    bool bRet;\
    MFX_CHECK_STS_NO_RETURN_TRACE_EXPR(sts, expr, bRet);\
    if (bRet) \
    return sts;\
}


#define MFX_CHECK_STS(expr) \
{ \
    mfxStatus s = expr; \
    MFX_CHECK_STS_TRACE_EXPR(s, expr);\
}

//provide calling to custom handler if error occured
#define MFX_CHECK_STS_CUSTOM_HANDLER(expr, custom_handler) \
{ \
    mfxStatus s = expr; \
    bool      bRet;\
    MFX_CHECK_STS_NO_RETURN_TRACE_EXPR(s, _TSTR(expr), bRet);\
    if (bRet)\
    {\
        custom_handler;\
        return s;\
    }\
}

//provide calling to custom handler if error occured
#define MFX_CHECK_STS_SKIP_CUSTOM_HANDLER(expr, skip_sts, custom_handler) \
{ \
    mfxStatus s = (expr);\
    s = (skip_sts == s) ? MFX_ERR_NONE : s; \
    bool      bRet;\
    MFX_CHECK_STS_NO_RETURN_TRACE_EXPR(s, expr, bRet);\
    if (bRet)\
    {\
        custom_handler;\
        return s;\
    }\
}

#define MFX_CHECK_STS_SKIP(expr, ...) \
{\
    mfxStatus s = (expr);\
    if (MFX_ERR_NONE != s)\
        s = CheckStatusMultiSkip(s, __VA_ARGS__, MFX_USER_ERR_STATUS);\
    MFX_CHECK_STS_TRACE_EXPR(s, expr);\
}

#define MFX_CHECK_STS_AND_THROW(expr)\
{ \
    mfxStatus s = expr; \
    bool      bRet;\
    MFX_CHECK_STS_NO_RETURN_TRACE_EXPR(s, _TSTR(expr), bRet);\
    if (bRet)\
    {\
        throw std::runtime_error(#expr);\
    }\
}

//////////////////////////////////////////////////////////////////////////
//NON MFX err codes

#define MFX_CHECK_UMC_STS(err) \
{\
    mfxStatus sts = (0 == err) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;\
    MFX_CHECK_STS_TRACE_EXPR(sts, err);\
}

#define MFX_CHECK_TRACE(value, trace_string)\
{ \
    int tmp_value = value;\
    if (0 == tmp_value) \
    { \
    PipelineSetLastMFXErr(LAST_ERR_OR_UNK(), MFX_ERR_UNKNOWN, trace_string, __TFILE__,__LINE__); \
    return MFX_ERR_UNKNOWN; \
    } \
}

#define MFX_TRACE_ERR(stream) { \
    tstringstream s;\
    s << stream;\
    PipelineSetLastErr(LAST_ERR_OR_UNK(), s.str().c_str(), __TFILE__,__LINE__); \
}

#define MFX_TRACE_INFO(stream) { \
    tstringstream s;\
    s << stream;\
    PipelineTrace((VM_STRING("%s"), s.str().c_str()));\
}


#define MFX_CHECK_TRACE_EXPR(value, expr) \
    MFX_CHECK_TRACE(value, _TSTR(expr))

#define MFX_CHECK_SET_ERR(expr, err_code, ret_val)\
    MFX_CHECK_SET_ERR_EXPR(expr, expr, err_code, ret_val);

#define MFX_CHECK_POINTER(ptr)\
    MFX_CHECK_SET_ERR(NULL != (ptr), LAST_ERR_OR_UNK(), MFX_ERR_NULL_PTR);\

#define MFX_CHECK(expr) \
    MFX_CHECK_SET_ERR(expr, LAST_ERR_OR_UNK(), MFX_ERR_UNKNOWN);\

#define MFX_CHECK_AND_THROW(expr)\
if (0 == (expr))\
{\
    PipelineSetLastErr(LAST_ERR_OR_UNK(), _TSTR(expr), __TFILE__,__LINE__);\
    throw std::runtime_error(#expr);\
}


#define MFX_CHECK_WITH_ERR(expr, ret) \
    MFX_CHECK_SET_ERR(expr,LAST_ERR_OR_UNK(), ret);\

#define MFX_CHECK_WITH_FNC_NAME(expr, fncName) \
    MFX_CHECK_SET_ERR_EXPR(expr, fncName, LAST_ERR_OR_UNK(), MFX_ERR_UNKNOWN);\

#if defined(_WIN32) || defined(_WIN64)
#define MFX_CHECK_LASTERR(expr) \
{if (0 == (expr))\
{\
    mfxU32 nLastErr = 0;\
    nLastErr = GetLastError();\
    vm_char pStringMsg[1024];\
    MessageFromCode(nLastErr, pStringMsg, MFX_ARRAY_SIZE(pStringMsg));\
    MFX_CHECK_SET_ERR_EXPR2(expr, expr, (VM_STRING("GetLastError() = %d : %s"), nLastErr, pStringMsg), LAST_ERR_OR_UNK(), MFX_ERR_UNKNOWN);\
}}
#else
#define MFX_CHECK_LASTERR(expr) MFX_CHECK_WITH_ERR(expr, MFX_ERR_UNKNOWN)
#endif // #if defined(_WIN32) || defined(_WIN64)

#define MFX_TRACE_AT_EXIT_IF(exit_code, expression, pe_error, trace)\
if (0==(expression))\
{\
    PipelineSetLastErr(pe_error);\
    PipelineTrace(trace);\
}\
return exit_code;

#define MFX_TRACE_AND_EXIT(exit_code, trace)\
{\
    PipelineSetLastErr(LAST_ERR_OR_UNK());\
    PipelineTrace (trace);\
    return exit_code;\
}

#define MFX_TRACE_AND_THROW(trace)\
{\
    PipelineSetLastErr(LAST_ERR_OR_UNK());\
    PipelineTrace (trace);\
    throw 1;\
}



#define MFX_SKIP_OR_RETURN(action, skip_sts, return_sts)\
{\
    mfxStatus err = action;\
    if (err == return_sts)\
    {\
        return MFX_ERR_NONE;\
    }\
    else \
    {\
        MFX_CHECK_STS_SKIP(err, skip_sts);\
    }\
}

#if defined(_WIN32) || defined(_WIN64)
#define mfx_strerror _tcserror
#else
#define mfx_strerror strerror
#endif

#define MFX_CHECK_VM_FOPEN(desc, pFileName, access) \
    if (NULL == (desc = vm_file_fopen(pFileName, access))) \
{\
    PipelineTrace((VM_STRING("Error opening %s=%s: %s\n"), VM_STRING(#pFileName), pFileName, mfx_strerror(errno)));\
    return MFX_ERR_UNKNOWN; \
}

extern std::map<std::string, std::basic_string<vm_char> > __stored_exprs ;

//stores expressions in global map, however stored expressions cannot be used in another function
#define STORE_EXP(expr,expr_id)\
    (__stored_exprs[EVAL(__FILE__) ## EVAL(__FUNCTION__) # expr_id] = _TSTR(expr), expr)\


#define RETRIEVE_EXPR(expr_id) __stored_exprs[EVAL(__FILE__) ## EVAL(__FUNCTION__) # expr_id].c_str()

#ifdef LOG_TIME
mfxF64 MFXGetTime(mfxU64 start_time, mfxU64 end_time);
#define TIME_START()\
    LARGE_INTEGER tstart, tend;\
    QueryPerformanceCounter(&tstart);

#define TIME_PRINT(str)\
{\
    QueryPerformanceCounter(&tend);\
    PipelineTrace((str##VM_STRING(": %f\n"),MFXGetTime(tstart.QuadPart, tend.QuadPart)));\
    tstart = tend;\
}

#else
#define TIME_START()
#define TIME_PRINT(str)
#endif

//////////////////////////////////////////////////////////////////////////
//other macros
#ifdef WIN32
    #define MFX_ARRAY_SIZE(array) _countof(array)
#else
    #define MFX_ARRAY_SIZE(array) (sizeof(array)/sizeof(*(array)))
#endif
#define MFX_ZERO_MEM(x) memset(&x, 0, sizeof(x))
template <class T> bool IsZero(T & Struct) {
    static T zeroStruct = {0};
    return 0 == memcmp(&zeroStruct, &Struct, sizeof(T));
}

#define MFX_DELETE(x) {if (x){delete x; x = NULL;}}
#define MFX_DELETE_ARRAY(x) {if (x){delete []x; x = NULL;}}
#define MFX_SAFE_RELEASE(PTR)   { if (PTR) { PTR->Release(); PTR = NULL; } }


//make operation for each element
#define MFX_FOR(n, op)\
{\
    for (int i = 0; i <n; i++)\
    {\
    op ;\
    }\
}

#define MFX_CHROMA_OFFSET(pSurface)\
    ((pSurface->Info.CropX    >> 1 /*pSurface->FrameInfo.ScaleCWidth*/) + \
    ((pSurface->Data.Pitch     >> 1 /*pSurface->FrameInfo.ScaleCPitch*/) * \
    (pSurface->Info.CropY     >> 1 /*pSurface->FrameInfo.ScaleCHeight*/)))

#define MFX_LUMA_OFFSET(pSurface)\
    (pSurface->Info.CropX + (pSurface->Data.Pitch * pSurface->Info.CropY))

#define MFX_CHROMA_SIZE(pSurface)\
((pSurface->Info.CropW >> 1/*pSurface->Info.ScaleCWidth*/)*\
(pSurface->Info.CropH  >> 1/*pSurface->Info.ScaleCHeight*/))

#ifndef UNREFERENCED_PARAMETER
    #define UNREFERENCED_PARAMETER(par) par;
#endif

//fills a double_value or returned with an error
#define MFX_PARSE_DOUBLE(double_value, string_value)\
{\
    vm_char *stopCharacter;\
    MFX_CHECK((double_value = vm_string_strtod(string_value, &stopCharacter), stopCharacter != string_value));\
}

//fills a int value or returned with an error
#define MFX_PARSE_INT(int_value, string_value)\
{\
    vm_char *stopCharacter;\
    MFX_CHECK((mfx_silent_assign(int_value, vm_string_strtol(string_value, &stopCharacter, 10)), stopCharacter != string_value));\
}

//clear error status in pipeline
#define MFX_CLEAR_LASTERR() PipelineSetLastErr(0,0,0,0)

//macro for parsing command line routines
#define HANDLE_INT_OPTION(opt_variable, optname1, description )  \
    HANDLE_INT_OPTION2(opt_variable, optname1, description )

//macro for parsing multiparams command line options
#define HANDLE_SPECIAL_OPTION(opt_variable, optname1, description, opt_special, params_description )  \
if (m_OptProc.Check(argv[0], optname1, description, opt_special, params_description ))\
{\
    MFX_CHECK(DeSerialize(opt_variable, ++argv, argvEnd));\
}

//long and short com
#define HANDLE_INT_OPTION2(opt_variable, optname1, description )  \
    if (m_OptProc.Check(argv[0], optname1, description, OPT_INT_32))                  \
{                                                                                       \
    MFX_CHECK(1 + argv != argvEnd);                                                     \
    MFX_PARSE_INT(opt_variable, argv[1])                                      \
    argv++;                                                                             \
}

#define HANDLE_INT_OPTION_PLUS1(opt_variable, optname1, description )  \
    if (m_OptProc.Check(argv[0], optname1, description, OPT_INT_32))                  \
{                                                                                       \
    MFX_CHECK(1 + argv != argvEnd);                                                     \
    MFX_PARSE_INT(opt_variable, argv[1])                                      \
    ++opt_variable;                                                             \
    argv++;                                                                             \
}

#define HANDLE_BOOL_OPTION(opt_variable, optname1, description)\
    if (m_OptProc.Check(argv[0], optname1, description, OPT_UNDEFINED))\
    for(bool sstep = false; !sstep; sstep = true, opt_variable = true)

#define HANDLE_64F_OPTION(opt_variable, optname1, description)\
    if (m_OptProc.Check(argv[0], optname1, description, OPT_64F))         \
{                                                                           \
    MFX_CHECK(1 + argv != argvEnd);                                         \
    MFX_PARSE_DOUBLE(opt_variable, argv[1]);                                \
    argv++;                                                                 \
}

#define HANDLE_64F_OPTION_AND_FLAG(opt_variable, optname1, description, flag_variable)\
    if (m_OptProc.Check(argv[0], optname1, description, OPT_64F))           \
{                                                                           \
    MFX_CHECK(1 + argv != argvEnd);                                         \
    MFX_PARSE_DOUBLE(opt_variable, argv[1]);                                \
    argv++;                                                                 \
    flag_variable = true;                                                   \
}

#define HANDLE_FILENAME_OPTION(opt_variable, optname1, description)\
if (m_OptProc.Check(argv[0], optname1, description, OPT_FILENAME)) \
{\
    MFX_CHECK(1 + argv != argvEnd);\
    argv++;\
    vm_string_strcpy_s(opt_variable, MFX_ARRAY_SIZE(opt_variable), argv[0]);\
}


//eliminating sizeof(a)==sizeof(b)
#pragma warning (disable: 4127)

#ifndef STATIC_ASSERT
    #define STATIC_ASSERT(expr, name) typedef char name[expr ? 1 : -1]
#endif

#define sizeofmem(s,m) sizeof(((s *)1000)->m)
//gcc compiler varies on this macro
#define myoffsetof(s,m) ((size_t)&(((s*)0)->m))

#define STATIC_CHECK_FIELD(struct_type1, struct_type2, member)\
    STATIC_ASSERT(myoffsetof(struct_type1, member) == myoffsetof(struct_type2, member), assert1##__LINE__);\
    STATIC_ASSERT(sizeofmem(struct_type1, member) == sizeofmem(struct_type2, member), assert2##__LINE__)

#define DYN_CHECK_FIELD(struct_type1, struct_type2, member)\
    MFX_CHECK_AND_THROW(myoffsetof(struct_type1, member) == myoffsetof(struct_type2, member));\
    MFX_CHECK_AND_THROW(sizeofmem(struct_type1, member) == sizeofmem(struct_type2, member));

#define MFX_MIN_POINTER( a, b ) ( ((a) < (b)) ? ((a == 0) ? (b) : (a)) : ((b == 0) ? (a) : (b)) )

//TODO:fix includes
#include "mfx_pipeline_types.h"


#endif//__MFX_PIPELINE_DEFS_H
