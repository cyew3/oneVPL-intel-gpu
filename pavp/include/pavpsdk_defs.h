/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __PAVPSDK_DEFS_H__
#define __PAVPSDK_DEFS_H__

#include <sdkddkver.h>
#include <assert.h>

#include "pavpsdk_error.h"

#define PAVPSDK_COUNT_OF(arr) (sizeof(arr)/sizeof((arr)[0]))
#define PAVPSDK_SAFE_RELEASE(x) if (x) { (x)->Release(); (x) = NULL; }
#define PAVPSDK_SAFE_DELETE(x) if (x) { delete (x); (x) = NULL; }
#define PAVPSDK_SAFE_DELETE_ARRAY(x) if (x) { delete[] (unsigned char*)(x); (x)=NULL; }
#define PAVPSDK_ASSERT(_expression) assert(_expression);

#include <tchar.h>
#include <stdio.h>
#define pavpsdk_T _T
#define pavpsdk_printf _tprintf
#define PAVPSDK_DEBUG_MSG( DisplayLevel, FormatString_AND_VariableParameterList ) \
    pavpsdk_printf FormatString_AND_VariableParameterList;

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define PAVPSDK_DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);               \
    void operator=(const TypeName&)   


#endif //__PAVPSDK_DEFS_H__