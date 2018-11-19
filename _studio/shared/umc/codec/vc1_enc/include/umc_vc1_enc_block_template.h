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

#ifndef _ENCODER_VC1_BLOCK_T_H_
#define _ENCODER_VC1_BLOCK_T_H_

#include "umc_vc1_enc_bitstream.h"
#include "ippvc.h"
#include "umc_vc1_enc_def.h"
#include "umc_structures.h"

namespace UMC_VC1_ENCODER
{
//template <class T>  UMC::Status   WriteBlockACIntra         (T*                     pCodedBlock,
//                                                            int16_t*                 pBlock,
//                                                            uint32_t                  blockStep,
//                                                            const uint8_t*            pScanMatrix,
//                                                            const sACTablesSet*     pACTablesSet,
//                                                            sACEscInfo*             pACEscInfo)
//   {    UMC::Status     err          = UMC::UMC_OK;
//        int32_t          i            = 0;
//
//        uint8_t           nPairs       = 0;
//        int16_t          levels[65];
//        uint8_t           runs[65];
//
//        const uint8_t     *pTableDR    = pACTablesSet->pTableDR;
//        const uint8_t     *pTableDL    = pACTablesSet->pTableDL;
//        const uint8_t     *pTableInd   = pACTablesSet->pTableInd;
//        const uint32_t    *pEncTable   = pACTablesSet->pEncTable;
//
//
//        runs[0] = 0;
//        //prepare runs, levels arrays
//        for (i = 1; i<64; i++)
//        {
//            int32_t  pos    = pScanMatrix[i];
//            int16_t  value = *((int16_t*)((uint8_t*)pBlock + blockStep*(pos>>3)) + (pos&0x07));
//            if (!value)
//            {
//                runs[nPairs]++;
//            }
//            else
//            {
//                levels[nPairs++] = value;
//                runs[nPairs]   = 0;
//            }
//        }
//        // put codes into bitstream
//        i = 0;
//        for (int32_t not_last = 1; not_last>=0; not_last--)
//        {
//            for (; i < nPairs - not_last; i++)
//            {
//                bool    sign = false;
//                uint8_t   run  = runs  [i];
//                int16_t  lev  = levels[i];
//
//                uint8_t mode = GetMode( run, lev, pTableDR, pTableDL, sign);
//
//                switch (mode)
//                {
//                    case 3:
//                        err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
//                        VC1_ENC_CHECK (err)
//                        err = pCodedBlock->PutBits(0,2);                       //mode
//                        VC1_ENC_CHECK (err)
//                        err = pCodedBlock->PutBits(!not_last,1);               //last
//                        VC1_ENC_CHECK (err)
//                        if ((!pACEscInfo->levelSize) && (!pACEscInfo->runSize))
//                        {
//                            pACEscInfo->runSize = 6;
//                            pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*11 +1]==0)? 8:11;
//                            pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize + 1]<=0) ?
//                                                            2 : pACEscInfo->levelSize;
//                            err = pCodedBlock->PutBits(pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize],
//                                                       pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize +1]);
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(pACEscInfo->runSize - 3, 2);
//                            VC1_ENC_CHECK (err)
//                        }
//                        err = pCodedBlock->PutBits(run,pACEscInfo->runSize);
//                        VC1_ENC_CHECK (err)
//                        err = pCodedBlock->PutBits(sign, 1);
//                        VC1_ENC_CHECK (err)
//                        if (lev>((1<<pACEscInfo->levelSize)-1))
//                        {
//                            lev =(1<<pACEscInfo->levelSize)-1;
//                            levels[i] = lev;
//                        }
//                        err = pCodedBlock->PutBits(lev,pACEscInfo->levelSize);
//                        VC1_ENC_CHECK (err)
//                        break;
//                    case 2:
//                    case 1:
//                        err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
//                        VC1_ENC_CHECK (err)
//                        err = pCodedBlock->PutBits(1,mode);                     //mode
//                        VC1_ENC_CHECK (err)
//                    case 0:
//                        int16_t index = pTableInd[run] + lev;
//                        err = pCodedBlock->PutBits(pEncTable[2*index], pEncTable[2*index + 1]);
//                        VC1_ENC_CHECK (err)
//                        err = pCodedBlock->PutBits(sign, 1);
//                        VC1_ENC_CHECK (err)
//
//                        break;
//
//                }
//             }
//
//            pTableDR    = pACTablesSet->pTableDRLast;
//            pTableDL    = pACTablesSet->pTableDLLast;
//            pTableInd   = pACTablesSet->pTableIndLast;
//
//        }
//        return UMC::UMC_OK;
//   }

//   template <class T>  UMC::Status   WriteBlockInter8x8type(T*                      pCodedBlock,
//                                                            int16_t*                 pBlock,
//                                                            uint32_t                  blockStep,
//                                                            const uint8_t*            pScanMatrix,
//                                                            const sACTablesSet*     pACTablesSet,
//                                                            sACEscInfo*             pACEscInfo,
//                                                            uint32_t                  pattern)
//   {    UMC::Status     err          = UMC::UMC_OK;
//        int32_t          i            = 0;
//
//        uint8_t           nPairs       = 0;
//        int16_t          levels[65];
//        uint8_t           runs[65];
//
//        const uint8_t     *pTableDR    = pACTablesSet->pTableDR;
//        const uint8_t     *pTableDL    = pACTablesSet->pTableDL;
//        const uint8_t     *pTableInd   = pACTablesSet->pTableInd;
//        const uint32_t    *pEncTable   = pACTablesSet->pEncTable;
//
//
//        runs[0] = 0;
//        //prepare runs, levels arrays
//        for (i = 0; i<64; i++)
//        {
//            int32_t  pos    = pScanMatrix[i];
//            int16_t  value = *((int16_t*)((uint8_t*)pBlock + blockStep*(pos>>3)) + (pos&0x07));
//            if (!value)
//            {
//                runs[nPairs]++;
//            }
//            else
//            {
//                levels[nPairs++] = value;
//                runs[nPairs]   = 0;
//            }
//        }
//        // put codes into bitstream
//        i = 0;
//        for (int32_t not_last = 1; not_last>=0; not_last--)
//        {
//            for (; i < nPairs - not_last; i++)
//            {
//                bool    sign = false;
//                uint8_t   run  = runs  [i];
//                int16_t  lev  = levels[i];
//
//                uint8_t mode = GetMode( run, lev, pTableDR, pTableDL, sign);
//
//                switch (mode)
//                {
//                    case 3:
//                        err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
//                        VC1_ENC_CHECK (err)
//                        err = pCodedBlock->PutBits(0,2);                       //mode
//                        VC1_ENC_CHECK (err)
//                        err = pCodedBlock->PutBits(!not_last,1);               //last
//                        VC1_ENC_CHECK (err)
//                        if ((!pACEscInfo->levelSize) && (!pACEscInfo->runSize))
//                        {
//                            pACEscInfo->runSize = 6;
//                            pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*11 +1]==0)? 8:11;
//                            pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize + 1]<=0) ?
//                                                            2 : pACEscInfo->levelSize;
//                            err = pCodedBlock->PutBits(pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize],
//                                                       pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize +1]);
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(pACEscInfo->runSize - 3, 2);
//                            VC1_ENC_CHECK (err)
//                        }
//                        err = pCodedBlock->PutBits(run,pACEscInfo->runSize);
//                        VC1_ENC_CHECK (err)
//                        err = pCodedBlock->PutBits(sign, 1);
//                        VC1_ENC_CHECK (err)
//                        if (lev>((1<<pACEscInfo->levelSize)-1))
//                        {
//                            lev =(1<<pACEscInfo->levelSize)-1;
//                            levels[i] = lev;
//                        }
//                        err = pCodedBlock->PutBits(lev,pACEscInfo->levelSize);
//                        VC1_ENC_CHECK (err)
//                        break;
//                    case 2:
//                    case 1:
//                        err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
//                        VC1_ENC_CHECK (err)
//                        err = pCodedBlock->PutBits(1,mode);                     //mode
//                        VC1_ENC_CHECK (err)
//                    case 0:
//                        int16_t index = pTableInd[run] + lev;
//                        err = pCodedBlock->PutBits(pEncTable[2*index], pEncTable[2*index + 1]);
//                        VC1_ENC_CHECK (err)
//                        err = pCodedBlock->PutBits(sign, 1);
//                        VC1_ENC_CHECK (err)
//                        break;
//
//                }
//             }
//
//            pTableDR    = pACTablesSet->pTableDRLast;
//            pTableDL    = pACTablesSet->pTableDLLast;
//            pTableInd   = pACTablesSet->pTableIndLast;
//
//        }
//        return UMC::UMC_OK;
//   }
//
//   template <class T>  UMC::Status   WriteBlockInter8x4type(T*                      pCodedBlock,
//                                                            int16_t*                 pBlock,
//                                                            uint32_t                  blockStep,
//                                                            const uint8_t*            pScanMatrix,
//                                                            const sACTablesSet*     pACTablesSet,
//                                                            sACEscInfo*             pACEscInfo,
//                                                            uint32_t                  pattern)
//   {    UMC::Status     err          = UMC::UMC_OK;
//        int32_t          i            = 0;
//
//        uint8_t           nPairs       = 0;
//        int16_t          levels[33];
//        uint8_t           runs[33];
//        const uint32_t    *pEncTable   = pACTablesSet->pEncTable;
//        uint8_t           patMask[2] = {0x0C, 0x03};
//
//        for (uint32_t nSubblock=0; nSubblock<2; nSubblock++)
//        {
//            const uint8_t     *pTableDR    = pACTablesSet->pTableDR;
//            const uint8_t     *pTableDL    = pACTablesSet->pTableDL;
//            const uint8_t     *pTableInd   = pACTablesSet->pTableInd;
//            int16_t*          pSubBlock   = (int16_t*)((uint8_t*)pBlock + blockStep*nSubblock*4);
//
//            if ((pattern & patMask[nSubblock])==0)
//                continue;
//
//            nPairs       = 0;
//            runs[0] = 0;
//            //prepare runs, levels arrays
//            for (i = 0; i<32; i++)
//            {
//                int32_t  pos    = pScanMatrix[i];
//                int16_t  value = *((int16_t*)((uint8_t*)pSubBlock + blockStep*(pos>>3)) + (pos&0x07));
//                if (!value)
//                {
//                    runs[nPairs]++;
//                }
//                else
//                {
//                    levels[nPairs++] = value;
//                    runs[nPairs]   = 0;
//                }
//            }
//            // put codes into bitstream
//            i = 0;
//            for (int32_t not_last = 1; not_last>=0; not_last--)
//            {
//                for (; i < nPairs - not_last; i++)
//                {
//                    bool    sign = false;
//                    uint8_t   run  = runs  [i];
//                    int16_t  lev  = levels[i];
//
//                    uint8_t mode = GetMode( run, lev, pTableDR, pTableDL, sign);
//
//                    switch (mode)
//                    {
//                        case 3:
//                            err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(0,2);                       //mode
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(!not_last,1);               //last
//                            VC1_ENC_CHECK (err)
//                            if ((!pACEscInfo->levelSize) && (!pACEscInfo->runSize))
//                            {
//                                pACEscInfo->runSize = 6;
//                                pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*11 +1]==0)? 8:11;
//                                pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize + 1]<=0) ?
//                                                                2 : pACEscInfo->levelSize;
//                                err = pCodedBlock->PutBits(pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize],
//                                                        pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize +1]);
//                                VC1_ENC_CHECK (err)
//                                err = pCodedBlock->PutBits(pACEscInfo->runSize - 3, 2);
//                                VC1_ENC_CHECK (err)
//                            }
//                            err = pCodedBlock->PutBits(run,pACEscInfo->runSize);
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(sign, 1);
//                            VC1_ENC_CHECK (err)
//                            if (lev>((1<<pACEscInfo->levelSize)-1))
//                            {
//                                lev =(1<<pACEscInfo->levelSize)-1;
//                                levels[i] = lev;
//                            }
//                            err = pCodedBlock->PutBits(lev,pACEscInfo->levelSize);
//                            VC1_ENC_CHECK (err)
//                            break;
//                        case 2:
//                        case 1:
//                            err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(1,mode);                     //mode
//                            VC1_ENC_CHECK (err)
//                        case 0:
//                            int16_t index = pTableInd[run] + lev;
//                            err = pCodedBlock->PutBits(pEncTable[2*index], pEncTable[2*index + 1]);
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(sign, 1);
//                            VC1_ENC_CHECK (err)
//                            break;
//
//                    }
//                }
//
//                pTableDR    = pACTablesSet->pTableDRLast;
//                pTableDL    = pACTablesSet->pTableDLLast;
//                pTableInd   = pACTablesSet->pTableIndLast;
//
//            }
//        }
//        return UMC::UMC_OK;
//   }
//  template <class T>  UMC::Status   WriteBlockInter4x8type (T*                      pCodedBlock,
//                                                            int16_t*                 pBlock,
//                                                            uint32_t                  blockStep,
//                                                            const uint8_t*            pScanMatrix,
//                                                            const sACTablesSet*     pACTablesSet,
//                                                            sACEscInfo*             pACEscInfo,
//                                                            uint32_t                  pattern)
//   {    UMC::Status     err          = UMC::UMC_OK;
//        int32_t          i            = 0;
//
//        uint8_t           nPairs       = 0;
//        int16_t          levels[33];
//        uint8_t           runs[33];
//        uint8_t           patMask[2]   = {0x0A, 0x05};
//        const uint32_t    *pEncTable   = pACTablesSet->pEncTable;
//
//        for (uint32_t nSubblock=0; nSubblock<2; nSubblock++)
//        {
//            const uint8_t     *pTableDR    = pACTablesSet->pTableDR;
//            const uint8_t     *pTableDL    = pACTablesSet->pTableDL;
//            const uint8_t     *pTableInd   = pACTablesSet->pTableInd;
//            int16_t*          pSubBlock   = pBlock + 4*nSubblock;
//
//            if ((pattern & patMask[nSubblock])==0)
//                continue;
//
//
//            nPairs       = 0;
//            runs[0] = 0;
//            //prepare runs, levels arrays
//            for (i = 0; i<32; i++)
//            {
//                int32_t  pos    = pScanMatrix[i];
//                int16_t  value = *((int16_t*)((uint8_t*)pSubBlock + blockStep*(pos>>2)) + (pos&0x03));
//                if (!value)
//                {
//                    runs[nPairs]++;
//                }
//                else
//                {
//                    levels[nPairs++] = value;
//                    runs[nPairs]   = 0;
//                }
//            }
//            // put codes into bitstream
//            i = 0;
//            for (int32_t not_last = 1; not_last>=0; not_last--)
//            {
//                for (; i < nPairs - not_last; i++)
//                {
//                    bool    sign = false;
//                    uint8_t   run  = runs  [i];
//                    int16_t  lev  = levels[i];
//
//                    uint8_t mode = GetMode( run, lev, pTableDR, pTableDL, sign);
//
//                    switch (mode)
//                    {
//                        case 3:
//                            err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(0,2);                       //mode
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(!not_last,1);               //last
//                            VC1_ENC_CHECK (err)
//                            if ((!pACEscInfo->levelSize) && (!pACEscInfo->runSize))
//                            {
//                                pACEscInfo->runSize = 6;
//                                pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*11 +1]==0)? 8:11;
//                                pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize + 1]<=0) ?
//                                                                2 : pACEscInfo->levelSize;
//                                err = pCodedBlock->PutBits(pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize],
//                                                        pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize +1]);
//                                VC1_ENC_CHECK (err)
//                                err = pCodedBlock->PutBits(pACEscInfo->runSize - 3, 2);
//                                VC1_ENC_CHECK (err)
//                            }
//                            err = pCodedBlock->PutBits(run,pACEscInfo->runSize);
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(sign, 1);
//                            VC1_ENC_CHECK (err)
//                            if (lev>((1<<pACEscInfo->levelSize)-1))
//                            {
//                                lev =(1<<pACEscInfo->levelSize)-1;
//                                levels[i] = lev;
//                            }
//                            err = pCodedBlock->PutBits(lev,pACEscInfo->levelSize);
//                            VC1_ENC_CHECK (err)
//                            break;
//                        case 2:
//                        case 1:
//                            err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(1,mode);                     //mode
//                            VC1_ENC_CHECK (err)
//                        case 0:
//                            int16_t index = pTableInd[run] + lev;
//                            err = pCodedBlock->PutBits(pEncTable[2*index], pEncTable[2*index + 1]);
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(sign, 1);
//                            VC1_ENC_CHECK (err)
//                            break;
//
//                    }
//                }
//
//                pTableDR    = pACTablesSet->pTableDRLast;
//                pTableDL    = pACTablesSet->pTableDLLast;
//                pTableInd   = pACTablesSet->pTableIndLast;
//
//            }
//        }
//        return UMC::UMC_OK;
//   }
//
// template <class T>  UMC::Status   WriteBlockInter4x4type  (T*                      pCodedBlock,
//                                                            int16_t*                 pBlock,
//                                                            uint32_t                  blockStep,
//                                                            const uint8_t*            pScanMatrix,
//                                                            const sACTablesSet*     pACTablesSet,
//                                                            sACEscInfo*             pACEscInfo,
//                                                            uint32_t                  pattern)
//   {    UMC::Status     err          = UMC::UMC_OK;
//        int32_t          i            = 0;
//
//        uint8_t           nPairs       = 0;
//        int16_t          levels[17];
//        uint8_t           runs[17];
//        const uint32_t    *pEncTable   = pACTablesSet->pEncTable;
//
//
//        for (uint32_t nSubblock=0; nSubblock<4; nSubblock++)
//        {
//            const uint8_t     *pTableDR    = pACTablesSet->pTableDR;
//            const uint8_t     *pTableDL    = pACTablesSet->pTableDL;
//            const uint8_t     *pTableInd   = pACTablesSet->pTableInd;
//            int16_t*          pSubBlock   = (int16_t*)((uint8_t*)pBlock + blockStep*4*(nSubblock>>1))+ 4*(nSubblock&1);
//
//            if ((pattern & (1<<(3-nSubblock)))==0)
//                continue;
//
//            nPairs       = 0;
//            runs[0] = 0;
//            //prepare runs, levels arrays
//            for (i = 0; i<16; i++)
//            {
//                int32_t  pos    = pScanMatrix[i];
//                int16_t  value = *((int16_t*)((uint8_t*)pSubBlock + blockStep*(pos>>2)) + (pos&0x03));
//                if (!value)
//                {
//                    runs[nPairs]++;
//                }
//                else
//                {
//                    levels[nPairs++] = value;
//                    runs[nPairs]   = 0;
//                }
//            }
//            // put codes into bitstream
//            i = 0;
//            for (int32_t not_last = 1; not_last>=0; not_last--)
//            {
//                for (; i < nPairs - not_last; i++)
//                {
//                    bool    sign = false;
//                    uint8_t   run  = runs  [i];
//                    int16_t  lev  = levels[i];
//
//                    uint8_t mode = GetMode( run, lev, pTableDR, pTableDL, sign);
//
//                    switch (mode)
//                    {
//                        case 3:
//                            err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(0,2);                       //mode
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(!not_last,1);               //last
//                            VC1_ENC_CHECK (err)
//                            if ((!pACEscInfo->levelSize) && (!pACEscInfo->runSize))
//                            {
//                                pACEscInfo->runSize = 6;
//                                pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*11 +1]==0)? 8:11;
//                                pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize + 1]<=0) ?
//                                                                2 : pACEscInfo->levelSize;
//                                err = pCodedBlock->PutBits(pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize],
//                                                        pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize +1]);
//                                VC1_ENC_CHECK (err)
//                                err = pCodedBlock->PutBits(pACEscInfo->runSize - 3, 2);
//                                VC1_ENC_CHECK (err)
//                            }
//                            err = pCodedBlock->PutBits(run,pACEscInfo->runSize);
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(sign, 1);
//                            VC1_ENC_CHECK (err)
//                            if (lev>((1<<pACEscInfo->levelSize)-1))
//                            {
//                                lev =(1<<pACEscInfo->levelSize)-1;
//                                levels[i] = lev;
//                            }
//                            err = pCodedBlock->PutBits(lev,pACEscInfo->levelSize);
//                            VC1_ENC_CHECK (err)
//                            break;
//                        case 2:
//                        case 1:
//                            err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(1,mode);                     //mode
//                            VC1_ENC_CHECK (err)
//                        case 0:
//                            int16_t index = pTableInd[run] + lev;
//                            err = pCodedBlock->PutBits(pEncTable[2*index], pEncTable[2*index + 1]);
//                            VC1_ENC_CHECK (err)
//                            err = pCodedBlock->PutBits(sign, 1);
//                            VC1_ENC_CHECK (err)
//                            break;
//
//                    }
//                }
//
//                pTableDR    = pACTablesSet->pTableDRLast;
//                pTableDL    = pACTablesSet->pTableDLLast;
//                pTableInd   = pACTablesSet->pTableIndLast;
//
//            }
//        }
//        return UMC::UMC_OK;
//   }
//
//
   template <class T>
   UMC::Status   WriteDCQuantOther (int16_t                  DC,
                                    const uint32_t*           pEncTable,
                                    T*                      pCodedBlock)
   {
        bool            sign        = (DC<0)? 1:0;
        UMC::Status     err         = UMC::UMC_OK;

        DC         = (sign)? -DC : DC;
        if (!DC )
        {
            err = pCodedBlock->PutBits(pEncTable[2*DC],pEncTable[2*DC+1]);
            VC1_ENC_CHECK (err)
        }
        else if (DC < VC1_ENC_DC_ESCAPE_INDEX)
        {
            err = pCodedBlock->PutBits(pEncTable[2*DC],pEncTable[2*DC+1]);
            VC1_ENC_CHECK (err)
            err = pCodedBlock->PutBits(sign,1);
            VC1_ENC_CHECK (err)
        }
        else
        {
            err = pCodedBlock->PutBits(pEncTable[2*VC1_ENC_DC_ESCAPE_INDEX],pEncTable[2*VC1_ENC_DC_ESCAPE_INDEX+1]);
            VC1_ENC_CHECK (err)
            err = pCodedBlock->PutBits(DC, 8);
            VC1_ENC_CHECK (err)
            err = pCodedBlock->PutBits(sign,1);
            VC1_ENC_CHECK (err)
        }
        return UMC::UMC_OK;
   }

   template <class T>
   UMC::Status       WriteDCQuant1     (int16_t                  DC,
                                        const uint32_t*           pEncTable,
                                        T*                      pCodedBlock)
   {
        bool    sign       = (DC<0)? 1:0;

        int16_t      DC1        = 0;
        int16_t      DC2        = 0;
        UMC::Status err        = UMC::UMC_OK;

        DC = (sign)? -DC : DC;

        DC1 = (DC+3)>>2;
        DC2 = (DC+3)&3 ;

        if (!DC)
        {
            err = pCodedBlock->PutBits(pEncTable[2*DC],pEncTable[2*DC+1]);
            VC1_ENC_CHECK (err)
        }
        else if (DC1 < VC1_ENC_DC_ESCAPE_INDEX)
        {
            err = pCodedBlock->PutBits(pEncTable[2*DC1],pEncTable[2*DC1+1]);
            VC1_ENC_CHECK (err)
            err = pCodedBlock->PutBits(DC2,2);
            VC1_ENC_CHECK (err)
            err = pCodedBlock->PutBits(sign,1);
            VC1_ENC_CHECK (err)
        }
        else
        {
            err = pCodedBlock->PutBits(pEncTable[2*VC1_ENC_DC_ESCAPE_INDEX],pEncTable[2*VC1_ENC_DC_ESCAPE_INDEX+1]);
            VC1_ENC_CHECK (err)
            err = pCodedBlock->PutBits(DC, 10);
            VC1_ENC_CHECK (err)
            err = pCodedBlock->PutBits(sign,1);
            VC1_ENC_CHECK (err)
        }
        return err;
   }
   template <class T>  inline
   UMC::Status        WriteDCQuant2     (int16_t                  DC,
                                         const uint32_t*           pEncTable,
                                         T*                      pCodedBlock)
   {
        bool        sign       = (DC<0)? 1:0;
        int16_t      DC1        = 0;
        int16_t      DC2        = 0;
        UMC::Status err        = UMC::UMC_OK;

        DC         = (sign)? -DC : DC;

        DC1 = (DC+1)>>1;
        DC2 = (DC+1)&1 ;

        if (!DC)
        {
            err = pCodedBlock->PutBits(pEncTable[2*DC],pEncTable[2*DC+1]);
            VC1_ENC_CHECK (err)
        }
        else if (DC1 < VC1_ENC_DC_ESCAPE_INDEX)
        {
            err = pCodedBlock->PutBits(pEncTable[2*DC1],pEncTable[2*DC1+1]);
            VC1_ENC_CHECK (err)
            err = pCodedBlock->PutBits(DC2,1);
            VC1_ENC_CHECK (err)
            err = pCodedBlock->PutBits(sign,1);
            VC1_ENC_CHECK (err)

        }
        else
        {
            err = pCodedBlock->PutBits(pEncTable[2*VC1_ENC_DC_ESCAPE_INDEX],pEncTable[2*VC1_ENC_DC_ESCAPE_INDEX+1]);
            VC1_ENC_CHECK (err)
            err = pCodedBlock->PutBits(DC, 9);
            VC1_ENC_CHECK (err)
            err = pCodedBlock->PutBits(sign,1);
            VC1_ENC_CHECK (err)
        }
        return UMC::UMC_OK;
   }





     /*
   template <class T>  inline
   UMC::Status         VC1EncoderBlock::WriteBlockDC      (  uint8_t                   Quant,
                                                                                const uint32_t*           pEncTable,
                                                                                T*                      pCodedBlock)
   {
       switch (Quant)
       {
       case 1:
            return WriteDCQuant1     (pEncTable,pCodedBlock);
       case 2:
            return WriteDCQuant2     (pEncTable,pCodedBlock);
       default:
            return WriteDCQuantOther (pEncTable,pCodedBlock);
       }
   }
*/
};
#endif
#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
