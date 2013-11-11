
/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include <list>
#include "test_object_params.h"
#include "mfx_types_traits.h"

#define TEST_METHOD_COMA() ,

#define TEST_METHOD_TYPE(method_name) method_name##_type
#define TEST_METHOD_NAME(method_name) _##method_name

#ifndef UNREFERENCED_PARAMETER
    #define UNREFERENCED_PARAMETER(par) par;
#endif

#define DECLARE_TEST_METHOD0(ret_type, method_name )\
    typedef ParameterHolder0<ret_type > TEST_METHOD_TYPE(method_name);\
    TestMethod0<ret_type> TEST_METHOD_NAME(method_name);\
    ret_type method_name()

#define DECLARE_TEST_METHOD1(ret_type, method_name, arg_type1)\
    typedef ParameterHolder1<ret_type, mfxTypeTrait<arg_type1>::store_type> TEST_METHOD_TYPE(method_name);\
    TestMethod1<ret_type, mfxTypeTrait<arg_type1>::store_type> TEST_METHOD_NAME(method_name);\
    ret_type method_name(mfxTypeTrait<arg_type1>::value_type _0)
    

#define DECLARE_TEST_METHOD2(ret_type, method_name, arg_type1, arg_type2)\
    typedef ParameterHolder2<ret_type, mfxTypeTrait<arg_type1>::store_type, mfxTypeTrait<arg_type2>::store_type> TEST_METHOD_TYPE(method_name);\
    TestMethod2<ret_type, mfxTypeTrait<arg_type1>::store_type, mfxTypeTrait<arg_type2>::store_type> TEST_METHOD_NAME(method_name);\
    ret_type method_name(mfxTypeTrait<arg_type1>::value_type _0, mfxTypeTrait<arg_type2>::value_type _1)
    

#define DECLARE_TEST_METHOD3(ret_type, method_name, arg_type1, arg_type2, arg_type3)\
    typedef ParameterHolder3<ret_type, mfxTypeTrait<arg_type1>::store_type, mfxTypeTrait<arg_type2>::store_type, mfxTypeTrait<arg_type3>::store_type> TEST_METHOD_TYPE(method_name);\
    TestMethod3<ret_type, mfxTypeTrait<arg_type1>::store_type, mfxTypeTrait<arg_type2>::store_type, mfxTypeTrait<arg_type3>::store_type> TEST_METHOD_NAME(method_name);\
    ret_type method_name(mfxTypeTrait<arg_type1>::value_type _0, mfxTypeTrait<arg_type2>::value_type _1, mfxTypeTrait<arg_type3>::value_type _2)
    

#define DECLARE_TEST_METHOD4(ret_type, method_name, arg_type1, arg_type2, arg_type3, arg_type4 )\
    typedef ParameterHolder4<ret_type, mfxTypeTrait<arg_type1>::store_type, mfxTypeTrait<arg_type2>::store_type, mfxTypeTrait<arg_type3>::store_type, mfxTypeTrait<arg_type4>::store_type> TEST_METHOD_TYPE(method_name);\
    TestMethod4<ret_type, mfxTypeTrait<arg_type1>::store_type, mfxTypeTrait<arg_type2>::store_type, mfxTypeTrait<arg_type3>::store_type, mfxTypeTrait<arg_type4>::store_type> TEST_METHOD_NAME(method_name);\
    ret_type method_name(mfxTypeTrait<arg_type1>::value_type _0, mfxTypeTrait<arg_type2>::value_type _1, mfxTypeTrait<arg_type3>::value_type _2, mfxTypeTrait<arg_type4>::value_type _3)

#define DECLARE_TEST_METHOD5(ret_type, method_name, arg_type1, arg_type2, arg_type3, arg_type4 , arg_type5 )\
    typedef ParameterHolder5<ret_type, mfxTypeTrait<arg_type1>::store_type, mfxTypeTrait<arg_type2>::store_type, mfxTypeTrait<arg_type3>::store_type, mfxTypeTrait<arg_type4>::store_type, mfxTypeTrait<arg_type5>::store_type> TEST_METHOD_TYPE(method_name);\
    TestMethod5<ret_type, mfxTypeTrait<arg_type1>::store_type, mfxTypeTrait<arg_type2>::store_type, mfxTypeTrait<arg_type3>::store_type, mfxTypeTrait<arg_type4>::store_type, mfxTypeTrait<arg_type5>::store_type> TEST_METHOD_NAME(method_name);\
    ret_type method_name(mfxTypeTrait<arg_type1>::value_type _0, mfxTypeTrait<arg_type2>::value_type _1, mfxTypeTrait<arg_type3>::value_type _2, mfxTypeTrait<arg_type4>::value_type _3, mfxTypeTrait<arg_type5>::value_type _4)

#define DECLARE_MOCK_METHOD2(ret_type, method_name, arg_type1, arg_type2) \
    DECLARE_TEST_METHOD2(ret_type, method_name, arg_type1, arg_type2){\
        TEST_METHOD_NAME(method_name).RegisterEvent(TEST_METHOD_TYPE(method_name)(ret_type(), _0, _1));\
        TEST_METHOD_TYPE(method_name) ret_params;\
        if (!TEST_METHOD_NAME(method_name).GetReturn(ret_params)){\
            return ret_type();\
        }\
        mfxTypeTrait<arg_type1>::assign()
        mfxAssign(_0, ret_params.value0);\
        mfxAssign(_1, ret_params.value1);\
        return ret_params.ret_val;\
    }

#define DECL_POINTER(type) 

class TestMethod
{
    //TODO: candidates to globalstorage
    std::list<BaseHolder*> m_params;
    std::list<BaseHolder*> m_called;

    BaseHolder* m_pDefaultReturn;

public:
    TestMethod() : m_pDefaultReturn () {
    }
    ~TestMethod() {
        delete m_pDefaultReturn;
    }
    virtual void WillReturn (const BaseHolder & params)
    {
        m_params.push_back(params.clone());
    }
    virtual void WillDefaultReturn (const BaseHolder * pDefaultParams)
    {
        delete m_pDefaultReturn;
        if (NULL != pDefaultParams)
        {
            m_pDefaultReturn = pDefaultParams->clone();
        }
        else
        {
            m_pDefaultReturn = NULL;
        }
    }
    //extracts params from storage
    virtual bool GetReturn (BaseHolder & params)
    {
        if (m_params.empty())
        {
            if (!m_pDefaultReturn)
            {
                return false;
            }
            params.assign(*m_pDefaultReturn);
            return true;
        }
        params.assign(*m_params.front());
        m_params.pop_front();
        return true;
    }
    //analog to willreturn however uses another list
    virtual bool WasCalled (BaseHolder * calledArgs = NULL)
    {
        if (m_called.empty())
        {
            return false;
        }
        if (calledArgs)
        {
            calledArgs->assign(*m_called.front());
        }
        
        m_called.pop_front();

        return true;
    }

    virtual void RegisterEvent(const BaseHolder & event_params)
    {
        m_called.push_back(event_params.clone());
    }
};

template <class TRet>
class TestMethod0 : public TestMethod
{
public:
    typedef ParameterHolder0<TRet> ArgsType;
    
};

template <class TRet, class T0>
class TestMethod1 : public TestMethod
{
public:
    typedef ParameterHolder1<TRet, T0> ArgsType;

};

template <class TRet, class T0, class T1>
class TestMethod2 : public TestMethod
{
public:
    typedef ParameterHolder2<TRet, T0, T1> ArgsType;

};

template <class TRet, class T0, class T1, class T2>
class TestMethod3 : public TestMethod
{
public:
    typedef ParameterHolder3<TRet, T0, T1, T2> ArgsType;

};

template <class TRet, class T0, class T1, class T2, class T3>
class TestMethod4 : public TestMethod
{
public:
    typedef ParameterHolder4<TRet, T0, T1, T2, T3> ArgsType;

};

template <class TRet, class T0, class T1, class T2, class T3, class T4>
class TestMethod5 : public TestMethod
{
public:
    typedef ParameterHolder5<TRet, T0, T1, T2, T3, T4> ArgsType;

};
