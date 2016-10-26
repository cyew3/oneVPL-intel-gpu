//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#include "umc_vc1_enc_mode_decision.h"
#include "umc_vc1_enc_common.h"
#include "umc_vc1_enc_tables.h"

namespace UMC_VC1_ENCODER
{

    void SaveResidual8x8Inter (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]);
    void SaveResidual8x4Inter (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]);
    void SaveResidual4x8Inter (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]);
    void SaveResidual4x4Inter (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]);

    typedef void (*pSaveResidual) (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk, Ipp8u uRunArr[6][65], Ipp16s iLevelArr[6][64], Ipp8u nPairsArr[6][4]);
    
    static Ipp32u     CalculateBlockAC(         eTransformType      tsType,
                                                Ipp8u               *pRun,
                                                Ipp16s              *pLevel,
                                                Ipp8u               *pnPairs,
                                                const sACTablesSet  *pACTablesSet)
    {
        Ipp32s                  i                      = 0;
        static const Ipp32s     nSubblk [4]     = {1,2,2,4};

        const Ipp32u            *pEncTable   = pACTablesSet->pEncTable;
        Ipp8u                   nPairsBlock  = 0;
        Ipp32u                  BlockLen     = 0;     


 
        for (Ipp32s nSubblock=0; nSubblock<nSubblk[tsType]; nSubblock++)
        {
            const Ipp8u     *pTableDR    = pACTablesSet->pTableDR;
            const Ipp8u     *pTableDL    = pACTablesSet->pTableDL;
            const Ipp8u     *pTableInd   = pACTablesSet->pTableInd;
            Ipp8u           nPairs       = pnPairs[nSubblock];

            if (nPairs == 0)
                continue;

            // put codes into bitstream
            i = 0;
            for (Ipp32s not_last = 1; not_last>=0; not_last--)
            {
                for (; i < nPairs - not_last; i++)
                {
                    bool    sign = false;
                    Ipp8u   run  = pRun [i+nPairsBlock];
                    Ipp16s  lev  = pLevel[i+nPairsBlock];

                    Ipp8u mode = GetMode( run, lev, pTableDR, pTableDL, sign);

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
                            Ipp16s index = pTableInd[run] + lev;
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
        Ipp16s TempBlock16s [4][64];
        Ipp8u  TempBlock8u  [64];

        Ipp64u BlockRate[6][4] = {0};
        Ipp32s BlockCost[6][4] = {0};

        Ipp8u  Run[6][65]={0};
        Ipp16s Level[6][64]={0};
        Ipp8u  nPairs[4][6][4]={0};

        Ipp32u RDLambda0 = (pIn->bUniform)? 10*(pIn->quant>>1)*(pIn->quant>>1):35*(pIn->quant>>1)*(pIn->quant>>1);
        Ipp32u RDLambda1 = (pIn->bUniform)? 10*(pIn->quant)*(pIn->quant):35*(pIn->quant)*(pIn->quant);
        Ipp32u RDLambda[6] = {RDLambda0,RDLambda0,RDLambda0,RDLambda0,RDLambda1,RDLambda1};
        
        if(RDLambda0<0 ||RDLambda1<0) 
            return UMC::UMC_OK;
        
        static const eTransformType pVST [4] = {  VC1_ENC_8x8_TRANSFORM,VC1_ENC_8x4_TRANSFORM, VC1_ENC_4x8_TRANSFORM, VC1_ENC_4x4_TRANSFORM };
        pSaveResidual SaveResidual[4] = {SaveResidual8x8Inter, SaveResidual8x4Inter, SaveResidual4x8Inter, SaveResidual4x4Inter};


        Ipp8u* srcBlockPtr [6] = {  pIn->pYSrc, 
                                    pIn->pYSrc + 8, 
                                    pIn->pYSrc + (pIn->srcYStep<<3), 
                                    pIn->pYSrc + (pIn->srcYStep<<3) + 8,
                                    pIn->pUSrc,
                                    pIn->pVSrc};
        Ipp8u* refBlockPtr [6] = {  pIn->pYRef, 
                                    pIn->pYRef + 8, 
                                    pIn->pYRef + (pIn->refYStep<<3), 
                                    pIn->pYRef + (pIn->refYStep<<3) + 8,
                                    pIn->pURef,
                                    pIn->pVRef};
        Ipp32u srcBlockStep [6] = { pIn->srcYStep,
                                    pIn->srcYStep,
                                    pIn->srcYStep,
                                    pIn->srcYStep,
                                    pIn->srcUStep,
                                    pIn->srcVStep};
        Ipp32u refBlockStep [6] = { pIn->refYStep,
                                    pIn->refYStep,
                                    pIn->refYStep,
                                    pIn->refYStep,
                                    pIn->refUStep,
                                    pIn->refVStep};

        InterTransformQuantFunction InterTransformQuantACFunction = (pIn->bUniform) ? InterTransformQuantUniform :InterTransformQuantNonUniform;
        InterInvTransformQuantFunction InterInvTransformQuantACFunction = (pIn->bUniform) ? InterInvTransformQuantUniform : InterInvTransformQuantNonUniform;
        
        const Ipp16s (*pTTMBVLC)[4][6]   = 0;
        const Ipp8u  (* pTTBlkVLC)[6]    = 0;
        const Ipp8u   *pSubPattern4x4VLC = 0;
        Ipp32u quant = pIn->quant>>1;

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
        const Ipp16u*   pCBPCYTable  = VLCTableCBPCY_PB[pIn->CBPTab];
        const Ipp32u*   pCBPCYTableF = CBPCYFieldTable_VLC[pIn->CBPTab];

        for (Ipp32u blk = 0; blk < 6; blk++)
        {
            if ((pIn->intraPattern & (1<<VC_ENC_PATTERN_POS(blk)))!=0)
                continue;
            
            ippiSub8x8_8u16s_C1R(srcBlockPtr[blk],srcBlockStep[blk],refBlockPtr[blk],refBlockStep[blk],TempBlock16s[0],8*sizeof(Ipp16s));

            memcpy (TempBlock16s[1],TempBlock16s[0],64*sizeof(Ipp16s));
            memcpy (TempBlock16s[2],TempBlock16s[0],64*sizeof(Ipp16s));
            memcpy (TempBlock16s[3],TempBlock16s[0],64*sizeof(Ipp16s));
            
            for (Ipp32u trasf_mode = 0; trasf_mode < 4; trasf_mode ++)
            {
                InterTransformQuantACFunction (TempBlock16s[trasf_mode],8*sizeof(Ipp16s),pVST [trasf_mode], pIn->quant,0,0);

                SaveResidual[trasf_mode](TempBlock16s[trasf_mode],8*sizeof(Ipp16s), pIn->pScanMatrix[trasf_mode], blk, Run, Level, nPairs[trasf_mode]);
                BlockRate[blk][trasf_mode] = CalculateBlockAC( pVST [trasf_mode],Run[blk],Level[blk],nPairs[trasf_mode][blk],pACTablesSetInter);                

                InterInvTransformQuantACFunction(TempBlock16s[trasf_mode],8*sizeof(Ipp16s),TempBlock16s[trasf_mode],8*sizeof(Ipp16s),pIn->quant ,pVST [trasf_mode]);
                ippiMC8x8_8u_C1(refBlockPtr[blk],refBlockStep[blk],TempBlock16s[trasf_mode],8*sizeof(Ipp16s),TempBlock8u,8*sizeof(Ipp8u),0,0);                

                ippiSSD8x8_8u32s_C1R(TempBlock8u,8*sizeof(Ipp8u),srcBlockPtr[blk],srcBlockStep[blk],&BlockCost[blk][trasf_mode],0);
            }
        }
        Ipp64u BestCost      = BIG_COST;
        Ipp64u CurCost       = 0;
        bool   bOneMBTSType  = true;
        Ipp32u Mode          = 0;


        //------------------- One transform type on  MB (modes: 8x8       ) -----------------------------------------------
        
        {
            Ipp32u trasf_mode = 0;
            Ipp32u CbpPattern = 0;
            for (Ipp32u blk = 0; blk < 6; blk++)
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

        for (Ipp32u trasf_mode = 1; trasf_mode < 3; trasf_mode ++)
        {
            bool    bMBSubBlockPattern  = true;
            Ipp32u  CbpPattern          = 0;
            
            CurCost             = 0;

            for (Ipp32u blk = 0; blk < 6; blk++)
            {
                
                CurCost += (RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode];
                Ipp8u  subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<1) + ((nPairs[trasf_mode][blk][1]>0)?1:0);
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
            Ipp32u  trasf_mode    = 3;   
            Ipp32u  CbpPattern    = 0;
           
            CurCost       = 0;
            for (Ipp32u blk = 0; blk < 6; blk++)
            {
                CurCost += (Ipp32u)(RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode];
                Ipp8u subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<3) + (((nPairs[trasf_mode][blk][1]>0)?1:0)<<2)
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
        
        for (Ipp32u mode = 0; mode < 4096; mode ++)
        {
            
            bool bMBSubBlockPattern = true;
            CurCost = 0;
            Ipp32u  CbpPattern    = 0;

            for (Ipp32u blk = 0; blk < 6; blk++)
            {
                Ipp32u trasf_mode = ((mode)>>(blk<<1)) & 0x03;
                Ipp8u  subPat = 0;
                
                CurCost += (Ipp32u)(RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode];
                
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
            for (Ipp32u blk = 0; blk < 6; blk++)
            {
                pVSTTypeOut[blk] = pVST[Mode];
                
            }
        }
        else
        {
            for (Ipp32u blk = 0; blk < 6; blk++)
            {
                pVSTTypeOut[blk] = pVST[((Mode)>>(blk<<1)) & 0x03];
            }
        }
        //printf("mode: %d, %d, %d, %d, %d, %d\n",pVSTTypeOut[0], pVSTTypeOut[1], pVSTTypeOut[2], pVSTTypeOut[3], pVSTTypeOut[4],pVSTTypeOut[5]);


        return UMC::UMC_OK;
    
    }
    UMC::Status GetVSTTypeB_RD (VC1EncMD_B* pIn, eTransformType* pVSTTypeOut) 
    {
        Ipp16s TempBlock16s [4][64];
        Ipp8u  TempBlock8u  [64];

        Ipp64u BlockRate[6][4] = {0};
        Ipp32s BlockCost[6][4] = {0};

        Ipp8u  Run[6][65]={0};
        Ipp16s Level[6][64]={0};
        Ipp8u  nPairs[4][6][4]={0};

        Ipp32u RDLambda0 = (pIn->bUniform)? 10*(pIn->quant>>1)*(pIn->quant>>1):35*(pIn->quant>>1)*(pIn->quant>>1);
        Ipp32u RDLambda1 = (pIn->bUniform)? 10*(pIn->quant)*(pIn->quant):35*(pIn->quant)*(pIn->quant);
        Ipp32u RDLambda[6] = {RDLambda0,RDLambda0,RDLambda0,RDLambda0,RDLambda1,RDLambda1};

        if(RDLambda0<0 ||RDLambda1<0) 
            return UMC::UMC_OK;

        static const eTransformType pVST [4] = {  VC1_ENC_8x8_TRANSFORM,VC1_ENC_8x4_TRANSFORM, VC1_ENC_4x8_TRANSFORM, VC1_ENC_4x4_TRANSFORM };
        pSaveResidual SaveResidual[4] = {SaveResidual8x8Inter, SaveResidual8x4Inter, SaveResidual4x8Inter, SaveResidual4x4Inter};


        Ipp8u* srcBlockPtr [6] = {  pIn->pYSrc, 
            pIn->pYSrc + 8, 
            pIn->pYSrc + (pIn->srcYStep<<3), 
            pIn->pYSrc + (pIn->srcYStep<<3) + 8,
            pIn->pUSrc,
            pIn->pVSrc};
        Ipp8u* refBlockPtr [2][6] = {  {pIn->pYRef[0], pIn->pYRef[0] + 8, pIn->pYRef[0] + (pIn->refYStep[0]<<3), pIn->pYRef[0] + (pIn->refYStep[0]<<3) + 8,
                                        pIn->pURef[0], pIn->pVRef[0]},
                                       {pIn->pYRef[1], pIn->pYRef[1] + 8, pIn->pYRef[1] + (pIn->refYStep[1]<<3), pIn->pYRef[1] + (pIn->refYStep[1]<<3) + 8,
                                        pIn->pURef[1], pIn->pVRef[1]}};
        Ipp32u srcBlockStep [6] = { pIn->srcYStep,
            pIn->srcYStep,
            pIn->srcYStep,
            pIn->srcYStep,
            pIn->srcUStep,
            pIn->srcVStep};

        Ipp32u refBlockStep [2][6] = {  {pIn->refYStep[0], pIn->refYStep[0], pIn->refYStep[0], pIn->refYStep[0], pIn->refUStep[0], pIn->refVStep[0]},
                                        {pIn->refYStep[1], pIn->refYStep[1], pIn->refYStep[1], pIn->refYStep[1], pIn->refUStep[1], pIn->refVStep[1]}};

        InterTransformQuantFunction InterTransformQuantACFunction = (pIn->bUniform) ? InterTransformQuantUniform :InterTransformQuantNonUniform;
        InterInvTransformQuantFunction InterInvTransformQuantACFunction = (pIn->bUniform) ? InterInvTransformQuantUniform : InterInvTransformQuantNonUniform;

        const Ipp16s (*pTTMBVLC)[4][6]   = 0;
        const Ipp8u  (* pTTBlkVLC)[6]    = 0;
        const Ipp8u   *pSubPattern4x4VLC = 0;
        Ipp32u quant = pIn->quant>>1;

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
        const Ipp16u*   pCBPCYTable  = VLCTableCBPCY_PB[pIn->CBPTab];
        const Ipp32u*   pCBPCYTableF = CBPCYFieldTable_VLC[pIn->CBPTab];

        for (Ipp32u blk = 0; blk < 6; blk++)
        {
            if ((pIn->intraPattern & (1<<VC_ENC_PATTERN_POS(blk)))!=0)
                continue;

            ippiGetDiff8x8B_8u16s_C1(srcBlockPtr[blk],srcBlockStep[blk],
                                    refBlockPtr[0][blk],refBlockStep[0][blk],0,
                                    refBlockPtr[1][blk],refBlockStep[1][blk],0,
                                    TempBlock16s[0],8*sizeof(Ipp16s),0);                 
            
            
            memcpy (TempBlock16s[1],TempBlock16s[0],64*sizeof(Ipp16s));
            memcpy (TempBlock16s[2],TempBlock16s[0],64*sizeof(Ipp16s));
            memcpy (TempBlock16s[3],TempBlock16s[0],64*sizeof(Ipp16s));

            for (Ipp32u trasf_mode = 0; trasf_mode < 4; trasf_mode ++)
            {
                InterTransformQuantACFunction (TempBlock16s[trasf_mode],8*sizeof(Ipp16s),pVST [trasf_mode], pIn->quant,0,0);

                SaveResidual[trasf_mode](TempBlock16s[trasf_mode],8*sizeof(Ipp16s), pIn->pScanMatrix[trasf_mode], blk, Run, Level, nPairs[trasf_mode]);
                BlockRate[blk][trasf_mode] = CalculateBlockAC( pVST [trasf_mode],Run[blk],Level[blk],nPairs[trasf_mode][blk],pACTablesSetInter);                

                InterInvTransformQuantACFunction(TempBlock16s[trasf_mode],8*sizeof(Ipp16s),TempBlock16s[trasf_mode],8*sizeof(Ipp16s),pIn->quant ,pVST [trasf_mode]);
                ippiMC8x8B_8u_C1(refBlockPtr[0][blk],refBlockStep[0][blk],0,
                                 refBlockPtr[1][blk],refBlockStep[1][blk],0,
                                 TempBlock16s[trasf_mode],8*sizeof(Ipp16s),
                                 TempBlock8u,8*sizeof(Ipp8u),0);                

                ippiSSD8x8_8u32s_C1R(TempBlock8u,8*sizeof(Ipp8u),srcBlockPtr[blk],srcBlockStep[blk],&BlockCost[blk][trasf_mode],0);
            }
        }
        Ipp64u BestCost      = BIG_COST;
        Ipp64u CurCost       = 0;
        bool   bOneMBTSType  = true;
        Ipp32u Mode          = 0;


        //------------------- One transform type on  MB (modes: 8x8       ) -----------------------------------------------

        {
            Ipp32u trasf_mode = 0;
            Ipp32u CbpPattern = 0;
            for (Ipp32u blk = 0; blk < 6; blk++)
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

        for (Ipp32u trasf_mode = 1; trasf_mode < 3; trasf_mode ++)
        {
            bool    bMBSubBlockPattern  = true;
            Ipp32u  CbpPattern          = 0;

            CurCost             = 0;

            for (Ipp32u blk = 0; blk < 6; blk++)
            {

                CurCost += (RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode];
                Ipp8u  subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<1) + ((nPairs[trasf_mode][blk][1]>0)?1:0);
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
            Ipp32u  trasf_mode    = 3;   
            Ipp32u  CbpPattern    = 0;

            CurCost       = 0;
            for (Ipp32u blk = 0; blk < 6; blk++)
            {
                CurCost += (Ipp32u)(RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode];
                Ipp8u subPat  = (((nPairs[trasf_mode][blk][0]>0)?1:0)<<3) + (((nPairs[trasf_mode][blk][1]>0)?1:0)<<2)
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

        for (Ipp32u mode = 0; mode < 4096; mode ++)
        {

            bool bMBSubBlockPattern = true;
            CurCost = 0;
            Ipp32u  CbpPattern    = 0;

            for (Ipp32u blk = 0; blk < 6; blk++)
            {
                Ipp32u trasf_mode = ((mode)>>(blk<<1)) & 0x03;
                Ipp8u  subPat = 0;

                CurCost += (Ipp32u)(RDLambda[blk]*BlockRate[blk][trasf_mode]) + BlockCost[blk][trasf_mode];

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
            for (Ipp32u blk = 0; blk < 6; blk++)
            {
                pVSTTypeOut[blk] = pVST[Mode];

            }
        }
        else
        {
            for (Ipp32u blk = 0; blk < 6; blk++)
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