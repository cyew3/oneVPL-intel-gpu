//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2011 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_VA_WIN)
#include <stdio.h>
#include "encoding_ddi.h"
#include "mfx_ddi_enc_dump.h"

#undef  logi
#define logi(x) { fprintf(f, #x " = %d\n", (int)x); fflush(f); }

void ddiDumpH264SPS(FILE *f, ENCODE_SET_SEQUENCE_PARAMETERS_H264* sps)
{
    logi(sps->FrameWidth);
    logi(sps->FrameHeight);
    logi(sps->Profile);
    logi(sps->Level);
    logi(sps->seq_parameter_set_id);
    logi(sps->chroma_format_idc);
    logi(sps->bit_depth_luma_minus8);
    logi(sps->bit_depth_chroma_minus8);
    logi(sps->log2_max_frame_num_minus4);
    logi(sps->pic_order_cnt_type);
    logi(sps->log2_max_pic_order_cnt_lsb_minus4);
    logi(sps->seq_scaling_matrix_present_flag);
    logi(sps->seq_scaling_list_present_flag);
    logi(sps->delta_pic_order_always_zero_flag);
    logi(sps->frame_mbs_only_flag);
    logi(sps->direct_8x8_inference_flag);
    logi(sps->vui_parameters_present_flag);
    logi(sps->GopPicSize);
    logi(sps->GopRefDist);
    logi(sps->GopOptFlag);
    logi(sps->TargetUsage);
    logi(sps->RateControlMethod);
    logi(sps->TargetBitRate);
    logi(sps->MaxBitRate);
    logi(sps->NumRefFrames);
}

void ddiDumpH264PPS(FILE *f, ENCODE_SET_PICTURE_PARAMETERS_H264* pps)
{
    logi(pps->CurrOriginalPic.AssociatedFlag);
    logi(pps->CurrOriginalPic.Index7Bits);
    logi(pps->CurrReconstructedPic.AssociatedFlag);
    logi(pps->CurrReconstructedPic.Index7Bits);
    logi(pps->CodingType);
    logi(pps->FieldCodingFlag);
    logi(pps->FieldFrameCodingFlag);
    logi(pps->InterleavedFieldBFF);
    logi(pps->ProgressiveField);
    logi(pps->NumSlice);
    logi(pps->bIdrPic);
    logi(pps->pic_parameter_set_id);
    logi(pps->seq_parameter_set_id);
    logi(pps->num_ref_idx_l0_active_minus1);
    logi(pps->num_ref_idx_l1_active_minus1);
    logi(pps->chroma_qp_index_offset);
    logi(pps->second_chroma_qp_index_offset);
//    logi(pps->padding);
    logi(pps->entropy_coding_mode_flag);
    logi(pps->pic_order_present_flag);
    logi(pps->weighted_pred_flag);
    logi(pps->weighted_bipred_idc);
    logi(pps->constrained_intra_pred_flag);
    logi(pps->transform_8x8_mode_flag);
    logi(pps->pic_scaling_matrix_present_flag);
    logi(pps->pic_scaling_list_present_flag);
    logi(pps->RefPicFlag);
    logi(pps->QpY);
    logi(pps->bUseRawPicForRef);
    for (UINT i = 0; i < 16; i++)
    {
        logi(pps->RefFrameList[i].AssociatedFlag);
        logi(pps->RefFrameList[i].Index7Bits);
    }
    logi(pps->UsedForReferenceFlags);
    logi(pps->CurrFieldOrderCnt[0]);
    logi(pps->CurrFieldOrderCnt[1]);
    for (UINT i = 0; i < 16; i++)
    {
        logi(pps->FieldOrderCntList[i][0]);
        logi(pps->FieldOrderCntList[i][1]);
    }
    logi(pps->frame_num);
    logi(pps->bLastPicInSeq);
    logi(pps->bLastPicInStream);
}

void ddiDumpH264SliceHeader(FILE *f, ENCODE_SET_SLICE_HEADER_H264* sh, int NumSlice)
{
    for (int sliceId = 0; sliceId < NumSlice; sliceId++)
    {
        logi(sh[sliceId].first_mb_in_slice);
        logi(sh[sliceId].NumMbsForSlice);
        logi(sh[sliceId].slice_type);
        logi(sh[sliceId].pic_parameter_set_id);
        logi(sh[sliceId].direct_spatial_mv_pred_flag);
        logi(sh[sliceId].num_ref_idx_active_override_flag);
        logi(sh[sliceId].long_term_reference_flag);
        logi(sh[sliceId].idr_pic_id);
        logi(sh[sliceId].pic_order_cnt_lsb);
        logi(sh[sliceId].delta_pic_order_cnt_bottom);
        logi(sh[sliceId].delta_pic_order_cnt[0]);
        logi(sh[sliceId].delta_pic_order_cnt[1]);
        logi(sh[sliceId].num_ref_idx_l0_active_minus1);
        logi(sh[sliceId].num_ref_idx_l1_active_minus1);
        logi(sh[sliceId].luma_log2_weight_denom);
        logi(sh[sliceId].chroma_log2_weight_denom);
        logi(sh[sliceId].cabac_init_idc);
//        logi(sh[sliceId].slice_qp_delta);
        logi(sh[sliceId].disable_deblocking_filter_idc);
        logi(sh[sliceId].slice_alpha_c0_offset_div2);
        logi(sh[sliceId].slice_beta_offset_div2);
        for (UINT i = 0; i < 32; i++)
        {
            logi(sh[sliceId].RefPicList[0][i].AssociatedFlag);
            logi(sh[sliceId].RefPicList[0][i].Index7Bits);
        }
        for (UINT i = 0; i < 32; i++)
        {
            logi(sh[sliceId].RefPicList[1][i].AssociatedFlag);
            logi(sh[sliceId].RefPicList[1][i].Index7Bits);
        }
        for (UINT i = 0; i < 32; i++)
        {
            logi(sh[sliceId].Weights[0][i][0][0]);
            logi(sh[sliceId].Weights[0][i][0][1]);
            logi(sh[sliceId].Weights[0][i][1][0]);
            logi(sh[sliceId].Weights[0][i][1][1]);
            logi(sh[sliceId].Weights[0][i][2][0]);
            logi(sh[sliceId].Weights[0][i][2][1]);
        }
        for (UINT i = 0; i < 32; i++)
        {
            logi(sh[sliceId].Weights[1][i][0][0]);
            logi(sh[sliceId].Weights[1][i][0][1]);
            logi(sh[sliceId].Weights[1][i][1][0]);
            logi(sh[sliceId].Weights[1][i][1][1]);
            logi(sh[sliceId].Weights[1][i][2][0]);
            logi(sh[sliceId].Weights[1][i][2][1]);
        }
        logi(sh[sliceId].slice_id);
    }
}

void ddiDumpH264MBData(FILE     *fText,
                       mfxI32   /*PicNum*/,
                       mfxI32   PicType,
                       mfxI32   PicWidth,
                       mfxI32   PicHeight,
                       mfxI32   NumSlicesPerPic,
                       mfxI32   NumMBs,
                       void     *pMBData,
                       void     *pMVData)
{
    ENCODE_MB_CONTROL_DATA_H264 *VMEMBData = (ENCODE_MB_CONTROL_DATA_H264*)pMBData;
    ENCODE_ENC_MV_H264              *VMEMVData = (ENCODE_ENC_MV_H264*)pMVData;

    static int PicNum = 0;
    PicNum++;

    fprintf(fText, "\n\n========================================================================\n");
    fprintf(fText, "Pic Num = [%d], Coding Type = [%d], [%d, %d], %d MBs, %d Slices\n", PicNum, PicType, PicWidth, PicHeight, NumMBs, NumSlicesPerPic);


    for (int i = 0; i< NumMBs; i++)
    {
        fprintf(fText, "MB index = %d, MB position = [%d, %d]\n", i, VMEMBData[i].MbXCnt, VMEMBData[i].MbYCnt);
        fprintf(fText, "InterMbMode             = %d\n", VMEMBData[i].InterMbMode);
        fprintf(fText, "SkipMbFlag              = %d\n", VMEMBData[i].SkipMbFlag);
        fprintf(fText, "IntraMbMode             = %d\n", VMEMBData[i].IntraMbMode);
        fprintf(fText, "FieldMbPolarityFlag     = %d\n", VMEMBData[i].FieldMbPolarityFlag);
        fprintf(fText, "MbType                  = %d\n", VMEMBData[i].MbType);
        fprintf(fText, "IntraMbFlag             = %d\n", VMEMBData[i].IntraMbFlag);
        fprintf(fText, "FieldMbFlag             = %d\n", VMEMBData[i].FieldMbFlag);
        fprintf(fText, "Transform8x8Flag        = %d\n", VMEMBData[i].Transform8x8Flag);
        fprintf(fText, "CbpDcY = [0x%X] CbpDcU = [0x%X], CbpDcV = [0x%X]\n", 
            VMEMBData[i].CbpDcY, VMEMBData[i].CbpDcU, VMEMBData[i].CbpDcV);
        fprintf(fText, "MvFormat                = %d\n", VMEMBData[i].MvFormat);
        fprintf(fText, "PackedMvNum             = %d\n", VMEMBData[i].PackedMvNum);
        fprintf(fText, "Cbp4x4Y = [0x%X] Cbp4x4U = [0x%X] Cbp4x4V = [0x%X]\n", 
            VMEMBData[i].Cbp4x4Y, VMEMBData[i].Cbp4x4U, VMEMBData[i].Cbp4x4V);
        fprintf(fText, "QpPrimeY                = %d\n", VMEMBData[i].QpPrimeY);
        fprintf(fText, "SkipMbConvDisable       = %d\n", VMEMBData[i].SkipMbConvDisable);
        fprintf(fText, "LastMbFlag              = %d\n", VMEMBData[i].LastMbFlag);
        fprintf(fText, "EnableCoeffClamp        = %d\n", VMEMBData[i].EnableCoeffClamp);
        fprintf(fText, "Skip8x8Pattern          = %d\n", VMEMBData[i].Skip8x8Pattern);

        if (1 == VMEMBData[i].IntraMbFlag)   //Intra MB                      
        {
            fprintf(fText, "LumaIntraPredModes\n");

            /* 4x4 block order
            0   1   4   5
            2   3   6   7
            8   9   12  13
            10  11  14  15
            */
            //Intra8x8
            if (1 == VMEMBData[i].IntraMbMode)
            {
                //8x8 blocks
                fprintf(fText, "\t%d\t%d\t%d\t%d\n", 
                    ((VMEMBData[i].LumaIntraPredModes[0]>>0) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>0) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>4) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>4) & 0x000000F));

                fprintf(fText, "\t%d\t%d\t%d\t%d\n", 
                    ((VMEMBData[i].LumaIntraPredModes[0]>>0) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>0) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>4) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>4) & 0x000000F));

                fprintf(fText, "\t%d\t%d\t%d\t%d\n", 
                    ((VMEMBData[i].LumaIntraPredModes[0]>>8) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>8) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>12)& 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>12)& 0x000000F));

                fprintf(fText, "\t%d\t%d\t%d\t%d\n", 
                    ((VMEMBData[i].LumaIntraPredModes[0]>>8) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>8) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>12)& 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>12)& 0x000000F));

            }
            else if (2 == VMEMBData[i].IntraMbMode) //Intra4x4
            {
                //4x4
                fprintf(fText, "\t%d\t%d\t%d\t%d\n", 
                    ((VMEMBData[i].LumaIntraPredModes[0]>>0) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>4) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[1]>>0) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[1]>>4) & 0x000000F));

                fprintf(fText, "\t%d\t%d\t%d\t%d\n", 
                    ((VMEMBData[i].LumaIntraPredModes[0]>>8) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[0]>>12)& 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[1]>>8) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[1]>>12)& 0x000000F));

                fprintf(fText, "\t%d\t%d\t%d\t%d\n", 
                    ((VMEMBData[i].LumaIntraPredModes[2]>>0) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[2]>>4) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[3]>>0) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[3]>>4) & 0x000000F));

                fprintf(fText, "\t%d\t%d\t%d\t%d\n", 
                    ((VMEMBData[i].LumaIntraPredModes[2]>>8) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[2]>>12)& 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[3]>>8) & 0x000000F),
                    ((VMEMBData[i].LumaIntraPredModes[3]>>12)& 0x000000F));
            }
            else
            {
                //16x16
                fprintf(fText, "\t%d\n", 
                    ((VMEMBData[i].LumaIntraPredModes[0]>>0) & 0x000000F));
            }

            fprintf(fText, "ChromaIntraPredMode     = %X\n", VMEMBData[i].ChromaIntraPredMode);

            fprintf(fText, "IntraPredAvilFlag = [%d, %d, %d, %d, %d, %d]\n", 
                VMEMBData[i].IntraPredAvilFlagA, VMEMBData[i].IntraPredAvilFlagB, VMEMBData[i].IntraPredAvilFlagC,
                VMEMBData[i].IntraPredAvilFlagD, VMEMBData[i].IntraPredAvilFlagE, VMEMBData[i].IntraPredAvilFlagF);
        }
        else    //Inter MB
        {
            fprintf(fText, "SubMbShape              = %d\n", VMEMBData[i].SubMbShape);
            fprintf(fText, "SubMbPredMode           = %d\n", VMEMBData[i].SubMbPredMode);

            fprintf(fText, "RefIdx0 = \n\t%d\t%d\n\t%d\t%d\n", 
                VMEMBData[i].RefIdxL0[0], VMEMBData[i].RefIdxL0[1], 
                VMEMBData[i].RefIdxL0[2], VMEMBData[i].RefIdxL0[3]);

            fprintf(fText, "RefIdx1 = \n\t%d\t%d\n\t%d\t%d\n", 
                VMEMBData[i].RefIdxL1[0], VMEMBData[i].RefIdxL1[1], 
                VMEMBData[i].RefIdxL1[2], VMEMBData[i].RefIdxL1[3]);

            fprintf(fText, "\n16 L0 Motion Vectors [x, y]\n");

            fprintf(fText, "\t[%d, %d]\t[%d, %d]\t[%d, %d]\t[%d, %d]\n",
                VMEMVData[i].MVL0[0].x, VMEMVData[i].MVL0[0].y,
                VMEMVData[i].MVL0[1].x, VMEMVData[i].MVL0[1].y,
                VMEMVData[i].MVL0[4].x, VMEMVData[i].MVL0[4].y,
                VMEMVData[i].MVL0[5].x, VMEMVData[i].MVL0[5].y);

            fprintf(fText, "\t[%d, %d]\t[%d, %d]\t[%d, %d]\t[%d, %d]\n",
                VMEMVData[i].MVL0[2].x, VMEMVData[i].MVL0[2].y,
                VMEMVData[i].MVL0[3].x, VMEMVData[i].MVL0[3].y,
                VMEMVData[i].MVL0[6].x, VMEMVData[i].MVL0[6].y,
                VMEMVData[i].MVL0[7].x, VMEMVData[i].MVL0[7].y);

            fprintf(fText, "\t[%d, %d]\t[%d, %d]\t[%d, %d]\t[%d, %d]\n",
                VMEMVData[i].MVL0[8].x, VMEMVData[i].MVL0[8].y,
                VMEMVData[i].MVL0[9].x, VMEMVData[i].MVL0[9].y,
                VMEMVData[i].MVL0[12].x, VMEMVData[i].MVL0[12].y,
                VMEMVData[i].MVL0[13].x, VMEMVData[i].MVL0[13].y);

            fprintf(fText, "\t[%d, %d]\t[%d, %d]\t[%d, %d]\t[%d, %d]\n",
                VMEMVData[i].MVL0[10].x, VMEMVData[i].MVL0[10].y,
                VMEMVData[i].MVL0[11].x, VMEMVData[i].MVL0[11].y,
                VMEMVData[i].MVL0[14].x, VMEMVData[i].MVL0[14].y,
                VMEMVData[i].MVL0[15].x, VMEMVData[i].MVL0[15].y);


            fprintf(fText, "\n16 L1 Motion Vectors [x, y]\n");

            fprintf(fText, "\t[%d, %d]\t[%d, %d]\t[%d, %d]\t[%d, %d]\n",
                VMEMVData[i].MVL1[0].x, VMEMVData[i].MVL1[0].y,
                VMEMVData[i].MVL1[1].x, VMEMVData[i].MVL1[1].y,
                VMEMVData[i].MVL1[4].x, VMEMVData[i].MVL1[4].y,
                VMEMVData[i].MVL1[5].x, VMEMVData[i].MVL1[5].y);

            fprintf(fText, "\t[%d, %d]\t[%d, %d]\t[%d, %d]\t[%d, %d]\n",
                VMEMVData[i].MVL1[2].x, VMEMVData[i].MVL1[2].y,
                VMEMVData[i].MVL1[3].x, VMEMVData[i].MVL1[3].y,
                VMEMVData[i].MVL1[6].x, VMEMVData[i].MVL1[6].y,
                VMEMVData[i].MVL1[7].x, VMEMVData[i].MVL1[7].y);

            fprintf(fText, "\t[%d, %d]\t[%d, %d]\t[%d, %d]\t[%d, %d]\n",
                VMEMVData[i].MVL1[8].x, VMEMVData[i].MVL1[8].y,
                VMEMVData[i].MVL1[9].x, VMEMVData[i].MVL1[9].y,
                VMEMVData[i].MVL1[12].x, VMEMVData[i].MVL1[12].y,
                VMEMVData[i].MVL1[13].x, VMEMVData[i].MVL1[13].y);

            fprintf(fText, "\t[%d, %d]\t[%d, %d]\t[%d, %d]\t[%d, %d]\n",
                VMEMVData[i].MVL1[10].x, VMEMVData[i].MVL1[10].y,
                VMEMVData[i].MVL1[11].x, VMEMVData[i].MVL1[11].y,
                VMEMVData[i].MVL1[14].x, VMEMVData[i].MVL1[14].y,
                VMEMVData[i].MVL1[15].x, VMEMVData[i].MVL1[15].y);
        }

        fprintf(fText, "\nTargetSizeInWord        = %d\n",  VMEMBData[i].TargetSizeInWord);
        fprintf(fText, "MaxSizeInWord           = %d\n\n",  VMEMBData[i].MaxSizeInWord);
    }
}
#endif // MFX_VA_WIN
/* EOF */
