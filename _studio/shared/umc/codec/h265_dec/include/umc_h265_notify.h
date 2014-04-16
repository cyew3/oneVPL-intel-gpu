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

// Abstract notification class
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

// Destructor callback class
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

    // Call callback function
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

} // namespace UMC_HEVC_DECODER

#endif // __UMC_NOTIFY_H265
#endif // UMC_ENABLE_H265_VIDEO_DECODER
