//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENC)

#include <stdlib.h>
#include <ipps.h>
#include "mfx_enc_common.h"
#include "mfx_mpeg2_enc.h"
#include "mfx_mpeg2_enc_defs.h"
#include "ippi.h"
//#include "umc_mpeg2_video_encoder.h"
//#include "umc_mpeg2_enc.h"
//#include "umc_mpeg2_enc_defs.h"

// temporary defines
//#define RET_UMC_TO_MFX(umc_ret) ConvertStatusUmc2Mfx(umc_ret)

#define CHECK_VERSION(ver) /*ver.Major==0;*/
#define CHECK_CODEC_ID(id, myid) /*id==myid;*/

//////////////////////////////////////
/* zigzag scan order ISO/IEC 13818-2, 7.3, fig. 7-2 */
static Ipp32s mfx_mpeg2_ZigZagScan[64] =
{
  0, 1, 8,16, 9, 2, 3,10,17,24,32,25,18,11, 4, 5,12,19,26,33,40,48,41,34,27,20,13, 6, 7,14,21,28,
  35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
};

/* alternate scan order ISO/IEC 13818-2, 7.3, fig. 7-3 */
static Ipp32s mfx_mpeg2_AlternateScan[64] =
{
  0,
  8,

  16,         24, 1,  9,              2,  10, 17,
  25,         32,         40,         48,         56,
  57,         49,             41,     33,             26,
  18,         3,              11,     4,              12,
  19,         27,             34,     42,             50,
  58,         35,             43,     51,             59,
  20,         28,         5,          13,         6,
  14,         21, 29, 36,             44, 52, 60,
  37,         45,                     53,
  61,         22,                     30,
  7,          15,                     23,
  31,         38,                     46,
  54,         62,                     39,
  47,         55,                     63
};

/* default intra quantization matrix ISO/IEC 13818-2, 6.3.11 */
static mfxU8 mfx_mpeg2_default_intra_qm[64] = {
  8,16,19,22,26,27,29,34,
  16,16,22,24,27,29,34,37,
  19,22,26,27,29,34,34,38,
  22,22,26,27,29,34,37,40,
  22,26,27,29,32,35,40,48,
  26,27,29,32,35,40,48,58,
  26,27,29,34,38,46,56,69,
  27,29,35,38,46,56,69,83
};

/* quant scale values table ISO/IEC 13818-2, 7.4.2.2 table 7-6 */
static mfxI32 mfx_mpeg2_Val_QScale[2][32] =
{
  /* linear q_scale */
  {0,  2,  4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
  32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62},
  /* non-linear q_scale */
  {0, 1,  2,  3,  4,  5,  6,  7,  8, 10, 12, 14, 16, 18, 20, 22,
  24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88, 96,104,112}
};

/* color index by block number */
const mfxI32 mfx_mpeg2_color_index[12] = {
  0, 0, 0, 0, 1, 2, 1, 2, 1, 2, 1, 2
};

/* reset DC value ISO/IEC 13818-2, 7.2.1, Table 7-2. */
const mfxI32 mfx_mpeg2_ResetTbl[4] =
{
  128, 256, 512, 1024
};


// while don't know how
// enum { MFX_MB_DATA_SIZE_ALIGNMENT = 128 / 8 };
#define BLOCK_DATA(bl) (pcoeff+(bl)*64)

#define MB_SKIPPED(pmb) (pmb->Skip8x8Flag!=0)
#define MB_FSEL(pmb,dir,i) ((mfxMbCodeAVC*)pmb)->RefPicSelect[dir][i] // tmp cast
#define MB_MV(pmb,dir,num) (pmb)->MV[(dir)*4+(num)]
#define MFX_COMP_CBP(pmb, cbp) { \
  mfxI32 bb; \
  pmb->CodedPattern4x4V=0, pmb->CodedPattern4x4U=0, pmb->CodedPattern4x4Y=0; \
  for(bb=0; bb<m_block_count; bb++) { \
    if(cbp & (1 << (m_block_count-1-bb))) { \
      if(bb<4) \
        pmb->CodedPattern4x4Y |= 0xf000>>bb*4; \
      else if(!(bb & 1)) \
        pmb->CodedPattern4x4U |= 0xf<<((bb-4)>>1)*4; \
      else \
        pmb->CodedPattern4x4V |= 0xf<<((bb-4)>>1)*4; \
    } \
  } \
}

static mfxStatus TranslateMfxMbTypeMPEG2(mfxMbCodeMPEG2* pmb, MBInfo* mbi) {
  mbi->skipped = MB_SKIPPED(pmb);
  mbi->dct_type = pmb->TransformFlag ? DCT_FIELD : DCT_FRAME;
  mbi->prediction_type = 0;

  if(pmb->IntraMbFlag) // MFX_TYPE_INTRA_MPEG2 is 6 bits!!!
    mbi->mb_type = MB_INTRA;
  else
    switch(pmb->MbType5Bits) {
    case MFX_MBTYPE_INTER_16X16_0: mbi->mb_type = MB_FORWARD; mbi->prediction_type = MC_FRAME; break;
    case MFX_MBTYPE_INTER_16X16_1: mbi->mb_type = MB_BACKWARD; mbi->prediction_type = MC_FRAME; break;
    case MFX_MBTYPE_INTER_16X16_2: mbi->mb_type = MB_FORWARD|MB_BACKWARD; mbi->prediction_type = MC_FRAME; break;
    case MFX_MBTYPE_INTER_16X8_00: mbi->mb_type = MB_FORWARD; mbi->prediction_type = MC_FIELD; break;
    case MFX_MBTYPE_INTER_16X8_11: mbi->mb_type = MB_BACKWARD; mbi->prediction_type = MC_FIELD; break;
    case MFX_MBTYPE_INTER_16X8_22: mbi->mb_type = MB_FORWARD|MB_BACKWARD; mbi->prediction_type = MC_FIELD; break;
    case MFX_MBTYPE_INTER_DUAL_PRIME: mbi->mb_type = MB_FORWARD; mbi->prediction_type = MC_DMV; break;
    default:
      return MFX_ERR_UNSUPPORTED;
  }
  return MFX_ERR_NONE;
}

static mfxExtBuffer* GetExtBuffer(mfxFrameCUC* cuc, mfxU32 id)
{
  for (mfxU32 i = 0; i < cuc->NumExtBuffer; i++)
    if (cuc->ExtBuffer[i] != 0 && cuc->ExtBuffer[i]->BufferId == id)
      return cuc->ExtBuffer[i];
  return 0;
}

mfxStatus MFXVideoENCMPEG2::Query(mfxVideoParam *in, mfxVideoParam *out)
{
  MFX_CHECK_NULL_PTR1(out)
  if(in==0) {
    memset(&out->mfx, 0, sizeof(mfxInfoMFX));
    out->mfx.FrameInfo.FourCC = 1;
    out->mfx.FrameInfo.Width = 1;
    out->mfx.FrameInfo.Height = 1;
    out->mfx.FrameInfo.CropX = 1;
    out->mfx.FrameInfo.CropY = 1;
    out->mfx.FrameInfo.CropW = 1;
    out->mfx.FrameInfo.CropH = 1;
    out->mfx.FrameInfo.ChromaFormat = 1;
    out->mfx.FrameInfo.AspectRatioW = 1;
    out->mfx.FrameInfo.AspectRatioH = 1;

    out->mfx.FrameInfo.FrameRateExtN = 1;
    out->mfx.FrameInfo.FrameRateExtD = 1;

    out->mfx.FrameInfo.PicStruct = 1;
    out->mfx.CodecProfile = 1;
    out->mfx.CodecLevel = 1;
    //out->mfx.ClosedGopOnly = 1;
    out->mfx.GopPicSize = 1;
    out->mfx.GopRefDist = 1;
    out->mfx.GopOptFlag = 1;
    out->mfx.RateControlMethod = 1; // not sure, it is BRC
    out->mfx.InitialDelayInKB = 1; // not sure, it is BRC
    out->mfx.BufferSizeInKB = 1; // not sure, it is BRC
    out->mfx.TargetKbps = 1;
    out->mfx.MaxKbps = 1; // not sure, it is BRC
    out->mfx.NumSlice = 1;
    out->mfx.NumThread = 1;
    out->mfx.TargetUsage = 1;

  } else {
    *out = *in;
    if (out->mfx.FrameInfo.FourCC != MFX_FOURCC_YV12 &&
        //out->mfx.FrameInfo.FourCC != MFX_FOURCC_IMC3 &&
        //out->mfx.FrameInfo.FourCC != MFX_FOURCC_IMC4 &&
        out->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12)
      out->mfx.FrameInfo.FourCC = MFX_FOURCC_YV12;

    out->mfx.FrameInfo.PicStruct &= MFX_PICSTRUCT_PROGRESSIVE |
      MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF;
    if (out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
        out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_TFF &&
        out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_BFF)
      out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    if (out->mfx.FrameInfo.Width > 0xff0)
      out->mfx.FrameInfo.Width = 0xff0;
    if ((out->mfx.FrameInfo.CropW == 0 || out->mfx.FrameInfo.CropW > out->mfx.FrameInfo.Width) && (out->mfx.FrameInfo.Width > 0))
      out->mfx.FrameInfo.CropW = out->mfx.FrameInfo.Width;
    out->mfx.FrameInfo.Width = (out->mfx.FrameInfo.Width + 15) & ~15;

    if (out->mfx.FrameInfo.Height > 0xff0)
      out->mfx.FrameInfo.Height = 0xff0;
    if ((out->mfx.FrameInfo.CropH == 0 || out->mfx.FrameInfo.CropH > out->mfx.FrameInfo.Height) && (out->mfx.FrameInfo.Height > 0))
      out->mfx.FrameInfo.CropH = out->mfx.FrameInfo.Height;
    if(/*out->mfx.FramePicture&&*/ (out->mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE))
      out->mfx.FrameInfo.Height = (out->mfx.FrameInfo.Height + 15) & ~15;
    else
      out->mfx.FrameInfo.Height = (out->mfx.FrameInfo.Height + 31) & ~31;

    out->mfx.FrameInfo.CropX = 0;
    out->mfx.FrameInfo.CropY = 0;

    if (out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 &&
        out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV422 &&
        out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444 )
      out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

    if (out->mfx.FrameInfo.AspectRatioW == 0 ||
        out->mfx.FrameInfo.AspectRatioH == 0 ) {
      out->mfx.FrameInfo.AspectRatioW = out->mfx.FrameInfo.CropW;
      out->mfx.FrameInfo.AspectRatioH = out->mfx.FrameInfo.CropH;
    }


    if (out->mfx.CodecProfile != MFX_PROFILE_MPEG2_SIMPLE &&
        out->mfx.CodecProfile != MFX_PROFILE_MPEG2_MAIN &&
        out->mfx.CodecProfile != MFX_PROFILE_MPEG2_HIGH)
      out->mfx.CodecProfile = MFX_PROFILE_MPEG2_MAIN;
    if (out->mfx.CodecLevel != MFX_LEVEL_MPEG2_LOW &&
        out->mfx.CodecLevel != MFX_LEVEL_MPEG2_MAIN &&
        out->mfx.CodecLevel != MFX_LEVEL_MPEG2_HIGH1440 &&
        out->mfx.CodecLevel != MFX_LEVEL_MPEG2_HIGH)
      out->mfx.CodecLevel = MFX_LEVEL_MPEG2_HIGH;
    if (out->mfx.RateControlMethod != MFX_RATECONTROL_CBR &&
        out->mfx.RateControlMethod != MFX_RATECONTROL_VBR)
      out->mfx.RateControlMethod = MFX_RATECONTROL_CBR;

    out->mfx.GopOptFlag &= MFX_GOP_CLOSED | MFX_GOP_STRICT;
    out->mfx.NumThread = 1;

    if (out->mfx.TargetUsage < MFX_TARGETUSAGE_BEST_QUALITY ||
        out->mfx.TargetUsage > MFX_TARGETUSAGE_BEST_SPEED )
      out->mfx.TargetUsage = MFX_TARGETUSAGE_UNKNOWN;
  }

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCMPEG2::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
  MFX_CHECK_NULL_PTR1(par)
  MFX_CHECK_NULL_PTR1(request)
  CHECK_VERSION(par->Version);
  CHECK_CODEC_ID(par->mfx.CodecId, MFX_CODEC_MPEG2);

  if (par->mfx.FrameInfo.Width > 0xff0 || (par->mfx.FrameInfo.Width & 0x0f) != 0)
  {
    return MFX_ERR_UNSUPPORTED;
  }
  mfxU32 mask = (/*par->mfx.FramePicture&&*/ (par->mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE))? 0x0f:0x1f;

  if (par->mfx.FrameInfo.Height > 0xff0 || (par->mfx.FrameInfo.Height & mask) != 0 )
  {
    return MFX_ERR_UNSUPPORTED;
  }

  request->Info              = par->mfx.FrameInfo;
  request->NumFrameMin       = 4; // 2 refs, 1 input, 1 for async
  request->NumFrameSuggested = 4;

  request->Type              = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME;

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCMPEG2::Init(mfxVideoParam *par)
{
  m_cuc = 0;
  m_frame.CucId = 0;
  m_slice.CucId = 0;

  m_me_alg_num = 3; // no way to control

  return Reset(par);
}

//virtual mfxStatus Reset(mfxVideoParam *par);
mfxStatus MFXVideoENCMPEG2::Reset(mfxVideoParam *par)
{
  mfxI32 i;
  MFX_CHECK_NULL_PTR1(par)
  CHECK_VERSION(par->Version);
  CHECK_CODEC_ID(par->mfx.CodecId, MFX_CODEC_MPEG2);

  if (par->mfx.FrameInfo.CropX!=0 || par->mfx.FrameInfo.CropY!=0 ||
      par->mfx.FrameInfo.Width > 0xff0 || par->mfx.FrameInfo.Height > 0xff0 ||
      par->mfx.FrameInfo.CropW > par->mfx.FrameInfo.Width ||
      par->mfx.FrameInfo.CropH > par->mfx.FrameInfo.Height)
    return MFX_ERR_UNSUPPORTED;

  m_info = par->mfx;

  // chroma format dependent
  switch (m_info.FrameInfo.ChromaFormat-0) {
    default:
      m_info.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420+0;
    case MFX_CHROMAFORMAT_YUV420:
      m_block_count = 6;
      if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
        func_getdiff_frame_c = tmp_ippiGetDiff8x8_8u16s_C2P2; // NV12 func!!!
        func_getdiff_field_c = tmp_ippiGetDiff8x4_8u16s_C2P2; // NV12 func!!!
        func_getdiffB_frame_c = tmp_ippiGetDiff8x8B_8u16s_C2P2; // NV12 func!!!
        func_getdiffB_field_c = tmp_ippiGetDiff8x4B_8u16s_C2P2; // NV12 func!!!
        func_mc_frame_c = tmp_ippiMC8x8_8u_C2P2; // NV12 func!!!
        func_mc_field_c = tmp_ippiMC8x4_8u_C2P2; // NV12 func!!!
        func_mcB_frame_c = tmp_ippiMC8x8B_8u_C2P2; // NV12 func!!!
        func_mcB_field_c = tmp_ippiMC8x4B_8u_C2P2; // NV12 func!!!
      } else {
        func_getdiff_frame_c = ippiGetDiff8x8_8u16s_C1;
        func_getdiff_field_c = ippiGetDiff8x4_8u16s_C1;
        func_getdiffB_frame_c = ippiGetDiff8x8B_8u16s_C1;
        func_getdiffB_field_c = ippiGetDiff8x4B_8u16s_C1;
        func_mc_frame_c = ippiMC8x8_8u_C1;
        func_mc_field_c = ippiMC8x4_8u_C1;
        func_mcB_frame_c = ippiMC8x8B_8u_C1;
        func_mcB_field_c = ippiMC8x4B_8u_C1;
      }
      break;

    case MFX_CHROMAFORMAT_YUV422:
      m_block_count = 8;
      func_getdiff_frame_c = ippiGetDiff8x16_8u16s_C1;
      func_getdiff_field_c = ippiGetDiff8x8_8u16s_C1;
      func_getdiffB_frame_c = ippiGetDiff8x16B_8u16s_C1;
      func_getdiffB_field_c = ippiGetDiff8x8B_8u16s_C1;
      func_mc_frame_c = ippiMC8x16_8u_C1;
      func_mc_field_c = ippiMC8x8_8u_C1;
      func_mcB_frame_c = ippiMC8x16B_8u_C1;
      func_mcB_field_c = ippiMC8x8B_8u_C1;
      break;

    case MFX_CHROMAFORMAT_YUV444:
      m_block_count = 12;
      func_getdiff_frame_c = ippiGetDiff16x16_8u16s_C1;
      func_getdiff_field_c = ippiGetDiff16x8_8u16s_C1;
      func_getdiffB_frame_c = ippiGetDiff16x16B_8u16s_C1;
      func_getdiffB_field_c = ippiGetDiff16x8B_8u16s_C1;
      func_mc_frame_c = ippiMC16x16_8u_C1;
      func_mc_field_c = ippiMC16x8_8u_C1;
      func_mcB_frame_c = ippiMC16x16B_8u_C1;
      func_mcB_field_c = ippiMC16x8B_8u_C1;
      break;
  }

  BlkWidth_c = (m_info.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444+0) ? 8 : 16;
  BlkHeight_c = (m_info.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420+0) ? 8 : 16;
  chroma_fld_flag = (m_info.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420+0) ? 0 : 1;

  for (i = 0; i < 4; i++) {
    frm_dct_step[i] = BlkStride_l*sizeof(Ipp16s);
    fld_dct_step[i] = 2*BlkStride_l*sizeof(Ipp16s);
  }
  for (i = 4; i < m_block_count; i++) {
    frm_dct_step[i] = BlkStride_c*sizeof(Ipp16s);
    fld_dct_step[i] = BlkStride_c*sizeof(Ipp16s) << chroma_fld_flag;
  }
  block_offset_frm[0]  = 0;
  block_offset_frm[1]  = 8;
  block_offset_frm[4]  = 0;
  block_offset_frm[5]  = 0;
  block_offset_frm[8]  = 8;
  block_offset_frm[9]  = 8;

  block_offset_fld[0]  = 0;
  block_offset_fld[1]  = 8;
  block_offset_fld[4]  = 0;
  block_offset_fld[5]  = 0;
  block_offset_fld[8]  = 8;
  block_offset_fld[9]  = 8;

  frm_diff_off[0] = 0;
  frm_diff_off[1] = 8;
  frm_diff_off[2] = BlkStride_l*8;
  frm_diff_off[3] = BlkStride_l*8 + 8;
  frm_diff_off[4] = OFF_U;
  frm_diff_off[5] = OFF_V;
  frm_diff_off[6] = OFF_U + BlkStride_c*8;
  frm_diff_off[7] = OFF_V + BlkStride_c*8;
  frm_diff_off[8] = OFF_U + 8;
  frm_diff_off[9] = OFF_V + 8;
  frm_diff_off[10] = OFF_U + BlkStride_c*8 + 8;
  frm_diff_off[11] = OFF_V + BlkStride_c*8 + 8;

  fld_diff_off[0] = 0;
  fld_diff_off[1] = 8;
  fld_diff_off[2] = BlkStride_l;
  fld_diff_off[3] = BlkStride_l + 8;
  fld_diff_off[4] = OFF_U;
  fld_diff_off[5] = OFF_V;
  fld_diff_off[6] = OFF_U + BlkStride_c;
  fld_diff_off[7] = OFF_V + BlkStride_c;
  fld_diff_off[8] = OFF_U + 8;
  fld_diff_off[9] = OFF_V + 8;
  fld_diff_off[10] = OFF_U + BlkStride_c + 8;
  fld_diff_off[11] = OFF_V + BlkStride_c + 8;
  //if(m_info.FrameInfo.AspectRatio < 1 || m_info.FrameInfo.AspectRatio > 4) {
  //  m_info.FrameInfo.AspectRatio = 2;
  //}

  //mfxU8 fr_code = TranslateMfxFRCodeMPEG2(m_info.FrameInfo.FrameRateCode);
  //if(fr_code>0 && fr_code<=8) {
  //  const Ipp64f ratetab[8]=
  //  {24000.0/1001.0,24.0,25.0,30000.0/1001.0,30.0,50.0,60000.0/1001.0,60.0};
  //  framerate = ratetab[fr_code - 1]
  //  * (m_info.FrameInfo.FrameRateExtN  + 1)/ (m_info.FrameInfo.FrameRateExtD + 1);
  //} else
  //  return MFX_ERR_UNSUPPORTED; // lack of return codes;

  //m_LastFrameNumber = 0;

  for(i=0; i<64; i++) {
    m_qm.IntraQMatrix[i] = mfx_mpeg2_default_intra_qm[i];
    m_qm.ChromaIntraQMatrix[i] = mfx_mpeg2_default_intra_qm[i];
    _IntraQM[i] = mfx_mpeg2_default_intra_qm[i];
    _IntraChromaQM[i] = mfx_mpeg2_default_intra_qm[i];
    m_qm.InterQMatrix[i] = 16;
    m_qm.ChromaInterQMatrix[i] = 16;
  }
  InvIntraQM = 0;
  InvInterQM = 0;
  InvIntraChromaQM = 0;
  InvInterChromaQM = 0;
  IntraQM = _IntraQM;
  InterQM = 0;
  IntraChromaQM = _IntraChromaQM;
  InterChromaQM = 0;

  // allocate MBinfo:
  mfxU32 cnt = ((m_info.FrameInfo.Width + 15)>>4) * ((m_info.FrameInfo.Height + 31)>>5) * 2;
  if(cnt > allocatedMBInfo) {
    if(allocatedMBInfo>0) {
      m_core->UnlockBuffer(mid_MBInfo);
      m_core->FreeBuffer(mid_MBInfo);
      allocatedMBInfo = 0;
    }
    mfxStatus sts = m_core->AllocBuffer(cnt*sizeof(MBInfo), MFX_MEMTYPE_PERSISTENT_MEMORY, &mid_MBInfo);
    MFX_CHECK_STS(sts);
    allocatedMBInfo = cnt;
    sts = m_core->LockBuffer(mid_MBInfo, (mfxU8**)&pMBInfo);
    pMBInfo0 = pMBInfo;
    MFX_CHECK_STS(sts);
  }

  //allocate me_matrix
  mfxU32 sz = IPP_MAX(m_info.FrameInfo.Width, 8*2<<5) * IPP_MAX(m_info.FrameInfo.Height, 8*2<<4) * 4;
  if(sz > me_matrix_size) {
    if(me_matrix_size>0) {
      m_core->UnlockBuffer(mid_me_matrix);
      m_core->FreeBuffer(mid_me_matrix);
      me_matrix_size = 0;
    }
    mfxStatus sts = m_core->AllocBuffer(sz*sizeof(mfxU8), MFX_MEMTYPE_PERSISTENT_MEMORY, &mid_me_matrix);
    MFX_CHECK_STS(sts);
    me_matrix_size = sz;
    sts = m_core->LockBuffer(mid_me_matrix, &me_matrix_buff);
    MFX_CHECK_STS(sts);
    //ippsZero_8u(me_matrix_buff, me_matrix_size);
    me_matrix_active_size = 0;
    me_matrix_id = 0;
  }

  return MFX_ERR_NONE;
}

//virtual mfxStatus Close(void); // same name
mfxStatus MFXVideoENCMPEG2::Close(void)
{
  if(allocatedMBInfo>0) {
    m_core->UnlockBuffer(mid_MBInfo);
    m_core->FreeBuffer(mid_MBInfo);
    allocatedMBInfo = 0;
  }
  if(me_matrix_size>0) {
    m_core->UnlockBuffer(mid_me_matrix);
    m_core->FreeBuffer(mid_me_matrix);
    me_matrix_size = 0;
  }
  return MFX_ERR_NONE;
}


mfxStatus MFXVideoENCMPEG2::GetVideoParam(mfxVideoParam *par)
{
  MFX_CHECK_NULL_PTR1(par)
  CHECK_VERSION(par->Version);
  par->mfx = m_info;
  par->mfx.CodecId = MFX_CODEC_MPEG2;



  return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCMPEG2::GetFrameParam(mfxFrameParam *par)
{
  //if (m_frame.CucId == 0)
  //  return MFX_ERR_NOT_INITIALIZED;
  MFX_CHECK_NULL_PTR1(par)

  par->MPEG2 = m_frame;
  return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCMPEG2::GetSliceParam(mfxSliceParam *par)
{
  MFX_CHECK_NULL_PTR1(par)
  //if (m_slice.CucId == 0)
  //  return MFX_ERR_NOT_INITIALIZED;

  par->MPEG2 = m_slice;
  return MFX_ERR_NONE;
}



mfxStatus MFXVideoENCMPEG2::RunSliceVmeENC(mfxFrameCUC *cuc)
{
  mfxStatus sts;
  MFX_CHECK_NULL_PTR1(cuc);
  MFX_CHECK_NULL_PTR1(cuc->FrameParam);
  MFX_CHECK_NULL_PTR1(cuc->SliceParam);

  mfxSliceParamMPEG2 *slice = &cuc->SliceParam[cuc->SliceId].MPEG2;
  mfxU32 mbstart = slice->FirstMbX + slice->FirstMbY*(cuc->FrameParam->MPEG2.FrameWinMbMinus1+1);
  mfxI32 fcodes[4]; // dir*2+xy
  mfxI32 mvmax[4];
  MPEG2FrameState state;
  MFX_CHECK_NULL_PTR1(cuc->MbParam);
  MFX_CHECK_NULL_PTR1(cuc->MbParam->Mb);

  m_cuc = cuc;
  sts = loadCUCFrameParams(fcodes, mvmax);
  MFX_CHECK_STS(sts);

  sts = preEnc(&state);
  MFX_CHECK_STS(sts);

  sts = sliceME(&state, mbstart, slice->NumMb, fcodes, mvmax);
  postEnc(&state);

  MFX_CHECK_STS(sts);


  m_frame = cuc->FrameParam->MPEG2;
  m_slice = *slice;
  return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCMPEG2::RunSlicePredENC(mfxFrameCUC* /*cuc*/) {return MFX_ERR_UNSUPPORTED;}
mfxStatus MFXVideoENCMPEG2::RunSliceFullENC(mfxFrameCUC *cuc)
{
  mfxStatus sts;
  MFX_CHECK_NULL_PTR1(cuc);
  MFX_CHECK_NULL_PTR1(cuc->FrameParam);
  MFX_CHECK_NULL_PTR1(cuc->ExtBuffer);
  MFX_CHECK_NULL_PTR1(cuc->SliceParam);

  mfxSliceParamMPEG2 *slice = &cuc->SliceParam[cuc->SliceId].MPEG2;
  mfxU32 mbstart = slice->FirstMbX + slice->FirstMbY*(cuc->FrameParam->MPEG2.FrameWinMbMinus1+1);
  mfxI32 fcodes[4]; // dir*2+xy
  mfxI32 mvmax[4];
  MPEG2FrameState state;
  MFX_CHECK_NULL_PTR1(cuc->MbParam);
  MFX_CHECK_NULL_PTR1(cuc->MbParam->Mb);

  m_cuc = cuc;
  sts = loadCUCFrameParams(fcodes, mvmax);
  MFX_CHECK_STS(sts);

  mfxExtMpeg2Coeffs* coeff = (mfxExtMpeg2Coeffs*)GetExtBuffer( cuc, MFX_CUC_MPEG2_RESCOEFF);
  MFX_CHECK_NULL_PTR2(coeff, coeff->Coeffs);

  sts = preEnc(&state);
  MFX_CHECK_STS(sts);

  sts = sliceME(&state, mbstart, slice->NumMb, fcodes, mvmax);
  MFX_CHECK_STS(sts);

  sts = sliceCoef(&state, mbstart, slice->NumMb, coeff);
  MFX_CHECK_STS(sts);

  postEnc(&state);


  m_frame = cuc->FrameParam->MPEG2;
  m_slice = *slice;
  return MFX_ERR_NONE;
}
mfxStatus MFXVideoENCMPEG2::GetSliceResid(mfxFrameCUC* /*cuc*/) {return MFX_ERR_UNSUPPORTED;}
mfxStatus MFXVideoENCMPEG2::RunSliceFTQ(mfxFrameCUC* /*cuc*/) {return MFX_ERR_UNSUPPORTED;}

#undef CALC_START_STOP_ROWS
#define CALC_START_STOP_ROWS \
  start_y = start_uv = 0;  \
  stop_y = state->YFrameVSize;    \
  k = 0;


using namespace UMC;

mfxStatus MFXVideoENCMPEG2::RunFrameVmeENC(mfxFrameCUC *cuc)
{
  mfxU32 i;
  mfxStatus sts;
  MFX_CHECK_NULL_PTR1(cuc)
  MFX_CHECK_NULL_PTR1(cuc->FrameParam);
  CHECK_VERSION(cuc->Version);
  mfxI32 fcodes[4]; // dir*2+xy
  mfxI32 mvmax[4];
  MPEG2FrameState state;
  MFX_CHECK_NULL_PTR1(cuc->MbParam);
  MFX_CHECK_NULL_PTR1(cuc->MbParam->Mb);
  m_cuc = cuc;

  sts = loadCUCFrameParams(fcodes, mvmax);
  MFX_CHECK_STS(sts);

  sts = preEnc(&state);
  MFX_CHECK_STS(sts);

  mfxU32 numSlices = cuc->NumSlice;
  for(i=0; i<numSlices; i++) {
    mfxSliceParamMPEG2 *slice = &cuc->SliceParam[i].MPEG2;
    mfxU32 mbstart = slice->FirstMbX + slice->FirstMbY*(cuc->FrameParam->MPEG2.FrameWinMbMinus1+1);

    sts = sliceME(&state, mbstart, slice->NumMb, fcodes, mvmax);
    MFX_CHECK_STS(sts);

  }

  postEnc(&state);

  //GetFrameParam(cuc->FrameParam); // to store frame params
  m_frame = cuc->FrameParam->MPEG2;

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCMPEG2::RunFramePredENC(mfxFrameCUC* /*cuc*/) {return MFX_ERR_UNSUPPORTED;}
mfxStatus MFXVideoENCMPEG2::RunFrameFullENC(mfxFrameCUC *cuc)
{
  mfxStatus sts;
  mfxI32 i;
  MFX_CHECK_NULL_PTR1(cuc)
  MFX_CHECK_NULL_PTR1(cuc->FrameParam);
  MFX_CHECK_NULL_PTR1(cuc->ExtBuffer);
  CHECK_VERSION(cuc->Version);
  mfxI32 fcodes[4]; // dir*2+xy
  mfxI32 mvmax[4];
  MPEG2FrameState state;
  MFX_CHECK_NULL_PTR1(cuc->MbParam);
  MFX_CHECK_NULL_PTR1(cuc->MbParam->Mb);

  m_cuc = cuc;

  updateQMatrices();

  sts = loadCUCFrameParams(fcodes, mvmax);
  MFX_CHECK_STS(sts);

  mfxExtMpeg2Coeffs* coeff = (mfxExtMpeg2Coeffs*)GetExtBuffer( cuc, MFX_CUC_MPEG2_RESCOEFF);
  MFX_CHECK_NULL_PTR2(coeff, coeff->Coeffs);

  sts = preEnc(&state);
  MFX_CHECK_STS(sts);

  mfxI32 numSlices = cuc->NumSlice;
  for(i=0; i<numSlices; i++) {
    mfxSliceParamMPEG2 *slice = &cuc->SliceParam[i].MPEG2;
    mfxU32 mbstart = slice->FirstMbX + slice->FirstMbY*(cuc->FrameParam->MPEG2.FrameWinMbMinus1+1);

    sts = sliceME(&state, mbstart, slice->NumMb, fcodes, mvmax);
    MFX_CHECK_STS(sts);

    sts = sliceCoef(&state, mbstart, slice->NumMb, coeff);
    MFX_CHECK_STS(sts);
  }
  postEnc(&state);

  //GetFrameParam(cuc->FrameParam); // to store frame params
  m_frame = cuc->FrameParam->MPEG2;
  return MFX_ERR_NONE;
}


mfxStatus MFXVideoENCMPEG2::GetFrameResid(mfxFrameCUC* /*cuc*/) {return MFX_ERR_UNSUPPORTED;}
mfxStatus MFXVideoENCMPEG2::RunFrameFTQ(mfxFrameCUC* /*cuc*/) {return MFX_ERR_UNSUPPORTED;}
// convert cuc params to/from internals
mfxStatus MFXVideoENCMPEG2::loadCUCFrameParams(
  mfxI32* fcodes, // dir*2+xy
  mfxI32* mvmax
  )
{
  mfxI32 i;
  // derive picture_structure and picture_coding_type
  if(m_cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_I) picture_coding_type = MPEG2_I_PICTURE; else
  if(m_cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_P) picture_coding_type = MPEG2_P_PICTURE; else
  if(m_cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_B) picture_coding_type = MPEG2_B_PICTURE; else
    return MFX_ERR_UNSUPPORTED;

  for(i=0; i<4; i++) {
    fcodes[i] = (m_cuc->FrameParam->MPEG2.BitStreamFcodes>>(12-(i*4))) & 15;
    if(fcodes[i]<1) return MFX_ERR_UNSUPPORTED;
    //m_MotionData.f_code[i>>1][i&1] = fcodes[i];
    if(fcodes[i] == 15) continue;
    mvmax[i] = 8<<fcodes[i];
  }
  return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCMPEG2::sliceME( MPEG2FrameState* state, mfxU32 mbstart, mfxU32 mbcount,
                                     mfxI32* /*fcodes*/, // dir*2+xy
                                     mfxI32* mvmax )
{
  const Ipp8u *pRef[2]; // parity same/opposite
  const Ipp8u *pRec[2]; // parity same/opposite
  Ipp32s      i, j, ic, jc, pred_ind, blk;
  Ipp32s      macroblock_address_increment;
  Ipp16s      *pDiff;
  IppMotionVector2   vector[3][2] = {0,}; // top/bottom/frame F/B
  Ipp32s      me_bound_left[2], me_bound_right[2];
  Ipp32s      me_bound_top[2], me_bound_bottom[2];
  Ipp32s      me_bound_2_top[2], me_bound_1_bottom[2];
  MB_prediction_info pred_info[2];
  MB_prediction_info *bestpred = pred_info;
  MB_prediction_info *currpred = pred_info + 1;
  mfxI16      Diff[2][64*12];
  IppiPoint   PMV[2][2];
  const Ipp8u *YBlock;
  const Ipp8u *UBlock;
  const Ipp8u *VBlock;
  Ipp32s      mean_frm[4], mean_fld[4];
  Ipp32s      slice_past_intra_address;
  Ipp32s      slice_macroblock_address;
  Ipp32s      dc_dct_pred[3];
  //Ipp32s      start_y;
  //Ipp32s      start_uv;
  //Ipp32s      stop_y;
  Ipp32s      skip_flag;
  Ipp32s      dir0, dir1;
  mfxMbCodeMPEG2   *Mb = &m_cuc->MbParam->Mb[mbstart].MPEG2;
  mfxU32      k;
  mfxI32      varThreshold;          // threshold for block variance
  mfxI32      meanThreshold;         // threshold for block mean
  mfxI32      sadThreshold;          // threshold for block mean
  MBInfo      *mbinfo = pMBInfo;
  mfxU32      curr;
  const IppiPoint MV_ZERO = {0,0};
  mfxI32      MBcountV = m_cuc->FrameParam->MPEG2.FrameHinMbMinus1+1;
  mfxI32      MBcountH = m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1;
  mfxI32      YFrameHSize = state->YFrameHSize;
  mfxI32      UVFrameHSize = state->UVFrameHSize;

  if(m_cuc->FrameParam->MPEG2.FieldPicFlag) {
    MBcountV >>= 1;
  }

  //// set Mb coords for all in slice // to CHECK if needed!!!
  Mb->MbYcnt = (mfxU8)(mbstart / (m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1));
  Mb->MbXcnt = (mfxU8)(mbstart % (m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1));
  for(k=1; k < mbcount; k++)
  {
    Mb[k].MbYcnt = Mb->MbYcnt;
    Mb[k].MbXcnt = (mfxU8)(Mb->MbXcnt+k);
  }

  if(picture_coding_type == MPEG2_I_PICTURE)
  {
    k = mbstart;
    j = Mb->MbYcnt*BlkHeight_l;
    jc = Mb->MbYcnt*BlkHeight_c;
    i = Mb->MbXcnt*BlkWidth_l;
    ic = Mb->MbXcnt*BlkWidth_c;
    for(; i < (Ipp32s)mbcount*BlkWidth_l; i += BlkWidth_l, ic += BlkWidth_c) {
      const Ipp8u *BlockSrc[3];
      Ipp32s cur_offset   = i  + j  * YFrameHSize;
      Ipp32s cur_offset_c = ic + jc * UVFrameHSize;
      if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
        cur_offset_c *= 2;
      }

      BlockSrc[0] = state->Y_src + cur_offset;
      BlockSrc[1] = state->U_src + cur_offset_c;
      BlockSrc[2] = state->V_src + cur_offset_c;

      pMBInfo[k].prediction_type = MC_FRAME;
      pMBInfo[k].mb_type         = MB_INTRA;
      pMBInfo[k].dct_type        = DCT_FRAME;

      if (!state->curr_frame_dct) {
        Ipp32s var_fld = 0, var = 0;
        ippiFrameFieldSAD16x16_8u32s_C1R(BlockSrc[0], YFrameHSize, &var, &var_fld);

        if (var_fld < var) {
          pMBInfo[k].dct_type = DCT_FIELD;
          //threadSpec[numTh].fieldCount ++;
          //} else {
          //threadSpec[numTh].fieldCount --;
        }
      }

      Mb = &m_cuc->MbParam->Mb[k].MPEG2;
      Mb->Skip8x8Flag = 0; //pMBInfo[k].skipped;
      Mb->TransformFlag = pMBInfo[k].dct_type == DCT_FIELD;
      Mb->MbType5Bits = MPEG2_Intra;
      Mb->IntraMbFlag = 1;
      //Mb->QpScaleCode = (mfxU8)quantiser_scale_code; // to CHECK if needed !!!
      //Mb->QpScaleType = (mfxU8)q_scale_type;

      k++;
    }
    return MFX_ERR_NONE;
  }

  dir0 = 2*state->B_count > state->P_distance ? 1 : 0;
  dir1 = 1 ^ dir0;

  //CALC_START_STOP_ROWS

  bestpred->pDiff = Diff[0];
  currpred->pDiff = Diff[1];

  //mfxExtMpeg2Coeffs* coeff = GetExtBuffer<mfxExtMpeg2Coeffs>(*m_cuc, MFX_LABEL_RESCOEFF);
  //MFX_CHECK_NULL_PTR2(coeff, coeff->Coeffs);
  //DctCoef *pcoeff = coeff->Coeffs;

#ifdef M2_USE_CME
  if (!bMEdone)
#endif // M2_USE_CME
  /*if (!bQuantiserChanged)*/ {
    Ipp32s mf, mb; // *2 to support IP case
    if(picture_coding_type == MPEG2_P_PICTURE) {
      mb = 2; // unused
      mf = state->ipflag ? 1 : 2*state->P_distance;
    } else {
      mb = 2*(state->B_count - state->P_distance);
      mf = 2*state->B_count;
    }
    for(k=mbstart; k < mbstart+mbcount; k++)
    {
      pMBInfo[k].MV[0][0].x = pMBInfo[k].MV_P[0].x*mf/512;
      pMBInfo[k].MV[0][0].y = pMBInfo[k].MV_P[0].y*mf/512;
      pMBInfo[k].MV[0][1].x = pMBInfo[k].MV_P[0].x*mb/512;
      pMBInfo[k].MV[0][1].y = pMBInfo[k].MV_P[0].y*mb/512;
      pMBInfo[k].MV[1][0].x = pMBInfo[k].MV_P[1].x*mf/512;
      pMBInfo[k].MV[1][0].y = pMBInfo[k].MV_P[1].y*mf/512;
      pMBInfo[k].MV[1][1].x = pMBInfo[k].MV_P[1].x*mb/512;
      pMBInfo[k].MV[1][1].y = pMBInfo[k].MV_P[1].y*mb/512;
    }
  }

  j = Mb->MbYcnt*BlkHeight_l;
  jc = Mb->MbYcnt*BlkHeight_c;
  k = mbstart;
  {
    //PutSliceHeader(j >> 4, numTh);
    macroblock_address_increment = 1;
    slice_macroblock_address = 0;
    slice_past_intra_address = 0;

    // reset predictors at the start of slice
    dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = mfx_mpeg2_ResetTbl[m_cuc->FrameParam->MPEG2.IntraDCprecision];

    ippsZero_8u((Ipp8u*)PMV, sizeof(PMV));
    BOUNDS_V(0,j)
    BOUNDS_V(1,j)
    if(m_cuc->FrameParam->MPEG2.FieldPicFlag) {
      BOUNDS_V_FIELD(0,j)
      BOUNDS_V_FIELD(1,j)
      //// to avoid reference below bottom in 16x8 mode when trying to skip MB // to check before try skip
      //me_bound_1_bottom[0] = me_bound_bottom[0];
      //me_bound_1_bottom[1] = me_bound_bottom[1];
    }

    i = Mb->MbXcnt*BlkWidth_l;
    ic = Mb->MbXcnt*BlkWidth_c;
    for(; i < (Ipp32s)mbcount*BlkWidth_l; i += BlkWidth_l, ic += BlkWidth_c)
    {
      Ipp32s goodpred = 0;
      Ipp32s cur_offset = i + j * YFrameHSize;
      Ipp32s cur_offset_c = ic + jc * UVFrameHSize;
      mfxMbCodeMPEG2 *Mb = &m_cuc->MbParam->Mb[k].MPEG2;
      mfxI32 q_scale_value = mfx_mpeg2_Val_QScale[m_cuc->FrameParam->MPEG2.QuantScaleType][Mb->QpScaleCode];
      varThreshold = q_scale_value;// + qq/32;
      meanThreshold = q_scale_value * 8;// + qq/32;
      sadThreshold = (q_scale_value + 0) * 8;// + qq/16;
      if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
        cur_offset_c *= 2;
      }
      curr = k;
      //Mb->MbXcnt = (mfxU8)(i>>4); // to CHECK if needed !!!
      //Mb->MbYcnt = (mfxU8)(j>>4);
      //Mb->QpScaleCode = (mfxU8)quantiser_scale_code;
      Mb->Skip8x8Flag = 0; //pMBInfo[k].skipped;

      YBlock = state->Y_src + cur_offset;
      UBlock = state->U_src + cur_offset_c;
      VBlock = state->V_src + cur_offset_c;

      //pRef[0] = YRefFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][DIR] + cur_offset;
      //pRef[1] = YRefFrame[1-m_cuc->FrameParam->MPEG2.BottomFieldFlag][DIR] + cur_offset;
      //pRec[0] = YRecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset;   // same parity
      //pRec[1] = YRecFrame[1-m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset; // opposite parity
      slice_macroblock_address = i >> 4;

      pMBInfo[k].skipped = 0;
      pMBInfo[k].mb_type = 0;
      pMBInfo[k].dct_type = DCT_FRAME;
      pMBInfo[k].prediction_type = MC_FRAME;
      VARMEAN_FRAME_Y(currpred->var, mean_frm, currpred->var_sum, YFrameHSize);
      //pMBInfo[k].var_sum = currpred->var_sum;

      // try to skip
      if(picture_coding_type == MPEG2_P_PICTURE) {
        if (k > mbstart && k < (mbstart + mbcount - 1) && !state->ipflag)
        {
          Ipp32s sad;
          //ippiSAD16x16_8u32s(YRefFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset, YFrameHSize,
          //  YBlock, YFrameHSize, &sad, 0);
          //skip_flag = (sad < quantiser_scale_value*256/8);
          Ipp32s var, mean;

          pDiff = Diff[0];

          ippiGetDiff16x16_8u16s_C1(YBlock, YFrameHSize,
            state->YRecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset, YFrameHSize,
            pDiff, 32, 0, 0, 0, 0);

          skip_flag = 1;

          for (blk = 0; blk < 4; blk++) {
            ippiVarSum8x8_16s32s_C1R(pDiff + frm_diff_off[blk], 32, &var, &mean);
            if(var > varThreshold) {
              skip_flag = 0;
              break;
            }
            if(mean >= meanThreshold || mean <= -meanThreshold) {
              skip_flag = 0;
              break;
            }
          }
          if (skip_flag) {

            skip_flag = 0;
            if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12)
              tmp_ippiSAD8x8_8u32s_C2R(state->URecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset_c,
                UVFrameHSize*2, UBlock, UVFrameHSize*2, &sad, 0);
            else
              ippiSAD8x8_8u32s_C1R(state->URecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset_c,
                UVFrameHSize, UBlock, UVFrameHSize, &sad, 0);
            if(sad < sadThreshold) {
              if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12)
                tmp_ippiSAD8x8_8u32s_C2R(state->VRecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset_c,
                  UVFrameHSize*2, VBlock, UVFrameHSize*2, &sad, 0);
              else
                ippiSAD8x8_8u32s_C1R(state->VRecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset_c,
                  UVFrameHSize, VBlock, UVFrameHSize, &sad, 0);
              if(sad < sadThreshold) {
                skip_flag = 1;
              }
            }
          }
          if (skip_flag) {
            //skip_macroblock:
            // skip this macroblock
            // no DCT coefficients and neither first nor last macroblock of slice and no motion
            ippsZero_8u((Ipp8u*)PMV, sizeof(PMV)); // zero vectors
            ippsZero_8u((Ipp8u*)pMBInfo[k].MV, sizeof(pMBInfo[k].MV)); // zero vectors
            pMBInfo[k].mb_type = 0; // skipped type

            Mb->MbType = MFX_MBTYPE_INTER_16X16_0; // clears flags; need + skip
            Mb->FieldMbFlag = m_cuc->FrameParam->MPEG2.FieldPicFlag;
            //Mb->IntraMbFlag = 0;
            ippsZero_8u((Ipp8u*)Mb->MV, sizeof(Mb->MV)); // zero vectors
            Mb->Skip8x8Flag = 0xf; //pMBInfo[k].skipped;
            Mb->CodedPattern4x4Y = 0;
            Mb->CodedPattern4x4U = 0;
            Mb->CodedPattern4x4V = 0;

            macroblock_address_increment++;
            k++;
            continue;
          }
        }
      }
      else if (k > mbstart && k < (mbstart + mbcount - 1) && // for B-picture
        !(pMBInfo[k-1].mb_type & MB_INTRA)
        && (!(pMBInfo[k - 1].mb_type & MB_FORWARD)
        || (((i+((PMV[0][0].x+1)>>1)+15) < MBcountH*16) && ((j+((PMV[0][0].y+1)>>1)+15) < MBcountV*16)))
        && (!(pMBInfo[k - 1].mb_type & MB_BACKWARD)
        || (((i+((PMV[0][1].x+1)>>1)+15) < MBcountH*16) && ((j+((PMV[0][1].y+1)>>1)+15) < MBcountV*16))) )
      {
        Ipp32s blk;
        Ipp32s var, mean;
        Ipp32s mb_type;

        pMBInfo[k].mv_field_sel[2][0] = m_cuc->FrameParam->MPEG2.BottomFieldFlag;
        pMBInfo[k].mv_field_sel[2][1] = m_cuc->FrameParam->MPEG2.BottomFieldFlag;

        mb_type = pMBInfo[k - 1].mb_type & (MB_FORWARD | MB_BACKWARD);
        pMBInfo[k].mb_type = mb_type;

        pDiff = Diff[0];

        SET_MOTION_VECTOR((&vector[2][0]), PMV[0][0].x, PMV[0][0].y);
        SET_MOTION_VECTOR((&vector[2][1]), PMV[0][1].x, PMV[0][1].y);

        if (mb_type == (MB_FORWARD|MB_BACKWARD))
        {
          GETDIFF_FRAME_FB(Y, Y, l, pDiff);
        }
        else if (mb_type & MB_FORWARD)
        {
          GETDIFF_FRAME(Y, Y, l, pDiff, 0);
        }
        else // if(mb_type & MB_BACKWARD)
        {
          GETDIFF_FRAME(Y, Y, l, pDiff, 1);
        }

        skip_flag = 1;
        for (blk = 0; blk < 4; blk++) {
          ippiVarSum8x8_16s32s_C1R(pDiff + frm_diff_off[blk], 32, &var, &mean);
          if(var > varThreshold) {
            skip_flag = 0;
            break;
          }
          if(mean >= meanThreshold || mean <= -meanThreshold) {
            skip_flag = 0;
            break;
          }
        }
        if (skip_flag) { // check UV
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
            if (mb_type == (MB_FORWARD|MB_BACKWARD))
            {
              Mb->MbType = MFX_MBTYPE_INTER_16X16_2; // can be changed if not skipped
              GETDIFF_FRAME_FB(U, Y, c, pDiff);
              GETDIFF_FRAME_FB(V, Y, c, pDiff);
            }
            else if (mb_type & MB_FORWARD)
            {
              Mb->MbType = MFX_MBTYPE_INTER_16X16_0; // can be changed if not skipped
              GETDIFF_FRAME(U, Y, c, pDiff, 0);
              GETDIFF_FRAME(V, Y, c, pDiff, 0);
            }
            else // if(mb_type & MB_BACKWARD)
            {
              Mb->MbType = MFX_MBTYPE_INTER_16X16_1; // can be changed if not skipped
              GETDIFF_FRAME(U, Y, c, pDiff, 1);
              GETDIFF_FRAME(V, Y, c, pDiff, 1);
            }
          } else {
            if (mb_type == (MB_FORWARD|MB_BACKWARD))
            {
              Mb->MbType = MFX_MBTYPE_INTER_16X16_2; // can be changed if not skipped
              GETDIFF_FRAME_FB(U, UV, c, pDiff);
              GETDIFF_FRAME_FB(V, UV, c, pDiff);
            }
            else if (mb_type & MB_FORWARD)
            {
              Mb->MbType = MFX_MBTYPE_INTER_16X16_0; // can be changed if not skipped
              GETDIFF_FRAME(U, UV, c, pDiff, 0);
              GETDIFF_FRAME(V, UV, c, pDiff, 0);
            }
            else // if(mb_type & MB_BACKWARD)
            {
              Mb->MbType = MFX_MBTYPE_INTER_16X16_1; // can be changed if not skipped
              GETDIFF_FRAME(U, UV, c, pDiff, 1);
              GETDIFF_FRAME(V, UV, c, pDiff, 1);
            }
          }
          for (blk = 4; blk < m_block_count; blk++) {
            ippiVarSum8x8_16s32s_C1R(pDiff + frm_diff_off[blk], 32, &var, &mean);
            if(var > varThreshold) {
              skip_flag = 0;
              break;
            }
            if(mean >= meanThreshold || mean <= -meanThreshold) {
              skip_flag = 0;
              break;
            }
          }
        }

        if (skip_flag)
        {
          // skip this B macroblock
          // no DCT coefficients and neither first nor last macroblock of slice and no motion
//          pMBInfo[k].mb_type = 0; // skipped type

          //Mb->MbType = MFX_MBTYPE_INTER_16X16_0; // clears flags; need + skip
          Mb->FieldMbFlag = m_cuc->FrameParam->MPEG2.FieldPicFlag;
          Mb->Skip8x8Flag = 0xf; //pMBInfo[k].skipped;
          //pcoeff += 64*m_block_count; // Mb->MbDataSize128b * MFX_MB_DATA_SIZE_ALIGNMENT;
          Mb->CodedPattern4x4Y = 0;
          Mb->CodedPattern4x4U = 0;
          Mb->CodedPattern4x4V = 0;

          macroblock_address_increment++;
          MFX_INTERNAL_CPY((Ipp8u*)pMBInfo[k].MV, (Ipp8u*)PMV, sizeof(PMV));
          MB_MV(Mb,0,0)[0] = (mfxI16)PMV[0][0].x;
          MB_MV(Mb,0,0)[1] = (mfxI16)PMV[0][0].y;
          MB_MV(Mb,0,1)[0] = (mfxI16)PMV[1][0].x;
          MB_MV(Mb,0,1)[1] = (mfxI16)PMV[1][0].y;
          MB_MV(Mb,1,0)[0] = (mfxI16)PMV[0][1].x;
          MB_MV(Mb,1,0)[1] = (mfxI16)PMV[0][1].y;
          MB_MV(Mb,1,1)[0] = (mfxI16)PMV[1][1].x;
          MB_MV(Mb,1,1)[1] = (mfxI16)PMV[1][1].y;
          pMBInfo[k].skipped = 1;
          Mb->IntraMbFlag = 0;
          k++;
          continue;
        }
      } // try skip

      pMBInfo[k].mb_type = 0;
      bestpred->var_sum = (1 << 30);

      //VARMEAN_FRAME_Y(currpred->var, mean_frm, currpred->var_sum);
      currpred->var_sum = SCALE_VAR_INTRA(currpred->var_sum);
      //var_intra_frame = currpred->var_sum;
      if(currpred->var_sum < bestpred->var_sum)
      {
        currpred->mb_type = MB_INTRA;
        currpred->dct_type = DCT_FRAME;
        SWAP_PTR(bestpred, currpred);
      }

      BOUNDS_H(dir0,i)
      ME_FRAME(dir0, currpred->var_sum, currpred->pDiff, currpred->dct_type);
      currpred->var_sum = SCALE_VAR(currpred->var_sum, SC_VAR_1V);

      if(currpred->var_sum < bestpred->var_sum)
      {
        currpred->mb_type = dir0 == 0 ? MB_FORWARD : MB_BACKWARD;
        currpred->pred_type = MC_FRAME;
        SWAP_PTR(bestpred, currpred);
      }

      IF_GOOD_PRED(bestpred->var, bestpred->mean) {
        goodpred = 1;
        goto encodeMB;
      }

      // Field type
      if (!state->curr_frame_dct) {
        VARMEAN_FIELD_Y(currpred->var, mean_fld, currpred->var_sum, YFrameHSize);
        currpred->var_sum = SCALE_VAR_INTRA(currpred->var_sum);
        if(currpred->var_sum < bestpred->var_sum)
        {
          currpred->mb_type = MB_INTRA;
          currpred->dct_type = DCT_FIELD;
          SWAP_PTR(bestpred, currpred);
        }
      }

      if (!state->curr_frame_pred && (picture_coding_type != MPEG2_P_PICTURE || (vector[2][0].x | vector[2][0].y)) ) {
        ME_FIELD(dir0, currpred->var_sum, currpred->pDiff, currpred->dct_type);
        currpred->var_sum = SCALE_VAR(currpred->var_sum, SC_VAR_2V);
        if(currpred->var_sum < bestpred->var_sum)
        {
          currpred->mb_type = dir0 == 0 ? MB_FORWARD : MB_BACKWARD;
          currpred->pred_type = MC_FIELD;
          SWAP_PTR(bestpred, currpred);
        }
      }

      // Backward vector search
      if(picture_coding_type == MPEG2_B_PICTURE) {
        BOUNDS_H(dir1,i)
        ME_FRAME(dir1, currpred->var_sum, currpred->pDiff, currpred->dct_type);
        currpred->var_sum = SCALE_VAR(currpred->var_sum, SC_VAR_1V);
        if(currpred->var_sum < bestpred->var_sum)
        {
          currpred->mb_type = dir1 == 0 ? MB_FORWARD : MB_BACKWARD;
          currpred->pred_type = MC_FRAME;
          SWAP_PTR(bestpred, currpred);

          IF_GOOD_PRED(bestpred->var, bestpred->mean) {
            goodpred = 1;
            goto encodeMB;
          }
        }

        if (!state->curr_frame_pred ) {
          ME_FIELD(dir1, currpred->var_sum, currpred->pDiff, currpred->dct_type);
          currpred->var_sum = SCALE_VAR(currpred->var_sum, SC_VAR_2V);
          if(currpred->var_sum < bestpred->var_sum)
          {
            currpred->mb_type = dir1 == 0 ? MB_FORWARD : MB_BACKWARD;
            currpred->pred_type = MC_FIELD;
            SWAP_PTR(bestpred, currpred);
          }
        }

        // Bi-directional
        for (pred_ind = 0; pred_ind <= (state->curr_frame_pred ? 0 : 1); pred_ind++) {
          Ipp32s pred_type;
          Ipp32s scale_var;

          if (!pred_ind) {
            pred_type = MC_FRAME;
            GETDIFF_FRAME_FB(Y, Y, l, currpred->pDiff);
            scale_var = SC_VAR_1VBI;
          } else {
            pred_type = MC_FIELD;
            GETDIFF_FIELD_FB(Y, Y, l, currpred->pDiff);
            scale_var = SC_VAR_2VBI;
          }

          if (!state->curr_frame_dct) {
            Ipp32s var_fld = 0, var = 0;
            //for(Ipp32s l=0;l<64*4;l++) currpred->pDiff[l] = -1;
            ippiFrameFieldSAD16x16_16s32s_C1R(currpred->pDiff, BlkStride_l*2, &var, &var_fld);
            if (var_fld < var) {
              currpred->dct_type = DCT_FIELD;
              VARMEAN_FIELD(currpred->pDiff, currpred->var, currpred->mean, currpred->var_sum);
            } else {
              currpred->dct_type = DCT_FRAME;
              VARMEAN_FRAME(currpred->pDiff, currpred->var, currpred->mean, currpred->var_sum);
            }
          } else {
            currpred->dct_type = DCT_FRAME;
            VARMEAN_FRAME(currpred->pDiff, currpred->var, currpred->mean, currpred->var_sum);
          }

          currpred->var_sum = SCALE_VAR(currpred->var_sum, scale_var);
          if(currpred->var_sum < bestpred->var_sum)
          {
            currpred->mb_type = MB_FORWARD | MB_BACKWARD;
            currpred->pred_type = pred_type;
            SWAP_PTR(bestpred, currpred);
          }

        }
      } // end of bw and bidir

encodeMB:
      pMBInfo[k].prediction_type = bestpred->pred_type;
      pMBInfo[k].dct_type = bestpred->dct_type;
      pMBInfo[k].mb_type = bestpred->mb_type;

      Mb->TransformFlag = pMBInfo[k].dct_type == DCT_FIELD;
      Mb->MbType5Bits = (pMBInfo[k].prediction_type == MC_FRAME)?MFX_MBTYPE_INTER_16X16_0:MFX_MBTYPE_INTER_16X8_00;

      pDiff = bestpred->pDiff;
      if (bestpred->mb_type & MB_INTRA)
      { // intra
        ippsZero_8u((Ipp8u*)PMV, sizeof(PMV));
        ippsZero_8u((Ipp8u*)pMBInfo[k].MV, sizeof(pMBInfo[k].MV));

        //PutAddrIncrement(macroblock_address_increment, numTh);
        macroblock_address_increment = 1;

        //if(slice_macroblock_address - slice_past_intra_address > 1) {
        //  dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = ResetTbl[m_cuc.FrameParam->IntraDCprecision];
        //}


        slice_past_intra_address = (i >> 4);

        Mb->IntraMbFlag = 1;
        k++;
        continue;
      } //Intra macroblock

      //non-intra
      pDiff = bestpred->pDiff;
      //Ipp32s try_skip = 0;

      // Non-intra macroblock
      if(pMBInfo[k].prediction_type == MC_FRAME)
      {
        //try_skip = (i != 0) && (i != MBcountH*16 - 16) &&
        //  (((pMBInfo[k].mb_type ^ pMBInfo[k-1].mb_type) & (MB_FORWARD | MB_BACKWARD)) == 0 &&
        //  ( (pMBInfo[k].mb_type&MB_FORWARD) == 0 ||
        //  vector[2][0].x == PMV[0][0].x &&
        //  vector[2][0].y == PMV[0][0].y &&
        //  pMBInfo[k].mv_field_sel[2][0] == m_cuc->FrameParam->MPEG2.BottomFieldFlag ) &&
        //  ( (pMBInfo[k].mb_type&MB_BACKWARD) == 0 ||
        //  vector[2][1].x == PMV[0][1].x &&
        //  vector[2][1].y == PMV[0][1].y &&
        //  pMBInfo[k].mv_field_sel[2][1] == m_cuc->FrameParam->MPEG2.BottomFieldFlag));
        if((pMBInfo[k].mb_type&MB_FORWARD)&&
          (pMBInfo[k].mb_type&MB_BACKWARD))
        {
          Mb->MbType = MFX_MBTYPE_INTER_16X16_2; // clears flags
          Mb->TransformFlag = pMBInfo[k].dct_type == DCT_FIELD;
          Mb->FieldMbFlag = m_cuc->FrameParam->MPEG2.FieldPicFlag;
          Mb->NumPackedMv = 2;
          MB_MV(Mb,0,0)[0] = (mfxI16)(PMV[0][0].x = PMV[1][0].x = vector[2][0].x);
          MB_MV(Mb,0,0)[1] = (mfxI16)(PMV[0][0].y = PMV[1][0].y = vector[2][0].y);
          MB_FSEL(Mb,0,0) = (mfxU8)pMBInfo[k].mv_field_sel[2][0];
          MB_MV(Mb,1,0)[0] = (mfxI16)(PMV[0][1].x = PMV[1][1].x = vector[2][1].x);
          MB_MV(Mb,1,0)[1] = (mfxI16)(PMV[0][1].y = PMV[1][1].y = vector[2][1].y);
          MB_FSEL(Mb,1,0) = (mfxU8)pMBInfo[k].mv_field_sel[2][1];
        }
        else if(pMBInfo[k].mb_type&MB_FORWARD)
        {
          Mb->MbType = MFX_MBTYPE_INTER_16X16_0; // clears flags
          Mb->TransformFlag = pMBInfo[k].dct_type == DCT_FIELD;
          Mb->FieldMbFlag = m_cuc->FrameParam->MPEG2.FieldPicFlag;
          Mb->NumPackedMv = 1;
          MB_MV(Mb,0,0)[0] = (mfxI16)(PMV[0][0].x = PMV[1][0].x = vector[2][0].x);
          MB_MV(Mb,0,0)[1] = (mfxI16)(PMV[0][0].y = PMV[1][0].y = vector[2][0].y);
          MB_FSEL(Mb,0,0) = (mfxU8)pMBInfo[k].mv_field_sel[2][0];
        }
        else //pMBInfo.mb_type&MB_BACKWARD
        {
          Mb->MbType = MFX_MBTYPE_INTER_16X16_1; // clears flags
          Mb->TransformFlag = pMBInfo[k].dct_type == DCT_FIELD;
          Mb->FieldMbFlag = m_cuc->FrameParam->MPEG2.FieldPicFlag;
          Mb->NumPackedMv = 1;
          MB_MV(Mb,1,0)[0] = (mfxI16)(PMV[0][1].x = PMV[1][1].x = vector[2][1].x);
          MB_MV(Mb,1,0)[1] = (mfxI16)(PMV[0][1].y = PMV[1][1].y = vector[2][1].y);
          MB_FSEL(Mb,1,0) = (mfxU8)pMBInfo[k].mv_field_sel[2][1];
        }
      }
      else //pMBInfo[k].prediction_type == MC_FIELD
      {
        if((pMBInfo[k].mb_type&MB_FORWARD)&&
          (pMBInfo[k].mb_type&MB_BACKWARD))
        {
          Mb->MbType = MFX_MBTYPE_INTER_16X8_22;
          Mb->TransformFlag = pMBInfo[k].dct_type == DCT_FIELD;
          Mb->FieldMbFlag = 1;
          Mb->NumPackedMv = 4;
          Ipp32s mv_shift = (m_cuc->FrameParam->MPEG2.FieldPicFlag) ? 0 : 1;
          MB_MV(Mb,0,0)[0] = (mfxI16)(PMV[0][0].x = vector[0][0].x);
          MB_MV(Mb,0,0)[1] = (mfxI16)(PMV[0][0].y = vector[0][0].y << mv_shift);
          MB_FSEL(Mb,0,0) = (mfxU8)pMBInfo[k].mv_field_sel[0][0];
          MB_MV(Mb,0,1)[0] = (mfxI16)(PMV[1][0].x = vector[1][0].x);
          MB_MV(Mb,0,1)[1] = (mfxI16)(PMV[1][0].y = vector[1][0].y << mv_shift);
          MB_FSEL(Mb,0,1) = (mfxU8)pMBInfo[k].mv_field_sel[1][0];
          MB_MV(Mb,1,0)[0] = (mfxI16)(PMV[0][1].x = vector[0][1].x);
          MB_MV(Mb,1,0)[1] = (mfxI16)(PMV[0][1].y = vector[0][1].y << mv_shift);
          MB_FSEL(Mb,1,0) = (mfxU8)pMBInfo[k].mv_field_sel[0][1];
          MB_MV(Mb,1,1)[0] = (mfxI16)(PMV[1][1].x = vector[1][1].x);
          MB_MV(Mb,1,1)[1] = (mfxI16)(PMV[1][1].y = vector[1][1].y << mv_shift);
          MB_FSEL(Mb,1,1) = (mfxU8)pMBInfo[k].mv_field_sel[1][1];
        } else
          if(pMBInfo[k].mb_type&MB_FORWARD)
          {
            Mb->MbType = MFX_MBTYPE_INTER_16X8_00;
            Mb->TransformFlag = pMBInfo[k].dct_type == DCT_FIELD;
            Mb->FieldMbFlag = 1;
            Mb->NumPackedMv = 2;
            Ipp32s mv_shift = (m_cuc->FrameParam->MPEG2.FieldPicFlag) ? 0 : 1;
            MB_MV(Mb,0,0)[0] = (mfxI16)(PMV[0][0].x = vector[0][0].x);
            MB_MV(Mb,0,0)[1] = (mfxI16)(PMV[0][0].y = vector[0][0].y << mv_shift);
            MB_FSEL(Mb,0,0) = (mfxU8)pMBInfo[k].mv_field_sel[0][0];
            MB_MV(Mb,0,1)[0] = (mfxI16)(PMV[1][0].x = vector[1][0].x);
            MB_MV(Mb,0,1)[1] = (mfxI16)(PMV[1][0].y = vector[1][0].y << mv_shift);
            MB_FSEL(Mb,0,1) = (mfxU8)pMBInfo[k].mv_field_sel[1][0];
          }
          else //pMBInfo.mb_type&MB_BACKWARD
          {
            Mb->MbType = MFX_MBTYPE_INTER_16X8_11;
            Mb->TransformFlag = pMBInfo[k].dct_type == DCT_FIELD;
            Mb->FieldMbFlag = 1;
            Mb->NumPackedMv = 2;
            Ipp32s mv_shift = (m_cuc->FrameParam->MPEG2.FieldPicFlag) ? 0 : 1;
            MB_MV(Mb,1,0)[0] = (mfxI16)(PMV[0][1].x = vector[0][1].x);
            MB_MV(Mb,1,0)[1] = (mfxI16)(PMV[0][1].y = vector[0][1].y << mv_shift);
            MB_FSEL(Mb,1,0) = (mfxU8)pMBInfo[k].mv_field_sel[0][1];
            MB_MV(Mb,1,1)[0] = (mfxI16)(PMV[1][1].x = vector[1][1].x);
            MB_MV(Mb,1,1)[1] = (mfxI16)(PMV[1][1].y = vector[1][1].y << mv_shift);
            MB_FSEL(Mb,1,1) = (mfxU8)pMBInfo[k].mv_field_sel[1][1];
          }
      } //FIELD_PREDICTION

      macroblock_address_increment = 1;

      //pMBInfo[k].cbp = CodedBlockPattern;
      MFX_INTERNAL_CPY((Ipp8u*)pMBInfo[k].MV, (Ipp8u*)PMV, sizeof(PMV));
      //if (!m_frame.FrameMbsOnlyFlag)
      //  threadSpec[numTh].fieldCount += (pMBInfo[k].dct_type == DCT_FIELD)? 1 : -1;

      Mb->IntraMbFlag = 0;
      k++;
    } // for(mb in slice)
  }

  if(picture_coding_type == MPEG2_P_PICTURE) {
    Ipp32s mul = state->ipflag ? 0x200 : 0x100/state->P_distance; // double for IP case
    for(k=mbstart; k < mbstart+mbcount; k++) {
      pMBInfo[k].MV_P[0].x = pMBInfo[k].MV[0][0].x*mul;
      pMBInfo[k].MV_P[0].y = pMBInfo[k].MV[0][0].y*mul;
      pMBInfo[k].MV_P[1].x = pMBInfo[k].MV[1][0].x*mul;
      pMBInfo[k].MV_P[1].y = pMBInfo[k].MV[1][0].y*mul;
    }
  }
  return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCMPEG2::sliceCoef( MPEG2FrameState* state, mfxU32 mbstart, mfxU32 mbcount, mfxExtMpeg2Coeffs* coeff)
{
  mfxI16      Diff[64*12];
  mfxI16      Block[64];
  mfxI16      *pDiff, *pBlock;
  Ipp32s      i, j, ic, jc, k;
  Ipp32s      Count[12], CodedBlockPattern;
  IppMotionVector2   vector[3][2] = {0,}; // top/bottom/frame F/B
  IppiSize    roi8x8 = {8,8};
  Ipp32s      cur_offset, cur_offset_c;
  const Ipp8u *YBlock;
  const Ipp8u *UBlock;
  const Ipp8u *VBlock;
  mfxU32      mbnum;
  mfxI32      quant_scale_value;
  MBInfo      mbinfo[2];
  mfxI32      left=0, curr=0;
  mfxI32      YFrameHSize = state->YFrameHSize;
  mfxI32      UVFrameHSize = state->UVFrameHSize;

  const mfxI32 *scan = m_cuc->FrameParam->MPEG2.AlternateScan ? mfx_mpeg2_AlternateScan : mfx_mpeg2_ZigZagScan;
  DctCoef *pcoeff = (DctCoef *)coeff->Coeffs;
  pcoeff += mbstart * 64*m_block_count;
  mfxMbCodeMPEG2 *Mb = &m_cuc->MbParam->Mb[mbstart].MPEG2;
  //slice->SliceHeaderSize = 0xffff; // 32+6 or +9 bits
  pDiff = Diff;
  pBlock = Block;

  j = Mb->MbYcnt*16; jc = Mb->MbYcnt*BlkHeight_c;
  for(mbnum=mbstart; mbnum<mbstart+mbcount; mbnum++, left=curr, curr^=1) {
    Mb = &m_cuc->MbParam->Mb[mbnum].MPEG2;
    i = Mb->MbXcnt*16; ic = Mb->MbXcnt*BlkWidth_c;
    k = Mb->MbXcnt + Mb->MbYcnt * (m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1);

    cur_offset = i + j * YFrameHSize;
    cur_offset_c = ic + jc * UVFrameHSize;
    if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
      cur_offset_c *= 2;
    }

    YBlock = state->Y_src + cur_offset;
    UBlock = state->U_src + cur_offset_c;
    VBlock = state->V_src + cur_offset_c;
    Ipp8u *YBlockRec = state->Y_src + cur_offset;
    Ipp8u *UBlockRec = state->U_src + cur_offset_c;
    Ipp8u *VBlockRec = state->V_src + cur_offset_c;

    if(MB_SKIPPED(Mb)) {
      memset(pcoeff, 0, sizeof(DctCoef)*64*m_block_count);
      pcoeff += 64*m_block_count;
      Mb->CodedPattern4x4Y = 0;
      Mb->CodedPattern4x4U = 0;
      Mb->CodedPattern4x4V = 0;
      if(picture_coding_type == MPEG2_B_PICTURE)
        mbinfo[curr].mb_type = mbinfo[left].mb_type;
      else // if(picture_coding_type == MPEG2_P_PICTURE)
        mbinfo[curr].mb_type = MB_FORWARD;
      mbinfo[curr].skipped = 1;

      if(picture_coding_type != MPEG2_B_PICTURE) {
        IppiSize    roi;
        roi.width = BlkWidth_l;
        roi.height = BlkHeight_l;
        ippiCopy_8u_C1R(state->YRecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset, YFrameHSize, YBlockRec, YFrameHSize, roi);
        if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
          roi.width = 16;
          roi.height = 8;
          ippiCopy_8u_C1R(state->URecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset_c, UVFrameHSize*2, UBlockRec, UVFrameHSize*2, roi);
        } else {
          roi.width = BlkWidth_c;
          roi.height = BlkHeight_c;
          ippiCopy_8u_C1R(state->URecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset_c, UVFrameHSize, UBlockRec, UVFrameHSize, roi);
          ippiCopy_8u_C1R(state->VRecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset_c, UVFrameHSize, VBlockRec, UVFrameHSize, roi);
        }
      }
      continue;
    }

    if(MFX_ERR_NONE != TranslateMfxMbTypeMPEG2(Mb, &mbinfo[curr]))
      return MFX_ERR_UNSUPPORTED;

    // next 2 variables used to emulate DualPrime like Bidirectional MB
    mfxI32 predtype = mbinfo[curr].prediction_type;
    mfxI32 mbtype = mbinfo[curr].mb_type;

    if (!(mbinfo[curr].mb_type & MB_INTRA)) {
      if (Mb->MbType5Bits == MFX_MBTYPE_INTER_DUAL_PRIME) {
        mfxI32 rnd;
        mfxI16 dvec[2][2]; // [num,xy]
        mbtype = MB_FORWARD|MB_BACKWARD; // BW is used for DMV. Ref frames are set in preEnc()
        if (m_cuc->FrameParam->MPEG2.FieldPicFlag) {
          predtype = MC_FRAME; // means 16x16 prediction
          mbinfo[curr].mv_field_sel[2][0] = m_cuc->FrameParam->MPEG2.BottomFieldFlag;
          mbinfo[curr].mv_field_sel[2][1] = (m_cuc->FrameParam->MPEG2.BottomFieldFlag ^ 1);
          rnd = MB_MV(Mb,0,0)[0] >= 0;
          dvec[0][0] = (mfxI16)(((MB_MV(Mb,0,0)[0] + rnd)>>1) + MB_MV(Mb,1,0)[0]);
          rnd = MB_MV(Mb,0,0)[1] >= 0;
          dvec[0][1] = (mfxI16)(((MB_MV(Mb,0,0)[1] + rnd)>>1) + MB_MV(Mb,1,0)[1]);
          dvec[0][1] += m_cuc->FrameParam->MPEG2.BottomFieldFlag ? 1 : -1;
          SET_MOTION_VECTOR((&vector[2][0]), MB_MV(Mb,0,0)[0], MB_MV(Mb,0,0)[1]);
          SET_MOTION_VECTOR((&vector[2][1]), dvec[0][0], dvec[0][1]);
        } else {
          predtype = MC_FIELD; // means 16x8 prediction
          mfxI32 mul0 = m_cuc->FrameParam->MPEG2.TopFieldFirst ? 1 : 3;
          mfxI32 mul1 = 4 - mul0; // 3 or 1
          mbinfo[curr].mv_field_sel[0][0] = 0;
          mbinfo[curr].mv_field_sel[1][0] = 1;
          mbinfo[curr].mv_field_sel[0][1] = 1;
          mbinfo[curr].mv_field_sel[1][1] = 0;
          rnd = MB_MV(Mb,0,0)[0] >= 0;
          dvec[0][0] = (mfxI16)(((mul0 * MB_MV(Mb,0,0)[0] + rnd)>>1) + MB_MV(Mb,1,0)[0]);
          dvec[1][0] = (mfxI16)(((mul1 * MB_MV(Mb,0,0)[0] + rnd)>>1) + MB_MV(Mb,1,0)[0]);
          rnd = MB_MV(Mb,0,0)[1] >= 0;
          dvec[0][1] = (mfxI16)((((mul0 * (MB_MV(Mb,0,0)[1]>>1) + rnd)>>1) + MB_MV(Mb,1,0)[1] - 1)<<1);
          dvec[1][1] = (mfxI16)((((mul1 * (MB_MV(Mb,0,0)[1]>>1) + rnd)>>1) + MB_MV(Mb,1,0)[1] + 1)<<1);
          SET_FIELD_VECTOR((&vector[0][0]), MB_MV(Mb,0,0)[0], MB_MV(Mb,0,0)[1] >> 1);
          SET_FIELD_VECTOR((&vector[1][0]), MB_MV(Mb,0,0)[0], MB_MV(Mb,0,0)[1] >> 1);
          SET_FIELD_VECTOR((&vector[0][1]), dvec[0][0], dvec[0][1] >> 1);
          SET_FIELD_VECTOR((&vector[1][1]), dvec[1][0], dvec[1][1] >> 1);
        }
      }
      else if (predtype == MC_FRAME) {
        mbinfo[curr].mv_field_sel[2][0] = MB_FSEL(Mb,0,0);
        mbinfo[curr].mv_field_sel[2][1] = MB_FSEL(Mb,1,0);
        SET_MOTION_VECTOR((&vector[2][0]), MB_MV(Mb,0,0)[0], MB_MV(Mb,0,0)[1]);
        SET_MOTION_VECTOR((&vector[2][1]), MB_MV(Mb,1,0)[0], MB_MV(Mb,1,0)[1]);
      } else { // (predtype == MC_FIELD)
        mbinfo[curr].mv_field_sel[0][0] = MB_FSEL(Mb,0,0);
        mbinfo[curr].mv_field_sel[1][0] = MB_FSEL(Mb,0,1);
        mbinfo[curr].mv_field_sel[0][1] = MB_FSEL(Mb,1,0);
        mbinfo[curr].mv_field_sel[1][1] = MB_FSEL(Mb,1,1);
        if (m_cuc->FrameParam->MPEG2.FieldPicFlag) {
          SET_MOTION_VECTOR((&vector[0][0]), MB_MV(Mb,0,0)[0], MB_MV(Mb,0,0)[1]);
          SET_MOTION_VECTOR((&vector[1][0]), MB_MV(Mb,0,1)[0], MB_MV(Mb,0,1)[1]);
          SET_MOTION_VECTOR((&vector[0][1]), MB_MV(Mb,1,0)[0], MB_MV(Mb,1,0)[1]);
          SET_MOTION_VECTOR((&vector[1][1]), MB_MV(Mb,1,1)[0], MB_MV(Mb,1,1)[1]);
        } else { // y offset is multiplied by 2
          SET_FIELD_VECTOR((&vector[0][0]), MB_MV(Mb,0,0)[0], MB_MV(Mb,0,0)[1] >> 1);
          SET_FIELD_VECTOR((&vector[1][0]), MB_MV(Mb,0,1)[0], MB_MV(Mb,0,1)[1] >> 1);
          SET_FIELD_VECTOR((&vector[0][1]), MB_MV(Mb,1,0)[0], MB_MV(Mb,1,0)[1] >> 1);
          SET_FIELD_VECTOR((&vector[1][1]), MB_MV(Mb,1,1)[0], MB_MV(Mb,1,1)[1] >> 1);
        }
      }

      if (predtype == MC_FRAME) {
        if (mbtype == (MB_FORWARD|MB_BACKWARD)) {
          GETDIFF_FRAME_FB(Y, Y, l, pDiff);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
            GETDIFF_FRAME_FB(U, Y, c, pDiff);
            GETDIFF_FRAME_FB(V, Y, c, pDiff);
          } else {
            GETDIFF_FRAME_FB(U, UV, c, pDiff);
            GETDIFF_FRAME_FB(V, UV, c, pDiff);
          }
        }
        else if (mbtype & MB_FORWARD) {
          GETDIFF_FRAME(Y, Y, l, pDiff, 0);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
            GETDIFF_FRAME(U, Y, c, pDiff, 0);
            GETDIFF_FRAME(V, Y, c, pDiff, 0);
          } else {
            GETDIFF_FRAME(U, UV, c, pDiff, 0);
            GETDIFF_FRAME(V, UV, c, pDiff, 0);
          }
        }
        else { // if(mbtype & MB_BACKWARD)
          GETDIFF_FRAME(Y, Y, l, pDiff, 1);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
            GETDIFF_FRAME(U, Y, c, pDiff, 1);
            GETDIFF_FRAME(V, Y, c, pDiff, 1);
          } else {
            GETDIFF_FRAME(U, UV, c, pDiff, 1);
            GETDIFF_FRAME(V, UV, c, pDiff, 1);
          }
        }
      } else {
        if (mbtype == (MB_FORWARD|MB_BACKWARD)) {
          GETDIFF_FIELD_FB(Y, Y, l, pDiff);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
            GETDIFF_FIELD_FB(U, Y, c, pDiff);
            GETDIFF_FIELD_FB(V, Y, c, pDiff);
          } else {
            GETDIFF_FIELD_FB(U, UV, c, pDiff);
            GETDIFF_FIELD_FB(V, UV, c, pDiff);
          }
        }
        else if (mbtype & MB_FORWARD) {
          GETDIFF_FIELD(Y, Y, l, pDiff, 0);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
            GETDIFF_FIELD(U, Y, c, pDiff, 0);
            GETDIFF_FIELD(V, Y, c, pDiff, 0);
          } else {
            GETDIFF_FIELD(U, UV, c, pDiff, 0);
            GETDIFF_FIELD(V, UV, c, pDiff, 0);
          }
        }
        else { // if(mbtype & MB_BACKWARD)
          GETDIFF_FIELD(Y, Y, l, pDiff, 1);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
            GETDIFF_FIELD(U, Y, c, pDiff, 1);
            GETDIFF_FIELD(V, Y, c, pDiff, 1);
          } else {
            GETDIFF_FIELD(U, UV, c, pDiff, 1);
            GETDIFF_FIELD(V, UV, c, pDiff, 1);
          }
        }
      }
    }

    //if (!m_frame.FrameMbsOnlyFlag) {
    //  Ipp32s var_fld = 0, var = 0;
    //  ippiFrameFieldSAD16x16_16s32s_C1R(curr->pDiff, BlkStride_l*2, &var, &var_fld);
    //  if (var_fld < var) {
    //    mbinfo[curr].dct_type = DCT_FIELD;
    //  } else {
    //    mbinfo[curr].dct_type = DCT_FRAME;
    //  }
    //} else {
    //  mbinfo[curr].dct_type = DCT_FRAME;
    //}

    Mb->CodedPattern4x4Y = 0;
    Mb->CodedPattern4x4U = 0;
    Mb->CodedPattern4x4V = 0;
    Mb->QpScaleType = m_cuc->FrameParam->MPEG2.QuantScaleType;
    quant_scale_value = mfx_mpeg2_Val_QScale[m_cuc->FrameParam->MPEG2.QuantScaleType][Mb->QpScaleCode];

    if (!(mbinfo[curr].mb_type & MB_INTRA)) {
      Ipp32s var[4];
      Ipp32s mean[4];
      const Ipp32s *dct_step, *diff_off;
      Ipp32s loop, n;
      Ipp32s      varThreshold;          // threshold for block variance and mean
      Ipp32s      meanThreshold;         // used in MB pattern computations

      //Ipp32s qq = quantiser_scale_value*quantiser_scale_value;
      varThreshold = quant_scale_value;// + qq/32;
      meanThreshold = quant_scale_value * 8;// + qq/32;

      if (mbinfo[curr].dct_type == DCT_FRAME) {
        dct_step = frm_dct_step;
        diff_off = frm_diff_off;
      } else {
        dct_step = fld_dct_step;
        diff_off = fld_diff_off;
      }
      CodedBlockPattern = 0;
      for (loop = 0; loop < m_block_count; loop++, pcoeff += 64) {
        CodedBlockPattern = (CodedBlockPattern << 1);
        if (loop < 4 )
          ippiVarSum8x8_16s32s_C1R(pDiff + diff_off[loop], dct_step[loop], &var[loop], &mean[loop]);
        if (loop < 4 && var[loop] < varThreshold && abs(mean[loop]) < meanThreshold) {
          Count[loop] = 0;
        } else
        {
          ippiDCT8x8Fwd_16s_C1R(pDiff + diff_off[loop], dct_step[loop], pBlock);

          // no 422 QM support!!!
          ippiQuant_MPEG2_16s_C1I(pBlock, quant_scale_value, InvInterQM, &Count[loop]);
          if (Count[loop] != 0) CodedBlockPattern++;
        }
        for(n=0; n<64; n++) {
          pcoeff[n] = 0;
        }
        if (!Count[loop]) {
          ippiSet_16s_C1R(0, pDiff + diff_off[loop], dct_step[loop], roi8x8);
          continue;
        }

        mfxI32 tofind;
        for(n=0, tofind = Count[loop]; n<64 && tofind>0; n++) {
          Ipp16s val = pBlock[scan[n]];
          if(val) {
            tofind--;
            pcoeff[n] = val;
          }
        }
        if(picture_coding_type != MPEG2_B_PICTURE) {
          ippiQuantInv_MPEG2_16s_C1I(pBlock, quant_scale_value, InterQM);
          ippiDCT8x8Inv_16s_C1R(pBlock, pDiff + diff_off[loop], dct_step[loop]);
        }
        if(loop<4) // Y
          Mb->CodedPattern4x4Y |= 0xf000>>loop*4;
        else if (!(loop&1)) // U
          Mb->CodedPattern4x4U |= 0xf<<((loop-4)>>1)*4;
        else // V
          Mb->CodedPattern4x4V |= 0xf<<((loop-4)>>1)*4;
      }
      if (!CodedBlockPattern && mbinfo[curr].prediction_type == MC_FRAME &&
        mbnum > mbstart && mbnum < (mbstart + mbcount - 1) &&
        Mb->MbType5Bits != MFX_MBTYPE_INTER_DUAL_PRIME ) {
          if ((picture_coding_type == MPEG2_P_PICTURE /*&& !state->ipflag*/
            && !(MB_MV(Mb,0,0)[0] | MB_MV(Mb,0,0)[1])
            && MB_FSEL(Mb,0,0) == m_cuc->FrameParam->MPEG2.BottomFieldFlag)
          ||
            (picture_coding_type == MPEG2_B_PICTURE
            && ((mbinfo[curr].mb_type ^ mbinfo[left].mb_type) & (MB_FORWARD | MB_BACKWARD)) == 0
            && ((mbinfo[curr].mb_type&MB_FORWARD) == 0 ||
                ((MB_MV(Mb,0,0)[0] == MB_MV(Mb-1,0,0)[0]) &&
                (MB_MV(Mb,0,0)[1] == MB_MV(Mb-1,0,0)[1]) &&
                (MB_FSEL(Mb,0,0) == m_cuc->FrameParam->MPEG2.BottomFieldFlag)) )
            && ((mbinfo[curr].mb_type&MB_BACKWARD) == 0 ||
                ((MB_MV(Mb,1,0)[0] == MB_MV(Mb-1,1,0)[0]) &&
                (MB_MV(Mb,1,0)[1] == MB_MV(Mb-1,1,0)[1]) &&
                (MB_FSEL(Mb,1,0) == m_cuc->FrameParam->MPEG2.BottomFieldFlag)))) ){
              Mb->Skip8x8Flag = 0xf;
          if(picture_coding_type == MPEG2_B_PICTURE)
            mbinfo[curr].mb_type = mbinfo[left].mb_type;
          else // if(picture_coding_type == MPEG2_P_PICTURE)
            mbinfo[curr].mb_type = MB_FORWARD;
          mbinfo[curr].skipped = 1;
        }
      }
      if(picture_coding_type != MPEG2_B_PICTURE) {

        if(MB_SKIPPED(Mb)) {
          mbinfo[curr].mb_type = MB_FORWARD;
          mbinfo[curr].skipped = 1;
          IppiSize    roi;
          roi.width = BlkWidth_l;
          roi.height = BlkHeight_l;
          ippiCopy_8u_C1R(state->YRecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset, YFrameHSize, YBlockRec, YFrameHSize, roi);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
            roi.width = 16;
            roi.height = 8;
            ippiCopy_8u_C1R(state->URecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset_c, UVFrameHSize*2, UBlockRec, UVFrameHSize*2, roi);
          } else {
            roi.width = BlkWidth_c;
            roi.height = BlkHeight_c;
            ippiCopy_8u_C1R(state->URecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset_c, UVFrameHSize, UBlockRec, UVFrameHSize, roi);
            ippiCopy_8u_C1R(state->VRecFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + cur_offset_c, UVFrameHSize, VBlockRec, UVFrameHSize, roi);
          }
        }
        else if(predtype == MC_FRAME)
        {
          MC_FRAME_F(Y, Y, l, pDiff);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
            MC_FRAME_F(U, Y, c, pDiff);
            MC_FRAME_F(V, Y, c, pDiff);
          } else {
            MC_FRAME_F(U, UV, c, pDiff);
            MC_FRAME_F(V, UV, c, pDiff);
          }
        } else {
          MC_FIELD_F(Y, Y, l, pDiff);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
            MC_FIELD_F(U, Y, c, pDiff);
            MC_FIELD_F(V, Y, c, pDiff);
          } else {
            MC_FIELD_F(U, UV, c, pDiff);
            MC_FIELD_F(V, UV, c, pDiff);
          }
        }
      }

    } else { // intra
      Ipp32s stride[3];
      Ipp32s *block_offset;
      Ipp32s *block_offset_ref;
      Ipp32s intra_dc_shift = 3 - m_cuc->FrameParam->MPEG2.IntraDCprecision;
      Ipp32s half_intra_dc = (1 << intra_dc_shift) >> 1;
      const Ipp8u *BlockSrc[3] = {YBlock, UBlock, VBlock};
      Ipp8u *BlockRec[3] = {YBlockRec, UBlockRec, VBlockRec};

      if (mbinfo[curr].dct_type == DCT_FRAME) {
        block_offset = block_offset_frm;
        block_offset_ref = block_offset_frm/*_ref*/;
        stride[0] = YFrameHSize;
        stride[1] = UVFrameHSize;
        stride[2] = UVFrameHSize;
      } else {
        block_offset = block_offset_fld;
        block_offset_ref = block_offset_fld/*_ref*/;
        stride[0] = 2*YFrameHSize;
        stride[1] = UVFrameHSize << chroma_fld_flag;
        stride[2] = UVFrameHSize << chroma_fld_flag;
      }
      if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
        stride[1] = YFrameHSize;
        stride[2] = YFrameHSize;
      }

      Mb->CodedPattern4x4Y = 0xffff;
      Mb->CodedPattern4x4U = 0xffff >> (24-2*m_block_count);
      Mb->CodedPattern4x4V = 0xffff >> (24-2*m_block_count);

      for (Ipp32s blk = 0; blk < m_block_count; blk++, pcoeff += 64) {
        mfxI32 cc = mfx_mpeg2_color_index[blk];
        if (blk < 4 || m_info.FrameInfo.FourCC != MFX_FOURCC_NV12) {
          ippiDCT8x8Fwd_8u16s_C1R(BlockSrc[cc] + block_offset[blk], stride[cc], pDiff);
        } else {
          tmp_ippiDCT8x8FwdUV_8u16s_C1R(BlockSrc[cc], stride[cc], pDiff);
        }
        if (pDiff[0] < 0) {
          pDiff[0] = (Ipp16s)(-((-pDiff[0] + half_intra_dc) >> intra_dc_shift));
        } else {
          pDiff[0] = (Ipp16s)((pDiff[0] + half_intra_dc) >> intra_dc_shift);
        }
        // no 422 QM support
        ippiQuantIntra_MPEG2_16s_C1I(pDiff, quant_scale_value, InvIntraQM, &Count[blk]);

        mfxI32 i, tofind;
        for(i=0; i<64; i++) pcoeff[i] = 0;
        pcoeff[0] = pDiff[0];
        for(i=1, tofind = Count[blk]; i<64 && tofind>0; i++) {
          Ipp16s val = pDiff[scan[i]];
          if(val) {
            tofind--;
            pcoeff[i] = val;
          }
        }
        // reconstruction
        if (picture_coding_type != MPEG2_B_PICTURE) {
          pDiff[0] <<= intra_dc_shift;
          if(Count[blk]) {
            ippiQuantInvIntra_MPEG2_16s_C1I(pDiff, quant_scale_value, IntraQM);
            if (blk < 4 || m_info.FrameInfo.FourCC != MFX_FOURCC_NV12) {
              ippiDCT8x8Inv_16s8u_C1R (pDiff, BlockRec[cc] + block_offset_ref[blk], stride[cc]);
            } else {
              tmp_ippiDCT8x8InvUV_16s8u_C1R (pDiff, BlockRec[cc], stride[cc]);
            }
          } else {
            if (blk < 4 || m_info.FrameInfo.FourCC != MFX_FOURCC_NV12) {
              ippiSet_8u_C1R((Ipp8u)(pDiff[0]/8), BlockRec[cc] + block_offset_ref[blk], stride[cc], roi8x8);
            } else {
              tmp_ippiSet8x8UV_8u_C2R((Ipp8u)(pDiff[0]/8), BlockRec[cc], stride[cc]);
            }
          }
        }
      }
    }
    if((DctCoef *)coeff->Coeffs + ((Mb->MbXcnt+1 + Mb->MbYcnt * (m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1))*m_block_count ) * 64 != pcoeff)
      pcoeff = (DctCoef *)coeff->Coeffs + ((Mb->MbXcnt+1 + Mb->MbYcnt * (m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1))*m_block_count ) * 64;
  }

  return MFX_ERR_NONE;
}


mfxStatus MFXVideoENCMPEG2::preEnc(MPEG2FrameState* state)
{
  mfxFrameSurface *surface = m_cuc->FrameSurface;

  // derive picture_structure and picture_coding_type
  if(m_cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_I) picture_coding_type = MPEG2_I_PICTURE; else
  if(m_cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_P) picture_coding_type = MPEG2_P_PICTURE; else
  if(m_cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_B) picture_coding_type = MPEG2_B_PICTURE; else
    return MFX_ERR_UNSUPPORTED;

  //if(encodeInfo.info.interlace_type == UMC::PROGRESSIVE || encodeInfo.FieldPicture)
  //  m_cuc->FrameParam->FrameDCTprediction = 1;
  //encodeInfo.frame_pred_frame_dct[picture_coding_type - 1] = m_cuc->FrameParam->FrameDCTprediction;
  if (!m_cuc->FrameParam->MPEG2.FieldPicFlag) {
    state->curr_frame_pred = m_frame.FrameDCTprediction;
    state->curr_frame_dct = m_frame.FrameDCTprediction;
  } else {
    state->curr_frame_pred = 0;//!encodeInfo.allow_prediction16x8;
    state->curr_frame_dct = 1; // no choice
  }
  //if(m_frame.FrameMbsOnlyFlag)
  //  curr_scan = encodeInfo.altscan_tab[picture_coding_type - 1] = encodeInfo.FieldPicture;
  //else
  //  curr_scan = encodeInfo.altscan_tab[picture_coding_type - 1];
  //m_cuc->FrameParam->ScanMethod = (curr_scan==0) ? MFX_SCANMETHOD_MPEG2_ZIGZAG:MFX_SCANMETHOD_MPEG2_ALTERNATIVE;

  //curr_intra_vlc_format = encodeInfo.intraVLCFormat[picture_coding_type - 1];

  // probably need a special flag in FrameParam ???
  state->ipflag = (picture_coding_type == MPEG2_P_PICTURE && m_cuc->FrameParam->MPEG2.NumRefFrame == 0 && m_cuc->FrameParam->MPEG2.SecondFieldFlag) ? 1 : 0;

  // GetFrame stuff
  mfxI32 i, ind[2];
  mfxI32 curind = m_cuc->FrameParam->MPEG2.CurrFrameLabel;
  mfxI32 recind = m_cuc->FrameParam->MPEG2.RecFrameLabel;
  ind[0] = m_cuc->FrameParam->MPEG2.RefFrameListB[0][0];
  ind[1] = m_cuc->FrameParam->MPEG2.RefFrameListB[1][0];
  if (state->ipflag) ind[0] = curind;

  // check surface dimension
  if (surface->Info.Height < 16*(m_cuc->FrameParam->MPEG2.FrameHinMbMinus1+1) ||
      surface->Info.Width  < 16*(m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1))
    return MFX_ERR_UNSUPPORTED; // not aligned to MB

  state->YFrameHSize = surface->Data[curind]->Pitch;
  state->UVFrameHSize = state->YFrameHSize >> 1;

  // check pitches. Can be changed, but must be the same in all frames
  for(i=0; i< m_cuc->FrameParam->MPEG2.NumRefFrame; i++) { // forward/backward
    if (state->YFrameHSize != surface->Data[ind[i]]->Pitch)
      return MFX_ERR_UNSUPPORTED;
  }

  // GOP related computations
  // cast to U16 to work correctly on counter clipping (overflow)
  if(picture_coding_type == MPEG2_B_PICTURE) {
    state->P_distance = (mfxU16)(surface->Data[ind[1]]->FrameOrder - surface->Data[ind[0]]->FrameOrder);
    state->B_count = (mfxU16)(surface->Data[curind]->FrameOrder - surface->Data[ind[0]]->FrameOrder);
  } else {
    state->P_distance = (mfxU16)(surface->Data[curind]->FrameOrder - surface->Data[ind[0]]->FrameOrder);
    state->B_count = 0;
  }

  //m_cuc->FrameParam->BrokenLinkFlag = 0;

  //closed_gop = m_cuc->FrameParam->CloseEntryFlag;
  //temporal_reference = m_cuc->FrameParam->TemporalReference;

  //if(picture_coding_type == MPEG2_I_PICTURE) {
  //  mp_f_code = 0;
  //} else {
  //  mp_f_code = pMotionData[B_count].f_code;
  //  for(i=0; i<4; i++) {
  //    mfxU32 fcode = (m_cuc->FrameParam->BitStreamFcodes>>(12-(i*4))) & 15;
  //    if(fcode<1)
  //      fcode = 1; //return MFX_ERR_UNSUPPORTED;
  //    pMotionData[B_count].f_code[i>>1][i&1] = fcode;
  //    if(fcode == 15) continue;
  //    pMotionData[B_count].searchRange[i>>1][i&1] = 4<<fcode;
  //  }
  //}

  state->needFrameUnlock[0] = 0;
  if(!surface->Data[curind]->Y || !surface->Data[curind]->U || !surface->Data[curind]->V) {
    state->needFrameUnlock[0] =
      MFX_ERR_NONE == m_core->LockFrame(surface->Data[curind]->MemId, surface->Data[curind]);
  }
  mfxU32 offl, offc;
  offl = surface->Info.CropX + surface->Info.CropY*state->YFrameHSize;
  // now only YV12
  //offc = (surface->Info.CropX>>surface->Info.ScaleCWidth) +
  //  (surface->Info.CropY>>surface->Info.ScaleCHeight)*state->UVFrameHSize;
  offc = (surface->Info.CropX>>1) +
    (surface->Info.CropY>>1)*state->UVFrameHSize;
  if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12)
    offc *= 2;
  state->Y_src = (Ipp8u*)surface->Data[curind]->Y + offl;
  state->U_src = (Ipp8u*)surface->Data[curind]->U + offc;
  state->V_src = (Ipp8u*)surface->Data[curind]->V + offc;
  state->needFrameUnlock[3] = 0;
  if(picture_coding_type != MPEG2_B_PICTURE) {
    if(!surface->Data[recind]->Y || !surface->Data[recind]->U || !surface->Data[recind]->V) {
      state->needFrameUnlock[3] =
        MFX_ERR_NONE == m_core->LockFrame(surface->Data[recind]->MemId, surface->Data[recind]);
    }
    state->Y_out = (Ipp8u*)surface->Data[recind]->Y + offl;
    state->U_out = (Ipp8u*)surface->Data[recind]->U + offc;
    state->V_out = (Ipp8u*)surface->Data[recind]->V + offc;
  }
  {
    Ipp32s w_shift = (m_cuc->FrameParam->MPEG2.FieldPicFlag) ? 1 : 0;
    Ipp32s nYPitch = state->YFrameHSize << w_shift;
    Ipp32s nUPitch = state->UVFrameHSize << w_shift;
    Ipp32s nVPitch = state->UVFrameHSize << w_shift;

    block_offset_frm[2] = 8*nYPitch;
    block_offset_frm[3] = 8*nYPitch + 8;
    block_offset_frm[6] = 8*nUPitch;
    block_offset_frm[7] = 8*nVPitch;
    block_offset_frm[10] = 8*nUPitch + 8;
    block_offset_frm[11] = 8*nVPitch + 8;

    block_offset_fld[2] = nYPitch;
    block_offset_fld[3] = nYPitch + 8;
    block_offset_fld[6] = nUPitch;
    block_offset_fld[7] = nVPitch;
    block_offset_fld[10] = nUPitch + 8;
    block_offset_fld[11] = nVPitch + 8;
  }

  for(i=0; i<2; i++) { // forward/backward
    state->needFrameUnlock[i+1] = 0;
    if(!surface->Data[ind[i]]->Y || !surface->Data[ind[i]]->U || !surface->Data[ind[i]]->V) {
      state->needFrameUnlock[i+1] =
        MFX_ERR_NONE == m_core->LockFrame(surface->Data[ind[i]]->MemId, surface->Data[ind[i]]);
    }
    state->YRecFrame[0][i] = (Ipp8u*)surface->Data[ind[i]]->Y + offl;
    state->URecFrame[0][i] = (Ipp8u*)surface->Data[ind[i]]->U + offc;
    state->VRecFrame[0][i] = (Ipp8u*)surface->Data[ind[i]]->V + offc;
    state->YRecFrame[1][i] = state->YRecFrame[0][i] + state->YFrameHSize;
    if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
      state->URecFrame[1][i] = state->URecFrame[0][i] + state->UVFrameHSize*2;
      state->VRecFrame[1][i] = state->VRecFrame[0][i] + state->UVFrameHSize*2;
    } else {
      state->URecFrame[1][i] = state->URecFrame[0][i] + state->UVFrameHSize;
      state->VRecFrame[1][i] = state->VRecFrame[0][i] + state->UVFrameHSize;
    }
  }

  if (m_cuc->FrameParam->MPEG2.FieldPicFlag) {
    if (!m_cuc->FrameParam->MPEG2.BottomFieldFlag) {
      if (picture_coding_type == MPEG2_P_PICTURE && m_cuc->FrameParam->MPEG2.SecondFieldFlag) {
        state->YRecFrame[1][0] = state->Y_src + state->YFrameHSize;  //YRecFrame[1][1];
        if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
          state->URecFrame[1][0] = state->U_src + state->UVFrameHSize*2; //URecFrame[1][1];
          state->VRecFrame[1][0] = state->V_src + state->UVFrameHSize*2; //VRecFrame[1][1];
        } else {
          state->URecFrame[1][0] = state->U_src + state->UVFrameHSize; //URecFrame[1][1];
          state->VRecFrame[1][0] = state->V_src + state->UVFrameHSize; //VRecFrame[1][1];
        }
      }
    } else { // (m_cuc->FrameParam->BottomFieldFlag)
      if (picture_coding_type == MPEG2_P_PICTURE && m_cuc->FrameParam->MPEG2.SecondFieldFlag) {
         state->YRecFrame[0][0] = state->Y_src; //YRecFrame[0][1];
         state->URecFrame[0][0] = state->U_src; //URecFrame[0][1];
         state->VRecFrame[0][0] = state->V_src; //VRecFrame[0][1];
      }
      state->Y_src += state->YFrameHSize;
      state->Y_out += state->YFrameHSize;
      if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
        state->U_src += state->UVFrameHSize*2;
        state->V_src += state->UVFrameHSize*2;
        state->U_out += state->UVFrameHSize*2;
        state->V_out += state->UVFrameHSize*2;
      } else {
        state->U_src += state->UVFrameHSize;
        state->V_src += state->UVFrameHSize;
        state->U_out += state->UVFrameHSize;
        state->V_out += state->UVFrameHSize;
      }
    }
    state->YFrameHSize<<=1;
    state->UVFrameHSize<<=1;
    if (m_cuc->FrameParam->MPEG2.SecondFieldFlag)
      pMBInfo = pMBInfo0 + (m_cuc->FrameParam->MPEG2.FrameHinMbMinus1+1)*(m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1) / 2; //need /2???
  }

//  changeQuant(5); // const while no RC

  //quantiser_scale_value = mfx_mpeg2_Val_QScale[m_cuc->FrameParam->MPEG2.QuantScaleType][m_cuc->MbParam->Mb[0].QpScaleCode];
  //if(quantiser_scale_value >= 8)
  //  intra_dc_precision = 0;
  //else if(quantiser_scale_value >= 4)
  //  intra_dc_precision = 1;
  //else
  //  intra_dc_precision = 2;
  //varThreshold = quantiser_scale_value;// + qq/32;
  //meanThreshold = quantiser_scale_value * 8;// + qq/32;
  //sadThreshold = (quantiser_scale_value + 0) * 8;// + qq/16;
  //if(picture_coding_type == UMC::MPEG2_I_PICTURE) {
  //  if(quantiser_scale_value < 5)
  //    encodeInfo.intraVLCFormat[picture_coding_type - UMC::MPEG2_I_PICTURE] = 1;
  //  else if(quantiser_scale_value > 15)
  //    encodeInfo.intraVLCFormat[picture_coding_type - UMC::MPEG2_I_PICTURE] = 0;
  //  curr_intra_vlc_format = encodeInfo.intraVLCFormat[picture_coding_type - UMC::MPEG2_I_PICTURE];
  //}

  //m_cuc->FrameParam->PicQuantizer = quantiser_scale_value;

  //m_cuc->FrameParam->IntraDCprecision =  intra_dc_precision;
  //m_cuc->FrameParam->PicStructure = picture_structure;
  //m_cuc->FrameParam->TopFieldFirst = top_field_first;
  //m_cuc->FrameParam->FrameDCTprediction = encodeInfo.frame_pred_frame_dct[picture_coding_type - 1];
  //m_cuc->FrameParam->ConcealmentMVs = concealment_motion_vectors;
  //m_cuc->FrameParam->QuantScaleType = q_scale_type;
  //m_cuc->FrameParam->IntraVLCformat = curr_intra_vlc_format;
  //m_cuc->FrameParam->AlternateScan = curr_scan;
  //m_cuc->FrameParam->RepeatFirstField = repeat_first_field;
  //mfxU32 progressive_frame = encodeInfo.info.interlace_type == UMC::PROGRESSIVE;
  //m_cuc->FrameParam->Chroma420type = progressive_frame; // chroma420 == prog_frame here
  //m_cuc->FrameParam->ProgressiveFrame = progressive_frame;

  if (picture_coding_type == MPEG2_P_PICTURE) { // BW would be used for DMV in DualPrime
    state->YRecFrame[0][1] = state->YRecFrame[0][0];
    state->URecFrame[0][1] = state->URecFrame[0][0];
    state->VRecFrame[0][1] = state->VRecFrame[0][0];
    state->YRecFrame[1][1] = state->YRecFrame[1][0];
    state->URecFrame[1][1] = state->URecFrame[1][0];
    state->VRecFrame[1][1] = state->VRecFrame[1][0];
  }

  return MFX_ERR_NONE;
}

void      MFXVideoENCMPEG2::postEnc(MPEG2FrameState* state)
{
  if (m_cuc->FrameParam->MPEG2.FieldPicFlag) {
    // restore params
    pMBInfo = pMBInfo0;
  }

  mfxFrameSurface *surface = m_cuc->FrameSurface;
  mfxI32 i, ind[2];
  mfxI32 curind = m_cuc->FrameParam->MPEG2.CurrFrameLabel;
  mfxI32 recind = m_cuc->FrameParam->MPEG2.RecFrameLabel;
  ind[0] = m_cuc->FrameParam->MPEG2.RefFrameListB[0][0];
  ind[1] = m_cuc->FrameParam->MPEG2.RefFrameListB[1][0];
  if (state->ipflag) ind[0] = curind;
  state->ipflag = 0;

  if(state->needFrameUnlock[0]) {
    m_core->UnlockFrame(surface->Data[curind]->MemId, surface->Data[curind]);
    state->needFrameUnlock[0] = 0;
  }
  if(state->needFrameUnlock[3]) {
    m_core->UnlockFrame(surface->Data[recind]->MemId, surface->Data[recind]);
    state->needFrameUnlock[3] = 0;
  }
  for(i=0; i<2; i++) { // forward/backward
    if(state->needFrameUnlock[i+1]) {
      m_core->UnlockFrame(surface->Data[ind[i]]->MemId, surface->Data[ind[i]]);
      state->needFrameUnlock[i+1] = 0;
    }
  }
  if (m_cuc->FrameParam->MPEG2.FieldPicFlag) {
    // restore params
    state->YFrameHSize>>=1;
    state->UVFrameHSize>>=1;
  }
}

// mode == 0 for sequence header, mode == 1 for quant_matrix_extension
mfxStatus MFXVideoENCMPEG2::updateQMatrices()
{
  mfxU32 i;
  mfxU32 sameflag[4] = {1,1,1,1};
  mfxExtMpeg2QMatrix* qm = 0;
  if(m_cuc != 0)
    qm = (mfxExtMpeg2QMatrix*)GetExtBuffer(m_cuc, MFX_CUC_MPEG2_QMATRIX);
  if(qm == 0) { // default are used
    // reset qm if current is not default
    InvIntraChromaQM = InvIntraQM = 0;
    InvInterChromaQM = InvInterQM = 0;
    IntraQM = _IntraQM;
    IntraChromaQM = _IntraChromaQM;
    InterChromaQM = InterQM = 0;
    for(i=0; i<64; i++) {
      _IntraQM[i] = _IntraChromaQM[i] = mfx_mpeg2_default_intra_qm[i];
    }
    return MFX_ERR_NONE;
  }

  for(i=0; i<64 && sameflag[0]; i++) {
    if(qm->IntraQMatrix[i] != mfx_mpeg2_default_intra_qm[i])
      sameflag[0] = 0;
  }
  for(i=0; i<64 && sameflag[1]; i++) {
    if(qm->InterQMatrix[i] != 16)
      sameflag[1] = 0;
  }

  if(m_info.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420+0) {
    for(i=0; i<64 && sameflag[2]; i++) {
      if(qm->ChromaIntraQMatrix[i] != qm->IntraQMatrix[i])
        sameflag[2] = 0;
    }
    for(i=0; i<64 && sameflag[3]; i++) {
      if(qm->ChromaInterQMatrix[i] != qm->InterQMatrix[i])
        sameflag[3] = 0;
    }
  }

  if(sameflag[0]) {
    InvIntraQM = 0;
    IntraQM = _IntraQM;
    for(i=0; i<64; i++) {
      _IntraQM[i] = mfx_mpeg2_default_intra_qm[i];
    }
  } else {
    InvIntraQM = _InvIntraQM;
    IntraQM = _IntraQM;
    for(i=0; i<64; i++) {
      _InvIntraQM[i] = 1.f/(mfxF32)qm->IntraQMatrix[i];
      _IntraQM[i] = qm->IntraQMatrix[i];
    }
  }
  if(sameflag[1]) {
    InvInterQM = 0;
    InterQM = 0;
  } else {
    InvInterQM = _InvInterQM;
    InterQM = _InterQM;
    for(i=0; i<64; i++) {
      _InvInterQM[i] = 1.f/(mfxF32)qm->InterQMatrix[i];
      _InterQM[i] = qm->InterQMatrix[i];
    }
  }
  if(m_info.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420+0) {
    if(sameflag[2]) {
      InvIntraChromaQM = 0;
      IntraChromaQM = _IntraChromaQM;
      for(i=0; i<64; i++) {
        _IntraChromaQM[i] = mfx_mpeg2_default_intra_qm[i];
      }
    } else {
      InvIntraChromaQM = _InvIntraChromaQM;
      IntraChromaQM = _IntraChromaQM;
      for(i=0; i<64; i++) {
        _InvIntraChromaQM[i] = 1.f/(mfxF32)qm->ChromaIntraQMatrix[i];
        _IntraChromaQM[i] = qm->ChromaIntraQMatrix[i];
      }
    }
    if(sameflag[3]) {
      InvInterChromaQM = 0;
      InterChromaQM = 0;
    } else {
      InvInterChromaQM = _InvInterChromaQM;
      InterChromaQM = _InterChromaQM;
      for(i=0; i<64; i++) {
        _InvInterChromaQM[i] = 1.f/(mfxF32)qm->ChromaInterQMatrix[i];
        _InterChromaQM[i] = qm->ChromaInterQMatrix[i];
      }
    }
  }

  return MFX_ERR_NONE;
}



#endif // MFX_ENABLE_MPEG2_VIDEO_ENC
