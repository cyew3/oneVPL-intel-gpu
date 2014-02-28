//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//
//
//*/

#pragma once

#include <ostream>
#include "sample_utils.h"

template <class T>
struct TypeToText {
     msdk_string Type() const{
         return MSDK_CHAR("unspecified type");
     }
};

template <>
struct TypeToText <mfxI64> {
    msdk_string Type() const {
        return MSDK_CHAR("int64");
    }
};

template <>
struct TypeToText <mfxU64> {
    msdk_string Type() const {
        return MSDK_CHAR("unsigned int64");
    }
};

template <>
struct TypeToText <int>{
    msdk_string Type() const{
        return MSDK_CHAR("int");
    }
};

template <>
struct TypeToText <unsigned int>{
    msdk_string Type() const{
        return MSDK_CHAR("unsigned int");
    }
};

struct kbps_t {
};

template <>
struct TypeToText <kbps_t>{
    msdk_string Type() const{
        return MSDK_CHAR("Kbps");
    }
};

template <>
struct TypeToText <char>{
    msdk_string Type() const{
        return MSDK_CHAR("char");
    }
};

template <>
struct TypeToText <unsigned char>{
    msdk_string Type() const{
        return MSDK_CHAR("unsigned char");
    }
};

template <>
struct TypeToText <short>{
    msdk_string Type() const{
        return MSDK_CHAR("short");
    }
};

template <>
struct TypeToText <unsigned short>{
    msdk_string Type() const{
        return MSDK_CHAR("unsigned short");
    }
};

template <>
struct TypeToText <float>{
    msdk_string Type() const{
        return MSDK_CHAR("float");
    }
};

template <>
struct TypeToText <double>{
    msdk_string Type() const{
        return MSDK_CHAR("double");
    }
};

template <>
struct TypeToText <msdk_string>{
    msdk_string Type() const{
        return MSDK_CHAR("string");
    }
};

template <>
struct TypeToText <bool>{
    msdk_string Type() const{
        return MSDK_CHAR("bool");
    }
};

template<class T>
inline msdk_ostream & operator << (msdk_ostream & os, const TypeToText<T> &tt) {
    os<<tt.Type();
    return os;
}
