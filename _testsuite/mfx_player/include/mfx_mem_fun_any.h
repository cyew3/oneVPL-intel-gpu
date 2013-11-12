/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: 

*******************************************************************************/

#pragma  once

#include <functional>

template <class S, class T, class A, class B>
class mem_fun2_t : public std::unary_function<T*, S>
{
public:
    typedef typename std::unary_function<T*, S>::argument_type argument_type;
    explicit mem_fun2_t(S (T::*p)(A, B), A x, B y)
        : ptr(p)
        , default_a(x)
        , default_b(y)
    {}
    S operator()(T* p) const
    {
        return (p->*ptr)(default_a, default_b);
    }
private:
    S (T::*ptr)(A, B);
    A default_a;
    B default_b;
};

template <class S, class T, class A, class B>
mem_fun2_t <S, T, A, B> 
    mem_fun2 (S (T::*p)(A, B), A x, B y)
{
    return mem_fun2_t<S, T, A, B>(p, x, y);
}

//boost functional were taken as a initial
template <class S, class T, class A, class B>
class mem_fun2_defA : public std::binary_function<T*, B, S>
{
public:
    explicit mem_fun2_defA(S (T::*p)(A, B), A x )
        : ptr(p)
        , default_a(x)
    {}
    S operator()(T* p, B y) const
    {
        return (p->*ptr)(default_a, y);
    }
private:
    S (T::*ptr)(A, B);
    A default_a;
};

template <class S, class T, class A, class B>
class mem_fun2_defB : public std::binary_function<T*, A, S>
{
public:
    explicit mem_fun2_defB(S (T::*p)(A, B), B y )
        : ptr(p)
        , default_b(y)
    {}
    S operator()(T* p, A x) const
    {
        return (p->*ptr)(x, default_b);
    }
private:
    S (T::*ptr)(A, B);
    B default_b;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
template <class S, class T>
class mem_fun_void_t 
{
public:
    typedef typename S result_type;
    explicit mem_fun_void_t(void (T::*p)())
        : ptr(p)
    {}
    S operator()(T* p) const
    {
        (p->*ptr)();
        return S();
    }
private:
    void (T::*ptr)();
};


template <class Result, class ClassType> inline
mem_fun_void_t <Result, ClassType> 
    mem_fun_any (void (ClassType::*p)())
{
    return mem_fun_void_t<Result, ClassType>(p);
}


//////////////////////////////////////////////////////////////////////////
template <class S, class T, class A, class B>
class mem_fun2_right_t : public std::unary_function<T*, S>
{
public:
    //typedef typename std::unary_function<T*, S>::argument_type argument_type;
    explicit mem_fun2_right_t(S (T::*p)(A, B))
        : ptr(p)
    {}
    S operator()(T* p, A a, B b) const
    {
        return (p->*ptr)(a, b);
    }
private:
    S (T::*ptr)(A, B);
};


template <class Result, class ClassType, class Arg1, class Arg2>
mem_fun2_right_t <Result, ClassType, Arg1, Arg2> 
    mem_fun_any (Result (ClassType::*p)(Arg1, Arg2))
{
    return mem_fun2_right_t<Result, ClassType, Arg1, Arg2>(p);
}


//////////////////////////////////////////////////////////////////////////

template <class Result, class ClassType, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7>
class mem_fun7_t {
    Result (ClassType::*ptr)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7);
public:
    typedef typename Result result_type;
    mem_fun7_t(Result (ClassType::*p)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7))
        : ptr(p) {
    }
    Result operator ()(ClassType* p, Arg1 x1, Arg2 x2, Arg3 x3, Arg4 x4, Arg5 x5, Arg6 x6, Arg7 x7) {
        return (p->*ptr)(x1,x2,x3,x4,x5,x6,x7);
    }
};

template <class Result, class ClassType, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7>
mem_fun7_t <Result, ClassType, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7> 
    mem_fun_any (Result (ClassType::*p)(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7)) 
{
    return mem_fun7_t<Result, ClassType, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7>(p);
}

//////////////////////////////////////////////////////////////////////////
template <class _Fn1, class _Ty>
class Binder1_t {
    _Fn1 mFn1;
    _Ty mLeft;
public:
    typedef typename _Fn1::result_type result_type;

    Binder1_t(const _Fn1 &fn, _Ty left) 
        : mFn1(fn)
        , mLeft(left) {
    }

    result_type operator ()() const {
        return mFn1(mLeft);
    }
};

template<class _Fn1, class _Ty> inline
    Binder1_t<_Fn1, _Ty> bind_any(const _Fn1& _Func, _Ty _Left)
{   
    return (Binder1_t<_Fn1, _Ty>(_Func, _Left));
}

//////////////////////////////////////////////////////////////////////////
template <class _Fn1, class _Ty1, class _Ty2>
class Binder2_t {
    _Fn1 mFn1;
    _Ty1 mLeft;
    _Ty2 mRight;
public:

    Binder2_t(const _Fn1 &fn, _Ty1 left, _Ty2 right) 
        : mFn1(fn)
        , mLeft(left)
        , mRight(right){
    }

    typename _Fn1::result_type operator ()() const {
        return mFn1(mLeft, mRight);
    }
};

template<class _Fn1, class _Ty1, class _Ty2> inline
    Binder2_t<_Fn1, _Ty1, _Ty2> bind_any(const _Fn1& _Func, _Ty1 _Left, _Ty2 _Right)
{   
    return Binder2_t<_Fn1, _Ty1, _Ty2>(_Func, _Left, _Right);
}

//////////////////////////////////////////////////////////////////////////
//transform 3th arguments functor to a functor with no arguments
template <class Fnc, class Arg1, class Arg2, class Arg3>
class Binder3_t {
    Fnc fn;
    Arg1 x1;
    Arg2 x2;
    Arg3 x3;
public:
    Binder3_t(const Fnc &fn, Arg1 x1, Arg2 x2, Arg3 x3)
        : fn(fn)
        , x1(x1)
        , x2(x2)
        , x3(x3) {
    }
    typename Fnc::result_type operator () ()  {
        return fn(x1,x2,x3);
    }
};

//transform any arguments functor to a functor with no arguments
template <class Fnc, class Arg1, class Arg2, class Arg3> inline
    Binder3_t <Fnc, Arg1, Arg2, Arg3> 
    bind_any (const Fnc &fn, Arg1 x1, Arg2 x2, Arg3 x3)
{
    return Binder3_t<Fnc, Arg1, Arg2, Arg3>(fn, x1, x2, x3);
}


//////////////////////////////////////////////////////////////////////////
//transform 8 arguments functor to a functor with no arguments
template <class Fnc, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8>
class Binder8_t {
    Fnc fn;
    Arg1 x1;
    Arg2 x2;
    Arg3 x3;
    Arg4 x4;
    Arg5 x5;
    Arg6 x6;
    Arg7 x7;
    Arg8 x8;
public:
    Binder8_t(const Fnc &fn, Arg1 x1, Arg2 x2, Arg3 x3, Arg4 x4, Arg5 x5, Arg6 x6, Arg7 x7, Arg8 x8)
        : fn(fn)
        , x1(x1)
        , x2(x2)
        , x3(x3)
        , x4(x4)
        , x5(x5)
        , x6(x6)
        , x7(x7)
        , x8(x8){
    }
    typename Fnc::result_type operator () () {
        return fn(x1,x2,x3,x4,x5,x6,x7,x8);
    }
};

//transform any arguments functor to a functor with no arguments
template <class Fnc, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8> inline
Binder8_t <Fnc, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8> 
bind_any (const Fnc &fn, Arg1 x1, Arg2 x2, Arg3 x3, Arg4 x4, Arg5 x5, Arg6 x6, Arg7 x7, Arg8 x8)
{
    return Binder8_t<Fnc, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8>(fn, x1, x2, x3, x4, x5, x6, x7, x8);
}
