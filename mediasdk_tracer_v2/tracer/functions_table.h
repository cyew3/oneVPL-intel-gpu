#ifndef FUNCTIONSTABLE_H_
#define FUNCTIONSTABLE_H_

#include "mfxvideo.h"
#include "mfxplugin.h"

#include "../loggers/log.h"
#include "../dumps/dump.h"

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    e##func_name,

typedef enum _mfxFunction
{
    eMFXInit,
    eMFXClose,
#include "bits/mfxfunctions.h"
    eFunctionsNum,
    eNoMoreFunctions = eFunctionsNum
} mfxFunction;

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    { e##func_name, #func_name },

typedef struct _mfxFunctionsTable
{
    mfxFunction id;
    const char* name;
} mfxFunctionsTable;

static const mfxFunctionsTable g_mfxFuncTable[] =
{
    { eMFXInit, "MFXInit" },
    { eMFXClose, "MFXClose" },
#include "bits/mfxfunctions.h"
    { eNoMoreFunctions, NULL }
};

typedef void (MFX_CDECL * mfxFunctionPointer)(void);

typedef mfxStatus (MFX_CDECL * MFXInitPointer)(mfxIMPL, mfxVersion *, mfxSession *);
typedef mfxStatus (MFX_CDECL * MFXClosePointer)(mfxSession session);

typedef struct _mfxLoader
{
    mfxSession session;
    void* dlhandle;
    mfxFunctionPointer table[eFunctionsNum];
} mfxLoader;

// function types
#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    typedef return_value (MFX_CDECL * f##func_name) formal_param_list;

#include "bits/mfxfunctions.h"

// forward declarations
#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    return_value func_name formal_param_list;

#include "bits/mfxfunctions.h"

#endif //FUNCTIONSTABLE_H_
