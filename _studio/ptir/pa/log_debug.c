//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include "log_debug.h"

void Interlaced_detection_log(Frame *frmBuffer[BUFMINSIZE], unsigned int uiNumFramesToDispatch)
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
        frmBuffer[i]->frmProperties.detection = FALSE;
    }

    fclose(fLogDetection);
#endif

}