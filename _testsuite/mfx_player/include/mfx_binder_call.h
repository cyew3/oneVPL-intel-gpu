/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

template<class Tret>
class IBinderCall
{
public:
    virtual ~IBinderCall(){}
    virtual Tret Call() = 0;
};

//attaches and calls member functions with zero params- like mem_func_ptr_t, however has base class and canbe stored at container
//required friending of BinderCall_0
//template<class Tret, class TClass>  friend class BinderCall_0;
//declaring member of type
//BinderCall_0<ret_type, class_name> m_member;
//and assigning particular pointer to member at constructor : m_member(&class_name::fnc)

template <class TRet, class TClass>
class BinderCall_0 : public IBinderCall<TRet>
{
    TRet (TClass::*m_ptrFunc)();
    TClass *m_Class;

public:
    BinderCall_0(TRet (TClass::*ptrFunc)() = NULL, TClass *pPipeline = NULL)
        : m_ptrFunc(ptrFunc)
        , m_Class(pPipeline)
    {
    }
    virtual TRet Call()
    {
        if (NULL != m_Class)
            return (m_Class->*m_ptrFunc)();

        return TRet();
    }
};
