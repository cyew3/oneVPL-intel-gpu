/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2015 Intel Corporation. All Rights Reserved.

File Name: msdk_import.h

\* ****************************************************************************** */

#pragma once

#include "mfx_dispatcher.h"
/* declare function prototypes */

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    extern return_value (* p##func_name ) formal_param_list;

#include "msdka_exposed_functions_list.h"


/* declare function exported from dispatcher_trace.lib*/
#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    return_value func_name formal_param_list;

#include "msdka_exposed_functions_list.h"