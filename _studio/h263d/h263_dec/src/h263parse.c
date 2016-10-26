//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2005-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#include "h263.h"
#include "h263dec.h"

static void h263_zero_oppmodes(h263_OPPTYPEmodes *modes)
{
  ippsZero_8u((Ipp8u *)modes, sizeof(h263_OPPTYPEmodes));
}

static void h263_zero_modes(h263_PTYPEmodes *modes)
{
  ippsZero_8u((Ipp8u *)modes, sizeof(h263_PTYPEmodes));
}

static h263_Status h263_parse_BCM(h263_Info* pInfo)
{
#if 0
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32u code;

  code = h263_GetBits(pInfo, 14);

  VPic->back_mess_type = code >> 12;
  if (VPic->back_mess_type < 2)
    return H263_STATUS_PARSE_ERROR;

  VPic->URF = (code >> 11) & 1;
  VPic->BT_TR = (code >> 1) & 0x3FF;

  if (code & 1) /* ELNUMI */
    VPic->BT_ELNUM = h263_GetBits9(pInfo, 4);
  else
    VPic->BT_ELNUM = 0; /* ??? */

  VPic->BCPM = h263_GetBit(pInfo);
  if (VPic->BCPM)
    VPic->BSBI = h263_GetBits9(pInfo, 2);

  h263_FlushBits(pInfo, 1); /* BEPB1 - only if videomux mode is in use ???s */

  /* GOB number / MBA ......*/
#endif
  return H263_STATUS_OK;
}

static h263_Status h263_decode_resampleParams(h263_Info* pInfo)
{
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  h263_PictureFormat *VPicFmt = &pInfo->VideoSequence.VideoPicture.format;
  Ipp32s i, wx, wy, bit, scaleX, scaleY;
  Ipp32u bits;
  IppMotionVector *ref_warps;

  if (VPic->picture_coding_type != H263_PIC_TYPE_EP) {
    for (i = 0; i < 4; i++) {
      h263_DecodeMVD_umvplus(pInfo, &wx, &wy);
      VPic->warpParams[i].dx = (Ipp16s)wx;
      VPic->warpParams[i].dy = (Ipp16s)wy;
    }
    VPic->fillMode = h263_GetBits9(pInfo, 2);

    if (VPic->fillMode == 0) {
      VPic->fillColor[0] = h263_GetBits9(pInfo, 8);

      for (i = 1; i < 3; i++) {
        bits = h263_GetBits9(pInfo, 9);
#ifdef H263_METICULOUS
        if (bits & 0x100) {
          h263_Error("Error: Wrong Color Fill (zero anti-emulation bit)");
          return H263_STATUS_PARSE_ERROR;
        }
#endif
        VPic->fillColor[i] = bits & 0xff;
      }
    }
  } else { /* one-bit refinements */
    bits = h263_GetBits9(pInfo, 8);

    if (VPic->ref_layer_num == 1)
      ref_warps = pInfo->VideoSequence.warpParams;
    else
      ref_warps = pInfo->VideoSequence.enhLayers[VPic->ref_layer_num - 2]->warpParams;

    scaleX = (VPicFmt->scalability_type == H263_SCALABILITY_SPATIAL_Y ? 0 : 1);
    scaleY = (VPicFmt->scalability_type == H263_SCALABILITY_SPATIAL_X ? 0 : 1);

    /* refinement should be 0 if scale == 0 ??? */

    for (i = 0; i < 4; i++) {
      bit = (bits >> (7 - 2*i)) & 1;
      pInfo->VideoSequence.enhLayers[VPic->enh_layer_num - 2]->warpParams[i].dx =
        VPic->warpParams[i].dx = (Ipp16s)((ref_warps[i].dx << scaleX) + bit);
      bit = (bits >> (6 - 2*i)) & 1;
      pInfo->VideoSequence.enhLayers[VPic->enh_layer_num - 2]->warpParams[i].dy =
        VPic->warpParams[i].dy = (Ipp16s)((ref_warps[i].dy << scaleY) + bit);
    }
    /* fillMode of the ref layer is used -> the base layer fillMode is used in ALL enh layers ??? */
    VPic->fillMode = pInfo->VideoSequence.fillMode;
    if (VPic->fillMode == 0) {
      for (i = 0; i < 3; i++)
        VPic->fillColor[i] = pInfo->VideoSequence.fillColor[i];
    }
  }

  return H263_STATUS_OK;
}

h263_Status h263_Parse_PictureHeader(h263_Info* pInfo)
{
/*  h263_VideoSequence *VSeq = &pInfo->VideoSequence; */
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32u code;
  h263_PTYPEmodes *VPicModes = &pInfo->VideoSequence.VideoPicture.modes;

  h263_OPPTYPEmodes *VSeqOppModes;
  h263_OPPTYPEmodes *VPicOppModes = &pInfo->VideoSequence.VideoPicture.oppmodes;
  h263_PictureFormat *VSeqFmt;
  h263_PictureFormat *VPicFmt = &pInfo->VideoSequence.VideoPicture.format;

  h263_PTYPEmodes modes;
  h263_OPPTYPEmodes oppmodes;

  Ipp32s source_format;
  Ipp32s etr;
  Ipp32s scalability;

  VPicFmt->PAR_code = H263_ASPECT_RATIO_12_11;
  h263_FlushBits(pInfo, 22); /* Flush Picture Start Code */

  VPic->prev_temporal_reference = VPic->temporal_reference;
  VPic->temporal_reference = h263_GetBits9(pInfo, 8);

  code = h263_GetBits(pInfo, 8);  /* PTYPE */

  if ((code >> 6) != 2) {
    h263_Error("Error: Wrong PTYPE (first 2 bits)");
    return H263_STATUS_PARSE_ERROR;
  }

  VPic->split_screen_indicator = (code >> 5) & 1;
  VPic->document_camera_indicator = (code >> 4) & 1;
  VPic->full_picture_freeze_release = (code >> 3) & 1;
  source_format = code & 7;

  if (source_format == 0 || source_format == 6) {
    h263_Error("Error: Bad value of source format");
    return H263_STATUS_PARSE_ERROR;
  }

  if (source_format == 7)
    VPic->plusptype = 1;
  else
    VPic->plusptype = 0;

  VPic->prev_PCF = VPic->PCF;
  VPic->prev_clock_conversion_code = VPic->clock_conversion_code;
  VPic->prev_clock_divisor = VPic->clock_divisor;

//  VPic->PCF = 0;
//  VPic->clock_conversion_code = 0;
//  VPic->clock_divisor = 0;

  if (VPic->plusptype) { /* PLUSPTYPE */

    h263_zero_modes(&modes);

    VPic->ufep = h263_GetBits9(pInfo, 3);
    if (VPic->ufep > 1) {
#ifdef H263_METICULUOS
      h263_Error("Error: Wrong UFEP value");
      return H263_STATUS_PARSE_ERROR;
#endif
      VPic->ufep = 1;
    }

    if (VPic->ufep == 1) { /* OPPTYPE  */
      code = h263_GetBits(pInfo, 18);
      source_format = code >> 15;
      if (source_format == 0 || source_format == 7) {
        h263_Error("Error: Bad value of plustype source format");
        return H263_STATUS_PARSE_ERROR;
      }
      VPic->PCF = (Ipp8u)((code >> 14) & 1); /* ??? */
      oppmodes.UMV = (Ipp8u)((code >> 13) & 1);
      oppmodes.SAC = (Ipp8u)((code >> 12) & 1);
      oppmodes.advPred = (Ipp8u)((code >> 11) & 1);
      oppmodes.advIntra = (Ipp8u)((code >> 10) & 1);
      oppmodes.deblockFilt = (Ipp8u)((code >> 9) & 1);
      oppmodes.sliceStruct = (Ipp8u)((code >> 8) & 1);
      oppmodes.RPS = (Ipp8u)((code >> 7) & 1);
      oppmodes.ISD = (Ipp8u)((code >> 6) & 1);
      oppmodes.altInterVLC = (Ipp8u)((code >> 5) & 1);
      oppmodes.modQuant = (Ipp8u)((code >> 4) & 1);

#ifdef H263_METICULOUS
      if ((code & 15) != 8) {
        h263_Error("Error: Wrong OPPTYPE (last 4 bits)");
        return H263_STATUS_PARSE_ERROR;
      }
#endif
    } else { /* OPPTYPE  */
      h263_zero_oppmodes(&oppmodes);
    }

    code = h263_GetBits(pInfo, 9); /* MPPTYPE */
    VPic->picture_coding_type = code >> 6;
    if (VPic->picture_coding_type > 5) {
      h263_Error("Error: Bad value of picture coding type (in MPPTYPE)");
      return H263_STATUS_PARSE_ERROR;
    }
    if (VPic->picture_coding_type == H263_PIC_TYPE_B || VPic->picture_coding_type == H263_PIC_TYPE_EP || VPic->picture_coding_type == H263_PIC_TYPE_EI)
      scalability = 1;
    else
      scalability = 0;

    modes.resample = (code >> 5) & 1;
    modes.redResUp = (code >> 4) & 1;
    VPic->rtype = (code >> 3) & 1;
#ifdef H263_METICULOUS
    if ((code & 7) != 1) {
      h263_Error("Error: Wrong MPPTYPE (last 3 bits)");
      return H263_STATUS_PARSE_ERROR;
    }
#endif

#ifdef H263_METICULOUS
    if (VPic->rtype && !(VPic->picture_coding_type == H263_PIC_TYPE_iPB ||
                         VPic->picture_coding_type == H263_PIC_TYPE_P ||
                         VPic->picture_coding_type == H263_PIC_TYPE_EP)) {
      h263_Error("Error: Wrong RTYPE");
      return H263_STATUS_PARSE_ERROR;
    }
#endif

    VPic->CPM = h263_GetBit(pInfo);
    if (VPic->CPM)
      VPic->PSBI = h263_GetBits9(pInfo, 2);

    if (VPic->ufep == 1) {
      if (source_format == 6) {  /* custom picture format */
        Ipp32s pwi, phi;
        code = h263_GetBits(pInfo, 23);
        VPicFmt->PAR_code = code >> 19;
        if (VPicFmt->PAR_code > H263_ASPECT_RATIO_40_33 && VPicFmt->PAR_code < H263_ASPECT_RATIO_EXTPAR)
          VPicFmt->PAR_code = H263_ASPECT_RATIO_FORBIDDEN;

        pwi = (code >> 10) & 511;

#ifdef H263_METICULOUS
        if ((code >> 9) & 1) {
          h263_Error("Error: Wrong CPFMT (zero anti-emulation bit)");
          return H263_STATUS_PARSE_ERROR;
        }
#endif
        phi = code & 511;

        VPicFmt->height = phi * 4;
        VPicFmt->width = (pwi + 1) * 4;

        if (VPicFmt->PAR_code == H263_ASPECT_RATIO_EXTPAR) { /* Extended PAR */
          VPicFmt->PAR_width = h263_GetBits9(pInfo, 8);
          if (VPicFmt->PAR_width == 0) {
            h263_Error("Error: Zero PAR width");
            return H263_STATUS_PARSE_ERROR;
          }
          VPicFmt->PAR_height = h263_GetBits9(pInfo, 8);
          if (VPicFmt->PAR_height == 0) {
            h263_Error("Error: Zero PAR height");
            return H263_STATUS_PARSE_ERROR;
          }
        }
      } else {
        VPicFmt->height = h263_pic_height[source_format - 1];
        VPicFmt->width = h263_pic_width[source_format - 1];
      }

      if (VPic->PCF) { /* custom picture clock frequency */
        code = h263_GetBits9(pInfo, 8);
        VPic->clock_conversion_code = code >> 7;
        VPic->clock_divisor = code & 0x7F;
        if (VPic->clock_divisor == 0) {
          h263_Error("Error: Zero clock divisor");
          return H263_STATUS_PARSE_ERROR;
        }
      }
    }

    if (VPic->PCF) { /* custom picture clock frequency */
      etr = h263_GetBits9(pInfo, 2);
      VPic->temporal_reference += (etr << 8);
    }
    if (VPic->ufep == 1) {
      if (oppmodes.UMV) {
        code = h263_GetBit(pInfo);
        if (code)
          VPic->unlimited_UMV = 0;
        else {
          code = h263_GetBit(pInfo);
          if (!code) {
            h263_Error("Error: Invalid UUI");
            return H263_STATUS_PARSE_ERROR;
          }
          VPic->unlimited_UMV = 1;
        }
      }
      if (oppmodes.sliceStruct)
        VPic->sliceSubmodes = h263_GetBits9(pInfo, 2); /* if ufep==0, the last decoded value is used */
    }
    if (scalability) {
      VPic->enh_layer_num = h263_GetBits9(pInfo, 4);
      if (VPic->enh_layer_num == 0) {
        h263_Error("Error: Invalid enhanced layer number");
        return H263_STATUS_PARSE_ERROR;
      }
      VSeqOppModes = &(pInfo->VideoSequence.oppmodes[VPic->enh_layer_num - 1]);
      VSeqFmt = &(pInfo->VideoSequence.format[VPic->enh_layer_num - 1]);
      if (VPic->ufep == 1) {
        VPic->ref_layer_num = h263_GetBits9(pInfo, 4);
        if (VPic->ref_layer_num == 0) {
          h263_Error("Error: Invalid reference layer number");
          return H263_STATUS_PARSE_ERROR;
        }
        pInfo->VideoSequence.ref_layer_num[VPic->enh_layer_num - 1] = VPic->ref_layer_num;
        if (VPicFmt->height != pInfo->VideoSequence.format[VPic->ref_layer_num - 1].height) {
          if (VPicFmt->width != pInfo->VideoSequence.format[VPic->ref_layer_num - 1].width)
            VPicFmt->scalability_type = H263_SCALABILITY_SPATIAL_XY;
          else
            VPicFmt->scalability_type = H263_SCALABILITY_SPATIAL_Y;
        } else {
          if (VPicFmt->width != pInfo->VideoSequence.format[VPic->ref_layer_num - 1].width)
            VPicFmt->scalability_type = H263_SCALABILITY_SPATIAL_X;
          else
            VPicFmt->scalability_type = (VPic->picture_coding_type == H263_PIC_TYPE_B ? H263_SCALABILITY_TEMPORAL : H263_SCALABILITY_SNR);
        }
      } else {
        VPic->ref_layer_num = pInfo->VideoSequence.ref_layer_num[VPic->enh_layer_num - 1];
        if (VPic->picture_coding_type == H263_PIC_TYPE_B) {
          if (pInfo->VideoSequence.prev_pic_type[VPic->enh_layer_num - 1] != H263_PIC_TYPE_B)
            VPic->ref_layer_num = pInfo->VideoSequence.ref_layer_num_b[VPic->enh_layer_num - 1] = VPic->enh_layer_num;
          else
            VPic->ref_layer_num = pInfo->VideoSequence.ref_layer_num_b[VPic->enh_layer_num - 1];
        }
      }
    } else {
      VPic->enh_layer_num = 1;
      VSeqOppModes = &(pInfo->VideoSequence.oppmodes[0]);
      VSeqFmt = &(pInfo->VideoSequence.format[0]);
    }

    if (oppmodes.RPS) {
      if (VPic->ufep == 1) {
        VPic->RPSflags = h263_GetBits9(pInfo, 3);
        if (VPic->RPSflags < 4) {
          h263_Error("Error: Invalid PRSMF");
          return H263_STATUS_PARSE_ERROR;
        }
      }

      code = h263_GetBit(pInfo); /* TRPI */
      if (code) {
        VPic->pred_temp_ref = h263_GetBits(pInfo, 10); /* TRP */
      }

      code = h263_GetBit(pInfo); /* BCI */
      if (code) {
        h263_parse_BCM(pInfo); /* not implemented yet !!! */
      } else {
        code = h263_GetBit(pInfo);
        if (!code) {
          h263_Error("Error: Invalid BCI");
          return H263_STATUS_PARSE_ERROR;
        }
      }
    }

    if (modes.resample) {
      VPic->wda = h263_GetBits9(pInfo, 2);
      if (VPic->wda < 2) {
        h263_Error("Error: Invalid wda");
        return H263_STATUS_PARSE_ERROR;
      }
      VPic->wda &= 1;
      h263_decode_resampleParams(pInfo);
    }

    if (VPic->ufep == 1) {
      *VSeqOppModes = oppmodes;
      *VPicOppModes = oppmodes;
      *VSeqFmt = *VPicFmt;
    } else {
      *VPicOppModes = *VSeqOppModes;
      *VPicFmt = *VSeqFmt;
    }

    VPic->height = VPicFmt->height;
    VPic->width = VPicFmt->width;

  } else { /* PLUSPTYPE */

    VPic->rtype = 0;
    code = h263_GetBits(pInfo, 5); /* the rest of PTYPE */

    VPic->picture_coding_type = (code >> 4) & 1;
    modes.UMV = (code >> 3) & 1;
    modes.SAC = (code >> 2) & 1;
    modes.advPred = (code >> 1) & 1;
    modes.PBframes = code & 1;
    modes.redResUp = 0;
    modes.resample = 0;

    if (modes.PBframes)
      VPic->picture_coding_type = H263_PIC_TYPE_PB;

    VSeqOppModes = &(pInfo->VideoSequence.oppmodes[0]);
    h263_zero_oppmodes(VSeqOppModes);
    h263_zero_oppmodes(VPicOppModes);
    VPic->height = h263_pic_height[source_format - 1];
    VPic->width = h263_pic_width[source_format - 1];

/* ??? may need that only if there are frames with and without PLUSPTYPE in the same sequence
    pInfo->VideoSequence.format[0].height = VPic->height;
    pInfo->VideoSequence.format[0].width = VPic->width;
*/
  }

  *VPicModes = modes;

  /* Mode restrictions (Rec. 5.1.4.5)  ??? */
  if (VPic->picture_coding_type == H263_PIC_TYPE_I) {
    VPicModes->UMV = VPicModes->advPred =
      VPicOppModes->UMV = VPicOppModes->advPred =
      VPicOppModes->altInterVLC = 0;
      VPicModes->resample = VPicModes->redResUp = 0;
  } else if (VPic->picture_coding_type == H263_PIC_TYPE_B) {
    VPicModes->SAC = VPicOppModes->SAC =
      VPicOppModes->deblockFilt =
      VPicOppModes->advPred = 0;
  } else if (VPic->picture_coding_type == H263_PIC_TYPE_EI) {
    VPicModes->UMV = VPicModes->SAC = VPicModes->advPred =
    VPicOppModes->UMV = VPicOppModes->SAC = VPicOppModes->advPred =
      VPicOppModes->altInterVLC = 0;
      VPicModes->resample = VPicModes->redResUp = 0;
  } else if (VPic->picture_coding_type == H263_PIC_TYPE_EP) {
    VPicModes->SAC = VPicModes->advPred = 0;
    VPicOppModes->SAC = VPicOppModes->advPred = 0;
  }

  VPic->pic_quant = h263_GetBits9(pInfo, 5);

  if (!VPic->plusptype) {
    VPic->CPM = h263_GetBit(pInfo);
    if (VPic->CPM)
      VPic->PSBI = h263_GetBits9(pInfo, 2);
  }

#if 0
  if (VPic->width != VSeq->format[VPic->enh_layer_num - 1].width || VPic->height != VSeq->format[VPic->enh_layer_num - 1].height) { /* ??? */
    if (!VPicModes->resample && !scalability && VPic->picture_coding_type != H263_PIC_TYPE_I) {
      h263_Error("Error: Source format change is incompatible with picture type");
      return H263_STATUS_PARSE_ERROR;
    }
  }
#endif

  if (VPic->picture_coding_type == H263_PIC_TYPE_PB ||
    VPic->picture_coding_type == H263_PIC_TYPE_iPB) {

    if (VPic->PCF)
      VPic->temporal_reference_B = h263_GetBits9(pInfo, 5);
    else
      VPic->temporal_reference_B = h263_GetBits9(pInfo, 3);

     VPic->dbquant = h263_GetBits9(pInfo, 2);
  }

  for (;;) {
    code = h263_GetBit(pInfo); /* pei */
    if (!code)
      break;
    code = h263_GetBits9(pInfo, 8); /* psupp (Annex L) TODO ??? */
  }

  if (!VPicModes->redResUp) {
    VPic->MacroBlockPerRow = (VPic->width + 15) >> 4;
    VPic->MacroBlockPerCol = (VPic->height + 15) >> 4;
    if (VPic->height <= 400) {
      VPic->RowsPerGOB = 1;
      VPic->num_gobs_in_pic = VPic->MacroBlockPerCol;
    } else if (VPic->height <= 800) {
      VPic->RowsPerGOB = 2;
      VPic->num_gobs_in_pic = (VPic->MacroBlockPerCol  + 1) >> 1;
    } else { /* if (VPic->height <= 1152) */
      VPic->RowsPerGOB = 4;
      VPic->num_gobs_in_pic = (VPic->MacroBlockPerCol + 3) >> 2;
    }
  } else {
    VPic->MacroBlockPerRow = (VPic->width + 31) >> 5;
    VPic->MacroBlockPerCol = (VPic->height + 31) >> 5;
    if (VPic->height <= 800) {
      VPic->RowsPerGOB = 1;
      VPic->num_gobs_in_pic = VPic->MacroBlockPerCol;
    } else { /* if (VPic->height <= 1152) */
      VPic->RowsPerGOB = 2;
      VPic->num_gobs_in_pic = (VPic->MacroBlockPerCol + 1) >> 1;
    }
  }

  return H263_STATUS_OK;
}

#ifndef UMC_H263_DISABLE_SORENSON
h263_Status h263_Parse_PictureHeader_Sorenson(h263_Info* pInfo)
{
  /*  h263_VideoSequence *VSeq = &pInfo->VideoSequence; */
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32u code;
  h263_PTYPEmodes *VPicModes = &pInfo->VideoSequence.VideoPicture.modes;
  h263_OPPTYPEmodes *VSeqOppModes;
  h263_OPPTYPEmodes *VPicOppModes = &pInfo->VideoSequence.VideoPicture.oppmodes;
  h263_PTYPEmodes modes;
  Ipp32s source_format;

  h263_FlushBits(pInfo, 21); /* Flush Picture Start Code */
  VPic->sorenson_format = h263_GetBit(pInfo);

  VPic->prev_temporal_reference = VPic->temporal_reference;
  VPic->temporal_reference = h263_GetBits9(pInfo, 8);

  source_format = h263_GetBits9(pInfo, 3);
  switch (source_format) {
  case 0:
    VPic->width = h263_GetBits9(pInfo, 8);
    VPic->height = h263_GetBits9(pInfo, 8);
    break;
  case 1:
    VPic->width = h263_GetBits(pInfo, 16);
    VPic->height = h263_GetBits(pInfo, 16);
    break;
  case 2:
    VPic->width = 352;
    VPic->height = 288;
    break;
  case 3:
    VPic->width = 176;
    VPic->height = 144;
    break;
  case 4:
    VPic->width = 128;
    VPic->height = 96;
    break;
  case 5:
    VPic->width = 320;
    VPic->height = 240;
    break;
  case 6:
    VPic->width = 160;
    VPic->height = 120;
    break;
  default:
    h263_Error("Error: Incorrect picture format");
    return H263_STATUS_PARSE_ERROR;
  }

  code = h263_GetBits9(pInfo, 8);
  switch (code >> 6) {
  case 0:
    VPic->picture_coding_type = H263_PIC_TYPE_I;
    break;
  case 1:
    VPic->picture_coding_type = H263_PIC_TYPE_P;
    break;
  case 2:
    VPic->picture_coding_type = H263_PIC_TYPE_P_UNREF;
    break;
  default:
    h263_Error("Error: Incorrect picture type");
    return H263_STATUS_PARSE_ERROR;
  }
  // 1 bit between pic type and quntizer - deblocking filter request, ignored ???
  VPic->pic_quant = code & 0x1F;

  VPic->prev_clock_conversion_code = VPic->clock_conversion_code;
  VPic->prev_clock_divisor = VPic->clock_divisor;

  VPic->rtype = 0;
//  modes.UMV = 1; // UMV support us always on according to SWF and FLV file format specification V.9,
                   // but all the clips I've tried are encoded with UMV off.
  modes.UMV = 0;
  modes.SAC = 0;
  modes.advPred = 0;
  modes.PBframes = 0;
  modes.redResUp = 0;
  modes.resample = 0;

  VSeqOppModes = &(pInfo->VideoSequence.oppmodes[0]);
  h263_zero_oppmodes(VSeqOppModes);
  h263_zero_oppmodes(VPicOppModes);

  *VPicModes = modes;
  for (;;) {
    code = h263_GetBit(pInfo); /* pei */
    if (!code)
      break;
    code = h263_GetBits9(pInfo, 8); /* psupp (Annex L) TODO ??? */
  }

  VPic->MacroBlockPerRow = (VPic->width + 15) >> 4;
  VPic->MacroBlockPerCol = (VPic->height + 15) >> 4;
  if (VPic->height <= 400) {
    VPic->RowsPerGOB = 1;
    VPic->num_gobs_in_pic = VPic->MacroBlockPerCol;
  } else if (VPic->height <= 800) {
    VPic->RowsPerGOB = 2;
    VPic->num_gobs_in_pic = (VPic->MacroBlockPerCol  + 1) >> 1;
  } else { /* if (VPic->height <= 1152) */
    VPic->RowsPerGOB = 4;
    VPic->num_gobs_in_pic = (VPic->MacroBlockPerCol + 3) >> 2;
  }

  return H263_STATUS_OK;
}
#endif

Ipp32s h263_ParseGOBHeader(h263_Info* pInfo)
{
  Ipp32u code;
  code = h263_ShowBits(pInfo, 17);
  if (code > 1)                        /* No GOB header  */
    return 0;
  if (code == 1) {                     /* GOB Start code, no GSTUF */
    h263_FlushBits(pInfo, 17);
  } else {
    Ipp32s index = 2 + ((pInfo->bitoff + 7) >> 3);
    if (pInfo->bufptr[index] > 127) {  /* GOB Start code, with GSTUF */
      pInfo->bufptr += index;
      pInfo->bitoff = 1;
    } else {
      return -1;
    }
  }

  pInfo->VideoSequence.VideoPicture.gob_number = h263_GetBits9(pInfo, 5);
  if (pInfo->VideoSequence.VideoPicture.gob_number > 30) /* EOS or EOSBS */
    return 1;

  if (pInfo->VideoSequence.VideoPicture.gob_number >= pInfo->VideoSequence.VideoPicture.num_gobs_in_pic)
    return -1;

  if (pInfo->VideoSequence.VideoPicture.CPM)
    pInfo->VideoSequence.VideoPicture.GSBI = h263_GetBits9(pInfo, 2);

  pInfo->VideoSequence.VideoPicture.GFID = h263_GetBits9(pInfo, 2);

  pInfo->VideoSequence.VideoPicture.pic_quant = h263_GetBits9(pInfo, 5);

  return 1;
}
