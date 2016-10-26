//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2011 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_LOG_H)
#define __MFX_LOG_H

#include <stddef.h>

// Declare the type of handle to logging object
typedef
void * log_t;

// Initialize the logging stuff. If log path is not set,
// the default log path and object is used.
extern "C"
log_t mfxLogInit(const char *pLogPath = 0);

// Write something to the log. If the log handle is zero,
// the default log file is used.
extern "C"
void mfxLogWriteA(const log_t log, const char *pString, ...);

// Close the specific log. Default log can't be closed.
extern "C"
void mfxLogClose(log_t log);

class mfxLog
{
public:
    // Default constructor
    mfxLog(void)
    {
        log = 0;
    }

    // Destructor
    ~mfxLog(void)
    {
        Close();
    }

    // Initialize the log
    log_t Init(const char *pLogPath)
    {
        // Close the object before initialization
        Close();

        log = mfxLogInit(pLogPath);

        return log;
    }

    // Initialize the log
    void Close()
    {
        mfxLogClose(log);

        log = 0;
    }

    // Handle to the protected log
    log_t log;
};

#endif // __MFX_LOG_H
