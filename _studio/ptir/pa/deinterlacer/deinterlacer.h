/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: api.h

\* ****************************************************************************** */

#ifndef DEINTERLACER_H
#define DEINTERLACER_H

#pragma once

#include "..\common.h"
#include <assert.h> 

#define    NEXTFIELD                6
#define PREVBUF                    7
#define    RSTBUF                    8
#define TEMPBUF                    9
#define BADMCBUF                10
#define EDGESBUF                11
#define    DIREDGEBUF                12
#define LASTFRAME                BUFMINSIZE + BUFEXTRASIZE

#ifndef MAX
/*! \def MAX(a,b)
*   \brief Defines maximum of two vlues
*
*  Defines a macro that return maximum of two vlues
*/    
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

#endif

__declspec(dllexport) __inline unsigned int __stdcall EDIError(BYTE* PrvLinePixel, BYTE* NxtLinePixel, int dir);
__declspec(dllexport) void __stdcall FilterMask_Main(Plane *s, Plane *d, unsigned int BotBase, int ybegin, int yend);
__declspec(dllexport) void __stdcall FillBaseLinesIYUV(Frame *pSrc, Frame* pDst, int BottomLinesBaseY, int BottomLinesBaseUV);
__declspec(dllexport) void __stdcall BilinearDeint(Frame *This, int BotBase);
__declspec(dllexport) void __stdcall MedianDeinterlace(Frame *This, int BotBase);
__declspec(dllexport) void __stdcall BuildLowEdgeMask_Main(Frame **frmBuffer, unsigned int frame, unsigned int BotBase);
__declspec(dllexport) void __stdcall CalculateEdgesIYUV(Frame **frmBuffer, unsigned int frame, int BotBase);
__declspec(dllexport) void __stdcall EdgeDirectionalIYUV_Main(Frame **frmBuffer, unsigned int curFrame, int BotBase);
__declspec(dllexport) void __stdcall DeinterlaceBorders(Frame **frmBuffer, unsigned int curFrame, int BotBase);
__declspec(dllexport) void __stdcall DeinterlaceBilinearFilter(Frame **frmBuffer, unsigned int curFrame, int BotBase);
__declspec(dllexport) void __stdcall DeinterlaceMedianFilter(Frame **frmBuffer, unsigned int curFrame, int BotBase);
#endif