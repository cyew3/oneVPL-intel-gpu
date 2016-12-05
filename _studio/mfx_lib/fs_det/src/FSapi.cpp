//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: FSApi.cpp
 *
 * Face & Skin Tone detection Api
 * 
 ********************************************************************************/
#include "FSapi.h"
#include "Def.h"

#include "Video.h"
#include "SkinDetBuff.h"
#include "SkinDetAlg.h"

// Globals
int g_FS_OPT_init = 0;
int g_FS_OPT_AVX2 = 0;
int g_FS_OPT_SSE4 = 0;

struct struct_FS {
    int POC;
    uint numSlice;
    int mode;
    int use_nv12;
    int w_orig;
    int h_orig;
    int w;
    int h;
    int wb;
    int hb;
    int ext;                    //flag indicating if frame is extended to multiple of 16

    SkinDetParameters sd_par;
    Sizes sz;                   //buffer sizes
    SkinDetBuff sd_buf;         //skin-detection buffer structure
    int bsf;                    //blocksize for face detection
    FDet fd;                    //face-detection structure
    SDet sd_struct;             //skin-detection algorithm structure

    FrameBuffElementPtr *fbpT[9];
    FrameBuffElementPtr *fbp;

    SliceInfoNv12 sliceData;
    unsigned short cpu_feature;
};


void FS_set_POC(FS *fs, int POC)
{
    fs->POC = POC;
}

void FS_set_SChg(FS *fs, int SChg)
{
    fs->sd_struct.bSceneChange = SChg;
}

int FS_get_NumSlice(FS *fs) 
{
    return fs->numSlice;
}


int FS_Init(FSP *fsp, int w_orig, int h_orig, int mode, int use_nv12, unsigned short cpu_feature) 
{
    int failed = 0;
    *fsp = (FS*) malloc(sizeof(struct_FS));
    if (*fsp == NULL) return 1;
    memset(*fsp, 0, sizeof(struct_FS));

    FS *fs = *fsp;

    fs->mode = mode;
    fs->use_nv12 = use_nv12;
    fs->w_orig = w_orig;
    fs->h_orig = h_orig;

    fs->w  = Allign16(w_orig);
    fs->h  = Allign16(h_orig);

    fs->wb = fs->w / BLOCKSIZE;
    fs->hb = fs->h / BLOCKSIZE;

    if (fs->w!=w_orig || fs->h!=h_orig) fs->ext = 1;
    else                                fs->ext = 0;

    GetBufferSizes(&fs->sz, fs->w, fs->h,  BLOCKSIZE);
    fs->bsf = get_bsz_fd(fs->w, fs->h);
    
    SetDefaultParameters(&fs->sd_par);
    
    // Buffers
    failed = AllocateBuffers(&fs->sd_buf, &fs->sz);
    if(failed) return failed;
    // SkinDet
    failed = SkinDetectionInit(&fs->sd_struct, fs->w, fs->h, fs->mode, &fs->sd_par);
    if(failed) return failed;
    // FaceDet
    failed = FDet_Alloc(&fs->fd, fs->w, fs->h, fs->bsf);
    if(failed) return failed;
    // Buffer Ptrs
    failed = InitFrameBuffPtr(&fs->fbp, &fs->sd_struct.dim, fs->mode, fs->use_nv12);
    if(failed) return failed;
    // Init frame list
    for(uint i = 0; i < fs->sd_struct.dim.numFr; i++)
        fs->fbpT[i] = &fs->fbp[i];
    // Init CPU
    fs->cpu_feature = cpu_feature;
    if(g_FS_OPT_init == 0) {
        switch (cpu_feature) {
        case 2:     g_FS_OPT_AVX2 = 1; g_FS_OPT_SSE4 = 1; break;
        case 1:     g_FS_OPT_AVX2 = 0; g_FS_OPT_SSE4 = 1; break;
        default:
        case 0:     g_FS_OPT_AVX2 = 0; g_FS_OPT_SSE4 = 0; break;
        }
        g_FS_OPT_init = 1;
    }
    // Init Slice
    fs->numSlice = 1;;
    fs->sliceData.picSizeInBlocks = fs->wb * fs->hb;
    fs->sliceData.numBlockRows[0] = fs->hb;
    fs->sliceData.numSampleRows[0] = fs->hb * BLOCKSIZE;
    fs->sliceData.numSampleRowsC[0] = fs->hb * BLOCKSIZE /2;

    return failed;
}


void FS_Free(FSP *fsp)
{
    FS *fs = *fsp;
    // Buffer Ptrs
    DeinitFrameBuffPtr(&fs->fbp, &fs->sd_struct.dim);
    // FaceDet
    FDet_Free(&fs->fd);
    // SkinDet
    SkinDetectionDeInit(&fs->sd_struct, fs->mode);
    // Buffers
    FreeBuffers(&fs->sd_buf);
    // object
    free(fs);
    *fsp = NULL;
}

void FS_set_Slice(FS *fs, int width, int height, int pitch, int pitchC, int numSlice) 
{
    int blkSize = BLOCKSIZE;
    int i, wb, hb, n, r;
    //    m_numSlice = numSlice;
    fs->numSlice = numSlice;
    wb = width / blkSize;
    hb = height / blkSize;
    fs->sliceData.picSizeInBlocks = wb * hb;
    n = hb / numSlice;
    r = hb % numSlice;

    fs->sliceData.numBlockRows[0] = (r>0) ? (n+1) : n;
    fs->sliceData.blockOffset[0] = 0;
    fs->sliceData.numSampleRows[0] = fs->sliceData.numBlockRows[0] * BLOCKSIZE;
    fs->sliceData.numSampleRowsC[0] = fs->sliceData.numSampleRows[0] / 2;
    fs->sliceData.sampleOffset[0] = 0;
    fs->sliceData.sampleOffsetC[0] = 0;
    r--;

    for (i=1; i<numSlice; i++) {
        fs->sliceData.numBlockRows[i] = (r>0) ? (n+1) : n;
        fs->sliceData.blockOffset[i] = fs->sliceData.blockOffset[i-1] + fs->sliceData.numBlockRows[i-1] * wb;
        fs->sliceData.numSampleRows[i] = fs->sliceData.numBlockRows[i] * 8;
        fs->sliceData.numSampleRowsC[i] = fs->sliceData.numSampleRows[i] / 2;
        fs->sliceData.sampleOffset[i] = fs->sliceData.sampleOffset[i-1] + fs->sliceData.numSampleRows[i-1] * pitch;
        fs->sliceData.sampleOffsetC[i] = fs->sliceData.sampleOffsetC[i-1] + fs->sliceData.numSampleRowsC[i-1] * pitchC;

        r--;
    }
}

void CopyFrameBuffPtr(FrameBuffElement *src, FrameBuffElementPtr *dst) 
{
    dst->frameY = src->frameY;
    dst->frameU = src->frameU;
    dst->frameV = src->frameV;
    dst->nv12 = src->nv12;
    dst->w = src->w;
    dst->h = src->h;
    dst->p = src->p;
    dst->pc = src->pc;
}


void FS_ProcessMode1_Slice_start(FS *fs, FrameBuffElement *in, BYTE *pOutY)
{
    fs->POC = in->poc;
    fs->sd_struct.tmpFr = pOutY;
    fs->sd_struct.mode = 1;

    // COPY FramePtrs
    CopyFrameBuffPtr(in, fs->fbpT[0]);

    if(!fs->sd_struct.bSceneChange)
    {
        //calculate motion
        fs->sd_struct.prvTotalSAD = 0;
        for(int k=0; k< NSLICES; k++) fs->sd_struct.prvSAD[k] = 0;
    } else {
        //subsample pixel-accurate frame to block-accurate YUV4:4:4
        SubsampleNV12(in->frameY, in->frameU, fs->sd_buf.pInYUV, fs->w, fs->h, in->p, BLOCKSIZE);
        //convert to different color spaces
        ConvertColorSpaces444(fs->sd_buf.pInYUV, fs->sd_buf.pYCbCr, fs->wb, fs->hb, fs->wb);

        // some non-slice processes 
        SkinDetectionMode1_NV12_slice_start(&fs->sd_struct, &fs->fd, fs->fbpT, fs->sd_buf.pInYUV, fs->sd_buf.pYCbCr, pOutY);
    }
}

void FS_ProcessMode1_Slice_main(FS *fs, int sliceId)
{
    // core slice-based processes 
    SkinDetectionMode1_NV12_slice_main(&fs->sd_struct, &fs->fd, fs->fbpT, fs->sd_buf.pInYUV, fs->sd_buf.pYCbCr, &fs->sliceData, sliceId);
}

void FS_ProcessMode1_Slice_end(FS *fs, BYTE *pOutY, BYTE *pOutLum)
{
    memcpy(pOutLum, fs->sd_buf.pInYUV, fs->wb*fs->hb);
    // core slice-based processes 
    SkinDetectionMode1_NV12_slice_end(&fs->sd_struct, &fs->fd, fs->fbpT, fs->sd_buf.pInYUV, fs->sd_buf.pYCbCr, pOutY);
    SwapFrameBuffPtrStackPointers(fs->fbpT, fs->sd_struct.dim.numFr); //swapping frame pointers
//#define DUMP_SEG
#ifdef DUMP_SEG
    {
        if(fs->POC == 0) memset(pOutY, 0, fs->wb*fs->hb);
        FILE *fp;
        char filename[100];
        sprintf(filename, "%s_%dx%d.seg", "hroi", fs->wb, fs->hb);
        if(fs->POC > 0) fp = fopen(filename, "ab");
        else            fp = fopen(filename, "wb");
        if(fp) {
            fwrite(pOutY, sizeof(char), fs->wb*fs->hb, fp);
            fclose(fp);
        }
    }
#endif
}

void FS_Luma_Slice_start(FS *fs, FrameBuffElement *in)
{
    // COPY FramePtrs
    CopyFrameBuffPtr(in, fs->fbpT[0]);
}

void FS_Luma_Slice_main(FS *fs, int sliceId)
{
    // core slice-based processes 
    AvgLuma_slice(fs->fbpT[0]->frameY, fs->sd_buf.pInYUV, fs->fbpT[0]->w, fs->fbpT[0]->h, fs->fbpT[0]->p, &fs->sliceData, sliceId);
}

void FS_Luma_Slice_end(FS *fs, BYTE *pOutLum)
{
    memcpy(pOutLum, fs->sd_buf.pInYUV, fs->wb*fs->hb);
}
