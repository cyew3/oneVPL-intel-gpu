/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: api.h

\* ****************************************************************************** */

#ifndef TELECINE_H
#define TELECINE_H

#include "..\api.h"

// Inverse telecine and deinterlacer
#define BUFMINSIZE                6
#define BUFEXTRASIZE            7
#define    T1                        1.01
#define    T2                        3
#define    T3                        1.03
#define    T4                        1.08
#define NABS(a)                    ((a<0)?-a:a)
#define TVBLACK                    16
#define    BLENDEDOFF                0.375


// SAD
#define PASAD(a,b)                ((a>b)?a-b:b-a)
#define    InterStraightTop        0
#define    InterStraightBottom        1
#define    InterCrossTop            2
#define    InterCrossBottom        3
#define    IntraSAD                4


__declspec(dllexport) void __stdcall Pattern_init(Pattern *ptrn);

__declspec(dllexport) void __stdcall Rotate_Buffer(Frame *frmBuffer[BUFMINSIZE]);
__declspec(dllexport) void __stdcall Rotate_Buffer_borders(Frame *frmBuffer[BUFMINSIZE], unsigned int LastInLatch);
__declspec(dllexport) void __stdcall Rotate_Buffer_deinterlaced(Frame *frmBuffer[BUFMINSIZE]);
__declspec(dllexport) double __stdcall SSAD8x2(unsigned char *line1, unsigned char *line2, unsigned int offset);
__declspec(dllexport) void __stdcall Rs_measurement(Frame *pfrmIn);
__declspec(dllexport) void __stdcall setLinePointers(unsigned char *pLine[10], Frame pfrmIn, int offset, BOOL lastStripe);
__declspec(dllexport) void __stdcall Line_rearrangement(unsigned char *pFrmDstTop,unsigned char *pFrmDstBottom, unsigned char **pucIn, Plane planeOut,unsigned int *off);
__declspec(dllexport) void __stdcall Extract_Fields_I420(unsigned char *pucLine, Frame *pfrmOut, BOOL TopFieldFirst);

__declspec(dllexport) void __stdcall sadCalc_I420_frame(Frame *pfrmCur, Frame *pfrmPrv);

__declspec(dllexport) unsigned int __stdcall Artifacts_Detection(Frame **pFrm);
__declspec(dllexport) void __stdcall Artifacts_Detection_frame(Frame **pFrm, unsigned int frameNum, unsigned int firstRun);
__declspec(dllexport) void __stdcall Detect_Solve_32BlendedPatterns(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch);
__declspec(dllexport) void __stdcall Detect_Solve_32Patterns(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch);
__declspec(dllexport) void __stdcall Detect_Solve_3223Patterns(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch);
__declspec(dllexport) void __stdcall Detect_Interlacing_Artifacts_fast(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch);
__declspec(dllexport) double __stdcall CalcSAD_Avg_NoSC(Frame **pFrm);
__declspec(dllexport) void __stdcall Detect_32Pattern_rigorous(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch);

__declspec(dllexport) void __stdcall UndoPatternTypes5and7(Frame *frmBuffer[BUFMINSIZE], unsigned int firstPos);
__declspec(dllexport) void __stdcall Undo2Frames(Frame *frmBuffer1, Frame *frmBuffer2, BOOL BFF);

__declspec(dllexport) void __stdcall Analyze_Buffer_Stats(Frame *frmBuffer[BUFMINSIZE], Pattern *ptrn, unsigned int *pdispatch, unsigned int *uiisInterlaced);




#endif