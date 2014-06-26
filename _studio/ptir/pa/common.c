/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: common.c

\* ****************************************************************************** */
#include "api.h"


#define DEBUG_RS 0

#if defined(_WIN32) || defined(_WIN64)
void PrintOutputStats(HANDLE hStd,
                                CONSOLE_SCREEN_BUFFER_INFO sbInfo, 
                                unsigned int uiFrame, 
                                unsigned int uiStart, 
                                unsigned int uiCount, 
                                unsigned int *uiProgress,
                                unsigned int uiFrameOut,
                                unsigned int uiTimer,
                                FrameQueue fqIn,
                                const char **cOperations,
                                double *dTime)
{
    unsigned int i;
    double dTimeTotal;

    // Progress Bar
    SetConsoleCursorPosition(hStd, sbInfo.dwCursorPosition);
    SetConsoleTextAttribute(hStd, BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY);
    while (sbInfo.dwSize.X * (uiFrame - uiStart) / uiCount > *uiProgress)
    {
        printf(" ");
        ++*uiProgress;
    }
    if (*uiProgress != sbInfo.dwSize.X)
        printf("\n");
    SetConsoleTextAttribute(hStd, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    // Progress Text
    printf("\n%32s %9d\n", "Frame", uiFrameOut);
    printf("%32s %9d\n\n", "Queue Length", fqIn.iCount);
    dTimeTotal = 0;
    for (i = 0; i < uiTimer - 1; ++i)
    {
        printf("%32s %9.2lf mspf\n", cOperations[i], 1000.0 * dTime[i] / (uiFrameOut + 1));
        dTimeTotal += dTime[i];
    }
    printf("%32s %9.2lf mspf\n", "Total Time", 1000.0 * dTimeTotal / (uiFrameOut + 1));
    printf("\n");
}
#endif

void Plane_Create(Plane *pplaIn, unsigned int uiWidth, unsigned int uiHeight, unsigned int uiBorder)
{
    pplaIn->uiSize = (uiWidth + 2 * uiBorder) * (uiHeight + 2 * uiBorder);
    pplaIn->ucData = (unsigned char *)malloc(pplaIn->uiSize);

    if (pplaIn->ucData)
    {
        pplaIn->uiWidth = uiWidth;
        pplaIn->uiHeight = uiHeight;
        pplaIn->uiBorder = uiBorder;
        pplaIn->uiStride = uiWidth + 2 * uiBorder;
        pplaIn->ucCorner = pplaIn->ucData + (pplaIn->uiStride + 1) * pplaIn->uiBorder;
    }
    else
        pplaIn->uiSize = 0;
}
void Plane_Release(Plane *pplaIn)
{
    if (pplaIn->ucData)
        free(pplaIn->ucData);
}
void Frame_Create(Frame *pfrmIn, unsigned int uiYWidth, unsigned int uiYHeight, unsigned int uiUVWidth, unsigned int uiUVHeight, unsigned int uiBorder)
{
    unsigned int uiOffset;
    pfrmIn->uiSize = (uiYWidth + 2 * uiBorder) * (uiYHeight + 2 * uiBorder) + (2 * (uiUVWidth + 2 * uiBorder) * (uiUVHeight + 2 * uiBorder));
    pfrmIn->ucMem = (unsigned char *)malloc(pfrmIn->uiSize);
    
    if (pfrmIn->ucMem)
    {
        pfrmIn->frmProperties.drop = 0;
        pfrmIn->frmProperties.candidate = 0;
        pfrmIn->plaY.uiWidth = uiYWidth;
        pfrmIn->plaY.uiHeight = uiYHeight;
        pfrmIn->plaY.uiBorder = uiBorder;
        pfrmIn->plaY.uiSize = (uiYWidth + 2 * uiBorder) * (uiYHeight + 2 * uiBorder);
        pfrmIn->plaY.uiStride = uiYWidth + 2 * uiBorder;
        pfrmIn->plaY.ucData = pfrmIn->ucMem;
        pfrmIn->plaY.ucCorner = pfrmIn->plaY.ucData + (pfrmIn->plaY.uiStride + 1) * pfrmIn->plaY.uiBorder;

        uiOffset = pfrmIn->plaY.uiStride * (uiYHeight + 2 * uiBorder);

        pfrmIn->plaU.uiWidth = uiUVWidth;
        pfrmIn->plaU.uiHeight = uiUVHeight;
        pfrmIn->plaU.uiBorder = uiBorder;
        pfrmIn->plaU.uiSize = (uiUVWidth + 2 * uiBorder) * (uiUVHeight + 2 * uiBorder);
        pfrmIn->plaU.uiStride = uiUVWidth + 2 * uiBorder;
        pfrmIn->plaU.ucData = pfrmIn->ucMem + uiOffset;
        pfrmIn->plaU.ucCorner = pfrmIn->plaU.ucData + (pfrmIn->plaU.uiStride + 1) * pfrmIn->plaU.uiBorder;

        uiOffset += pfrmIn->plaU.uiStride * (uiUVHeight + 2 * uiBorder);

        pfrmIn->plaV.uiWidth = uiUVWidth;
        pfrmIn->plaV.uiHeight = uiUVHeight;
        pfrmIn->plaV.uiBorder = uiBorder;
        pfrmIn->plaV.uiSize = (uiUVWidth + 2 * uiBorder) * (uiUVHeight + 2 * uiBorder);
        pfrmIn->plaV.uiStride = uiUVWidth + 2 * uiBorder;
        pfrmIn->plaV.ucData = pfrmIn->ucMem + uiOffset;
        pfrmIn->plaV.ucCorner = pfrmIn->plaV.ucData + (pfrmIn->plaV.uiStride + 1) * pfrmIn->plaV.uiBorder;
    }
    else
        pfrmIn->uiSize = 0;

}
void Frame_Release(Frame *pfrmIn)
{
    if (pfrmIn->ucMem)
        free(pfrmIn->ucMem);
}
void FrameQueue_Initialize(FrameQueue *pfq)
{
    pfq->iCount = 0;
    pfq->pfrmHead = NULL;
    pfq->pfrmTail = NULL;
}
void FrameQueue_Add(FrameQueue *pfq, Frame *pfrm)
{
    FrameNode
        *pfn = (FrameNode *)malloc(sizeof(FrameNode));
    pfn->pfrmItem = pfrm;
    pfn->pfnNext = NULL;

    pfq->iCount++;
    if (pfq->pfrmHead == NULL)
    {
        pfq->pfrmHead = pfn;
        pfq->pfrmTail = pfn;
    }
    else
    {
        pfq->pfrmTail->pfnNext = pfn;
        pfq->pfrmTail = pfn;
    }
}
Frame * FrameQueue_Get(FrameQueue *pfq)
{
    Frame *pfrm = NULL;
    if (pfq->iCount > 0)
    {
        pfrm = pfq->pfrmHead->pfrmItem;
        pfq->pfrmHead = pfq->pfrmHead->pfnNext;
        pfq->iCount--;
    }
    return pfrm;
}
void FrameQueue_Release(FrameQueue *pfq)
{
    int i;
    FrameNode
        *pfn = pfq->pfrmHead;

    for (i = 0; i < pfq->iCount; ++i)
    {
        Frame_Release(pfn->pfrmItem);
        pfn = pfn->pfnNext;

        free(pfq->pfrmHead);
        pfq->pfrmHead = pfn;
    }
    pfq->iCount = 0;
    pfq->pfrmHead = NULL;
    pfq->pfrmTail = NULL;
}
void AddBorders(Plane *pplaIn, Plane *pplaOut, unsigned int uiBorder)
{
    unsigned int i;

    pplaOut->uiWidth = pplaIn->uiWidth;
    pplaOut->uiHeight = pplaIn->uiHeight;
    pplaOut->uiStride = pplaIn->uiWidth + 2 * uiBorder;
    pplaOut->uiBorder = uiBorder;

    // Top
    for (i = 0; i < pplaOut->uiBorder; ++i)
        memcpy(&pplaOut->ucData[i * pplaOut->uiStride + pplaOut->uiBorder], &pplaIn->ucData[0], pplaIn->uiWidth);

    // Center
    for (i = 0; i < pplaIn->uiHeight; ++i)
        memcpy(&pplaOut->ucData[(i + pplaOut->uiBorder) * pplaOut->uiStride + pplaOut->uiBorder], &pplaIn->ucData[i * pplaIn->uiWidth], pplaIn->uiWidth);

    // Bottom
    for (i = 0; i < pplaOut->uiBorder; ++i)
        memcpy(&pplaOut->ucData[(pplaOut->uiHeight + i + pplaOut->uiBorder) * pplaOut->uiStride + pplaOut->uiBorder], &pplaIn->ucData[pplaIn->uiWidth * (pplaIn->uiHeight - 1)], pplaIn->uiWidth);

    // Left
    for (i = 0; i < pplaOut->uiHeight + 2 * pplaOut->uiBorder; ++i)
        memset(&pplaOut->ucData[i * pplaOut->uiStride], pplaOut->ucData[i * pplaOut->uiStride + pplaOut->uiBorder], pplaOut->uiBorder);

    // Right
    for (i = 0; i < pplaOut->uiHeight + 2 * pplaOut->uiBorder; ++i)
        memset(&pplaOut->ucData[i * pplaOut->uiStride + pplaOut->uiWidth + pplaOut->uiBorder], pplaOut->ucData[i * pplaOut->uiStride + (pplaOut->uiWidth + pplaOut->uiBorder - 1)], pplaOut->uiBorder);
}
void TrimBorders(Plane *pplaIn, Plane *pplaOut)
{
    unsigned int i;

    pplaOut->uiWidth = pplaIn->uiWidth;
    pplaOut->uiHeight = pplaIn->uiHeight;
    pplaOut->uiStride = pplaIn->uiWidth;
    pplaOut->uiBorder = 0;

    for (i = 0; i < pplaOut->uiHeight; ++i)
        memcpy(&pplaOut->ucData[i * pplaOut->uiStride], &pplaIn->ucData[(i + pplaIn->uiBorder) * pplaIn->uiStride + pplaIn->uiBorder], pplaIn->uiWidth);
}
static void Convert_UYVY_to_I420(unsigned char *pucIn, Frame *pfrmOut)
{
    unsigned int i, j;

    memset(pfrmOut->ucMem, 128, pfrmOut->uiSize);

    for (j = 0; j < pfrmOut->plaY.uiHeight; ++j)
    {
        for (i = 0; i < pfrmOut->plaY.uiWidth; ++i)
        {
            int q = i + j * pfrmOut->plaY.uiStride;
            int q1 = 1 + 2 * q;
            
            pfrmOut->plaY.ucCorner[q] = pucIn[q1];
        }
    }

    for (j = 0; j < pfrmOut->plaU.uiHeight; ++j)
    {
        for (i = 0; i < pfrmOut->plaU.uiWidth; ++i)
        {
            int q = i + j * pfrmOut->plaU.uiStride;
            int q1 = 4 * (i + j * pfrmOut->plaY.uiStride);

            pfrmOut->plaU.ucCorner[q] = pucIn[q1];
            pfrmOut->plaV.ucCorner[q] = pucIn[q1 + 2];
        }
    }
}
void Convert_to_I420(unsigned char *pucIn, Frame *pfrmOut, char *pcFormat, double uiFrameRate)
{
    pfrmOut->frmProperties.fr = uiFrameRate;
    if (strcmp(pcFormat, "I420") == 0)
        memcpy(pfrmOut->ucMem, pucIn, pfrmOut->uiSize);
    else if (strcmp(pcFormat, "UYVY") == 0)
        Convert_UYVY_to_I420(pucIn, pfrmOut);
    else
        return;
}
void CheckGenFrame(Frame **pfrmIn, unsigned int frameNum, unsigned int patternType, unsigned int uiOPMode)
{
    BOOL stop = FALSE;
    if((pfrmIn[frameNum]->frmProperties.candidate || patternType == 0) && (uiOPMode == 0))
    {
        Rs_measurement(pfrmIn[frameNum]);
        pfrmIn[frameNum]->frmProperties.interlaced = FALSE;
        Artifacts_Detection_frame(pfrmIn,frameNum, FALSE);
        if(pfrmIn[frameNum]->frmProperties.interlaced)
        {
            FillBaseLinesIYUV(pfrmIn[frameNum], pfrmIn[BUFMINSIZE], FALSE, FALSE);
            DeinterlaceMedianFilter(pfrmIn, frameNum, FALSE);
        }
    }
}
void Prepare_frame_for_queue(Frame **pfrmOut, Frame *pfrmIn, unsigned int uiWidth, unsigned int uiHeight)
{
    *pfrmOut = (Frame *)malloc(sizeof(Frame));
    Frame_Create(*pfrmOut, uiWidth, uiHeight, uiWidth / 2, uiHeight / 2, 64);
    (*pfrmOut)->frmProperties.tindex = pfrmIn->frmProperties.tindex;
    ReSample(*pfrmOut, pfrmIn);
    (*pfrmOut)->frmProperties.tindex = pfrmIn->frmProperties.tindex;
}
