
/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "test_registry.h"

#define VERIFY_CALL(obj, method_name)\
    CHECK(TestParamsRegistry::Instance().GetParams(obj->MockClassName() + #method_name, 0, NULL));

#define VERIFY_CALL_GETARGS(obj, method_name, params)\
    CHECK(TestParamsRegistry::Instance().GetParams(obj->MockClassName() + #method_name, 0, (BaseHolder**)&params)));

#define VERIFY_NCALL(obj, method_name, order)\
    CHECK(TestParamsRegistry::Instance().GetParams(obj->MockClassName() + #method_name, order, NULL));

#define VERIFY_NCALL_GETARGS(obj, method_name, order, params)\
    CHECK(TestParamsRegistry::Instance().GetParams(obj->MockClassName() + #method_name, order, (BaseHolder**)&params));

#define VERIFY_NOT_CALL(obj, method_name)\
    CHECK(!TestParamsRegistry::Instance().GetParams(obj->MockClassName() + #method_name, 0, NULL));

#define VERIFY_NOT_NCALL(obj, method_name, order)\
    CHECK(!TestParamsRegistry::Instance().GetParams(obj->MockClassName() + #method_name, order, NULL));


#define DECLARE_MOCK_CLASS(class_name)\
    public:\
    static std::string MockClassName(){return #class_name;}


