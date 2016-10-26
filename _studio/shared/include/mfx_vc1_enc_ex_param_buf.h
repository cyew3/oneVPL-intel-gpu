//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2009 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_ENC_EX_PARAM_BUF_H_
#define _MFX_ENC_EX_PARAM_BUF_H_

#include "mfxvideo.h"

/*------------------- CUCs definition ---------------------------------------*/

/*
    MFX_CUC_VC1_SAVEDMVBUF
    FrameCUC Extended buffer
    Buffer for storing MV from last reference frame.
    Necessary for direct MV calculation in B frames and fields.
    Structure: ExtVC1SavedMV
    MFX_LABEL_MVDATA
*/
#define  MFX_CUC_VC1_SAVEDMVBUF MFX_MAKEFOURCC('C','0','1','1')
/*
    MFX_CUC_VC1_SAVEDMVDIRBUF
    FrameCUC Extended buffer
    Buffer for storing MV direction from last reference frame.
    (most recent reference field or second most recent reference field)
    Necessary for direct MV calculation in B fields.
    Structure: ExtVC1SavedMVDirection
    MFX_LABEL_MVDATA
*/

#define  MFX_CUC_VC1_SAVEDMVDIRBUF MFX_MAKEFOURCC('C','1','1','1')

/*
    MFX_CUC_VC1_RESIDAUALBUF
    FrameCUC Extended buffer
    Buffer for storing residual coeffsients. Necessary in "FULL ENC" profile.
    Structure: MFX_LABEL_RESCOEFF
*/
#define  MFX_CUC_VC1_RESIDAUALBUF MFX_MAKEFOURCC('C','U','1','0')

/*
    MFX_CUC_VC1_EXPANDPARAMSBUF
    FrameCUC Extended buffer
    Buffer for storing FCM types and IC params of reference frames. Now it's used only in decoder. It will be used in encoder for intensity compensation
    Structure: ExtVC1RefFrameInfo
*/
#define  MFX_CUC_VC1_EXPANDPARAMSBUF MFX_MAKEFOURCC('C','U','C','2')

/*
    MFX_CUC_VC1_SEQPARAMSBUF
    VideoParams Extended buffer (it's used only for testing)
    Buffer for storing sequence headers parameters.
    Structure: ExtVC1RefFrameInfo
*/
#define  MFX_CUC_VC1_SEQPARAMSBUF MFX_MAKEFOURCC('C','U','C','1')


/*
MFX_CUC_VC1_TRELLISBUF
FrameCUC Extended buffer
Buffer for Round Control Parameters for residual coefficients. Used for trellis quantization.
Structure: MFX_LABEL_RESCOEFF
*/
//#define  MFX_CUC_VC1_TRELLISBUF MFX_MAKEFOURCC('C','U','1','1')

/*--------------- Additional structures ---------------------------------------*/

//struct sMV
//{
//    mfxI16        X;
//    mfxI16        Y;
//    mfxU8         bRefField;
//};

/*---------Extended buffer descriptions ----------------------------------------*/


// stores info about reference frame
struct ExtVC1SavedMV
{
    mfxU32         CucID;  // MFX_CUC_VC1_SAVEDMVBUF
    mfxU32         CucSz;
    mfxU16         numMVs; // nMB*2
    mfxI16*        pMVs;   // MV.x, MV.y
};
// stores info about reference frame
struct ExtVC1SavedMVDirection
{
    mfxU32         CucID;  // MFX_CUC_VC1_SAVEDMVDIRBUF
    mfxU32         CucSz;
    mfxU16         numMVs; // nMB
    mfxU8*         bRefField;// 0 (most recent reference field) or 1 (second most recent reference field)
};

//struct ExtVC1PredMV
//{
//    mfxU32         CucID;  // MFX_CUC_VC1_PREDMVBUF
//    mfxU32         CucSz;
//    mfxU16         numMVs; // nMB
//    sMV*           pMVs;
//};
//struct ExtVC1TrellisQuant
//{
//    mfxU32         CucID;  // MFX_CUC_VC1_TRELLISBUF
//    mfxU32         CucSz;  // nMB*64*8*sizeof(mfxI8) + sizeof (ExtVC1TrellisQuant)
//    mfxU16         numMBs; // nMB
//    mfxI8*         pRC;    // Pointer to Round control buffer
//};
struct ExtVC1SequenceParams
{
    mfxU32         CucID;            // MFX_CUC_VC1_SEQPARAMSBUF
    mfxU32         CucSz;
    mfxU8          quantizer:4;      //implicity, explicity,non-uniform, uniform
    mfxU8          dQuant:4;         // Should be equal to 0 in simple profile
    mfxI8          rangeMapY;        // advance
    mfxI8          rangeMapUV;       // advance

    // simple/main

    mfxI8          multiRes:1;        // Multi resolution coding
    mfxI8          syncMarkers;      // Should be equal to 0 in simple profile
    mfxU8          rangeRedution:1;

    // common

    mfxU8          deblocking:1;      // Should be equal to 0 in simple profile
    mfxU8          noFastUV:1;        // Should be equal to 0 in simple profile
    mfxU8          extendedMV:1;      // Should be equal to 0 in simple profile
    mfxU8          vsTransform:1;
    mfxU8          overlapSmoothing:2;  // [0-3] in advance profile: 0 - No smoothing, 
                                        //                           other - Smoothing is switch on, but for I frames:
                                        //                           1 - NO if (PQuant <9),
                                        //                           2 - SOME if (PQuant <9), 3 - ALL if (PQuant <9)
                                        // [0-1] in simple/main profiles
                                        // values 2 and 3 are used for UF profile
    mfxU8          intensityCompensation:1;


    // only for advance profile

    mfxU8          extendedDMV:1;
    mfxU8          closedEntryPoint:1 ;
    mfxU8          brokenLink:1      ; // if !m_bClosedEnryPoint -> true or false
    mfxU8          sizeInEntryPoint:1;
    mfxU8          referenceFrameDistance:1; 
   
    // only for user-friendly profile:

    mfxU8          mixed :1;        // 0 - switch of or 1 - switch on
    mfxU8          refFieldType:2;  // for P field: 0 - first reference; 1 - second reference field;  2 - both

    // motion estimation parameters

    mfxU8          MESpeed;
    mfxU8          useFB:1;
    mfxU8          fastFB:1;
    mfxU8          changeInterpolationType:1;
    mfxU8          changeVLCTables:1;
    mfxU8          trellisQuantization:1;

};
// need for new interpolation functions with on fly padding and IC
struct ExtVC1RefFrameInfo
{
    mfxU32         CucID;  // MFX_CUC_VC1_SEQPARAMSBUF
    mfxU32         CucSz;
    // FCM type of forward reference frame
    // 0 - Progressive
    // 1 - Interlace Field
    // 2 - Interlace Frame
    mfxU32        FCMForward;
    // FCM type of forward reference frame
    mfxU32        FCMBackward;

    // VC-1 standard allows double IC. We need to  check if one 
    // of reference fields should be compensated.
    // RefFieldComp = -1 - no compensation
    // RefFieldComp =  0 - top field compensation 
    // RefFieldComp =  1 - bottom field compensation
    mfxI32        RefFieldComp;
    // IC scale of field to compensation
    mfxU32        LUMSCALE;
    // IC shift of field to compensation
    mfxU32        LUMSHIFT;
};
struct ExtVC1Residual
{
    mfxU32         CucID;
    mfxU32         CucSz;
    mfxU16         numMbs; // nMB
    mfxI16*        pRes;
};

//
#endif //_MFX_ENC_EX_PARAM_BUF_H_
