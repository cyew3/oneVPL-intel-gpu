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

#include "../common.h"
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

__inline unsigned int EDIError(BYTE* PrvLinePixel, BYTE* NxtLinePixel, int dir);
void FilterMask_Main(Plane *s, Plane *d, unsigned int BotBase, unsigned int ybegin, unsigned int yend);
void FillBaseLinesIYUV(Frame *pSrc, Frame* pDst, int BottomLinesBaseY, int BottomLinesBaseUV);
void BilinearDeint(Frame *This, int BotBase);
void MedianDeinterlace(Frame *This, int BotBase);
void BuildLowEdgeMask_Main(Frame **frmBuffer, unsigned int frame, unsigned int BotBase);
void CalculateEdgesIYUV(Frame **frmBuffer, unsigned int frame, int BotBase);
void EdgeDirectionalIYUV_Main(Frame **frmBuffer, unsigned int curFrame, int BotBase);
void DeinterlaceBorders(Frame **frmBuffer, unsigned int curFrame, int BotBase);
void DeinterlaceBilinearFilter(Frame **frmBuffer, unsigned int curFrame, int BotBase);
void DeinterlaceMedianFilter(Frame **frmBuffer, unsigned int curFrame, int BotBase);
#endif
