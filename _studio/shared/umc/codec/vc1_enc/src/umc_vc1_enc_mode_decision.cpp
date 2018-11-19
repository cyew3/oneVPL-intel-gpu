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

#include "umc_vc1_enc_mode_decision.h"
#include "umc_vc1_enc_common.h"
#include "umc_vc1_enc_tables.h"

namespace UMC_VC1_ENCODER
{

    void SaveResidual8x8Inter (int16_t* pBlk,uint32_t  step, const uint8_t* pScanMatrix, int32_t blk, uint8_t uRunArr[6][65], int16_t iLevelArr[6][64], uint8_t nPairsArr[6][4]);
    void SaveResidual8x4Inter (int16_t* pBlk,uint32_t  step, const uint8_t* pScanMatrix, int32_t blk, uint8_t uRunArr[6][65], int16_t iLevelArr[6][64], uint8_t nPairsArr[6][4]);
    void SaveResidual4x8Inter (int16_t* pBlk,uint32_t  step, const uint8_t* pScanMatrix, int32_t blk, uint8_t uRunArr[6][65], int16_t iLevelArr[6][64], uint8_t nPairsArr[6][4]);
    void SaveResidual4x4Inter (int16_t* pBlk,uint32_t  step, const uint8_t* pScanMatrix, int32_t blk, uint8_t uRunArr[6][65], int16_t iLevelArr[6][64], uint8_t nPairsArr[6][4]);

    typedef void (*pSaveResidual) (int16_t* pBlk,uint32_t  step, const uint8_t* pScanMatrix, int32_t blk, uint8_t uRunArr[6][65], int16_t iLevelArr[6][64], uint8_t nPairsArr[6][4]);
    
    static uint32_t     CalculateBlockAC(         eTransformType      tsType,
                                                uint8_t               *pRun,
                                                int16_t              *pLevel,
                                                uint8_t               *pnPairs,
                                                const sACTablesSet  *pACTablesSet)
    {
        int32_t                  i                      = 0;
        static const int32_t     nSubblk [4]     = {1,2,2,4};

        const uint32_t            *pEncTable   = pACTablesSet->pEncTable;
        uint8_t                   nPairsBlock  = 0;
        uint32_t                  BlockLen     = 0;     


 
        for (int32_t nSubblock=0; nSubblock<nSubblk[tsType]; nSubblock++)
        {
            const uint8_t     *pTableDR    = pACTablesSet->pTableDR;
            const uint8_t     *pTableDL    = pACTablesSet->pTableDL;
            const uint8_t     *pTableInd   = pACTablesSet->pTableInd;
            uint8_t           nPairs       = pnPairs[nSubblock];

            if (nPairs == 0)
                continue;

            // put codes into bitstream
            i = 0;
            for (int32_t not_last = 1; not_last>=0; not_last--)
            {
                for (; i < nPairs - not_last; i++)
                {
                    bool    sign = false;
                    uint8_t   run  = pRun [i+nPairsBlock];
                    int16_t  lev  = pLevel[i+nPairsBlock];

                    uint8_t mode = GetMode( run, lev, pTableDR, pTableDL, sign);

                    switch (mode)
                    {
                        case 3:
                            BlockLen += pEncTable[1]; //ESCAPE
                            BlockLen += 22;            
                            break;
                        case 2:
                        case 1:
                            BlockLen += pEncTable[1]; //ESCAPE
                            BlockLen += mode;         //mode
                         case 0:
                            int16_t index = pTableInd[run] + lev;
                            BlockLen +=  pEncTable[2*index + 1];
                            BlockLen += 1;
                            break;

                    }
                }

                pTableDR    = pACTablesSet->pTableDRLast;
                pTableDL    = pACTablesSet->pTableDLLast;
                pTableInd   = pACTablesSet->pTableIndLast;

            }
            nPairsBlock = nPairsBlock + pnPairs[nSubblock];
        }
 
        return BlockLen;
    }

#define BIG_COST 0x1234567;

    UMC::Status GetVSTTypeP_RD (VC1EncMD_P* pIn, eTransformType* pVSTTypeOut) 
    {
        int16_t TempBlock16s [4][64];
        uint8_t  TempBlock8u  [64];

        unsigned long long BlockRate[6][4] = {0};
        int32_t BlockCost[6][4] = {0};

        uint8_t  Run[6][65]={0};
        int16_t Level[6][64]={0};
        uint8_t  nPairs[4][6][4]={0};

        uint32_t RDLambda0 = (pIn->bUniform)? 10*(pIn->quant>>1)*(pIn->quant>>1):35*(pIn->quant>>1)*(pIn->quant>>1);
        uint32_t RDLambda1 = (pIn->bUniform)? 10*(pIn->quant)*(pIn->quant):35*(pIn->quant)*(pIn->quant);
        uint32_t RDLambda[6] = {RDLambda0,RDLambda0,RDLambda0,RDLambda0,RDLambda1,RDLambda1};
        
        if(RDLambda0<0 ||RDLambda1<0) 
            return UMC::UMC_OK;
        
        static const eTransformType pVST [4] = {  VC1_ENC_8x8_TRANSFORM,VC1_ENC_8x4_TRANSFORM, VC1_ENC_4x8_TRANSFORM, VC1_ENC_4x4_TRANSFORM };
        pSaveResidual SaveResidual[4] = {SaveResidual8x8Inter, SaveResidual8x4Inter, SaveResidual4x8Inter, SaveResidual4x4Inter};


        uint8_t* srcBlockPtr [6] = {  pIn->pYSrc, 
                                    pIn->pYSrc + 8, 
                                    pIn->pYSrc + (pIn->srcYStep<<3), 
                                    pIn->pYSrc + (pIn->srcYStep<<3) + 8,
                                    pIn->pUSrc,
                                    pIn->pVSrc};
        uint8_t* refBlockPtr [6] = {  pIn->pYRef, 
                                    pIn->pYRef + 8, 
                                    pIn->pYRef + (pIn->refYStep<<3), 
                                    pIn->pYRef + (pIn->refYStep<<3) + 8,
                                    pIn->pURef,
                                    pIn->pVRef};
        uint32_t srcBlockStep [6] = { pIn->srcYStep,
                                    pIn->srcYStep,
                                    pIn->srcYStep,
                                    pIn->srcYStep,
                                    pIn->srcUStep,
                                    pIn->srcVStep};
        uint32_t refBlockStep [6] = { pIn->refYStep,
                                    pIn->refYStep,
                                    pIn->refYStep,
                                    pIn->refYStep,
                                    pIn->refUStep,
                                    pIn->refVStep};

        InterTransformQuantFunction InterTransformQuantACFunction = (pIn->bUniform) ? InterTransformQuantUniform :InterTransformQuantNonUniform;
        InterInvTransformQuantFunction InterInvTransformQuantACFunction = (pIn->bUniform) ? InterInvTransformQuantUniform : InterInvTransformQuantNonUniform;
        
        const int16_t (*pTTMBVLC)[4][6]   = 0;
        const uint8_t  (* pTTBlkVLC)[6]    = 0;
        const uint8_t   *pSubPattern4x4VLC = 0;
        uint32_t quant = pIn->quant>>1;

        eCodingSet            CodingSetInter   = CodingSetsInter [quant>8][pIn->DecTypeAC1];
        const sACTablesSet*   pACTablesSetInter = &(ACTablesSet[CodingSetInter]);

        if (quant<5)
        {
            pTTMBVLC            =  TTMBVLC_HighRate;
            pTTBlkVLC           =  TTBLKVLC_HighRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
        }
        else if (quant<13)
        {
            pTTMBVLC            =  TTMBVLC_MediumRate;
            pTTBlkVLC           =  TTBLKVLC_MediumRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

        }
        else
        {
            pTTMBVLC            =  TTMBVLC_LowRate;
            pTTBlkVLC           =  TTBLKVLC_LowRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
        }   
        const uint16_t*   pCBPCYTable  = VLCTableCBPCY_PB[pIn->CBPTab];
        const uint32_t*   pCBPCYTableF = CBPCYFieldTable_VLC[pIn->CBPTab];

        for (uint32_t blk = 0; blk < 6; blk++)
        {
            if ((pIn->intraPattern & (1<<VC_ENC_PATTERN_POS(blk)))!=0)
                continue;
            
            ippiSub8x8_8u16s_C1R(srcBlockPtr[blk],srcBlockStep[blk],refBlockPtr[blk],refBlockStep[blk],TempBlock16s[0],8*sizeof(int16_t));

            memcpy (TempBlock16s[1],TempBlock16s[0],64*sizeof(int16_t));
            memcpy (TempBlock16s[2],TempBlock16s[0],64*sizeof(int16_t));
            memcpy (TempBlock16s[3],TempBlock16s[0],64*sizeof(int16_t));
            
            for (uint32_t trasf_mode = 0; trasf_mode < 4; trasf_mode ++)
            {
                InterTransformQuantACFunction (TempBlock16s[trasf_mode],8*sizeof(int16_t),pVST [trasf_mode], pIn->quant,0,0);

                SaveResidual[trasf_mode](TempBlock16s[trasf_mode],8*sizeof(int16_t), pIn->pScanMatrix[trasf_mode], blk, Run, Level, nPairs[trasf_mode]);
                BlockRate[blk][trasf_mode] = CalculateBlockAC( pVST [trasf_mode],Run[blk],Level[blk],nPairs[trasf_mode][blk],pACTablesSetInter);                

                InterInvTransformQuantACFunction(TempBlock16s[trasf_mode],8*sizeof(int16_t),TempBlock16s[trasf_mode],8*sizeof(int16_t),pIn->quant ,pVST [trasf_mode]);
                ippiMC8x8_8u_C1(refBlockPtr[blk],refBlockStep[blk],TempBlock16s[trasf_mode],8*sizeof(int16_t),TempBlock8u,8*sizeof(uint8_t),0,0);                

                ippiSSD8x8_8u32s_C1R(TempBlock8u,8*sizeof(uint8_t),srcBlockPtr[blk],srcBlockStep[blk],&BlockCost[blk][trasf_mode],0);
            }
        }
        unsigned long long BestCost      = BIG_COST;
        unsigned long long CurCost       = 0;
        bool   bOneMBTSType  = true;
        uint32_t Mode          = 0;


        //------------------- One transform type on  MB (modes: 8x8       ) -----------------------------------------------
        
        {
            uint32_t trasf_mode = 0;
            uint32_t CbpPattern = 0;
            for (uint32_t blk = 0; blk < 6; blk++)
            {
                CurCost += (RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode]; 
                CbpPattern |= ((BlockRate[blk][trasf_mode])?1:0) << (5-blk);
            }
            
            CurCost += RDLambda[0]*pTTMBVLC[1][trasf_mode][1];
            CurCost += RDLambda[0]* (pIn->bField)?pCBPCYTableF[2*(CbpPattern&0x3F)+1]: pCBPCYTable[2*(CbpPattern&0x3F)+1];
            
            if (CurCost < BestCost)
            {
                BestCost   = CurCost; 
                Mode       = trasf_mode;                
            }
        }      
         //------------------- One transform type on  MB (modes: 8x4, 4x8 ) -----------------------------------------------

        for (uint32_t trasf_mode = 1; trasf_mode < 3; trasf_mode ++)
        {
            bool    bMBSubBlockPattern  = true;
            uint32_t  CbpPattern          = 0;
            
            CurCost             = 0;

            for (uint32_t blk = 0; blk < 6; blk++)
            {
                
                CurCost += (RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode];
                uint8_t  subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<1) + ((nPairs[trasf_mode][blk][1]>0)?1:0);
                if (subPat ==0 ) continue;

                CbpPattern |= (1) << (5-blk);

                if (bMBSubBlockPattern)
                {
                    CurCost += RDLambda[blk]*pTTMBVLC[1][trasf_mode][((subPat - 1)<<1)+1];
                    bMBSubBlockPattern = false;
                }
                else
                {
                    CurCost += RDLambda[blk]*(((nPairs[trasf_mode][blk][0] + nPairs[trasf_mode][blk][1])==1)? 2:1);
                }
            }
            CurCost += RDLambda[0]*(pIn->bField)?pCBPCYTableF[2*(CbpPattern&0x3F)+1]:pCBPCYTable[2*(CbpPattern&0x3F)+1];

            if (CurCost < BestCost )
            {
                BestCost   = CurCost; 
                Mode       = trasf_mode;                
            }
        }
        //-------------------      One transform type on  MB (modes: 4x4 )      --------------------------
        {
            uint32_t  trasf_mode    = 3;   
            uint32_t  CbpPattern    = 0;
           
            CurCost       = 0;
            for (uint32_t blk = 0; blk < 6; blk++)
            {
                CurCost += (uint32_t)(RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode];
                uint8_t subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<3) + (((nPairs[trasf_mode][blk][1]>0)?1:0)<<2)
                              + (((nPairs[trasf_mode][blk][2]>0)?1:0)<<1) +  ((nPairs[trasf_mode][blk][3]>0)?1:0);
                if (subPat ==0 ) continue;
                CbpPattern |= (1) << (5-blk);
                CurCost += RDLambda[blk]*pSubPattern4x4VLC[(subPat<<1)+1];
                
            }
            CurCost += RDLambda[0]*pTTMBVLC[1][trasf_mode][1];
            CurCost += RDLambda[0]*(pIn->bField)?pCBPCYTableF[2*(CbpPattern&0x3F)+1]:pCBPCYTable[2*(CbpPattern&0x3F)+1];

            if (CurCost < BestCost )
            {
                BestCost   = CurCost; 
                Mode       = trasf_mode;                
            }
        }
        //------------------- Different transform types on  MB ----------------------------------------
        
        for (uint32_t mode = 0; mode < 4096; mode ++)
        {
            
            bool bMBSubBlockPattern = true;
            CurCost = 0;
            uint32_t  CbpPattern    = 0;

            for (uint32_t blk = 0; blk < 6; blk++)
            {
                uint32_t trasf_mode = ((mode)>>(blk<<1)) & 0x03;
                uint8_t  subPat = 0;
                
                CurCost += (uint32_t)(RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode];
                
                if (bMBSubBlockPattern)
                {
                    switch (trasf_mode)
                    { 
                    case 0:
                        CurCost += RDLambda[blk]*pTTMBVLC[0][trasf_mode][1];
                        CbpPattern |= ((BlockRate[blk][trasf_mode])?1:0) << (5-blk);
                        break;
                    case 1:
                    case 2:
                        subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<1) + ((nPairs[trasf_mode][blk][1]>0)?1:0);
                        if (subPat == 0) 
                        {
                            continue;
                        }
                        CbpPattern |= (1) << (5-blk);
                        CurCost += RDLambda[blk]*pTTMBVLC[0][trasf_mode][((subPat - 1)<<1)+1];
                        break;
                    case 3:
                        subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<3) + (((nPairs[trasf_mode][blk][1]>0)?1:0)<<2)
                                + (((nPairs[trasf_mode][blk][2]>0)?1:0)<<1) +  ((nPairs[trasf_mode][blk][3]>0)?1:0);
                        if (subPat == 0) 
                        {
                            continue;
                        }
                        CbpPattern |= (1) << (5-blk);
                        CurCost += RDLambda[blk]*pTTMBVLC[0][trasf_mode][1];
                        CurCost += RDLambda[blk]*pSubPattern4x4VLC[(subPat<<1)+1]; 
                        break;
                    }
                    bMBSubBlockPattern = false;
                }
                else
                {
                    switch (trasf_mode)
                    { 
                    case 0:
                        CurCost += RDLambda[blk]*pTTMBVLC[0][trasf_mode][1];
                        CbpPattern |= ((BlockRate[blk][trasf_mode])?1:0) << (5-blk);
                        break;
                    case 1:
                    case 2:
                        subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<1) + ((nPairs[trasf_mode][blk][1]>0)?1:0);
                        if (subPat == 0) 
                        {
                            continue;
                        }
                        CbpPattern |= (1) << (5-blk);
                        CurCost += RDLambda[blk]*pTTBlkVLC[trasf_mode][((subPat - 1)<<1)+1];
                        break;
                    case 3:
                        subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<3) + (((nPairs[trasf_mode][blk][1]>0)?1:0)<<2)
                                + (((nPairs[trasf_mode][blk][2]>0)?1:0)<<1) +  ((nPairs[trasf_mode][blk][3]>0)?1:0);
                        if (subPat == 0) 
                        { 
                            continue;
                        }
                        CbpPattern |= (1) << (5-blk);
                        CurCost += RDLambda[blk]*pTTBlkVLC[trasf_mode][1];
                        CurCost += RDLambda[blk]*pSubPattern4x4VLC[(subPat<<1)+1]; 
                        break;
                    }
                }
            }
            CurCost += RDLambda[0]*(pIn->bField)?pCBPCYTableF[2*(CbpPattern&0x3F)+1]:pCBPCYTable[2*(CbpPattern&0x3F)+1];

            if (CurCost < BestCost)
            {
                BestCost = CurCost; 
                Mode     = mode; 
                bOneMBTSType  = false;
            }
        }
        //----------------------------------------------------------------------------------------------
        //printf("mode: %d, %d, %d, %d, %d, %d\t",pVSTTypeOut[0], pVSTTypeOut[1], pVSTTypeOut[2], pVSTTypeOut[3], pVSTTypeOut[4],pVSTTypeOut[5]);

        if (bOneMBTSType)
        {
            for (uint32_t blk = 0; blk < 6; blk++)
            {
                pVSTTypeOut[blk] = pVST[Mode];
                
            }
        }
        else
        {
            for (uint32_t blk = 0; blk < 6; blk++)
            {
                pVSTTypeOut[blk] = pVST[((Mode)>>(blk<<1)) & 0x03];
            }
        }
        //printf("mode: %d, %d, %d, %d, %d, %d\n",pVSTTypeOut[0], pVSTTypeOut[1], pVSTTypeOut[2], pVSTTypeOut[3], pVSTTypeOut[4],pVSTTypeOut[5]);


        return UMC::UMC_OK;
    
    }
    UMC::Status GetVSTTypeB_RD (VC1EncMD_B* pIn, eTransformType* pVSTTypeOut) 
    {
        int16_t TempBlock16s [4][64];
        uint8_t  TempBlock8u  [64];

        unsigned long long BlockRate[6][4] = {0};
        int32_t BlockCost[6][4] = {0};

        uint8_t  Run[6][65]={0};
        int16_t Level[6][64]={0};
        uint8_t  nPairs[4][6][4]={0};

        uint32_t RDLambda0 = (pIn->bUniform)? 10*(pIn->quant>>1)*(pIn->quant>>1):35*(pIn->quant>>1)*(pIn->quant>>1);
        uint32_t RDLambda1 = (pIn->bUniform)? 10*(pIn->quant)*(pIn->quant):35*(pIn->quant)*(pIn->quant);
        uint32_t RDLambda[6] = {RDLambda0,RDLambda0,RDLambda0,RDLambda0,RDLambda1,RDLambda1};

        if(RDLambda0<0 ||RDLambda1<0) 
            return UMC::UMC_OK;

        static const eTransformType pVST [4] = {  VC1_ENC_8x8_TRANSFORM,VC1_ENC_8x4_TRANSFORM, VC1_ENC_4x8_TRANSFORM, VC1_ENC_4x4_TRANSFORM };
        pSaveResidual SaveResidual[4] = {SaveResidual8x8Inter, SaveResidual8x4Inter, SaveResidual4x8Inter, SaveResidual4x4Inter};


        uint8_t* srcBlockPtr [6] = {  pIn->pYSrc, 
            pIn->pYSrc + 8, 
            pIn->pYSrc + (pIn->srcYStep<<3), 
            pIn->pYSrc + (pIn->srcYStep<<3) + 8,
            pIn->pUSrc,
            pIn->pVSrc};
        uint8_t* refBlockPtr [2][6] = {  {pIn->pYRef[0], pIn->pYRef[0] + 8, pIn->pYRef[0] + (pIn->refYStep[0]<<3), pIn->pYRef[0] + (pIn->refYStep[0]<<3) + 8,
                                        pIn->pURef[0], pIn->pVRef[0]},
                                       {pIn->pYRef[1], pIn->pYRef[1] + 8, pIn->pYRef[1] + (pIn->refYStep[1]<<3), pIn->pYRef[1] + (pIn->refYStep[1]<<3) + 8,
                                        pIn->pURef[1], pIn->pVRef[1]}};
        uint32_t srcBlockStep [6] = { pIn->srcYStep,
            pIn->srcYStep,
            pIn->srcYStep,
            pIn->srcYStep,
            pIn->srcUStep,
            pIn->srcVStep};

        uint32_t refBlockStep [2][6] = {  {pIn->refYStep[0], pIn->refYStep[0], pIn->refYStep[0], pIn->refYStep[0], pIn->refUStep[0], pIn->refVStep[0]},
                                        {pIn->refYStep[1], pIn->refYStep[1], pIn->refYStep[1], pIn->refYStep[1], pIn->refUStep[1], pIn->refVStep[1]}};

        InterTransformQuantFunction InterTransformQuantACFunction = (pIn->bUniform) ? InterTransformQuantUniform :InterTransformQuantNonUniform;
        InterInvTransformQuantFunction InterInvTransformQuantACFunction = (pIn->bUniform) ? InterInvTransformQuantUniform : InterInvTransformQuantNonUniform;

        const int16_t (*pTTMBVLC)[4][6]   = 0;
        const uint8_t  (* pTTBlkVLC)[6]    = 0;
        const uint8_t   *pSubPattern4x4VLC = 0;
        uint32_t quant = pIn->quant>>1;

        eCodingSet            CodingSetInter   = CodingSetsInter [quant>8][pIn->DecTypeAC1];
        const sACTablesSet*   pACTablesSetInter = &(ACTablesSet[CodingSetInter]);

        if (quant<5)
        {
            pTTMBVLC            =  TTMBVLC_HighRate;
            pTTBlkVLC           =  TTBLKVLC_HighRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
        }
        else if (quant<13)
        {
            pTTMBVLC            =  TTMBVLC_MediumRate;
            pTTBlkVLC           =  TTBLKVLC_MediumRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

        }
        else
        {
            pTTMBVLC            =  TTMBVLC_LowRate;
            pTTBlkVLC           =  TTBLKVLC_LowRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
        }   
        const uint16_t*   pCBPCYTable  = VLCTableCBPCY_PB[pIn->CBPTab];
        const uint32_t*   pCBPCYTableF = CBPCYFieldTable_VLC[pIn->CBPTab];

        for (uint32_t blk = 0; blk < 6; blk++)
        {
            if ((pIn->intraPattern & (1<<VC_ENC_PATTERN_POS(blk)))!=0)
                continue;

            ippiGetDiff8x8B_8u16s_C1(srcBlockPtr[blk],srcBlockStep[blk],
                                    refBlockPtr[0][blk],refBlockStep[0][blk],0,
                                    refBlockPtr[1][blk],refBlockStep[1][blk],0,
                                    TempBlock16s[0],8*sizeof(int16_t),0);                 
            
            
            memcpy (TempBlock16s[1],TempBlock16s[0],64*sizeof(int16_t));
            memcpy (TempBlock16s[2],TempBlock16s[0],64*sizeof(int16_t));
            memcpy (TempBlock16s[3],TempBlock16s[0],64*sizeof(int16_t));

            for (uint32_t trasf_mode = 0; trasf_mode < 4; trasf_mode ++)
            {
                InterTransformQuantACFunction (TempBlock16s[trasf_mode],8*sizeof(int16_t),pVST [trasf_mode], pIn->quant,0,0);

                SaveResidual[trasf_mode](TempBlock16s[trasf_mode],8*sizeof(int16_t), pIn->pScanMatrix[trasf_mode], blk, Run, Level, nPairs[trasf_mode]);
                BlockRate[blk][trasf_mode] = CalculateBlockAC( pVST [trasf_mode],Run[blk],Level[blk],nPairs[trasf_mode][blk],pACTablesSetInter);                

                InterInvTransformQuantACFunction(TempBlock16s[trasf_mode],8*sizeof(int16_t),TempBlock16s[trasf_mode],8*sizeof(int16_t),pIn->quant ,pVST [trasf_mode]);
                ippiMC8x8B_8u_C1(refBlockPtr[0][blk],refBlockStep[0][blk],0,
                                 refBlockPtr[1][blk],refBlockStep[1][blk],0,
                                 TempBlock16s[trasf_mode],8*sizeof(int16_t),
                                 TempBlock8u,8*sizeof(uint8_t),0);                

                ippiSSD8x8_8u32s_C1R(TempBlock8u,8*sizeof(uint8_t),srcBlockPtr[blk],srcBlockStep[blk],&BlockCost[blk][trasf_mode],0);
            }
        }
        unsigned long long BestCost      = BIG_COST;
        unsigned long long CurCost       = 0;
        bool   bOneMBTSType  = true;
        uint32_t Mode          = 0;


        //------------------- One transform type on  MB (modes: 8x8       ) -----------------------------------------------

        {
            uint32_t trasf_mode = 0;
            uint32_t CbpPattern = 0;
            for (uint32_t blk = 0; blk < 6; blk++)
            {
                CurCost += (RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode]; 
                CbpPattern |= ((BlockRate[blk][trasf_mode])?1:0) << (5-blk);
            }

            CurCost += RDLambda[0]*pTTMBVLC[1][trasf_mode][1];
            CurCost += RDLambda[0]* (pIn->bField)?pCBPCYTableF[2*(CbpPattern&0x3F)+1]: pCBPCYTable[2*(CbpPattern&0x3F)+1];

            if (CurCost < BestCost)
            {
                BestCost   = CurCost; 
                Mode       = trasf_mode;                
            }
        }      
        //------------------- One transform type on  MB (modes: 8x4, 4x8 ) -----------------------------------------------

        for (uint32_t trasf_mode = 1; trasf_mode < 3; trasf_mode ++)
        {
            bool    bMBSubBlockPattern  = true;
            uint32_t  CbpPattern          = 0;

            CurCost             = 0;

            for (uint32_t blk = 0; blk < 6; blk++)
            {

                CurCost += (RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode];
                uint8_t  subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<1) + ((nPairs[trasf_mode][blk][1]>0)?1:0);
                if (subPat ==0 ) continue;

                CbpPattern |= (1) << (5-blk);

                if (bMBSubBlockPattern)
                {
                    CurCost += RDLambda[blk]*pTTMBVLC[1][trasf_mode][((subPat - 1)<<1)+1];
                    bMBSubBlockPattern = false;
                }
                else
                {
                    CurCost += RDLambda[blk]*(((nPairs[trasf_mode][blk][0] + nPairs[trasf_mode][blk][1])==1)? 2:1);
                }
            }
            CurCost += RDLambda[0]*(pIn->bField)?pCBPCYTableF[2*(CbpPattern&0x3F)+1]:pCBPCYTable[2*(CbpPattern&0x3F)+1];

            if (CurCost < BestCost )
            {
                BestCost   = CurCost; 
                Mode       = trasf_mode;                
            }
        }
        //-------------------      One transform type on  MB (modes: 4x4 )      --------------------------
        {
            uint32_t  trasf_mode    = 3;   
            uint32_t  CbpPattern    = 0;

            CurCost       = 0;
            for (uint32_t blk = 0; blk < 6; blk++)
            {
                CurCost += (uint32_t)(RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode];
                uint8_t subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<3) + (((nPairs[trasf_mode][blk][1]>0)?1:0)<<2)
                    + (((nPairs[trasf_mode][blk][2]>0)?1:0)<<1) +  ((nPairs[trasf_mode][blk][3]>0)?1:0);
                if (subPat ==0 ) continue;
                CbpPattern |= (1) << (5-blk);
                CurCost += RDLambda[blk]*pSubPattern4x4VLC[(subPat<<1)+1];

            }
            CurCost += RDLambda[0]*pTTMBVLC[1][trasf_mode][1];
            CurCost += RDLambda[0]*(pIn->bField)?pCBPCYTableF[2*(CbpPattern&0x3F)+1]:pCBPCYTable[2*(CbpPattern&0x3F)+1];

            if (CurCost < BestCost )
            {
                BestCost   = CurCost; 
                Mode       = trasf_mode;                
            }
        }
        //------------------- Different transform types on  MB ----------------------------------------

        for (uint32_t mode = 0; mode < 4096; mode ++)
        {

            bool bMBSubBlockPattern = true;
            CurCost = 0;
            uint32_t  CbpPattern    = 0;

            for (uint32_t blk = 0; blk < 6; blk++)
            {
                uint32_t trasf_mode = ((mode)>>(blk<<1)) & 0x03;
                uint8_t  subPat = 0;

                CurCost += (uint32_t)(RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode];

                if (bMBSubBlockPattern)
                {
                    switch (trasf_mode)
                    { 
                    case 0:
                        CurCost += RDLambda[blk]*pTTMBVLC[0][trasf_mode][1];
                        CbpPattern |= ((BlockRate[blk][trasf_mode])?1:0) << (5-blk);
                        break;
                    case 1:
                    case 2:
                        subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<1) + ((nPairs[trasf_mode][blk][1]>0)?1:0);
                        if (subPat == 0) 
                        {
                            continue;
                        }
                        CbpPattern |= (1) << (5-blk);
                        CurCost += RDLambda[blk]*pTTMBVLC[0][trasf_mode][((subPat - 1)<<1)+1];
                        break;
                    case 3:
                        subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<3) + (((nPairs[trasf_mode][blk][1]>0)?1:0)<<2)
                            + (((nPairs[trasf_mode][blk][2]>0)?1:0)<<1) +  ((nPairs[trasf_mode][blk][3]>0)?1:0);
                        if (subPat == 0) 
                        {
                            continue;
                        }
                        CbpPattern |= (1) << (5-blk);
                        CurCost += RDLambda[blk]*pTTMBVLC[0][trasf_mode][1];
                        CurCost += RDLambda[blk]*pSubPattern4x4VLC[(subPat<<1)+1]; 
                        break;
                    }
                    bMBSubBlockPattern = false;
                }
                else
                {
                    switch (trasf_mode)
                    { 
                    case 0:
                        CurCost += RDLambda[blk]*pTTMBVLC[0][trasf_mode][1];
                        CbpPattern |= ((BlockRate[blk][trasf_mode])?1:0) << (5-blk);
                        break;
                    case 1:
                    case 2:
                        subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<1) + ((nPairs[trasf_mode][blk][1]>0)?1:0);
                        if (subPat == 0) 
                        {
                            continue;
                        }
                        CbpPattern |= (1) << (5-blk);
                        CurCost += RDLambda[blk]*pTTBlkVLC[trasf_mode][((subPat - 1)<<1)+1];
                        break;
                    case 3:
                        subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<3) + (((nPairs[trasf_mode][blk][1]>0)?1:0)<<2)
                            + (((nPairs[trasf_mode][blk][2]>0)?1:0)<<1) +  ((nPairs[trasf_mode][blk][3]>0)?1:0);
                        if (subPat == 0) 
                        { 
                            continue;
                        }
                        CbpPattern |= (1) << (5-blk);
                        CurCost += RDLambda[blk]*pTTBlkVLC[trasf_mode][1];
                        CurCost += RDLambda[blk]*pSubPattern4x4VLC[(subPat<<1)+1]; 
                        break;
                    }
                }
            }
            CurCost += RDLambda[0]*(pIn->bField)?pCBPCYTableF[2*(CbpPattern&0x3F)+1]:pCBPCYTable[2*(CbpPattern&0x3F)+1];

            if (CurCost < BestCost)
            {
                BestCost = CurCost; 
                Mode     = mode; 
                bOneMBTSType  = false;
            }
        }
        //----------------------------------------------------------------------------------------------
        //printf("mode: %d, %d, %d, %d, %d, %d\t",pVSTTypeOut[0], pVSTTypeOut[1], pVSTTypeOut[2], pVSTTypeOut[3], pVSTTypeOut[4],pVSTTypeOut[5]);

        if (bOneMBTSType)
        {
            for (uint32_t blk = 0; blk < 6; blk++)
            {
                pVSTTypeOut[blk] = pVST[Mode];

            }
        }
        else
        {
            for (uint32_t blk = 0; blk < 6; blk++)
            {
                pVSTTypeOut[blk] = pVST[((Mode)>>(blk<<1)) & 0x03];
            }
        }
        //printf("mode: %d, %d, %d, %d, %d, %d\n",pVSTTypeOut[0], pVSTTypeOut[1], pVSTTypeOut[2], pVSTTypeOut[3], pVSTTypeOut[4],pVSTTypeOut[5]);


        return UMC::UMC_OK;

    }

#undef BIG_COST
}
#endif