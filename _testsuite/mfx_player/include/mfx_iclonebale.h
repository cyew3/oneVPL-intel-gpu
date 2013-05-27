/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

//prototype based renderer are used in multirender
struct ICloneable
{
public :
    virtual ~ICloneable(){}
public:
    virtual ICloneable * Clone() = 0;
};

#define IMPLEMENT_CLONE(class_name)\
public:\
    virtual class_name * Clone(){return new class_name(*this);}


//unimplemented version of clone
#define DECLARE_CLONE(class_name)\
    public:\
    virtual class_name * Clone() = 0;
