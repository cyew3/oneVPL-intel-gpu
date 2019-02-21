// Copyright (c) 2003-2019 Intel Corporation
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
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#include "umc_h264_segment_decoder.h"
#include "umc_h264_bitstream_inlines.h"
#include "umc_h264_dec_tables.h"

namespace UMC
{

///////////////////////////////////////////////////////////////////////////////
// lookup table to translate B frame type code to MB type
const
uint8_t CodeToMBTypeB[] =
{
    MBTYPE_DIRECT,          // 0
    MBTYPE_FORWARD,
    MBTYPE_BACKWARD,
    MBTYPE_BIDIR,
    MBTYPE_INTER_16x8,
    MBTYPE_INTER_8x16,      // 5
    MBTYPE_INTER_16x8,
    MBTYPE_INTER_8x16,
    MBTYPE_INTER_16x8,
    MBTYPE_INTER_8x16,
    MBTYPE_INTER_16x8,      // 10
    MBTYPE_INTER_8x16,
    MBTYPE_INTER_16x8,
    MBTYPE_INTER_8x16,
    MBTYPE_INTER_16x8,
    MBTYPE_INTER_8x16,      // 15
    MBTYPE_INTER_16x8,
    MBTYPE_INTER_8x16,
    MBTYPE_INTER_16x8,
    MBTYPE_INTER_8x16,
    MBTYPE_INTER_16x8,      // 20
    MBTYPE_INTER_8x16,
    MBTYPE_INTER_8x8
};

// lookup table to extract prediction direction from MB type code for
// 16x8 and 8x16 MB types. Contains direction for first and second
// subblocks at each entry.
const
uint8_t CodeToBDir[][2] =
{
    {D_DIR_FWD, D_DIR_FWD},
    {D_DIR_BWD, D_DIR_BWD},
    {D_DIR_FWD, D_DIR_BWD},
    {D_DIR_BWD, D_DIR_FWD},
    {D_DIR_FWD, D_DIR_BIDIR},
    {D_DIR_BWD, D_DIR_BIDIR},
    {D_DIR_BIDIR, D_DIR_FWD},
    {D_DIR_BIDIR, D_DIR_BWD},
    {D_DIR_BIDIR, D_DIR_BIDIR}
};

// lookup table to translate B frame 8x8 subblock code to type and
// prediction direction
static
const struct
{
    uint8_t type;
    uint8_t dir;
} CodeToSBTypeAndDir[] =
{
    {SBTYPE_DIRECT, D_DIR_DIRECT},
    {SBTYPE_8x8, D_DIR_FWD},
    {SBTYPE_8x8, D_DIR_BWD},
    {SBTYPE_8x8, D_DIR_BIDIR},
    {SBTYPE_8x4, D_DIR_FWD},
    {SBTYPE_4x8, D_DIR_FWD},
    {SBTYPE_8x4, D_DIR_BWD},
    {SBTYPE_4x8, D_DIR_BWD},
    {SBTYPE_8x4, D_DIR_BIDIR},
    {SBTYPE_4x8, D_DIR_BIDIR},
    {SBTYPE_4x4, D_DIR_FWD},
    {SBTYPE_4x4, D_DIR_BWD},
    {SBTYPE_4x4, D_DIR_BIDIR}
};

const
int32_t NIT2LIN[16] =
{
    0, 1, 4, 5,
    2, 3, 6, 7,
    8, 9,12,13,
    10,11,14,15
};

void H264SegmentDecoder::DecodeMBFieldDecodingFlag_CAVLC(void)
{
    uint32_t bit = m_pBitStream->Get1Bit();

    pSetPairMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo,
                                m_cur_mb.GlobalMacroblockPairInfo,
                                bit);

} // void H264SegmentDecoder::DecodeMBFieldDecodingFlag_CAVLC(void)

void H264SegmentDecoder::DecodeMBFieldDecodingFlag(void)
{
    int32_t iFirstMB = m_iFirstSliceMb;
    int32_t iCurrentField = 0;
    int32_t mbAddr;

    // try to get left MB pair info
    mbAddr = m_CurMBAddr - 2;
    if ((iFirstMB <= mbAddr) &&
        (m_CurMB_X))
    {
        iCurrentField = GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbAddr]);
    }
    else
    {
        // get above MB pair info
        mbAddr = m_CurMBAddr - mb_width * 2;
        if ((iFirstMB <= mbAddr) &&
            (m_CurMB_Y))
            iCurrentField = GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbAddr]);
    }

    pSetPairMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo,
                                m_cur_mb.GlobalMacroblockPairInfo,
                                iCurrentField);

} // void H264SegmentDecoder::DecodeMBFieldDecodingFlag(void)

uint32_t H264SegmentDecoder::DecodeMBSkipRun_CAVLC(void)
{
    return (uint32_t) m_pBitStream->GetVLCElement(false);

} // uint32_t H264SegmentDecoder::DecodeMBSkipRun_CAVLC(void)

void H264SegmentDecoder::DecodeMBTypeISlice_CAVLC(void)
{
    uint32_t uCodeNum;

    uCodeNum = m_pBitStream->GetVLCElement(false);

    // the macroblock has I_NxN type
    if (0 == uCodeNum)
        m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA;
    // the macroblock has I_16x16 type
    else if (25 != uCodeNum)
    {
        m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA_16x16;
        IntraType *pMBIntraTypes = m_pMBIntraTypes + m_CurMBAddr*NUM_INTRA_TYPE_ELEMENTS;

        pMBIntraTypes[0] =
        pMBIntraTypes[1] =
        pMBIntraTypes[2] =
        pMBIntraTypes[3] = (uCodeNum - 1) & 0x03;


        // set the CBP
        {
            uCodeNum -= 1;
            if (12 <= uCodeNum)
            {
                m_cur_mb.LocalMacroblockInfo->cbp = 0x0f;
                uCodeNum -= 12;
            }
            else
                m_cur_mb.LocalMacroblockInfo->cbp = 0x00;

            uCodeNum <<= 2;
            m_cur_mb.LocalMacroblockInfo->cbp = (uint8_t) (m_cur_mb.LocalMacroblockInfo->cbp | (uCodeNum & 0x30));
        }
    }
    // the macroblock has PCM type
    else
        m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_PCM;

} // void H264SegmentDecoder::DecodeMBTypeISlice_CAVLC(void)

void H264SegmentDecoder::DecodeMBTypePSlice_CAVLC(void)
{
    uint32_t uCodeNum;

    uCodeNum = m_pBitStream->GetVLCElement(false);

    // the macroblock has inter type
    if (5 > uCodeNum)
    {
        switch (uCodeNum)
        {
        case 0:
            m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_FORWARD;
            break;

        case 1:
            m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_16x8;
            break;

        case 2:
            m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_8x16;
            break;

        case 3:
        case 4:
            m_cur_mb.GlobalMacroblockInfo->mbtype = (uint8_t) ((uCodeNum == 4) ?
                                                             MBTYPE_INTER_8x8_REF0 :
                                                             MBTYPE_INTER_8x8);
            // read subblock types
            {
                int32_t subblock;
                int32_t sbtype;

                for (subblock = 0; subblock < 4; subblock++)
                {
                    uCodeNum = m_pBitStream->GetVLCElement(false);

                    switch (uCodeNum)
                    {
                    case 0:
                        sbtype = SBTYPE_8x8;
                        break;

                    case 1:
                        sbtype = SBTYPE_8x4;
                        break;

                    case 2:
                        sbtype = SBTYPE_4x8;
                        break;

                    case 3:
                        sbtype = SBTYPE_4x4;
                        break;

                    default:
                        throw h264_exception(UMC_ERR_INVALID_STREAM);
                    }

                    m_cur_mb.GlobalMacroblockInfo->sbtype[subblock] = (uint8_t) sbtype;
                }
            }
            break;
        }
    }
    else
    {
        uCodeNum -= 5;

        // the macroblock has I_NxN type
        if (0 == uCodeNum)
            m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA;
        // the macroblock has I_16x16 type
        else if (25 != uCodeNum)
        {
            m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA_16x16;
            IntraType *pMBIntraTypes = m_pMBIntraTypes + m_CurMBAddr*NUM_INTRA_TYPE_ELEMENTS;

            pMBIntraTypes[0] =
            pMBIntraTypes[1] =
            pMBIntraTypes[2] =
            pMBIntraTypes[3] = (uCodeNum - 1) & 0x03;

            // set the CBP
            {
                uCodeNum -= 1;
                if (12 <= uCodeNum)
                {
                    m_cur_mb.LocalMacroblockInfo->cbp = 0x0f;
                    uCodeNum -= 12;
                }
                else
                    m_cur_mb.LocalMacroblockInfo->cbp = 0x00;

                uCodeNum <<= 2;
                m_cur_mb.LocalMacroblockInfo->cbp = (uint8_t) (m_cur_mb.LocalMacroblockInfo->cbp | (uCodeNum & 0x30));
            }
        }
        // the macroblock has PCM type
        else
            m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_PCM;
    }

} // void H264SegmentDecoder::DecodeMBTypePSlice_CAVLC(void)

void H264SegmentDecoder::DecodeMBTypeBSlice_CAVLC(void)
{
    uint32_t uCodeNum;

    uCodeNum = m_pBitStream->GetVLCElement(false);

    // the macroblock has inter type
    if (uCodeNum < 23)
    {
        m_cur_mb.GlobalMacroblockInfo->mbtype = CodeToMBTypeB[uCodeNum];
        if (m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_DIRECT)
        {
            pSetMBDirectFlag(m_cur_mb.GlobalMacroblockInfo);
            memset(m_cur_mb.GlobalMacroblockInfo->sbtype, 0, sizeof(m_cur_mb.GlobalMacroblockInfo->sbtype));
        }
        else if (m_cur_mb.GlobalMacroblockInfo->mbtype  == MBTYPE_INTER_16x8 ||
            m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x16)
        {
            // direction for the two subblocks
            m_cur_mb.LocalMacroblockInfo->sbdir[0] = CodeToBDir[(uCodeNum-4)>>1][0];
            m_cur_mb.LocalMacroblockInfo->sbdir[1] = CodeToBDir[(uCodeNum-4)>>1][1];
        }
        else if (m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8)
        {
            // read subblock types and prediction direction
            uint32_t subblock;

            for (subblock = 0; subblock < 4; subblock++)
            {
                uCodeNum = m_pBitStream->GetVLCElement(false);
                if (uCodeNum < 13)
                {
                    m_cur_mb.GlobalMacroblockInfo->sbtype[subblock] =  CodeToSBTypeAndDir[uCodeNum].type;
                    m_cur_mb.LocalMacroblockInfo->sbdir[subblock] = CodeToSBTypeAndDir[uCodeNum].dir;
                }
                else
                    throw h264_exception(UMC_ERR_INVALID_STREAM);
            }
        }
    }
    else
    {
        uCodeNum -= 23;

        // the macroblock has I_NxN type
        if (0 == uCodeNum)
            m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA;
        // the macroblock has I_16x16 type
        else if (25 != uCodeNum)
        {
            m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA_16x16;
            IntraType *pMBIntraTypes = m_pMBIntraTypes + m_CurMBAddr*NUM_INTRA_TYPE_ELEMENTS;

            pMBIntraTypes[0] =
            pMBIntraTypes[1] =
            pMBIntraTypes[2] =
            pMBIntraTypes[3] = (uCodeNum - 1) & 0x03;

            // set the CBP
            {
                uCodeNum -= 1;
                if (12 <= uCodeNum)
                {
                    m_cur_mb.LocalMacroblockInfo->cbp = 0x0f;
                    uCodeNum -= 12;
                }
                else
                    m_cur_mb.LocalMacroblockInfo->cbp = 0x00;

                uCodeNum <<= 2;
                m_cur_mb.LocalMacroblockInfo->cbp = (uint8_t) (m_cur_mb.LocalMacroblockInfo->cbp | (uCodeNum & 0x30));
            }
        }
        // the macroblock has PCM type
        else
            m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_PCM;
    }

} // void H264SegmentDecoder::DecodeMBTypeBSlice_CAVLC(void)

void H264SegmentDecoder::DecodeMacroBlockType(IntraType *pMBIntraTypes,
                                                int32_t *MBSkipCount, // On entry if < 0, run of skipped MBs has just been completed
                                                                     // On return, zero or skip MB run count read from bitstream
                                                int32_t *PassFDFDecode)
{
    uint32_t uCodeNum;

    // interpretation of code depends upon slice type
    if (m_pSliceHeader->slice_type == INTRASLICE)
    {
        if (m_isMBAFF)
        {
            if ((m_CurMBAddr & 1)==0)
            {
                uint32_t bit = m_pBitStream->Get1Bit();
                pSetPairMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo,m_cur_mb.GlobalMacroblockPairInfo,bit);
            }
        }
        else
        {
            pSetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo,0);
        }
        uCodeNum = m_pBitStream->GetVLCElement(false);
        if (uCodeNum == 0)
             m_cur_mb.GlobalMacroblockInfo->mbtype =  MBTYPE_INTRA;
        else if (uCodeNum == 25)
            m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_PCM;
        else
        {
            m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA_16x16;
            uCodeNum--;
        }
    }   // intra
    else
    {   // not Intra

        if (*MBSkipCount >= 0) //actually it has to be = 0
        {
            VM_ASSERT(*MBSkipCount<=0);
            uCodeNum = m_pBitStream->GetVLCElement(false);
            // skipped MB count
            if (uCodeNum)
            {
                *PassFDFDecode = 0;
                *MBSkipCount = uCodeNum;
                m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;
                return;
            }
        }
        else
        {
            // first MB after run of skipped MBs, no new skip count
            // in bitstream to read, clear MBSkipCount to detect next skip run
            *MBSkipCount = 0;
        }

        if (m_isMBAFF)
        {
            if (*PassFDFDecode==0)
            {
                uint32_t bit = m_pBitStream->Get1Bit();
                pSetPairMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo,m_cur_mb.GlobalMacroblockPairInfo,bit);
                *PassFDFDecode = 1;
            }
            else
            {
                *PassFDFDecode = 0;
            }
        }
        else
        {
            pSetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo,0);
            *PassFDFDecode = 0;
        }
        uCodeNum = m_pBitStream->GetVLCElement(false);

        if (m_pSliceHeader->slice_type == PREDSLICE)
        {
            switch (uCodeNum)
            {
            case 0:
                // 16x16
                m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_FORWARD;
                break;
            case 1:
                // 16x8
                m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_16x8;
                break;
            case 2:
                // 8x16
                m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_8x16;
                break;
            case 3:
            case 4:
                // 8x8
                m_cur_mb.GlobalMacroblockInfo->mbtype = (uint8_t) ((uCodeNum == 4) ? MBTYPE_INTER_8x8_REF0 : MBTYPE_INTER_8x8);
                {
                    // read subblock types
                    uint32_t subblock;
                    uint8_t sbtype;

                    for (subblock=0; subblock<4; subblock++)
                    {
                        uCodeNum = m_pBitStream->GetVLCElement(false);
                        switch (uCodeNum)
                        {
                        case 0:
                            sbtype = SBTYPE_8x8;
                            break;
                        case 1:
                            sbtype = SBTYPE_8x4;
                            break;
                        case 2:
                            sbtype = SBTYPE_4x8;
                            break;
                        case 3:
                            sbtype = SBTYPE_4x4;
                            break;
                        default:
                            sbtype = (uint8_t) -1;
                            throw h264_exception(UMC_ERR_INVALID_STREAM);
                            break;
                        }
                        m_cur_mb.GlobalMacroblockInfo->sbtype[subblock] = sbtype;

                    }   // for subblock
                }   // 8x8 subblocks
                break;
            case 5:
                m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA;
                break;
            default:
                if (uCodeNum < 30)
                {
                    m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA_16x16;
                    uCodeNum -= 6;
                }
                else if (uCodeNum == 30)
                {
                    m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_PCM;
                }
                else
                {
                    throw h264_exception(UMC_ERR_INVALID_STREAM);
                }
                break;
            }
        }   // P frame
        else if (m_pSliceHeader->slice_type == BPREDSLICE)
        {
            if (uCodeNum < 23)
            {
                m_cur_mb.GlobalMacroblockInfo->mbtype = CodeToMBTypeB[uCodeNum];
                if (m_cur_mb.GlobalMacroblockInfo->mbtype  == MBTYPE_INTER_16x8 ||
                    m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x16)
                {
                    // direction for the two subblocks
                    m_cur_mb.LocalMacroblockInfo->sbdir[0] = CodeToBDir[(uCodeNum-4)>>1][0];
                    m_cur_mb.LocalMacroblockInfo->sbdir[1] = CodeToBDir[(uCodeNum-4)>>1][1];
                }
                if (m_cur_mb.GlobalMacroblockInfo->mbtype  == MBTYPE_INTER_8x8 || m_cur_mb.GlobalMacroblockInfo->mbtype  == MBTYPE_INTER_8x8_REF0)
                {
                    // read subblock types and prediction direction
                    uint32_t subblock;
                    for (subblock=0; subblock<4; subblock++)
                    {
                        uCodeNum = m_pBitStream->GetVLCElement(false);
                        if (uCodeNum < 13)
                        {
                            m_cur_mb.GlobalMacroblockInfo->sbtype[subblock] =  CodeToSBTypeAndDir[uCodeNum].type;
                            m_cur_mb.LocalMacroblockInfo->sbdir[subblock] = CodeToSBTypeAndDir[uCodeNum].dir;
                        }
                        else
                        {
                            throw h264_exception(UMC_ERR_INVALID_STREAM);
                        }
                    }   // for subblock
                }   // 8x8 subblocks
            }
            else if (uCodeNum == 23)
                m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA;
            else if (uCodeNum < 48)
            {
                    m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA_16x16;
                    uCodeNum -= 24;
            }
            else if (uCodeNum == 48)
            {
                m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_PCM;
            }
            else
            {
                throw h264_exception(UMC_ERR_INVALID_STREAM);
            }
        }   // B frame
        else
        {
            throw h264_exception(UMC_ERR_INVALID_STREAM);
        }


    }   // not Intra

    if (m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA_16x16)
    {
        // 16x16 INTRA, code includes prediction mode and cbp info
        pMBIntraTypes[0] =
        pMBIntraTypes[1] =
        pMBIntraTypes[2] =
        pMBIntraTypes[3] = (IntraType)((uCodeNum) & 0x03);

        if (uCodeNum > 11)
        {
            m_cur_mb.LocalMacroblockInfo->cbp = 0x0f;
            uCodeNum -= 12;
        }
        else
            m_cur_mb.LocalMacroblockInfo->cbp = 0x00;
        uCodeNum <<= 2;
        m_cur_mb.LocalMacroblockInfo->cbp = (uint8_t) (m_cur_mb.LocalMacroblockInfo->cbp | (uCodeNum & 0x30));

    } // INTRA_16x16
} // void H264SegmentDecoder::DecodeMacroBlockType(uint32_t *pMBIntraTypes,

void H264SegmentDecoder::DecodeIntraTypes4x4_CAVLC(IntraType *pMBIntraTypes,
                                                     bool bUseConstrainedIntra)
{
    uint32_t block;
    // Temp arrays for modes from above and left, initially filled from
    // outside the MB, then updated with modes within the MB
    uint32_t uModeAbove[4];
    uint32_t uModeLeft[4];
    uint32_t uPredMode;        // predicted mode for current 4x4 block
    uint32_t uBSMode;          // mode bits from bitstream

    IntraType *pRefIntraTypes;
    uint32_t uLeftIndex;      // indexes into mode arrays, dependent on 8x8 block
    uint32_t uAboveIndex;
    H264DecoderMacroblockGlobalInfo *gmbinfo=m_gmbinfo->mbs;
    uint32_t predictors=31;//5 lsb bits set
    //new version
    {
        // above, left MB available only if they are INTRA
        if ((m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num<0) || ((!IS_INTRA_MBTYPE(gmbinfo[m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num].mbtype) && bUseConstrainedIntra)))
            predictors &= (~1);//clear 1-st bit
        if ((m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num<0) || ((!IS_INTRA_MBTYPE(gmbinfo[m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num].mbtype) && bUseConstrainedIntra)))
            predictors &= (~2); //clear 2-nd bit
        if ((m_cur_mb.CurrentBlockNeighbours.mbs_left[1].mb_num<0) || ((!IS_INTRA_MBTYPE(gmbinfo[m_cur_mb.CurrentBlockNeighbours.mbs_left[1].mb_num].mbtype) && bUseConstrainedIntra)))
            predictors &= (~4); //clear 3-rd bit
        if ((m_cur_mb.CurrentBlockNeighbours.mbs_left[2].mb_num<0) || ((!IS_INTRA_MBTYPE(gmbinfo[m_cur_mb.CurrentBlockNeighbours.mbs_left[2].mb_num].mbtype) && bUseConstrainedIntra)))
            predictors &= (~8); //clear 4-th bit
        if ((m_cur_mb.CurrentBlockNeighbours.mbs_left[3].mb_num<0) || ((!IS_INTRA_MBTYPE(gmbinfo[m_cur_mb.CurrentBlockNeighbours.mbs_left[3].mb_num].mbtype) && bUseConstrainedIntra)))
            predictors &= (~16); //clear 5-th bit
    }

    // Get modes of blocks above and to the left, substituting 0
    // when above or to left is outside this MB slice. Substitute mode 2
    // when the adjacent macroblock is not 4x4 INTRA. Add 1 to actual
    // modes, so mode range is 1..9.

    if (predictors&1)
    {
        if (gmbinfo[m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num].mbtype == MBTYPE_INTRA)
        {
            pRefIntraTypes = m_pMBIntraTypes + m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num * NUM_INTRA_TYPE_ELEMENTS;
            uModeAbove[0] = pRefIntraTypes[10] + 1;
            uModeAbove[1] = pRefIntraTypes[11] + 1;
            uModeAbove[2] = pRefIntraTypes[14] + 1;
            uModeAbove[3] = pRefIntraTypes[15] + 1;
        }
        else
        {   // MB above in slice but not INTRA, use mode 2 (+1)
            uModeAbove[0] = uModeAbove[1] = uModeAbove[2] = uModeAbove[3] = 2 + 1;
        }
    }
    else
    {
        uModeAbove[0] = uModeAbove[1] = uModeAbove[2] = uModeAbove[3] = 0;
    }

    int32_t mask = 2;
    for(int32_t i = 0; i < 4; i++)
    {
        int32_t mb_num = m_cur_mb.CurrentBlockNeighbours.mbs_left[i].mb_num;
        if (predictors & mask)
        {
            if (gmbinfo[mb_num].mbtype  == MBTYPE_INTRA)
            {
                pRefIntraTypes = m_pMBIntraTypes + mb_num*NUM_INTRA_TYPE_ELEMENTS;
                uModeLeft[i] = pRefIntraTypes[NIT2LIN[m_cur_mb.CurrentBlockNeighbours.mbs_left[i].block_num]] + 1;
            }
            else
            { // MB left in slice but not INTRA, use mode 2 (+1)
                uModeLeft[i] = 2+1;
            }
        }
        else
        {
            uModeLeft[i] = 0;
        }

        mask = mask << 1;
    }

    for (block = 0; block < 4; block++)
    {
        uAboveIndex = (block & 1) * 2;      // 0,2,0,2
        uLeftIndex = (block & 2);           // 0,0,2,2

        // upper left 4x4

        // Predicted mode is minimum of the above and left modes, or
        // mode 2 if above or left is outside slice, indicated by 0 in
        // mode array.
        uPredMode = MFX_MIN(uModeLeft[uLeftIndex], uModeAbove[uAboveIndex]);
        if (uPredMode)
            uPredMode--;
        else
            uPredMode = 2;

        // If next bitstream bit is 1, use predicted mode, else read new mode
        if (m_pBitStream->Peek1Bit() == 0)
        {
            // get 3 more bits to determine new mode
            uBSMode = m_pBitStream->GetBits(4);
            if (uBSMode < uPredMode)
                uPredMode = uBSMode;
            else
                uPredMode = uBSMode + 1;
        }
        else
        {
            m_pBitStream->Drop1Bit();
        }

        // Save mode
        pMBIntraTypes[0] = (IntraType)uPredMode;
        uModeAbove[uAboveIndex] = uPredMode + 1;

        // upper right 4x4
        uPredMode = MFX_MIN(uPredMode + 1, uModeAbove[uAboveIndex + 1]);
        if (uPredMode)
            uPredMode--;
        else
            uPredMode = 2;

        if (m_pBitStream->Peek1Bit() == 0)
        {
            uBSMode = m_pBitStream->GetBits(4);
            if (uBSMode < uPredMode)
                uPredMode = uBSMode;
            else
                uPredMode = uBSMode + 1;
        }
        else
        {
            m_pBitStream->Drop1Bit();
        }

        pMBIntraTypes[1] = (IntraType)uPredMode;
        uModeAbove[uAboveIndex+1] = uPredMode + 1;
        uModeLeft[uLeftIndex] = uPredMode + 1;

        // lower left 4x4
        uPredMode = MFX_MIN(uModeLeft[uLeftIndex+1], uModeAbove[uAboveIndex]);
        if (uPredMode)
            uPredMode--;
        else
            uPredMode = 2;

        if (m_pBitStream->Peek1Bit() == 0)
        {
            uBSMode = m_pBitStream->GetBits(4);
            if (uBSMode < uPredMode)
                uPredMode = uBSMode;
            else
                uPredMode = uBSMode + 1;
        }
        else
        {
            m_pBitStream->Drop1Bit();
        }

        pMBIntraTypes[2] = (IntraType)uPredMode;
        uModeAbove[uAboveIndex] = uPredMode + 1;

        // lower right 4x4 (above and left must always both be in slice)
        uPredMode = MFX_MIN(uPredMode + 1, uModeAbove[uAboveIndex + 1]) - 1;

        if (m_pBitStream->Peek1Bit() == 0)
        {
            uBSMode = m_pBitStream->GetBits(4);
            if (uBSMode < uPredMode)
                uPredMode = uBSMode;
            else
                uPredMode = uBSMode + 1;
        }
        else
        {
            m_pBitStream->Drop1Bit();
        }

        pMBIntraTypes[3]             = (IntraType)uPredMode;
        uModeAbove   [uAboveIndex+1] = uPredMode + 1;
        uModeLeft    [uLeftIndex+1]  = uPredMode + 1;

        pMBIntraTypes += 4;
    }   // block

} // void H264SegmentDecoder::DecodeIntraTypes4x4_CAVLC(uint32_t *pMBIntraTypes,

void H264SegmentDecoder::DecodeIntraTypes8x8_CAVLC(IntraType *pMBIntraTypes,
                                                     bool bUseConstrainedIntra)
{
    uint32_t uModeAbove[2];
    uint32_t uModeLeft[2];
    uint32_t uPredMode;        // predicted mode for current 4x4 block
    uint32_t uBSMode;          // mode bits from bitstream

    IntraType *pRefIntraTypes;
    uint32_t uLeftIndex;      // indexes into mode arrays, dependent on 8x8 block
    uint32_t uAboveIndex;
    H264DecoderMacroblockGlobalInfo *gmbinfo=m_gmbinfo->mbs;
    uint32_t predictors=31;//5 lsb bits set
    //new version
    {
        // above, left MB available only if they are INTRA
        if ((m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num<0) || ((!IS_INTRA_MBTYPE(gmbinfo[m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num].mbtype) && bUseConstrainedIntra)))
            predictors &= (~1);//clear 1-st bit
        if ((m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num<0) || ((!IS_INTRA_MBTYPE(gmbinfo[m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num].mbtype) && bUseConstrainedIntra)))
            predictors &= (~2); //clear 2-nd bit
        if ((m_cur_mb.CurrentBlockNeighbours.mbs_left[2].mb_num<0) || ((!IS_INTRA_MBTYPE(gmbinfo[m_cur_mb.CurrentBlockNeighbours.mbs_left[2].mb_num].mbtype) && bUseConstrainedIntra)))
            predictors &= (~4); //clear 4-th bit
    }

    // Get modes of blocks above and to the left, substituting 0
    // when above or to left is outside this MB slice. Substitute mode 2
    // when the adjacent macroblock is not 4x4 INTRA. Add 1 to actual
    // modes, so mode range is 1..9.

    if (predictors&1)
    {
        if (gmbinfo[m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num].mbtype == MBTYPE_INTRA)
        {
            pRefIntraTypes = m_pMBIntraTypes + m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num * NUM_INTRA_TYPE_ELEMENTS;
            uModeAbove[0] = pRefIntraTypes[10] + 1;
            uModeAbove[1] = pRefIntraTypes[14] + 1;
        }
        else
        {   // MB above in slice but not INTRA, use mode 2 (+1)
            uModeAbove[0] = uModeAbove[1] = 2 + 1;
        }
    }
    else
    {
        uModeAbove[0] = uModeAbove[1] = 0;
    }
    int32_t mask = 2;
    for(int32_t i = 0; i < 2; i++)
    {
        if (predictors & mask)
        {
            if (gmbinfo[m_cur_mb.CurrentBlockNeighbours.mbs_left[2*i].mb_num].mbtype  == MBTYPE_INTRA)
            {
                pRefIntraTypes = m_pMBIntraTypes + m_cur_mb.CurrentBlockNeighbours.mbs_left[2*i].mb_num*NUM_INTRA_TYPE_ELEMENTS;
                uModeLeft[i] = pRefIntraTypes[NIT2LIN[m_cur_mb.CurrentBlockNeighbours.mbs_left[2*i].block_num]] + 1;
            }
            else
            {   // MB left in slice but not INTRA, use mode 2 (+1)
                uModeLeft[i] = 2+1;
            }
        }
        else
        {
            uModeLeft[i] = 0;
        }

        mask = mask << 1;
    }

    uAboveIndex = 0;
    uLeftIndex = 0;

    // upper left 8x8

    // Predicted mode is minimum of the above and left modes, or
    // mode 2 if above or left is outside slice, indicated by 0 in
    // mode array.
    uPredMode = MFX_MIN(uModeLeft[uLeftIndex], uModeAbove[uAboveIndex]);
    if (uPredMode)
        uPredMode--;
    else
        uPredMode = 2;

    // If next bitstream bit is 1, use predicted mode, else read new mode
    if (m_pBitStream->Get1Bit() == 0)
    {
        // get 3 more bits to determine new mode
        uBSMode = m_pBitStream->GetBits(3);
        if (uBSMode < uPredMode)
            uPredMode = uBSMode;
        else
            uPredMode = uBSMode + 1;
    }
    // Save mode
    pMBIntraTypes[0] =
    pMBIntraTypes[1] =
    pMBIntraTypes[2] =
    pMBIntraTypes[3] =
        (IntraType)uPredMode;
    uModeAbove[uAboveIndex] = uPredMode + 1;

    // upper right 8x8
    uPredMode = MFX_MIN(uPredMode+1, uModeAbove[uAboveIndex+1]);
    if (uPredMode)
        uPredMode--;
    else
        uPredMode = 2;

    if (m_pBitStream->Get1Bit() == 0)
    {
        uBSMode = m_pBitStream->GetBits(3);
        if (uBSMode < uPredMode)
            uPredMode = uBSMode;
        else
            uPredMode = uBSMode + 1;
    }

    pMBIntraTypes[4] =
    pMBIntraTypes[5] =
    pMBIntraTypes[6] =
    pMBIntraTypes[7] =
        (IntraType)uPredMode;
    uModeAbove[uAboveIndex+1] = uPredMode + 1;
    uModeLeft[uLeftIndex] = uPredMode + 1;

    // lower left 4x4
    uPredMode = MFX_MIN(uModeLeft[uLeftIndex+1], uModeAbove[uAboveIndex]);
    if (uPredMode)
        uPredMode--;
    else
        uPredMode = 2;

    if (m_pBitStream->Get1Bit() == 0)
    {
        uBSMode = m_pBitStream->GetBits(3);
        if (uBSMode < uPredMode)
            uPredMode = uBSMode;
        else
            uPredMode = uBSMode + 1;
    }
    pMBIntraTypes[8] =
    pMBIntraTypes[9] =
    pMBIntraTypes[10] =
    pMBIntraTypes[11] =
     (IntraType)uPredMode;
    uModeAbove[uAboveIndex] = uPredMode + 1;

    // lower right 4x4 (above and left must always both be in slice)
    uPredMode = MFX_MIN(uPredMode+1, uModeAbove[uAboveIndex+1]) - 1;

    if (m_pBitStream->Get1Bit() == 0)
    {
        uBSMode = m_pBitStream->GetBits(3);
        if (uBSMode < uPredMode)
            uPredMode = uBSMode;
        else
            uPredMode = uBSMode + 1;
    }
    pMBIntraTypes[12] =
    pMBIntraTypes[13] =
    pMBIntraTypes[14] =
    pMBIntraTypes[15] =
        (IntraType)uPredMode;
    uModeAbove   [uAboveIndex+1] = uPredMode + 1;
    uModeLeft    [uLeftIndex+1]  = uPredMode + 1;

    // copy last IntraTypes to first 4 for reconstruction since they're not used for further prediction
    pMBIntraTypes[1] = pMBIntraTypes[4];
    pMBIntraTypes[2] = pMBIntraTypes[8];
    pMBIntraTypes[3] = pMBIntraTypes[12];

} // void H264SegmentDecoder::DecodeIntraTypes8x8_CAVLC(uint32_t *pMBIntraTypes,

uint32_t H264SegmentDecoder::DecodeCBP_CAVLC(uint32_t color_format)
{
   uint32_t cbp;

    uint32_t index = (uint32_t) m_pBitStream->GetVLCElement(false);


    if (index < 48)
    {
        if (MBTYPE_INTRA == m_cur_mb.GlobalMacroblockInfo->mbtype)
            cbp = dec_cbp_intra[0 != color_format][index];
        else
            cbp = dec_cbp_inter[0 != color_format][index];
    }
    else
    {
        throw h264_exception(UMC_ERR_INVALID_STREAM);
    }

    if (!cbp)
    {
        m_cur_mb.LocalMacroblockInfo->cbp4x4_luma = 0;
        m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0] = 0;
        m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1] = 0;
        m_prev_dquant = 0;
    }

    return cbp;

} // uint32_t H264SegmentDecoder::DecodeCBP_CAVLC(uint32_t color_format)

void H264SegmentDecoder::DecodeEdgeType()
{
    uint8_t edge_type = 0;
    uint8_t edge_type_2t = 0;
    uint8_t edge_type_2b = 0;
    uint8_t special_MBAFF_case =0;

    int32_t nLeft = m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num;
    int32_t nTop = m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num;
    int32_t nTopLeft = m_cur_mb.CurrentBlockNeighbours.mb_above_left.mb_num;
    int32_t nTopRight = m_cur_mb.CurrentBlockNeighbours.mb_above_right.mb_num;
    if (m_isMBAFF)
    {
        int32_t currmb_fdf = pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
        if (m_IsUseConstrainedIntra)
        {
            int32_t mbA_fdf=1;
            int32_t mbA_is_intra=0;
            int32_t mbpA_is_intra=0;

            if (nLeft >= 0)
            {
                mbA_fdf = GetMBFieldDecodingFlag(m_gmbinfo->mbs[m_cur_mb.CurrentMacroblockNeighbours.mb_A]);
                mbA_is_intra = IS_INTRA_MBTYPE(m_gmbinfo->mbs[m_cur_mb.CurrentMacroblockNeighbours.mb_A].mbtype);
                mbpA_is_intra = IS_INTRA_MBTYPE(m_gmbinfo->mbs[m_cur_mb.CurrentMacroblockNeighbours.mb_A+1].mbtype);
            }

            if (currmb_fdf) //current mb coded as field MB
            {
                if (!mbA_fdf) //(special case is allowed only in this branch)
                {
                    if (mbA_is_intra && !mbpA_is_intra) special_MBAFF_case = 1;//only 2 top blocks can use left samples
                    if (!mbA_is_intra && mbpA_is_intra) special_MBAFF_case = 2;//only 2 bottom blocks can use left samples
                }
            }
            else
            {
                if (mbA_fdf) //(special case is allowed only in this branch)
                {
                    //if (mbA_is_intra && !mbpA_is_intra) special_MBAFF_case = 1;//only 2 top blocks can use left samples
                    if (!mbA_is_intra && mbpA_is_intra) special_MBAFF_case = 2;//only 2 bottom blocks can use left samples
                }
            }

            switch (special_MBAFF_case)
            {
            case 1:
                if (0 > nTop)
                    edge_type_2t |= IPPVC_TOP_EDGE;
                else
                    if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nTop].mbtype)) edge_type_2t |= IPPVC_TOP_EDGE;

                if (0 > nTopLeft)
                    edge_type_2t |= IPPVC_TOP_LEFT_EDGE;
                else
                    if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nTopLeft].mbtype)) edge_type_2t |= IPPVC_TOP_LEFT_EDGE;

                if (0 > nTopRight)
                    edge_type_2t |= IPPVC_TOP_RIGHT_EDGE;
                else
                    if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nTopRight].mbtype)) edge_type_2t |= IPPVC_TOP_RIGHT_EDGE;

                if (currmb_fdf)
                {
                    edge_type_2b = IPPVC_LEFT_EDGE | IPPVC_TOP_RIGHT_EDGE;
                }
                else
                {
                    nLeft = m_cur_mb.CurrentBlockNeighbours.mbs_left[2].mb_num;
                    if (0 > nLeft)
                        edge_type_2b |= IPPVC_LEFT_EDGE;
                    else
                    {
                        if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nLeft].mbtype)) edge_type_2b |= IPPVC_LEFT_EDGE;
                        if (mbA_fdf && !currmb_fdf && (!mbpA_is_intra || !mbA_is_intra)) edge_type_2b |= IPPVC_LEFT_EDGE;
                    }

                    edge_type_2b |= IPPVC_TOP_RIGHT_EDGE;
                    if (!mbpA_is_intra)
                        edge_type_2b |= IPPVC_TOP_LEFT_EDGE;
                }
                break;
            case 2:
                if (0 > nLeft)
                    edge_type_2t |= IPPVC_LEFT_EDGE;
                else
                {
                    if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nLeft].mbtype)) edge_type_2t |= IPPVC_LEFT_EDGE;
                    if (mbA_fdf && !currmb_fdf && (!mbpA_is_intra || !mbA_is_intra)) edge_type_2t |= IPPVC_LEFT_EDGE;
                }

                if (0 > nTop)
                    edge_type_2t |= IPPVC_TOP_EDGE;
                else
                    if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nTop].mbtype)) edge_type_2t |= IPPVC_TOP_EDGE;

                if (0 > nTopLeft)
                    edge_type_2t |= IPPVC_TOP_LEFT_EDGE;
                else
                    if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nTopLeft].mbtype)) edge_type_2t |= IPPVC_TOP_LEFT_EDGE;

                if (0 > nTopRight)
                    edge_type_2t |= IPPVC_TOP_RIGHT_EDGE;
                else
                    if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nTopRight].mbtype)) edge_type_2t |= IPPVC_TOP_RIGHT_EDGE;

                if (currmb_fdf)
                {
                    edge_type_2b = IPPVC_TOP_LEFT_EDGE | IPPVC_TOP_RIGHT_EDGE;
                }
                else
                {
                    nLeft = m_cur_mb.CurrentBlockNeighbours.mbs_left[2].mb_num;
                    if (0 > nLeft)
                        edge_type_2b |= IPPVC_LEFT_EDGE;
                    else
                    {
                        if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nLeft].mbtype)) edge_type_2b |= IPPVC_LEFT_EDGE;
                        if (mbA_fdf && !currmb_fdf && (!mbpA_is_intra || !mbA_is_intra)) edge_type_2b |= IPPVC_LEFT_EDGE;
                    }

                    edge_type_2b |= IPPVC_TOP_RIGHT_EDGE;
                    if (!mbpA_is_intra)
                        edge_type_2b |= IPPVC_TOP_LEFT_EDGE;
                }
                break;
            default:
                if (0 > nLeft)
                    edge_type |= IPPVC_LEFT_EDGE;
                else
                {
                    if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nLeft].mbtype)) edge_type |= IPPVC_LEFT_EDGE;
                    if (mbA_fdf && !currmb_fdf && (!mbpA_is_intra || !mbA_is_intra)) edge_type |= IPPVC_LEFT_EDGE;
                }
                if (0 > nTop)
                    edge_type |= IPPVC_TOP_EDGE;
                else
                    if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nTop].mbtype)) edge_type |= IPPVC_TOP_EDGE;
                if (0 > nTopLeft)
                    edge_type |= IPPVC_TOP_LEFT_EDGE;
                else
                    if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nTopLeft].mbtype)) edge_type |= IPPVC_TOP_LEFT_EDGE;
                if (0 > nTopRight)
                    edge_type |= IPPVC_TOP_RIGHT_EDGE;
                else
                    if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nTopRight].mbtype)) edge_type |= IPPVC_TOP_RIGHT_EDGE;
                break;
            }
        }
        else
        {
            if (0 > nLeft)
                edge_type |= IPPVC_LEFT_EDGE;
            if (0 > nTop)
                edge_type |= IPPVC_TOP_EDGE;
            if (0 > nTopLeft)
                edge_type |= IPPVC_TOP_LEFT_EDGE;
            if (0 > nTopRight)
                edge_type |= IPPVC_TOP_RIGHT_EDGE;

        }

        if (special_MBAFF_case)
        {
            m_mbinfo.mbs[m_CurMBAddr].IntraTypes.edge_type = (uint16_t) ((edge_type_2t << 8) | edge_type_2b);
            m_mbinfo.mbs[m_CurMBAddr].IntraTypes.edge_type |= 0x8000;
        }
        else
        {
            m_mbinfo.mbs[m_CurMBAddr].IntraTypes.edge_type = edge_type;
        }
    }
    else
    {
        if (m_IsUseConstrainedIntra)
        {
            if (0 > nLeft)
                edge_type |= IPPVC_LEFT_EDGE;
            else
                if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nLeft].mbtype)) edge_type |= IPPVC_LEFT_EDGE;
            if (0 > nTop)
                edge_type |= IPPVC_TOP_EDGE;
            else
                if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nTop].mbtype)) edge_type |= IPPVC_TOP_EDGE;
            if (0 > nTopLeft)
                edge_type |= IPPVC_TOP_LEFT_EDGE;
            else
                if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nTopLeft].mbtype)) edge_type |= IPPVC_TOP_LEFT_EDGE;
            if (0 > nTopRight)
                edge_type |= IPPVC_TOP_RIGHT_EDGE;
            else
                if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nTopRight].mbtype)) edge_type |= IPPVC_TOP_RIGHT_EDGE;
        }
        else
        {
            if (0 > nLeft)
                edge_type |= IPPVC_LEFT_EDGE;
            if (0 > nTop)
                edge_type |= IPPVC_TOP_EDGE;
            if (0 > nTopLeft)
                edge_type |= IPPVC_TOP_LEFT_EDGE;
            if (0 > nTopRight)
                edge_type |= IPPVC_TOP_RIGHT_EDGE;
        }

        m_cur_mb.LocalMacroblockInfo->IntraTypes.edge_type = edge_type;
    }

} // void H264SegmentDecoder::DecodeEdgeType()

void H264SegmentDecoder::ReconstructEdgeType(uint8_t &edge_type_2t, uint8_t &edge_type_2b, int32_t &special_MBAFF_case)
{
    if (m_cur_mb.LocalMacroblockInfo->IntraTypes.edge_type > 0xff)
    {
        edge_type_2t = (uint8_t)((m_cur_mb.LocalMacroblockInfo->IntraTypes.edge_type >> 8) & 0x7f);
        edge_type_2b = (uint8_t)(m_cur_mb.LocalMacroblockInfo->IntraTypes.edge_type & 0xff);
        //m_cur_mb.LocalMacroblockInfo->IntraTypes.edge_type |= 0x8000;

        special_MBAFF_case = (edge_type_2t & IPPVC_LEFT_EDGE) ? 2 : 1;
    }
    else
    {
        special_MBAFF_case = 0;
        edge_type_2t = (uint8_t)m_cur_mb.LocalMacroblockInfo->IntraTypes.edge_type;
    }
}

} // namespace UMC
#endif // MFX_ENABLE_H264_VIDEO_DECODE
