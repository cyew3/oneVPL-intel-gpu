//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015 Intel Corporation. All Rights Reserved.
//

#include "stdio.h"
#include "time.h"
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "vector"
#include "../include/test_common.h"
#include "../include/genx_hevce_intra_avc_bdw_isa.h"
#include "../include/genx_hevce_intra_avc_hsw_isa.h"

#ifdef CMRT_EMU
extern "C" void IntraAvc(SurfaceIndex CURBE_DATA, SurfaceIndex SRC_AND_REF, SurfaceIndex SRC, SurfaceIndex MBDISTINTRA);

#endif //CMRT_EMU

enum { MFX_FRAMETYPE_I=1, MFX_FRAMETYPE_P=2, MFX_FRAMETYPE_B=4 };

struct Curbe {
    union {
        mfxU32   DW0;
        struct {
            mfxU32  SkipModeEn:1;
            mfxU32  AdaptiveEn:1;
            mfxU32  BiMixDis:1;
            mfxU32  MBZ1:2;
            mfxU32  EarlyImeSuccessEn:1;
            mfxU32  MBZ2:1;
            mfxU32  T8x8FlagForInterEn:1;
            mfxU32  MBZ3:16;
            mfxU32  EarlyImeStop:8;
        }; };
    union {
        mfxU32   DW1;
        struct {
            mfxU32  MaxNumMVs:6;
            mfxU32  MBZ4:10;
            mfxU32  BiWeight:6;
            mfxU32  MBZ5:6;
            mfxU32  UniMixDisable:1;
            mfxU32  MBZ6:3;
        }; };
    union {
        mfxU32   DW2;
        struct {
            mfxU32  LenSP:8;
            mfxU32  MaxNumSU:8;
            mfxU32  MBZ7:16;
        }; };
    union {
        mfxU32   DW3;
        struct {
            mfxU32  SrcSize:2;
            mfxU32  MBZ8:1;
            mfxU32  MBZ9:1;
            mfxU32  MbTypeRemap:2;
            mfxU32  SrcAccess:1;
            mfxU32  RefAccess:1;
            mfxU32  SearchCtrl:3;
            mfxU32  DualSearchPathOption:1;
            mfxU32  SubPelMode:2;
            mfxU32  SkipType:1;
            mfxU32  DisableFieldCacheAllocation:1;
            mfxU32  InterChromaMode:1;
            mfxU32  FTQSkipEnable:1;
            mfxU32  BMEDisableFBR:1;
            mfxU32  BlockBasedSkipEnabled:1;
            mfxU32  InterSAD:2;
            mfxU32  IntraSAD:2;
            mfxU32  SubMbPartMask:7;
            mfxU32  MBZ10:1;
        }; };
    union {
        mfxU32   DW4;
        struct {
            mfxU32  SliceHeightMinusOne:8;
            mfxU32  PictureHeightMinusOne:8;
            mfxU32  PictureWidth:8;
            mfxU32  Log2MvScaleFactor:4;
            mfxU32  Log2MbScaleFactor:4;
        }; };
    union {
        mfxU32   DW5;
        struct {
            mfxU32  MBZ12:16;
            mfxU32  RefWidth:8;
            mfxU32  RefHeight:8;
        }; };
    union {
        mfxU32   DW6;
        struct {
            mfxU32  BatchBufferEndCommand:32;
        }; };
    union {
        mfxU32   DW7;
        struct {
            mfxU32  IntraPartMask:5;
            mfxU32  NonSkipZMvAdded:1;
            mfxU32  NonSkipModeAdded:1;
            mfxU32  IntraCornerSwap:1;
            mfxU32  MBZ13:8;
            mfxU32  MVCostScaleFactor:2;
            mfxU32  BilinearEnable:1;
            mfxU32  SrcFieldPolarity:1;
            mfxU32  WeightedSADHAAR:1;
            mfxU32  AConlyHAAR:1;
            mfxU32  RefIDCostMode:1;
            mfxU32  MBZ00:1;
            mfxU32  SkipCenterMask:8;
        }; };
    union {
        mfxU32   DW8;
        struct {
            mfxU32  ModeCost_0:8;
            mfxU32  ModeCost_1:8;
            mfxU32  ModeCost_2:8;
            mfxU32  ModeCost_3:8;
        }; };
    union {
        mfxU32   DW9;
        struct {
            mfxU32  ModeCost_4:8;
            mfxU32  ModeCost_5:8;
            mfxU32  ModeCost_6:8;
            mfxU32  ModeCost_7:8;
        }; };
    union {
        mfxU32   DW10;
        struct {
            mfxU32  ModeCost_8:8;
            mfxU32  ModeCost_9:8;
            mfxU32  RefIDCost:8;
            mfxU32  ChromaIntraModeCost:8;
        }; };
    union {
        mfxU32   DW11;
        struct {
            mfxU32  MvCost_0:8;
            mfxU32  MvCost_1:8;
            mfxU32  MvCost_2:8;
            mfxU32  MvCost_3:8;
        }; };
    union {
        mfxU32   DW12;
        struct {
            mfxU32  MvCost_4:8;
            mfxU32  MvCost_5:8;
            mfxU32  MvCost_6:8;
            mfxU32  MvCost_7:8;
        }; };
    union {
        mfxU32   DW13;
        struct {
            mfxU32  QpPrimeY:8;
            mfxU32  QpPrimeCb:8;
            mfxU32  QpPrimeCr:8;
            mfxU32  TargetSizeInWord:8;
        }; };
    union {
        mfxU32   DW14;
        struct {
            mfxU32  FTXCoeffThresh_DC:16;
            mfxU32  FTXCoeffThresh_1:8;
            mfxU32  FTXCoeffThresh_2:8;
        }; };
    union {
        mfxU32   DW15;
        struct {
            mfxU32  FTXCoeffThresh_3:8;
            mfxU32  FTXCoeffThresh_4:8;
            mfxU32  FTXCoeffThresh_5:8;
            mfxU32  FTXCoeffThresh_6:8;
        }; };
    union {
        mfxU32   DW16;
        struct {
            mfxU32  IMESearchPath0:8;
            mfxU32  IMESearchPath1:8;
            mfxU32  IMESearchPath2:8;
            mfxU32  IMESearchPath3:8;
        }; };
    union {
        mfxU32   DW17;
        struct {
            mfxU32  IMESearchPath4:8;
            mfxU32  IMESearchPath5:8;
            mfxU32  IMESearchPath6:8;
            mfxU32  IMESearchPath7:8;
        }; };
    union {
        mfxU32   DW18;
        struct {
            mfxU32  IMESearchPath8:8;
            mfxU32  IMESearchPath9:8;
            mfxU32  IMESearchPath10:8;
            mfxU32  IMESearchPath11:8;
        }; };
    union {
        mfxU32   DW19;
        struct {
            mfxU32  IMESearchPath12:8;
            mfxU32  IMESearchPath13:8;
            mfxU32  IMESearchPath14:8;
            mfxU32  IMESearchPath15:8;
        }; };
    union {
        mfxU32   DW20;
        struct {
            mfxU32  IMESearchPath16:8;
            mfxU32  IMESearchPath17:8;
            mfxU32  IMESearchPath18:8;
            mfxU32  IMESearchPath19:8;
        }; };
    union {
        mfxU32   DW21;
        struct {
            mfxU32  IMESearchPath20:8;
            mfxU32  IMESearchPath21:8;
            mfxU32  IMESearchPath22:8;
            mfxU32  IMESearchPath23:8;
        }; };
    union {
        mfxU32   DW22;
        struct {
            mfxU32  IMESearchPath24:8;
            mfxU32  IMESearchPath25:8;
            mfxU32  IMESearchPath26:8;
            mfxU32  IMESearchPath27:8;
        }; };
    union {
        mfxU32   DW23;
        struct {
            mfxU32  IMESearchPath28:8;
            mfxU32  IMESearchPath29:8;
            mfxU32  IMESearchPath30:8;
            mfxU32  IMESearchPath31:8;
        }; };
    union {
        mfxU32   DW24;
        struct {
            mfxU32  IMESearchPath32:8;
            mfxU32  IMESearchPath33:8;
            mfxU32  IMESearchPath34:8;
            mfxU32  IMESearchPath35:8;
        }; };
    union {
        mfxU32   DW25;
        struct {
            mfxU32  IMESearchPath36:8;
            mfxU32  IMESearchPath37:8;
            mfxU32  IMESearchPath38:8;
            mfxU32  IMESearchPath39:8;
        }; };
    union {
        mfxU32   DW26;
        struct {
            mfxU32  IMESearchPath40:8;
            mfxU32  IMESearchPath41:8;
            mfxU32  IMESearchPath42:8;
            mfxU32  IMESearchPath43:8;
        }; };
    union {
        mfxU32   DW27;
        struct {
            mfxU32  IMESearchPath44:8;
            mfxU32  IMESearchPath45:8;
            mfxU32  IMESearchPath46:8;
            mfxU32  IMESearchPath47:8;
        }; };
    union {
        mfxU32   DW28;
        struct {
            mfxU32  IMESearchPath48:8;
            mfxU32  IMESearchPath49:8;
            mfxU32  IMESearchPath50:8;
            mfxU32  IMESearchPath51:8;
        }; };
    union {
        mfxU32   DW29;
        struct {
            mfxU32  IMESearchPath52:8;
            mfxU32  IMESearchPath53:8;
            mfxU32  IMESearchPath54:8;
            mfxU32  IMESearchPath55:8;
        }; };
    union {
        mfxU32   DW30;
        struct {
            mfxU32  Intra4x4ModeMask:9;
            mfxU32  MBZ14:7;
            mfxU32  Intra8x8ModeMask:9;
            mfxU32  MBZ15:7;
        }; };
    union {
        mfxU32   DW31;
        struct {
            mfxU32  Intra16x16ModeMask:4;
            mfxU32  IntraChromaModeMask:4;
            mfxU32  IntraComputeType:2;
            mfxU32  MBZ16:22;
        }; };
    union {
        mfxU32   DW32;
        struct {
            mfxU32  SkipVal:16;
            mfxU32  MultipredictorL0EnableBit:8;
            mfxU32  MultipredictorL1EnableBit:8;
        }; };
    union {
        mfxU32   DW33;
        struct {
            mfxU32  IntraNonDCPenalty16x16:8;
            mfxU32  IntraNonDCPenalty8x8:8;
            mfxU32  IntraNonDCPenalty4x4:8;
            mfxU32  MBZ18:8;
        }; };
    union {
        mfxU32   DW34;
        struct {
            mfxU32  MaxVmvR:16;
            mfxU32  MBZ19:16;
        }; };
    union {
        mfxU32   DW35;
        struct {
            mfxU32  PanicModeMBThreshold:16;
            mfxU32  SmallMbSizeInWord:8;
            mfxU32  LargeMbSizeInWord:8;
        }; };
    union {
        mfxU32   DW36;
        struct {
            mfxU32  HMERefWindowsCombiningThreshold:8;
            mfxU32  HMECombineOverlap:2;
            mfxU32  CheckAllFractionalEnable:1;
            mfxU32  MBZ20:21;
        }; };
    union {
        mfxU32   DW37;
        struct {
            mfxU32  CurLayerDQId:8;
            mfxU32  TemporalId:4;
            mfxU32  NoInterLayerPredictionFlag:1;
            mfxU32  AdaptivePredictionFlag:1;
            mfxU32  DefaultBaseModeFlag:1;
            mfxU32  AdaptiveResidualPredictionFlag:1;
            mfxU32  DefaultResidualPredictionFlag:1;
            mfxU32  AdaptiveMotionPredictionFlag:1;
            mfxU32  DefaultMotionPredictionFlag:1;
            mfxU32  TcoeffLevelPredictionFlag:1;
            mfxU32  UseRawMePredictor:1;
            mfxU32  SpatialResChangeFlag:1;
            mfxU32  isFwdFrameShortTermRef:1;
            mfxU32  MBZ21:9;
        }; };
    union {
        mfxU32   DW38;
        struct {
            mfxU32  ScaledRefLayerLeftOffset:16;
            mfxU32  ScaledRefLayerRightOffset:16;
        }; };
    union {
        mfxU32   DW39;
        struct {
            mfxU32  ScaledRefLayerTopOffset:16;
            mfxU32  ScaledRefLayerBottomOffset:16;
        }; };
};

void SetCurbeData(Curbe &curbeData, mfxU32 PicType, mfxU32 qp, mfxU32 width, mfxU32 height)
{
    //DW0
    curbeData.SkipModeEn            = 0;//!(PicType & MFX_FRAMETYPE_I);
    curbeData.AdaptiveEn            = 1;
    curbeData.BiMixDis              = 0;
    curbeData.EarlyImeSuccessEn     = 0;
    curbeData.T8x8FlagForInterEn    = 1;
    curbeData.EarlyImeStop          = 0;
    //DW1
    curbeData.MaxNumMVs             = 0x3F/*(GetMaxMvsPer2Mb(m_video.mfx.CodecLevel) >> 1) & 0x3F*/;
    curbeData.BiWeight              = /*((task.m_frameType & MFX_FRAMETYPE_B) && extDdi->WeightedBiPredIdc == 2) ? CalcBiWeight(task, 0, 0) :*/ 32;
    curbeData.UniMixDisable         = 0;
    //DW2
    curbeData.MaxNumSU              = 57;
    curbeData.LenSP                 = 16;
    //DW3
    curbeData.SrcSize               = 0;
    curbeData.MbTypeRemap           = 0;
    curbeData.SrcAccess             = 0;
    curbeData.RefAccess             = 0;
    curbeData.SearchCtrl            = (PicType & MFX_FRAMETYPE_B) ? 7 : 0;
    curbeData.DualSearchPathOption  = 0;
    curbeData.SubPelMode            = 0;//3; // all modes
    curbeData.SkipType              = 0; //!!(task.m_frameType & MFX_FRAMETYPE_B); //for B 0-16x16, 1-8x8
    curbeData.DisableFieldCacheAllocation = 0;
    curbeData.InterChromaMode       = 0;
    curbeData.FTQSkipEnable         = 0;//!(PicType & MFX_FRAMETYPE_I);
    curbeData.BMEDisableFBR         = !!(PicType & MFX_FRAMETYPE_P);
    curbeData.BlockBasedSkipEnabled = 0;
    curbeData.InterSAD              = 2;
    curbeData.IntraSAD              = 2;
    curbeData.SubMbPartMask         = 0x7e; // only 16x16 for Inter
    //DW4
    curbeData.SliceHeightMinusOne   = height / 16 - 1;
    curbeData.PictureHeightMinusOne = height / 16 - 1;
    curbeData.PictureWidth          = width / 16;
    curbeData.Log2MvScaleFactor     = 0;
    curbeData.Log2MbScaleFactor     = 1;
    //DW5
    curbeData.RefWidth              = (PicType & MFX_FRAMETYPE_B) ? 32 : 48;
    curbeData.RefHeight             = (PicType & MFX_FRAMETYPE_B) ? 32 : 40;
    //DW6
    curbeData.BatchBufferEndCommand = 0x5000000;
    //DW7
    curbeData.IntraPartMask         = 0; // all partitions enabled
    curbeData.NonSkipZMvAdded       = 0;//!!(PicType & MFX_FRAMETYPE_P);
    curbeData.NonSkipModeAdded      = 0;//!!(PicType & MFX_FRAMETYPE_P);
    curbeData.IntraCornerSwap       = 0;
    curbeData.MVCostScaleFactor     = 0;
    curbeData.BilinearEnable        = 0;
    curbeData.SrcFieldPolarity      = 0;
    curbeData.WeightedSADHAAR       = 0;
    curbeData.AConlyHAAR            = 0;
    curbeData.RefIDCostMode         = 0;//!(PicType & MFX_FRAMETYPE_I);
    curbeData.SkipCenterMask        = !!(PicType & MFX_FRAMETYPE_P);
    //DW8
    curbeData.ModeCost_0            = 0;
    curbeData.ModeCost_1            = 0;
    curbeData.ModeCost_2            = 0;
    curbeData.ModeCost_3            = 0;
    //DW9
    curbeData.ModeCost_4            = 0;
    curbeData.ModeCost_5            = 0;
    curbeData.ModeCost_6            = 0;
    curbeData.ModeCost_7            = 0;
    //DW10
    curbeData.ModeCost_8            = 0;
    curbeData.ModeCost_9            = 0;
    curbeData.RefIDCost             = 0;
    curbeData.ChromaIntraModeCost   = 0;
    //DW11
    curbeData.MvCost_0              = 0;
    curbeData.MvCost_1              = 0;
    curbeData.MvCost_2              = 0;
    curbeData.MvCost_3              = 0;
    //DW12
    curbeData.MvCost_4              = 0;
    curbeData.MvCost_5              = 0;
    curbeData.MvCost_6              = 0;
    curbeData.MvCost_7              = 0;
    //DW13
    curbeData.QpPrimeY              = qp;
    curbeData.QpPrimeCb             = qp;
    curbeData.QpPrimeCr             = qp;
    curbeData.TargetSizeInWord      = 0xff;
    //DW14
    curbeData.FTXCoeffThresh_DC               = 0;
    curbeData.FTXCoeffThresh_1                = 0;
    curbeData.FTXCoeffThresh_2                = 0;
    //DW15
    curbeData.FTXCoeffThresh_3                = 0;
    curbeData.FTXCoeffThresh_4                = 0;
    curbeData.FTXCoeffThresh_5                = 0;
    curbeData.FTXCoeffThresh_6                = 0;
    //DW30
    curbeData.Intra4x4ModeMask                = 0;
    curbeData.Intra8x8ModeMask                = 0;
    //DW31
    curbeData.Intra16x16ModeMask              = 0;
    curbeData.IntraChromaModeMask             = 0;
    curbeData.IntraComputeType                = 1; // luma only
    //DW32
    curbeData.SkipVal                         = 0;//skipVal;
    curbeData.MultipredictorL0EnableBit       = 0xEF;
    curbeData.MultipredictorL1EnableBit       = 0xEF;
    //DW33
    curbeData.IntraNonDCPenalty16x16          = 0;//36;
    curbeData.IntraNonDCPenalty8x8            = 0;//12;
    curbeData.IntraNonDCPenalty4x4            = 0;//4;
    //DW34
    //curbeData.MaxVmvR                         = GetMaxMvLenY(m_video.mfx.CodecLevel) * 4;
    curbeData.MaxVmvR                         = 0x7FFF;
    //DW35
    curbeData.PanicModeMBThreshold            = 0xFF;
    curbeData.SmallMbSizeInWord               = 0xFF;
    curbeData.LargeMbSizeInWord               = 0xFF;
    //DW36
    curbeData.HMECombineOverlap               = 1;  //  0;  !sergo
    curbeData.HMERefWindowsCombiningThreshold = (PicType & MFX_FRAMETYPE_B) ? 8 : 16; //  0;  !sergo (should be =8 for B frames)
    curbeData.CheckAllFractionalEnable        = 0;
    //DW37
    curbeData.CurLayerDQId                    = 0;
    curbeData.TemporalId                      = 0;
    curbeData.NoInterLayerPredictionFlag      = 1;
    curbeData.AdaptivePredictionFlag          = 0;
    curbeData.DefaultBaseModeFlag             = 0;
    curbeData.AdaptiveResidualPredictionFlag  = 0;
    curbeData.DefaultResidualPredictionFlag   = 0;
    curbeData.AdaptiveMotionPredictionFlag    = 0;
    curbeData.DefaultMotionPredictionFlag     = 0;
    curbeData.TcoeffLevelPredictionFlag       = 0;
    curbeData.UseRawMePredictor               = 1;
    curbeData.SpatialResChangeFlag            = 0;
    //DW38
    curbeData.ScaledRefLayerLeftOffset        = 0;
    curbeData.ScaledRefLayerRightOffset       = 0;
    //DW39
    curbeData.ScaledRefLayerTopOffset         = 0;
    curbeData.ScaledRefLayerBottomOffset      = 0;
}

namespace {
    int RunGpu(const Curbe &curbe, const mfxU8 *srcData, mfxU32 *distData);
    int RunCpu(const Curbe &curbe, const mfxU8 *srcData, mfxU32 *distData);
    int Compare(const Me2xControl &ctrl, const mfxI16Pair *mv1, const mfxI16Pair *mv2);
};


int main()
{
    int width = ROUNDUP(1920, 16);
    int height = ROUNDUP(1080, 16);
    int frameSize = width * height * 3 / 2;

    std::vector<mfxU8> src(frameSize);
    std::vector<mfxU32> dist((width/16) * (height/16));

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;
    if (fread(src.data(), 1, src.size(), f) != src.size()) // read first frame
        return printf("FAILED to read first frame from yuv file\n"), 1;
    fclose(f);

    Curbe curbe = {};
    SetCurbeData(curbe, MFX_FRAMETYPE_P, 26, width, height);

    Ipp32s res;
    res = RunGpu(curbe, src.data(), dist.data());
    CHECK_ERR(res);

    //res = RunCpu(input.data(), outputCpu.data(), inWidth, inHeight);
    //CHECK_ERR(res);

    //res = Compare(outputCpu.data(), outputGpu.data(), outWidth, outHeight);
    //CHECK_ERR(res);

    return printf("passed\n"), 0;
}


namespace {
    int RunGpu(const Curbe &curbe, const mfxU8 *srcData, mfxU32 *distData)
    {
        mfxU32 version = 0;
        CmDevice *device = 0;
        Ipp32s res = ::CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_intra_avc_hsw, sizeof(genx_hevce_intra_avc_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(IntraAvc), kernel);
        CHECK_CM_ERR(res);

        CmBuffer *curbeBuf = 0;
        res = device->CreateBuffer(sizeof(Curbe), curbeBuf);
        CHECK_CM_ERR(res);
        res = curbeBuf->WriteSurface((const Ipp8u *)&curbe, NULL, sizeof(Curbe));
        CHECK_CM_ERR(res);

        int width = curbe.PictureWidth * 16;
        int height = (curbe.PictureHeightMinusOne + 1) * 16;
        CmSurface2D *src = 0;
        res = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_NV12, src);
        CHECK_CM_ERR(res);
        res = src->WriteSurfaceStride(srcData, NULL, width);
        CHECK_CM_ERR(res);

        SurfaceIndex *genxRefs = 0;
        res = device->CreateVmeSurfaceG7_5(src, NULL, NULL, 0, 0, genxRefs);
        CHECK_CM_ERR(res);

        Ipp32u distPitch = 0;
        Ipp32u distSize = 0;
        res = device->GetSurface2DInfo(width / 16 * sizeof(mfxU32), height / 16, CM_SURFACE_FORMAT_P8, distPitch, distSize);
        CHECK_CM_ERR(res);
        void *distSys = CM_ALIGNED_MALLOC(distSize, 0x1000);
        CmSurface2DUP *dist = 0;
        res = device->CreateSurface2DUP(width / 16 * sizeof(mfxU32), height / 16, CM_SURFACE_FORMAT_P8, distSys, dist);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxCurbe = 0;
        res = curbeBuf->GetIndex(idxCurbe);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxSrc = 0;
        res = src->GetIndex(idxSrc);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxDist = 0;
        res = dist->GetIndex(idxDist);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(0, sizeof(*idxCurbe), idxCurbe);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(1, sizeof(*genxRefs), genxRefs);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(2, sizeof(*idxSrc), idxSrc);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(3, sizeof(*idxDist), idxDist);
        CHECK_CM_ERR(res);

        mfxU32 tsWidth  = width / 16;
        mfxU32 tsHeight = height / 16;
        res = kernel->SetThreadCount(tsWidth * tsHeight);
        CHECK_CM_ERR(res);

        CmThreadSpace * threadSpace = 0;
        res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
        CHECK_CM_ERR(res);

        res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
        CHECK_CM_ERR(res);

        CmTask * task = 0;
        res = device->CreateTask(task);
        CHECK_CM_ERR(res);

        res = task->AddKernel(kernel);
        CHECK_CM_ERR(res);

        CmQueue *queue = 0;
        res = device->CreateQueue(queue);
        CHECK_CM_ERR(res);

        CmEvent * e = 0;
        res = queue->Enqueue(task, e, threadSpace);
        CHECK_CM_ERR(res);

        res = e->WaitForTaskFinished();
        CHECK_CM_ERR(res);

        for (int y = 0; y < height / 16; y++)
            memcpy(distData + y * (width / 16), (char *)distSys + y * distPitch, sizeof(mfxU32) * (width / 16));

#ifndef CMRT_EMU
        printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        device->DestroyVmeSurfaceG7_5(genxRefs);
        device->DestroySurface(curbeBuf);
        device->DestroySurface(src);
        device->DestroySurface2DUP(dist);
        CM_ALIGNED_FREE(distSys);
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);

        return PASSED;
    }
}
