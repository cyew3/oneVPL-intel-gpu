
/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

template<class T>
class mfxTypeTrait
{
public:
    typedef T value_type;
    typedef T store_type;
    static store_type assign(T from){
        return from;
    }
};

#define DECL_REFERENCE_TO_POINTER_TRAIT(type)\
template<>\
class mfxTypeTrait<type &>\
{\
public:\
    typedef type  value_type;\
    typedef type *store_type;\
    static store_type assign(type & from){\
        return &from;\
    }\
};

#define DECL_REFERENCE_TO_VALUE_TRAIT(type)\
    template<>\
class mfxTypeTrait<type &>\
{\
public:\
    typedef type value_type;\
    typedef type store_type;\
    static store_type assign(type &from){\
    return from;\
}\
};

#define DECL_REFERENCE_TO_CONST_VALUE_TRAIT(type)\
    template<>\
class mfxTypeTrait<const type &>\
{\
public:\
    typedef type value_type;\
    typedef type store_type;\
    static store_type assign(const type &from){\
    return from;\
}\
};



DECL_REFERENCE_TO_POINTER_TRAIT(mfxU8);
DECL_REFERENCE_TO_POINTER_TRAIT(mfxI8);
DECL_REFERENCE_TO_POINTER_TRAIT(mfxI16);
DECL_REFERENCE_TO_POINTER_TRAIT(mfxU16);
DECL_REFERENCE_TO_POINTER_TRAIT(mfxU32);
DECL_REFERENCE_TO_POINTER_TRAIT(mfxI32);
DECL_REFERENCE_TO_POINTER_TRAIT(mfxUL32);
DECL_REFERENCE_TO_POINTER_TRAIT(mfxL32);
DECL_REFERENCE_TO_POINTER_TRAIT(mfxF32);
DECL_REFERENCE_TO_POINTER_TRAIT(mfxF64);
DECL_REFERENCE_TO_POINTER_TRAIT(mfxU64);
DECL_REFERENCE_TO_POINTER_TRAIT(mfxI64);

//pipeline level traits
DECL_REFERENCE_TO_CONST_VALUE_TRAIT(tstring);
