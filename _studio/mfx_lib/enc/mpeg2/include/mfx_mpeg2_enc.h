//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_MPEG2_ENC_H_
#define _MFX_MPEG2_ENC_H_

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENC)

#include "mfxvideo++int.h"
#include "ippvc.h"
//#include "umc_mpeg2_enc_defs.h"
#include "umc_mpeg2_newfunc.h"

#if defined (WIN32)
#pragma warning (push)
#pragma warning (disable:4351)  // value initialization is intended and disabling this warning recommended by MS: https://msdn.microsoft.com/en-us/en-en/library/1ywe7hcy.aspx
#endif

enum MPEG2FrameType
{
  MPEG2_I_PICTURE = 1,
  MPEG2_P_PICTURE = 2,
  MPEG2_B_PICTURE = 3
};

typedef mfxI16 DctCoef;

typedef struct _MBInfo  // macroblock information
{
  mfxI32 mb_type;               // intra/forward/backward/interpolated
  mfxI32 dct_type;              // field/frame DCT
  mfxI32 prediction_type;       // MC_FRAME/MC_FIELD
  IppiPoint MV[2][2];           // motion vectors [vecnum][F/B]
  IppiPoint MV_P[2];            // motion vectors from P frame [vecnum]
  mfxI32 mv_field_sel[3][2];    // motion vertical field select:
  // the first index: 0-top field, 1-bottom field, 2-frame;
  // the second index:0-forward, 1-backward
  mfxI32 skipped;
} MBInfo;

typedef struct _MPEG2FrameState
{
  // Input frame
  mfxU8       *Y_src;
  mfxU8       *U_src;
  mfxU8       *V_src;
  // Input reference reconstructed frames
  Ipp8u       *YRecFrame[2][2];   // [top/bottom][fwd/bwd]
  Ipp8u       *URecFrame[2][2];
  Ipp8u       *VRecFrame[2][2];
  // Output reconstructed frame
  mfxU8       *Y_out;
  mfxU8       *U_out;
  mfxU8       *V_out;
  mfxI32      YFrameHSize;
  mfxI32      UVFrameHSize;
  mfxI32      B_count;
  mfxI32      P_distance;
  mfxI32      ipflag;
  mfxI32      curr_frame_pred;
  mfxI32      curr_frame_dct;
  //Ipp32s      varThreshold;          // threshold for block variance
  //Ipp32s      meanThreshold;         // threshold for block mean
  //Ipp32s      sadThreshold;          // threshold for block mean
  mfxU32      needFrameUnlock[4]; // 0-src, 1,2-Rec, 3-out
} MPEG2FrameState;


typedef struct _IppMotionVector2
{
  Ipp32s x;
  Ipp32s y;
  Ipp32s mctype_l;
  Ipp32s offset_l;
  Ipp32s mctype_c;
  Ipp32s offset_c;
} IppMotionVector2;

//typedef struct
//{
//  //bitBuffer bBuf;
//  //Ipp16s *pMBlock;
//  //Ipp16s *pDiff;
//  //Ipp16s *pDiff1;
//  //IppiPoint PMV[2][2];
//  //Ipp32s start_row;
//  //Ipp32s numIntra;
//  //Ipp32s fieldCount;
//  //Ipp32s weightIntra;
//  //Ipp32s weightInter;
//  //Ipp32s sizeVLCtab0;
//  //Ipp32s sizeVLCtab1;
//
//  // motion estimation
//  //Ipp32s me_matrix_size;
//  //Ipp8u *me_matrix_buff;
//  //Ipp32s me_matrix_id;
//} threadSpecificData;

#define ME_PARAMS             \
  const Ipp8u* pRefFld,       \
  const Ipp8u* pRecFld,       \
  Ipp32s   RefStep,           \
  const Ipp8u* pBlock,        \
  Ipp32s   BlockStep,         \
  Ipp32s*  pSrcMean,          \
  Ipp32s*  pDstVar,           \
  Ipp32s*  pDstMean,          \
  Ipp32s   limit_left,        \
  Ipp32s   limit_right,       \
  Ipp32s   limit_top,         \
  Ipp32s   limit_bottom,      \
  IppiPoint InitialMV0,       \
  IppiPoint InitialMV1,       \
  IppiPoint InitialMV2,       \
  IppMotionVector2 *vector,   \
  MPEG2FrameState *state,     \
  Ipp32s   i,                 \
  Ipp32s   j,                 \
  Ipp32s   *vertical_field_select, \
  Ipp32s   *currMAD,          \
  Ipp32s   parity, /*0-same*/ \
  Ipp32s*   resVar

#if defined (_WIN32_WCE) && defined (_M_IX86) && defined (__stdcall)
#define _IPP_STDCALL_CDECL
#undef __stdcall
#endif

typedef IppStatus (__STDCALL *functype_getdiff)(
  const Ipp8u*  pSrcCur,
  Ipp32s  srcCurStep,
  const Ipp8u*  pSrcRef,
  Ipp32s  srcRefStep,
  Ipp16s* pDstDiff,
  Ipp32s  dstDiffStep,
  Ipp16s* pDstPredictor,
  Ipp32s  dstPredictorStep,
  Ipp32s  mcType,
  Ipp32s  roundControl);

typedef IppStatus (__STDCALL *functype_getdiffB)(
  const Ipp8u*       pSrcCur,
  Ipp32s       srcCurStep,
  const Ipp8u*       pSrcRefF,
  Ipp32s       srcRefStepF,
  Ipp32s       mcTypeF,
  const Ipp8u*       pSrcRefB,
  Ipp32s       srcRefStepB,
  Ipp32s       mcTypeB,
  Ipp16s*      pDstDiff,
  Ipp32s       dstDiffStep,
  Ipp32s       roundControl);

typedef IppStatus (__STDCALL *functype_mc)(
  const Ipp8u*       pSrcRef,
  Ipp32s       srcStep,
  const Ipp16s*      pSrcYData,
  Ipp32s       srcYDataStep,
  Ipp8u*       pDst,
  Ipp32s       dstStep,
  Ipp32s       mcType,
  Ipp32s       roundControl);

typedef IppStatus (__STDCALL *functype_mcB)(
  const Ipp8u*       pSrcRefF,
  Ipp32s       srcStepF,
  Ipp32s       mcTypeF,
  const Ipp8u*       pSrcRefB,
  Ipp32s       srcStepB,
  Ipp32s       mcTypeB,
  const Ipp16s*      pSrcYData,
  Ipp32s       srcYDataStep,
  Ipp8u*       pDst,
  Ipp32s       dstStep,
  Ipp32s       roundControl);

#if defined (_IPP_STDCALL_CDECL)
#undef  _IPP_STDCALL_CDECL
#define __stdcall __cdecl
#endif

class MFXVideoENCMPEG2 : public VideoENC {
public:
  static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
  static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

  MFXVideoENCMPEG2(VideoCORE *core, mfxStatus *sts)
      : VideoENC()
      , needFrameUnlock()
      , m_core(core)
      , m_cuc()
      , m_info()
      , m_frame()
      , m_slice()
      , pMBInfo()
      , pMBInfo0()
      , allocatedMBInfo()
      , mid_MBInfo()
      , m_qm()
      , _InvIntraQM()
      , _InvInterQM()
      , _InvIntraChromaQM()
      , _InvInterChromaQM()
      , InvIntraQM()
      , InvInterQM()
      , InvIntraChromaQM()
      , InvInterChromaQM()
      , _IntraQM()
      , _InterQM()
      , _IntraChromaQM()
      , _InterChromaQM()
      , IntraQM()
      , InterQM()
      , IntraChromaQM()
      , InterChromaQM()
      , picture_coding_type()
      , m_block_count()
      , block_offset_frm()
      , block_offset_fld()
      , frm_dct_step()
      , fld_dct_step()
      , frm_diff_off()
      , fld_diff_off()
      , func_getdiff_frame_c()
      , func_getdiff_field_c()
      , func_getdiffB_frame_c()
      , func_getdiffB_field_c()
      , func_mc_frame_c()
      , func_mc_field_c()
      , func_mcB_frame_c()
      , func_mcB_field_c()
      , BlkWidth_c()
      , BlkHeight_c()
      , chroma_fld_flag()
      , m_me_alg_num()
      , me_matrix_size()
      , me_matrix_active_size()
      , me_matrix_buff()
      , me_matrix_id()
      , mid_me_matrix()
  {
    *sts = MFX_ERR_NONE;
  }
  virtual ~MFXVideoENCMPEG2() { Close(); }

  virtual mfxStatus Init(mfxVideoParam *par);
  virtual mfxStatus Reset(mfxVideoParam *par);
  virtual mfxStatus Close(void);

  virtual mfxStatus GetVideoParam(mfxVideoParam *par);
  virtual mfxStatus GetFrameParam(mfxFrameParam *par);
  virtual mfxStatus GetSliceParam(mfxSliceParam *par);

  virtual mfxStatus RunSliceVmeENC(mfxFrameCUC *cuc);
  virtual mfxStatus RunSlicePredENC(mfxFrameCUC *cuc);
  virtual mfxStatus RunSliceFullENC(mfxFrameCUC *cuc);
  virtual mfxStatus GetSliceResid(mfxFrameCUC *cuc);
  virtual mfxStatus RunSliceFTQ(mfxFrameCUC *cuc);

  virtual mfxStatus RunFrameVmeENC(mfxFrameCUC *cuc);
  virtual mfxStatus RunFramePredENC(mfxFrameCUC *cuc);
  virtual mfxStatus RunFrameFullENC(mfxFrameCUC *cuc);
  virtual mfxStatus GetFrameResid(mfxFrameCUC *cuc);
  virtual mfxStatus RunFrameFTQ(mfxFrameCUC *cuc);

private:
  mfxStatus loadCUCFrameParams(mfxI32* fcodes, mfxI32* mvmax);
  mfxStatus sliceME( MPEG2FrameState* state, mfxU32 mbstart, mfxU32 mbcount,
    mfxI32* fcodes, // dir*2+xy
    mfxI32* mvmax);
  mfxStatus sliceCoef( MPEG2FrameState* state, mfxU32 mbstart, mfxU32 mbcount, mfxExtMpeg2Coeffs* coeff);
  mfxStatus preEnc(MPEG2FrameState* state);  // convert cuc params to/from internals
  void      postEnc(MPEG2FrameState* state);
  mfxU32      needFrameUnlock[3];

  /////Motion estimation
  mfxStatus MotionEstimation_Frame(ME_PARAMS);
  mfxStatus MotionEstimation_Field(ME_PARAMS);
  mfxStatus MotionEstimation_FieldPict(ME_PARAMS);


  //UMC::MPEG2VideoEncoderBase m_codec;
  VideoCORE*    m_core;
  mfxFrameCUC*  m_cuc;
  mfxInfoMFX    m_info;
  mfxFrameParamMPEG2 m_frame;
  mfxSliceParamMPEG2 m_slice;

  MBInfo *pMBInfo;
  MBInfo *pMBInfo0;
  mfxU32 allocatedMBInfo;
  mfxMemId mid_MBInfo;

  mfxStatus updateQMatrices();
  mfxExtMpeg2QMatrix m_qm;
  mfxF32  _InvIntraQM[64];
  mfxF32  _InvInterQM[64];
  mfxF32  _InvIntraChromaQM[64];
  mfxF32  _InvInterChromaQM[64];
  mfxF32* InvIntraQM;
  mfxF32* InvInterQM;
  mfxF32* InvIntraChromaQM;
  mfxF32* InvInterChromaQM;
  mfxI16  _IntraQM[64];
  mfxI16  _InterQM[64];
  mfxI16  _IntraChromaQM[64];
  mfxI16  _InterChromaQM[64];
  mfxI16* IntraQM;
  mfxI16* InterQM;
  mfxI16* IntraChromaQM;
  mfxI16* InterChromaQM;
  MPEG2FrameType picture_coding_type;
  mfxU16 m_block_count;
  // Offsets
  Ipp32s block_offset_frm[12];
  Ipp32s block_offset_fld[12];
  Ipp32s frm_dct_step[12];
  Ipp32s fld_dct_step[12];
  Ipp32s frm_diff_off[12];
  Ipp32s fld_diff_off[12];

  // GetDiff & MotionCompensation functions
  functype_getdiff
    func_getdiff_frame_c, func_getdiff_field_c;
  functype_getdiffB
    func_getdiffB_frame_c, func_getdiffB_field_c;
  functype_mc
    func_mc_frame_c, func_mc_field_c;
  functype_mcB
    func_mcB_frame_c, func_mcB_field_c;

  // Internal var's
  Ipp32s BlkWidth_c;
  Ipp32s BlkHeight_c;
  Ipp32s chroma_fld_flag;

  mfxU32 m_me_alg_num;
  mfxU32 me_matrix_size;
  mfxU32 me_matrix_active_size;
  mfxU8 *me_matrix_buff;
  mfxU32 me_matrix_id;
  mfxMemId mid_me_matrix;

};

#if defined (WIN32)
#pragma warning (pop)
#endif

#endif // MFX_ENABLE_MPEG2_VIDEO_ENC
#endif //_MFX_MPEG2_ENC_H_
