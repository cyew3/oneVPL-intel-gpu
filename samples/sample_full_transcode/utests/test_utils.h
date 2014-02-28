//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
#pragma once


template <class T>
inline std::auto_ptr<T> & instant_auto_ptr(T *obj) {
    static std::auto_ptr<T> ptr;
    ptr.reset(obj);
    return ptr;
}

template <class T, class TObj>
inline std::auto_ptr<T> & instant_auto_ptr2(TObj *obj) {
    static std::auto_ptr<T> ptr;
    ptr.reset(obj);
    return ptr;
}

//requires mock_parser object
#define DECL_OPTION_IN_PARSER(option_name, option_value, hdl)\
EXPECT_CALL(hdl, GetValue(0)).WillRepeatedly(Return(MSDK_CHAR(option_value)));\
EXPECT_CALL(hdl, Exist()).WillRepeatedly(Return(true));\
EXPECT_CALL(mock_parser, at(msdk_string(option_name))).WillRepeatedly(ReturnRef(hdl));\
EXPECT_CALL(mock_parser, IsPresent(msdk_string(option_name))).WillRepeatedly(Return(true));

#define DECL_NO_OPTION_IN_PARSER(option_name)\
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(option_name))).WillRepeatedly(Return(false));