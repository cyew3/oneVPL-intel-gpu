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
#include <signal.h>
#include <time.h>
#include "mondello_util.h"
#include "libcamhal/api/ICamera.h"
#include "libcamhal/api/Parameters.h"
using namespace icamera;

// Global Declaration
static bool mExiting = false;

static void CtrlCTerminationHandler(int s) { mExiting = true; }

void MondelloDevice::SignalSetup(void)
{
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = CtrlCTerminationHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
}

int MondelloDevice::GetMondelloTerminationSignal()
{
    return (mExiting)? 1 : 0;
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

    SignalSetup();
    MondelloInit();
}

void MondelloDevice::MondelloInit()
{
    int ret = -1, count = 0;
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

void MondelloDevice::MondelloSetup(camera_buffer_t *buffers)
{
    int i, ret = -1;

    for (i = 0, CurBuffers = buffers; i < m_num_buffers; i++, CurBuffers++)
    {
        CurBuffers->s = m_streams[0];

        ret = camera_stream_qbuf(m_device_id, m_stream_id, CurBuffers, 1, NULL);
        BYE_ON(ret < 0, "qbuf failed: %s\n", ERRSTR);
    }
}

void MondelloDevice::MondelloStartCapture()
{
    int ret = -1;

    ret = camera_device_start(m_device_id);
    BYE_ON(ret < 0, "mondello STREAMON failed: %s\n", ERRSTR);
}

void MondelloDevice::MondelloStopCapture()
{
    int ret = -1;

    ret = camera_device_stop(m_device_id);
    BYE_ON(ret < 0, "mondello STREAMOFF failed: %s\n", ERRSTR);
}

void MondelloDevice::MondelloQbuf(camera_buffer_t *buf)
{
    int ret = -1;

    ret = camera_stream_qbuf(m_device_id, m_stream_id, buf, 1, NULL);
    BYE_ON(ret < 0, "Mondello QBUF failed: %s\n", ERRSTR);
}

camera_buffer_t * MondelloDevice::MondelloDQbuf()
{
    int ret = -1;

    ret = camera_stream_dqbuf(m_device_id, m_stream_id, &CurBuffers, NULL);
    BYE_ON(ret < 0, "Mondello QBUF failed: %s\n", ERRSTR);

    return CurBuffers;
}

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
