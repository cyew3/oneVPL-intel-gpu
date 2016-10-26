//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include "api.h"

#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

#if 0
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
#if !PRINTDEBUG
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
#endif
}
#endif

void PrintOutputStats_PvsI(unsigned long ulNumberofInterlacedFrames, unsigned long ulTotalNumberofFramesProcessed, const char **cSummary)
{
#if !PRINTDEBUG
    unsigned long ulDifference = 0;
    unsigned int uiMessage = 5;
    if(ulNumberofInterlacedFrames <= ulTotalNumberofFramesProcessed)
    {
        ulDifference = (ulTotalNumberofFramesProcessed - ulNumberofInterlacedFrames);
        if(ulDifference <= ulTotalNumberofFramesProcessed * 0.02)
            uiMessage = 4;
        else if(ulDifference <= ulTotalNumberofFramesProcessed * 0.49)
            uiMessage = 3;
        else if(ulDifference <= ulTotalNumberofFramesProcessed * 0.98)
            uiMessage = 2;
        else
            uiMessage = 1;
    }

#if THOMSONMODE
    printf("%32s      %s\n",cSummary[0], cSummary[uiMessage]);
#else
    printf("%32s      %s\t%lu\t%lu\n",cSummary[0], cSummary[uiMessage], ulNumberofInterlacedFrames, ulTotalNumberofFramesProcessed);
#endif
#endif
}
#endif

void* aligned_malloc(size_t required_bytes, size_t alignment)
{
    void* p1;
    void** p2;
    size_t offset = alignment - 1 + sizeof(void*);
    if ((p1 = (void*)malloc(required_bytes + offset)) == NULL)
        return NULL;
    p2 = (void**)(((size_t)(p1) + offset) & ~(alignment - 1));
    p2[-1] = p1;
    return p2;
}
 
void aligned_free(void *p)
{
    free(((void**)p)[-1]);
}


void Plane_Create(Plane *pplaIn, unsigned int uiWidth, unsigned int uiHeight, unsigned int uiBorder)
{
    pplaIn->uiSize = (uiWidth + (2 * uiBorder)) * (uiHeight + (2 * uiBorder));
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
    //pfrmIn->uiSize = (uiYWidth + 2 * uiBorder) * (uiYHeight + 2 * uiBorder) + (2 * (uiUVWidth + (2 * uiBorder)) * (uiUVHeight + (2 * uiBorder)));
    pfrmIn->uiSize = ((uiYWidth + 2 * uiBorder) * (uiYHeight + 2 * uiBorder)) + ((uiUVWidth + 2 * uiBorder) * (uiUVHeight + 2 * uiBorder)) + ((uiUVWidth + 2 * uiBorder) * (uiUVHeight + 2 * uiBorder));
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
        //pfrmIn->plaY.ucCorner = pfrmIn->plaY.ucData + (pfrmIn->plaY.uiStride + 1) * pfrmIn->plaY.uiBorder;
        pfrmIn->plaY.ucCorner = pfrmIn->plaY.ucData + (pfrmIn->plaY.uiStride * pfrmIn->plaY.uiBorder) + pfrmIn->plaY.uiBorder;
        

        uiOffset = pfrmIn->plaY.uiStride * (uiYHeight + 2 * uiBorder);
        memset( pfrmIn->plaY.ucData, 0 , uiOffset);

        pfrmIn->plaU.uiWidth = uiUVWidth;
        pfrmIn->plaU.uiHeight = uiUVHeight;
        pfrmIn->plaU.uiBorder = uiBorder;
        pfrmIn->plaU.uiSize = (uiUVWidth + 2 * uiBorder) * (uiUVHeight + 2 * uiBorder);
        pfrmIn->plaU.uiStride = uiUVWidth + 2 * uiBorder;
        pfrmIn->plaU.ucData = pfrmIn->ucMem + uiOffset;
        //pfrmIn->plaU.ucCorner = pfrmIn->plaU.ucData + (pfrmIn->plaU.uiStride + 1) * pfrmIn->plaU.uiBorder;
        pfrmIn->plaU.ucCorner = pfrmIn->plaU.ucData + (pfrmIn->plaU.uiStride * pfrmIn->plaU.uiBorder) + pfrmIn->plaU.uiBorder;

        uiOffset += pfrmIn->plaU.uiStride * (uiUVHeight + 2 * uiBorder);
        memset(pfrmIn->plaU.ucData, 0 , pfrmIn->plaU.uiStride * (uiUVHeight + 2 * uiBorder));

        pfrmIn->plaV.uiWidth = uiUVWidth;
        pfrmIn->plaV.uiHeight = uiUVHeight;
        pfrmIn->plaV.uiBorder = uiBorder;
        pfrmIn->plaV.uiSize = (uiUVWidth + 2 * uiBorder) * (uiUVHeight + 2 * uiBorder);
        pfrmIn->plaV.uiStride = uiUVWidth + 2 * uiBorder;
        pfrmIn->plaV.ucData = pfrmIn->ucMem + uiOffset;
        //pfrmIn->plaV.ucCorner = pfrmIn->plaV.ucData + (pfrmIn->plaV.uiStride + 1) * pfrmIn->plaV.uiBorder;
        pfrmIn->plaV.ucCorner = pfrmIn->plaV.ucData + (pfrmIn->plaV.uiStride * pfrmIn->plaV.uiBorder) + pfrmIn->plaV.uiBorder;
        memset(pfrmIn->plaV.ucData, 0 , pfrmIn->plaV.uiStride * (uiUVHeight + 2 * uiBorder));
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
    FrameNode  *pfn = 0;
    if(!pfq || !pfrm)
        return;
    pfn = (FrameNode *)malloc(sizeof(FrameNode));
    if(!pfn)
        return;
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
    FrameNode *node = NULL;
    if (pfq->iCount > 0)
    {
        node = pfq->pfrmHead;
        pfrm = pfq->pfrmHead->pfrmItem;
        pfq->pfrmHead = pfq->pfrmHead->pfnNext;
        pfq->iCount--;
    }
    if(node)
        free(node);
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
        ptir_memcpy(&pplaOut->ucData[i * pplaOut->uiStride + pplaOut->uiBorder], &pplaIn->ucData[0], pplaIn->uiWidth);

    // Center
    for (i = 0; i < pplaIn->uiHeight; ++i)
        ptir_memcpy(&pplaOut->ucData[(i + pplaOut->uiBorder) * pplaOut->uiStride + pplaOut->uiBorder], &pplaIn->ucData[i * pplaIn->uiWidth], pplaIn->uiWidth);

    // Bottom
    for (i = 0; i < pplaOut->uiBorder; ++i)
        ptir_memcpy(&pplaOut->ucData[(pplaOut->uiHeight + i + pplaOut->uiBorder) * pplaOut->uiStride + pplaOut->uiBorder], &pplaIn->ucData[pplaIn->uiWidth * (pplaIn->uiHeight - 1)], pplaIn->uiWidth);

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
        ptir_memcpy(&pplaOut->ucData[i * pplaOut->uiStride], &pplaIn->ucData[(i + pplaIn->uiBorder) * pplaIn->uiStride + pplaIn->uiBorder], pplaIn->uiWidth);
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

unsigned int Convert_to_I420(unsigned char *pucIn, Frame *pfrmOut, char *pcFormat, double dFrameRate)
{
    pfrmOut->frmProperties.fr = dFrameRate;
    if (strcmp(pcFormat, "I420") == 0)
    {
        ptir_memcpy(pfrmOut->ucMem, pucIn, pfrmOut->uiSize);
        return 1;
    }
    else if (strcmp(pcFormat, "UYVY") == 0)
    {
        Convert_UYVY_to_I420(pucIn, pfrmOut);
        return 1;
    }
    else
        return 0;
}

void Clean_Frame_Info(Frame **frmBuffer)
{
    unsigned int i;

    for(i = 0; i < BUFMINSIZE - 1; i++)
        frmBuffer[i]->frmProperties.interlaced = 0;
}

void Frame_Prep_and_Analysis(Frame **frmBuffer, char *pcFormat, double dFrameRate, unsigned int uiframeBufferIndexCur, unsigned int uiframeBufferIndexNext, unsigned int uiTemporalIndex)
{
#if PRINTDEBUG
    double    *dSAD,
            *dRs;
    unsigned int dPicSize;

    dPicSize = frmBuffer[uiframeBufferIndexCur]->plaY.uiWidth * frmBuffer[uiframeBufferIndexCur]->plaY.uiHeight;
#endif

    ptir_memcpy(frmBuffer[uiframeBufferIndexCur]->ucMem, frmBuffer[BUFMINSIZE]->ucMem, frmBuffer[BUFMINSIZE]->uiSize);
    frmBuffer[uiframeBufferIndexCur]->frmProperties.tindex = uiTemporalIndex + 1;
    sadCalc_I420_frame(frmBuffer[uiframeBufferIndexCur],frmBuffer[uiframeBufferIndexNext]);
    Rs_measurement(frmBuffer[uiframeBufferIndexCur]);
    frmBuffer[uiframeBufferIndexCur]->frmProperties.processed = TRUE;

    if(uiframeBufferIndexCur == uiframeBufferIndexNext)
    {
        frmBuffer[uiframeBufferIndexCur]->plaY.ucStats.ucSAD[0] = 99.999;
        frmBuffer[uiframeBufferIndexCur]->plaY.ucStats.ucSAD[1] = 99.999;
    }

#if PRINTDEBUG
    dSAD = frmBuffer[uiframeBufferIndexCur]->plaY.ucStats.ucSAD;
    dRs = frmBuffer[uiframeBufferIndexCur]->plaY.ucStats.ucRs;
    printf("%i\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\n", uiTemporalIndex + 1, dSAD[0], dSAD[1], dSAD[2], dSAD[3], dSAD[4], dRs[0], dRs[1], dRs[2], dRs[3], dRs[4], dRs[5] / dPicSize * 1000, dRs[6], dRs[7] / dPicSize * 1000, dRs[8], dRs[9], dRs[10]);
#endif
}


void CheckGenFrame(Frame **pfrmIn, unsigned int frameNum, unsigned int uiOPMode)
{
    BOOL stop = FALSE;
    if ((pfrmIn[frameNum]->frmProperties.candidate) && (uiOPMode == 0 || uiOPMode == 3))
    {
        Rs_measurement(pfrmIn[frameNum]);
        pfrmIn[frameNum]->frmProperties.interlaced = FALSE;
        Artifacts_Detection_frame(pfrmIn,frameNum);
        if(pfrmIn[frameNum]->frmProperties.interlaced)
        {
            FillBaseLinesIYUV(pfrmIn[frameNum], pfrmIn[BUFMINSIZE], FALSE, FALSE);
            DeinterlaceMedianFilter(pfrmIn, frameNum, BUFMINSIZE, FALSE);
        }
    }
}
void Prepare_frame_for_queue(Frame **pfrmOut, Frame *pfrmIn, unsigned int uiWidth, unsigned int uiHeight)
{
    if(!pfrmOut)
        return;
    *pfrmOut = (Frame *)malloc(sizeof(Frame));
    if(!*pfrmOut)
        return;
    Frame_Create(*pfrmOut, uiWidth, uiHeight, uiWidth / 2, uiHeight / 2, 0);
    (*pfrmOut)->frmProperties.tindex = pfrmIn->frmProperties.tindex;
    (*pfrmOut)->frmProperties.tindex = pfrmIn->frmProperties.detection;
    ReSample(*pfrmOut, pfrmIn);
    (*pfrmOut)->frmProperties.tindex = pfrmIn->frmProperties.tindex;
    (*pfrmOut)->frmProperties.tindex = pfrmIn->frmProperties.detection;
}

double Calculate_Resulting_timestamps(Frame** frmBuffer, unsigned int uiDispatch, unsigned int uiCur, double dBaseTime, unsigned int *uiNumFramesToDispatch, unsigned int PatternType, unsigned int uiEndOfFrames)
{
    unsigned int uiNumFramesToDrop,
                 i;

    double dCurrentTimeStart,
           dCurrentTimeEnd,
           dCurrentFrameDuration,
           dTimePerFrame;

    if(!uiEndOfFrames)
        *uiNumFramesToDispatch = min(uiDispatch, uiCur + 1);
    else {
        //*uiNumFramesToDispatch = min(uiDispatch, uiCur);
        *uiNumFramesToDispatch = uiCur;
    }

    uiNumFramesToDrop = 0;
    for (i = 0; i < *uiNumFramesToDispatch; i++)
        uiNumFramesToDrop += frmBuffer[i]->frmProperties.drop;

    if ((*uiNumFramesToDispatch - uiNumFramesToDrop) > 2) {
        dCurrentFrameDuration = frmBuffer[1]->frmProperties.timestamp - frmBuffer[0]->frmProperties.timestamp;
        dCurrentTimeStart = frmBuffer[0]->frmProperties.timestamp;
        //dCurrentTimeEnd = frmBuffer[*uiNumFramesToDispatch - 1]->frmProperties.tindex * dBaseTime;
        dCurrentTimeEnd = frmBuffer[*uiNumFramesToDispatch - 1]->frmProperties.timestamp;
        dTimePerFrame = (dCurrentTimeEnd - dCurrentTimeStart + dCurrentFrameDuration) / ((double)(*uiNumFramesToDispatch - uiNumFramesToDrop));
    } else {
        if ((*uiNumFramesToDispatch - uiNumFramesToDrop) == 2) {
            dTimePerFrame = frmBuffer[1]->frmProperties.timestamp - frmBuffer[0]->frmProperties.timestamp;
        } else
            dTimePerFrame =  dBaseTime;
    }

    return dTimePerFrame;
}

void Update_Frame_BufferNEW(Frame** frmBuffer, unsigned int frameIndex, double dCurTimeStamp, double dTimePerFrame, unsigned int uiisInterlaced, unsigned int uiInterlaceParity, unsigned int bFullFrameRate, Frame* frmIn, FrameQueue *fqIn)
{
    if (frmBuffer[frameIndex]->frmProperties.interlaced)
    {
        FillBaseLinesIYUV(frmBuffer[frameIndex], frmBuffer[BUFMINSIZE], uiInterlaceParity, uiInterlaceParity);
        //First field
        DeinterlaceMedianFilter(frmBuffer, frameIndex, BUFMINSIZE, uiInterlaceParity);
        //Second field
        if(bFullFrameRate)
            DeinterlaceMedianFilter(frmBuffer, BUFMINSIZE, frameIndex, !uiInterlaceParity);
     }
     else
         CheckGenFrame(frmBuffer, frameIndex, uiisInterlaced);

    Prepare_frame_for_queue(&frmIn,frmBuffer[frameIndex], frmBuffer[frameIndex]->plaY.uiWidth, frmBuffer[frameIndex]->plaY.uiHeight);
    if(!frmIn)
        return;
    ptir_memcpy(frmIn->plaY.ucStats.ucRs,frmBuffer[frameIndex]->plaY.ucStats.ucRs,sizeof(double) * 10);

    //Timestamp
    frmIn->frmProperties.timestamp = dCurTimeStamp;

    FrameQueue_Add(fqIn, frmIn);

    if (bFullFrameRate && uiisInterlaced == 1)
    {
        Prepare_frame_for_queue(&frmIn, frmBuffer[BUFMINSIZE], frmBuffer[frameIndex]->plaY.uiWidth, frmBuffer[frameIndex]->plaY.uiHeight); // Go to double frame rate
        if(!frmIn)
            return;
        ptir_memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[BUFMINSIZE]->plaY.ucStats.ucRs, sizeof(double)* 10);

        //Timestamp
        frmIn->frmProperties.timestamp = dCurTimeStamp + dTimePerFrame / 2;

        FrameQueue_Add(fqIn, frmIn);
    }

    frmBuffer[frameIndex]->frmProperties.drop = FALSE;
    frmBuffer[frameIndex]->frmProperties.candidate = FALSE;
}

void Update_Frame_Buffer(Frame** frmBuffer, unsigned int frameIndex, double dTimePerFrame, unsigned int uiisInterlaced, unsigned int uiInterlaceParity, unsigned int bFullFrameRate, Frame* frmIn, FrameQueue *fqIn)
{
    if (frmBuffer[frameIndex]->frmProperties.interlaced)
    {
        FillBaseLinesIYUV(frmBuffer[frameIndex], frmBuffer[BUFMINSIZE], uiInterlaceParity, uiInterlaceParity);
        //First field
        DeinterlaceMedianFilter(frmBuffer, frameIndex, BUFMINSIZE, uiInterlaceParity);
        //Second field
        if(bFullFrameRate)
            DeinterlaceMedianFilter(frmBuffer, BUFMINSIZE, frameIndex, !uiInterlaceParity);
     }
     else
         CheckGenFrame(frmBuffer, frameIndex, uiisInterlaced);

    Prepare_frame_for_queue(&frmIn,frmBuffer[frameIndex], frmBuffer[frameIndex]->plaY.uiWidth, frmBuffer[frameIndex]->plaY.uiHeight);
    if(!frmIn)
        return;
    ptir_memcpy(frmIn->plaY.ucStats.ucRs,frmBuffer[frameIndex]->plaY.ucStats.ucRs,sizeof(double) * 10);
           
    //Timestamp
    if (frmBuffer[frameIndex]->frmProperties.interlaced && bFullFrameRate == 1 && uiisInterlaced == 1)
    {
        frmBuffer[BUFMINSIZE]->frmProperties.tindex = frmBuffer[frameIndex]->frmProperties.tindex;
        frmBuffer[BUFMINSIZE]->frmProperties.timestamp = frmBuffer[frameIndex]->frmProperties.timestamp + dTimePerFrame;
        frmBuffer[frameIndex + 1]->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp + dTimePerFrame;
    }
    else
        frmBuffer[frameIndex + 1]->frmProperties.timestamp = frmBuffer[frameIndex]->frmProperties.timestamp + dTimePerFrame;

    frmIn->frmProperties.timestamp = frmBuffer[frameIndex]->frmProperties.timestamp;

    FrameQueue_Add(fqIn, frmIn);

    if (bFullFrameRate && uiisInterlaced == 1)
    {
        Prepare_frame_for_queue(&frmIn, frmBuffer[BUFMINSIZE], frmBuffer[frameIndex]->plaY.uiWidth, frmBuffer[frameIndex]->plaY.uiHeight); // Go to double frame rate
        if(!frmIn)
            return;
        ptir_memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[BUFMINSIZE]->plaY.ucStats.ucRs, sizeof(double)* 10);

        //Timestamp
        frmIn->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp;

        FrameQueue_Add(fqIn, frmIn);
    }
    
    frmBuffer[frameIndex]->frmProperties.drop = FALSE;
    frmBuffer[frameIndex]->frmProperties.candidate = FALSE;
}

// timestamp in msecs
int PTIR_PutFrame(unsigned char *pucIO, PTIRSystemBuffer *SysBuffer, double dTimestamp)
{
    unsigned int status = 0;

    //SysBuffer->control.uiNext = SysBuffer->control.uiCur - 1;

    SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.timestamp = dTimestamp;
    status = Convert_to_I420(pucIO, SysBuffer->frmBuffer[BUFMINSIZE], "I420", SysBuffer->control.dFrameRate);
    if(status)
        SysBuffer->control.uiFrameCount += status;
    else
        return(-1000);            // error

    //SysBuffer->control.uiFrame++;

    return 0;                    // success
}

void PTIR_Frame_Prep_and_Analysis(PTIRSystemBuffer *SysBuffer)
{
#if PRINTDEBUG
    double    *dSAD,
            *dRs;
    unsigned int dPicSize;

    dPicSize = SysBuffer->frmBuffer[SysBuffer->control.uiCur]->plaY.uiWidth * SysBuffer->frmBuffer[SysBuffer->control.uiCur]->plaY.uiHeight;
#endif
    if(SysBuffer->control.uiCur == 0)
    {
        SysBuffer->control.uiNext = 0;
        SysBuffer->control.uiFrame = 0;
    }

    ptir_memcpy(SysBuffer->frmBuffer[SysBuffer->control.uiCur]->ucMem, SysBuffer->frmBuffer[BUFMINSIZE]->ucMem, SysBuffer->frmBuffer[BUFMINSIZE]->uiSize);
    SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.tindex = SysBuffer->control.uiFrame + 1;
    sadCalc_I420_frame(SysBuffer->frmBuffer[SysBuffer->control.uiCur],SysBuffer->frmBuffer[SysBuffer->control.uiNext]);
    Rs_measurement(SysBuffer->frmBuffer[SysBuffer->control.uiCur]);
    Artifacts_Detection_frame(SysBuffer->frmBuffer, SysBuffer->control.uiCur);
    SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.processed = TRUE;

    if(SysBuffer->control.uiCur == SysBuffer->control.uiNext)
    {
        SysBuffer->frmBuffer[SysBuffer->control.uiCur]->plaY.ucStats.ucSAD[0] = 99.999;
        SysBuffer->frmBuffer[SysBuffer->control.uiCur]->plaY.ucStats.ucSAD[1] = 99.999;
    }

#if PRINTDEBUG
    dSAD = SysBuffer->frmBuffer[SysBuffer->control.uiCur]->plaY.ucStats.ucSAD;
    dRs = SysBuffer->frmBuffer[SysBuffer->control.uiCur]->plaY.ucStats.ucRs;
    printf("%i\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\n", SysBuffer->control.uiFrame + 1, dSAD[0], dSAD[1], dSAD[2], dSAD[3], dSAD[4], dRs[0], dRs[1], dRs[2], dRs[3], dRs[4], dRs[5] / dPicSize * 1000, dRs[6], dRs[7] / dPicSize * 1000, dRs[8], dRs[9], dRs[10]);
#endif
    //Frame_Prep_and_Analysis(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    SysBuffer->control.uiCur++;
}

int PTIR_AutoMode_FF(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiDeinterlace = 0,
                 uiNumFramesToDispatch;
    double cur_timestamp;

    if(SysBuffer->control.uiEndOfFrames)
    {
        if(SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.tindex != SysBuffer->control.uiFrame)
            SysBuffer->control.uiCur = max(1, SysBuffer->control.uiCur - 1);
    }

    Frame_Prep_and_Analysis(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Analyze_Buffer_Stats_Automode(SysBuffer->frmBuffer, &SysBuffer->control.mainPattern, &SysBuffer->control.uiDispatch);
        if(SysBuffer->control.mainPattern.ucPatternFound)
        {
            SysBuffer->control.dTimePerFrame = Calculate_Resulting_timestamps(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch, SysBuffer->control.uiCur, SysBuffer->control.dBaseTime, &uiNumFramesToDispatch, SysBuffer->control.mainPattern.ucPatternType, SysBuffer->control.uiEndOfFrames);
            cur_timestamp = SysBuffer->frmBuffer[0]->frmProperties.timestamp;

            for (i = 0; i < uiNumFramesToDispatch; i++)
            {
                if (!SysBuffer->frmBuffer[i]->frmProperties.drop)
                {
                    Update_Frame_BufferNEW(SysBuffer->frmBuffer, i, cur_timestamp, SysBuffer->control.dTimePerFrame, AUTOMODE, SysBuffer->control.uiInterlaceParity, FULLFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                    cur_timestamp += SysBuffer->control.dTimePerFrame;
                }
                else
                    SysBuffer->frmBuffer[i]->frmProperties.drop = FALSE;
            }
            Rotate_Buffer_borders(SysBuffer->frmBuffer, uiNumFramesToDispatch);
            SysBuffer->control.uiCur -= min(uiNumFramesToDispatch,SysBuffer->control.uiCur);
        }
        else
        {
            uiDeinterlace = SysBuffer->control.mainPattern.uiOverrideHalfFrameRate;

            if(!SysBuffer->control.uiEndOfFrames)
                uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur + 1);
            else
                uiNumFramesToDispatch = SysBuffer->control.uiCur;

            for(i = 0; i < uiNumFramesToDispatch; i++)
            {
                Update_Frame_BufferNEW(SysBuffer->frmBuffer, 0, SysBuffer->frmBuffer[0]->frmProperties.timestamp, SysBuffer->control.dBaseTime, uiDeinterlace, SysBuffer->control.uiInterlaceParity, FULLFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                Rotate_Buffer_deinterlaced(SysBuffer->frmBuffer);
                SysBuffer->control.uiCur--;
            }
        }
        SysBuffer->control.uiCur++;
        return READY;
    }
    else
    {
        SysBuffer->control.uiCur++;
        return NOTREADY;
    }
}

int PTIR_AutoMode_HF(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiDeinterlace = 0,
                 uiNumFramesToDispatch;

    if(SysBuffer->control.uiEndOfFrames)
    {
        if(SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.tindex != SysBuffer->control.uiFrame)
            SysBuffer->control.uiCur = max(1, SysBuffer->control.uiCur - 1);
    }

    Frame_Prep_and_Analysis(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Analyze_Buffer_Stats_Automode(SysBuffer->frmBuffer, &SysBuffer->control.mainPattern, &SysBuffer->control.uiDispatch);
        if(SysBuffer->control.mainPattern.ucPatternFound)
        {
            SysBuffer->control.dTimePerFrame = Calculate_Resulting_timestamps(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch, SysBuffer->control.uiCur, SysBuffer->control.dBaseTime, &uiNumFramesToDispatch, SysBuffer->control.mainPattern.ucPatternType, SysBuffer->control.uiEndOfFrames);
            for (i = 0; i < uiNumFramesToDispatch; i++)
            {
                if (!SysBuffer->frmBuffer[i]->frmProperties.drop)
                    Update_Frame_Buffer(SysBuffer->frmBuffer, i, SysBuffer->control.dTimePerFrame, AUTOMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                else
                {
                    SysBuffer->frmBuffer[i + 1]->frmProperties.timestamp = SysBuffer->frmBuffer[i]->frmProperties.timestamp;
                    SysBuffer->frmBuffer[i]->frmProperties.drop = FALSE;
                }
            }
            Rotate_Buffer_borders(SysBuffer->frmBuffer, uiNumFramesToDispatch);
            SysBuffer->control.uiCur -= min(uiNumFramesToDispatch,SysBuffer->control.uiCur);
        }
        else
        {
            SysBuffer->control.dTimePerFrame = Calculate_Resulting_timestamps(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch, SysBuffer->control.uiCur, SysBuffer->control.dBaseTime, &uiNumFramesToDispatch, SysBuffer->control.mainPattern.ucPatternType, SysBuffer->control.uiEndOfFrames);
            for(i = 0; i < uiNumFramesToDispatch; i++)
            {
                //FillBaseLinesIYUV(SysBuffer->frmBuffer[0], SysBuffer->frmBuffer[BUFMINSIZE], SysBuffer->control.uiInterlaceParity, SysBuffer->control.uiInterlaceParity);
                if (!SysBuffer->frmBuffer[0]->frmProperties.drop)
                {
                    if (!(SysBuffer->control.mainPattern.uiInterlacedFramesNum == (BUFMINSIZE - 1)))
                    {
                        Update_Frame_Buffer(SysBuffer->frmBuffer, 0, SysBuffer->control.dTimePerFrame, AUTOMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                        Rotate_Buffer(SysBuffer->frmBuffer);
                    }
                    else
                    {
                        Update_Frame_Buffer(SysBuffer->frmBuffer, 0, SysBuffer->control.dTimePerFrame, uiDeinterlace, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                        Rotate_Buffer_deinterlaced(SysBuffer->frmBuffer);
                    }
                }
                else
                    SysBuffer->frmBuffer[0]->frmProperties.drop = FALSE;
                SysBuffer->control.uiCur--;
            }
        }
        SysBuffer->control.uiCur++;
        return READY;
    }
    else
    {
        SysBuffer->control.uiCur++;
        return NOTREADY;
    }
}

int PTIR_DeinterlaceMode_FF(PTIRSystemBuffer *SysBuffer)
{
    if(SysBuffer->control.uiCur >= 1)
    {
        SysBuffer->frmBuffer[0]->frmProperties.interlaced = 1;
        Frame_Prep_and_Analysis(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
        SysBuffer->control.dTimePerFrame = SysBuffer->control.dBaseTime / 2;
        Update_Frame_Buffer(SysBuffer->frmBuffer, 0, SysBuffer->control.dTimePerFrame, DEINTERLACEMODE, SysBuffer->control.uiInterlaceParity, FULLFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
        Rotate_Buffer_deinterlaced(SysBuffer->frmBuffer);
        return READY;
    }
    else
        return NOTREADY;
}

int PTIR_DeinterlaceMode_HF(PTIRSystemBuffer *SysBuffer)
{
    if(SysBuffer->control.uiCur >= 1)
    {
        SysBuffer->frmBuffer[0]->frmProperties.interlaced = 1;
        Frame_Prep_and_Analysis(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
        Update_Frame_Buffer(SysBuffer->frmBuffer, 0, SysBuffer->control.dBaseTime, DEINTERLACEMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
        Rotate_Buffer_deinterlaced(SysBuffer->frmBuffer);
        return READY;
    }
    else
        return NOTREADY;
}

int PTIR_BaseFrameMode(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiDeinterlace = 0,
                 uiNumFramesToDispatch;

    Frame_Prep_and_Analysis(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Analyze_Buffer_Stats_Automode(SysBuffer->frmBuffer, &SysBuffer->control.mainPattern, &SysBuffer->control.uiDispatch);
        if(!SysBuffer->control.uiEndOfFrames)
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur + 1);
        else
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur);

        Interlaced_detection_log(SysBuffer->frmBuffer, uiNumFramesToDispatch);
        if(SysBuffer->control.mainPattern.ucPatternFound)
        {
            for (i = 0; i < uiNumFramesToDispatch; i++)
                Update_Frame_Buffer(SysBuffer->frmBuffer, i, SysBuffer->control.dBaseTime, BASEFRAMEMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
            Rotate_Buffer_borders(SysBuffer->frmBuffer, uiNumFramesToDispatch);
            SysBuffer->control.uiCur -= min(uiNumFramesToDispatch,SysBuffer->control.uiCur);
        }
        else
        {
            for(i = 0; i < uiNumFramesToDispatch; i++)
            {
                Update_Frame_Buffer(SysBuffer->frmBuffer, 0, SysBuffer->control.dBaseTime, BASEFRAMEMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                Rotate_Buffer_deinterlaced(SysBuffer->frmBuffer);
                SysBuffer->control.uiCur--;
            }
        }
        SysBuffer->control.uiCur++;
        return READY;
    }
    else
    {
        SysBuffer->control.uiCur++;
        return NOTREADY;
    }
}

int PTIR_BaseFrameMode_NoChange(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiDeinterlace = 0,
                 uiNumFramesToDispatch;

    Frame_Prep_and_Analysis(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Artifacts_Detection(SysBuffer->frmBuffer);
        uiNumFramesToDispatch = min(BUFMINSIZE - 1, SysBuffer->control.uiCur);
        Interlaced_detection_log(SysBuffer->frmBuffer, uiNumFramesToDispatch);
        for (i = 0; i < uiNumFramesToDispatch; i++)
            SysBuffer->control.mainPattern.ulCountInterlacedDetections += SysBuffer->frmBuffer[i]->frmProperties.interlaced;
        Rotate_Buffer_borders(SysBuffer->frmBuffer, uiNumFramesToDispatch);
        SysBuffer->control.uiCur -= uiNumFramesToDispatch;
        SysBuffer->control.uiCur++;
        return READY;
    }
    else
    {
        SysBuffer->control.uiCur++;
        return NOTREADY;
    }
}

int PTIR_Auto24fpsMode(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiTelecineMode = TELECINE24FPSMODE,
                 uiNumFramesToDispatch, uiLetGo = 0, uiDone = 0, uiCheckCount = 0;
    const double filmfps = 1 / 23.976 * 1000;
    double frmVal = 0.0;

    if(SysBuffer->control.uiEndOfFrames)
    {
        if(SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.tindex != SysBuffer->control.uiFrame)
            SysBuffer->control.uiCur = max(1, SysBuffer->control.uiCur - 1);
    }

    Frame_Prep_and_Analysis(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Analyze_Buffer_Stats(SysBuffer->frmBuffer, &SysBuffer->control.mainPattern, &SysBuffer->control.uiDispatch, &uiTelecineMode);
        if(!SysBuffer->control.uiEndOfFrames)
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur + 1);
        else
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur);

        for(i = 0; i < uiNumFramesToDispatch; i++)
        {
            if(SysBuffer->frmBuffer[i]->frmProperties.drop || SysBuffer->frmBuffer[i]->frmProperties.drop24fps)
                uiLetGo++;
        }

        if(SysBuffer->control.mainPattern.ucPatternFound && (!SysBuffer->control.uiEndOfFrames || uiNumFramesToDispatch == (BUFMINSIZE - 1)) )
        {
            uiDone = 0;
            for (i = 0; i < uiNumFramesToDispatch; i++)
            {
                uiCheckCount = ((SysBuffer->control.uiBufferCount + 1 + i == 5) && !uiLetGo);
                if (!SysBuffer->frmBuffer[i]->frmProperties.drop24fps && !SysBuffer->frmBuffer[i]->frmProperties.drop && (!uiCheckCount || uiDone))
                    Update_Frame_Buffer(SysBuffer->frmBuffer, i, filmfps, TELECINE24FPSMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                else
                {
                    SysBuffer->frmBuffer[i + 1]->frmProperties.timestamp = SysBuffer->frmBuffer[i]->frmProperties.timestamp;
                    SysBuffer->frmBuffer[i]->frmProperties.drop24fps = FALSE;
                    SysBuffer->frmBuffer[i]->frmProperties.drop = FALSE;
                    if((SysBuffer->control.uiBufferCount + 1 + i == 5) && !uiLetGo)
                        SysBuffer->control.uiBufferCount = 0;
                    if(uiCheckCount)
                        uiDone = TRUE;
                }
            }
            Rotate_Buffer_borders(SysBuffer->frmBuffer, uiNumFramesToDispatch);
            SysBuffer->control.uiCur -= min(uiNumFramesToDispatch,SysBuffer->control.uiCur);
            if((uiNumFramesToDispatch < (BUFMINSIZE - 1)) && !uiLetGo)
                SysBuffer->control.uiBufferCount += uiNumFramesToDispatch;
        }
        else
        {
            uiDone = 1;
            for(i = 0; i < uiNumFramesToDispatch; i++)
            {
                frmVal = 0.4 + (double) (SysBuffer->frmBuffer[0]->frmProperties.tindex) * 4 / 5;
                uiCheckCount = ((SysBuffer->control.uiFrameOut + uiDone + i) <= frmVal);
                if(!uiCheckCount)
                    uiDone = 0;
                if ((!SysBuffer->frmBuffer[0]->frmProperties.drop24fps && !SysBuffer->frmBuffer[0]->frmProperties.drop))
                {
                    SysBuffer->control.uiBufferCount++;

                    if ((SysBuffer->control.uiBufferCount < (BUFMINSIZE - 1)) && uiCheckCount)
                        Update_Frame_Buffer(SysBuffer->frmBuffer, 0, filmfps, TELECINE24FPSMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                    else
                    {
                        SysBuffer->control.uiBufferCount = 0;
                        SysBuffer->frmBuffer[1]->frmProperties.timestamp = SysBuffer->frmBuffer[0]->frmProperties.timestamp;
                        uiDone = 0;
                    }
                }
                else
                {
                    SysBuffer->frmBuffer[0]->frmProperties.drop24fps = FALSE;
                    SysBuffer->frmBuffer[0]->frmProperties.drop = FALSE;
                    SysBuffer->control.uiBufferCount = 0;
                }
                Rotate_Buffer(SysBuffer->frmBuffer);
                SysBuffer->control.uiCur--;
            }
        }
        SysBuffer->control.uiCur++;
        return READY;
    }
    else
    {
        SysBuffer->control.uiCur++;
        return NOTREADY;
    }
}

int PTIR_FixedTelecinePatternMode(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiTelecineMode = TELECINE24FPSMODE,
                 uiNumFramesToDispatch, uiLetGo = 0;
    const double filmfps = 1 / 23.976 * 1000;

    Frame_Prep_and_Analysis(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        RemovePattern(SysBuffer->frmBuffer, SysBuffer->control.uiPatternTypeInit, SysBuffer->control.uiPatternTypeNumber, &SysBuffer->control.uiDispatch, SysBuffer->control.uiInterlaceParity);

        if(!SysBuffer->control.uiEndOfFrames)
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur + 1);
        else
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur);

        for(i = 0; i < uiNumFramesToDispatch; i++)
        {
            if(!SysBuffer->frmBuffer[i]->frmProperties.drop)
                Update_Frame_Buffer(SysBuffer->frmBuffer, i, filmfps, TELECINE24FPSMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
            else
            {
                SysBuffer->frmBuffer[i + 1]->frmProperties.timestamp = SysBuffer->frmBuffer[i]->frmProperties.timestamp;
                SysBuffer->frmBuffer[i]->frmProperties.drop = FALSE;
            }
        }
        Rotate_Buffer_borders(SysBuffer->frmBuffer, uiNumFramesToDispatch);
        SysBuffer->control.uiCur -= min(uiNumFramesToDispatch,SysBuffer->control.uiCur);

        SysBuffer->control.uiCur++;
        return READY;
    }
    else
    {
        SysBuffer->control.uiCur++;
        return NOTREADY;
    }
}

int PTIR_MultipleMode(PTIRSystemBuffer *SysBuffer, unsigned int uiOpMode)
{
    if(uiOpMode == 0)
        return(PTIR_AutoMode_FF(SysBuffer));
    else if(uiOpMode == 1)
        return(PTIR_AutoMode_HF(SysBuffer));
    else if(uiOpMode == 2)
        return(PTIR_DeinterlaceMode_FF(SysBuffer));
    else if(uiOpMode == 3)
        return(PTIR_DeinterlaceMode_HF(SysBuffer));
    else if(uiOpMode == 4)
        return(PTIR_Auto24fpsMode(SysBuffer));
    else if(uiOpMode == 5)
        return(PTIR_FixedTelecinePatternMode(SysBuffer));
    else if (uiOpMode == 6)
        return(PTIR_BaseFrameMode(SysBuffer));
    else if (uiOpMode == 7)
        return(PTIR_BaseFrameMode_NoChange(SysBuffer));
    else
    {
        //throw (int) -16;
        return -16;
    }
}

int PTIR_SimpleMode(PTIRSystemBuffer *SysBuffer, unsigned int uiOpMode)
{
    if (uiOpMode)
        return(PTIR_BaseFrameMode_NoChange(SysBuffer));
    else
        return(PTIR_BaseFrameMode(SysBuffer));
        
}


Frame* PTIR_GetFrame(PTIRSystemBuffer *SysBuffer)
{
    return FrameQueue_Get(&SysBuffer->fqIn);
}

void PTIR_Init(PTIRSystemBuffer *SysBuffer, unsigned int _uiInWidth, unsigned int _uiInHeight, double _dFrameRate, unsigned int _uiInterlaceParity, unsigned int _uiPatternTypeNumber, unsigned int _uiPatternTypeInit)
{
    unsigned int i;

    SysBuffer->control.uiInWidth = _uiInWidth;
    SysBuffer->control.uiInHeight = _uiInHeight;
    SysBuffer->control.dFrameRate = _dFrameRate;
    SysBuffer->control.uiInterlaceParity = _uiInterlaceParity;
    SysBuffer->control.uiWidth = _uiInWidth;
    SysBuffer->control.uiHeight = _uiInHeight;
    SysBuffer->control.uiPatternTypeNumber = _uiPatternTypeNumber;
    SysBuffer->control.uiPatternTypeInit = _uiPatternTypeInit;

    SysBuffer->control.uiCur = 0;
    SysBuffer->control.uiNext = 0;
    SysBuffer->control.uiFrame = 0;
    SysBuffer->control.uiBufferCount = 0;
    SysBuffer->control.uiFrameCount = 0;
    SysBuffer->control.uiFrameOut = 0;
    SysBuffer->control.uiDispatch = 0;
    SysBuffer->control.dBaseTime = (1 / SysBuffer->control.dFrameRate) * 1000;
    SysBuffer->control.dDeIntTime = SysBuffer->control.dBaseTime / 2;
    SysBuffer->control.dBaseTimeSw = SysBuffer->control.dBaseTime;
    SysBuffer->control.dTimePerFrame = 0.0;
    SysBuffer->control.dTimeStamp = 0.0;
    SysBuffer->control.uiSize = SysBuffer->control.uiInWidth * SysBuffer->control.uiInHeight * 3 / 2;
    SysBuffer->control.uiEndOfFrames = 0;
    SysBuffer->control.pts = 0.0;
    SysBuffer->control.frame_duration = 1000 / _dFrameRate;

    for(i = 0; i < LASTFRAME; i++)
    {
        SysBuffer->frmIO[i] = (Frame*)malloc(sizeof(Frame));
        Frame_Create(SysBuffer->frmIO[i], SysBuffer->control.uiInWidth, SysBuffer->control.uiInHeight, SysBuffer->control.uiInWidth / 2, SysBuffer->control.uiInHeight / 2, 0);
        SysBuffer->frmBuffer[i] = SysBuffer->frmIO[i];
        SysBuffer->frmBuffer[i]->frmProperties.candidate = FALSE;
        SysBuffer->frmBuffer[i]->frmProperties.detection = FALSE;
        SysBuffer->frmBuffer[i]->frmProperties.drop = FALSE;
        SysBuffer->frmBuffer[i]->frmProperties.drop24fps = FALSE;
        SysBuffer->frmBuffer[i]->frmProperties.sceneChange = FALSE;
        SysBuffer->frmBuffer[i]->frmProperties.fr = 0.0;
        SysBuffer->frmBuffer[i]->frmProperties.interlaced = FALSE;
        SysBuffer->frmBuffer[i]->frmProperties.parity = 0;
        SysBuffer->frmBuffer[i]->frmProperties.processed = FALSE;
        SysBuffer->frmBuffer[i]->frmProperties.timestamp = 0.0;
        SysBuffer->frmBuffer[i]->frmProperties.tindex = 0;
        SysBuffer->frmBuffer[i]->frmProperties.uiInterlacedDetectionValue = 0;
    }
    SysBuffer->frmIO[LASTFRAME] = (Frame*)malloc(sizeof(Frame));
    Frame_Create(SysBuffer->frmIO[LASTFRAME], SysBuffer->control.uiWidth, SysBuffer->control.uiHeight, SysBuffer->control.uiWidth / 2, SysBuffer->control.uiHeight / 2, 0);
    FrameQueue_Initialize(&SysBuffer->fqIn);
    Pattern_init(&SysBuffer->control.mainPattern);
    SysBuffer->frmIn = NULL;
}

void PTIR_Clean(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i;
    if(!SysBuffer)
        return;

    for (i = 0; i <= LASTFRAME; ++i)
    {
        Frame_Release(SysBuffer->frmIO[i]);
        if(SysBuffer->frmIO[i])
        {
            free(SysBuffer->frmIO[i]);
            SysBuffer->frmIO[i] = 0;
        }
    }

    //free(SysBuffer->frmIO);
    //free(SysBuffer->frmBuffer);

}
