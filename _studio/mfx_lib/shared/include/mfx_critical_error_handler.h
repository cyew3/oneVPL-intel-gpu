/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

File Name: mfx_critical_error_handler.h

\* ****************************************************************************** */

#pragma once

#include "mfx_common.h"

class MfxCriticalErrorHandler
{
public:
    MfxCriticalErrorHandler(void);

protected:
    mfxStatus ReturningCriticalStatus();
    bool      NeedToReturnCriticalStatus(mfxBitstream *bs);

    void      SetCriticalErrorOccured(mfxStatus errorStatus);
    bool      CriticalErrorOccured();

private:
    bool      m_CriticalErrorReportedAtLeastOnce;
    mfxStatus m_CriticalErrorStatus;
};