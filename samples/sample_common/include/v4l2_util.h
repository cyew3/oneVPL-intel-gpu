/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#ifndef __V4L2_UTIL_H__
#define __V4L2_UTIL_H__

#include <pthread.h>
#include <stdlib.h>
#include <va/va.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "sample_defs.h"
#if defined (ENABLE_MONDELLO_SUPPORT)
#include "libcamhal/api/ICamera.h"
#include "vaapi_utils.h"
#endif

/* MIPI DRIVER Configurations*/
#define _ISP_MODE_NONE          0x0000
#define _ISP_MODE_CONTINUOUS    0x1000
#define _ISP_MODE_STILL         0x2000
#define _ISP_MODE_VIDEO         0x4000
#define _ISP_MODE_PREVIEW       0x8000

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define ERRSTR strerror(errno)

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

enum AtomISPMode
{
    NONE = 0, PREVIEW, STILL, VIDEO, CONTINUOUS
};

enum V4L2PixelFormat
{
    NO_FORMAT = 0, UYVY, YUY2, RGB4
};

typedef struct _V4L2Buffer
{
    int fd, index;
} V4L2Buffer;

typedef struct
{
    V4L2Buffer *V4L2Buf;
#if defined (ENABLE_MONDELLO_SUPPORT)
    camera_buffer_t *MondelloBuf;
#endif
} Buffer;

extern Buffer buffers;
void *PollingThread(void *data);


#if defined (ENABLE_MONDELLO_SUPPORT)
class MondelloDevice
{
public:
    MfxLoader::LibCamhalProxy libcamhal;

    MondelloDevice(const char *devname = "mondello",
        uint32_t width = 1920,
        uint32_t height = 1080,
        enum V4L2PixelFormat mondelloFormat = NO_FORMAT,
        bool interlaced = false,
        uint32_t num_buffers = 4);

    ~MondelloDevice();

    void Init(uint32_t width,
        uint32_t height,
        enum V4L2PixelFormat mondelloFormat,
        uint32_t num_buffers,
        bool isInterlacedEnabled);

    const char* GetCurrentCameraName();
    int GetCurrentCameraId();
    int ConvertToV4L2FourCC();
    void MondelloInit();
    void MondelloAlloc();
    void MondelloSetup();
    void MondelloStartCapture();
    void MondelloStopCapture();
    int GetStreamId() { return m_stream_id; }
    int GetDeviceId() { return m_device_id; }
    bool SetPrintFPSMode(bool printfps) { m_printfps = printfps; }
    bool GetPrintFPSMode() { return m_printfps; }
    bool ISMondelloEnabled() { return (m_device_id > 0)? true : false; }

protected:
    const char *m_device_name;
    int m_device_id;
    uint32_t m_height;
    uint32_t m_width;
    uint32_t m_num_buffers;
    bool m_interlaced;
    stream_t m_streams[1];
    stream_config_t m_StreamList;
    int m_stream_id;
    enum V4L2PixelFormat m_mondelloFormat;
    bool m_printfps;
};
#endif // ifdef ENABLE_MONDELLO_SUPPORT


class v4l2Device
{
public:
    v4l2Device(const char *devname = "/dev/video0",
        uint32_t width = 1920,
        uint32_t height = 1080,
        uint32_t num_buffer = 4,
        enum AtomISPMode MipiMode = NONE,
        enum V4L2PixelFormat m_v4l2Format = NO_FORMAT);

    ~v4l2Device();

    void Init(const char *devname,
        uint32_t width,
        uint32_t height,
        uint32_t num_buffer,
        enum V4L2PixelFormat v4l2Format,
        enum AtomISPMode MipiMode,
        int m_MipiPort);

    void V4L2Init();
    void V4L2Alloc();
    int blockIOCTL(int handle, int request, void *args);
    int GetAtomISPModes(enum AtomISPMode mode);
    void V4L2QueueBuffer(V4L2Buffer *buffer);
    V4L2Buffer *V4L2DeQueueBuffer(V4L2Buffer *buffer);
    void V4L2StartCapture();
    void V4L2StopCapture();
    int GetV4L2TerminationSignal();
    void PutOnQ(int x);
    int GetOffQ();
    int ConvertToMFXFourCC(enum V4L2PixelFormat v4l2Format);
    int ConvertToV4L2FourCC();
    int GetV4L2DisplayID() { return m_fd; }
    bool ISV4L2Enabled() { return (m_fd > 0)? true : false; }
#if defined (ENABLE_MONDELLO_SUPPORT)
    MondelloDevice Mondello;

    bool SetMondelloRenderMode(bool isMondelloRender)
    { m_isMondelloRender = isMondelloRender; }
#endif

protected:
    const char *m_devname;
    uint32_t m_height;
    uint32_t m_width;
    uint32_t m_num_buffers;
    struct v4l2_pix_format m_format;
    int m_MipiPort;
    enum AtomISPMode m_MipiMode;
    enum V4L2PixelFormat m_v4l2Format;
    int m_fd;
#if defined (ENABLE_MONDELLO_SUPPORT)
    bool m_isMondelloRender;
#endif
};

#endif // ifdef __V4L2_UTIL_H__
