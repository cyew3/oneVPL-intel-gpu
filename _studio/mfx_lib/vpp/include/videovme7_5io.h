//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __VIDEOVME7_5IO_H
#define __VIDEOVME7_5IO_H

#include "mfxdefs.h"

#pragma warning( disable : 4201 )

typedef struct {
    mfxU8          *SrcLuma;    
    mfxU8          *SrcCb;    
    mfxU8          *SrcCr;    
    mfxU8          *RefLuma[2][16];
    mfxU8       *RefCb[2][16];
    mfxU8       *RefCr[2][16];
    mfxI32        SrcPitch;    
    mfxI32        RefPitch;    
    mfxI16        RefWidth;    
    mfxI16        RefHeight;    
    mfxU8          RefW;
    mfxU8        RefH;
    mfxU32      mbz0;    
    mfxU8          SrcType;    
    mfxU8          VmeModes;    
    mfxU8          SadType;    
    mfxU8          ShapeMask;    
    mfxI16Pair    SrcMB;        
    mfxI16Pair    Ref[2];                
    mfxU8          IntraFlags;            
    mfxU8          IntraAvail;            
    mfxU8        MvFlags;            
    mfxU8        SkipCenterEnables;    
    mfxU8        BlockRefId[4];
    mfxI16Pair    CostCenter[2];        
    mfxU32        mbz1;
    mfxU8          MaxLenSP;            
    mfxU8          MaxNumSU;            
    mfxU8        SPCenter[2];        
    mfxU8          MaxMvs;                
    mfxU8        mbz2;    
    struct{
        mfxU8          BiWeight    : 6;
        mfxU8       mbz2b       : 2;
    };
    mfxU8          BidirMask;            
    mfxU8        VmeFlags;            
    mfxU8        mbz3[2];            
    mfxU8          EarlyImeStop;    
    mfxU16        FTXCoeffThresh_DC;
    mfxU8        FTXCoeffThresh[6];
    mfxU8        FBRMbInfo;
    mfxU8        FBRSubMbShape;
    mfxU8        FBRSubPredMode;
    mfxU8        mbz4;
    mfxU8        MvCost[8];
    mfxU8        ModeCost[12];

}mfxVME7_5UNIIn;    

typedef struct {
    mfxI16Pair    SkipCenter[4][2];    
    mfxU8        LeftPels[16];        
    mfxU8        TopMinus2Pels[26];
    mfxU8        IntraModeMask;        
    mfxU8        IntraComputeType;
    mfxU16        Intra4x4ModeMask;    
    mfxU16        Intra8x8ModeMask;    
    mfxU8        IntraNonDCPenalty16x16;
    mfxU8        IntraNonDCPenalty8x8;
    mfxU8        IntraNonDCPenalty4x4;
    mfxU8        mbz2;
    mfxU32        mbz0;
    mfxU16        NeighborChromaPel;
    mfxU16        mbz1;
    mfxU16        LeftModes;            
    mfxU16        TopModes;            
    mfxU8        LeftChromaPels[16];
    mfxU8        TopChromaPels[16];
}mfxVME7_5SICIn;    

typedef struct {
    mfxU8        Multicall;         //VmeState, not part of IME Inf
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
}mfxVME7_5IMEIn;    

typedef struct {
    mfxI16Pair    SubblockMVs[16][2];
}mfxVME7_5FBRIn;    

typedef struct 
{
    mfxU16         ClockCompute;    
    mfxU16         ClockStalled;    
    mfxU8        IntraAvail;        
    mfxU8          MbSubShape;            
    mfxU8          MbSubPredMode;        
    mfxU8          ImeStop_MaxMV_AltNumSU;        
    mfxU16      IntraPredModes[4];    
    mfxU16        DistIntra;            
    mfxU16        DistChromaIntra; 
    mfxU16        DistInter;            
    mfxU16      DistSkip;            
    mfxU8          RefBorderMark;    
    mfxU8          NumSUinIME;        
    mfxU16        mbz2;
    mfxU8          MbMode;                
    mfxU8          InterMbType;        
    mfxU8        IntraMbType;
    mfxU8          NumPackedMv;            
    mfxI16Pair    Mvs[16][2];            
    mfxU16        Distortion[16];            
    mfxU32        mbz0[2];
    mfxU16        ChromaCoeffSum[2];
    mfxU8        ChromaNonZeroCoeff[2];
    mfxU16        LumaCoeffSum[4];
    mfxU8        LumaNonZeroCoeff[4];
    mfxU8        RefId[4];
    mfxU16        mbz1;

}mfxVME7_5UNIOutput; 

typedef struct 
{ 
    mfxU16        RecordDst[2][8];
    mfxU16        RecordDst16[2];    
    mfxI16Pair    RecordMvs16[2];    
    mfxI16Pair    RecordMvs[2][8];
    mfxU8       RecordRefId[2][4];
    mfxU16      RecordRefId16[2]; 
}mfxVME7_5IMEOutput; 


#endif // __VIDEOVME7IO_H
/* EOF */
