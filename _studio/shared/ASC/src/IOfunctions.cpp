//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.
//
#include "../include/IOfunctions.h"
#include "ASCstructures.h"
#include "asc_defs.h"

using namespace ns_asc;

void TimeStart(ASCTime* timer) {
#if defined (_WIN32) || (_WIN64)
    QueryPerformanceFrequency(&timer->tFrequency);
    QueryPerformanceCounter(&timer->tStart);
#endif
}

void TimeStart(ASCTime* timer, int index) {
#if defined (_WIN32) || (_WIN64)
    QueryPerformanceCounter(&timer->tPause[index]);
#endif
}

void TimeStop(ASCTime* timer) {
#if defined (_WIN32) || (_WIN64)
    QueryPerformanceCounter(&timer->tStop);
#endif
}

mfxF64 CatchTime(ASCTime *timer, const char* message)
{
    mfxF64
        timeval = 0.0;
    timeval = TimeMeasurement(timer->tStart, timer->tStop, timer->tFrequency);
    ASC_PRINTF("%s %0.3f ms.\n", message, timeval);
    return timeval;
}

mfxF64 CatchTime(ASCTime *timer, int index, const char* message) {
#if defined (_WIN32) || (_WIN64)
    mfxF64
        timeval = 0.0;
    QueryPerformanceCounter(&timer->tPause[index]);
    timeval = TimeMeasurement(timer->tStart, timer->tPause[index], timer->tFrequency);
    ASC_PRINTF("%s %0.3f ms.\n", message, timeval);
    return timeval;
#else
    return 0.0;
#endif
}

mfxF64 CatchTime(ASCTime *timer, int indexInit, int indexEnd, const char* message) {
#if defined (_WIN32) || (_WIN64)
    mfxF64
        timeval = 0.0;
    QueryPerformanceCounter(&timer->tPause[indexEnd]);
    timeval = TimeMeasurement(timer->tPause[indexInit], timer->tPause[indexEnd], timer->tFrequency);
    ASC_PRINTF("%s %0.3f ms.\n", message, timeval);
    return timeval;
#else
    return 0.0;
#endif
}



void imageInit(ASCYUV *buffer) {
    memset(buffer, 0, sizeof(ASCYUV));
}

void nullifier(ASCimageData *Buffer) {
    memset(Buffer, 0, sizeof(ASCimageData));
}

void ImDetails_Init(ASCImDetails *Rdata) {
    memset(Rdata, 0, sizeof(ASCImDetails));
}

void ASCTSCstat_Init(ASCTSCstat **logic) {
    for(int i = 0; i < TSCSTATBUFFER; i++)
    {
        logic[i] = new ASCTSCstat;
        memset(logic[i],0,sizeof(ASCTSCstat));
    }
}

mfxI32 ASClogBase2aligned(mfxI32 number) {
    mfxI32 data = (mfxI32)(ceil((log((double)number) / LN2)));
    return (mfxI32)pow(2,(double)data);
}

void ASCU8AllocandSet(pmfxU8 *ImageLayer, mfxI32 imageSize) {
    *ImageLayer = (pmfxU8) CM_ALIGNED_MALLOC(imageSize,16);
    if(*ImageLayer == NULL)
        exit(MEMALLOCERRORU8);
    else
        memset(*ImageLayer,0,imageSize);
}

void PdYuvImage_Alloc(ASCYUV *pImage, mfxI32 dimVideoExtended) {
    mfxI32
        dimAligned;
    dimAligned = ASClogBase2aligned(dimVideoExtended);
    ASCU8AllocandSet(&pImage->data,dimAligned);
}

void PdMVector_Alloc(ASCMVector **MV, mfxI32 mvArraysize){
    mfxI32
        MVspace = sizeof(ASCMVector) * mvArraysize;
    *MV = new ASCMVector[MVspace];//(MVector*) malloc(MVspace);
    if(*MV == NULL)
        exit(MEMALLOCERRORMV);
    else
        memset(*MV,0,MVspace);
}

void PdRsCs_Alloc(pmfxU16 *RCs, mfxI32 mvArraysize) {
    mfxI32
        MVspace = sizeof(mfxU16) * mvArraysize;
    *RCs = (pmfxU16) malloc(MVspace);
    if(*RCs == NULL)
        exit(MEMALLOCERROR);
    else
        memset(*RCs,0,MVspace);
}

void PdSAD_Alloc(pmfxU16 *SAD, mfxI32 mvArraysize) {
    mfxI32
        MVspace = sizeof(mfxU16) * mvArraysize;
    *SAD = (pmfxU16) malloc(MVspace);
    if(*SAD == NULL)
        exit(MEMALLOCERROR);
    else
        memset(*SAD,0,MVspace);
}

void Pdmem_AllocGeneral(ASCimageData *Buffer, ASCImDetails Rval) {
    /*YUV frame mem allocation*/
    PdYuvImage_Alloc(&(Buffer->Image), Rval.Extended_Width * Rval.Extended_Height);
    Buffer->Image.Y = Buffer->Image.data + Rval.initial_point;

    /*Motion Vector array mem allocation*/
    PdMVector_Alloc(&(Buffer->pInteger),Rval.Blocks_in_Frame);

    /*RsCs array mem allocation*/
    PdRsCs_Alloc(&(Buffer->Cs),Rval.Pixels_in_Y_layer / 16);
    PdRsCs_Alloc(&(Buffer->Rs),Rval.Pixels_in_Y_layer / 16);
    PdRsCs_Alloc(&(Buffer->RsCs),Rval.Pixels_in_Y_layer / 16);

    /*SAD array mem allocation*/
    PdSAD_Alloc(&(Buffer->SAD),Rval.Blocks_in_Frame);
}

void Pdmem_disposeGeneral(ASCimageData *Buffer) {
    free(Buffer->Cs);
    Buffer->Cs = NULL;
    free(Buffer->Rs);
    Buffer->Rs = NULL;
    free(Buffer->RsCs);
    Buffer->RsCs = NULL;
    //free(Buffer->pInteger);
    delete[] Buffer->pInteger;
    Buffer->pInteger = NULL;
    free(Buffer->SAD);
    Buffer->SAD = NULL;
    Buffer->Image.Y = NULL;
    Buffer->Image.U = NULL;
    Buffer->Image.V = NULL;
    CM_ALIGNED_FREE(Buffer->Image.data);
    Buffer->Image.data = NULL;

}