/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_singleton.h"

class MFXNotifyMessage
    : public MFXNTSSingleton<MFXNotifyMessage>
{
    friend class MFXNTSSingleton<MFXNotifyMessage>;
    mfxU32 m_benchmarkMsgId;

    MFXNotifyMessage()
        : m_benchmarkMsgId()
    {
#if defined(WIN32) && !defined(WIN_TRESHOLD_MOBILE)
        m_benchmarkMsgId = RegisterWindowMessage(VM_STRING("MFX_Message"));
#endif
    }

public:
    void SendBroadcast(mfxU32 message_id, mfxU32 test_id)
    {
#if defined(WIN32) && !defined(WIN_TRESHOLD_MOBILE)
        SendNotifyMessage(HWND_BROADCAST, m_benchmarkMsgId, message_id, test_id);
#endif
    }
};

class AutoMessageNotifier
{
    mfxU32 m_enter;
    mfxU32 m_error;
    mfxU32 m_exit;
    mfxU32 m_entered;
    mfxU32 m_testId;

public:
    AutoMessageNotifier(mfxU32 enter, mfxU32 exit, mfxU32 error, mfxU32 testId)
        : m_enter(enter)
        , m_error(error)
        , m_exit(exit)
        , m_entered()
        , m_testId(testId)
    {
        if (m_testId != NOT_ASSIGNED_VALUE)
            Started();
    }
    void Started()
    {
        MFXNotifyMessage::Instance().SendBroadcast(m_enter, m_testId);
        m_entered++;
    }
    void Abort()
    {
        MFXNotifyMessage::Instance().SendBroadcast(m_error, m_testId);
    }
    void Complete()
    {
        if (m_entered > 0)
        {
            MFXNotifyMessage::Instance().SendBroadcast(m_exit, m_testId);
            m_entered--;
        }
    }
    ~AutoMessageNotifier()
    {
        if (m_entered > 0 && m_testId != NOT_ASSIGNED_VALUE)
        {
            Abort();
        }
    }
};