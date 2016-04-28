/* ****************************************************************************** *\
 *
 * INTEL CORPORATION PROPRIETARY INFORMATION
 * This software is supplied under the terms of a license agreement or nondisclosure
 * agreement with Intel Corporation and may not be copied or disclosed except in
 * accordance with the terms of that agreement
 * Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.
 *
 * \* ****************************************************************************** */

#if defined (ENABLE_MONDELLO_SUPPORT)

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <assert.h>
#include "mondello_util.h"
#include "libcamhal/api/ICamera.h"
#include "vaapi_utils.h"
#include <time.h>
#define FPS_TIME_INTERVAL 2
#define FPS_BUF_COUNT_START 10
using namespace icamera;

/* Global Declaration */
camera_buffer_t *buffers, *CurBuffers;
bool CtrlFlag = false;
int m_q[5], m_first = 0, m_last = 0, m_numInQ = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t empty = PTHREAD_MUTEX_INITIALIZER;

void MondelloDevice::PutOnQ(int x)
{
    pthread_mutex_lock(&mutex);
    m_q[m_first] = x;
    m_first = (m_first+1) % 5;
    m_numInQ++;
    pthread_mutex_unlock(&mutex);
    pthread_mutex_unlock(&empty);
}

int MondelloDevice::GetOffQ()
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

int MondelloDevice::GetMondelloTerminationSignal()
{
    if (m_isMondelloRender)
        return (CtrlFlag)? 1 : 0;

    return (CtrlFlag && m_numInQ == 0)? 1 : 0;
}

MondelloDevice::MondelloDevice(const char *devname,
    uint32_t width,
    uint32_t height,
    enum MondelloPixelFormat mondelloFormat,
    bool interlaced,
    uint32_t num_buffers,
    int device_id):
    m_device_name(devname),
    m_height(height),
    m_width(width),
    m_mondelloFormat(mondelloFormat),
    m_interlaced(interlaced),
    m_num_buffers(num_buffers),
    m_device_id(device_id)
{}

MondelloDevice::~MondelloDevice()
{
    if (m_device_id > -1)
    {
        free(buffers);

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
    enum MondelloPixelFormat mondelloFormat,
    uint32_t num_buffers,
    bool isInterlacedEnabled,
    bool isMondelloRender,
    bool printfps)
{
    (m_width != width )? m_width = width : m_width;
    (m_height != height)? m_height = height : m_height;
    (m_mondelloFormat != mondelloFormat )? m_mondelloFormat = mondelloFormat : m_mondelloFormat;
    (m_num_buffers != num_buffers)? m_num_buffers = num_buffers : m_num_buffers;
    (m_interlaced != isInterlacedEnabled)? m_interlaced = isInterlacedEnabled : m_interlaced;
    (m_isMondelloRender != isMondelloRender)? m_isMondelloRender = isMondelloRender : m_isMondelloRender;
    (m_printfps != printfps)? m_printfps = printfps : m_printfps;

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
    buffers = (camera_buffer_t *)malloc(sizeof(camera_buffer_t) * (int)m_num_buffers);
}

void MondelloDevice::MondelloSetup()
{
    int i, ret;

    for (i = 0, CurBuffers = buffers; i < m_num_buffers; i++, CurBuffers++)
    {
        CurBuffers->s = m_streams[0];

        /* camera_device_stream_qbuf() */
        ret = libcamhal._ZN7icamera18camera_stream_qbufEiiPNS_15camera_buffer_tE(m_device_id, m_stream_id, CurBuffers);
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

static void CtrlCTerminationHandler(int s) { CtrlFlag = true; }

int ConvertToMFXFourCC(enum MondelloPixelFormat MondelloFormat)
{
    switch (MondelloFormat)
    {
        case UYVY:  return MFX_FOURCC_UYVY;
        case RGB4:  return MFX_FOURCC_RGB4;
        case NO_FORMAT:

        default:
            assert( !"Unsupported mfx fourcc");
            return 0;
    }
}

void *PollingThread(void *data)
{

    MondelloDevice *Mondello = (MondelloDevice *)data;

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = CtrlCTerminationHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    int ret;
    int m_device_id, m_stream_id;
    struct timeval dqbuf_start_tm_count, dqbuf_tm_start,qbuf_tm_end;
    double buf_count = 0, last_buf_count = 0;
    double max_fps = 0, min_fps = 0;
    double av_fps = 0, sum_time = 0, tm_interval = 0, init_max_min_fps = true;

    m_device_id = Mondello->GetDeviceId();
    m_stream_id = Mondello->GetStreamId();
    bool m_printfps = Mondello->GetPrintFPSMode();

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
        ret = Mondello->libcamhal.
            _ZN7icamera19camera_stream_dqbufEiiPPNS_15camera_buffer_tE(
            m_device_id, m_stream_id, &CurBuffers);

        BYE_ON(ret < 0, "Mondello DQBUF failed: %s\n", ERRSTR);

        Mondello->PutOnQ(CurBuffers->index);

        if (CtrlFlag)
            break;

        if (CurBuffers)
        {
            /* camera_stream_qbuf() */
            ret = Mondello->
                libcamhal._ZN7icamera18camera_stream_qbufEiiPNS_15camera_buffer_tE(
                m_device_id, m_stream_id, CurBuffers);

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

#endif // ifdef ENABLE_MONDELLO_SUPPORT
