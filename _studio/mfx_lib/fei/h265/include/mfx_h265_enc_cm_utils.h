//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_H265_ENC_CM_UTILS_H
#define _MFX_H265_ENC_CM_UTILS_H

#pragma warning(disable: 4100; disable: 4505)

#include "cm_def.h"
#include "cm_vm.h"

///#include "mfx_h265_enc.h"
#include "libmfx_core_interface.h"
#include "mfxplugin++.h"
#include "cmrt_cross_platform.h"

#include <vector>
///#include <assert.h>

namespace H265Enc {

class CmRuntimeError : public std::exception
{
public:
    CmRuntimeError() : std::exception() {
        assert(!"CmRuntimeError");
    }
};

struct VmeSearchPath // sizeof=58
{
    mfxU8 sp[56];
    mfxU8 maxNumSu;
    mfxU8 lenSp;
};

struct MeControl // sizeof=120
{
    mfxU8  longSp[32];          // 0 B
    mfxU8  shortSp[32];         // 32 B
    mfxU8  longSpLenSp;         // 64 B
    mfxU8  longSpMaxNumSu;      // 65 B
    mfxU8  shortSpLenSp;        // 66 B
    mfxU8  shortSpMaxNumSu;     // 67 B
    mfxU16 width;               // 34 W
    mfxU16 height;              // 35 W
    mfxU8  mvCost[5][8];         // 72 B (1x), 80 B (2x), 88 B (4x), 96 B (8x) 104 B (16x)
    mfxU8  mvCostScaleFactor[5]; // 112 B (1x), 113 B (2x), 114 B (4x), 115 B (8x) 116 B (16x)
    mfxU8  reserved[3];          // 117 B
};

struct H265EncCURBEData {
    //DW0
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
        };
    };

    //DW1
    union {
        mfxU32   DW1;
        struct {
            mfxU32  MaxNumMVs:6;
            mfxU32  MBZ4:10;
            mfxU32  BiWeight:6;
            mfxU32  MBZ5:6;
            mfxU32  UniMixDisable:1;
            mfxU32  MBZ6:3;
        };
    };

    //DW2
    union {
        mfxU32   DW2;
        struct {
            mfxU32  LenSP:8;
            mfxU32  MaxNumSU:8;
            mfxU32  MBZ7:16;
        };
    };

    //DW3
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
        };
    };

    //DW4
    union {
        mfxU32   DW4;
        struct {
            mfxU32  SliceHeightMinusOne:8;
            mfxU32  PictureHeightMinusOne:8;
            mfxU32  PictureWidth:8;
            mfxU32  Log2MvScaleFactor:4;
            mfxU32  Log2MbScaleFactor:4;
        };
    };

    //DW5
    union {
        mfxU32   DW5;
        struct {
            mfxU32  MBZ12:16;
            mfxU32  RefWidth:8;
            mfxU32  RefHeight:8;
        };
    };

    //DW6
    union {
        mfxU32   DW6;
        struct {
            mfxU32  BatchBufferEndCommand:32;
        };
    };

    //DW7
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
        };
    };

    //DW8
    union {
        mfxU32   DW8;
        struct {
            mfxU32  ModeCost_0:8;
            mfxU32  ModeCost_1:8;
            mfxU32  ModeCost_2:8;
            mfxU32  ModeCost_3:8;
        };
    };

    //DW9
    union {
        mfxU32   DW9;
        struct {
            mfxU32  ModeCost_4:8;
            mfxU32  ModeCost_5:8;
            mfxU32  ModeCost_6:8;
            mfxU32  ModeCost_7:8;
        };
    };

    //DW10
    union {
        mfxU32   DW10;
        struct {
            mfxU32  ModeCost_8:8;
            mfxU32  ModeCost_9:8;
            mfxU32  RefIDCost:8;
            mfxU32  ChromaIntraModeCost:8;
        };
    };

    //DW11
    union {
        mfxU32   DW11;
        struct {
            mfxU32  MvCost_0:8;
            mfxU32  MvCost_1:8;
            mfxU32  MvCost_2:8;
            mfxU32  MvCost_3:8;
        };
    };

    //DW12
    union {
        mfxU32   DW12;
        struct {
            mfxU32  MvCost_4:8;
            mfxU32  MvCost_5:8;
            mfxU32  MvCost_6:8;
            mfxU32  MvCost_7:8;
        };
    };

    //DW13
    union {
        mfxU32   DW13;
        struct {
            mfxU32  QpPrimeY:8;
            mfxU32  QpPrimeCb:8;
            mfxU32  QpPrimeCr:8;
            mfxU32  TargetSizeInWord:8;
        };
    };

    //DW14
    union {
        mfxU32   DW14;
        struct {
            mfxU32  FTXCoeffThresh_DC:16;
            mfxU32  FTXCoeffThresh_1:8;
            mfxU32  FTXCoeffThresh_2:8;
        };
    };

    //DW15
    union {
        mfxU32   DW15;
        struct {
            mfxU32  FTXCoeffThresh_3:8;
            mfxU32  FTXCoeffThresh_4:8;
            mfxU32  FTXCoeffThresh_5:8;
            mfxU32  FTXCoeffThresh_6:8;
        };
    };


    //DW16
    union {
        mfxU32   DW16;
        struct {
            mfxU32  IMESearchPath0:8;
            mfxU32  IMESearchPath1:8;
            mfxU32  IMESearchPath2:8;
            mfxU32  IMESearchPath3:8;
        };
    };

    //DW17
    union {
        mfxU32   DW17;
        struct {
            mfxU32  IMESearchPath4:8;
            mfxU32  IMESearchPath5:8;
            mfxU32  IMESearchPath6:8;
            mfxU32  IMESearchPath7:8;
        };
    };

    //DW18
    union {
        mfxU32   DW18;
        struct {
            mfxU32  IMESearchPath8:8;
            mfxU32  IMESearchPath9:8;
            mfxU32  IMESearchPath10:8;
            mfxU32  IMESearchPath11:8;
        };
    };

    //DW19
    union {
        mfxU32   DW19;
        struct {
            mfxU32  IMESearchPath12:8;
            mfxU32  IMESearchPath13:8;
            mfxU32  IMESearchPath14:8;
            mfxU32  IMESearchPath15:8;
        };
    };

    //DW20
    union {
        mfxU32   DW20;
        struct {
            mfxU32  IMESearchPath16:8;
            mfxU32  IMESearchPath17:8;
            mfxU32  IMESearchPath18:8;
            mfxU32  IMESearchPath19:8;
        };
    };

    //DW21
    union {
        mfxU32   DW21;
        struct {
            mfxU32  IMESearchPath20:8;
            mfxU32  IMESearchPath21:8;
            mfxU32  IMESearchPath22:8;
            mfxU32  IMESearchPath23:8;
        };
    };

    //DW22
    union {
        mfxU32   DW22;
        struct {
            mfxU32  IMESearchPath24:8;
            mfxU32  IMESearchPath25:8;
            mfxU32  IMESearchPath26:8;
            mfxU32  IMESearchPath27:8;
        };
    };

    //DW23
    union {
        mfxU32   DW23;
        struct {
            mfxU32  IMESearchPath28:8;
            mfxU32  IMESearchPath29:8;
            mfxU32  IMESearchPath30:8;
            mfxU32  IMESearchPath31:8;
        };
    };

    //DW24
    union {
        mfxU32   DW24;
        struct {
            mfxU32  IMESearchPath32:8;
            mfxU32  IMESearchPath33:8;
            mfxU32  IMESearchPath34:8;
            mfxU32  IMESearchPath35:8;
        };
    };

    //DW25
    union {
        mfxU32   DW25;
        struct {
            mfxU32  IMESearchPath36:8;
            mfxU32  IMESearchPath37:8;
            mfxU32  IMESearchPath38:8;
            mfxU32  IMESearchPath39:8;
        };
    };

    //DW26
    union {
        mfxU32   DW26;
        struct {
            mfxU32  IMESearchPath40:8;
            mfxU32  IMESearchPath41:8;
            mfxU32  IMESearchPath42:8;
            mfxU32  IMESearchPath43:8;
        };
    };

    //DW27
    union {
        mfxU32   DW27;
        struct {
            mfxU32  IMESearchPath44:8;
            mfxU32  IMESearchPath45:8;
            mfxU32  IMESearchPath46:8;
            mfxU32  IMESearchPath47:8;
        };
    };

    //DW28
    union {
        mfxU32   DW28;
        struct {
            mfxU32  IMESearchPath48:8;
            mfxU32  IMESearchPath49:8;
            mfxU32  IMESearchPath50:8;
            mfxU32  IMESearchPath51:8;
        };
    };

    //DW29
    union {
        mfxU32   DW29;
        struct {
            mfxU32  IMESearchPath52:8;
            mfxU32  IMESearchPath53:8;
            mfxU32  IMESearchPath54:8;
            mfxU32  IMESearchPath55:8;
        };
    };

    //DW30
    union {
        mfxU32   DW30;
        struct {
            mfxU32  Intra4x4ModeMask:9;
            mfxU32  MBZ14:7;
            mfxU32  Intra8x8ModeMask:9;
            mfxU32  MBZ15:7;
        };
    };

    //DW31
    union {
        mfxU32   DW31;
        struct {
            mfxU32  Intra16x16ModeMask:4;
            mfxU32  IntraChromaModeMask:4;
            mfxU32  IntraComputeType:2;
            mfxU32  MBZ16:22;
        };
    };

    //DW32
    union {
        mfxU32   DW32;
        struct {
            mfxU32  SkipVal:16;
            mfxU32  MultipredictorL0EnableBit:8;
            mfxU32  MultipredictorL1EnableBit:8;
        };
    };

    //DW33
    union {
        mfxU32   DW33;
        struct {
            mfxU32  IntraNonDCPenalty16x16:8;
            mfxU32  IntraNonDCPenalty8x8:8;
            mfxU32  IntraNonDCPenalty4x4:8;
            mfxU32  MBZ18:8;
        };
    };

    //DW34
    union {
        mfxU32   DW34;
        struct {
            mfxU32  MaxVmvR:16;
            mfxU32  MBZ19:16;
        };
    };


    //DW35
    union {
        mfxU32   DW35;
        struct {
            mfxU32  PanicModeMBThreshold:16;
            mfxU32  SmallMbSizeInWord:8;
            mfxU32  LargeMbSizeInWord:8;
        };
    };

    //DW36
    union {
        mfxU32   DW36;
        struct {
            mfxU32  HMERefWindowsCombiningThreshold:8;
            mfxU32  HMECombineOverlap:2;
            mfxU32  CheckAllFractionalEnable:1;
            mfxU32  MBZ20:21;
        };
    };

    //DW37
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
        };
    };

    //the following offsets are in the unit of 2 pixels.
    //DW38
    union {
        mfxU32   DW38;
        struct {
            mfxU32  ScaledRefLayerLeftOffset:16;
            mfxU32  ScaledRefLayerRightOffset:16;
        };
    };

    //DW39
    union {
        mfxU32   DW39;
        struct {
            mfxU32  ScaledRefLayerTopOffset:16;
            mfxU32  ScaledRefLayerBottomOffset:16;
        };
    };
};


struct mfxVMEIMEIn
{
    union{
        mfxU8 Multicall;
        struct{
            mfxU8 StreamOut            :1;
            mfxU8 StreamIn            :1;
        };
    };
    mfxU8        IMESearchPath0to31[32];
    mfxU32        mbz0[2];
    mfxU8        IMESearchPath32to55[24];
    mfxU32        mbz1;
    mfxU8        RecRefID[2][4];
    mfxI16Pair    RecordMvs16[2];
    mfxU16        RecordDst16[2];
    mfxU16        RecRefID16[2];
    mfxU16        RecordDst[2][8];
    mfxI16Pair    RecordMvs[2][8];
};

const mfxU8 SingleSU[56] =
{
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};


const mfxU8 L1 = 0x0F;
const mfxU8 R1 = 0x01;
const mfxU8 U1 = 0xF0;
const mfxU8 D1 = 0x10;


const mfxU8 RasterScan_48x40[56] =
{// Starting location does not matter so long as wrapping enabled.
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1
};


//      37 1E 1F 20 21 22 23 24
//      36 1D 0C 0D 0E 0F 10 25
//      35 1C 0B 02 03 04 11 26
//      34 1B 0A 01>00<05 12 27
//      33 1A 09 08 07 06 13 28
//      32 19 18 17 16 15 14 29
//      31 30 2F 2E 2D 2C 2B 2A
const mfxU8 FullSpiral_48x40[56] =
{ // L -> U -> R -> D
    L1,
    U1,
    R1,R1,
    D1,D1,
    L1,L1,L1,
    U1,U1,U1,
    R1,R1,R1,R1,
    D1,D1,D1,D1,
    L1,L1,L1,L1,L1,
    U1,U1,U1,U1,U1,
    R1,R1,R1,R1,R1,R1,
    D1,D1,D1,D1,D1,D1,      // The last D1 steps outside the search window.
    L1,L1,L1,L1,L1,L1,L1,   // These are outside the search window.
    U1,U1,U1,U1,U1,U1,U1
};

const mfxU8 Diamond[56] =
{
    0x0F,0xF1,0x0F,0x12,//5
    0x0D,0xE2,0x22,0x1E,//9
    0x10,0xFF,0xE2,0x20,//13
    0xFC,0x06,0xDD,//16
    0x2E,0xF1,0x3F,0xD3,0x11,0x3D,0xF3,0x1F,//24
    0xEB,0xF1,0xF1,0xF1,//28
    0x4E,0x11,0x12,0xF2,0xF1,//33
    0xE0,0xFF,0xFF,0x0D,0x1F,0x1F,//39
    0x20,0x11,0xCF,0xF1,0x05,0x11,//45
    0x00,0x00,0x00,0x00,0x00,0x00,//51
};

const mfxU8 SmallPath[3] =
{
    0x0F, 0xF0, 0x01
};
CmProgram * ReadProgram(CmDevice * device, std::istream & is);
CmProgram * ReadProgram(CmDevice * device, char const * filename);
CmProgram * ReadProgram(CmDevice * device, const unsigned char* buffer, int len);
CmKernel * CreateKernel(CmDevice * device, CmProgram * program, char const * name, void * funcptr);
void EnqueueKernel(CmDevice *device, CmQueue *queue, CmKernel *kernel, mfxU32 tsWidth, mfxU32 tsHeight, CmEvent *&event, CM_DEPENDENCY_PATTERN tsPattern = CM_NONE_DEPENDENCY);
void EnqueueKernel(CmDevice *device, CmQueue *queue, CmKernel *kernel0, CmKernel *kernel1, mfxU32 tsWidth, mfxU32 tsHeight, CmEvent *&event, CM_DEPENDENCY_PATTERN tsPattern = CM_NONE_DEPENDENCY);
void EnqueueKernel(CmDevice *device, CmQueue *queue, CmTask * cmTask, CmKernel *kernel, mfxU32 tsWidth, mfxU32 tsHeight, CmEvent *&event);
void EnqueueKernelLight(CmDevice *device, CmQueue *queue, CmTask * cmTask, CmKernel *kernel, mfxU32 tsWidth, mfxU32 tsHeight, CmEvent *&event);
void EnqueueCopyCPUToGPU(CmQueue *queue, CmSurface2D *surface, const void *sysMem, CmEvent *&event);
void EnqueueCopyGPUToCPU(CmQueue *queue, CmSurface2D *surface, void *sysMem, CmEvent *&event);
void EnqueueCopyCPUToGPUStride(CmQueue *queue, CmSurface2D *surface, const void *sysMem, mfxU32 pitch, CmEvent *&event);
void EnqueueCopyGPUToCPUStride(CmQueue *queue, CmSurface2D *surface, void *sysMem, mfxU32 pitch, CmEvent *&event);
void Read(CmBuffer * buffer, void * buf, CmEvent * e = 0);
void Write(CmBuffer * buffer, void * buf, CmEvent * e = 0);
CmDevice * TryCreateCmDevicePtr(MFXCoreInterface * core, mfxU32 * version);
CmDevice * CreateCmDevicePtr(MFXCoreInterface * core, mfxU32 * version);
CmBuffer * CreateBuffer(CmDevice * device, mfxU32 size);
CmBufferUP * CreateBuffer(CmDevice * device, mfxU32 size, void * mem);
SurfaceIndex * CreateVmeSurfaceG75(CmDevice * device, CmSurface2D * source, CmSurface2D ** fwdRefs, CmSurface2D ** bwdRefs, mfxU32 numFwdRefs, mfxU32 numBwdRefs);

void SetSearchPath(VmeSearchPath *spath);
void SetCurbeData(H265EncCURBEData & curbeData, mfxU32 PicType, mfxU32  qp, mfxU32  width, mfxU32 height);
void SetupMeControl(MeControl &ctrl, int width, int height, double lambda);

CmDevice * TryCreateCmDevicePtr(MFXCoreInterface * core, mfxU32 * version = 0);
CmDevice * CreateCmDevicePtr(MFXCoreInterface * core, mfxU32 * version = 0);
CmBuffer * CreateBuffer(CmDevice * device, mfxU32 size);
CmBufferUP * CreateBuffer(CmDevice * device, mfxU32 size, void * mem);
CmSurface2D * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc);
CmSurface2DUP * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc, void * mem);

template <class T>
SurfaceIndex & GetIndex(const T * cmResource)
{
    SurfaceIndex * index = 0;
    int result = const_cast<T *>(cmResource)->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}

template <class T0>
void SetKernelArgLast(CmKernel * kernel, T0 const & arg, unsigned int index)
{
    kernel->SetKernelArg(index, sizeof(arg), &arg);
}
template <> inline
    void SetKernelArgLast<CmSurface2D *>(CmKernel * kernel, CmSurface2D * const & arg, unsigned int index)
{
    kernel->SetKernelArg(index, sizeof(SurfaceIndex), &GetIndex(arg));
}
template <> inline
    void SetKernelArgLast<CmSurface2DUP *>(CmKernel * kernel, CmSurface2DUP * const & arg, unsigned int index)
{
    kernel->SetKernelArg(index, sizeof(SurfaceIndex), &GetIndex(arg));
}
template <> inline
    void SetKernelArgLast<CmBuffer *>(CmKernel * kernel, CmBuffer * const & arg, unsigned int index)
{
    kernel->SetKernelArg(index, sizeof(SurfaceIndex), &GetIndex(arg));
}
template <> inline
    void SetKernelArgLast<CmBufferUP *>(CmKernel * kernel, CmBufferUP * const & arg, unsigned int index)
{
    kernel->SetKernelArg(index, sizeof(SurfaceIndex), &GetIndex(arg));
}

template <> inline
    void SetKernelArgLast<vector<SurfaceIndex, 1> >(CmKernel * kernel, vector<SurfaceIndex, 1> const & arg, unsigned int index)
{
    kernel->SetKernelArg(index, arg.get_size_data(), ((vector<SurfaceIndex, 1> &)arg).get_addr_data());
}

template <class T0>
void SetKernelArg(CmKernel * kernel, T0 const & arg0)
{
    SetKernelArgLast(kernel, arg0, 0);
}

template <class T0, class T1>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1)
{
    SetKernelArgLast(kernel, arg0, 0);
    SetKernelArgLast(kernel, arg1, 1);
}

template <class T0, class T1, class T2>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2)
{
    SetKernelArg(kernel, arg0, arg1);
    SetKernelArgLast(kernel, arg2, 2);
}

template <class T0, class T1, class T2, class T3>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3)
{
    SetKernelArg(kernel, arg0, arg1, arg2);
    SetKernelArgLast(kernel, arg3, 3);
}

template <class T0, class T1, class T2, class T3, class T4>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3);
    SetKernelArgLast(kernel, arg4, 4);
}

template <class T0, class T1, class T2, class T3, class T4, class T5>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4);
    SetKernelArgLast(kernel, arg5, 5);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5);
    SetKernelArgLast(kernel, arg6, 6);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    SetKernelArgLast(kernel, arg7, 7);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7, T8 const & arg8)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    SetKernelArgLast(kernel, arg8, 8);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7, T8 const & arg8, T9 const & arg9)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    SetKernelArgLast(kernel, arg9, 9);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7, T8 const & arg8, T9 const & arg9, T10 const & arg10)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    SetKernelArgLast(kernel, arg10, 10);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                  T8 const & arg8, T9 const & arg9, T10 const & arg10, T11 const & arg11)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    SetKernelArgLast(kernel, arg11, 11);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                  T8 const & arg8, T9 const & arg9, T10 const & arg10, T11 const & arg11, T12 const & arg12)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
    SetKernelArgLast(kernel, arg12, 12);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                  T8 const & arg8, T9 const & arg9, T10 const & arg10, T11 const & arg11, T12 const & arg12, T13 const & arg13)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12);
    SetKernelArgLast(kernel, arg13, 13);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                  T8 const & arg8, T9 const & arg9, T10 const & arg10, T11 const & arg11, T12 const & arg12, T13 const & arg13, T14 const & arg14)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13);
    SetKernelArgLast(kernel, arg14, 14);
}

struct Kernel
{
    Kernel();
    ~Kernel() { Destroy(); }
    void AddKernel(CmDevice *device, CmProgram *program, const char *name,
                   mfxU32 tsWidth, mfxU32 tsHeight, CM_DEPENDENCY_PATTERN tsPattern, bool addSync = false);
    void Destroy();
    void Enqueue(CmQueue *queue, CmEvent *&e);
    CmKernel     *&kernel;
    CmDevice      *m_device;
    CmKernel      *m_kernel[16];
    CmTask        *m_task;
    CmThreadSpace *m_ts[16];
    mfxU32         m_numKernels;

private:
    void operator=(const Kernel&);
};

}

#endif /* _MFX_H265_ENC_CM_UTILS_H */
