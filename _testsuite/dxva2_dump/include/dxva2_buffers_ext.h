/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2009 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __DXVA_SPY_BUFFERS_H
#define __DXVA_SPY_BUFFERS_H

#include <windows.h>

//vc1 buffers extension to standard dxva buffers types
enum _DXVA2_Intel_ExtensionCompressedBuffers
{
    DXVA2_VC1PictureParametersExtBufferType = 20,
    DXVA2_VC1BitplaneBufferType             = 21
};

typedef struct _DXVA_Intel_VC1PictureParameters
{
    BYTE                    bBScaleFactor;
    BYTE                    bPQuant;
    BYTE                    bAltPQuant;

    union
    {
        BYTE                bPictureFlags;
        struct
        {
            BYTE            FrameCodingMode                 :2;
            BYTE            PictureType                     :3;
            BYTE            CondOverFlag                    :2;
            BYTE            Reserved1bit                    :1;
        };
    };

    union
    {
        BYTE                bPQuantFlags;
        struct
        {
            BYTE            PQuantUniform                   :1;
            BYTE            HalfQP                          :1;
            BYTE            AltPQuantConfig                 :2;
            BYTE            AltPQuantEdgeMask               :4;
        };
    };

    union 
    {
        BYTE                bMvRange;
        struct
        {
            BYTE            ExtendedMVrange                 :2;
            BYTE            ExtendedDMVrange                :2;
            BYTE            Reserved4bits                   :4;
        };
    };

    union 
    {
        WORD                wMvReference;
        struct
        {
            WORD            ReferenceDistance               :4;
            WORD            BwdReferenceDistance            :4;
            WORD            NumberOfReferencePictures       :1;
            WORD            ReferenceFieldPicIndicator      :1;
            WORD            FastUVMCflag                    :1;
            WORD            FourMvSwitch                    :1;
            WORD            UnifiedMvMode                   :2;
            WORD            IntensityCompensationField      :2;
        };
    };

    union
    {
        WORD                wTransformFlags;
        struct
        {
            WORD            CBPTable                        :3;
            WORD            TransDCtable                    :1;
            WORD            TransACtable                    :2;
            WORD            TransACtable2                   :2; 
            WORD            MbModeTable                     :3;
            WORD            TTMBF                           :1;
            WORD            TTFRM                           :2;
            WORD            Reserved2bits                   :2; 
        };
    };

    union
    {
        BYTE                  bMvTableFlags;
        struct
        {
            BYTE              TwoMVBPtable                  :2; 
            BYTE              FourMVBPtable                 :2; 
            BYTE              MvTable                       :3; 
            BYTE              reserved1bit                  :1; 
        };
    };

    union
    {
        BYTE                  bRawCodingFlag;
        struct
        {
            BYTE              FieldTX                       :1;
            BYTE              ACpred                        :1;
            BYTE              Overflags                     :1;
            BYTE              DirectMB                      :1;
            BYTE              SkipMB                        :1;
            BYTE              MvTypeMB                      :1;
            BYTE              ForwardMB                     :1;
            BYTE              IsBitplanePresent             :1;
        };
    };
} DXVA_Intel_VC1PictureParameters;

/* MPEG4 Picture Decoding Parameters                                                */
typedef struct _DXVA_PicParams_MPEG4
{
    WORD    wDecodedPictureIndex;
    WORD    wForwardRefPictureIndex;
    WORD    wBackwardRefPictureIndex;

    WORD    wPicWidthInMBminus1;
    WORD    wPicHeightInMBminus1;

    WORD    wDisplayWidthMinus1;
    WORD    wDisplayHeightMinus1;

    BYTE    bMacroblockWidthMinus1;
    BYTE    bMacroblockHeightMinus1;

    BYTE    bPicIntra;
    BYTE    bPicBackwardPrediction;
    BYTE    bPicSprite;
    BYTE    bChromaFormat;

    WORD    wProfile;
    BYTE    bShortHeader;
    BYTE    bQuantPrecision;
    BYTE    bGMC;
    BYTE    bSpriteWarpingPoints;
    BYTE    bSpriteWarpingAccuracy;
    BYTE    bReversibleVLC;
    BYTE    bDataPartitioned;
    BYTE    bInterlaced;
    BYTE    bBwdRefVopType;
    BYTE    bIntraDcVlcThr;
    BYTE    bVopFcodeFwd;
    BYTE    bVopFcodeBwd;
    WORD    wNumMBsInGob;
    BYTE    bNumGobsInVop;
    BYTE    bAlternateScan;
    BYTE    bTopFieldFirst;

    BYTE    bQuantType;
    BYTE    bObmcDisable;
    BYTE    bQuarterSample;
    BYTE    bVopRoundingType;
    WORD    wSpriteTrajectoryDu;
    WORD    wSpriteTrajectoryDv;
    WORD    wTrb;
    WORD    wTrd;

    /*BYTE    bCoded;
    WORD    wQuant;
    WORD    wQuantScale;
    BYTE    bSourceFormat;*/
} DXVA_PicParams_MPEG4, *LPDXVA_PicParams_MPEG4;

#endif//__DXVA_SPY_BUFFERS_H