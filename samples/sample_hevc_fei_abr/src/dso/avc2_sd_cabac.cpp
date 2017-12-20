#include "avc2_parser.h"

using namespace BS_AVC2;
using namespace BsReader2;

#undef BS2_TRO
#define BS2_TRO\
 if (TraceOffset()) fprintf(GetLog(), "0x%016llX[%i]|%3u|%3u: ",\
     GetByteOffset(), GetBitOffset(), GetR(), GetV());

void SDParser::parseSD_CABAC(Slice& s)
{
    Bs8u  moreDataFlag   = 1;
    Bs8u  prevMbSkipped  = 0;
    MB zeroMB;

    memset(&zeroMB, 0, sizeof(MB));

    while (GetBitOffset())
    {
        if (!u1()) //cabac_alignment_one_bit
            throw InvalidSyntax();
    }

    CABAC::Init(s);

    if (m_mb.capacity() < PicSizeInMbs)
        m_mb.resize(PicSizeInMbs);

    m_mb.resize(0);

    for (;;)
    {
        m_mb.push_back(zeroMB);
        MB& mb = m_mb.back();
        Bs64u startBit = CABAC::BitCount();
        Bs64u startBin = CABAC::BinCount;

        newMB(mb);

        if (TLTest(TRACE_MARKERS))
            BS2_TRACE_STR("--- MB ---");

        TLStart(TRACE_MB_ADDR);
        BS2_TRACE(mb.Addr, mb.Addr);
        TLEnd();

        TLStart(TRACE_MB_LOC);
        BS2_TRACE(mb.x, mb.x);
        BS2_TRACE(mb.y, mb.y);
        TLEnd();

        if (!isI && !isSI)
        {
            TLStart(TRACE_RESIDUAL_FLAGS);
            BS2_SET(CABAC::mbSkipFlag(), mb.mb_skip_flag);
            TLEnd();

            if (mb.mb_skip_flag)
            {
                TLStart(TRACE_MB_TYPE);
                BS2_SETM(isB ? B_Skip : P_Skip, mb.MbType, mbTypeTraceMap);
                TLEnd();
            }
            moreDataFlag = !mb.mb_skip_flag;
        }

        if (moreDataFlag)
        {
            TLStart(TRACE_MB_FIELD);

            if (MbaffFrameFlag && (mb.Addr % 2 == 0 || prevMbSkipped))
                BS2_SET(CABAC::mbFieldDecFlag(), mb.mb_field_decoding_flag);

            TLEnd();

            if (MbaffFrameFlag && mb.Addr % 2 == 1 && prevMbSkipped)
                MBRef(mb.Addr - 1).mb_field_decoding_flag = mb.mb_field_decoding_flag;

            parseMB_CABAC(mb);
        }

        prevMbSkipped = mb.mb_skip_flag;

        if (MbaffFrameFlag && mb.Addr % 2 == 0)
            moreDataFlag = 1;
        else
        {
            TLAuto tl(*this, TRACE_MB_EOS);

            BS2_SET(CABAC::endOfSliceFlag(), mb.end_of_slice_flag);
            moreDataFlag = !mb.end_of_slice_flag;
        }

        mb.NumBits += Bs16u(CABAC::BitCount() - startBit);
        mb.NumBins  = Bs16u(CABAC::BinCount - startBin);

        TLStart(TRACE_SIZE);
        BS2_TRACE(mb.NumBits, mb.NumBits);
        BS2_TRACE(mb.NumBins, mb.NumBins);
        TLEnd();

        if (!moreDataFlag)
            break;
    }

    while (GetBitOffset())
        if (u1())
            throw InvalidSyntax();

    TLStart(TRACE_SIZE);
    BS2_SET(CABAC::BinCount, s.BinCount);
    BS2_SET((Bs32u)m_mb.size(), s.NumMB);
    TLEnd();
}

void SDParser::parseMB_CABAC(MB& mb)
{
    TLStart(TRACE_MB_TYPE);
    BS2_SETM(CABAC::mbType(), mb.MbType, mbTypeTraceMap);
    TLEnd();

    mb.SubMbType[0] = mb.SubMbType[1] = mb.SubMbType[2] = mb.SubMbType[3] = mb.MbType;

    if (mb.MbType == I_PCM)
    {
        TLAuto tl(*this, TRACE_RESIDUAL_ARRAYS);

        CABAC::InitPCMBlock();

        while (GetBitOffset())
        {
            mb.NumBits++;

            if (u1()) //pcm_alignment_zero_bit
                throw InvalidSyntax();
        }

        BS2_TRACE_ARR_VF(u(BitDepthY), pcm_sample_luma, 256, 16, "%02X ");
        BS2_TRACE_ARR_VF(u(BitDepthC), pcm_sample_chroma,
            MbWidthC * MbHeightC * 2U, MbWidthC, "%02X ");

        mb.NumBits += 8 * (256 + MbWidthC * MbHeightC * 2);

        CABAC::InitADE();

        return;
    }

    Bs8u noSubMbPartSizeLessThan8x8Flag = 1;
    Bs8s MbPartPredMode0 = MbPartPredMode(mb, 0);

    if (   mb.MbType != I_NxN
        && MbPartPredMode0 != Intra_16x16
        && NumMbPart(mb) == 4)
    {
        parseSubMbPred_CABAC(mb);

        for (Bs8u i = 0; i < 4; i++)
        {
            if (mb.SubMbType[i] != B_Direct_8x8)
            {
                if (NumSubMbPart(mb, i) > 1)
                {
                    noSubMbPartSizeLessThan8x8Flag = 0;
                    break;
                }
            }
            else if (!cSPS().direct_8x8_inference_flag)
            {
                noSubMbPartSizeLessThan8x8Flag = 0;
                break;
            }
        }
    }
    else
    {
        if (cPPS().transform_8x8_mode_flag && mb.MbType == I_NxN)
        {
            TLAuto tl(*this, TRACE_MB_TRANS8x8);
            BS2_SET(CABAC::transformSize8x8Flag(), mb.transform_size_8x8_flag);
        }

        parseMbPred_CABAC(mb);
    }

    if (MbPartPredMode0 != Intra_16x16)
    {
        TLStart(TRACE_CBP);
        BS2_SET(CABAC::CBP(), mb.coded_block_pattern);
        TLEnd();

        if (   cbpY(mb) > 0
            && cPPS().transform_8x8_mode_flag
            && mb.MbType != I_NxN
            && noSubMbPartSizeLessThan8x8Flag
            && (mb.MbType != B_Direct_16x16 || cSPS().direct_8x8_inference_flag))
        {
            TLAuto tl(*this, TRACE_MB_TRANS8x8);
            BS2_SET(CABAC::transformSize8x8Flag(), mb.transform_size_8x8_flag);
        }
    }

    if (cbpY(mb) > 0 || cbpC(mb) > 0 || MbPartPredMode0 == Intra_16x16)
    {
        TLStart(TRACE_MB_QP_DELTA);
        BS2_SET(CABAC::mbQpDelta(), mb.mb_qp_delta);
        TLEnd();

        TLStart(TRACE_MB_QP);
        BS2_SET(QPy(mb), mb.QPy);
        TLEnd();

        parseResidual_CABAC(mb, 0, 15);
    }
}

void SDParser::parseSubMbPred_CABAC(MB& mb)
{
    const Bs16u dim[] = { 1, 4, 4, 2 };
    bool trace_arr[4] = {};

    TLStart(TRACE_SUB_MB_TYPE);
    BS2_SET_ARR_M(CABAC::subMbType(), mb.SubMbType, 4, 2, "%12s(%2i), ", mbTypeTraceMap);
    TLEnd();

    TLStart(TRACE_RESIDUAL_FLAGS);

    if (   cSlice().num_ref_idx_l0_active_minus1 > 0
        || mb.mb_field_decoding_flag != cSlice().field_pic_flag)
    {
        for (Bs8u i = 0; i < 4; i++)
        {
            if (   mb.MbType != P_8x8ref0
                && mb.SubMbType[i] != B_Direct_8x8
                && SubMbPredMode(mb, i) != Pred_L1)
            {
                BS2_SET(CABAC::refIdxLX(i, 0), mb.pred.ref_idx_lX[0][i]);
                trace_arr[0] = true;
            }
        }
    }


    if (   cSlice().num_ref_idx_l1_active_minus1 > 0
        || mb.mb_field_decoding_flag != cSlice().field_pic_flag)
    {
        for (Bs8u i = 0; i < 4; i++)
        {
            if (   mb.SubMbType[i] != B_Direct_8x8
                && SubMbPredMode(mb, i) != Pred_L0)
            {
                BS2_SET(CABAC::refIdxLX(i, 1), mb.pred.ref_idx_lX[1][i]);
                trace_arr[1] = true;
            }
        }
    }

    for (Bs8u m = 0; m < 4; m++)
    {
        if (   mb.SubMbType[m] != B_Direct_8x8
            && SubMbPredMode(mb, m) != Pred_L1)
        {
            for (Bs8u s = 0; s < NumSubMbPart(mb, m); s++)
            {
                for (Bs8u c = 0; c < 2; c++)
                {
                    BS2_SET(CABAC::mvdLX(m, s, !!c, 0), mb.pred.mvd_lX[0][m][s][c]);
                    trace_arr[2] = true;
                }
            }
        }
    }

    for (Bs8u m = 0; m < 4; m++)
    {
        if (   mb.SubMbType[m] != B_Direct_8x8
            && SubMbPredMode(mb, m) != Pred_L0)
        {
            for (Bs8u s = 0; s < NumSubMbPart(mb, m); s++)
            {
                for (Bs8u c = 0; c < 2; c++)
                {
                    BS2_SET(CABAC::mvdLX(m, s, !!c, 1), mb.pred.mvd_lX[1][m][s][c]);
                    trace_arr[3] = true;
                }
            }
        }
    }

    TLEnd();

    TLStart(TRACE_REF_IDX);
    if (trace_arr[0])
        BS2_TRACE_ARR(mb.pred.ref_idx_lX[0], 4, 0);
    if (trace_arr[1])
        BS2_TRACE_ARR(mb.pred.ref_idx_lX[1], 4, 0);
    TLEnd();

    TLStart(TRACE_MVD);
    if (trace_arr[2])
        BS2_TRACE_MDARR(Bs16s, mb.pred.mvd_lX[0], dim, 1, 0, "%6i ");
    if (trace_arr[3])
        BS2_TRACE_MDARR(Bs16s, mb.pred.mvd_lX[1], dim, 1, 0, "%6i ");
    TLEnd();
}

void SDParser::parseMbPred_CABAC(MB& mb)
{
    const Bs16u dim[] = { 1, 4, 4, 2 };
    Bs8u pm0 = MbPartPredMode(mb, 0);
    Bs8u numMbPart = NumMbPart(mb);
    bool trace_arr[4] = {};

    if (pm0 == Direct)
        return;

    if (   pm0 == Intra_4x4
        || pm0 == Intra_8x8
        || pm0 == Intra_16x16)
    {
        Bs16u nIdx = (pm0 == Intra_4x4) * 16 + (pm0 == Intra_8x8) * 4;

        if (nIdx)
        {
            TLStart(TRACE_RESIDUAL_FLAGS);

            for (Bs16u i = 0; i < nIdx; i++)
            {
                BS2_SET(CABAC::prevIntraPredModeFlag(),
                    mb.pred.Blk[i].prev_intra_pred_mode_flag);

                if (!mb.pred.Blk[i].prev_intra_pred_mode_flag)
                    BS2_SET(CABAC::remIntraPredMode(),
                        mb.pred.Blk[i].rem_intra_pred_mode);
            }

            TLEnd();

            TLStart(TRACE_PRED_MODE);

            if (pm0 == Intra_4x4)
            {
                BS2_SET_ARR_M(
                    Intra4x4PredMode(mb, Invers4x4Scan[_i],
                    mb.pred.Blk[Invers4x4Scan[_i]].prev_intra_pred_mode_flag,
                    mb.pred.Blk[Invers4x4Scan[_i]].rem_intra_pred_mode),
                    mb.pred.IntraPredMode, 16, 4, "%20s(%i), ", predMode4x4TraceMap);
            }
            else
            {
                BS2_SET_ARR_M(
                    Intra8x8PredMode(mb, _i,
                    mb.pred.Blk[_i].prev_intra_pred_mode_flag,
                    mb.pred.Blk[_i].rem_intra_pred_mode),
                    mb.pred.IntraPredMode, 4, 2, "%20s(%i), ", predMode8x8TraceMap);
            }

            TLEnd();
        }
        else
        {
            TLStart(TRACE_PRED_MODE);
            BS2_SETM((mb.MbType - I_16x16_0_0_0) % 4, mb.pred.IntraPredMode[0], predMode16x16TraceMap);
            TLEnd();
        }

        TLStart(TRACE_PRED_MODE);

        if (ChromaArrayType == 1 || ChromaArrayType == 2)
            BS2_SETM(CABAC::intraChromaPredMode(),
                mb.pred.intra_chroma_pred_mode, predModeChromaTraceMap);

        TLEnd();

        return;
    }

    TLStart(TRACE_RESIDUAL_FLAGS);

    if (   cSlice().num_ref_idx_l0_active_minus1 > 0
        || mb.mb_field_decoding_flag != cSlice().field_pic_flag)
    {
        for (Bs8u i = 0; i < numMbPart; i++)
        {
            if (MbPartPredMode(mb, i) != Pred_L1)
            {
                BS2_SET(CABAC::refIdxLX(i, 0), mb.pred.ref_idx_lX[0][i]);
                trace_arr[0] = true;
            }
        }
    }

    if (   cSlice().num_ref_idx_l1_active_minus1 > 0
        || mb.mb_field_decoding_flag != cSlice().field_pic_flag)
    {
        for (Bs8u i = 0; i < numMbPart; i++)
        {
            if (MbPartPredMode(mb, i) != Pred_L0)
            {
                BS2_SET(CABAC::refIdxLX(i, 1), mb.pred.ref_idx_lX[1][i]);
                trace_arr[1] = true;
            }
        }
    }

    for (Bs8u m = 0; m < numMbPart; m++)
    {
        if (MbPartPredMode(mb, m) != Pred_L1)
        {
            BS2_SET(CABAC::mvdLX(m, 0, 0, 0), mb.pred.mvd_lX[0][m][0][0]);
            BS2_SET(CABAC::mvdLX(m, 0, 1, 0), mb.pred.mvd_lX[0][m][0][1]);
            trace_arr[2] = true;
        }
    }

    for (Bs8u m = 0; m < numMbPart; m++)
    {
        if (MbPartPredMode(mb, m) != Pred_L0)
        {
            BS2_SET(CABAC::mvdLX(m, 0, 0, 1), mb.pred.mvd_lX[1][m][0][0]);
            BS2_SET(CABAC::mvdLX(m, 0, 1, 1), mb.pred.mvd_lX[1][m][0][1]);
            trace_arr[3] = true;
        }
    }

    TLEnd();

    TLStart(TRACE_REF_IDX);
    if (trace_arr[0])
        BS2_TRACE_ARR(mb.pred.ref_idx_lX[0], numMbPart, 0);
    if (trace_arr[1])
        BS2_TRACE_ARR(mb.pred.ref_idx_lX[1], numMbPart, 0);
    TLEnd();

    TLStart(TRACE_MVD);
    if (trace_arr[2])
        BS2_TRACE_MDARR(Bs16s, mb.pred.mvd_lX[0], dim, 1, 0, "%6i ");
    if (trace_arr[3])
        BS2_TRACE_MDARR(Bs16s, mb.pred.mvd_lX[1], dim, 1, 0, "%6i ");
    TLEnd();
}

void SDParser::parseResidual_CABAC(
    MB&    mb,
    Bs8u   startIdx,
    Bs8u   endIdx)
{
    TLAuto tl(*this, TRACE_RESIDUAL_ARRAYS);
    Bs16s i16x16DClevel[16 * 16] = {};
    Bs16s i16x16AClevel[16][16] = {};
    Bs16s level4x4[16][16] = {};
    Bs16s level8x8[4][64] = {};
    Bs16s ChromaDC[2][4 * 4] = {};
    Bs16s ChromaAC[2][4 * 4][16] = {};
    const Bs16u dimCDC[] = { 1, 2, 4u * 4u };
    const Bs16u dimCAC[] = { 1, (Bs16u)(4u * NumC8x8), 16 };

    BS2_TRACE_STR("Residual Luma:");
    parseResidualLuma_CABAC(
        mb, i16x16DClevel, i16x16AClevel, level4x4, level8x8, startIdx, endIdx, 0);

    if (ChromaArrayType == 0)
        return;

    if (ChromaArrayType == 1 || ChromaArrayType == 2)
    {
        Bs8u cbp = cbpC(mb);

        BS2_TRACE_STR("Residual Chroma:");

        if ((cbp & 3) && startIdx == 0)
        {
            for (Bs8u i = 0; i < 2; i++)
            {
                parseResidualBlock_CABAC(
                    mb, ChromaDC[i], 0, 4 * NumC8x8 - 1, 4 * NumC8x8, 0, ChromaDCLevel, !i);
            }
            BS2_TRACE_MDARR(Bs16s, ChromaDC, dimCDC, 1, 0, "%6i ");
        }

        if ((cbp & 2) && startIdx == 0)
        {
            for (Bs8u i = 0; i < 2; i++)
            {
                for (Bs8u blkIdx = 0; blkIdx < NumC8x8 * 4; blkIdx++)
                {
                    parseResidualBlock_CABAC(
                        mb, ChromaAC[i][blkIdx],
                        startIdx ? startIdx - 1 : 0, endIdx - 1, 15,
                        blkIdx, ChromaACLevel, !i);
                }
                BS2_TRACE_MDARR(Bs16s, ChromaAC[i], dimCAC, 1, 0, "%6i ");
            }
        }

        return;
    }

    BS2_TRACE_STR("Residual Cb:");
    parseResidualLuma_CABAC(
        mb, i16x16DClevel, i16x16AClevel, level4x4, level8x8, startIdx, endIdx, BLK_Cb);

    BS2_TRACE_STR("Residual Cr:");
    parseResidualLuma_CABAC(
        mb, i16x16DClevel, i16x16AClevel, level4x4, level8x8, startIdx, endIdx, BLK_Cr);
}

void SDParser::parseResidualLuma_CABAC(
    MB&    mb,
    Bs16s  i16x16DClevel[16 * 16],
    Bs16s  i16x16AClevel[16][16],
    Bs16s  level4x4[16][16],
    Bs16s  level8x8[4][64],
    Bs8u   startIdx,
    Bs8u   endIdx,
    Bs8u   blkType)
{
    Bs8u pm0 = MbPartPredMode(mb, 0);
    Bs8u cbp = cbpY(mb);
    const Bs16u dim4x64[] = { 1, 4, 64 };
    const Bs16u dim16x16[] = { 1, 16, 16 };

    if (startIdx == 0 && pm0 == Intra_16x16)
    {
        parseResidualBlock_CABAC(
            mb, i16x16DClevel,
            0, 15, 16, 0,
            blkType | BLK_16x16 | BLK_DC, false);
        BS2_TRACE_ARR(i16x16DClevel, 16, 0);
    }

    if (mb.transform_size_8x8_flag)
    {
        for (Bs8u i = 0; i < 4; i++)
        {
            if (!(cbp & (1 << i)))
                continue;

            parseResidualBlock_CABAC(
                mb, level8x8[i],
                4 * startIdx, 4 * endIdx + 3, 64, i,
                blkType | BLK_8x8, false);
        }
        BS2_TRACE_MDARR(Bs16s, level8x8, dim4x64, 1, 16, "%6i ");
        return;
    }

    if (pm0 == Intra_16x16)
    {
        for (Bs8u i8x8 = 0; i8x8 < 4; i8x8++)
        {
            if (!(cbp & (1 << i8x8)))
                continue;

            for (Bs8u i = i8x8 * 4; i < (i8x8 + 1) * 4; i++)
            {
                parseResidualBlock_CABAC(
                    mb, i16x16AClevel[i],
                    startIdx ? startIdx - 1 : 0, endIdx - 1, 15, i,
                    blkType | BLK_16x16 | BLK_AC, false);
            }
        }

        BS2_TRACE_MDARR(Bs16s, i16x16AClevel, dim16x16, 1, 0, "%6i ");
        return;
    }


    for (Bs8u i8x8 = 0; i8x8 < 4; i8x8++)
    {
        if (!(cbp & (1 << i8x8)))
            continue;

        for (Bs8u i = i8x8 * 4; i < (i8x8 + 1) * 4; i++)
        {
            parseResidualBlock_CABAC(
                mb, level4x4[i],
                startIdx, endIdx, 16, i,
                blkType | BLK_4x4, false);
        }
    }

    BS2_TRACE_MDARR(Bs16s, level4x4, dim16x16, 1, 0, "%6i ");
}

void setCBF(CodedBlockFlags& cbf, Bs8u blkIdx, Bs8u blkType, bool isCb, bool f)
{
    switch (blkType)
    {
    case Intra16x16DCLevel  : cbf.lumaDC = f; return;
    case Intra16x16ACLevel  : cbf.luma4x4 |= (f << blkIdx); return;
    case LumaLevel4x4       : cbf.luma4x4 |= (f << blkIdx); return;
    case LumaLevel8x8       : cbf.luma8x8 |= (f << blkIdx); return;
    case CbIntra16x16DCLevel: cbf.CbDC = f; return;
    case CbIntra16x16ACLevel: cbf.Cb4x4 |= (f << blkIdx); return;
    case CbLumaLevel4x4     : cbf.Cb4x4 |= (f << blkIdx); return;
    case CbLumaLevel8x8     : cbf.Cb8x8 |= (f << blkIdx); return;
    case CrIntra16x16DCLevel: cbf.CrDC = f; return;
    case CrIntra16x16ACLevel: cbf.Cr4x4 |= (f << blkIdx); return;
    case CrLumaLevel4x4     : cbf.Cr4x4 |= (f << blkIdx); return;
    case CrLumaLevel8x8     : cbf.Cr8x8 |= (f << blkIdx); return;
    case ChromaDCLevel:
        if (isCb) cbf.CbDC = f;
        else      cbf.CrDC = f;
        return;
    case ChromaACLevel:
        if (isCb) cbf.Cb4x4 |= (f << blkIdx);
        else      cbf.Cr4x4 |= (f << blkIdx);
        return;
    default: break;
    }
}

void SDParser::parseResidualBlock_CABAC(
    MB&     mb,
    Bs16s* coeffLevel,
    Bs8u   startIdx,
    Bs8u   endIdx,
    Bs8u   maxNumCoeff,
    Bs8u   blkIdx,
    Bs8u   blkType,
    bool   isCb)
{
    TLAuto tl(*this, TRACE_RESIDUAL_FLAGS);
    bool  coded_block_flag = true
        , lscf = false
        , coeff_sign_flag = false;
    bool  scf[64] = {};
    Bs16u coeff_abs_level = 0;
    Bs8u  ctxBlockCat, eq1 = 0, gt1 = 0;
    Bs8s i;

    switch(blkType)
    {
    case Intra16x16DCLevel  : ctxBlockCat =  0; break;
    case Intra16x16ACLevel  : ctxBlockCat =  1; break;
    case LumaLevel4x4       : ctxBlockCat =  2; break;
    case ChromaDCLevel      : ctxBlockCat =  3; break;
    case ChromaACLevel      : ctxBlockCat =  4; break;
    case LumaLevel8x8       : ctxBlockCat =  5; break;
    case CbIntra16x16DCLevel: ctxBlockCat =  6; break;
    case CbIntra16x16ACLevel: ctxBlockCat =  7; break;
    case CbLumaLevel4x4     : ctxBlockCat =  8; break;
    case CbLumaLevel8x8     : ctxBlockCat =  9; break;
    case CrIntra16x16DCLevel: ctxBlockCat = 10; break;
    case CrIntra16x16ACLevel: ctxBlockCat = 11; break;
    case CrLumaLevel4x4     : ctxBlockCat = 12; break;
    case CrLumaLevel8x8     : ctxBlockCat = 13; break;
    default: throw InvalidSyntax();
    }

    if (maxNumCoeff != 64 || ChromaArrayType == 3)
        BS2_SET(CABAC::codedBlockFlag(blkIdx, ctxBlockCat, isCb), coded_block_flag);

    setCBF(mb.cbf, blkIdx, blkType, isCb, coded_block_flag);

    if (coded_block_flag)
    {
        Bs8u numCoeff = endIdx + 1;
        i = startIdx;

        for (; i < numCoeff - 1; i++)
        {
            BS2_SET(CABAC::SCF(ctxBlockCat, i), scf[i]);

            if (scf[i])
            {
                BS2_SET(CABAC::LSCF(ctxBlockCat, i), lscf);

                if (lscf)
                    numCoeff = i + 1;
            }
        }

        //BS2_TRACE(numCoeff, numCoeff);
        i = numCoeff - 1;

        BS2_SET(CABAC::coeffAbsLevel(ctxBlockCat, gt1, eq1), coeff_abs_level);
        BS2_SET(CABAC::coeffSignFlag(), coeff_sign_flag);

        eq1 += (coeff_abs_level == 1);
        gt1 += (coeff_abs_level >  1);

        coeffLevel[i] = (1 - 2 * coeff_sign_flag) * coeff_abs_level;

        for (i = numCoeff - 2; i >= startIdx; i--)
        {
            if (!scf[i])
                continue;

            BS2_SET(CABAC::coeffAbsLevel(ctxBlockCat, gt1, eq1), coeff_abs_level);
            BS2_SET(CABAC::coeffSignFlag(), coeff_sign_flag);

            eq1 += (coeff_abs_level == 1);
            gt1 += (coeff_abs_level >  1);

            coeffLevel[i] = (1 - 2 * coeff_sign_flag) * coeff_abs_level;
        }
    }
}