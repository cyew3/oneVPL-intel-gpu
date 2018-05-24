/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2018 Intel Corporation. All Rights Reserved.
//
*/

#pragma once


#ifndef TRUE
    #define TRUE  1
    #define FALSE 0 
#endif

///////////////////////////////////////////////////////
//configurable LOG defines

#define LOG_VIDEO_PROCESSOR_SERVICE TRUE
#define LOG_VIDEO_PROCESSOR         TRUE
#define LOG_VIDEO_DECODER_SERVICE   TRUE
#define LOG_VIDEO_DECODER           TRUE
#define LOG_D3D_DEVICE_MANAGER      TRUE
#define LOG_CREATE_FUNCTIONS        TRUE

///////////////////////////////////////////////////////
extern char pDir[256];
extern bool f_init;
extern FILE *f;


#define LLL \
{ \
    char fname[256]; \
    CreateDirectoryA(pDir, NULL);\
    strcpy(fname, pDir); \
    strcat(fname, "\\x_spy.txt"); \
    if (!f) f = fopen(fname, (f_init) ? "a" : "w"); \
    f_init = true; \
    if (f){\
    fprintf(f, "%s: %d: %s\n", __FILE__, __LINE__, __FUNCTION__); fflush(f);}\
}

#define OPEN_SPEC_FILE \
{ \
    char fname[1024]; \
    char fname2[1024]; \
    if (f) fclose(f); \
    strcpy(fname, __FUNCTION__); \
    char *p = strrchr(fname, ':'); \
    if (p) p++; else p = fname; \
    strcpy(fname2, pDir); \
    strcat(fname2, "\\"); \
    strcat(fname2, p); \
    strcat(fname2, ".log"); \
    f = fopen(fname2, "w"); \
}

#define CLOSE_SPEC_FILE \
{ \
    fclose(f); \
    f = NULL; \
}

#define logi(x) fprintf(f, #x " = %d\n", (int)x);
#define logui(x) fprintf(f, #x " = %u\n", x);
#define logp(x) fprintf(f, #x " = %p\n", (void*)x);
#define logi_flush(x) fprintf(f, #x " = %d\n", (int)x); fflush(f)
#define flush   fflush(f)

#define logs(x) fprintf(f, "%s\n", x); fflush(f)
#define logcc(x) \
    fprintf(f, #x " = %c%c%c%c\n", \
    ((char*)&(x))[0],  \
    ((char*)&(x))[1],  \
    ((char*)&(x))[2],  \
    ((char*)&(x))[3])

#define LOG_GUID(x) \
    /*LOG_KNOWN_GUID(x, __uuidof(IMFGetService)) \
LOG_KNOWN_GUID(x, IID_IMixerPinConfig) \
LOG_KNOWN_GUID(x, __uuidof(IDirectXVideoMemoryConfiguration)) \
LOG_KNOWN_GUID(x, __uuidof(IDirect3DDeviceManager9)) \
LOG_KNOWN_GUID(x, __uuidof(IDirectXVideoDecoderService)) \
LOG_KNOWN_GUID(x, __uuidof(IDirectXVideoProcessorService)) \
LOG_KNOWN_GUID(x, __uuidof(IDirectXVideoDecoder)) \
LOG_KNOWN_GUID(x, __uuidof(IDirectXVideoProcessor)) \
LOG_KNOWN_GUID(x, __uuidof(IKsPropertySet)) \
LOG_KNOWN_GUID(x, __uuidof(IOverlay)) \
LOG_KNOWN_GUID(x, __uuidof(IStreamBufferSink)) \
LOG_KNOWN_GUID(x, IID_IMFVideoProcessor)*/ \
{ \
    fprintf(f, #x " = %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x, ", \
    x.Data1, x.Data2, x.Data3, \
    x.Data4[0], x.Data4[1], x.Data4[2], x.Data4[3], \
    x.Data4[4], x.Data4[5], x.Data4[6], x.Data4[7]); \
    logcc(x.Data1); \
}

#define BUFFER_SIZE (8*1024*1024)

#if LOG_VIDEO_PROCESSOR_SERVICE
#define LLLVPROCSRV LLL
#else
#define LLLVPROCSRV 
#endif

#if LOG_VIDEO_PROCESSOR
#define LLLVPROC LLL
#else
#define LLLVPROC 
#endif

#if LOG_VIDEO_DECODER_SERVICE
#define LLLVDECSRV LLL
#else
#define LLLVDECSRV 
#endif

#if LOG_VIDEO_DECODER
#define LLLVDEC LLL
#else
#define LLLVDEC 
#endif

#if LOG_D3D_DEVICE_MANAGER
#define LLLD3DDEV LLL
#else
#define LLLD3DDEV 
#endif

#if LOG_CREATE_FUNCTIONS
#define LLLCREATE LLL
#else
#define LLLCREATE 
#endif