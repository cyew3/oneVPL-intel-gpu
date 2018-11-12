// Copyright (c) 2012-2018 Intel Corporation
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
/********************************************************************************
 * 
 * File: FSApi.h
 *
 * Face Skin Top Level API
 * 
 ********************************************************************************/
#ifndef _FSAPI_H_
#define _FSAPI_H_

//Element of buffer of frames
typedef struct {
    int poc;
    unsigned char *frameY; //full frame
    unsigned char *frameU; //full frame
    unsigned char *frameV; //full frame
    int w;
    int h;
    int p;
    int pc;
    int nv12;
} FrameBuffElement;

typedef struct struct_FS FS;

typedef FS* FSP;

int FS_Init(FSP *fsp, int w_orig, int h_orig, int mode, int use_nv12, unsigned short cpu_feature);

void FS_set_POC(FS *fs, int POC);
void FS_set_SChg(FS *fs, int SChg);
void FS_set_Slice(FS *fs, int width, int height, int pitch, int pitchC, int numSlice);
int  FS_get_NumSlice(FS *fs);

void FS_ProcessMode1_Slice_start(FS *fs, FrameBuffElement *in, unsigned char *pOutY);
void FS_ProcessMode1_Slice_main (FS *fs, int sliceId);
void FS_ProcessMode1_Slice_end  (FS *fs, unsigned char *pOutY, unsigned char *pOutLum, int outSize);

void FS_Luma_Slice_start(FS *fs, FrameBuffElement *in);
void FS_Luma_Slice_main(FS *fs, int sliceId, unsigned char bitDepthLuma);
void FS_Luma_Slice_end(FS *fs, unsigned char *pOutLum, int outSize, unsigned char bitDepthLuma);

void FS_Free(FSP *fsp);

#endif

