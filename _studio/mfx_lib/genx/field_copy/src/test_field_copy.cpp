/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#include "stdio.h"
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "../include/test_common.h"
#include "../include/genx_fcopy_cmcode_isa.h"

namespace {
    int RunGpu(const unsigned char *inData, unsigned char *outData, int fieldMask, CmDevice *device, CmKernel *kernel);
    int RunCpu(const unsigned char *inData, unsigned char *outData, int fieldMask);
};

enum {
    TFF2TFF = 0x0,
    TFF2BFF = 0x1,
    BFF2TFF = 0x2,
    BFF2BFF = 0x3,
    FIELD2TFF = 0x4,  // FIELD means half-sized frame w/ one field only
    FIELD2BFF = 0x5,
    TFF2FIELD = 0x6,
    BFF2FIELD = 0x7
};

char* fMaskStr[] = {
    "TFF2TFF",
    "TFF2BFF",
    "BFF2TFF",
    "BFF2BFF",
    "FIELD2TFF",  // FIELD means half-sized frame w/ one field only
    "FIELD2BFF",
    "TFF2FIELD",
    "BFF2FIELD"
};


int CheckYUVDiff(unsigned char* yuv0, unsigned char* yuv1, int h, int fmask)
{
    if (memcmp(yuv0, yuv1, WIDTH * h * 3 / 2)) {
        printf("ERROR in CheckDiff %s ", fMaskStr[fmask]);
        if (memcmp(yuv0, yuv1, WIDTH * h))
            printf("LUMA");
        if (memcmp(yuv0 + WIDTH * h, yuv1 + WIDTH * h, WIDTH * h / 2))
            printf(" CHROMA");
        printf(" !!!\n");
        return FAILED;
    }
    return PASSED;
}

int TestFieldCopy()
{
    unsigned char *buf_white  = new unsigned char[WIDTH * HEIGHT * 3 / 2];
    unsigned char *buf_black  = new unsigned char[WIDTH * HEIGHT * 3 / 2];
    unsigned char *buf_out_gpu  = new unsigned char[WIDTH * HEIGHT * 3 / 2];
    unsigned char *buf_out_cpu  = new unsigned char[WIDTH * HEIGHT * 3 / 2];
    int res = PASSED;

    FILE *foi_white = fopen(YUV_NAME_W, "rb");
    if (!foi_white)
        return FAILED;
    FILE *foi_black = fopen(YUV_NAME_B, "rb");
    if (!foi_black)
        return FAILED;
    

    FILE *fout = fopen("out_1fr.yuv", "wb");
    if (!fout)
        return FAILED;

    int bytesRead = fread(buf_white, 1, WIDTH * HEIGHT * 3 / 2, foi_white);
    if (bytesRead != WIDTH * HEIGHT * 3 / 2)
        return FAILED;
    fclose(foi_white);

    bytesRead = fread(buf_black, 1, WIDTH * HEIGHT * 3 / 2, foi_black);
    if (bytesRead != WIDTH * HEIGHT * 3 / 2)
        return FAILED;
    fclose(foi_black);

    unsigned int version = 0;
    CmDevice *device = 0;
    res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_fcopy_cmcode, sizeof(genx_fcopy_cmcode), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(MbCopyFieLd), kernel);  // MbCopyFieLd copies TOP or BOTTOM field from inSurf to OutSurf
    CHECK_CM_ERR(res);

    // initial output is black
    
    int failed = 0;
    int outHeight = HEIGHT;
    printf("\n");
    for (int fmask = TFF2TFF; fmask <= BFF2FIELD; fmask++)
    {
        if (fmask > FIELD2BFF)
            outHeight = HEIGHT / 2;
        printf("%s ", fMaskStr[fmask]);
        memcpy(buf_out_gpu, buf_black,  WIDTH * outHeight * 3 / 2);
        memcpy(buf_out_cpu, buf_black,  WIDTH * outHeight * 3 / 2);
        res = RunGpu(buf_white, buf_out_gpu, fmask, device, kernel);
        CHECK_ERR(res);
        res = RunCpu(buf_white, buf_out_cpu, fmask);
        CHECK_ERR(res);
///        fwrite(buf_out_gpu, 1, WIDTH * outHeight * 3 / 2, fout);
        if (CheckYUVDiff(buf_out_gpu, buf_out_cpu, outHeight, fmask))
            failed++;
        else
            printf("passed\n");
    }
    
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    ::DestroyCmDevice(device);

    fclose(fout);

    delete [] buf_white;
    delete [] buf_black;
    delete [] buf_out_gpu;
    delete [] buf_out_cpu;

    if (failed)
        return FAILED;

    return PASSED;
}

namespace {

int RunGpu(const unsigned char *inData, unsigned char *outData, int fieldMask, CmDevice *device, CmKernel *kernel)
{
    int res;
    CmSurface2D *input = 0;
    int inHeight = HEIGHT;
    int outHeight = HEIGHT;
    if ((fieldMask == FIELD2TFF) || (fieldMask == FIELD2BFF))
        inHeight /= 2;
    if ((fieldMask == BFF2FIELD) || (fieldMask == BFF2FIELD))
        outHeight /= 2;

    res = device->CreateSurface2D(WIDTH, inHeight, CM_SURFACE_FORMAT_NV12, input);
    CHECK_CM_ERR(res);
    res = input->WriteSurface(inData, NULL);
    CHECK_CM_ERR(res);

    CmSurface2D *output = 0;
    res = device->CreateSurface2D(WIDTH, outHeight, CM_SURFACE_FORMAT_NV12, output);
    CHECK_CM_ERR(res);
    res = output->WriteSurface(outData, NULL);
    CHECK_CM_ERR(res);

    unsigned int tsWidth = WIDTH / 16;
    unsigned int tsHeight = outHeight / 16;
    res = kernel->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);

    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInput = 0;
    SurfaceIndex *idxOutput = 0;
    res = input->GetIndex(idxInput);
    CHECK_CM_ERR(res);
    res = output->GetIndex(idxOutput);
    CHECK_CM_ERR(res);

    res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(*idxOutput), idxOutput);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(2, sizeof(int), &fieldMask);
    CHECK_CM_ERR(res);

    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);

    res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

    CmQueue *queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);

    CmEvent * e = 0;
    res = queue->Enqueue(task, e, threadSpace);
    if (res != CM_SUCCESS)
        printf("FAILED in Enqueue!!!\n");
    CHECK_CM_ERR(res);

    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);

    res = output->ReadSurface(outData, e);
    CHECK_CM_ERR(res);

    queue->DestroyEvent(e);
    device->DestroySurface(input);
    device->DestroySurface(output);

    return PASSED;
}

int RunCpu(const unsigned char *inData, unsigned char *outData, int fieldMask)
{
    int res;
    int inHeight = HEIGHT;
    int outHeight = HEIGHT;
    int inOff = 0;
    int outOff = 0;
    int outStep = 0;
    int inStep = 0;

    switch (fieldMask)
    {
        case 0: //TFF2TFF
            inOff = 0; inStep = 2; outOff = 0; outStep = 2;
            break;
        case 1: //TFF2BFF
            inOff = 0; inStep = 2; outOff = 1; outStep = 2;
            break;
        case 2: //BFF2TFF
            inOff = 1; inStep = 2; outOff = 0; outStep = 2;
            break;
        case 3: //BFF2BFF
            inOff = 1; inStep = 2; outOff = 1; outStep = 2;
            break;
        case 4: //FIELD2TFF
            inHeight /= 2;
            inOff = 0; inStep = 1; outOff = 0; outStep = 2;
            break;
        case 5: //FIELD2BFF
            inHeight /= 2;
            inOff = 0; inStep = 1; outOff = 1; outStep = 2;
            break;
        case 6: //TFF2FIELD
            outHeight /= 2;
            inOff = 0; inStep = 2; outOff = 0; outStep = 1;
            break;
        case 7: //BFF2FIELD
            outHeight /= 2;
            inOff = 1; inStep = 2; outOff = 0; outStep = 1;
            break;
        default:
            return FAILED;
    }

    // number of rows in IN and OUT is equal !!!
    int inR = 0, outR = 0;
    unsigned char *pInR, *pOutR;
    unsigned char *pInY = (unsigned char *)inData;
    unsigned char *pOutY = outData;
    unsigned char *pInUV = (unsigned char *)inData + WIDTH * inHeight;
    unsigned char *pOutUV = outData + WIDTH * outHeight;

    for (inR = inOff, outR = outOff; outR < outHeight; inR+=inStep, outR+=outStep) {
            pInR = pInY + inR * WIDTH;
            pOutR = pOutY + outR * WIDTH;
            memcpy(pOutR, pInR, WIDTH);
    }

    for (inR = inOff, outR = outOff; outR < outHeight / 2; inR+=inStep, outR+=outStep) {
        pInR = pInUV + inR * WIDTH;
        pOutR = pOutUV + outR * WIDTH;
        memcpy(pOutR, pInR, WIDTH);
    }

    return PASSED;
}

};
