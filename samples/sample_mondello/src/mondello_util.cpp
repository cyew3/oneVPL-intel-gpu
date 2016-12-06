/* ****************************************************************************** *\
 *
 * INTEL CORPORATION PROPRIETARY INFORMATION
 * This software is supplied under the terms of a license agreement or nondisclosure
 * agreement with Intel Corporation and may not be copied or disclosed except in
 * accordance with the terms of that agreement
 * Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.
 *
 * \* ****************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include "mondello_util.h"
#include "libcamhal/api/ICamera.h"
#include "libcamhal/api/Parameters.h"
#define MAX_BUFFER_COUNT 10
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

    // wait if the queue is empty.
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
    enum MondelloPicStructType mondelloPicStruct,
    uint32_t num_buffers,
    int device_id):
    m_device_name(devname),
    m_height(height),
    m_width(width),
    m_mondelloFormat(mondelloFormat),
    m_mondelloPicStruct(mondelloPicStruct),
    m_num_buffers(num_buffers),
    m_device_id(device_id)
{}

MondelloDevice::~MondelloDevice()
{
    if (m_device_id > -1)
    {
        free(buffers);

        camera_device_close(m_device_id);
    }

    camera_hal_deinit();
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
    int count = get_number_of_cameras();

    for (; id < count; id++)
    {
        camera_info_t info;
        CLEAR(info);

        get_camera_info(id, info);

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
        case YUY2:  return V4L2_PIX_FMT_YUYV;
        case UYVY:  return V4L2_PIX_FMT_UYVY;
        case RGB4:  return V4L2_PIX_FMT_XBGR32;

        case NO_FORMAT:
        default:
            BYE_ON(1, "Unsupported v4l2 fourcc!\n");
            return 0;
    }
}

void MondelloDevice::Init(uint32_t width,
    uint32_t height,
    enum MondelloPixelFormat mondelloFormat,
    uint32_t num_buffers,
    enum MondelloPicStructType mondelloPicStruct,
    bool isMondelloRender,
    bool printfps)
{
    (m_width != width )? m_width = width : m_width;
    (m_height != height)? m_height = height : m_height;
    (m_mondelloFormat != mondelloFormat )? m_mondelloFormat = mondelloFormat : m_mondelloFormat;
    (m_num_buffers != num_buffers)? m_num_buffers = num_buffers : m_num_buffers;
    (m_mondelloPicStruct != mondelloPicStruct )? m_mondelloPicStruct = mondelloPicStruct : m_mondelloPicStruct;
    (m_isMondelloRender != isMondelloRender)? m_isMondelloRender = isMondelloRender : m_isMondelloRender;
    (m_printfps != printfps)? m_printfps = printfps : m_printfps;

    // Setup the streams information
    m_streams[0].format = ConvertToV4L2FourCC();
    m_streams[0].width = m_width;
    m_streams[0].height = m_height;
    m_streams[0].memType = V4L2_MEMORY_DMABUF;

    (m_mondelloPicStruct == PROGRESSIVE)?
        m_streams[0].field = V4L2_FIELD_ANY :
        m_streams[0].field = V4L2_FIELD_ALTERNATE;

    m_StreamList.num_streams = 1;
    m_StreamList.streams = m_streams;

    MondelloInit();
}

void MondelloDevice::MondelloInit()
{
    int ret, count = 0;
    camera_info_t info;

    ret = camera_hal_init();
    BYE_ON(ret < 0, "failed to init libcamhal device: %s\n", ERRSTR);

    m_device_id = GetCurrentCameraId();

    ret = camera_device_open(m_device_id, 0);
    if (ret < 0)
    {
        camera_hal_deinit();
        BYE_ON(ret < 0, "inCorrect device id: %s\n", ERRSTR);
    }

    if (m_mondelloPicStruct == IPU_WEAVED)
    {
        // This operation is to do weaving on the IPU end.
        ret = m_param.setDeinterlaceMode(DEINTERLACE_WEAVING);
        BYE_ON(ret < 0, "failed to setDeinterlaceMode: %s\n", ERRSTR);

        ret = camera_set_parameters(m_device_id, m_param);
        BYE_ON(ret < 0, "failed to set parameters: %s\n", ERRSTR);
    }

    ret = camera_device_config_streams(m_device_id, &m_StreamList, m_streams[0].format);
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

    // libcamhal only request for 10 buffers, QBUF operation will fail if
    // we acceed more than that buffer size
    if (m_num_buffers > MAX_BUFFER_COUNT)
       m_num_buffers = MAX_BUFFER_COUNT;

    for (i = 0, CurBuffers = buffers; i < m_num_buffers; i++, CurBuffers++)
    {
        CurBuffers->s = m_streams[0];

        ret = camera_stream_qbuf(m_device_id, m_stream_id, CurBuffers, m_num_buffers, NULL);
        BYE_ON(ret < 0, "qbuf failed: %s\n", ERRSTR);
    }
}

void MondelloDevice::MondelloStartCapture()
{
    int ret;

    ret = camera_device_start(m_device_id);
    BYE_ON(ret < 0, "mondello STREAMON failed: %s\n", ERRSTR);
}

void MondelloDevice::MondelloStopCapture()
{
    int ret;

    ret = camera_device_stop(m_device_id);
    BYE_ON(ret < 0, "mondello STREAMOFF failed: %s\n", ERRSTR);
}

static void CtrlCTerminationHandler(int s) { CtrlFlag = true; }

int ConvertToMFXFourCC(enum MondelloPixelFormat MondelloFormat)
{
    switch (MondelloFormat)
    {
        case YUY2:  return MFX_FOURCC_YUY2;
        case UYVY:  return MFX_FOURCC_UYVY;
        case RGB4:  return MFX_FOURCC_RGB4;
        case NO_FORMAT:

        default:
            BYE_ON(1, "Unsupported mfx fourcc!\n");
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
    int prev_field = V4L2_FIELD_ALTERNATE, field_cnt = 0;
    bool flags = true;
    int m_device_id, m_stream_id;
    struct timeval dqbuf_start_tm_count, dqbuf_tm_start,qbuf_tm_end;
    double buf_count = 0, last_buf_count = 0;
    double max_fps = 0, min_fps = 0;
    double av_fps = 0, sum_time = 0, tm_interval = 0, init_max_min_fps = true;

    m_device_id = Mondello->GetDeviceId();
    m_stream_id = Mondello->GetStreamId();
    bool m_printfps = Mondello->GetPrintFPSMode();
    enum MondelloPicStructType m_mondelloPicStruct = Mondello->GetMondelloPicStructType();

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

        ret = camera_stream_dqbuf(m_device_id, m_stream_id, &CurBuffers, NULL);
        BYE_ON(ret < 0, "Mondello DQBUF failed: %s\n", ERRSTR);

        // IPU does not guarantee if there is a frame drop. An exception handling
        // mechanism is needed to be done on the app level to fix the odd/even
        // misalignment issue when it undergo GPU weave operation (this is the case
        // where we submit V4L2_FIELD_ALTERNATE to the IPU).

        if (m_mondelloPicStruct == GPU_WEAVED)
        {
            if (flags)
            {
                if (CurBuffers->s.field == V4L2_FIELD_TOP)
                    prev_field = V4L2_FIELD_TOP;

                flags = false;
            }

            if (prev_field != CurBuffers->s.field)
            {
                prev_field = CurBuffers->s.field;
                Mondello->PutOnQ(CurBuffers->index);
                field_cnt = 0;
            }
            else
                field_cnt++;

            BYE_ON(field_cnt > MAX_BUFFER_COUNT, "Overloading similar fields!\n");
        }
        else
            Mondello->PutOnQ(CurBuffers->index);

        if (CtrlFlag)
            break;

        if (CurBuffers)
        {
            ret = camera_stream_qbuf(m_device_id, m_stream_id, CurBuffers, MAX_BUFFER_COUNT, NULL);
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
