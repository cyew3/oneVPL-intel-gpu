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

#if defined (ENABLE_V4L2_SUPPORT)

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <assert.h>
#include <linux/videodev2.h>
#include "v4l2_util.h"
#if defined (ENABLE_MONDELLO_SUPPORT)
#include "libcamhal/api/ICamera.h"
#include "vaapi_utils.h"
#include <time.h>
#define FPS_TIME_INTERVAL 2
#define FPS_BUF_COUNT_START 10
using namespace icamera;
#endif

/* Global Declaration */
Buffer buffers, CurBuffers;
bool CtrlFlag = false;
int m_q[5], m_first = 0, m_last = 0, m_numInQ = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t empty = PTHREAD_MUTEX_INITIALIZER;

v4l2Device::v4l2Device( const char *devname,
            uint32_t width,
            uint32_t height,
            uint32_t num_buffers,
            enum AtomISPMode MipiMode,
            enum V4L2PixelFormat v4l2Format):
            m_devname(devname),
            m_height(height),
            m_width(width),
            m_num_buffers(num_buffers),
            m_MipiPort(0),
            m_MipiMode(MipiMode),
            m_v4l2Format(v4l2Format),
            m_fd(-1)
{
}

v4l2Device::~v4l2Device()
{
    if (m_fd > -1)
    {
        BYE_ON(close(m_fd) < 0, "V4L2 device close failed: %s\n", ERRSTR);
    }
}

int v4l2Device::blockIOCTL(int handle, int request, void *args)
{
    int ioctlStatus;
    do
    {
        ioctlStatus = ioctl(handle, request, args);
    } while (-1 == ioctlStatus && EINTR == errno);
    return ioctlStatus;
}

int v4l2Device::GetAtomISPModes(enum AtomISPMode mode)
{
    switch(mode)
    {
        case VIDEO:    return _ISP_MODE_VIDEO;
        case PREVIEW:    return _ISP_MODE_PREVIEW;
        case CONTINUOUS:    return _ISP_MODE_CONTINUOUS;
        case STILL:    return _ISP_MODE_STILL;
        case NONE:

        default:
            return _ISP_MODE_NONE;
    }
}

int v4l2Device::ConvertToMFXFourCC(enum V4L2PixelFormat v4l2Format)
{
    switch (v4l2Format)
    {
        case UYVY:  return MFX_FOURCC_UYVY;
        case YUY2:  return MFX_FOURCC_YUY2;
        case RGB4:  return MFX_FOURCC_RGB4;
        case NO_FORMAT:

        default:
            assert( !"Unsupported mfx fourcc");
            return 0;
    }
}

int v4l2Device::ConvertToV4L2FourCC()
{
    switch (m_v4l2Format)
    {
        case UYVY:    return V4L2_PIX_FMT_UYVY;
        case YUY2:    return V4L2_PIX_FMT_YUYV;
        case NO_FORMAT:

        default:
            assert( !"Unsupported v4l2 fourcc");
            return 0;
    }
}

void v4l2Device::Init( const char *devname,
            uint32_t width,
            uint32_t height,
            uint32_t num_buffers,
            enum V4L2PixelFormat v4l2Format,
            enum AtomISPMode MipiMode,
            int MipiPort)
{

    (devname != NULL)? m_devname = devname : m_devname;
    (m_width != width )? m_width = width : m_width;
    (m_height != height)? m_height = height : m_height;
    (m_num_buffers != num_buffers)? m_num_buffers = num_buffers : m_num_buffers;
    (m_v4l2Format != v4l2Format )? m_v4l2Format = v4l2Format : m_v4l2Format;
    (m_MipiMode != MipiMode )? m_MipiMode = MipiMode : m_MipiMode;
    (m_MipiPort != MipiPort )? m_MipiPort = MipiPort : m_MipiPort;

    memset(&m_format, 0, sizeof m_format);
    m_format.width = m_width;
    m_format.height = m_height;
    m_format.pixelformat = ConvertToV4L2FourCC();

    V4L2Init();
}

void v4l2Device::V4L2Init()
{
    int ret;
    struct v4l2_format fmt;
    struct v4l2_capability caps;
    struct v4l2_streamparm parm;
    struct v4l2_requestbuffers rqbufs;
    CLEAR(parm);

    m_fd = open(m_devname, O_RDWR);
    BYE_ON(m_fd < 0, "failed to open %s: %s\n", m_devname, ERRSTR);
    CLEAR(caps);

    /* Specifically for setting up mipi configuration. DMABUFF is
     * also enable by default here.
     */
    if (m_MipiPort > -1  && m_MipiMode != NONE) {
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.capturemode = GetAtomISPModes(m_MipiMode);

    ret = blockIOCTL(m_fd, VIDIOC_S_INPUT, &m_MipiPort);
    BYE_ON(ret < 0, "VIDIOC_S_INPUT failed: %s\n", ERRSTR);

    ret = blockIOCTL(m_fd, VIDIOC_S_PARM, &parm);
    BYE_ON(ret < 0, "VIDIOC_S_PARAM failed: %s\n", ERRSTR);
    }

    ret = blockIOCTL(m_fd, VIDIOC_QUERYCAP, &caps);
    msdk_printf( "Driver Caps:\n"
            "  Driver: \"%s\"\n"
            "  Card: \"%s\"\n"
            "  Bus: \"%s\"\n"
            "  Version: %d.%d\n"
            "  Capabilities: %08x\n",
            caps.driver,
            caps.card,
            caps.bus_info,
            (caps.version>>16)&&0xff,
            (caps.version>>24)&&0xff,
            caps.capabilities);

    BYE_ON(ret, "VIDIOC_QUERYCAP failed: %s\n", ERRSTR);
    BYE_ON(~caps.capabilities & V4L2_CAP_VIDEO_CAPTURE,
                "video: singleplanar capture is not supported\n");

    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = blockIOCTL(m_fd, VIDIOC_G_FMT, &fmt);

    BYE_ON(ret < 0, "VIDIOC_G_FMT failed: %s\n", ERRSTR);

    msdk_printf("G_FMT(start): width = %u, height = %u, 4cc = %.4s, BPP = %u sizeimage = %d field = %d\n",
        fmt.fmt.pix.width, fmt.fmt.pix.height,
        (char*)&fmt.fmt.pix.pixelformat,
        fmt.fmt.pix.bytesperline,
        fmt.fmt.pix.sizeimage,
        fmt.fmt.pix.field);

    fmt.fmt.pix = m_format;

    msdk_printf("G_FMT(pre): width = %u, height = %u, 4cc = %.4s, BPP = %u sizeimage = %d field = %d\n",
        fmt.fmt.pix.width, fmt.fmt.pix.height,
        (char*)&fmt.fmt.pix.pixelformat,
        fmt.fmt.pix.bytesperline,
        fmt.fmt.pix.sizeimage,
        fmt.fmt.pix.field);

    ret = blockIOCTL(m_fd, VIDIOC_S_FMT, &fmt);
    BYE_ON(ret < 0, "VIDIOC_S_FMT failed: %s\n", ERRSTR);

    ret = blockIOCTL(m_fd, VIDIOC_G_FMT, &fmt);
    BYE_ON(ret < 0, "VIDIOC_G_FMT failed: %s\n", ERRSTR);
    msdk_printf("G_FMT(final): width = %u, height = %u, 4cc = %.4s, BPP = %u\n",
        fmt.fmt.pix.width, fmt.fmt.pix.height,
        (char*)&fmt.fmt.pix.pixelformat,
        fmt.fmt.pix.bytesperline);

    CLEAR(rqbufs);
    rqbufs.count = m_num_buffers;
    rqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rqbufs.memory = V4L2_MEMORY_DMABUF;

    ret = blockIOCTL(m_fd, VIDIOC_REQBUFS, &rqbufs);
    BYE_ON(ret < 0, "VIDIOC_REQBUFS failed: %s\n", ERRSTR);
    BYE_ON(rqbufs.count < m_num_buffers, "video node allocated only "
            "%u of %u buffers\n", rqbufs.count, m_num_buffers);

    m_format = fmt.fmt.pix;
}

void v4l2Device::V4L2Alloc()
{
    buffers.V4L2Buf = (V4L2Buffer *)malloc(sizeof(V4L2Buffer) * (int) m_num_buffers);
}

void v4l2Device::V4L2QueueBuffer(V4L2Buffer *buffer)
{
    struct v4l2_buffer buf;
    int ret;

    memset(&buf, 0, sizeof buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.index = buffer->index;
    buf.m.fd = buffer->fd;

    ret = blockIOCTL(m_fd, VIDIOC_QBUF, &buf);
    BYE_ON(ret < 0, "VIDIOC_QBUF for buffer %d failed: %s (fd %u) (i %u)\n",
    buf.index, ERRSTR, buffer->fd, buffer->index);
}

V4L2Buffer *v4l2Device::V4L2DeQueueBuffer(V4L2Buffer *buffer)
{
    struct v4l2_buffer buf;
    int ret;

    memset(&buf, 0, sizeof buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_DMABUF;

    ret = blockIOCTL(m_fd, VIDIOC_DQBUF, &buf);
    BYE_ON(ret, "VIDIOC_DQBUF failed: %s\n", ERRSTR);

    return &buffer[buf.index];
}

void v4l2Device::V4L2StartCapture()
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret = 0;

    ret = blockIOCTL(m_fd, VIDIOC_STREAMON, &type);
    BYE_ON(ret < 0, "STREAMON failed: %s\n", ERRSTR);
}

void v4l2Device::V4L2StopCapture()
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret = 0;

    ret = blockIOCTL(m_fd, VIDIOC_STREAMOFF, &type);
    BYE_ON(ret < 0, "STREAMOFF failed: %s\n", ERRSTR);
}

void v4l2Device::PutOnQ(int x)
{
    pthread_mutex_lock(&mutex);
    m_q[m_first] = x;
    m_first = (m_first+1) % 5;
    m_numInQ++;
    pthread_mutex_unlock(&mutex);
    pthread_mutex_unlock(&empty);
}

int v4l2Device::GetOffQ()
{
    int thing;

    /* wait if the queue is empty. */
    while (m_numInQ == 0)
    pthread_mutex_lock(&empty);

    pthread_mutex_lock(&mutex);
    thing = m_q[m_last];
    m_last = (m_last+1) % 5;
    m_numInQ--;
    pthread_mutex_unlock(&mutex);

    return thing;
}

int v4l2Device::GetV4L2TerminationSignal()
{
#if defined (ENABLE_MONDELLO_SUPPORT)
    if (m_isMondelloRender)
        return (CtrlFlag)? 1 : 0;
#endif

    return (CtrlFlag && m_numInQ == 0)? 1 : 0;
}

#if defined (ENABLE_MONDELLO_SUPPORT)

MondelloDevice::MondelloDevice(const char *devname,
    uint32_t width,
    uint32_t height,
    enum V4L2PixelFormat mondelloFormat,
    bool interlaced,
    uint32_t num_buffers):
    m_device_name(devname),
    m_height(height),
    m_width(width),
    m_mondelloFormat(mondelloFormat),
    m_interlaced(interlaced),
    m_num_buffers(num_buffers)
{}

MondelloDevice::~MondelloDevice()
{
    if (m_device_id > 0)
    {
        free(buffers.MondelloBuf);

        /* camera_device_close() */
        libcamhal._ZN7icamera19camera_device_closeEi(m_device_id);
    }

    /* camera_hal_deinit() */
    libcamhal._ZN7icamera17camera_hal_deinitEv();
}

const char* MondelloDevice::GetCurrentCameraName()
{
    const char* CAMERA_INPUT = "cameraInput";
    const char *input = getenv(CAMERA_INPUT);
    if (!input)
        input = "tpg";

    return input;
}

int MondelloDevice::GetCurrentCameraId()
{
    int cameraId = 0, id = 0;
    const char* input = GetCurrentCameraName();
    /* get_number_of_cameras() */
    int count = libcamhal._ZN7icamera21get_number_of_camerasEv();

    for (; id < count; id++)
    {
        camera_info_t info;
        CLEAR(info);

        /* get_camera_info() */
        libcamhal._ZN7icamera15get_camera_infoEiRNS_13camera_info_tE(id, info);

        if (strcmp(info.name, input) == 0)
        {
            cameraId = id;
            break;
        }
    }
    BYE_ON(id == count, "No camera name matched, please check if cameraInput is correc: %s\n", ERRSTR);

    return cameraId;
}

int MondelloDevice::ConvertToV4L2FourCC()
{
    switch (m_mondelloFormat)
    {
        case UYVY:  return V4L2_PIX_FMT_UYVY;
        case RGB4:  return V4L2_PIX_FMT_XBGR32;

        case NO_FORMAT:
        default:
            assert( !"Unsupported v4l2 fourcc");
            return 0;
    }
}

void MondelloDevice::Init(uint32_t width,
    uint32_t height,
    enum V4L2PixelFormat mondelloFormat,
    uint32_t num_buffers,
    bool isInterlacedEnabled)
{
    (m_width != width )? m_width = width : m_width;
    (m_height != height)? m_height = height : m_height;
    (m_mondelloFormat != mondelloFormat )? m_mondelloFormat = mondelloFormat : m_mondelloFormat;
    (m_num_buffers != num_buffers)? m_num_buffers = num_buffers : m_num_buffers;
    (m_interlaced != isInterlacedEnabled)? m_interlaced = isInterlacedEnabled : m_interlaced;

    /* Setup the streams information */
    m_streams[0].format = ConvertToV4L2FourCC();
    m_streams[0].width = m_width;
    m_streams[0].height = m_height;
    m_streams[0].memType = V4L2_MEMORY_DMABUF;

    (m_interlaced == true)?
        m_streams[0].field = V4L2_FIELD_ALTERNATE :
        m_streams[0].field = V4L2_FIELD_ANY;

    m_StreamList.num_streams = 1;
    m_StreamList.streams = m_streams;

    MondelloInit();
}

void MondelloDevice::MondelloInit()
{
    int ret, count = 0;
    camera_info_t info;

    /* camera_hal_init() */
    ret = libcamhal._ZN7icamera15camera_hal_initEv();
    BYE_ON(ret < 0, "failed to init libcamhal device: %s\n", ERRSTR);

    m_device_id = GetCurrentCameraId();

    /* camera_device_open() */
    ret = libcamhal._ZN7icamera18camera_device_openEi(m_device_id);
    if (ret < 0)
    {
        /* camera_hal_deinit() */
        libcamhal._ZN7icamera17camera_hal_deinitEv();
        BYE_ON(ret < 0, "inCorrect device id: %s\n", ERRSTR);
    }

    /* camera_device_config_streams() */
    ret = libcamhal._ZN7icamera28camera_device_config_streamsEiPNS_15stream_config_tE(m_device_id, &m_StreamList);
    BYE_ON(ret < 0, "failed to add stream: %s\n", ERRSTR);
    m_stream_id = m_streams[0].id;
}

void MondelloDevice::MondelloAlloc()
{
    buffers.MondelloBuf = (camera_buffer_t *)malloc(sizeof(camera_buffer_t) * (int)m_num_buffers);
}

void MondelloDevice::MondelloSetup()
{
    int i, ret;

    for (i = 0, CurBuffers.MondelloBuf = buffers.MondelloBuf;
        i < m_num_buffers;
        i++, CurBuffers.MondelloBuf++)
    {
        CurBuffers.MondelloBuf->s = m_streams[0];

        /* camera_device_stream_qbuf() */
        ret = libcamhal._ZN7icamera18camera_stream_qbufEiiPNS_15camera_buffer_tE(m_device_id, m_stream_id, CurBuffers.MondelloBuf);
        BYE_ON(ret < 0, "qbuf failed: %s\n", ERRSTR);
    }
}

void MondelloDevice::MondelloStartCapture()
{
    int ret;

    /* camera_device_start() */
    ret = libcamhal._ZN7icamera19camera_device_startEi(m_device_id);
    BYE_ON(ret < 0, "mondello STREAMON failed: %s\n", ERRSTR);
}

void MondelloDevice::MondelloStopCapture()
{
    int ret;

    /* camera_device_stop() */
    ret = libcamhal._ZN7icamera18camera_device_stopEi(m_device_id);
    BYE_ON(ret < 0, "mondello STREAMOFF failed: %s\n", ERRSTR);
}

#endif // ifdef ENABLE_MONDELLO_SUPPORT

static void CtrlCTerminationHandler(int s) { CtrlFlag = true; }

void *PollingThread(void *data)
{

    v4l2Device *v4l2 = (v4l2Device *)data;

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = CtrlCTerminationHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    if (v4l2->ISV4L2Enabled())
    {
        struct pollfd fd;
        fd.fd = v4l2->GetV4L2DisplayID();
        fd.events = POLLIN;

        while(1)
        {
            if (poll(&fd, 1, 5000) > 0)
            {
                if (fd.revents & POLLIN)
                {
                    CurBuffers.V4L2Buf = v4l2->V4L2DeQueueBuffer(buffers.V4L2Buf);
                    v4l2->PutOnQ(CurBuffers.V4L2Buf->index);

                    if (CtrlFlag)
                        break;

                    if (CurBuffers.V4L2Buf)
                        v4l2->V4L2QueueBuffer(&buffers.V4L2Buf[CurBuffers.V4L2Buf->index]);
                }
            }
        }
    }

#if defined (ENABLE_MONDELLO_SUPPORT)
    if (v4l2->Mondello.ISMondelloEnabled())
    {
        int ret;
        int m_device_id, m_stream_id;
        struct timeval dqbuf_start_tm_count, dqbuf_tm_start,qbuf_tm_end;
        double buf_count = 0, last_buf_count = 0;
        double max_fps = 0, min_fps = 0;
        double av_fps = 0, sum_time = 0, tm_interval = 0, init_max_min_fps = true;

        m_device_id = v4l2->Mondello.GetDeviceId();
        m_stream_id = v4l2->Mondello.GetStreamId();
        bool m_printfps = v4l2->Mondello.GetPrintFPSMode();

        while(1)
        {
            if (m_printfps)
            {
                buf_count++;
                if (buf_count == FPS_BUF_COUNT_START)
                {
                    gettimeofday(&dqbuf_start_tm_count,NULL);
                    dqbuf_tm_start = dqbuf_start_tm_count;
                    last_buf_count = buf_count;
                }
                else if (buf_count > FPS_BUF_COUNT_START)
                {
                    gettimeofday(&qbuf_tm_end, NULL);

                    double div = (qbuf_tm_end.tv_sec-dqbuf_tm_start.tv_sec) *
                        1000000 + qbuf_tm_end.tv_usec - dqbuf_tm_start.tv_usec;

                    sum_time += div;
                    tm_interval += div;
                    dqbuf_tm_start = qbuf_tm_end;

                    if (tm_interval >= (FPS_TIME_INTERVAL*1000000))
                    {
                        double interval_fps = (buf_count - last_buf_count) /
                            (tm_interval/1000000);

                        msdk_printf("fps:%.4f\n",interval_fps);

                        if (init_max_min_fps)
                        {
                            max_fps = interval_fps;
                            min_fps = interval_fps;
                            init_max_min_fps = false;
                        }

                        if (interval_fps >= max_fps)
                            max_fps = interval_fps;
                        else if (interval_fps < min_fps)
                            min_fps = interval_fps;

                        tm_interval = 0;
                        last_buf_count = buf_count;
                    }
                }
            }

            /* camera_stream_dqbuf() */
            ret = v4l2->Mondello.libcamhal.
                _ZN7icamera19camera_stream_dqbufEiiPPNS_15camera_buffer_tE(
                m_device_id, m_stream_id, &CurBuffers.MondelloBuf);

            BYE_ON(ret < 0, "Mondello DQBUF failed: %s\n", ERRSTR);

            v4l2->PutOnQ(CurBuffers.MondelloBuf->index);

            if (CtrlFlag)
                break;

            if (CurBuffers.MondelloBuf)
            {
                /* camera_stream_qbuf() */
                ret = v4l2->Mondello.
                    libcamhal._ZN7icamera18camera_stream_qbufEiiPNS_15camera_buffer_tE(
                    m_device_id, m_stream_id, CurBuffers.MondelloBuf);

                BYE_ON(ret < 0, "Mondello QBUF failed: %s\n", ERRSTR);
            }
        }

        if (m_printfps)
        {
            av_fps = (buf_count - FPS_BUF_COUNT_START) / (sum_time / 1000000);

            if (buf_count <= FPS_BUF_COUNT_START)
                msdk_printf("num-buffers value is too low, should be at least %d\n\n", FPS_BUF_COUNT_START);
            else if (max_fps == 0 || min_fps == 0)
                msdk_printf("Average fps is:%.4f\n\n",av_fps);
            else
                msdk_printf("\nTotal frame is:%g\n",buf_count);

            msdk_printf("\nMax fps is:%.4f,Minimum fps is:%.4f,Average fps is:%.4f\n\n",
                max_fps, min_fps, av_fps);
        }
    }
#endif
}

#endif // ifdef ENABLE_V4L2_SUPPORT
