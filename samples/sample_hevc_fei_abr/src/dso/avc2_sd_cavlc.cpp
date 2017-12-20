#include "avc2_parser.h"

using namespace BS_AVC2;
using namespace BsReader2;

void SDParser::parseSD_CAVLC(Slice& s)
{
    bool  moreDataFlag  = true;
    bool  prevMbSkipped = false;
    Bs16u mb_skip_run = 0;
    MB zeroMB;

    memset(&zeroMB, 0, sizeof(MB));

    if (m_mb.capacity() < PicSizeInMbs)
        m_mb.resize(PicSizeInMbs);

    m_mb.resize(0);

    m_mb.push_back(zeroMB);
    newMB(m_mb.back());

    for (;;)
    {
        MB& mb = m_mb.back();
        Bs64u startByte = GetByteOffset();
        Bs16u startBit  = GetBitOffset();

        if (mb_skip_run)
        {
            mb.mb_skip_flag = 1;

            if (mb.mb_skip_flag)
            {
                TLStart(TRACE_MB_TYPE);
                BS2_SETM(isB ? B_Skip : P_Skip, mb.MbType, mbTypeTraceMap);
                TLEnd();
            }

            moreDataFlag = --mb_skip_run || more_rbsp_data();
        }
        else
        {
            if (!more_rbsp_data())
                break;

            if (TLTest(TRACE_MARKERS))
                BS2_TRACE_STR("--- MB ---");

            TLStart(TRACE_MB_ADDR);
            BS2_TRACE(mb.Addr, mb.Addr);
            TLEnd();

            TLStart(TRACE_MB_LOC);
            BS2_TRACE(mb.x, mb.x);
            BS2_TRACE(mb.y, mb.y);
            TLEnd();

            if (!isI && !isSI && !prevMbSkipped)
            {
                TLStart(TRACE_RESIDUAL_FLAGS);
                BS2_SET(ue(), mb_skip_run);
                prevMbSkipped = !!mb_skip_run;
                TLEnd();

                if (mb_skip_run)
                {
                    TLStart(TRACE_SIZE);
                    BS2_SET(Bs16u(GetByteOffset() - startByte) * 8
                        + GetBitOffset() - startBit, mb.NumBits);
                    TLEnd();
                    continue;
                }
            }

            if (moreDataFlag)
            {
                TLStart(TRACE_MB_FIELD);

                if (MbaffFrameFlag && (mb.Addr % 2 == 0 || prevMbSkipped))
                    BS2_SET(u1(), mb.mb_field_decoding_flag);

                TLEnd();

                if (MbaffFrameFlag && mb.Addr % 2 == 1 && prevMbSkipped)
                    MBRef(mb.Addr - 1).mb_field_decoding_flag = mb.mb_field_decoding_flag;

                parseMB_CAVLC(mb);
            }

            prevMbSkipped = 0;
            moreDataFlag = more_rbsp_data();

            TLStart(TRACE_SIZE);
            BS2_SET(Bs16u(GetByteOffset() - startByte) * 8 + GetBitOffset() - startBit, mb.NumBits);
            TLEnd();
        }

        if (!moreDataFlag)
            break;

        m_mb.push_back(zeroMB);
        newMB(m_mb.back());
    }

    m_mb.back().end_of_slice_flag = 1;

    if (!TrailingBits())
        throw InvalidSyntax();

    TLStart(TRACE_SIZE);
    BS2_SET((Bs32u)m_mb.size(), s.NumMB);
    TLEnd();
}

void SDParser::parseMB_CAVLC(MB& mb)
{
    TLStart(TRACE_MB_TYPE);
    BS2_SETM(MbType(ue()), mb.MbType, mbTypeTraceMap);
    mb.SubMbType[0] = mb.SubMbType[1] = mb.SubMbType[2] = mb.SubMbType[3] = mb.MbType;
    TLEnd();

    if (mb.MbType == I_PCM)
    {
        TLAuto tl(*this, TRACE_RESIDUAL_ARRAYS);

        while (GetBitOffset())
        {
            if (u1()) //pcm_alignment_zero_bit
                throw InvalidSyntax();
        }

        BS2_TRACE_ARR_VF(u(BitDepthY), pcm_sample_luma, 256, 16, "%02X ");
        BS2_TRACE_ARR_VF(u(BitDepthC), pcm_sample_chroma,
            MbWidthC * MbHeightC * 2U, MbWidthC, "%02X ");

        return;
    }

    Bs8u noSubMbPartSizeLessThan8x8Flag = 1;
    Bs8s MbPartPredMode0 = MbPartPredMode(mb, 0);

    if (    mb.MbType != I_NxN
        && MbPartPredMode0 != Intra_16x16
        && NumMbPart(mb) == 4)
    {
        parseSubMbPred_CAVLC(mb);

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
        TLStart(TRACE_MB_TRANS8x8);
        if (cPPS().transform_8x8_mode_flag && mb.MbType == I_NxN)
            BS2_SET(u1(), mb.transform_size_8x8_flag);
        TLEnd();

        parseMbPred_CAVLC(mb);
    }

    if (MbPartPredMode0 != Intra_16x16)
    {
        TLStart(TRACE_CBP);
        BS2_SET(CAVLC::CBP(), mb.coded_block_pattern);
        TLEnd();

        if (   cbpY(mb) > 0
            && cPPS().transform_8x8_mode_flag
            && mb.MbType != I_NxN
            && noSubMbPartSizeLessThan8x8Flag
            && (mb.MbType != B_Direct_16x16 || cSPS().direct_8x8_inference_flag))
        {
            TLStart(TRACE_MB_TRANS8x8);
            BS2_SET(u1(), mb.transform_size_8x8_flag);
            TLEnd();
        }
    }

    if (haveResidual(mb))
    {
        TLStart(TRACE_MB_QP_DELTA);
        BS2_SET(se(), mb.mb_qp_delta);
        TLEnd();

        TLStart(TRACE_MB_QP);
        BS2_SET(QPy(mb), mb.QPy);
        TLEnd();

        parseResidual_CAVLC(mb, 0, 15);
    }
}

void SDParser::parseSubMbPred_CAVLC(MB& mb)
{
    const Bs16u dim[] = { 1, 4, 4, 2 };
    bool trace_arr[4] = {};
    Bs32u x0 = cSlice().num_ref_idx_l0_active_minus1;
    Bs32u x1 = cSlice().num_ref_idx_l1_active_minus1;
    Bs8u inc = MBTYPE_SubB_start * isB + MBTYPE_SubP_start * isP;

    if (MbaffFrameFlag && mb.mb_field_decoding_flag)
    {
        x0 = x0 * 2 + 1;
        x1 = x1 * 2 + 1;
    }

    TLStart(TRACE_SUB_MB_TYPE);
    BS2_SET_ARR_M(ue() + inc, mb.SubMbType, 4, 2, "%12s(%2i), ", mbTypeTraceMap);
    TLEnd();

    TLStart(TRACE_RESIDUAL_FLAGS);

    if (  (cSlice().num_ref_idx_l0_active_minus1 > 0
        || mb.mb_field_decoding_flag != cSlice().field_pic_flag)
        && mb.MbType != P_8x8ref0)
    {
        for (Bs8u i = 0; i < 4; i++)
        {
            if (   mb.SubMbType[i] != B_Direct_8x8
                && SubMbPredMode(mb, i) != Pred_L1)
            {
                BS2_SET(te(x0), mb.pred.ref_idx_lX[0][i]);
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
                BS2_SET(te(x1), mb.pred.ref_idx_lX[1][i]);
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
                    BS2_SET(se(), mb.pred.mvd_lX[0][m][s][c]);
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
                    BS2_SET(se(), mb.pred.mvd_lX[1][m][s][c]);
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


void SDParser::parseMbPred_CAVLC(MB& mb)
{
    const Bs16u dim[] = { 1, 4, 4, 2 };
    Bs8u pm0 = MbPartPredMode(mb, 0);
    Bs8u numMbPart = NumMbPart(mb);
    bool trace_arr[4] = {};
    Bs32u x0 = cSlice().num_ref_idx_l0_active_minus1;
    Bs32u x1 = cSlice().num_ref_idx_l1_active_minus1;

    if (pm0 == Direct)
        return;

    if (MbaffFrameFlag && mb.mb_field_decoding_flag)
    {
        x0 = x0 * 2 + 1;
        x1 = x1 * 2 + 1;
    }

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
                BS2_SET(u1(), mb.pred.Blk[i].prev_intra_pred_mode_flag);

                if (!mb.pred.Blk[i].prev_intra_pred_mode_flag)
                    BS2_SET(u(3), mb.pred.Blk[i].rem_intra_pred_mode);
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
            BS2_SETM(ue(), mb.pred.intra_chroma_pred_mode, predModeChromaTraceMap);

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
                BS2_SET(te(x0), mb.pred.ref_idx_lX[0][i]);
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
                BS2_SET(te(x1), mb.pred.ref_idx_lX[1][i]);
                trace_arr[1] = true;
            }
        }

    }

    for (Bs8u m = 0; m < numMbPart; m++)
    {
        if (MbPartPredMode(mb, m) != Pred_L1)
        {
            BS2_SET(se(), mb.pred.mvd_lX[0][m][0][0]);
            BS2_SET(se(), mb.pred.mvd_lX[0][m][0][1]);
            trace_arr[2] = true;
        }
    }

    for (Bs8u m = 0; m < numMbPart; m++)
    {
        if (MbPartPredMode(mb, m) != Pred_L0)
        {
            BS2_SET(se(), mb.pred.mvd_lX[1][m][0][0]);
            BS2_SET(se(), mb.pred.mvd_lX[1][m][0][1]);
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

void SDParser::parseResidual_CAVLC(
    MB&    mb,
    Bs8u   startIdx,
    Bs8u   endIdx)
{
    TLAuto tl(*this, TRACE_RESIDUAL_ARRAYS);
    Bs16s i16x16DClevel[16 * 16] = {};
    Bs16s i16x16AClevel[16][16] = {};
    Bs16s level4x4[16][16] = {};
    Bs16s ChromaDC[2][4 * 4] = {};
    Bs16s ChromaAC[2][4 * 4][16] = {};
    const Bs16u dimCDC[] = { 1, 2, 4 * 4 };
    const Bs16u dimCAC[] = { 1, (Bs16u)(4u * NumC8x8), 16 };

    BS2_TRACE_STR("Residual Luma:");
    parseResidualLuma_CAVLC( mb, i16x16DClevel, i16x16AClevel, level4x4, startIdx, endIdx, 0);

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
                parseResidualBlock_CAVLC(
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
                    parseResidualBlock_CAVLC(
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
    parseResidualLuma_CAVLC(
        mb, i16x16DClevel, i16x16AClevel, level4x4, startIdx, endIdx, BLK_Cb);

    BS2_TRACE_STR("Residual Cr:");
    parseResidualLuma_CAVLC(
        mb, i16x16DClevel, i16x16AClevel, level4x4, startIdx, endIdx, BLK_Cr);
}

void SDParser::parseResidualLuma_CAVLC(
    MB&    mb,
    Bs16s  i16x16DClevel[16 * 16],
    Bs16s  i16x16AClevel[16][16],
    Bs16s  level4x4[16][16],
    Bs8u   startIdx,
    Bs8u   endIdx,
    Bs8u   blkType)
{
    Bs8u pm0 = MbPartPredMode(mb, 0);
    Bs8u cbp = cbpY(mb);
    const Bs16u dim16x16[] = { 1, 16, 16 };

    if (startIdx == 0 && pm0 == Intra_16x16)
    {
        parseResidualBlock_CAVLC(mb, i16x16DClevel, 0, 15, 16,
            0, blkType | BLK_16x16 | BLK_DC, false);
        BS2_TRACE_ARR(i16x16DClevel, 16, 0);
    }

    if (pm0 == Intra_16x16)
    {
        for (Bs8u i8x8 = 0; i8x8 < 4; i8x8++)
        {
            if (!(cbp & (1 << i8x8)))
                continue;

            for (Bs8u i = i8x8 * 4; i < (i8x8 + 1) * 4; i++)
            {
                parseResidualBlock_CAVLC(
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
            parseResidualBlock_CAVLC(
                mb, level4x4[i],
                startIdx, endIdx, 16, i,
                blkType | BLK_4x4, false);
        }
    }

    BS2_TRACE_MDARR(Bs16s, level4x4, dim16x16, 1, 0, "%6i ");
}

void setCT(CoeffTokens& ct, Bs16u blkType, Bs16u blkIdx, bool isCb, Bs16u f)
{
    switch (blkType)
    {
    case Intra16x16DCLevel  : ct.lumaDC = f; return;
    case CbIntra16x16DCLevel: ct.CbDC   = f; return;
    case CrIntra16x16DCLevel: ct.CrDC   = f; return;
    case Intra16x16ACLevel  : ct.luma4x4[blkIdx] = f; return;
    case LumaLevel4x4       : ct.luma4x4[blkIdx] = f; return;
    case CbIntra16x16ACLevel: ct.Cb4x4[blkIdx]   = f; return;
    case CbLumaLevel4x4     : ct.Cb4x4[blkIdx]   = f; return;
    case CrIntra16x16ACLevel: ct.Cr4x4[blkIdx]   = f; return;
    case CrLumaLevel4x4     : ct.Cr4x4[blkIdx]   = f; return;
    case ChromaDCLevel      :
        if (isCb) ct.CbDC = f;
        else      ct.CrDC = f;
        return;
    case ChromaACLevel      :
        if (isCb) ct.Cb4x4[blkIdx] = f;
        else      ct.Cr4x4[blkIdx] = f;
        return;
    default: throw InvalidSyntax();
    }
}

void SDParser::parseResidualBlock_CAVLC(
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
    Bs16u coeff_token = 0;
    Bs16u suffixLength = 0;
    Bs8u  trailing_ones_sign_flag = 0;
    Bs16s levelVal[16] = {};
    Bs16s runVal[16] = {};
    Bs16s coeffNum = -1;
    Bs16u zerosLeft = 0;
    Bs16u total_zeros = 0;
    Bs16u run_before = 0;

    BS2_SET(CAVLC::CoeffToken(blkType, blkIdx, isCb), coeff_token);
    setCT(mb.ct, blkType, blkIdx, isCb, coeff_token);

    if (TotalCoeff(coeff_token) == 0)
        return;

    if (TotalCoeff(coeff_token) > 10 && TrailingOnes(coeff_token) < 3)
        suffixLength = 1;
    else
        suffixLength = 0;

    for (Bs16u i = 0; i < TotalCoeff(coeff_token); i++)
    {
        Bs16u level_prefix = 0;
        Bs16u level_suffix = 0;
        Bs16s levelCode = 0;

        if (i < TrailingOnes(coeff_token))
        {
            BS2_SET(CAVLC::TrailingOnesSign(), trailing_ones_sign_flag);
            levelVal[i] = 1 - 2 * trailing_ones_sign_flag;
        }
        else
        {
            BS2_SET(CAVLC::LevelPrefix(), level_prefix);

            levelCode = (BS_MIN(15, level_prefix) << suffixLength);

            if (suffixLength > 0 || level_prefix >= 14)
            {
                BS2_SET(CAVLC::LevelSuffix(suffixLength, level_prefix), level_suffix);
                levelCode += level_suffix;
            }

            if (level_prefix >= 15 && suffixLength == 0)
                levelCode += 15;

            if (level_prefix >= 16)
                levelCode += (1 << (level_prefix - 3)) - 4096;

            if (   i == TrailingOnes(coeff_token)
                && TrailingOnes(coeff_token) < 3)
                levelCode += 2;

            if (levelCode % 2 == 0)
                levelVal[i] = (levelCode + 2) >> 1;
            else
                levelVal[i] = (-levelCode - 1) >> 1;

            if (suffixLength == 0)
                suffixLength = 1;

            if (BS_ABS(levelVal[i]) > (3 << (suffixLength - 1)) && suffixLength < 6)
                suffixLength++;
        }
    }

    if (TotalCoeff(coeff_token) < (endIdx - startIdx + 1))
    {
        BS2_SET(CAVLC::TotalZeros(maxNumCoeff, TotalCoeff(coeff_token)), total_zeros);
        zerosLeft = total_zeros;
    }
    else
        zerosLeft = 0;

    for (Bs16u i = 0; i < TotalCoeff(coeff_token) - 1; i++)
    {
        if (zerosLeft > 0)
        {
            BS2_SET(CAVLC::RunBefore(zerosLeft), run_before);
            runVal[i] = run_before;
        }
        else
            runVal[i] = 0;

        zerosLeft = zerosLeft - runVal[i];
    }

    runVal[(TotalCoeff(coeff_token) - 1) % 16] = zerosLeft;

    for (Bs16s i = TotalCoeff(coeff_token) - 1; i >= 0; i--)
    {
        coeffNum += runVal[i] + 1;

        if (coeffNum > endIdx)
            throw InvalidSyntax();

        coeffLevel[startIdx + coeffNum] = levelVal[i];
    }
}