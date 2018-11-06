/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2013 Intel Corporation. All Rights Reserved.

File Name: msdka_analyzer_functions_list.h

\* ****************************************************************************** */

#include "mfx_exposed_functions_list.h"

#undef FUNCTION_VERSION_MAJOR
#undef FUNCTION_VERSION_MINOR
#define FUNCTION_VERSION_MAJOR 1
#define FUNCTION_VERSION_MINOR 2

FUNCTION(mfxStatus, MFXJoinSession, (mfxSession session, mfxSession child),(session,child))
FUNCTION(mfxStatus, MFXCloneSession, (mfxSession session, mfxSession *clone), (session, clone))
FUNCTION(mfxStatus, MFXQueryIMPL, (mfxSession session, mfxIMPL* impl),(session, impl))
FUNCTION(mfxStatus, MFXQueryVersion, (mfxSession session, mfxVersion *version), (session, version))
FUNCTION(mfxStatus, MFXDisjoinSession, (mfxSession session), (session))
FUNCTION(mfxStatus, MFXSetPriority, (mfxSession session, mfxPriority priority), (session, priority))
FUNCTION(mfxStatus, MFXGetPriority, (mfxSession session, mfxPriority *priority), (session, priority))
#if 0
FUNCTION(mfxStatus, MFXGetLogMessage, (mfxSession session, char *msg, mfxU32 size), (session, msg, size))
#endif