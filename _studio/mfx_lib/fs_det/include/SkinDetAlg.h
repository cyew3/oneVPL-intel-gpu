//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
* 
* File: SkinDetAlg.h
*
* Structures and routines for skin detection algorithm
* 
********************************************************************************/
#ifndef _SKIN_DET_ALG_H_
#define _SKIN_DET_ALG_H_

#include "Def.h"
#include "Dim.h"
#include "Params.h"
#include "FrameBuff.h"
#include "FaceDet.h"
#include "SkinSegm.h"


typedef struct {
    const SkinDetParameters *prms;      //Skin Detection parameters
    Dim dim;                            //Dimensions of a frame
    uint frameNum;                      //Indicates if we are processing first frame or not
    int minSegSizeBl, maxSegSizeBl;

    int mode;                           //Mode of operation (0=full quality, 1=fast)

    uint* segmTmp;                      //Temporary buffer for color segments
    uint* segmHlp;                      //Additional temporary table for segmentation
    BYTE* blankFr;                      //Temporary frame
    BYTE* tmpFr;                        //Temporary frame
    BYTE* centersProb;                  //Probabilities of segments centers

    BYTE* motionMask;                   //Motion mask
    BYTE* curMotionMask;                //Motion mask
    BYTE* sadMax;                       //Maximal SAD of a frame
    BYTE* prvProb;                      //Previous probability values
    BYTE* skinProbP;                    //Skin Prob Pixel Level
    BYTE* skinProbB;                    //Skin Prob Block Level

    uint maxSegSz;                      //Maximal Segment Size for previous frame
    uint prvTotalSAD;                   //Total SAD for previous frame
    uint prvSAD[NSLICES];               //Total SAD for previous frame

    BOOL bSceneChange;                  //Scene Change Flag

    int square[256][256];               //Temporary for counting
    BYTE hist[256][256];

    int *stack;
    int stackptr;
    //int MAX_BLK_RESOLUTION;           // dim->blTotal

    int upd_skin;
    int Tk;
    int faceskin_perc;
    int hist_ind;
    uint sceneNum;
    BYTE *prvMask;
    uint yTh;                           //luma threshold
    uint bg;                            //background noise luma threshold
    FrmSegmFeat fsg[NUM_SEG_TRACK_FRMS];//frame-level segments tracking structure
} SDet;

typedef struct {
    haar_cascade cascade;               //Face detection classifier
    haarclass_cascade *class_cascade;   //Face detection classifier helper
    int *sum;                           //Integral image buffer
    double *sqsum;                      //Squared integral image buffer
    BYTE *mat;                          //Face detection input frame data buffer
    BYTE *mat_norm;                     //Face detection normalized input frame data buffer
    std::vector<Rct> *faces;            //Face detection classification results
    std::vector<Pt>  *prevfc;           //Previous face centroids used for tracking
} FDet;

typedef struct {
    uint slice_offset_fr[NSLICES];
    int  slice_nlines_fr[NSLICES];
    int  slice_nlines[NSLICES];
    uint slice_offset[NSLICES];
} SliceInfo;

typedef struct {
    int  picSizeInBlocks;
    int  numBlockRows[NSLICES];
    int  blockOffset[NSLICES];
    int  numSampleRows[NSLICES];
    int  numSampleRowsC[NSLICES];
    int  sampleOffset[NSLICES];
    int  sampleOffsetC[NSLICES];
} SliceInfoNv12;

//SD initialization
int SkinDetectionInit(SDet* sd, int w, int h, int mode, SkinDetParameters* prms);

//Frees resources that had been used during SD
void SkinDetectionDeInit(SDet* sd, int mode);

//FD initialization
void FDet_Init(FDet *fd);

int FDet_Alloc(FDet *fd, int w, int h, int bsf);

void FDet_Free(FDet *fd);

//Subsample Yuv from pixel to block accuracy
void SubsampleYUV12(BYTE* pSrc, BYTE* pDst, int width, int height, int pitch, int blksz);

//subsample Yuv from pixel to block accuracy (slice-based)
void SubsampleYUV12_slice(BYTE* pSrc, BYTE* pDst, int width, int height, int pitch, int blksz, uint slice_offset, uint slice_offset_fr, int slice_nlines_fr);

//Subsample NV12 from pixel to block accuracy
void SubsampleNV12(BYTE* pYSrc, BYTE *pUVSrc, BYTE* pYDst, int width, int height, int pitch, int blksz);

//subsample NV12 from pixel to block accuracy (slice-based)
void SubsampleNV12_slice(BYTE* pYSrc, BYTE* pUVSrc, BYTE* pDst, int width, int height, int pitch, int pitchC, int blksz, SliceInfoNv12 *si, int sliceId);

//Main function of MA FD
void SkinDetection(SDet *cFD, BYTE *pInYUV12, BYTE *pInYUV, BYTE *pInYCbCr, BYTE *pOut, FDet *fd, SliceInfo *si, int use_nv12, int use_slices);
void SkinDetectionMode1_NV12(SDet *sd, FrameBuffElementPtr **in, BYTE *pInYUV, BYTE *pInYCbCr, BYTE *pOut, FDet* fd);

void SkinDetectionMode1_NV12_slice_start(SDet *sd, FDet* fd, FrameBuffElementPtr **FrameBuff, BYTE *pInYUV, BYTE *pInYCbCr, BYTE *pOut);
void SkinDetectionMode1_NV12_slice_main (SDet *sd, FDet* fd, FrameBuffElementPtr **FrameBuff, BYTE *pInYUV, BYTE *pInYCbCr, SliceInfoNv12 *si, int sliceId);
void SkinDetectionMode1_NV12_slice_end  (SDet *sd, FDet* fd, FrameBuffElementPtr **FrameBuff, BYTE *pInYUV, BYTE *pInYCbCr, BYTE *pOut);

//
void AvgLuma_slice(BYTE *pYSrc, BYTE *pLuma, int w, int h, int p, SliceInfoNv12 *si, int sliceId);

#endif