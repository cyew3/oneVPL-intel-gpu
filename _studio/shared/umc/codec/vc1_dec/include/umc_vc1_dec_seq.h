// Copyright (c) 2004-2019 Intel Corporation
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

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#ifndef __UMC_VC1_DEC_SEQ_H__
#define __UMC_VC1_DEC_SEQ_H__

#include "umc_vc1_common_defs.h"
#include "umc_frame_allocator.h"

typedef struct
{
    uint16_t                   MBStartRow;
    uint16_t                   MBEndRow;
    uint16_t                   MBRowsToDecode;
    uint32_t*                  m_pstart;
    int32_t                   m_bitOffset;
    VC1PictureLayerHeader*   m_picLayerHeader;
    VC1VLCTables*            m_vlcTbl;
    bool                     is_continue;
    uint32_t                   slice_settings;
#ifdef ALLOW_SW_VC1_FALLBACK
    IppiEscInfo_VC1          EscInfo;
#endif
    bool                     is_NewInSlice;
    bool                     is_LastInSlice;
    int32_t                   iPrevDblkStartPos; //need to interlace frames
} SliceParams;

//sequence layer
VC1Status SequenceLayer                               (VC1Context* pContext);

//picture layer
//Simple/main
VC1Status GetNextPicHeader                            (VC1Context* pContext, bool isExtHeader);
VC1Status DecodePictureHeader                         (VC1Context* pContext,  bool isExtHeader);
VC1Status Decode_PictureLayer                         (VC1Context* pContext);

//Advanced
VC1Status DecodePictureHeader_Adv                     (VC1Context* pContext);
VC1Status GetNextPicHeader_Adv                        (VC1Context* pContext);


//frame rate calculation
void MapFrameRateIntoMfx(uint32_t& ENR,uint32_t& EDR, uint16_t FCode);
double MapFrameRateIntoUMC(uint32_t ENR,uint32_t EDR, uint32_t& FCode);

//I,P,B headers
VC1Status DecodePicHeader                             (VC1Context* pContext);

VC1Status DecodePictHeaderParams_InterlaceFieldPicture_Adv (VC1Context* pContext);
VC1Status DecodeSkippicture                       (VC1Context* pContext);

#ifdef ALLOW_SW_VC1_FALLBACK
void      ChooseDCTable                               (VC1Context* pContext,
                                                       int32_t transDCtableIndex);

void      ChooseACTable                               (VC1Context* pContext,
                                                       int32_t transACtableIndex1,
                                                       int32_t transACtableIndex2);

void   ChooseTTMB_TTBLK_SBP                          (VC1Context* pContext);

void ChooseMBModeInterlaceFrame                       (VC1Context* pContext,
                                                       uint32_t MV4SWITCH,
                                                       uint32_t MBMODETAB);
void ChooseMBModeInterlaceField                       (VC1Context* pContext,
                                                       int32_t MBMODETAB);
void ChoosePredScaleValuePPictbl                      (VC1PictureLayerHeader* picLayerHeader);
void ChoosePredScaleValueBPictbl                      (VC1PictureLayerHeader* picLayerHeader);
uint8_t GetTTBLK                                        (VC1Context* pContext,
                                                       int32_t blk_num);
#endif

// Simple/Main
VC1Status DecodePictureLayer_ProgressiveIpicture            (VC1Context* pContext);
// Advanced
VC1Status DecodePictHeaderParams_ProgressiveIpicture_Adv    (VC1Context* pContext);
VC1Status DecodePictHeaderParams_InterlaceIpicture_Adv      (VC1Context* pContext);
VC1Status DecodeFieldHeaderParams_InterlaceFieldIpicture_Adv (VC1Context* pContext);
VC1Status Decode_InterlaceFieldIpicture_Adv                 (VC1Context* pContext);

// Simple/Main
VC1Status DecodePictureLayer_ProgressivePpicture            (VC1Context* pContext);
// Advanced
VC1Status DecodePictHeaderParams_ProgressivePpicture_Adv    (VC1Context* pContext);
VC1Status DecodePictHeaderParams_InterlacePpicture_Adv      (VC1Context* pContext);
VC1Status DecodeFieldHeaderParams_InterlaceFieldPpicture_Adv (VC1Context* pContext);
VC1Status Decode_InterlaceFieldPpicture_Adv                 (VC1Context* pContext);

// Simple/Main
VC1Status DecodePictureLayer_ProgressiveBpicture            (VC1Context* pContext);
// Advanced
VC1Status DecodePictHeaderParams_ProgressiveBpicture_Adv    (VC1Context* pContext);
VC1Status DecodePictHeaderParams_InterlaceBpicture_Adv      (VC1Context* pContext);
VC1Status Decode_InterlaceFieldBpicture_Adv                 (VC1Context* pContext);
VC1Status DecodeFieldHeaderParams_InterlaceFieldBpicture_Adv(VC1Context* pContext);

VC1Status MVRangeDecode                                (VC1Context* pContext);
VC1Status DMVRangeDecode                               (VC1Context* pContext);

//DeQuantization
VC1Status VOPDQuant(VC1Context* pContext);
VC1Status CalculatePQuant(VC1Context* pContext);

//Bitplane decoding
void  DecodeBitplane(VC1Context* pContext, VC1Bitplane* pBitplane, int32_t rowMB, int32_t colMB, int32_t offset);

VC1Status EntryPointLayer(VC1Context* m_pContext);


#ifdef ALLOW_SW_VC1_FALLBACK

VC1Status FillTablesForIntensityCompensation(VC1Context* pContext,
    uint32_t scale,
    uint32_t shift);

VC1Status FillTablesForIntensityCompensation_Adv(VC1Context* pContext,
    uint32_t scale,
    uint32_t shift,
    uint32_t bottom_field,
    int32_t index);

//for threading
void PrepareForNextFrame(VC1Context*pContext);

//I picture MB Layer
VC1Status MBLayer_ProgressiveIpicture                  (VC1Context* pContext);
VC1Status MBLayer_ProgressiveIpicture_Adv              (VC1Context* pContext);
VC1Status MBLayer_Frame_InterlaceIpicture              (VC1Context* pContext);
VC1Status MBLayer_Field_InterlaceIpicture              (VC1Context* pContext);

//P picture MB Layer
VC1Status MBLayer_ProgressivePpicture                  (VC1Context* pContext);
VC1Status MBLayer_Frame_InterlacedPpicture             (VC1Context* pContext);
VC1Status MBLayer_Field_InterlacedPpicture              (VC1Context* pContext);
VC1Status MBLayer_ProgressivePpicture_Adv              (VC1Context* pContext);

//B picture MB Layer
VC1Status MBLayer_ProgressiveBpicture                  (VC1Context* pContext);
VC1Status MBLayer_Frame_InterlacedBpicture             (VC1Context* pContext);
VC1Status MBLayer_Field_IntrlacedBpicture              (VC1Context* pContext);
VC1Status MBLayer_ProgressiveBpicture_Adv              (VC1Context* pContext);
VC1Status MBLayer_Field_InterlacedBpicture             (VC1Context* pContext);

void GetIntraDCPredictors                              (VC1Context* pContext);
void GetPDCPredictors                                  (VC1Context* pContext);
void GetIntraScaleDCPredictors                         (VC1Context* pContext);
void GetPScaleDCPredictors                             (VC1Context* pContext);
int32_t DecodeSymbol                                    (uint32_t** pbs,
                                                        int32_t* bitOffset,
                                                        int16_t* run,
                                                        int16_t* level,
                                                        int32_t* last_flag,
                                                        const IppiACDecodeSet_VC1 * decodeSet,
                                                        IppiEscInfo_VC1* EscInfo);

IppStatus DecodeBlockACIntra_VC1                        (IppiBitstream* pBitstream,
                                                         int16_t* pDst,
                                                         const  uint8_t* pZigzagTbl,
                                                         const IppiACDecodeSet_VC1 * pDecodeSet,
                                                         IppiEscInfo_VC1* pEscInfo);

#ifdef _MSVC_LANG
#pragma warning(disable : 4100)
#endif

// interfaces should be tha same
IppStatus DecodeBlockInter8x8_VC1                       (IppiBitstream* pBitstream,
                                                         int16_t* pDst,
                                                         const  uint8_t* pZigzagTbl,
                                                         const IppiACDecodeSet_VC1 * pDecodeSet,
                                                         IppiEscInfo_VC1* pEscInfo,
                                                         int32_t subBlockPattern);

IppStatus DecodeBlockInter4x8_VC1                       (IppiBitstream* pBitstream,
                                                         int16_t* pDst,
                                                         const  uint8_t* pZigzagTbl,
                                                         const IppiACDecodeSet_VC1 * pDecodeSet,
                                                         IppiEscInfo_VC1* pEscInfo,
                                                         int32_t subBlockPattern);

IppStatus DecodeBlockInter8x4_VC1                       (IppiBitstream* pBitstream,
                                                         int16_t* pDst,
                                                         const  uint8_t* pZigzagTbl,
                                                         const IppiACDecodeSet_VC1 * pDecodeSet,
                                                         IppiEscInfo_VC1* pEscInfo,
                                                         int32_t subBlockPattern);

IppStatus DecodeBlockInter4x4_VC1                       (IppiBitstream* pBitstream,
                                                         int16_t* pDst,
                                                         const  uint8_t* pZigzagTbl,
                                                         const IppiACDecodeSet_VC1 * pDecodeSet,
                                                         IppiEscInfo_VC1* pEscInfo,
                                                         int32_t subBlockPattern);

inline
int32_t CalculateLeftTopRightPositionFlag (VC1SingletonMB* sMB)
{
    int32_t LeftTopRightPositionFlag = VC1_COMMON_MB;
    //Top position
    //macroblock is on the first column
    //#0 and #1 blocks have top border
    if(sMB->slice_currMBYpos == 0)
    {
        LeftTopRightPositionFlag |= VC1_TOP_MB;
    }
    //Left position
    //macroblock is on the first row
    //#0 and #2 blocks have left border
    if(sMB->m_currMBXpos == 0)
    {
        LeftTopRightPositionFlag |= VC1_LEFT_MB;
    }

    //Right position

    if (sMB->m_currMBXpos == sMB->widthMB - 1)
    {
        LeftTopRightPositionFlag |= VC1_RIGHT_MB;
    }
    return LeftTopRightPositionFlag;
}
void CalculateIntraFlag                                (VC1Context* pContext);

//Block layer
VC1Status BLKLayer_Intra_Luma                          (VC1Context* pContext,
                                                        int32_t blk_num,
                                                        uint32_t bias,
                                                        uint32_t ACPRED);
VC1Status BLKLayer_Intra_Chroma                        (VC1Context* pContext,
                                                        int32_t blk_num,
                                                        uint32_t bias,
                                                        uint32_t ACPRED);
VC1Status BLKLayer_Inter_Luma                          (VC1Context* pContext,
                                                        int32_t blk_num);
VC1Status BLKLayer_Inter_Chroma                        (VC1Context* pContext,
                                                        int32_t blk_num);

VC1Status BLKLayer_Intra_Luma_Adv                     (VC1Context* pContext,
                                                       int32_t blk_num,
                                                       uint32_t ACPRED);
VC1Status BLKLayer_Intra_Chroma_Adv                   (VC1Context* pContext,
                                                       int32_t blk_num,
                                                       uint32_t ACPRED);
VC1Status BLKLayer_Inter_Luma_Adv                     (VC1Context* pContext,
                                                       int32_t blk_num);
VC1Status BLKLayer_Inter_Chroma_Adv                   (VC1Context* pContext,
                                                       int32_t blk_num);

int32_t CalculateCBP                                    (VC1MB* pCurrMB,
                                                        uint32_t decoded_cbpy,
                                                        int32_t width);

uint16_t DecodeMVDiff                      (VC1Context* pContext,int32_t hpelfl,
                                          int16_t *dmv_x, int16_t *dmv_y);

void DecodeMVDiff_Adv                    (VC1Context* pContext,int16_t* pdmv_x, int16_t* pdmv_y);
uint8_t DecodeMVDiff_TwoReferenceField_Adv  (VC1Context* pContext,
                                           int16_t* pdmv_x,
                                           int16_t* pdmv_y);


void Progressive1MVPrediction                               (VC1Context* pContext);
void Progressive4MVPrediction                               (VC1Context* pContext);
void Field1MVPrediction                                     (VC1Context* pContext);
void Field4MVPrediction                                     (VC1Context* pContext);

inline void ApplyMVPrediction  ( VC1Context* pContext,
                                 int32_t blk_num,
                                 int16_t* pMVx, int16_t* pMVy,
                                 int16_t dmv_x, int16_t dmv_y,
                                 int32_t Backwards)
{
    const VC1MVRange *pMVRange = pContext->m_picLayerHeader->m_pCurrMVRangetbl;
    uint16_t RangeX, RangeY, YBias=0;
    int32_t Count;
    VC1MB *pMB = pContext->m_pCurrMB;
    int16_t MVx, MVy;
    RangeX = pMVRange->r_x;
    RangeY = pMVRange->r_y;

    dmv_x = (int16_t) (dmv_x + *pMVx);
    dmv_y = (int16_t) (dmv_y + *pMVy);

    MVx = (int16_t) (((dmv_x + RangeX) & ((RangeX << 1) - 1)) - RangeX);
    MVy = (int16_t) (((dmv_y + RangeY - YBias) & ( (RangeY <<1) - 1)) - RangeY + YBias);

    if((pMB->mbType&0x03) == VC1_MB_1MV_INTER)
    {
        for(Count = 0; Count < 4; Count++)
        {
            pMB->m_pBlocks[Count].mv[Backwards][0] = (int16_t)MVx;
            pMB->m_pBlocks[Count].mv[Backwards][1] = (int16_t)MVy;
        }
    }
    else if((pMB->mbType&0x03) == VC1_MB_2MV_INTER)
    {
        for(Count = 0; Count < 2; Count++)
        {
            pMB->m_pBlocks[Count+blk_num].mv[Backwards][0] = (int16_t)MVx;
            pMB->m_pBlocks[Count+blk_num].mv[Backwards][1] = (int16_t)MVy;
        }
    }
    else    /* 4MV */
    {
        pMB->m_pBlocks[blk_num].mv[Backwards][0] = (int16_t)MVx;
        pMB->m_pBlocks[blk_num].mv[Backwards][1] = (int16_t)MVy;
    }
    *pMVx = MVx;
    *pMVy = MVy;
}

/* Apply MVPrediction need to lead to ApplyMVPredictionCalculate*/
void ApplyMVPredictionCalculate (VC1Context* pContext,
                                       int16_t* pMVx,
                                       int16_t* pMVy,
                                       int32_t dmv_x,
                                       int32_t dmv_y);
void ApplyMVPredictionCalculateOneReference( VC1PictureLayerHeader* picLayerHeader,
                                             int16_t* pMVx,
                                             int16_t* pMVy,
                                             int32_t dmv_x,
                                             int32_t dmv_y,
                                             uint8_t same_polatity);
void ApplyMVPredictionCalculateTwoReference( VC1PictureLayerHeader* picLayerHeader,
                                             int16_t* pMVx,
                                             int16_t* pMVy,
                                             int32_t dmv_x,
                                             int32_t dmv_y,
                                             uint8_t same_polatity);

void Decode_BMVTYPE                (VC1Context* pContext);
void Decode_InterlaceFrame_BMVTYPE (VC1Context* pContext);
void Decode_InterlaceField_BMVTYPE (VC1Context* pContext);
void CalculateMV              (int16_t x[],int16_t y[], int16_t *X, int16_t* Y);

void CalculateMV_Interlace    (int16_t x[],int16_t y[],
                               int16_t x_bottom[],int16_t y_bottom[],
                               int16_t *Xt, int16_t* Yt,int16_t *Xb, int16_t* Yb );

void CalculateMV_InterlaceField  (VC1Context* pContext, int16_t *X, int16_t* Y);

void Scale_Direct_MV          (VC1PictureLayerHeader* picHeader, int16_t X, int16_t Y,
                               int16_t* Xf, int16_t* Yf,int16_t* Xb, int16_t* Yb);

void Scale_Direct_MV_Interlace(VC1PictureLayerHeader* picHeader, int16_t X, int16_t Y,
                               int16_t* Xf, int16_t* Yf,int16_t* Xb, int16_t* Yb);

void PullBack_PPred(VC1Context* pContext, int16_t *pMVx, int16_t* pMVy, int32_t blk_num);
void PullBack_PPred4MV(VC1SingletonMB* sMB, int16_t *pMVx, int16_t* pMVy, int32_t blk_num);

void AssignCodedBlockPattern                          (VC1MB * pMB,VC1SingletonMB* sMB);

uint8_t GetSubBlockPattern_8x4_4x8                  (VC1Context* pContext,int32_t num_blk);
uint8_t GetSubBlockPattern_4x4                      (VC1Context* pContext,int32_t num_blk);

uint32_t GetDCStepSize                                (int32_t MQUANT);

inline int16_t median3(int16_t* pSrc)
{
   if(pSrc[0] > pSrc[1])
    {
        if(pSrc[1]>pSrc[2])
            return pSrc[1];

        if(pSrc[0]>pSrc[2])
            return pSrc[2];
        else
            return pSrc[0];
    }
    else
    {
        if(pSrc[0]>pSrc[2])
            return pSrc[0];

        if(pSrc[1]>pSrc[2])
            return pSrc[2];
        else
            return pSrc[1];
    }
}

inline int16_t median4(int16_t* pSrc)
{
    int16_t max, min;
    int16_t med;
    static mfxSize size = {1,4};
    ippiMinMax_16s_C1R(pSrc, 2, size, &min, &max);
    med = (int16_t) ((pSrc[0]+ pSrc[1] + pSrc[2] + pSrc[3] - max - min) / 2);
    return med;
}

inline
uint16_t vc1_abs_16s(int16_t pSrc)
{
    uint16_t S;
    S = (uint16_t) (pSrc >> 15);
    S = (uint16_t) ((pSrc + S)^S);
    return S;
}

void DecodeTransformInfo    (VC1Context* pContext);
VC1Status GetTTMB           (VC1Context* pContext);

//MV prediction
//common
void DeriveSecondStageChromaMV                        (VC1Context* pContext,
                                                       int16_t* xMV,
                                                       int16_t* yMV);
void DeriveSecondStageChromaMV_Interlace              (VC1Context* pContext,
                                                       int16_t* xMV,
                                                       int16_t* yMV);


void PackDirectMVProgressive(VC1MB* pCurrMB, int16_t* pSavedMV);
void PackDirectMVIField(VC1MB* pCurrMB, int16_t* pSavedMV, bool isBottom, uint8_t* bRefField);
void PackDirectMVIFrame(VC1MB* pCurrMB, int16_t* pSavedMV);
void PackDirectMVs(VC1MB*  pCurrMB,
                   int16_t* pSavedMV,
                   uint8_t   isBottom,
                   uint8_t*  bRefField,
                   uint32_t  FCM);

inline void Derive4MV(VC1SingletonMB* sMB, int16_t* xMV, int16_t* yMV)
{
    uint32_t _MVcount = sMB->MVcount;
    int16_t* xLuMV = sMB->xLuMV;
    int16_t* yLuMV = sMB->yLuMV;
    switch(_MVcount)
    {
    case 0:
    case 1:
        {
            *xMV = VC1_MVINTRA;
            *yMV = VC1_MVINTRA;
            return;
        }
    case 2:
        {
            *xMV = (int16_t) ((xLuMV[0] + xLuMV[1]) / 2);
            *yMV = (int16_t) ((yLuMV[0] + yLuMV[1]) / 2);
            return;
        }
    case 3:
        {
            *xMV = median3(xLuMV);
            *yMV = median3(yLuMV);
            return;
        }
    case 4:
        {
            *xMV = median4(xLuMV);
            *yMV = median4(yLuMV);
            return;
        }
    }
}

void Derive4MV_Field                                  (uint32_t _MVcount,
                                                       int16_t* xMV, int16_t* yMV,
                                                       int16_t* xLuMV, int16_t* yLuMV);

void GetPredictProgressiveMV                          (VC1Block *pA,
                                                       VC1Block *pB,
                                                       VC1Block *pC,
                                                       int16_t *pX,
                                                       int16_t *pY,
                                                       int32_t Back);

void HybridMV                                         (VC1Context* pContext,
                                                       VC1Block *pA,
                                                       VC1Block *pC,
                                                       int16_t *pPredMVx,
                                                       int16_t *pPredMVy,
                                                       int32_t Back);


void CalculateMV                                     (int16_t x[],
                                                      int16_t y[],
                                                      int16_t *X,
                                                      int16_t* Y);

void Decode_BMVTYPE                                  (VC1Context* pContext);
void Decode_InterlaceFrame_BMVTYPE                   (VC1Context* pContext);

VC1Status PredictBlock_P                            (VC1Context* pContext);
VC1Status PredictBlock_B                            (VC1Context* pContext);

VC1Status PredictBlock_InterlacePPicture            (VC1Context* pContext);
VC1Status PredictBlock_InterlaceBPicture            (VC1Context* pContext);

VC1Status PredictBlock_InterlaceFieldPPicture       (VC1Context* pContext);
VC1Status PredictBlock_InterlaceFieldBPicture       (VC1Context* pContext);

void CropLumaPullBack                                 (VC1Context* pContext,
                                                       int16_t* xMV, int16_t* yMV);
void CropChromaPullBack                               (VC1Context* pContext,
                                                       int16_t* xMV, int16_t* yMV);
#define PullBack_BDirect CropChromaPullBack // same algorithm

void SZTables                                           (VC1Context* pContext);
void CreateComplexICTablesForFields                     (VC1Context* pContext);
void CreateComplexICTablesForFrame                      (VC1Context* pContext);
void UpdateICTablesForSecondField                       (VC1Context* pContext);

VC1Status Set_MQuant                                  (VC1Context* pContext);
VC1Status Set_MQuant_Field                            (VC1Context* pContext);
VC1Status Set_Alt_MQUANT                              (VC1Context* pContext);
void GetMQUANT                                        (VC1Context* pContext);

//advanced
void CropLumaPullBack_Adv                             (VC1Context* pContext,
                                                       int16_t* xMV,
                                                       int16_t* yMV);
void CropLumaPullBackField_Adv                        (VC1Context* pContext,
                                                       int16_t* xMV,
                                                       int16_t* yMV);

void CropChromaPullBack_Adv                           (VC1Context* pContext,
                                                       int16_t* xMV,
                                                       int16_t* yMV);

void CalculateProgressive1MV                          (VC1Context* pContext,
                                                        int16_t *pPredMVx,
                                                        int16_t *pPredMVy);
void CalculateProgressive4MV                           (VC1Context* pContext,
                                                        int16_t *pPredMVx,
                                                        int16_t *pPredMVy,
                                                        int32_t blk_num);

void CalculateProgressive4MV_Adv                        (VC1Context* pContext,
                                                        int16_t *pPredMVx,int16_t *pPredMVy,
                                                        int32_t blk_num);

void CalculateProgressive1MV_B                          (VC1Context* pContext,
                                                         int16_t *pPredMVx,
                                                         int16_t *pPredMVy,
                                                         int32_t Back);
void CalculateProgressive1MV_B_Adv                      (VC1Context* pContext,
                                                         int16_t *pPredMVx,
                                                         int16_t *pPredMVy,
                                                         int32_t Back);

void CalculateInterlaceFrame1MV_P                       (VC1MVPredictors* MVPredictors,
                                                        int16_t *pPredMVx,
                                                        int16_t *pPredMVy);

void CalculateInterlaceFrame1MV_B                       (VC1MVPredictors* MVPredictors,
                                                           int16_t *f_x,int16_t *f_y,
                                                           int16_t *b_x,int16_t *b_y,
                                                           uint32_t back);

void CalculateInterlaceFrame1MV_B_Interpolate           (VC1MVPredictors* MVPredictors,
                                                         int16_t *f_x,int16_t *f_y,
                                                         int16_t *b_x,int16_t *b_y);


void Calculate4MVFrame_Adv                              (VC1MVPredictors* MVPredictors,
                                                        int16_t *pPredMVx,
                                                        int16_t *pPredMVy,
                                                        uint32_t blk_num);

void CalculateInterlace4MV_TopField_Adv                 (VC1MVPredictors* MVPredictors,
                                                        int16_t *pPredMVx,
                                                        int16_t *pPredMVy,
                                                        uint32_t blk_num);

void CalculateInterlace4MV_BottomField_Adv              (VC1MVPredictors* MVPredictors,
                                                        int16_t *pPredMVx,int16_t *pPredMVy,
                                                        uint32_t blk_num);

void CalculateField1MVOneReferencePPic                  (VC1Context* pContext,
                                                        int16_t *pPredMVx,
                                                        int16_t *pPredMVy);

void CalculateField1MVTwoReferencePPic                  (VC1Context* pContext,
                                                         int16_t *pPredMVx,
                                                         int16_t *pPredMVy,
                                                         uint8_t* PredFlag);
void CalculateField4MVOneReferencePPic                  (VC1Context* pContext,
                                                         int16_t *pPredMVx,
                                                         int16_t *pPredMVy,
                                                         int32_t blk_num);

void CalculateField4MVTwoReferencePPic                  (VC1Context* pContext,
                                                        int16_t *pPredMVx,
                                                        int16_t *pPredMVy,
                                                        int32_t blk_num,
                                                        uint8_t* PredFlag);

void CalculateField1MVTwoReferenceBPic                  (VC1Context* pContext,
                                                         int16_t *pPredMVx,
                                                         int16_t *pPredMVy,
                                                         int32_t Back,
                                                         uint8_t* PredFlag);

void CalculateField4MVTwoReferenceBPic                  (VC1Context* pContext,
                                                         int16_t *pPredMVx,int16_t *pPredMVy,
                                                         int32_t blk_num,int32_t Back,
                                                         uint8_t* PredFlag);

void PredictInterlaceFrame1MV                          (VC1Context* pContext);

void PredictInterlace4MVFrame_Adv                       (VC1Context* pContext);

void PredictInterlace4MVField_Adv                       (VC1Context* pContext);

void PredictInterlace2MV_Field_Adv                     (VC1MB* pCurrMB,
                                                        int16_t pPredMVx[2],
                                                        int16_t pPredMVy[2],
                                                        int16_t backTop,
                                                        int16_t backBottom,
                                                        uint32_t widthMB);

void ScaleOppositePredPPic                             (VC1PictureLayerHeader* picLayerHeader,
                                                       int16_t *x,
                                                       int16_t *y);
void ScaleSamePredPPic                                (VC1PictureLayerHeader* picLayerHeader,
                                                       int16_t *x,
                                                       int16_t *y,
                                                       int32_t dominant,
                                                       int32_t fieldFlag);

void ScaleOppositePredBPic                             (VC1PictureLayerHeader* picLayerHeader,
                                                        int16_t *x,
                                                        int16_t *y,
                                                        int32_t dominant,
                                                        int32_t fieldFlag,
                                                        int32_t back);

void ScaleSamePredBPic                                (VC1PictureLayerHeader* picLayerHeader,
                                                       int16_t *x,
                                                       int16_t *y,
                                                       int32_t dominant,
                                                       int32_t fieldFlag,
                                                       int32_t back);

void HybridFieldMV                                     (VC1Context* pContext,
                                                        int16_t *pPredMVx,
                                                        int16_t *pPredMVy,
                                                        int16_t MV_px[3],
                                                        int16_t MV_py[3]);

VC1Status MBLayer_ProgressiveBpicture_NONDIRECT_Prediction                  (VC1Context* pContext);
VC1Status MBLayer_ProgressiveBpicture_DIRECT_Prediction                     (VC1Context* pContext);
VC1Status MBLayer_ProgressiveBpicture_SKIP_NONDIRECT_Prediction             (VC1Context* pContext);
VC1Status MBLayer_ProgressiveBpicture_SKIP_DIRECT_Prediction                (VC1Context* pContext);

VC1Status MBLayer_ProgressiveBpicture_SKIP_NONDIRECT_AdvPrediction          (VC1Context* pContext);
VC1Status MBLayer_ProgressiveBpicture_SKIP_DIRECT_AdvPrediction             (VC1Context* pContext);
VC1Status MBLayer_ProgressiveBpicture_NONDIRECT_AdvPrediction               (VC1Context* pContext);
VC1Status MBLayer_ProgressiveBpicture_DIRECT_AdvPrediction                  (VC1Context* pContext);

VC1Status MBLayer_InterlaceFrameBpicture_SKIP_NONDIRECT_Prediction          (VC1Context* pContext);
VC1Status MBLayer_InterlaceFrameBpicture_SKIP_DIRECT_Prediction             (VC1Context* pContext);
VC1Status MBLayer_InterlaceFrameBpicture_NONDIRECT_Prediction               (VC1Context* pContext);
VC1Status MBLayer_InterlaceFrameBpicture_DIRECT_Prediction                  (VC1Context* pContext);

VC1Status MBLayer_InterlaceFieldBpicture_NONDIRECT_Predicition              (VC1Context* pContext);
VC1Status MBLayer_InterlaceFieldBpicture_DIRECT_Prediction                  (VC1Context* pContext);

//Smoothing
void Smoothing_I                                      (VC1Context* pContext,
                                                       int32_t Height);
void Smoothing_P                                      (VC1Context* pContext,
                                                       int32_t Height);
void Smoothing_I_Adv                                  (VC1Context* pContext,
                                                       int32_t Height);
void Smoothing_P_Adv                                  (VC1Context* pContext,
                                                       int32_t Height);

#ifdef _OWN_FUNCTION
IppStatus _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R        (int16_t* pSrcLeft, int32_t srcLeftStep,
                                                               int16_t* pSrcRight, int32_t srcRightStep,
                                                               uint8_t* pDst, int32_t dstStep,
                                                               uint32_t fieldNeighbourFlag,
                                                               uint32_t edgeDisableFlag);
IppStatus _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R        (int16_t* pSrcUpper, int32_t srcUpperStep,
                                                               int16_t* pSrcBottom, int32_t srcBottomStep,
                                                               uint8_t* pDst, int32_t dstStep,
                                                               uint32_t edgeDisableFlag);
IppStatus _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R      (int16_t* pSrcUpper, int32_t srcUpperStep,
                                                               int16_t* pSrcBottom, int32_t srcBottomStep,
                                                               uint8_t* pDst, int32_t dstStep);
IppStatus _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R      (int16_t* pSrcLeft, int32_t srcLeftStep,
                                                               int16_t* pSrcRight, int32_t srcRightStep,
                                                               uint8_t* pDst, int32_t dstStep);
#else

#define _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R      ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R
#define _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R      ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R
#define _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R    ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R
#define _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R    ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R
#endif


#ifdef _OWN_FUNCTION
IppStatus _own_FilterDeblockingLuma_VerEdge_VC1            (uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag);
IppStatus _own_FilterDeblockingChroma_VerEdge_VC1          (uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag);
IppStatus _own_FilterDeblockingLuma_HorEdge_VC1            (uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag);
IppStatus _own_FilterDeblockingChroma_HorEdge_VC1          (uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag);
#else
#define _own_FilterDeblockingLuma_VerEdge_VC1     ippiFilterDeblockingLuma_VerEdge_VC1_8u_C1IR
#define _own_FilterDeblockingChroma_VerEdge_VC1   ippiFilterDeblockingChroma_VerEdge_VC1_8u_C1IR
#define _own_FilterDeblockingLuma_HorEdge_VC1     ippiFilterDeblockingLuma_HorEdge_VC1_8u_C1IR
#define _own_FilterDeblockingChroma_HorEdge_VC1   ippiFilterDeblockingChroma_HorEdge_VC1_8u_C1IR
#endif


#ifdef _OWN_FUNCTION
IppStatus _own_ippiInterpolateQPBicubicIC_VC1_8u_C1R   (_IppVCInterpolate_8u* inter_struct);
IppStatus _own_ippiInterpolateQPBilinearIC_VC1_8u_C1R  (_IppVCInterpolate_8u* inter_struct);
#else
#define _own_ippiInterpolateQPBicubicIC_VC1_8u_C1R      ippiInterpolateQPBicubic_VC1_8u_C1R
#define _own_ippiInterpolateQPBilinearIC_VC1_8u_C1R     ippiInterpolateQPBilinear_VC1_8u_C1R
#endif
#define _own_ippiInterpolateQPBilinearIC_VC1_8u_C1R     ippiInterpolateQPBilinear_VC1_8u_C1R




#ifdef _OWN_FUNCTION
IppStatus _own_ippiQuantInvIntraUniform_VC1_16s_C1IR(int16_t* pSrcDst, int32_t srcDstStep,
                                                     int32_t doubleQuant, mfxSize* pDstSizeNZ);
IppStatus _own_ippiQuantInvIntraNonuniform_VC1_16s_C1IR(int16_t* pSrcDst, int32_t srcDstStep,
                                                     int32_t doubleQuant, mfxSize* pDstSizeNZ);
IppStatus _own_ippiQuantInvInterUniform_VC1_16s_C1IR(int16_t* pSrcDst, int32_t srcDstStep,
                                                     int32_t doubleQuant, mfxSize roiSize,
                                                     mfxSize* pDstSizeNZ);
IppStatus _own_ippiQuantInvInterNonuniform_VC1_16s_C1IR(int16_t* pSrcDst, int32_t srcDstStep,
                                                        int32_t doubleQuant, mfxSize roiSize,
                                                        mfxSize* pDstSizeNZ);
#else
#define _own_ippiQuantInvIntraUniform_VC1_16s_C1IR     ippiQuantInvIntraUniform_VC1_16s_C1IR
#define _own_ippiQuantInvIntraNonuniform_VC1_16s_C1IR  ippiQuantInvIntraNonuniform_VC1_16s_C1IR
#define _own_ippiQuantInvInterUniform_VC1_16s_C1IR     ippiQuantInvInterUniform_VC1_16s_C1IR
#define _own_ippiQuantInvInterNonuniform_VC1_16s_C1IR  ippiQuantInvInterNonuniform_VC1_16s_C1IR
#endif

IppStatus ippiPXInterpolatePXICBicubicBlock_VC1_8u_C1R(const IppVCInterpolateBlockIC_8u* interpolateInfo);
IppStatus ippiPXInterpolatePXICBilinearBlock_VC1_8u_C1R(const IppVCInterpolateBlockIC_8u* interpolateInfo);


IppStatus _own_ippiReconstructIntraUniform_VC1_16s_C1IR    (int16_t* pSrcDst, int32_t srcDstStep, int32_t doubleQuant);
IppStatus _own_ippiReconstructIntraNonuniform_VC1_16s_C1IR (int16_t* pSrcDst, int32_t srcDstStep, int32_t doubleQuant);
IppStatus _own_ippiReconstructInterUniform_VC1_16s_C1IR    (int16_t* pSrcDst, int32_t srcDstStep, int32_t doubleQuant,uint32_t BlkType);
IppStatus _own_ippiReconstructInterNonuniform_VC1_16s_C1IR (int16_t* pSrcDst, int32_t srcDstStep, int32_t doubleQuant,uint32_t BlkType);

inline int32_t SubBlockPattern(VC1Block* _pBlk, VC1SingletonBlock* _sBlk)
{
    int32_t subbpattern = 0;
    if ((_sBlk->Coded ==0)&&(_pBlk->blkType < VC1_BLK_INTRA_TOP))
    {
        _pBlk->blkType = VC1_BLK_INTER8X8;
    }
    if (_sBlk->Coded ==0)
    {
        return 0;
    }

    switch (_pBlk->blkType)
    {
    case VC1_BLK_INTER4X4:
        subbpattern =  _sBlk->numCoef;
        break;

    case VC1_BLK_INTER8X4:
        if (_sBlk->numCoef & VC1_SBP_0)
        {
            subbpattern |= 0xC;
        }

        if (_sBlk->numCoef & VC1_SBP_1)
        {
            subbpattern |= 0x3;
        }
        break;

    case VC1_BLK_INTER4X8:
        if (_sBlk->numCoef & VC1_SBP_0)
        {
            subbpattern |= 0xA;
        }

        if (_sBlk->numCoef & VC1_SBP_1)
        {
            subbpattern |= 0x5;
        }
        break;

    default:
        subbpattern = 0xF;
    }
    return subbpattern;
}

void HorizontalDeblockingBlkI                         (uint8_t* pUUpBlock,
                                                       uint32_t Pquant,
                                                       int32_t Pitch);

void VerticalDeblockingBlkI                           (uint8_t* pUUpLBlock,
                                                       uint32_t Pquant,
                                                       int32_t Pitch);
//Deblocking simple/main
void Deblocking_ProgressiveIpicture                   (VC1Context* pContext);

void Deblocking_ProgressivePpicture                   (VC1Context* pContext);
//Deblocking advanced
void Deblocking_ProgressiveIpicture_Adv               (VC1Context* pContext);

void Deblocking_ProgressivePpicture_Adv               (VC1Context* pContext);
void Deblocking_InterlaceFieldBpicture_Adv            (VC1Context* pContext);

void Deblocking_InterlaceFrameIpicture_Adv            (VC1Context* pContext);
void Deblocking_InterlaceFramePpicture_Adv            (VC1Context* pContext);

void HorizontalDeblockingBlkInterlaceI                (uint8_t* pUUpBlock,
                                                       uint32_t Pquant,
                                                       int32_t Pitch,
                                                       uint32_t foffset_1);

void VerticalDeblockingBlkInterlaceI                  (uint8_t*pUUpLBlock,
                                                       uint32_t Pquant,
                                                       int32_t Pitch,
                                                       int32_t foffset_1);


#ifdef _OWN_FUNCTION
//range mape
void _own_ippiRangeMap_VC1_8u_C1R                     (uint8_t* pSrc,
                                                       int32_t srcStep,
                                                       uint8_t* pDst,
                                                       int32_t dstStep,
                                                       mfxSize roiSize,
                                                       int32_t rangeMapParam);
#else
#define _own_ippiRangeMap_VC1_8u_C1R    ippiRangeMapping_VC1_8u_C1R
#endif

void RangeMapping(VC1Context* pContext, UMC::FrameMemID input, UMC::FrameMemID output);
void RangeDown                                        (uint8_t* pSrc,
                                                       int32_t srcStep,
                                                       uint8_t* pDst,
                                                       int32_t dstStep,
                                                       int32_t width,
                                                       int32_t height);

void RangeRefFrame                                  (VC1Context* pContext);

//Advanced

void write_Intraluma_to_interlace_frame_Adv          (VC1MB * pCurrMB,
                                                      int16_t* pBlock);

void write_Interluma_to_interlace_frame_Adv          (VC1MB * pCurrMB,
                                                      uint8_t* pPred,
                                                      int16_t* pBlock);
void write_Interluma_to_interlace_frame_MC_Adv       (VC1MB * pCurrMB,
                                                      const uint8_t* pDst,
                                                      uint32_t dstStep,
                                                      int16_t* pBlock);
void write_Interluma_to_interlace_frame_MC_Adv_Copy   (VC1MB * pCurrMB,
                                                      int16_t* pBlock);

void write_Interluma_to_interlace_B_frame_MC_Adv     (VC1MB * pCurrMB,
                                                      const uint8_t* pDst1,
                                                      uint32_t dstStep1,
                                                      const uint8_t* pDst2,
                                                      uint32_t dstStep2,
                                                      int16_t* pBlock);
VC1Status VC1ProcessDiffIntra                         (VC1Context* pContext,int32_t blk_num);
VC1Status VC1ProcessDiffInter                         (VC1Context* pContext,int32_t blk_num);
VC1Status VC1ProcessDiffSpeedUpIntra                  (VC1Context* pContext,int32_t blk_num);
VC1Status VC1ProcessDiffSpeedUpInter                  (VC1Context* pContext,int32_t blk_num);

inline int16_t PullBack_PredMV(int16_t *pMV,int32_t pos,
                            int32_t Min, int32_t Max)
{
    int16_t MV = *pMV;
    int32_t IMV = pos + MV;

    if (IMV < Min)
    {
        MV = (int16_t)(Min - pos);
    }

    else if (IMV > Max)
    {
        MV = (int16_t)(Max - pos);
    }

    return MV;
}
#endif // #ifdef ALLOW_SW_VC1_FALLBACK

#endif //__umc_vc1_dec_seq_H__
#endif //MFX_ENABLE_VC1_VIDEO_DECODE
