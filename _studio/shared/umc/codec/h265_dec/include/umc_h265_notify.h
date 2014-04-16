/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_NOTIFY_H265
#define __UMC_NOTIFY_H265

#include <list>

namespace UMC_HEVC_DECODER
{

class notifier_base
{
public:
    notifier_base()
        : next_(0)
        , m_isNeedNotification(true)
    {
    }

    virtual ~notifier_base()
    {}

    virtual void Notify() = 0;

    void ClearNotification() { m_isNeedNotification = false; }

    notifier_base * next_;

protected:
    bool m_isNeedNotification;
};

template <typename Object>
class notifier0 : public notifier_base
{
public:
    typedef void (Object::*Function)();

    notifier0(Object* object, Function function)
        : object_(object)
        , function_(function)
    {
    }

    ~notifier0()
    {
        Notify();
    }

    virtual void Notify()
    {
        if (m_isNeedNotification)
        {
            m_isNeedNotification = false;
            (object_->*function_)();
        }
    }

private:
    Object* object_;
    Function function_;
};

template <typename Object, typename Param1>
class notifierH : public notifier_base
{
public:
    notifierH(Object* object, Param1 param1)
        : object_(object)
        , param1_(param1)
    {
    }

    ~notifierH()
    {
        Notify();
    }

    virtual void Notify()
    {
        if (m_isNeedNotification)
        {
            m_isNeedNotification = false;
            (object_->Signal)(param1_);
        }
    }

private:
    Object* object_;
    Param1 param1_;
};

template <typename Object, typename Param1>
class notifier1 : public notifier_base
{
public:
    typedef void (Object::*Function)(Param1 param1);

    notifier1(Object* object, Function function, Param1 param1)
        : object_(object)
        , function_(function)
        , param1_(param1)
    {
    }

    ~notifier1()
    {
        Notify();
    }

    virtual void Notify()
    {
        if (m_isNeedNotification)
        {
            m_isNeedNotification = false;
            (object_->*function_)(param1_);
        }
    }

private:
    Object* object_;
    Function function_;
    Param1 param1_;
};

template <typename Object, typename Param1, typename Param2>
class notifier2 : public notifier_base
{
public:
    typedef void (Object::*Function)(Param1 param1, Param2 param2);

    notifier2(Object* object, Function function, Param1 param1, Param2 param2)
        : object_(object)
        , function_(function)
        , param1_(param1)
        , param2_(param2)
    {
    }

    ~notifier2()
    {
        Notify();
    }

    void SetParam1(Param1 param1)
    {
        param1_ = param1;
    }

    void SetParam2(Param2 param2)
    {
        param2_ = param2;
    }

    virtual void Notify()
    {
        if (m_isNeedNotification)
        {
            m_isNeedNotification = false;
            (object_->*function_)(param1_, param2_);
        }
    }

private:
    Object* object_;
    Function function_;
    Param1 param1_;
    Param2 param2_;
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_NOTIFY_H265
#endif // UMC_ENABLE_H265_VIDEO_DECODER
