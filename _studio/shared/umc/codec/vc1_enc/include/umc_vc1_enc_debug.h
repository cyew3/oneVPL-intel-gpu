// Copyright (c) 2008-2018 Intel Corporation
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

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef __UMC_VC1_ENC_DEBUG_H__
#define __UMC_VC1_ENC_DEBUG_H__

#include "vm_types.h"
#include "ippdefs.h"
#include "ippvc.h"
#include "umc_vc1_enc_def.h"

namespace UMC_VC1_ENCODER
{

#ifdef VC1_ENC_DEBUG_ON

extern const uint32_t  VC1_ENC_DEBUG;             //current debug output
extern const uint32_t  VC1_ENC_FRAME_DEBUG;       //on/off frame debug
extern const uint32_t  VC1_ENC_FRAME_MIN;         //first frame to debug
extern const uint32_t  VC1_ENC_FRAME_MAX;         //last frame to debug

extern const uint32_t VC1_ENC_POSITION;          //Frame num, frame type, frame size,
                                               //MB, Block positions, skip info
extern const uint32_t VC1_ENC_COEFFS;            // DC, AC coefficiens
extern const uint32_t VC1_ENC_AC_PRED;           // AC prediction
extern const uint32_t VC1_ENC_QUANT;             //quant info
extern const uint32_t VC1_ENC_CBP;               // coded block patern info
extern const uint32_t VC1_ENC_MV;                // motion vectors info
extern const uint32_t VC1_ENC_PRED;
extern const uint32_t VC1_ENC_DEBLOCKING;        //deblocking info
extern const uint32_t VC1_ENC_DEBLK_EDGE;        //deblocking edge
extern const uint32_t VC1_ENC_FIELD_MV;          //field motion vectors info
extern const uint32_t VC1_ENC_SMOOTHING;         //print restored frame

typedef struct
{
    uint32_t pred[3][2][2]; //A/B/C, foeward/backward, X/Y
    //int32_t predField[3];

    uint8_t predFlag[2];
    int8_t scaleType[2];
    uint8_t hybrid[2];
}FieldBlkMVInfo;

typedef struct
{
    int16_t BlockDiff[64];
    uint8_t  intra;
    int32_t DC;
    int32_t DCDiff;
    int32_t DCDirection;
    int16_t AC[7];
    int32_t ACPred;

//motion vectors info
    int16_t MV[2][2];       //X/Y, forward/backward
    int32_t MVField[2];    //bSecond
    int16_t PredMV[2][2];   //X/Y, forward/backward
    int16_t difMV[2][2];    //X/Y, forward/backward
    int16_t IntrpMV[2][2];    //X/Y, forward/backward

//run, level paramc
    uint8_t  Run  [65];
    int16_t Level[64];
    uint8_t  Pairs[4];
    uint8_t  Mode[65];

//interpolation info
    uint8_t  FDst[64];   //forward
    uint8_t  BDst[64];   //backward

    eTransformType VTSType;
    FieldBlkMVInfo FieldMV;

}VC1BlockDebugInfo;

typedef struct
{
    VC1BlockDebugInfo Block[6];
    int32_t CBP;
    int32_t PredCBP;
    int32_t MBType;
    int32_t MQuant;
    uint8_t  HalfQP;
    uint8_t  skip;

    uint32_t InHorEdgeLuma;
    uint32_t InUpHorEdgeLuma;
    uint32_t InBotHorEdgeLuma;
    uint32_t ExHorEdgeLuma;

    uint32_t InVerEdgeLuma;
    uint32_t InLeftVerEdgeLuma;
    uint32_t InRightVerEdgeLuma;
    uint32_t ExVerEdgeLuma;

    uint32_t InHorEdgeU;
    uint32_t ExHorEdgeU;

    uint32_t InVerEdgeU;
    uint32_t ExVerEdgeU;

    uint32_t InHorEdgeV;
    uint32_t ExHorEdgeV;

    uint32_t InVerEdgeV;
    uint32_t ExVerEdgeV;
}VC1MBDebugInfo;

class VC1EncoderMBData;

class VC1EncDebug
{
public:
    VC1EncDebug();
    ~VC1EncDebug();

    void Init(int32_t Width, int32_t Height, uint32_t NV12flag);
    void Close();

    void WriteParams(int32_t _cur_frame, int32_t level,  vm_char *format,...);
    void WriteFrameInfo();   //wrote debug information
                             //and prepare frame params for next frame

    //Set picture params
    void SetPicType(int32_t picType);     //set current picture type
    void SetRefNum(uint32_t num);
    void SetFrameSize(int32_t frameSize); //set currenr frame size
    void NextMB();      //change X, Y MB position
    void SetCurrMBFirst(bool bSecondField = 0);
    void SetCurrMB(bool bSecondField, uint32_t NumMBX, uint32_t NumMBY);
    void SetSliceInfo(uint32_t startRow, uint32_t endRow);

    //run - level params
    void SetRunLevelCoefs(uint8_t* run, int16_t* level, uint8_t* pairs, int32_t blk_num);
    void SetRLMode(uint8_t mode, int32_t blk_num, int32_t coef_num);

    //DC, AC coef information
    void SetDCACInfo(int32_t ACPred, int16_t* pBlock, int32_t BlockStep,
                     int16_t* pPredBlock, int32_t PredBlockStep,
                     int32_t direction, int32_t blk_num);

    //MV information
    void SetMVInfo(sCoordinate* MV, int16_t predX, int16_t predY,int32_t forward);
    void SetMVInfo(sCoordinate* MV, int16_t predX, int16_t predY, int32_t forward, int32_t blk_num);

    void SetMVInfoField(sCoordinate* MV, int16_t predX, int16_t predY, int32_t forward);
    void SetMVInfoField(sCoordinate* MV, int16_t predX, int16_t predY, int32_t forward, int32_t blk_num);

    void SetMVDiff(int16_t DiffX, int16_t DiffY, int32_t forward, int32_t blk_num);
    void SetMVDiff(int16_t DiffX, int16_t DiffY, int32_t forward);

    void SetIntrpMV(int16_t X, int16_t Y, int32_t forward, int32_t blk_num);
    void SetIntrpMV(int16_t X, int16_t Y, int32_t forward);

    //block difference
    void SetBlockDifference(int16_t** pBlock, uint32_t* step);

    //MB information
    void SetMBAsSkip();
    void SetMBType(int32_t type);

    //intra information
    void SetBlockAsIntra(int32_t blk_num);

    //CBP
    void SetCPB(int32_t predcbp, int32_t cbp);

    //Interpolation Info
    void SetInterpInfoLuma(uint8_t* pBlock, int32_t step, int32_t blk_num, int32_t forward);
    void VC1EncDebug::SetInterpInfoChroma(uint8_t* pBlockU, int32_t stepU,
                                      uint8_t* pBlockV, int32_t stepV,
                                      int32_t forward);
    void SetInterpInfo(IppVCInterpolate_8u* YInfo, IppVCInterpolate_8u* UInfo, IppVCInterpolate_8u* VInfo,
        int32_t forward, bool padded);

    void SetInterpolType(int32_t type);
    void SetRounControl(int32_t rc);
    void VC1EncDebug::SetACDirection(int32_t direction, int32_t blk_num);

    void SetQuant(int32_t MQuant, uint8_t  HalfQP);
    void SetHalfCoef(bool half);

    void SetDeblkFlag(bool flag);
    void SetVTSFlag(bool flag);

    void SetDblkHorEdgeLuma(uint32_t ExHorEdge,     uint32_t InHorEdge,
                            uint32_t InUpHorEdge,   uint32_t InBotHorEdge);
    void SetDblkVerEdgeLuma(uint32_t ExVerEdge,     uint32_t InVerEdge,
                            uint32_t InLeftVerEdge, uint32_t InRightVerEdge);

    void SetDblkHorEdgeU(uint32_t ExHorEdge, uint32_t InHorEdge);
    void SetDblkVerEdgeU(uint32_t ExVerEdge, uint32_t InVerEdge);

    void SetDblkHorEdgeV(uint32_t ExHorEdge, uint32_t InHorEdge);
    void SetDblkVerEdgeV(uint32_t ExVerEdge, uint32_t InVerEdge);

    void SetVTSType(eTransformType type[6]);

    void SetFieldMVPred2Ref(sCoordinate *A, sCoordinate *C, int32_t forward, int32_t blk);
    void SetFieldMVPred2Ref(sCoordinate *A, sCoordinate *C, int32_t forward);

    void SetFieldMVPred1Ref(sCoordinate *A, sCoordinate *C, int32_t forward, int32_t blk);
    void SetFieldMVPred1Ref(sCoordinate *A, sCoordinate *C, int32_t forward);

    void SetHybrid(uint8_t hybrid, int32_t forward, int32_t blk);
    void SetHybrid(uint8_t hybrid, int32_t forward);

    void SetScaleType(uint8_t type, int32_t forward, int32_t blk);
    void SetScaleType(uint8_t type, int32_t forward);

    void SetPredFlag(uint8_t pred, int32_t forward, int32_t blk);
    void SetPredFlag(uint8_t pred, int32_t forward);

    void PrintRestoredFrame(uint8_t* pY, int32_t Ystep, uint8_t* pU, int32_t Ustep, uint8_t* pV, int32_t Vstep, bool BFrame);

    void PrintSmoothingDataFrame(uint8_t MBXpos, uint8_t MBYpos,
                                 VC1EncoderMBData* pCurRData,
                                 VC1EncoderMBData* pLeftRData,
                                 VC1EncoderMBData* pLeftTopRData,
                                 VC1EncoderMBData* pTopRData);

private:
    vm_file*  LogFile;
    uint32_t FrameCount;
    uint32_t FrameType;
    int32_t FrameSize;
    uint32_t FieldPic;
    uint32_t RefNum;
    uint32_t NV12;

    int32_t MBHeight; //frame height in macroblocks
    int32_t MBWidth;  //frame width in macroblocks

    int32_t XPos;      // current MB X position
    int32_t YPos;      // current MB Y position

    int32_t XFieldPos;      // current MB X position
    int32_t YFieldPos;      // current MB Y position

    uint32_t NumSlices;
    uint32_t Slices[512][2];//0 - start row; 1 - end row

    VC1MBDebugInfo* MBs;
    VC1MBDebugInfo* pCurrMB;

    int32_t InterpType;
    int32_t RoundControl;
    int32_t HalfCoef;
    bool DeblkFlag;
    bool VTSFlag;

    void PrintBlockDifference(int32_t blk_num);
    void PrintInterpolationInfo();
    void PrintInterpolationInfoField();
    void PrintCopyPatchInterpolation(int32_t blk_num, uint8_t back);
    void PrintInterpQuarterPelBicubic(int32_t blk_num, uint8_t back);
    void PrintInterpQuarterPelBicubicVert(int32_t blk_num, uint8_t back);
    void PrintInterpQuarterPelBicubicHoriz(int32_t blk_num, uint8_t back);

    void PrintChroma_B_4MV(uint8_t back);
    void PrintChroma_P_4MVField();
    void PrintChroma_B_4MVField();
    void PrintChroma();
    void Print1MVHalfPelBilinear_P();
    void Print1MVHalfPelBilinear_PField();
    void Print1MVHalfPelBilinear_B_FB();
    void Print1MVHalfPelBilinear_B_F();
    void Print1MVHalfPelBilinear_B_B();
    void Print1MVHalfPelBilinear_B_FB_Field();
    void Print1MVHalfPelBilinear_B_F_Field();
    void Print1MVHalfPelBilinear_B_B_Field();

    void PrintMVInfo();
    void PrintMVFieldInfo();
    void PrintFieldMV(int32_t forward);
    void PrintFieldMV(int32_t forward, int32_t blk);

    void PrintRunLevel(int32_t blk_num);
    void PrintDblkInfo(uint32_t start, uint32_t end);
    void PrintDblkInfoVTS(uint32_t start, uint32_t end);
    void PrintDblkInfoNoVTS(uint32_t start, uint32_t end);
    void PrintFieldDblkInfo(bool bSecondField, uint32_t start, uint32_t end);
    void PrintFieldDblkInfoVTS(bool bSecondField, uint32_t start, uint32_t end);
    void PrintFieldDblkInfoNoVTS(bool bSecondField, uint32_t start, uint32_t end);
    void PrintBlkVTSInfo();
    void PrintPictureType();
    void PrintFieldInfo(bool bSecondField, uint32_t start, uint32_t end); //wrote debug information
    void PrintFieldInfoSlices(bool bSecondField); //wrote debug information
    void PrintFrameInfo(uint32_t start, uint32_t end);
    void PrintFrameInfoSlices();
};

extern VC1EncDebug *pDebug;

#endif
}//namespace

#endif //__UMC_VC1_ENC_DEBUG_H__

#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
