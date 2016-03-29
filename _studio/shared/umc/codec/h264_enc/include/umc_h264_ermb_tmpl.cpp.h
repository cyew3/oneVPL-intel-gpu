////               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004 - 2016 Intel Corporation. All Rights Reserved.
//

#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#if PIXBITS == 8

#define PIXTYPE Ipp8u
#define COEFFSTYPE Ipp16s
#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s

#elif PIXBITS == 16

#define PIXTYPE Ipp16u
#define COEFFSTYPE Ipp32s
#define H264ENC_MAKE_NAME(NAME) NAME##_16u32s

#else

void H264EncoderFakeFunction() { UNSUPPORTED_PIXBITS; }

#endif //PIXBITS

#define H264CoreEncoderType H264ENC_MAKE_NAME(H264CoreEncoder)
#define H264SliceType H264ENC_MAKE_NAME(H264Slice)
#define H264CurrentMacroblockDescriptorType H264ENC_MAKE_NAME(H264CurrentMacroblockDescriptor)

Ipp16s normTable8x8[] = {
    2048, 2312, 1280, 2312, 2048, 2312, 1280, 2312,
    2312, 2610, 1445, 2610, 2312, 2610, 1445, 2610,
    1280, 1445,  800, 1445, 1280, 1445,  800, 1445,
    2312, 2610, 1445, 2610, 2312, 2610, 1445, 2610,
    2048, 2312, 1280, 2312, 2048, 2312, 1280, 2312,
    2312, 2610, 1445, 2610, 2312, 2610, 1445, 2610,
    1280, 1445,  800, 1445, 1280, 1445,  800, 1445,
    2312, 2610, 1445, 2610, 2312, 2610, 1445, 2610,
};


void ScanIdxZero(Ipp16s *coeffs, Ipp32s start, Ipp32s endp1, const Ipp32s *dec_scan, Ipp32s len) {
    int i;

    for (i = 0; i < start; i++)
        coeffs[dec_scan[i]] = 0;
    for (i = endp1; i < len; i++)
        coeffs[dec_scan[i]] = 0;
}


Ipp16s normTable4x4[] = {64, 80, 64, 80, 80, 100, 80, 100, 64, 80, 64, 80, 80, 100, 80, 100};

/////

Ipp32u CBPcalculate(Ipp16s *coeffPtr)
{
    Ipp32s blockNum = 0;
    Ipp32u cbp4x4;
    int iii;

    cbp4x4 = 0;

    for (iii = 0; iii < 16; iii++)
    {
        Ipp64s *tmpCoeffPtr = (Ipp64s *)(coeffPtr + block_subblock_mapping[iii] * 16);
        if ((tmpCoeffPtr[0] | tmpCoeffPtr[1] | tmpCoeffPtr[2] | tmpCoeffPtr[3]) != 0)
        {
            cbp4x4 |= (1 << blockNum);
        }
        tmpCoeffPtr += 4;
        blockNum++;
    }

    return cbp4x4;
}

Ipp32u CBPcalculate_8x8(Ipp16s *coeffPtr)
{
    Ipp64s *tmpCoeffPtr = (Ipp64s *)coeffPtr;
    Ipp32s blockNum = 0;
    Ipp32u cbp4x4;
    int iii, jjj;

    cbp4x4 = 0;

    for (iii = 0; iii < 4; iii++)
    {
        Ipp32s tmp = 0;
        for (jjj = 0; jjj < 4; jjj++)
        {
            if ((tmpCoeffPtr[0] | tmpCoeffPtr[1] | tmpCoeffPtr[2] | tmpCoeffPtr[3]) != 0)
            {
                tmp = 0xf;

            }
            tmpCoeffPtr += 4;
        }
        cbp4x4 |= (tmp << blockNum);
        blockNum += 4;
    }

    return cbp4x4;
}

Ipp32u CBPcalculateResidual(Ipp16s *coeffPtr,
                            Ipp32s pitch)
{
    Ipp32s blockNum = 0;
    Ipp32u cbp4x4;
    int iii, jjj;

    cbp4x4 = 0;

    for (iii = 0; iii < 4; iii++)
    {
        for (jjj = 0; jjj < 4; jjj++)
        {
            Ipp32s index  = (iii >> 1) * 8 * pitch + (iii & 1) * 8 +
                (jjj >> 1) * 4 * pitch + (jjj & 1) * 4;
            Ipp64s tmp0 = *(Ipp64s *)(coeffPtr + index);
            Ipp64s tmp1 = *(Ipp64s *)(coeffPtr + index + pitch);
            Ipp64s tmp2 = *(Ipp64s *)(coeffPtr + index + 2 * pitch);
            Ipp64s tmp3 = *(Ipp64s *)(coeffPtr + index + 3 * pitch);

            if ((tmp0 | tmp1 | tmp2 | tmp3) != 0)
            {
                cbp4x4 |= (1 << blockNum);
            }
            blockNum++;
        }
    }

    return cbp4x4;
}

Ipp32u CBPcalculateResidual_8x8(Ipp16s *coeffPtr,
                                Ipp32s pitch)
{
    Ipp32s cbp = CBPcalculateResidual(coeffPtr, pitch);
    Ipp32s i, m = 0xf;

    for (i = 0; i < 4; i++) {
        if (cbp & m) cbp |= m;
        m <<= 4;
    }

    return cbp;
}

void ReconstructLumaBaseMode(COEFFSTYPE *tmpCoeffPtr,
                             Ipp8u *m_pYPlane,
                             Ipp32u offsetY,
                             Ipp32u rec_pitch_luma)
{
    Ipp32s blockNum = 1;
    int iii, jjj;

    for (iii = 0; iii < 4; iii++)
    {
        for (jjj = 0; jjj < 4; jjj++)
        {
            Ipp32s index  = (iii >> 1) * 8 * rec_pitch_luma + (iii & 1) * 8 +
                (jjj >> 1) * 4 * rec_pitch_luma + (jjj & 1) * 4;
            Ipp8u* pYPlane = (Ipp8u*)(m_pYPlane + offsetY + index);


            ippiTransformResidualAndAdd_H264_16s8u_C1I(pYPlane,rec_pitch_luma,
                tmpCoeffPtr,
                pYPlane,
                rec_pitch_luma);

            tmpCoeffPtr += 16;
            blockNum++;
        }
    }
}


void ReconstructLumaBaseModeResidual(COEFFSTYPE *tmpCoeffPtr,
                                     COEFFSTYPE *m_pYResidualPred,
                                     COEFFSTYPE *m_pYResidual,
                                     Ipp32u offsetY,
                                     Ipp32u rec_pitch_luma)
{
    Ipp32s blockNum = 1;
    int iii, jjj, k, l;

    if (m_pYResidualPred != NULL)
    {
        for (iii = 0; iii < 4; iii++)
        {
            for (jjj = 0; jjj < 4; jjj++)
            {
                Ipp32s index = xoff[4*iii+jjj] + yoff[4*iii+jjj] * rec_pitch_luma;
                Ipp16s* pYPlanePred = m_pYResidualPred + offsetY + index;
                Ipp16s* pYPlane = m_pYResidual + offsetY + index;

                ippiTransformResidualAndAdd_H264_16s16s_C1I(pYPlanePred,
                    tmpCoeffPtr + block_subblock_mapping[iii*4+jjj] * 16,
                    pYPlane, rec_pitch_luma << 1,
                    rec_pitch_luma << 1);

                for (k = 0; k < 4; k++) {
                    for (l = 0; l < 4; l++) {
                        pYPlane[l] = IPP_MIN(pYPlane[l], 255);
                        pYPlane[l] = IPP_MAX(pYPlane[l], -255);
                    }
                    pYPlane += rec_pitch_luma;
                }

                blockNum++;
            }
        }
    }
}

void ReconstructLumaBaseModeResidual_8x8(COEFFSTYPE *tmpCoeffPtr,
                                         COEFFSTYPE *m_pYResidualPred,
                                         COEFFSTYPE *m_pYResidual,
                                         Ipp32u offsetY,
                                         Ipp32u rec_pitch_luma)
{
    Ipp32s blockNum = 1;
    int iii, k, l;

    if (m_pYResidualPred != NULL)
    {
        for (iii = 0; iii < 4; iii++)
        {
            Ipp32s index  = (iii >> 1) * 8 * rec_pitch_luma + (iii & 1) * 8;
            Ipp16s* pYPlanePred = m_pYResidualPred + offsetY + index;
            Ipp16s* pYPlane = m_pYResidual + offsetY + index;

            ippiTransformResidualAndAdd8x8_H264_16s16s_C1I(pYPlanePred,
                tmpCoeffPtr,
                pYPlane, rec_pitch_luma << 1,
                rec_pitch_luma << 1);

            for (k = 0; k < 8; k++) {
                for (l = 0; l < 8; l++) {
                    pYPlane[l] = IPP_MIN(pYPlane[l], 255);
                    pYPlane[l] = IPP_MAX(pYPlane[l], -255);
                }
                pYPlane += rec_pitch_luma;
            }

            tmpCoeffPtr += 64;
            blockNum++;
        }
    }
}

void AddMacroblockSat(Ipp16s *src, Ipp8u *srcdst, int len, int stride1, int stride2)
{
    int ii, jj;
    for (ii = 0; ii < len; ii++) {
        for (jj = 0; jj < len; jj++) {
            Ipp16s tmp = src[jj] + srcdst[jj];
            srcdst[jj] = (Ipp8u)(tmp > 255 ? 255 : (tmp < 0 ? 0 : tmp));
        }
        src += stride1;
        srcdst += stride2;
    }
}

////

////////////////////////////////////////////////////////////////////////////////
// CEncAndRec4x4IntraBlock
//
// Encode and Reconstruct one blocks in an Intra macroblock with 4x4 prediction
//
////////////////////////////////////////////////////////////////////////////////
void H264ENC_MAKE_NAME(H264CoreEncoder_Encode4x4IntraBlock)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s block)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32s      iNumCoeffs=0;
    Ipp32s      iLastCoeff=0;
    __ALIGN16 Ipp16s pDiffBuf[16];
    COEFFSTYPE*  pTransformResult;
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32u uMBQP       = cur_mb.lumaQP;
    Ipp32s pitchPixels = cur_mb.mbPitchPixels;
    Ipp32u uCBPLuma     = cur_mb.m_uIntraCBP4x4;
    PIXTYPE* pBlockData = cur_mb.mbPtr + xoff[block] + yoff[block]*pitchPixels;
    PIXTYPE* pPredBuf = cur_mb.mb4x4.prediction + xoff[block] + yoff[block]*16;
    PIXTYPE* pReconBuf = cur_mb.mb4x4.reconstruct + xoff[block] + yoff[block]*16;
    __ALIGN16 COEFFSTYPE pTransRes[16];

    pTransformResult = &cur_mb.mb4x4.transform[block*16];
    Diff4x4(pPredBuf, pBlockData, pitchPixels, pDiffBuf);
    if (!core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag || uMBQP != 0) {
        H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
            pDiffBuf,
            pTransformResult,
            uMBQP,
            &iNumCoeffs,
            1,
            enc_single_scan[curr_slice->m_is_cur_mb_field],
            &iLastCoeff,
            NULL,
            NULL,
            0,
            NULL); //Always use f for INTRA
        if (!iNumCoeffs) {
            Copy4x4(pPredBuf, 16, pReconBuf, 16);
            uCBPLuma &= ~CBP4x4Mask[block];
        } else {
            MFX_INTERNAL_CPY( pTransRes, pTransformResult, 16*sizeof( COEFFSTYPE ));
            H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264)(
                pPredBuf,
                pTransRes,
                NULL,
                pReconBuf,
                16,
                16,
                uMBQP,
                ((iNumCoeffs < -1) || (iNumCoeffs > 0)),
                core_enc->m_PicParamSet->bit_depth_luma,
                NULL);
        }
    } else {
        // Transform bypass => lossless.
        Copy4x4(pBlockData, pitchPixels, pReconBuf, 16);
        for( Ipp32s i = 0; i < 16; i++) pTransformResult[i] = pDiffBuf[i];
        ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan[curr_slice->m_is_cur_mb_field], &iLastCoeff, 16);
        if (iNumCoeffs == 0) {
            uCBPLuma &= ~CBP4x4Mask[block];
            Copy4x4(pBlockData, pitchPixels, pPredBuf, 16);
        }
    }
    cur_mb.m_iNumCoeffs4x4[ block ] = iNumCoeffs;
    cur_mb.m_iLastCoeff4x4[ block ] = iLastCoeff;
    cur_mb.m_uIntraCBP4x4 = uCBPLuma;
}

void H264ENC_MAKE_NAME(H264CoreEncoder_Encode8x8IntraBlock)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s block)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32s     iNumCoeffs;
    Ipp32s     iLastCoeff;
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32u uMBQP       = cur_mb.lumaQP;
    Ipp32s pitchPixels = cur_mb.mbPitchPixels;

    PIXTYPE* pBlockData = cur_mb.mbPtr + xoff[4*block] + yoff[4*block]*pitchPixels;
    // loop over all 8x8 blocks in Y plane for the MB
    PIXTYPE* pPredBuf = cur_mb.mb8x8.prediction + xoff[block<<2] + yoff[block<<2]*16;
    PIXTYPE* pReconBuf = cur_mb.mb8x8.reconstruct + xoff[block<<2] + yoff[block<<2]*16;

    Ipp32u uCBPLuma     = cur_mb.m_uIntraCBP8x8;
    COEFFSTYPE* pTransformResult = &cur_mb.mb8x8.transform[block*64];
    __ALIGN16 Ipp16s pDiffBuf[64];
    __ALIGN16 COEFFSTYPE pTransRes[64];

    Diff8x8(pPredBuf, pBlockData, pitchPixels, pDiffBuf);
    if (!core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag || uMBQP != 0) {
        H264ENC_MAKE_NAME(ippiTransformLuma8x8Fwd_H264)(pDiffBuf, pTransformResult);
        H264ENC_MAKE_NAME(ippiQuantLuma8x8_H264)(
            pTransformResult,
            pTransformResult,
            QP_DIV_6[uMBQP],
            1,
            enc_single_scan_8x8[curr_slice->m_is_cur_mb_field],
            core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[0][QP_MOD_6[uMBQP]],
            &iNumCoeffs,
            &iLastCoeff,
            NULL,
            NULL,
            NULL);

        if (!iNumCoeffs) {
            Copy8x8(pPredBuf, 16, pReconBuf, 16);
            uCBPLuma &= ~CBP8x8Mask[block];
        } else {
            MFX_INTERNAL_CPY( pTransRes, pTransformResult, 64*sizeof( COEFFSTYPE ));
            H264ENC_MAKE_NAME(ippiQuantLuma8x8Inv_H264)(pTransRes, QP_DIV_6[uMBQP], core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[0][QP_MOD_6[uMBQP]]);
            H264ENC_MAKE_NAME(ippiTransformLuma8x8InvAddPred_H264)(pPredBuf, 16, pTransRes, pReconBuf, 16, core_enc->m_PicParamSet->bit_depth_luma);
        }
    } else {
        // Transform bypass => lossless.
        Copy8x8(pBlockData, pitchPixels, pReconBuf, 16);
        for (Ipp32s i = 0; i < 64; i++)
            pTransformResult[i] = pDiffBuf[i];
        ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan_8x8[curr_slice->m_is_cur_mb_field], &iLastCoeff, 64);
        if (iNumCoeffs == 0) {
            uCBPLuma &= ~CBP8x8Mask[block];
            Copy8x8(pBlockData, pitchPixels, pPredBuf, 16);
        }
    }
    cur_mb.m_iNumCoeffs8x8[ block ] = iNumCoeffs;
    cur_mb.m_iLastCoeff8x8[ block ] = iLastCoeff;
    cur_mb.m_uIntraCBP8x8 = uCBPLuma;
}

void H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantIntra16x16_RD)(
    void* state,
    H264SliceType *curr_slice)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;     // block number, 0 to 23
    Ipp32u  uMBQP;          // QP of current MB
    //Ipp32u  uMB;
    Ipp32u  uCBPLuma;        // coded flags for all 4x4 blocks
    COEFFSTYPE* pDCBuf;     // chroma & luma dc coeffs pointer
    PIXTYPE*  pPredBuf;       // prediction block pointer
    PIXTYPE*  pReconBuf;       // prediction block pointer
    Ipp16s* pDiffBuf;       // difference block pointer
    Ipp16s* pTempDiffBuf;       // difference block pointer
    COEFFSTYPE *pTransformResult; // for transform results.
    Ipp16s* pMassDiffBuf;   // difference block pointer
    COEFFSTYPE* pQBuf;          // quantized block pointer
    Ipp32s  pitchPixels;     // buffer pitch in pixels
    Ipp8u   bCoded; // coded block flag
    Ipp32s  iNumCoeffs; // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s  iLastCoeff; // Number of nonzero coeffs after quant (negative if DC is nonzero)
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;

    pitchPixels = cur_mb.mbPitchPixels;
    uCBPLuma    = cur_mb.LocalMacroblockInfo->cbp_luma;
    uMBQP       = cur_mb.lumaQP;
    pDiffBuf    = (Ipp16s*) (curr_slice->m_pMBEncodeBuffer + 512);
    pTransformResult = (COEFFSTYPE*)(pDiffBuf + 16);
    pQBuf       = (COEFFSTYPE*) (pTransformResult + 16);
    pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    pMassDiffBuf = (Ipp16s*) (pDCBuf + 16);
    //uMB = cur_mb.uMB;

    //--------------------------------------------------------------------------
    // encode Y plane blocks (0-15)
    //--------------------------------------------------------------------------

    // initialize pointers and offset
    pPredBuf    = cur_mb.mb16x16.prediction; // 16-byte aligned work buffer
    pReconBuf    = cur_mb.mb16x16.reconstruct; // 16-byte aligned work buffer
    Ipp32s pitchPix = 16;

    cur_mb.MacroblockCoeffsInfo->lumaAC = 0;
    H264ENC_MAKE_NAME(ippiSumsDiff16x16Blocks4x4)(cur_mb.mbPtr, pitchPixels, pPredBuf, 16, pDCBuf, pMassDiffBuf); // compute the 4x4 luma DC transform coeffs

    // apply second transform on the luma DC transform coeffs
    H264ENC_MAKE_NAME(ippiTransformQuantLumaDC_H264)(
        pDCBuf,
        pQBuf,
        uMBQP,
        &iNumCoeffs,
        1,
        enc_single_scan[is_cur_mb_field],&iLastCoeff,
        NULL);

    if (core_enc->m_PicParamSet->entropy_coding_mode)
    {
        CabacBlock4x4* c_data = &cur_mb.cabac_data->dcBlock[Y_DC_RLE - U_DC_RLE];
        bCoded = c_data->uNumSigCoeffs = ABS(iNumCoeffs);
        c_data->uLastSignificant = iLastCoeff;
        c_data->CtxBlockCat = BLOCK_LUMA_DC_LEVELS;
        c_data->uFirstCoeff = 0;
        c_data->uLastCoeff = 15;
        H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
            pDCBuf,
            dec_single_scan[is_cur_mb_field],
            c_data);
    }
    else
    {
        H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(pDCBuf,0, dec_single_scan[is_cur_mb_field],iLastCoeff,
                                   &curr_slice->Block_RLE[Y_DC_RLE].uTrailing_Ones,
                                   &curr_slice->Block_RLE[Y_DC_RLE].uTrailing_One_Signs,
                                   &curr_slice->Block_RLE[Y_DC_RLE].uNumCoeffs,
                                   &curr_slice->Block_RLE[Y_DC_RLE].uTotalZeros,
                                   curr_slice->Block_RLE[Y_DC_RLE].iLevels,
                                   curr_slice->Block_RLE[Y_DC_RLE].uRuns);
        bCoded = curr_slice->Block_RLE[Y_DC_RLE].uNumCoeffs;
    }

    H264ENC_MAKE_NAME(ippiTransformDequantLumaDC_H264)(
        pDCBuf,
        uMBQP,
        NULL);

    // loop over all 4x4 blocks in Y plane for the MB
    for (uBlock = 0; uBlock < 16; uBlock++)
    {
        pPredBuf = cur_mb.mb16x16.prediction + xoff[uBlock] + yoff[uBlock]*16;
        pReconBuf = cur_mb.mb16x16.reconstruct + xoff[uBlock] + yoff[uBlock]*16;

        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;        // This will be updated if the block is coded
        if (core_enc->m_PicParamSet->entropy_coding_mode)
        {
            cur_mb.cabac_data->lumaBlock4x4[uBlock].uNumSigCoeffs = 0;
        }
        else
        {
            curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
            curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
            curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
            curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
        }

        bCoded = ((uCBPLuma & CBP4x4Mask[uBlock])?(1):(0)); // check if block is coded

        if (!bCoded){
            Copy4x4(pPredBuf, 16, pReconBuf, pitchPix); // update reconstruct frame for the empty block
        }else{   // block not declared empty, encode
            pTempDiffBuf = pMassDiffBuf+ xoff[uBlock]*4 + yoff[uBlock]*16;
            H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                pTempDiffBuf,
                pTransformResult,
                uMBQP,
                &iNumCoeffs,
                1,
                enc_single_scan[is_cur_mb_field],
                &iLastCoeff,
                NULL,
                NULL,
                0,
                NULL); //Always use f for INTRA

            cur_mb.MacroblockCoeffsInfo->lumaAC |= ((iNumCoeffs < -1) || (iNumCoeffs > 0));

            if (!iNumCoeffs){
                bCoded = 0;
            } else {
                if (core_enc->m_PicParamSet->entropy_coding_mode){
                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)((iNumCoeffs < 0) ? -(iNumCoeffs+1) : iNumCoeffs);

                    CabacBlock4x4* c_data = &cur_mb.cabac_data->lumaBlock4x4[uBlock];
                    c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                    c_data->uLastSignificant = iLastCoeff;
                    c_data->CtxBlockCat = BLOCK_LUMA_AC_LEVELS;
                    c_data->uFirstCoeff = 1;
                    c_data->uLastCoeff = 15;
                    H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                        pTransformResult,
                        dec_single_scan[is_cur_mb_field],
                        c_data);

                    bCoded = c_data->uNumSigCoeffs;
                } else {
                    H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(pTransformResult, 1, dec_single_scan[is_cur_mb_field], iLastCoeff,
                                               &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
                                               &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
                                               &curr_slice->Block_RLE[uBlock].uNumCoeffs,
                                               &curr_slice->Block_RLE[uBlock].uTotalZeros,
                                               curr_slice->Block_RLE[uBlock].iLevels,
                                               curr_slice->Block_RLE[uBlock].uRuns);
                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = bCoded = curr_slice->Block_RLE[uBlock].uNumCoeffs;
                }
            }

            if (!bCoded) uCBPLuma &= ~CBP4x4Mask[uBlock];

            // If the block wasn't coded and the DC coefficient is zero
            if (!bCoded && !pDCBuf[block_subblock_mapping[uBlock]]){
                Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
            } else {
                H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264)(
                    pPredBuf,
                    pTransformResult,
                    &pDCBuf[block_subblock_mapping[uBlock]],
                    pReconBuf,
                    16,
                    pitchPix,
                    uMBQP,
                    ((iNumCoeffs < -1) || (iNumCoeffs > 0)),
                    core_enc->m_PicParamSet->bit_depth_luma,
                    NULL);
            }
        }
    }

    cur_mb.LocalMacroblockInfo->cbp_luma = uCBPLuma;
    if (cur_mb.MacroblockCoeffsInfo->lumaAC > 1)
        cur_mb.MacroblockCoeffsInfo->lumaAC = 1;

}

#if 1
static IppStatus ippiSumsDiff8x8Blocks4x4_NV12 (const Ipp8u *pSrc,
                                                      Ipp32s SrcPitch,
                                                      const Ipp8u *pPred,
                                                      Ipp32s PredPitch,
                                                      Ipp16s *pPredResiduals,
                                                      Ipp32s PredResidualsPitch,
                                                      Ipp16s *pDC,
                                                      Ipp16s *pDiff)
{
    Ipp32u i, j, m, n;
    /* compute 2x2 DC coeffs */
    if (pDiff)
    {
        if (pPredResiduals) {
            for (i = 0; i < 2; i ++)
            {
                for (j = 0; j < 2; j ++)
                {
                    pDC[j] = 0;

                    for (m = 0; m < 4; m ++)
                    {
                        for (n = 0; n < 4; n ++)
                        {
                            Ipp32s temp = pSrc[m * SrcPitch + 2*n] - pPred[m * PredPitch + n];
                            temp -= pPredResiduals[m * PredResidualsPitch + n];
                            pDC[j] = (Ipp16s) (pDC[j] + temp);
                            pDiff[i*32+j*16+m*4+n] = (Ipp16s) temp;
                        }
                    }
                    pPred += 4;
                    pSrc += 8;
                    pPredResiduals += 4;
                }
                pSrc += 4 * SrcPitch - 16;
                pPred += 4 * PredPitch - 8;
                pPredResiduals += 4 * PredResidualsPitch - 8;
                pDC += 2;
            }
        } else {
            for (i = 0; i < 2; i ++)
            {
                for (j = 0; j < 2; j ++)
                {
                    pDC[j] = 0;

                    for (m = 0; m < 4; m ++)
                    {
                        for (n = 0; n < 4; n ++)
                        {
                            Ipp32s temp = pSrc[m * SrcPitch + 2*n] - pPred[m * PredPitch + n];
                            pDC[j] = (Ipp16s) (pDC[j] + temp);
                            pDiff[i*32+j*16+m*4+n] = (Ipp16s) temp;
                        }
                    }
                    pPred += 4;
                    pSrc += 8;
                }
                pSrc += 4 * SrcPitch - 16;
                pPred += 4 * PredPitch - 8;
                pDC += 2;
            }
        }
    }
    else
    {
        for (i = 0; i < 2; i ++)
        {
            for (j = 0; j < 2; j ++)
            {
                pDC[j] = 0;

                for (m = 0; m < 4; m ++)
                {
                    for (n = 0; n < 4; n ++)
                    {
                        Ipp32s temp = pSrc[m * SrcPitch + 2*n] - pPred[m * PredPitch + n];
                        pDC[j] = (Ipp16s) (pDC[j] + temp);
                    }
                }
                pPred += 4;
                pSrc += 8;
            }
            pSrc += 4 * SrcPitch - 16;
            pPred += 4 * PredPitch - 8;
            pDC += 2;
        }

    }

    return (IppStatus)0;

} /* IPPFUN(IppStatus, ippiSumsDiff8x8Blocks4x4_8u16s_C1, (Ipp8u *pSrc, */
#endif

#if 0
static IppStatus ippiSumsDiff8x8Blocks4x4_NV12n(const Ipp8u *pSrc,
                                     Ipp32s SrcPitch,
                               const Ipp8u *pPredU,
                                     Ipp32s PredPitchU,
                               const Ipp8u *pPredV,
                                     Ipp32s PredPitchV,
                                     Ipp16s *pDCU,
                                     Ipp16s *pDiffU,
                                     Ipp16s *pDCV,
                                     Ipp16s *pDiffV)
{
    Ipp32u i, j, m, n,temp1,temp2;
    if ((pDiffU) && (pDiffV))
    {
        for (i = 0; i < 2; i ++)
        {
            for (j = 0; j < 2; j ++)
            {
                pDCU[j] = 0;
                pDCV[j] = 0;

                for (m = 0; m < 4; m ++)
                {
                    for (n = 0; n < 4; n ++)
                    {
                        temp1 = pSrc[m * SrcPitch + 2*n    ] - pPredU[m * PredPitchU + n];
                        temp2 = pSrc[m * SrcPitch + 2*n + 1] - pPredV[m * PredPitchV + n];
                        pDCU[j] = (Ipp16s) (pDCU[j] + temp1);
                        pDCV[j] = (Ipp16s) (pDCV[j] + temp2);

                        pDiffU[i*32+j*16+m*4+n] = (Ipp16s) temp1;
                        pDiffV[i*32+j*16+m*4+n] = (Ipp16s) temp2;
                    }
                }

                pPredU += 4;
                pPredV += 4;
                pSrc += 8;
            }

            pSrc += 4 * SrcPitch - 16;
            pPredU += 4 * PredPitchU - 8;
            pPredV += 4 * PredPitchV - 8;
            pDCU += 2;
            pDCV += 2;
        }
    }
    else
    {
        for (i = 0; i < 2; i ++)
        {
            for (j = 0; j < 2; j ++)
            {
                pDCU[j] = 0;
                pDCV[j] = 0;

                for (m = 0; m < 4; m ++)
                {
                    for (n = 0; n < 4; n ++)
                    {
                        temp1 = pSrc[m * SrcPitch + 2*n    ] - pPredU[m * PredPitchU + n];
                        temp2 = pSrc[m * SrcPitch + 2*n + 1] - pPredV[m * PredPitchV + n];
                        pDCU[j] = (Ipp16s) (pDCU[j] + temp1);
                        pDCV[j] = (Ipp16s) (pDCV[j] + temp2);
                    }
                }
                pPredU += 4;
                pPredV += 4;
                pSrc += 8;
            }
            pSrc += 4 * SrcPitch - 16;
            pPredU += 4 * PredPitchU - 8;
            pPredV += 4 * PredPitchV - 8;
            pDCU += 2;
            pDCV += 2;
        }
    }
        return (IppStatus)0;
}
#endif

void H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma_SVC)(
    void* state,
    H264SliceType *curr_slice)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;         // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    PIXTYPE*  pSrcPlane;    // start of plane to encode
    Ipp32s    pitchPixels;  // buffer pitch
    COEFFSTYPE *pDCBuf;     // chroma & luma dc coeffs pointer
    PIXTYPE*  pPredBuf;     // prediction block pointer
    PIXTYPE*  pReconBuf;     // prediction block pointer
    PIXTYPE*  pRefLayerPixels;
    PIXTYPE*  pPredBuf_copy;     // prediction block pointer
    PIXTYPE*  pReconBuf_copy;     // prediction block pointer
    COEFFSTYPE* pQBuf;      // quantized block pointer
    Ipp16s* pMassDiffBuf;   // difference block pointer
    Ipp32u   uCBPChroma;    // coded flags for all chroma blocks
    Ipp32s   bCoded;        // coded block flag
    Ipp32s   iNumCoeffs = 0;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s   iLastCoeff;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s   RLE_Offset;    // Index into BlockRLE array

    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    EnumSliceType slice_type = curr_slice->m_slice_type;
    COEFFSTYPE *pTransformResult;
    COEFFSTYPE *pTransform;
    COEFFSTYPE *pPredSCoeffs, *pPredTCoeffs;
    COEFFSTYPE *pResidualBuf;
    COEFFSTYPE *pResidualBuf0;
    COEFFSTYPE *pRefPredResiduals;
    COEFFSTYPE *pRefPredResiduals0;
    Ipp32s QPy = cur_mb.lumaQP;
    Ipp32s pitchPix;

    pitchPix = pitchPixels = cur_mb.mbPitchPixels;
    uMBQP       = cur_mb.chromaQP;
    pTransform = (COEFFSTYPE*)curr_slice->m_pMBEncodeBuffer;
    pQBuf       = (COEFFSTYPE*) (pTransform + 64*2);
    pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    pMassDiffBuf= (Ipp16s*) (pDCBuf+ 16);
    Ipp16s*  pTempDiffBuf;
    Ipp32u  uMB = cur_mb.uMB;
    Ipp32u  mbOffset;
    // initialize pointers and offset
    mbOffset = uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    bool transform_bypass = core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag && QPy == 0;
    uCBPChroma  = cur_mb.LocalMacroblockInfo->cbp_chroma;
    bool intra = IS_TRUEINTRA_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype);
    Ipp32s doIntraPred = !pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo) && 
        (curr_slice->m_scan_idx_start != 0 || curr_slice->m_scan_idx_end != 15);

    pPredSCoeffs = &(core_enc->m_SavedMacroblockCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);
    pPredTCoeffs = &(core_enc->m_SavedMacroblockTCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);

    if (intra) {
        pPredBuf = cur_mb.mbChromaIntra.prediction;

        if (pGetMBIBLFlag(cur_mb.GlobalMacroblockInfo))
        {
#ifdef USE_NV12
            Ipp32s i,j;
            pRefLayerPixels = core_enc->m_pUInputRefPlane+uOffset;
            PIXTYPE *r = pPredBuf;
            for(i=0;i<8;i++){
                for(j=0;j<8;j++){
                    r[j] = pRefLayerPixels[2*j];
                    r[j+8] = pRefLayerPixels[2*j+1];
                }
                r += 16;
                pRefLayerPixels += pitchPixels;
            }
#endif
        }

        pReconBuf = cur_mb.mbChromaIntra.reconstruct;
        if((!((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE)) &&
            (!pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))) || curr_slice->doTCoeffsPrediction || doIntraPred){
            cur_mb.MacroblockCoeffsInfo->chromaNC = 0;
#ifdef USE_NV12
              H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectChromaMBs_8x8_NV12)(
                state,
                curr_slice,
                core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                pitchPixels,
                &cur_mb.LocalMacroblockInfo->intra_chroma_mode,
                pPredBuf,
                pPredBuf+8,
                0,
                curr_slice->doTCoeffsPrediction);  //up to 422 only
#else
            H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectChromaMBs_8x8)(
                state,
                curr_slice,
                core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                core_enc->m_pCurrentFrame->m_pVPlane + uOffset,
                core_enc->m_pReconstructFrame->m_pVPlane + uOffset,
                pitchPixels,
                &cur_mb.LocalMacroblockInfo->intra_chroma_mode,
                pPredBuf,
                pPredBuf+8,
                curr_slice->doTCoeffsPrediction);  //up to 422 only
#endif
        }
    } else {
        cur_mb.MacroblockCoeffsInfo->chromaNC = 0;
        pPredBuf = cur_mb.mbChromaInter.prediction;
        pReconBuf = cur_mb.mbChromaInter.reconstruct;

        H264MotionVector mv_f[16], *pmv_f;
        H264MotionVector mv_b[16], *pmv_b;
        if (core_enc->m_svc_layer.isActive) {
            pmv_f = mv_f;
            pmv_b = mv_b;
            truncateMVs(state, curr_slice, cur_mb.MVs[LIST_0]->MotionVectors, mv_f);
            if (curr_slice->m_slice_type == BPREDSLICE)
                truncateMVs(state, curr_slice, cur_mb.MVs[LIST_1]->MotionVectors, mv_b);
        } else {
            pmv_f = cur_mb.MVs[LIST_0]->MotionVectors;
            pmv_b = cur_mb.MVs[LIST_1]->MotionVectors;
        }

        if (curr_slice->doMCDirectForSkipped)
            cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_DIRECT; //for correct MC

        H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBChroma)(state, curr_slice, pmv_f, pmv_b, pPredBuf);

        if (curr_slice->doMCDirectForSkipped)
            cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;

        if (curr_slice->doInterIntraPrediction)
        {
                //RestoreIntraPrediction - copy intra predicted parts
                Ipp32s i,j;
                for (i=0; i<2; i++) {
                    for (j=0; j<2; j++) {
                        if (!cur_mb.GlobalMacroblockInfo->i_pred[2*j+i]) continue;
                        Ipp32s offx = (core_enc->svc_ResizeMap[0][cur_mb.uMBx].border+1)>>1;
                        Ipp32s offy = (core_enc->svc_ResizeMap[1][cur_mb.uMBy].border+1)>>1;
                        IppiSize roiSize = {i?8-offx:offx, j?8-offy:offy};
                        if(!i) offx = 0;
                        if(!j) offy = 0;
                        // NV12 !!
                        Ipp32s x, y;
                        PIXTYPE* ptr = core_enc->m_pReconstructFrame->m_pUPlane+uOffset;
                        for (y=0; y<roiSize.height; y++) {
                            for (x=0; x<roiSize.width; x++) {
                                pPredBuf[(offy+y)*16 + offx+x] =
                                    ptr[(offy+y)*pitchPixels + (offx+x) * 2];
                                pPredBuf[(offy+y)*16 + offx+x + 8] =
                                    ptr[(offy+y)*pitchPixels + (offx+x) * 2 + 1];
                            }
                        }
                    }
                }
        }
    }

    // initialize pointers for the U plane blocks
    Ipp32s num_blocks = 2 << core_enc->m_PicParamSet->chroma_format_idc;
    Ipp32s startBlock;
    startBlock = uBlock = 16;
    Ipp32u uLastBlock = uBlock+num_blocks;
    Ipp32u uFinalBlock = uBlock+2*num_blocks;
    PIXTYPE* pPredBufV = pPredBuf+8;
#if 0
    uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    ippiSumsDiff8x8Blocks4x4_NV12n(core_enc->m_pCurrentFrame->m_pUPlane + uOffset, pitchPixels, pPredBuf, 16,
        pPredBuf+8,16,pDCBuf,pMassDiffBuf, pDCBuf+8, pMassDiffBuf+128);
#endif
    do
    {
        if (uBlock == uLastBlock) {
            startBlock = uBlock;
            uOffset = mbOffset;
            pSrcPlane = core_enc->m_pCurrentFrame->m_pVPlane;
            pPredBuf = pPredBufV;
#ifdef USE_NV12
            pReconBuf = cur_mb.mbChromaInter.reconstruct+8;
            pitchPix = 16;
#else
            pReconBuf = core_enc->m_pReconstructFrame->m_pVPlane+uOffset;
#endif
            RLE_Offset = V_DC_RLE;
            uLastBlock += num_blocks;
#if 0
            pDCBuf += 8;
            pMassDiffBuf += 128;
#endif
            pResidualBuf = core_enc->m_pVResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
            pRefPredResiduals = core_enc->m_pVInputResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
            pResidualBuf0 = pResidualBuf;
            pRefPredResiduals0 = pRefPredResiduals;
            if(pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))
                pRefPredResiduals = core_enc->m_pVInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
        } else {
            RLE_Offset = U_DC_RLE;
            pSrcPlane = core_enc->m_pCurrentFrame->m_pUPlane;
#ifdef USE_NV12
            pReconBuf = cur_mb.mbChromaInter.reconstruct;
            pitchPix = 16;
#else
            pReconBuf = core_enc->m_pReconstructFrame->m_pUPlane+uOffset;
#endif
            pResidualBuf = core_enc->m_pUResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
            pRefPredResiduals = core_enc->m_pUInputResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
            pResidualBuf0 = pResidualBuf;
            pRefPredResiduals0 = pRefPredResiduals;
            if(pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))
                pRefPredResiduals = core_enc->m_pUInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
        }

        //TODO add for 422
#ifdef USE_NV12
        ippiSumsDiff8x8Blocks4x4_NV12(pSrcPlane + uOffset, pitchPixels, pPredBuf, 16,
            pGetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo)?pRefPredResiduals:0, core_enc->m_WidthInMBs * 8, pDCBuf, pMassDiffBuf);
#else
        H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(pSrcPlane + uOffset, pitchPixels, pPredBuf, 16, pDCBuf, pMassDiffBuf);
        VM_ASSERT(0); // fixme: subtract residuals
#endif

        if (core_enc->m_PicParamSet->chroma_format_idc == 2)
             H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(pSrcPlane + uOffset+8*pitchPixels, pitchPixels, pPredBuf+8*16, 16, pDCBuf+4, pMassDiffBuf+64);

        if (curr_slice->doSCoeffsPrediction)
        {
            Ipp32s i;
            for (i = 0; i < 4; i ++)
            {
                pDCBuf[i] -= ((pPredSCoeffs[(startBlock+i)*16] * normTable4x4[0]) >> 8);
            }
        }
        // Code chromaDC
        if (!transform_bypass)  {
            switch (core_enc->m_PicParamSet->chroma_format_idc) {
                case 1:
                    H264ENC_MAKE_NAME(ippiTransformQuantChromaDC_H264)(pDCBuf, pQBuf, uMBQP, &iNumCoeffs, (slice_type == INTRASLICE), 1, NULL);
                    break;
                case 2:
                    H264ENC_MAKE_NAME(ippiTransformQuantChroma422DC_H264)(pDCBuf, pQBuf, uMBQP, &iNumCoeffs, (slice_type == INTRASLICE), 1, NULL);
                    break;
                default:
                    break;
            }
            if (curr_slice->doTCoeffsPrediction)
            {
                Ipp32s i;
                for (i = 0; i < 4; i ++)
                {
                    pDCBuf[i] -= pPredTCoeffs[(startBlock+i)*16];
                }
                ippiCalcNonZero(pDCBuf, 4, &iNumCoeffs);
            }
        } else {
            Ipp32s i,j;
            Ipp32s num_rows, num_cols;
            Ipp32s bPitch;
            num_cols = ((core_enc->m_PicParamSet->chroma_format_idc - 1) & 0x2) ? 4 : 2;
            num_rows = (core_enc->m_PicParamSet->chroma_format_idc & 0x2) ? 4 : 2;
            bPitch = num_cols * 16;
            for(i = 0; i < num_rows; i++) {
                for(j = 0; j < num_cols; j++) {
                    pDCBuf[i*num_cols+j] = pMassDiffBuf[i*bPitch + j*16];
                }
            }
            ippiCalcNonZero(pDCBuf, num_blocks, &iNumCoeffs);
        }

        if (cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_SKIPPED || curr_slice->m_scan_idx_start != 0)
        {
            memset(pDCBuf, 0, 4*sizeof(COEFFSTYPE));
            iNumCoeffs = 0;
        }

        // DC values in this block if iNonEmpty is 1.
        cur_mb.MacroblockCoeffsInfo->chromaNC |= (iNumCoeffs != 0);
        // record RLE info
        if (curr_slice->m_scan_idx_start == 0) {
            if (core_enc->m_PicParamSet->entropy_coding_mode){
                Ipp32s ctxIdxBlockCat = BLOCK_CHROMA_DC_LEVELS;
                switch (core_enc->m_PicParamSet->chroma_format_idc) {
                    case 1:
                        H264ENC_MAKE_NAME(H264CoreEncoder_ScanSignificant_CABAC)(
                            pDCBuf,
                            ctxIdxBlockCat,
                            4,
                            dec_single_scan_p,
                            &cur_mb.cabac_data->dcBlock[RLE_Offset - U_DC_RLE]);
                        break;
                    case 2:
                        H264ENC_MAKE_NAME(H264CoreEncoder_ScanSignificant_CABAC)(
                            pDCBuf,
                            ctxIdxBlockCat,
                            8,
                            dec_single_scan_p422,
                            &cur_mb.cabac_data->dcBlock[RLE_Offset - U_DC_RLE]);
                        break;
                    default:
                        break;
                }
           }else{
                switch( core_enc->m_PicParamSet->chroma_format_idc ){
                    case 1:
                       H264ENC_MAKE_NAME(ippiEncodeChromaDcCoeffsCAVLC_H264)(
                           pDCBuf,
                           &curr_slice->Block_RLE[RLE_Offset].uTrailing_Ones,
                           &curr_slice->Block_RLE[RLE_Offset].uTrailing_One_Signs,
                           &curr_slice->Block_RLE[RLE_Offset].uNumCoeffs,
                           &curr_slice->Block_RLE[RLE_Offset].uTotalZeros,
                           curr_slice->Block_RLE[RLE_Offset].iLevels,
                           curr_slice->Block_RLE[RLE_Offset].uRuns);
                        break;
                    case 2:
                        H264ENC_MAKE_NAME(ippiEncodeChroma422DC_CoeffsCAVLC_H264)(
                            pDCBuf,
                            &curr_slice->Block_RLE[RLE_Offset].uTrailing_Ones,
                            &curr_slice->Block_RLE[RLE_Offset].uTrailing_One_Signs,
                            &curr_slice->Block_RLE[RLE_Offset].uNumCoeffs,
                            &curr_slice->Block_RLE[RLE_Offset].uTotalZeros,
                            curr_slice->Block_RLE[RLE_Offset].iLevels,
                            curr_slice->Block_RLE[RLE_Offset].uRuns);
                        break;
                }
            }
        }

        if (curr_slice->doTCoeffsPrediction)
        {
            Ipp32s i;
            for (i = 0; i < 4; i ++)
            {
                pDCBuf[i] += pPredTCoeffs[(startBlock+i)*16];
                pPredTCoeffs[(startBlock+i)*16] = pDCBuf[i];
            }
        } else {
            Ipp32s i;
            for (i = 0; i < 4; i ++)
            {
                pPredTCoeffs[(startBlock+i)*16] = pDCBuf[i];
            }
        }

        // Inverse transform and dequantize for chroma DC
        if (!transform_bypass ){
            switch( core_enc->m_PicParamSet->chroma_format_idc ){
             case 1:
                 H264ENC_MAKE_NAME(ippiTransformDequantChromaDC_H264)(pDCBuf, uMBQP, NULL);
                 break;
             case 2:
                 H264ENC_MAKE_NAME(ippiTransformDequantChromaDC422_H264)(pDCBuf, uMBQP, NULL);
                 break;
            default:
                break;
            }
        }
//Encode croma AC
        Ipp32s coeffsCost = 0;
        pPredBuf_copy = pPredBuf;
        pReconBuf_copy = pReconBuf;
        for (uBlock = startBlock; uBlock < uLastBlock; uBlock ++) {
            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;     // This will be updated if the block is coded
            if (core_enc->m_PicParamSet->entropy_coding_mode)
            {
                cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
            }
            else
            {
                curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
            }
            // check if block is coded
            bCoded = ((uCBPChroma & CBP4x4Mask[uBlock-16])?(1):(0));
            if (!bCoded){ // update reconstruct frame for the empty block
                Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
            } else {   // block not declared empty, encode
                pTempDiffBuf = pMassDiffBuf + (uBlock-startBlock)*16;
                pTransformResult = pTransform + (uBlock-startBlock)*16;
                if(!transform_bypass) {
                    if (!curr_slice->doSCoeffsPrediction)
                    {
                        H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                            pTempDiffBuf,
                            pTransformResult,
                            uMBQP,
                            &iNumCoeffs,
                            0,
                            enc_single_scan[is_cur_mb_field],
                            &iLastCoeff,
                            NULL,
                            NULL,
                            9,
                            NULL);//,NULL, curr_slice, 1, &cbSt);
                    }
                    else
                    {
                        __ALIGN16 COEFFSTYPE tmpCoeff[16];
                        Ipp32s i;
                        for (i = 0; i < 16; i++)
                        {
                            tmpCoeff[i] = ((pPredSCoeffs[uBlock*16+i] * normTable4x4[i]) >> 8);
                        }

                        H264ENC_MAKE_NAME(ippiTransformSubQuantResidual_H264)(
                            pTempDiffBuf,
                            tmpCoeff,
                            pTransformResult,
                            uMBQP,
                            &iNumCoeffs,
                            0,
                            enc_single_scan[is_cur_mb_field],
                            &iLastCoeff,
                            NULL,
                            NULL,
                            9,
                            NULL);//,NULL, curr_slice, 1, &cbSt);
                    }

                    if (curr_slice->doTCoeffsPrediction)
                    {
                        ippsSub_16s_I(pPredTCoeffs + uBlock*16, pTransformResult, 16);
                        ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan[is_cur_mb_field], &iLastCoeff, 16);
                    }
                    if (core_enc->m_svc_layer.isActive &&
                        (curr_slice->m_scan_idx_start != 0 || curr_slice->m_scan_idx_end != 15)) {
                            ScanIdxZero(pTransformResult, curr_slice->m_scan_idx_start, curr_slice->m_scan_idx_end+1, dec_single_scan[is_cur_mb_field], 16);
                            ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan[is_cur_mb_field], &iLastCoeff, 16);
                    }

                    coeffsCost += iNumCoeffs
                        ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 15, &dec_single_scan[is_cur_mb_field][1])
                        : 0;
//                    if (core_enc->m_svc_layer.isActive)
//                        ippsCopy_16s(pTransformResult, cur_mb.m_pTCoeffsTmp + uBlock * 16, 16);
                 }else {
                    for(Ipp32s i = 0; i < 16; i++) {
                        pTransformResult[i] = pTempDiffBuf[i];
                    }
                    ippiCountCoeffs(pTempDiffBuf, &iNumCoeffs, enc_single_scan[is_cur_mb_field], &iLastCoeff, 16);
                }
                // if everything quantized to zero, skip RLE
                cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)((iNumCoeffs < 0) ? -(iNumCoeffs+1) : iNumCoeffs);
                if (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock])
                {
                    // the block is empty so it is not coded
                    if (core_enc->m_PicParamSet->entropy_coding_mode)
                    {
                        CabacBlock4x4* c_data = &cur_mb.cabac_data->chromaBlock[uBlock - 16];
                        c_data->uLastSignificant = iLastCoeff;
                        c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                        c_data->CtxBlockCat = BLOCK_CHROMA_AC_LEVELS;
                        c_data->uFirstCoeff = 1;
                        c_data->uLastCoeff = 15;
                        H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                            pTransformResult,
                            dec_single_scan[is_cur_mb_field],
                            c_data);
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = c_data->uNumSigCoeffs;
                    }
                    else
                    {
                        H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(
                            pTransformResult,// pDiffBuf,
                            curr_slice->m_scan_idx_start > 1 ? curr_slice->m_scan_idx_start : 1,
                            dec_single_scan[is_cur_mb_field],
                            iLastCoeff,
                            &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
                            &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
                            &curr_slice->Block_RLE[uBlock].uNumCoeffs,
                            &curr_slice->Block_RLE[uBlock].uTotalZeros,
                            curr_slice->Block_RLE[uBlock].iLevels,
                            curr_slice->Block_RLE[uBlock].uRuns);

                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = curr_slice->Block_RLE[uBlock].uNumCoeffs;
                    }
                }
            }
            pPredBuf += chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock]; //!!!
#ifdef USE_NV12
            pReconBuf += chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock];
#else
            pReconBuf += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
#endif
        }

        if(!transform_bypass && coeffsCost <= (CHROMA_COEFF_MAX_COST<<(core_enc->m_PicParamSet->chroma_format_idc-1)) )
        {
            //Reset all ac coeffs
            for(uBlock = startBlock; uBlock < uLastBlock; uBlock++){
                cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
                ippsZero_16s(pTransform + (uBlock-startBlock)*16 + 1, 15);
                if (core_enc->m_PicParamSet->entropy_coding_mode)
                {
                   cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
                }
                else
                {
                    curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                    curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                    curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                    curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
                }
            }
        }

        pPredBuf = pPredBuf_copy;
        pReconBuf = pReconBuf_copy;
        for (uBlock = startBlock; uBlock < uLastBlock; uBlock ++) {
            __ALIGN16 Ipp16s tmpCoeffsBuff[16];
            cur_mb.MacroblockCoeffsInfo->chromaNC |= 2*(cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock]!=0);
            if (core_enc->m_svc_layer.isActive) {
                if (!cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] && !pDCBuf[ chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock] ]){
                    uCBPChroma &= ~CBP4x4Mask[uBlock-16];
                    memset(pTransform + (uBlock-startBlock)*16, 0, 16*sizeof(COEFFSTYPE));
                }
                if (curr_slice->doTCoeffsPrediction)
                {
                    ippsAdd_16s_I(pPredTCoeffs + uBlock*16, pTransform + (uBlock-startBlock)*16, 16);
                    //                CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);
                }
                ippsCopy_16s(pTransform + (uBlock-startBlock)*16 + 1, pPredTCoeffs + uBlock*16 + 1, 15);

                ippiCountCoeffs(pTransform + (uBlock-startBlock)*16, &iNumCoeffs, enc_single_scan[is_cur_mb_field], &iLastCoeff, 16);
                iNumCoeffs = (T_NumCoeffs)((iNumCoeffs < 0) ? -(iNumCoeffs+1) : iNumCoeffs);

                if (!transform_bypass)
                {
                    if (iNumCoeffs != 0) {
                        ippsCopy_16s(pTransform + (uBlock-startBlock)*16, tmpCoeffsBuff, 16);
                        ippiDequantResidual4x4_H264_16s(tmpCoeffsBuff, tmpCoeffsBuff, uMBQP);
                    } else {
                        memset(tmpCoeffsBuff, 0, 16*sizeof(COEFFSTYPE));
                    }
                    tmpCoeffsBuff[0] = pDCBuf[chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock]];

                    if (curr_slice->doSCoeffsPrediction)
                    {
                        ippsAdd_16s_I(tmpCoeffsBuff, pPredSCoeffs + uBlock*16, 16);
                        //                    CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);
                    } else {
                        ippsCopy_16s(tmpCoeffsBuff, pPredSCoeffs + uBlock*16, 16);
                    }
                }

                if (intra) {
                    if(!transform_bypass) {
                        ippsCopy_16s(pPredSCoeffs + uBlock*16, tmpCoeffsBuff, 16);

                        ippiTransformResidualAndAdd_H264_16s8u_C1I(pPredBuf,16,
                            tmpCoeffsBuff,
                            pReconBuf,
                            pitchPix);
                    }
                } else {

                    ippiTransformResidualAndAdd_H264_16s16s_C1I(pRefPredResiduals, pPredSCoeffs + uBlock*16,
                        pResidualBuf, core_enc->m_WidthInMBs * 8 * 2, core_enc->m_WidthInMBs * 8 * 2);

                    PIXTYPE *dst = pReconBuf;
                    Ipp16s *src1 = pResidualBuf;
                    Ipp8u *src2 = pPredBuf;
                    int ii, jj;
                    for (ii = 0; ii < 4; ii++) {
                        for (jj = 0; jj < 4; jj++) {
                            Ipp16s tmp;
                            src1[jj] = IPP_MIN(src1[jj], 255);
                            src1[jj] = IPP_MAX(src1[jj], -255);
                            tmp = src1[jj] + src2[jj];
                            tmp = IPP_MIN(tmp, 255);
                            tmp = IPP_MAX(tmp, 0);
                            dst[jj] = tmp;
                        }
                        dst += pitchPix;
                        src1 += core_enc->m_WidthInMBs * 8;
                        src2 += 16;
                    }
                }
            }else {
                if (!cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] && !pDCBuf[ chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock] ]){
                    uCBPChroma &= ~CBP4x4Mask[uBlock-16];
                    Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
                } else if(!transform_bypass) {
                    H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264)(
                        pPredBuf,
                        pTransform + (uBlock-startBlock)*16,
                        &pDCBuf[ chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock] ],
                        pReconBuf,
                        16,
                        pitchPix,
                        uMBQP,
                        (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock]!=0),
                        core_enc->m_SeqParamSet->bit_depth_chroma,
                        NULL);
                }
            }

            pPredBuf += chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock]; //!!!
#ifdef USE_NV12
            pReconBuf += chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock];
#else
            pReconBuf += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
#endif
            pResidualBuf += ((uBlock & 1) ? (core_enc->m_WidthInMBs * 8 * 4 - 4) : 4);
            pRefPredResiduals += ((uBlock & 1) ? (core_enc->m_WidthInMBs * 8 * 4 - 4) : 4);
        }   // for uBlock in chroma plane
        if (!intra && curr_slice->doInterIntraPrediction && pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))
        {
            pRefPredResiduals = pRefPredResiduals0;
            pResidualBuf = pResidualBuf0;
            Ipp32s i,j;
            for (i=0; i<2; i++) {
                for (j=0; j<2; j++) {
                    if (!cur_mb.GlobalMacroblockInfo->i_pred[2*j+i]) continue;
                    Ipp32s offx = (core_enc->svc_ResizeMap[0][cur_mb.uMBx].border+1)>>1;
                    Ipp32s offy = (core_enc->svc_ResizeMap[1][cur_mb.uMBy].border+1)>>1;
                    IppiSize roiSize = {i?8-offx:offx, j?8-offy:offy};
                    if(!i) offx = 0;
                    if(!j) offy = 0;
                    Ipp32s x, y;
                    for (y=0; y<roiSize.height; y++) {
                        for (x=0; x<roiSize.width; x++) {
                            Ipp16s tmp = pResidualBuf[(offy+y)*(core_enc->m_WidthInMBs * 8) + offx+x] +
                                pRefPredResiduals[(offy+y)*(core_enc->m_WidthInMBs * 8) + offx+x];
                            tmp = IPP_MIN(tmp, 255);
                            tmp = IPP_MAX(tmp, -255);
                            pResidualBuf[(offy+y)*(core_enc->m_WidthInMBs * 8) + offx+x] = tmp;
                        }
                    }
                }
            }
        }
    } while (uBlock < uFinalBlock);
    //Reset other chroma
    uCBPChroma &= ~(0xffffffff<<(uBlock-16));
    cur_mb.LocalMacroblockInfo->cbp_chroma = uCBPChroma;
    if (cur_mb.MacroblockCoeffsInfo->chromaNC == 3)
        cur_mb.MacroblockCoeffsInfo->chromaNC = 2;

#ifdef USE_NV12
    Ipp32s i,j;
    uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    pReconBuf = core_enc->m_pReconstructFrame->m_pUPlane+uOffset;
    PIXTYPE *r = cur_mb.mbChromaInter.reconstruct;
    for(i=0;i<8;i++){
        for(j=0;j<8;j++){
            pReconBuf[2*j] = r[j];
            pReconBuf[2*j+1] = r[j+8];
        }
        r += 16;
        pReconBuf += pitchPixels;
    }
#endif

    if (core_enc->m_svc_layer.isActive && !pGetMBIBLFlag(cur_mb.GlobalMacroblockInfo))
    {
        uOffset = core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
#ifdef USE_NV12
        Copy16x8(core_enc->m_pReconstructFrame->m_pUPlane + uOffset, cur_mb.mbPitchPixels,
                 core_enc->m_pUInputRefPlane + uOffset, cur_mb.mbPitchPixels);
#else
        Copy8x8(core_enc->m_pReconstructFrame->m_pUPlane + uOffset, cur_mb.mbPitchPixels,
                core_enc->m_pUInputRefPlane + uOffset, cur_mb.mbPitchPixels);

        Copy8x8(core_enc->m_pReconstructFrame->m_pVPlane + uOffset, cur_mb.mbPitchPixels,
                core_enc->m_pVInputRefPlane + uOffset, cur_mb.mbPitchPixels);
#endif
    }
}

void H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma)(
    void* state,
    H264SliceType *curr_slice)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;         // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    PIXTYPE*  pSrcPlane;    // start of plane to encode
    Ipp32s    pitchPixels;  // buffer pitch
    COEFFSTYPE *pDCBuf;     // chroma & luma dc coeffs pointer
    PIXTYPE*  pPredBuf;     // prediction block pointer
    PIXTYPE*  pReconBuf;     // prediction block pointer
    PIXTYPE*  pPredBuf_copy;     // prediction block pointer
    PIXTYPE*  pReconBuf_copy;     // prediction block pointer
    COEFFSTYPE* pQBuf;      // quantized block pointer
    Ipp16s* pMassDiffBuf;   // difference block pointer
    Ipp32u   uCBPChroma;    // coded flags for all chroma blocks
    Ipp32s   bCoded;        // coded block flag
    Ipp32s   iNumCoeffs = 0;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s   iLastCoeff;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s   RLE_Offset;    // Index into BlockRLE array

    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    EnumSliceType slice_type = curr_slice->m_slice_type;
    COEFFSTYPE *pTransformResult;
    COEFFSTYPE *pTransform;
    Ipp32s QPy = cur_mb.lumaQP;
    Ipp32s pitchPix;

    pitchPix = pitchPixels = cur_mb.mbPitchPixels;
    uMBQP       = cur_mb.chromaQP;
    pTransform = (COEFFSTYPE*)curr_slice->m_pMBEncodeBuffer;
    pQBuf       = (COEFFSTYPE*) (pTransform + 64*2);
    pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    pMassDiffBuf= (Ipp16s*) (pDCBuf+ 16);
    Ipp16s*  pTempDiffBuf;
    Ipp32u  uMB = cur_mb.uMB;
    Ipp32u  mbOffset;
    // initialize pointers and offset
    mbOffset = uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    bool transform_bypass = core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag && QPy == 0;
    uCBPChroma  = cur_mb.LocalMacroblockInfo->cbp_chroma;
    bool intra = (cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA) || (cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA_16x16);
    if (intra) {
        pPredBuf = cur_mb.mbChromaIntra.prediction;
        pReconBuf = cur_mb.mbChromaIntra.reconstruct;
        if(!((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE))){
            cur_mb.MacroblockCoeffsInfo->chromaNC = 0;
#ifdef USE_NV12
              H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectChromaMBs_8x8_NV12)(
                state,
                curr_slice,
                core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                pitchPixels,
                &cur_mb.LocalMacroblockInfo->intra_chroma_mode,
                pPredBuf,
                pPredBuf+8,
                0);  //up to 422 only
#else
            H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectChromaMBs_8x8)(
                state,
                curr_slice,
                core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                core_enc->m_pCurrentFrame->m_pVPlane + uOffset,
                core_enc->m_pReconstructFrame->m_pVPlane + uOffset,
                pitchPixels,
                &cur_mb.LocalMacroblockInfo->intra_chroma_mode,
                pPredBuf,
                pPredBuf+8);  //up to 422 only
#endif
        }
    } else {
        cur_mb.MacroblockCoeffsInfo->chromaNC = 0;
        pPredBuf = cur_mb.mbChromaInter.prediction;
        pReconBuf = cur_mb.mbChromaInter.reconstruct;
        H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBChroma)(state, curr_slice, cur_mb.MVs[LIST_0]->MotionVectors, cur_mb.MVs[LIST_1]->MotionVectors, pPredBuf);
    }
    // initialize pointers for the U plane blocks
    Ipp32s num_blocks = 2 << core_enc->m_PicParamSet->chroma_format_idc;
    Ipp32s startBlock;
    startBlock = uBlock = 16;
    Ipp32u uLastBlock = uBlock+num_blocks;
    Ipp32u uFinalBlock = uBlock+2*num_blocks;
    PIXTYPE* pPredBufV = pPredBuf+8;
#if 0
    uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    ippiSumsDiff8x8Blocks4x4_NV12n(core_enc->m_pCurrentFrame->m_pUPlane + uOffset, pitchPixels, pPredBuf, 16,
        pPredBuf+8,16,pDCBuf,pMassDiffBuf, pDCBuf+8, pMassDiffBuf+128);
#endif
    do
    {
        if (uBlock == uLastBlock) {
            startBlock = uBlock;
            uOffset = mbOffset;
            pSrcPlane = core_enc->m_pCurrentFrame->m_pVPlane;
            pPredBuf = pPredBufV;
#ifdef USE_NV12
            pReconBuf = cur_mb.mbChromaInter.reconstruct+8;
            pitchPix = 16;
#else
            pReconBuf = core_enc->m_pReconstructFrame->m_pVPlane+uOffset;
#endif
            RLE_Offset = V_DC_RLE;
            uLastBlock += num_blocks;
#if 0
            pDCBuf += 8;
            pMassDiffBuf += 128;
#endif
        } else {
            RLE_Offset = U_DC_RLE;
            pSrcPlane = core_enc->m_pCurrentFrame->m_pUPlane;
#ifdef USE_NV12
            pReconBuf = cur_mb.mbChromaInter.reconstruct;
            pitchPix = 16;
#else
            pReconBuf = core_enc->m_pReconstructFrame->m_pUPlane+uOffset;
#endif
        }
        //TODO add for 422
#ifdef USE_NV12
        ippiSumsDiff8x8Blocks4x4_NV12(pSrcPlane + uOffset, pitchPixels, pPredBuf, 16, 0, core_enc->m_WidthInMBs * 8, pDCBuf, pMassDiffBuf);
#else
        H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(pSrcPlane + uOffset, pitchPixels, pPredBuf, 16, pDCBuf, pMassDiffBuf);
#endif
        if (core_enc->m_PicParamSet->chroma_format_idc == 2)
             H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(pSrcPlane + uOffset+8*pitchPixels, pitchPixels, pPredBuf+8*16, 16, pDCBuf+4, pMassDiffBuf+64);
        // Code chromaDC
        if (!transform_bypass)  {
            switch (core_enc->m_PicParamSet->chroma_format_idc) {
                case 1:
                    H264ENC_MAKE_NAME(ippiTransformQuantChromaDC_H264)(pDCBuf, pQBuf, uMBQP, &iNumCoeffs, (slice_type == INTRASLICE), 1, NULL);
                    break;
                case 2:
                    H264ENC_MAKE_NAME(ippiTransformQuantChroma422DC_H264)(pDCBuf, pQBuf, uMBQP, &iNumCoeffs, (slice_type == INTRASLICE), 1, NULL);
                    break;
                default:
                    break;
            }
        } else {
            Ipp32s i,j;
            Ipp32s num_rows, num_cols;
            Ipp32s bPitch;
            num_cols = ((core_enc->m_PicParamSet->chroma_format_idc - 1) & 0x2) ? 4 : 2;
            num_rows = (core_enc->m_PicParamSet->chroma_format_idc & 0x2) ? 4 : 2;
            bPitch = num_cols * 16;
            for(i = 0; i < num_rows; i++) {
                for(j = 0; j < num_cols; j++) {
                    pDCBuf[i*num_cols+j] = pMassDiffBuf[i*bPitch + j*16];
                }
            }
            ippiCalcNonZero(pDCBuf, num_blocks, &iNumCoeffs);
        }
        // DC values in this block if iNonEmpty is 1.
        cur_mb.MacroblockCoeffsInfo->chromaNC |= (iNumCoeffs != 0);
        // record RLE info
        if (core_enc->m_PicParamSet->entropy_coding_mode){
            Ipp32s ctxIdxBlockCat = BLOCK_CHROMA_DC_LEVELS;
            switch (core_enc->m_PicParamSet->chroma_format_idc) {
                case 1:
                    H264ENC_MAKE_NAME(H264CoreEncoder_ScanSignificant_CABAC)(
                        pDCBuf,
                        ctxIdxBlockCat,
                        4,
                        dec_single_scan_p,
                        &cur_mb.cabac_data->dcBlock[RLE_Offset - U_DC_RLE]);
                    break;
                case 2:
                    H264ENC_MAKE_NAME(H264CoreEncoder_ScanSignificant_CABAC)(
                        pDCBuf,
                        ctxIdxBlockCat,
                        8,
                        dec_single_scan_p422,
                        &cur_mb.cabac_data->dcBlock[RLE_Offset - U_DC_RLE]);
                    break;
                default:
                    break;
            }
       }else{
            switch( core_enc->m_PicParamSet->chroma_format_idc ){
                case 1:
                   H264ENC_MAKE_NAME(ippiEncodeChromaDcCoeffsCAVLC_H264)(
                       pDCBuf,
                       &curr_slice->Block_RLE[RLE_Offset].uTrailing_Ones,
                       &curr_slice->Block_RLE[RLE_Offset].uTrailing_One_Signs,
                       &curr_slice->Block_RLE[RLE_Offset].uNumCoeffs,
                       &curr_slice->Block_RLE[RLE_Offset].uTotalZeros,
                       curr_slice->Block_RLE[RLE_Offset].iLevels,
                       curr_slice->Block_RLE[RLE_Offset].uRuns);
                    break;
                case 2:
                    H264ENC_MAKE_NAME(ippiEncodeChroma422DC_CoeffsCAVLC_H264)(
                        pDCBuf,
                        &curr_slice->Block_RLE[RLE_Offset].uTrailing_Ones,
                        &curr_slice->Block_RLE[RLE_Offset].uTrailing_One_Signs,
                        &curr_slice->Block_RLE[RLE_Offset].uNumCoeffs,
                        &curr_slice->Block_RLE[RLE_Offset].uTotalZeros,
                        curr_slice->Block_RLE[RLE_Offset].iLevels,
                        curr_slice->Block_RLE[RLE_Offset].uRuns);
                    break;
            }
        }
        // Inverse transform and dequantize for chroma DC
        if (!transform_bypass ){
            switch( core_enc->m_PicParamSet->chroma_format_idc ){
             case 1:
                 H264ENC_MAKE_NAME(ippiTransformDequantChromaDC_H264)(pDCBuf, uMBQP, NULL);
                 break;
             case 2:
                 H264ENC_MAKE_NAME(ippiTransformDequantChromaDC422_H264)(pDCBuf, uMBQP, NULL);
                 break;
            default:
                break;
            }
        }
//Encode croma AC
        Ipp32s coeffsCost = 0;
        pPredBuf_copy = pPredBuf;
        pReconBuf_copy = pReconBuf;
        for (uBlock = startBlock; uBlock < uLastBlock; uBlock ++) {
            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;     // This will be updated if the block is coded
            if (core_enc->m_PicParamSet->entropy_coding_mode)
            {
                cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
            }
            else
            {
                curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
            }
            // check if block is coded
            bCoded = ((uCBPChroma & CBP4x4Mask[uBlock-16])?(1):(0));
            if (!bCoded){ // update reconstruct frame for the empty block
                Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
            } else {   // block not declared empty, encode
                pTempDiffBuf = pMassDiffBuf + (uBlock-startBlock)*16;
                pTransformResult = pTransform + (uBlock-startBlock)*16;
                if(!transform_bypass) {
                    H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                         pTempDiffBuf,
                         pTransformResult,
                         uMBQP,
                         &iNumCoeffs,
                         0,
                         enc_single_scan[is_cur_mb_field],
                         &iLastCoeff,
                         NULL,
                         NULL,
                         9,
                         NULL);//,NULL, curr_slice, 1, &cbSt);
                    coeffsCost += iNumCoeffs
                        ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 15, &dec_single_scan[is_cur_mb_field][1])
                        : 0;
                 }else {
                    for(Ipp32s i = 0; i < 16; i++) {
                        pTransformResult[i] = pTempDiffBuf[i];
                    }
                    ippiCountCoeffs(pTempDiffBuf, &iNumCoeffs, enc_single_scan[is_cur_mb_field], &iLastCoeff, 16);
                }
                // if everything quantized to zero, skip RLE
                cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)((iNumCoeffs < 0) ? -(iNumCoeffs+1) : iNumCoeffs);
                if (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock])
                {
                    // the block is empty so it is not coded
                    if (core_enc->m_PicParamSet->entropy_coding_mode)
                    {
                        CabacBlock4x4* c_data = &cur_mb.cabac_data->chromaBlock[uBlock - 16];
                        c_data->uLastSignificant = iLastCoeff;
                        c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                        c_data->CtxBlockCat = BLOCK_CHROMA_AC_LEVELS;
                        c_data->uFirstCoeff = 1;
                        c_data->uLastCoeff = 15;
                        H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                            pTransformResult,
                            dec_single_scan[is_cur_mb_field],
                            c_data);
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = c_data->uNumSigCoeffs;
                    }
                    else
                    {
                        H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(
                            pTransformResult,// pDiffBuf,
                            1,
                            dec_single_scan[is_cur_mb_field],
                            iLastCoeff,
                            &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
                            &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
                            &curr_slice->Block_RLE[uBlock].uNumCoeffs,
                            &curr_slice->Block_RLE[uBlock].uTotalZeros,
                            curr_slice->Block_RLE[uBlock].iLevels,
                            curr_slice->Block_RLE[uBlock].uRuns);

                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = curr_slice->Block_RLE[uBlock].uNumCoeffs;
                    }
                }
            }
            pPredBuf += chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock]; //!!!
#ifdef USE_NV12
            pReconBuf += chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock];
#else
            pReconBuf += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
#endif
        }

        if(!transform_bypass && coeffsCost <= (CHROMA_COEFF_MAX_COST<<(core_enc->m_PicParamSet->chroma_format_idc-1)) )
        {
            //Reset all ac coeffs
            for(uBlock = startBlock; uBlock < uLastBlock; uBlock++){
                cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
                if (core_enc->m_PicParamSet->entropy_coding_mode)
                {
                   cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
                }
                else
                {
                    curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                    curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                    curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                    curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
                }
            }
        }

        pPredBuf = pPredBuf_copy;
        pReconBuf = pReconBuf_copy;
        for (uBlock = startBlock; uBlock < uLastBlock; uBlock ++) {
            cur_mb.MacroblockCoeffsInfo->chromaNC |= 2*(cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock]!=0);
            if (!cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] && !pDCBuf[ chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock] ]){
                uCBPChroma &= ~CBP4x4Mask[uBlock-16];
                Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
            }else if(!transform_bypass){
                    H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264)(
                        pPredBuf,
                        pTransform + (uBlock-startBlock)*16,
                        &pDCBuf[ chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock] ],
                        pReconBuf,
                        16,
                        pitchPix,
                        uMBQP,
                        (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock]!=0),
                        core_enc->m_SeqParamSet->bit_depth_chroma,
                        NULL);
            }
            pPredBuf += chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock]; //!!!
#ifdef USE_NV12
            pReconBuf += chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock];
#else
            pReconBuf += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
#endif
        }   // for uBlock in chroma plane
    } while (uBlock < uFinalBlock);
    //Reset other chroma
    uCBPChroma &= ~(0xffffffff<<(uBlock-16));
    cur_mb.LocalMacroblockInfo->cbp_chroma = uCBPChroma;
    if (cur_mb.MacroblockCoeffsInfo->chromaNC == 3)
        cur_mb.MacroblockCoeffsInfo->chromaNC = 2;

#ifdef USE_NV12
        Ipp32s i,j;
        uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        pReconBuf = core_enc->m_pReconstructFrame->m_pUPlane+uOffset;
        PIXTYPE *r = cur_mb.mbChromaInter.reconstruct;
        for(i=0;i<8;i++){
            for(j=0;j<8;j++){
                pReconBuf[2*j] = r[j];
                pReconBuf[2*j+1] = r[j+8];
            }
            r += 16;
            pReconBuf += pitchPixels;
        }
#endif
}

void H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChromaRec)(
    void* state,
    H264SliceType *curr_slice)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;         // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    PIXTYPE*  pSrcPlane;    // start of plane to encode
    Ipp32s    pitchPixels;  // buffer pitch
    COEFFSTYPE *pDCBuf;     // chroma & luma dc coeffs pointer
    PIXTYPE*  pPredBuf;     // prediction block pointer
    PIXTYPE*  pReconBuf;     // prediction block pointer
    PIXTYPE*  pRefLayerPixels;
    COEFFSTYPE* pQBuf;      // quantized block pointer
    Ipp16s* pMassDiffBuf;   // difference block pointer
    Ipp32u   uCBPChroma;    // coded flags for all chroma blocks
    Ipp32s   iNumCoeffs = 0;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s   RLE_Offset;    // Index into BlockRLE array

    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    COEFFSTYPE *pTransform;
    COEFFSTYPE *pPredSCoeffs, *pPredTCoeffs;
    COEFFSTYPE *pResidualBuf;
    COEFFSTYPE *pResidualBuf0;
    COEFFSTYPE *pRefPredResiduals;
    COEFFSTYPE *pRefPredResiduals0;
    Ipp32s QPy = cur_mb.lumaQP;
    Ipp32s pitchPix;

    pitchPix = pitchPixels = cur_mb.mbPitchPixels;
    uMBQP       = cur_mb.chromaQP;
    pTransform = (COEFFSTYPE*)curr_slice->m_pMBEncodeBuffer;
    pQBuf       = (COEFFSTYPE*) (pTransform + 64*2);
    pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    pMassDiffBuf= (Ipp16s*) (pDCBuf+ 16);
    Ipp32u  uMB = cur_mb.uMB;
    Ipp32u  mbOffset;
    // initialize pointers and offset
    mbOffset = uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    bool transform_bypass = core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag && QPy == 0;
    uCBPChroma  = cur_mb.LocalMacroblockInfo->cbp_chroma;
    bool intra = IS_TRUEINTRA_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype);

    pPredSCoeffs = &(core_enc->m_SavedMacroblockCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);
    pPredTCoeffs = &(core_enc->m_SavedMacroblockTCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);

    if (intra) {
        pPredBuf = cur_mb.mbChromaIntra.prediction;

        if (pGetMBIBLFlag(cur_mb.GlobalMacroblockInfo))
        {
#ifdef USE_NV12
            Ipp32s i,j;
            pRefLayerPixels = core_enc->m_pUInputRefPlane+uOffset;
            PIXTYPE *r = pPredBuf;
            for(i=0;i<8;i++){
                for(j=0;j<8;j++){
                    r[j] = pRefLayerPixels[2*j];
                    r[j+8] = pRefLayerPixels[2*j+1];
                }
                r += 16;
                pRefLayerPixels += pitchPixels;
            }
#endif
        }

        pReconBuf = cur_mb.mbChromaIntra.reconstruct;
        if((!((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE)) &&
            (!pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))) || curr_slice->doTCoeffsPrediction){
            cur_mb.MacroblockCoeffsInfo->chromaNC = 0;
#ifdef USE_NV12
              H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectChromaMBs_8x8_NV12)(
                state,
                curr_slice,
                core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                pitchPixels,
                &cur_mb.LocalMacroblockInfo->intra_chroma_mode,
                pPredBuf,
                pPredBuf+8,
                0,
                curr_slice->doTCoeffsPrediction);  //up to 422 only
#else
            H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectChromaMBs_8x8)(
                state,
                curr_slice,
                core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                core_enc->m_pCurrentFrame->m_pVPlane + uOffset,
                core_enc->m_pReconstructFrame->m_pVPlane + uOffset,
                pitchPixels,
                &cur_mb.LocalMacroblockInfo->intra_chroma_mode,
                pPredBuf,
                pPredBuf+8,
                curr_slice->doTCoeffsPrediction);  //up to 422 only
#endif
        }
    } else {
        cur_mb.MacroblockCoeffsInfo->chromaNC = 0;
        pPredBuf = cur_mb.mbChromaInter.prediction;
        pReconBuf = cur_mb.mbChromaInter.reconstruct;

        H264MotionVector mv_f[16], *pmv_f;
        H264MotionVector mv_b[16], *pmv_b;
        if (core_enc->m_svc_layer.isActive) {
            pmv_f = mv_f;
            pmv_b = mv_b;
            truncateMVs(state, curr_slice, cur_mb.MVs[LIST_0]->MotionVectors, mv_f);
            if (curr_slice->m_slice_type == BPREDSLICE)
                truncateMVs(state, curr_slice, cur_mb.MVs[LIST_1]->MotionVectors, mv_b);
        } else {
            pmv_f = cur_mb.MVs[LIST_0]->MotionVectors;
            pmv_b = cur_mb.MVs[LIST_1]->MotionVectors;
        }

        if (curr_slice->doMCDirectForSkipped)
            cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_DIRECT; //for correct MC

        H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBChroma)(state, curr_slice, pmv_f, pmv_b, pPredBuf);

        if (curr_slice->doMCDirectForSkipped)
            cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;

        if (curr_slice->doInterIntraPrediction)
        {
                //RestoreIntraPrediction - copy intra predicted parts
                Ipp32s i,j;
                for (i=0; i<2; i++) {
                    for (j=0; j<2; j++) {
                        if (!cur_mb.GlobalMacroblockInfo->i_pred[2*j+i]) continue;
                        Ipp32s offx = (core_enc->svc_ResizeMap[0][cur_mb.uMBx].border+1)>>1;
                        Ipp32s offy = (core_enc->svc_ResizeMap[1][cur_mb.uMBy].border+1)>>1;
                        IppiSize roiSize = {i?8-offx:offx, j?8-offy:offy};
                        if(!i) offx = 0;
                        if(!j) offy = 0;
                        // NV12 !!
                        Ipp32s x, y;
                        PIXTYPE* ptr = core_enc->m_pReconstructFrame->m_pUPlane+uOffset;
                        for (y=0; y<roiSize.height; y++) {
                            for (x=0; x<roiSize.width; x++) {
                                pPredBuf[(offy+y)*16 + offx+x] =
                                    ptr[(offy+y)*pitchPixels + (offx+x) * 2];
                                pPredBuf[(offy+y)*16 + offx+x + 8] =
                                    ptr[(offy+y)*pitchPixels + (offx+x) * 2 + 1];
                            }
                        }
                    }
                }
        }
    }

    // initialize pointers for the U plane blocks
    Ipp32s num_blocks = 2 << core_enc->m_PicParamSet->chroma_format_idc;
    Ipp32s startBlock;
    startBlock = uBlock = 16;
    Ipp32u uLastBlock = uBlock+num_blocks;
    Ipp32u uFinalBlock = uBlock+2*num_blocks;
    PIXTYPE* pPredBufV = pPredBuf+8;

    do
    {
        if (uBlock == uLastBlock) {
            startBlock = uBlock;
            uOffset = mbOffset;
            pSrcPlane = core_enc->m_pCurrentFrame->m_pVPlane;
            pPredBuf = pPredBufV;
#ifdef USE_NV12
            pReconBuf = cur_mb.mbChromaInter.reconstruct+8;
            pitchPix = 16;
#else
            pReconBuf = core_enc->m_pReconstructFrame->m_pVPlane+uOffset;
#endif
            RLE_Offset = V_DC_RLE;
            uLastBlock += num_blocks;

            pResidualBuf = core_enc->m_pVResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
            pResidualBuf0 = pResidualBuf;
            pRefPredResiduals = core_enc->m_pVInputResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
            pRefPredResiduals0 = pRefPredResiduals;
            if(pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))
                pRefPredResiduals = core_enc->m_pVInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
        } else {
            RLE_Offset = U_DC_RLE;
            pSrcPlane = core_enc->m_pCurrentFrame->m_pUPlane;
#ifdef USE_NV12
            pReconBuf = cur_mb.mbChromaInter.reconstruct;
            pitchPix = 16;
#else
            pReconBuf = core_enc->m_pReconstructFrame->m_pUPlane+uOffset;
#endif
            pResidualBuf = core_enc->m_pUResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
            pResidualBuf0 = pResidualBuf;
            pRefPredResiduals = core_enc->m_pUInputResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
            pRefPredResiduals0 = pRefPredResiduals;
            if(pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))
                pRefPredResiduals = core_enc->m_pUInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
        }

        if (cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_SKIPPED)
        {
            memset(pDCBuf, 0, 4*sizeof(COEFFSTYPE));
            iNumCoeffs = 0;
        }

        if (curr_slice->doTCoeffsPrediction)
        {
            Ipp32s i;
            for (i = 0; i < 4; i ++)
            {
                pDCBuf[i] = pPredTCoeffs[(startBlock+i)*16];
            }
        }/* else {
            Ipp32s i;
            for (i = 0; i < 4; i ++)
            {
                pPredTCoeffs[(startBlock+i)*16] = pDCBuf[i];
            }
        }*/

        // Inverse transform and dequantize for chroma DC
        if (!transform_bypass ){
            switch( core_enc->m_PicParamSet->chroma_format_idc ){
             case 1:
                 H264ENC_MAKE_NAME(ippiTransformDequantChromaDC_H264)(pDCBuf, uMBQP, NULL);
                 break;
             case 2:
                 H264ENC_MAKE_NAME(ippiTransformDequantChromaDC422_H264)(pDCBuf, uMBQP, NULL);
                 break;
            default:
                break;
            }
        }
//Encode croma AC

        for (uBlock = startBlock; uBlock < uLastBlock; uBlock ++) {
            __ALIGN16 Ipp16s tmpCoeffsBuff[16];
            pTransform = pPredTCoeffs + uBlock * 16;

            if (core_enc->m_svc_layer.isActive) {
                if (!transform_bypass)
                {
                    ippsCopy_16s(pTransform, tmpCoeffsBuff, 16);
                    ippiDequantResidual4x4_H264_16s(tmpCoeffsBuff, tmpCoeffsBuff, uMBQP);
                    tmpCoeffsBuff[0] = pDCBuf[chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock]];

                    if (curr_slice->doSCoeffsPrediction)
                    {
                        ippsAdd_16s_I(tmpCoeffsBuff, pPredSCoeffs + uBlock*16, 16);
                        //                    CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);
                    } else {
                        ippsCopy_16s(tmpCoeffsBuff, pPredSCoeffs + uBlock*16, 16);
                    }
                }

                if (intra) {
                    if(!transform_bypass) {
                        ippsCopy_16s(pPredSCoeffs + uBlock*16, tmpCoeffsBuff, 16);

                        ippiTransformResidualAndAdd_H264_16s8u_C1I(pPredBuf,16,
                            tmpCoeffsBuff,
                            pReconBuf,
                            pitchPix);
                    }
                } else {

                    ippiTransformResidualAndAdd_H264_16s16s_C1I(pRefPredResiduals, pPredSCoeffs + uBlock*16,
                        pResidualBuf, core_enc->m_WidthInMBs * 8 * 2, core_enc->m_WidthInMBs * 8 * 2);

                    PIXTYPE *dst = pReconBuf;
                    Ipp16s *src1 = pResidualBuf;
                    Ipp8u *src2 = pPredBuf;
                    int ii, jj;
                    for (ii = 0; ii < 4; ii++) {
                        for (jj = 0; jj < 4; jj++) {
                            Ipp16s tmp;
                            src1[jj] = IPP_MIN(src1[jj], 255);
                            src1[jj] = IPP_MAX(src1[jj], -255);
                            tmp = src1[jj] + src2[jj];
                            tmp = IPP_MIN(tmp, 255);
                            tmp = IPP_MAX(tmp, 0);
                            dst[jj] = tmp;
                        }
                        dst += pitchPix;
                        src1 += core_enc->m_WidthInMBs * 8;
                        src2 += 16;
                    }
                }
            }

            pPredBuf += chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock]; //!!!
#ifdef USE_NV12
            pReconBuf += chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock];
#else
            pReconBuf += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
#endif
            pResidualBuf += ((uBlock & 1) ? (core_enc->m_WidthInMBs * 8 * 4 - 4) : 4);
            pRefPredResiduals += ((uBlock & 1) ? (core_enc->m_WidthInMBs * 8 * 4 - 4) : 4);
        }   // for uBlock in chroma plane
        if (!intra && curr_slice->doInterIntraPrediction && pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))
        {
            pRefPredResiduals = pRefPredResiduals0;
            pResidualBuf = pResidualBuf0;
            Ipp32s i,j;
            for (i=0; i<2; i++) {
                for (j=0; j<2; j++) {
                    if (!cur_mb.GlobalMacroblockInfo->i_pred[2*j+i]) continue;
                    Ipp32s offx = (core_enc->svc_ResizeMap[0][cur_mb.uMBx].border+1)>>1;
                    Ipp32s offy = (core_enc->svc_ResizeMap[1][cur_mb.uMBy].border+1)>>1;
                    IppiSize roiSize = {i?8-offx:offx, j?8-offy:offy};
                    if(!i) offx = 0;
                    if(!j) offy = 0;
                    // NV12 !!
                    Ipp32s x, y;
                    for (y=0; y<roiSize.height; y++) {
                        for (x=0; x<roiSize.width; x++) {
                            Ipp16s tmp = pResidualBuf[(offy+y)*(core_enc->m_WidthInMBs * 8) + offx+x] +
                                pRefPredResiduals[(offy+y)*(core_enc->m_WidthInMBs * 8) + offx+x];
                            tmp = IPP_MIN(tmp, 255);
                            tmp = IPP_MAX(tmp, -255);
                            pResidualBuf[(offy+y)*(core_enc->m_WidthInMBs * 8) + offx+x] = tmp;
                        }
                    }
                }
            }
        }
    } while (uBlock < uFinalBlock);

#ifdef USE_NV12
    Ipp32s i,j;
    uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    pReconBuf = core_enc->m_pReconstructFrame->m_pUPlane+uOffset;
    PIXTYPE *r = cur_mb.mbChromaInter.reconstruct;
    for(i=0;i<8;i++){
        for(j=0;j<8;j++){
            pReconBuf[2*j] = r[j];
            pReconBuf[2*j+1] = r[j+8];
        }
        r += 16;
        pReconBuf += pitchPixels;
    }
#endif

    if (core_enc->m_svc_layer.isActive && !pGetMBIBLFlag(cur_mb.GlobalMacroblockInfo))
    {
        uOffset = core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
#ifdef USE_NV12
        Copy16x8(core_enc->m_pReconstructFrame->m_pUPlane + uOffset, cur_mb.mbPitchPixels,
                 core_enc->m_pUInputRefPlane + uOffset, cur_mb.mbPitchPixels);
#else
        Copy8x8(core_enc->m_pReconstructFrame->m_pUPlane + uOffset, cur_mb.mbPitchPixels,
                core_enc->m_pUInputRefPlane + uOffset, cur_mb.mbPitchPixels);

        Copy8x8(core_enc->m_pReconstructFrame->m_pVPlane + uOffset, cur_mb.mbPitchPixels,
                core_enc->m_pVInputRefPlane + uOffset, cur_mb.mbPitchPixels);
#endif
    }
}


void DoScaleTCoeffs(COEFFSTYPE *in_pSrcDstCoeffs,
                    Ipp8u in_curQP,
                    Ipp8u in_baseQP,
                    Ipp32s len)
{
    Ipp32s i;
    Ipp32s qp_delta;
    Ipp32s scale;
    Ipp32s shift;
    Ipp32s tcoeffs_scale_factors[6]  = { 8, 9, 10, 11, 13, 14 };

    qp_delta = in_baseQP - in_curQP + 54;
    scale = tcoeffs_scale_factors[ qp_delta % 6 ];
    shift = qp_delta / 6;

    for( i = 0; i < len; i++ )
    {
        in_pSrcDstCoeffs[i] = (COEFFSTYPE)(( ( scale * in_pSrcDstCoeffs[i] ) << shift ) >> 12);
    }
}

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec4x4IntraMB)(
    void* state,
    H264SliceType *curr_slice)
{
    __ALIGN16 COEFFSTYPE tmpCoeffsBuff[64];
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;     // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    //Ipp32u  uMBType;        // current MB type
    Ipp32u  uMB;
    Ipp32u  uCBPLuma;        // coded flags for all 4x4 blocks
    Ipp32u  uIntraSAD;      // intra MB SAD
    //Ipp16s* pMassDiffBuf;   // difference block pointer

    //COEFFSTYPE* pDCBuf;     // chroma & luma dc coeffs pointer
    COEFFSTYPE *pPredSCoeffs, *pPredTCoeffs;
    PIXTYPE*  pPredBuf;       // prediction block pointer
    PIXTYPE*  pReconBuf;       // prediction block pointer
    PIXTYPE*  pRefLayerPixels;
    Ipp16s*   pDiffBuf;       // difference block pointer
    COEFFSTYPE* pTransformResult; // Result of the transformation.
    //COEFFSTYPE* pQBuf;          // quantized block pointer
    PIXTYPE*  pSrcPlane;      // start of plane to encode
    Ipp32s    pitchPixels;     // buffer pitch
    //Ipp32s    iMBCost;        // recode MB cost counter
    //Ipp32s    iBlkCost[2];    // coef removal counter for left/right 8x8 luma blocks
    Ipp8u     bCoded; // coded block flag
    Ipp32s    iNumCoeffs;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s    iLastCoeff;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32u    uTotalCoeffs = 0;    // Used to detect single expensive coeffs.

    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    __ALIGN16 TrellisCabacState cbSt;

    uMB = cur_mb.uMB;
    pPredSCoeffs = &(core_enc->m_SavedMacroblockCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);
    pPredTCoeffs = &(core_enc->m_SavedMacroblockTCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);

    pitchPixels = cur_mb.mbPitchPixels;
    uCBPLuma     = cur_mb.LocalMacroblockInfo->cbp_luma;
    uMBQP       = cur_mb.lumaQP;
    //uMBType     = cur_mb.GlobalMacroblockInfo->mbtype;
    pDiffBuf    = (Ipp16s*) (curr_slice->m_pMBEncodeBuffer + 512);
    pTransformResult = (COEFFSTYPE*)(pDiffBuf + 64);
    //pQBuf       = (COEFFSTYPE*) (pTransformResult + 64);
    //pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    //pMassDiffBuf= (Ipp16s*) (pDCBuf+ 16);
//  uIntraSAD   = rd_quant_intra[uMBQP] * 24;   // TODO ADB 'handicap' using reconstructed data
    uIntraSAD   = 0;
    //iMBCost     = 0;
    //iBlkCost[0] = 0;
    //iBlkCost[1] = 0;
    Ipp32s doIntraPred = !pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo) &&
        (curr_slice->m_scan_idx_start != 0 || curr_slice->m_scan_idx_end != 15);

    //--------------------------------------------------------------------------
    // encode Y plane blocks (0-15)
    //--------------------------------------------------------------------------

    // initialize pointers and offset
    pSrcPlane = core_enc->m_pCurrentFrame->m_pYPlane;
    uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    bool transform_bypass = core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag && uMBQP == 0;

    Ipp32s pitchPix;
    pitchPix = pitchPixels;

    if(pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo))
    {
        if (core_enc->m_info.quant_opt_level > OPT_QUANT_INTRA8x8)
        {
            uCBPLuma = 0xffff;
            H264BsBase_CopyContextCABAC_Trellis8x8(&cbSt, curr_slice->m_pbitstream, !is_cur_mb_field);
        }

        pSetMB8x8TSFlag(cur_mb.GlobalMacroblockInfo, 1);

        //loop over all 8x8 blocks in Y plane for the MB
        for (uBlock = 0; uBlock < 4; uBlock ++)
        {
            Ipp32s idxb, idx, idxe;

            idxb = uBlock<<2;
            idxe = idxb+4;
            pPredBuf = cur_mb.mb8x8.prediction + xoff[4*uBlock] + yoff[4*uBlock]*16;
            pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;

            if (pGetMBIBLFlag(cur_mb.GlobalMacroblockInfo)) {
                pRefLayerPixels = core_enc->m_pYInputRefPlane + uOffset;
                Copy8x8(pRefLayerPixels, pitchPix, pPredBuf, 16);
            }

            if (core_enc->m_PicParamSet->entropy_coding_mode)
            {
                cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;        // These will be updated if the block is coded
                cur_mb.cabac_data->lumaBlock8x8[uBlock].uNumSigCoeffs = 0;
            }
            else
            {
                for (idx = idxb; idx<idxe; idx++)
                {
                    curr_slice->Block_RLE[idx].uNumCoeffs = 0;
                    curr_slice->Block_RLE[idx].uTrailing_Ones = 0;
                    curr_slice->Block_RLE[idx].uTrailing_One_Signs = 0;
                    curr_slice->Block_RLE[idx].uTotalZeros = 16;
                    cur_mb.MacroblockCoeffsInfo->numCoeff[idx] = 0;
               }
            }

            if ((!curr_slice->m_use_transform_for_intra_decision) &&
                (!pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)))
            {
                uIntraSAD += H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectOneMB_8x8)(
                    state,
                    curr_slice,
                    pSrcPlane + uOffset,
                    pReconBuf,
                    uBlock,
                    cur_mb.intra_types,
                    pPredBuf);
            }

            // check if block is coded
            bCoded = ((uCBPLuma & CBP8x8Mask[uBlock])?(1):(0));

            if (!bCoded && !doIntraPred)
            {
                // update reconstruct frame for the empty block
                Copy8x8(pPredBuf, 16, pReconBuf, pitchPix);
                if (core_enc->m_svc_layer.isActive) {
                    ippsZero_16s(pPredTCoeffs + uBlock * 64, 64);
//                    ippsZero_16s(cur_mb.m_pTCoeffsTmp + 64*uBlock, 64);
                }
            }
            else
            {   // block not declared empty, encode
                // compute difference of predictor and source pels
                // note: asm version does not use pDiffBuf
                //       output is being passed in the mmx registers
                if (!curr_slice->m_use_transform_for_intra_decision || (core_enc->m_info.quant_opt_level > OPT_QUANT_INTRA8x8) ||
                    (pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)) || doIntraPred)
                {
                    if (!transform_bypass)
                    {
                        if (!curr_slice->doSCoeffsPrediction)
                        {
                            if (((core_enc->m_info.quant_opt_level > OPT_QUANT_INTRA8x8) &&
                                (!pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))) || curr_slice->doTCoeffsPrediction || doIntraPred)
                            {
                                __ALIGN16 PIXTYPE pred[64];
                                PIXTYPE pred_pels[25]; //Sources for prediction
                                Ipp32u pred_pels_mask = 0;
                                Ipp32s i;
                                bool top_avbl;
                                bool left_avbl;
                                bool left_above_avbl = curr_slice->m_cur_mb.IntraNeighbours.mb_D >= 0;
                                bool right_above_avbl = curr_slice->m_cur_mb.IntraNeighbours.mb_C >= 0;

                                if( uBlock & 0x2 ){
                                    top_avbl = true;
                                }else{
                                    top_avbl = curr_slice->m_cur_mb.IntraNeighbours.mb_B >= 0;
                                }

                                if( uBlock & 0x1 ){
                                    left_avbl = true;
                                }else{
                                    left_avbl = curr_slice->m_cur_mb.IntraNeighbours.mb_A >= 0;
                                }

                                //Copy pels
                                //TOP
                                if( top_avbl ){
                                    for( i=0; i<8; i++ )
                                        pred_pels[1+i] = *(pReconBuf-pitchPixels+i);
                                    pred_pels_mask |= 0x000001fe;
                                }

                                //LEFT
                                if( left_avbl ){
                                    for( i=0; i<8; i++ )
                                        pred_pels[17+i] = *(pReconBuf+i*pitchPixels-1);
                                    pred_pels_mask |= 0x1fe0000;
                                }

                                //LEFT_ABOVE
                                if( (uBlock == 0 && left_above_avbl) || uBlock == 3 ||
                                    (uBlock == 1 && top_avbl) || ( uBlock == 2 && left_avbl)){
                                        pred_pels[0] = *(pReconBuf-pitchPixels-1);
                                    pred_pels_mask |= 0x01;
                                }

                                //RIGHT_ABOVE
                                if( (uBlock == 2) || (uBlock == 0 && top_avbl) ||
                                    (uBlock == 1 && right_above_avbl) ){
                                    for( i=0; i<8; i++ )
                                        pred_pels[9+i] = *(pReconBuf-pitchPixels+i+8);
                                    pred_pels_mask |= 0x0001fe00;
                                }

                                if( !((pred_pels_mask & 0x0001FE00)==0x0001FE00) && (pred_pels_mask & 0x00000100) ){
                                    pred_pels_mask |= 0x0001FE00;
                                    for( i=0; i<8; i++ ) pred_pels[9+i] = pred_pels[1+7];
                                }

                                H264ENC_MAKE_NAME(H264CoreEncoder_Filter8x8Pels)(pred_pels, pred_pels_mask);
                                H264ENC_MAKE_NAME(H264CoreEncoder_GetPrediction8x8)(state, cur_mb.intra_types[uBlock<<2], pred_pels, pred_pels_mask, pred );
                                PIXTYPE* p = pPredBuf;
                                for( i=0; i<8; i++){
                                    MFX_INTERNAL_CPY(p, &pred[i*8], 8*sizeof(PIXTYPE));
                                    p += 16; //pitch = 16
                                }
                                Diff8x8(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);
                                H264ENC_MAKE_NAME(ippiTransformLuma8x8Fwd_H264)(pDiffBuf, pTransformResult);
                                H264ENC_MAKE_NAME(ippiQuantLuma8x8_H264)(
                                    pTransformResult,
                                    pTransformResult,
                                    uMBQP,
                                    1,
                                    enc_single_scan_8x8[is_cur_mb_field],
                                    core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[0][QP_MOD_6[uMBQP]], //Use scaling matrix for INTRA
                                    &iNumCoeffs,
                                    &iLastCoeff,
                                    curr_slice,
                                    &cbSt,
                                    core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[0][QP_MOD_6[uMBQP]]);
                            }
                            else
                            {
                                // forward transform and quantization, in place in pDiffBuf
                                Diff8x8(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);
                                H264ENC_MAKE_NAME(ippiTransformLuma8x8Fwd_H264)(pDiffBuf, pTransformResult);
                                H264ENC_MAKE_NAME(ippiQuantLuma8x8_H264)(
                                    pTransformResult,
                                    pTransformResult,
                                    QP_DIV_6[uMBQP],
                                    1,
                                    enc_single_scan_8x8[is_cur_mb_field],
                                    core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[0][QP_MOD_6[uMBQP]], //Use scaling matrix for INTRA
                                    &iNumCoeffs,
                                    &iLastCoeff,
                                    NULL,
                                    NULL,
                                    NULL);
                            }
                        } else {
                            Ipp32s i;

                            Diff8x8(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);
                            H264ENC_MAKE_NAME(ippiTransformLuma8x8Fwd_H264)(pDiffBuf, pTransformResult);

                            for (i = 0; i < 64; i++)
                            {
                                pTransformResult[i] -= ((pPredSCoeffs[uBlock*64+i] * normTable8x8[i]) >> 11);
                            }

                            if (core_enc->m_info.quant_opt_level > OPT_QUANT_INTRA8x8)
                            {
                                H264ENC_MAKE_NAME(ippiQuantLuma8x8_H264)(pTransformResult, pTransformResult, uMBQP, 0,
                                    enc_single_scan_8x8[is_cur_mb_field], core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[0][QP_MOD_6[uMBQP]], // INTRA scaling matrix
                                    &iNumCoeffs, &iLastCoeff,curr_slice,&cbSt,core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[0][QP_MOD_6[uMBQP]]);
                            }
                            else
                            {
                                H264ENC_MAKE_NAME(ippiQuantLuma8x8_H264)(
                                    pTransformResult,
                                    pTransformResult,
                                    QP_DIV_6[uMBQP],
                                    0,
                                    enc_single_scan_8x8[is_cur_mb_field],
                                    core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[0][QP_MOD_6[uMBQP]], // INTRA scaling matrix
                                    &iNumCoeffs,
                                    &iLastCoeff,
                                    NULL,
                                    NULL,
                                    NULL);
                            }
                        }
                    }
                    else
                    {
                        Diff8x8(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);
                        for(Ipp32s i = 0; i < 64; i++) {
                            pTransformResult[i] = pDiffBuf[i];
                        }
                        ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan_8x8[is_cur_mb_field], &iLastCoeff, 64);
                    }
                }
                else
                {
                        iNumCoeffs = cur_mb.m_iNumCoeffs8x8[ uBlock ];
                        iLastCoeff = cur_mb.m_iLastCoeff8x8[ uBlock ];
                        pTransformResult = &cur_mb.mb8x8.transform[ uBlock*64 ];
                }

                if (curr_slice->doTCoeffsPrediction)
                {
                    ippsSub_16s_I(pPredTCoeffs + uBlock*64, pTransformResult, 64);
                    ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan_8x8[is_cur_mb_field], &iLastCoeff, 64);
                }

                if (core_enc->m_svc_layer.isActive &&
                    (curr_slice->m_scan_idx_start != 0 || curr_slice->m_scan_idx_end != 15)) {
                        ScanIdxZero(pTransformResult, curr_slice->m_scan_idx_start*4, curr_slice->m_scan_idx_end*4+4, dec_single_scan_8x8[is_cur_mb_field], 64);
                        ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan_8x8[is_cur_mb_field], &iLastCoeff, 64);
                }

//                if (core_enc->m_svc_layer.isActive)
//                    ippsCopy_16s(pTransformResult, cur_mb.m_pTCoeffsTmp + 64*uBlock, 64);

                // if everything quantized to zero, skip RLE
                if (!iNumCoeffs)
                {
                    // the block is empty so it is not coded
                    bCoded = 0;
                }
                else
                {
                    uTotalCoeffs += ((iNumCoeffs < 0) ? -(iNumCoeffs*2) : iNumCoeffs);

                    // record RLE info
                    if (core_enc->m_PicParamSet->entropy_coding_mode)
                    {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iNumCoeffs);

                        CabacBlock8x8* c_data = &cur_mb.cabac_data->lumaBlock8x8[uBlock];
                        c_data->uLastSignificant = iLastCoeff;
                        c_data->CtxBlockCat = BLOCK_LUMA_64_LEVELS;
                        c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                        c_data->uFirstCoeff = 0;
                        c_data->uLastCoeff = 63;
                        H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                            pTransformResult,
                            dec_single_scan_8x8[is_cur_mb_field],
                            c_data);

                        bCoded = c_data->uNumSigCoeffs;
                    }
                    else
                    {
                        COEFFSTYPE buf4x4[4][16];
                        Ipp32s i4x4;

                        //Reorder 8x8 block for coding with CAVLC
                        for(i4x4=0; i4x4<4; i4x4++ ) {
                            Ipp32s i;
                            for(i = 0; i<16; i++ )
                                buf4x4[i4x4][dec_single_scan[is_cur_mb_field][i]] =
                                    pTransformResult[dec_single_scan_8x8[is_cur_mb_field][4*i+i4x4]];
                        }

                        bCoded = 0;
                        //Encode each block with CAVLC 4x4
                        for(i4x4 = 0; i4x4<4; i4x4++ ) {
                            Ipp32s i;
                            iLastCoeff = 0;
                            idx = idxb + i4x4;

                            //Check for last coeff
                            for(i = 0; i<16; i++ ) if( buf4x4[i4x4][dec_single_scan[is_cur_mb_field][i]] != 0 ) iLastCoeff=i;

                            H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(
                                buf4x4[i4x4],
                                curr_slice->m_scan_idx_start, //Luma
                                dec_single_scan[is_cur_mb_field],
                                iLastCoeff,
                                &curr_slice->Block_RLE[idx].uTrailing_Ones,
                                &curr_slice->Block_RLE[idx].uTrailing_One_Signs,
                                &curr_slice->Block_RLE[idx].uNumCoeffs,
                                &curr_slice->Block_RLE[idx].uTotalZeros,
                                curr_slice->Block_RLE[idx].iLevels,
                                curr_slice->Block_RLE[idx].uRuns);

                            bCoded += curr_slice->Block_RLE[idx].uNumCoeffs;
                            cur_mb.MacroblockCoeffsInfo->numCoeff[idx] = curr_slice->Block_RLE[idx].uNumCoeffs;
                         }
                    }
                }

                if (core_enc->m_svc_layer.isActive) {
                    if (curr_slice->doTCoeffsPrediction)
                    {
                        ippsAdd_16s_I(pTransformResult, pPredTCoeffs + uBlock*64, 64);
                        //                CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);
                    }
                    else
                    {
                        ippsCopy_16s(pTransformResult, pPredTCoeffs + uBlock*64, 64);
                    }

                    if (!transform_bypass)
                    {
                        ippsCopy_16s(pPredTCoeffs + uBlock*64, tmpCoeffsBuff, 64);
                        H264ENC_MAKE_NAME(ippiQuantLuma8x8Inv_H264)(tmpCoeffsBuff, QP_DIV_6[uMBQP], core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[0][QP_MOD_6[uMBQP]]);

                        if (curr_slice->doSCoeffsPrediction)
                        {
                            ippsAdd_16s_I(tmpCoeffsBuff, pPredSCoeffs + uBlock*64, 64);
                            //                    CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);
                        } else {
                            ippsCopy_16s(tmpCoeffsBuff, pPredSCoeffs + uBlock*64, 64);
                        }
                    }
                }

                // update flags if block quantized to empty
                if ((curr_slice->m_use_transform_for_intra_decision && core_enc->m_info.quant_opt_level < OPT_QUANT_INTRA8x8 + 1) &&
                    (!pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)) && !doIntraPred)
                {
                    if (!bCoded)
                    {
                        uCBPLuma &= ~CBP8x8Mask[uBlock];
                        //Copy prediction
                        Copy8x8(pPredBuf, 16, pReconBuf, pitchPix);
                    }
                    else
                    {
                        //Copy reconstruct
                        Copy8x8(pPredBuf + 256, 16, pReconBuf, pitchPix);
                    }
                }
                else
                {
                    if (!bCoded)
                        uCBPLuma &= ~CBP8x8Mask[uBlock];
                    // update flags if block quantized to empty
                    if (!bCoded && !pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))
                    {
//                        uCBPLuma &= ~CBP8x8Mask[uBlock];
                        // update reconstruct frame with prediction
                        Copy8x8(pPredBuf, 16, pReconBuf, pitchPix);
                    }
                    else if (!transform_bypass)
                    {
                        if (core_enc->m_svc_layer.isActive) {
                            ippsCopy_16s(pPredSCoeffs + uBlock*64, tmpCoeffsBuff, 64);
                            ippiTransformResidualAndAdd8x8_H264_16s8u_C1I(pPredBuf,
                                tmpCoeffsBuff,
                                pReconBuf,
                                16,
                                pitchPix);
                        } else {
                            if (iNumCoeffs != 0)
                            {
                                H264ENC_MAKE_NAME(ippiQuantLuma8x8Inv_H264)(pTransformResult, QP_DIV_6[uMBQP], core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[0][QP_MOD_6[uMBQP]]);
                                H264ENC_MAKE_NAME(ippiTransformLuma8x8InvAddPred_H264)(pPredBuf, 16, pTransformResult, pReconBuf, pitchPix, core_enc->m_PicParamSet->bit_depth_luma);
                            }
                        }
                    }
                    else
                    {
                        // Transform bypass => lossless
                        // RecPlane == SrcPlane => there is no need to copy.
                    }
                }   // block not declared empty
            } //curr_slice->m_use_transform_for_intra_decision

            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock] * 2;
        }  // for uBlock in luma plane
    }
    else
    {
        // loop over all 4x4 blocks in Y plane for the MB
        if (core_enc->m_info.quant_opt_level > OPT_QUANT_INTRA4x4)
        {
            uCBPLuma = 0xffff;
            H264BsBase_CopyContextCABAC_Trellis4x4(&cbSt, curr_slice->m_pbitstream, !is_cur_mb_field);
        }

        for (uBlock = 0; uBlock < 16; uBlock++)
        {
            COEFFSTYPE *pPredSCoeffsTmp = pPredSCoeffs;
            COEFFSTYPE *pPredTCoeffsTmp = pPredTCoeffs;

            pPredSCoeffsTmp += block_subblock_mapping[uBlock] * 16;
            pPredTCoeffsTmp += block_subblock_mapping[uBlock] * 16;

            pPredBuf = cur_mb.mb4x4.prediction + xoff[uBlock] + yoff[uBlock]*16;
            pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;

            if (pGetMBIBLFlag(cur_mb.GlobalMacroblockInfo)) {
                pRefLayerPixels = core_enc->m_pYInputRefPlane + uOffset;
                Copy4x4(pRefLayerPixels, pitchPix, pPredBuf, 16);
            }

            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0; // These will be updated if the block is coded
            if (core_enc->m_PicParamSet->entropy_coding_mode)
            {
                cur_mb.cabac_data->lumaBlock4x4[uBlock].uNumSigCoeffs = 0;
            }
            else
            {
                curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                curr_slice->Block_RLE[uBlock].uTotalZeros = 16;
            }

            // find advanced intra prediction block, store in PredBuf
            // Select best AI mode for the block, using reconstructed
            // predictor pels. This function also stores the block
            // predictor pels at pPredBuf.
            if (!curr_slice->m_use_transform_for_intra_decision &&
                !pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))
            {
                uIntraSAD += H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectOneBlock)(
                    state,
                    curr_slice,
                    pSrcPlane + uOffset,
                    pReconBuf,
                    uBlock,
                    cur_mb.intra_types,
                    pPredBuf);
            }

            // check if block is coded
            bCoded = ((uCBPLuma & CBP4x4Mask[uBlock])?(1):(0));

            if (!bCoded && !doIntraPred)
            {
                // update reconstruct frame for the empty block
                Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
                if (core_enc->m_svc_layer.isActive) {
                    ippsZero_16s(pPredTCoeffsTmp, 16);
//                    ippsZero_16s(cur_mb.m_pTCoeffsTmp + block_subblock_mapping[uBlock] * 16, 16);
                }
            }
            else
            {
                // block not declared empty, encode
                // compute difference of predictor and source pels
                // note: asm version does not use pDiffBuf
                //       output is being passed in the mmx registers

// we have everything calculated and stored for baseMode  - optimize this !!! ??? kta
                if ((!curr_slice->m_use_transform_for_intra_decision || core_enc->m_info.quant_opt_level > OPT_QUANT_INTRA4x4 ) ||
                    (pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)) || doIntraPred)
                {
                    if(!transform_bypass)
                    {
                        if (!curr_slice->doSCoeffsPrediction)
                        {
                            if (((core_enc->m_info.quant_opt_level > OPT_QUANT_INTRA4x4) &&
                                (!pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))) || curr_slice->doTCoeffsPrediction || doIntraPred)
                            {
                                PIXTYPE PredPel[13];
                                //We need to calculate new prediction
                                H264ENC_MAKE_NAME(H264CoreEncoder_GetBlockPredPels)(state, curr_slice, pReconBuf, pitchPixels, pReconBuf, pitchPixels, pReconBuf, pitchPixels, uBlock, PredPel);
                                H264ENC_MAKE_NAME(H264CoreEncoder_GetPredBlock)(cur_mb.intra_types[uBlock], pPredBuf, PredPel);
                                Diff4x4(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);

                                H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                                    pDiffBuf,
                                    pTransformResult,
                                    uMBQP,
                                    &iNumCoeffs,
                                    1, //Always use f for INTRA
                                    enc_single_scan[is_cur_mb_field],
                                    &iLastCoeff,
                                    NULL,
                                    curr_slice,
                                    0,
                                    &cbSt);
                            }
                            else
                            {
                                Diff4x4(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);
                                H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                                    pDiffBuf,
                                    pTransformResult,
                                    uMBQP,
                                    &iNumCoeffs,
                                    1, //Always use f for INTRA
                                    enc_single_scan[is_cur_mb_field],
                                    &iLastCoeff,
                                    NULL,
                                    NULL,
                                    0,
                                    NULL);
                            }
                        } else {
                            Ipp32s i;

                            Diff4x4(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);

                            for (i = 0; i < 16; i++)
                            {
                                tmpCoeffsBuff[i] = ((pPredSCoeffsTmp[i] * normTable4x4[i]) >> 8);
                            }

                            if (core_enc->m_info.quant_opt_level > OPT_QUANT_INTER)
                            {
                                H264ENC_MAKE_NAME(ippiTransformSubQuantResidual_H264)(pDiffBuf,
                                    tmpCoeffsBuff,
                                    pTransformResult,
                                    uMBQP,
                                    &iNumCoeffs,
                                    0,
                                    enc_single_scan[is_cur_mb_field],
                                    &iLastCoeff,
                                    NULL,
                                    curr_slice,
                                    0,
                                    &cbSt);
                            }
                            else
                            {
                                H264ENC_MAKE_NAME(ippiTransformSubQuantResidual_H264)(pDiffBuf,
                                    tmpCoeffsBuff,
                                    pTransformResult,
                                    uMBQP,
                                    &iNumCoeffs,
                                    0,
                                    enc_single_scan[is_cur_mb_field],
                                    &iLastCoeff,
                                    NULL,
                                    NULL,
                                    0,
                                    NULL);
                            }
                        }
                    }
                    else
                    {
                        Diff4x4(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);

                        for(Ipp32s i = 0; i < 16; i++) {
                            pTransformResult[i] = pDiffBuf[i];
                        }
                        ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan[is_cur_mb_field],&iLastCoeff, 16);
                    }
                }else
                {
                    iNumCoeffs = cur_mb.m_iNumCoeffs4x4[ uBlock ];
                    iLastCoeff = cur_mb.m_iLastCoeff4x4[ uBlock ];
                    pTransformResult = &cur_mb.mb4x4.transform[ uBlock*16 ];
                }

                if (curr_slice->doTCoeffsPrediction)
                {
                    ippsSub_16s_I(pPredTCoeffsTmp, pTransformResult, 16);
                    ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan[is_cur_mb_field],&iLastCoeff, 16);
                }
                if (core_enc->m_svc_layer.isActive &&
                    (curr_slice->m_scan_idx_start != 0 || curr_slice->m_scan_idx_end != 15)) {
                        ScanIdxZero(pTransformResult, curr_slice->m_scan_idx_start, curr_slice->m_scan_idx_end+1, dec_single_scan[is_cur_mb_field], 16);
                        ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan[is_cur_mb_field],&iLastCoeff, 16);
                }

//                if (core_enc->m_svc_layer.isActive)
//                    ippsCopy_16s(pTransformResult, cur_mb.m_pTCoeffsTmp + block_subblock_mapping[uBlock] * 16, 16);

                // if everything quantized to zero, skip RLE
                if (!iNumCoeffs)
                {
                    // the block is empty so it is not coded
                    bCoded = 0;
                }
                else
                {
                    // Preserve the absolute number of coeffs.
                    if (core_enc->m_PicParamSet->entropy_coding_mode)
                    {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iNumCoeffs);

                        CabacBlock4x4* c_data = &cur_mb.cabac_data->lumaBlock4x4[uBlock];
                        c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                        c_data->uLastSignificant = iLastCoeff;
                        c_data->CtxBlockCat = BLOCK_LUMA_LEVELS;
                        c_data->uFirstCoeff = 0;
                        c_data->uLastCoeff = 15;
                        H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                            pTransformResult,
                            dec_single_scan[is_cur_mb_field],
                            c_data);
                        bCoded = c_data->uNumSigCoeffs;
                    } else {
                    // record RLE info
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iNumCoeffs);
                        H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(
                            pTransformResult,
                            curr_slice->m_scan_idx_start,
                            dec_single_scan[is_cur_mb_field],
                            iLastCoeff,
                            &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
                            &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
                            &curr_slice->Block_RLE[uBlock].uNumCoeffs,
                            &curr_slice->Block_RLE[uBlock].uTotalZeros,
                            curr_slice->Block_RLE[uBlock].iLevels,
                            curr_slice->Block_RLE[uBlock].uRuns);
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = bCoded = curr_slice->Block_RLE[uBlock].uNumCoeffs;
                    }
                }

                if (core_enc->m_svc_layer.isActive) {
                    if (curr_slice->doTCoeffsPrediction)
                    {
                        ippsAdd_16s_I(pTransformResult, pPredTCoeffsTmp, 16);
                        //                CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);
                    }
                    else
                    {
                        ippsCopy_16s(pTransformResult, pPredTCoeffsTmp, 16);
                    }

                    if (!transform_bypass)
                    {
                        ippiDequantResidual4x4_H264_16s(pPredTCoeffsTmp, tmpCoeffsBuff, uMBQP);

                        if (curr_slice->doSCoeffsPrediction)
                        {
                            ippsAdd_16s_I(tmpCoeffsBuff, pPredSCoeffsTmp, 16);
                            //                    CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);
                        } else {
                            ippsCopy_16s(tmpCoeffsBuff, pPredSCoeffsTmp, 16);
                        }
                    }
                }

                // update flags if block quantized to empty
                if ((curr_slice->m_use_transform_for_intra_decision && core_enc->m_info.quant_opt_level < OPT_QUANT_INTRA4x4+1 ) &&
                   (!pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)) && !doIntraPred)
                {
                    if (!bCoded) {
                        uCBPLuma &= ~CBP4x4Mask[uBlock]; //Copy prediction
                        Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
                    } else {//Copy reconstruct
                        Copy4x4(pPredBuf + 256, 16, pReconBuf, pitchPix);
                    }
                } else {
                    if (!bCoded)
                        uCBPLuma &= ~CBP4x4Mask[uBlock];
                    if (!bCoded && !pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)){
                        // update reconstruct frame for the empty block
                        Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
                    } else if(!transform_bypass) {
                        if (core_enc->m_svc_layer.isActive) {
                            ippsCopy_16s(pPredSCoeffsTmp, tmpCoeffsBuff, 16);
                            ippiTransformResidualAndAdd_H264_16s8u_C1I(pPredBuf,16,
                                tmpCoeffsBuff,
                                pReconBuf,
                                pitchPix);
                        } else {
                            H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264)(
                                pPredBuf,
                                pTransformResult,
                                NULL,
                                pReconBuf,
                                16,
                                pitchPix,
                                uMBQP,
                                ((iNumCoeffs < -1) || (iNumCoeffs > 0)),
                                core_enc->m_PicParamSet->bit_depth_luma,
                                NULL);
                        }
                    }
                }
            }

            // proceed to the next block
            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
        }
    }

    if (core_enc->m_svc_layer.isActive && pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo) &&
        pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo) && !uCBPLuma && core_enc->m_PicParamSet->transform_8x8_mode_flag) {
        Ipp8u BaseMB8x8TSFlag = pGetMB8x8TSBaseFlag(cur_mb.GlobalMacroblockInfo);
        if (!core_enc->m_spatial_resolution_change)
        {
            pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, BaseMB8x8TSFlag);
        } else {
            pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, 0);
        }
        if(!pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo)) {
            uIntraSAD   = 0;
            uTotalCoeffs = 0;
            uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];

            for (uBlock = 0; uBlock < 16; uBlock++)
            {
                COEFFSTYPE *pPredSCoeffsTmp = pPredSCoeffs;
                COEFFSTYPE *pPredTCoeffsTmp = pPredTCoeffs;

                pPredSCoeffsTmp += block_subblock_mapping[uBlock] * 16;
                pPredTCoeffsTmp += block_subblock_mapping[uBlock] * 16;

                pPredBuf = cur_mb.mb4x4.prediction + xoff[uBlock] + yoff[uBlock]*16;
                pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;

                if (pGetMBIBLFlag(cur_mb.GlobalMacroblockInfo)) {
                    pRefLayerPixels = core_enc->m_pYInputRefPlane + uOffset;
                    Copy4x4(pRefLayerPixels, pitchPix, pPredBuf, 16);
                }

                if ((!curr_slice->m_use_transform_for_intra_decision) &&
                    (!pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)))
                {
                    uIntraSAD += H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectOneBlock)(
                        state,
                        curr_slice,
                        pSrcPlane + uOffset,
                        pReconBuf,
                        uBlock,
                        cur_mb.intra_types,
                        pPredBuf);
                }

                ippsCopy_16s(pPredSCoeffsTmp, tmpCoeffsBuff, 16);
                ippiTransformResidualAndAdd_H264_16s8u_C1I(pPredBuf,16,
                    tmpCoeffsBuff,
                    pReconBuf,
                    pitchPix);

                // proceed to the next block
                uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
            }
        }
    }

    if (core_enc->m_svc_layer.isActive && !pGetMBIBLFlag(cur_mb.GlobalMacroblockInfo))
    {
        uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        Copy16x16(core_enc->m_pReconstructFrame->m_pYPlane + uOffset, cur_mb.mbPitchPixels,
                  core_enc->m_pYInputRefPlane + uOffset, cur_mb.mbPitchPixels);
    }

    // update coded block flags
    cur_mb.LocalMacroblockInfo->cbp_luma = uCBPLuma;

    // for each block of the MB initialize the AI mode (for INTER MB)
    // or motion vectors (for INTRA MB) to values which will be
    // correct predictors of that type. MV and AI mode prediction
    // depend upon this instead of checking MB type.

    return 1;
}   // CEncAndRec4x4IntraMB

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec4x4IntraMBRec)(
    void* state,
    H264SliceType *curr_slice)
{
    __ALIGN16 COEFFSTYPE tmpCoeffsBuff[64];
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;     // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    Ipp32u  uMB;
    Ipp32u  uCBPLuma;        // coded flags for all 4x4 blocks

    COEFFSTYPE *pPredSCoeffs, *pPredTCoeffs;
    PIXTYPE*  pPredBuf;       // prediction block pointer
    PIXTYPE*  pReconBuf;       // prediction block pointer
    Ipp16s*   pDiffBuf;       // difference block pointer
    PIXTYPE*  pSrcPlane;      // start of plane to encode
    Ipp32s    pitchPixels;     // buffer pitch
    Ipp8u     bCoded; // coded block flag

    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;

    uMB = cur_mb.uMB;
    pPredSCoeffs = &(core_enc->m_SavedMacroblockCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);
    pPredTCoeffs = &(core_enc->m_SavedMacroblockTCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);

    pitchPixels = cur_mb.mbPitchPixels;
    uCBPLuma     = cur_mb.LocalMacroblockInfo->cbp_luma;
    uMBQP       = cur_mb.lumaQP;
    pDiffBuf    = (Ipp16s*) (curr_slice->m_pMBEncodeBuffer + 512);

    //--------------------------------------------------------------------------
    // encode Y plane blocks (0-15)
    //--------------------------------------------------------------------------

    // initialize pointers and offset
    pSrcPlane = core_enc->m_pCurrentFrame->m_pYPlane;
    uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    bool transform_bypass = core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag && uMBQP == 0;

    Ipp32s pitchPix;
    pitchPix = pitchPixels;

    if(pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo))
    {
        pSetMB8x8TSFlag(cur_mb.GlobalMacroblockInfo, 1);

        //loop over all 8x8 blocks in Y plane for the MB
        for (uBlock = 0; uBlock < 4; uBlock ++)
        {
            pPredBuf = cur_mb.mb8x8.prediction + xoff[4*uBlock] + yoff[4*uBlock]*16;
            pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;

            // check if block is coded
            bCoded = ((uCBPLuma & CBP8x8Mask[uBlock])?(1):(0));

            {   // block not declared empty, encode
                // compute difference of predictor and source pels
                // note: asm version does not use pDiffBuf
                //       output is being passed in the mmx registers
                if (!curr_slice->m_use_transform_for_intra_decision || (core_enc->m_info.quant_opt_level > OPT_QUANT_INTRA8x8) ||
                    (pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)))
                {
                    if (!curr_slice->doSCoeffsPrediction)
                    {
                        if (((core_enc->m_info.quant_opt_level > OPT_QUANT_INTRA8x8) &&
                            (!pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))) || curr_slice->doTCoeffsPrediction)
                        {
                            __ALIGN16 PIXTYPE pred[64];
                            PIXTYPE pred_pels[25]; //Sources for prediction
                            Ipp32u pred_pels_mask = 0;
                            Ipp32s i;
                            bool top_avbl;
                            bool left_avbl;
                            bool left_above_avbl = curr_slice->m_cur_mb.IntraNeighbours.mb_D >= 0;
                            bool right_above_avbl = curr_slice->m_cur_mb.IntraNeighbours.mb_C >= 0;

                            if( uBlock & 0x2 ){
                                top_avbl = true;
                            }else{
                                top_avbl = curr_slice->m_cur_mb.IntraNeighbours.mb_B >= 0;
                            }

                            if( uBlock & 0x1 ){
                                left_avbl = true;
                            }else{
                                left_avbl = curr_slice->m_cur_mb.IntraNeighbours.mb_A >= 0;
                            }

                            //Copy pels
                            //TOP
                            if( top_avbl ){
                                for( i=0; i<8; i++ )
                                    pred_pels[1+i] = *(pReconBuf-pitchPixels+i);
                                pred_pels_mask |= 0x000001fe;
                            }

                            //LEFT
                            if( left_avbl ){
                                for( i=0; i<8; i++ )
                                    pred_pels[17+i] = *(pReconBuf+i*pitchPixels-1);
                                pred_pels_mask |= 0x1fe0000;
                            }

                            //LEFT_ABOVE
                            if( (uBlock == 0 && left_above_avbl) || uBlock == 3 ||
                                (uBlock == 1 && top_avbl) || ( uBlock == 2 && left_avbl)){
                                    pred_pels[0] = *(pReconBuf-pitchPixels-1);
                                    pred_pels_mask |= 0x01;
                            }

                            //RIGHT_ABOVE
                            if( (uBlock == 2) || (uBlock == 0 && top_avbl) ||
                                (uBlock == 1 && right_above_avbl) ){
                                    for( i=0; i<8; i++ )
                                        pred_pels[9+i] = *(pReconBuf-pitchPixels+i+8);
                                    pred_pels_mask |= 0x0001fe00;
                            }

                            if( !((pred_pels_mask & 0x0001FE00)==0x0001FE00) && (pred_pels_mask & 0x00000100) ){
                                pred_pels_mask |= 0x0001FE00;
                                for( i=0; i<8; i++ ) pred_pels[9+i] = pred_pels[1+7];
                            }

                            H264ENC_MAKE_NAME(H264CoreEncoder_Filter8x8Pels)(pred_pels, pred_pels_mask);
                            H264ENC_MAKE_NAME(H264CoreEncoder_GetPrediction8x8)(state, cur_mb.intra_types[uBlock<<2], pred_pels, pred_pels_mask, pred );
                            PIXTYPE* p = pPredBuf;
                            for( i=0; i<8; i++){
                                MFX_INTERNAL_CPY(p, &pred[i*8], 8*sizeof(PIXTYPE));
                                p += 16; //pitch = 16
                            }
                        }
                    }
                }

                if (core_enc->m_svc_layer.isActive) {
                    ippsCopy_16s(pPredTCoeffs + uBlock*64, tmpCoeffsBuff, 64);
                    H264ENC_MAKE_NAME(ippiQuantLuma8x8Inv_H264)(tmpCoeffsBuff, QP_DIV_6[uMBQP], core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[0][QP_MOD_6[uMBQP]]);

                    if (curr_slice->doSCoeffsPrediction)
                    {
                        ippsAdd_16s_I(tmpCoeffsBuff, pPredSCoeffs + uBlock*64, 64);
                        //                    CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);
                    } else {
                        ippsCopy_16s(tmpCoeffsBuff, pPredSCoeffs + uBlock*64, 64);
                    }

                    ippsCopy_16s(pPredSCoeffs + uBlock*64, tmpCoeffsBuff, 64);
                    ippiTransformResidualAndAdd8x8_H264_16s8u_C1I(pPredBuf,
                        tmpCoeffsBuff,
                        pReconBuf,
                        16,
                        pitchPix);
                }
            } //curr_slice->m_use_transform_for_intra_decision

            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock] * 2;
        }  // for uBlock in luma plane
    }
    else
    {
        // loop over all 4x4 blocks in Y plane for the MB

        for (uBlock = 0; uBlock < 16; uBlock++)
        {
            COEFFSTYPE *pPredSCoeffsTmp = pPredSCoeffs;
            COEFFSTYPE *pPredTCoeffsTmp = pPredTCoeffs;

            pPredSCoeffsTmp += block_subblock_mapping[uBlock] * 16;
            pPredTCoeffsTmp += block_subblock_mapping[uBlock] * 16;

            pPredBuf = cur_mb.mb4x4.prediction + xoff[uBlock] + yoff[uBlock]*16;
            pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;

            // check if block is coded
            bCoded = ((uCBPLuma & CBP4x4Mask[uBlock])?(1):(0));
            if ((!curr_slice->m_use_transform_for_intra_decision || core_enc->m_info.quant_opt_level > OPT_QUANT_INTRA4x4 ) ||
                (pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)))
            {
                if(!transform_bypass)
                {
                    if (!curr_slice->doSCoeffsPrediction)
                    {
                        if (((core_enc->m_info.quant_opt_level > OPT_QUANT_INTRA4x4) &&
                            (!pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))) || curr_slice->doTCoeffsPrediction)
                        {
                            PIXTYPE PredPel[13];
                            //We need to calculate new prediction
                            H264ENC_MAKE_NAME(H264CoreEncoder_GetBlockPredPels)(state, curr_slice, pReconBuf, pitchPixels, pReconBuf, pitchPixels, pReconBuf, pitchPixels, uBlock, PredPel);
                            H264ENC_MAKE_NAME(H264CoreEncoder_GetPredBlock)(cur_mb.intra_types[uBlock], pPredBuf, PredPel);
                        }
                    }
                }
            }

            {
                if (core_enc->m_svc_layer.isActive) {
                    if (!transform_bypass)
                    {
                        ippiDequantResidual4x4_H264_16s(pPredTCoeffsTmp, tmpCoeffsBuff, uMBQP);

                        if (curr_slice->doSCoeffsPrediction)
                        {
                            ippsAdd_16s_I(tmpCoeffsBuff, pPredSCoeffsTmp, 16);
                            //                    CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);
                        } else {
                            ippsCopy_16s(tmpCoeffsBuff, pPredSCoeffsTmp, 16);
                        }
                    }

                    ippsCopy_16s(pPredSCoeffsTmp, tmpCoeffsBuff, 16);
                    ippiTransformResidualAndAdd_H264_16s8u_C1I(pPredBuf,16,
                        tmpCoeffsBuff,
                        pReconBuf,
                        pitchPix);
                }
            }

            // proceed to the next block
            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
        }
    }

    if (core_enc->m_svc_layer.isActive && !pGetMBIBLFlag(cur_mb.GlobalMacroblockInfo))
    {
        uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        Copy16x16(core_enc->m_pReconstructFrame->m_pYPlane + uOffset, cur_mb.mbPitchPixels,
                  core_enc->m_pYInputRefPlane + uOffset, cur_mb.mbPitchPixels);
    }

    return 1;
}   // CEncAndRec4x4IntraMB


////////////////////////////////////////////////////////////////////////////////
// CEncAndRec16x16IntraMB
//
// Encode and Reconstruct all blocks in one 16x16 Intra macroblock
//
////////////////////////////////////////////////////////////////////////////////

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec16x16IntraMB)(
    void* state,
    H264SliceType *curr_slice)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;     // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    Ipp32u  uMB;
    Ipp32u  uCBPLuma;        // coded flags for all 4x4 blocks
    //Ipp32u  uCBPChroma;        // coded flags for all chroma blocks
    //Ipp32u  uIntraSAD;      // intra MB SAD
    COEFFSTYPE* pDCBuf;     // chroma & luma dc coeffs pointer
    COEFFSTYPE* pTempBuf;     // temp buffer to store DC block for check on overflow of CAVLC "level_prefix"
    PIXTYPE*  pPredBuf;       // prediction block pointer
    PIXTYPE*  pReconBuf;       // prediction block pointer
    Ipp16s* pDiffBuf;       // difference block pointer
    Ipp16s* pTempDiffBuf;       // difference block pointer
    COEFFSTYPE *pTransformResult; // for transform results.
    Ipp16s* pMassDiffBuf;   // difference block pointer
    COEFFSTYPE* pQBuf;          // quantized block pointer
    PIXTYPE*  pSrcPlane;      // start of plane to encode
    Ipp32s  pitchPixels;     // buffer pitch in pixels
    Ipp8u   bCoded; // coded block flag
    Ipp32s  iNumCoeffs = 0; // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s  iLastCoeff = 0; // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32u  RLE_Offset;    // Index into BlockRLE array
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    COEFFSTYPE *pPredTCoeffs;

    pitchPixels = cur_mb.mbPitchPixels;
    uCBPLuma    = cur_mb.LocalMacroblockInfo->cbp_luma;
    //uCBPChroma  = cur_mb.LocalMacroblockInfo->cbp_chroma;
    uMBQP       = cur_mb.lumaQP;
    pDiffBuf    = (Ipp16s*) (curr_slice->m_pMBEncodeBuffer + 512);
    pTransformResult = (COEFFSTYPE*)(pDiffBuf + 16);
    pQBuf       = (COEFFSTYPE*) (pTransformResult + 16);
    pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    pMassDiffBuf = (Ipp16s*) (pDCBuf + 16);
    pTempBuf = (COEFFSTYPE*)(pMassDiffBuf + 256);

    __ALIGN16 Ipp16s pDCBuf_save[16];
//  uIntraSAD   = rd_quant_intra[uMBQP] * 24;   // 'handicap' using reconstructed data
    //uIntraSAD   = 0;
    uMB = cur_mb.uMB;

    pPredTCoeffs = &(core_enc->m_SavedMacroblockTCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);

    bool transform_bypass = core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag && uMBQP == 0;
    //--------------------------------------------------------------------------
    // encode Y plane blocks (0-15)
    //--------------------------------------------------------------------------

    // initialize pointers and offset
    pSrcPlane = core_enc->m_pCurrentFrame->m_pYPlane;
    uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    RLE_Offset = Y_DC_RLE;  // Used in 16x16 Intra mode only
    pPredBuf    = cur_mb.mb16x16.prediction; // 16-byte aligned work buffer
    Ipp32s pitchPix;
    pReconBuf    = core_enc->m_pReconstructFrame->m_pYPlane; // 16-byte aligned work buffer
    pitchPix = pitchPixels;

    // for INTRA 16x16 MBs computation of luma prediction was done as
    // a byproduct of sad calculation prior to this function being
    // called; the predictor blocks are already at pPredBuf.

    // Initialize the AC coeff flag value
    cur_mb.MacroblockCoeffsInfo->lumaAC = 0;


    if (curr_slice->doTCoeffsPrediction) {
        __ALIGN16 PIXTYPE uPred[256*4];
        bool topAvailable = curr_slice->m_cur_mb.IntraNeighbours.mb_B >= 0;
        bool leftAvailable = curr_slice->m_cur_mb.IntraNeighbours.mb_A >= 0;
        bool left_above_aval = curr_slice->m_cur_mb.IntraNeighbours.mb_D >= 0;

        /* to do: need to implement function which calculates only 1 predictions instead of 4 */
        H264ENC_MAKE_NAME(PredictIntraLuma16x16)(pReconBuf + uOffset, pitchPixels, uPred, core_enc->m_PicParamSet->bit_depth_luma, leftAvailable, topAvailable, left_above_aval);
        assert(cur_mb.LocalMacroblockInfo->intra_16x16_mode < 4);
        MFX_INTERNAL_CPY(pPredBuf, uPred + 256 * cur_mb.LocalMacroblockInfo->intra_16x16_mode, 256*sizeof(PIXTYPE));
    }

    // compute the 4x4 luma DC transform coeffs and AC residuals
    H264ENC_MAKE_NAME(ippiSumsDiff16x16Blocks4x4)(pSrcPlane + uOffset, pitchPixels, pPredBuf, 16, pDCBuf, pMassDiffBuf);

    if (curr_slice->m_scan_idx_start == 0) {
        if(!transform_bypass) {
            // apply second transform on the luma DC transform coeffs
            // check for CAVLC "level_prefix" overflow for baseline and main profiles (see A.2.1, A.2.2)
            if( (core_enc->m_info.profile_idc == H264_BASE_PROFILE || core_enc->m_info.profile_idc == H264_MAIN_PROFILE) &&
                !core_enc->m_info.entropy_coding_mode ){
                bool CAVLC_overflow;
                Ipp32s i;
                MFX_INTERNAL_CPY(pTempBuf, pDCBuf, 16 * sizeof(COEFFSTYPE)); // store DC block
                do{
                    H264ENC_MAKE_NAME(ippiTransformQuantLumaDC_H264)(
                        pDCBuf,
                        pQBuf,
                        uMBQP,
                        &iNumCoeffs,
                        1,
                        enc_single_scan[is_cur_mb_field],
                        &iLastCoeff,
                        NULL);
                    CAVLC_overflow = false;
                    for(i=0; i<16; i++)
                        if( pDCBuf[i] > MAX_CAVLC_LEVEL || pDCBuf[i] < (1 - MAX_CAVLC_LEVEL) ){ CAVLC_overflow = true; break; }
                    if( CAVLC_overflow ){
                        cur_mb.LocalMacroblockInfo->QP++;
                        uMBQP = cur_mb.lumaQP = getLumaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                        cur_mb.chromaQP = getChromaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->chroma_qp_index_offset, core_enc->m_SeqParamSet->bit_depth_chroma);
                        MFX_INTERNAL_CPY(pDCBuf, pTempBuf, 16 * sizeof(COEFFSTYPE)); // load DC block
                    }
                }while(CAVLC_overflow);
            }else
                H264ENC_MAKE_NAME(ippiTransformQuantLumaDC_H264)(
                    pDCBuf,
                    pQBuf,
                    uMBQP,
                    &iNumCoeffs,
                    1,
                    enc_single_scan[is_cur_mb_field],
                    &iLastCoeff,
                    NULL);

        }else {
           for(Ipp32s i = 0; i < 4; i++) {
                for(Ipp32s j = 0; j < 4; j++) {
                    Ipp32s x, y;
                    x = j*16;
                    y = i*64;
                    pDCBuf[i*4 + j] = pMassDiffBuf[x+y];
                }
            }
            ippiCountCoeffs(pDCBuf, &iNumCoeffs, enc_single_scan[is_cur_mb_field], &iLastCoeff, 16);
        }
    } else {
        for(uBlock = 0; uBlock < 16; uBlock++) 
            pDCBuf[uBlock] = pPredTCoeffs[uBlock*16];
    }

    // insert the quantized luma Ipp64f transform DC coeffs into
    // RLE buffer

    if (!curr_slice->doTCoeffsPrediction && curr_slice->m_scan_idx_start == 0) {
        // record RLE info
        if (core_enc->m_PicParamSet->entropy_coding_mode)
        {
            CabacBlock4x4* c_data = &cur_mb.cabac_data->dcBlock[Y_DC_RLE - U_DC_RLE];
            c_data->uNumSigCoeffs = bCoded = ABS(iNumCoeffs);
            c_data->uLastSignificant = iLastCoeff;
            c_data->CtxBlockCat = BLOCK_LUMA_DC_LEVELS;
            c_data->uFirstCoeff = 0;
            c_data->uLastCoeff = 15;
            H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                pDCBuf,
                dec_single_scan[is_cur_mb_field],
                c_data);
        }
        else
        {
            H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(
                pDCBuf,
                0,
                dec_single_scan[is_cur_mb_field],
                iLastCoeff,
                &curr_slice->Block_RLE[RLE_Offset].uTrailing_Ones,
                &curr_slice->Block_RLE[RLE_Offset].uTrailing_One_Signs,
                &curr_slice->Block_RLE[RLE_Offset].uNumCoeffs,
                &curr_slice->Block_RLE[RLE_Offset].uTotalZeros,
                curr_slice->Block_RLE[RLE_Offset].iLevels,
                curr_slice->Block_RLE[RLE_Offset].uRuns);

            bCoded = curr_slice->Block_RLE[RLE_Offset].uNumCoeffs;
        }
    }

    if (core_enc->m_svc_layer.isActive)
        ippsCopy_16s(pDCBuf, pDCBuf_save, 16);

    if(!transform_bypass) {
        H264ENC_MAKE_NAME(ippiTransformDequantLumaDC_H264)(pDCBuf, uMBQP, NULL);
    }

    TrellisCabacState cbSt;
    if (core_enc->m_info.quant_opt_level > OPT_QUANT_INTRA16x16)
    {
        H264BsBase_CopyContextCABAC_Trellis16x16(&cbSt, curr_slice->m_pbitstream, !is_cur_mb_field);
    }

    // loop over all 4x4 blocks in Y plane for the MB
    for (uBlock = 0; uBlock < 16; uBlock++ ){
        COEFFSTYPE *pPredTCoeffsTmp = pPredTCoeffs;

        pPredTCoeffsTmp += block_subblock_mapping[uBlock] * 16;

        pPredBuf = cur_mb.mb16x16.prediction + xoff[uBlock] + yoff[uBlock]*16;
        pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane+uOffset;
        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;        // This will be updated if the block is coded
        if (core_enc->m_PicParamSet->entropy_coding_mode)
        {
            cur_mb.cabac_data->lumaBlock4x4[uBlock].uNumSigCoeffs = 0;
        }
        else
        {
            curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
            curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
            curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
            curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
        }

        // check if block is coded
        bCoded = ((uCBPLuma & CBP4x4Mask[uBlock])?(1):(0));

        if (!bCoded){
            // update reconstruct frame for the empty block
            Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
        }else{   // block not declared empty, encode

            pTempDiffBuf = pMassDiffBuf+ xoff[uBlock]*4 + yoff[uBlock]*16;
            if(!transform_bypass) {
                if( core_enc->m_info.quant_opt_level > OPT_QUANT_INTRA16x16 ){
                    H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                        pTempDiffBuf,
                        pTransformResult,
                        uMBQP,
                        &iNumCoeffs,
                        1, //Always use f for INTRA
                        enc_single_scan[is_cur_mb_field],
                        &iLastCoeff,
                        NULL,
                        curr_slice,
                        1,
                        &cbSt);
                }else{
                    H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                        pTempDiffBuf,
                        pTransformResult,
                        uMBQP,
                        &iNumCoeffs,
                        1, //Always use f for INTRA
                        enc_single_scan[is_cur_mb_field],
                        &iLastCoeff,
                        NULL,
                        NULL,
                        0,
                        NULL);
                }

                if (curr_slice->doTCoeffsPrediction)
                {
                    pTransformResult[0] = pDCBuf_save[block_subblock_mapping[uBlock]];
                    ippsSub_16s_I(pPredTCoeffsTmp, pTransformResult, 16);
                    ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan[is_cur_mb_field], &iLastCoeff, 16);
                }
            }else{
                for(Ipp32s i = 0; i < 16; i++){
                    pTransformResult[i] = pTempDiffBuf[i];
                }
                ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan[is_cur_mb_field], &iLastCoeff, 16);
            }
            if (core_enc->m_svc_layer.isActive && (curr_slice->m_scan_idx_start != 0 || curr_slice->m_scan_idx_end != 15)) {
                ScanIdxZero(pTransformResult, curr_slice->m_scan_idx_start, curr_slice->m_scan_idx_end+1, dec_single_scan[is_cur_mb_field], 16);
                ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan[is_cur_mb_field], &iLastCoeff, 16);
            }

//            if (core_enc->m_svc_layer.isActive)
//                ippsCopy_16s(pTransformResult, cur_mb.m_pTCoeffsTmp + block_subblock_mapping[uBlock] * 16, 16);

            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = curr_slice->doTCoeffsPrediction ?
                (T_NumCoeffs)ABS(iNumCoeffs) : (T_NumCoeffs)((iNumCoeffs < 0) ? -(iNumCoeffs+1) : iNumCoeffs);
            if( cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] ){
                cur_mb.MacroblockCoeffsInfo->lumaAC |= 1;
                // Preserve the absolute number of coeffs.
                if (core_enc->m_PicParamSet->entropy_coding_mode)
                {
                    CabacBlock4x4* c_data = &cur_mb.cabac_data->lumaBlock4x4[uBlock];
                    c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];;
                    c_data->uLastSignificant = iLastCoeff;
                    c_data->CtxBlockCat = curr_slice->doTCoeffsPrediction ? BLOCK_LUMA_LEVELS : BLOCK_LUMA_AC_LEVELS;
                    c_data->uFirstCoeff = curr_slice->doTCoeffsPrediction ? 0 : 1;
                    c_data->uLastCoeff = 15;
                    H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                        pTransformResult,
                        dec_single_scan[is_cur_mb_field],
                        c_data);

                    bCoded = c_data->uNumSigCoeffs;
                } else {
                    Ipp32s uFirstCoeff = curr_slice->doTCoeffsPrediction ? 0 : 1;
                    if (uFirstCoeff < curr_slice->m_scan_idx_start)
                        uFirstCoeff = curr_slice->m_scan_idx_start;

                    H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(
                        pTransformResult,//pDiffBuf,
                        uFirstCoeff,
                        dec_single_scan[is_cur_mb_field],
                        iLastCoeff,
                        &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
                        &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
                        &curr_slice->Block_RLE[uBlock].uNumCoeffs,
                        &curr_slice->Block_RLE[uBlock].uTotalZeros,
                        curr_slice->Block_RLE[uBlock].iLevels,
                        curr_slice->Block_RLE[uBlock].uRuns);

                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = bCoded = curr_slice->Block_RLE[uBlock].uNumCoeffs;
                }
            }else{
//               bCoded = 0;
               uCBPLuma &= ~CBP4x4Mask[uBlock];
            }

            if (core_enc->m_svc_layer.isActive) {
                if (curr_slice->doTCoeffsPrediction)
                {
                    ippsAdd_16s_I(pPredTCoeffsTmp, pTransformResult, 16);
                    ippsCopy_16s(pTransformResult, pPredTCoeffsTmp, 16);
                    pTransformResult[0] = 0;
                    ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan[is_cur_mb_field], &iLastCoeff, 16);
                }
                else
                {
                    pPredTCoeffsTmp[0] = pDCBuf_save[block_subblock_mapping[uBlock]];
                    ippsCopy_16s(pTransformResult + 1, pPredTCoeffsTmp + 1, 15);
                }
            }

            // If the block wasn't coded and the DC coefficient is zero
            if (!bCoded && !pDCBuf[block_subblock_mapping[uBlock]]){
                // update reconstruct frame for the empty block
                Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
            } else if(!transform_bypass) {
                // inverse transform for reconstruct AND...
                // add inverse transformed coefficients to original predictor
                // to obtain reconstructed block, store in reconstruct frame
                // buffer
                H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264) (
                    pPredBuf,
                    pTransformResult,
                    &pDCBuf[block_subblock_mapping[uBlock]],
                    pReconBuf,
                    16,
                    pitchPix,
                    uMBQP,
                    ((iNumCoeffs < (curr_slice->doTCoeffsPrediction ? 0 : -1)) || (iNumCoeffs > 0)),
                    core_enc->m_PicParamSet->bit_depth_luma,
                    NULL);
            }
        }   // block not declared empty

        // proceed to the next block
        uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
    }  // for uBlock in luma plane

    //{
    //    Ipp32s i;
    //    pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;
    //    PIXTYPE* pRefLayerPixels = core_enc->m_pYInputRefPlane + uOffset;

    //    for (i = 0; i < 16; i++)
    //    {
    //        MFX_INTERNAL_CPY(pRefLayerPixels, pReconBuf, sizeof(PIXTYPE)*16);
    //        pReconBuf += pitchPixels;
    //        pRefLayerPixels += pitchPixels;
    //    }
    //} FIXED by the block below
    if (core_enc->m_svc_layer.isActive && !pGetMBIBLFlag(cur_mb.GlobalMacroblockInfo))
    {
        uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        Copy16x16(core_enc->m_pReconstructFrame->m_pYPlane + uOffset, cur_mb.mbPitchPixels,
            core_enc->m_pYInputRefPlane + uOffset, cur_mb.mbPitchPixels);
    }

    // In JVT, Chroma is Intra if any part of luma is intra.
    // update coded block flags
    cur_mb.LocalMacroblockInfo->cbp_luma = uCBPLuma;

    // Correct the value of nc if both chroma DC and AC coeffs will be coded.
    if (cur_mb.MacroblockCoeffsInfo->lumaAC > 1)
        cur_mb.MacroblockCoeffsInfo->lumaAC = 1;

    // for each block of the MB initialize the AI mode (for INTER MB)
    // or motion vectors (for INTRA MB) to values which will be
    // correct predictors of that type. MV and AI mode prediction
    // depend upon this instead of checking MB type.

    cur_mb.intra_types[0] = cur_mb.LocalMacroblockInfo->intra_16x16_mode;
    for (Ipp32s i=1; i<16; i++)
    {
        cur_mb.intra_types[i] = 2;
    }

    return 1;

}   // CEncAndRec16x16IntraMB

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec16x16IntraMBRec)(
    void* state,
    H264SliceType *curr_slice)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;     // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    Ipp32u  uMB;
    Ipp32u  uCBPLuma;        // coded flags for all 4x4 blocks
    COEFFSTYPE* pDCBuf;     // chroma & luma dc coeffs pointer
    COEFFSTYPE* pTempBuf;     // temp buffer to store DC block for check on overflow of CAVLC "level_prefix"
    PIXTYPE*  pPredBuf;       // prediction block pointer
    PIXTYPE*  pReconBuf;       // prediction block pointer
    Ipp16s* pDiffBuf;       // difference block pointer
    COEFFSTYPE *pTransformResult; // for transform results.
    Ipp16s* pMassDiffBuf;   // difference block pointer
    COEFFSTYPE* pQBuf;          // quantized block pointer
    PIXTYPE*  pSrcPlane;      // start of plane to encode
    Ipp32s  pitchPixels;     // buffer pitch in pixels
    Ipp32s  iNumCoeffs = 0; // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s  iLastCoeff; // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32u  RLE_Offset;    // Index into BlockRLE array
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    COEFFSTYPE *pPredTCoeffs;

    pitchPixels = cur_mb.mbPitchPixels;
    uCBPLuma    = cur_mb.LocalMacroblockInfo->cbp_luma;
    //uCBPChroma  = cur_mb.LocalMacroblockInfo->cbp_chroma;
    uMBQP       = cur_mb.lumaQP;
    pDiffBuf    = (Ipp16s*) (curr_slice->m_pMBEncodeBuffer + 512);
    pTransformResult = (COEFFSTYPE*)(pDiffBuf + 16);
    pQBuf       = (COEFFSTYPE*) (pTransformResult + 16);
    pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    pMassDiffBuf = (Ipp16s*) (pDCBuf + 16);
    pTempBuf = (COEFFSTYPE*)(pMassDiffBuf + 256);

    uMB = cur_mb.uMB;

    pPredTCoeffs = &(core_enc->m_SavedMacroblockTCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);

    bool transform_bypass = core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag && uMBQP == 0;
    //--------------------------------------------------------------------------
    // encode Y plane blocks (0-15)
    //--------------------------------------------------------------------------

    // initialize pointers and offset
    pSrcPlane = core_enc->m_pCurrentFrame->m_pYPlane;
    uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    RLE_Offset = Y_DC_RLE;  // Used in 16x16 Intra mode only
    pPredBuf    = cur_mb.mb16x16.prediction; // 16-byte aligned work buffer
    Ipp32s pitchPix;
    pReconBuf    = core_enc->m_pReconstructFrame->m_pYPlane; // 16-byte aligned work buffer
    pitchPix = pitchPixels;

    // for INTRA 16x16 MBs computation of luma prediction was done as
    // a byproduct of sad calculation prior to this function being
    // called; the predictor blocks are already at pPredBuf.

    // Initialize the AC coeff flag value
    cur_mb.MacroblockCoeffsInfo->lumaAC = 0;

    if (curr_slice->doTCoeffsPrediction) {
        __ALIGN16 PIXTYPE uPred[256*4];
        bool topAvailable = curr_slice->m_cur_mb.IntraNeighbours.mb_B >= 0;
        bool leftAvailable = curr_slice->m_cur_mb.IntraNeighbours.mb_A >= 0;
        bool left_above_aval = curr_slice->m_cur_mb.IntraNeighbours.mb_D >= 0;

        /* to do: need to implement function which calculates only 1 predictions instead of 4 */
        H264ENC_MAKE_NAME(PredictIntraLuma16x16)(pReconBuf + uOffset, pitchPixels, uPred, core_enc->m_PicParamSet->bit_depth_luma, leftAvailable, topAvailable, left_above_aval);
        assert(cur_mb.LocalMacroblockInfo->intra_16x16_mode < 4);
        MFX_INTERNAL_CPY(pPredBuf, uPred + 256 * cur_mb.LocalMacroblockInfo->intra_16x16_mode, 256*sizeof(PIXTYPE));
    }
/*
    if (core_enc->m_svc_layer.isActive)
        ippsCopy_16s(pDCBuf, pDCBuf_save, 16);*/
    for (uBlock = 0; uBlock < 16; uBlock ++) {
        pDCBuf[block_subblock_mapping[uBlock]] = pPredTCoeffs[block_subblock_mapping[uBlock] * 16];
//        + cur_mb.m_pTCoeffsTmp[block_subblock_mapping[uBlock] * 16];
    }

    if(!transform_bypass) {
        H264ENC_MAKE_NAME(ippiTransformDequantLumaDC_H264)(pDCBuf, uMBQP, NULL);
    }

    // loop over all 4x4 blocks in Y plane for the MB
    for (uBlock = 0; uBlock < 16; uBlock++ ){
        COEFFSTYPE *pPredTCoeffsTmp = pPredTCoeffs;

        pPredTCoeffsTmp += block_subblock_mapping[uBlock] * 16;

        pPredBuf = cur_mb.mb16x16.prediction + xoff[uBlock] + yoff[uBlock]*16;
        pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane+uOffset;

        if (core_enc->m_svc_layer.isActive) {
            ippsCopy_16s(pPredTCoeffsTmp, pTransformResult, 16);
            pTransformResult[0] = 0;
            ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan[is_cur_mb_field], &iLastCoeff, 16);
        }

        if(!transform_bypass) {
            // inverse transform for reconstruct AND...
            // add inverse transformed coefficients to original predictor
            // to obtain reconstructed block, store in reconstruct frame
            // buffer
            H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264) (
                pPredBuf,
                pTransformResult,
                &pDCBuf[block_subblock_mapping[uBlock]],
                pReconBuf,
                16,
                pitchPix,
                uMBQP,
                ((iNumCoeffs < (curr_slice->doTCoeffsPrediction ? 0 : -1)) || (iNumCoeffs > 0)),
                core_enc->m_PicParamSet->bit_depth_luma,
                NULL);
        }

        // proceed to the next block
        uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
    }  // for uBlock in luma plane

    if (core_enc->m_svc_layer.isActive && !pGetMBIBLFlag(cur_mb.GlobalMacroblockInfo))
    {
        uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        Copy16x16(core_enc->m_pReconstructFrame->m_pYPlane + uOffset, cur_mb.mbPitchPixels,
            core_enc->m_pYInputRefPlane + uOffset, cur_mb.mbPitchPixels);
    }

    return 1;

}   // CEncAndRec16x16IntraMB

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRecInterMB)(
    void* state,
    H264SliceType *curr_slice)
{
    __ALIGN16 COEFFSTYPE tmpCoeffsBuff[64];
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;     // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    //Ipp32u  uMBType;        // current MB type
    Ipp32u  uMB;
    Ipp32u  uCBPLuma;        // coded flags for all 4x4 blocks

    //COEFFSTYPE* pDCBuf;     // chroma & luma dc coeffs pointer
    PIXTYPE*  pPredBuf;       // prediction block pointer
    PIXTYPE*  pReconBuf;       // prediction block pointer
    Ipp16s* pDiffBuf;       // difference block pointer
    COEFFSTYPE *pTransform; // result of the transform.
    COEFFSTYPE *pTransformResult; // result of the transform.
    COEFFSTYPE *pPredSCoeffs, *pPredTCoeffs;
    COEFFSTYPE *pResidualBuf;
    COEFFSTYPE *pRefPredResiduals;
    //COEFFSTYPE* pQBuf;          // quantized block pointer
    //Ipp16s* pMassDiffBuf;   // difference block pointer
    PIXTYPE*  pSrcPlane;      // start of plane to encode
    Ipp32s    pitchPixels;     // buffer pitch in pixels
    Ipp8u     bCoded;        // coded block flag
    Ipp32s    iNumCoeffs;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s    iLastCoeff;  // Number of nonzero coeffs after quant (negative if DC is nonzero)
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    bool allow_subblock_skip = !(pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo) ||
            curr_slice->doTCoeffsPrediction ||
            curr_slice->doSCoeffsPrediction ||
            curr_slice->m_scan_idx_start != 0 ||
            curr_slice->m_scan_idx_end != 15);

    uMBQP       = cur_mb.lumaQP;
    bool transform_bypass = core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag && uMBQP == 0;
    __ALIGN16 TrellisCabacState cbSt;

    uCBPLuma    = cur_mb.LocalMacroblockInfo->cbp_luma;
    pitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels<<is_cur_mb_field;
    //uMBType     = cur_mb.GlobalMacroblockInfo->mbtype;
    pTransform  = (COEFFSTYPE*)curr_slice->m_pMBEncodeBuffer;
    pDiffBuf    = (Ipp16s*) (curr_slice->m_pMBEncodeBuffer + 512);
    //pQBuf       = (COEFFSTYPE*) (pDiffBuf+64);
    //pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    //pMassDiffBuf= (Ipp16s*) (pDCBuf+ 16);
    uMB=cur_mb.uMB;
    pPredSCoeffs = &(core_enc->m_SavedMacroblockCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);
    pPredTCoeffs = &(core_enc->m_SavedMacroblockTCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);

    //--------------------------------------------------------------------------
    // encode Y plane blocks (0-15)
    //--------------------------------------------------------------------------

    Ipp32s pitchPix;
    pitchPix = pitchPixels;

    // initialize pointers and offset
    pSrcPlane = core_enc->m_pCurrentFrame->m_pYPlane;
    uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    pPredBuf = cur_mb.mbInter.prediction;

    H264MotionVector mv_f[16], *pmv_f;
    H264MotionVector mv_b[16], *pmv_b;
    if (core_enc->m_svc_layer.isActive) {
        pmv_f = mv_f;
        pmv_b = mv_b;
        truncateMVs(state, curr_slice, cur_mb.MVs[LIST_0]->MotionVectors, mv_f);
        if (curr_slice->m_slice_type == BPREDSLICE)
            truncateMVs(state, curr_slice, cur_mb.MVs[LIST_1]->MotionVectors, mv_b);
    } else {
        pmv_f = cur_mb.MVs[LIST_0]->MotionVectors;
        pmv_b = cur_mb.MVs[LIST_1]->MotionVectors;
    }

    if (curr_slice->doMCDirectForSkipped)
        cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_DIRECT; //for correct MC

    // Motion Compensate this MB
    H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBLuma)(state, curr_slice, pmv_f, pmv_b, pPredBuf);

    if (curr_slice->m_slice_type == BPREDSLICE && core_enc->m_svc_layer.isActive && !core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag)
        //pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))
        MC_bidirMB_luma( state, curr_slice, mv_f, mv_b, pPredBuf );

    if (curr_slice->doMCDirectForSkipped)
        cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;

    if (/*!refMbAff &&*/ !core_enc->m_SliceHeader.MbaffFrameFlag && pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)
        //&& !sd->m_mbinfo.layerMbs[pFrame->m_currentLayer].restricted_spatial_resolution_change_flag
        && core_enc->m_spatial_resolution_change) {
        //RestoreIntraPrediction - copy intra predicted parts
        Ipp32s i,j;
        for (i=0; i<2; i++) {
            for (j=0; j<2; j++) {
                if (!cur_mb.GlobalMacroblockInfo->i_pred[2*j+i]) continue;
                Ipp32s offx = core_enc->svc_ResizeMap[0][cur_mb.uMBx].border;
                Ipp32s offy = core_enc->svc_ResizeMap[1][cur_mb.uMBy].border;
                IppiSize roiSize = {i?16-offx:offx, j?16-offy:offy};
                if(!i) offx = 0;
                if(!j) offy = 0;

                ippiCopy_8u_C1R(core_enc->m_pReconstructFrame->m_pYPlane +
                    (cur_mb.uMBy*16 + offy) * core_enc->m_pReconstructFrame->m_pitchPixels + cur_mb.uMBx*16 + offx,
                    core_enc->m_pReconstructFrame->m_pitchPixels,
                    pPredBuf + offy*16 + offx,
                    16,
                    roiSize);
            }
        }
    }

    for (uBlock = 0; uBlock < 16; uBlock++)
        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;

    if (core_enc->m_PicParamSet->entropy_coding_mode)
    {
        // These will be updated if the block is coded
        if (pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo))
            for (uBlock = 0; uBlock < 4; uBlock++)
                cur_mb.cabac_data->lumaBlock8x8[uBlock].uNumSigCoeffs = 0;
        else
            for (uBlock = 0; uBlock < 16; uBlock++)
                cur_mb.cabac_data->lumaBlock4x4[uBlock].uNumSigCoeffs = 0;
    }
    else
    {
        for (uBlock = 0; uBlock < 16; uBlock++)
        {
            // These will be updated if the block is coded
            curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
            curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
            curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
            curr_slice->Block_RLE[uBlock].uTotalZeros = 16;
        }
    }

    if (pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo))
    {
        Ipp32s mbCost = 0;

        if (core_enc->m_info.quant_opt_level > OPT_QUANT_INTER)
        {
            H264BsBase_CopyContextCABAC_Trellis8x8(&cbSt, curr_slice->m_pbitstream, !is_cur_mb_field);
        }

        pSetMB8x8TSFlag(cur_mb.GlobalMacroblockInfo, 1);
        //loop over all 8x8 blocks in Y plane for the MB
        Ipp32s coeffCost = 0;
        for (uBlock = 0; uBlock < 4; uBlock++)
        {
            pResidualBuf = core_enc->m_pYResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16 +
                xoff[uBlock*4] + yoff[uBlock*4] * core_enc->m_WidthInMBs * 16;
            pRefPredResiduals = (pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)?core_enc->m_pYInputBaseResidual:core_enc->m_pYInputResidual) +
                cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16 + xoff[uBlock*4] + yoff[uBlock*4] * core_enc->m_WidthInMBs * 16;
            pPredBuf = cur_mb.mbInter.prediction + xoff[uBlock*4] + yoff[uBlock*4]*16;
            // check if block is coded
            bCoded = ((uCBPLuma & CBP8x8Mask[uBlock])?(1):(0));

            if (bCoded)
            {
                Diff8x8(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);
                if (core_enc->m_svc_layer.isActive)
                    Diff8x8Residual(pRefPredResiduals, core_enc->m_WidthInMBs * 16, pDiffBuf, 8, pDiffBuf, 8);

                pTransformResult = pTransform + uBlock*64;
                if (!transform_bypass)
                {
                    // forward transform and quantization, in place in pDiffBuf
                    H264ENC_MAKE_NAME(ippiTransformLuma8x8Fwd_H264)(pDiffBuf, pTransformResult);

                    if (curr_slice->doSCoeffsPrediction)
                    {
                        Ipp32s i;
                        for (i = 0; i < 64; i++)
                        {
                            pTransformResult[i] -= ((pPredSCoeffs[uBlock*64+i] * normTable8x8[i]) >> 11);
                        }
                    }

                    if (core_enc->m_info.quant_opt_level > OPT_QUANT_INTER)
                    {
                        H264ENC_MAKE_NAME(ippiQuantLuma8x8_H264)(pTransformResult, pTransformResult, uMBQP, 0,
                            enc_single_scan_8x8[is_cur_mb_field], core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[1][QP_MOD_6[uMBQP]], // INTER scaling matrix
                            &iNumCoeffs, &iLastCoeff,curr_slice,&cbSt,core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[1][QP_MOD_6[uMBQP]]);
                    }
                    else
                    {
                        H264ENC_MAKE_NAME(ippiQuantLuma8x8_H264)(
                            pTransformResult,
                            pTransformResult,
                            QP_DIV_6[uMBQP],
                            0,
                            enc_single_scan_8x8[is_cur_mb_field],
                            core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[1][QP_MOD_6[uMBQP]], // INTER scaling matrix
                            &iNumCoeffs,
                            &iLastCoeff,
                            NULL,
                            NULL,
                            NULL);
                    }

                    if (curr_slice->doTCoeffsPrediction)
                    {
                        ippsSub_16s_I(pPredTCoeffs + uBlock*64, pTransformResult, 64);
                        ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan_8x8[is_cur_mb_field], &iLastCoeff, 64);
                    }

                    coeffCost = iNumCoeffs
                        ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 64, dec_single_scan_8x8[is_cur_mb_field])
                        : 0;
                    mbCost += coeffCost;
                }
                else
                {
                    for (Ipp32s i = 0; i < 64; i++) {
                        pTransformResult[i] = pDiffBuf[i];
                    }
                    ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan_8x8[is_cur_mb_field], &iLastCoeff, 64);
                }

                if (core_enc->m_svc_layer.isActive &&
                    (curr_slice->m_scan_idx_start != 0 || curr_slice->m_scan_idx_end != 15)) {
                        ScanIdxZero(pTransformResult, curr_slice->m_scan_idx_start*4, curr_slice->m_scan_idx_end*4+4, dec_single_scan_8x8[is_cur_mb_field], 64);
                        ippiCountCoeffs(pTransformResult, &iNumCoeffs, enc_single_scan_8x8[is_cur_mb_field], &iLastCoeff, 64);
                }

                // if everything quantized to zero, skip RLE
                if (!iNumCoeffs || (!transform_bypass && allow_subblock_skip && 
                    coeffCost < LUMA_COEFF_8X8_MAX_COST && core_enc->m_info.quant_opt_level < OPT_QUANT_INTER+1))
                {
                    uCBPLuma &= ~CBP8x8Mask[uBlock];
                }
                else
                {
                    // record RLE info
                    if (core_enc->m_PicParamSet->entropy_coding_mode)
                    {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iNumCoeffs);

                        CabacBlock8x8* c_data = &cur_mb.cabac_data->lumaBlock8x8[uBlock];
                        c_data->uLastSignificant = iLastCoeff;
                        c_data->CtxBlockCat = BLOCK_LUMA_64_LEVELS;
                        c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                        c_data->uFirstCoeff = 0;
                        c_data->uLastCoeff = 63;
                        H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                            pTransformResult,
                            dec_single_scan_8x8[is_cur_mb_field],
                            c_data);
                    }
                    else
                    {
                        COEFFSTYPE buf4x4[4][16];
                        Ipp32s i4x4;

                        //Reorder 8x8 block for coding with CAVLC
                        for(i4x4=0; i4x4<4; i4x4++ ) {
                            Ipp32s i;
                            for(i = 0; i<16; i++ )
                                buf4x4[i4x4][dec_single_scan[is_cur_mb_field][i]] =
                                    pTransformResult[dec_single_scan_8x8[is_cur_mb_field][4*i+i4x4]];
                        }

                        Ipp32s idx = uBlock*4;
                        //Encode each block with CAVLC 4x4
                        for(i4x4 = 0; i4x4<4; i4x4++, idx++ ) {
                            Ipp32s i;
                            iLastCoeff = 0;

                            //Check for last coeff
                            for(i = 0; i<16; i++ ) if( buf4x4[i4x4][dec_single_scan[is_cur_mb_field][i]] != 0 ) iLastCoeff=i;

                            H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(
                                buf4x4[i4x4],
                                curr_slice->m_scan_idx_start, //Luma
                                dec_single_scan[is_cur_mb_field],
                                iLastCoeff,
                                &curr_slice->Block_RLE[idx].uTrailing_Ones,
                                &curr_slice->Block_RLE[idx].uTrailing_One_Signs,
                                &curr_slice->Block_RLE[idx].uNumCoeffs,
                                &curr_slice->Block_RLE[idx].uTotalZeros,
                                curr_slice->Block_RLE[idx].iLevels,
                                curr_slice->Block_RLE[idx].uRuns);

                            cur_mb.MacroblockCoeffsInfo->numCoeff[idx] = curr_slice->Block_RLE[idx].uNumCoeffs;
                         }
                    }
                }
            }

            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock] * 2;
        }

        if (!transform_bypass && allow_subblock_skip && mbCost < LUMA_COEFF_MB_8X8_MAX_COST)
        {
            uCBPLuma = 0;
        }

        uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        for (uBlock = 0; uBlock < 4; uBlock++)
        {
            bCoded = ((uCBPLuma & CBP8x8Mask[uBlock])?(1):(0));

            if (core_enc->m_svc_layer.isActive) {
                if (!bCoded) {
                    memset(pTransform + uBlock*64, 0, 64*sizeof(COEFFSTYPE));
                    if (core_enc->m_PicParamSet->entropy_coding_mode)
                    {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
                    }
                    else
                    {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock*4+0] =
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock*4+1] =
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock*4+2] =
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock*4+3] = 0;
                    }
                }

                if (curr_slice->doTCoeffsPrediction)
                {
                    ippsAdd_16s_I(pTransform + uBlock*64, pPredTCoeffs + uBlock*64, 64);
                    //                CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);
                }
                else
                {
                    ippsCopy_16s(pTransform + uBlock*64, pPredTCoeffs + uBlock*64, 64);
                }

                if (!transform_bypass)
                {
                    ippsCopy_16s(pPredTCoeffs + uBlock*64, tmpCoeffsBuff, 64);
                    H264ENC_MAKE_NAME(ippiQuantLuma8x8Inv_H264)(tmpCoeffsBuff, QP_DIV_6[uMBQP], core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[1][QP_MOD_6[uMBQP]]); //scaling matrix for INTER slice

                    if (curr_slice->doSCoeffsPrediction)
                    {
                        ippsAdd_16s_I(tmpCoeffsBuff, pPredSCoeffs + uBlock*64, 64);
                        //                    DoAddCoeffs(TransformPixels, pRefLayerSCoeffsTmp, qNumCoeffs, BlockSize, InvTransformMode);
                        //                    CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);
                    } else {
                        ippsCopy_16s(tmpCoeffsBuff, pPredSCoeffs + uBlock*64, 64);
                    }
                }
            } else { // AVC
                pPredBuf = cur_mb.mbInter.prediction + xoff[uBlock*4] + yoff[uBlock*4]*16;
                pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;

                if (!bCoded)
                {
                    Copy8x8(pPredBuf, 16, pReconBuf, pitchPix);
                    if (core_enc->m_PicParamSet->entropy_coding_mode)
                    {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
                    }
                    else
                    {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock*4+0] =
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock*4+1] =
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock*4+2] =
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock*4+3] = 0;
                    }
                }
                else if(!transform_bypass)
                {
                    H264ENC_MAKE_NAME(ippiQuantLuma8x8Inv_H264)(pTransform + uBlock*64, QP_DIV_6[uMBQP], core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[1][QP_MOD_6[uMBQP]]); //scaling matrix for INTER slice
                    H264ENC_MAKE_NAME(ippiTransformLuma8x8InvAddPred_H264)(pPredBuf, 16, pTransform + uBlock*64, pReconBuf, pitchPix, core_enc->m_PicParamSet->bit_depth_luma);
                }
            }
            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock] * 2;
        }
    }
    else
    {
        //loop over all 4x4 blocks in Y plane for the MB
        //first make transform for all blocks
        if (core_enc->m_info.quant_opt_level > OPT_QUANT_INTER)
        {
            H264BsBase_CopyContextCABAC_Trellis4x4(&cbSt, curr_slice->m_pbitstream, !is_cur_mb_field);
        }

        Ipp32s iaNumCoeffs[16], CoeffsCost[16] = { 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9 };
        for (uBlock = 0; uBlock < 16; uBlock++)
        {
            COEFFSTYPE *pPredSCoeffsTmp = pPredSCoeffs;
            COEFFSTYPE *pPredTCoeffsTmp = pPredTCoeffs;

            pPredSCoeffsTmp += block_subblock_mapping[uBlock] * 16;
            pPredTCoeffsTmp += block_subblock_mapping[uBlock] * 16;

            pPredBuf = cur_mb.mbInter.prediction + xoff[uBlock] + yoff[uBlock]*16;
            pResidualBuf = core_enc->m_pYResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16 +
                xoff[uBlock] + yoff[uBlock] * core_enc->m_WidthInMBs * 16;
            pRefPredResiduals = (pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)?core_enc->m_pYInputBaseResidual:core_enc->m_pYInputResidual) +
                cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16 +
                xoff[uBlock] + yoff[uBlock] * core_enc->m_WidthInMBs * 16;

            // check if block is coded
            bCoded = ((uCBPLuma & CBP4x4Mask[uBlock])?(1):(0));

            if (bCoded)
            {
                // block not declared empty, encode
                Diff4x4(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);

                if (core_enc->m_svc_layer.isActive)
                    Diff4x4Residual(pRefPredResiduals, core_enc->m_WidthInMBs * 16, pDiffBuf, 4, pDiffBuf, 4);

                pTransformResult = pTransform + uBlock*16;
                if (!transform_bypass)
                {
                    if (!curr_slice->doSCoeffsPrediction)
                    {
                        // forward transform and quantization, in place in pDiffBuf
                        if (core_enc->m_info.quant_opt_level > OPT_QUANT_INTER)
                        {
                            H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(pDiffBuf,
                                                                               pTransformResult,
                                                                               uMBQP,
                                                                               &iaNumCoeffs[uBlock],
                                                                               0,
                                                                               enc_single_scan[is_cur_mb_field],
                                                                               &iLastCoeff,
                                                                               NULL,
                                                                               curr_slice,
                                                                               0,
                                                                               &cbSt);
                        }
                        else
                        {
                            H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(pDiffBuf,
                                                                               pTransformResult,
                                                                               uMBQP,
                                                                               &iaNumCoeffs[uBlock],
                                                                               0,
                                                                               enc_single_scan[is_cur_mb_field],
                                                                               &iLastCoeff,
                                                                               NULL,
                                                                               NULL,
                                                                               0,
                                                                               NULL);
                        }
                    }
                    else
                    {
                        __ALIGN16 COEFFSTYPE tmpCoeff[16];
                        Ipp32s i;
                        for (i = 0; i < 16; i++)
                        {
                            tmpCoeff[i] = ((pPredSCoeffsTmp[i] * normTable4x4[i]) >> 8);
                        }


                        if (core_enc->m_info.quant_opt_level > OPT_QUANT_INTER)
                        {
                            H264ENC_MAKE_NAME(ippiTransformSubQuantResidual_H264)(pDiffBuf,
                                                                                  tmpCoeff,
                                                                                  pTransformResult,
                                                                                  uMBQP,
                                                                                  &iaNumCoeffs[uBlock],
                                                                                  0,
                                                                                  enc_single_scan[is_cur_mb_field],
                                                                                  &iLastCoeff,
                                                                                  NULL,
                                                                                  curr_slice,
                                                                                  0,
                                                                                  &cbSt);
                        }
                        else
                        {
                            H264ENC_MAKE_NAME(ippiTransformSubQuantResidual_H264)(pDiffBuf,
                                                                                  tmpCoeff,
                                                                                  pTransformResult,
                                                                                  uMBQP,
                                                                                  &iaNumCoeffs[uBlock],
                                                                                  0,
                                                                                  enc_single_scan[is_cur_mb_field],
                                                                                  &iLastCoeff,
                                                                                  NULL,
                                                                                  NULL,
                                                                                  0,
                                                                                  NULL);
                        }

                    }


                    if (curr_slice->doTCoeffsPrediction)
                    {
                        ippsSub_16s_I(pPredTCoeffsTmp, pTransformResult, 16);
                        ippiCountCoeffs(pTransformResult, &iaNumCoeffs[uBlock], enc_single_scan[is_cur_mb_field], &iLastCoeff, 16);
                    }

                    CoeffsCost[uBlock] = iaNumCoeffs[uBlock]
                        ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 16, dec_single_scan[is_cur_mb_field])
                        : 0;
                }
                else
                {
                    for(Ipp32s i = 0; i < 16; i++) {
                        pTransformResult[i] = pDiffBuf[i];
                    }

                    ippiCountCoeffs(pTransformResult, &iaNumCoeffs[uBlock], enc_single_scan[is_cur_mb_field], &iLastCoeff, 16);
                }

                if (core_enc->m_svc_layer.isActive &&
                    (curr_slice->m_scan_idx_start != 0 || curr_slice->m_scan_idx_end != 15)) {
                        ScanIdxZero(pTransformResult, curr_slice->m_scan_idx_start, curr_slice->m_scan_idx_end+1, dec_single_scan[is_cur_mb_field], 16);
                        ippiCountCoeffs(pTransformResult, &iaNumCoeffs[uBlock], enc_single_scan[is_cur_mb_field],&iLastCoeff, 16);
                }

                if (!iaNumCoeffs[uBlock])
                {
                    // if everything quantized to zero, skip RLE
                    uCBPLuma &= ~CBP4x4Mask[uBlock];
                }
                else
                {
                    // Preserve the absolute number of coeffs.
                    if (core_enc->m_PicParamSet->entropy_coding_mode)
                    {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iaNumCoeffs[uBlock]);

                        CabacBlock4x4* c_data = &cur_mb.cabac_data->lumaBlock4x4[uBlock];
                        c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                        c_data->uLastSignificant = iLastCoeff;
                        c_data->CtxBlockCat = BLOCK_LUMA_LEVELS;
                        c_data->uFirstCoeff = 0;
                        c_data->uLastCoeff = 15;
                        H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                            pTransformResult,
                            dec_single_scan[is_cur_mb_field],
                            c_data);
                    }
                    else
                    {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iaNumCoeffs[uBlock]);
                        H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(
                            pTransformResult,
                            curr_slice->m_scan_idx_start,
                            dec_single_scan[is_cur_mb_field],
                            iLastCoeff,
                            &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
                            &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
                            &curr_slice->Block_RLE[uBlock].uNumCoeffs,
                            &curr_slice->Block_RLE[uBlock].uTotalZeros,
                            curr_slice->Block_RLE[uBlock].iLevels,
                            curr_slice->Block_RLE[uBlock].uRuns);
                    }
                }
            }

            // proceed to the next block
            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
        }  // for 4x4 uBlock in luma plane

        //Skip subblock 8x8 if it cost is < 4 or skip MB if it's cost is < 5
        if (!transform_bypass && allow_subblock_skip)
        {
            Ipp32s mbCost = 0;
            for (uBlock = 0; uBlock < 4; uBlock++)
            {
                Ipp32s sb = uBlock*4;
                Ipp32s block8x8cost = CoeffsCost[sb] + CoeffsCost[sb+1] + CoeffsCost[sb+2] + CoeffsCost[sb+3];

                mbCost += block8x8cost;
                if (block8x8cost <= LUMA_8X8_MAX_COST && core_enc->m_info.quant_opt_level < OPT_QUANT_INTER + 1)
                    uCBPLuma &= ~CBP8x8Mask[uBlock];
            }

            if (mbCost <= LUMA_MB_MAX_COST)
                uCBPLuma = 0;
        }

        //Make inverse quantization and transform for non zero blocks
        uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        for (uBlock = 0; uBlock < 16; uBlock++)
        {
            bCoded = ((uCBPLuma & CBP4x4Mask[uBlock])?(1):(0));

            if (core_enc->m_svc_layer.isActive) {
                COEFFSTYPE *pPredSCoeffsTmp = pPredSCoeffs;
                COEFFSTYPE *pPredTCoeffsTmp = pPredTCoeffs;
                pPredSCoeffsTmp += block_subblock_mapping[uBlock] * 16;
                pPredTCoeffsTmp += block_subblock_mapping[uBlock] * 16;

                if (!bCoded) {
                    memset(pTransform + uBlock*16, 0, 16*sizeof(COEFFSTYPE));
                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
                }

                if (curr_slice->doTCoeffsPrediction)
                {
                    ippsAdd_16s_I(pTransform + uBlock*16, pPredTCoeffsTmp, 16);
    //                CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);

                }
                else
                {
                    ippsCopy_16s(pTransform + uBlock*16, pPredTCoeffsTmp, 16);
                }

                if (!transform_bypass)
                {
                    ippiDequantResidual4x4_H264_16s(pPredTCoeffsTmp, tmpCoeffsBuff, uMBQP);

                    if (curr_slice->doSCoeffsPrediction)
                    {
                        ippsAdd_16s_I(tmpCoeffsBuff, pPredSCoeffsTmp, 16);
    //                    CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);
                    } else {
                        ippsCopy_16s(tmpCoeffsBuff, pPredSCoeffsTmp, 16);
                    }
                }
            } else {
                pPredBuf = cur_mb.mbInter.prediction + xoff[uBlock] + yoff[uBlock]*16;
                pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;
                if (!bCoded)
                {
                    // update reconstruct frame for the empty block
                    Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
                }
                else if (!transform_bypass)
                {
                    H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264) (
                        pPredBuf,
                        pTransform + uBlock*16,
                        NULL,
                        pReconBuf,
                        16,
                        pitchPix,
                        uMBQP,
                        ((iaNumCoeffs[uBlock] < -1) || (iaNumCoeffs[uBlock] > 0)),
                        core_enc->m_PicParamSet->bit_depth_luma,
                        NULL);
                }
            }
            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
        }
    }

    if (core_enc->m_svc_layer.isActive && !uCBPLuma &&
        !core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag &&
        core_enc->m_PicParamSet->transform_8x8_mode_flag) {
        Ipp8u BaseMB8x8TSFlag = pGetMB8x8TSBaseFlag(cur_mb.GlobalMacroblockInfo);
        if (!core_enc->m_spatial_resolution_change &&
            pGetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo))
        {
            pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, BaseMB8x8TSFlag);
        } else {
            pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, 0);
        }
    }

    if (core_enc->m_svc_layer.isActive) {
        //pPredBuf = cur_mb.mbInter.prediction + xoff[uBlock] + yoff[uBlock]*16;
        uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;

        pResidualBuf = core_enc->m_pYResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
        if(pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))
            pRefPredResiduals = core_enc->m_pYInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
        else
            pRefPredResiduals = core_enc->m_pYInputResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;

        if (pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo)) {
            ReconstructLumaBaseModeResidual_8x8(pPredSCoeffs, pRefPredResiduals,
                pResidualBuf, 0,
                core_enc->m_WidthInMBs * 16);
        } else {
            ReconstructLumaBaseModeResidual(pPredSCoeffs, pRefPredResiduals,
                pResidualBuf, 0,
                core_enc->m_WidthInMBs * 16);
        }

        PIXTYPE *dst = pReconBuf;
        Ipp16s *src1 = pResidualBuf;
        Ipp8u *src2 = cur_mb.mbInter.prediction;
        int ii, jj;
        for (ii = 0; ii < 16; ii++) {
            for (jj = 0; jj < 16; jj++) {
                Ipp16s tmp = src1[jj] + src2[jj];
                tmp = IPP_MIN(tmp, 255);
                tmp = IPP_MAX(tmp, 0);
                dst[jj] = tmp;
            }
            dst += pitchPix;
            src1 += core_enc->m_WidthInMBs * 16;
            src2 += 16;
        }
    }
    //--------------------------------------------------------------------------
    // encode U plane blocks then V plane blocks
    //--------------------------------------------------------------------------

    // update coded block flags
    cur_mb.LocalMacroblockInfo->cbp_luma = uCBPLuma;

    return 1;
}

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRecInterMBRec)(
    void* state,
    H264SliceType *curr_slice)
{
    __ALIGN16 COEFFSTYPE tmpCoeffsBuff[64];
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;     // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    Ipp32u  uMB;
    Ipp32u  uCBPLuma;        // coded flags for all 4x4 blocks

    PIXTYPE*  pPredBuf;       // prediction block pointer
    PIXTYPE*  pReconBuf;       // prediction block pointer
    Ipp16s* pDiffBuf;       // difference block pointer
    COEFFSTYPE *pPredSCoeffs, *pPredTCoeffs;
    COEFFSTYPE *pResidualBuf;
    COEFFSTYPE *pRefPredResiduals;
    PIXTYPE*  pSrcPlane;      // start of plane to encode
    Ipp32s    pitchPixels;     // buffer pitch in pixels
    Ipp8u     bCoded;        // coded block flag
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;

    uMBQP       = cur_mb.lumaQP;
    bool transform_bypass = core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag && uMBQP == 0;

    uCBPLuma    = cur_mb.LocalMacroblockInfo->cbp_luma;
    pitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels<<is_cur_mb_field;
    pDiffBuf    = (Ipp16s*) (curr_slice->m_pMBEncodeBuffer + 512);
    uMB=cur_mb.uMB;
    pPredSCoeffs = &(core_enc->m_SavedMacroblockCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);
    pPredTCoeffs = &(core_enc->m_SavedMacroblockTCoeffs[uMB*COEFFICIENTS_BUFFER_SIZE]);

    //--------------------------------------------------------------------------
    // encode Y plane blocks (0-15)
    //--------------------------------------------------------------------------

    Ipp32s pitchPix;
    pitchPix = pitchPixels;

    // initialize pointers and offset
    pSrcPlane = core_enc->m_pCurrentFrame->m_pYPlane;
    uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    pPredBuf = cur_mb.mbInter.prediction;

    H264MotionVector mv_f[16], *pmv_f;
    H264MotionVector mv_b[16], *pmv_b;
    if (core_enc->m_svc_layer.isActive) {
        pmv_f = mv_f;
        pmv_b = mv_b;
        truncateMVs(state, curr_slice, cur_mb.MVs[LIST_0]->MotionVectors, mv_f);
        if (curr_slice->m_slice_type == BPREDSLICE)
            truncateMVs(state, curr_slice, cur_mb.MVs[LIST_1]->MotionVectors, mv_b);
    } else {
        pmv_f = cur_mb.MVs[LIST_0]->MotionVectors;
        pmv_b = cur_mb.MVs[LIST_1]->MotionVectors;
    }

    if (curr_slice->doMCDirectForSkipped)
        cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_DIRECT; //for correct MC

    // Motion Compensate this MB
    H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBLuma)(state, curr_slice, pmv_f, pmv_b, pPredBuf);

    if (curr_slice->m_slice_type == BPREDSLICE && core_enc->m_svc_layer.isActive && !core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag)
        //pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))
        MC_bidirMB_luma( state, curr_slice, mv_f, mv_b, pPredBuf );

    if (curr_slice->doMCDirectForSkipped)
        cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;

    if (/*!refMbAff &&*/ !core_enc->m_SliceHeader.MbaffFrameFlag && pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)
        //&& !sd->m_mbinfo.layerMbs[pFrame->m_currentLayer].restricted_spatial_resolution_change_flag
        && core_enc->m_spatial_resolution_change) {
        //RestoreIntraPrediction - copy intra predicted parts
        Ipp32s i,j;
        for (i=0; i<2; i++) {
            for (j=0; j<2; j++) {
                if (!cur_mb.GlobalMacroblockInfo->i_pred[2*j+i]) continue;
                Ipp32s offx = core_enc->svc_ResizeMap[0][cur_mb.uMBx].border;
                Ipp32s offy = core_enc->svc_ResizeMap[1][cur_mb.uMBy].border;
                IppiSize roiSize = {i?16-offx:offx, j?16-offy:offy};
                if(!i) offx = 0;
                if(!j) offy = 0;

                ippiCopy_8u_C1R(core_enc->m_pReconstructFrame->m_pYPlane +
                    (cur_mb.uMBy*16 + offy) * core_enc->m_pReconstructFrame->m_pitchPixels + cur_mb.uMBx*16 + offx,
                    core_enc->m_pReconstructFrame->m_pitchPixels,
                    pPredBuf + offy*16 + offx,
                    16,
                    roiSize);
            }
        }
    }

    if (pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo))
    {
        pSetMB8x8TSFlag(cur_mb.GlobalMacroblockInfo, 1);
        //loop over all 8x8 blocks in Y plane for the MB

        uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        for (uBlock = 0; uBlock < 4; uBlock++)
        {
            bCoded = ((uCBPLuma & CBP8x8Mask[uBlock])?(1):(0));

            if (core_enc->m_svc_layer.isActive) {
                if (!transform_bypass)
                {
                    ippsCopy_16s(pPredTCoeffs + uBlock*64, tmpCoeffsBuff, 64);
                    H264ENC_MAKE_NAME(ippiQuantLuma8x8Inv_H264)(tmpCoeffsBuff, QP_DIV_6[uMBQP], core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[1][QP_MOD_6[uMBQP]]); //scaling matrix for INTER slice

                    if (curr_slice->doSCoeffsPrediction)
                    {
                        ippsAdd_16s_I(tmpCoeffsBuff, pPredSCoeffs + uBlock*64, 64);
                        //                    DoAddCoeffs(TransformPixels, pRefLayerSCoeffsTmp, qNumCoeffs, BlockSize, InvTransformMode);
                        //                    CalculateCBP(TransformPixels, cbpLumaCoeffs, BlockSize, uBlock+uSubBlock);
                    } else {
                        ippsCopy_16s(tmpCoeffsBuff, pPredSCoeffs + uBlock*64, 64);
                    }
                }
            }
            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock] * 2;
        }
    }
    else
    {
        uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        for (uBlock = 0; uBlock < 16; uBlock++)
        {
            bCoded = ((uCBPLuma & CBP4x4Mask[uBlock])?(1):(0));

            if (core_enc->m_svc_layer.isActive) {
                COEFFSTYPE *pPredSCoeffsTmp = pPredSCoeffs;
                COEFFSTYPE *pPredTCoeffsTmp = pPredTCoeffs;
                pPredSCoeffsTmp += block_subblock_mapping[uBlock] * 16;
                pPredTCoeffsTmp += block_subblock_mapping[uBlock] * 16;

                if (!transform_bypass)
                {
                    ippiDequantResidual4x4_H264_16s(pPredTCoeffsTmp, tmpCoeffsBuff, uMBQP);

                    if (curr_slice->doSCoeffsPrediction)
                    {
                        ippsAdd_16s_I(tmpCoeffsBuff, pPredSCoeffsTmp, 16);
                    } else {
                        ippsCopy_16s(tmpCoeffsBuff, pPredSCoeffsTmp, 16);
                    }
                }
            }
            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
        }
    }

    if (core_enc->m_svc_layer.isActive) {
        //pPredBuf = cur_mb.mbInter.prediction + xoff[uBlock] + yoff[uBlock]*16;
        uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;

        pResidualBuf = core_enc->m_pYResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
        if(pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo))
            pRefPredResiduals = core_enc->m_pYInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
        else
            pRefPredResiduals = core_enc->m_pYInputResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;

        if (pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo)) {
            ReconstructLumaBaseModeResidual_8x8(pPredSCoeffs, pRefPredResiduals,
                pResidualBuf, 0,
                core_enc->m_WidthInMBs * 16);
        } else {
            ReconstructLumaBaseModeResidual(pPredSCoeffs, pRefPredResiduals,
                pResidualBuf, 0,
                core_enc->m_WidthInMBs * 16);
        }

        PIXTYPE *dst = pReconBuf;
        Ipp16s *src1 = pResidualBuf;
        Ipp8u *src2 = cur_mb.mbInter.prediction;
        int ii, jj;
        for (ii = 0; ii < 16; ii++) {
            for (jj = 0; jj < 16; jj++) {
                Ipp16s tmp = src1[jj] + src2[jj];
                tmp = IPP_MIN(tmp, 255);
                tmp = IPP_MAX(tmp, 0);
                dst[jj] = tmp;
            }
            dst += pitchPix;
            src1 += core_enc->m_WidthInMBs * 16;
            src2 += 16;
        }
    }
    //--------------------------------------------------------------------------
    // encode U plane blocks then V plane blocks
    //--------------------------------------------------------------------------

    // update coded block flags
    cur_mb.LocalMacroblockInfo->cbp_luma = uCBPLuma;

    return 1;
}


////////////////////////////////////////////////////////////////////////////////
// CEncAndRecMB for SVC
//
// Main function to drive encode and reconstruct for all blocks
// of one macroblock.
////////////////////////////////////////////////////////////////////////////////
void H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRecMB_SVC)(
    void* state,
    H264SliceType *curr_slice)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    curr_slice->m_cur_mb.MacroblockCoeffsInfo->chromaNC = 0;
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    Ipp32s pitchPix;
    PIXTYPE *pDst, *pSrc;

    Ipp8u baseModeFlag = pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo);
    Ipp8u residualPredictionFlag = pGetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo);
    Ipp8u tcoeff_level_prediction_flag = curr_slice->m_tcoeff_level_prediction_flag; // fixme
    IppiSize roiSize16 = {16, 16};
    IppiSize roiSize8 = {8, 8};
    Ipp32s needRecalculate = 0;


    COEFFSTYPE *y_residual = core_enc->m_pYResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
    COEFFSTYPE *u_residual = core_enc->m_pUResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
    COEFFSTYPE *v_residual = core_enc->m_pVResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;

    COEFFSTYPE *y_ref_residual = core_enc->m_pYInputResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
    COEFFSTYPE *u_ref_residual = core_enc->m_pUInputResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
    COEFFSTYPE *v_ref_residual = core_enc->m_pVInputResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;

    curr_slice->doInterIntraPrediction = 0;
    curr_slice->doSCoeffsPrediction = 0;
    curr_slice->doTCoeffsPrediction = 0;

    if (core_enc->m_svc_layer.isActive) {
        curr_slice->doInterIntraPrediction = 1;
        if (core_enc->m_SliceHeader.MbaffFrameFlag ||
        //  RefLayerMbaffFrameFlag ||
            core_enc->m_restricted_spatial_resolution_change)
            curr_slice->doInterIntraPrediction = 0;

        if (core_enc->m_svc_layer.svc_ext.quality_id == 0) {
            ippiSet_16s_C1R(0, y_residual, core_enc->m_WidthInMBs * 16 * sizeof(COEFFSTYPE), roiSize16);
            ippiSet_16s_C1R(0, u_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
            ippiSet_16s_C1R(0, v_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
        }

        if (!core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag)
        {
            if (baseModeFlag && tcoeff_level_prediction_flag &&
                pGetMBIBLFlag(curr_slice->m_cur_mb.GlobalMacroblockInfo))
            {
                tcoeff_level_prediction_flag = 0;
                VM_ASSERT(0); // error - incompatible flags
            }

            if (!core_enc->m_spatial_resolution_change)
            {
                curr_slice->doInterIntraPrediction = 0;

                if (IS_INTER_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype))
                {
                    if (residualPredictionFlag && IS_INTER_MBTYPE(cur_mb.LocalMacroblockInfo->mbtype_ref))
                    {
                        if (!tcoeff_level_prediction_flag)
                            curr_slice->doSCoeffsPrediction = 1;
                        else
                            curr_slice->doTCoeffsPrediction = 1;
                    }
                }
                else if (baseModeFlag && (cur_mb.GlobalMacroblockInfo->mbtype != MBTYPE_PCM))
                {
                    if (pGetMBIBLFlag(cur_mb.GlobalMacroblockInfo))
                        curr_slice->doSCoeffsPrediction = 1;
                    else if (tcoeff_level_prediction_flag)
                        curr_slice->doTCoeffsPrediction = 1;
                }
            }

            /* ??????????? */
            //if (!residualPredictionFlag || curr_slice->doTCoeffsPrediction) {
            //       ippiSet_16s_C1R(0, y_residual, core_enc->m_WidthInMBs * 16 * sizeof(COEFFSTYPE), roiSize16);
            //        ippiSet_16s_C1R(0, u_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
            //        ippiSet_16s_C1R(0, v_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
            //}

            if (baseModeFlag && (IS_TRUEINTRA_MBTYPE(curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype)))
            {
                pSetMBIBLFlag(cur_mb.GlobalMacroblockInfo, (curr_slice->doTCoeffsPrediction ? 0 : 1));
                if (!curr_slice->doTCoeffsPrediction)
                {
                    cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA;
                } else {
                    cur_mb.GlobalMacroblockInfo->mbtype = curr_slice->m_cur_mb.LocalMacroblockInfo->mbtype_ref;
                    MFX_INTERNAL_CPY(curr_slice->m_cur_mb.intra_types, curr_slice->m_cur_mb.intra_types_save, 16*sizeof(T_AIMode));
                }
            }
            else
            {
                pSetMBIBLFlag(cur_mb.GlobalMacroblockInfo, 0);
            }

            if (IS_INTRA_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype))
            {
                if ((/*core_enc->m_spatial_resolution_change || */pGetMBIBLFlag(cur_mb.GlobalMacroblockInfo) &&
                    !curr_slice->doTCoeffsPrediction && !curr_slice->doSCoeffsPrediction)
                    && !core_enc->recodeFlag)
                {
                    Ipp32u uOffset = core_enc->m_pMBOffsets[cur_mb.uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                    PIXTYPE *pSrcPlane = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;
                    PIXTYPE *pDstPlane = core_enc->m_pYInputRefPlane + uOffset;

                    Copy16x16(pSrcPlane, cur_mb.mbPitchPixels, pDstPlane, cur_mb.mbPitchPixels);

                    uOffset = core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                    pSrcPlane = core_enc->m_pReconstructFrame->m_pUPlane + uOffset;
                    pDstPlane = core_enc->m_pUInputRefPlane + uOffset;
#ifdef USE_NV12
                    Copy16x8(pSrcPlane, cur_mb.mbPitchPixels, pDstPlane, cur_mb.mbPitchPixels);
#else
                    Copy8x8(pSrcPlane, cur_mb.mbPitchPixels, pDstPlane, cur_mb.mbPitchPixels);

                    pSrcPlane = core_enc->m_pReconstructFrame->m_pVPlane + uOffset;
                    pDstPlane = core_enc->m_pVInputRefPlane + uOffset;

                    Copy8x8(pSrcPlane, cur_mb.mbPitchPixels, pDstPlane, cur_mb.mbPitchPixels);
#endif
                }

                residualPredictionFlag = 0;

                //ippiSet_16s_C1R(0, y_residual, core_enc->m_WidthInMBs * 16 * sizeof(COEFFSTYPE), roiSize16);
                //ippiSet_16s_C1R(0, u_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
                //ippiSet_16s_C1R(0, v_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
            }

            if (!baseModeFlag)
                curr_slice->doInterIntraPrediction = 0;
        }
        else
        {
            pSetMBIBLFlag(cur_mb.GlobalMacroblockInfo, 0);
            curr_slice->doInterIntraPrediction = 0;
            residualPredictionFlag = 0;

            //ippiSet_16s_C1R(0, y_residual, core_enc->m_WidthInMBs * 16 * sizeof(COEFFSTYPE), roiSize16);
            //ippiSet_16s_C1R(0, u_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
            //ippiSet_16s_C1R(0, v_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
        }

        if (residualPredictionFlag == 0)
        {
            // TODO exclude this code block
            ippiSet_16s_C1R(0, y_ref_residual, core_enc->m_WidthInMBs * 16 * sizeof(COEFFSTYPE), roiSize16);
            ippiSet_16s_C1R(0, u_ref_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
            ippiSet_16s_C1R(0, v_ref_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
            COEFFSTYPE *y_BM_residual = core_enc->m_pYInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
            COEFFSTYPE *u_BM_residual = core_enc->m_pUInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
            COEFFSTYPE *v_BM_residual = core_enc->m_pVInputBaseResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
            ippiSet_16s_C1R(0, y_BM_residual, core_enc->m_WidthInMBs * 16 * sizeof(COEFFSTYPE), roiSize16);
            ippiSet_16s_C1R(0, u_BM_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
            ippiSet_16s_C1R(0, v_BM_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
            ippiSet_16s_C1R(0, y_residual, core_enc->m_WidthInMBs * 16 * sizeof(COEFFSTYPE), roiSize16);
            ippiSet_16s_C1R(0, u_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
            ippiSet_16s_C1R(0, v_residual, core_enc->m_WidthInMBs * 8 * sizeof(COEFFSTYPE), roiSize8);
        }

        if (core_enc->m_svc_layer.isActive &&
            !core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag &&
            !core_enc->m_spatial_resolution_change &&
            ((IS_INTRA_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype) && pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)) ||
            (IS_INTER_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype) && pGetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo))) )
        {
            needRecalculate = 1;
        }

        if (!curr_slice->doSCoeffsPrediction)
            ippsSet_16s(0, core_enc->m_SavedMacroblockCoeffs + cur_mb.uMB*COEFFICIENTS_BUFFER_SIZE, 256+64*2);
//        else if (needRecalculate)
//            MFX_INTERNAL_CPY( cur_mb.m_pSCoeffsRef, core_enc->m_SavedMacroblockCoeffs + cur_mb.uMB*COEFFICIENTS_BUFFER_SIZE, 384*sizeof(COEFFSTYPE));

        if (curr_slice->doTCoeffsPrediction)
        {
            if (needRecalculate)
                MFX_INTERNAL_CPY( cur_mb.m_pTCoeffsRef, core_enc->m_SavedMacroblockTCoeffs + cur_mb.uMB*COEFFICIENTS_BUFFER_SIZE, 384*sizeof(COEFFSTYPE));

            if (cur_mb.lumaQP != cur_mb.LocalMacroblockInfo->lumaQP_ref) {
                DoScaleTCoeffs(core_enc->m_SavedMacroblockTCoeffs + cur_mb.uMB*COEFFICIENTS_BUFFER_SIZE,
                    cur_mb.lumaQP,
                    cur_mb.LocalMacroblockInfo->lumaQP_ref,
                    256);
            }
            if (cur_mb.chromaQP != cur_mb.LocalMacroblockInfo->chromaQP_ref) {
                DoScaleTCoeffs(core_enc->m_SavedMacroblockTCoeffs + cur_mb.uMB*COEFFICIENTS_BUFFER_SIZE + 256,
                    cur_mb.chromaQP,
                    cur_mb.LocalMacroblockInfo->chromaQP_ref,
                    64*2);
            }
        } else {
            ippsSet_16s(0, core_enc->m_SavedMacroblockTCoeffs + cur_mb.uMB*COEFFICIENTS_BUFFER_SIZE, 256+64*2);
        }
    }

    curr_slice->doMCDirectForSkipped = 0;
    if (curr_slice->m_slice_type == BPREDSLICE && core_enc->m_svc_layer.isActive &&
        !core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag &&
        curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_SKIPPED &&
        (curr_slice->doSCoeffsPrediction || curr_slice->doTCoeffsPrediction))
        curr_slice->doMCDirectForSkipped = 1;

    if (baseModeFlag && (IS_INTRA_MBTYPE( curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype)))
    {
        cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
        cur_mb.LocalMacroblockInfo->cbp_chroma = 0xffffffff;
        if (curr_slice->doTCoeffsPrediction &&
            curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA_16x16) {
            H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec16x16IntraMB)(state, curr_slice);
        } else {
            H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec4x4IntraMB)(state, curr_slice);
        }
        if (core_enc->m_PicParamSet->chroma_format_idc != 0)
        {
            if (core_enc->m_svc_layer.isActive) // SVC case
                H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma_SVC)(state, curr_slice);
            else // AVC case
                H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma)(state, curr_slice);
        }
        if (needRecalculate && curr_slice->doTCoeffsPrediction &&
            !(cur_mb.LocalMacroblockInfo->cbp_luma  | cur_mb.MacroblockCoeffsInfo->chromaNC))
        {
            cur_mb.lumaQP = cur_mb.LocalMacroblockInfo->lumaQP_ref;
            cur_mb.chromaQP = cur_mb.LocalMacroblockInfo->chromaQP_ref;
//            memset(cur_mb.m_pTCoeffsTmp, 0, 384*sizeof(COEFFSTYPE));
//            if (curr_slice->doSCoeffsPrediction)
//                MFX_INTERNAL_CPY(core_enc->m_SavedMacroblockCoeffs + cur_mb.uMB*COEFFICIENTS_BUFFER_SIZE, cur_mb.m_pSCoeffsRef, 384*sizeof(COEFFSTYPE));
            if (curr_slice->doTCoeffsPrediction)
                MFX_INTERNAL_CPY(core_enc->m_SavedMacroblockTCoeffs + cur_mb.uMB*COEFFICIENTS_BUFFER_SIZE, cur_mb.m_pTCoeffsRef, 384*sizeof(COEFFSTYPE));
            if (curr_slice->doTCoeffsPrediction &&
                curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA_16x16) {
                    H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec16x16IntraMBRec)(state, curr_slice);
            } else {
                H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec4x4IntraMBRec)(state, curr_slice);
            }
            if (core_enc->m_PicParamSet->chroma_format_idc != 0)
                H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChromaRec)(state, curr_slice);
        }
    }
    else
    {
        MBTypeValue mbtype_tmp = curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype;

        if (curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_SKIPPED &&
            (curr_slice->doSCoeffsPrediction || curr_slice->doTCoeffsPrediction))
            mbtype_tmp = MBTYPE_INTER;

        if ((baseModeFlag ||
            curr_slice->doTCoeffsPrediction ||
            curr_slice->doSCoeffsPrediction ||
            curr_slice->m_scan_idx_start != 0 ||
            curr_slice->m_scan_idx_end != 15) &&
            curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype != MBTYPE_SKIPPED)
        {
            cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
            cur_mb.LocalMacroblockInfo->cbp_chroma = 0xffffffff;
        }

        switch (mbtype_tmp)
        {
        case MBTYPE_INTRA:
            H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec4x4IntraMB)(state, curr_slice);
            if (core_enc->m_PicParamSet->chroma_format_idc != 0)
            {
                if (core_enc->m_svc_layer.isActive) // SVC case
                    H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma_SVC)(state, curr_slice);
                else // AVC case
                    H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma)(state, curr_slice);
            }
            break;
        case MBTYPE_INTER:
        case MBTYPE_INTER_8x8:
        case MBTYPE_INTER_8x8_REF0:
        case MBTYPE_INTER_8x16:
        case MBTYPE_INTER_16x8:
        case MBTYPE_FORWARD:
        case MBTYPE_BACKWARD:
        case MBTYPE_FWD_FWD_16x8:
        case MBTYPE_FWD_BWD_16x8:
        case MBTYPE_BWD_FWD_16x8:
        case MBTYPE_BWD_BWD_16x8:
        case MBTYPE_FWD_FWD_8x16:
        case MBTYPE_FWD_BWD_8x16:
        case MBTYPE_BWD_FWD_8x16:
        case MBTYPE_BWD_BWD_8x16:
        case MBTYPE_BIDIR_FWD_16x8:
        case MBTYPE_FWD_BIDIR_16x8:
        case MBTYPE_BIDIR_BWD_16x8:
        case MBTYPE_BWD_BIDIR_16x8:
        case MBTYPE_BIDIR_BIDIR_16x8:
        case MBTYPE_BIDIR_FWD_8x16:
        case MBTYPE_FWD_BIDIR_8x16:
        case MBTYPE_BIDIR_BWD_8x16:
        case MBTYPE_BWD_BIDIR_8x16:
        case MBTYPE_BIDIR_BIDIR_8x16:
        case MBTYPE_B_8x8:
        case MBTYPE_DIRECT:
        case MBTYPE_BIDIR:
            H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRecInterMB)(state, curr_slice);
            if( core_enc->m_PicParamSet->chroma_format_idc != 0 )
            {
                if (core_enc->m_svc_layer.isActive) // SVC case
                    H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma_SVC)(state, curr_slice);
                else // AVC case
                    H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma)(state, curr_slice);
            }
            if (needRecalculate && curr_slice->doTCoeffsPrediction &&
                !(cur_mb.LocalMacroblockInfo->cbp_luma  | cur_mb.MacroblockCoeffsInfo->chromaNC))
            {
                cur_mb.lumaQP = cur_mb.LocalMacroblockInfo->lumaQP_ref;
                cur_mb.chromaQP = cur_mb.LocalMacroblockInfo->chromaQP_ref;
//                memset(cur_mb.m_pTCoeffsTmp, 0, 384*sizeof(COEFFSTYPE));
//                if (curr_slice->doSCoeffsPrediction)
//                    MFX_INTERNAL_CPY(core_enc->m_SavedMacroblockCoeffs + cur_mb.uMB*COEFFICIENTS_BUFFER_SIZE, cur_mb.m_pSCoeffsRef, 384*sizeof(COEFFSTYPE));
                if (curr_slice->doTCoeffsPrediction)
                    MFX_INTERNAL_CPY(core_enc->m_SavedMacroblockTCoeffs + cur_mb.uMB*COEFFICIENTS_BUFFER_SIZE, cur_mb.m_pTCoeffsRef, 384*sizeof(COEFFSTYPE));
                H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRecInterMBRec)(state, curr_slice);
                if( core_enc->m_PicParamSet->chroma_format_idc != 0 )
                    H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChromaRec)(state, curr_slice);
            }

//            if (pGetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo) && curr_slice->m_adaptive_residual_prediction_flag) break;

            //Check for possible skips after cbp reset for MBTYPE_DIRECT & MBTYPE_INTER

            if (!core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag &&
                (curr_slice->m_adaptive_residual_prediction_flag ||
                curr_slice->m_default_residual_prediction_flag) &&
                residualPredictionFlag) break;

            if (baseModeFlag) break;

            if (cur_mb.LocalMacroblockInfo->cbp_luma != 0 || cur_mb.LocalMacroblockInfo->cbp_chroma != 0) break;
            if( cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER && cur_mb.RefIdxs[LIST_0]->RefIdxs[0] == 0)
            {
                H264MotionVector skip_vec;
                H264ENC_MAKE_NAME(H264CoreEncoder_Skip_MV_Predicted)(state, curr_slice, NULL, &skip_vec);
                if (cur_mb.MVs[LIST_0]->MotionVectors[0] != skip_vec) break;
            }
            else if (cur_mb.GlobalMacroblockInfo->mbtype != MBTYPE_DIRECT) break;

        case MBTYPE_SKIPPED: //copy prediction to reconstruct
            {
                Ipp32s i, j;

                pitchPix = cur_mb.mbPitchPixels;
                pDst = core_enc->m_pReconstructFrame->m_pYPlane + core_enc->m_pMBOffsets[cur_mb.uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                pSrc = cur_mb.mbInter.prediction;

                if( curr_slice->m_slice_type == BPREDSLICE )
                {
                    curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_DIRECT; //for correct MC
                }

                H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBLuma)(state, curr_slice, cur_mb.MVs[LIST_0]->MotionVectors,
                                                               cur_mb.MVs[LIST_1]->MotionVectors, pSrc);
                curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;

                COEFFSTYPE *y_res = y_residual;
                for (i = 0; i < 16; i++)
                {
                    if (residualPredictionFlag)
                    {
                        for (j = 0; j < 16; j++)
                        {
                            Ipp16s tmp = pSrc[j] + y_ref_residual[j];
                            tmp = IPP_MIN(IPP_MAX(tmp, 0), 255);
                            y_res[j] = y_ref_residual[j];
                            pDst[j] = tmp;
                        }
                        y_ref_residual += core_enc->m_WidthInMBs * 16;
                    }
                    else
                    {
                        memset(y_res, 0, 16*sizeof(COEFFSTYPE));
                        MFX_INTERNAL_CPY( pDst, pSrc, 16*sizeof(PIXTYPE));
                    }
                    y_res += core_enc->m_WidthInMBs * 16;
                    pDst += pitchPix;
                    pSrc += 16;
                }

                memset( cur_mb.MacroblockCoeffsInfo->numCoeff, 0, 16 );  //Reset this number for skips
                for ( i= 0; i < 16; i++) cur_mb.intra_types[i] = 2;

                if (core_enc->m_PicParamSet->chroma_format_idc != 0 )
                {
                    pDst = core_enc->m_pReconstructFrame->m_pUPlane + core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                    pSrc = cur_mb.mbChromaInter.prediction;

                    if( curr_slice->m_slice_type == BPREDSLICE )
                    {
                        curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_DIRECT; //for correct MC
                    }

                    H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBChroma)(state, curr_slice, cur_mb.MVs[LIST_0]->MotionVectors,
                                                                     cur_mb.MVs[LIST_1]->MotionVectors, pSrc);
                    curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;
#ifdef USE_NV12
                    pDst = core_enc->m_pReconstructFrame->m_pUPlane + core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                    pSrc = cur_mb.mbChromaInter.prediction;

                    for (i = 0; i < 8; i++)
                    {
                        if (residualPredictionFlag)
                        {
                            for (j = 0; j < 8; j++)
                            {
                                Ipp16s tmp;

                                tmp = pSrc[j] + u_ref_residual[j];
                                tmp = IPP_MIN(IPP_MAX(tmp, 0), 255);
                                pDst[2*j] = tmp;
                                u_residual[j] = u_ref_residual[j];

                                tmp = pSrc[j+8] + v_ref_residual[j];
                                tmp = IPP_MIN(IPP_MAX(tmp, 0), 255);
                                pDst[2*j+1] = tmp;
                                v_residual[j] = v_ref_residual[j];
                            }
                        }
                        else
                        {
                            for (j = 0; j < 8; j++)
                            {
                                pDst[2*j] = pSrc[j];
                                pDst[2*j+1] = pSrc[j+8];
                                u_residual[j] = u_ref_residual[j];
                                v_residual[j] = v_ref_residual[j];
                            }
                        }
                        u_ref_residual += core_enc->m_WidthInMBs * 8;
                        v_ref_residual += core_enc->m_WidthInMBs * 8;
                        u_residual += core_enc->m_WidthInMBs * 8;
                        v_residual += core_enc->m_WidthInMBs * 8;
                        pDst += pitchPix;
                        pSrc += 16;
                    }
#else
                    Ipp32s vsize = 8;
                    if( core_enc->m_info.chroma_format_idc == 2 ) vsize = 16;
                    for( i = 0; i < vsize; i++ ){
                        MFX_INTERNAL_CPY( pDst, pSrc, 8*sizeof(PIXTYPE));
                        pDst += pitchPix;
                        pSrc += 16;
                    }
                    pDst = core_enc->m_pReconstructFrame->m_pVPlane + core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                    pSrc = cur_mb.mbChromaInter.prediction+8;
                    for( i = 0; i < vsize; i++ ){
                        MFX_INTERNAL_CPY( pDst, pSrc, 8*sizeof(PIXTYPE));
                        pDst += pitchPix;
                        pSrc += 16;
                    }
#endif
                    memset( &cur_mb.MacroblockCoeffsInfo->numCoeff[16], 0, 32 );  //Reset this number for skips
                    cur_mb.LocalMacroblockInfo->cbp_chroma = 0;
                }
            }
            break;

        case MBTYPE_INTRA_16x16:
            H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec16x16IntraMB)(state, curr_slice);
            if (core_enc->m_PicParamSet->chroma_format_idc != 0)
            {
                if (core_enc->m_svc_layer.isActive) // SVC case
                    H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma_SVC)(state, curr_slice);
                else // AVC case
                    H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma)(state, curr_slice);
            }
            break;

        default:
            break;
        }
    }

    Ipp32u cbp_coeffs;

    if (!core_enc->m_svc_layer.isActive ||
        core_enc->m_svc_layer.svc_ext.quality_id < core_enc->QualityNum - 1) {
        cbp_coeffs = cur_mb.LocalMacroblockInfo->cbp_luma;
    } else {
        if (pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo)) {
            cbp_coeffs = CBPcalculate_8x8(core_enc->m_SavedMacroblockCoeffs + cur_mb.uMB*COEFFICIENTS_BUFFER_SIZE);
        } else {
            cbp_coeffs = CBPcalculate(core_enc->m_SavedMacroblockCoeffs + cur_mb.uMB*COEFFICIENTS_BUFFER_SIZE);
        }
    }
    cur_mb.LocalMacroblockInfo->cbp_luma_coeffs = cbp_coeffs;

    if( IS_INTRA_MBTYPE( curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype ) ){
        Ipp32s k;
        for (k = 0; k < 16; k ++){
            curr_slice->m_cur_mb.MVs[LIST_0]->MotionVectors[k] = null_mv;
            curr_slice->m_cur_mb.MVs[LIST_1]->MotionVectors[k] = null_mv;
            curr_slice->m_cur_mb.MVs[LIST_0 + 2]->MotionVectors[k] = null_mv;
            curr_slice->m_cur_mb.MVs[LIST_1 + 2]->MotionVectors[k] = null_mv;
            curr_slice->m_cur_mb.RefIdxs[LIST_0]->RefIdxs[k] = -1;
            curr_slice->m_cur_mb.RefIdxs[LIST_1]->RefIdxs[k] = -1;
        }

        cur_mb.LocalMacroblockInfo->cbp_luma_residual = cur_mb.LocalMacroblockInfo->cbp_luma_coeffs;
    }else{
        Ipp32s k, cbp_residual;
        for (k = 0; k < 16; k++)
            curr_slice->m_cur_mb.intra_types[k] = (T_AIMode) 2;

        if (!core_enc->m_svc_layer.isActive ||
            core_enc->m_svc_layer.svc_ext.quality_id < core_enc->QualityNum - 1) {
            cbp_residual = cur_mb.LocalMacroblockInfo->cbp_luma_coeffs;
        } else {
            if (curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_SKIPPED && !residualPredictionFlag) {
                cbp_residual = 0;
            } else {
                if (pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo)) {
                    cbp_residual = CBPcalculateResidual_8x8(y_residual, core_enc->m_WidthInMBs * 16);
                } else {
                    cbp_residual = CBPcalculateResidual(y_residual, core_enc->m_WidthInMBs * 16);
                }
            }
        }

        cur_mb.LocalMacroblockInfo->cbp_luma_residual = cbp_residual;
    }

    // luma residuals should be updated after cbp calculation
    if (!IS_TRUEINTRA_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype) &&
        curr_slice->doInterIntraPrediction && pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo)) {
        COEFFSTYPE *pResidualBuf = core_enc->m_pYResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
        COEFFSTYPE *pRefPredResiduals = core_enc->m_pYInputResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;

        Ipp32s i,j;
        for (i=0; i<2; i++) {
            for (j=0; j<2; j++) {
                if (!cur_mb.GlobalMacroblockInfo->i_pred[2*j+i]) continue;
                Ipp32s offx = core_enc->svc_ResizeMap[0][cur_mb.uMBx].border;
                Ipp32s offy = core_enc->svc_ResizeMap[1][cur_mb.uMBy].border;
                IppiSize roiSize = {i?16-offx:offx, j?16-offy:offy};
                if(!i) offx = 0;
                if(!j) offy = 0;

                Ipp32s x, y;
                for (y=0; y<roiSize.height; y++) {
                    for (x=0; x<roiSize.width; x++) {
                        Ipp16s tmp = pResidualBuf[(offy+y)*(core_enc->m_WidthInMBs * 16) + offx+x] +
                            pRefPredResiduals[(offy+y)*(core_enc->m_WidthInMBs * 16) + offx+x];
                        tmp = IPP_MIN(tmp, 255);
                        tmp = IPP_MAX(tmp, -255);
                        pResidualBuf[(offy+y)*(core_enc->m_WidthInMBs * 16) + offx+x] = tmp;
                    }
                }
            }
        }
    }

    if (core_enc->m_svc_layer.isActive &&
        !core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag &&
        !core_enc->m_spatial_resolution_change &&
        !(cur_mb.LocalMacroblockInfo->cbp_luma | cur_mb.MacroblockCoeffsInfo->chromaNC)) {
            if (
                (pGetMBBaseModeFlag(cur_mb.GlobalMacroblockInfo) && IS_INTRA_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype)) ||
                (IS_INTER_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype) && pGetMBResidualPredictionFlag(cur_mb.GlobalMacroblockInfo))) {
                    cur_mb.LocalMacroblockInfo->QP = curr_slice->m_cur_mb.lumaQP =
                        cur_mb.LocalMacroblockInfo->lumaQP_ref;
            }
    }

    Ipp16s cbp_dequant = 0;

    if (core_enc->m_svc_layer.isActive &&
        tcoeff_level_prediction_flag &&
        curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype != MBTYPE_INTRA_16x16) {
        Ipp64s *ptr = (Ipp64s*)(core_enc->m_SavedMacroblockTCoeffs + cur_mb.uMB*COEFFICIENTS_BUFFER_SIZE);
        for (int i = 0; i < (256+128)/4; i++) {
            if (*ptr) {
                cbp_dequant = 1;
                break;
            }
            ptr++;
        }
    }

    if (!core_enc->m_svc_layer.isActive ||
        !tcoeff_level_prediction_flag ||
         cbp_dequant ||
        /*        (sd->cbp4x4_luma_dequant != 0) ||
        (sd->cbp4x4_chroma_dequant[0] != 0) ||
        (sd->cbp4x4_chroma_dequant[1] != 0) ||*/
        (curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA_16x16) ||
        (cur_mb.uMB == 0))
    {
        cur_mb.LocalMacroblockInfo->QP_deblock = cur_mb.LocalMacroblockInfo->QP;
    }
    else if (curr_slice->m_first_mb_in_slice == (Ipp32s)cur_mb.uMB) {
        cur_mb.LocalMacroblockInfo->QP_deblock =
            core_enc->m_mbinfo.mbs[cur_mb.uMB-1].QP_deblock;
    } else {
        cur_mb.LocalMacroblockInfo->QP_deblock = curr_slice->last_QP_deblock;
    }

    curr_slice->last_QP_deblock = cur_mb.LocalMacroblockInfo->QP_deblock;

    curr_slice->m_cur_mb.LocalMacroblockInfo->lumaQP_ref_new = curr_slice->m_cur_mb.lumaQP;
    curr_slice->m_cur_mb.LocalMacroblockInfo->chromaQP_ref_new = curr_slice->m_cur_mb.chromaQP;
//kla moved after brc recode decision
//   curr_slice->m_cur_mb.LocalMacroblockInfo->lumaQP_ref = curr_slice->m_cur_mb.lumaQP;
//   curr_slice->m_cur_mb.LocalMacroblockInfo->chromaQP_ref = curr_slice->m_cur_mb.chromaQP;
// kta
//  curr_slice->m_cur_mb.LocalMacroblockInfo->mbtype_ref = curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype;

/*    if (curr_slice->m_cur_mb.LocalMacroblockInfo->cbp_chroma == 0)
        curr_slice->m_cur_mb.MacroblockCoeffsInfo->chromaNC = 0;*/
}

////////////////////////////////////////////////////////////////////////////////
// CEncAndRecMB
//
// Main function to drive encode and reconstruct for all blocks
// of one macroblock.
////////////////////////////////////////////////////////////////////////////////
void H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRecMB)(
    void* state,
    H264SliceType *curr_slice)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    curr_slice->m_cur_mb.MacroblockCoeffsInfo->chromaNC = 0;
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    Ipp32s pitchPix;
    PIXTYPE *pDst, *pSrc;

    if ((curr_slice->m_scan_idx_start != 0 ||
    curr_slice->m_scan_idx_end != 15) &&
    curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype != MBTYPE_SKIPPED)
    {
        cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
        cur_mb.LocalMacroblockInfo->cbp_chroma = 0xffffffff;
    }

    switch (curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype)
    {
        case MBTYPE_INTRA:
            H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec4x4IntraMB)(state, curr_slice);
            if( core_enc->m_PicParamSet->chroma_format_idc != 0 ) H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma)(state, curr_slice);
            break;
        case MBTYPE_INTER:
        case MBTYPE_INTER_8x8:
        case MBTYPE_INTER_8x8_REF0:
        case MBTYPE_INTER_8x16:
        case MBTYPE_INTER_16x8:
        case MBTYPE_FORWARD:
        case MBTYPE_BACKWARD:
        case MBTYPE_FWD_FWD_16x8:
        case MBTYPE_FWD_BWD_16x8:
        case MBTYPE_BWD_FWD_16x8:
        case MBTYPE_BWD_BWD_16x8:
        case MBTYPE_FWD_FWD_8x16:
        case MBTYPE_FWD_BWD_8x16:
        case MBTYPE_BWD_FWD_8x16:
        case MBTYPE_BWD_BWD_8x16:
        case MBTYPE_BIDIR_FWD_16x8:
        case MBTYPE_FWD_BIDIR_16x8:
        case MBTYPE_BIDIR_BWD_16x8:
        case MBTYPE_BWD_BIDIR_16x8:
        case MBTYPE_BIDIR_BIDIR_16x8:
        case MBTYPE_BIDIR_FWD_8x16:
        case MBTYPE_FWD_BIDIR_8x16:
        case MBTYPE_BIDIR_BWD_8x16:
        case MBTYPE_BWD_BIDIR_8x16:
        case MBTYPE_BIDIR_BIDIR_8x16:
        case MBTYPE_B_8x8:
        case MBTYPE_DIRECT:
        case MBTYPE_BIDIR:
            H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRecInterMB)(state, curr_slice);
            if( core_enc->m_PicParamSet->chroma_format_idc != 0 )
                H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma)(state, curr_slice);
            //Check for possible skips after cbp reset for MBTYPE_DIRECT & MBTYPE_INTER
            {
                if( cur_mb.LocalMacroblockInfo->cbp_luma != 0 || cur_mb.LocalMacroblockInfo->cbp_chroma != 0 ) break;
                if( cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER && cur_mb.RefIdxs[LIST_0]->RefIdxs[0] == 0 ){
                    H264MotionVector skip_vec;
                    H264ENC_MAKE_NAME(H264CoreEncoder_Skip_MV_Predicted)(state, curr_slice, NULL, &skip_vec);
                    if( cur_mb.MVs[LIST_0]->MotionVectors[0] != skip_vec ) break;
                }else if( cur_mb.GlobalMacroblockInfo->mbtype != MBTYPE_DIRECT ) break;
            }

        case MBTYPE_SKIPPED: //copy prediction to recostruct
            {
                Ipp32s i;
//                for( i = 0; i<4; i++ )  cur_mb.GlobalMacroblockInfo->sbtype[i] = (MBTypeValue)NUMBER_OF_MBTYPES;
                pitchPix = cur_mb.mbPitchPixels;
                pDst = core_enc->m_pReconstructFrame->m_pYPlane + core_enc->m_pMBOffsets[cur_mb.uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                pSrc = cur_mb.mbInter.prediction;
                if( curr_slice->m_slice_type == BPREDSLICE ){
                    curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_DIRECT; //for correct MC
                }
                H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBLuma)(state, curr_slice, cur_mb.MVs[LIST_0]->MotionVectors, cur_mb.MVs[LIST_1]->MotionVectors, pSrc);
                curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;
                for( i = 0; i<16; i++ ){
                    MFX_INTERNAL_CPY( pDst, pSrc, 16*sizeof(PIXTYPE));
                    pDst += pitchPix;
                    pSrc += 16;
                }
                memset( cur_mb.MacroblockCoeffsInfo->numCoeff, 0, 16 );  //Reset this number for skips
                for (i=0; i<16; i++) cur_mb.intra_types[i] = 2;

                if( core_enc->m_PicParamSet->chroma_format_idc != 0 ){
                    pDst = core_enc->m_pReconstructFrame->m_pUPlane + core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                    pSrc = cur_mb.mbChromaInter.prediction;
                    if( curr_slice->m_slice_type == BPREDSLICE )
                         curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_DIRECT; //for correct MC
                    H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBChroma)(state, curr_slice, cur_mb.MVs[LIST_0]->MotionVectors,
                                                                     cur_mb.MVs[LIST_1]->MotionVectors, pSrc);
                    curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;
#ifdef USE_NV12
                        pDst = core_enc->m_pReconstructFrame->m_pUPlane + core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                        pSrc = cur_mb.mbChromaInter.prediction;
                        for( i=0; i<8; i++ ){
                            for( Ipp32s j=0; j<8; j++ ){
                                pDst[2*j] = pSrc[j];
                                pDst[2*j+1] = pSrc[j+8];
                            }
                            pDst += pitchPix;
                            pSrc += 16;
                        }
#else
                    Ipp32s vsize = 8;
                    if( core_enc->m_info.chroma_format_idc == 2 ) vsize = 16;
                    for( i = 0; i < vsize; i++ ){
                        MFX_INTERNAL_CPY( pDst, pSrc, 8*sizeof(PIXTYPE));
                        pDst += pitchPix;
                        pSrc += 16;
                    }
                    pDst = core_enc->m_pReconstructFrame->m_pVPlane + core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                    pSrc = cur_mb.mbChromaInter.prediction+8;
                    for( i = 0; i < vsize; i++ ){
                         MFX_INTERNAL_CPY( pDst, pSrc, 8*sizeof(PIXTYPE));
                         pDst += pitchPix;
                         pSrc += 16;
                    }
#endif
                    memset( &cur_mb.MacroblockCoeffsInfo->numCoeff[16], 0, 32 );  //Reset this number for skips
                    cur_mb.LocalMacroblockInfo->cbp_chroma = 0;
                }
            }
            break;

        case MBTYPE_INTRA_16x16:
            H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec16x16IntraMB)(state, curr_slice);
            if( core_enc->m_PicParamSet->chroma_format_idc != 0 )
                H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma)(state, curr_slice);
            break;

        default:
            break;
    }

   if( IS_INTRA_MBTYPE( curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype ) ){
        Ipp32s k;
        for (k = 0; k < 16; k ++){
            curr_slice->m_cur_mb.MVs[LIST_0]->MotionVectors[k] = null_mv;
            curr_slice->m_cur_mb.MVs[LIST_1]->MotionVectors[k] = null_mv;
            curr_slice->m_cur_mb.MVs[LIST_0 + 2]->MotionVectors[k] = null_mv;
            curr_slice->m_cur_mb.MVs[LIST_1 + 2]->MotionVectors[k] = null_mv;
            curr_slice->m_cur_mb.RefIdxs[LIST_0]->RefIdxs[k] = -1;
            curr_slice->m_cur_mb.RefIdxs[LIST_1]->RefIdxs[k] = -1;
         }
   }else{
      Ipp32s k;
      for (k = 0; k < 16; k++)
           curr_slice->m_cur_mb.intra_types[k] = (T_AIMode) 2;
   }

    if (curr_slice->m_cur_mb.LocalMacroblockInfo->cbp_chroma == 0)
        curr_slice->m_cur_mb.MacroblockCoeffsInfo->chromaNC = 0;

    cur_mb.LocalMacroblockInfo->cbp_luma_residual = cur_mb.LocalMacroblockInfo->cbp_luma_coeffs = cur_mb.LocalMacroblockInfo->cbp_luma;
    cur_mb.LocalMacroblockInfo->QP_deblock = cur_mb.LocalMacroblockInfo->QP;
    curr_slice->last_QP_deblock = cur_mb.LocalMacroblockInfo->QP_deblock;
    curr_slice->m_cur_mb.LocalMacroblockInfo->lumaQP_ref_new = curr_slice->m_cur_mb.lumaQP;
    curr_slice->m_cur_mb.LocalMacroblockInfo->chromaQP_ref_new = curr_slice->m_cur_mb.chromaQP;
}

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantIntra_RD)(
    void* state,
    H264SliceType *curr_slice)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;     // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    //Ipp32u  uMBType;        // current MB type
    Ipp32u  uMB;
    Ipp32u  uCBPLuma;        // coded flags for all 4x4 blocks
    Ipp32u  uIntraSAD;      // intra MB SAD
    //Ipp16s* pMassDiffBuf;   // difference block pointer

    //COEFFSTYPE* pDCBuf;     // chroma & luma dc coeffs pointer
    PIXTYPE*  pPredBuf;       // prediction block pointer
    PIXTYPE*  pReconBuf;       // prediction block pointer
    Ipp16s*   pDiffBuf;       // difference block pointer
    COEFFSTYPE* pTransformResult; // Result of the transformation.
    //COEFFSTYPE* pQBuf;          // quantized block pointer
    PIXTYPE*  pSrcPlane;      // start of plane to encode
    Ipp32s    pitchPixels;     // buffer pitch
    //Ipp32s    iMBCost;        // recode MB cost counter
    //Ipp32s    iBlkCost[2];    // coef removal counter for left/right 8x8 luma blocks
    Ipp8u     bCoded; // coded block flag
    Ipp32s    iNumCoeffs;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s    iLastCoeff;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32u    uTotalCoeffs = 0;    // Used to detect single expensive coeffs.

    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    uMB = cur_mb.uMB;

    pitchPixels = cur_mb.mbPitchPixels;
    uCBPLuma     = cur_mb.LocalMacroblockInfo->cbp_luma;
    uMBQP       = cur_mb.lumaQP;
    //uMBType     = cur_mb.GlobalMacroblockInfo->mbtype;
    pDiffBuf    = (Ipp16s*) (curr_slice->m_pMBEncodeBuffer + 512);
    pTransformResult = (COEFFSTYPE*)(pDiffBuf + 64);
    //pQBuf       = (COEFFSTYPE*) (pTransformResult + 64);
    //pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    //pMassDiffBuf= (Ipp16s*) (pDCBuf+ 16);
    uIntraSAD   = 0;
    //iMBCost     = 0;
    //iBlkCost[0] = 0;
    //iBlkCost[1] = 0;

    //--------------------------------------------------------------------------
    // encode Y plane blocks (0-15)
    //--------------------------------------------------------------------------

    // initialize pointers and offset
    pSrcPlane = core_enc->m_pCurrentFrame->m_pYPlane;
    uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    TrellisCabacState cbSt;

    Ipp32s pitchPix = 16;
//    pitchPix = pitchPixels;

    if(pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo))
    {
        if( core_enc->m_info.quant_opt_level > 1)
        {
            H264BsBase_CopyContextCABAC_Trellis8x8(&cbSt, curr_slice->m_pbitstream, !is_cur_mb_field);
        }

        pSetMB8x8TSFlag(cur_mb.GlobalMacroblockInfo, 1);

        for (uBlock = 0; uBlock < 4; uBlock++)
        {
            Ipp32s idxb, idx, idxe;

            idxb = uBlock<<2;
            idxe = idxb+4;
            pPredBuf = cur_mb.mb8x8.prediction + xoff[4*uBlock] + yoff[4*uBlock]*16;
            pReconBuf = cur_mb.mb8x8.reconstruct + xoff[4*uBlock] + yoff[4*uBlock]*16;
            //pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;

            if (core_enc->m_PicParamSet->entropy_coding_mode)
            {
                cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;        // These will be updated if the block is coded
                cur_mb.cabac_data->lumaBlock8x8[uBlock].uNumSigCoeffs = 0;
            }
            else
            {
                for (idx = idxb; idx < idxe; idx++)
                {
                    curr_slice->Block_RLE[idx].uNumCoeffs = 0;
                    curr_slice->Block_RLE[idx].uTrailing_Ones = 0;
                    curr_slice->Block_RLE[idx].uTrailing_One_Signs = 0;
                    curr_slice->Block_RLE[idx].uTotalZeros = 16;
                    cur_mb.MacroblockCoeffsInfo->numCoeff[idx] = 0;
               }
            }

            if (!curr_slice->m_use_transform_for_intra_decision)
            {
                uIntraSAD += H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectOneMB_8x8)(
                    state,
                    curr_slice,
                    pSrcPlane + uOffset,
                    pReconBuf,
                    uBlock,
                    cur_mb.intra_types,
                    pPredBuf);
            }

            // check if block is coded
            bCoded = ((uCBPLuma & CBP8x8Mask[uBlock])?(1):(0));

            if (!bCoded)
            {
                // update reconstruct frame for the empty block
                Copy8x8(pPredBuf, 16, pReconBuf, pitchPix);
            }
            else
            {
                // block not declared empty, encode
                // compute difference of predictor and source pels
                // note: asm version does not use pDiffBuf
                //       output is being passed in the mmx registers
                if (!curr_slice->m_use_transform_for_intra_decision /*|| core_enc->m_info.quant_opt_level > 1*/)
                {
                    Diff8x8(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);
                    // forward transform and quantization, in place in pDiffBuf
                    H264ENC_MAKE_NAME(ippiTransformLuma8x8Fwd_H264)(pDiffBuf, pTransformResult);
                    if (core_enc->m_info.quant_opt_level > 1)
                    {
                        H264ENC_MAKE_NAME(ippiQuantLuma8x8_H264)(
                            pTransformResult,
                            pTransformResult,
                            uMBQP,
                            1,
                            enc_single_scan_8x8[is_cur_mb_field],
                            core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[0][QP_MOD_6[uMBQP]], //Use scaling matrix for INTRA
                            &iNumCoeffs,
                            &iLastCoeff,
                            curr_slice,
                            &cbSt,
                            core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[0][QP_MOD_6[uMBQP]]);
                    }
                    else
                    {
                        H264ENC_MAKE_NAME(ippiQuantLuma8x8_H264)(
                            pTransformResult,
                            pTransformResult,
                            QP_DIV_6[uMBQP],
                            1,
                            enc_single_scan_8x8[is_cur_mb_field],
                            core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[0][QP_MOD_6[uMBQP]], //Use scaling matrix for INTRA
                            &iNumCoeffs,
                            &iLastCoeff,
                            NULL,
                            NULL,
                            NULL);
                    }
                }
                else
                {
                    iNumCoeffs = cur_mb.m_iNumCoeffs8x8[ uBlock ];
                    iLastCoeff = cur_mb.m_iLastCoeff8x8[ uBlock ];
                    pTransformResult = &cur_mb.mb8x8.transform[ uBlock*64 ];
                }

                // if everything quantized to zero, skip RLE
                if (!iNumCoeffs)
                {
                    // the block is empty so it is not coded
                    bCoded = 0;
                }
                else
                {
                    uTotalCoeffs += ((iNumCoeffs < 0) ? -(iNumCoeffs*2) : iNumCoeffs);

                    // record RLE info
                    if (core_enc->m_PicParamSet->entropy_coding_mode)
                    {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iNumCoeffs);

                        CabacBlock8x8* c_data = &cur_mb.cabac_data->lumaBlock8x8[uBlock];
                        c_data->uLastSignificant = iLastCoeff;
                        c_data->CtxBlockCat = BLOCK_LUMA_64_LEVELS;
                        c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                        c_data->uFirstCoeff = 0;
                        c_data->uLastCoeff = 63;
                        H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                            pTransformResult,
                            dec_single_scan_8x8[is_cur_mb_field],
                            c_data);

                        bCoded = c_data->uNumSigCoeffs;
                    }
                    else
                    {
                        COEFFSTYPE buf4x4[4][16];
                        Ipp32s i4x4;

                        //Reorder 8x8 block for coding with CAVLC
                        for(i4x4=0; i4x4<4; i4x4++ ) {
                            Ipp32s i;
                            for(i = 0; i<16; i++ )
                                buf4x4[i4x4][dec_single_scan[is_cur_mb_field][i]] =
                                    pTransformResult[dec_single_scan_8x8[is_cur_mb_field][4*i+i4x4]];
                        }

                        bCoded = 0;
                        //Encode each block with CAVLC 4x4
                        for(i4x4 = 0; i4x4<4; i4x4++ ) {
                            Ipp32s i;
                            iLastCoeff = 0;
                            idx = idxb + i4x4;

                            //Check for last coeff
                            for(i = 0; i<16; i++ ) if( buf4x4[i4x4][dec_single_scan[is_cur_mb_field][i]] != 0 ) iLastCoeff=i;

                            H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(
                                buf4x4[i4x4],
                                0, //Luma
                                dec_single_scan[is_cur_mb_field],
                                iLastCoeff,
                                &curr_slice->Block_RLE[idx].uTrailing_Ones,
                                &curr_slice->Block_RLE[idx].uTrailing_One_Signs,
                                &curr_slice->Block_RLE[idx].uNumCoeffs,
                                &curr_slice->Block_RLE[idx].uTotalZeros,
                                curr_slice->Block_RLE[idx].iLevels,
                                curr_slice->Block_RLE[idx].uRuns);

                            bCoded += curr_slice->Block_RLE[idx].uNumCoeffs;
                            cur_mb.MacroblockCoeffsInfo->numCoeff[idx] = curr_slice->Block_RLE[idx].uNumCoeffs;
                         }
                    }
                }

                // update flags if block quantized to empty
                if (curr_slice->m_use_transform_for_intra_decision)
                {
                    if (!bCoded){
                        uCBPLuma &= ~CBP8x8Mask[uBlock];
                        //Copy  prediction
                        Copy8x8(pPredBuf, 16, pReconBuf, pitchPix);
                    }else //Copy reconstruct
                        Copy8x8(pPredBuf + 256, 16, pReconBuf, pitchPix);
                }
                else
                {
                    // update flags if block quantized to empty
                    if (!bCoded){
                        uCBPLuma &= ~CBP8x8Mask[uBlock];
                        // update reconstruct frame with prediction
                        Copy8x8(pPredBuf, 16, pReconBuf, pitchPix);
                    }else {
                        // inverse transform for reconstruct AND...
                        // add inverse transformed coefficients to original predictor
                        // to obtain reconstructed block, store in reconstruct frame
                        // buffer
                        if(iNumCoeffs != 0) {
                            H264ENC_MAKE_NAME(ippiQuantLuma8x8Inv_H264)(pTransformResult, QP_DIV_6[uMBQP], core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[0][QP_MOD_6[uMBQP]]);
                            H264ENC_MAKE_NAME(ippiTransformLuma8x8InvAddPred_H264)(pPredBuf, 16, pTransformResult, pReconBuf, pitchPix, core_enc->m_PicParamSet->bit_depth_luma);
                        }
                    }
                }   // block not declared empty
            } //curr_slice->m_use_transform_for_intra_decision
            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock] * 2;
        }  // for uBlock in luma plane
    }
    else
    {
        if (core_enc->m_info.quant_opt_level > 1)
        {
            H264BsBase_CopyContextCABAC_Trellis4x4(&cbSt, curr_slice->m_pbitstream, !is_cur_mb_field);
        }

        for (uBlock = 0; uBlock < 16; uBlock++ )
        {
            pPredBuf = cur_mb.mb4x4.prediction + xoff[uBlock] + yoff[uBlock]*16;
            pReconBuf = cur_mb.mb4x4.reconstruct + xoff[uBlock] + yoff[uBlock]*16;
            //pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;

            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0; // These will be updated if the block is coded
            if (core_enc->m_PicParamSet->entropy_coding_mode)
            {
                cur_mb.cabac_data->lumaBlock4x4[uBlock].uNumSigCoeffs = 0;
            }else{
                curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                curr_slice->Block_RLE[uBlock].uTotalZeros = 16;
            }

            // find advanced intra prediction block, store in PredBuf
            // Select best AI mode for the block, using reconstructed
            // predictor pels. This function also stores the block
            // predictor pels at pPredBuf.
            if (!curr_slice->m_use_transform_for_intra_decision){
                uIntraSAD += H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectOneBlock)(
                    state,
                    curr_slice,
                    pSrcPlane + uOffset,
                    pReconBuf,
                    uBlock,
                    cur_mb.intra_types,
                    pPredBuf);
            }

            // check if block is coded
            bCoded = ((uCBPLuma & CBP4x4Mask[uBlock])?(1):(0));

            if (!bCoded){
                // update reconstruct frame for the empty block
                Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
            } else {   // block not declared empty, encode
                // compute difference of predictor and source pels
                // note: asm version does not use pDiffBuf
                //       output is being passed in the mmx registers
              if (!curr_slice->m_use_transform_for_intra_decision /*|| core_enc->m_info.quant_opt_level > 1*/){
                Diff4x4(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);
                    if( core_enc->m_info.quant_opt_level > 1 ){
                        H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                            pDiffBuf,
                            pTransformResult,
                            uMBQP,
                            &iNumCoeffs,
                            1, //Always use f for INTRA
                            enc_single_scan[is_cur_mb_field],
                            &iLastCoeff,
                            NULL,
                            curr_slice,
                            0,
                            &cbSt);
                    }else{
                        H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                            pDiffBuf,
                            pTransformResult,
                            uMBQP,
                            &iNumCoeffs,
                            1, //Always use f for INTRA
                            enc_single_scan[is_cur_mb_field],
                            &iLastCoeff,
                            NULL,
                            NULL,
                            0,
                            NULL);
                     }
              }else{
                  iNumCoeffs = cur_mb.m_iNumCoeffs4x4[ uBlock ];
                  iLastCoeff = cur_mb.m_iLastCoeff4x4[ uBlock ];
                  pTransformResult = &cur_mb.mb4x4.transform[ uBlock*16 ];
              }
                // if everything quantized to zero, skip RLE
                if (!iNumCoeffs){
                    // the block is empty so it is not coded
                    bCoded = 0;
                } else {
                    // Preserve the absolute number of coeffs.
                    if (core_enc->m_PicParamSet->entropy_coding_mode)
                    {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iNumCoeffs);

                        CabacBlock4x4* c_data = &cur_mb.cabac_data->lumaBlock4x4[uBlock];
                        c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                        c_data->uLastSignificant = iLastCoeff;
                        c_data->CtxBlockCat = BLOCK_LUMA_LEVELS;
                        c_data->uFirstCoeff = 0;
                        c_data->uLastCoeff = 15;
                        H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                            pTransformResult,
                            dec_single_scan[is_cur_mb_field],
                            c_data);

                        bCoded = c_data->uNumSigCoeffs;
                    }
                    else
                    {
                        // record RLE info
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iNumCoeffs);
                        H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(
                            pTransformResult,
                            0,
                            dec_single_scan[is_cur_mb_field],
                            iLastCoeff,
                            &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
                            &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
                            &curr_slice->Block_RLE[uBlock].uNumCoeffs,
                            &curr_slice->Block_RLE[uBlock].uTotalZeros,
                            curr_slice->Block_RLE[uBlock].iLevels,
                            curr_slice->Block_RLE[uBlock].uRuns);
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = bCoded = curr_slice->Block_RLE[uBlock].uNumCoeffs;
                    }
                }

                // update flags if block quantized to empty
                if (curr_slice->m_use_transform_for_intra_decision) {
                    if (!bCoded) {
                        uCBPLuma &= ~CBP4x4Mask[uBlock]; //Copy predition
                        Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
                    }else //Copy reconstruct
                        Copy4x4(pPredBuf + 256, 16, pReconBuf, pitchPix);
                } else {
                    if (!bCoded){
                        uCBPLuma &= ~CBP4x4Mask[uBlock];
                        Copy4x4(pPredBuf, 16, pReconBuf, pitchPix);
                    } else {
                        H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264)(
                            pPredBuf,
                            pTransformResult,
                            NULL,
                            pReconBuf,
                            16,
                            pitchPix,
                            uMBQP,
                            ((iNumCoeffs < -1) || (iNumCoeffs > 0)),
                            core_enc->m_PicParamSet->bit_depth_luma,
                            NULL);
                    }
                }
            }   // block not declared empty

            // proceed to the next block
            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
        }  // for uBlock in luma plane
    }

    cur_mb.LocalMacroblockInfo->cbp_luma = uCBPLuma;

    return 1;
}

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantInter_RD)(
    void* state,
    H264SliceType *curr_slice,
    Ipp16s *pResPredDiff)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;     // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    //Ipp32u  uMBType;        // current MB type
    Ipp32u  uMB;
    Ipp32u  uCBPLuma;        // coded flags for all 4x4 blocks

    //COEFFSTYPE* pDCBuf;     // chroma & luma dc coeffs pointer
    PIXTYPE*  pPredBuf;       // prediction block pointer
    PIXTYPE*  pReconBuf;       // prediction block pointer
    Ipp16s* pDiffBuf;       // difference block pointer
    COEFFSTYPE *pTransform; // result of the transform.
    COEFFSTYPE *pTransformResult; // result of the transform.
    //COEFFSTYPE* pQBuf;          // quantized block pointer
    //Ipp16s* pMassDiffBuf;   // difference block pointer
    PIXTYPE*  pSrcPlane;      // start of plane to encode
    Ipp32s    pitchPixels;     // buffer pitch in pixels
    Ipp8u     bCoded;        // coded block flag
    Ipp32s    iNumCoeffs;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s    iLastCoeff;  // Number of nonzero coeffs after quant (negative if DC is nonzero)
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;

    uMBQP       = cur_mb.lumaQP;
    TrellisCabacState cbSt;

    uCBPLuma    = cur_mb.LocalMacroblockInfo->cbp_luma;
    pitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels << is_cur_mb_field;
    //uMBType     = cur_mb.GlobalMacroblockInfo->mbtype;
    pTransform  = (COEFFSTYPE*)curr_slice->m_pMBEncodeBuffer;
    pDiffBuf    = (Ipp16s*) (curr_slice->m_pMBEncodeBuffer + 512);
    //pQBuf       = (COEFFSTYPE*) (pDiffBuf+64);
    //pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    //pMassDiffBuf= (Ipp16s*) (pDCBuf+ 16);
    uMB=cur_mb.uMB;

    //--------------------------------------------------------------------------
    // encode Y plane blocks (0-15)
    //--------------------------------------------------------------------------

    // initialize pointers and offset
    pSrcPlane = core_enc->m_pCurrentFrame->m_pYPlane;
    uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    pPredBuf = cur_mb.mbInter.prediction;

    Ipp32s residualPrediction = (pResPredDiff == NULL ? 0 : 1);
    Ipp16s *pRes;
    Ipp16s *y_residual = core_enc->m_pYResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;
    Ipp32s stepR = core_enc->m_WidthInMBs * 16; // step in pels
    Ipp16s *src1;
    Ipp32s ii, jj;

    // Motion Compensate this MB
    H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBLuma)(
        state,
        curr_slice,
        cur_mb.MVs[LIST_0]->MotionVectors,
        cur_mb.MVs[LIST_1]->MotionVectors,
        pPredBuf);

    for (uBlock = 0; uBlock < 16; uBlock++)
    {
        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;        // These will be updated if the block is coded
    }

    if (core_enc->m_PicParamSet->entropy_coding_mode)
    {
        if (pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo))
            for (uBlock = 0; uBlock < 4; uBlock++)
                cur_mb.cabac_data->lumaBlock8x8[uBlock].uNumSigCoeffs = 0;
        else
            for (uBlock = 0; uBlock < 16; uBlock++)
                cur_mb.cabac_data->lumaBlock4x4[uBlock].uNumSigCoeffs = 0;
    }
    else
    {
        for (uBlock = 0; uBlock < 16; uBlock++)
        {
            curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
            curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
            curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
            curr_slice->Block_RLE[uBlock].uTotalZeros = 16;
        }
    }

    if(pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo))
    {
        Ipp32s mbCost=0;

        if( core_enc->m_info.quant_opt_level > OPT_QUANT_INTER_RD )
        {
            H264BsBase_CopyContextCABAC_Trellis8x8(&cbSt, curr_slice->m_pbitstream, !is_cur_mb_field);
        }

        pSetMB8x8TSFlag(cur_mb.GlobalMacroblockInfo, 1);
        //loop over all 8x8 blocks in Y plane for the MB
        Ipp32s coeffCost;

        for (uBlock = 0; uBlock < 4; uBlock++)
        {
            pPredBuf = cur_mb.mbInter.prediction + xoff[uBlock*4] + yoff[uBlock*4]*16;
            // check if block is coded
            bCoded = ((uCBPLuma & CBP8x8Mask[uBlock])?(1):(0));

            if (bCoded)
            {
                if (!residualPrediction)
                  Diff8x8(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);
                else {
                  pRes = pResPredDiff + xoff[uBlock*4] + yoff[uBlock*4]*16;
                  Diff8x8_ResPred(pRes, pPredBuf, pDiffBuf);
                }

                pTransformResult = pTransform + uBlock*64;
                // forward transform and quantization, in place in pDiffBuf
                H264ENC_MAKE_NAME(ippiTransformLuma8x8Fwd_H264)(pDiffBuf, pTransformResult);
                if (core_enc->m_info.quant_opt_level > OPT_QUANT_INTER_RD)
                {
                    H264ENC_MAKE_NAME(ippiQuantLuma8x8_H264)(
                        pTransformResult,
                        pTransformResult,
                        uMBQP,
                        0,
                        enc_single_scan_8x8[is_cur_mb_field],
                        core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[1][QP_MOD_6[uMBQP]], // INTER scaling matrix
                        &iNumCoeffs,
                        &iLastCoeff,
                        curr_slice,
                        &cbSt,
                        core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[1][QP_MOD_6[uMBQP]]);
                }
                else
                {
                    H264ENC_MAKE_NAME(ippiQuantLuma8x8_H264)(
                        pTransformResult,
                        pTransformResult,
                        QP_DIV_6[uMBQP],
                        0,
                        enc_single_scan_8x8[is_cur_mb_field],
                        core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[1][QP_MOD_6[uMBQP]], // INTER scaling matrix
                        &iNumCoeffs,
                        &iLastCoeff,
                        NULL,
                        NULL,
                        NULL);
                }
                coeffCost = iNumCoeffs
                    ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 64, dec_single_scan_8x8[is_cur_mb_field])
                    : 0;
                mbCost += coeffCost;

                // if everything quantized to zero, skip RLE
                if (!iNumCoeffs || (coeffCost < LUMA_COEFF_8X8_MAX_COST && core_enc->m_info.quant_opt_level < OPT_QUANT_INTER_RD+1))
                {
                    uCBPLuma &= ~CBP8x8Mask[uBlock];
                }
                else
                {
                    // record RLE info
                    if (core_enc->m_PicParamSet->entropy_coding_mode)
                    {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iNumCoeffs);

                        CabacBlock8x8* c_data = &cur_mb.cabac_data->lumaBlock8x8[uBlock];
                        c_data->uLastSignificant = iLastCoeff;
                        c_data->CtxBlockCat = BLOCK_LUMA_64_LEVELS;
                        c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                        c_data->uFirstCoeff = 0;
                        c_data->uLastCoeff = 63;
                        H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                            pTransformResult,
                            dec_single_scan_8x8[is_cur_mb_field],
                            c_data);
                    }
                    else
                    {
                        COEFFSTYPE buf4x4[4][16];
                        Ipp32s i4x4;

                        //Reorder 8x8 block for coding with CAVLC
                        for(i4x4=0; i4x4<4; i4x4++ ) {
                            Ipp32s i;
                            for(i = 0; i<16; i++ )
                                buf4x4[i4x4][dec_single_scan[is_cur_mb_field][i]] =
                                    pTransformResult[dec_single_scan_8x8[is_cur_mb_field][4*i+i4x4]];
                        }

                        Ipp32s idx = uBlock*4;
                        //Encode each block with CAVLC 4x4
                        for(i4x4 = 0; i4x4<4; i4x4++, idx++ ) {
                            Ipp32s i;
                            iLastCoeff = 0;

                            //Check for last coeff
                            for(i = 0; i<16; i++ ) if( buf4x4[i4x4][dec_single_scan[is_cur_mb_field][i]] != 0 ) iLastCoeff=i;

                            H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(
                                buf4x4[i4x4],
                                0, //Luma
                                dec_single_scan[is_cur_mb_field],
                                iLastCoeff,
                                &curr_slice->Block_RLE[idx].uTrailing_Ones,
                                &curr_slice->Block_RLE[idx].uTrailing_One_Signs,
                                &curr_slice->Block_RLE[idx].uNumCoeffs,
                                &curr_slice->Block_RLE[idx].uTotalZeros,
                                curr_slice->Block_RLE[idx].iLevels,
                                curr_slice->Block_RLE[idx].uRuns);

                            cur_mb.MacroblockCoeffsInfo->numCoeff[idx] = curr_slice->Block_RLE[idx].uNumCoeffs;
                         }
                    }
                }
            }
                uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock] * 2;
        }

        if( mbCost < LUMA_COEFF_MB_8X8_MAX_COST ){
                uCBPLuma = 0;
        }

       uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
       for (uBlock = 0; uBlock < 4; uBlock++){
            pPredBuf = cur_mb.mbInter.prediction + xoff[uBlock*4] + yoff[uBlock*4]*16;
            pReconBuf = cur_mb.mbInter.reconstruct + xoff[uBlock*4] + yoff[uBlock*4]*16;
            //pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;

            bCoded = ((uCBPLuma & CBP8x8Mask[uBlock])?(1):(0));
            if (!bCoded){

                if (residualPrediction) {
                  src1 = y_residual + xoff[4*uBlock] + yoff[4*uBlock]*stepR;
                  for (ii = 0; ii < 8; ii++) {
                    for (jj = 0; jj < 8; jj++) {
                      Ipp16s tmp = src1[jj] + pPredBuf[jj];
                      tmp = IPP_MIN(tmp, 255);
                      tmp = IPP_MAX(tmp, 0);
                      pReconBuf[jj] = tmp;
                    }
                    pReconBuf += 16;
                    pPredBuf += 16;
                    src1 += stepR;
                  }
                } else
                  Copy8x8(pPredBuf, 16, pReconBuf, 16);

                if (core_enc->m_PicParamSet->entropy_coding_mode)
                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
                else
                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock*4+0] =
                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock*4+1] =
                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock*4+2] =
                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock*4+3] = 0;
            } else {
                if (residualPrediction) {
                  src1 = pTransform + uBlock*64;
                  ippiDequantResidual8x8_H264_16s(src1, src1, uMBQP, core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[0][QP_MOD_6[uMBQP]]);
                  ippiTransformResidualAndAdd8x8_H264_16s16s_C1I(y_residual + xoff[uBlock] + yoff[uBlock]*stepR, src1, src1, stepR << 1, 16);

                  for (ii = 0; ii < 8; ii++) {
                    for (jj = 0; jj < 8; jj++) {
                      Ipp16s tmp = IPP_MAX(*src1, -255);
                      tmp = IPP_MIN(tmp, 255);
                      tmp += pPredBuf[jj];
                      src1++;
                      tmp = IPP_MIN(tmp, 255);
                      tmp = IPP_MAX(tmp, 0);
                      pReconBuf[jj] = tmp;
                    }
                    pReconBuf += 16;
                    pPredBuf += 16;
                  }
                } else {
                    H264ENC_MAKE_NAME(ippiQuantLuma8x8Inv_H264)(pTransform + uBlock*64, QP_DIV_6[uMBQP], core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[1][QP_MOD_6[uMBQP]]); //scaling matrix for INTER slice
                    H264ENC_MAKE_NAME(ippiTransformLuma8x8InvAddPred_H264)(pPredBuf, 16, pTransform + uBlock*64, pReconBuf, 16, core_enc->m_PicParamSet->bit_depth_luma);
                }
            }
            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock] * 2;
        }
    }
    else
    {
        if (core_enc->m_info.quant_opt_level > OPT_QUANT_INTER_RD)
        {
            H264BsBase_CopyContextCABAC_Trellis4x4(&cbSt, curr_slice->m_pbitstream, !is_cur_mb_field);
        }

        Ipp32s iaNumCoeffs[16], CoeffsCost[16] = {9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9};
        for (uBlock = 0; uBlock < 16; uBlock++)
        {
            pPredBuf = cur_mb.mbInter.prediction + xoff[uBlock] + yoff[uBlock]*16;

            // check if block is coded
            bCoded = ((uCBPLuma & CBP4x4Mask[uBlock])?(1):(0));

            if (bCoded)
            {
                // block not declared empty, encode

                if (residualPrediction) {
                  pRes = pResPredDiff + xoff[uBlock] + yoff[uBlock]*16;
                  Diff4x4_ResPred(pRes, pPredBuf, pDiffBuf);
                } else
                  Diff4x4(pPredBuf, pSrcPlane + uOffset, pitchPixels, pDiffBuf);

                pTransformResult = pTransform + uBlock*16;
                if( core_enc->m_info.quant_opt_level > OPT_QUANT_INTER_RD ){
                    H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                        pDiffBuf,
                        pTransformResult,
                        uMBQP,
                        &iaNumCoeffs[uBlock],
                        0,
                        enc_single_scan[is_cur_mb_field],
                        &iLastCoeff,
                        NULL,
                        curr_slice,
                        0,
                        &cbSt);
                }else{
                    H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                        pDiffBuf,
                        pTransformResult,
                        uMBQP,
                        &iaNumCoeffs[uBlock],
                        0,
                        enc_single_scan[is_cur_mb_field],
                        &iLastCoeff,
                        NULL,
                        NULL,
                        0,
                        NULL);
                }
                CoeffsCost[uBlock] = iaNumCoeffs[uBlock]
                    ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 16, dec_single_scan[is_cur_mb_field])
                    : 0;

                if (!iaNumCoeffs[uBlock]) { // if everything quantized to zero, skip RLE
                    uCBPLuma &= ~CBP4x4Mask[uBlock];
                }else{
                    // Preserve the absolute number of coeffs.
                    if (core_enc->m_PicParamSet->entropy_coding_mode)
                    {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iaNumCoeffs[uBlock]);

                        CabacBlock4x4* c_data = &cur_mb.cabac_data->lumaBlock4x4[uBlock];
                        c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                        c_data->uLastSignificant = iLastCoeff;
                        c_data->CtxBlockCat = BLOCK_LUMA_LEVELS;
                        c_data->uFirstCoeff = 0;
                        c_data->uLastCoeff = 15;
                        H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                            pTransformResult,
                            dec_single_scan[is_cur_mb_field],
                            c_data);
                    } else {
                        cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)ABS(iaNumCoeffs[uBlock]);
                        H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(
                            pTransformResult,
                            0,
                            dec_single_scan[is_cur_mb_field],
                            iLastCoeff,
                            &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
                            &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
                            &curr_slice->Block_RLE[uBlock].uNumCoeffs,
                            &curr_slice->Block_RLE[uBlock].uTotalZeros,
                            curr_slice->Block_RLE[uBlock].iLevels,
                            curr_slice->Block_RLE[uBlock].uRuns);
                    }
                }
            }

            // proceed to the next block
            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
        }  // for 4x4 uBlock in luma plane

        //Skip subblock 8x8 if it cost is < 4 or skip MB if it's cost is < 5
            Ipp32s mbCost=0;
            for( uBlock = 0; uBlock < 4; uBlock++ ){
                Ipp32s sb = uBlock*4;
                Ipp32s block8x8cost = CoeffsCost[sb] + CoeffsCost[sb+1] + CoeffsCost[sb+2] + CoeffsCost[sb+3];

                mbCost += block8x8cost;
                if( block8x8cost <= LUMA_8X8_MAX_COST && core_enc->m_info.quant_opt_level < OPT_QUANT_INTER_RD+1)
                    uCBPLuma &= ~CBP8x8Mask[uBlock];
            }
                if( mbCost <= LUMA_MB_MAX_COST )
                    uCBPLuma = 0;

        //Make inverse quantization and transform for non zero blocks
        uOffset = core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        for( uBlock=0; uBlock < 16; uBlock++ ){
            pPredBuf = cur_mb.mbInter.prediction + xoff[uBlock] + yoff[uBlock]*16;
            pReconBuf = cur_mb.mbInter.reconstruct + xoff[uBlock] + yoff[uBlock]*16;
            //pReconBuf = core_enc->m_pReconstructFrame->m_pYPlane + uOffset;

            bCoded = ((uCBPLuma & CBP4x4Mask[uBlock])?(1):(0));
            if (!bCoded) {
                // update reconstruct frame for the empty block

                if (residualPrediction) {
                  src1 = y_residual + xoff[uBlock] + yoff[uBlock]*stepR;
                  for (ii = 0; ii < 4; ii++) {
                    for (jj = 0; jj < 4; jj++) {
                      Ipp16s tmp = src1[jj] + pPredBuf[jj];
                      tmp = IPP_MIN(tmp, 255);
                      tmp = IPP_MAX(tmp, 0);
                      pReconBuf[jj] = tmp;
                    }
                    pReconBuf += 16;
                    pPredBuf += 16;
                    src1 += stepR;
                  }
                } else
                  Copy4x4(pPredBuf, 16, pReconBuf, 16);

                cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;

            } else {
                if (residualPrediction) {
                  src1 = pTransform + uBlock*16;
                  ippiDequantResidual4x4_H264_16s(src1, src1, uMBQP);
                  ippiTransformResidualAndAdd_H264_16s16s_C1I(y_residual + xoff[uBlock] + yoff[uBlock]*stepR, src1, src1, stepR << 1, 8);
                  for (ii = 0; ii < 4; ii++) {
                    for (jj = 0; jj < 4; jj++) {
                      Ipp16s tmp = IPP_MAX(*src1, -255);
                      tmp = IPP_MIN(tmp, 255);
                      tmp += pPredBuf[jj];
                      src1++;
                      tmp = IPP_MIN(tmp, 255);
                      tmp = IPP_MAX(tmp, 0);
                      pReconBuf[jj] = tmp;
                    }
                    pReconBuf += 16;
                    pPredBuf += 16;
                  }
                } else
                  H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264) (
                       pPredBuf,
                       pTransform + uBlock*16,
                       NULL,
                       pReconBuf,
                       16,
                       16,
                       uMBQP,
                       ((iaNumCoeffs[uBlock] < -1) || (iaNumCoeffs[uBlock] > 0)),
                       core_enc->m_PicParamSet->bit_depth_luma,
                       NULL);
              }
            uOffset += core_enc->m_EncBlockOffsetInc[is_cur_mb_field][uBlock];
           }
   }
    cur_mb.LocalMacroblockInfo->cbp_luma = uCBPLuma;
    return 1;
}

void H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantChromaIntra_RD)(
    void* state,
    H264SliceType *curr_slice)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;         // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    Ipp32u  uMB;
    PIXTYPE*  pSrcPlane;    // start of plane to encode
    Ipp32s    pitchPixels;  // buffer pitch
    COEFFSTYPE *pDCBuf;     // chroma & luma dc coeffs pointer
    PIXTYPE*  pPredBuf;     // prediction block pointer
    PIXTYPE*  pReconBuf;     // prediction block pointer
    PIXTYPE*  pPredBuf_copy;     // prediction block pointer
    COEFFSTYPE* pQBuf;      // quantized block pointer
    Ipp16s* pMassDiffBuf;   // difference block pointer

    Ipp32u   uCBPChroma;    // coded flags for all chroma blocks
    Ipp32s   iNumCoeffs;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s   iLastCoeff;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s   RLE_Offset;    // Index into BlockRLE array

    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    EnumSliceType slice_type = curr_slice->m_slice_type;
    COEFFSTYPE *pTransformResult;
    COEFFSTYPE *pTransform;

    pitchPixels = cur_mb.mbPitchPixels;
    uMBQP       = cur_mb.chromaQP;
    pTransform = (COEFFSTYPE*)curr_slice->m_pMBEncodeBuffer;
    pQBuf       = (COEFFSTYPE*) (pTransform + 64*2);
    pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    pMassDiffBuf= (Ipp16s*) (pDCBuf+ 16);
    Ipp16s*  pTempDiffBuf;
    uMB = cur_mb.uMB;

    // initialize pointers and offset
    uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
//    uCBPChroma  = cur_mb.LocalMacroblockInfo->cbp_chroma;
    uCBPChroma  = cur_mb.LocalMacroblockInfo->cbp_chroma = 0xffffffff;
    cur_mb.MacroblockCoeffsInfo->chromaNC = 0;

    pPredBuf = cur_mb.mbChromaIntra.prediction;
        // initialize pointers for the U plane blocks
        Ipp32s num_blocks = 2<<core_enc->m_PicParamSet->chroma_format_idc;
        Ipp32s startBlock;
        startBlock = uBlock = 16;
        Ipp32u uLastBlock = uBlock+num_blocks;
        Ipp32u uFinalBlock = uBlock+2*num_blocks;

        // encode first chroma U plane then V plane
        do{
            // Adjust the pPredBuf to point at the V plane predictor when appropriate:
            // (blocks and an INTRA Y plane mode...)
            if (uBlock == uLastBlock) {
                startBlock = uBlock;
                uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                pSrcPlane = core_enc->m_pCurrentFrame->m_pVPlane;
                pPredBuf = cur_mb.mbChromaIntra.prediction+8;
                pReconBuf = cur_mb.mbChromaIntra.reconstruct+8;
                RLE_Offset = V_DC_RLE;
                uLastBlock += num_blocks;
            } else {
                RLE_Offset = U_DC_RLE;
                pSrcPlane = core_enc->m_pCurrentFrame->m_pUPlane;
                pPredBuf = cur_mb.mbChromaIntra.prediction;
                pReconBuf = cur_mb.mbChromaIntra.reconstruct;
            }

            if( core_enc->m_PicParamSet->chroma_format_idc == 2 ){
                H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(
                    pSrcPlane + uOffset,    // source pels
                    pitchPixels,                 // source pitch
                    pPredBuf,               // predictor pels
                    16,
                    pDCBuf,                 // result buffer
                    pMassDiffBuf);
                // Process second part of 2x4 block for DC coeffs
                H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(
                    pSrcPlane + uOffset+8*pitchPixels,    // source pels
                    pitchPixels,                 // source pitch
                    pPredBuf+8*16,               // predictor pels
                    16,
                    pDCBuf+4,                 // result buffer
                    pMassDiffBuf+64);   //+Offset for second path
                H264ENC_MAKE_NAME(ippiTransformQuantChroma422DC_H264)(
                    pDCBuf,
                    pQBuf,
                    uMBQP,
                    &iNumCoeffs,
                    (slice_type == INTRASLICE),
                    1,
                    NULL);
                 // DC values in this block if iNonEmpty is 1.
                cur_mb.MacroblockCoeffsInfo->chromaNC |= (iNumCoeffs != 0);
                if (core_enc->m_PicParamSet->entropy_coding_mode)
                {
                    Ipp32s ctxIdxBlockCat = BLOCK_CHROMA_DC_LEVELS;
                    H264ENC_MAKE_NAME(H264CoreEncoder_ScanSignificant_CABAC)(
                        pDCBuf,
                        ctxIdxBlockCat,
                        8,
                        dec_single_scan_p422,
                        &cur_mb.cabac_data->dcBlock[RLE_Offset - U_DC_RLE]);
                }
                else
                {
                    H264ENC_MAKE_NAME(ippiEncodeChroma422DC_CoeffsCAVLC_H264)(
                        pDCBuf,
                        &curr_slice->Block_RLE[RLE_Offset].uTrailing_Ones,
                        &curr_slice->Block_RLE[RLE_Offset].uTrailing_One_Signs,
                        &curr_slice->Block_RLE[RLE_Offset].uNumCoeffs,
                        &curr_slice->Block_RLE[RLE_Offset].uTotalZeros,
                        curr_slice->Block_RLE[RLE_Offset].iLevels,
                        curr_slice->Block_RLE[RLE_Offset].uRuns);
                }
                H264ENC_MAKE_NAME(ippiTransformDequantChromaDC422_H264)(pDCBuf, uMBQP, NULL);
           }else{
                H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(
                    pSrcPlane + uOffset,    // source pels
                    pitchPixels,                 // source pitch
                    pPredBuf,               // predictor pels
                    16,
                    pDCBuf,                 // result buffer
                    pMassDiffBuf);
                H264ENC_MAKE_NAME(ippiTransformQuantChromaDC_H264)(
                    pDCBuf,
                    pQBuf,
                    uMBQP,
                    &iNumCoeffs,
                    (slice_type == INTRASLICE),
                    1,
                    NULL);
                // DC values in this block if iNonEmpty is 1.
                cur_mb.MacroblockCoeffsInfo->chromaNC |= (iNumCoeffs != 0);
                if (core_enc->m_PicParamSet->entropy_coding_mode)
                {
                    Ipp32s ctxIdxBlockCat = BLOCK_CHROMA_DC_LEVELS;
                    H264ENC_MAKE_NAME(H264CoreEncoder_ScanSignificant_CABAC)(
                        pDCBuf,
                        ctxIdxBlockCat,
                        4,
                        dec_single_scan_p,
                        &cur_mb.cabac_data->dcBlock[RLE_Offset - U_DC_RLE]);
                }
                else
                {
                    H264ENC_MAKE_NAME(ippiEncodeChromaDcCoeffsCAVLC_H264)(
                        pDCBuf,
                        &curr_slice->Block_RLE[RLE_Offset].uTrailing_Ones,
                        &curr_slice->Block_RLE[RLE_Offset].uTrailing_One_Signs,
                        &curr_slice->Block_RLE[RLE_Offset].uNumCoeffs,
                        &curr_slice->Block_RLE[RLE_Offset].uTotalZeros,
                        curr_slice->Block_RLE[RLE_Offset].iLevels,
                        curr_slice->Block_RLE[RLE_Offset].uRuns);
                }
                H264ENC_MAKE_NAME(ippiTransformDequantChromaDC_H264)(pDCBuf, uMBQP, NULL);
           }

//Encode croma AC
       Ipp32s coeffsCost = 0;
       pPredBuf_copy = pPredBuf;
       for (uBlock = startBlock; uBlock < uLastBlock; uBlock++)
       {
            // This will be updated if the block is coded
            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
            if (core_enc->m_PicParamSet->entropy_coding_mode)
            {
                cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
            }
            else
            {
                curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
            }

            pTempDiffBuf = pMassDiffBuf + (uBlock-startBlock)*16;
            pTransformResult = pTransform + (uBlock-startBlock)*16;
            H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                pTempDiffBuf,
                pTransformResult,
                uMBQP,
                &iNumCoeffs,
                0,
                enc_single_scan[is_cur_mb_field],
                &iLastCoeff,
                NULL,
                NULL,
                0,
                NULL);
            coeffsCost += iNumCoeffs
                ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 15, &dec_single_scan[is_cur_mb_field][1])
                : 0;

            // if everything quantized to zero, skip RLE
            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)((iNumCoeffs < 0) ? -(iNumCoeffs+1) : iNumCoeffs);
            if (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock])
            {
                // the block is empty so it is not coded
                if (core_enc->m_PicParamSet->entropy_coding_mode)
                {
                    CabacBlock4x4* c_data = &cur_mb.cabac_data->chromaBlock[uBlock - 16];
                    c_data->uLastSignificant = iLastCoeff;
                    c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                    c_data->CtxBlockCat = BLOCK_CHROMA_AC_LEVELS;
                    c_data->uFirstCoeff = 1;
                    c_data->uLastCoeff = 15;
                    H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                        pTransformResult,
                        dec_single_scan[is_cur_mb_field],
                        c_data);

                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = c_data->uNumSigCoeffs;
                }
                else
                {
                    H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264) (pTransformResult,// pDiffBuf,
                                                1,
                                                dec_single_scan[is_cur_mb_field],
                                                iLastCoeff,
                                                &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
                                                &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
                                                &curr_slice->Block_RLE[uBlock].uNumCoeffs,
                                                &curr_slice->Block_RLE[uBlock].uTotalZeros,
                                                curr_slice->Block_RLE[uBlock].iLevels,
                                                curr_slice->Block_RLE[uBlock].uRuns);

                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = curr_slice->Block_RLE[uBlock].uNumCoeffs;
                }
            }

            pPredBuf += chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock]; //!!!
       }

       if(coeffsCost <= (CHROMA_COEFF_MAX_COST<<(core_enc->m_PicParamSet->chroma_format_idc-1)) )
       {
            //Reset all ac coeffs
            for (uBlock = startBlock; uBlock < uLastBlock; uBlock++)
            {
                cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
                if (core_enc->m_PicParamSet->entropy_coding_mode)
                {
                   cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
                }
                else
                {
                    curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                    curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                    curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                    curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
                }
            }
        }

        pPredBuf = pPredBuf_copy;
        for (uBlock = startBlock; uBlock < uLastBlock; uBlock++)
        {
            cur_mb.MacroblockCoeffsInfo->chromaNC |= 2*(cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock]!=0);
            if (!cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] && !pDCBuf[ chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock] ])
            {
                uCBPChroma &= ~CBP4x4Mask[uBlock-16];
                Copy4x4(pPredBuf, 16, pReconBuf, 16);
            }
            else
            {
                H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264)(
                    pPredBuf,
                    pTransform + (uBlock-startBlock)*16,
                    &pDCBuf[ chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock] ],
                    pReconBuf,
                    16,
                    16,
                    uMBQP,
                    (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock]!=0),
                    core_enc->m_SeqParamSet->bit_depth_chroma,
                    NULL);
            }

            Ipp32s inc = chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock];
            pPredBuf += inc; //!!!
            pReconBuf += inc;
        }

    } while (uBlock < uFinalBlock);

    uCBPChroma &= ~(0xffffffff<<(uBlock-16));
    cur_mb.LocalMacroblockInfo->cbp_chroma = uCBPChroma;

    if (cur_mb.MacroblockCoeffsInfo->chromaNC == 3)
        cur_mb.MacroblockCoeffsInfo->chromaNC = 2;

    if ((cur_mb.GlobalMacroblockInfo->mbtype != MBTYPE_INTRA_16x16) && (cur_mb.GlobalMacroblockInfo->mbtype!= MBTYPE_PCM))
    {
        cur_mb.LocalMacroblockInfo->cbp = (cur_mb.MacroblockCoeffsInfo->chromaNC << 4);
    }
    else
    {
        cur_mb.LocalMacroblockInfo->cbp = 0;
    }
}

void H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantChromaIntra_RD_NV12)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s isCurMBSeparated)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;         // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    Ipp32u  uMB;
    PIXTYPE*  pSrcPlane;    // start of plane to encode
    Ipp32s    pitchPixels;  // buffer pitch
    COEFFSTYPE *pDCBuf;     // chroma & luma dc coeffs pointer
    PIXTYPE*  pPredBuf;     // prediction block pointer
    PIXTYPE*  pReconBuf;     // prediction block pointer
    PIXTYPE*  pPredBuf_copy;     // prediction block pointer
    COEFFSTYPE* pQBuf;      // quantized block pointer
    Ipp16s* pMassDiffBuf;   // difference block pointer

    Ipp32u   uCBPChroma;    // coded flags for all chroma blocks
    Ipp32s   iNumCoeffs;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s   iLastCoeff;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s   RLE_Offset;    // Index into BlockRLE array

    __ALIGN16 PIXTYPE pUSrc[64];
    __ALIGN16 PIXTYPE pVSrc[64];
    PIXTYPE * pSrc;

    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    EnumSliceType slice_type = curr_slice->m_slice_type;
    COEFFSTYPE *pTransformResult;
    COEFFSTYPE *pTransform;

    pitchPixels = cur_mb.mbPitchPixels;
    uMBQP       = cur_mb.chromaQP;
    pTransform = (COEFFSTYPE*)curr_slice->m_pMBEncodeBuffer;
    pQBuf       = (COEFFSTYPE*) (pTransform + 64*2);
    pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    pMassDiffBuf= (Ipp16s*) (pDCBuf+ 16);
    Ipp16s*  pTempDiffBuf;
    uMB = cur_mb.uMB;

    // initialize pointers and offset
    uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    //    uCBPChroma  = cur_mb.LocalMacroblockInfo->cbp_chroma;
    uCBPChroma  = cur_mb.LocalMacroblockInfo->cbp_chroma = 0xffffffff;
    cur_mb.MacroblockCoeffsInfo->chromaNC = 0;

    pPredBuf = cur_mb.mbChromaIntra.prediction;
    // initialize pointers for the U plane blocks
    Ipp32s num_blocks = 2<<core_enc->m_PicParamSet->chroma_format_idc;
    Ipp32s startBlock;
    startBlock = uBlock = 16;
    Ipp32u uLastBlock = uBlock+num_blocks;
    Ipp32u uFinalBlock = uBlock+2*num_blocks;

    if (!isCurMBSeparated)
    {
        pSrc = core_enc->m_pCurrentFrame->m_pUPlane + uOffset;
        for(Ipp32s i=0;i<8;i++){
            Ipp32s s = 8*i;
            pUSrc[s+0] = pSrc[0];
            pVSrc[s+0] = pSrc[1];
            pUSrc[s+1] = pSrc[2];
            pVSrc[s+1] = pSrc[3];
            pUSrc[s+2] = pSrc[4];
            pVSrc[s+2] = pSrc[5];
            pUSrc[s+3] = pSrc[6];
            pVSrc[s+3] = pSrc[7];
            pUSrc[s+4] = pSrc[8];
            pVSrc[s+4] = pSrc[9];
            pUSrc[s+5] = pSrc[10];
            pVSrc[s+5] = pSrc[11];
            pUSrc[s+6] = pSrc[12];
            pVSrc[s+6] = pSrc[13];
            pUSrc[s+7] = pSrc[14];
            pVSrc[s+7] = pSrc[15];

            pSrc[0] = pUSrc[s+0];
            pSrc[1] = pUSrc[s+1];
            pSrc[2] = pUSrc[s+2];
            pSrc[3] = pUSrc[s+3];
            pSrc[4] = pUSrc[s+4];
            pSrc[5] = pUSrc[s+5];
            pSrc[6] = pUSrc[s+6];
            pSrc[7] = pUSrc[s+7];
            pSrc[8] = pVSrc[s+0];
            pSrc[9] = pVSrc[s+1];
            pSrc[10] = pVSrc[s+2];
            pSrc[11] = pVSrc[s+3];
            pSrc[12] = pVSrc[s+4];
            pSrc[13] = pVSrc[s+5];
            pSrc[14] = pVSrc[s+6];
            pSrc[15] = pVSrc[s+7];

            pSrc += pitchPixels;
        }
    }

    // encode first chroma U plane then V plane
    do{
        // Adjust the pPredBuf to point at the V plane predictor when appropriate:
        // (blocks and an INTRA Y plane mode...)
        if (uBlock == uLastBlock) {
            startBlock = uBlock;
            uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
            pSrcPlane = core_enc->m_pCurrentFrame->m_pUPlane + 8;
            pPredBuf = cur_mb.mbChromaIntra.prediction+8;
            pReconBuf = cur_mb.mbChromaIntra.reconstruct+8;
            RLE_Offset = V_DC_RLE;
            uLastBlock += num_blocks;
        } else {
            RLE_Offset = U_DC_RLE;
            pSrcPlane = core_enc->m_pCurrentFrame->m_pUPlane;
            pPredBuf = cur_mb.mbChromaIntra.prediction;
            pReconBuf = cur_mb.mbChromaIntra.reconstruct;
        }

        if( core_enc->m_PicParamSet->chroma_format_idc == 2 ){
            H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(
                pSrcPlane + uOffset,    // source pels
                pitchPixels,                 // source pitch
                pPredBuf,               // predictor pels
                16,
                pDCBuf,                 // result buffer
                pMassDiffBuf);
            // Process second part of 2x4 block for DC coeffs
            H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(
                pSrcPlane + uOffset+8*pitchPixels,    // source pels
                pitchPixels,                 // source pitch
                pPredBuf+8*16,               // predictor pels
                16,
                pDCBuf+4,                 // result buffer
                pMassDiffBuf+64);   //+Offset for second path
            H264ENC_MAKE_NAME(ippiTransformQuantChroma422DC_H264)(
                pDCBuf,
                pQBuf,
                uMBQP,
                &iNumCoeffs,
                (slice_type == INTRASLICE),
                1,
                NULL);
            // DC values in this block if iNonEmpty is 1.
            cur_mb.MacroblockCoeffsInfo->chromaNC |= (iNumCoeffs != 0);
            if (core_enc->m_PicParamSet->entropy_coding_mode)
            {
                Ipp32s ctxIdxBlockCat = BLOCK_CHROMA_DC_LEVELS;
                H264ENC_MAKE_NAME(H264CoreEncoder_ScanSignificant_CABAC)(
                    pDCBuf,
                    ctxIdxBlockCat,
                    8,
                    dec_single_scan_p422,
                    &cur_mb.cabac_data->dcBlock[RLE_Offset - U_DC_RLE]);
            }
            else
            {
                H264ENC_MAKE_NAME(ippiEncodeChroma422DC_CoeffsCAVLC_H264)(
                    pDCBuf,
                    &curr_slice->Block_RLE[RLE_Offset].uTrailing_Ones,
                    &curr_slice->Block_RLE[RLE_Offset].uTrailing_One_Signs,
                    &curr_slice->Block_RLE[RLE_Offset].uNumCoeffs,
                    &curr_slice->Block_RLE[RLE_Offset].uTotalZeros,
                    curr_slice->Block_RLE[RLE_Offset].iLevels,
                    curr_slice->Block_RLE[RLE_Offset].uRuns);
            }
            H264ENC_MAKE_NAME(ippiTransformDequantChromaDC422_H264)(pDCBuf, uMBQP, NULL);
        }else{
            H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(
                pSrcPlane + uOffset,    // source pels
                pitchPixels,                 // source pitch
                pPredBuf,               // predictor pels
                16,
                pDCBuf,                 // result buffer
                pMassDiffBuf);
            H264ENC_MAKE_NAME(ippiTransformQuantChromaDC_H264)(
                pDCBuf,
                pQBuf,
                uMBQP,
                &iNumCoeffs,
                (slice_type == INTRASLICE),
                1,
                NULL);
            // DC values in this block if iNonEmpty is 1.
            cur_mb.MacroblockCoeffsInfo->chromaNC |= (iNumCoeffs != 0);
            if (core_enc->m_PicParamSet->entropy_coding_mode){
                Ipp32s ctxIdxBlockCat = BLOCK_CHROMA_DC_LEVELS;
                H264ENC_MAKE_NAME(H264CoreEncoder_ScanSignificant_CABAC)(
                    pDCBuf,
                    ctxIdxBlockCat,
                    4,
                    dec_single_scan_p,
                    &cur_mb.cabac_data->dcBlock[RLE_Offset - U_DC_RLE]);
            }else{
                H264ENC_MAKE_NAME(ippiEncodeChromaDcCoeffsCAVLC_H264)(
                    pDCBuf,
                    &curr_slice->Block_RLE[RLE_Offset].uTrailing_Ones,
                    &curr_slice->Block_RLE[RLE_Offset].uTrailing_One_Signs,
                    &curr_slice->Block_RLE[RLE_Offset].uNumCoeffs,
                    &curr_slice->Block_RLE[RLE_Offset].uTotalZeros,
                    curr_slice->Block_RLE[RLE_Offset].iLevels,
                    curr_slice->Block_RLE[RLE_Offset].uRuns);
            }
            H264ENC_MAKE_NAME(ippiTransformDequantChromaDC_H264)(pDCBuf, uMBQP, NULL);
        }

        //Encode croma AC
        Ipp32s coeffsCost = 0;
        pPredBuf_copy = pPredBuf;
        for (uBlock = startBlock; uBlock < uLastBlock; uBlock ++) {
            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;     // This will be updated if the block is coded
            if (core_enc->m_PicParamSet->entropy_coding_mode)
            {
                cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
            }
            else
            {
                curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
            }
            pTempDiffBuf = pMassDiffBuf + (uBlock-startBlock)*16;
            pTransformResult = pTransform + (uBlock-startBlock)*16;
            H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                pTempDiffBuf,
                pTransformResult,
                uMBQP,
                &iNumCoeffs,
                0,
                enc_single_scan[is_cur_mb_field],
                &iLastCoeff,
                NULL,
                NULL,
                0,
                NULL);
            coeffsCost += iNumCoeffs
                ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 15, &dec_single_scan[is_cur_mb_field][1])
                : 0;

            // if everything quantized to zero, skip RLE
            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)((iNumCoeffs < 0) ? -(iNumCoeffs+1) : iNumCoeffs);
            if (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock])
            {
                // the block is empty so it is not coded
                if (core_enc->m_PicParamSet->entropy_coding_mode)
                {
                    CabacBlock4x4* c_data = &cur_mb.cabac_data->chromaBlock[uBlock - 16];
                    c_data->uLastSignificant = iLastCoeff;
                    c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                    c_data->CtxBlockCat = BLOCK_CHROMA_AC_LEVELS;
                    c_data->uFirstCoeff = 1;
                    c_data->uLastCoeff = 15;
                    H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                        pTransformResult,
                        dec_single_scan[is_cur_mb_field],
                        c_data);

                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = c_data->uNumSigCoeffs;
                }
                else
                {
                    H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264) (pTransformResult,// pDiffBuf,
                        1,
                        dec_single_scan[is_cur_mb_field],
                        iLastCoeff,
                        &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
                        &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
                        &curr_slice->Block_RLE[uBlock].uNumCoeffs,
                        &curr_slice->Block_RLE[uBlock].uTotalZeros,
                        curr_slice->Block_RLE[uBlock].iLevels,
                        curr_slice->Block_RLE[uBlock].uRuns);

                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = curr_slice->Block_RLE[uBlock].uNumCoeffs;
                }
            }
            pPredBuf += chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock]; //!!!
        }

        if (coeffsCost <= (CHROMA_COEFF_MAX_COST<<(core_enc->m_PicParamSet->chroma_format_idc-1)) )
        {
            //Reset all ac coeffs
            for(uBlock = startBlock; uBlock < uLastBlock; uBlock++)
            {
                cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
                if (core_enc->m_PicParamSet->entropy_coding_mode)
                {
                    cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
                }
                else
                {
                    curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                    curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                    curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                    curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
                }
            }
        }


        pPredBuf = pPredBuf_copy;
        for (uBlock = startBlock; uBlock < uLastBlock; uBlock ++) {
            cur_mb.MacroblockCoeffsInfo->chromaNC |= 2*(cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock]!=0);
            if (!cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] && !pDCBuf[ chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock] ]){
                uCBPChroma &= ~CBP4x4Mask[uBlock-16];
                Copy4x4(pPredBuf, 16, pReconBuf, 16);
            }else {
                H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264)(
                    pPredBuf,
                    pTransform + (uBlock-startBlock)*16,
                    &pDCBuf[ chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock] ],
                    pReconBuf,
                    16,
                    16,
                    uMBQP,
                    (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock]!=0),
                    core_enc->m_SeqParamSet->bit_depth_chroma,
                    NULL);
            }
            Ipp32s inc = chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock];
            pPredBuf += inc; //!!!
            pReconBuf += inc;
        }   // for uBlock in chroma plane
    } while (uBlock < uFinalBlock);
    uCBPChroma &= ~(0xffffffff<<(uBlock-16));

    cur_mb.LocalMacroblockInfo->cbp_chroma = uCBPChroma;

    if (cur_mb.MacroblockCoeffsInfo->chromaNC == 3)
        cur_mb.MacroblockCoeffsInfo->chromaNC = 2;

    if ((cur_mb.GlobalMacroblockInfo->mbtype != MBTYPE_INTRA_16x16) && (cur_mb.GlobalMacroblockInfo->mbtype!= MBTYPE_PCM)){
        cur_mb.LocalMacroblockInfo->cbp = (cur_mb.MacroblockCoeffsInfo->chromaNC << 4);
    } else  {
        cur_mb.LocalMacroblockInfo->cbp = 0;
    }

    if (!isCurMBSeparated)
    {
        pSrc = core_enc->m_pCurrentFrame->m_pUPlane + uOffset;
        for(Ipp32s i=0;i<8;i++){
            Ipp32s s = 8*i;

            pSrc[0] = pUSrc[s+0];
            pSrc[1] = pVSrc[s+0];
            pSrc[2] = pUSrc[s+1];
            pSrc[3] = pVSrc[s+1];
            pSrc[4] = pUSrc[s+2];
            pSrc[5] = pVSrc[s+2];
            pSrc[6] = pUSrc[s+3];
            pSrc[7] = pVSrc[s+3];
            pSrc[8] = pUSrc[s+4];
            pSrc[9] = pVSrc[s+4];
            pSrc[10] = pUSrc[s+5];
            pSrc[11] = pVSrc[s+5];
            pSrc[12] = pUSrc[s+6];
            pSrc[13] = pVSrc[s+6];
            pSrc[14] = pUSrc[s+7];
            pSrc[15] = pVSrc[s+7];

            pSrc += pitchPixels;
        }
    }
}

void H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantChromaInter_RD)(
    void* state,
    H264SliceType *curr_slice)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;         // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    Ipp32u  uMB;
    PIXTYPE*  pSrcPlane;    // start of plane to encode
    Ipp32s    pitchPixels;  // buffer pitch
    COEFFSTYPE *pDCBuf;     // chroma & luma dc coeffs pointer
    PIXTYPE*  pPredBuf;     // prediction block pointer
    PIXTYPE*  pReconBuf;     // prediction block pointer
    PIXTYPE*  pPredBuf_copy;     // prediction block pointer
    COEFFSTYPE* pQBuf;      // quantized block pointer
    Ipp16s* pMassDiffBuf;   // difference block pointer

    Ipp32u   uCBPChroma;    // coded flags for all chroma blocks
    Ipp32s   iNumCoeffs;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s   iLastCoeff;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s   RLE_Offset;    // Index into BlockRLE array

    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    EnumSliceType slice_type = curr_slice->m_slice_type;
    COEFFSTYPE *pTransformResult;
    COEFFSTYPE *pTransform;
    //bool  VPlane;

    pitchPixels = cur_mb.mbPitchPixels;
    uMBQP       = cur_mb.chromaQP;
    pTransform = (COEFFSTYPE*)curr_slice->m_pMBEncodeBuffer;
    pQBuf       = (COEFFSTYPE*) (pTransform + 64*2);
    pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    pMassDiffBuf= (Ipp16s*) (pDCBuf+ 16);
    Ipp16s*  pTempDiffBuf;
    uMB = cur_mb.uMB;

    // initialize pointers and offset
    uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    uCBPChroma  = cur_mb.LocalMacroblockInfo->cbp_chroma;

    cur_mb.MacroblockCoeffsInfo->chromaNC = 0;

    // initialize pointers for the U plane blocks
    Ipp32s num_blocks = 2<<core_enc->m_PicParamSet->chroma_format_idc;
    Ipp32s startBlock;
    startBlock = uBlock = 16;
    Ipp32u uLastBlock = uBlock+num_blocks;
    Ipp32u uFinalBlock = uBlock+2*num_blocks;

    pPredBuf = cur_mb.mbChromaInter.prediction;
    H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBChroma)(state, curr_slice, cur_mb.MVs[LIST_0]->MotionVectors, cur_mb.MVs[LIST_1]->MotionVectors, pPredBuf);
    // encode first chroma U plane then V plane
    do
    {
        // Adjust the pPredBuf to point at the V plane predictor when appropriate:
        // (blocks and an INTRA Y plane mode...)
        if (uBlock == uLastBlock) {
            startBlock = uBlock;
            uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
            pSrcPlane = core_enc->m_pCurrentFrame->m_pVPlane;
            pPredBuf = cur_mb.mbChromaInter.prediction+8;
            pReconBuf = cur_mb.mbChromaInter.reconstruct+8;
            RLE_Offset = V_DC_RLE;
            // initialize pointers for the V plane blocks
            uLastBlock += num_blocks;
            //VPlane = true;
        } else {
            RLE_Offset = U_DC_RLE;
            pSrcPlane = core_enc->m_pCurrentFrame->m_pUPlane;
            pPredBuf = cur_mb.mbChromaInter.prediction;
            pReconBuf = cur_mb.mbChromaInter.reconstruct;
            //VPlane = false;
        }
        if( core_enc->m_PicParamSet->chroma_format_idc == 2 ){
            H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(
                pSrcPlane + uOffset,    // source pels
                pitchPixels,                 // source pitch
                pPredBuf,               // predictor pels
                16,
                pDCBuf,                 // result buffer
                pMassDiffBuf);
            // Process second part of 2x4 block for DC coeffs
            H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(
                pSrcPlane + uOffset+8*pitchPixels,    // source pels
                pitchPixels,                 // source pitch
                pPredBuf+8*16,               // predictor pels
                16,
                pDCBuf+4,                 // result buffer
                pMassDiffBuf+64);   //+Offset for second path
            H264ENC_MAKE_NAME(ippiTransformQuantChroma422DC_H264)(
                pDCBuf,
                pQBuf,
                uMBQP,
                &iNumCoeffs,
                (slice_type == INTRASLICE),
                1,
                NULL);
             // DC values in this block if iNonEmpty is 1.
            cur_mb.MacroblockCoeffsInfo->chromaNC |= (iNumCoeffs != 0);
            if (core_enc->m_PicParamSet->entropy_coding_mode)
            {
                Ipp32s ctxIdxBlockCat = BLOCK_CHROMA_DC_LEVELS;
                H264ENC_MAKE_NAME(H264CoreEncoder_ScanSignificant_CABAC)(
                    pDCBuf,
                    ctxIdxBlockCat,
                    8,
                    dec_single_scan_p422,
                    &cur_mb.cabac_data->dcBlock[RLE_Offset - U_DC_RLE]);
            }
            else
            {
                H264ENC_MAKE_NAME(ippiEncodeChroma422DC_CoeffsCAVLC_H264)(
                    pDCBuf,
                    &curr_slice->Block_RLE[RLE_Offset].uTrailing_Ones,
                    &curr_slice->Block_RLE[RLE_Offset].uTrailing_One_Signs,
                    &curr_slice->Block_RLE[RLE_Offset].uNumCoeffs,
                    &curr_slice->Block_RLE[RLE_Offset].uTotalZeros,
                    curr_slice->Block_RLE[RLE_Offset].iLevels,
                    curr_slice->Block_RLE[RLE_Offset].uRuns);
            }
            H264ENC_MAKE_NAME(ippiTransformDequantChromaDC422_H264)(pDCBuf, uMBQP, NULL);
        } else {
            H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(
                pSrcPlane + uOffset,    // source pels
                pitchPixels,                 // source pitch
                pPredBuf,               // predictor pels
                16,
                pDCBuf,                 // result buffer
                pMassDiffBuf);
            H264ENC_MAKE_NAME(ippiTransformQuantChromaDC_H264)(
                pDCBuf,
                pQBuf,
                uMBQP,
                &iNumCoeffs,
                (slice_type == INTRASLICE),
                1,
                NULL);
            // DC values in this block if iNonEmpty is 1.
            cur_mb.MacroblockCoeffsInfo->chromaNC |= (iNumCoeffs != 0);
            if (core_enc->m_PicParamSet->entropy_coding_mode){
                Ipp32s ctxIdxBlockCat = BLOCK_CHROMA_DC_LEVELS;
                H264ENC_MAKE_NAME(H264CoreEncoder_ScanSignificant_CABAC)(
                    pDCBuf,
                    ctxIdxBlockCat,
                    4,
                    dec_single_scan_p,
                    &cur_mb.cabac_data->dcBlock[RLE_Offset - U_DC_RLE]);
            }else{
                   H264ENC_MAKE_NAME(ippiEncodeChromaDcCoeffsCAVLC_H264)(
                       pDCBuf,
                       &curr_slice->Block_RLE[RLE_Offset].uTrailing_Ones,
                       &curr_slice->Block_RLE[RLE_Offset].uTrailing_One_Signs,
                       &curr_slice->Block_RLE[RLE_Offset].uNumCoeffs,
                       &curr_slice->Block_RLE[RLE_Offset].uTotalZeros,
                       curr_slice->Block_RLE[RLE_Offset].iLevels,
                       curr_slice->Block_RLE[RLE_Offset].uRuns);
            }
            H264ENC_MAKE_NAME(ippiTransformDequantChromaDC_H264)(pDCBuf, uMBQP, NULL);
       }

        //Encode croma AC
#ifdef H264_RD_TRELLIS
        //Save current cabac state
        //TrellisCabacState cbSt;
        //H264BsBase_CopyContextCABAC_TrellisChroma(&cbSt, curr_slice->m_pbitstream, !is_cur_mb_field);
#endif
        Ipp32s coeffsCost = 0;
        pPredBuf_copy = pPredBuf;
        for (uBlock = startBlock; uBlock < uLastBlock; uBlock++)
        {
            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
            if (core_enc->m_PicParamSet->entropy_coding_mode)
            {
                cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
            }
            else
            {
                curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
            }

            // check if block is coded
            pTempDiffBuf = pMassDiffBuf + (uBlock-startBlock)*16;
            pTransformResult = pTransform + (uBlock-startBlock)*16;
            H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                pTempDiffBuf,
                pTransformResult,
                uMBQP,
                &iNumCoeffs,
                0,
                enc_single_scan[is_cur_mb_field],
                &iLastCoeff,
                NULL,
                NULL,
                0,
                NULL);
            coeffsCost += iNumCoeffs
                ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 15, &dec_single_scan[is_cur_mb_field][1])
                : 0;

            // if everything quantized to zero, skip RLE
            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)((iNumCoeffs < 0) ? -(iNumCoeffs+1) : iNumCoeffs);
            if (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock])
            {
                // the block is empty so it is not coded
                if (core_enc->m_PicParamSet->entropy_coding_mode)
                {
                    CabacBlock4x4* c_data = &cur_mb.cabac_data->chromaBlock[uBlock - 16];
                    c_data->uLastSignificant = iLastCoeff;
                    c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                    c_data->CtxBlockCat = BLOCK_CHROMA_AC_LEVELS;
                    c_data->uFirstCoeff = 1;
                    c_data->uLastCoeff = 15;
                    H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                        pTransformResult,
                        dec_single_scan[is_cur_mb_field],
                        c_data);

                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = c_data->uNumSigCoeffs;
                }
                else
                {
                    H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264)(
                        pTransformResult, //pDiffBuf,
                        1,
                        dec_single_scan[is_cur_mb_field],
                        iLastCoeff,
                        &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
                        &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
                        &curr_slice->Block_RLE[uBlock].uNumCoeffs,
                        &curr_slice->Block_RLE[uBlock].uTotalZeros,
                        curr_slice->Block_RLE[uBlock].iLevels,
                        curr_slice->Block_RLE[uBlock].uRuns);

                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = curr_slice->Block_RLE[uBlock].uNumCoeffs;
                }
            }

            pPredBuf += chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock]; //!!!
        }

        if (coeffsCost <= (CHROMA_COEFF_MAX_COST<<(core_enc->m_PicParamSet->chroma_format_idc-1)))
        {
            //Reset all ac coeffs
            for(uBlock = startBlock; uBlock < uLastBlock; uBlock++)
            {
                cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
                if (core_enc->m_PicParamSet->entropy_coding_mode)
                {
                   cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
                }
                else
                {
                    curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                    curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                    curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                    curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
                }
            }
        }

        pPredBuf = pPredBuf_copy;
        for (uBlock = startBlock; uBlock < uLastBlock; uBlock++)
        {
            cur_mb.MacroblockCoeffsInfo->chromaNC |= 2*(cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock]!=0);

            if (!cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] && !pDCBuf[ chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock] ])
            {
                uCBPChroma &= ~CBP4x4Mask[uBlock-16];
                Copy4x4(pPredBuf, 16, pReconBuf, 16);
            }
            else
            {
                H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264)(
                    pPredBuf,
                    pTransform + (uBlock-startBlock)*16,
                    &pDCBuf[ chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock] ],
                    pReconBuf,
                    16,
                    16,
                    uMBQP,
                    (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock]!=0),
                    core_enc->m_SeqParamSet->bit_depth_chroma,
                    NULL);
            }

            Ipp32s inc = chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock];
            pPredBuf += inc;
            pReconBuf += inc;
        }

    } while (uBlock < uFinalBlock);

    //Reset other chroma
    uCBPChroma &= ~(0xffffffff<<(uBlock-16));

    cur_mb.LocalMacroblockInfo->cbp_chroma = uCBPChroma;

    if (cur_mb.MacroblockCoeffsInfo->chromaNC == 3)
        cur_mb.MacroblockCoeffsInfo->chromaNC = 2;
}

#ifdef USE_NV12
void H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantChromaInter_RD_NV12)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s isCurMBSeparated,
    Ipp16s *pResPredDiff)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u  uBlock;         // block number, 0 to 23
    Ipp32u  uOffset;        // to upper left corner of block from start of plane
    Ipp32u  uMBQP;          // QP of current MB
    Ipp32u  uMB;
    PIXTYPE*  pSrcPlane;    // start of plane to encode
    Ipp32s    pitchPixels;  // buffer pitch
    COEFFSTYPE *pDCBuf;     // chroma & luma dc coeffs pointer
    PIXTYPE*  pPredBuf;     // prediction block pointer
    PIXTYPE*  pReconBuf;     // prediction block pointer
    PIXTYPE*  pPredBuf_copy;     // prediction block pointer
    COEFFSTYPE* pQBuf;      // quantized block pointer
    Ipp16s* pMassDiffBuf;   // difference block pointer

    Ipp32u   uCBPChroma;    // coded flags for all chroma blocks
    Ipp32s   iNumCoeffs;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s   iLastCoeff;    // Number of nonzero coeffs after quant (negative if DC is nonzero)
    Ipp32s   RLE_Offset;    // Index into BlockRLE array

    __ALIGN16 PIXTYPE pUSrc[64];
    __ALIGN16 PIXTYPE pVSrc[64];
    PIXTYPE * pSrc;

    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    EnumSliceType slice_type = curr_slice->m_slice_type;
    COEFFSTYPE *pTransformResult;
    COEFFSTYPE *pTransform;
    //bool  VPlane;

    pitchPixels = cur_mb.mbPitchPixels;
    uMBQP       = cur_mb.chromaQP;
    pTransform = (COEFFSTYPE*)curr_slice->m_pMBEncodeBuffer;
    pQBuf       = (COEFFSTYPE*) (pTransform + 64*2);
    pDCBuf      = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
    pMassDiffBuf= (Ipp16s*) (pDCBuf+ 16);
    Ipp16s*  pTempDiffBuf;
    uMB = cur_mb.uMB;

    Ipp32s residualPrediction = (pResPredDiff == NULL ? 0 : 1);
    Ipp16s *pResDiff;
    Ipp16s *pResidual;
    Ipp16s *u_residual = core_enc->m_pUResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
    Ipp16s *v_residual = core_enc->m_pVResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 64 + cur_mb.uMBx * 8;
    Ipp32s stepR =  core_enc->m_WidthInMBs * 8; // step in pels
    Ipp16s *src1;
    Ipp32s ii, jj;

    // initialize pointers and offset
    uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    uCBPChroma  = cur_mb.LocalMacroblockInfo->cbp_chroma;

    cur_mb.MacroblockCoeffsInfo->chromaNC = 0;

    // initialize pointers for the U plane blocks
    Ipp32s num_blocks = 4;
    Ipp32s startBlock;
    startBlock = uBlock = 16;
    Ipp32u uLastBlock = uBlock+num_blocks;
    Ipp32u uFinalBlock = uBlock+2*num_blocks;

    pPredBuf = cur_mb.mbChromaInter.prediction;
    H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBChroma)(state, curr_slice, cur_mb.MVs[LIST_0]->MotionVectors, cur_mb.MVs[LIST_1]->MotionVectors, pPredBuf);

    uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];

    if (!isCurMBSeparated)
    {
        pSrc = core_enc->m_pCurrentFrame->m_pUPlane + uOffset;
        for(Ipp32s i=0;i<8;i++){
            Ipp32s s = 8*i;
            pUSrc[s+0] = pSrc[0];
            pVSrc[s+0] = pSrc[1];
            pUSrc[s+1] = pSrc[2];
            pVSrc[s+1] = pSrc[3];
            pUSrc[s+2] = pSrc[4];
            pVSrc[s+2] = pSrc[5];
            pUSrc[s+3] = pSrc[6];
            pVSrc[s+3] = pSrc[7];
            pUSrc[s+4] = pSrc[8];
            pVSrc[s+4] = pSrc[9];
            pUSrc[s+5] = pSrc[10];
            pVSrc[s+5] = pSrc[11];
            pUSrc[s+6] = pSrc[12];
            pVSrc[s+6] = pSrc[13];
            pUSrc[s+7] = pSrc[14];
            pVSrc[s+7] = pSrc[15];

            pSrc[0] = pUSrc[s+0];
            pSrc[1] = pUSrc[s+1];
            pSrc[2] = pUSrc[s+2];
            pSrc[3] = pUSrc[s+3];
            pSrc[4] = pUSrc[s+4];
            pSrc[5] = pUSrc[s+5];
            pSrc[6] = pUSrc[s+6];
            pSrc[7] = pUSrc[s+7];
            pSrc[8] = pVSrc[s+0];
            pSrc[9] = pVSrc[s+1];
            pSrc[10] = pVSrc[s+2];
            pSrc[11] = pVSrc[s+3];
            pSrc[12] = pVSrc[s+4];
            pSrc[13] = pVSrc[s+5];
            pSrc[14] = pVSrc[s+6];
            pSrc[15] = pVSrc[s+7];

            pSrc += pitchPixels;
        }
    }

    // encode first chroma U plane then V plane
    do
    {
        // Adjust the pPredBuf to point at the V plane predictor when appropriate:
        // (blocks and an INTRA Y plane mode...)
        if (uBlock == uLastBlock) {
            startBlock = uBlock;
            pSrcPlane = core_enc->m_pCurrentFrame->m_pUPlane + 8;
            pPredBuf = cur_mb.mbChromaInter.prediction+8;
            pReconBuf = cur_mb.mbChromaInter.reconstruct+8;
            RLE_Offset = V_DC_RLE;
            // initialize pointers for the V plane blocks
            uLastBlock += num_blocks;
            pResidual = v_residual;
            pResDiff = pResPredDiff + 8;
            //VPlane = true;
        } else {
            RLE_Offset = U_DC_RLE;
            pSrcPlane = core_enc->m_pCurrentFrame->m_pUPlane;
            pPredBuf = cur_mb.mbChromaInter.prediction;
            pReconBuf = cur_mb.mbChromaInter.reconstruct;
            pResidual = u_residual;
            pResDiff = pResPredDiff;
            //VPlane = false;
        }

        if (residualPrediction) {
          Ipp32s i, j, m, n;
          pPredBuf_copy = pPredBuf;
          for (i = 0; i < 2; i++) {
            for (j = 0; j < 2; j ++) {
              Ipp16s *pRes = &pResDiff[i*4*16 + j*4];
              pDCBuf[i*2+j] = 0;
              for (m = 0; m < 4; m ++) {
                for (n = 0; n < 4; n ++) {
                  Ipp32s temp = pRes[m*16 + n] - pPredBuf[m*16 + n];
                  pDCBuf[i*2+j] = (Ipp16s)(pDCBuf[i*2+j] + temp);
                  pMassDiffBuf[i*32+j*16+m*4+n] = (Ipp16s)temp;
                }
              }
              pPredBuf += 4;
              pSrcPlane += 4;
            }
            pPredBuf += 4 * 16 - 8;
          }
          pPredBuf = pPredBuf_copy;
        } else {
           H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(
              pSrcPlane + uOffset,    // source pels
              pitchPixels,                 // source pitch
              pPredBuf,               // predictor pels
              16,
              pDCBuf,                 // result buffer
              pMassDiffBuf);
        }
        H264ENC_MAKE_NAME(ippiTransformQuantChromaDC_H264)(
            pDCBuf,
            pQBuf,
            uMBQP,
            &iNumCoeffs,
            (slice_type == INTRASLICE),
            1,
            NULL);
        // DC values in this block if iNonEmpty is 1.
        cur_mb.MacroblockCoeffsInfo->chromaNC |= (iNumCoeffs != 0);
        if (core_enc->m_PicParamSet->entropy_coding_mode){
            Ipp32s ctxIdxBlockCat = BLOCK_CHROMA_DC_LEVELS;
            H264ENC_MAKE_NAME(H264CoreEncoder_ScanSignificant_CABAC)(
                pDCBuf,
                ctxIdxBlockCat,
                4,
                dec_single_scan_p,
                &cur_mb.cabac_data->dcBlock[RLE_Offset - U_DC_RLE]);
        }else{
            H264ENC_MAKE_NAME(ippiEncodeChromaDcCoeffsCAVLC_H264)(
                pDCBuf,
                &curr_slice->Block_RLE[RLE_Offset].uTrailing_Ones,
                &curr_slice->Block_RLE[RLE_Offset].uTrailing_One_Signs,
                &curr_slice->Block_RLE[RLE_Offset].uNumCoeffs,
                &curr_slice->Block_RLE[RLE_Offset].uTotalZeros,
                curr_slice->Block_RLE[RLE_Offset].iLevels,
                curr_slice->Block_RLE[RLE_Offset].uRuns);
        }
        H264ENC_MAKE_NAME(ippiTransformDequantChromaDC_H264)(pDCBuf, uMBQP, NULL);

        //Encode croma AC
#ifdef H264_RD_TRELLIS
        //Save current cabac state
        //TrellisCabacState cbSt;
        //H264BsBase_CopyContextCABAC_TrellisChroma(&cbSt, curr_slice->m_pbitstream, !is_cur_mb_field);
#endif

        Ipp32s coeffsCost = 0;
        pPredBuf_copy = pPredBuf;
        for (uBlock = startBlock; uBlock < uLastBlock; uBlock ++) {
            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;     // This will be updated if the block is coded
            if (core_enc->m_PicParamSet->entropy_coding_mode){
                cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
            } else {
                curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
            }

            // check if block is coded
            pTempDiffBuf = pMassDiffBuf + (uBlock-startBlock)*16;
            pTransformResult = pTransform + (uBlock-startBlock)*16;
            H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                pTempDiffBuf,
                pTransformResult,
                uMBQP,
                &iNumCoeffs,
                0,
                enc_single_scan[is_cur_mb_field],
                &iLastCoeff,
                NULL,
                NULL,
                0,
                NULL);
            coeffsCost += iNumCoeffs
                ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 15, &dec_single_scan[is_cur_mb_field][1])
                : 0;

            // if everything quantized to zero, skip RLE
            cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = (T_NumCoeffs)((iNumCoeffs < 0) ? -(iNumCoeffs+1) : iNumCoeffs);
            if (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock])
            {
                // the block is empty so it is not coded
                if (core_enc->m_PicParamSet->entropy_coding_mode)
                {
                    CabacBlock4x4* c_data = &cur_mb.cabac_data->chromaBlock[uBlock - 16];
                    c_data->uLastSignificant = iLastCoeff;
                    c_data->uNumSigCoeffs = cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock];
                    c_data->CtxBlockCat = BLOCK_CHROMA_AC_LEVELS;
                    c_data->uFirstCoeff = 1;
                    c_data->uLastCoeff = 15;
                    H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)(
                        pTransformResult,
                        dec_single_scan[is_cur_mb_field],
                        c_data);

                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = c_data->uNumSigCoeffs;
                }
                else
                {
                    H264ENC_MAKE_NAME(ippiEncodeCoeffsCAVLC_H264) (pTransformResult,// pDiffBuf,
                        1,
                        dec_single_scan[is_cur_mb_field],
                        iLastCoeff,
                        &curr_slice->Block_RLE[uBlock].uTrailing_Ones,
                        &curr_slice->Block_RLE[uBlock].uTrailing_One_Signs,
                        &curr_slice->Block_RLE[uBlock].uNumCoeffs,
                        &curr_slice->Block_RLE[uBlock].uTotalZeros,
                        curr_slice->Block_RLE[uBlock].iLevels,
                        curr_slice->Block_RLE[uBlock].uRuns);

                    cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = curr_slice->Block_RLE[uBlock].uNumCoeffs;
                }
            }
            pPredBuf += chromaPredInc[0][uBlock-startBlock]; //!!!
        }

        if (coeffsCost <= CHROMA_COEFF_MAX_COST)
        {
            //Reset all ac coeffs
            for(uBlock = startBlock; uBlock < uLastBlock; uBlock++)
            {
                cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] = 0;
                if (core_enc->m_PicParamSet->entropy_coding_mode)
                {
                    cur_mb.cabac_data->chromaBlock[uBlock - 16].uNumSigCoeffs = 0;
                }
                else
                {
                    curr_slice->Block_RLE[uBlock].uNumCoeffs = 0;
                    curr_slice->Block_RLE[uBlock].uTrailing_Ones = 0;
                    curr_slice->Block_RLE[uBlock].uTrailing_One_Signs = 0;
                    curr_slice->Block_RLE[uBlock].uTotalZeros = 15;
                }
            }
        }

        Ipp8u *pReconBuf_copy = pReconBuf;
        for (uBlock = startBlock; uBlock < uLastBlock; uBlock++)
        {
            pPredBuf = pPredBuf_copy + (uBlock & 2)*32 + (uBlock & 1)*4; //!!!
            pReconBuf = pReconBuf_copy + (uBlock & 2)*32 + (uBlock & 1)*4; //!!!
            cur_mb.MacroblockCoeffsInfo->chromaNC |= 2*(cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock]!=0);

            if (!cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock] && !pDCBuf[ chromaDCOffset[0][uBlock-startBlock] ])
            {
                uCBPChroma &= ~CBP4x4Mask[uBlock-16];
                if (residualPrediction) {
                  src1 = pResidual + (uBlock & 1)*4 + ((uBlock & 2) >> 1)*stepR;
                  for (ii = 0; ii < 4; ii++) {
                    for (jj = 0; jj < 4; jj++) {
                      Ipp16s tmp = src1[jj] + pPredBuf[jj];
                      tmp = IPP_MIN(tmp, 255);
                      tmp = IPP_MAX(tmp, 0);
                      pReconBuf[jj] = (PIXTYPE)tmp;
                    }
                    pReconBuf += 16;
                    pPredBuf += 16;
                    src1 += stepR;
                  }
                } else
                  Copy4x4(pPredBuf, 16, pReconBuf, 16);
            }
            else
            {
              pTransformResult = pTransform + (uBlock-startBlock)*16;
              if (residualPrediction) {
                if (iNumCoeffs != 0)
                  ippiDequantResidual4x4_H264_16s(pTransformResult, pTransformResult, uMBQP);

                pTransformResult[0] = pDCBuf[chromaDCOffset[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock]];

                src1 = pResidual + (uBlock & 1)*4 + ((uBlock & 2) >> 1)*stepR;
                ippiTransformResidualAndAdd_H264_16s16s_C1I(src1, pTransformResult,
                  pTransformResult, stepR * 2, 8);

                src1 = pTransformResult;
                for (ii = 0; ii < 4; ii++) {
                  for (jj = 0; jj < 4; jj++) {
                    Ipp16s tmp;
                    tmp = IPP_MIN(src1[jj], 255);
                    tmp = IPP_MAX(tmp, -255);
                    tmp += pPredBuf[jj];
                    src1++;
                    tmp = IPP_MIN(tmp, 255);
                    tmp = IPP_MAX(tmp, 0);
                    pReconBuf[jj] = (PIXTYPE)tmp;
                  }
                  pReconBuf += 16;
                  pPredBuf += 16;
                }

              } else {
                H264ENC_MAKE_NAME(ippiDequantTransformResidualAndAdd_H264)(
                    pPredBuf,
                    pTransform + (uBlock-startBlock)*16,
                    &pDCBuf[ chromaDCOffset[0][uBlock-startBlock] ],
                    pReconBuf,
                    16,
                    16,
                    uMBQP,
                    (cur_mb.MacroblockCoeffsInfo->numCoeff[uBlock]!=0),
                    core_enc->m_SeqParamSet->bit_depth_chroma,
                    NULL);
              }
            }
//            Ipp32s inc = chromaPredInc[core_enc->m_PicParamSet->chroma_format_idc-1][uBlock-startBlock];
//            pPredBuf += inc; //!!!
//            pReconBuf += inc; //!!!
        }   // for uBlock in chroma plane
    } while (uBlock < uFinalBlock);

    //Reset other chroma
    uCBPChroma &= ~(0xffffffff<<(uBlock-16));

    cur_mb.LocalMacroblockInfo->cbp_chroma = uCBPChroma;

    if (cur_mb.MacroblockCoeffsInfo->chromaNC == 3)
        cur_mb.MacroblockCoeffsInfo->chromaNC = 2;

    if (!isCurMBSeparated)
    {
        pSrc = core_enc->m_pCurrentFrame->m_pUPlane + uOffset;
        for(Ipp32s i=0;i<8;i++){
            Ipp32s s = 8*i;

            pSrc[0] = pUSrc[s+0];
            pSrc[1] = pVSrc[s+0];
            pSrc[2] = pUSrc[s+1];
            pSrc[3] = pVSrc[s+1];
            pSrc[4] = pUSrc[s+2];
            pSrc[5] = pVSrc[s+2];
            pSrc[6] = pUSrc[s+3];
            pSrc[7] = pVSrc[s+3];
            pSrc[8] = pUSrc[s+4];
            pSrc[9] = pVSrc[s+4];
            pSrc[10] = pUSrc[s+5];
            pSrc[11] = pVSrc[s+5];
            pSrc[12] = pUSrc[s+6];
            pSrc[13] = pVSrc[s+6];
            pSrc[14] = pUSrc[s+7];
            pSrc[15] = pVSrc[s+7];

            pSrc += pitchPixels;
        }
    }
}
#endif // USE_NV12


#undef H264CurrentMacroblockDescriptorType
#undef H264SliceType
#undef H264CoreEncoderType
#undef H264ENC_MAKE_NAME
#undef COEFFSTYPE
#undef PIXTYPE
