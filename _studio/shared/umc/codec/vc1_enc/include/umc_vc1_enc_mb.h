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

#ifndef _ENCODER_VC1_MB_H_
#define _ENCODER_VC1_MB_H_

#include "ippvc.h"
#include "umc_structures.h"
#include "umc_vc1_enc_def.h"
#include "umc_vc1_enc_bitstream.h"
#include "umc_vc1_enc_debug.h"
#include "umc_vc1_enc_common.h"
#include "umc_vc1_enc_block_template.h"
#include "umc_memory_allocator.h"
#include "umc_vc1_enc_statistic.h"
#include "umc_vc1_enc_tables.h"

#ifdef VC1_ME_MB_STATICTICS
#include "umc_me.h"
#endif
namespace UMC_VC1_ENCODER
{

class VC1EncoderCodedMB
{
private:

    eMBType         m_MBtype;

    // inter MB
    sCoordinate     m_dMV[2][4];      //  backward,forward. up to 4 coded MV for 1 MB.
                                      //  field bSecond contains flag Opposite.
    uint8_t           m_pHybrid[4];

    eTransformType  m_tsType[6];
    bool            m_bMBTSType;      //transform type on MB level
    uint8_t           m_uiFirstCodedBlock;

    // intra
    int16_t          m_iDC[6];

    //all
    uint8_t           m_uRun   [6][65];
    int16_t          m_iLevel [6][64];
    uint8_t           m_nPairs [6][4]; // 6 blocks, 4 subblocks
    uint8_t           m_uiMBCBPCY;

    //for I MB
    int8_t           m_iACPrediction;

    // 4MV
    uint8_t           m_uiIntraPattern;
    bool            m_bOverFlag;

    #ifdef VC1_ME_MB_STATICTICS
        UMC::MeMbStat* m_MECurMbStat;
    #endif
public:
    VC1EncoderCodedMB()
    {
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat = NULL;
    #endif
    }
    UMC::Status Init(eMBType type)
    {
        m_MBtype                = type;
        m_uiMBCBPCY             = 0;
        m_uiFirstCodedBlock     = 0;
        m_iACPrediction         = 0;
        m_pHybrid[0]            = 0;
        m_pHybrid[1]            = 0;
        m_pHybrid[2]            = 0;
        m_pHybrid[3]            = 0;
        m_bMBTSType             = true;
        m_uiIntraPattern        = 0;
        m_bOverFlag             = false;

        memset(m_iDC,0,6*sizeof(int16_t));

        memset(m_nPairs[0],0,4*sizeof(uint8_t));
        memset(m_nPairs[1],0,4*sizeof(uint8_t));
        memset(m_nPairs[2],0,4*sizeof(uint8_t));
        memset(m_nPairs[3],0,4*sizeof(uint8_t));
        memset(m_nPairs[4],0,4*sizeof(uint8_t));
        memset(m_nPairs[5],0,4*sizeof(uint8_t));

        m_tsType[0] = VC1_ENC_8x8_TRANSFORM;
        m_tsType[1] = VC1_ENC_8x8_TRANSFORM;
        m_tsType[2] = VC1_ENC_8x8_TRANSFORM;
        m_tsType[3] = VC1_ENC_8x8_TRANSFORM;
        m_tsType[4] = VC1_ENC_8x8_TRANSFORM;
        m_tsType[5] = VC1_ENC_8x8_TRANSFORM;

        return UMC::UMC_OK;
    }
    inline bool isIntra(int32_t blockNum)
    {
        return  ((m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(blockNum)))!=0);
    }
    inline uint8_t GetIntraPattern()
    {
        return m_uiIntraPattern;
    }

    inline void SetMBIntraPattern(uint8_t uiIntraPattern)
    {
        m_uiIntraPattern = uiIntraPattern;
    }
    inline void SetIntraBlock(uint32_t blockNum)
    {
        m_uiIntraPattern = m_uiIntraPattern | (1<<VC_ENC_PATTERN_POS(blockNum));
    }
    inline void ChangeType(eMBType type)
    {
        m_MBtype = type;
    }
    inline eMBType GetMBType()
    {
        return m_MBtype;
    }

    inline uint8_t GetMBPattern()
    {
        return
           ((((m_nPairs[0][0] + m_nPairs[0][1] + m_nPairs[0][2] + m_nPairs[0][3])>0)<< (5 - 0))|
            (((m_nPairs[1][0] + m_nPairs[1][1] + m_nPairs[1][2] + m_nPairs[1][3])>0)<< (5 - 1))|
            (((m_nPairs[2][0] + m_nPairs[2][1] + m_nPairs[2][2] + m_nPairs[2][3])>0)<< (5 - 2))|
            (((m_nPairs[3][0] + m_nPairs[3][1] + m_nPairs[3][2] + m_nPairs[3][3])>0)<< (5 - 3))|
            (((m_nPairs[4][0] + m_nPairs[4][1] + m_nPairs[4][2] + m_nPairs[4][3])>0)<< (5 - 4))|
            (((m_nPairs[5][0] + m_nPairs[5][1] + m_nPairs[5][2] + m_nPairs[5][3])>0)<< (5 - 5)));

    }
     inline uint32_t GetBlocksPattern()
    {
        uint32_t  blocksPattern=0;
        int32_t  i;
        bool SubBlk[4];

        for(i=0;i<6;i++)
        {
            switch (m_tsType[i])
            {
            case  VC1_ENC_8x8_TRANSFORM:
                SubBlk[0]=SubBlk[1]=SubBlk[2]=SubBlk[3] = (m_nPairs[i][0]>0);
                break;
            case  VC1_ENC_8x4_TRANSFORM:
                SubBlk[0]=SubBlk[1]= (m_nPairs[i][0]>0);
                SubBlk[2]=SubBlk[3]= (m_nPairs[i][1]>0);
                break;
            case  VC1_ENC_4x8_TRANSFORM:
                SubBlk[0]=SubBlk[2]= (m_nPairs[i][0]>0);
                SubBlk[1]=SubBlk[3]= (m_nPairs[i][1]>0);
                break;
            case  VC1_ENC_4x4_TRANSFORM:
                SubBlk[0] =(m_nPairs[i][0]>0);
                SubBlk[1] =(m_nPairs[i][1]>0);
                SubBlk[2] =(m_nPairs[i][2]>0);
                SubBlk[3] =(m_nPairs[i][3]>0);
                break;
           }
           blocksPattern |= SubBlk[0]<<VC_ENC_PATTERN_POS1(i,0)
                         |  SubBlk[1]<<VC_ENC_PATTERN_POS1(i,1)
                         |  SubBlk[2]<<VC_ENC_PATTERN_POS1(i,2)
                         |  SubBlk[3]<<VC_ENC_PATTERN_POS1(i,3) ;

        }
        return blocksPattern;

    }
    inline bool isSkip()
    {
        switch (m_MBtype)
        {
            case VC1_ENC_I_MB:
            case VC1_ENC_P_MB_INTRA:
            case VC1_ENC_B_MB_INTRA:
                 return false;
            case VC1_ENC_B_MB_DIRECT:
                return (m_uiMBCBPCY == 0);
            case VC1_ENC_P_MB_1MV:
            case VC1_ENC_B_MB_F:
                return ((m_uiMBCBPCY == 0)&&(m_dMV[1][0].x==0 && m_dMV[1][0].y==0));
            case VC1_ENC_B_MB_B:
                return ((m_uiMBCBPCY == 0)&&(m_dMV[0][0].x==0 && m_dMV[0][0].y==0));
            case VC1_ENC_B_MB_FB:
                return ((m_uiMBCBPCY == 0)&&(m_dMV[1][0].x==0 && m_dMV[1][0].y==0)&&(m_dMV[0][0].x==0 && m_dMV[0][0].y==0));
            case VC1_ENC_P_MB_4MV:
                return ((m_uiMBCBPCY == 0)&&(m_dMV[1][0].x==0 && m_dMV[1][0].y==0 &&
                                             m_dMV[1][1].x==0 && m_dMV[1][1].y==0 &&
                                             m_dMV[1][2].x==0 && m_dMV[1][2].y==0 &&
                                             m_dMV[1][3].x==0 && m_dMV[1][3].y==0 ));
            case VC1_ENC_P_MB_SKIP_1MV:
            case VC1_ENC_P_MB_SKIP_4MV:
            case VC1_ENC_B_MB_SKIP_F:
            case VC1_ENC_B_MB_SKIP_B:
            case VC1_ENC_B_MB_SKIP_F_4MV:
            case VC1_ENC_B_MB_SKIP_B_4MV:
            case VC1_ENC_B_MB_SKIP_FB:
            case VC1_ENC_B_MB_SKIP_DIRECT:
                return true;
            default:
                return false;
          }


    }
    inline void SetHybrid (uint8_t Hybrid, uint32_t nBlock=0)
    {
        m_pHybrid[nBlock] = Hybrid;
    }

    inline uint8_t GetHybrid (uint32_t nBlock=0)
    {
       return m_pHybrid[nBlock];
    }

    inline void SetMBCBPCY (uint8_t  MBCBPCY)
    {
        m_uiMBCBPCY     =  MBCBPCY; //???
    }

    inline uint8_t GetMBCBPCY()
    {
        return m_uiMBCBPCY;
    }
    inline void SetdMV(sCoordinate dmv, bool bForward=true)
    {
        m_dMV[bForward][0].x = m_dMV[bForward][1].x = m_dMV[bForward][2].x = m_dMV[bForward][3].x = dmv.x;
        m_dMV[bForward][0].y = m_dMV[bForward][1].y = m_dMV[bForward][2].y = m_dMV[bForward][3].y = dmv.y;
    }
    inline void SetdMV_F(sCoordinate dmv, bool bForward=true)
    {
        m_dMV[bForward][0].x = m_dMV[bForward][1].x = m_dMV[bForward][2].x = m_dMV[bForward][3].x = dmv.x;
        m_dMV[bForward][0].y = m_dMV[bForward][1].y = m_dMV[bForward][2].y = m_dMV[bForward][3].y = dmv.y;
        m_dMV[bForward][0].bSecond = m_dMV[bForward][1].bSecond =
        m_dMV[bForward][2].bSecond = m_dMV[bForward][3].bSecond = dmv.bSecond;
    }
    inline void SetBlockdMV(sCoordinate dmv,int32_t block ,bool bForward=true)
    {
        m_dMV[bForward][block].x = dmv.x;
        m_dMV[bForward][block].y = dmv.y;
    }
    inline void SetBlockdMV_F(sCoordinate dmv,int32_t block ,bool bForward=true)
    {
        m_dMV[bForward][block].x = dmv.x;
        m_dMV[bForward][block].y = dmv.y;
        m_dMV[bForward][block].bSecond =dmv.bSecond;

    }

    inline void SetACPrediction (int8_t ACPred)
    {
        m_iACPrediction   = ACPred;
    }
    inline uint8_t GetACPrediction()
    {
        return m_iACPrediction;
    }

    inline void GetdMV(sCoordinate* dMV, bool forward = 1, uint8_t blk = 0)
    {
        dMV->x = m_dMV[forward][blk].x;
        dMV->y = m_dMV[forward][blk].y;
        dMV->bSecond = m_dMV[forward][blk].bSecond;
    }

    inline void SetTSType(eTransformType* tsType)
    {
        m_tsType[0]= tsType[0];
        m_tsType[1]= tsType[1];
        m_tsType[2]= tsType[2];
        m_tsType[3]= tsType[3];
        m_tsType[4]= tsType[4];
        m_tsType[5]= tsType[5];

        m_bMBTSType = ( m_tsType[0] == m_tsType[1] && m_tsType[0] == m_tsType[2] &&
                        m_tsType[0] == m_tsType[3] && m_tsType[0] == m_tsType[4] &&
                        m_tsType[0] == m_tsType[5]);
    }
    inline eTransformType* GetTSType()
    {
        return m_tsType;
    }

    inline eTransformType GetTSTypeBlk(uint8_t blk)
    {
        return m_tsType[blk];
    }

    inline void SetOverFlag(bool overflag)
    {
        m_bOverFlag = overflag;
    }

    inline bool GetOverFlag()
    {
        return m_bOverFlag;
    }

    void SaveResidual (int16_t* pBlk,uint32_t  step, const uint8_t* pScanMatrix, int32_t blk);

    void GetResiduals (int16_t* pBlk, uint32_t  step, const uint8_t* pScanMatrix, int32_t blk);

    inline bool isNulldMV(int32_t blk, bool bForward=true)
    {
        return (!((m_dMV[bForward][blk].x!=0)|| (m_dMV[bForward][blk].y!=0)));
    }
    // VLC coding

 #ifdef VC1_ME_MB_STATICTICS
    UMC::Status     SetMEFrStatPointer(UMC::MeMbStat* MEFrStat)
    {
        if(!MEFrStat)
        {
            assert(0);
            return UMC::UMC_ERR_NULL_PTR;
        }

        m_MECurMbStat = MEFrStat;

        return UMC::UMC_OK;
    }
 #endif

   UMC::Status WriteMBHeaderI_SM (VC1EncoderBitStreamSM* pCodedMB, bool bBitplanesRaw);
   UMC::Status WriteMBHeaderI_ADV(VC1EncoderBitStreamAdv* pCodedMB,bool bBitplanesRaw, bool bOverlapMB);
   UMC::Status WriteMBHeaderI_Field(VC1EncoderBitStreamAdv* pCodedMB,bool bBitplanesRaw, bool bOverlapMB);

template <class T>
UMC::Status WriteMBHeaderP_INTRA    ( T*              pCodedMB,
                                      bool            bBitplanesRaw,
                                      const uint16_t*   pMVDiffTable,
                                      const uint16_t*   pCBPCYTable)
{
    UMC::Status     err     =   UMC::UMC_OK;
    bool coded = (m_uiMBCBPCY!=0); //only 8x8 transform

    VC1_NULL_PTR(pCodedMB)

#ifdef VC1_ME_MB_STATICTICS
      uint16_t MBStart = (uint16_t)pCodedMB->GetCurrBit();
#endif
    if (bBitplanesRaw)
    {
        err = pCodedMB->PutBits(0,1); //if intra, then non-skip
        VC1_ENC_CHECK (err)
    }

    err = WriteMVDataPIntra(pCodedMB,pMVDiffTable);
    VC1_ENC_CHECK (err)


    if (coded ==0)
    {
        //err = WriteMBQuantParameter(pCodedMB, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        err = pCodedMB->PutBits(m_iACPrediction,1);
        VC1_ENC_CHECK (err)

    }
    else
    {
        err = pCodedMB->PutBits(m_iACPrediction,1);
        VC1_ENC_CHECK (err)
        err = pCodedMB->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)
    }
#ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (uint16_t)pCodedMB->GetCurrBit()- MBStart;
#endif
    return err;
}
template <class T>
UMC::Status WriteMBHeaderPField_INTRA          (  T*              pCodedMB,
                                                const  uint8_t*     pMBTypeFieldTable,
                                                const uint32_t*     pCBPCYTable)
{
    UMC::Status     err     =   UMC::UMC_OK;
    bool coded = (m_uiMBCBPCY!=0);
    VC1_NULL_PTR(pCodedMB)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedMB->GetCurrBit();
#endif

   err = pCodedMB->PutBits(pMBTypeFieldTable[coded<<1],pMBTypeFieldTable[(coded<<1)+1]); //if intra, then non-skip
   VC1_ENC_CHECK (err)

   //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
   //VC1_ENC_CHECK (err)

   err = pCodedMB->PutBits(m_iACPrediction,1);
   VC1_ENC_CHECK (err)

   if (coded)
   {
       err = pCodedMB->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
       VC1_ENC_CHECK (err)
   }
#ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (uint16_t)pCodedMB->GetCurrBit()- MBStart;
#endif
    return err;
}

template <class T>
UMC::Status WriteMBHeaderB_INTRA    ( T*              pCodedMB,
                                      bool            bBitplanesRaw,
                                      const uint16_t*   pMVDiffTable,
                                      const uint16_t*   pCBPCYTable)
{
    UMC::Status     err     =   UMC::UMC_OK;

    VC1_NULL_PTR(pCodedMB)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedMB->GetCurrBit();
#endif
    if (bBitplanesRaw)
    {
        err = pCodedMB->PutBits(0,1); //non directMB
        VC1_ENC_CHECK (err)
    }

    err =  WriteMBHeaderP_INTRA (pCodedMB,bBitplanesRaw,pMVDiffTable,pCBPCYTable);

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedMB->GetCurrBit()- MBStart;
#endif
    return err;
}


template <class T>  UMC::Status   WritePMB1MV (T*    pCodedBlock,
                                                     bool                   bRawBitplanes,
                                                     uint8_t                  MVTab,
                                                     uint8_t                  MVRangeIndex,
                                                     const uint16_t*          pCBPCYTable,
                                                     bool                   bVTS,
                                                     bool                   bMVHalf,
                                                     const int16_t           (*pTTMBVLC)[4][6],
                                                     const uint8_t            (*pTTBLKVLC)[6],
                                                     const uint8_t*           pSBP4x4Table,
                                                     const sACTablesSet*    pACTablesSet,
                                                     sACEscInfo*            pACEscInfo
                                                     )

{
UMC::Status  err=UMC::UMC_OK;
int32_t       blk;

VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    uint32_t MBStart = pCodedBlock->GetCurrBit();
#endif
if (bRawBitplanes)
{
    err = pCodedBlock->PutBits(0,1);//skip
    VC1_ENC_CHECK (err)
}
#ifdef VC1_ME_MB_STATICTICS
    uint16_t MVStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
    err = WriteMVDataInter(pCodedBlock, MVDiffTablesVLC[MVTab],MVRangeIndex, m_uiMBCBPCY!=0, bMVHalf);
    VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (uint16_t)pCodedBlock->GetCurrBit()- MVStart;
#endif

    if (m_pHybrid[0])
    {
        err = pCodedBlock->PutBits(m_pHybrid[0]-1,1); // Hybrid
        VC1_ENC_CHECK (err)
    }

if (m_uiMBCBPCY!=0)
{

    err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
    VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

    if (bVTS)
    {
        err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
        VC1_ENC_CHECK (err)
    }

    for (blk = 0; blk<6; blk++)
    {
        if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
        {
#ifdef VC1_ME_MB_STATICTICS
            //uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
            err = WriteVSTSBlk ( pCodedBlock,
                pTTBLKVLC,
                pSBP4x4Table,
                bVTS,
                blk);

            VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
            err = WriteBlockAC(pCodedBlock,
                   pACTablesSet,
                   pACEscInfo,
                   blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

            VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
            //m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for

}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)(pCodedBlock->GetCurrBit()- MBStart);
#endif
return err;
}
template <class T>  UMC::Status   WritePMB1MVField (T*                     pCodedBlock,
                                                    const uint8_t*            pMBTypeFieldTable,
                                                    sMVFieldInfo*          pMVFieldInfo,
                                                    uint8_t                  MVRangeIndex,
                                                    const uint32_t*          pCBPCYTable,
                                                    bool                   bVTS,
                                                    bool                   bMVHalf,
                                                    const int16_t           (*pTTMBVLC)[4][6],
                                                    const uint8_t            (*pTTBLKVLC)[6],
                                                    const uint8_t*           pSBP4x4Table,
                                                    const sACTablesSet*    pACTablesSet,
                                                    sACEscInfo*            pACEscInfo
                                                    )

{
    UMC::Status     err=UMC::UMC_OK;
    int32_t          blk;
    bool            bMVCoded =  (m_dMV[1][0].x || m_dMV[1][0].y);
    bool            bCoded   =  (m_uiMBCBPCY!=0);
    uint8_t           nMBType =   (((uint8_t)bCoded)<<1) + bMVCoded + 2;

    VC1_NULL_PTR(pCodedBlock)


#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MVStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
   if (bMVCoded)
   {
        err = WriteMVDataInterField1Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf);
        VC1_ENC_CHECK (err)
   }
   if (m_pHybrid[0])
   {
        err = pCodedBlock->PutBits(m_pHybrid[0]-1,1); // Hybrid
        VC1_ENC_CHECK (err)
   }
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (uint16_t)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WritePMB1MVFieldMixed (   T*                     pCodedBlock,
                                                            const uint8_t*            pMBTypeFieldTable,
                                                            sMVFieldInfo*          pMVFieldInfo,
                                                            uint8_t                  MVRangeIndex,
                                                            const uint32_t*          pCBPCYTable,
                                                            bool                   bVTS,
                                                            bool                   bMVHalf,
                                                            const int16_t           (*pTTMBVLC)[4][6],
                                                            const uint8_t            (*pTTBLKVLC)[6],
                                                            const uint8_t*           pSBP4x4Table,
                                                            const sACTablesSet*    pACTablesSet,
                                                            const uint8_t*           pMV4BP,
                                                            sACEscInfo*            pACEscInfo)

{
    UMC::Status     err=UMC::UMC_OK;
    int32_t          blk;
    uint8_t           MVPattern = 0;

    bool            bCoded   =  (m_uiMBCBPCY!=0);
    uint8_t           nMBType = (bCoded)? 7:6;


    VC1_NULL_PTR(pCodedBlock)


#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)

    for (int i = 0; i<4; i++)
    {
        if (m_dMV[1][i].x || m_dMV[1][i].y)
        {
            MVPattern = (uint8_t)(MVPattern | (1<<(3-i)));
        }    
    }
#ifdef VC1_ME_MB_STATICTICS
    uint16_t MVStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
   err = pCodedBlock->PutBits(pMV4BP[MVPattern<<1],pMV4BP[(MVPattern<<1)+1]);
   VC1_ENC_CHECK (err)

   for (int i = 0; i<4; i++)
   {
        if (MVPattern & (1<<(3-i)))
        {
            err = WriteMVDataInterField1Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, i);
            VC1_ENC_CHECK (err)
        }  
        if (m_pHybrid[i])
        {
            err = pCodedBlock->PutBits(m_pHybrid[i]-1,1); // Hybrid
            VC1_ENC_CHECK (err)
        }
   } 


#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (uint16_t)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WritePMB2MVField (T*                     pCodedBlock,
                                                    const uint8_t*           pMBTypeFieldTable,
                                                    sMVFieldInfo*          pMVFieldInfo,
                                                    uint8_t                  MVRangeIndex,
                                                    const uint32_t*          pCBPCYTable,
                                                    bool                   bVTS,
                                                    bool                   bMVHalf,
                                                    const int16_t           (*pTTMBVLC)[4][6],
                                                    const uint8_t            (*pTTBLKVLC)[6],
                                                    const uint8_t*           pSBP4x4Table,
                                                    const sACTablesSet*    pACTablesSet,
                                                    sACEscInfo*            pACEscInfo,
                                                    bool                   bForward=true)

{
    UMC::Status     err=UMC::UMC_OK;
    int32_t          blk;
    bool            bMVCoded =  (m_dMV[bForward][0].x || m_dMV[bForward][0].y || m_dMV[bForward][0].bSecond);
    bool            bCoded   =  (m_uiMBCBPCY!=0);
    uint8_t           nMBType =   (((uint8_t)bCoded)<<1) + bMVCoded + 2;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MVStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
   if (bMVCoded)
   {
        err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf);
        VC1_ENC_CHECK (err)
   }
   if (m_pHybrid[0])
   {
        err = pCodedBlock->PutBits(m_pHybrid[0]-1,1); // Hybrid
        VC1_ENC_CHECK (err)
   }

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (uint16_t)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}

template <class T>  UMC::Status   WritePMB2MVFieldMixed (T*                     pCodedBlock,
                                                         const uint8_t*           pMBTypeFieldTable,
                                                         sMVFieldInfo*          pMVFieldInfo,
                                                         uint8_t                  MVRangeIndex,
                                                         const uint32_t*          pCBPCYTable,
                                                         bool                   bVTS,
                                                         bool                   bMVHalf,
                                                         const int16_t           (*pTTMBVLC)[4][6],
                                                         const uint8_t            (*pTTBLKVLC)[6],
                                                         const uint8_t*           pSBP4x4Table,
                                                         const sACTablesSet*    pACTablesSet,
                                                         sACEscInfo*            pACEscInfo,
                                                         const uint8_t*           pMV4BP,
                                                         bool                   bForward=true)

{
    UMC::Status     err=UMC::UMC_OK;
    int32_t          blk;
    uint8_t           MVPattern = 0;

    bool            bCoded   =  (m_uiMBCBPCY!=0);
    uint8_t           nMBType = (bCoded)? 7:6;


    VC1_NULL_PTR(pCodedBlock)

    for (int i = 0; i<4; i++)
    {
        if (m_dMV[bForward][i].x || m_dMV[bForward][i].y || m_dMV[bForward][i].bSecond)
        {
            MVPattern = (uint8_t)(MVPattern | (1<<(3-i)));
        }    
    }

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)
   err = pCodedBlock->PutBits(pMV4BP[MVPattern<<1],pMV4BP[(MVPattern<<1)+1]);
   VC1_ENC_CHECK (err)



#ifdef VC1_ME_MB_STATICTICS
    uint16_t MVStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif 

   for (int i = 0; i<4; i++)
   {
        if (MVPattern & (1<<(3-i)))
        {
            err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, i);
            VC1_ENC_CHECK (err)
        }  
        if (m_pHybrid[i])
        {
            err = pCodedBlock->PutBits(m_pHybrid[i]-1,1); // Hybrid
            VC1_ENC_CHECK (err)
        }
   }  

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (uint16_t)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WriteBMBFieldForward     (T*                     pCodedBlock,
                                                            const uint8_t*           pMBTypeFieldTable,
                                                            sMVFieldInfo*          pMVFieldInfo,
                                                            uint8_t                  MVRangeIndex,
                                                            const uint32_t*          pCBPCYTable,
                                                            bool                   bVTS,
                                                            bool                   bMVHalf,
                                                            const int16_t           (*pTTMBVLC)[4][6],
                                                            const uint8_t            (*pTTBLKVLC)[6],
                                                            const uint8_t*           pSBP4x4Table,
                                                            const sACTablesSet*    pACTablesSet,
                                                            sACEscInfo*            pACEscInfo,
                                                            bool                   bBitPlaneRaw)

{
    UMC::Status     err=UMC::UMC_OK;
    int32_t          blk;
    bool            bMVCoded =  (m_dMV[1][0].x || m_dMV[1][0].y || m_dMV[1][0].bSecond);
    bool            bCoded   =  (m_uiMBCBPCY!=0);
    uint8_t           nMBType =   (((uint8_t)bCoded)<<1) + bMVCoded + 2;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)
   if (bBitPlaneRaw)
   {
        err = pCodedBlock->PutBits(1,1);
        VC1_ENC_CHECK (err)
   }

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MVStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
   if (bMVCoded)
   {
        err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf);
        VC1_ENC_CHECK (err)
   }

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (uint16_t)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WriteBMBFieldForwardMixed     (T*                     pCodedBlock,
                                                            const uint8_t*           pMBTypeFieldTable,
                                                            sMVFieldInfo*          pMVFieldInfo,
                                                            uint8_t                  MVRangeIndex,
                                                            const uint32_t*          pCBPCYTable,
                                                            bool                   bVTS,
                                                            bool                   bMVHalf,
                                                            const int16_t           (*pTTMBVLC)[4][6],
                                                            const uint8_t            (*pTTBLKVLC)[6],
                                                            const uint8_t*           pSBP4x4Table,
                                                            const sACTablesSet*    pACTablesSet,
                                                            sACEscInfo*            pACEscInfo,
                                                            const uint8_t*           pMV4BP,
                                                            bool                   bBitPlaneRaw)

{
    UMC::Status     err=UMC::UMC_OK;
    int32_t          blk;
    uint8_t           MVPattern = 0;
    bool            bCoded   =  (m_uiMBCBPCY!=0);
    uint8_t           nMBType = (bCoded)? 7:6;

    VC1_NULL_PTR(pCodedBlock)
    for (int i = 0; i<4; i++)
    {
        if (m_dMV[1][i].x || m_dMV[1][i].y || m_dMV[1][i].bSecond)
        {
            MVPattern = (uint8_t)(MVPattern | (1<<(3-i)));
        }    
    }
#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif

    err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
    VC1_ENC_CHECK (err)
    if (bBitPlaneRaw)
    {
        err = pCodedBlock->PutBits(1,1);
        VC1_ENC_CHECK (err)
    }

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MVStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
    err = pCodedBlock->PutBits(pMV4BP[MVPattern<<1],pMV4BP[(MVPattern<<1)+1]);
    VC1_ENC_CHECK (err)
    for (int i = 0; i<4; i++)
    {
        if (MVPattern & (1<<(3-i)))
        {
            err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, i, true);
            VC1_ENC_CHECK (err)
        }  
    }  

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (uint16_t)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
            //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
            //VC1_ENC_CHECK (err)

            if (bVTS)
            {
                err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
                VC1_ENC_CHECK (err)
            }

            for (blk = 0; blk<6; blk++)
            {
                if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
                {
#ifdef VC1_ME_MB_STATICTICS
                    uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
                    err = WriteVSTSBlk ( pCodedBlock,
                        pTTBLKVLC,
                        pSBP4x4Table,
                        bVTS,
                        blk);

                    VC1_ENC_CHECK (err)

                        STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                    err = WriteBlockAC(pCodedBlock,
                        pACTablesSet,
                        pACEscInfo,
                        blk);
                    STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                    VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
#endif
                }//if
            }//for
    }//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
   m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
#endif
   return err;
}
template <class T>  UMC::Status   WriteBMBFieldBackward    (T*                     pCodedBlock,
                                                            const uint8_t*           pMBTypeFieldTable,
                                                            sMVFieldInfo*          pMVFieldInfo,
                                                            uint8_t                  MVRangeIndex,
                                                            const uint32_t*          pCBPCYTable,
                                                            bool                   bVTS,
                                                            bool                   bMVHalf,
                                                            const int16_t           (*pTTMBVLC)[4][6],
                                                            const uint8_t            (*pTTBLKVLC)[6],
                                                            const uint8_t*           pSBP4x4Table,
                                                            const sACTablesSet*    pACTablesSet,
                                                            sACEscInfo*            pACEscInfo,
                                                            bool                   bBitPlaneRaw)

{
    UMC::Status     err=UMC::UMC_OK;
    int32_t          blk;
    bool            bMVCoded =  (m_dMV[0][0].x || m_dMV[0][0].y || m_dMV[0][0].bSecond);
    bool            bCoded   =  (m_uiMBCBPCY!=0);
    uint8_t           nMBType =   (((uint8_t)bCoded)<<1) + bMVCoded + 2;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)
   if (bBitPlaneRaw)
   {
        err = pCodedBlock->PutBits(0,1);
        VC1_ENC_CHECK (err)
   }
   err = pCodedBlock->PutBits(0,1);
   VC1_ENC_CHECK (err)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MVStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif

   if (bMVCoded)
   {
        err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, 0, false);
        VC1_ENC_CHECK (err)
   }

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (uint16_t)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit() - MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WriteBMBFieldBackwardMixed    (T*                     pCodedBlock,
                                                            const uint8_t*           pMBTypeFieldTable,
                                                            sMVFieldInfo*          pMVFieldInfo,
                                                            uint8_t                  MVRangeIndex,
                                                            const uint32_t*          pCBPCYTable,
                                                            bool                   bVTS,
                                                            bool                   bMVHalf,
                                                            const int16_t           (*pTTMBVLC)[4][6],
                                                            const uint8_t            (*pTTBLKVLC)[6],
                                                            const uint8_t*           pSBP4x4Table,
                                                            const sACTablesSet*    pACTablesSet,
                                                            sACEscInfo*            pACEscInfo,
                                                            const uint8_t*           pMV4BP,
                                                            bool                   bBitPlaneRaw)

{
    UMC::Status     err=UMC::UMC_OK;
    int32_t          blk;
    uint8_t           MVPattern = 0;
    bool            bCoded   =  (m_uiMBCBPCY!=0);
    uint8_t           nMBType = (bCoded)? 7:6;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
        uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
    for (int i = 0; i<4; i++)
    {
        if (m_dMV[0][i].x || m_dMV[0][i].y || m_dMV[0][i].bSecond)
        {
            MVPattern = (uint8_t)(MVPattern | (1<<(3-i)));
        }    
    }
    err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
    VC1_ENC_CHECK (err)

    if (bBitPlaneRaw)
    {
        err = pCodedBlock->PutBits(0,1);
        VC1_ENC_CHECK (err)
    }
    //err = pCodedBlock->PutBits(0,1);
    //VC1_ENC_CHECK (err)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MVStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
    err = pCodedBlock->PutBits(pMV4BP[MVPattern<<1],pMV4BP[(MVPattern<<1)+1]);
    VC1_ENC_CHECK (err)

    for (int i = 0; i<4; i++)
    {
        if (MVPattern & (1<<(3-i)))
        {
            err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, i, false);
            VC1_ENC_CHECK (err)
        }  
    }  
#ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->MVF[0] += (uint16_t)pCodedBlock->GetCurrBit()- MVStart;
#endif
        if (bCoded)
        {
            err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
            VC1_ENC_CHECK (err)
                //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
                //VC1_ENC_CHECK (err)

                if (bVTS)
                {
                    err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
                    VC1_ENC_CHECK (err)
                }

                for (blk = 0; blk<6; blk++)
                {
                    if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
                    {
#ifdef VC1_ME_MB_STATICTICS
                        uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
                        err = WriteVSTSBlk ( pCodedBlock,
                            pTTBLKVLC,
                            pSBP4x4Table,
                            bVTS,
                            blk);

                        VC1_ENC_CHECK (err)

                            STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                        err = WriteBlockAC(pCodedBlock,
                            pACTablesSet,
                            pACEscInfo,
                            blk);
                        STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                        VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                            m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
#endif
                    }//if
                }//for
        }//MBPattern!=0

#ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit() - MBStart;
#endif
        return err;
}
template <class T>  UMC::Status   WriteBMBFieldInterpolated    (T*                     pCodedBlock,
                                                                const uint8_t*           pMBTypeFieldTable,
                                                                sMVFieldInfo*          pMVFieldInfo,
                                                                uint8_t                  MVRangeIndex,
                                                                const uint32_t*          pCBPCYTable,
                                                                bool                   bVTS,
                                                                bool                   bMVHalf,
                                                                const int16_t           (*pTTMBVLC)[4][6],
                                                                const uint8_t            (*pTTBLKVLC)[6],
                                                                const uint8_t*           pSBP4x4Table,
                                                                const sACTablesSet*    pACTablesSet,
                                                                sACEscInfo*            pACEscInfo,
                                                                bool                   bBitPlaneRaw)

{
    UMC::Status     err=UMC::UMC_OK;
    int32_t          blk;
    bool            bMVCodedF =  (m_dMV[1][0].x || m_dMV[1][0].y || m_dMV[1][0].bSecond);
    bool            bMVCodedB =  (m_dMV[0][0].x || m_dMV[0][0].y || m_dMV[0][0].bSecond);

    bool            bCoded   =  (m_uiMBCBPCY!=0);
    uint8_t           nMBType =   (((uint8_t)bCoded)<<1) + (bMVCodedF) + 2;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)
   if (bBitPlaneRaw)
   {
        err = pCodedBlock->PutBits(0,1);
        VC1_ENC_CHECK (err)
   }
   err = pCodedBlock->PutBits(3,2);
   VC1_ENC_CHECK (err)

   err = pCodedBlock->PutBits(bMVCodedB,1);
   VC1_ENC_CHECK (err)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MVStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
   if (bMVCodedF)
   {
        err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, 0 ,true);
        VC1_ENC_CHECK (err)
   }
    if (bMVCodedB)
   {
        err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, 0,false);
        VC1_ENC_CHECK (err)
   }
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (uint16_t)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WriteBMBFieldDirect           (T*                     pCodedBlock,
                                                                const uint8_t*           pMBTypeFieldTable,
                                                                sMVFieldInfo*          /*pMVFieldInfo*/,
                                                                uint8_t                  /*MVRangeIndex*/,
                                                                const uint32_t*          pCBPCYTable,
                                                                bool                   bVTS,
                                                                const int16_t           (*pTTMBVLC)[4][6],
                                                                const uint8_t            (*pTTBLKVLC)[6],
                                                                const uint8_t*           pSBP4x4Table,
                                                                const sACTablesSet*    pACTablesSet,
                                                                sACEscInfo*            pACEscInfo,
                                                                bool                   bBitPlaneRaw)

{
    UMC::Status     err=UMC::UMC_OK;
    int32_t          blk;

    bool            bCoded   =  (m_uiMBCBPCY!=0);
    uint8_t           nMBType =   (((uint8_t)bCoded)<<1) + 0 + 2;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)
   if (bBitPlaneRaw)
   {
        err = pCodedBlock->PutBits(0,1);
        VC1_ENC_CHECK (err)
   }
   err = pCodedBlock->PutBits(2,2);
   VC1_ENC_CHECK (err)

    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WritePMB1MVMixed  (T*                     pCodedBlock,
                                                     bool                   bRawBitplanes,
                                                     uint8_t                  MVTab,
                                                     uint8_t                  MVRangeIndex,
                                                     const uint16_t*          pCBPCYTable,
                                                     bool                   bVTS,
                                                     bool                   bMVHalf,
                                                     const int16_t           (*pTTMBVLC)[4][6],
                                                     const uint8_t            (*pTTBLKVLC)[6],
                                                     const uint8_t*           pSBP4x4Table,
                                                     const sACTablesSet*    pACTablesSet,
                                                     sACEscInfo*            pACEscInfo)

{
UMC::Status  err        =   UMC::UMC_OK;
VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
if (bRawBitplanes)
{
    UMC::Status  err=UMC::UMC_OK;
    err = pCodedBlock->PutBits(0,1);//mixed 1 MV
    VC1_ENC_CHECK (err)
}

err =  WritePMB1MV  (pCodedBlock,  bRawBitplanes, MVTab,
                     MVRangeIndex, pCBPCYTable,   bVTS, bMVHalf,
                     pTTMBVLC,     pTTBLKVLC,     pSBP4x4Table,
                     pACTablesSet, pACEscInfo);

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
#endif

return err;

}
template <class T>  UMC::Status   WritePMB4MVMixed (T*                     pCodedBlock,
                                                    bool                   bRawBitplanes,
                                                    uint32_t                 quant,
                                                    uint8_t                  MVTab,
                                                    uint8_t                  MVRangeIndex,
                                                    const uint16_t*          pCBPCYTable,
                                                    const uint16_t*          pMVDiffTable,
                                                    bool                   bVTS,
                                                    bool                   bMVHalf,
                                                    const int16_t           (*pTTMBVLC)[4][6],
                                                    const uint8_t            (*pTTBLKVLC)[6],
                                                    const uint8_t*           pSBP4x4Table,
                                                    const uint32_t**         pDCEncTable,
                                                    const sACTablesSet**   pACTablesSetIntra,
                                                    const sACTablesSet*    pACTablesSetInter,
                                                    sACEscInfo*            pACEscInfo
                                                    )

{
    UMC::Status  err        =   UMC::UMC_OK;
    int32_t       blk        =   0;
    uint32_t       pattern    =   m_uiMBCBPCY;
    bool         bInterCoded=   (m_uiMBCBPCY & (~m_uiIntraPattern))? true:false;

    #ifdef VC1_ME_MB_STATICTICS
        uint32_t MBStart = pCodedBlock->GetCurrBit();
    #endif
    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(1,1);//mixed 1 MV
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(0,1);//skip
        VC1_ENC_CHECK (err)
    }
    for (blk=0; blk<4;blk++)
    {
        pattern |=
            ( !isNulldMV(blk)|| (m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(blk))))?
            (1<<VC_ENC_PATTERN_POS(blk)):0;

    }
    err = pCodedBlock->PutBits(pCBPCYTable[2*(pattern&0x3F)], pCBPCYTable[2*(pattern&0x3F)+1]);
    VC1_ENC_CHECK (err)

    for (blk=0; blk<4;blk++)
    {
    #ifdef VC1_ME_MB_STATICTICS
        uint32_t BlkStart = pCodedBlock->GetCurrBit();
        uint32_t MVStart = pCodedBlock->GetCurrBit();
    #endif
        if(pattern & (1<<VC_ENC_PATTERN_POS(blk)))
        {
            if (m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(blk)))
            {
                err = WriteMVDataPIntra(pCodedBlock,pMVDiffTable,blk);
                VC1_ENC_CHECK (err)
            }
            else
            {
                err = WriteMVDataInter(pCodedBlock, MVDiffTablesVLC[MVTab],MVRangeIndex,(m_uiMBCBPCY&(1<<VC_ENC_PATTERN_POS(blk)))!=0, bMVHalf, blk);
                VC1_ENC_CHECK (err)
            }
        }
        if (m_pHybrid[blk] && (!(m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(blk)))))
        {
            err = pCodedBlock->PutBits(m_pHybrid[blk]-1,1); // Hybrid
            VC1_ENC_CHECK (err)
        }
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->MVF[blk]   =  m_MECurMbStat->MVF[blk]  + (uint16_t)(pCodedBlock->GetCurrBit()- MVStart);
        m_MECurMbStat->coeff[blk] =  m_MECurMbStat->coeff[blk]+ (uint16_t)(pCodedBlock->GetCurrBit()- BlkStart);
    #endif
    }
//err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
//VC1_ENC_CHECK (err)

if (m_uiIntraPattern && m_iACPrediction>=0)
{
    err = pCodedBlock->PutBits(m_iACPrediction,1);
    VC1_ENC_CHECK (err)
}
if (bVTS && bInterCoded)
{
    err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
    VC1_ENC_CHECK (err)
}

for (blk = 0; blk<6; blk++)
{
    const sACTablesSet*   pACTablesSet = 0;

#ifdef VC1_ME_MB_STATICTICS
    uint32_t BlkStart = pCodedBlock->GetCurrBit();
#endif

    if (m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(blk)))
    {
        err = WriteBlockDC(pCodedBlock,pDCEncTable[blk],quant,blk);
        VC1_ENC_CHECK (err)
        pACTablesSet =  pACTablesSetIntra[blk];
    }
    else
    {
         if ((m_uiMBCBPCY & (1<<VC_ENC_PATTERN_POS(blk))))
         {
            err = WriteVSTSBlk ( pCodedBlock,
            pTTBLKVLC,
            pSBP4x4Table,
            bVTS,
            blk);

            VC1_ENC_CHECK (err)
         }
         pACTablesSet =  pACTablesSetInter;
    }

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
    if ((m_uiMBCBPCY & (1<<VC_ENC_PATTERN_POS(blk))))
    {
        err = WriteBlockAC(pCodedBlock,
            pACTablesSet,
            pACEscInfo,
            blk);

        VC1_ENC_CHECK (err)
    }
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->coeff[blk] = m_MECurMbStat->coeff[blk]+(uint16_t)(pCodedBlock->GetCurrBit()- BlkStart);
#endif
}//for

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)(pCodedBlock->GetCurrBit()- MBStart);
#endif
return err;
}

template <class T>  UMC::Status   WritePMB4MVSkipMixed    (T*                   pCodedBlock,
                                                           bool                 bRawBitplanes)

{
    UMC::Status  err        =   UMC::UMC_OK;
    int32_t       blk        =   0;

    VC1_NULL_PTR(pCodedBlock)

    #ifdef VC1_ME_MB_STATICTICS
        uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
    #endif
    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(1,1);//mixed 1 MV
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(1,1);//skip
        VC1_ENC_CHECK (err)
    }
    for (blk=0; blk<4;blk++)
    {
    #ifdef VC1_ME_MB_STATICTICS
        uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
    #endif
        if (m_pHybrid[blk])
        {
            err = pCodedBlock->PutBits(m_pHybrid[blk]-1,1); // Hybrid
            VC1_ENC_CHECK (err)
        }
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
    #endif
    }
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
    #endif
    return err;
}
template <class T>  UMC::Status   WritePMB1MVSkipMixed    (T*                   pCodedBlock,
                                                           bool                 bRawBitplanes)

{
    UMC::Status  err        =   UMC::UMC_OK;
    //int32_t       blk        =   0;

    VC1_NULL_PTR(pCodedBlock)
    #ifdef VC1_ME_MB_STATICTICS
        uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
    #endif

    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(0,1);//mixed 1 MV
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(1,1);//skip
        VC1_ENC_CHECK (err)
    }
    if (m_pHybrid[0])
    {
        err = pCodedBlock->PutBits(m_pHybrid[0]-1,1); // Hybrid
        VC1_ENC_CHECK (err)
    }

    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
    #endif
    return err;
}

template <class T>  UMC::Status   WriteBMB_DIRECT      (T*                     pCodedBlock,
                                                        bool                   bRawBitplanes,
                                                        const uint16_t*          pCBPCYTable,
                                                        bool                   bVTS,
                                                        const int16_t           (*pTTMBVLC)[4][6],
                                                        const uint8_t            (*pTTBLKVLC)[6],
                                                        const uint8_t*           pSBP4x4Table,
                                                        const sACTablesSet*    pACTablesSet,
                                                        sACEscInfo*            pACEscInfo)
{
    UMC::Status  err=UMC::UMC_OK;
    int32_t       blk;

    VC1_NULL_PTR(pCodedBlock)
    #ifdef VC1_ME_MB_STATICTICS
        uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
    #endif

    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(1,1); //direct MB
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(m_uiMBCBPCY==0,1); //skip
        VC1_ENC_CHECK (err)
    }
    if (m_uiMBCBPCY!=0)
    {

        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //if (err != UMC::UMC_OK) return err;

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }
        for (blk = 0; blk<6; blk++)
        {
           if ((m_uiMBCBPCY & (1<<VC_ENC_PATTERN_POS(blk))))
           {
    #ifdef VC1_ME_MB_STATICTICS
        uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
    #endif
            err = WriteVSTSBlk ( pCodedBlock,
                pTTBLKVLC,
                pSBP4x4Table,
                bVTS,
                blk);

            VC1_ENC_CHECK (err)

    STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                pACTablesSet,
                pACEscInfo,
                blk);
    STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

            VC1_ENC_CHECK (err)
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
    #endif
           }
        }//for
    }//MBPattern!=0

    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
    #endif
    return err;
}
template <class T>  UMC::Status  WriteBMB      (T*                     pCodedBlock,
                                                bool                   bRawBitplanes,
                                                uint8_t                  MVTab,
                                                uint8_t                  MVRangeIndex,
                                                const uint16_t*          pCBPCYTable,
                                                bool                   bVTS,
                                                bool                   bMVHalf,
                                                const int16_t           (*pTTMBVLC)[4][6],
                                                const uint8_t            (*pTTBLKVLC)[6],
                                                const uint8_t*           pSBP4x4Table,
                                                const sACTablesSet*    pACTablesSet,
                                                sACEscInfo*            pACEscInfo,
                                                bool                   bBFraction)
{
    UMC::Status  err=UMC::UMC_OK;
    int32_t       blk;
    uint8_t        mvType = (m_MBtype == VC1_ENC_B_MB_F)? 0:((m_MBtype == VC1_ENC_B_MB_B)?1:2);
    bool         NotLastInter = false;


    VC1_NULL_PTR(pCodedBlock)
    #ifdef VC1_ME_MB_STATICTICS
        uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
    #endif

    if (m_MBtype == VC1_ENC_B_MB_FB)
    {
        NotLastInter = (m_dMV[1][0].x!=0 || m_dMV[1][0].y!=0 || m_uiMBCBPCY!=0);

    }
    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(0,1); //direct MB
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(0,1); //skip
        VC1_ENC_CHECK (err)
    }
    #ifdef VC1_ME_MB_STATICTICS
        uint16_t MVStart = (uint16_t)pCodedBlock->GetCurrBit();
    #endif
    err = WriteMVDataInter(pCodedBlock, MVDiffTablesVLC[MVTab],MVRangeIndex, m_uiMBCBPCY!=0 || NotLastInter,  bMVHalf, 0, m_MBtype == VC1_ENC_B_MB_F);
    VC1_ENC_CHECK (err)

    #ifdef VC1_ME_MB_STATICTICS
        if(m_MBtype == VC1_ENC_B_MB_F)
            m_MECurMbStat->MVF[0] += (uint16_t)pCodedBlock->GetCurrBit()- MVStart;
        else
            m_MECurMbStat->MVB[0] += (uint16_t)pCodedBlock->GetCurrBit()- MVStart;
    #endif

    err = pCodedBlock->PutBits(BMVTypeVLC[bBFraction][mvType*2],BMVTypeVLC[bBFraction][mvType*2+1]);
    VC1_ENC_CHECK (err)

    if (NotLastInter)
    {
    #ifdef VC1_ME_MB_STATICTICS
        MVStart = (uint16_t)pCodedBlock->GetCurrBit();
    #endif
            err = WriteMVDataInter(pCodedBlock, MVDiffTablesVLC[MVTab],MVRangeIndex,   m_uiMBCBPCY!=0, bMVHalf, 0, true);
            VC1_ENC_CHECK (err)
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->MVB[0] += (uint16_t)pCodedBlock->GetCurrBit()- MVStart;
    #endif
    }

    if (m_uiMBCBPCY!=0)
    {

        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //if (err != UMC::UMC_OK) return err;

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }
        for (blk = 0; blk<6; blk++)
        {
            if ((m_uiMBCBPCY & (1<<VC_ENC_PATTERN_POS(blk))))
            {
    #ifdef VC1_ME_MB_STATICTICS
        uint16_t BlkStart = (uint16_t)pCodedBlock->GetCurrBit();
    #endif

            err = WriteVSTSBlk ( pCodedBlock,
                pTTBLKVLC,
                pSBP4x4Table,
                bVTS,
                blk);

            VC1_ENC_CHECK (err)

    STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                pACTablesSet,
                pACEscInfo,
                blk);
    STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

            VC1_ENC_CHECK (err)
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->coeff[blk] += (uint16_t)pCodedBlock->GetCurrBit()- BlkStart;
    #endif
            }
        }//for
    }//MBPattern!=0
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
    #endif
    return err;
}


template <class T>
UMC::Status   WritePMB_SKIP(T*        pCodedBlock,
                            bool      bRawBitplanes)
{
    UMC::Status  err=UMC::UMC_OK;

    VC1_NULL_PTR(pCodedBlock)
#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif

    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(1,1);//skip
        VC1_ENC_CHECK (err)
    }
    if (m_pHybrid[0])
    {

        err = pCodedBlock->PutBits(m_pHybrid[0]-1,1); // Hybrid
        VC1_ENC_CHECK (err)

    }
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
#endif
    return err;
}

template <class T>
UMC::Status   WriteBMB_SKIP_NONDIRECT(T*      pCodedBlock,
                                      bool                   bRawBitplanes,
                                      bool                   bBFraction)
{
    UMC::Status     err     = UMC::UMC_OK;
    uint8_t           mvType  = 0;

    VC1_NULL_PTR(pCodedBlock)
#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif

    switch(m_MBtype)
    {
    case VC1_ENC_B_MB_SKIP_F :
    case VC1_ENC_B_MB_F:
            mvType = 0;
            break;
    case VC1_ENC_B_MB_SKIP_B :
    case VC1_ENC_B_MB_B:
            mvType = 1;
            break;
    default:
            mvType = 2;
            break;
    }
    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(0,1); //direct MB
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(1,1); //skip
        VC1_ENC_CHECK (err)
    }
    err = pCodedBlock->PutBits(BMVTypeVLC[bBFraction][mvType*2],BMVTypeVLC[bBFraction][mvType*2+1]);
    VC1_ENC_CHECK (err);
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
#endif
    return err;
}

template <class T>
UMC::Status  WriteBMB_SKIP_DIRECT(T*                     pCodedBlock,
                                  bool                   bRawBitplanes)
{
    UMC::Status  err = UMC::UMC_OK;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedBlock->GetCurrBit();
#endif
    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(1,1); //direct MB
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(1,1); //skip
        VC1_ENC_CHECK (err)
    }

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedBlock->GetCurrBit()- MBStart;
#endif
    return err;
}

template <class T>
UMC::Status WritePMB_INTRA (T*                     pCodedMB,
                            bool                   bBitplanesRaw,
                            uint32_t                 quant,
                            const uint16_t*          pMVDiffTable,
                            const uint16_t*          pCBPCYTable,
                            const uint32_t**         pDCEncTable,
                            const sACTablesSet**   pACTablesSet,
                            sACEscInfo*            pACEscInfo)
{
    UMC::Status     err     =   UMC::UMC_OK;
    uint32_t          blk;

        VC1_NULL_PTR(pCodedMB)

#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedMB->GetCurrBit();
#endif
    err     = WriteMBHeaderP_INTRA    ( pCodedMB,bBitplanesRaw, pMVDiffTable,pCBPCYTable);
    VC1_ENC_CHECK (err)

    for (blk=0; blk<6; blk++)
    {
#ifdef VC1_ME_MB_STATICTICS
    uint16_t BlkStart = (uint16_t)pCodedMB->GetCurrBit();
#endif
        err = WriteBlockDC(pCodedMB,pDCEncTable[blk],quant,blk);
        VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
        err = WriteBlockAC(pCodedMB,pACTablesSet[blk], pACEscInfo,blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);
        VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->coeff[blk] += (uint16_t)pCodedMB->GetCurrBit()- BlkStart;
#endif
    }

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedMB->GetCurrBit()- MBStart;
#endif
    return err;
}

template <class T>
UMC::Status WritePMBMixed_INTRA (T*                     pCodedMB,
                                 bool                   bBitplanesRaw,
                                 uint32_t                 quant,
                                 const uint16_t*          pMVDiffTable,
                                 const uint16_t*          pCBPCYTable,
                                 const uint32_t**         pDCEncTable,
                                 const sACTablesSet**   pACTablesSet,
                                 sACEscInfo*            pACEscInfo)
{
     UMC::Status     err     =   UMC::UMC_OK;
#ifdef VC1_ME_MB_STATICTICS
    uint16_t MBStart = (uint16_t)pCodedMB->GetCurrBit();
#endif
    if (bBitplanesRaw)
    {
        UMC::Status     err     =   UMC::UMC_OK;
        err = pCodedMB->PutBits(0,1);
        VC1_ENC_CHECK (err)
    }

    err = WritePMB_INTRA(pCodedMB, bBitplanesRaw,quant,pMVDiffTable,pCBPCYTable,pDCEncTable, pACTablesSet, pACEscInfo);

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (uint16_t)pCodedMB->GetCurrBit()- MBStart;
#endif
    return err;
}

/*-------------------------------------------------------------------*/

template <class T>
UMC::Status   WriteBlockDC  (    T*                pCodedBlock,
                                    const uint32_t*     pEncTable,
                                    uint32_t            quant,
                                    int32_t             blk)
{
    UMC::Status UMCSts = UMC::UMC_OK;
#ifdef VC1_ME_MB_STATICTICS
    uint32_t BlkStart = pCodedBlock->GetCurrBit();;
#endif
    switch (quant)
    {
    case 1:
            UMCSts = WriteDCQuant1    (m_iDC[blk],pEncTable, pCodedBlock);
        break;
    case 2:
            UMCSts = WriteDCQuant2    (m_iDC[blk],pEncTable, pCodedBlock);
        break;
    default:
            UMCSts = WriteDCQuantOther (m_iDC[blk],pEncTable, pCodedBlock);
    }
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->coeff[blk] = m_MECurMbStat->coeff[blk] +(uint16_t)(pCodedBlock->GetCurrBit() - BlkStart);
#endif

    return UMCSts;
}
template <class T>  UMC::Status   WriteBlockAC( T*                      pCodedBlock,
                                                const sACTablesSet*     pACTablesSet,
                                                sACEscInfo*             pACEscInfo,
                                                uint32_t                  blk)
{
    UMC::Status     err                    = UMC::UMC_OK;
    int32_t          i                      = 0;
    static const int32_t    nSubblk [4]     = {1,2,2,4};

    const uint32_t    *pEncTable   = pACTablesSet->pEncTable;
    uint8_t           nPairsBlock  = 0;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
      uint32_t BlkStart = pCodedBlock->GetCurrBit();
#endif
    for (int32_t nSubblock=0; nSubblock<nSubblk[m_tsType[blk]]; nSubblock++)
    {
        const uint8_t     *pTableDR    = pACTablesSet->pTableDR;
        const uint8_t     *pTableDL    = pACTablesSet->pTableDL;
        const uint8_t     *pTableInd   = pACTablesSet->pTableInd;
        uint8_t           nPairs       = m_nPairs[blk][nSubblock];

        if (nPairs == 0)
            continue;

        // put codes into bitstream
        i = 0;
        for (int32_t not_last = 1; not_last>=0; not_last--)
        {
            for (; i < nPairs - not_last; i++)
            {
                bool    sign = false;
                uint8_t   run  = m_uRun [blk] [i+nPairsBlock];
                int16_t  lev  = m_iLevel[blk][i+nPairsBlock];

                uint8_t mode = GetMode( run, lev, pTableDR, pTableDL, sign);

#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetRLMode(mode, blk, i+nPairsBlock);
#endif

                switch (mode)
                {
                    case 3:
                        err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
                        VC1_ENC_CHECK (err)
                        err = pCodedBlock->PutBits(0,2);                       //mode
                        VC1_ENC_CHECK (err)
                        err = pCodedBlock->PutBits(!not_last,1);               //last
                        VC1_ENC_CHECK (err)
                        if ((!pACEscInfo->levelSize) && (!pACEscInfo->runSize))
                        {
                            pACEscInfo->runSize = 6;
                            pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*11 +1]==0)? 8:11;
                            pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize + 1]<=0) ?
                                                            2 : pACEscInfo->levelSize;
                            err = pCodedBlock->PutBits(pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize],
                                                    pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize +1]);
                            VC1_ENC_CHECK (err)
                            err = pCodedBlock->PutBits(pACEscInfo->runSize - 3, 2);
                            VC1_ENC_CHECK (err)
                        }
                        err = pCodedBlock->PutBits(run,pACEscInfo->runSize);
                        VC1_ENC_CHECK (err)
                        err = pCodedBlock->PutBits(sign, 1);
                        VC1_ENC_CHECK (err)
                        if (lev>((1<<pACEscInfo->levelSize)-1))
                        {
                            lev =(1<<pACEscInfo->levelSize)-1;
                            m_iLevel[blk][i+nPairsBlock] = lev;
                        }
                        err = pCodedBlock->PutBits(lev,pACEscInfo->levelSize);
                        VC1_ENC_CHECK (err)
                        break;
                    case 2:
                    case 1:
                        err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
                        VC1_ENC_CHECK (err)
                        err = pCodedBlock->PutBits(1,mode);                     //mode
                        VC1_ENC_CHECK (err)
                     case 0:
                        int16_t index = pTableInd[run] + lev;
                        err = pCodedBlock->PutBits(pEncTable[2*index], pEncTable[2*index + 1]);
                        VC1_ENC_CHECK (err)
                        err = pCodedBlock->PutBits(sign, 1);
                        VC1_ENC_CHECK (err)
                        break;

                }
            }

            pTableDR    = pACTablesSet->pTableDRLast;
            pTableDL    = pACTablesSet->pTableDLLast;
            pTableInd   = pACTablesSet->pTableIndLast;

        }
        nPairsBlock = nPairsBlock + m_nPairs[blk][nSubblock];
    }
#ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->coeff[blk] = m_MECurMbStat->coeff[blk] + (uint16_t)(pCodedBlock->GetCurrBit() - BlkStart);
#endif
    return UMC::UMC_OK;
}


protected:
   template <class T>
   UMC::Status   WriteVSTSBlk (    T*                      pCodedBlock,
                                   const uint8_t            (*pTTBLKVLC)[6],
                                   const uint8_t*            pSBP4x4Table,
                                   bool                    bVTS,
                                   uint32_t                   blk)
   {
       UMC::Status     err                    = UMC::UMC_OK;
       eTransformType  type                   = m_tsType[blk];

       VC1_NULL_PTR(pCodedBlock)

       if ( m_uiMBCBPCY == 0)
            return err;

        if (bVTS && !m_bMBTSType && blk!= m_uiFirstCodedBlock)
        {
            uint8_t          subPat  = ((m_nPairs[blk][0]>0)<<1) + (m_nPairs[blk][1]>0);

            subPat = (subPat>0)?subPat-1:subPat;

            assert(subPat < 4);

            //block level transform type
            err = pCodedBlock->PutBits(pTTBLKVLC[type][2*subPat],pTTBLKVLC[type][2*subPat+1] );
            VC1_ENC_CHECK (err)
        }
        if (type == VC1_ENC_4x4_TRANSFORM)
        {
            uint8_t          subPat  = ((m_nPairs[blk][0]>0)<<3) + ((m_nPairs[blk][1]>0)<<2)+
                                     ((m_nPairs[blk][2]>0)<<1) +  (m_nPairs[blk][3]>0);
             //VC1_ENC_4x4_TRANSFORM
            err = pCodedBlock->PutBits(pSBP4x4Table[2*subPat],pSBP4x4Table[2*subPat+1] );
            VC1_ENC_CHECK (err)
        }
        else if (type != VC1_ENC_8x8_TRANSFORM &&
                (m_bMBTSType && blk!= m_uiFirstCodedBlock || !bVTS))
        {
            uint8_t          subPat  = ((m_nPairs[blk][0]>0)<<1) + (m_nPairs[blk][1]>0) - 1;
            //if MB level or frame level
            err = pCodedBlock->PutBits(SubPattern8x4_4x8VLC[2*subPat],SubPattern8x4_4x8VLC[2*subPat+1] );
            VC1_ENC_CHECK (err)
        }
        return err;

   }

template <class T>  inline
UMC::Status WriteMVDataPIntra(T* pCodedMB, const uint16_t* table)
{
    UMC::Status     ret     =   UMC::UMC_OK;
    bool            NotSkip =   (m_uiMBCBPCY != 0);
    uint8_t           index   =   37*NotSkip + 36 - 1;

    VC1_NULL_PTR  (pCodedMB)

    ret = pCodedMB->PutBits(table[2*index], table[2*index+1]);
    VC1_ENC_CHECK (ret)

    //printf("MVDiff index = %d\n", index);
    //printf("DMV_X  = %d, DMV_Y  = %d\n", 0, 0);

    return ret;
}
template <class T>  inline
UMC::Status WriteMVDataPIntra(T* pCodedMB, const uint16_t* table, uint32_t blockNum)
{
    UMC::Status     ret     =   UMC::UMC_OK;
    bool            NotSkip =   ((m_uiMBCBPCY &(1<<VC_ENC_PATTERN_POS(blockNum))) != 0);
    uint8_t           index   =   37*NotSkip + 36 - 1;

    VC1_NULL_PTR  (pCodedMB)

    ret = pCodedMB->PutBits(table[2*index], table[2*index+1]);
    VC1_ENC_CHECK (ret)

    //printf("MVDiff index = %d\n", index);
    //printf("DMV_X  = %d, DMV_Y  = %d\n", 0, 0);

    return ret;
}
template <class T>  inline
UMC::Status WriteMVDataInter(T* pCodedMB, const uint16_t* table, uint8_t rangeIndex, bool   NotSkip, bool bMVHalf, uint32_t blockNum = 0, bool bForward = true)
{
    UMC::Status     ret     =   UMC::UMC_OK;
    int16_t          index   =   0;
    int16_t          dx      =   m_dMV[bForward][blockNum].x;
    int16_t          dy      =   m_dMV[bForward][blockNum].y;
    bool            signX   =   (dx<0);
    bool            signY   =   (dy<0);
    uint8_t           limit   =   (bMVHalf)? VC1_ENC_HALF_MV_LIMIT : VC1_ENC_MV_LIMIT;


    VC1_NULL_PTR  (pCodedMB)

    dx = dx*(1 -2*signX);
    dy = dy*(1 -2*signY);

    if (bMVHalf)
    {
        dx = dx >> 1;
        dy = dy >> 1;
    }

    index = (dx < limit && dy < limit)? 6*MVSizeOffset[3*dy]+ MVSizeOffset[3*dx] - 1:35;

    if (index < 34)
    {
        int32_t diffX = dx - MVSizeOffset[3*dx+1];
        int32_t diffY = dy - MVSizeOffset[3*dy+1];
        uint8_t sizeX = MVSizeOffset[3*dx+2];
        uint8_t sizeY = MVSizeOffset[3*dy+2];

        diffX =  (diffX<<1)+signX;
        diffY =  (diffY<<1)+signY;

        if (bMVHalf)
        {
            sizeX -= sizeX>>3;
            sizeY -= sizeY>>3;
        }
        index+= 37*NotSkip;

        ret = pCodedMB->PutBits(table[2*index], table[2*index+1]);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(diffX, sizeX);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(diffY, sizeY);
        VC1_ENC_CHECK (ret);

#ifdef VC1_ENC_DEBUG_ON
        if(bMVHalf)
            pDebug->SetMVDiff(m_dMV[bForward][blockNum].x/2, m_dMV[bForward][blockNum].y/2,(!bForward),blockNum);
        else
            pDebug->SetMVDiff(m_dMV[bForward][blockNum].x, m_dMV[bForward][blockNum].y,(!bForward),blockNum);
#endif
    }
    else
    {
         // escape mode
        uint8_t sizeX = longMVLength[2*rangeIndex];
        uint8_t sizeY = longMVLength[2*rangeIndex+1];


        dx = ((1<<sizeX)-1)& (m_dMV[bForward][blockNum].x);
        dy = ((1<<sizeY)-1)& (m_dMV[bForward][blockNum].y);

        if (bMVHalf)
        {
            dx= dx>>1;
            dy= dy>>1;
        }

        index= 35 + 37*NotSkip - 1;

        ret = pCodedMB->PutBits(table[2*index], table[2*index+1]);
        VC1_ENC_CHECK (ret);

        if (bMVHalf)
        {
            sizeX -= sizeX>>3;
            sizeY -= sizeY>>3;
        }
        ret = pCodedMB->PutBits(dx, sizeX);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dy, sizeY);
        VC1_ENC_CHECK (ret);

#ifdef VC1_ENC_DEBUG_ON
    pDebug->SetMVDiff(dx, dy,(!bForward),blockNum);
#endif
    }
    return ret;
}
template <class T>  inline
UMC::Status WriteMVDataInterField1Ref(T* pCodedMB, uint8_t rangeIndex,
                                      sMVFieldInfo* pMVFieldInfo,
                                      bool bMVHalf,
                                      uint32_t blockNum=0,
                                      bool bForward=true )
{
    UMC::Status     ret     =   UMC::UMC_OK;
    int16_t          indexX  =   0;
    int16_t          indexY  =   0;
    int16_t          index   =   0;

    int16_t          dx      =   m_dMV[bForward][blockNum].x;
    int16_t          dy      =   m_dMV[bForward][blockNum].y;
    bool            signX   =   (dx<0);
    bool            signY   =   (dy<0);

    VC1_NULL_PTR  (pCodedMB)

    dx = (signX)? -dx:dx;
    dy = (signY)? -dy:dy;

    if (bMVHalf)
    {
        dx = dx>>1;
        dy = dy>>1;
    }

    index = (dx < pMVFieldInfo->limitX && dy < pMVFieldInfo->limitY)?
                9*(indexY = pMVFieldInfo->pMVSizeOffsetFieldIndexY[dy])+
                  (indexX = pMVFieldInfo->pMVSizeOffsetFieldIndexX[dx]) - 1:71;

    if (index < 71)
    {
        dx = dx - pMVFieldInfo->pMVSizeOffsetFieldX[indexX];
        dy = dy - pMVFieldInfo->pMVSizeOffsetFieldY[indexY];

        dx =  (dx<<1)+signX;
        dy =  (dy<<1)+signY;

        ret = pCodedMB->PutBits(pMVFieldInfo->pMVModeField1RefTable_VLC[2*index],
                                pMVFieldInfo->pMVModeField1RefTable_VLC[2*index+1]);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dx, indexX + pMVFieldInfo->bExtendedX);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dy, indexY + pMVFieldInfo->bExtendedY);
        VC1_ENC_CHECK (ret);

#ifdef VC1_ENC_DEBUG_ON
        if(bMVHalf)
            pDebug->SetMVDiff(m_dMV[bForward][blockNum].x/2, m_dMV[bForward][blockNum].y/2,(!bForward),blockNum);
        else
            pDebug->SetMVDiff(m_dMV[bForward][blockNum].x, m_dMV[bForward][blockNum].y,(!bForward),blockNum);
#endif
    }
    else
    {
         // escape mode
        index       = 71;

        indexX = longMVLength[2*rangeIndex];
        indexY = longMVLength[2*rangeIndex+1];

        dx = ((1<<indexX)-1)& ((signX)? -dx:dx);
        dy = ((1<<indexY)-1)& ((signY)? -dy:dy);

        ret = pCodedMB->PutBits(pMVFieldInfo->pMVModeField1RefTable_VLC[2*index],
                                pMVFieldInfo->pMVModeField1RefTable_VLC[2*index+1]);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dx, indexX );
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dy, indexY );
        VC1_ENC_CHECK (ret);

#ifdef VC1_ENC_DEBUG_ON
    pDebug->SetMVDiff(dx, dy,(!bForward),blockNum);
#endif
    }
    return ret;
}
template <class T>  inline
UMC::Status WriteMVDataInterField2Ref(T* pCodedMB, uint8_t rangeIndex,
                                      sMVFieldInfo* pMVFieldInfo,
                                      bool bMVHalf,
                                      uint32_t blockNum=0,
                                      bool bForward=true )
{
    UMC::Status     ret     =   UMC::UMC_OK;
    int16_t          indexX  =   0;
    int16_t          indexY  =   0;
    int16_t          index   =   0;

    int16_t          dx      =   m_dMV[bForward][blockNum].x;
    int16_t          dy      =   m_dMV[bForward][blockNum].y;
    bool            bNonDominant =   m_dMV[bForward][blockNum].bSecond;
    bool            signX   =   (dx<0);
    bool            signY   =   (dy<0);

    VC1_NULL_PTR  (pCodedMB)

    dx = (signX)? -dx:dx;
    dy = (signY)? -dy:dy;

    if (bMVHalf)
    {
        dx = dx>>1;
        dy = dy>>1;
    }
    index = (dx < pMVFieldInfo->limitX && dy < pMVFieldInfo->limitY)?
                9*(((indexY = pMVFieldInfo->pMVSizeOffsetFieldIndexY[dy])<<1) + bNonDominant)+
                    (indexX = pMVFieldInfo->pMVSizeOffsetFieldIndexX[dx]) - 1:125;

    if (index < 125)
    {
        dx = dx - pMVFieldInfo->pMVSizeOffsetFieldX[indexX];
        dy = dy - pMVFieldInfo->pMVSizeOffsetFieldY[indexY];

        dx =  (dx <<1)+signX;
        dy =  (dy <<1)+signY;

        ret = pCodedMB->PutBits(pMVFieldInfo->pMVModeField1RefTable_VLC[2*index],
                                pMVFieldInfo->pMVModeField1RefTable_VLC[2*index+1]);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dx, indexX + pMVFieldInfo->bExtendedX);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dy, indexY + pMVFieldInfo->bExtendedY);
        VC1_ENC_CHECK (ret);

#ifdef VC1_ENC_DEBUG_ON
        if(bMVHalf)
            pDebug->SetMVDiff(m_dMV[bForward][blockNum].x/2, m_dMV[bForward][blockNum].y/2,(!bForward),blockNum);
        else
            pDebug->SetMVDiff(m_dMV[bForward][blockNum].x, m_dMV[bForward][blockNum].y,(!bForward),blockNum);
#endif
    }
    else
    {
        // escape mode

        int32_t     y = ((((signY)? -dy:dy) - bNonDominant)<<1) + bNonDominant;

        index       = 125;


        indexX = longMVLength[2*rangeIndex];
        indexY = longMVLength[2*rangeIndex+1];

        dx = (int16_t)(((1<<indexX)-1)&  ((signX)? -dx: dx));
        dy = (int16_t)(((1<<indexY)-1)&  (y));

#ifdef VC1_ENC_DEBUG_ON
    if(bMVHalf)
        pDebug->SetMVDiff(dx, dy/2,(!bForward),blockNum);
    else
        pDebug->SetMVDiff(dx, dy,(!bForward),blockNum);
#endif

        ret = pCodedMB->PutBits(pMVFieldInfo->pMVModeField1RefTable_VLC[2*index],
                                pMVFieldInfo->pMVModeField1RefTable_VLC[2*index+1]);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dx, indexX );
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dy, indexY );
        VC1_ENC_CHECK (ret);

    }
    return ret;
}
template <class T>
UMC::Status   WriteVSTSMB (   T*           pCodedBlock,
                             const int16_t  (*pTTMBVLC)[4][6])
{
    UMC::Status err           = UMC::UMC_OK;
    uint32_t interPattern  = m_uiMBCBPCY & (~m_uiIntraPattern);
    int32_t blk;

    VC1_NULL_PTR(pCodedBlock)

    for(blk=0;blk<6;blk++)
    {
        if (interPattern & (1<<VC_ENC_PATTERN_POS(blk))) break;
    }

    m_uiFirstCodedBlock    = (uint8_t)blk;
    eTransformType MBtype  = m_tsType[blk];

#ifdef VC1_ENC_DEBUG_ON
    pDebug->SetVTSType(m_tsType);
#endif

    uint8_t          subPat  = ((m_nPairs[blk][0]>0)<<1) + (m_nPairs[blk][1]>0);
    subPat = (subPat>0)?subPat-1:subPat;

    err = pCodedBlock->PutBits(pTTMBVLC[m_bMBTSType][MBtype][2*subPat],pTTMBVLC[m_bMBTSType][MBtype][2*subPat+1] );
    return err;
}
};
 
/*------------------------------------------------------------------------------------------*/
inline UMC::Status  copyChromaBlockYV12 (uint8_t*  pURow,  uint8_t* pVRow,   uint32_t UVRowStep, 
                                         int16_t* pUBlock,int16_t* pVBlock,uint32_t UVBlockStep,
                                         uint32_t  nPos)
{
    mfxSize roiSize = {8, 8};
    _own_Copy8x8_16x16_8u16s(pURow + (nPos <<3), UVRowStep, pUBlock, UVBlockStep, roiSize);
    _own_Copy8x8_16x16_8u16s(pVRow + (nPos <<3), UVRowStep, pVBlock, UVBlockStep, roiSize);

    return UMC::UMC_OK;
}

inline UMC::Status  copyChromaBlockNV12 (uint8_t*  pUVRow,  uint8_t* /*pVRow*/,  uint32_t UVRowStep, 
                                        int16_t* pUBlock, int16_t* pVBlock,    uint32_t UVBlockStep,
                                        uint32_t  nPos)
{
    pUVRow += (nPos << 4);
    for (int i=0; i<8; i++)
    {
        int16_t* pU = pUBlock;
        int16_t* pV = pVBlock;
        uint8_t* pPlane = pUVRow;

        for (int j = 0; j <8; j++)
        {
            *(pU ++) = *(pPlane ++);
            *(pV ++) = *(pPlane ++);
        }
        pUBlock = (int16_t*)((uint8_t*)pUBlock + UVBlockStep);
        pVBlock = (int16_t*)((uint8_t*)pVBlock + UVBlockStep);
        pUVRow +=  UVRowStep;

    }
    return UMC::UMC_OK;
}
inline UMC::Status  copyDiffChromaBlockYV12 (uint8_t*  pURowS,  uint8_t* pVRowS,   uint32_t UVRowStepS, 
                                            uint8_t*   pURowR,  uint8_t* pVRowR,   uint32_t UVRowStepR, 
                                            int16_t*  pUBlock,int16_t* pVBlock,  uint32_t UVBlockStep,
                                            uint32_t  nPosS, uint32_t nPosR)
{
    
    _own_ippiGetDiff8x8_8u16s_C1  ( pURowS + (nPosS <<3), UVRowStepS, pURowR + (nPosR<<3), UVRowStepR,
                                    pUBlock, UVBlockStep,0,0,0,0);
    _own_ippiGetDiff8x8_8u16s_C1  ( pVRowS + (nPosS <<3), UVRowStepS, pVRowR + (nPosR<<3), UVRowStepR,
                                    pVBlock, UVBlockStep,0,0,0,0);

    return UMC::UMC_OK;
}
inline UMC::Status  copyDiffChromaBlockNV12 (uint8_t*   pUVRowS,   uint8_t* /*pVRowS*/,    uint32_t UVRowStepS, 
                                             uint8_t*   pUVRowR,   uint8_t* /*pVRowR*/,    uint32_t UVRowStepR, 
                                             int16_t*  pUBlock,   int16_t* pVBlock,      uint32_t UVBlockStep,
                                             uint32_t  nPosS, uint32_t nPosR)
{
    pUVRowS += (nPosS << 4);
    pUVRowR += (nPosR << 4);

    for (int i=0; i<8; i++)
    {
        int16_t* pU     = pUBlock;
        int16_t* pV     = pVBlock;
        uint8_t* pPlaneS = pUVRowS;
        uint8_t* pPlaneR = pUVRowR;

        for (int j = 0; j <8; j++)
        {
            *(pU ++) = *(pPlaneS ++) - *(pPlaneR ++);
            *(pV ++) = *(pPlaneS ++) - *(pPlaneR ++);
        }
        pUBlock = (int16_t*)((uint8_t*)pUBlock + UVBlockStep);
        pVBlock = (int16_t*)((uint8_t*)pVBlock + UVBlockStep);
        pUVRowS +=  UVRowStepS;
        pUVRowR +=  UVRowStepR;

    }
    return UMC::UMC_OK;
}
inline UMC::Status  copyBDiffChromaBlockYV12 (uint8_t*   pURowS,  uint8_t* pVRowS,   uint32_t UVRowStepS, 
                                              uint8_t*   pURowF,  uint8_t* pVRowF,   uint32_t UVRowStepF, 
                                              uint8_t*   pURowB,  uint8_t* pVRowB,   uint32_t UVRowStepB,  
                                              int16_t*  pUBlock,int16_t* pVBlock,  uint32_t UVBlockStep,
                                              uint32_t  nPosS, uint32_t nPosFB)
{

    ippiGetDiff8x8B_8u16s_C1  (pURowS  + (nPosS <<3), UVRowStepS,   pURowF  + (nPosFB <<3),   UVRowStepF, 0,
                                                                    pURowB  + (nPosFB <<3),   UVRowStepB, 0,
                                                                    pUBlock, UVBlockStep,0);

    ippiGetDiff8x8B_8u16s_C1  (pVRowS  + (nPosS <<3), UVRowStepS,   pVRowF  + (nPosFB <<3),   UVRowStepF, 0,
                                                                    pVRowB  + (nPosFB <<3),   UVRowStepB, 0,
                                                                    pVBlock, UVBlockStep,0);

    return UMC::UMC_OK;
}
inline UMC::Status  copyBDiffChromaBlockNV12 (uint8_t*   pUVRowS,   uint8_t* /*pVRowS*/,    uint32_t UVRowStepS, 
                                              uint8_t*   pUVRowF,   uint8_t* /*pVRowF*/,    uint32_t UVRowStepF,
                                              uint8_t*   pUVRowB,   uint8_t* /*pVRowB*/,    uint32_t UVRowStepB ,
                                              int16_t*  pUBlock,   int16_t* pVBlock,      uint32_t UVBlockStep,
                                              uint32_t  nPosS, uint32_t nPosFB)
{
    pUVRowS += (nPosS  << 4);
    pUVRowF += (nPosFB << 4);
    pUVRowB += (nPosFB << 4);

    for (int i=0; i<8; i++)
    {
        int16_t* pU     = pUBlock;
        int16_t* pV     = pVBlock;
        uint8_t* pPlaneS = pUVRowS;
        uint8_t* pPlaneF = pUVRowF;
        uint8_t* pPlaneB = pUVRowB;

        for (int j = 0; j <8; j++)
        {
            *(pU ++) = *(pPlaneS ++) - ((*(pPlaneF ++) + *(pPlaneB ++) + 1)>>1);
            *(pV ++) = *(pPlaneS ++) - ((*(pPlaneF ++) + *(pPlaneB ++) + 1)>>1);
        }
        pUBlock = (int16_t*)((uint8_t*)pUBlock + UVBlockStep);
        pVBlock = (int16_t*)((uint8_t*)pVBlock + UVBlockStep);
        pUVRowS +=  UVRowStepS;
        pUVRowF +=  UVRowStepF;
        pUVRowB +=  UVRowStepB;

    }
    return UMC::UMC_OK;
}
inline UMC::Status  pasteChromaBlockYV12 (uint8_t*  pURow,  uint8_t* pVRow,   uint32_t UVRowStep, 
                                         int16_t* pUBlock,int16_t* pVBlock,uint32_t UVBlockStep,
                                         uint32_t  nPos)
{
    mfxSize roiSize = {8, 8};
    _own_ippiConvert_16s8u_C1R(pUBlock, UVBlockStep,pURow + (nPos <<3), UVRowStep,roiSize);
    _own_ippiConvert_16s8u_C1R(pVBlock, UVBlockStep,pVRow + (nPos <<3), UVRowStep,roiSize);
    return UMC::UMC_OK;
}
inline UMC::Status  pasteChromaBlockNV12 (uint8_t*  pUVRow,  uint8_t* /*pVRow*/,   uint32_t UVRowStep, 
                                          int16_t* pUBlock, int16_t* pVBlock,    uint32_t UVBlockStep,
                                          uint32_t  nPos)
{
    pUVRow += (nPos << 4);
    for (int i=0; i<8; i++)
    {
        int16_t* pU = pUBlock;
        int16_t* pV = pVBlock;
        uint8_t* pPlane = pUVRow;

        for (int j = 0; j <8; j++)
        {
            int16_t U = *(pU ++);
            int16_t V = *(pV ++);

            U = (U < 0)  ? 0   : U;
            U = (U > 255)? 255 : U;
            V = (V < 0)  ? 0   : V;
            V = (V >255) ? 255 : V;

            *(pPlane ++) = (uint8_t)U;
            *(pPlane ++) = (uint8_t)V;
        }
        pUBlock = (int16_t*)((uint8_t*)pUBlock + UVBlockStep);
        pVBlock = (int16_t*)((uint8_t*)pVBlock + UVBlockStep);
        pUVRow +=  UVRowStep;

    }
    return UMC::UMC_OK;
}
inline UMC::Status  pasteSumChromaBlockYV12 (uint8_t*  pURowRef,  uint8_t* pVRowRef,   uint32_t UVRowStepRef, 
                                             uint8_t*  pURowDst,  uint8_t* pVRowDst,   uint32_t UVRowStepDst,
                                             int16_t* pUBlock,   int16_t* pVBlock,   uint32_t UVBlockStep,
                                             uint32_t  nPosRef,   uint32_t  nPosDst)
{
    _own_Add8x8_8u16s(pURowRef + (nPosRef <<3), UVRowStepRef,pUBlock,UVBlockStep, pURowDst + (nPosDst <<3), UVRowStepDst, 0, 0);
    _own_Add8x8_8u16s(pVRowRef + (nPosRef <<3), UVRowStepRef,pVBlock,UVBlockStep, pVRowDst + (nPosDst <<3), UVRowStepDst, 0, 0);

   return UMC::UMC_OK;
}
inline UMC::Status  pasteSumChromaBlockNV12 (uint8_t*  pUVRowRef,  uint8_t* /*pVRowRef*/,   uint32_t UVRowStepRef, 
                                             uint8_t*  pUVRowDst,  uint8_t* /*pVRowDst*/,   uint32_t UVRowStepDst,
                                             int16_t* pUBlock,   int16_t* pVBlock,   uint32_t UVBlockStep,
                                             uint32_t  nPosRef,   uint32_t  nPosDst)
{
    pUVRowDst += (nPosDst << 4);
    pUVRowRef += (nPosRef << 4);

    for (int i=0; i<8; i++)
    {
        int16_t* pU = pUBlock;
        int16_t* pV = pVBlock;
        uint8_t* pPlane    = pUVRowDst;
        uint8_t* pPlaneRef = pUVRowRef;


        for (int j = 0; j <8; j++)
        {
            int16_t U = *(pU ++) + *(pPlaneRef++);
            int16_t V = *(pV ++) + *(pPlaneRef++);

            U = (U < 0)  ? 0   : U;
            U = (U > 255)? 255 : U;
            V = (V < 0)  ? 0   : V;
            V = (V >255) ? 255 : V;

            *(pPlane ++) = (uint8_t)U;
            *(pPlane ++) = (uint8_t)V;
        }
        pUBlock = (int16_t*)((uint8_t*)pUBlock + UVBlockStep);
        pVBlock = (int16_t*)((uint8_t*)pVBlock + UVBlockStep);
        pUVRowDst +=  UVRowStepDst;
        pUVRowRef +=  UVRowStepRef;
    }
    return UMC::UMC_OK;
}
inline UMC::Status  pasteBSumChromaBlockYV12 (uint8_t*  pURowRefF,  uint8_t* pVRowRefF, uint32_t UVRowStepRefF,
                                               uint8_t*  pURowRefB,  uint8_t* pVRowRefB, uint32_t UVRowStepRefB, 
                                               uint8_t*  pURowDst,   uint8_t* pVRowDst,   uint32_t UVRowStepDst,
                                               int16_t* pUBlock,    int16_t* pVBlock,   uint32_t UVBlockStep,
                                               uint32_t  nPosRef,    uint32_t  nPosDst)
{
    ippiMC8x8B_8u_C1     (  pURowRefF + (nPosRef <<3),UVRowStepRefF, 0,
                            pURowRefB + (nPosRef <<3),UVRowStepRefB, 0,
                            pUBlock, UVBlockStep,
                            pURowDst+(nPosDst <<3), UVRowStepDst,  0);

    ippiMC8x8B_8u_C1     (  pVRowRefF + (nPosRef <<3),UVRowStepRefF, 0,
                            pVRowRefB + (nPosRef <<3),UVRowStepRefB, 0,
                            pVBlock, UVBlockStep,
                            pVRowDst+(nPosDst <<3), UVRowStepDst,  0);

    return UMC::UMC_OK;
}
inline UMC::Status  pasteBSumChromaBlockNV12 (uint8_t*  pUVRowRefF, uint8_t* /*pVRowRefF*/,   uint32_t UVRowStepRefF, 
                                              uint8_t*  pUVRowRefB, uint8_t* /*pVRowRefF*/,   uint32_t UVRowStepRefB, 
                                              uint8_t*  pUVRowDst,  uint8_t* /*pVRowDst*/,   uint32_t UVRowStepDst,
                                              int16_t* pUBlock,    int16_t* pVBlock,   uint32_t UVBlockStep,
                                              uint32_t  nPosRef,    uint32_t  nPosDst)
{
    pUVRowDst += (nPosDst << 4);
    pUVRowRefF += (nPosRef << 4);
    pUVRowRefB += (nPosRef << 4);

    for (int i=0; i<8; i++)
    {
        int16_t* pU = pUBlock;
        int16_t* pV = pVBlock;
        uint8_t* pPlane    = pUVRowDst;
        uint8_t* pPlaneRefF = pUVRowRefF;
        uint8_t* pPlaneRefB = pUVRowRefB;


        for (int j = 0; j <8; j++)
        {
            int16_t U = *(pU ++) + ((*(pPlaneRefF++) + *(pPlaneRefB++) + 1)>>1);
            int16_t V = *(pV ++) + ((*(pPlaneRefF++) + *(pPlaneRefB++) + 1)>>1);

            U = (U < 0)  ? 0   : U;
            U = (U > 255)? 255 : U;
            V = (V < 0)  ? 0   : V;
            V = (V >255) ? 255 : V;

            *(pPlane ++) = (uint8_t)U;
            *(pPlane ++) = (uint8_t)V;
        }
        pUBlock = (int16_t*)((uint8_t*)pUBlock + UVBlockStep);
        pVBlock = (int16_t*)((uint8_t*)pVBlock + UVBlockStep);
        pUVRowDst +=  UVRowStepDst;
        pUVRowRefF +=  UVRowStepRefF;
        pUVRowRefB +=  UVRowStepRefB;

    }
    return UMC::UMC_OK;
}
inline UMC::Status  pasteSumSkipChromaBlockYV12 (   uint8_t*  pURowRef,  uint8_t* pVRowRef,   uint32_t UVRowStepRef, 
                                                    uint8_t*  pURowDst,  uint8_t* pVRowDst,   uint32_t UVRowStepDst,                                             
                                                    uint32_t  nPosRef,   uint32_t  nPosDst)
{
    mfxSize roiSize = {8, 8};
    ippiCopy_8u_C1R(pURowRef + (nPosRef <<3), UVRowStepRef, pURowDst + (nPosDst <<3), UVRowStepDst,roiSize);
    ippiCopy_8u_C1R(pVRowRef + (nPosRef <<3), UVRowStepRef, pVRowDst + (nPosDst <<3), UVRowStepDst,roiSize);

    return UMC::UMC_OK;
}
inline UMC::Status  pasteSumSkipChromaBlockNV12 (   uint8_t*  pUVRowRef,  uint8_t* /*pVRowRef*/,   uint32_t UVRowStepRef, 
                                                    uint8_t*  pUVRowDst,  uint8_t* /*pVRowDst*/,   uint32_t UVRowStepDst,
                                                    uint32_t  nPosRef,   uint32_t  nPosDst)
{
    mfxSize roiSize = {16, 8};

    pUVRowDst += (nPosDst << 4);
    pUVRowRef += (nPosRef << 4);

    ippiCopy_8u_C1R(pUVRowRef, UVRowStepRef, pUVRowDst, UVRowStepDst,roiSize);

    return UMC::UMC_OK;
}
inline UMC::Status  pasteBSumSkipChromaBlockYV12 (  uint8_t*  pURowRefF,  uint8_t* pVRowRefF,   uint32_t UVRowStepRefF, 
                                                    uint8_t*  pURowRefB,  uint8_t* pVRowRefB,   uint32_t UVRowStepRefB,
                                                    uint8_t*  pURowDst,   uint8_t* pVRowDst,    uint32_t UVRowStepDst,                                             
                                                    uint32_t  nPosRef,   uint32_t  nPosDst)
{
    mfxSize roiSize = {8, 8};
    ippiAverage8x8_8u_C1R(  pURowRefF + (nPosRef <<3), UVRowStepRefF,
                            pURowRefB + (nPosRef <<3), UVRowStepRefB,
                            pURowDst  + (nPosDst <<3), UVRowStepDst);
    ippiAverage8x8_8u_C1R(  pVRowRefF + (nPosRef <<3), UVRowStepRefF,
                            pVRowRefB + (nPosRef <<3), UVRowStepRefB,
                            pVRowDst  + (nPosDst <<3), UVRowStepDst);
 
    return UMC::UMC_OK;
}
inline UMC::Status  pasteBSumSkipChromaBlockNV12 (   uint8_t*  pUVRowRefF,  uint8_t* /*pVRowRefF*/,   uint32_t UVRowStepRefF, 
                                                     uint8_t*  pUVRowRefB,  uint8_t* /*pVRowRefB*/,   uint32_t UVRowStepRefB, 
                                                     uint8_t*  pUVRowDst,   uint8_t* /*pVRowDst*/,   uint32_t UVRowStepDst,
                                                     uint32_t  nPosRef,     uint32_t  nPosDst)
{
     //ippiAverage16x8_8u_C1R

    pUVRowDst += (nPosDst << 4);
    pUVRowRefF += (nPosRef << 4);
    pUVRowRefB += (nPosRef << 4);

    for (int i=0; i<8; i++)
    {
        uint8_t* pPlane    = pUVRowDst;
        uint8_t* pPlaneRefF = pUVRowRefF;
        uint8_t* pPlaneRefB = pUVRowRefB;

        for (int j = 0; j < 16; j++)
        {
            *(pPlane ++) = ((*(pPlaneRefF++) + *(pPlaneRefB++) + 1)>>1);
        }
        pUVRowDst +=  UVRowStepDst;
        pUVRowRefF +=  UVRowStepRefF;
        pUVRowRefB +=  UVRowStepRefB;

    }

    return UMC::UMC_OK;
}

class VC1EncoderMBData
{
public:

    int16_t*                     m_pBlock[VC1_ENC_NUMBER_OF_BLOCKS];
    uint32_t                      m_uiBlockStep[VC1_ENC_NUMBER_OF_BLOCKS];

public:

   UMC::Status InitBlocks(int16_t* pBuffer);
   void Reset()
   {
       assert(m_pBlock[0]!=0);
       memset(m_pBlock[0],0,VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS*sizeof(int16_t));
   }


   VC1EncoderMBData()
   {
        memset(m_pBlock,        0,sizeof(int16_t*)*VC1_ENC_NUMBER_OF_BLOCKS);
        memset(m_uiBlockStep,   0,sizeof(uint32_t) *VC1_ENC_NUMBER_OF_BLOCKS);

   }
   
   virtual UMC::Status CopyMBProg(uint8_t* pY, uint32_t stepY, uint8_t* pU, uint8_t* pV, uint32_t stepUV, uint32_t nPos)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;


        /* luma block */
        mfxSize roiSize = {16, 16};
       _own_Copy8x8_16x16_8u16s(pY + (nPos <<4), stepY, m_pBlock[0], m_uiBlockStep[0], roiSize); 
        /* chroma block*/
       copyChromaBlockYV12(pU,pV,stepUV,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPos);       

       return UMC::UMC_OK;
   }
   virtual UMC::Status CopyDiffMBProg(  uint8_t*   pYS,  uint32_t stepYS, 
                                        uint8_t*   pUS,  uint8_t* pVS,   uint32_t stepUVS, 
                                        uint8_t*   pYR,  uint32_t stepYR,
                                        uint8_t*   pUR,  uint8_t* pVR,   uint32_t stepUVR,                                 
                                        uint32_t  nPosS, uint32_t nPosR)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;

       
       _own_ippiGetDiff16x16_8u16s_C1(  pYS + (nPosS <<4), stepYS,
                                        pYR + (nPosR <<4), stepYR,
                                        m_pBlock[0], m_uiBlockStep[0], 0,0,0,0);

       copyDiffChromaBlockYV12(pUS,pVS,stepUVS,pUR,pVR,stepUVR,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosS,nPosR);      
       

       return UMC::UMC_OK;
   }
   virtual UMC::Status CopyBDiffMBProg( uint8_t*   pYS,  uint32_t stepYS, 
                                        uint8_t*   pUS,  uint8_t* pVS,   uint32_t stepUVS, 
                                        uint8_t*   pYF,  uint32_t stepYF,
                                        uint8_t*   pUF,  uint8_t* pVF,   uint32_t stepUVF,
                                        uint8_t*   pYB,  uint32_t stepYB,
                                        uint8_t*   pUB,  uint8_t* pVB,   uint32_t stepUVB,
                                        uint32_t  nPosS, uint32_t nPosFB)

   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;

       ippiGetDiff16x16B_8u16s_C1(pYS + (nPosS <<4),stepYS, pYF + (nPosFB <<4), stepYF, 0, pYB + (nPosFB <<4), stepYB, 0, m_pBlock[0], m_uiBlockStep[0],0);                                 

       copyBDiffChromaBlockYV12(pUS,pVS,stepUVS,pUF,pVF,stepUVF,pUB,pVB,stepUVB,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosS,nPosFB);   

       return UMC::UMC_OK;
   }
   virtual UMC::Status PasteMBProg(uint8_t* pY, uint32_t stepY, uint8_t* pU, uint8_t* pV, uint32_t stepUV, uint32_t nPos)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;
 
       /* luma block */
       mfxSize roiSize = {16, 16};
       _own_ippiConvert_16s8u_C1R(m_pBlock[0], m_uiBlockStep[0],pY + (nPos <<4), stepY,roiSize);
       /* chroma block */
       pasteChromaBlockYV12(pU,pV,stepUV,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPos);       

       return UMC::UMC_OK;
   }

   virtual UMC::Status PasteSumMBProg(  uint8_t* pYRef, uint32_t stepYRef, 
                                        uint8_t* pURef, uint8_t* pVRef, uint32_t stepUVRef, 
                                        uint8_t* pYDst, uint32_t stepYDst, 
                                        uint8_t* pUDst, uint8_t* pVDst, uint32_t stepUVDst,                             
                                        uint32_t nPosRef, uint32_t  nPosDst)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;

       /* luma block */
       _own_Add16x16_8u16s(pYRef + (nPosRef <<4),stepYRef,m_pBlock[0],m_uiBlockStep[0],
                           pYDst + (nPosDst <<4) ,stepYDst, 0, 0);
      
       /* chroma block */
       pasteSumChromaBlockYV12(pURef,pVRef,stepUVRef,pUDst,pVDst,stepUVDst, m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosRef, nPosDst);       

       return UMC::UMC_OK;
   }
   virtual UMC::Status PasteBSumMBProg( uint8_t* pYRefF, uint32_t stepYRefF, 
                                        uint8_t* pURefF, uint8_t* pVRefF, uint32_t stepUVRefF, 
                                        uint8_t* pYRefB, uint32_t stepYRefB, 
                                        uint8_t* pURefB, uint8_t* pVRefB, uint32_t stepUVRefB, 
                                        uint8_t* pYDst, uint32_t stepYDst, 
                                        uint8_t* pUDst, uint8_t* pVDst, uint32_t stepUVDst,                             
                                        uint32_t nPosRef, uint32_t  nPosDst)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;

       /* luma block */
       ippiMC16x16B_8u_C1   (pYRefF + (nPosRef <<4),stepYRefF, 0,
                             pYRefB + (nPosRef <<4),stepYRefB, 0,
                             m_pBlock[0],m_uiBlockStep[0],
                             pYDst + (nPosDst <<4) ,stepYDst, 0);

       /* chroma block */
       pasteBSumChromaBlockYV12(pURefF,pVRefF,stepUVRefF,pURefB,pVRefB,stepUVRefB, pUDst,pVDst,stepUVDst, m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosRef, nPosDst);       

       return UMC::UMC_OK;
   }

   virtual UMC::Status PasteSumSkipMBProg(  uint8_t* pYRef, uint32_t stepYRef, 
                                            uint8_t* pURef, uint8_t* pVRef, uint32_t stepUVRef, 
                                            uint8_t* pYDst, uint32_t stepYDst, 
                                            uint8_t* pUDst, uint8_t* pVDst, uint32_t stepUVDst,                             
                                            uint32_t nPosRef, uint32_t  nPosDst)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;

       /* luma block */
       mfxSize roiSize = {16, 16};
       ippiCopy_8u_C1R(pYRef + (nPosRef <<4), stepYRef, pYDst + (nPosDst <<4) ,stepYDst,roiSize);
       
       /* chroma block */
       pasteSumSkipChromaBlockYV12(pURef,pVRef,stepUVRef,pUDst,pVDst,stepUVDst,nPosRef, nPosDst);      

       return UMC::UMC_OK;
   }
   virtual UMC::Status PasteBSumSkipMBProg( uint8_t* pYRefF, uint32_t stepYRefF, 
                                            uint8_t* pURefF, uint8_t* pVRefF, uint32_t stepUVRefF,
                                            uint8_t* pYRefB, uint32_t stepYRefB, 
                                            uint8_t* pURefB, uint8_t* pVRefB, uint32_t stepUVRefB,
                                            uint8_t* pYDst, uint32_t stepYDst, 
                                            uint8_t* pUDst, uint8_t* pVDst, uint32_t stepUVDst,                             
                                            uint32_t nPosRef, uint32_t  nPosDst)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;

       /* luma block */

       ippiAverage16x16_8u_C1R(pYRefF + (nPosRef <<4), stepYRefF, pYRefB + (nPosRef <<4),stepYRefB, pYDst + (nPosDst <<4),  stepYDst);
 
       /* chroma block */
       pasteBSumSkipChromaBlockYV12(pURefF,pVRefF,stepUVRefF,pURefB,pVRefB,stepUVRefB,pUDst,pVDst,stepUVDst,nPosRef, nPosDst);      

       return UMC::UMC_OK;
   }

   inline static bool IsCodedBlock(uint8_t MBPattern, int32_t blk)
   {
        return ((MBPattern &(1 << (5 - blk)))!=0);
   }
};

class VC1EncoderMBDataNV12: public VC1EncoderMBData
{
public:

    virtual UMC::Status CopyMBProg(uint8_t* pY, uint32_t stepY, uint8_t* pU, uint8_t* pV, uint32_t stepUV, uint32_t nPos)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;


        /* luma block */
        mfxSize roiSize = {16, 16};
        _own_Copy8x8_16x16_8u16s(pY + (nPos <<4), stepY, m_pBlock[0], m_uiBlockStep[0], roiSize); 
        /* chroma block*/
        copyChromaBlockNV12(pU,pV,stepUV,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPos);       

        return UMC::UMC_OK;
    }
    virtual UMC::Status CopyDiffMBProg( uint8_t*   pYS,  uint32_t stepYS, 
                                        uint8_t*   pUS,  uint8_t* pVS,   uint32_t stepUVS, 
                                        uint8_t*   pYR,  uint32_t stepYR,
                                        uint8_t*   pUR,  uint8_t* pVR,   uint32_t stepUVR,                                 
                                        uint32_t  nPosS, uint32_t nPosR)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;


        _own_ippiGetDiff16x16_8u16s_C1( pYS + (nPosS <<4), stepYS,
                                        pYR + (nPosR <<4), stepYR,
                                        m_pBlock[0], m_uiBlockStep[0], 0,0,0,0);

        copyDiffChromaBlockNV12(pUS,pVS,stepUVS,pUR,pVR,stepUVR,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosS,nPosR);      


        return UMC::UMC_OK;
    }
    virtual UMC::Status CopyBDiffMBProg(uint8_t*   pYS,  uint32_t stepYS, 
                                        uint8_t*   pUS,  uint8_t* pVS,   uint32_t stepUVS, 
                                        uint8_t*   pYF,  uint32_t stepYF,
                                        uint8_t*   pUF,  uint8_t* pVF,   uint32_t stepUVF,
                                        uint8_t*   pYB,  uint32_t stepYB,
                                        uint8_t*   pUB,  uint8_t* pVB,   uint32_t stepUVB,
                                        uint32_t  nPosS, uint32_t nPosFB)

    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;

        ippiGetDiff16x16B_8u16s_C1(pYS + (nPosS <<4),stepYS, pYF + (nPosFB <<4), stepYF, 0, pYB + (nPosFB <<4), stepYB, 0, m_pBlock[0], m_uiBlockStep[0],0);                                 

        copyBDiffChromaBlockNV12(pUS,pVS,stepUVS,pUF,pVF,stepUVF,pUB,pVB,stepUVB,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosS,nPosFB);   

        return UMC::UMC_OK;
    }
    virtual UMC::Status PasteMBProg(uint8_t* pY, uint32_t stepY, uint8_t* pU, uint8_t* pV, uint32_t stepUV, uint32_t nPos)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;

        /* luma block */
        mfxSize roiSize = {16, 16};
        _own_ippiConvert_16s8u_C1R(m_pBlock[0], m_uiBlockStep[0],pY + (nPos <<4), stepY,roiSize);
        /* chroma block */
        pasteChromaBlockNV12(pU,pV,stepUV,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPos);       

        return UMC::UMC_OK;
    }

    virtual UMC::Status PasteSumMBProg( uint8_t* pYRef, uint32_t stepYRef, 
                                        uint8_t* pURef, uint8_t* pVRef, uint32_t stepUVRef, 
                                        uint8_t* pYDst, uint32_t stepYDst, 
                                        uint8_t* pUDst, uint8_t* pVDst, uint32_t stepUVDst,                             
                                        uint32_t nPosRef, uint32_t  nPosDst)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;

        /* luma block */
        _own_Add16x16_8u16s(pYRef + (nPosRef <<4),stepYRef,m_pBlock[0],m_uiBlockStep[0], pYDst + (nPosDst <<4) ,stepYDst, 0, 0);

        /* chroma block */
        pasteSumChromaBlockNV12(pURef,pVRef,stepUVRef,pUDst,pVDst,stepUVDst, m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosRef, nPosDst);       

        return UMC::UMC_OK;
    }
    virtual UMC::Status PasteBSumMBProg(uint8_t* pYRefF, uint32_t stepYRefF, 
                                        uint8_t* pURefF, uint8_t* pVRefF, uint32_t stepUVRefF, 
                                        uint8_t* pYRefB, uint32_t stepYRefB, 
                                        uint8_t* pURefB, uint8_t* pVRefB, uint32_t stepUVRefB, 
                                        uint8_t* pYDst, uint32_t stepYDst, 
                                        uint8_t* pUDst, uint8_t* pVDst, uint32_t stepUVDst,                             
                                        uint32_t nPosRef, uint32_t  nPosDst)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;

        /* luma block */
        ippiMC16x16B_8u_C1   (  pYRefF + (nPosRef <<4),stepYRefF, 0,
                                pYRefB + (nPosRef <<4),stepYRefB, 0,
                                m_pBlock[0],m_uiBlockStep[0],
                                pYDst + (nPosDst <<4) ,stepYDst, 0);

        /* chroma block */
        pasteBSumChromaBlockNV12(pURefF,pVRefF,stepUVRefF,pURefB,pVRefB,stepUVRefB, pUDst,pVDst,stepUVDst, m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosRef, nPosDst);       

        return UMC::UMC_OK;
    }

    virtual UMC::Status PasteSumSkipMBProg( uint8_t* pYRef, uint32_t stepYRef, 
                                            uint8_t* pURef, uint8_t* pVRef, uint32_t stepUVRef, 
                                            uint8_t* pYDst, uint32_t stepYDst, 
                                            uint8_t* pUDst, uint8_t* pVDst, uint32_t stepUVDst,                             
                                            uint32_t nPosRef, uint32_t  nPosDst)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;

        /* luma block */
        mfxSize roiSize = {16, 16};
        ippiCopy_8u_C1R(pYRef + (nPosRef <<4), stepYRef, pYDst + (nPosDst <<4) ,stepYDst,roiSize);

        /* chroma block */
        pasteSumSkipChromaBlockNV12(pURef,pVRef,stepUVRef,pUDst,pVDst,stepUVDst,nPosRef, nPosDst);      

        return UMC::UMC_OK;
    }
    virtual UMC::Status PasteBSumSkipMBProg(    uint8_t* pYRefF, uint32_t stepYRefF, 
                                                uint8_t* pURefF, uint8_t* pVRefF, uint32_t stepUVRefF,
                                                uint8_t* pYRefB, uint32_t stepYRefB, 
                                                uint8_t* pURefB, uint8_t* pVRefB, uint32_t stepUVRefB,
                                                uint8_t* pYDst, uint32_t stepYDst, 
                                                uint8_t* pUDst, uint8_t* pVDst, uint32_t stepUVDst,                             
                                                uint32_t nPosRef, uint32_t  nPosDst)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;

        /* luma block */

        ippiAverage16x16_8u_C1R(pYRefF + (nPosRef <<4), stepYRefF, pYRefB + (nPosRef <<4),stepYRefB, pYDst + (nPosDst <<4),  stepYDst);

        /* chroma block */
        pasteBSumSkipChromaBlockNV12(pURefF,pVRefF,stepUVRefF,pURefB,pVRefB,stepUVRefB,pUDst,pVDst,stepUVDst,nPosRef, nPosDst);      

        return UMC::UMC_OK;
    }
};
struct MBEdges
{
    unsigned YExHor:4;
    unsigned YExVer:4;
    unsigned YInHor:4;
    unsigned YInVer:4;

    unsigned YAdUpp:4;
    unsigned YAdBot:4;
    unsigned YAdLef:4;
    unsigned YAdRig:4;

    unsigned UExHor:4;
    unsigned UExVer:4;
    unsigned UAdHor:4;
    unsigned UAdVer:4;

    unsigned VExHor:4;
    unsigned VExVer:4;
    unsigned VAdHor:4;
    unsigned VAdVer:4;
};
class VC1EncoderMBInfo
{

private:

    bool            m_bIntra;
    uint8_t           m_uiMBPattern;

    //for P frames
    sCoordinate     m_MV [2][6];
    uint8_t           m_uiIntraPattern;
    uint32_t          m_uiBlocksPattern;
    MBEdges         m_sMBEdges;
    uint32_t          m_uiVSTPattern;

    //for I frames, smoothing
    bool            m_uiOverlap;


public:
    VC1EncoderMBInfo():
        m_bIntra (true),
        m_uiMBPattern(0),
        m_uiIntraPattern(0),
        m_uiBlocksPattern(0),
        m_uiVSTPattern(0),
        m_uiOverlap(false)
    {

        memset(m_MV[0],       0, sizeof(sCoordinate)*6);
        memset(m_MV[1],       0, sizeof(sCoordinate)*6);

    }
    inline void SetEdgesIntra(bool top, bool left)
    {
        m_sMBEdges.YExHor = m_sMBEdges.UExHor = m_sMBEdges.VExHor = 0;
        m_sMBEdges.YExVer = m_sMBEdges.UExVer = m_sMBEdges.VExVer = 0;
        m_sMBEdges.YInHor = m_sMBEdges.YInVer =  0;

        m_sMBEdges.YAdUpp=m_sMBEdges.YAdBot=m_sMBEdges.YAdLef=m_sMBEdges.YAdRig=
        m_sMBEdges.UAdHor=m_sMBEdges.UAdVer=m_sMBEdges.VAdHor=m_sMBEdges.VAdVer = 0xff;

        if (top)
        {
            m_sMBEdges.YExHor = m_sMBEdges.UExHor = m_sMBEdges.VExHor = 0xff;
        }
        if (left)
        {
            m_sMBEdges.YExVer = m_sMBEdges.UExVer = m_sMBEdges.VExVer =  0xff;
        }
    }
    inline void SetInternalBlockEdge(uint8_t YFlagUp, uint8_t YFlagBot, uint8_t UFlagH, uint8_t VFlagH,
                                     uint8_t YFlagL,  uint8_t YFlagR,   uint8_t UFlagV, uint8_t VFlagV)
    {
        m_sMBEdges.YAdUpp = YFlagUp  & 0x0F;
        m_sMBEdges.YAdBot = YFlagBot & 0x0F;
        m_sMBEdges.UAdHor = UFlagH   & 0x0F;
        m_sMBEdges.VAdHor = VFlagH   & 0x0F;

        m_sMBEdges.YAdLef = YFlagL   & 0x0F;
        m_sMBEdges.YAdRig = YFlagR   & 0x0F;
        m_sMBEdges.UAdVer = UFlagV   & 0x0F;
        m_sMBEdges.VAdVer = VFlagV   & 0x0F;

    }
    inline void SetExternalEdgeVer(uint8_t YFlag, uint8_t UFlag, uint8_t VFlag)
    {
        m_sMBEdges.YExVer = YFlag & 0x0F;
        m_sMBEdges.UExVer = UFlag & 0x0F;
        m_sMBEdges.VExVer = VFlag & 0x0F;
    }
    inline void SetExternalEdgeHor(uint8_t YFlag, uint8_t UFlag, uint8_t VFlag)
    {
        m_sMBEdges.YExHor = YFlag & 0x0F;
        m_sMBEdges.UExHor = UFlag & 0x0F;
        m_sMBEdges.VExHor = VFlag & 0x0F;
    }
    inline void SetInternalEdge(uint8_t YFlagV, uint8_t YFlagH)
    {
        m_sMBEdges.YInVer = YFlagV & 0x0F;
        m_sMBEdges.YInHor = YFlagH & 0x0F;
    }
    inline uint32_t GetLumaExHorEdge()
    {
        return m_sMBEdges.YExHor;
    }
    inline uint32_t GetLumaExVerEdge()
    {
        return m_sMBEdges.YExVer;
    }
    inline uint32_t GetLumaInHorEdge()
    {
        return m_sMBEdges.YInHor;
    }
    inline uint32_t GetLumaInVerEdge()
    {
        return m_sMBEdges.YInVer;
    }
    inline uint32_t GetLumaAdUppEdge()
    {
        return m_sMBEdges.YAdUpp;
    }
    inline uint32_t GetLumaAdBotEdge()
    {
        return m_sMBEdges.YAdBot;
    }
    inline uint32_t GetLumaAdLefEdge()
    {
        return m_sMBEdges.YAdLef;
    }
    inline uint32_t GetLumaAdRigEdge()
    {
        return m_sMBEdges.YAdRig;
    }
    inline uint32_t GetUExHorEdge()
    {
        return m_sMBEdges.UExHor;
    }
    inline uint32_t GetVExHorEdge()
    {
        return m_sMBEdges.VExHor;
    }
    inline uint32_t GetUExVerEdge()
    {
        return m_sMBEdges.UExVer;
    }
    inline uint32_t GetVExVerEdge()
    {
        return m_sMBEdges.VExVer;
    }
    inline uint32_t GetVAdVerEdge()
    {
        return m_sMBEdges.VAdVer;
    }
   inline uint32_t GetUAdHorEdge()
    {
        return m_sMBEdges.UAdHor;
    }
    inline uint32_t GetVAdHorEdge()
    {
        return m_sMBEdges.VAdHor;
    }
    inline uint32_t GetUAdVerEdge()
    {
        return m_sMBEdges.UAdVer;
    }

    inline  void SetBlocksPattern(uint32_t blocksPattern)
    {
        m_uiBlocksPattern = blocksPattern;
    }
    inline bool isCoded(int32_t blk, int32_t subblk)
    {
        return ((m_uiBlocksPattern & (1<<VC_ENC_PATTERN_POS1(blk, subblk)))!=0);
    }

    inline void Init( bool intra)
    {
         m_bIntra =  intra;

         SetMVNULL();

         if (intra)
         {
            m_uiIntraPattern = VC1_ENC_PAT_MASK_MB;
         }
         else
         {
            m_uiIntraPattern = 0;
         }
    }
    inline bool isIntra()
    {
        return  (m_bIntra);
    }
    inline bool isIntra(int32_t i)
    {
        return  ((m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(i)))!=0);
    }

    inline bool GetLumaMV(sCoordinate *mv, bool bForward=true)
    {

        switch(m_uiIntraPattern >> 2)
        {
        case 0x0:
            mv->x = median4(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][2].x,m_MV[bForward][3].x );
            mv->y = median4(m_MV[bForward][0].y ,m_MV[bForward][1].y ,m_MV[bForward][2].y ,m_MV[bForward][3].y );
            return true;
        case 0x8: //1000
            mv->x = median3(m_MV[bForward][1].x , m_MV[bForward][2].x , m_MV[bForward][3].x );
            mv->y = median3(m_MV[bForward][1].y , m_MV[bForward][2].y , m_MV[bForward][3].y );
            return true;
        case 0x4: //100
            mv->x = median3(m_MV[bForward][0].x , m_MV[bForward][2].x , m_MV[bForward][3].x );
            mv->y = median3(m_MV[bForward][0].y , m_MV[bForward][2].y , m_MV[bForward][3].y );
            return true;
        case 0x2://10
            mv->x = median3(m_MV[bForward][0].x , m_MV[bForward][1].x , m_MV[bForward][3].x );
            mv->y = median3(m_MV[bForward][0].y , m_MV[bForward][1].y , m_MV[bForward][3].y );
            return true;
        case 0x1:
            mv->x = median3(m_MV[bForward][0].x , m_MV[bForward][1].x , m_MV[bForward][2].x );
            mv->y = median3(m_MV[bForward][0].y , m_MV[bForward][1].y , m_MV[bForward][2].y );
            return true;
        case 0xC:
        case 0xA:
        case 0x9:
        case 0x6:
        case 0x5:
        case 0x3:
            mv->x = (m_MV[bForward][0].x + m_MV[bForward][1].x + m_MV[bForward][2].x + m_MV[bForward][3].x)/2;
            mv->y = (m_MV[bForward][0].y + m_MV[bForward][1].y + m_MV[bForward][2].y + m_MV[bForward][3].y)/2;
            return true;
        default:
            mv->x = 0;
            mv->y = 0;
            return false;
        }
    }
    inline bool  GetLumaVectorFields (sCoordinate *mv, bool bSecond,bool bForward=true)
    {        
        uint32_t n =  m_MV[bForward][0].bSecond + m_MV[bForward][1].bSecond + m_MV[bForward][2].bSecond + m_MV[bForward][3].bSecond;
        
        switch (n)
        {
        case 4:
            mv->bSecond = 1;
            mv->x = median4(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][2].x,m_MV[bForward][3].x);
            mv->y = median4(m_MV[bForward][0].y ,m_MV[bForward][1].y ,m_MV[bForward][2].y ,m_MV[bForward][3].y);
            return true;
        case 3:
            mv->bSecond = 1;
            if (m_MV[bForward][0].bSecond == 0)
            {
                mv->x = median3( m_MV[bForward][1].x, m_MV[bForward][2].x, m_MV[bForward][3].x);
                mv->y = median3( m_MV[bForward][1].y, m_MV[bForward][2].y ,m_MV[bForward][3].y);            
            }
            else if (m_MV[bForward][1].bSecond == 0)
            {
                mv->x = median3(m_MV[bForward][0].x, m_MV[bForward][2].x,m_MV[bForward][3].x);
                mv->y = median3(m_MV[bForward][0].y ,m_MV[bForward][2].y ,m_MV[bForward][3].y);            
            }
            else if (m_MV[bForward][2].bSecond == 0)
            {
                mv->x = median3(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][3].x);
                mv->y = median3(m_MV[bForward][0].y ,m_MV[bForward][1].y,m_MV[bForward][3].y);            
            }
            else
            {
                mv->x = median3(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][2].x);
                mv->y = median3(m_MV[bForward][0].y ,m_MV[bForward][1].y ,m_MV[bForward][2].y);            
            }
            return true;
        case 2:
            uint32_t mask[4];
            mask[0] = (m_MV[bForward][0].bSecond == bSecond)? 0xffffffff:0;
            mask[1] = (m_MV[bForward][1].bSecond == bSecond)? 0xffffffff:0;
            mask[2] = (m_MV[bForward][2].bSecond == bSecond)? 0xffffffff:0;
            mask[3] = (m_MV[bForward][3].bSecond == bSecond)? 0xffffffff:0;
            mv->x = ((int16_t)(mask[0]&m_MV[bForward][0].x)+ (int16_t)(mask[1]&m_MV[bForward][1].x) + (int16_t)(mask[2]&m_MV[bForward][2].x) + (int16_t)(mask[3]&m_MV[bForward][3].x))/2;
            mv->y = ((int16_t)(mask[0]&m_MV[bForward][0].y)+ (int16_t)(mask[1]&m_MV[bForward][1].y) + (int16_t)(mask[2]&m_MV[bForward][2].y) + (int16_t)(mask[3]&m_MV[bForward][3].y))/2;
            mv->bSecond = bSecond;
            
            return true;   
        case 1:
            if (m_MV[bForward][0].bSecond == 1)
            {
                mv->x = median3( m_MV[bForward][1].x, m_MV[bForward][2].x, m_MV[bForward][3].x);
                mv->y = median3( m_MV[bForward][1].y, m_MV[bForward][2].y ,m_MV[bForward][3].y);            
            }
            else if (m_MV[bForward][1].bSecond == 1)
            {
                mv->x = median3(m_MV[bForward][0].x, m_MV[bForward][2].x,m_MV[bForward][3].x);
                mv->y = median3(m_MV[bForward][0].y ,m_MV[bForward][2].y ,m_MV[bForward][3].y);            
            }
            else if (m_MV[bForward][2].bSecond == 1)
            {
                mv->x = median3(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][3].x);
                mv->y = median3(m_MV[bForward][0].y ,m_MV[bForward][1].y,m_MV[bForward][3].y);            
            }
            else
            {
                mv->x = median3(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][2].x);
                mv->y = median3(m_MV[bForward][0].y ,m_MV[bForward][1].y ,m_MV[bForward][2].y);            
            }
            mv->bSecond = 0;
            return true;          
         case 0:
            mv->x = median4(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][2].x,m_MV[bForward][3].x);
            mv->y = median4(m_MV[bForward][0].y ,m_MV[bForward][1].y ,m_MV[bForward][2].y ,m_MV[bForward][3].y);
            mv->bSecond = 0;
            return true;          
         default:
            assert (0);
            return false;     
        
        }
    
    }
    inline void SetMVNULL()
    {
        m_MV[0][0].x = m_MV[0][1].x = m_MV[0][2].x = m_MV[0][3].x = 0;
        m_MV[0][0].y = m_MV[0][1].y = m_MV[0][2].y = m_MV[0][3].y = 0;
        m_MV[1][0].x = m_MV[1][1].x = m_MV[1][2].x = m_MV[1][3].x = 0;
        m_MV[1][0].y = m_MV[1][1].y = m_MV[1][2].y = m_MV[1][3].y = 0;
    }
    inline void SetMV(sCoordinate mv, bool bForward=true)
    {
        m_MV[bForward][0].x = m_MV[bForward][1].x = m_MV[bForward][2].x = m_MV[bForward][3].x = mv.x;
        m_MV[bForward][0].y = m_MV[bForward][1].y = m_MV[bForward][2].y = m_MV[bForward][3].y = mv.y;
    }
    inline void SetMV_F(sCoordinate mv, bool bForward=true)
    {
        m_MV[bForward][0].x = m_MV[bForward][1].x = m_MV[bForward][2].x = m_MV[bForward][3].x = mv.x;
        m_MV[bForward][0].y = m_MV[bForward][1].y = m_MV[bForward][2].y = m_MV[bForward][3].y = mv.y;
        m_MV[bForward][0].bSecond = m_MV[bForward][1].bSecond =
        m_MV[bForward][2].bSecond = m_MV[bForward][3].bSecond = mv.bSecond;
    }
    inline void SetMVChroma(sCoordinate mv, bool bForward=true)
    {
        m_MV[bForward][4].x = m_MV[bForward][5].x = mv.x;
        m_MV[bForward][4].y = m_MV[bForward][5].y = mv.y;
    }
    inline void SetMVChroma_F(sCoordinate mv, bool bForward=true)
    {
        m_MV[bForward][4].x = m_MV[bForward][5].x = mv.x;
        m_MV[bForward][4].y = m_MV[bForward][5].y = mv.y;
        m_MV[bForward][4].bSecond = m_MV[bForward][5].bSecond = mv.bSecond;
    }

    inline UMC::Status SetMV(sCoordinate mv, int32_t blockNum, bool bForward=true)
    {
        if (blockNum>=6)
            return UMC::UMC_ERR_FAILED;
        m_MV[bForward][blockNum].x =  mv.x;
        m_MV[bForward][blockNum].y =  mv.y;
        return UMC::UMC_OK;
    }
    inline UMC::Status SetMV_F(sCoordinate mv, int32_t blockNum, bool bForward=true)
    {
        if (blockNum>=6)
            return UMC::UMC_ERR_FAILED;
        m_MV[bForward][blockNum].x =  mv.x;
        m_MV[bForward][blockNum].y =  mv.y;
        m_MV[bForward][blockNum].bSecond = mv.bSecond;
        return UMC::UMC_OK;
    }
    inline UMC::Status GetMV(sCoordinate* mv, int32_t blockNum, bool bForward=true)
    {
        if (blockNum>=6)
            return 0;
        mv->x =  m_MV[bForward][blockNum].x ;
        mv->y =  m_MV[bForward][blockNum].y;
        return UMC::UMC_OK;
    }

    inline UMC::Status GetMV_F(sCoordinate* mv, int32_t blockNum, bool bForward=true)
    {
        if (blockNum>=6)
            return 0;
        mv->x =  m_MV[bForward][blockNum].x ;
        mv->y =  m_MV[bForward][blockNum].y;
        mv->bSecond = m_MV[bForward][blockNum].bSecond;
        return UMC::UMC_OK;
    }

    inline void        SetMBPattern(uint8_t pattern)
    {
        m_uiMBPattern = pattern;
    }
    inline uint8_t       GetPattern()
    {
        return m_uiMBPattern;
    }
    inline uint8_t       GetIntraPattern()
    {
        return m_uiIntraPattern;
    }
    inline void SetIntraBlock(int32_t blockNum)
    {
        m_MV[0][blockNum].x = m_MV[0][blockNum].y =
        m_MV[1][blockNum].x = m_MV[1][blockNum].y = 0;
        m_uiIntraPattern = m_uiIntraPattern | (1<<VC_ENC_PATTERN_POS(blockNum));
    }

    inline uint32_t GetBlkPattern(int32_t blk)
    {
        return (m_uiBlocksPattern >> (5 - blk)*4) & 0xf;
    }
    inline eTransformType GetBlkVSTType(int32_t blk)
    {
        uint32_t num = blk;

        uint32_t pattern = (m_uiVSTPattern >> (num*2));

        return BlkTransformTypeTabl[pattern & 0x03];
    }

    inline void SetVSTPattern(eTransformType* pBlkVTSType)
    {
        uint32_t blk = 0;
        eTransformType VTSType = VC1_ENC_8x8_TRANSFORM;
        m_uiVSTPattern = 0;

        for(blk = 0; blk < 6; blk ++)
        {
            VTSType = pBlkVTSType[blk];
            if(!GetBlkPattern(blk))
                VTSType = VC1_ENC_8x8_TRANSFORM;

            switch(VTSType)
            {
                case VC1_ENC_8x8_TRANSFORM:
                    break;
                case VC1_ENC_8x4_TRANSFORM:
                    m_uiVSTPattern |= (0x1<<(blk*2));
                    break;
                case VC1_ENC_4x8_TRANSFORM:
                    m_uiVSTPattern |= (0x2<<(blk*2));
                    break;
                case VC1_ENC_4x4_TRANSFORM:
                    m_uiVSTPattern |= (0x3<<(blk*2));
                    break;
                default:
                    assert(0);
                    break;
            }
        }
    }

    inline uint32_t GetVSTPattern()
    {
        return m_uiVSTPattern;
    }
    inline void SetOverlap(bool overlap)
    {
        m_uiOverlap = overlap;
    }
    inline bool GetOverlap()
    {
        return m_uiOverlap;
    }
};

struct VC1EncoderMB
{
    VC1EncoderMBData    * pData;
    VC1EncoderMBData    * pRecData;
    VC1EncoderMBInfo    * pInfo;
};


class VC1EncoderMBs
{
private:
    VC1EncoderMBInfo ** m_MBInfo;       // rows of macroblocks info
    VC1EncoderMBData ** m_MBData;       // rows of macroblocks data
    VC1EncoderMBData ** m_MBRecData;    // rows of macroblocks data

    int32_t              m_iCurrRowIndex;
    int32_t              m_iPrevRowIndex;
    uint32_t              m_iCurrMBIndex;
    uint32_t              m_uiMBsInRow;
    uint32_t              m_uiMBsInCol;

public:
    VC1EncoderMBs():
        m_iCurrRowIndex(0),
        m_iPrevRowIndex(-1),
        m_iCurrMBIndex(0),
        m_uiMBsInRow(0),
        m_uiMBsInCol(0)
    {
        m_MBInfo = 0;
        m_MBData = 0;
    }
    ~VC1EncoderMBs()
    {
        Close();
    }

    VC1EncoderMBInfo*       GetCurrMBInfo();
    VC1EncoderMBInfo*       GetTopMBInfo();
    VC1EncoderMBInfo*       GetLeftMBInfo();
    VC1EncoderMBInfo*       GetTopLeftMBInfo();
    VC1EncoderMBInfo*       GetTopRightMBInfo();
    VC1EncoderMBInfo*       GetPevMBInfo(int32_t x, int32_t y);

    VC1EncoderMBData*       GetCurrMBData();
    VC1EncoderMBData*       GetTopMBData();
    VC1EncoderMBData*       GetLeftMBData();
    VC1EncoderMBData*       GetTopLeftMBData();

    VC1EncoderMBData*       GetRecCurrMBData();
    VC1EncoderMBData*       GetRecTopMBData();
    VC1EncoderMBData*       GetRecLeftMBData();
    VC1EncoderMBData*       GetRecTopLeftMBData();


    VC1EncoderMB*           GetCurrMB();
    VC1EncoderMB*           GetTopMB();
    VC1EncoderMB*           GetLeftMB();
    VC1EncoderMB*           GetTopLeft();

    static uint32_t           CalcAllocMemorySize(uint32_t MBsInRow, uint32_t MBsInCol, bool bNV12);
    UMC::Status             Init(uint8_t* pPicBufer, uint32_t AllocatedMemSize, uint32_t MBsInRow, uint32_t MBsInCol, bool bNV12);
    UMC::Status             Close();
    UMC::Status             NextMB();
    UMC::Status             NextRow();
    UMC::Status             StartRow();
    UMC::Status             Reset();
};

typedef struct
{
    VC1EncoderMBData* LeftMB;
    VC1EncoderMBData* TopMB;
    VC1EncoderMBData* TopLeftMB;
} NeighbouringMBsData;

inline void GetTSType (uint32_t pattern, eTransformType* pBlockTT)
{
    uint8_t BlockTrans = (uint8_t)(pattern & 0x03);
    for(uint32_t blk_num = 0; blk_num < 6; blk_num++)
    {
        pBlockTT[blk_num] = BlkTransformTypeTabl[BlockTrans];
        pattern>>=2;
        BlockTrans = (uint8_t)(pattern & 0x03);
    }
}
}
#endif
#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
