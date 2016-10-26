//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_MPEG2_PAK_H_
#define _MFX_MPEG2_PAK_H_

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_PAK)

#include "mfxvideo++int.h"
#include "ippvc.h"
#include "umc_mpeg2_newfunc.h"
#include "mfx_mpeg2_enc_common_hw.h"
#include "vm_thread.h"
#include "vm_event.h"
#include "vm_mutex.h"
#include <queue>

#define IPP_NV12

#define MPEG2_PAK_THREADED

#ifdef MPEG2_PAK_THREADED
class ThreadedMPEG2PAK;
#endif

class MFXVideoPAKMPEG2 : public VideoPAK 
{
public:
    enum MPEG2FrameType
    {
      MPEG2_I_PICTURE = 1,
      MPEG2_P_PICTURE = 2,
      MPEG2_B_PICTURE = 3
    };
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
      mfxI32      YSrcFrameHSize;
      mfxI32      UVSrcFrameHSize;

      // Input reference reconstructed frames
      Ipp8u       *YRefFrame[2][2];   // [top/bottom][fwd/bwd]
      Ipp8u       *URefFrame[2][2];
      Ipp8u       *VRefFrame[2][2];
      mfxI32      YRefFrameHSize;
      mfxI32      UVRefFrameHSize;     
      
      // Output reconstructed frame
      mfxU8       *Y_out;
      mfxU8       *U_out;
      mfxU8       *V_out;

      mfxI32      YOutFrameHSize;
      mfxI32      UVOutFrameHSize;     


      mfxU32      needFrameUnlock[4]; // 0-src, 1,2-Rec, 3-out
    } MPEG2FrameState;
    typedef struct
    {
      mfxI32 bit_offset;
      mfxI32 bytelen;
      mfxU8  *start_pointer;
      mfxU32 *current_pointer;
    } bitBuffer;

 
 


    // used to specify motion estimation ranges for different types of pictures
    typedef struct _MotionData  // motion data
    {
      Ipp32s f_code[2][2];      // [forward=0/backward=1][x=0/y=1]
      Ipp32s searchRange[2][2]; // search range, in pixels, -sr <= x < sr
    } MotionData;

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

  typedef IppStatus (__STDCALL *functype_getdiffB_nv12)(
  const Ipp8u *pSrcCur, 
        Ipp32s srcCurStep,
  const Ipp8u *pSrcRefF,
        Ipp32s srcRefStepF,
        Ipp32s mcTypeF,
  const Ipp8u *pSrcRefB,
        Ipp32s srcRefStepB,
        Ipp32s mcTypeB,
        Ipp16s *pDstDiffU, 
        Ipp32s dstDiffStepU,
        Ipp16s *pDstDiffV, 
        Ipp32s dstDiffStepV,
        Ipp32s roundControl);

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

   typedef IppStatus (__STDCALL *functype_mc_nv12)(
      const Ipp8u*   pSrcRef,
        Ipp32s       srcRefStep,
  const Ipp16s*      pSrcU,
  const Ipp16s*      pSrcV,
        Ipp32s       srcUVStep,
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

typedef IppStatus (__STDCALL *functype_mcB_nv12)(
  const Ipp8u*       pSrcRefF,
        Ipp32s       srcRefFStep,
        Ipp32s       mcTypeF,
  const Ipp8u*       pSrcRefB,
        Ipp32s       srcRefBStep,
        Ipp32s       mcTypeB,
  const Ipp16s*      pSrcU,
  const Ipp16s*      pSrcV,
        Ipp32s       srcUVStep,
        Ipp8u*       pDst,
        Ipp32s       dstStep,
        Ipp32s       roundControl);   

    struct FrameUserData
    {   
        mfxU8* m_pUserData;
        mfxU32 m_len;
    };

    #if defined (_IPP_STDCALL_CDECL)
    #undef  _IPP_STDCALL_CDECL
    #define __stdcall __cdecl
    #endif
public:
  static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
  static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

  MFXVideoPAKMPEG2(VideoCORE *core, mfxStatus *sts);
  ~MFXVideoPAKMPEG2();

  virtual mfxStatus Init(mfxVideoParam *par);
  virtual mfxStatus Reset(mfxVideoParam *par);
  virtual mfxStatus Close(void);

  virtual mfxStatus GetVideoParam(mfxVideoParam *par);
  virtual mfxStatus GetFrameParam(mfxFrameParam *par);
  virtual mfxStatus GetSliceParam(mfxSliceParam *par);

  virtual mfxStatus RunSeqHeader(mfxFrameCUC *cuc);
  virtual mfxStatus RunGopHeader(mfxFrameCUC *cuc);
  virtual mfxStatus RunPicHeader(mfxFrameCUC *cuc);
  virtual mfxStatus RunSliceHeader(mfxFrameCUC *cuc);
  virtual mfxStatus InsertUserData(mfxU8 *ud,mfxU32 len,mfxU64 ts,mfxBitstream *bs);
  virtual mfxStatus InsertBytes(mfxU8 *data, mfxU32 len, mfxBitstream *bs);
  virtual mfxStatus AlignBytes(mfxU8 pattern, mfxU32 len, mfxBitstream *bs);

  virtual mfxStatus RunSlicePAK(mfxFrameCUC *cuc);
          mfxStatus RunSlicePAK(mfxFrameCUC *cuc, mfxI32 numSlice, mfxBitstream *bs, MPEG2FrameState* pState);

  virtual mfxStatus RunSliceBSP(mfxFrameCUC *cuc);
  virtual mfxStatus RunFramePAK(mfxFrameCUC *cuc);
  virtual mfxStatus RunFrameBSP(mfxFrameCUC *cuc);

  static inline mfxU16 GetIOPattern ()
  {
     return MFX_IOPATTERN_IN_SYSTEM_MEMORY;
  }

#ifdef MPEG2_PAK_THREADED
  ThreadedMPEG2PAK *pThreadedMPEG2PAK ;
#endif

private:
  mfxStatus loadCUCFrameParams(mfxI32* fcodes, mfxI32* mvmax);
  mfxStatus sliceBSP(mfxU32 mbstart, mfxU32 mbcount,
    mfxI32* fcodes, // dir*2+xy
    mfxI32* mvmax,
    mfxExtMpeg2Coeffs* coeff,
    bitBuffer* outbb);
  mfxStatus sliceCoef( MPEG2FrameState* state, mfxU32 mbstart, mfxU32 mbcount, mfxExtMpeg2Coeffs* coeff);
  mfxStatus preEnc(MPEG2FrameState* state);  // convert cuc params to/from internals
  void      postEnc(MPEG2FrameState* state);

  VideoCORE*    m_core;
  mfxFrameCUC*  m_cuc;
  mfxInfoMFX    m_info;

  mfxFrameParamMPEG2    m_frame;
  mfxSliceParamMPEG2    m_slice;
  mfxExtVideoSignalInfo m_vsi;

  mfxStatus putQMatrices(int mode, bitBuffer* outbb);
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
// Variable Length Coding tables
  IppVCHuffmanSpec_32s *vlcTableB5c_e;
  IppVCHuffmanSpec_32s *vlcTableB15;

  MotionData m_MotionData;
  MPEG2FrameType picture_coding_type;
  const IppVCHuffmanSpec_32u *m_DC_Tbl[3];
  Ipp64f framerate;
  mfxU16 m_block_count;
  // Offsets
  Ipp32s src_block_offset_frm[12];
  Ipp32s src_block_offset_fld[12];
  Ipp32s out_block_offset_frm[12];
  Ipp32s out_block_offset_fld[12];

  Ipp32s frm_dct_step[12];
  Ipp32s fld_dct_step[12];
  Ipp32s frm_diff_off[12];
  Ipp32s fld_diff_off[12];

  // GetDiff & MotionCompensation functions
   functype_getdiff
    func_getdiff_frame_c, func_getdiff_field_c;
  functype_getdiff
    func_getdiff_frame_nv12_c, func_getdiff_field_nv12_c;
  functype_getdiffB
    func_getdiffB_frame_c, func_getdiffB_field_c;
  functype_getdiffB_nv12
    func_getdiffB_frame_nv12_c, func_getdiffB_field_nv12_c;
  functype_mc
    func_mc_frame_c, func_mc_field_c;
  functype_mcB
    func_mcB_frame_c, func_mcB_field_c;
  functype_mc_nv12
    func_mc_frame_nv12_c, func_mc_field_nv12_c;
  functype_mcB_nv12
    func_mcB_frame_nv12_c, func_mcB_field_nv12_c;

  // Internal var's
  Ipp32s BlkWidth_c;
  Ipp32s BlkHeight_c;
  Ipp32s chroma_fld_flag;

  mfxU32 m_LastFrameNumber;

  std::queue<FrameUserData>  frUserData;
  mfxStatus InsertUserData(mfxBitstream *bs);





#ifdef MPEG2_ENCODE_DEBUG_HW
  friend class MPEG2EncodeDebug_HW;
  public: MPEG2EncodeDebug_HW *mpeg2_debug_PAK;
#endif //MPEG2_ENCODE_DEBUG_HW

};

#ifdef MPEG2_PAK_THREADED
class ThreadedMPEG2PAK
{
public:
    typedef struct
    {
      mfxBitstream sBitstream;
      mfxI32       nStartSlice;
      mfxI32       nSlices;
      vm_event     taskReadyEvent;
      mfxStatus    returnStatus;
      vm_event     startTaskEvent;
   }sTaskInfo;        
 
    MFXVideoPAKMPEG2    *m_pMPEG2PAK;
    mfxFrameCUC         *m_pCUC;
    MFXVideoPAKMPEG2::MPEG2FrameState     *m_pState;

protected:
    sTaskInfo           *m_pTasks;
    mfxI32              m_nMaxTasks;
    mfxI32              m_nTasks;
    mfxI32              m_curTask;
    mfxI32              m_curTasksBuffer;
    mfxStatus           m_curStatus;
    mfxI32              m_nThreads;
    mfxU8**             m_pTastksBuffers;
    mfxI32              m_TasksBuffersLen;
    vm_mutex            m_mGuard;
    VideoCORE *            m_pCore;
    void *              m_pEncoder;
public:

    inline void SetEncoderPointer (void *pEncoder)
    {
        m_pEncoder = pEncoder;        
    }

    ThreadedMPEG2PAK(mfxI32 nSlices, mfxI32 nThreads, MFXVideoPAKMPEG2* pPAKMPEG2, mfxI32 nMBsInSlice,VideoCORE *pCore);
    ~ThreadedMPEG2PAK();
    mfxStatus  GetNextTask(sTaskInfo** p)
    {
        vm_mutex_lock(&m_mGuard);
        if (m_curTask < m_nTasks)
        {
            mfxStatus sts = MFX_ERR_NONE;
            if (VM_TIMEOUT != vm_event_timed_wait (&m_pTasks[m_curTask].startTaskEvent,0))
            {
                *p = &m_pTasks[m_curTask];
                m_curTask ++;
            }
            else
            {
                sts = MFX_ERR_MORE_DATA;
            }

            vm_mutex_unlock(&m_mGuard);
            return sts;
        }
        vm_mutex_unlock(&m_mGuard);
        return MFX_ERR_NOT_FOUND; 
    }    
    mfxStatus   EncodeFrame (mfxFrameCUC* pCUC, MFXVideoPAKMPEG2::MPEG2FrameState *pState);
    mfxStatus   PAKSlice();
    inline      mfxU32      GetNumOfThreads ()
    {
        return m_nThreads;
    
    }

protected:
    void PrepareNewFrame(mfxFrameCUC* pCUC, MFXVideoPAKMPEG2::MPEG2FrameState *pState);
    mfxStatus CopyTaskBuffers ();

private:
    // Declare private copy constructor to avoid accidental assignment
    // and klocwork complaining.
    ThreadedMPEG2PAK(const ThreadedMPEG2PAK &);
    ThreadedMPEG2PAK & operator = (const ThreadedMPEG2PAK &);
};
#endif

#endif // MFX_ENABLE_MPEG2_VIDEO_PAK
#endif //_MFX_MPEG2_PAK_H_
