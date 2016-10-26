//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#ifndef PA_UTILITIES_H
#define PA_UTILITIES_H

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif

#include <stdio.h>
#include "pacm.h"
#include "../ptir_plugin/include/ptir_wrap.h"

#define TEST_UNDO2FRAMES_CM 1

void Interlaced_detection_logCM(Frame *frmBuffer[BUFMINSIZE], unsigned int uiNumFramesToDispatch);

#if TEST_UNDO2FRAMES_CM
void Undo2Frames_CMTest(Frame *frmBuffer1, Frame *frmBuffer2, BOOL BFF);
#endif

void Analyze_Buffer_Stats_CM(Frame *frmBuffer[BUFMINSIZE], Pattern *ptrn, unsigned int *pdispatch, unsigned int *uiisInterlaced);
void Analyze_Buffer_Stats_Automode_CM(Frame *frmBuffer[BUFMINSIZE], Pattern *ptrn, unsigned int *pdispatch);
void Detect_Solve_32BlendedPatternsCM(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch);
void UndoPatternTypes5and7CM(Frame *frmBuffer[BUFMINSIZE], unsigned int firstPos);
void Pattern32RemovalCM(Frame **frmBuffer, unsigned int uiInitFramePosition, unsigned int *pdispatch, unsigned int parity);
void Pattern41aRemovalCM(Frame **frmBuffer, unsigned int uiInitFramePosition, unsigned int *pdispatch, unsigned int parity);
void RemovePatternCM(Frame **frmBuffer, unsigned int uiPatternNumber, unsigned int uiInitFramePosition, unsigned int *pdispatch, unsigned int parity);
void CheckGenFrameCM(Frame **pfrmIn, unsigned int frameNum, /*unsigned int patternType, */unsigned int uiOPMode);
void Prepare_frame_for_queueCM(Frame **pfrmOut, Frame *pfrmIn, unsigned int uiWidth, unsigned int uiHeight, frameSupplier* frmSupply = 0, bool bCreate = false);
void Frame_CreateCM(Frame *pfrmIn, unsigned int uiYWidth, unsigned int uiYHeight, unsigned int uiUVWidth, unsigned int uiUVHeight, unsigned int uiBorder, bool bcreate = true);
void Frame_ReleaseCM(Frame *pfrmIn);

void Frame_Prep_and_AnalysisCM(unsigned char *pucIO, Frame **frmBuffer, char *pcFormat, double dFrameRate, unsigned int uiframeBufferIndexCur, unsigned int uiframeBufferIndexNext, unsigned int uiTemporalIndex);
void PTIRCM_Frame_Prep_and_Analysis(PTIRSystemBuffer *SysBuffer);

int  PTIRCM_AutoMode_FF(PTIRSystemBuffer *SysBuffer); /*Detection and processing of progressive, telecined and interlaced content.
                                                                         System returns full frame rate from interlaced content. Intended for variable frame rate encoding*/
int  PTIRCM_AutoMode_HF(PTIRSystemBuffer *SysBuffer); /*Detection and processing of progressive, telecined and interlaced content.
                                                                         System returns half frame rate from interlaced content. Intended for variable frame rate encoding*/
int  PTIRCM_DeinterlaceMode_FF(PTIRSystemBuffer *SysBuffer); /*Direct deinterlacing without content analysis. System returns full frame rate from deinterlacing process.
                                                                                Constant frame rate*/
int  PTIRCM_DeinterlaceMode_HF(PTIRSystemBuffer *SysBuffer); /*Direct deinterlacing without content analysis. System returns half frame rate from deinterlacing process.
                                                                                Constant frame rate*/
int  PTIRCM_BaseFrameMode(PTIRSystemBuffer *SysBuffer); /*Detection and processing of progressive, telecined and interlaced content.
                                                                           System returns same input framerate. Constant frame rate*/
int  PTIRCM_BaseFrameMode_NoChange(PTIRSystemBuffer *SysBuffer);
int  PTIRCM_Auto24fpsMode(PTIRSystemBuffer *SysBuffer);
int  PTIRCM_FixedTelecinePatternMode(PTIRSystemBuffer *SysBuffer);
int  PTIRCM_MultipleMode(PTIRSystemBuffer *SysBuffer, unsigned int uiOpMode);


void PTIRCM_Init(PTIRSystemBuffer *SysBuffer, unsigned int _uiInWidth, unsigned int _uiInHeight, double _dFrameRate, unsigned int _uiInterlaceParity, unsigned int _uiPatternTypeNumber, unsigned int _uiPatternTypeInit);
void PTIRCM_PutFrame(unsigned char *pucIO, PTIRSystemBuffer *SysBuffer, double dTimestamp);
void PTIRCM_Clean(PTIRSystemBuffer *SysBuffer);

#endif
