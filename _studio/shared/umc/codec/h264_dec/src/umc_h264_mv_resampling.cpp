/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2008-2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include <ippi.h>
#include "umc_h264_slice_decoding.h"
#include "umc_h264_segment_decoder_mt.h"

namespace UMC
{
#define INTRA -1
#define SBTYPE_UNDEF -1
#define MBTYPE_UNDEF -1

Ipp32s resTable0[16] = {0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15};
Ipp32s resTable1[16] = {0, 1, 2, 3, 4, 5, 6, 7};

Ipp32s shiftTable[] = { 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};

Ipp32s bicubicFilter[16][4] = {
    { 0, 32,  0,  0},
    {-1, 32,  2, -1},
    {-2, 31,  4, -1},
    {-3, 30,  6, -1},
    {-3, 28,  8, -1},
    {-4, 26, 11, -1},
    {-4, 24, 14, -2},
    {-3, 22, 16, -3},
    {-3, 19, 19, -3},
    {-3, 16, 22, -3},
    {-2, 14, 24, -4},
    {-1, 11, 26, -4},
    {-1,  8, 28, -3},
    {-1,  6, 30, -3},
    {-1,  4, 31, -2},
    {-1,  2, 32, -1}
};

Ipp32s bilinearFilter[16][2] = {
    {32,  0},
    {30,  2},
    {28,  4},
    {26,  6},
    {24,  8},
    {22, 10},
    {20, 12},
    {18, 14},
    {16, 16},
    {14, 18},
    {12, 20},
    {10, 22},
    {8, 24},
    {6, 26},
    {4, 28},
    {2, 30}
};

Ipp32s minPositive(Ipp32s x, Ipp32s y)
{
    if ((x >= 0) && (y >= 0))
    {
        return ((x < y) ? x : y);
    }
    else
    {
        return ((x > y) ? x : y);
    }
}

Ipp32s mvDiff(Ipp32s *mv1,
                     Ipp32s *mv2)
{
    Ipp32s tmp0 = mv1[0] - mv2[0];
    Ipp32s tmp1 = mv1[1] - mv2[1];

    if (tmp0 < 0) tmp0 = -tmp0;
    if (tmp1 < 0) tmp1 = -tmp1;

    return (tmp0 + tmp1);
}

const Ipp32s NxNPartIdx[6][4] =
{
  { 0, 0, 0, 0 }, // MODE_SKIP (P Slice)
  { 0, 0, 0, 0 }, // MODE_16x16     or    BLK_8x8
  { 0, 0, 2, 2 }, // MODE_16x8      or    BLK_8x4
  { 0, 1, 0, 1 }, // MODE_8x16      or    BLK_4x8
  { 0, 1, 2, 3 }, // MODE_8x8       or    BLK_4x4
  { 0, 1, 2, 3 }  // MODE_8x8ref0
};

int umc_h264_getAddr(Ipp32s m_isMBAFF, Ipp32s frame_width_in_mbs, Ipp32s i, Ipp32s j) {
    if (m_isMBAFF) {
        return (j/2 * (frame_width_in_mbs) + i) * 2 + (j & 1);
    } else {
        return j * (frame_width_in_mbs) + i;
    }
}

void umc_h264_MB_mv_resampling(H264SegmentDecoderMultiThreaded *sd,
                                      H264DecoderLayerDescriptor *curLayerD,
                                      Ipp32s curMB_X,
                                      Ipp32s curMB_Y,
                                      Ipp32s curMBAddr,
                                      Ipp32s slice_type,
                                      Ipp32s curWidth,
                                      Ipp32s curHeigh,
                                      Ipp32s refWidth,
                                      Ipp32s refHeigh,
                                      H264DecoderLayerDescriptor *refLayerMb,
                                      H264DecoderGlobalMacroblocksDescriptor *gmbinfo,
                                      H264DecoderLocalMacroblockDescriptor *mbinfo,
                                      Ipp32s scaleX,
                                      Ipp32s scaleY,
                                      Ipp32s RestrictedSpatialResolutionChangeFlag,
                                      Ipp32s CroppingChangeFlag,
                                      Ipp32s curFieldFlag)
{
    Ipp32s refLayerPartIdc[4][4];
    Ipp32s refIdxILPredL[2][2][2];
    Ipp32s tempRefIdxPredL[2][4][4];
    Ipp32s mvILPredL[2][4][4][2];
    Ipp32s tempMvAL[2][2];
    Ipp32s tempMvBL[2][2];
    H264DecoderMacroblockLocalInfo *lmbs;
    H264DecoderMacroblockGlobalInfo *gmbs;
    H264DecoderMacroblockMVs *MV[2];
    RefIndexType *RefIdxs[2];
    H264DecoderLayerResizeParameter *pResizeParameter;
    Ipp32s refMbType = 0;
    Ipp32s intraILPredFlag;
    Ipp32s mbtype, sbtype, subblock;
    Ipp32s maxX;
    Ipp32s x, y, i, j;
    Ipp32s fieldInFrameFlag = 0;
    Ipp32s mbStride = (refWidth >> 4);

    if (sd->m_isMBAFF && curFieldFlag)
        fieldInFrameFlag = 1;

    lmbs = &(mbinfo->mbs[curMBAddr]);
    gmbs = &(gmbinfo->mbs[curMBAddr]);
    MV[0] = &(gmbinfo->MV[0][curMBAddr]);
    MV[1] = &(gmbinfo->MV[1][curMBAddr]);
    RefIdxs[0] = GetRefIdxs(gmbinfo, 0, curMBAddr);
    RefIdxs[1] = GetRefIdxs(gmbinfo, 1, curMBAddr);
    pResizeParameter = curLayerD->m_pResizeParameter;

    maxX = 1;
    if (slice_type == BPREDSLICE)
        maxX = 2;

    Ipp32u refMBOffset = 0;
    H264DecoderMacroblockLayerInfo * refLayerLocalInfo = refLayerMb->mbs;
    if (refLayerMb->field_pic_flag && refLayerMb->bottom_field_flag)
    {
        refLayerLocalInfo += (refWidth * refHeigh) / 512;
        refMBOffset = (refWidth * refHeigh) / 512;
    }

    for (y = 0; y < 4; y++)
    {
        Ipp32s t_curMB_Y = ((curMB_Y >> fieldInFrameFlag) << (4 + fieldInFrameFlag)) + (curMB_Y & fieldInFrameFlag);

        Ipp32s yRef = t_curMB_Y + ((4 * y + 1) << fieldInFrameFlag);
        Ipp32s refMbPartIdxY;

        yRef = (yRef * pResizeParameter->scaleY + pResizeParameter->addY) >> pResizeParameter->shiftY;

        for (x = 0; x < 4; x++)
        {
            Ipp32s xRef = ((curMB_X * 16 + 4 * x + 1) * pResizeParameter->scaleX +
                pResizeParameter->addX) >> pResizeParameter->shiftX;
            Ipp32s refMbAddr = umc_h264_getAddr(refLayerMb->is_MbAff, mbStride, (xRef >> 4), (yRef >> 4));
            Ipp32s refMbPartIdxX, idx8x8Base, idx4x4Base, base8x8MbPartIdx, base4x4SubMbPartIdx;
            Ipp32s refFieldFlag = refLayerMb->field_pic_flag || GetMBFieldDecodingFlag(refLayerLocalInfo[refMbAddr]);
            Ipp32s mbOffset = 0;

            refMbPartIdxX = xRef & 15;

            if (!curLayerD->is_MbAff && !refLayerMb->is_MbAff) {
                refMbPartIdxY = yRef & 15;
            } else if (refFieldFlag == curFieldFlag) {
                refMbPartIdxY = ( yRef & ( 15 + 16 * curFieldFlag ) ) >> curFieldFlag;
            } else if (!curFieldFlag) {
                Ipp32s     iBaseTopMbY = ( (yRef >> 4) >> 1 ) << 1;
                Ipp32s topAddr = mbOffset + umc_h264_getAddr(refLayerMb->is_MbAff, mbStride, (xRef >> 4), iBaseTopMbY);
                Ipp32s botAddr = topAddr + 1;
                refMbAddr = IS_INTRA_MBTYPE(refLayerLocalInfo[topAddr].mbtype) ? botAddr: topAddr;
                refMbPartIdxY     = ( ( ( yRef >> 4 ) & 1 ) << 3 ) + ( ( ( yRef & 15 ) >> 3 ) << 2 );
            } else {
                refMbPartIdxY = yRef & 15;
                refMbPartIdxX = xRef & 15;
            }

            idx8x8Base    = ( (   refMbPartIdxY       >> 3 ) << 1 ) + (   refMbPartIdxX       >> 3 );
            idx4x4Base    = ( ( ( refMbPartIdxY & 7 ) >> 2 ) << 1 ) + ( ( refMbPartIdxX & 7 ) >> 2 );
            base8x8MbPartIdx     = 0;
            base4x4SubMbPartIdx  = 0;

            refMbType = refLayerLocalInfo[refMbAddr].mbtype;

            if (refLayerLocalInfo[refMbAddr].mbtype <= MBTYPE_PCM)
            {
                refLayerPartIdc[y][x] = INTRA;
                continue;
            }

            if( GetMBDirectSkipFlag(refLayerLocalInfo[refMbAddr]) && sd->m_pSliceHeader->slice_type == BPREDSLICE )
            {
                if( sd->m_pSliceHeader->ref_layer_dq_id == 0 )
                {
                    base8x8MbPartIdx     = idx8x8Base;
                    base4x4SubMbPartIdx  = idx4x4Base;
                }
            } else {
                Ipp32s eMbModeBase = 0;

                if (refLayerLocalInfo[refMbAddr].mbtype == MBTYPE_INTER_16x8)
                    eMbModeBase = 2;
                else if (refLayerLocalInfo[refMbAddr].mbtype == MBTYPE_INTER_8x16)
                    eMbModeBase = 3;
                else if (refLayerLocalInfo[refMbAddr].mbtype == MBTYPE_INTER_8x8 ||
                    refLayerLocalInfo[refMbAddr].mbtype == MBTYPE_INTER_8x8_REF0)
                    eMbModeBase = 4;
                else if (refLayerLocalInfo[refMbAddr].mbtype == MBTYPE_BIDIR ||
                    refLayerLocalInfo[refMbAddr].mbtype == MBTYPE_BACKWARD ||
                    refLayerLocalInfo[refMbAddr].mbtype == MBTYPE_FORWARD)
                    eMbModeBase = 1;

                base8x8MbPartIdx = NxNPartIdx[ eMbModeBase ][ idx8x8Base ];

                if( refLayerLocalInfo[refMbAddr].mbtype == MBTYPE_INTER_8x8 ||
                    refLayerLocalInfo[refMbAddr].mbtype == MBTYPE_INTER_8x8_REF0 )
                {
                    Ipp32s blkModeBase = 0;

                    if (refLayerLocalInfo[refMbAddr].sbtype[idx8x8Base] == SBTYPE_8x8)
                        blkModeBase = 1;
                    else if (refLayerLocalInfo[refMbAddr].sbtype[idx8x8Base] == SBTYPE_8x4)
                        blkModeBase = 2;
                    if (refLayerLocalInfo[refMbAddr].sbtype[idx8x8Base] == SBTYPE_4x8)
                        blkModeBase = 3;
                    if (refLayerLocalInfo[refMbAddr].sbtype[idx8x8Base] == SBTYPE_4x4)
                        blkModeBase = 4;

                    if(/* blkModeBase == 0*/refLayerLocalInfo[refMbAddr].sbdir[idx8x8Base] >= D_DIR_DIRECT)
                    {
                        if( sd->m_pSliceHeader->ref_layer_dq_id == 0 )
                        {
                            base4x4SubMbPartIdx = idx4x4Base;
                        }
                    }
                    else
                    {
                        base4x4SubMbPartIdx = NxNPartIdx[ blkModeBase ][ idx4x4Base ];
                    }
                }
            }

            Ipp32s base4x4BlkX    = ( ( base8x8MbPartIdx  & 1 ) << 1 ) + ( base4x4SubMbPartIdx  & 1 );
            Ipp32s base4x4BlkY    = ( ( base8x8MbPartIdx >> 1 ) << 1 ) + ( base4x4SubMbPartIdx >> 1 );
            Ipp32s base4x4BlkIdx  = ( base4x4BlkY << 2 ) + base4x4BlkX;
            refLayerPartIdc[y][x]           = 16 * refMbAddr + base4x4BlkIdx;
        }
    }

    intraILPredFlag = 1;

    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 4; x++)
        {
            if (refLayerPartIdc[y][x] != INTRA)
            {
                intraILPredFlag = 0;
                break;
            }
        }
        if (intraILPredFlag == 0)
        {
            break;
        }
    }

    if (intraILPredFlag == 1)
    {
        if (sd->m_spatial_resolution_change)
        {
            gmbs->mbtype = MBTYPE_INTRA;
        }
        else
        {
            gmbs->mbtype = (Ipp8s)refMbType;
        }

        memset((void *) MV[0]->MotionVectors, 0, sizeof(H264DecoderMotionVector) * 16);
        memset((void *) MV[1]->MotionVectors, 0, sizeof(H264DecoderMotionVector) * 16);

        memset((void *) RefIdxs[0], -1, sizeof(H264DecoderMacroblockRefIdxs));
        memset((void *) RefIdxs[1], -1, sizeof(H264DecoderMacroblockRefIdxs));

        memset(lmbs->sbdir, 0, 4);

        return;
    }

    if ((intraILPredFlag == 0) && (RestrictedSpatialResolutionChangeFlag == 0))
    {
        Ipp32s restored[2][2];

        for (y = 0; y < 4; y += 2)
        {
            for (x = 0; x < 4; x += 2)
            {
                restored[0][0] = 0;
                restored[0][1] = 0;
                restored[1][0] = 0;
                restored[1][1] = 0;

                for (j = 0; j < 2; j++)
                {
                    for (i = 0; i < 2; i++)
                    {
                        if (refLayerPartIdc[y + j][x + i] == INTRA)
                        {
                            restored[j][i] = 1;

                            /* look at the right (left) */
                            if ((refLayerPartIdc[y + j][x + 1 - i] != INTRA) &&
                                (!restored[j][1 - i]))
                            {
                                refLayerPartIdc[y + j][x + i] =
                                    refLayerPartIdc[y + j][x + 1 - i];

                            }
                            /* look at the down (up) */
                            else if ((refLayerPartIdc[y + 1 - j][x + i] != INTRA) &&
                                     (!restored[1 - j][i]))
                            {
                                refLayerPartIdc[y + j][x + i] =
                                    refLayerPartIdc[y + 1 - j][x + i];
                            }
                            /* diagonal */
                            else if ((refLayerPartIdc[y + 1 - j][x + 1 - i] != INTRA) &&
                                     (!restored[1 - j][1 - i]))
                            {
                                refLayerPartIdc[y + j][x + i] =
                                    refLayerPartIdc[y + 1 - j][x + 1 - i];
                            }
                        }
                    }
                }
            }
        }

        restored[0][0] = 0;
        restored[0][1] = 0;
        restored[1][0] = 0;
        restored[1][1] = 0;

        for (y = 0; y < 2; y++)
        {
            for (x = 0; x < 2; x++)
            {
                if (refLayerPartIdc[2 * y][2 * x] == INTRA)
                {
                    restored[y][x] = 1;

                    if ((refLayerPartIdc[2 * y][2 - x] != INTRA) &&
                        (!restored[y][1 - x]))
                    {
                        for (j = 0; j < 2; j++)
                        {
                            for (i = 0; i < 2; i++)
                            {
                                refLayerPartIdc[2 * y + j][2 * x + i] =
                                    refLayerPartIdc[2 * y + j][2 - x];
                            }
                        }
                    }
                    else if ((refLayerPartIdc[2 - y][2 * x] != INTRA) &&
                             (!restored[1 - y][x]))
                    {
                        for (j = 0; j < 2; j++)
                        {
                            for (i = 0; i < 2; i++)
                            {
                                refLayerPartIdc[2 * y + j][2 * x + i] =
                                    refLayerPartIdc[2 - y][2 * x + i];
                            }
                        }
                    }
                    else if ((refLayerPartIdc[2 - y][2 - x] != INTRA) &&
                             (!restored[1 - y][1 - x]))
                    {
                        for (j = 0; j < 2; j++)
                        {
                            for (i = 0; i < 2; i++)
                            {
                                refLayerPartIdc[2 * y + j][2 * x + i] =
                                    refLayerPartIdc[2 - y][2 - x];
                            }
                        }
                    }
                }
            }
        }
    }

    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 4; x++)
        {
            Ipp32s aMv[2][2];
            Ipp32s refMbAddr, refMbPartIdx;

            refMbAddr = refLayerPartIdc[y][x] / 16;
            refMbPartIdx = refLayerPartIdc[y][x] % 16;

            Ipp32s refFieldFlag = refLayerMb->field_pic_flag || GetMBFieldDecodingFlag(refLayerLocalInfo[refMbAddr]);

            for (i = 0; i < maxX; i++)
            {
                Ipp32s dOX, dOY, dSW, dSH;
                Ipp32s refLayerRefIdx = refLayerLocalInfo[refMbAddr].refIdxs[i].refIndexs[subblock_block_membership[refMbPartIdx]];
                tempRefIdxPredL[i][y][x]  = ( ( ( refLayerRefIdx ) << (curFieldFlag - curLayerD->field_pic_flag) ) >> (refFieldFlag - refLayerMb->field_pic_flag) );

                if (tempRefIdxPredL[i][y][x] < 0)
                {
                    mvILPredL[i][y][x][0] = 0;
                    mvILPredL[i][y][x][1] = 0;
                    continue;
                }

                aMv[i][0] = refLayerMb->MV[i][refMbAddr + refMBOffset].MotionVectors[refMbPartIdx].mvx;
                aMv[i][1] = refLayerMb->MV[i][refMbAddr + refMBOffset].MotionVectors[refMbPartIdx].mvy * (1 + refFieldFlag);

                if (!CroppingChangeFlag || !sd->m_pRefPicList[i] || !sd->m_pRefPicList[i][tempRefIdxPredL[i][y][x]])
                {
                    dOX = dOY = dSW = dSH = 0;
                }
                else
                {
                    Ipp32s did = sd->m_pSliceHeader->nal_ext.svc.dependency_id;

                    //Ipp32s listNum = sd->m_pFields[i][tempRefIdxPredL[i][y][x]].field;
                    dOX = pResizeParameter->leftOffset - sd->m_pRefPicList[i][tempRefIdxPredL[i][y][x]]->m_offsets[did][0];
                    dOY = pResizeParameter->topOffset - sd->m_pRefPicList[i][tempRefIdxPredL[i][y][x]]->m_offsets[did][1];
                    dSW = pResizeParameter->rightOffset - sd->m_pRefPicList[i][tempRefIdxPredL[i][y][x]]->m_offsets[did][2] + dOX;
                    dSH = pResizeParameter->bottomOffset - sd->m_pRefPicList[i][tempRefIdxPredL[i][y][x]]->m_offsets[did][3] + dOY;
                }

                scaleX = (((curWidth + dSW) << 16) + (refWidth >> 1)) / refWidth;
                scaleY = (((curHeigh + dSH) << 16) + (refHeigh >> 1)) / refHeigh;

                aMv[i][0] = (aMv[i][0] * scaleX + 32768) >> 16;
                aMv[i][1] = (aMv[i][1] * scaleY + 32768) >> 16;

                if (CroppingChangeFlag == 1)
                {
                    Ipp32s xFrm = (curMB_X * 16 + (4 * x + 1));
                    Ipp32s yFrm = (curMB_Y * 16 + (4 * y + 1)*(1 + curFieldFlag - curLayerD->field_pic_flag) ) * (1 + curLayerD->field_pic_flag);

                    scaleX = (((4 * dSW) << 16) + (curWidth >> 1)) / curWidth;
                    scaleY = (((4 * dSH) << 16) + (curHeigh >> 1)) / curHeigh;

                    aMv[i][0] += (((xFrm - pResizeParameter->leftOffset) * scaleX + 32768 ) >> 16 ) - 4 * dOX;
                    aMv[i][1] += (((yFrm - pResizeParameter->topOffset) * scaleY + 32768 ) >> 16 ) - 4 * dOY;

                }

                mvILPredL[i][y][x][0] = aMv[i][0];
                mvILPredL[i][y][x][1] = (aMv[i][1] ) / (1 + curFieldFlag);
            }
        }
    }

    refIdxILPredL[0][0][0] = tempRefIdxPredL[0][0][0];
    refIdxILPredL[0][0][1] = tempRefIdxPredL[0][0][2];
    refIdxILPredL[0][1][0] = tempRefIdxPredL[0][2][0];
    refIdxILPredL[0][1][1] = tempRefIdxPredL[0][2][2];

    refIdxILPredL[1][0][0] = tempRefIdxPredL[1][0][0];
    refIdxILPredL[1][0][1] = tempRefIdxPredL[1][0][2];
    refIdxILPredL[1][1][0] = tempRefIdxPredL[1][2][0];
    refIdxILPredL[1][1][1] = tempRefIdxPredL[1][2][2];

    if (RestrictedSpatialResolutionChangeFlag == 0)
    {
        Ipp32s xP, yP, xS, yS;

        for (yP = 0; yP < 2; yP++)
        {
            for (xP = 0; xP < 2; xP++)
            {
                for (yS = 0; yS < 2; yS++)
                {
                    for (xS = 0; xS < 2; xS++)
                    {
                        for (i = 0; i < 2; i++)
                        {
                            refIdxILPredL[i][yP][xP] =
                                minPositive(refIdxILPredL[i][yP][xP],
                                tempRefIdxPredL[i][2 * yP + yS][2 * xP + xS]);
                        }
                    }
                }

                for (yS = 0; yS < 2; yS++)
                {
                    for (xS = 0; xS < 2; xS++)
                    {
                        for (i = 0; i < maxX; i++)
                        {
                            if (tempRefIdxPredL[i][2 * yP + yS][2 * xP + xS] !=
                                refIdxILPredL[i][yP][xP])
                            {
                                if (tempRefIdxPredL[i][2 * yP + yS][2 * xP + 1 - xS] ==
                                    refIdxILPredL[i][yP][xP])
                                {
                                    mvILPredL[i][2 * yP + yS][2 * xP + xS][0] =
                                        mvILPredL[i][2 * yP + yS][2 * xP + 1 - xS][0];

                                    mvILPredL[i][2 * yP + yS][2 * xP + xS][1] =
                                        mvILPredL[i][2 * yP + yS][2 * xP + 1 - xS][1];
                                }
                                else if (tempRefIdxPredL[i][2 * yP + 1 - yS][2 * xP + xS] ==
                                    refIdxILPredL[i][yP][xP])
                                {
                                    mvILPredL[i][2 * yP + yS][2 * xP + xS][0] =
                                        mvILPredL[i][2 * yP + 1 - yS][2 * xP + xS][0];

                                    mvILPredL[i][2 * yP + yS][2 * xP + xS][1] =
                                        mvILPredL[i][2 * yP + 1 - yS][2 * xP + xS][1];
                                }
                                else if (tempRefIdxPredL[i][2 * yP + 1 - yS][2 * xP + 1 - xS] ==
                                    refIdxILPredL[i][yP][xP])
                                {
                                    mvILPredL[i][2 * yP + yS][2 * xP + xS][0] =
                                        mvILPredL[i][2 * yP + 1 - yS][2 * xP + 1 - xS][0];

                                    mvILPredL[i][2 * yP + yS][2 * xP + xS][1] =
                                        mvILPredL[i][2 * yP + 1 - yS][2 * xP + 1 - xS][1];
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (RestrictedSpatialResolutionChangeFlag == 0)
    {
        Ipp32s yO, xO;

        for (yO = 0; yO < 4; yO += 2)
        {
            for (xO = 0; xO < 4; xO += 2)
            {
                if (sd->m_IsUseDirect8x8Inference && slice_type == BPREDSLICE) {
                    for (i = 0; i < maxX; i++)
                    {
                      Ipp32s aMv[2];
                      Ipp32s iXC = ( xO >> 1 ) * 3;
                      Ipp32s iYC = ( yO >> 1 ) * 3;
                      aMv[0] = mvILPredL[i][iYC][iXC][0];
                      aMv[1] = mvILPredL[i][iYC][iXC][1];
                      mvILPredL[i][yO  ][xO  ][0]  = aMv[0];
                      mvILPredL[i][yO  ][xO  ][1]  = aMv[1];
                      mvILPredL[i][yO+1][xO  ][0]  = aMv[0];
                      mvILPredL[i][yO+1][xO  ][1]  = aMv[1];
                      mvILPredL[i][yO  ][xO+1][0]  = aMv[0];
                      mvILPredL[i][yO  ][xO+1][1]  = aMv[1];
                      mvILPredL[i][yO+1][xO+1][0]  = aMv[0];
                      mvILPredL[i][yO+1][xO+1][1]  = aMv[1];
                    }
                }

                sbtype = SBTYPE_8x8;
                for (i = 0; i < maxX; i++)
                {
                    if (((mvDiff(mvILPredL[i][yO][xO], mvILPredL[i][yO][xO + 1])) > 1) ||
                        ((mvDiff(mvILPredL[i][yO][xO], mvILPredL[i][yO + 1][xO])) > 1) ||
                        ((mvDiff(mvILPredL[i][yO][xO], mvILPredL[i][yO + 1][xO + 1])) > 1))
                    {
                        sbtype = SBTYPE_UNDEF;
                        break;
                    }
                }

                if (sbtype == SBTYPE_UNDEF)
                {
                    for (i = 0; i < maxX; i++)
                    {
                        sbtype = SBTYPE_8x4;

                        if (((mvDiff(mvILPredL[i][yO][xO], mvILPredL[i][yO][xO + 1])) > 1) ||
                            ((mvDiff(mvILPredL[i][yO + 1][xO], mvILPredL[i][yO + 1][xO + 1])) > 1))
                        {
                            sbtype = SBTYPE_UNDEF;
                            break;
                        }
                    }
                }

                if (sbtype == SBTYPE_UNDEF)
                {
                    for (i = 0; i < maxX; i++)
                    {
                        sbtype = SBTYPE_4x8;

                        if (((mvDiff(mvILPredL[i][yO][xO], mvILPredL[i][yO + 1][xO])) > 1) ||
                            ((mvDiff(mvILPredL[i][yO][xO + 1], mvILPredL[i][yO + 1][xO + 1])) > 1))
                        {
                            sbtype = SBTYPE_4x4;
                            break;
                        }
                    }
                }

                if (sbtype == SBTYPE_8x8)
                {
                    for (i = 0; i < maxX; i++)
                    {
                        tempMvAL[i][0] = (mvILPredL[i][yO][xO][0] +
                                          mvILPredL[i][yO][xO + 1][0] +
                                          mvILPredL[i][yO + 1][xO][0] +
                                          mvILPredL[i][yO + 1][xO + 1][0] + 2) >> 2;

                        tempMvAL[i][1] = (mvILPredL[i][yO][xO][1] +
                                          mvILPredL[i][yO][xO + 1][1] +
                                          mvILPredL[i][yO + 1][xO][1] +
                                          mvILPredL[i][yO + 1][xO + 1][1] + 2) >> 2;

                        mvILPredL[i][yO][xO][0] = tempMvAL[i][0];
                        mvILPredL[i][yO][xO + 1][0] = tempMvAL[i][0];
                        mvILPredL[i][yO + 1][xO][0] = tempMvAL[i][0];
                        mvILPredL[i][yO + 1][xO + 1][0] = tempMvAL[i][0];

                        mvILPredL[i][yO][xO][1] = tempMvAL[i][1];
                        mvILPredL[i][yO][xO + 1][1] = tempMvAL[i][1];
                        mvILPredL[i][yO + 1][xO][1] = tempMvAL[i][1];
                        mvILPredL[i][yO + 1][xO + 1][1] = tempMvAL[i][1];

                    }
                }
                else if (sbtype == SBTYPE_8x4)
                {
                    for (i = 0; i < maxX; i++)
                    {
                        tempMvAL[i][0] = (mvILPredL[i][yO][xO][0] +
                                          mvILPredL[i][yO][xO + 1][0] + 1) >> 1;

                        tempMvAL[i][1] = (mvILPredL[i][yO][xO][1] +
                                          mvILPredL[i][yO][xO + 1][1] + 1) >> 1;

                        tempMvBL[i][0] = (mvILPredL[i][yO + 1][xO][0] +
                                          mvILPredL[i][yO + 1][xO + 1][0] + 1) >> 1;

                        tempMvBL[i][1] = (mvILPredL[i][yO + 1][xO][1] +
                                          mvILPredL[i][yO + 1][xO + 1][1] + 1) >> 1;

                        mvILPredL[i][yO][xO][0] = tempMvAL[i][0];
                        mvILPredL[i][yO][xO + 1][0] = tempMvAL[i][0];

                        mvILPredL[i][yO][xO][1] = tempMvAL[i][1];
                        mvILPredL[i][yO][xO + 1][1] = tempMvAL[i][1];

                        mvILPredL[i][yO + 1][xO][0] = tempMvBL[i][0];
                        mvILPredL[i][yO + 1][xO + 1][0] = tempMvBL[i][0];

                        mvILPredL[i][yO + 1][xO][1] = tempMvBL[i][1];
                        mvILPredL[i][yO + 1][xO + 1][1] = tempMvBL[i][1];
                    }
                }
                else if (sbtype == SBTYPE_4x8)
                {
                    for (i = 0; i < maxX; i++)
                    {
                        tempMvAL[i][0] = (mvILPredL[i][yO][xO][0] +
                                          mvILPredL[i][yO + 1][xO][0] + 1) >> 1;

                        tempMvAL[i][1] = (mvILPredL[i][yO][xO][1] +
                                          mvILPredL[i][yO + 1][xO][1] + 1) >> 1;

                        tempMvBL[i][0] = (mvILPredL[i][yO][xO + 1][0] +
                                          mvILPredL[i][yO + 1][xO + 1][0] + 1) >> 1;

                        tempMvBL[i][1] = (mvILPredL[i][yO][xO + 1][1] +
                                          mvILPredL[i][yO + 1][xO + 1][1] + 1) >> 1;

                        mvILPredL[i][yO][xO][0]     = tempMvAL[i][0];
                        mvILPredL[i][yO + 1][xO][0] = tempMvAL[i][0];

                        mvILPredL[i][yO][xO][1]     = tempMvAL[i][1];
                        mvILPredL[i][yO + 1][xO][1] = tempMvAL[i][1];

                        mvILPredL[i][yO][xO + 1][0]     = tempMvBL[i][0];
                        mvILPredL[i][yO + 1][xO + 1][0] = tempMvBL[i][0];

                        mvILPredL[i][yO][xO + 1][1]     = tempMvBL[i][1];
                        mvILPredL[i][yO + 1][xO + 1][1] = tempMvBL[i][1];
                    }
                }
            }
        }
    }

    if ((slice_type == INTRASLICE) || (slice_type == S_INTRASLICE))
        return;

    for (i = 0; i < 4; i++)
    {
        lmbs->sbdir[i] = 0;
    }

    mbtype = MBTYPE_FORWARD;

    for (i = 0; i < maxX; i++)
    {
        Ipp32s tmp0, tmp1;

        if ((refIdxILPredL[i][0][0] != refIdxILPredL[i][0][1]) ||
            (refIdxILPredL[i][0][0] != refIdxILPredL[i][1][0]) ||
            (refIdxILPredL[i][0][0] != refIdxILPredL[i][1][1]))
        {
            mbtype = MBTYPE_UNDEF;
            break;
        }

        tmp0 = mvILPredL[i][0][0][0];
        tmp1 = mvILPredL[i][0][0][1];

        for (y = 0; y < 4; y++)
        {
            for (x = 0; x < 4; x++)
            {
                if ((mvILPredL[i][y][x][0] != tmp0) ||
                    (mvILPredL[i][y][x][1] != tmp1))
                {
                    mbtype = MBTYPE_UNDEF;
                    break;
                }
            }
            if (mbtype == MBTYPE_UNDEF)
                break;
        }

        if (mbtype == MBTYPE_UNDEF)
            break;
    }

    if (mbtype == MBTYPE_UNDEF)
    {
        mbtype = MBTYPE_INTER_16x8;
        for (i = 0; i < maxX; i++)
        {
            Ipp32s tmp0, tmp1;

            if ((refIdxILPredL[i][0][0] != refIdxILPredL[i][0][1]) ||
                (refIdxILPredL[i][1][0] != refIdxILPredL[i][1][1]))
            {
                mbtype = MBTYPE_UNDEF;
                break;
            }

            tmp0 = mvILPredL[i][0][0][0];
            tmp1 = mvILPredL[i][0][0][1];

            for (x = 0; x < 4; x++)
            {
                if ((mvILPredL[i][0][x][0] != tmp0) ||
                    (mvILPredL[i][1][x][0] != tmp0) ||
                    (mvILPredL[i][0][x][1] != tmp1) ||
                    (mvILPredL[i][1][x][1] != tmp1))
                {
                    mbtype = MBTYPE_UNDEF;
                    break;
                }
            }

            if (mbtype == MBTYPE_UNDEF)
                break;

            tmp0 = mvILPredL[i][2][0][0];
            tmp1 = mvILPredL[i][2][0][1];

            for (x = 0; x < 4; x++)
            {
                if ((mvILPredL[i][2][x][0] != tmp0) ||
                    (mvILPredL[i][3][x][0] != tmp0) ||
                    (mvILPredL[i][2][x][1] != tmp1) ||
                    (mvILPredL[i][3][x][1] != tmp1))
                {
                    mbtype = MBTYPE_UNDEF;
                    break;
                }
            }

            if (mbtype == MBTYPE_UNDEF)
                break;
        }
    }

    if (mbtype == MBTYPE_UNDEF)
    {
        mbtype = MBTYPE_INTER_8x16;
        for (i = 0; i < maxX; i++)
        {
            Ipp32s tmp0, tmp1;

            if ((refIdxILPredL[i][0][0] != refIdxILPredL[i][1][0]) ||
                (refIdxILPredL[i][0][1] != refIdxILPredL[i][1][1]))
            {
                mbtype = MBTYPE_UNDEF;
                break;
            }

            tmp0 = mvILPredL[i][0][0][0];
            tmp1 = mvILPredL[i][0][0][1];

            for (y = 0; y < 4; y++)
            {
                if ((mvILPredL[i][y][0][0] != tmp0) ||
                    (mvILPredL[i][y][1][0] != tmp0) ||
                    (mvILPredL[i][y][0][1] != tmp1) ||
                    (mvILPredL[i][y][1][1] != tmp1))
                {
                    mbtype = MBTYPE_UNDEF;
                    break;
                }
            }

            if (mbtype == MBTYPE_UNDEF)
                break;

            tmp0 = mvILPredL[i][0][2][0];
            tmp1 = mvILPredL[i][0][2][1];

            for (y = 0; y < 4; y++)
            {
                if ((mvILPredL[i][y][2][0] != tmp0) ||
                    (mvILPredL[i][y][3][0] != tmp0) ||
                    (mvILPredL[i][y][2][1] != tmp1) ||
                    (mvILPredL[i][y][3][1] != tmp1))
                {
                    mbtype = MBTYPE_UNDEF;
                    break;
                }
            }

            if (mbtype == MBTYPE_UNDEF)
                break;
        }
    }

    if (mbtype == MBTYPE_UNDEF)
        mbtype = MBTYPE_INTER_8x8;

    if (slice_type == BPREDSLICE)
    {
        Ipp32s ind0, ind1;
        if (mbtype != MBTYPE_INTER_8x8)
        {
            ind0 = (refIdxILPredL[1][0][0] >= 0 ? 2 : 0) +
                   (refIdxILPredL[0][0][0] >= 0 ? 1 : 0);

            if (mbtype != MBTYPE_FORWARD)
            {
                ind1 = (refIdxILPredL[1][1][1] >= 0 ? 2 : 0) +
                       (refIdxILPredL[0][1][1] >= 0 ? 1 : 0);

                if (ind0 == 1)
                {
                    lmbs->sbdir[0] = D_DIR_FWD;
                }
                else if (ind0 == 2)
                {
                    lmbs->sbdir[0] = D_DIR_BWD;
                }
                else if (ind0 == 3)
                {
                    lmbs->sbdir[0] = D_DIR_BIDIR;
                }

                if (ind1 == 1)
                {
                    lmbs->sbdir[1] = D_DIR_FWD;
                }
                else if (ind1 == 2)
                {
                    lmbs->sbdir[1] = D_DIR_BWD;
                }
                else if (ind1 == 3)
                {
                    lmbs->sbdir[1] = D_DIR_BIDIR;
                }
            }
            else
            {
                if (ind0 == 2)
                {
                    mbtype = MBTYPE_BACKWARD;

                }
                else if (ind0 == 3)
                {
                    mbtype = MBTYPE_BIDIR;
                }
            }
        }
    }

    gmbs->mbtype = (Ipp8s)mbtype;

    if (mbtype == MBTYPE_INTER_8x8)
    {
        for (subblock = 0; subblock < 4; subblock++)
        {
            Ipp32s yO, xO;
            xO = 2 * (subblock % 2);
            yO = 2 * (subblock / 2);

            sbtype = SBTYPE_8x8;

            for (i = 0; i < maxX; i++)
            {
                Ipp32s tmp0 = mvILPredL[i][yO][xO][0];
                Ipp32s tmp1 = mvILPredL[i][yO][xO][1];

                if ((mvILPredL[i][yO][xO + 1][0] != tmp0) ||
                    (mvILPredL[i][yO + 1][xO][0] != tmp0) ||
                    (mvILPredL[i][yO + 1][xO + 1][0] != tmp0) ||
                    (mvILPredL[i][yO][xO + 1][1] != tmp1) ||
                    (mvILPredL[i][yO + 1][xO][1] != tmp1) ||
                    (mvILPredL[i][yO + 1][xO + 1][1] != tmp1))
                {
                    sbtype = SBTYPE_UNDEF;
                    break;
                }
            }

            if (sbtype == SBTYPE_UNDEF)
            {
                sbtype = SBTYPE_8x4;
                for (i = 0; i < maxX; i++)
                {
                    if ((mvILPredL[i][yO][xO][0] != mvILPredL[i][yO][xO + 1][0]) ||
                        (mvILPredL[i][yO + 1][xO][0] != mvILPredL[i][yO + 1][xO + 1][0]) ||
                        (mvILPredL[i][yO][xO][1] != mvILPredL[i][yO][xO + 1][1]) ||
                        (mvILPredL[i][yO + 1][xO][1] != mvILPredL[i][yO + 1][xO + 1][1]))
                    {
                        sbtype = SBTYPE_UNDEF;
                        break;
                    }
                }
            }

            if (sbtype == SBTYPE_UNDEF)
            {
                sbtype = SBTYPE_4x8;
                for (i = 0; i < maxX; i++)
                {
                    if ((mvILPredL[i][yO][xO][0] != mvILPredL[i][yO + 1][xO][0]) ||
                        (mvILPredL[i][yO][xO + 1][0] != mvILPredL[i][yO + 1][xO + 1][0]) ||
                        (mvILPredL[i][yO][xO][1] != mvILPredL[i][yO + 1][xO][1]) ||
                        (mvILPredL[i][yO][xO + 1][1] != mvILPredL[i][yO + 1][xO + 1][1]))
                    {
                        sbtype = SBTYPE_UNDEF;
                        break;
                    }
                }
            }

            if (sbtype == SBTYPE_UNDEF)
                sbtype = SBTYPE_4x4;

            if (slice_type == BPREDSLICE) {
                Ipp32s ind;
                ind = (refIdxILPredL[1][yO / 2][xO / 2] >= 0 ? 2 : 0) +
                      (refIdxILPredL[0][yO / 2][xO / 2] >= 0 ? 1 : 0);

                if (ind == 1)
                {
                    lmbs->sbdir[subblock] = D_DIR_FWD;
                }
                else if (ind == 2)
                {
                    lmbs->sbdir[subblock] = D_DIR_BWD;
                }
                else if (ind == 3)
                {
                    lmbs->sbdir[subblock] = D_DIR_BIDIR;
                }

            }
            gmbs->sbtype[subblock] = (Ipp8u)sbtype;
        }
    }

    for (i = 0; i < maxX; i++)
    {
        for (y = 0; y < 4; y++)
        {
            for (x = 0; x < 4; x++)
            {
                MV[i]->MotionVectors[y * 4 + x].mvx = (Ipp16s)mvILPredL[i][y][x][0];
                MV[i]->MotionVectors[y * 4 + x].mvy = (Ipp16s)mvILPredL[i][y][x][1];
            }
        }

        for (y = 0; y < 2; y++)
        {
            for (x = 0; x < 2; x++)
            {
                RefIdxs[i][2*y+x]   = (RefIndexType)refIdxILPredL[i][y][x];
            }
        }
    }
}

void umc_h264_mv_resampling(H264SegmentDecoderMultiThreaded *sd,
                            H264Slice *curSlice,
                            H264DecoderLayerDescriptor *refLayer,
                            H264DecoderLayerDescriptor *curLayer,
                            H264DecoderGlobalMacroblocksDescriptor *gmbinfo,
                            H264DecoderLocalMacroblockDescriptor *mbinfo)
{
    const H264SeqParamSetSVCExtension *curSps = curSlice->GetSeqSVCParam();
    H264SliceHeader *curSliceHeader = curSlice->GetSliceHeader();
    H264DecoderLayerResizeParameter *pResizeParameter = curLayer->m_pResizeParameter;

    Ipp32s leftOffset = pResizeParameter->leftOffset;
    Ipp32s topOffset = pResizeParameter->topOffset;
    Ipp32s rightOffset = pResizeParameter->rightOffset;
    Ipp32s bottomOffset = pResizeParameter->bottomOffset;

    Ipp32s CroppingChangeFlag;
    Ipp32s SpatialResolutionChangeFlag, RestrictedSpatialResolutionChangeFlag;

    if (curSps->extended_spatial_scalability == 2)
        CroppingChangeFlag = 1;
    else
        CroppingChangeFlag = 0;

    Ipp32s scaledW = pResizeParameter->scaled_ref_layer_width;
    Ipp32s scaledH = pResizeParameter->scaled_ref_layer_height;

    Ipp32s refW = pResizeParameter->ref_layer_width;
    Ipp32s refH = pResizeParameter->ref_layer_height;

    if ((CroppingChangeFlag == 0) &&
        (refW == scaledW) &&
        (refH == scaledH) &&
        ((leftOffset & 0xF) == 0) &&
        ((topOffset % (16 * (1 + curSliceHeader->field_pic_flag + curSliceHeader->MbaffFrameFlag))) == 0) &&
        (curLayer->field_pic_flag == refLayer->field_pic_flag) &&
        (curSliceHeader->MbaffFrameFlag == refLayer->is_MbAff) &&
        (pResizeParameter->refPhaseX == pResizeParameter->phaseX) &&
        (pResizeParameter->refPhaseX == pResizeParameter->phaseX))
    {
        SpatialResolutionChangeFlag = 0;
    }
    else
        SpatialResolutionChangeFlag = 1;

    if ((SpatialResolutionChangeFlag == 0) ||
        (((refW == scaledW) || (2 * refW == scaledW)) &&
        ((refH == scaledH) || (2 * refH == scaledH)) &&
        ((leftOffset & 0xF) == 0) &&
        ((topOffset % (16 * (1 + curSliceHeader->field_pic_flag))) == 0) &&
        !curSliceHeader->MbaffFrameFlag &&
        !refLayer->is_MbAff &&
        (curLayer->field_pic_flag == refLayer->field_pic_flag))
        )
    {
        RestrictedSpatialResolutionChangeFlag = 1;
    }
    else
        RestrictedSpatialResolutionChangeFlag = 0;

    if (!SpatialResolutionChangeFlag)
        RestrictedSpatialResolutionChangeFlag = 1;

    curLayer->restricted_spatial_resolution_change_flag = RestrictedSpatialResolutionChangeFlag;

    /* all offsets in macroblocks */

    leftOffset = (leftOffset + 15) >> 4;
    rightOffset = (rightOffset + 15) >> 4;
    topOffset = (topOffset + 15) >> 4;
    bottomOffset = (bottomOffset + 15) >> 4;

    for (Ipp32s j = 0; j < (Ipp32s)curSps->frame_height_in_mbs; j++)
    {
        for (Ipp32s i = 0; i < (Ipp32s)curSps->frame_width_in_mbs; i++)
        {
            Ipp32s curMbAddr;

            if (sd->m_isMBAFF) {
                curMbAddr = (j/2 * (curSps->frame_width_in_mbs) + i) * 2 + (j & 1);
            } else {
                curMbAddr = j * (curSps->frame_width_in_mbs) + i;
            }

            if ((i >= leftOffset) && (i < (Ipp32s)curSps->frame_width_in_mbs - rightOffset) &&
                (j >= topOffset) && (j < (Ipp32s)curSps->frame_height_in_mbs - bottomOffset))
            {
            } else {
                H264DecoderMacroblockLocalInfo *lmbs;
                H264DecoderMacroblockGlobalInfo *gmbs;
                H264DecoderMacroblockMVs *MV[2];
                RefIndexType *RefIdxs[2];

                lmbs = &(mbinfo->mbs[curMbAddr]);
                gmbs = &(gmbinfo->mbs[curMbAddr]);
                MV[0] = &(gmbinfo->MV[0][curMbAddr]);
                MV[1] = &(gmbinfo->MV[1][curMbAddr]);
                RefIdxs[0] = GetRefIdxs(gmbinfo, 0, curMbAddr);
                RefIdxs[1] = GetRefIdxs(gmbinfo, 1, curMbAddr);

                memset((void *) RefIdxs[0], -1, sizeof(H264DecoderMacroblockRefIdxs));
                memset((void *) RefIdxs[1], -1, sizeof(H264DecoderMacroblockRefIdxs));
                memset((void *) MV[0], 0, sizeof(H264DecoderMacroblockMVs));
                memset((void *) MV[1], 0, sizeof(H264DecoderMacroblockMVs));

                pSetMBSkippedFlag(gmbs);
                pSetMBResidualPredictionFlag(gmbs, 0);
                lmbs->cbp = 0;
                lmbs->cbp4x4_luma = 0;
                lmbs->cbp4x4_chroma[0] = 0;
                lmbs->cbp4x4_chroma[1] = 0;
                memset(lmbs->sbdir, 0, 4);
            }
        }
    }
}

void umc_h264_mv_resampling_mb(H264SegmentDecoderMultiThreaded *sd,
                            Ipp32s mb_x, Ipp32s mb_y, Ipp32s field)
{
    H264Slice *curSlice = sd->m_pSlice;
    H264SliceHeader *pSliceHeader = curSlice->GetSliceHeader();

    H264DecoderLayerDescriptor *layerMb, *curlayerMb;
    const H264SeqParamSetSVCExtension *curSps = curSlice->GetSeqSVCParam();
    Ipp32s curMbAddr;
    if (!(/*curSlice->m_bIsFirstSliceInDepLayer &&*/ curSlice->GetSliceHeader()->nal_ext.svc.quality_id == 0 &&
        (sd->m_currentLayer > 0) &&
        (pSliceHeader->ref_layer_dq_id >= 0)))
    {
        return ;
    }

    VM_ASSERT((pSliceHeader->ref_layer_dq_id >> 4) < sd->m_currentLayer);

    layerMb = &sd->m_mbinfo.layerMbs[(pSliceHeader->ref_layer_dq_id >> 4)];
    curlayerMb = &sd->m_mbinfo.layerMbs[sd->m_currentLayer];

    if (!field)
    {
        if (curlayerMb->field_pic_flag)
            field = 1;
    }

    H264DecoderLayerResizeParameter *pResizeParameter = curlayerMb->m_pResizeParameter;

    Ipp32s slice_type = pSliceHeader->slice_type;
    H264DecoderLayerDescriptor *refLayer = layerMb;
    H264DecoderLayerDescriptor *curLayer = curlayerMb;

    Ipp32s leftOffset, rightOffset, topOffset, bottomOffset;
    Ipp32s refW, refH, scaledW, scaledH;
    Ipp32s scaleX, scaleY;
    Ipp32s CroppingChangeFlag;
    Ipp32s i, j;

    leftOffset = pResizeParameter->leftOffset;
    topOffset = pResizeParameter->topOffset;
    rightOffset = pResizeParameter->rightOffset;
    bottomOffset = pResizeParameter->bottomOffset;

    if (curSps->extended_spatial_scalability == 2)
        CroppingChangeFlag = 1;
    else
        CroppingChangeFlag = 0;

    scaledW = pResizeParameter->scaled_ref_layer_width;
    scaledH = pResizeParameter->scaled_ref_layer_height;

    refW = pResizeParameter->ref_layer_width;
    refH = pResizeParameter->ref_layer_height;

    scaleX = (((Ipp32u)scaledW << 16) + (refW >> 1)) / refW;
    scaleY = (((Ipp32u)scaledH << 16) + (refH >> 1)) / refH;

    /* all offsets in macroblocks */

    leftOffset = (leftOffset + 15) >> 4;
    rightOffset = (rightOffset + 15) >> 4;
    topOffset = (topOffset + 15) >> 4;
    bottomOffset = (bottomOffset + 15) >> 4;

    j = mb_y;
    i = mb_x;

    if (sd->m_isMBAFF) {
        curMbAddr = (j/2 * (curSps->frame_width_in_mbs) + i) * 2 + (j & 1);
    } else {
        curMbAddr = j * (curSps->frame_width_in_mbs) + i;
    }

    if ((i >= leftOffset) && (i < (Ipp32s)curSps->frame_width_in_mbs - rightOffset) &&
        (j >= topOffset) && (j < (Ipp32s)curSps->frame_height_in_mbs - bottomOffset))
    {
        umc_h264_MB_mv_resampling(sd,
            curLayer,
            i, j,
            curMbAddr,
            slice_type,
            scaledW, scaledH,
            refW, refH,
            refLayer, sd->m_gmbinfo, &sd->m_mbinfo,
            scaleX, scaleY, curLayer->restricted_spatial_resolution_change_flag, CroppingChangeFlag, field);
    } else {
        H264DecoderMacroblockLocalInfo *lmbs;
        H264DecoderMacroblockGlobalInfo *gmbs;
        H264DecoderMacroblockMVs *MV[2];
        RefIndexType *RefIdxs[2];

        lmbs = &(sd->m_mbinfo.mbs[curMbAddr]);
        gmbs = &(sd->m_gmbinfo->mbs[curMbAddr]);
        MV[0] = &(sd->m_gmbinfo->MV[0][curMbAddr]);
        MV[1] = &(sd->m_gmbinfo->MV[1][curMbAddr]);
        RefIdxs[0] = GetRefIdxs(sd->m_gmbinfo, 0, curMbAddr);
        RefIdxs[1] = GetRefIdxs(sd->m_gmbinfo, 1, curMbAddr);

        memset((void *) RefIdxs[0], -1, sizeof(H264DecoderMacroblockRefIdxs));
        memset((void *) RefIdxs[1], -1, sizeof(H264DecoderMacroblockRefIdxs));
        memset((void *) MV[0], 0, sizeof(H264DecoderMacroblockMVs));
        memset((void *) MV[1], 0, sizeof(H264DecoderMacroblockMVs));

        pSetMBResidualPredictionFlag(gmbs, 0);
        lmbs->cbp = 0;
        lmbs->cbp4x4_luma = 0;
        lmbs->cbp4x4_chroma[0] = 0;
        lmbs->cbp4x4_chroma[1] = 0;
        memset(lmbs->sbdir, 0, 4);
    }
}

void constrainedIntraResamplingCheckMBs(const H264SeqParamSet *curSps,
                                        H264DecoderLayerDescriptor *curLayer,
                                        H264DecoderLayerDescriptor *refLayer,
                                        H264DecoderMacroblockGlobalInfo *mbs,
                                        Ipp32s  *buf,
                                        Ipp32s  widthMB,
                                        Ipp32s  heightMB)
{
    H264DecoderLayerResizeParameter *resizeParams = curLayer->m_pResizeParameter;

    //Ipp32s baseFieldFlag  = refLayer->frame_mbs_only_flag ? 0 : 1;
    Ipp32s curFieldFlag = (refLayer->frame_mbs_only_flag && curLayer->frame_mbs_only_flag) ? 0 : 1;

    Ipp32s phaseX = resizeParams->phaseX;
    Ipp32s phaseY = resizeParams->phaseY;
    Ipp32s refPhaseX = resizeParams->refPhaseX;
    Ipp32s refPhaseY = resizeParams->refPhaseY;
    Ipp32s leftOffsetLuma = resizeParams->leftOffset;
    Ipp32s topOffsetLuma = resizeParams->topOffset >> curFieldFlag;

    Ipp32s scaledW = resizeParams->scaled_ref_layer_width;
    Ipp32s scaledH = resizeParams->scaled_ref_layer_height;

    if (!curLayer->frame_mbs_only_flag && refLayer->frame_mbs_only_flag)
        scaledH >>= 1;

    Ipp32s refWLuma = resizeParams->ref_layer_width;
    Ipp32s refHLuma = resizeParams->ref_layer_height;

    Ipp32s shiftXLuma = ((resizeParams->level_idc <= 30) ? 16 : countShift(refWLuma));
    Ipp32s shiftYLuma = ((resizeParams->level_idc <= 30) ? 16 : countShift(refHLuma));

    Ipp32s scaleXLuma = resizeParams->scaleX;
    Ipp32s scaleYLuma = resizeParams->scaleY;

    Ipp32s bottom_field_flag = refLayer->frame_mbs_only_flag ? 0 : curLayer->field_pic_flag ? curLayer->bottom_field_flag : refLayer->field_pic_flag ? refLayer->bottom_field_flag : 0;        
    if (!curLayer->frame_mbs_only_flag || !refLayer->frame_mbs_only_flag)
    {
        if (refLayer->frame_mbs_only_flag)
        {
            phaseY    = phaseY + 4 * bottom_field_flag + 2;
            refPhaseY = 2 * refPhaseY + 2;
        }
        else
        {
            phaseY    = phaseY    + 4 * bottom_field_flag;
            refPhaseY = refPhaseY + 4 * bottom_field_flag;
        }
    }

    Ipp32s addXLuma = ((refWLuma << (shiftXLuma - 1)) + (scaledW >> 1)) / scaledW + (1 << (shiftXLuma - 5));
    Ipp32s addYLuma = ((refHLuma << (shiftYLuma - 1)) + (scaledH >> 1)) / scaledH + (1 << (shiftYLuma - 5));
    Ipp32s deltaXLuma = 8;
    Ipp32s deltaYLuma = 8;

    if (!curLayer->frame_mbs_only_flag || !refLayer->frame_mbs_only_flag)
    {
        addYLuma = (((refHLuma * (2 + phaseY) ) << (shiftYLuma - 3)) + (scaledH >> 1) ) / scaledH + (1 << (shiftYLuma - 5));
        deltaYLuma = 2 * (2 + refPhaseY);
    }

    /* 4:2:0 */
    Ipp32s chromaShiftW = 1;
    Ipp32s chromaShiftH = 1;

    Ipp32s leftOffsetChroma = leftOffsetLuma >> chromaShiftW;
    Ipp32s refWChroma = refWLuma >> chromaShiftW;

    Ipp32s topOffsetChroma = topOffsetLuma >> chromaShiftH;
    Ipp32s refHChroma = refHLuma >> chromaShiftH;

    scaledW >>= chromaShiftW;
    scaledH >>= chromaShiftH;

    Ipp32s shiftXChroma = ((curSps->level_idc <= 30) ? 16 : countShift(refWChroma));
    Ipp32s shiftYChroma = ((curSps->level_idc <= 30) ? 16 : countShift(refHChroma));

    Ipp32s scaleXChroma = (((Ipp32u)refWChroma << shiftXChroma) + (scaledW >> 1)) / scaledW;
    Ipp32s scaleYChroma = (((Ipp32u)refHChroma << shiftYChroma) + (scaledH >> 1)) / scaledH;

    Ipp32s addXChroma = (((refWChroma * (2 + phaseX)) << (shiftXChroma - 2)) + (scaledW >> 1)) / scaledW +
                   (1 << (shiftXChroma - 5));

    Ipp32s addYChroma = (((refHChroma * (2 + phaseY)) << (shiftYChroma - 2)) + (scaledH >> 1)) / scaledH +
                   (1 << (shiftYChroma - 5));

    Ipp32s deltaXChroma = 4 * (2 + refPhaseX);
    Ipp32s deltaYChroma = 4 * (2 + refPhaseY);

    Ipp32s MbAddr = 0;

    for (Ipp32s j = 0; j < heightMB; j++)
    {
        Ipp32s yRef, refMbAddrMinYLuma, refMbAddrMaxYLuma;
        Ipp32s refMbAddrMinYChroma, refMbAddrMaxYChroma;

        yRef = (((((j * 16 - topOffsetLuma) * scaleYLuma +  addYLuma) >>
                   (shiftYLuma - 4)) - deltaXLuma) >> 4) + 1;

        if (yRef < 0) yRef = 0;
        if (yRef >= refHLuma) yRef = refHLuma - 1;

        refMbAddrMinYLuma = (yRef >> 4) * (refWLuma >> 4);

        yRef = (((((j * 16 - topOffsetLuma + 15) * scaleYLuma +  addYLuma) >>
                  (shiftYLuma - 4)) - deltaYLuma + 15) >> 4) - 1;

        if (yRef < 0) yRef = 0;
        if (yRef >= refHLuma) yRef = refHLuma - 1;

        refMbAddrMaxYLuma = (yRef >> 4) * (refWLuma >> 4);

        yRef = (((((j * 8 - topOffsetChroma) * scaleYChroma +  addYChroma) >>
                   (shiftYChroma - 4)) - deltaYChroma) >> 4) + 1;

        if (yRef < 0) yRef = 0;
        if (yRef >= refHChroma) yRef = refHChroma - 1;

        refMbAddrMinYChroma = (yRef >> 3) * (refWLuma >> 4);

        yRef = (((((j * 8 - topOffsetChroma + 7) * scaleYChroma +  addYChroma) >>
                   (shiftYChroma - 4)) - deltaYChroma + 15) >> 4) - 1;

        if (yRef < 0) yRef = 0;
        if (yRef >= refHChroma) yRef = refHChroma - 1;

        refMbAddrMaxYChroma = (yRef >> 3) * (refWLuma >> 4);


        for (Ipp32s i = 0; i < widthMB; i++)
        {
            Ipp32s xRef, clear, index;
            Ipp32s refMbAddr[8];

            xRef = (((((i * 16 - leftOffsetLuma) * scaleXLuma +  addXLuma) >>
                       (shiftXLuma - 4)) - 8) >> 4) + 1;

            if (xRef < 0) xRef = 0;
            if (xRef >= refWLuma) xRef = refWLuma - 1;

            refMbAddr[0] = refMbAddrMinYLuma + (xRef >> 4);
            refMbAddr[1] = refMbAddrMaxYLuma + (xRef >> 4);


            xRef = (((((i * 16 - leftOffsetLuma + 15) * scaleXLuma +  addXLuma) >>
                       (shiftXLuma - 4)) - 8 + 15) >> 4) - 1;

            if (xRef < 0) xRef = 0;
            if (xRef >= refWLuma) xRef = refWLuma - 1;

            refMbAddr[2] = refMbAddrMinYLuma + (xRef >> 4);
            refMbAddr[3] = refMbAddrMaxYLuma + (xRef >> 4);

            xRef = (((((i * 8 - leftOffsetChroma) * scaleXChroma +  addXChroma) >>
                       (shiftXChroma - 4)) - deltaXChroma) >> 4) + 1;

            if (xRef < 0) xRef = 0;
            if (xRef >= refWChroma) xRef = refWChroma - 1;

            refMbAddr[4] = refMbAddrMinYChroma + (xRef >> 3);
            refMbAddr[5] = refMbAddrMaxYChroma + (xRef >> 3);

            xRef = (((((i * 8 - leftOffsetChroma + 7) * scaleXChroma +  addXChroma) >>
                       (shiftXChroma - 4)) - deltaXChroma + 15) >> 4) - 1;

            if (xRef < 0) xRef = 0;
            if (xRef >= refWChroma) xRef = refWChroma - 1;

            refMbAddr[6] = refMbAddrMinYChroma + (xRef >> 3);
            refMbAddr[7] = refMbAddrMaxYChroma + (xRef >> 3);

            index = -1;

            for (Ipp32s ii = 0; ii < 8; ii++)
            {
                if (mbs[refMbAddr[ii]].mbtype <= MBTYPE_PCM)
                {
                    index = ii;
                }
                else
                {
                    refMbAddr[ii] = -1;
                }
            }

            clear = 1;

            if (index >= 0)
            {
                Ipp32s slice_id = mbs[refMbAddr[index]].slice_id;

                clear = 0;

                for (Ipp32s ii = index - 1; ii >= 0; ii--)
                {
                    if ((refMbAddr[ii] >= 0) &&
                        (mbs[refMbAddr[ii]].slice_id != slice_id))
                    {
                        clear = 1;
                        break;
                    }
                }
            }


            if (clear)
            {
                buf[MbAddr] = -1;
            }
            else
            {
                buf[MbAddr] = mbs[refMbAddr[index]].slice_id;
            }
            MbAddr++;
        }
    }
}


} // namespace UMC

#endif // UMC_ENABLE_H264_VIDEO_DECODER
