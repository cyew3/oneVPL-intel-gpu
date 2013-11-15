
/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfxdefs.h"
#include "mfx_bitstream2.h"

//store parameter type same as input parameter type, works great for parametes passed by value
template<class TValue>
struct mfxTypeTrait
{
    typedef TValue value_type;
    typedef TValue store_type;
    static store_type assign(value_type from){
        return from;
    }
};

//void return stored as int to avoid compiler errors
template<>
struct mfxTypeTrait<void>
{
    typedef void value_type;
    typedef int store_type;
    static store_type assign() {
        return 1;
    }
};

//reference parameters are stored by pointer
template<class TValue>
struct  mfxTypeTrait<TValue &> 
{
    typedef typename TValue & value_type;
    typedef typename TValue* store_type;
    static store_type assign(value_type from){
        return &from;
    }

};

//const reference parameters are obviously stored by value
template<class TValue>
struct mfxTypeTrait<const TValue &> 
{
    typedef typename const TValue & value_type;
    typedef typename TValue store_type;
    static store_type assign(value_type from){
        return from;
    }
};

//special class to store one type but function will be called with different type, and custom conversion will be applied
template <class TStore, class TValue, class TConverter>
struct mfxCustomTrait
{
public:
    typedef typename TValue value_type;
    typedef typename TStore store_type;
    typedef typename TConverter converter_type;
};


template<class TStore, class TAccept, class TConverter>
struct mfxTypeTrait<mfxCustomTrait<TStore, TAccept, TConverter> >
{
    typedef mfxCustomTrait<TStore, TAccept, TConverter> cust;
    typedef typename cust::value_type value_type;
    typedef typename cust::store_type store_type;
    static store_type assign(value_type from){
        return cust::converter_type()(from);
    }
};

//and several converters
template <class TTo, class TFrom>
struct mfxStaticCastConverter
    : public std::unary_function<TTo, TFrom>
{
    TTo operator () (TFrom from) const {
        return static_cast<TTo>(from);
    }
};

template <class TTo, class TFrom>
struct mfxDynamicCastConverter
    : public std::unary_function<TTo, TFrom>
{
    TTo operator () (TFrom from) const {
        return dynamic_cast<TTo>(from);
    }
};

//makes template params and use sub macro for COMAs
#define MAKE_STATIC_TRAIT(store_type, value_type)\
mfxCustomTrait<store_type, value_type , mfxStaticCastConverter<store_type , value_type> >

//makes template params and use sub macro for COMAs
#define MAKE_DYNAMIC_TRAIT(store_type, value_type)\
mfxCustomTrait<store_type, value_type, mfxDynamicCastConverter<store_type, value_type> >

//makes template params and use sub macro for COMAs
#define MAKE_ANY_TRAIT(store_type, value_type, converter_type)\
    mfxCustomTrait<store_type, value_type, converter_type<store_type, value_type> >

//makes template params and use sub macro for COMAs
#define MAKE_REF_BY_VALUE_TRAIT(value_type)\
    mfxCustomTrait<value_type, value_type &, mfxStaticCastConverter<value_type, value_type &> >

template <class A, class B>
struct mfxAssign_t {
    void operator()( A & a, B b) const{
        a = b;
    }
};

template <class A, class B>
struct mfxAssign_t<A*, B*> {
    void operator ()( A *& a, B* b) const{
        *a = *b;
    }
};

template <class A, class B>
struct mfxAssign_t<A*, B> {
    void operator ()( A *& a, B b) const{
        *a = b;
    }
};

template <class A, class B>
void mfxAssign(A a, B b) {
    mfxAssign_t<A,B>().operator()(a,b);
}
