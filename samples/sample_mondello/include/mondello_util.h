/* ****************************************************************************** *\
 *
 * INTEL CORPORATION PROPRIETARY INFORMATION
 * This software is supplied under the terms of a license agreement or nondisclosure
 * agreement with Intel Corporation and may not be copied or disclosed except in
 * accordance with the terms of that agreement
 * Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.
 *
 * \* ****************************************************************************** */

#ifndef __MONDELLO_UTIL_H__
#define __MONDELLO_UTIL_H__

#include <pthread.h>
#include <stdlib.h>
#include <va/va.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "sample_defs.h"
#include "libcamhal/api/ICamera.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define ERRSTR strerror(errno)
#define DEBUG 0 // for debugging purposes

#define FPS_TIME_INTERVAL 2
#define FPS_BUF_COUNT_START 10

#define BYE_ON(cond, ...) \
    do { \
    if (cond) { \
        int errsv = errno; \
        fprintf(stderr, "ERROR(%s:%d) : ", \
            __FILE__, __LINE__); \
            errno = errsv; \
        fprintf(stderr,  __VA_ARGS__); \
        abort(); \
    } \
    } while(0)

#define DEBUG_PRINT(...) \
    do { \
    if (DEBUG) { \
        fprintf(stderr,  __VA_ARGS__); \
    } \
    } while(0)

using namespace icamera;

enum MondelloPixelFormat
{
    NO_FORMAT = 0, YUY2, UYVY, RGB4
};

enum MondelloPicStructType
{
    PROGRESSIVE = 0, IPU_WEAVED
};

int ConvertToMFXFourCC(enum MondelloPixelFormat MondelloFormat);

class MondelloDevice
{
public:
    MondelloDevice(const char *devname = "mondello",
        uint32_t width = 1920,
        uint32_t height = 1080,
        enum MondelloPixelFormat mondelloFormat = NO_FORMAT,
        enum MondelloPicStructType mondelloPicStruct = PROGRESSIVE,
        uint32_t num_buffers = 4,
        int device_id = -1);

    ~MondelloDevice();

    void Init(uint32_t width,
        uint32_t height,
        enum MondelloPixelFormat mondelloFormat,
        uint32_t num_buffers,
        enum MondelloPicStructType mondelloPicStruct,
        bool isMondelloRender,
        bool printfps);

    const char* GetCurrentCameraName();
    int GetCurrentCameraId();
    int ConvertToV4L2FourCC();
    void SignalSetup();
    void MondelloInit();
    void MondelloSetup(camera_buffer_t *buffers);
    void MondelloStartCapture();
    void MondelloStopCapture();
    int GetMondelloTerminationSignal();
    int GetStreamId() { return m_stream_id; }
    int GetDeviceId() { return m_device_id; }
    bool GetPrintFPSMode() { return m_printfps; }
    enum MondelloPicStructType GetMondelloPicStructType() { return m_mondelloPicStruct; }
    void MondelloQbuf(camera_buffer_t *buf);
    camera_buffer_t *MondelloDQbuf();

protected:
    const char *m_device_name;
    int m_device_id;
    uint32_t m_height;
    uint32_t m_width;
    uint32_t m_num_buffers;
    stream_t m_streams[1];
    stream_config_t m_StreamList;
    camera_buffer_t *CurBuffers;
    int m_stream_id;
    enum MondelloPixelFormat m_mondelloFormat;
    enum MondelloPicStructType m_mondelloPicStruct;
    bool m_isMondelloRender;
    bool m_printfps;
    Parameters m_param;
};

#endif // ifdef __MONDELLO_UTIL_H__
