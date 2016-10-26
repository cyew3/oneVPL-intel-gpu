//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include <Windows.h>
#include <stdio.h>
#include "pacm.h"
#include "utilities.h"

DeinterlaceFilter *pdeinterlaceFilter;

void ParseCommandLine(int argc, char *argv[], char *pcFileIn, char *pcFileOut, unsigned int *uiInWidth, unsigned int *uiInHeight, unsigned int *uiWidth, unsigned int *uiHeight, unsigned int *uiStart, unsigned int *uiCount, char *pcFormat, double *dFrameRate, unsigned int *uiisInterlaced, BOOL *bFullFrameRate)
{
    if (argc != 11)
    {
        printf("usage: <inputfile.yuv> <outputfile.yuv> <width> <height> <starting frame> <Number of frames to process> <I420> <fps> <Dectection mode> <Full framerate>\n");
        printf("Automatic detection example: JohnnyTest_1920x1080_30.yuv JohnnyTest_1920x1080_24.yuv 1920 1080 0 300 I420 29.976 0 0\n");
        printf("Deinterlace only example: Basketball_30_1080i.yuv Basketball_60_1080p.yuv 1920 1080 0 1302 I420 29.976 1 0\n");
        printf("Reverse Telecine only example: JohnnyTest_1920x1080_30.yuv JohnnyTest_1920x1080_24.yuv 1920 1080 0 300 I420 29.976 2 0\n");
        exit(2);
    }

    strcpy_s(pcFileIn, MAX_PATH, argv[1]);        // Source file
    strcpy_s(pcFileOut, MAX_PATH, argv[2]);        // Destination file

    *uiInWidth    = atoi(argv[3]);                // Source Width 
    *uiInHeight    = atoi(argv[4]);                // Source Height
    *uiStart    = atoi(argv[5]);                // Starting frame
    *uiCount    = atoi(argv[6]);                // Number of frames
    strcpy_s(pcFormat, 5, argv[7]);                // Frame Format
    *dFrameRate= atof(argv[8]);                    // Frame rate (fps)
    *uiisInterlaced = atoi(argv[9]);                // Automatic detection (0), Is video interlaced (1), Is video telecined (2)
    *bFullFrameRate = atoi(argv[10]);            // if a portion of the video is interlaced, it determines if number of frames to output will be the same or double.

    *uiWidth    = *uiInWidth;
    *uiHeight    = *uiInHeight;
}



// FIXME error handling
void LoadNextFrameInI420(HANDLE hIn, const char * pcFormat, Frame * pFrame, unsigned int uiSize, unsigned char * pucIn)
{
    DWORD uiBytesRead = 0;
    if (strcmp(pcFormat, "I420") == 0) {
#ifdef CPUPATH
        void* ucMem = pFrame->ucMem;
        BOOL ferror = ReadFile(hIn, pFrame->ucMem, uiSize, &uiBytesRead, NULL);
        pdeinterlaceFilter->WriteRAWI420ToGPUNV12(pFrame, ucMem);
#else
        void* ucMem = malloc(uiSize);
        BOOL ferror = ReadFile(hIn, ucMem, uiSize, &uiBytesRead, NULL);
        pdeinterlaceFilter->WriteRAWI420ToGPUNV12(pFrame, ucMem);
        free(ucMem);
#endif

    } else if (strcmp(pcFormat, "UYVY") == 0) {
        ;
    } 
}

#if 1
int main(int argc, char *argv[])
{
    try {    

        const char *
            cOperations[] = { "Loading", "Telecine Reverse / Deinterlace", "Saving", "end" },
            ctCodesLabel[] = { "# timecode format v2" };
        char
            pcFileIn[MAX_PATH],
            pcFileOut[MAX_PATH],
            pcFormat[5] = { 0 };
        unsigned int
            i,
            uiDeinterlacingMode = INTL_MODE_SMART_CHECK,
            uiDeinterlacingMeasure = 16,
            uiSupBuf,
            uiCur,
            uiCurTemp,
            uiNext,
            uiTimer,
            uiFrame,
            uiFrameOut,
            uiProgress,
            uiInWidth,
            uiInHeight,
            uiWidth,
            uiHeight,
            uiStart,
            uiCount,
            uiFrameCount,
            uiBufferCount,
            uiSize,
            uiisInterlaced,
            uiTeleCount = 0,
            uiLastFrameNumber = 0,
            uiDispatch;
        unsigned char
            *pucIO;
        Frame
            frmIO[LASTFRAME + 1],
            *frmIn,
            *frmBuffer[LASTFRAME];
        FrameQueue
            fqIn;
        DWORD
            uiBytesRead;
        double
            dTime[sizeof(cOperations) / sizeof(const char *)] = { 0 },
            *dSAD,
            *dRs,
            dPicSize,
            dTimeTotal,
            dFrameRate,
            dTimeStamp = 0.0,
            dBaseTime = 0.0,
            dOutBaseTime = 0.0,
            dBaseTimeSw = 0.0,
            dDeIntTime = 0.0;
        LARGE_INTEGER
            liTime[sizeof(cOperations) / sizeof(const char *)+1] = { 0 },
            liFreq = { 0 },
            liFileSize = { 0 };
        HANDLE
            hStd = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO
            sbInfo;
        HANDLE
            hIn,
            hOut;
        FILE
            *fTCodeOut = NULL;
        BOOL
            patternFound,
            ferror,
            bFullFrameRate;
        Pattern
            mainPattern;
        errno_t
            errorT;

        SetConsoleTitleA("Telecine Reverser & Deinterlacer");
        SetConsoleTextAttribute(hStd, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

        QueryPerformanceFrequency(&liFreq);

        ParseCommandLine(argc, argv, pcFileIn, pcFileOut, &uiInWidth, &uiInHeight, &uiWidth, &uiHeight, &uiStart, &uiCount, pcFormat, &dFrameRate, &uiisInterlaced, &bFullFrameRate);

        const char * pIsaFileNames[] = { "Deinterlace_genx.isa" };
        DeinterlaceFilter deinterlaceFilter(pIsaFileNames, sizeof(pIsaFileNames) / sizeof(const char *), uiInWidth, uiInHeight);
        pdeinterlaceFilter = &deinterlaceFilter;

        printf("%s -> %s (%s)\n", pcFileIn, pcFileOut, pcFormat);

        if (strcmp(pcFormat, "I420") == 0)
            uiSize = uiInWidth * uiInHeight * 3 / 2;
        else if (strcmp(pcFormat, "UYVY") == 0)
            uiSize = uiInWidth * uiInHeight * 2;
        else
            return 1;

        uiSupBuf = BUFMINSIZE;
        pucIO = (unsigned char *)malloc(uiSize);

        for (i = 0; i < LASTFRAME; i++)
        {
            Frame_CreateCM(&frmIO[i], uiInWidth, uiInHeight, uiInWidth / 2, uiInHeight / 2, 0);
            frmBuffer[i] = frmIO + i;
        }
        Frame_CreateCM(&frmIO[LASTFRAME], uiWidth, uiHeight, uiWidth / 2, uiHeight / 2, 0);

        FrameQueue_Initialize(&fqIn);

        Pattern_init(&mainPattern);

        hIn = CreateFileA(pcFileIn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hIn == INVALID_HANDLE_VALUE)
        {
            printf("Unable to open the YUV file, please check if the file exist\n");
            return 1;
        }
        GetFileSizeEx(hIn, &liFileSize);
        uiCount = (unsigned int)min(uiCount, (liFileSize.QuadPart / (uiWidth * uiHeight * 3 / 2)));
        if (uiStart > uiCount)
            uiStart = 0;
        else
            uiCount -= uiStart;

        SetFilePointer(hIn, uiSize * uiStart, NULL, FILE_BEGIN);


        errorT = fopen_s(&fTCodeOut, "timecodes.txt", "w+");
        errorT = fprintf(fTCodeOut, "#timecode format v2\n");

        dPicSize = uiWidth * uiHeight;
        //--------------------------------------------
        // First frame analysis
        //--------------------------------------------

        //----------------------------------------
        // Loading first frame
        //----------------------------------------
        LoadNextFrameInI420(hIn, pcFormat, frmBuffer[0], uiSize, pucIO);

        frmBuffer[0]->frmProperties.tindex = uiStart + 1;

        deinterlaceFilter.CalculateSADRs(frmBuffer[0], frmBuffer[0]);

        Artifacts_Detection_frame(frmBuffer, 0, TRUE);
        frmBuffer[0]->frmProperties.processed = FALSE;

        frmBuffer[0]->plaY.ucStats.ucSAD[0] = 99.999;
        frmBuffer[0]->plaY.ucStats.ucSAD[1] = 99.999;


#if PRINTX == 2
        dSAD = frmBuffer[0]->plaY.ucStats.ucSAD;
        dRs = frmBuffer[0]->plaY.ucStats.ucRs;
        printf("%i\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\n", 1, dSAD[0], dSAD[1], dSAD[2], dSAD[3], dSAD[4], dRs[0], dRs[1], dRs[2], dRs[3], dRs[4], dRs[5] / dPicSize * 1000, dRs[6], dRs[7] / dPicSize * 1000, dRs[8], dRs[9], dRs[10]);
#endif

        frmIn = NULL;

        hOut = CreateFileA(pcFileOut, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        GetConsoleScreenBufferInfo(hStd, &sbInfo);

        uiCur = 1;
        uiCurTemp = uiCur;
        uiBufferCount = 0;
        patternFound = FALSE;
        uiDispatch = 0;
        uiStart = 1;
        mainPattern.blendedCount = 0.0;
        mainPattern.uiIFlush = 0;
        mainPattern.uiPFlush = 0;

        dBaseTime = (1 / dFrameRate) * 1000;
        dDeIntTime = dBaseTime / 2;
        frmBuffer[0]->frmProperties.timestamp = dTimeStamp;
        if (uiisInterlaced == 2)
            dBaseTimeSw = (dBaseTime * 5 / 4);
        else
            dBaseTimeSw = dBaseTime;

        for (uiFrame = uiStart, uiFrameOut = 0, uiCur = 1; uiFrame < uiCount; ++uiFrame)
        {
            uiNext = uiCur - 1;

            uiProgress = 0;
            uiTimer = 0;

            if (uiFrame < uiStart + uiCount)
            {
                QueryPerformanceCounter(&liTime[uiTimer++]);
                //----------------------------------------
                // Loading
                //----------------------------------------
                LoadNextFrameInI420(hIn, pcFormat, frmBuffer[uiCur], uiSize, pucIO);

                QueryPerformanceCounter(&liTime[uiTimer++]);
                //----------------------------------------
                // Picture Format Conversion
                //----------------------------------------
                frmBuffer[uiCur]->frmProperties.tindex = uiFrame + 1;
                deinterlaceFilter.CalculateSADRs(frmBuffer[uiCur], frmBuffer[uiNext]);
                frmBuffer[uiCur]->frmProperties.processed = TRUE;
                dSAD = frmBuffer[uiCur]->plaY.ucStats.ucSAD;
                dRs = frmBuffer[uiCur]->plaY.ucStats.ucRs;
#if PRINTX == 2
                printf("%i\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\n", uiFrame + 1, dSAD[0], dSAD[1], dSAD[2], dSAD[3], dSAD[4], dRs[0], dRs[1], dRs[2], dRs[3], dRs[4], dRs[5] / dPicSize * 1000, dRs[6], dRs[7] / dPicSize * 1000, dRs[8], dRs[9], dRs[10]);
#endif
                if ((uiCur == BUFMINSIZE - 1) || (uiFrame == (uiCount - 1)))
                {                
                    Analyze_Buffer_Stats_CM(frmBuffer, &mainPattern, &uiDispatch, &uiisInterlaced);
                    if(mainPattern.ucPatternFound && uiisInterlaced != 1)
                    {
                        if (mainPattern.ucPatternType == 0 && uiisInterlaced != 2)
                            dOutBaseTime = dBaseTime;
                        else
                            dOutBaseTime = (dBaseTime * 5 / 4);

                        for (i = 0; i < min(uiDispatch, uiCur + 1); i++)
                        {
                            if (!frmBuffer[i]->frmProperties.drop)
                            {
                                CheckGenFrameCM(frmBuffer, i,/* mainPattern.ucPatternType,*/uiisInterlaced);
                                Prepare_frame_for_queueCM(&frmIn, frmBuffer[i], uiWidth, uiHeight);
                                ptir_memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[i]->plaY.ucStats.ucRs, sizeof(double)* 10);

                                //Timestamp
                                frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp + dOutBaseTime;
                                frmIn->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;

                                FrameQueue_Add(&fqIn, frmIn);
                            }
                            else
                            {
                                frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;
                                frmBuffer[i]->frmProperties.drop = FALSE;
                            }
                        }
                        Rotate_Buffer_borders(frmBuffer, uiDispatch);
                        uiCur -= min(uiDispatch, uiCur);
                        dBaseTimeSw = dOutBaseTime;
                    }
                    else
                    {
                        uiCurTemp = uiCur;
                        if (uiisInterlaced == 2)
                            dBaseTimeSw = (dBaseTime * 5 / 4);
                        for (i = 0; i < min(uiDispatch, uiCurTemp + 1); i++)
                        {
                            if (!frmBuffer[0]->frmProperties.drop)
                            {
                                uiBufferCount++;
                                if ((uiisInterlaced == 0 && !(mainPattern.uiInterlacedFramesNum == (BUFMINSIZE - 1))) || uiisInterlaced == 2)
                                {
                                    if ((uiisInterlaced == 2 && uiBufferCount < (BUFMINSIZE - 1)) || uiisInterlaced == 0)
                                    {
                                        if (frmBuffer[0]->frmProperties.interlaced)
                                        {
                                            deinterlaceFilter.DeinterlaceMedianFilterCM(frmBuffer, 0, BUFMINSIZE);
                                        }

                                        Prepare_frame_for_queueCM(&frmIn, frmBuffer[0], uiWidth, uiHeight);
                                        ptir_memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[0]->plaY.ucStats.ucRs, sizeof(double)* 10);

                                        //Timestamp
                                        frmBuffer[1]->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp + dBaseTimeSw;
                                        frmIn->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp;

                                        FrameQueue_Add(&fqIn, frmIn);
                                    }
                                    else
                                    {
                                        frmBuffer[1]->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp;// +dBaseTimeSw;
                                        uiBufferCount = 0;
                                    }
                                    Rotate_Buffer(frmBuffer);
                                }
                                else
                                {
                                    deinterlaceFilter.DeinterlaceMedianFilterCM(frmBuffer, 0, BUFMINSIZE);

                                    Prepare_frame_for_queueCM(&frmIn, frmBuffer[0], uiWidth, uiHeight); // Go to input frame rate
                                    ptir_memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[0]->plaY.ucStats.ucRs, sizeof(double)* 10);

                                    //Timestamp
                                    frmBuffer[BUFMINSIZE]->frmProperties.tindex = frmBuffer[0]->frmProperties.tindex;
                                    frmBuffer[BUFMINSIZE]->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp + dDeIntTime;
                                    frmBuffer[1]->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp + dDeIntTime;
                                    frmIn->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp;

                                    FrameQueue_Add(&fqIn, frmIn);
                                    if (bFullFrameRate && uiisInterlaced == 1)
                                    {
                                        Prepare_frame_for_queueCM(&frmIn, frmBuffer[BUFMINSIZE], uiWidth, uiHeight); // Go to double frame rate
                                        ptir_memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[BUFMINSIZE]->plaY.ucStats.ucRs, sizeof(double)* 10);

                                        //Timestamp
                                        frmIn->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp;

                                        FrameQueue_Add(&fqIn, frmIn);
                                    }
                                    Rotate_Buffer_deinterlaced(frmBuffer);
                                }
                            }
                            else
                            {
                                uiBufferCount = 0;
                                frmBuffer[0]->frmProperties.drop = FALSE;
                            }
                            uiCur--;
                        }
                    }

                }
                uiCur++;
            }
            else
            {
                QueryPerformanceCounter(&liTime[uiTimer++]);
                QueryPerformanceCounter(&liTime[uiTimer++]);
            }

            frmIn = NULL;
            QueryPerformanceCounter(&liTime[uiTimer++]);
            if (fqIn.iCount >= 1 && !mainPattern.ucPatternFound)
            {
                uiFrameCount = fqIn.iCount;
                for (i = 0; i < uiFrameCount; i++)
                {
                    uiLastFrameNumber = fqIn.pfrmHead->pfrmItem->frmProperties.tindex;
                    frmIn = FrameQueue_Get(&fqIn);
                    //----------------------------------------
                    // Saving Test Output
                    //----------------------------------------
                    if (frmIn)
                    {
                        ferror = FALSE;
#ifndef GPUPATH
                        TrimBorders(&frmIn->plaY, &frmIO[LASTFRAME].plaY);
                        TrimBorders(&frmIn->plaU, &frmIO[LASTFRAME].plaU);
                        TrimBorders(&frmIn->plaV, &frmIO[LASTFRAME].plaV);

                        fprintf(fTCodeOut, "%4.3lf\n", frmIn->frmProperties.timestamp);
                        ferror = WriteFile(hOut, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
#else
                        void* ucMem = malloc(frmIn->uiSize);
                        pdeinterlaceFilter->ReadRAWI420FromGPUNV12(frmIn, ucMem);
                        fprintf(fTCodeOut, "%4.3lf\n", frmIn->frmProperties.timestamp);
                        ferror = WriteFile(hOut, ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
                        free(ucMem);
#endif
#if PRINTX == 1
                        dSAD = frmIn->plaY.ucStats.ucSAD;
                        dRs = frmIn->plaY.ucStats.ucRs;
                        printf("%i\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%6.3lf\t%4.3lf\t%4.3lf\t%4.3lf\n", frmIn->frmProperties.tindex, dSAD[0], dSAD[1], dSAD[2], dSAD[3], dSAD[4], dRs[0], dRs[1], dRs[4], dRs[5]);
#endif
                        Frame_ReleaseCM(frmIn);
                        free(frmIn);

                        uiFrameOut++;
                    }
                }
            }
            else if (mainPattern.ucPatternFound)
            {
                mainPattern.ucPatternFound = FALSE;
                uiFrameCount = fqIn.iCount;
                for (i = 0; i < uiFrameCount; i++)
                {
                    ferror = FALSE;
                    uiLastFrameNumber = fqIn.pfrmHead->pfrmItem->frmProperties.tindex;
                    frmIn = FrameQueue_Get(&fqIn);
                    if (frmIn)
                    {
#ifndef GPUPATH
                        TrimBorders(&frmIn->plaY, &frmIO[LASTFRAME].plaY);
                        TrimBorders(&frmIn->plaU, &frmIO[LASTFRAME].plaU);
                        TrimBorders(&frmIn->plaV, &frmIO[LASTFRAME].plaV);

                        fprintf(fTCodeOut, "%4.3lf\n", frmIn->frmProperties.timestamp);
                        ferror = WriteFile(hOut, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
#else
                        void* ucMem = malloc(frmIn->uiSize);
                        pdeinterlaceFilter->ReadRAWI420FromGPUNV12(frmIn, ucMem);
                        fprintf(fTCodeOut, "%4.3lf\n", frmIn->frmProperties.timestamp);
                        ferror = WriteFile(hOut, ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
                        free(ucMem);
#endif

#if PRINTX == 1
                        dSAD = frmIn->plaY.ucStats.ucSAD;
                        dRs = frmIn->plaY.ucStats.ucRs;
                        printf("%i\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%6.3lf\t%4.3lf\t%4.3lf\t%4.3lf\n", frmIn->frmProperties.tindex, dSAD[0], dSAD[1], dSAD[2], dSAD[3], dSAD[4], dRs[0], dRs[1], dRs[4], dRs[5]);
#endif
                        Frame_ReleaseCM(frmIn);
                        free(frmIn);

                        uiFrameOut++;
                    }
                }
            }
            QueryPerformanceCounter(&liTime[uiTimer++]);

            // Update Time Counters
            for (i = 0; i < uiTimer - 1; ++i)
                dTime[i] += (double)(liTime[i + 1].QuadPart - liTime[i].QuadPart) / liFreq.QuadPart;
#if !PRINTX
            //----------------------------------------
            // Output
            //----------------------------------------
            //PrintOutputStats(hStd, sbInfo, uiFrame, uiStart, uiCount, &uiProgress, uiFrameOut, uiTimer, fqIn, cOperations, dTime);
#endif
        }
        if (uiCur)
        {
            for(i = 0; i < uiCur; i++)
            {
                if (!frmBuffer[i]->frmProperties.drop && (frmBuffer[i]->frmProperties.tindex > uiLastFrameNumber))
                {
                    if (uiisInterlaced == 1)
                    {
                        deinterlaceFilter.DeinterlaceMedianFilterCM(frmBuffer, i, BUFMINSIZE);

                        Prepare_frame_for_queueCM(&frmIn, frmBuffer[i], uiWidth, uiHeight); // Go to input frame rate
                        ptir_memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[i]->plaY.ucStats.ucRs, sizeof(double)* 10);

                        //Timestamp
                        frmBuffer[BUFMINSIZE]->frmProperties.tindex = frmBuffer[i]->frmProperties.tindex;
                        frmBuffer[BUFMINSIZE]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp + dDeIntTime;
                        frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp + dDeIntTime;
                        frmIn->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;

                        FrameQueue_Add(&fqIn, frmIn);
                        if (bFullFrameRate)
                        {
                            Prepare_frame_for_queueCM(&frmIn, frmBuffer[BUFMINSIZE], uiWidth, uiHeight); // Go to double frame rate
                            ptir_memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[BUFMINSIZE]->plaY.ucStats.ucRs, sizeof(double)* 10);

                            //Timestamp
                            frmIn->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp;

                            FrameQueue_Add(&fqIn, frmIn);
                        }
                    }
                    else
                    {
                        uiBufferCount++;
                        if (uiisInterlaced != 2 || (uiisInterlaced == 2 && uiBufferCount < 5))
                        {
                            CheckGenFrameCM(frmBuffer, i, mainPattern.ucPatternType,uiisInterlaced);
                            Prepare_frame_for_queueCM(&frmIn, frmBuffer[i], uiWidth, uiHeight);
                            ptir_memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[i]->plaY.ucStats.ucRs, sizeof(double)* 10);

                            //timestamp
                            frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp + (dBaseTime * 5 / 4);
                            frmIn->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;

                            FrameQueue_Add(&fqIn, frmIn);
                        }
                        else
                            uiBufferCount = 0;
                    }
                
                }
            }
            
            uiFrameCount = fqIn.iCount;
            for (i = 0; i < uiFrameCount; i++)
            {
                ferror = FALSE;
                frmIn = FrameQueue_Get(&fqIn);
                if (frmIn)
                {
#ifndef GPUPATH
                    TrimBorders(&frmIn->plaY, &frmIO[LASTFRAME].plaY);
                    TrimBorders(&frmIn->plaU, &frmIO[LASTFRAME].plaU);
                    TrimBorders(&frmIn->plaV, &frmIO[LASTFRAME].plaV);

                    fprintf(fTCodeOut, "%4.3lf\n", frmIn->frmProperties.timestamp);
                    ferror = WriteFile(hOut, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
#else
                    void* ucMem = malloc(frmIn->uiSize);
                    pdeinterlaceFilter->ReadRAWI420FromGPUNV12(frmIn, ucMem);
                    fprintf(fTCodeOut, "%4.3lf\n", frmIn->frmProperties.timestamp);
                    ferror = WriteFile(hOut, ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
                    free(ucMem);
#endif

#if PRINTX == 1
                    dSAD = frmIn->plaY.ucStats.ucSAD;
                    dRs = frmIn->plaY.ucStats.ucRs;
                    printf("%i\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%6.3lf\t%4.3lf\t%4.3lf\t%4.3lf\n", frmIn->frmProperties.tindex, dSAD[0], dSAD[1], dSAD[2], dSAD[3], dSAD[4], dRs[0], dRs[1], dRs[4], dRs[5]);
#endif
                    Frame_ReleaseCM(frmIn);
                    free(frmIn);

                    uiFrameOut++;
                }
            }
        }

        //----------------------------------------
        // Output
        //----------------------------------------
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
        //#endif

        SetConsoleTextAttribute(hStd, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

        free(pucIO);


        for (i = 0; i <= LASTFRAME; ++i)
            Frame_ReleaseCM(&frmIO[i]);

        fclose(fTCodeOut);
        CloseHandle(hIn);
        CloseHandle(hOut);

        return 0;
    }
    catch (std::exception e)
    {
        cout<<e.what()<<std::endl;
    }
}
#endif