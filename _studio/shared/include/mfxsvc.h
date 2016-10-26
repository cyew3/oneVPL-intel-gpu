//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __MFXSVC_H__
#define __MFXSVC_H__

#include "mfxdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CodecProfile, CodecLevel */
enum {
    MFX_PROFILE_AVC_SCALABLE_BASELINE = 83,
    MFX_PROFILE_AVC_SCALABLE_HIGH = 86
};

/* Extended Buffer Ids */
enum {
    MFX_EXTBUFF_SVC_SEQ_DESC        =   MFX_MAKEFOURCC('S','V','C','D'),
    MFX_EXTBUFF_SVC_RATE_CONTROL    =   MFX_MAKEFOURCC('S','V','C','R'),
    MFX_EXTBUFF_SVC_TARGET_LAYER    =   MFX_MAKEFOURCC('S','V','C','T'),
    MFX_EXTBUFF_VPP_SVC_DOWNSAMPLING  = MFX_MAKEFOURCC('D','W','N','S')
};

typedef struct {
    mfxExtBuffer    Header;

    mfxU16  RateControlMethod;
    mfxU16  reserved1[10];


    mfxU16  NumLayers;
    struct mfxLayer {
        mfxU16  TemporalId;
        mfxU16  DependencyId;
        mfxU16  QualityId;
        mfxU16  reserved2[5];

        union{
            struct{
                mfxU32  TargetKbps;
                mfxU32  InitialDelayInKB;
                mfxU32  BufferSizeInKB;
                mfxU32  MaxKbps;
                mfxU32  reserved3[4];
            } CbrVbr;

            struct{
                mfxU16  QPI;
                mfxU16  QPP;
                mfxU16  QPB;
            }Cqp;

            struct{
                mfxU32  TargetKbps;
                mfxU32  Convergence;
                mfxU32  Accuracy;
            }Avbr;
        };
    }Layer[1024];
} mfxExtSVCRateControl;

typedef struct {
    mfxU16    Active;

    mfxU16    Width;
    mfxU16    Height;

    mfxU16    CropX;
    mfxU16    CropY;
    mfxU16    CropW;
    mfxU16    CropH;

    mfxU16    RefLayerDid;
    mfxU16    RefLayerQid;

    mfxU16    GopPicSize;
    mfxU16    GopRefDist;
    mfxU16    GopOptFlag;
    mfxU16    IdrInterval;

    mfxU16    BasemodePred; /* four-state option, UNKNOWN/ON/OFF/ADAPTIVE */
    mfxU16    MotionPred;   /* four-state option, UNKNOWN/ON/OFF/ADAPTIVE */
    mfxU16    ResidualPred; /* four-state option, UNKNOWN/ON/OFF/ADAPTIVE */

    mfxU16    DisableDeblockingFilter;  /* tri -state option */
    mfxI16    ScaledRefLayerOffsets[4];
    mfxU16    ScanIdxPresent; /* tri -state option */
    mfxU16    reserved2[8];

    mfxU16   TemporalNum;
    mfxU16   TemporalId[8];

    mfxU16   QualityNum;

    struct mfxQualityLayer {
            mfxU16 ScanIdxStart; 
            mfxU16 ScanIdxEnd;

            mfxU16 TcoeffPredictionFlag; /* tri -state option */
            mfxU16 reserved3[5];
    } QualityLayer[16];

}  mfxExtSVCDepLayer;

typedef struct {
    mfxExtBuffer        Header;

    mfxU16              TemporalScale[8];
    mfxU16              RefBaseDist;
    mfxU16              reserved1[3];

    mfxExtSVCDepLayer   DependencyLayer[8];
} mfxExtSVCSeqDesc;

typedef struct {
    mfxExtBuffer    Header;

    mfxU16  TargetTemporalID;
    mfxU16  TargetDependencyID;
    mfxU16  TargetQualityID;
    mfxU16  reserved[9];
} mfxExtSvcTargetLayer ;

enum {
   MFX_DWNSAMPLING_ALGM_BEST_QUALITY    = 0x0001,
   MFX_DWNSAMPLING_ALGM_BEST_SPEED      = 0x0002
};

typedef struct {
    mfxExtBuffer    Header;
    mfxU16  Algorithm;
    mfxU16  reserved[11];
} mfxExtSVCDownsampling;

#ifdef __cplusplus
} // extern "C"
#endif

#endif

