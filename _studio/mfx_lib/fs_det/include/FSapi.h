//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
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
void FS_ProcessMode1_Slice_end  (FS *fs, unsigned char *pOutY, unsigned char *pOutLum);

void FS_Luma_Slice_start(FS *fs, FrameBuffElement *in);
void FS_Luma_Slice_main(FS *fs, int sliceId);
void FS_Luma_Slice_end(FS *fs, unsigned char *pOutLum);

void FS_Free(FSP *fsp);

#endif

