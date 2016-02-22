//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2016 Intel Corporation. All Rights Reserved.
//

#include "pipeline_fei.h"

mfxStatus CheckOptions(sInputParams* pParams);
mfxStatus CheckDRCParams(sInputParams* pParams);

void PrintHelp(msdk_char *strAppName, msdk_char *strErrorMessage)
{
    msdk_printf(MSDK_STRING("AVC FEI Encoding Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);

    if (strErrorMessage)
    {
        msdk_printf(MSDK_STRING("Error: %s\n"), strErrorMessage);
    }

    msdk_printf(MSDK_STRING("Usage: %s [<options>] -i InputYUVFile -o OutputEncodedFile -w width -h height\n"), strAppName);
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Only AVC is supported for FEI encoding.\n"));
    msdk_printf(MSDK_STRING("Options: \n"));
    msdk_printf(MSDK_STRING("   [-i::h264|mpeg2|vc1] - set input encoded file and decoder type\n"));
    msdk_printf(MSDK_STRING("   [-nv12] - input is in NV12 color format, if not specified YUV420 is expected\n"));
    msdk_printf(MSDK_STRING("   [-tff|bff] - input stream is interlaced, top|bottom field first, if not specified progressive is expected\n"));
    msdk_printf(MSDK_STRING("   [-single_field_processing] - single-field coding mode, one call for each field, tff/bff option required\n"));
    msdk_printf(MSDK_STRING("   [-bref] - arrange B frames in B pyramid reference structure\n"));
    msdk_printf(MSDK_STRING("   [-nobref] - do not use B-pyramid (by default the decision is made by library)\n"));
    msdk_printf(MSDK_STRING("   [-idr_interval size] - idr interval, default 0 means every I is an IDR, 1 means every other I frame is an IDR etc\n"));
    msdk_printf(MSDK_STRING("   [-f frameRate] - video frame rate (frames per second)\n"));
    msdk_printf(MSDK_STRING("   [-b bitRate] - encoded bit rate (KBits per second), valid for H.264, H.265, MPEG2 and MVC encoders \n"));
    msdk_printf(MSDK_STRING("   [-u speed|quality|balanced] - target usage, valid for H.264, H.265, MPEG2 and MVC encoders\n"));
    msdk_printf(MSDK_STRING("   [-n number] - number of frames to process\n"));
    msdk_printf(MSDK_STRING("   [-timeout seconds] - set time to run processing in seconds\n"));
    msdk_printf(MSDK_STRING("   [-r distance] - Distance between I- or P- key frames (1 means no B-frames) (0 - by default(I frames))\n"));
    msdk_printf(MSDK_STRING("   [-g size] - GOP size (1(default) means I-frames only)\n"));
    msdk_printf(MSDK_STRING("   [-l numSlices] - number of slices \n"));
    msdk_printf(MSDK_STRING("   [-x numRefs]   - number of reference frames \n"));
    msdk_printf(MSDK_STRING("   [-qp qp_value] - QP value for frames\n"));
    msdk_printf(MSDK_STRING("   [-gop_opt closed|strict] - GOP optimization flags (can be used together)\n"));
    msdk_printf(MSDK_STRING("   [-trellis value] - bitfield: 0 = default, 1 = off, 2 = on for I frames, 4 = on for P frames, 8 = on for B frames (ENCODE only)\n"));
    msdk_printf(MSDK_STRING("   [-preenc ds_strength] - use extended FEI interface PREENC (RC is forced to constant QP)\n"));
    msdk_printf(MSDK_STRING("                            if ds_strength parameter is missed or less than 2, PREENC is used on the full resolution\n"));
    msdk_printf(MSDK_STRING("                            otherwise PREENC is used on downscaled (by VPP resize in ds_strength times) surfaces\n"));
    msdk_printf(MSDK_STRING("   [-encode] - use extended FEI interface ENC+PAK (FEI ENCODE) (RC is forced to constant QP)\n"));
    msdk_printf(MSDK_STRING("   [-encpak] - use extended FEI interface ENC only and PAK only (separate calls)\n"));
    msdk_printf(MSDK_STRING("   [-reset_start] - set Start Frame No. to enable Dynamic Resolution change,please specify frame size with -dstw -dsth\n"));
    msdk_printf(MSDK_STRING("   [-reset_end]   - set End Frame No. to disable Dynamic Resolution change\n"));

    //msdk_printf(MSDK_STRING("   [-enc] - use extended FEI interface ENC (only)\n"));
    msdk_printf(MSDK_STRING("   [-pak] - use extended FEI interface PAK (only)\n"));
    msdk_printf(MSDK_STRING("   [-profile decimal] - set AVC profile\n"));
    msdk_printf(MSDK_STRING("   [-level decimal] - set AVC level\n"));
    msdk_printf(MSDK_STRING("   [-EncodedOrder] - use internal logic for reordering, reading from files will be in encoded order (default is display; ENCODE only)\n"));
    msdk_printf(MSDK_STRING("   [-mbctrl file] - use the input to set MB control for FEI (only ENC+PAK)\n"));
    msdk_printf(MSDK_STRING("   [-mbsize] - with this options size control fields will be used from MB control structure (only ENC+PAK)\n"));
    msdk_printf(MSDK_STRING("   [-mvin file] - use this input to set MV predictor for FEI. PREENC and ENC (ENCODE) expect different structures\n"));
    msdk_printf(MSDK_STRING("   [-repack_preenc_mv] - use this in pair with -mvin to import preenc MVout directly\n"));
    msdk_printf(MSDK_STRING("   [-mvout file] - use this to output MV predictors\n"));
    msdk_printf(MSDK_STRING("   [-mbcode file] - file to output per MB information (structure mfxExtFeiPakMBCtrl) for each frame\n"));
    msdk_printf(MSDK_STRING("   [-mbstat file] - file to output per MB distortions for each frame\n"));
    msdk_printf(MSDK_STRING("   [-mbqp file] - file to input per MB QPs the same for each frame\n"));
    msdk_printf(MSDK_STRING("   [-sys] - use system memory for surfaces (ENCODE only)\n"));
    msdk_printf(MSDK_STRING("   [-pass_headers] - pass SPS, PPS and Slice headers to Media SDK instead of default one (ENC or/and PAK only)\n"));
    msdk_printf(MSDK_STRING("   [-8x8stat] - set 8x8 block for statistic report, default is 16x16 (PREENC only)\n"));
    msdk_printf(MSDK_STRING("   [-search_window value] - specifies one of the predefined search path and window size. In range [1,8] (0 is default).\n"));
    msdk_printf(MSDK_STRING("                            If non-zero value specified: -ref_window_w / _h, -len_sp, -max_len_sp are ignored\n"));
    msdk_printf(MSDK_STRING("   [-ref_window_w width] - width of search region (should be multiple of 4), maximum allowed search window w*h is 2048 for\n"));
    msdk_printf(MSDK_STRING("                            one direction and 1048 for bidirectional search\n"));
    msdk_printf(MSDK_STRING("   [-ref_window_h height] - height of search region (should be multiple of 4), maximum allowed is 32\n"));
    msdk_printf(MSDK_STRING("   [-len_sp length] - defines number of search units in search path. In range [1,63]\n"));
    msdk_printf(MSDK_STRING("   [-search_path value] - defines shape of search path. In range [1,2]\n"));
    msdk_printf(MSDK_STRING("   [-sub_mb_part_mask mask_hex] - specifies which partitions should be excluded from search. 0x00 - enable all (default is 0x77)\n"));
    msdk_printf(MSDK_STRING("   [-sub_pel_mode mode_hex] - specifies sub pixel precision for motion estimation 0x00-0x01-0x03 integer-half-quarter (default is 0)\n"));
    msdk_printf(MSDK_STRING("   [-intra_part_mask mask_hex] - specifies what blocks and sub-blocks partitions are enabled for intra prediction (default is 0)\n"));
    msdk_printf(MSDK_STRING("   [-intra_SAD] - specifies intra distortion adjustment. 0x00 - none, 0x02 - Haar transform (default)\n"));
    msdk_printf(MSDK_STRING("   [-inter_SAD] - specifies inter distortion adjustment. 0x00 - none, 0x02 - Haar transform (default)\n"));
    msdk_printf(MSDK_STRING("   [-adaptive_search] - enables adaptive search\n"));
    msdk_printf(MSDK_STRING("   [-forward_transform] - enables forward transform. Additional stat is calculated and reported, QP required (PREENC only)\n"));
    msdk_printf(MSDK_STRING("   [-repartition_check] - enables additional sub pixel and bidirectional refinements (ENC)\n"));
    msdk_printf(MSDK_STRING("   [-multi_pred_l0] - MVs from neighbor MBs will be used as predictors for L0 prediction list (ENC)\n"));
    msdk_printf(MSDK_STRING("   [-multi_pred_l1] - MVs from neighbor MBs will be used as predictors for L1 prediction list (ENC)\n"));
    msdk_printf(MSDK_STRING("   [-adjust_distortion] - if enabled adds a cost adjustment to distortion, default is RAW distortion (ENC)\n"));
    msdk_printf(MSDK_STRING("   [-n_mvpredictors num] - number of MV predictors, up to 4 is supported (default is 1) (ENC)\n"));
    msdk_printf(MSDK_STRING("   [-colocated_mb_distortion] - provides the distortion between the current and the co-located MB. It has performance impact(ENC)\n"));
    msdk_printf(MSDK_STRING("                                do not use it, unless it is necessary\n"));
    msdk_printf(MSDK_STRING("   [-dblk_idc value] - value of DisableDeblockingIdc (default is 0), in range [0,2]\n"));
    msdk_printf(MSDK_STRING("   [-dblk_alpha value] - value of SliceAlphaC0OffsetDiv2 (default is 0), in range [-6,6]\n"));
    msdk_printf(MSDK_STRING("   [-dblk_beta value] - value of SliceBetaOffsetDiv2 (default is 0), in range [-6,6]\n"));
    msdk_printf(MSDK_STRING("   [-chroma_qpi_offset first_offset] - first offset used for chroma qp in range [-12, 12] (used in PPS, pass_headers should be set)\n"));
    msdk_printf(MSDK_STRING("   [-s_chroma_qpi_offset second_offset] - second offset used for chroma qp in range [-12, 12] (used in PPS, pass_headers should be set)\n"));
    msdk_printf(MSDK_STRING("   [-constrained_intra_pred_flag] - use constrained intra prediction (default is off, used in PPS, pass_headers should be set)\n"));
    msdk_printf(MSDK_STRING("   [-transform_8x8_mode_flag] - enables 8x8 transform, by default only 4x4 is used (used in PPS, pass_headers should be set)\n"));
    msdk_printf(MSDK_STRING("   [-dstw width] - destination picture width, invokes VPP resizing\n"));
    msdk_printf(MSDK_STRING("   [-dsth height] - destination picture height, invokes VPP resizing\n"));
    msdk_printf(MSDK_STRING("   [-perf] - switch on performance mode (no file operations, simplified predictors repacking)\n"));

    // user module options
    msdk_printf(MSDK_STRING("\n"));
}

#define GET_OPTION_POINTER(PTR)         \
{                                       \
    if (2 == msdk_strlen(strInput[i]))      \
    {                                   \
        i++;                            \
        if (strInput[i][0] == MSDK_CHAR('-'))  \
        {                               \
            i = i - 1;                  \
        }                               \
        else                            \
        {                               \
            PTR = strInput[i];          \
        }                               \
    }                                   \
    else                                \
    {                                   \
        PTR = strInput[i] + 2;          \
    }                                   \
}

mfxStatus ParseInputString(msdk_char* strInput[], mfxU8 nArgNum, sInputParams* pParams)
{
    msdk_char* strArgument = MSDK_STRING("");
    msdk_char* stopCharacter;

    bool bRefWSizeSpecified = false, bAlrShownHelp = false, bHeaderValSpecified = false,
         bIDRintSet = false;

    if (1 == nArgNum)
    {
        PrintHelp(strInput[0], NULL);
        return MFX_ERR_UNSUPPORTED;
    }

    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    // parse command line parameters
    for (mfxU8 i = 1; i < nArgNum; i++)
    {
        MSDK_CHECK_POINTER(strInput[i], MFX_ERR_NULL_PTR);

        // process multi-character options
        if (0 == msdk_strncmp(MSDK_STRING("-i::"), strInput[i], msdk_strlen(MSDK_STRING("-i::"))))
        {
            pParams->bDECODE = true;

            mfxStatus sts = StrFormatToCodecFormatFourCC(strInput[i] + 4, pParams->DecodeId);
            if (sts != MFX_ERR_NONE)
            {
                return MFX_ERR_UNSUPPORTED;
            }
            i++;
            msdk_opt_read(strInput[i], pParams->strSrcFile);

            switch (pParams->DecodeId)
            {
            case MFX_CODEC_MPEG2:
            case MFX_CODEC_AVC:
            case MFX_CODEC_VC1:
                break;
            default:
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-encode")))
        {
            pParams->bENCODE = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-encpak")))
        {
            pParams->bENCPAK = true;
        }
        /*else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-enc")))
        {
            pParams->bOnlyENC = true;
        }*/
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-pak")))
        {
            pParams->bOnlyPAK = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-preenc")))
        {
            pParams->bPREENC = true;

            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->preencDSstrength) || pParams->preencDSstrength < 0)
            {
                pParams->preencDSstrength = 0;
                i--;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-profile")))
        {
            i++;
            pParams->CodecProfile = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-level")))
        {
            i++;
            pParams->CodecLevel = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-EncodedOrder")))
        {
            pParams->EncodedOrder = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mvin")))
        {
            pParams->mvinFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-repack_preenc_mv")))
        {
            pParams->bRepackPreencMV = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mvout")))
        {
            pParams->mvoutFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbcode")))
        {
            pParams->mbcodeoutFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbstat")))
        {
            pParams->mbstatoutFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbqp")))
        {
            pParams->mbQpFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbsize")))
        {
            pParams->bMBSize = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbctrl")))
        {
            pParams->mbctrinFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-nv12")))
        {
            pParams->ColorFormat = MFX_FOURCC_NV12;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-tff")))
        {
            pParams->nPicStruct = MFX_PICSTRUCT_FIELD_TFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bff")))
        {
            pParams->nPicStruct = MFX_PICSTRUCT_FIELD_BFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-single_field_processing")))
        {
            pParams->bFieldProcessingMode = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bref")))
        {
            pParams->bRefType = MFX_B_REF_PYRAMID;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-nobref")))
        {
            pParams->bRefType = MFX_B_REF_OFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-idr_interval")))
        {
            bIDRintSet = true;
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nIdrInterval))
            {
                PrintHelp(strInput[0], MSDK_STRING("IdrInterval is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-qp")))
        {
            i++;
            pParams->QP = (mfxU8)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-gop_opt")))
        {
            i++;
            if (0 == msdk_strcmp(strInput[i], MSDK_STRING("closed")))
            {
                pParams->GopOptFlag |= MFX_GOP_CLOSED;
            }
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("strict")))
            {
                pParams->GopOptFlag |= MFX_GOP_STRICT;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-trellis")))
        {
            i++;
            pParams->Trellis = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
#if D3D_SURFACES_SUPPORT
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d")))
        {
            pParams->memType = D3D9_MEMORY;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d11")))
        {
            pParams->memType = D3D11_MEMORY;
        }
#endif
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-sys")))
        {
            pParams->memType = SYSTEM_MEMORY;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-pass_headers")))
        {
            pParams->bPassHeaders = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-8x8stat")))
        {
            pParams->Enable8x8Stat = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-forward_transform")))
        {
            pParams->FTEnable = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-adaptive_search")))
        {
            pParams->AdaptiveSearch = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-repartition_check")))
        {
            pParams->RepartitionCheckEnable = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-multi_pred_l0")))
        {
            pParams->MultiPredL0 = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-multi_pred_l1")))
        {
            pParams->MultiPredL1 = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-adjust_distortion")))
        {
            pParams->DistortionType = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-colocated_mb_distortion")))
        {
            pParams->ColocatedMbDistortion = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-search_window")))
        {
            i++;
            pParams->SearchWindow = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-ref_window_w")))
        {
            i++;
            pParams->RefWidth = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
            bRefWSizeSpecified = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-ref_window_h")))
        {
            i++;
            pParams->RefHeight = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
            bRefWSizeSpecified = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-len_sp")))
        {
            i++;
            pParams->LenSP = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-search_path")))
        {
            i++;
            pParams->SearchPath = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-sub_mb_part_mask")))
        {
            i++;
            pParams->SubMBPartMask = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 16);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-sub_pel_mode")))
        {
            i++;
            pParams->SubPelMode = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 16);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-intra_part_mask")))
        {
            i++;
            pParams->IntraPartMask = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 16);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-intra_SAD")))
        {
            i++;
            pParams->IntraSAD = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 16);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-inter_SAD")))
        {
            i++;
            pParams->InterSAD = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 16);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-n_mvpredictors")))
        {
            i++;
            pParams->bNPredSpecified = true;
            pParams->NumMVPredictors = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dblk_idc")))
        {
            i++;
            pParams->DisableDeblockingIdc = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dblk_alpha")))
        {
            i++;
            pParams->SliceAlphaC0OffsetDiv2 = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dblk_beta")))
        {
            i++;
            pParams->SliceBetaOffsetDiv2 = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-chroma_qpi_offset")))
        {
            i++;
            pParams->ChromaQPIndexOffset = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
            bHeaderValSpecified = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-s_chroma_qpi_offset")))
        {
            i++;
            pParams->SecondChromaQPIndexOffset = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
            bHeaderValSpecified = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-constrained_intra_pred_flag")))
        {
            pParams->ConstrainedIntraPredFlag = true;
            bHeaderValSpecified = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-transform_8x8_mode_flag")))
        {
            pParams->Transform8x8ModeFlag = true;
            bHeaderValSpecified = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dstw")))
        {
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nDstWidth))
            {
                PrintHelp(strInput[0], MSDK_STRING("Destination picture Width is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
            if (pParams->bDynamicRC)
            {
               pParams->nDrcWidth.push_back(pParams->nDstWidth);
               pParams->MaxDrcWidth = pParams->nDstWidth > pParams->MaxDrcWidth? pParams->nDstWidth : pParams->MaxDrcWidth;
               pParams->MaxDrcWidth = pParams->MaxDrcWidth >  pParams->nWidth ?pParams->MaxDrcWidth : pParams->nWidth;
               pParams->nDstWidth = pParams->MaxDrcWidth;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dsth")))
        {
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nDstHeight))
            {
                PrintHelp(strInput[0], MSDK_STRING("Destination picture Height is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
            if(pParams->bDynamicRC)
            {
                pParams->nDrcHeight.push_back(pParams->nDstHeight);
                pParams->MaxDrcHeight = pParams->nDstHeight > pParams->MaxDrcHeight? pParams->nDstHeight :pParams->MaxDrcHeight;
                pParams->MaxDrcHeight = pParams->MaxDrcHeight >  pParams->nHeight ?pParams->MaxDrcHeight : pParams->nHeight;
                pParams->nDstHeight = pParams->MaxDrcHeight;
            }

        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-timeout")))
        {
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nTimeout))
            {
                PrintHelp(strInput[0], MSDK_STRING("Timeout is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-perf")))
        {
            pParams->bPerfMode = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-reset_start")))
        {
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nResetStart))
            {
                PrintHelp(strInput[0], MSDK_STRING("Timeout is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
             //for first -resetstart
            if (!pParams->bDynamicRC)
            {
                if (pParams->nDstWidth && pParams->nDstHeight)
                {
                    pParams->nDRCdefautW = pParams->nDstWidth;
                    pParams->nDRCdefautH = pParams->nDstHeight;
                    pParams->MaxDrcWidth = pParams->nDstWidth;
                    pParams->MaxDrcHeight = pParams->nDstHeight;
                }
                else if (pParams->nWidth && pParams->nHeight)
                {
                    pParams->nDRCdefautW = pParams->nWidth;
                    pParams->nDRCdefautH = pParams->nHeight;
                }
                if (0 != pParams->nResetStart)
                {
                   pParams->nResetEnd = pParams->nResetStart-1;
                   pParams->nDrcEnd.push_back(pParams->nResetEnd);
                   pParams->nDrcStart.push_back(0);
                   pParams->nDrcWidth.push_back(pParams->nDRCdefautW);
                   pParams->nDrcHeight.push_back(pParams->nDRCdefautH);
                }
                pParams->bDynamicRC = true;
            }
            pParams->nDrcStart.push_back(pParams->nResetStart);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-reset_end")))
        {
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nResetEnd))
            {
                PrintHelp(strInput[0], MSDK_STRING("Timeout is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
            pParams->nDrcEnd.push_back(pParams->nResetEnd);
        }
        else // 1-character options
        {
            switch (strInput[i][1])
            {
            case MSDK_CHAR('u'):
                GET_OPTION_POINTER(strArgument);
                pParams->nTargetUsage = StrToTargetUsage(strArgument);
                break;
            case MSDK_CHAR('w'):
                GET_OPTION_POINTER(strArgument);
                pParams->nWidth = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('h'):
                GET_OPTION_POINTER(strArgument);
                pParams->nHeight = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('f'):
                GET_OPTION_POINTER(strArgument);
                pParams->dFrameRate = (mfxF64)msdk_strtod(strArgument, &stopCharacter);
                break;
            case MSDK_CHAR('n'):
                GET_OPTION_POINTER(strArgument);
                pParams->nNumFrames = (mfxU32)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('g'):
                GET_OPTION_POINTER(strArgument);
                pParams->gopSize = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('r'):
                GET_OPTION_POINTER(strArgument);
                pParams->refDist = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('b'):
                GET_OPTION_POINTER(strArgument);
                pParams->nBitRate = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('l'):
                GET_OPTION_POINTER(strArgument);
                pParams->numSlices = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('x'):
                GET_OPTION_POINTER(strArgument);
                pParams->numRef = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('i'):
                GET_OPTION_POINTER(strArgument);
                if (msdk_strlen(strArgument) < sizeof(pParams->strSrcFile)){
                    msdk_strcopy(pParams->strSrcFile, strArgument);
                }else{
                    PrintHelp(strInput[0], MSDK_STRING("Too long input filename (limit is 1023 characters)!"));
                    return MFX_ERR_UNSUPPORTED;
                }
                break;
            case MSDK_CHAR('o'):
                GET_OPTION_POINTER(strArgument);
                pParams->dstFileBuff.push_back(strArgument);
                break;
            case MSDK_CHAR('?'):
                PrintHelp(strInput[0], NULL);
                return MFX_ERR_UNSUPPORTED;
            default:
                if (!bAlrShownHelp){
                    msdk_printf(MSDK_STRING("\nError: Unknown option %s\n\n"), strInput[i]);
                    PrintHelp(strInput[0], NULL);// MSDK_STRING("Unknown options"));
                    bAlrShownHelp = true;
                }else {
                    msdk_printf(MSDK_STRING("\nUnknown option %s\n\n"), strInput[i]);
                }
            }
        }
    }

    if (!( (pParams->bPREENC && !pParams->bOnlyENC && !pParams->bOnlyPAK && !pParams->bENCPAK  && !pParams->bENCODE) ||
          //(!pParams->bPREENC &&  pParams->bOnlyENC && !pParams->bOnlyPAK && !pParams->bENCPAK  && !pParams->bENCODE) ||
          (!pParams->bPREENC && !pParams->bOnlyENC &&  pParams->bOnlyPAK && !pParams->bENCPAK  && !pParams->bENCODE) ||
          (!pParams->bPREENC && !pParams->bOnlyENC && !pParams->bOnlyPAK && !pParams->bENCPAK  &&  pParams->bENCODE) ||
          (!pParams->bPREENC && !pParams->bOnlyENC && !pParams->bOnlyPAK &&  pParams->bENCPAK  && !pParams->bENCODE) ||
           //(pParams->bPREENC &&  pParams->bOnlyENC && !pParams->bOnlyPAK && !pParams->bENCPAK  && !pParams->bENCODE) ||
           (pParams->bPREENC && !pParams->bOnlyENC && !pParams->bOnlyPAK && !pParams->bENCPAK  &&  pParams->bENCODE) ||
           (pParams->bPREENC && !pParams->bOnlyENC && !pParams->bOnlyPAK &&  pParams->bENCPAK  && !pParams->bENCODE) )
    ){
        if (bAlrShownHelp){
            msdk_printf(MSDK_STRING("\nUnsupported pipeline!\n"));

            msdk_printf(MSDK_STRING("Supported pipelines are:\n"));
            msdk_printf(MSDK_STRING("PREENC\n"));
            //msdk_printf(MSDK_STRING("ENC\n"));
            msdk_printf(MSDK_STRING("PAK\n"));
            msdk_printf(MSDK_STRING("ENCODE\n"));
            msdk_printf(MSDK_STRING("ENC + PAK (ENCPAK)\n"));
            //msdk_printf(MSDK_STRING("PREENC + ENC\n"));
            msdk_printf(MSDK_STRING("PREENC + ENCODE\n"));
            msdk_printf(MSDK_STRING("PREENC + ENC + PAK (PREENC + ENCPAK)\n"));
        } else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported pipeline!\nSupported pipelines are:\nPREENC\nENC\nPAK\nENCODE\nENC + PAK (ENCPAK)\nPREENC + ENC\nPREENC + ENCODE\nPREENC + ENC + PAK (PREENC + ENCPAK)\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (!!pParams->preencDSstrength)
    {
        switch (pParams->preencDSstrength)
        {
        case 2:
        case 4:
        case 8:
            break;
        default:
            msdk_printf(MSDK_STRING("\nUnsupported strength of PREENC downscaling!\n"));
            return MFX_ERR_UNSUPPORTED;
        }
    }

    // check if all mandatory parameters were set
    if (0 == msdk_strlen(pParams->strSrcFile))
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nSource file name not found\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Source file name not found"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pParams->dstFileBuff.empty() ) &&
        (pParams->bENCPAK || pParams->bOnlyPAK) )
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nDestination file name not found\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Destination file name not found"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (0 == pParams->nWidth || 0 == pParams->nHeight)
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\n-w, -h must be specified\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("-w, -h must be specified"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (MFX_TARGETUSAGE_BEST_QUALITY != pParams->nTargetUsage && MFX_TARGETUSAGE_BEST_SPEED != pParams->nTargetUsage)
    {
        pParams->nTargetUsage = MFX_TARGETUSAGE_BALANCED;
    }

    if (pParams->dFrameRate <= 0)
    {
        pParams->dFrameRate = 30;
    }

    // if no destination picture width or height wasn't specified set it to the source picture size
    if (pParams->nDstWidth == 0)
    {
        pParams->nDstWidth = pParams->nWidth;
    }

    if (pParams->nDstHeight == 0)
    {
        pParams->nDstHeight = pParams->nHeight;
    }

    // calculate default bitrate based on the resolution (a parameter for encoder, so Dst resolution is used)
    if (pParams->nBitRate == 0)
    {
        pParams->nBitRate = CalculateDefaultBitrate(pParams->CodecId, pParams->nTargetUsage, pParams->nDstWidth,
            pParams->nDstHeight, pParams->dFrameRate);
    }

    // if nv12 option wasn't specified we expect input YUV file in YUV420 color format
    if (!pParams->ColorFormat)
    {
        pParams->ColorFormat = MFX_FOURCC_YV12;
    }

    if (pParams->nPicStruct == MFX_PICSTRUCT_UNKNOWN)
    {
        pParams->nPicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    if (pParams->SearchWindow < 0 || pParams->SearchWindow > 8)
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported value of -search_window parameter, must be in range [0, 8]!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -search_window parameter, must be in range [0, 8]!"));
        return MFX_ERR_UNSUPPORTED; // valid values 0-8
    }

    if (bRefWSizeSpecified && pParams->SearchWindow != 0){
        msdk_printf(MSDK_STRING("\n-search_window specified, -ref_window_w and -ref_window_h will be ignored.\n"));
        switch (pParams->SearchWindow){
        case 1: msdk_printf(MSDK_STRING("Search window size is 24x24 (4 SUs); Path - Tiny\n\n"));
            break;
        case 2: msdk_printf(MSDK_STRING("Search window size is 28x28 (9 SUs); Path - Small\n\n"));
            break;
        case 3: msdk_printf(MSDK_STRING("Search window size is 48x40 (16 SUs); Path - Diamond\n\n"));
            break;
        case 4: msdk_printf(MSDK_STRING("Search window size is 48x40 (32 SUs); Path - Large Diamond\n\n"));
            break;
        case 5: msdk_printf(MSDK_STRING("Search window size is 48x40 (48 SUs); Path - Exhaustive\n\n"));
            break;
        case 6: msdk_printf(MSDK_STRING("Search window size is 64x32 (16 SUs); Path - Horizontal Diamond\n\n"));
            break;
        case 7: msdk_printf(MSDK_STRING("Search window size is 64x32 (32 SUs); Path - Horizontal Large Diamond\n\n"));
            break;
        case 8: msdk_printf(MSDK_STRING("Search window size is 64x32 (48 SUs); Path - Horizontal Exhaustive\n\n"));
            break;
        default:
            break;
        }
    }

    if ((pParams->SearchWindow != 0) && (pParams->RefHeight <= 0 || pParams->RefWidth <= 0 || pParams->RefHeight % 4 != 0 || pParams->RefWidth % 4 != 0 ||
         pParams->RefHeight*pParams->RefWidth > 2048))
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported value of search window size. -ref_window_h, -ref_window_w parameters, must be multiple of 4!\n"
                                    "For bi-prediction window w*h must less than 1024 and less 2048 otherwise.\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported value of search window size. -ref_window_h, -ref_window_w parameters, must be multiple of 4!\n"
                                               "For bi-prediction window w*h must less than 1024 and less 2048 otherwise."));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->LenSP < 1 || pParams->LenSP > 63)
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported value of -len_sp parameter, must be in range [1, 63]!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -len_sp parameter, must be in range [1, 63]!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->SearchPath < 1 || pParams->SearchPath > 2)
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported value of -search_path parameter, must be in range [1, 2]!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -search_path parameter, must be in range [1, 2]!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->SubMBPartMask >= 0x7f ){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported value of -sub_mb_part_mask parameter, 0x7f disables all partitions!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -sub_mb_part_mask parameter, 0x7f disables all partitions!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->SubPelMode != 0x00 && pParams->SubPelMode != 0x01 && pParams->SubPelMode != 0x03){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported value of -sub_pel_mode parameter, must be 0x00/0x01/0x03!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -sub_pel_mode parameter, must be 0x00/0x01/0x03!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->IntraPartMask >= 0x07){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported value of -inra_part_mask parameter, 0x07 disables all partitions!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -inra_part_mask parameter, 0x07 disables all partitions!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->IntraSAD != 0x02 && pParams->IntraSAD != 0x00){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported value of -intra_SAD, must be 0x00 or 0x02!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -intra_SAD, must be 0x00 or 0x02!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->InterSAD != 0x02 && pParams->InterSAD != 0x00){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported value of -inter_SAD, must be 0x00 or 0x02!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -inter_SAD, must be 0x00 or 0x02!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->NumMVPredictors < 0 || pParams->NumMVPredictors > 4){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported value number of MV predictors!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported value number of MV predictors!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->Trellis > (MFX_TRELLIS_I | MFX_TRELLIS_P | MFX_TRELLIS_B)){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported trellis value!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported trellis value!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->DisableDeblockingIdc > 2){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported DisableDeblockingIdc value!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported DisableDeblockingIdc value!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->SliceAlphaC0OffsetDiv2 > 6 || pParams->SliceAlphaC0OffsetDiv2 < -6){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported SliceAlphaC0OffsetDiv2 value!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported SliceAlphaC0OffsetDiv2 value!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->SliceBetaOffsetDiv2 > 6 || pParams->SliceBetaOffsetDiv2 < -6){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported SliceBetaOffsetDiv2 value!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported SliceBetaOffsetDiv2 value!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pParams->bENCPAK || pParams->bOnlyENC || pParams->bOnlyPAK) && !pParams->bPassHeaders && (pParams->nIdrInterval || pParams->bRefType != MFX_B_REF_OFF)){
        if (bIDRintSet || pParams->bRefType == MFX_B_REF_PYRAMID){
            msdk_printf(MSDK_STRING("\nWARNING: Specified B-pyramid/IDR-interval control(s) for ENC+PAK would be ignored!\n"));
            msdk_printf(MSDK_STRING("           Please use them together with -pass_headers option\n"));
        }

        pParams->bRefType = MFX_B_REF_OFF;
        pParams->nIdrInterval = 0;
    }

    if ((pParams->bENCPAK || pParams->bOnlyENC || pParams->bOnlyPAK) /*&& (pParams->nPicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))*/ && !pParams->bPassHeaders){
        msdk_printf(MSDK_STRING("\nWARNING: ENCPAK uses SliceHeader to store references; -pass_headers flag forced.\n"));

        pParams->bPassHeaders = true;
    }

    if (bHeaderValSpecified && !pParams->bPassHeaders){
        msdk_printf(MSDK_STRING("\nWARNING: Specified SPS/PPS/Slice header parameters would be ignored!\n"));
        msdk_printf(MSDK_STRING("           Please use them together with -pass_headers option\n"));
    }

    if (pParams->bPassHeaders && (pParams->ChromaQPIndexOffset > 12 || pParams->ChromaQPIndexOffset < -12)){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported ChromaQPIndexOffset value!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported ChromaQPIndexOffset value!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->bPassHeaders && (pParams->SecondChromaQPIndexOffset > 12 || pParams->SecondChromaQPIndexOffset < -12)){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nUnsupported SecondChromaQPIndexOffset value!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("Unsupported SecondChromaQPIndexOffset value!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->EncodedOrder && pParams->bENCODE && pParams->nNumFrames == 0){
        msdk_printf(MSDK_STRING("\nWARNING: without number of frames setting (-n) last frame of FEI ENCODE in Encoded Order\n"));
        msdk_printf(MSDK_STRING("           could be non-bitexact with last frame in FEI ENCODE with Display Order!\n"));
    }

    if (pParams->bPerfMode && (pParams->mvinFile || pParams->mvoutFile || pParams->mbctrinFile || pParams->mbstatoutFile || pParams->mbcodeoutFile || pParams->mbQpFile))
    {
        msdk_printf(MSDK_STRING("\nWARNING: All file operations would be ignored in performance mode!\n"));

        pParams->mvinFile      = NULL;
        pParams->mvoutFile     = NULL;
        pParams->mbctrinFile   = NULL;
        pParams->mbstatoutFile = NULL;
        pParams->mbcodeoutFile = NULL;
        pParams->mbQpFile      = NULL;
    }

    if (pParams->bENCODE || pParams->bENCPAK || pParams->bOnlyENC || pParams->bOnlyPAK){
        if (!pParams->CodecProfile)
            pParams->CodecProfile = 100; // MFX_PROFILE_AVC_HIGH

        if (!pParams->CodecLevel)
            pParams->CodecLevel = 41;    // MFX_LEVEL_AVC_41
    }

    /* One slice by default */
    if (0 == pParams->numSlices)
        pParams->numSlices = 1;

    /* One Ref by default */
    if (0 == pParams->numRef)
        pParams->numRef = 1;

    if ((pParams->nPicStruct == MFX_PICSTRUCT_FIELD_TFF || pParams->nPicStruct == MFX_PICSTRUCT_FIELD_BFF) && pParams->numRef == 1){
        msdk_printf(MSDK_STRING("\nWARNING: minimal number of references on interlaced content is 2!\n"));
        msdk_printf(MSDK_STRING("           Current number of references extended.\n"));

        pParams->numRef = 2;
    }

    return MFX_ERR_NONE;
}

#if defined(_WIN32) || defined(_WIN64)
int _tmain(int argc, msdk_char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    sInputParams Params = {};   // input parameters from command line
    std::auto_ptr<CEncodingPipeline>  pPipeline;

    mfxStatus sts = MFX_ERR_NONE; // return value check

    Params.CodecId  = MFX_CODEC_AVC; //only AVC is supported
    Params.DecodeId = 0; //default (invalid) value
    Params.nPicStruct = MFX_PICSTRUCT_UNKNOWN;
    Params.nNumFrames = 0; //unlimited
    Params.nTimeout   = 0; //unlimited
    Params.refDist = 1; //only I frames
    Params.gopSize = 1; //only I frames
    Params.numRef  = 1; //one ref by default
    Params.nIdrInterval = 0xffff; //infinite
    Params.bDECODE   = false;
    Params.bENCODE   = false;
    Params.bENCPAK   = false;
    Params.bOnlyENC  = false;
    Params.bOnlyPAK  = false;
    Params.bPREENC   = false;
    Params.bPerfMode = false;
    Params.bNPredSpecified = false;
    Params.preencDSstrength = 0;
    Params.EncodedOrder    = false;
    Params.Enable8x8Stat   = false;
    Params.FTEnable        = false;
    Params.AdaptiveSearch  = false;
    Params.DistortionType  = false;
    Params.bPassHeaders    = false;
    Params.RepartitionCheckEnable = false;
    Params.MultiPredL0 = false;
    Params.MultiPredL1 = false;
    Params.ColocatedMbDistortion    = false;
    Params.ConstrainedIntraPredFlag = false;
    Params.Transform8x8ModeFlag     = false;
    Params.mvinFile      = NULL;
    Params.mvoutFile     = NULL;
    Params.mbctrinFile   = NULL;
    Params.mbstatoutFile = NULL;
    Params.mbcodeoutFile = NULL;
    Params.mbQpFile      = NULL;
    Params.bRepackPreencMV      = false;
    Params.bFieldProcessingMode = false;
    Params.bMBSize  = false;
    Params.bDynamicRC = false;
    Params.nResetStart  = 0;
    Params.nResetEnd    = 0;
    Params.MaxDrcWidth  = 0;
    Params.MaxDrcHeight = 0;
    Params.nDRCdefautW  = 0;
    Params.nDRCdefautH  = 0;
    Params.memType  = D3D9_MEMORY; //only HW memory is supported (ENCODE supports SW memory)
    Params.bRefType = MFX_B_REF_UNKNOWN; //let MSDK library to decide wheather to use B-pyramid or not
    Params.QP              = 26; //default qp value
    Params.SearchWindow    = 5;  //48x40 (48 SUs)
    Params.RefWidth        = 32;
    Params.RefHeight       = 32;
    Params.LenSP           = 57;
    Params.SearchPath      = 2;    // exhaustive
    Params.SubMBPartMask   = 0x00; // all enabled
    Params.IntraPartMask   = 0x00; // all enabled
    Params.SubPelMode      = 0x03; // quarter-pixel
    Params.IntraSAD        = 0x02; // Haar transform
    Params.InterSAD        = 0x02; // Haar transform
    Params.NumMVPredictors = 1;
    Params.GopOptFlag      = 0;
    Params.CodecProfile    = 0;
    Params.CodecLevel      = 0;
    Params.Trellis         = 0;    // MFX_TRELLIS_UNKNOWN
    Params.DisableDeblockingIdc   = 0;
    Params.SliceAlphaC0OffsetDiv2 = 0;
    Params.SliceBetaOffsetDiv2    = 0;
    Params.ChromaQPIndexOffset       = 0;
    Params.SecondChromaQPIndexOffset = 0;

    sts = ParseInputString(argv, (mfxU8)argc, &Params);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    sts = CheckOptions(&Params);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    sts = CheckDRCParams(&Params);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    pPipeline.reset(new CEncodingPipeline);

    MSDK_CHECK_POINTER(pPipeline.get(), MFX_ERR_MEMORY_ALLOC);

    sts = pPipeline->Init(&Params);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

    pPipeline->PrintInfo();

    msdk_printf(MSDK_STRING("Processing started\n"));

    msdk_tick startTime;
    msdk_tick frequency = msdk_time_get_frequency();
    startTime = msdk_time_get_tick();

    for (;;)
    {
        sts = pPipeline->Run();

        if (MFX_ERR_DEVICE_LOST == sts || MFX_ERR_DEVICE_FAILED == sts)
        {
            msdk_printf(MSDK_STRING("\nERROR: Hardware device was lost or returned an unexpected error. Recovering...\n"));
            sts = pPipeline->ResetDevice();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

            sts = pPipeline->ResetMFXComponents(&Params);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            continue;
        }
        else
        {
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            break;
        }
    }

    pPipeline->Close();
    msdk_printf(MSDK_STRING("\nProcessing finished after %.2f sec \n"), MSDK_GET_TIME(msdk_time_get_tick(), startTime, frequency));

    return 0;
}

mfxStatus CheckOptions(sInputParams* pParams)
{
    mfxStatus sts = MFX_ERR_NONE;
    if(pParams->bENCODE && pParams->dstFileBuff.size() == 0) {
        fprintf(stderr,"Output bitstream file should be specified for ENC+PAK (-o)\n");
        sts = MFX_ERR_UNSUPPORTED;
    }

    /*
    if (pParams->bPREENC && pParams->nPicStruct == MFX_PICSTRUCT_FIELD_BFF) {
        fprintf(stderr, "Preenc doesn't support bff, use tff\n");
        sts = MFX_ERR_UNSUPPORTED;
    } */

    if (!pParams->bENCODE && pParams->memType == SYSTEM_MEMORY) {
        fprintf(stderr, "Only ENCODE supports SW memory\n");
        sts = MFX_ERR_UNSUPPORTED;
    }

    if (pParams->bPREENC && pParams->bFieldProcessingMode && pParams->nPicStruct == MFX_PICSTRUCT_FIELD_BFF)
    {
        fprintf(stderr, "PREENC: Field Processing mode works only for TFF\n");
        sts = MFX_ERR_UNSUPPORTED;
    }

    if (pParams->bFieldProcessingMode &&
        !((pParams->nPicStruct == MFX_PICSTRUCT_FIELD_BFF) || (pParams->nPicStruct == MFX_PICSTRUCT_FIELD_TFF)))
    {
        fprintf(stderr, "Field Processing mode works only with interlaced content (TFF or BFF)\n");
        sts = MFX_ERR_UNSUPPORTED;
    }

    return sts;
}
mfxStatus CheckDRCParams(sInputParams* pParams)
{
    mfxStatus sts = MFX_ERR_NONE;
    if (!pParams->bDynamicRC)
    {
        return MFX_ERR_NONE;
    }
    if (pParams->bENCPAK || pParams->bOnlyPAK || pParams->bPREENC)
    {
        fprintf(stderr, "Only ENCODE supports Dynamic Resolution Change\n");
        sts = MFX_ERR_UNSUPPORTED;
    }
    if (pParams->nDrcStart.size() != pParams->nDrcEnd.size())
   {
        fprintf(stderr, "Please Check -reset_start and -reset_End\n");
        sts = MFX_ERR_UNSUPPORTED;
    }

    if (pParams->nDrcWidth.size() != pParams->nDrcHeight.size())
    {
        fprintf(stderr, "Please Check -dstw and -dsth\n");
        sts = MFX_ERR_UNSUPPORTED;
    }

    if (pParams->nDrcStart.size() != pParams->nDrcWidth.size())
    {
        fprintf(stderr, "Please Check -reset_start/-reset_end and -dstw/-dsth\n");
        sts = MFX_ERR_UNSUPPORTED;
    }

    for(size_t i=0;i < pParams->nDrcStart.size();i++)
    {

       if (pParams->nDrcEnd[i] < pParams->nDrcStart[i] + 1)
       {
          fprintf(stderr, "reset_end %d should be greater than %d reset_start+1\n",pParams->nDrcEnd[i] ,pParams->nDrcStart[i]);
          sts = MFX_ERR_UNSUPPORTED;
       }
       if (i > 0 && pParams->nDrcEnd[i-1] >= pParams->nDrcStart[i])
       {
          fprintf(stderr, "Current -reset_end should be less than the next -reset_start.\n");
          sts = MFX_ERR_UNSUPPORTED;
       }

    }
    return sts;
}
