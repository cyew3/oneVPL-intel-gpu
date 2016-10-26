//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_critical_error_handler.h"
#include "vm_debug.h"

MfxCriticalErrorHandler::MfxCriticalErrorHandler(void):
    m_CriticalErrorReportedAtLeastOnce(false),
    m_CriticalErrorStatus(MFX_ERR_NONE)
{
}

mfxStatus MfxCriticalErrorHandler::ReturningCriticalStatus()
{
    VM_ASSERT(m_CriticalErrorStatus != MFX_ERR_NONE);
    m_CriticalErrorReportedAtLeastOnce = true;
    if (m_CriticalErrorStatus == MFX_ERR_NONE)
        m_CriticalErrorStatus = MFX_ERR_UNKNOWN;

    return m_CriticalErrorStatus;
}

bool MfxCriticalErrorHandler::NeedToReturnCriticalStatus(mfxBitstream *bs)
{
    bool RetrievingCachedFrames_and_CriticalErrorReportedAtLeastOnce = m_CriticalErrorReportedAtLeastOnce && bs == NULL;
    return CriticalErrorOccured() && !RetrievingCachedFrames_and_CriticalErrorReportedAtLeastOnce;
}

void MfxCriticalErrorHandler::SetCriticalErrorOccured(mfxStatus errorStatus)
{
    VM_ASSERT(errorStatus != MFX_ERR_NONE);
    m_CriticalErrorStatus = errorStatus;
}

bool MfxCriticalErrorHandler::CriticalErrorOccured()
{
    return m_CriticalErrorStatus != MFX_ERR_NONE;
}