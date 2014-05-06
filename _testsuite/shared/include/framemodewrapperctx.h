/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2008-2011 Intel Corporation. All Rights Reserved.
//
//
*/

/**
* @file   FrameModeWrapperCtx.h
* 
* @brief  CFrameModeWrapper external API for integrated applications (DLLs)
* 
* 
*/

#ifndef FRAME_MODE_WRAPPER_CTX_H
#define FRAME_MODE_WRAPPER_CTX_H

#ifdef __cplusplus
namespace lucas {
extern "C" {
#endif

  typedef int FFrameModeRead(void *fmWrapperPtr, char* buffer, int max_len);
  typedef int FFrameModeSeek(void *fmWrapperPtr, long long offset);
  typedef char *FFrameModeOutput(void *fmWrapperPtr, int id, char* buffer, int bufferLen, int newBufferRequest);
  typedef void FFrameModeParameter(void *fmWrapperPtr, int paramID, int paramValue);
  typedef void FFrameModeParameterPtr(void *fmWrapperPtr, int paramID, int *paramValue);
  typedef long long FFrameModeStreamOffset(void *fmWrapperPtr);
  typedef int FFrameModeStreamEnd(void *fmWrapperPtr);
  typedef void FFrameModeLog(void *fmWrapperPtr, int logLevel, char *msg);
  typedef void FFrameModeExit(void *fmWrapperPtr, int exitCode);

enum TFMWrapperParamID
{
  FM_WRAPPER_PARAM_RESOLUTION_X,
  FM_WRAPPER_PARAM_RESOLUTION_Y,
  FM_WRAPPER_PARAM_FRAME_RATE
};

enum TFMWrapperLogLevel
{
  FM_WRAPPER_LOG_DEBUG = 0,
  FM_WRAPPER_LOG_INFO = 1,
  FM_WRAPPER_LOG_DEFAULT = 2,
  FM_WRAPPER_LOG_WARNING = 3,
  FM_WRAPPER_LOG_ERROR = 4,
  FM_WRAPPER_LOG_ALWAYS = 5,
  FM_WRAPPER_LOG_CONT = 6
};

// Context passed to integrated application (DLL)
// It contains pointers to callback functions for input operations (virtual file):
//    read(void *fmWrapperPtr, char* buffer, int max_len)
//    seek(void *fmWrapperPtr, long long offset)
// callback function for output exporting and output buffers management:
//    output(void *fmWrapperPtr, int id, char* buffer, int len, int newBufferRequest)
// callback function for additional data/parameters passing from DLL to Lucas block:
//    parameter(void *fmWrapperPtr, int paramID, int paramValue)
typedef struct FMWrapperCtx
{
  int ctxLen;
  int logLevel;
  void *fmWrapperPtr;
  FFrameModeRead *read;
  FFrameModeSeek *seek;
  FFrameModeOutput *output;
  FFrameModeParameter *parameter;
  FFrameModeParameterPtr *parameterPtr;
  FFrameModeStreamOffset *streamOffset;
  FFrameModeStreamEnd *streamEnd;
  FFrameModeLog *log;
  FFrameModeExit *exit;
} TFMWrapperCtx;

#ifdef __cplusplus
} // extern "C"
} // namespace lucas
#endif

#endif  /* FRAME_MODE_WRAPPER_CTX_H */
