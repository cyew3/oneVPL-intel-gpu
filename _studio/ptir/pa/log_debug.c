// Copyright (c) 2014-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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