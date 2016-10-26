//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
//

#include "utilities.h"

#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

extern DeinterlaceFilter *pdeinterlaceFilter;

static void CopyFrame(Frame *pSrc, Frame* pDst)
{
    unsigned int i, val = 0;
    const unsigned char* pSrcLine;
    unsigned char* pDstLine;

    // copying Y-component
    for (i = 0; i < pSrc->plaY.uiHeight; i++)
    {
        pSrcLine = pSrc->plaY.ucCorner + i * pSrc->plaY.uiStride;
        pDstLine = pDst->plaY.ucCorner + i * pDst->plaY.uiStride;
        ptir_memcpy_s(pDstLine, pDst->plaY.uiWidth, pSrcLine, pSrc->plaY.uiWidth);
    }

    // copying U-component
    for (i = 0; i < pSrc->plaU.uiHeight; i++)
    {
        pSrcLine = pSrc->plaU.ucCorner + i * pSrc->plaU.uiStride;
        pDstLine = pDst->plaU.ucCorner + i * pDst->plaU.uiStride;
        ptir_memcpy_s(pDstLine, pDst->plaU.uiWidth, pSrcLine, pSrc->plaU.uiWidth);
    }
    // copying V-component
    for (i = 0; i < pSrc->plaV.uiHeight; i++)
    {
        pSrcLine = pSrc->plaV.ucCorner + i * pSrc->plaV.uiStride;
        pDstLine = pDst->plaV.ucCorner + i * pDst->plaV.uiStride;
        ptir_memcpy_s(pDstLine, pDst->plaV.uiWidth, pSrcLine, pSrc->plaV.uiWidth);
    }
}

void Interlaced_detection_logCM(Frame *frmBuffer[BUFMINSIZE], unsigned int uiNumFramesToDispatch)
{
    frmBuffer;
    uiNumFramesToDispatch;
    return;

#if 0
    unsigned int i = 0;
    FILE* fLogDetection = NULL;

    if(frmBuffer[0]->frmProperties.tindex == 1)
    {
#if defined(_WIN32) || defined(_WIN64)
        fopen_s(&fLogDetection,"prog_interlace_detect.txt","w");
#else
        fLogDetection = fopen("prog_interlace_detect.txt","w");
#endif
        if(fLogDetection)
            fprintf(fLogDetection,"Frame\tInterlaced\n");
    }
    else
#if defined(_WIN32) || defined(_WIN64)
        fopen_s(&fLogDetection,"prog_interlace_detect.txt","a+");
#else
        fLogDetection = fopen("prog_interlace_detect.txt","a+");
#endif

    for (i = 0; i < uiNumFramesToDispatch; i++)
    {
#if PRINTDEBUG || PRINTXPERT
        fprintf(fLogDetection,"%i\t%i\t%i\n",frmBuffer[i]->frmProperties.tindex,frmBuffer[i]->frmProperties.detection, frmBuffer[i]->frmProperties.uiInterlacedDetectionValue);
#else
        fprintf(fLogDetection,"%i\t%i\n",frmBuffer[i]->frmProperties.tindex - 1,frmBuffer[i]->frmProperties.detection);
#endif
        frmBuffer[i]->frmProperties.detection = false;
    }

    fclose(fLogDetection);
#endif
}


#if TEST_UNDO2FRAMES_CM
void Undo2Frames_CMTest(Frame *frmBuffer1, Frame *frmBuffer2, bool BFF)
{    
    Frame frmBackup;
    Frame *frmBuffer1_c = &frmBackup;
    Frame_Create(frmBuffer1_c, frmBuffer1->plaY.uiWidth, frmBuffer1->plaY.uiHeight, frmBuffer1->plaY.uiWidth/ 2, frmBuffer1->plaY.uiHeight/ 2, 0);
    CopyFrame(frmBuffer1, frmBuffer1_c);
    int comp = 0;

    pdeinterlaceFilter->Undo2FramesYUVCM(frmBuffer1, frmBuffer2, BFF);

    unsigned int 
        i,
        start;

    start = BFF;

    for(i = start; i < frmBuffer1_c->plaY.uiHeight; i += 2)
        ptir_memcpy_s(frmBuffer1_c->plaY.ucData + (i * frmBuffer1_c->plaY.uiStride), frmBuffer1_c->plaY.uiStride,frmBuffer2->plaY.ucData + (i * frmBuffer2->plaY.uiStride),frmBuffer2->plaY.uiStride);
    for(i = start; i < frmBuffer1_c->plaU.uiHeight; i += 2)
        ptir_memcpy_s(frmBuffer1_c->plaU.ucData + (i * frmBuffer1_c->plaU.uiStride), frmBuffer1_c->plaU.uiStride, frmBuffer2->plaU.ucData + (i * frmBuffer2->plaU.uiStride),frmBuffer2->plaU.uiStride);
    for(i = start; i < frmBuffer1_c->plaV.uiHeight; i += 2)
        ptir_memcpy_s(frmBuffer1_c->plaV.ucData + (i * frmBuffer1_c->plaV.uiStride), frmBuffer1_c->plaV.uiStride, frmBuffer2->plaV.ucData + (i * frmBuffer2->plaV.uiStride),frmBuffer2->plaV.uiStride);

    unsigned char * line0 = frmBuffer1->plaY.ucData;
    unsigned char * line1 = frmBuffer1->plaY.ucData + frmBuffer1->plaY.uiStride;

    comp = memcmp(frmBuffer1_c->ucMem, frmBuffer1->ucMem, frmBuffer1->uiSize);
    //printf("Undo2Frames_CM test result: %s", comp?"Failed!\n":"Successful!\n");
    frmBuffer1->frmProperties.candidate = true;
    frmBuffer1->frmProperties.interlaced = false;
}
#endif

static void Undo2Frames_CM(Frame *frmBuffer1, Frame *frmBuffer2, bool BFF)
{    
 //memset(frmBuffer1->ucMem, 0, frmBuffer1->uiSize);
 //return;
#ifdef GPUPATH
    pdeinterlaceFilter->Undo2FramesYUVCM(frmBuffer1, frmBuffer2, BFF);

    frmBuffer1->frmProperties.candidate = true;
    frmBuffer1->frmProperties.interlaced = false;
#endif

#ifdef CPUPATH
    Undo2Frames(frmBuffer1, frmBuffer2, BFF);
#endif
}

static void Detect_Solve_32Patterns_CM(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch)
{
    bool condition[10];


    //Case: First and last frames have pattern, need to move buffer two frames up
    //Texture Analysis
    condition[0] = (pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7])) ||
                   pFrm[0]->frmProperties.interlaced;
    condition[1] = (pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7])) ||
                   pFrm[4]->frmProperties.interlaced;
    condition[2] = (pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7])) ||
                   pFrm[5]->frmProperties.interlaced;
    //SAD analysis
    condition[3] = pFrm[0]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[0]->plaY.ucStats.ucSAD[2]),(pFrm[0]->plaY.ucStats.ucSAD[3]));
    condition[4] = pFrm[4]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[4]->plaY.ucStats.ucSAD[2]),(pFrm[4]->plaY.ucStats.ucSAD[3]));
    condition[5] = pFrm[5]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[5]->plaY.ucStats.ucSAD[2]),(pFrm[5]->plaY.ucStats.ucSAD[3]));


    if(condition[0] && condition[1] && condition[2] && condition[3] && condition[4] && condition[5])
    {
        pFrm[0]->frmProperties.drop = false;//true;
        pFrm[0]->frmProperties.drop24fps = true;
        pFrm[0]->frmProperties.candidate = true;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = true;
        ptrn->ucLatch.ucFullLatch = true;
        if(pFrm[4]->plaY.ucStats.ucSAD[2] > pFrm[4]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;
        *dispatch = 2;
        return;
    }

    //Case: First and second frames have pattern, need to move buffer three frames up
    //Texture Analysis
    condition[0] = (pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[0]->frmProperties.interlaced;
    condition[1] = (pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[1]->frmProperties.interlaced;
    condition[2] = (pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[5]->frmProperties.interlaced;
    //SAD analysis
    condition[3] = pFrm[0]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[0]->plaY.ucStats.ucSAD[2]),(pFrm[0]->plaY.ucStats.ucSAD[3]));
    condition[4] = pFrm[1]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[1]->plaY.ucStats.ucSAD[2]),(pFrm[1]->plaY.ucStats.ucSAD[3]));
    //condition[5] = pFrm[5]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[5]->plaY.ucStats.ucSAD[2]),(pFrm[5]->plaY.ucStats.ucSAD[3]));
    if(condition[0] && condition[1] && condition[2] && condition[3] && condition[4]/* && condition[5]*/)
    {
        pFrm[0]->frmProperties.drop = false;
        pFrm[1]->frmProperties.drop = false;//true;
        pFrm[1]->frmProperties.drop24fps = true;
        pFrm[1]->frmProperties.candidate = true;
        pFrm[2]->frmProperties.drop = false;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = true;
        ptrn->ucLatch.ucFullLatch = true;

        if(pFrm[1]->plaY.ucStats.ucSAD[2] > pFrm[1]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;

        *dispatch = 3;
        Undo2Frames_CM(pFrm[0],pFrm[1],!!ptrn->ucLatch.ucParity);
        return;
    }

    //Case: second and third frames have pattern, need to move buffer four frames up
    //Texture Analysis
    condition[0] = (pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[1]->frmProperties.interlaced;
    condition[1] = (pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[2]->frmProperties.interlaced;
    //SAD analysis
    condition[2] = pFrm[1]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[1]->plaY.ucStats.ucSAD[2]),(pFrm[1]->plaY.ucStats.ucSAD[3]));
    condition[3] = pFrm[2]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[2]->plaY.ucStats.ucSAD[2]),(pFrm[2]->plaY.ucStats.ucSAD[3]));
    if(condition[0] && condition[1] && condition[2] && condition[3])
    {
        pFrm[0]->frmProperties.drop = false;
        pFrm[1]->frmProperties.drop = false;
        pFrm[2]->frmProperties.drop = false;/*true*/
        pFrm[2]->frmProperties.drop24fps = true;
        pFrm[2]->frmProperties.candidate = true;
        pFrm[3]->frmProperties.drop = false;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = true;
        ptrn->ucLatch.ucFullLatch = true;
        if(pFrm[2]->plaY.ucStats.ucSAD[2] > pFrm[2]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;

        *dispatch = 4;
        Undo2Frames_CM(pFrm[1],pFrm[2],!!ptrn->ucLatch.ucParity);
        return;
    }

    //Case: third and fourth frames have pattern, pattern is synchronized
    //Texture Analysis
    condition[0] = (pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[2]->frmProperties.interlaced;
    condition[1] = (pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[3]->frmProperties.interlaced;
    //SAD analysis
    condition[2] = pFrm[2]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[2]->plaY.ucStats.ucSAD[2]),(pFrm[2]->plaY.ucStats.ucSAD[3]));
    condition[3] = pFrm[3]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));
    if(condition[0] && condition[1] && condition[2] && condition[3])
    {
        pFrm[3]->frmProperties.drop = true;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = false;
        ptrn->ucLatch.ucFullLatch = true;
        if(pFrm[3]->plaY.ucStats.ucSAD[2] > pFrm[3]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;
        *dispatch = 5;
        Undo2Frames_CM(pFrm[2],pFrm[3],!!ptrn->ucLatch.ucParity);
        return;
    }

    //Case: fourth and fifth frames have pattern, one frame needs to be moved out
    //Texture Analysis
    condition[0] = (pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7])) ||
                   pFrm[3]->frmProperties.interlaced;
    condition[1] = (pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7])) ||
                   pFrm[4]->frmProperties.interlaced;
    //SAD analysis
    condition[2] = pFrm[3]->plaY.ucStats.ucSAD[4] /** T1*/ > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));
    condition[3] = pFrm[4]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[4]->plaY.ucStats.ucSAD[2]),(pFrm[4]->plaY.ucStats.ucSAD[3]));
    if((condition[0] && condition[1] && condition[2]) || (condition[0] && condition[1] && condition[3]))
    {
        pFrm[0]->frmProperties.drop = false;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = true;
        ptrn->ucLatch.ucFullLatch = true;
        if(pFrm[3]->plaY.ucStats.ucSAD[2] > pFrm[3]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;
        pFrm[0]->frmProperties.candidate = true;

        *dispatch = 1;
        return;
    }
}

static void Detect_32Pattern_rigorous_CM(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch)
{
    bool condition[10];
    int previousPattern = ptrn->ucPatternType;
    double previousAvgSAD = ptrn->ucAvgSAD;

    ptrn->ucLatch.ucFullLatch = false;
    ptrn->ucAvgSAD = CalcSAD_Avg_NoSC(pFrm);

    Init_drop_frames(pFrm);

    condition[8] = ptrn->ucAvgSAD < previousAvgSAD * T2;

    if(ptrn->uiInterlacedFramesNum < 3)
        Detect_Solve_32RepeatedFramePattern(pFrm, ptrn, dispatch);

    if(!ptrn->ucLatch.ucFullLatch &&(ptrn->uiInterlacedFramesNum >= 4))
        Detect_Solve_41FramePattern(pFrm, ptrn, dispatch);

    if (!ptrn->ucLatch.ucFullLatch && (ptrn->uiInterlacedFramesNum >= 3 || previousPattern == 5 || previousPattern == 6 || previousPattern == 7))
        Detect_Solve_32BlendedPatternsCM(pFrm, ptrn, dispatch);

    if((!ptrn->ucLatch.ucFullLatch && !(ptrn->uiInterlacedFramesNum >= 3)) || ((previousPattern == 1 || previousPattern == 2) && ptrn->ucLatch.ucSHalfLatch))
    {
        ptrn->ucLatch.ucSHalfLatch = false;
        ptrn->ucPatternType = 0;
        //Texture Analysis  - To override previous pattern if needed
        condition[0] = pFrm[2]->plaY.ucStats.ucRs[0] > (pFrm[2]->plaY.ucStats.ucRs[1]) ||
                       pFrm[2]->plaY.ucStats.ucRs[0] > (pFrm[2]->plaY.ucStats.ucRs[2]) ||
                       pFrm[2]->frmProperties.interlaced;
        condition[1] = pFrm[3]->plaY.ucStats.ucRs[0] > (pFrm[3]->plaY.ucStats.ucRs[1]) ||
                       pFrm[3]->plaY.ucStats.ucRs[0] > (pFrm[3]->plaY.ucStats.ucRs[2]) ||
                       pFrm[3]->frmProperties.interlaced;


        if(previousPattern == 1 || (condition[0] && condition[1]))
        {
            //Assuming pattern is synchronized
            //Texture Analysis 2
            condition[2] = (pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7])) || pFrm[2]->frmProperties.interlaced;
            condition[3] = (pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) || pFrm[3]->frmProperties.interlaced;

            condition[9] = (pFrm[0]->plaY.ucStats.ucRs[5] == 0) && (pFrm[1]->plaY.ucStats.ucRs[5] == 0) && (pFrm[4]->plaY.ucStats.ucRs[5] == 0);

            //SAD analysis
            condition[4] = pFrm[2]->plaY.ucStats.ucSAD[4] * T3 > min((pFrm[2]->plaY.ucStats.ucSAD[2]),(pFrm[2]->plaY.ucStats.ucSAD[3]));
            condition[5] = pFrm[3]->plaY.ucStats.ucSAD[4] * T4 > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));

            condition[6] = pFrm[2]->plaY.ucStats.ucSAD[4] * T4 > min((pFrm[2]->plaY.ucStats.ucSAD[2]),(pFrm[2]->plaY.ucStats.ucSAD[3]));
            condition[7] = pFrm[3]->plaY.ucStats.ucSAD[4] * T3 > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));


            if((condition[0] && condition[1] && ptrn->uiInterlacedFramesNum > 0) || (condition[2] && condition[3]) || (condition[4] && condition[5]) || (condition[6] && condition[7]))
            {
                pFrm[3]->frmProperties.drop = true;
                ptrn->ucLatch.ucFullLatch = true;
                if(pFrm[3]->plaY.ucStats.ucSAD[2] > pFrm[3]->plaY.ucStats.ucSAD[3] && pFrm[2]->plaY.ucStats.ucSAD[2] > pFrm[2]->plaY.ucStats.ucSAD[3] && pFrm[2]->plaY.ucStats.ucSAD[0] > pFrm[2]->plaY.ucStats.ucSAD[1] && pFrm[4]->plaY.ucStats.ucSAD[1] > pFrm[4]->plaY.ucStats.ucSAD[0])
                    ptrn->ucLatch.ucParity = 1;
                else
                    ptrn->ucLatch.ucParity = 0;
                ptrn->ucPatternType = 1;
                *dispatch = 5;
                Undo2Frames_CM(pFrm[2],pFrm[3],!!ptrn->ucLatch.ucParity);
                return;
            }
            if((condition[0] || condition[1] || condition[2] || condition[3] || (condition[4] || condition[5]) || condition[6] || condition[7] || condition[9]) && (ptrn->ucAvgSAD < 2.3))
            {
                pFrm[3]->frmProperties.drop = true;
                ptrn->ucLatch.ucFullLatch = true;
                if(pFrm[2]->plaY.ucStats.ucSAD[2] > pFrm[2]->plaY.ucStats.ucSAD[3])
                    ptrn->ucLatch.ucParity = 1;
                else
                    ptrn->ucLatch.ucParity = 0;
                if(condition[8] || condition[9])
                    ptrn->ucPatternType = 1;
                else
                    ptrn->ucPatternType = 0;
                *dispatch = 5;
                Undo2Frames_CM(pFrm[2],pFrm[3],!!ptrn->ucLatch.ucParity);
                return;
            }
        }
        else if(previousPattern == 2)
        {
            //Assuming pattern is synchronized
            //Texture Analysis
            condition[0] = (pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                           pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                           pFrm[3]->frmProperties.interlaced;
            //SAD analysis
            condition[1] = pFrm[3]->plaY.ucStats.ucSAD[4] * T3 > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));
            if(condition[0] || condition[1])
            {
                pFrm[2]->frmProperties.candidate = true;
                pFrm[3]->frmProperties.drop = true;
                pFrm[4]->frmProperties.candidate = true;
                ptrn->ucLatch.ucFullLatch = true;
                ptrn->ucPatternType = 2;
                *dispatch = 5;
                return;
            }
            else if(pFrm[3]->plaY.ucStats.ucRs[7] > 0)
            {
                pFrm[2]->frmProperties.candidate = true;
                pFrm[3]->frmProperties.drop = true;
                pFrm[4]->frmProperties.candidate = true;
                ptrn->ucLatch.ucFullLatch = true;
                ptrn->ucPatternType = 0;
                *dispatch = 5;
                return;
            }
        }
    }

    if(!ptrn->ucLatch.ucFullLatch && !(ptrn->uiInterlacedFramesNum > 2) && (ptrn->uiInterlacedFramesNum > 0))
    {
        Detect_Solve_32Patterns_CM(pFrm, ptrn, dispatch);
        if(!ptrn->ucLatch.ucFullLatch)
            Detect_Solve_3223Patterns(pFrm, ptrn, dispatch);
    }
    if(!ptrn->ucLatch.ucFullLatch && (ptrn->uiInterlacedFramesNum == 4))
    {
        ptrn->ucLatch.ucFullLatch = true;
        ptrn->ucPatternType = 3;
        *dispatch = 5;
    }
}

static void Detect_Interlacing_Artifacts_fast_CM(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch)
{
    unsigned int i;

    ptrn->ucLatch.ucFullLatch = false;
    ptrn->ucPatternType = 0;

    Init_drop_frames(pFrm);

    ptrn->ucAvgSAD = CalcSAD_Avg_NoSC(pFrm);

    if(ptrn->uiInterlacedFramesNum < 3)
        Detect_Solve_32RepeatedFramePattern(pFrm, ptrn, dispatch);

    if(!ptrn->ucLatch.ucFullLatch &&(ptrn->uiInterlacedFramesNum >= 4))
        Detect_Solve_41FramePattern(pFrm, ptrn, dispatch);

    if (!ptrn->ucLatch.ucFullLatch && (ptrn->uiInterlacedFramesNum >= 3 || ptrn->ucPatternType == 5 || ptrn->ucPatternType == 6))
        Detect_Solve_32BlendedPatternsCM(pFrm, ptrn, dispatch);

    if(!ptrn->ucLatch.ucFullLatch)
        Detect_Solve_32Patterns_CM(pFrm, ptrn, dispatch);
    
    if(!ptrn->ucLatch.ucFullLatch)
        Detect_Solve_3223Patterns(pFrm, ptrn, dispatch);

    for(i = 0; i < BUFMINSIZE; i++)
        pFrm[i]->frmProperties.processed = true;

}
void Analyze_Buffer_Stats_CM(Frame *frmBuffer[BUFMINSIZE], Pattern *ptrn, unsigned int *pdispatch, unsigned int *uiisInterlaced)
{
    unsigned int uiDropCount = 0,
        i = 0,
        uiPrevInterlacedFramesNum = (*pdispatch == 1 ? 0:ptrn->uiInterlacedFramesNum);

    ptrn->uiInterlacedFramesNum = Artifacts_Detection(frmBuffer) + frmBuffer[0]->frmProperties.interlaced;

    if ((*uiisInterlaced != 1) && !(ptrn->uiInterlacedFramesNum > 4 && ptrn->ucPatternType < 1))
    {
        if(!frmBuffer[0]->frmProperties.processed)
            Detect_Interlacing_Artifacts_fast_CM(frmBuffer, ptrn, pdispatch);
        else
            Detect_32Pattern_rigorous_CM(frmBuffer, ptrn, pdispatch);
        if(!ptrn->ucLatch.ucFullLatch)
        {
            if(ptrn->uiInterlacedFramesNum < 2 && !ptrn->ucLatch.ucThalfLatch)
            {
                ptrn->ucLatch.ucFullLatch = true;
                ptrn->ucPatternType = 0;
                *pdispatch = 5;
            }
            else
            {
                *pdispatch = 1;
                ptrn->uiOverrideHalfFrameRate = false;
            }
        }
        if (*uiisInterlaced == 2)
        {
            if (*pdispatch >= BUFMINSIZE - 1)
            {
                for (i = 0; i < *pdispatch; i++)
                    uiDropCount += frmBuffer[i]->frmProperties.drop;

                if (!uiDropCount)
                    frmBuffer[BUFMINSIZE - 1]->frmProperties.drop = 1;
            }
        }
    }
    else
    {
        for (i = 0; i < BUFMINSIZE - 1; i++)
        {
            frmBuffer[i]->frmProperties.drop = 0;
            frmBuffer[i]->frmProperties.interlaced = 1;
        }

        *pdispatch = (BUFMINSIZE - 1);

        if(uiPrevInterlacedFramesNum == BUFMINSIZE - 1)
        {
            if(ptrn->uiInterlacedFramesNum + frmBuffer[BUFMINSIZE - 1]->frmProperties.interlaced == BUFMINSIZE)
            {
                ptrn->uiOverrideHalfFrameRate = true;
                ptrn->uiCountFullyInterlacedBuffers = min(ptrn->uiCountFullyInterlacedBuffers + 1, 2);
            }
        }
        else
        {
            if(ptrn->uiInterlacedFramesNum + frmBuffer[BUFMINSIZE - 1]->frmProperties.interlaced == BUFMINSIZE)
                ptrn->uiOverrideHalfFrameRate = true;
                //ptrn->uiCountFullyInterlacedBuffers = min(ptrn->uiCountFullyInterlacedBuffers + 1, 2);
            else
            {
                if(ptrn->uiCountFullyInterlacedBuffers > 0)
                    ptrn->uiCountFullyInterlacedBuffers--;
                else
                    ptrn->uiOverrideHalfFrameRate = false;
            }
        }
    }

    if(!ptrn->ucLatch.ucFullLatch)
    {
        for (i = 0; i < *pdispatch; i++)
            ptrn->ulCountInterlacedDetections += frmBuffer[i]->frmProperties.interlaced;
    }
    else
        ptrn->ulCountInterlacedDetections += ptrn->uiInterlacedFramesNum;

    ptrn->ucPatternFound = ptrn->ucLatch.ucFullLatch;
}

void Analyze_Buffer_Stats_Automode_CM(Frame *frmBuffer[BUFMINSIZE], Pattern *ptrn, unsigned int *pdispatch)
{
    unsigned int uiAutomode = AUTOMODE;

    Analyze_Buffer_Stats_CM(frmBuffer, ptrn, pdispatch, &uiAutomode);

}

void Detect_Solve_32BlendedPatternsCM(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch)
{
    unsigned int i, count, start, previousPattern;
 bool condition[10];

    ptrn->ucLatch.ucFullLatch = false;
    ptrn->ucLatch.ucThalfLatch = false;
    previousPattern = ptrn->ucPatternType;
    ptrn->ucPatternType = 0;
    if (pFrm[0]->plaY.uiHeight > 720)
    {
        for (i = 0; i < 10; i++)
            condition[i] = 0;

        if (previousPattern != 5 && previousPattern != 7)
        {
            condition[0] = (pFrm[0]->plaY.ucStats.ucRs[4] + 2 < pFrm[1]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[0]->plaY.ucStats.ucRs[4] + 2 < pFrm[2]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[0]->plaY.ucStats.ucRs[4] + 2 < pFrm[3]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[0]->plaY.ucStats.ucRs[4] + 2 < pFrm[4]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[0]->plaY.ucStats.ucRs[4] < 100);

            condition[1] = (pFrm[4]->plaY.ucStats.ucRs[4] + 2 < pFrm[0]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[4]->plaY.ucStats.ucRs[4] + 2 < pFrm[1]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[4]->plaY.ucStats.ucRs[4] + 2 < pFrm[2]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[4]->plaY.ucStats.ucRs[4] + 2 < pFrm[3]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[4]->plaY.ucStats.ucRs[4] < 100);

            if (condition[0] && !condition[1])
                start = 1;
            else if (!condition[0] && condition[1])
                start = 0;
            else
            {
                if (!pFrm[5]->frmProperties.interlaced)
                    ptrn->ucLatch.ucSHalfLatch = true;
                return;
            }

            for (i = 0; i < 4; i++)
                condition[i + start] = (pFrm[i + start]->plaY.ucStats.ucRs[6] > 100) && (pFrm[i + start]->plaY.ucStats.ucRs[7] > 0.1);

            count = 1;
            for (i = 1; i < 4; i++)
                count += condition[i];
            if (count > 3)
            {
                if (condition[0])
                {
                    start = 1;
                    pFrm[4]->frmProperties.drop = true;
                    *dispatch = 5;
                }
                else
                {
                    start = 0;
                    pFrm[3]->frmProperties.drop = true;
                    *dispatch = 4;
                }
                UndoPatternTypes5and7CM(pFrm, start);
                ptrn->ucLatch.ucFullLatch = true;
                pFrm[3]->frmProperties.candidate = true;
                if (previousPattern != 6)
                    ptrn->ucPatternType = 5;
                else
                    ptrn->ucPatternType = 7;
            }
        }
        else
        {
            count = 0;
            for (i = 0; i < 3; i++)
                count += pFrm[i]->frmProperties.interlaced;
            if (count < 3)
            {
                if (previousPattern != 7)
                {
                    ptrn->ucLatch.ucFullLatch = true;
                    ptrn->blendedCount += BLENDEDOFF;
                    if (ptrn->blendedCount > 1)
                    {
                        pdeinterlaceFilter->DeinterlaceMedianFilterSingleFieldCM(pFrm, 1, 0);
                        ptrn->blendedCount -= 1;
                    }
                    else
                    {
                        pFrm[1]->frmProperties.drop = false;
                        //pFrm[1]->frmProperties.candidate;
                    }
                    ptrn->ucPatternType = 6;
                    *dispatch = 2;
                }
                else
                {
                    count = 0;
                    for (i = 3; i < 5; i++)
                        count += pFrm[i]->frmProperties.interlaced;
                    if (count > 1 && pFrm[5]->frmProperties.interlaced)
                    {
                        ptrn->ucLatch.ucFullLatch = true;
                        ptrn->ucPatternType = 8;
                        *dispatch = 2;
                    }
                    else
                    {
                        ptrn->ucLatch.ucFullLatch = true;
                        ptrn->ucPatternType = 1;
                        *dispatch = 1;
                    }
                }
            }
        }
    }
}

void UndoPatternTypes5and7CM(Frame *frmBuffer[BUFMINSIZE], unsigned int firstPos)
{
    unsigned int
        start = firstPos;
    //Frame 1
    //FillBaseLinesIYUV(frmBuffer[start], frmBuffer[BUFMINSIZE], start < (firstPos + 2), start < (firstPos + 2));
    pdeinterlaceFilter->DeinterlaceMedianFilterSingleFieldCM(frmBuffer, start, start < (firstPos + 2));

    //Frame 2
    start++;
    //FillBaseLinesIYUV(frmBuffer[start], frmBuffer[BUFMINSIZE], start < (firstPos + 2), start < (firstPos + 2));
    pdeinterlaceFilter->DeinterlaceMedianFilterSingleFieldCM(frmBuffer, start, start < (firstPos + 2));
 

    //Frame 3
    start++;
    frmBuffer[start]->frmProperties.drop = true;


    //Frame 4
    start++;
    //FillBaseLinesIYUV(frmBuffer[start], frmBuffer[BUFMINSIZE], start < (firstPos + 2), start < (firstPos + 2));
    pdeinterlaceFilter->DeinterlaceMedianFilterSingleFieldCM(frmBuffer, start, start < (firstPos + 2));

}

void Pattern32RemovalCM(Frame **frmBuffer, unsigned int uiInitFramePosition, unsigned int *pdispatch, unsigned int parity)
{
    if(uiInitFramePosition < BUFMINSIZE - 2)
    {
        frmBuffer[uiInitFramePosition + 1]->frmProperties.drop = true;
        Undo2Frames_CM(frmBuffer[uiInitFramePosition],frmBuffer[uiInitFramePosition + 1], !!parity);//TelecineParityCheck(*frmBuffer[uiInitFramePosition + 1]));
        *pdispatch = 5;
    }
    else
    {
        frmBuffer[uiInitFramePosition]->frmProperties.interlaced = true;
        *pdispatch = 2;
    }
}

void Pattern41aRemovalCM(Frame **frmBuffer, unsigned int uiInitFramePosition, unsigned int *pdispatch, unsigned int parity)
{
    unsigned int i = 0;
    if(uiInitFramePosition < BUFMINSIZE - 4)
    {
        frmBuffer[uiInitFramePosition + 3]->frmProperties.drop = true;
        Undo2Frames_CM(frmBuffer[uiInitFramePosition],frmBuffer[uiInitFramePosition + 1], !!parity);
        Undo2Frames_CM(frmBuffer[uiInitFramePosition + 1],frmBuffer[uiInitFramePosition + 2], !!parity);
        Undo2Frames_CM(frmBuffer[uiInitFramePosition + 2],frmBuffer[uiInitFramePosition + 3], !!parity);
        *pdispatch = 5;
    }
    else
    {
        for (i = 0; i < BUFMINSIZE - 1; i++)
            frmBuffer[uiInitFramePosition]->frmProperties.interlaced = false;
        frmBuffer[uiInitFramePosition - 1]->frmProperties.interlaced = false;
        *pdispatch = uiInitFramePosition;
    }
}

void RemovePatternCM(Frame **frmBuffer, unsigned int uiPatternNumber, unsigned int uiInitFramePosition, unsigned int *pdispatch, unsigned int parity)
{
    Init_drop_frames(frmBuffer);

    //Artifacts_Detection(frmBuffer);

    if(uiPatternNumber == 0)
        Pattern32RemovalCM(frmBuffer, uiInitFramePosition, pdispatch, parity);
    else if(uiPatternNumber <= 2)
        Pattern2332Removal(frmBuffer, uiInitFramePosition, pdispatch);
    else if(uiPatternNumber == 3)
        Pattern41aRemovalCM(frmBuffer, uiInitFramePosition, pdispatch, parity);
    else
    {
        throw (int) -16;
        //printf("\nUnknown Pattern, please check your selection\n");
        //exit(-1001);
    }
    Clean_Frame_Info(frmBuffer);
}

void CheckGenFrameCM(Frame **pfrmIn, unsigned int frameNum, /*unsigned int patternType, */unsigned int uiOPMode)
{
    bool stop = false;
    if ((pfrmIn[frameNum]->frmProperties.candidate/* || patternType == 0*/) && (uiOPMode == 0 || uiOPMode == 3))
    {
        pdeinterlaceFilter->MeasureRs(pfrmIn[frameNum]);
        pfrmIn[frameNum]->frmProperties.interlaced = false;
        Artifacts_Detection_frame(pfrmIn,frameNum);
        if(pfrmIn[frameNum]->frmProperties.interlaced)
        {
            ///FillBaseLinesIYUV(pfrmIn[frameNum], pfrmIn[BUFMINSIZE], false, false);
            pdeinterlaceFilter->DeinterlaceMedianFilterCM(pfrmIn, frameNum);
        }
    }
}

void Prepare_frame_for_queueCM(Frame **pfrmOut, Frame *pfrmIn, unsigned int uiWidth, unsigned int uiHeight, frameSupplier* frmSupply, bool bCreate) 
{
    assert(0 != pfrmOut);
    assert(pfrmIn != NULL && pfrmIn->inSurf != NULL && pfrmIn->outSurf != NULL);
    if(!pfrmOut || !pfrmIn)
        return;
    *pfrmOut = (Frame *)malloc(sizeof(Frame));
    if(!*pfrmOut)
        return;
    memset(*pfrmOut, 0, sizeof(Frame));
    assert(*pfrmOut != NULL);
    Frame_CreateCM(*pfrmOut, uiWidth, uiHeight, uiWidth / 2, uiHeight / 2, 64, bCreate);
    //if(frmSupply && !bCreate)
    //    static_cast<CmSurface2DEx*>((*pfrmOut)->outSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();
    (*pfrmOut)->frmProperties.tindex = pfrmIn->frmProperties.tindex;
#ifdef CPUPATH
    ReSample(*pfrmOut, pfrmIn);
#endif
    std::swap((*pfrmOut)->outSurf, pfrmIn->outSurf);
    std::swap((*pfrmOut)->inSurf, pfrmIn->inSurf);
    std::swap((*pfrmOut)->outState, pfrmIn->outState);
    (*pfrmOut)->frmProperties.detection = pfrmIn->frmProperties.detection;
    if(frmSupply && Frame::OUT_UNCHANGED == (*pfrmOut)->outState)
    {
        if(static_cast<CmSurface2DEx*>((*pfrmOut)->inSurf)->pCmSurface2D)
        {
            if(!static_cast<CmSurface2DEx*>((*pfrmOut)->outSurf)->pCmSurface2D)
                static_cast<CmSurface2DEx*>((*pfrmOut)->outSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();
            frmSupply->CMCopyGPUToGpu(static_cast<CmSurface2DEx*>((*pfrmOut)->outSurf)->pCmSurface2D, static_cast<CmSurface2DEx*>((*pfrmOut)->inSurf)->pCmSurface2D);
            (*pfrmOut)->outState = Frame::OUT_PROCESSED;
        }
        else
            assert(false);
    }
    assert(static_cast<CmSurface2DEx*>((*pfrmOut)->outSurf)->pCmSurface2D != 0);
}

void Frame_CreateCM(Frame *pfrmIn, unsigned int uiYWidth, unsigned int uiYHeight, unsigned int uiUVWidth, unsigned int uiUVHeight, unsigned int uiBorder, bool bcreate)
{
#ifdef CPUPATH
    Frame_Create(pfrmIn, uiYWidth, uiYHeight, uiUVWidth, uiUVHeight, uiBorder);
#else
    unsigned int uiOffset;
    pfrmIn->uiSize = (uiYWidth + 2 * uiBorder) * (uiYHeight + 2 * uiBorder) + (2 * (uiUVWidth + 2 * uiBorder) * (uiUVHeight + 2 * uiBorder));
    //pfrmIn->ucMem = (unsigned char *)malloc(pfrmIn->uiSize);
    //assert(pfrmIn->inSurf == 0);
    //assert(pfrmIn->outSurf == 0);
    pfrmIn->ucMem = NULL;
    pfrmIn->inSurf = NULL;
    pfrmIn->outSurf = NULL;

    //if (pfrmIn->ucMem)
    {
        pfrmIn->frmProperties.drop = 0;
        pfrmIn->frmProperties.candidate = 0;
        pfrmIn->plaY.uiWidth = uiYWidth;
        pfrmIn->plaY.uiHeight = uiYHeight;
        pfrmIn->plaY.uiBorder = uiBorder;
        pfrmIn->plaY.uiSize = (uiYWidth + 2 * uiBorder) * (uiYHeight + 2 * uiBorder);
        pfrmIn->plaY.uiStride = uiYWidth + 2 * uiBorder;
        pfrmIn->plaY.ucData = pfrmIn->ucMem;
        pfrmIn->plaY.ucCorner = NULL;//pfrmIn->plaY.ucData + (pfrmIn->plaY.uiStride + 1) * pfrmIn->plaY.uiBorder;

        uiOffset = pfrmIn->plaY.uiStride * (uiYHeight + 2 * uiBorder);

        pfrmIn->plaU.uiWidth = uiUVWidth;
        pfrmIn->plaU.uiHeight = uiUVHeight;
        pfrmIn->plaU.uiBorder = uiBorder;
        pfrmIn->plaU.uiSize = (uiUVWidth + 2 * uiBorder) * (uiUVHeight + 2 * uiBorder);
        pfrmIn->plaU.uiStride = uiUVWidth + 2 * uiBorder;
        pfrmIn->plaU.ucData = NULL;//pfrmIn->ucMem + uiOffset;
        pfrmIn->plaU.ucCorner = NULL;//pfrmIn->plaU.ucData + (pfrmIn->plaU.uiStride + 1) * pfrmIn->plaU.uiBorder;

        uiOffset += pfrmIn->plaU.uiStride * (uiUVHeight + 2 * uiBorder);

        pfrmIn->plaV.uiWidth = uiUVWidth;
        pfrmIn->plaV.uiHeight = uiUVHeight;
        pfrmIn->plaV.uiBorder = uiBorder;
        pfrmIn->plaV.uiSize = (uiUVWidth + 2 * uiBorder) * (uiUVHeight + 2 * uiBorder);
        pfrmIn->plaV.uiStride = uiUVWidth + 2 * uiBorder;
        pfrmIn->plaV.ucData = NULL;//pfrmIn->ucMem + uiOffset;
        pfrmIn->plaV.ucCorner = NULL;//pfrmIn->plaV.ucData + (pfrmIn->plaV.uiStride + 1) * pfrmIn->plaV.uiBorder;
    }
    //else
    // pfrmIn->uiSize = 0;
#endif
    pdeinterlaceFilter->FrameCreateSurface(pfrmIn, bcreate);
}

void Frame_ReleaseCM(Frame *pfrmIn)
{
    pdeinterlaceFilter->FrameReleaseSurface(pfrmIn);
    Frame_Release(pfrmIn);
}

// FIXME error handling
//void LoadNextFrameInI420(HANDLE hIn, unsigned char *pucIn,  const char * pcFormat, Frame * pFrame, unsigned int uiSize, double dFrameRate)
//{
//    DWORD uiBytesRead = 0;
//    pFrame->frmProperties.fr = dFrameRate;
//    if (strcmp(pcFormat, "I420") == 0) {
//#ifdef CPUPATH
//        void* ucMem = pFrame->ucMem;
//        bool ferror = ReadFile(hIn, pFrame->ucMem, uiSize, &uiBytesRead, NULL);
//        pdeinterlaceFilter->WriteRAWI420ToGPUNV12(pFrame, ucMem);
//#else
//        void* ucMem = malloc(uiSize);
//        bool ferror = ReadFile(hIn, ucMem, uiSize, &uiBytesRead, NULL);
//        pdeinterlaceFilter->WriteRAWI420ToGPUNV12(pFrame, ucMem);
//        free(ucMem);
//#endif
//
//    } else if (strcmp(pcFormat, "UYVY") == 0) {
//        ;
//    } 
//}

unsigned int Convert_to_I420CM(unsigned char *pucIn, Frame *pfrmOut, const char *pcFormat, double dFrameRate)
{
    pfrmOut->frmProperties.fr = dFrameRate;
    if (strcmp(pcFormat, "I420") == 0)
    {
        ptir_memcpy_s(pfrmOut->ucMem, pfrmOut->uiSize, pucIn, pfrmOut->uiSize);
        return 1;
    }
    else if (strcmp(pcFormat, "UYVY") == 0)
    {
       // Convert_UYVY_to_I420(pucIn, pfrmOut);
        return 0;
    }
    else
        return 0;
}

void Frame_Prep_and_AnalysisCM(Frame **frmBuffer, const char *pcFormat, double dFrameRate, unsigned int uiframeBufferIndexCur, unsigned int uiframeBufferIndexNext, unsigned int uiTemporalIndex)
{
#if PRINTDEBUG
    double    *dSAD,
            *dRs;
    unsigned int dPicSize;

    dPicSize = frmBuffer[uiframeBufferIndexCur]->plaY.uiWidth * frmBuffer[uiframeBufferIndexCur]->plaY.uiHeight;
#endif

    //pdeinterlaceFilter->WriteRAWI420ToGPUNV12(frmBuffer[uiframeBufferIndexCur], frmBuffer[BUFMINSIZE]->ucMem);
    frmBuffer[uiframeBufferIndexCur]->frmProperties.tindex = uiTemporalIndex + 1;
    pdeinterlaceFilter->CalculateSADRs(frmBuffer[uiframeBufferIndexCur], frmBuffer[uiframeBufferIndexNext]);
    frmBuffer[uiframeBufferIndexCur]->frmProperties.processed = true;

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

void PTIRCM_Frame_Prep_and_Analysis(PTIRSystemBuffer *SysBuffer)
{
    if(SysBuffer->control.uiCur == 0)
    {
        SysBuffer->control.uiNext = 0;
        SysBuffer->control.uiFrame = 0;
    }
    //pdeinterlaceFilter->WriteRAWI420ToGPUNV12(SysBuffer->frmBuffer[SysBuffer->control.uiCur], SysBuffer->frmBuffer[BUFMINSIZE]->ucMem);
    SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.tindex = SysBuffer->control.uiFrame + 1;
    pdeinterlaceFilter->CalculateSAD(SysBuffer->frmBuffer[SysBuffer->control.uiCur], SysBuffer->frmBuffer[SysBuffer->control.uiNext]);
    pdeinterlaceFilter->MeasureRs(SysBuffer->frmBuffer[SysBuffer->control.uiCur]); //Inside this function I made the hack, I do RS sum for all columns but last one, and let the rest of the system continue.
    //pdeinterlaceFilter->CalculateSADRs(SysBuffer->frmBuffer[SysBuffer->control.uiCur], SysBuffer->frmBuffer[SysBuffer->control.uiNext]);
    Artifacts_Detection_frame(SysBuffer->frmBuffer, SysBuffer->control.uiCur);
    SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.processed = true;

    if(SysBuffer->control.uiCur == SysBuffer->control.uiNext)
    {
        SysBuffer->frmBuffer[SysBuffer->control.uiCur]->plaY.ucStats.ucSAD[0] = 99.999;
        SysBuffer->frmBuffer[SysBuffer->control.uiCur]->plaY.ucStats.ucSAD[1] = 99.999;
    }
    SysBuffer->control.uiCur++;
}

void Update_Frame_Buffer_CM(Frame** frmBuffer, unsigned int frameIndex, double dTimePerFrame, unsigned int uiisInterlaced, unsigned int uiInterlaceParity, unsigned int bFullFrameRate, Frame* frmIn, FrameQueue *fqIn, frameSupplier *frmSupply = 0, bool bCreate = false)
{
    bool restore = false;
    CmSurface2D *old_surf = 0;
    if (frmBuffer[frameIndex]->frmProperties.interlaced)
    {
        if(bFullFrameRate)
        {
            //static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->inSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();
            //frmSupply->CMCopyGPUToGpu(static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->inSurf)->pCmSurface2D, static_cast<CmSurface2DEx*>(frmBuffer[frameIndex]->inSurf)->pCmSurface2D);
            old_surf = static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->inSurf)->pCmSurface2D;
            static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->inSurf)->pCmSurface2D = static_cast<CmSurface2DEx*>(frmBuffer[frameIndex]->inSurf)->pCmSurface2D;
            restore = true;

            if(frmBuffer[BUFMINSIZE]->outState != Frame::OUT_UNCHANGED) //This is needed after pointer is copied
                frmBuffer[BUFMINSIZE]->outState = Frame::OUT_UNCHANGED;

            pdeinterlaceFilter->DeinterlaceMedianFilterSingleFieldCM(frmBuffer, frameIndex, uiInterlaceParity);
            if(static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D)
                //static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();
                pdeinterlaceFilter->DeinterlaceMedianFilterSingleFieldCM(frmBuffer, BUFMINSIZE, !uiInterlaceParity);
            else
                assert(0);
            //static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->inSurf)->pCmSurface2D = old_surf;
        }
        else
            pdeinterlaceFilter->DeinterlaceMedianFilterSingleFieldCM(frmBuffer, frameIndex, uiInterlaceParity);
     }
     else
         CheckGenFrameCM(frmBuffer, frameIndex, uiisInterlaced);

    Prepare_frame_for_queueCM(&frmIn,frmBuffer[frameIndex], frmBuffer[frameIndex]->plaY.uiWidth, frmBuffer[frameIndex]->plaY.uiHeight, frmSupply, false);
    if(!frmIn)
        return;
    ptir_memcpy_s(frmIn->plaY.ucStats.ucRs, sizeof(frmIn->plaY.ucStats.ucRs),frmBuffer[frameIndex]->plaY.ucStats.ucRs,sizeof(double) * 10);

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
        Prepare_frame_for_queueCM(&frmIn, frmBuffer[BUFMINSIZE], frmBuffer[frameIndex]->plaY.uiWidth, frmBuffer[frameIndex]->plaY.uiHeight, frmSupply, false); // Go to double frame rate
        if(!frmIn)
            return;
        ptir_memcpy_s(frmIn->plaY.ucStats.ucRs,sizeof(frmIn->plaY.ucStats.ucRs), frmBuffer[BUFMINSIZE]->plaY.ucStats.ucRs, sizeof(double)* 10);

        //Timestamp
        frmIn->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp;

        FrameQueue_Add(fqIn, frmIn);
    }
    
    if(restore)
        static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D = old_surf;

    frmBuffer[frameIndex]->frmProperties.drop = false;
    frmBuffer[frameIndex]->frmProperties.candidate = false;
}

void Update_Frame_BufferNEW_CM(Frame** frmBuffer, unsigned int frameIndex, double dCurTimeStamp, double dTimePerFrame, unsigned int uiisInterlaced, unsigned int uiInterlaceParity, unsigned int bFullFrameRate, Frame* frmIn, FrameQueue *fqIn, frameSupplier *frmSupply = 0, bool bCreate = false)
{
    bool restore = false;
    CmSurface2D *old_surf = 0;
    if (frmBuffer[frameIndex]->frmProperties.interlaced)
    {
        if(bFullFrameRate)
        {
            //static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->inSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();
            //frmSupply->CMCopyGPUToGpu(static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->inSurf)->pCmSurface2D, static_cast<CmSurface2DEx*>(frmBuffer[frameIndex]->inSurf)->pCmSurface2D);
            old_surf = static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->inSurf)->pCmSurface2D;
            static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->inSurf)->pCmSurface2D = static_cast<CmSurface2DEx*>(frmBuffer[frameIndex]->inSurf)->pCmSurface2D;
            restore = true;

            if(frmBuffer[BUFMINSIZE]->outState != Frame::OUT_UNCHANGED)//This is needed after pointer is copied
                frmBuffer[BUFMINSIZE]->outState = Frame::OUT_UNCHANGED;
            //pdeinterlaceFilter->DeinterlaceMedianFilterSingleFieldCM(frmBuffer, frameIndex, uiInterlaceParity);
            if(!static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D)
                static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();
            if(uiInterlaceParity)
                pdeinterlaceFilter->DeinterlaceMedianFilterCM(frmBuffer, BUFMINSIZE, frameIndex);
            else
                pdeinterlaceFilter->DeinterlaceMedianFilterCM(frmBuffer, frameIndex, BUFMINSIZE);
        }
        else
            pdeinterlaceFilter->DeinterlaceMedianFilterSingleFieldCM(frmBuffer, frameIndex, uiInterlaceParity);
     }
     else
         CheckGenFrameCM(frmBuffer, frameIndex, uiisInterlaced);

    Prepare_frame_for_queueCM(&frmIn,frmBuffer[frameIndex], frmBuffer[frameIndex]->plaY.uiWidth, frmBuffer[frameIndex]->plaY.uiHeight, frmSupply, false);
    if(!frmIn)
        return;
    ptir_memcpy_s(frmIn->plaY.ucStats.ucRs,sizeof(frmIn->plaY.ucStats.ucRs),frmBuffer[frameIndex]->plaY.ucStats.ucRs,sizeof(double) * 10);
           
    //Timestamp
    frmIn->frmProperties.timestamp = dCurTimeStamp;

    FrameQueue_Add(fqIn, frmIn);

    if (bFullFrameRate && uiisInterlaced == 1)
    {
        Prepare_frame_for_queueCM(&frmIn, frmBuffer[BUFMINSIZE], frmBuffer[frameIndex]->plaY.uiWidth, frmBuffer[frameIndex]->plaY.uiHeight, frmSupply, false); // Go to double frame rate
        if(!frmIn)
            return;
        ptir_memcpy_s(frmIn->plaY.ucStats.ucRs, sizeof(frmIn->plaY.ucStats.ucRs),frmBuffer[BUFMINSIZE]->plaY.ucStats.ucRs, sizeof(double)* 10);

        //Timestamp
        frmIn->frmProperties.timestamp = dCurTimeStamp + dTimePerFrame / 2;

        FrameQueue_Add(fqIn, frmIn);
    }

    if(restore)
        static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D = old_surf;
    
    frmBuffer[frameIndex]->frmProperties.drop = false;
    frmBuffer[frameIndex]->frmProperties.candidate = false;
}

int PTIRCM_AutoMode_FF(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiDeinterlace = 0,
                 uiNumFramesToDispatch;
    double cur_timestamp;
    unsigned int uiFramesLeft = SysBuffer->control.uiCur;

    if(SysBuffer->control.uiEndOfFrames)
    {
        if(SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.tindex != SysBuffer->control.uiFrame)
            SysBuffer->control.uiCur = max(1, SysBuffer->control.uiCur - 1);
    }

    Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Analyze_Buffer_Stats_Automode_CM(SysBuffer->frmBuffer, &SysBuffer->control.mainPattern, &SysBuffer->control.uiDispatch);
        if(SysBuffer->control.mainPattern.ucPatternFound)
        {
            SysBuffer->control.dTimePerFrame = Calculate_Resulting_timestamps(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch, SysBuffer->control.uiCur, SysBuffer->control.dBaseTime, &uiNumFramesToDispatch, SysBuffer->control.mainPattern.ucPatternType, SysBuffer->control.uiEndOfFrames);
            cur_timestamp = SysBuffer->frmBuffer[0]->frmProperties.timestamp;
            
            for (i = 0; i < uiNumFramesToDispatch; i++)
            {
                if (!SysBuffer->frmBuffer[i]->frmProperties.drop)
                {
                    Update_Frame_BufferNEW_CM(SysBuffer->frmBuffer, i, cur_timestamp, SysBuffer->control.dTimePerFrame, AUTOMODE, SysBuffer->control.uiInterlaceParity, FULLFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn, (frameSupplier*) SysBuffer->frmSupply, false);
                    cur_timestamp += SysBuffer->control.dTimePerFrame;
                }
                else
                    SysBuffer->frmBuffer[i]->frmProperties.drop = false;
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
                Update_Frame_BufferNEW_CM(SysBuffer->frmBuffer, 0, SysBuffer->frmBuffer[0]->frmProperties.timestamp, SysBuffer->control.dBaseTime, uiDeinterlace, SysBuffer->control.uiInterlaceParity, FULLFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn, (frameSupplier*) SysBuffer->frmSupply, false);
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

int PTIRCM_AutoMode_HF(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiDeinterlace = 0,
                 uiNumFramesToDispatch;
    double cur_timestamp;
    unsigned int uiFramesLeft = SysBuffer->control.uiCur;

    if(SysBuffer->control.uiEndOfFrames)
    {
        if(SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.tindex != SysBuffer->control.uiFrame)
            SysBuffer->control.uiCur = max(1, SysBuffer->control.uiCur - 1);
    }

    Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Analyze_Buffer_Stats_Automode_CM(SysBuffer->frmBuffer, &SysBuffer->control.mainPattern, &SysBuffer->control.uiDispatch);
        if(SysBuffer->control.mainPattern.ucPatternFound)
        {
            SysBuffer->control.dTimePerFrame = Calculate_Resulting_timestamps(SysBuffer->frmBuffer, SysBuffer->control.uiDispatch, SysBuffer->control.uiCur, SysBuffer->control.dBaseTime, &uiNumFramesToDispatch, SysBuffer->control.mainPattern.ucPatternType, SysBuffer->control.uiEndOfFrames);
            cur_timestamp = SysBuffer->frmBuffer[0]->frmProperties.timestamp;
            for (i = 0; i < uiNumFramesToDispatch; i++)
            {
                if (!SysBuffer->frmBuffer[i]->frmProperties.drop)
                {
                    Update_Frame_Buffer_CM(SysBuffer->frmBuffer, i, SysBuffer->control.dTimePerFrame, AUTOMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn, (frameSupplier*) SysBuffer->frmSupply, true);
                    cur_timestamp += SysBuffer->control.dTimePerFrame;
                }
                else
                {
                    SysBuffer->frmBuffer[i + 1]->frmProperties.timestamp = SysBuffer->frmBuffer[i]->frmProperties.timestamp;
                    SysBuffer->frmBuffer[i]->frmProperties.drop = false;
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
                if (!SysBuffer->frmBuffer[0]->frmProperties.drop)
                {
                    if (!(SysBuffer->control.mainPattern.uiInterlacedFramesNum == (BUFMINSIZE - 1)))
                    {
                        Update_Frame_BufferNEW_CM(SysBuffer->frmBuffer, 0, SysBuffer->frmBuffer[0]->frmProperties.timestamp, SysBuffer->control.dTimePerFrame, AUTOMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn, (frameSupplier*) SysBuffer->frmSupply, true);
                        Rotate_Buffer(SysBuffer->frmBuffer);
                    }
                    else
                    {
                        Update_Frame_BufferNEW_CM(SysBuffer->frmBuffer, 0, SysBuffer->frmBuffer[0]->frmProperties.timestamp, SysBuffer->control.dTimePerFrame, AUTOMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn, (frameSupplier*) SysBuffer->frmSupply, true);
                        Rotate_Buffer_deinterlaced(SysBuffer->frmBuffer);
                    }
                }
                else
                    SysBuffer->frmBuffer[0]->frmProperties.drop = false;
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

int PTIRCM_DeinterlaceMode_FF(PTIRSystemBuffer *SysBuffer)
{
    if(SysBuffer->control.uiCur >= 1)
    {
        SysBuffer->frmBuffer[0]->frmProperties.interlaced = 1;
        Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
        SysBuffer->control.dTimePerFrame = SysBuffer->control.dBaseTime / 2;
        Update_Frame_Buffer_CM(SysBuffer->frmBuffer, 0, SysBuffer->control.dTimePerFrame, DEINTERLACEMODE, SysBuffer->control.uiInterlaceParity, FULLFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn, (frameSupplier*) SysBuffer->frmSupply, false);
        Rotate_Buffer_deinterlaced(SysBuffer->frmBuffer);
        return READY;
    }
    else
        return NOTREADY;
}

int PTIRCM_DeinterlaceMode_HF(PTIRSystemBuffer *SysBuffer)
{
    if(SysBuffer->control.uiCur >= 1)
    {
        SysBuffer->frmBuffer[0]->frmProperties.interlaced = 1;
        Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
        //FillBaseLinesIYUV(SysBuffer->frmBuffer[0], SysBuffer->frmBuffer[BUFMINSIZE], SysBuffer->control.uiInterlaceParity, SysBuffer->control.uiInterlaceParity);
        Update_Frame_Buffer_CM(SysBuffer->frmBuffer, 0, SysBuffer->control.dBaseTime, DEINTERLACEMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
        Rotate_Buffer_deinterlaced(SysBuffer->frmBuffer);
        return READY;
    }
    else
        return NOTREADY;
}

int PTIRCM_BaseFrameMode(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiDeinterlace = 0,
                 uiNumFramesToDispatch;
    unsigned int uiFramesLeft = SysBuffer->control.uiCur;

    if(SysBuffer->control.uiEndOfFrames)
    {
        if(SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.tindex != SysBuffer->control.uiFrame)
            SysBuffer->control.uiCur = max(1, SysBuffer->control.uiCur - 1);
    }

    Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Analyze_Buffer_Stats_Automode_CM(SysBuffer->frmBuffer, &SysBuffer->control.mainPattern, &SysBuffer->control.uiDispatch);
        if(!SysBuffer->control.uiEndOfFrames)
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur + 1);
        else
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur + 1);

        Interlaced_detection_logCM(SysBuffer->frmBuffer, uiNumFramesToDispatch);
        if(SysBuffer->control.mainPattern.ucPatternFound)
        {
            for (i = 0; i < uiNumFramesToDispatch; i++)
                Update_Frame_Buffer_CM(SysBuffer->frmBuffer, i, SysBuffer->control.dBaseTime, BASEFRAMEMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
            Rotate_Buffer_borders(SysBuffer->frmBuffer, uiNumFramesToDispatch);
            SysBuffer->control.uiCur -= min(uiNumFramesToDispatch,SysBuffer->control.uiCur);
        }
        else
        {
            for(i = 0; i < uiNumFramesToDispatch; i++)
            {
                Update_Frame_Buffer_CM(SysBuffer->frmBuffer, 0, SysBuffer->control.dBaseTime, BASEFRAMEMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
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

int PTIRCM_BaseFrameMode_NoChange(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiDeinterlace = 0,
                 uiNumFramesToDispatch;

    Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Artifacts_Detection(SysBuffer->frmBuffer);
        uiNumFramesToDispatch = min(BUFMINSIZE - 1, SysBuffer->control.uiCur);
        Interlaced_detection_logCM(SysBuffer->frmBuffer, uiNumFramesToDispatch);
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

int PTIRCM_Auto24fpsMode(PTIRSystemBuffer *SysBuffer)
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

    Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        Analyze_Buffer_Stats_CM(SysBuffer->frmBuffer, &SysBuffer->control.mainPattern, &SysBuffer->control.uiDispatch, &uiTelecineMode);
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
                    Update_Frame_Buffer_CM(SysBuffer->frmBuffer, i, filmfps, TELECINE24FPSMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                else
                {
                    SysBuffer->frmBuffer[i + 1]->frmProperties.timestamp = SysBuffer->frmBuffer[i]->frmProperties.timestamp;
                    SysBuffer->frmBuffer[i]->frmProperties.drop24fps = false;
                    SysBuffer->frmBuffer[i]->frmProperties.drop = false;
                    if((SysBuffer->control.uiBufferCount + 1 + i == 5) && !uiLetGo)
                        SysBuffer->control.uiBufferCount = 0;
                    if(uiCheckCount)
                        uiDone = true;
                }
            }
            Rotate_Buffer_borders(SysBuffer->frmBuffer, uiNumFramesToDispatch);
            SysBuffer->control.uiCur -= min(SysBuffer->control.uiDispatch,SysBuffer->control.uiCur);
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
                if (!SysBuffer->frmBuffer[0]->frmProperties.drop24fps && !SysBuffer->frmBuffer[i]->frmProperties.drop)
                {
                    SysBuffer->control.uiBufferCount++;

                    if ((SysBuffer->control.uiBufferCount < (BUFMINSIZE - 1)) && uiCheckCount)
                        Update_Frame_Buffer_CM(SysBuffer->frmBuffer, 0, filmfps, TELECINE24FPSMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn);
                    else
                    {
                        SysBuffer->control.uiBufferCount = 0;
                        SysBuffer->frmBuffer[1]->frmProperties.timestamp = SysBuffer->frmBuffer[0]->frmProperties.timestamp;
                        uiDone = 0;
                    }
                }
                else
                {
                    SysBuffer->frmBuffer[0]->frmProperties.drop24fps = false;
                    SysBuffer->frmBuffer[0]->frmProperties.drop = false;
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

int PTIRCM_FixedTelecinePatternMode(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i, uiTelecineMode = TELECINE24FPSMODE,
                 uiNumFramesToDispatch, uiLetGo = 0;
    const double filmfps = 1 / 23.976 * 1000;

    Frame_Prep_and_AnalysisCM(SysBuffer->frmBuffer, "I420", SysBuffer->control.dFrameRate, SysBuffer->control.uiCur, SysBuffer->control.uiNext, SysBuffer->control.uiFrame);
    if(SysBuffer->control.uiCur == BUFMINSIZE - 1 || SysBuffer->control.uiEndOfFrames)
    {
        RemovePatternCM(SysBuffer->frmBuffer, SysBuffer->control.uiPatternTypeInit, SysBuffer->control.uiPatternTypeNumber, &SysBuffer->control.uiDispatch, SysBuffer->control.uiInterlaceParity);

        if(!SysBuffer->control.uiEndOfFrames)
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur + 1);
        else
            uiNumFramesToDispatch = min(SysBuffer->control.uiDispatch, SysBuffer->control.uiCur);

        for(i = 0; i < uiNumFramesToDispatch; i++)
        {
            if(!SysBuffer->frmBuffer[i]->frmProperties.drop)
                Update_Frame_Buffer_CM(SysBuffer->frmBuffer, i, filmfps, TELECINE24FPSMODE, SysBuffer->control.uiInterlaceParity, HALFFRAMERATEMODE, SysBuffer->frmIn, &SysBuffer->fqIn, (frameSupplier*) SysBuffer->frmSupply, true);
            else
            {
                SysBuffer->frmBuffer[i + 1]->frmProperties.timestamp = SysBuffer->frmBuffer[i]->frmProperties.timestamp;
                SysBuffer->frmBuffer[i]->frmProperties.drop = false;
            }
        }
        Rotate_Buffer_borders(SysBuffer->frmBuffer, uiNumFramesToDispatch);
        SysBuffer->control.uiCur -= min(SysBuffer->control.uiDispatch,SysBuffer->control.uiCur);

        SysBuffer->control.uiCur++;
        return READY;
    }
    else
    {
        SysBuffer->control.uiCur++;
        return NOTREADY;
    }
}

int PTIRCM_MultipleMode(PTIRSystemBuffer *SysBuffer, unsigned int uiOpMode)
{
    if(uiOpMode == 0)
        return(PTIRCM_AutoMode_FF(SysBuffer));
    else if(uiOpMode == 1)
        return(PTIRCM_AutoMode_HF(SysBuffer));
    else if(uiOpMode == 2)
        return(PTIRCM_DeinterlaceMode_FF(SysBuffer));
    else if(uiOpMode == 3)
        return(PTIRCM_DeinterlaceMode_HF(SysBuffer));
    else if(uiOpMode == 4)
        return(PTIRCM_Auto24fpsMode(SysBuffer));
    else if(uiOpMode == 5)
        return(PTIRCM_FixedTelecinePatternMode(SysBuffer));
    else if(uiOpMode == 6)
        return(PTIRCM_BaseFrameMode(SysBuffer));
    else if (uiOpMode == 7)
        return(PTIRCM_BaseFrameMode_NoChange(SysBuffer));
    else
    {
        throw (int) -16;
    }
}

int PTIRCM_SimpleMode(PTIRSystemBuffer *SysBuffer, unsigned int uiOpMode)
{
    if (uiOpMode)
        return(PTIRCM_BaseFrameMode_NoChange(SysBuffer));
    else
        return(PTIRCM_BaseFrameMode(SysBuffer));
        
}

void PTIRCM_Init(PTIRSystemBuffer *SysBuffer, unsigned int _uiInWidth, unsigned int _uiInHeight, double _dFrameRate, unsigned int _uiInterlaceParity, unsigned int _uiPatternTypeNumber, unsigned int _uiPatternTypeInit)
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
    SysBuffer->control.uiSize = SysBuffer->control.uiInWidth * SysBuffer->control.uiInHeight * 2;//3 / 2;
    SysBuffer->control.uiEndOfFrames = 0;
    SysBuffer->control.pts = 0.0;
    SysBuffer->control.frame_duration = 1000 / _dFrameRate;

    for(i = 0; i < LASTFRAME; i++)
    {
        SysBuffer->frmIO[i] = (Frame*)malloc(sizeof(Frame));
        Frame_CreateCM(SysBuffer->frmIO[i], SysBuffer->control.uiInWidth, SysBuffer->control.uiInHeight, SysBuffer->control.uiInWidth / 2, SysBuffer->control.uiInHeight / 2, 0, false);
        SysBuffer->frmBuffer[i] = SysBuffer->frmIO[i];
        SysBuffer->frmBuffer[i]->frmProperties.candidate = false;
        SysBuffer->frmBuffer[i]->frmProperties.detection = false;
        SysBuffer->frmBuffer[i]->frmProperties.drop = false;
        SysBuffer->frmBuffer[i]->frmProperties.drop24fps = false;
        SysBuffer->frmBuffer[i]->frmProperties.fr = 0.0;
        SysBuffer->frmBuffer[i]->frmProperties.interlaced = false;
        SysBuffer->frmBuffer[i]->frmProperties.parity = 0;
        SysBuffer->frmBuffer[i]->frmProperties.processed = false;
        SysBuffer->frmBuffer[i]->frmProperties.timestamp = 0.0;
        SysBuffer->frmBuffer[i]->frmProperties.tindex = 0;
        SysBuffer->frmBuffer[i]->frmProperties.uiInterlacedDetectionValue = 0;
    }
    SysBuffer->frmIO[LASTFRAME] = (Frame*)malloc(sizeof(Frame));
    Frame_CreateCM(SysBuffer->frmIO[LASTFRAME], SysBuffer->control.uiWidth, SysBuffer->control.uiHeight, SysBuffer->control.uiWidth / 2, SysBuffer->control.uiHeight / 2, 0, false);
    FrameQueue_Initialize(&SysBuffer->fqIn);
    Pattern_init(&SysBuffer->control.mainPattern);
    SysBuffer->frmIn = NULL;
}

void PTIRCM_PutFrame(unsigned char *pucIO, PTIRSystemBuffer *SysBuffer, double dTimestamp)
{
    unsigned int status = 0;
    SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.timestamp = dTimestamp;
    status = Convert_to_I420CM(pucIO, SysBuffer->frmBuffer[BUFMINSIZE], "I420", SysBuffer->control.dFrameRate);
    if(status)
        SysBuffer->control.uiFrameCount += status;
    else
        exit(-1000);
}

void PTIRCM_Clean(PTIRSystemBuffer *SysBuffer)
{
    unsigned int i;
    if(!SysBuffer)
        return;

    for (i = 0; i <= LASTFRAME; ++i)
    {
        Frame_ReleaseCM(SysBuffer->frmIO[i]);
        if(SysBuffer->frmIO[i])
        {
            free(SysBuffer->frmIO[i]);
            SysBuffer->frmIO[i] = 0;
        }
    }
}
