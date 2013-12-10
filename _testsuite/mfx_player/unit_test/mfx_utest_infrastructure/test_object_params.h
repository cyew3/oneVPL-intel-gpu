/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once
#include "mfx_types_traits.h"

class BaseHolder
{
public:
    virtual ~BaseHolder() {}
    virtual  void assign(BaseHolder & ){}
    virtual  BaseHolder* clone() const {return new BaseHolder();}
};

template <class TReturn>
class ParameterHolder0
    : public BaseHolder
{
public:
    typedef typename mfxTypeTrait<TReturn>::store_type return_type;
    return_type ret_val;

    ParameterHolder0(return_type _ret_val = return_type())
        : ret_val(_ret_val){}

    virtual  void assign(BaseHolder & params)
    {
        ret_val = dynamic_cast<ParameterHolder0*>(&params)->ret_val;
    }
    virtual  ParameterHolder0* clone() const
    {
        ParameterHolder0* pClone = new ParameterHolder0();
        pClone->ret_val = ret_val;
        return pClone;
    }
};

template <class TReturn, class T0>
class ParameterHolder1
    : public ParameterHolder0<TReturn>
{
public:
    typedef typename mfxTypeTrait<T0>::store_type store_type0;
    store_type0 value0;

    ParameterHolder1(return_type _ret_val = return_type(), store_type0 _value0 = store_type0())
        : ParameterHolder0(_ret_val)
        , value0(_value0)
    {}


    virtual  void assign(BaseHolder & params)
    {
        ParameterHolder1* pIn = dynamic_cast<ParameterHolder1*>(&params);
        ret_val = pIn->ret_val;
        value0 = pIn->value0;
    }
    virtual  ParameterHolder1* clone() const
    {
        ParameterHolder1* pClone = new ParameterHolder1();
        pClone->ret_val = ret_val;
        pClone->value0 = value0;
        return pClone;
    }
};

template <class TReturn, class T0, class T1>
class ParameterHolder2
    : public ParameterHolder1<TReturn, T0>
{
public:
    typedef typename mfxTypeTrait<T1>::store_type store_type1;
    store_type1 value1;

    ParameterHolder2(return_type _ret_val = return_type(), store_type0 _value0 = store_type0(), store_type1 _value1 = store_type1())
        : ParameterHolder1(_ret_val, _value0)
        , value1(_value1)

    {}

    virtual  void assign(BaseHolder & params)
    {
        ParameterHolder2* pIn = dynamic_cast<ParameterHolder2*>(&params);
        ret_val = pIn->ret_val;
        value0  = pIn->value0; 
        value1  = pIn->value1; 
    }
    virtual  ParameterHolder2* clone() const
    {
        ParameterHolder2* pClone = new ParameterHolder2();
        pClone->ret_val = ret_val;
        pClone->value0 = value0;
        pClone->value1 = value1;
        return pClone;
    }
};


template <class TReturn, class T0, class T1, class T2>
class ParameterHolder3
    : public ParameterHolder2<TReturn, T0, T1>
{
public:
    typedef typename mfxTypeTrait<T2>::store_type store_type2;
    store_type2 value2;

    ParameterHolder3(return_type _ret_val = return_type(), store_type0 _value0 = store_type0(), store_type1 _value1 = store_type1()
        , store_type2 _value2 = store_type2())
        : ParameterHolder2(_ret_val, _value0, _value1)
        , value2(_value2)

    {}

    virtual  void assign(BaseHolder & params)
    {
        ParameterHolder3* pIn = dynamic_cast<ParameterHolder3*>(&params);
        ret_val = pIn->ret_val;
        value0 = pIn->value0;  
        value1 = pIn->value1;
        value2 = pIn->value2;
    }
    virtual  ParameterHolder3* clone() const
    {
        ParameterHolder3* pClone = new ParameterHolder3();
        pClone->ret_val = ret_val;
        pClone->value0 = value0;
        pClone->value1 = value1;
        pClone->value2 = value2;
        return pClone;
    }

};


template <class TReturn, class T0, class T1, class T2, class T3>
class ParameterHolder4
    : public ParameterHolder3<TReturn, T0, T1, T2>
{
public:
    typedef typename mfxTypeTrait<T3>::store_type store_type3;
    store_type3 value3;

    ParameterHolder4(return_type _ret_val = return_type()
        , store_type0 _value0 = store_type0()
        , store_type1 _value1 = store_type1()
        , store_type2 _value2 = store_type2()
        , store_type3 _value3 = store_type3())
        : ParameterHolder3(_ret_val, _value0, _value1, _value2)
        , value3(_value3)

    {}


    virtual  void assign(BaseHolder & params)
    {
        ParameterHolder4* pIn = dynamic_cast<ParameterHolder4*>(&params);
        ret_val = pIn->ret_val;
        value0 = pIn->value0;
        value1 = pIn->value1;
        value2 = pIn->value2;
        value3 = pIn->value3;
    }
    virtual  ParameterHolder4* clone() const
    {
        ParameterHolder4* pClone = new ParameterHolder4();
        pClone->ret_val = ret_val;
        pClone->value0 = value0;
        pClone->value1 = value1;
        pClone->value2 = value2;
        pClone->value3 = value3;
        return pClone;
    }
};


template <class TReturn, class T0, class T1, class T2, class T3, class T4>
class ParameterHolder5
    : public ParameterHolder4<TReturn, T0, T1, T2, T3>
{
public:
    typedef typename mfxTypeTrait<T4>::store_type store_type4;
    T4 value4;

    ParameterHolder5(return_type _ret_val = return_type()
        , store_type0 _value0 = store_type0()
        , store_type1 _value1 = store_type1()
        , store_type2 _value2 = store_type2()
        , store_type3 _value3 = store_type3()
        , store_type4 _value4 = store_type4())
        : ParameterHolder4(_ret_val, _value0, _value1, _value2, _value3)
        , value4(_value4)

    {}

    virtual  void assign(BaseHolder & params)
    {
        ParameterHolder5* pIn = dynamic_cast<ParameterHolder5*>(&params);
        ret_val = pIn->ret_val ;
        value0  = pIn->value0  ;
        value1  = pIn->value1  ;
        value2  = pIn->value2  ;
        value3  = pIn->value3  ;
        value4  = pIn->value4  ;
    }
    virtual  ParameterHolder5* clone() const
    {
        ParameterHolder5* pClone = new ParameterHolder5();
        pClone->ret_val = ret_val;
        pClone->value0 = value0;
        pClone->value1 = value1;
        pClone->value2 = value2;
        pClone->value3 = value3;
        pClone->value4 = value4;
        return pClone;
    }

};

template <class TReturn, class T0, class T1, class T2, class T3, class T4>
ParameterHolder5<TReturn, T0, T1, T2, T3, T4> Hold(TReturn _ret_val, T0 _value0, T1 _value1, T2 _value2, T3 _value3, T4 _value4)
{
    return ParameterHolder5<TReturn, T0, T1, T2, T3, T4>(_ret_val, _value0, _value1, _value2, _value3, _value4);
}

template <class TReturn, class T0, class T1, class T2, class T3>
ParameterHolder4<TReturn, T0, T1, T2, T3> Hold(TReturn _ret_val, T0 _value0, T1 _value1, T2 _value2, T3 _value3)
{
    return ParameterHolder4<TReturn, T0, T1, T2, T3>(_ret_val, _value0, _value1, _value2, _value3);
}

template <class TReturn, class T0, class T1, class T2>
ParameterHolder3<TReturn, T0, T1, T2> Hold(TReturn _ret_val, T0 _value0, T1 _value1, T2 _value2)
{
    return ParameterHolder3<TReturn, T0, T1, T2>(_ret_val, _value0, _value1, _value2);
}

template <class TReturn, class T0, class T1>
ParameterHolder2<TReturn, T0, T1> Hold(TReturn _ret_val, T0 _value0, T1 _value1)
{
    return ParameterHolder2<TReturn, T0, T1>(_ret_val, _value0, _value1);
}

template <class TReturn, class T0>
ParameterHolder1<TReturn, T0> Hold(TReturn _ret_val, T0 _value0)
{
    return ParameterHolder1<TReturn, T0>(_ret_val, _value0);
}

template <class TReturn>
ParameterHolder0<TReturn> Hold(TReturn _ret_val)
{
    return ParameterHolder0<TReturn>(_ret_val);
}
