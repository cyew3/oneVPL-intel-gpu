//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//
#ifndef _IOFUNCTIONS_H_
#define _IOFUNCTIONS_H_

#include "ASCstructures.h"
#include "asc.h"
using namespace ns_asc;

#pragma warning(disable:4505)
static mfxF64 TimeMeasurement(LARGE_INTEGER start, LARGE_INTEGER stop, LARGE_INTEGER frequency) {
    return((stop.QuadPart - start.QuadPart) * mfxF64(1000.0) / frequency.QuadPart);
}
#pragma warning(default:4505)

void TimeStart(ASCTime* timer);
void TimeStart(ASCTime* timer, int index);
void TimeStop(ASCTime* timer);
mfxF64 CatchTime(ASCTime *timer, const char* message);
mfxF64 CatchTime(ASCTime *timer, int index, const char* message);
mfxF64 CatchTime(ASCTime *timer, int indexInit, int indexEnd, const char* message);

void imageInit(ASCYUV *buffer);
void nullifier(ASCimageData *Buffer);
void ImDetails_Init(ASCImDetails *Rdata);
void ASCTSCstat_Init(ASCTSCstat **logic);


mfxI32 ASClogBase2aligned(mfxI32 number);
void ASCU8AllocandSet(pmfxU8 *ImageLayer, mfxI32 imageSize);
void PdYuvImage_Alloc(ASCYUV *pImage, mfxI32 dimVideoExtended);
void PdMVector_Alloc(ASCMVector **MV, mfxI32 mvArraysize);
void PdRsCs_Alloc(pmfxF32 *RCs, mfxI32 mvArraysize);
void PdSAD_Alloc(pmfxU32 *SAD, mfxI32 mvArraysize);
void Pdmem_AllocGeneral(ASCimageData *Buffer, ASCImDetails Rval);
void Pdmem_disposeGeneral(ASCimageData *Buffer);


#endif //_IOFUNCTIONS_H_