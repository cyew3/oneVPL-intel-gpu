/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: ptir_vpp_plugin.h

\* ****************************************************************************** */

#ifndef PA_UTILITIES_H
#define PA_UTILITIES_H

#include <Windows.h>
#include <stdio.h>
#include "pacm.h"
#include "..\ptir_plugin\include\ptir_wrap.h"

#define TEST_UNDO2FRAMES_CM 1

#if TEST_UNDO2FRAMES_CM
void Undo2Frames_CMTest(Frame *frmBuffer1, Frame *frmBuffer2, BOOL BFF);
#endif

void Analyze_Buffer_Stats_CM(Frame *frmBuffer[BUFMINSIZE], Pattern *ptrn, unsigned int *pdispatch, unsigned int *uiisInterlaced);
void Detect_Solve_32BlendedPatternsCM(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch);
void UndoPatternTypes5and7CM(Frame *frmBuffer[BUFMINSIZE], unsigned int firstPos);
void CheckGenFrameCM(Frame **pfrmIn, unsigned int frameNum, unsigned int patternType, unsigned int uiOPMode);
void Prepare_frame_for_queueCM(Frame **pfrmOut, Frame *pfrmIn, unsigned int uiWidth, unsigned int uiHeight, frameSupplier* frmSupply = 0, bool bCreate = false);
void Frame_CreateCM(Frame *pfrmIn, unsigned int uiYWidth, unsigned int uiYHeight, unsigned int uiUVWidth, unsigned int uiUVHeight, unsigned int uiBorder, bool bcreate = true);
void Frame_ReleaseCM(Frame *pfrmIn);

#endif
