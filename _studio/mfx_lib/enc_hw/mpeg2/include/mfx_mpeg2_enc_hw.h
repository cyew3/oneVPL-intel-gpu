//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2012 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_MPEG2_ENC_H_
#define _MFX_MPEG2_ENC_H_

#include "mfx_common.h"
#include "mfx_utils.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENC)

#include "mfxvideo++int.h"
#include "mfx_mpeg2_enc_common_hw.h"


#ifndef __SW_ENC
//#include "mfx_mpeg2_enc_ddi_hw.h"
//#include "mfx_mpeg2_encode_d3d9.h"
#include "mfx_mpeg2_encode_interface.h"

#ifdef MPEG2_ENC_HW_PERF
#include "vm_time.h"
#endif


class MFXVideoENCMPEG2_HW_DDI
{
public:
    MFXVideoENCMPEG2_HW_DDI(VideoCORE *core, mfxStatus *sts);

    virtual ~MFXVideoENCMPEG2_HW_DDI();

    mfxStatus Close(); 

    mfxStatus Reset(mfxVideoParamEx_MPEG2 *par);

    mfxStatus Init(mfxVideoParamEx_MPEG2 *par);

    mfxStatus RunFrameVmeENC(mfxFrameCUC *cuc);  

    mfxStatus RunFrameVmeENC_Stage1(mfxFrameCUC *cuc); 

    mfxStatus RunFrameVmeENC_Stage2(mfxFrameCUC *cuc);

    static inline mfxU16 GetIOPattern ()
    {
        return MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }

private:
    // Declare private copy constructor to avoid accidental assignment
    // and klocwork complaining.
    MFXVideoENCMPEG2_HW_DDI(const MFXVideoENCMPEG2_HW_DDI &);
    MFXVideoENCMPEG2_HW_DDI & operator = (const MFXVideoENCMPEG2_HW_DDI &);

    VideoCORE*                      m_core;
    MfxHwMpeg2Encode::ExecuteBuffers*    m_pExecuteBuffers;
    MfxHwMpeg2Encode::DriverEncoder*     m_pDdiEncoder;
    bool                            m_bStage2Ready;

#ifdef MPEG2_ENC_HW_PERF
    vm_time enc_call_time;
    vm_time enc_fill_MB_buffer_time;
    vm_time enc_set_MB_data_time;
#endif 

};

#else

/////////////////////////////////////////////
#include "ippvc.h"
#include "umc_mpeg2_newfunc.h"

class MFXVideoENCMPEG2_HW : public VideoENC 
{
public:
    typedef struct _IppMotionVector2
    {
      Ipp32s x;
      Ipp32s y;
      Ipp32s mctype_l;
      Ipp32s offset_l;
      Ipp32s mctype_c;
      Ipp32s offset_c;
    } IppMotionVector2;

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

    struct MPEG2FrameState
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
    } ;
    #define ME_PARAMS_ENC         \
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


public:
  static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
  static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

  MFXVideoENCMPEG2_HW(VideoCORE *core, mfxStatus *sts) : VideoENC()
  {
    m_core = core;
    m_frame.CucId = 0;
    m_slice.CucId = 0;
    allocatedMBInfo = 0;
    me_matrix_size = 0;
    *sts = MFX_ERR_NONE;
  }
  virtual ~MFXVideoENCMPEG2_HW() { Close(); }

  virtual mfxStatus Init(mfxVideoParam *par);
 
  virtual mfxStatus Close(void);
  virtual mfxStatus Reset(mfxVideoParam *par);

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

  static inline mfxU16 GetIOPattern ()
  {
    return MFX_IOPATTERN_IN_SYSTEM_MEMORY;
  }


public:
  inline mfxStatus Init(mfxVideoParamEx_MPEG2 *par)
  {
    return Init(&par->mfxVideoParams);
  }
  inline mfxStatus Reset(mfxVideoParamEx_MPEG2 *par)
  {
     return Reset(&par->mfxVideoParams); 
  }

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
  mfxStatus MotionEstimation_Frame(ME_PARAMS_ENC);
  mfxStatus MotionEstimation_Field(ME_PARAMS_ENC);
  mfxStatus MotionEstimation_FieldPict(ME_PARAMS_ENC);


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

#ifdef MPEG2_ENCODE_DEBUG_HW
  friend class MPEG2EncodeDebug_HW;
  public: MPEG2EncodeDebug_HW *mpeg2_debug_ENC_HW;
#endif //MPEG2_ENCODE_DEBUG_HW

private:
    // Declare private copy constructor to avoid accidental assignment
    // and klocwork complaining.
    //MFXVideoENCMPEG2_HW const MFXVideoENCMPEG2_HW  &);
    //MFXVideoENCMPEG2_HW & operator = (const MFXVideoENCMPEG2_HW  &);
};
#endif //SW_ENC

#endif // MFX_ENABLE_MPEG2_VIDEO_ENC
#endif //_MFX_MPEG2_ENC_H_
