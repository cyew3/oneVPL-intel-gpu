/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __TRACE_UTILS_H__
#define __TRACE_UTILS_H__

class AutoTrace
{
public:
    AutoTrace(const char *name, int level = 0);
    ~AutoTrace();
protected:
    int m_level;
};

#if defined(LINUX32) || defined(LINUX64)
//#define MFX_TRACE_MPA_VTUNE
#endif

#if defined(_WIN32) || defined(_WIN64)
#define MPA_TRACE(TASK_NAME)  AutoTrace trace(TASK_NAME);
#define MPA_TRACE_FUNCTION()  MPA_TRACE(__FUNCTION__)
#elif defined (MFX_TRACE_MPA_VTUNE)
#define MPA_TRACE(TASK_NAME)  AutoTrace trace(TASK_NAME);
#define MPA_TRACE_FUNCTION()  MPA_TRACE(__FUNCTION__)
#else
#define MPA_TRACE(TASK_NAME)
#define MPA_TRACE_FUNCTION()
#endif

#endif // __TRACE_UTILS_H__
