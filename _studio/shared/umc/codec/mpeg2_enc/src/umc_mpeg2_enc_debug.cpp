//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2002-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)
#include "umc_mpeg2_enc_defs.h"
#if defined (MPEG2_ENC_DEBUG)
#include "ippi.h"

using namespace UMC;

typedef struct _IppMotionVector2
{
  Ipp32s x;
  Ipp32s y;
  Ipp32s mctype_l;
  Ipp32s offset_l;
  Ipp32s mctype_c;
  Ipp32s offset_c;
} IppMotionVector2;

void MPEG2EncDebug::SetMotionFlag(int k,int value)
{
  if(encoder->encodeInfo.numEncodedFrames < minFrameIndex || encoder->encodeInfo.numEncodedFrames > maxFrameIndex)
    return;
  mb_debug_info[k].motion_flag = value;
}

#define COPY_FRAME_BLOCK(X, CC, C, DIR) \
    ippiCopy_8u_C1R( \
      encoder->X##RecFrame[encoder->pMBInfo[k].mv_field_sel[2][DIR]][DIR] + vector[2][DIR].offset_##C, \
      encoder->CC##FrameHSize, \
      this->mb_debug_info[k].X##BlockRec[DIR][0], \
      roi_##C.width, \
      roi_##C \
      )

#define COPY_FIELD_BLOCKS(X, CC, C, DIR) \
  if(encoder->picture_structure == FRAME_PICTURE) \
  { \
    ippiCopy_8u_C1R( \
      encoder->X##RecFrame[encoder->pMBInfo[k].mv_field_sel[0][DIR]][DIR] + vector[0][DIR].offset_##C, \
      2*(encoder->CC##FrameHSize), \
      this->mb_debug_info[k].X##BlockRec[DIR][0], \
      roi_##C.width, \
      roi_##C \
      ); \
    ippiCopy_8u_C1R( \
      encoder->X##RecFrame[encoder->pMBInfo[k].mv_field_sel[1][DIR]][DIR] + vector[1][DIR].offset_##C, \
      2*(encoder->CC##FrameHSize), \
      this->mb_debug_info[k].X##BlockRec[DIR][1], \
      roi_##C.width, \
      roi_##C \
      ); \
  }else{ \
    ippiCopy_8u_C1R( \
      encoder->X##RecFrame[encoder->pMBInfo[k].mv_field_sel[0][DIR]][DIR] + vector[0][DIR].offset_##C, \
      encoder->CC##FrameHSize, \
      this->mb_debug_info[k].X##BlockRec[DIR][0], \
      roi_##C.width, \
      roi_##C \
      ); \
    ippiCopy_8u_C1R( \
      encoder->X##RecFrame[encoder->pMBInfo[k].mv_field_sel[1][DIR]][DIR] + vector[1][DIR].offset_##C + roi_##C.height*(encoder->CC##FrameHSize), \
      encoder->CC##FrameHSize, \
      this->mb_debug_info[k].X##BlockRec[DIR][1], \
      roi_##C.width, \
      roi_##C \
      ); \
  }
#define COPY_FIELD_BLOCKS_DP(X, CC, C) \
  if(encoder->picture_structure == FRAME_PICTURE) \
  { \
    ippiCopy_8u_C1R( \
      encoder->X##RecFrame[0][0] + vector[0][0].offset_##C, \
      2*(encoder->CC##FrameHSize), \
      this->mb_debug_info[k].X##BlockRec[0][0], \
      roi_##C.width, \
      roi_##C \
      ); \
    ippiCopy_8u_C1R( \
      encoder->X##RecFrame[1][0] + vector[0][0].offset_##C, \
      2*(encoder->CC##FrameHSize), \
      this->mb_debug_info[k].X##BlockRec[0][1], \
      roi_##C.width, \
      roi_##C \
      ); \
  }else{ \
    ippiCopy_8u_C1R( \
      encoder->X##RecFrame[encoder->curr_field][0] + vector[0][0].offset_##C, \
      encoder->CC##FrameHSize, \
      this->mb_debug_info[k].X##BlockRec[0][0], \
      roi_##C.width, \
      roi_##C \
      ); \
  }

void MPEG2EncDebug::GatherInterRefMBlocksData(int k,void *vector_in)
{
  if(use_reference_log != 1 || encoder->encodeInfo.numEncodedFrames < minFrameIndex || encoder->encodeInfo.numEncodedFrames > maxFrameIndex)
    return;

  IppMotionVector2 *tmp_vector;
  IppMotionVector2 vector[3][2];
  IppiSize roi_l = {16,16},
           roi_c = {encoder->BlkWidth_c,encoder->BlkHeight_c};

  tmp_vector = (IppMotionVector2*)vector_in;
  if(encoder->picture_coding_type == MPEG2_B_PICTURE)
  {
    vector[0][0] = tmp_vector[0];
    vector[0][1] = tmp_vector[1];

    vector[1][0] = tmp_vector[2];
    vector[1][1] = tmp_vector[3];

    vector[2][0] = tmp_vector[4];
    vector[2][1] = tmp_vector[5];
  }
  else
  {
    vector[0][0] = tmp_vector[0];
    vector[1][0] = tmp_vector[1];
    vector[2][0] = tmp_vector[2];
  }

  if(encoder->pMBInfo[k].prediction_type == MC_FRAME)
  {
    if(encoder->pMBInfo[k].mb_type & MB_FORWARD)
    {
      COPY_FRAME_BLOCK(Y,Y,l,0);
      if(encoder->encodeInfo.info.color_format == NV12) {
        COPY_FRAME_BLOCK(U,Y,c,0);
        COPY_FRAME_BLOCK(V,Y,c,0);
      } else {
        COPY_FRAME_BLOCK(U,UV,c,0);
        COPY_FRAME_BLOCK(V,UV,c,0);
      }
    }
    if(encoder->pMBInfo[k].mb_type & MB_BACKWARD)
    {
      COPY_FRAME_BLOCK(Y,Y,l,1);
      if(encoder->encodeInfo.info.color_format == NV12) {
        COPY_FRAME_BLOCK(U,Y,c,1);
        COPY_FRAME_BLOCK(V,Y,c,1);
      } else {
        COPY_FRAME_BLOCK(U,UV,c,1);
        COPY_FRAME_BLOCK(V,UV,c,1);
      }
    }
  }else if(encoder->pMBInfo[k].prediction_type == MC_FIELD)
  {
    roi_l.height /= 2;
    roi_c.height /= 2;

    if(encoder->pMBInfo[k].mb_type & MB_FORWARD)
    {
      COPY_FIELD_BLOCKS(Y,Y,l,0);
      if(encoder->encodeInfo.info.color_format == NV12) {
        COPY_FIELD_BLOCKS(U,Y,c,0);
        COPY_FIELD_BLOCKS(V,Y,c,0);
      } else {
        COPY_FIELD_BLOCKS(U,UV,c,0);
        COPY_FIELD_BLOCKS(V,UV,c,0);
      }
    }
    if(encoder->pMBInfo[k].mb_type & MB_BACKWARD)
    {
      COPY_FIELD_BLOCKS(Y,Y,l,1);
      if(encoder->encodeInfo.info.color_format == NV12) {
        COPY_FIELD_BLOCKS(U,Y,c,1);
        COPY_FIELD_BLOCKS(V,Y,c,1);
      } else {
        COPY_FIELD_BLOCKS(U,UV,c,1);
        COPY_FIELD_BLOCKS(V,UV,c,1);
      }
    }
  }else if(encoder->pMBInfo[k].prediction_type == MC_DMV)
  {
    if(encoder->picture_structure == FRAME_PICTURE)
    {
      roi_l.height /= 2;
      roi_c.height /= 2;
    }
    COPY_FIELD_BLOCKS_DP(Y,Y,l);
    if(encoder->encodeInfo.info.color_format == NV12) {
      COPY_FIELD_BLOCKS_DP(U,Y,c);
      COPY_FIELD_BLOCKS_DP(V,Y,c);
    } else {
      COPY_FIELD_BLOCKS_DP(U,UV,c);
      COPY_FIELD_BLOCKS_DP(V,UV,c);
    }
  }
}
void MPEG2EncDebug::GatherInterBlockData(int k,Ipp16s *pMBlock,int blk,int Count)
{
  if(encoder->encodeInfo.numEncodedFrames < minFrameIndex || encoder->encodeInfo.numEncodedFrames > maxFrameIndex)
    return;

  mb_debug_info[k].quantizer = encoder->quantiser_scale_value;

  if(use_extended_log == 1)
  {
    if (Count) {
      mb_debug_info[k].Count[blk] = 1;
      Ipp16s pMBlock_tmp[64];
      MFX_INTERNAL_CPY(pMBlock_tmp,pMBlock,sizeof(pMBlock_tmp));
      ippiQuantInv_MPEG2_16s_C1I(pMBlock_tmp, encoder->quantiser_scale_value, encoder->NonIntraQMatrix);
      MFX_INTERNAL_CPY(mb_debug_info[k].encMbData[blk],pMBlock_tmp,sizeof(pMBlock_tmp));
    }
    else
    {
      mb_debug_info[k].Count[blk] = 0;
    }
  }
}
void MPEG2EncDebug::GatherIntraBlockData(int k,Ipp16s *pMBlock,int blk,int Count,int intra_dc_shift)
{
  if(encoder->encodeInfo.numEncodedFrames < minFrameIndex || encoder->encodeInfo.numEncodedFrames > maxFrameIndex)
    return;

  mb_debug_info[k].quantizer = encoder->quantiser_scale_value;
  if(use_extended_log == 1)
  {
    //debug for non reference frames should prepare data by itself
    if(!(encoder->picture_coding_type != MPEG2_B_PICTURE && !encoder->onlyIFrames))
    {
      pMBlock[0] <<= intra_dc_shift;
      ippiQuantInvIntra_MPEG2_16s_C1I(pMBlock, encoder->quantiser_scale_value, encoder->IntraQMatrix);
    }

    mb_debug_info[k].Count[blk] = Count;
    if(Count)
      MFX_INTERNAL_CPY(mb_debug_info[k].encMbData[blk],pMBlock,8*8*sizeof(Ipp16s));
    //ippiCopy_16s_C1R(pMBlock, 8*sizeof(Ipp16s),pMBInfo[k].encMbData[blk],8*sizeof(Ipp16s),roi8x8);
    else
      mb_debug_info[k].encMbData[blk][0] = pMBlock[0];
  }
}

void MPEG2EncDebug::CreateDEBUGframeLog()
{
  if(encoder->encodeInfo.numEncodedFrames < minFrameIndex || encoder->encodeInfo.numEncodedFrames > maxFrameIndex)
    return;

  int i,j,k,is_skipped;
  char frameLogName[2048],mb_type_log[128],mv_log[128],quantizer_log[128],*fieldName,frame_type;
  frame_type = (encoder->picture_coding_type == MPEG2_I_PICTURE)? 'I' : ((encoder->picture_coding_type == MPEG2_P_PICTURE)? 'P':'B');
  fieldName = (encoder->picture_structure == FRAME_PICTURE)? "" :((encoder->curr_field == 0)? "(top)":"(bottom)");
  sprintf(frameLogName,"%s\\%d_%c%s.log",debug_logs_folder,encoder->encodeInfo.numEncodedFrames,frame_type,fieldName);
  FILE *frameLogFile = fopen(frameLogName,"w");

  if(frameLogFile == NULL)
  {
    printf("Couldn't write to %s file\n",frameLogName);
    return ;
  }
  for(k = 0, j=0; j < encoder->MBcountV*16; j += 16)
  {
    for(i=0; i < encoder->encodeInfo.info.clip_info.width; i += 16, k++)
    {
      is_skipped       = 0;
      mv_log[0]        = 0;
      quantizer_log[0] = 0;

      if(encoder->pMBInfo[k].mb_type & MB_INTRA){
        strcpy(mb_type_log,"mb_type Intra;");
        sprintf(quantizer_log,"quant: %d;",this->mb_debug_info[k].quantizer);
      }
      else if(encoder->picture_coding_type == MPEG2_B_PICTURE && encoder->pMBInfo[k].skipped == 1 ||  encoder->pMBInfo[k].mb_type == 0){
        strcpy(mb_type_log,"mb_type Inter skip;");
        is_skipped = 1;
      }
      else if(encoder->picture_coding_type == MPEG2_P_PICTURE && this->mb_debug_info[k].motion_flag == 0 && encoder->pMBInfo[k].prediction_type == MC_FRAME){
        strcpy(mb_type_log,"mb_type Inter (MC_ZERO);");
        sprintf(quantizer_log,"quant: %d;",this->mb_debug_info[k].quantizer);
      }
      else if((encoder->pMBInfo[k].mb_type & (MB_FORWARD | MB_BACKWARD)) == (MB_FORWARD | MB_BACKWARD)){
        strcpy(mb_type_log,"mb_type Inter F/B");
        sprintf(quantizer_log,"quant: %d;",this->mb_debug_info[k].quantizer);
        sprintf(mv_log,"MV_F x:%+2d y:%+2d; MV_B x:%+2d y:%+2d",encoder->pMBInfo[k].MV[0][0].x,
                                                                encoder->pMBInfo[k].MV[0][0].y,
                                                                encoder->pMBInfo[k].MV[0][1].x,
                                                                encoder->pMBInfo[k].MV[0][1].y);
      }
      else if(encoder->pMBInfo[k].mb_type & MB_FORWARD){
        strcpy(mb_type_log,"mb_type Inter F");
        sprintf(quantizer_log,"quant: %d;",this->mb_debug_info[k].quantizer);

        if(encoder->pMBInfo[k].prediction_type == MC_DMV)
          sprintf(mv_log,"MV_F x:%+2d y:%+2d, DMV x:%+2d y:%+2d",encoder->pMBInfo[k].dpMV[encoder->curr_field][0].x,
                                                                         encoder->pMBInfo[k].dpMV[encoder->curr_field][0].y << (encoder->picture_structure == FRAME_PICTURE),
                                                                         encoder->pMBInfo[k].dpDMV[encoder->curr_field].x,
                                                                         encoder->pMBInfo[k].dpDMV[encoder->curr_field].y);
        else
          sprintf(mv_log,"MV_F x:%+2d y:%+2d",encoder->pMBInfo[k].MV[0][0].x,
                                              encoder->pMBInfo[k].MV[0][0].y);
      }
      else //if(encoder->pMBInfo[k].mb_type & MB_BACKWARD)
      {
        strcpy(mb_type_log,"mb_type Inter B");
        sprintf(quantizer_log,"quant: %d;",this->mb_debug_info[k].quantizer);

        sprintf(mv_log,"MV_B x:%+2d y:%+2d",encoder->pMBInfo[k].MV[0][1].x,
                                            encoder->pMBInfo[k].MV[0][1].y);
      }

      if(mv_log[0] != 0)
      {
        if(encoder->pMBInfo[k].prediction_type == MC_FIELD)
        {
          char tmp_buf[128];

          if((encoder->pMBInfo[k].mb_type & (MB_FORWARD | MB_BACKWARD)) == (MB_FORWARD | MB_BACKWARD))
            sprintf(tmp_buf," (field1); MV_F x:%+2d y:%+2d; MV_B x:%+2d y:%+2d (field2)",encoder->pMBInfo[k].MV[1][0].x,encoder->pMBInfo[k].MV[1][0].y,encoder->pMBInfo[k].MV[1][1].x,encoder->pMBInfo[k].MV[1][1].y);
          else if(encoder->pMBInfo[k].mb_type & MB_FORWARD)
            sprintf(tmp_buf," (field1); MV_F x:%+2d y:%+2d (field2)",encoder->pMBInfo[k].MV[1][0].x,encoder->pMBInfo[k].MV[1][0].y);
          else //if(encoder->pMBInfo[k].mb_type & MB_BACKWARD)
            sprintf(tmp_buf," (field1); MV_B x:%+2d y:%+2d (field2)",encoder->pMBInfo[k].MV[1][1].x,encoder->pMBInfo[k].MV[1][1].y);

          strcat(mb_type_log," (MC_FIELD);");
          strcat(mv_log,tmp_buf);
        }else if(encoder->pMBInfo[k].prediction_type == MC_FRAME)
        {
          strcat(mb_type_log," (MC_FRAME);");
        }
        else
        {
          strcat(mb_type_log," (MC_DMV);");
        }
      }
      if(!use_mb_type)
        mb_type_log[0] = 0;

      if(!use_mb_MVinfo)
        mv_log[0] = 0;

      if(!use_quantizer_info)
        quantizer_log[0] = 0;

      int mb_y = (encoder->picture_structure == FRAME_PICTURE)? j/16 :((encoder->curr_field == 0)? j/16 : j/16 + encoder->MBcountV);
      fprintf(frameLogFile,"MB(%dx%d) %s %s %s\n",i/16,mb_y,mb_type_log,quantizer_log,mv_log);

      if(use_extended_log == 1 && !(encoder->picture_coding_type == MPEG2_B_PICTURE && encoder->pMBInfo[k].skipped == 1 ||  encoder->pMBInfo[k].mb_type == 0))
      {
        fprintf(frameLogFile,"MB data:");
        for (int blk = 0; blk < encoder->block_count; blk++) {
          if(this->mb_debug_info[k].Count[blk])
          {
            fprintf(frameLogFile,"\n            blk %d:",blk);
            for(int i = 0; i < 64; i++)
              fprintf(frameLogFile," %d",this->mb_debug_info[k].encMbData[blk][i]);
          }else if(encoder->pMBInfo[k].mb_type & MB_INTRA)
          {
            fprintf(frameLogFile,"\n            blk %d: %d",blk,this->mb_debug_info[k].encMbData[blk][0]);
          }else
          {
            fprintf(frameLogFile,"\n            blk %d: ---",blk);
          }
        }
        fprintf(frameLogFile,"\n\n");
      }
      if(use_reference_log == 1 && encoder->picture_coding_type != MPEG2_I_PICTURE && !is_skipped)
      {
        int print_size[2][2][2],size_l,size_c;//l|c,fwd|bwd,fld1|fld2

        size_l = BlkHeight_l * BlkWidth_l;
        size_c = encoder->BlkHeight_c * encoder->BlkWidth_c;
        if(encoder->pMBInfo[k].prediction_type == MC_FIELD || (encoder->pMBInfo[k].prediction_type == MC_DMV && encoder->picture_structure == FRAME_PICTURE))
        {
          size_l /= 2;
          size_c /= 2;
        }

        memset(print_size,0,sizeof(print_size));

        if(encoder->pMBInfo[k].mb_type & MB_FORWARD)
        {
          print_size[0][0][0] = size_l;
          print_size[1][0][0] = size_c;
          if(encoder->pMBInfo[k].prediction_type == MC_FIELD || (encoder->pMBInfo[k].prediction_type == MC_DMV && encoder->picture_structure == FRAME_PICTURE))
          {
            print_size[0][0][1] = size_l;
            print_size[1][0][1] = size_c;
          }
        }
        if(encoder->pMBInfo[k].mb_type & MB_BACKWARD)
        {
          print_size[0][1][0] = size_l;
          print_size[1][1][0] = size_c;
          if(encoder->pMBInfo[k].prediction_type == MC_FIELD || (encoder->pMBInfo[k].prediction_type == MC_DMV && encoder->picture_structure == FRAME_PICTURE))
          {
            print_size[0][1][1] = size_l;
            print_size[1][1][1] = size_c;
          }
        }

        for(int dir_i = 0; dir_i < 2; dir_i++)
        {
          for(int fld_i = 0; fld_i < 2 && print_size[0][dir_i][fld_i] > 0; fld_i++)
          {
            if(dir_i == 0)
              fprintf(frameLogFile,"\n            F ref block %d",fld_i);
            else
              fprintf(frameLogFile,"\n            B ref block %d",fld_i);

            fprintf(frameLogFile,"\n            Y:");
            for(int i = 0; i < print_size[0][dir_i][fld_i]; i++)fprintf(frameLogFile," %d",mb_debug_info[k].YBlockRec[dir_i][fld_i][i]);

            fprintf(frameLogFile,"\n            U:");
            for(int i = 0; i < print_size[1][dir_i][fld_i]; i++)fprintf(frameLogFile," %d",mb_debug_info[k].UBlockRec[dir_i][fld_i][i]);

            fprintf(frameLogFile,"\n            V:");
            for(int i = 0; i < print_size[1][dir_i][fld_i]; i++)fprintf(frameLogFile," %d",mb_debug_info[k].VBlockRec[dir_i][fld_i][i]);
          }
        }
      }
      fprintf(frameLogFile,"\n\n");
    }
  }
  if(frameLogFile)fclose(frameLogFile);
}

void MPEG2EncDebug::Init(MPEG2VideoEncoderBase *encoder_d)
{
  encoder            = encoder_d;
  use_manual_setup   = MP2_DBG_MANUAL_SETUP;
  use_reference_log  = MP2_DBG_ADD_REF_DATA;

  use_mb_type        = MP2_DBG_ADD_MB_TYPE;
  use_mb_MVinfo      = MP2_DBG_ADD_MB_MVINFO;
  use_quantizer_info = MP2_DBG_ADD_QUANT;

  minFrameIndex      = MP2_DBG_FRAME_MIN;
  maxFrameIndex      = MP2_DBG_FRAME_MAX;

  debug_logs_folder = (char*)malloc(1024);
  if(use_manual_setup)
  {
    printf("Debug mode.\nEnter folder to put logs in it:");
    scanf("%s",debug_logs_folder);

    printf("\nEnter mode (1 - with MB data, 0 - without):");
    scanf("%d",&use_extended_log);
  }
  else
  {
    use_extended_log  = MP2_DBG_EXTENDED_MODE;
    strcpy(debug_logs_folder,MP2_DBG_DEBUG_LOG_FOLDER);
  }

//allocating memory
  int mb_num = (encoder->encodeInfo.info.clip_info.height*encoder->encodeInfo.info.clip_info.width/256);
  if(encoder->picture_structure != FRAME_PICTURE) mb_num /=2;
  mb_debug_info = (MbDebugInfo*)malloc(mb_num*sizeof(MbDebugInfo));
}

void MPEG2EncDebug::Free()
{
  if(debug_logs_folder != NULL)
  {
    free(debug_logs_folder);
  }
  if(mb_debug_info != NULL)
  {
    free(mb_debug_info);
  }
}
#endif // MPEG2_ENC_DEBUG
#endif // UMC_ENABLE_MPEG2_VIDEO_ENCODER