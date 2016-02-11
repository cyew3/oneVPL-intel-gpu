/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.
//
//
//          MPEG2 encoder
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_PAK)
#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "mfx_mpeg2_pak.h"
#include "mfx_mpeg2_pak_tabs.h"
#include "ippvc.h"
#include "ippi.h"
#include "mfx_enc_common.h"
#include "mfx_mpeg2_enc_common_hw.h"
#include "mfx_utils.h"
#include "vm_sys_info.h"

#ifdef MPEG2_PAK_THREADED

mfxStatus ThreadedMPEG2PAK::PAKSlice()
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_PRIVATE, "MPEG2_PAK");
    ThreadedMPEG2PAK::sTaskInfo *p = 0;
    mfxStatus sts = MFX_ERR_NONE;
    if ((sts = GetNextTask(&p))== MFX_ERR_NONE)
    {
        mfxStatus PAK_sts = MFX_ERR_NONE;
        for (mfxI32 i = p->nStartSlice; i < p->nStartSlice + p->nSlices; i++)
        {
            PAK_sts = m_pMPEG2PAK->RunSlicePAK(m_pCUC,i, &p->sBitstream,m_pState);
            if (PAK_sts != MFX_ERR_NONE)
            {
                break;
            }
        }
        p->returnStatus = PAK_sts;
        vm_event_signal(&p->taskReadyEvent);       
    }
    return sts;
}
#define N_SLICES_IN_TASK 2
ThreadedMPEG2PAK::ThreadedMPEG2PAK(mfxI32 nSlices, mfxI32 nThreads, MFXVideoPAKMPEG2* pPAKMPEG2, mfxI32 nMBsInSlice, VideoCORE *pCore)
{
    nThreads    = (nThreads>0)? nThreads: vm_sys_info_get_cpu_num();
    m_pCore        = pCore ;

    m_nThreads = (nThreads - 1);
    m_curTask  = 0;
    m_curTasksBuffer = 0;
    m_curStatus = MFX_ERR_NONE; 
    m_nTasks = 0;
    m_pMPEG2PAK = pPAKMPEG2;
    vm_mutex_set_invalid(&m_mGuard);
    vm_mutex_init(&m_mGuard);
    m_pCUC = 0;
    m_pState = 0;

    if (m_nThreads > 0)
    {
        m_nMaxTasks = (nSlices + N_SLICES_IN_TASK - 1)/N_SLICES_IN_TASK;
        m_pTasks = new sTaskInfo [m_nMaxTasks];
        m_TasksBuffersLen = nMBsInSlice*400*2*N_SLICES_IN_TASK;
        
        if (m_nMaxTasks > 0)
           m_pTastksBuffers = new mfxU8* [m_nMaxTasks] ;
        else
            m_pTastksBuffers = 0;


        for (mfxI32 i=0; i<m_nMaxTasks; i++)
        {
            memset (&m_pTasks[i],0,sizeof(sTaskInfo));
            if (i != 0)
            {
                m_pTastksBuffers[i] = new mfxU8 [m_TasksBuffersLen];            
            }
            else
            {
                m_pTastksBuffers[i] = 0;
            }
            m_pTasks[i].returnStatus = MFX_ERR_NONE;
            vm_event_set_invalid(&m_pTasks[i].taskReadyEvent);
            vm_event_init(&m_pTasks[i].taskReadyEvent, 0, 0); 
            vm_event_set_invalid(&m_pTasks[i].startTaskEvent);
            vm_event_init(&m_pTasks[i].startTaskEvent, 0, 0); 
        }
    }
    else
    {
        m_nMaxTasks   = 1;
        m_nThreads    = 0;
        m_pTastksBuffers = 0;
        m_TasksBuffersLen = 0;


        m_pTasks = new sTaskInfo [m_nMaxTasks];
        memset (&m_pTasks[0],0,sizeof(sTaskInfo));

        m_pTasks[0].returnStatus = MFX_ERR_NONE;
        vm_event_set_invalid(&m_pTasks[0].taskReadyEvent);
        vm_event_init(&m_pTasks[0].taskReadyEvent, 0, 0);             
        vm_event_set_invalid(&m_pTasks[0].startTaskEvent);
        vm_event_init(&m_pTasks[0].startTaskEvent, 0, 0);                  
   }
}

ThreadedMPEG2PAK::~ThreadedMPEG2PAK()
{   
    for (mfxI32 i=0; i<m_nMaxTasks; i++)
    {
        if (m_pTastksBuffers && m_pTastksBuffers[i])
            delete [] m_pTastksBuffers[i];
        vm_event_destroy(&m_pTasks[i].taskReadyEvent); 
        vm_event_destroy(&m_pTasks[i].startTaskEvent); 

    }
    if (m_pTastksBuffers)
        delete [] m_pTastksBuffers;

    if (m_pTasks)
        delete [] m_pTasks;


    vm_mutex_destroy(&m_mGuard);    
}
    
void ThreadedMPEG2PAK::PrepareNewFrame(mfxFrameCUC* pCUC, MFXVideoPAKMPEG2::MPEG2FrameState *pState)
{
    vm_mutex_lock(&m_mGuard);

    m_pCUC = pCUC;
    m_curTask = 0;
    m_curTasksBuffer = 0;
    m_curStatus = MFX_ERR_NONE;
    m_pState = pState;
    m_nTasks = (pCUC->NumSlice + N_SLICES_IN_TASK - 1)/N_SLICES_IN_TASK;
    m_nTasks = (m_nTasks <= m_nMaxTasks)? m_nTasks : m_nMaxTasks;

    for (mfxI32 i = 0; i < m_nTasks; i++)
    {
        sTaskInfo *pTask = &m_pTasks[i];
        pTask->nStartSlice = N_SLICES_IN_TASK*i;
        m_pTasks[i].nSlices = (i == m_nTasks - 1) ? pCUC->NumSlice - i*N_SLICES_IN_TASK : N_SLICES_IN_TASK;

        pTask->returnStatus = MFX_ERR_NONE;            
        vm_event_reset(&pTask->taskReadyEvent);
        vm_event_reset(&pTask->startTaskEvent);

        if (i == 0)
        {
            pTask->sBitstream.Data = pCUC->Bitstream->Data + pCUC->Bitstream->DataLength;
            pTask->sBitstream.MaxLength = pCUC->Bitstream->MaxLength - pCUC->Bitstream->DataLength;;
        }
        else
        {
            pTask->sBitstream.Data = m_pTastksBuffers[i];
            pTask->sBitstream.MaxLength = m_TasksBuffersLen;            
        }
        
        pTask->sBitstream.DataLength = 0;
        pTask->sBitstream.DataOffset = 0;
        
    }
    vm_mutex_unlock(&m_mGuard);
     
}
mfxStatus ThreadedMPEG2PAK::CopyTaskBuffers ()
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "MPEG2Enc_copy_output");

    for (mfxI32 i = m_curTasksBuffer; i < m_nTasks; i++)
    {
        sTaskInfo *pTask = &m_pTasks[i];
        vm_status st = vm_event_timed_wait(&pTask->taskReadyEvent, 0);

        if (st == VM_TIMEOUT)
        {
            m_curTasksBuffer = i;
            return MFX_ERR_MORE_DATA;
        }
        else if (st == VM_OK)
        {
            if (pTask->returnStatus!= MFX_ERR_NONE )
            {
                m_curStatus = pTask->returnStatus;
            }
            else if (m_curStatus == MFX_ERR_NONE)
            {
                if (i > 0)
                {
                    if (m_pCUC->Bitstream->DataLength + pTask->sBitstream.DataLength >= m_pCUC->Bitstream->MaxLength)
                    {
                       m_curStatus = MFX_ERR_NOT_ENOUGH_BUFFER;
                    }
                    else
                    {
                        memcpy_s (m_pCUC->Bitstream->Data + m_pCUC->Bitstream->DataLength, m_pCUC->Bitstream->MaxLength, pTask->sBitstream.Data,pTask->sBitstream.DataLength);
                    }
                } 
                m_pCUC->Bitstream->DataLength += pTask->sBitstream.DataLength;
            }            
        }
        else
        {
            return MFX_ERR_UNKNOWN;
        }        
    } 
    return m_curStatus;
}
mfxStatus ThreadedMPEG2PAK::EncodeFrame (mfxFrameCUC* pCUC, MFXVideoPAKMPEG2::MPEG2FrameState  *pState)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_PRIVATE, "MPEG2_PAK_main");
    mfxStatus sts = MFX_ERR_NONE;
    PrepareNewFrame(pCUC, pState);
    for (mfxI32 i = 0; i < m_nTasks; i ++)
    {
        vm_event_signal (&m_pTasks[i].startTaskEvent);
    }
    
    m_pCore->INeedMoreThreadsInside(m_pEncoder);
    while((sts = CopyTaskBuffers ()) == MFX_ERR_MORE_DATA)
    {
        sTaskInfo* p = 0;
        sts = GetNextTask(&p);
        if (sts == MFX_ERR_NONE)
        {
            for (mfxI32 i = p->nStartSlice; i < p->nStartSlice + p->nSlices; i++)
            {
                sts = m_pMPEG2PAK->RunSlicePAK(m_pCUC,i, &p->sBitstream, pState);
                MFX_CHECK_STS (sts);
            } // for

            vm_event_signal(&p->taskReadyEvent);
        }        
    } //while
    return sts; 
}
#undef N_SLICES_IN_TASK
#endif //MPEG2_PAK_THREADED

#ifdef MPEG2_ENCODE_DEBUG_HW
class MPEG2EncodeDebug_HW
{
public:
  void GatherInterRefMBlocksData(int k,void *vector_in,void* state,void *mbinfo);
  void GatherBlockData(int k,int blk,int picture_coding_type,int quantiser_scale_value,Ipp16s *quantMatrix,Ipp16s *pMBlock,int Count,int intra_flag,int intra_dc_shift);
  void SetSkippedMb(int k);
  void SetNoMVMb(int k);
};
#endif //MPEG2_ENCODE_DEBUG_HW

// temporary defines
#define CHECK_VERSION(ver) /*ver.Major==0;*/
#define CHECK_CODEC_ID(id, myid) /*id==myid;*/
//////////////////////////////////////

typedef mfxI16 DctCoef;
#define BLOCK_DATA(bl) (pcoeff+(bl)*64)

#define MB_SKIPPED(pmb) (pmb->Skip8x8Flag!=0)
#define MB_MV_COUNT(mbi) (mbi.prediction_type == MC_FRAME?1:2)
#define MB_HAS_FSEL(pmb) pmb->FieldMbFlag //?? can be incorrect for IP frame
#define MB_FSEL(pmb,dir,i) ((mfxMbCodeAVC*)pmb)->RefPicSelect[dir][i] // tmp cast
#define MB_MV(pmb,dir,num) (pmb)->MV[(dir)*4+(num)]
#define MB_COMP_CBP(pmb, cbp) { \
  mfxI32 bb; \
  for(bb=0, cbp=0; bb<4; bb++) \
    if(pmb->CodedPattern4x4Y & (0xf<<bb*4)) \
      cbp |= 1 << bb; \
  for( ; bb<m_block_count; bb+=2) { \
    cbp <<= 2; \
    if(pmb->CodedPattern4x4U & (0xf<<((m_block_count-2-bb)>>1)*4)) \
      cbp |= 2; \
    if(pmb->CodedPattern4x4V & (0xf<<((m_block_count-2-bb)>>1)*4)) \
      cbp |= 1; \
  } \
}

static mfxStatus TranslateMfxMbTypeMPEG2(mfxMbCodeMPEG2* pmb, MFXVideoPAKMPEG2::MBInfo* mbi) {
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

MFXVideoPAKMPEG2::MFXVideoPAKMPEG2(VideoCORE *core, mfxStatus *sts) : VideoPAK() {
  m_core = core;
  m_DC_Tbl[0] = mfx_mpeg2_Y_DC_Tbl;
  m_DC_Tbl[1] = mfx_mpeg2_Cr_DC_Tbl;
  m_DC_Tbl[2] = mfx_mpeg2_Cr_DC_Tbl;
  ippiCreateRLEncodeTable(mfx_mpeg2_Table15, &vlcTableB15);
  ippiCreateRLEncodeTable(mfx_mpeg2_dct_coeff_next_RL, &vlcTableB5c_e);
  m_frame.CucId = 0;
  m_slice.CucId = 0;

#ifdef MPEG2_PAK_THREADED
  pThreadedMPEG2PAK = 0;
#endif

  *sts = (core ? MFX_ERR_NONE : MFX_ERR_NULL_PTR);
}
MFXVideoPAKMPEG2::~MFXVideoPAKMPEG2() {
  ippiHuffmanTableFree_32s(vlcTableB15);
  ippiHuffmanTableFree_32s(vlcTableB5c_e);
}


mfxStatus MFXVideoPAKMPEG2::Query(mfxVideoParam *in, mfxVideoParam *out)
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
    out->mfx.FrameInfo.FrameRateExtN = 1;
    out->mfx.FrameInfo.FrameRateExtD = 1;
    out->mfx.FrameInfo.AspectRatioW = 1;
    out->mfx.FrameInfo.AspectRatioH = 1;
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

    if (out->mfx.FrameInfo.Width > 0x1fff)
      out->mfx.FrameInfo.Width = 0x1fff;
    if (out->mfx.FrameInfo.CropW == 0 ||
        (out->mfx.FrameInfo.CropW > out->mfx.FrameInfo.Width && out->mfx.FrameInfo.Width > 0))
      out->mfx.FrameInfo.CropW = out->mfx.FrameInfo.Width;
    out->mfx.FrameInfo.Width = (out->mfx.FrameInfo.Width + 15) & ~15;

    if (out->mfx.FrameInfo.Height > 0x1fff)
      out->mfx.FrameInfo.Height = 0x1fff;
    if (out->mfx.FrameInfo.CropH == 0 ||
        (out->mfx.FrameInfo.CropH > out->mfx.FrameInfo.Height && out->mfx.FrameInfo.Height > 0))
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

mfxStatus MFXVideoPAKMPEG2::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
  MFX_CHECK_NULL_PTR1(par)
  MFX_CHECK_NULL_PTR1(request)
  CHECK_VERSION(par->Version);
  CHECK_CODEC_ID(par->mfx.CodecId, MFX_CODEC_MPEG2);

  if (par->mfx.FrameInfo.Width > 0x1fff || (par->mfx.FrameInfo.Width & 0x0f) != 0)
  {
    return MFX_ERR_UNSUPPORTED;
  }
  mfxU32 mask = (/*par->mfx.FramePicture&&*/ (par->mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE))? 0x0f:0x1f;

  if (par->mfx.FrameInfo.Height > 0x1fff || (par->mfx.FrameInfo.Height & mask) != 0 )
  {
    return MFX_ERR_UNSUPPORTED;
  }

  request->Info              = par->mfx.FrameInfo;
  request->NumFrameMin       = 4; // 2 refs, 1 input, 1 for async
  request->NumFrameSuggested = 4;

  request->Type              = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME;

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKMPEG2::Init(mfxVideoParam *par)
{
  mfxStatus sts = MFX_ERR_NONE;

  m_cuc = 0;
  m_frame.CucId = 0;
  m_slice.CucId = 0;

  sts =  Reset(par);
#ifdef MPEG2_PAK_THREADED
  pThreadedMPEG2PAK = new ThreadedMPEG2PAK(par->mfx.NumSlice, par->mfx.NumThread, this, m_info.FrameInfo.Width >> 4,m_core);
#endif
  return sts;
}

//virtual mfxStatus Reset(mfxVideoParam *par);
mfxStatus MFXVideoPAKMPEG2::Reset(mfxVideoParam *par)
{
  mfxI32 i;
  MFX_CHECK_NULL_PTR1(par)
  CHECK_VERSION(par->Version);
  CHECK_CODEC_ID(par->mfx.CodecId, MFX_CODEC_MPEG2);

  if (par->mfx.FrameInfo.CropX!=0 || par->mfx.FrameInfo.CropY!=0 ||
      par->mfx.FrameInfo.Width > 0x1fff || par->mfx.FrameInfo.Height > 0x1fff ||
      par->mfx.FrameInfo.CropW > par->mfx.FrameInfo.Width ||
      par->mfx.FrameInfo.CropH > par->mfx.FrameInfo.Height)
    return MFX_ERR_UNSUPPORTED;

  m_info = par->mfx;

  if (mfxExtVideoSignalInfo * vsi = GetExtVideoSignalInfo(par->ExtParam, par->NumExtParam))
  {
    CheckExtVideoSignalInfo(vsi);
    m_vsi = *vsi;
  }
  else
  {
    memset(&m_vsi, 0, sizeof m_vsi);
  }

  // chroma format dependent
  switch (m_info.FrameInfo.ChromaFormat-0) {
    default:
      m_info.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420+0;
    case MFX_CHROMAFORMAT_YUV420:
      m_block_count = 6;
      if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
#ifdef IPP_NV12    
        func_getdiff_frame_nv12_c = ippiGetDiff8x8_8u16s_C2P2; // NV12 func!!!
        func_getdiff_field_nv12_c = ippiGetDiff8x4_8u16s_C2P2; // NV12 func!!!
        func_getdiffB_frame_nv12_c = ippiGetDiff8x8B_8u16s_C2P2; // NV12 func!!!
        func_getdiffB_field_nv12_c = ippiGetDiff8x4B_8u16s_C2P2; // NV12 func!!!
        func_mc_frame_nv12_c = ippiMC8x8_16s8u_P2C2R;// NV12 func!!!
        func_mc_field_nv12_c = ippiMC8x4_16s8u_P2C2R;// NV12 func!!!
        func_mcB_frame_nv12_c = ippiMC8x8B_16s8u_P2C2R; // NV12 func!!!
        func_mcB_field_nv12_c = ippiMC8x4B_16s8u_P2C2R; // NV12 func!!!
#else
        func_getdiff_frame_c = tmp_ippiGetDiff8x8_8u16s_C2P2; // NV12 func!!!
        func_getdiff_field_c = tmp_ippiGetDiff8x4_8u16s_C2P2; // NV12 func!!!
        func_getdiffB_frame_c = tmp_ippiGetDiff8x8B_8u16s_C2P2; // NV12 func!!!
        func_getdiffB_field_c = tmp_ippiGetDiff8x4B_8u16s_C2P2; // NV12 func!!!
        func_mc_frame_c = tmp_ippiMC8x8_8u_C2P2; // NV12 func!!!
        func_mc_field_c = tmp_ippiMC8x4_8u_C2P2; // NV12 func!!!
        func_mcB_frame_c = tmp_ippiMC8x8B_8u_C2P2; // NV12 func!!!
        func_mcB_field_c = tmp_ippiMC8x4B_8u_C2P2; // NV12 func!!!
#endif
        
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
  src_block_offset_frm[0]  = out_block_offset_frm[0]  = 0;
  src_block_offset_frm[1]  = out_block_offset_frm[1]  = 8;
  src_block_offset_frm[4]  = out_block_offset_frm[4]  = 0;
  src_block_offset_frm[5]  = out_block_offset_frm[5]  = 0;
  src_block_offset_frm[8]  = out_block_offset_frm[8]  = 8;
  src_block_offset_frm[9]  = out_block_offset_frm[9]  = 8;

  src_block_offset_fld[0]  = out_block_offset_fld[0]  = 0;
  src_block_offset_fld[1]  = out_block_offset_fld[1]  = 8;
  src_block_offset_fld[4]  = out_block_offset_fld[4]  = 0;
  src_block_offset_fld[5]  = out_block_offset_fld[5]  = 0;
  src_block_offset_fld[8]  = out_block_offset_fld[8]  = 8;
  src_block_offset_fld[9]  = out_block_offset_fld[9]  = 8;

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

  framerate = CalculateUMCFramerate(m_info.FrameInfo.FrameRateExtN, m_info.FrameInfo.FrameRateExtD);
  if (framerate <= 0)
    return MFX_ERR_UNSUPPORTED; // lack of return codes;

  m_LastFrameNumber = 0;

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

  while(!frUserData.empty())
  {
    frUserData.pop();
  }
  
  return MFX_ERR_NONE;
}

//virtual mfxStatus Close(void); // same name
mfxStatus MFXVideoPAKMPEG2::Close(void)
{
#ifdef MPEG2_PAK_THREADED
   if (pThreadedMPEG2PAK)
   {
    delete pThreadedMPEG2PAK;
    pThreadedMPEG2PAK = 0;
   }
#endif

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKMPEG2::GetVideoParam(mfxVideoParam *par)
{
  MFX_CHECK_NULL_PTR1(par)
  CHECK_VERSION(par->Version);
  par->mfx = m_info;
  par->mfx.CodecId = MFX_CODEC_MPEG2;

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKMPEG2::GetFrameParam(mfxFrameParam *par)
{
  //if (m_frame.CucId == 0)
  //  return MFX_ERR_NOT_INITIALIZED;
  MFX_CHECK_NULL_PTR1(par)

  par->MPEG2 = m_frame;
  return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKMPEG2::GetSliceParam(mfxSliceParam *par)
{
  MFX_CHECK_NULL_PTR1(par)
  //if (m_slice.CucId == 0)
  //  return MFX_ERR_NOT_INITIALIZED;

  par->MPEG2 = m_slice;
  //// set some unused values
  //mfxU8 slicetype;
  //if(picture_coding_type == UMC::MPEG2_B_PICTURE) slicetype = MFX_SLICETYPE_B;
  //else if(picture_coding_type == UMC::MPEG2_P_PICTURE) slicetype = MFX_SLICETYPE_P;
  //else if(picture_coding_type == UMC::MPEG2_I_PICTURE) slicetype = MFX_SLICETYPE_I;
  //else return MFX_ERR_NULL_PTR;
  //par->SliceType = slicetype;
  //par->CucId = MFX_CUC_MPEG2_SLICEPARAM;
  //par->CucSz = sizeof(mfxSliceParam);
  //par->SliceDataOffset = 0; // only dec
  //par->SliceDataSize = 0; // only dec
  //par->BadSliceChopping = MFX_SLICECHOP_NONE;
  //par->SliceHeaderSize = 0; // only dec
  //par->BsInsertionFlag = 0; //?
  //par->QScaleCode = 0; // no info

  return MFX_ERR_NONE;
}

#define SP   5
#define MP   4
#define SNR  3
#define SPAT 2
#define HP   1

#define LL  10
#define ML   8
#define H14  6
#define HL   4

mfxStatus MFXVideoPAKMPEG2::RunSeqHeader(mfxFrameCUC *cuc)
{
  Ipp32s size;
  mfxBitstream *bs;
  bitBuffer bb;
  MFX_CHECK_NULL_PTR1(cuc)
  MFX_CHECK_NULL_PTR1(cuc->Bitstream)
  CHECK_VERSION(cuc->Version);
  mfxU32  h = (m_info.FrameInfo.CropH!=0)? m_info.FrameInfo.CropH:m_info.FrameInfo.Height;
  mfxU32  w = (m_info.FrameInfo.CropW!=0)? m_info.FrameInfo.CropW:m_info.FrameInfo.Width;
  mfxI32 fr_code, fr_codeN, fr_codeD;
  mfxU32 aw = (m_info.FrameInfo.AspectRatioW != 0) ? m_info.FrameInfo.AspectRatioW * w :w;
  mfxU32 ah = (m_info.FrameInfo.AspectRatioH != 0) ? m_info.FrameInfo.AspectRatioH * h :h;
  
  mfxU16 ar_code = GetAspectRatioCode (aw, ah);

  ConvertFrameRateMPEG2(m_info.FrameInfo.FrameRateExtD, m_info.FrameInfo.FrameRateExtN, fr_code, fr_codeN, fr_codeD);
  
  m_cuc = cuc;
  bs = cuc->Bitstream;

  SET_BUFFER(bb, bs);
  MFX_CHECK_NULL_PTR1(bs->Data);

  try
  {
      PUT_START_CODE(SEQ_START_CODE);                  // sequence_header_code
      PUT_BITS((w & 0xfff), 12);                       // horizontal_size_value
      PUT_BITS((h & 0xfff), 12);                       // vertical_size_value
      PUT_BITS(ar_code, 4);                            // aspect_ratio_information
      PUT_BITS(fr_code, 4);                            // frame_rate_code
      if (m_info.RateControlMethod != MFX_RATECONTROL_CQP)
      {
        PUT_BITS((((m_info.TargetKbps * 5 + 1)>>1) & 0x3ffff), 18); // bit_rate_value
        PUT_BITS(1, 1);
        PUT_BITS((m_info.BufferSizeInKB>>1 & 0x3ff), 10); // vbv_buffer_size_value

      }
      else
      {
        PUT_BITS(0, 18); // bit_rate_value
        PUT_BITS(1, 1);
        PUT_BITS(0, 10); // vbv_buffer_size_value

      }
      PUT_BITS(0, 1);                                  // constrained_parameters_flag
      putQMatrices(0, &bb);

      //SequenceExtension
      Ipp32s prog_seq = (m_info.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 0;
      Ipp32s chroma_format_code;
      switch(m_info.FrameInfo.ChromaFormat-0) {
          case MFX_CHROMAFORMAT_YUV420: chroma_format_code = 1; break;
          case MFX_CHROMAFORMAT_YUV422: chroma_format_code = 2; break;
          case MFX_CHROMAFORMAT_YUV444: chroma_format_code = 3; break;
          default:     chroma_format_code = 1;
      } 
      PUT_START_CODE(EXT_START_CODE);               // extension_start_code
      PUT_BITS(SEQ_ID, 4);  
      // extension_start_code_identifier
      // Really profile-level enums must be shifted opposite way.
      Ipp32u profile = 0;
      Ipp32u level = 0;

      switch (m_info.CodecProfile)
      {
      case MFX_PROFILE_MPEG2_SIMPLE:
          profile = SP;
          break;
      case MFX_PROFILE_MPEG2_MAIN:
          profile = MP;
          break;
      case MFX_PROFILE_MPEG2_HIGH:
          profile = HP;
          break;
      default:
          profile = MP;
          break;
      }
      switch (m_info.CodecLevel)
      {
      case MFX_LEVEL_MPEG2_LOW:
          level = LL;
          break;
      case MFX_LEVEL_MPEG2_MAIN:
          level = ML;
          break;
      case MFX_LEVEL_MPEG2_HIGH:
          level = HL;
          break;
      case MFX_LEVEL_MPEG2_HIGH1440:
          level = H14;
          break;
      default:
          level = ML;
          break;  
      }
      PUT_BITS(( (profile<<4) | (level) )&0xff, 8); // profile_and_level_indication
      PUT_BITS(prog_seq, 1);                        // progressive sequence
      PUT_BITS(chroma_format_code, 2);              // chroma_format
      PUT_BITS(w >> 12, 2);                         // horizontal_size_extension
      PUT_BITS(h >> 12, 2);                         // vertical_size_extension
      if (m_info.RateControlMethod != MFX_RATECONTROL_CQP)
      {
        PUT_BITS(((m_info.TargetKbps * 5 + 1) >> (1+18)), 12);    // bit_rate_extension
        PUT_BITS(1, 1);                               // marker_bit
        PUT_BITS(m_info.BufferSizeInKB >> 11, 8);   // vbv_buffer_size_extension
      }
      else
      {
        PUT_BITS(0, 12);    // bit_rate_extension
        PUT_BITS(1, 1);                               // marker_bit
        PUT_BITS(0, 8);   // vbv_buffer_size_extension       
      }
      PUT_BITS(0, 1);                               // low_delay  (not implemented)
      PUT_BITS(fr_codeN, 2);
      PUT_BITS(fr_codeD, 5);

      if (m_vsi.Header.BufferId == MFX_EXTBUFF_VIDEO_SIGNAL_INFO)
      {
        //SequenceDisplayExtension
        PUT_START_CODE(EXT_START_CODE);                 // extension_start_code
        PUT_BITS(DISP_ID, 4);                           // extension_start_code_identifier
        PUT_BITS(m_vsi.VideoFormat, 3);                 // video_format
        PUT_BITS(m_vsi.ColourDescriptionPresent, 1);    // colour_description
        if (m_vsi.ColourDescriptionPresent)
        {
          PUT_BITS(m_vsi.ColourPrimaries, 8);           // colour_primaries
          PUT_BITS(m_vsi.TransferCharacteristics, 8);   // transfer_characteristics
          PUT_BITS(m_vsi.MatrixCoefficients, 8);        // matrix_coefficients
        }
        PUT_BITS(w, 14);                                // display_horizontal_size
        PUT_BITS(1, 1);                                 // marker_bit
        PUT_BITS(h, 14);                                // display_vertical_size
      }

      FLUSH_BITSTREAM(bb, bs)
  }
  catch (MPEG2_Exceptions exception)
  {
    return exception.GetType();
  }
  size = BITPOS(bb)>>3;

  bs->DataLength += size;
  //bs->HeaderSize = (mfxU16)(bs->HeaderSize + size);
  //bs->FrameCount = (mfxU16)(m_LastFrameNumber & 0xffff);
  //bs->SliceId = 0xF0;
  //bs->NumSlice = MBcountH; ?
  //bs->DataFlag |= MFX_BITSTREAM_KEY_FRAME;
  //bs->HeaderSize = 0; ?
  //bs->FourCC = MFX_CODEC_MPEG2;

  return MFX_ERR_NONE;
}


//virtual mfxStatus RunGopHeader(mfxBitstream *bs);
mfxStatus MFXVideoPAKMPEG2::RunGopHeader(mfxFrameCUC *cuc)
{
  Ipp32s size;
  mfxBitstream *bs;
  bitBuffer bb;
  mfxI32 tc;
  mfxI32 num;
  MFX_CHECK_NULL_PTR1(cuc)
  MFX_CHECK_NULL_PTR1(cuc->Bitstream)
  MFX_CHECK_NULL_PTR1(cuc->FrameParam)
  CHECK_VERSION(cuc->Version);

  bs = cuc->Bitstream;
  SET_BUFFER(bb, bs);
  MFX_CHECK_NULL_PTR1(bs->Data);

  if(cuc->FrameSurface && cuc->FrameSurface->Data && cuc->FrameSurface->Data[cuc->FrameParam->MPEG2.RecFrameLabel]) {
    num = cuc->FrameSurface->Data[cuc->FrameParam->MPEG2.RecFrameLabel]->FrameOrder;
    m_LastFrameNumber = num;
  } else {
    num = m_LastFrameNumber;
  }
  mfxI32 fps, pict, sec, minute, hour;
  fps = (Ipp32s)(framerate + 0.5);
  pict = num % fps;
  num = (num - pict) / fps;
  sec = num % 60;
  num = (num - sec) / 60;
  minute = num % 60;
  num = (num - minute) / 60;
  hour = num % 24;
  tc = (hour<<19) | (minute<<13) | (1<<12) | (sec<<6) | pict;

  try
  {
    PUT_START_CODE(GOP_START_CODE);
    PUT_BITS(tc, 25);            // time_code
    //closed_gop = (Count && encodeInfo.IPDistance>1)?0:1;
    PUT_BITS(cuc->FrameParam->MPEG2.CloseEntryFlag, 1); // closed_gop (all except first GOP are open)
    PUT_BITS(cuc->FrameParam->MPEG2.BrokenLinkFlag, 1); // broken_link

    FLUSH_BITSTREAM(bb, bs)
    size = BITPOS(bb)>>3;
  }
  catch (MPEG2_Exceptions exception)
  {
     return exception.GetType();
  }


  bs->DataLength += size;
  //bs->HeaderSize = (mfxU16)(bs->HeaderSize + size);
  bs->DataFlag |= 0;
  //bs->HeaderSize = 0;
  //bs->FourCC = MFX_CODEC_MPEG2;

  return MFX_ERR_NONE;
}

//virtual mfxStatus RunPicHeader(mfxBitstream *bs)=0;
mfxStatus MFXVideoPAKMPEG2::RunPicHeader(mfxFrameCUC *cuc)
{
  Ipp32s size;
  mfxBitstream *bs;
  bitBuffer bb;
  MFX_CHECK_NULL_PTR1(cuc)
  MFX_CHECK_NULL_PTR1(cuc->Bitstream)
  MFX_CHECK_NULL_PTR1(cuc->FrameParam)
  CHECK_VERSION(cuc->Version);

  m_cuc = cuc;
  bs = cuc->Bitstream;
  SET_BUFFER(bb, bs);
  MFX_CHECK_NULL_PTR1(bs->Data);

  mfxU32 pict_coding_type, pict_structure;
  mfxU32 fr_pred_fr_dct, actual_tff;
  if(cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_I) pict_coding_type = MPEG2_I_PICTURE; else
  if(cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_P) pict_coding_type = MPEG2_P_PICTURE; else
  if(cuc->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_B) pict_coding_type = MPEG2_B_PICTURE; else
    return MFX_ERR_UNSUPPORTED;
  if(cuc->FrameParam->MPEG2.FieldPicFlag) {
    pict_structure = cuc->FrameParam->MPEG2.BottomFieldFlag ? MPEG2_BOTTOM_FIELD : MPEG2_TOP_FIELD;
    fr_pred_fr_dct = 0;
  } else {
    pict_structure = MPEG2_FRAME_PICTURE;
    fr_pred_fr_dct = cuc->FrameParam->MPEG2.ProgressiveFrame ? 1 : cuc->FrameParam->MPEG2.FrameDCTprediction;
  }
  actual_tff = ((pict_structure == MPEG2_FRAME_PICTURE && !cuc->FrameParam->MPEG2.ProgressiveFrame) ||
    cuc->FrameParam->MPEG2.RepeatFirstField);
  mfxI32 vbv_delay = (mfxI32)cuc->FrameParam->MPEG2.VBVDelay;

  try
  {
      PUT_START_CODE(PICTURE_START_CODE); // picture_start_code
      PUT_BITS((cuc->FrameParam->MPEG2.TemporalReference & 0x3ff), 10);       // temporal_reference
      PUT_BITS(pict_coding_type, 3);      // picture_coding_type

      if (m_info.RateControlMethod != MFX_RATECONTROL_CQP)
      {
        PUT_BITS(vbv_delay, 16);            // vbv_delay TODO!
      }
      else
      {
        PUT_BITS(0, 16);  
      }

      if( pict_coding_type == MPEG2_P_PICTURE || pict_coding_type == MPEG2_B_PICTURE ) {
        PUT_BITS(0, 1);                   // full_pel_forward_vector
        PUT_BITS(7, 3);                   // forward_f_code
      }
      if( pict_coding_type == MPEG2_B_PICTURE ) {
        PUT_BITS(0, 1);                   // full_pel_backward_vector
        PUT_BITS(7, 3);                   // backward_f_code
      }
      PUT_BITS(0, 1);                     // extra_bit_picture

      PUT_START_CODE(EXT_START_CODE);     // extension_start_code
      PUT_BITS(CODING_ID, 4);             // extension_start_code_identifier
      PUT_BITS(cuc->FrameParam->MPEG2.BitStreamFcodes, 16);
      PUT_BITS(cuc->FrameParam->MPEG2.IntraDCprecision, 2); // intra_dc_precision
      PUT_BITS(pict_structure, 2);        // picture_structure
      PUT_BITS(cuc->FrameParam->MPEG2.TopFieldFirst, 1);// top_field_first
      PUT_BITS(fr_pred_fr_dct, 1);        // frame_pred_frame_dct
      PUT_BITS(0, 1);                     // concealment_motion_vectors (not implemented yet)
      PUT_BITS(cuc->FrameParam->MPEG2.QuantScaleType, 1);   // q_scale_type
      PUT_BITS(cuc->FrameParam->MPEG2.IntraVLCformat, 1);   // intra_vlc_format
      PUT_BITS(cuc->FrameParam->MPEG2.AlternateScan, 1);    // alternate_scan
      PUT_BITS(cuc->FrameParam->MPEG2.RepeatFirstField, 1); // repeat_first_field
      PUT_BITS(cuc->FrameParam->MPEG2.Chroma420type, 1);    // chroma_420_type
      PUT_BITS(cuc->FrameParam->MPEG2.ProgressiveFrame, 1); // progressive_frame
      PUT_BITS(0, 1);                     // composite_display_flag

      putQMatrices(1, &bb);

      FLUSH_BITSTREAM(bb, bs)
  }
  catch (MPEG2_Exceptions exception)
  {
     return exception.GetType();
  }

  size = BITPOS(bb)>>3;
  bs->DataLength += size;

  //bs->FrameCount = (mfxU16)(m_LastFrameNumber & 0xffff);
  //bs->SliceId = 0xF0;
  //bs->DataFlag |= MFX_BITSTREAM_PICTURE_HEADER;
  //bs->HeaderSize = 0;
  //bs->FourCC = MFX_CODEC_MPEG2;

  m_LastFrameNumber++;
  return MFX_ERR_NONE;
}


mfxStatus MFXVideoPAKMPEG2::RunSliceHeader(mfxFrameCUC* /*cuc*/) {return MFX_ERR_UNSUPPORTED;}

mfxStatus MFXVideoPAKMPEG2::InsertUserData(mfxU8 *ptr, mfxU32 len, mfxU64 /*ts*/, mfxBitstream * /*bs*/)
{  
    MFX_CHECK_NULL_PTR1(ptr)

    FrameUserData user_data = {ptr, len};
    frUserData.push(user_data);

    return MFX_ERR_NONE;

}
mfxStatus MFXVideoPAKMPEG2::InsertUserData(mfxBitstream *bs)
{
  bool bFirstStartCode = false;
  mfxI32  i = 0;

  while (!frUserData.empty())
  {
      FrameUserData user_data = {0};
      user_data = frUserData.front();

      mfxU8 *ptr =   user_data.m_pUserData;    
      mfxI32 len =   (mfxI32)user_data.m_len;

      frUserData.pop();

      if(len==0) 
          return MFX_ERR_NONE;

      MFX_CHECK_NULL_PTR1(ptr);
      MFX_CHECK_NULL_PTR1(bs);
      MFX_CHECK_NULL_PTR1(bs->Data);

      mfxI32 offset = bs->DataOffset + bs->DataLength;
      mfxI32 avail  = bs->MaxLength - offset;

      MFX_CHECK(avail>len+4, MFX_ERR_NOT_ENOUGH_BUFFER);


      for(i=0; i<len-2; i++) { // stop len if start code happens
        if(!ptr[i] && !ptr[i+1] && ptr[i+2]==1) {
          // start code! - stop right before
          if (i == 0){
              bFirstStartCode = true;      
          }else{
            break;
          }
        }
      }

      if(i>=len-2)
        i = len;

      if(i==0) 
        return MFX_ERR_NONE;
      
      bs->Data[offset + 0] = 0;
      bs->Data[offset + 1] = 0;
      bs->Data[offset + 2] = 1;
      bs->Data[offset + 3] = (mfxU8)(USER_START_CODE & 0xff);

      bs->DataLength += 4;
      offset += 4;

      len = i; 
      if (bFirstStartCode)
      {
          if (!(i > 3 && ptr[3] == 0xb2))
              return MFX_ERR_NONE;
          ptr +=4;
          len = len - 4;
      }
      memcpy_s(bs->Data + offset, avail, ptr, len);
      bs->DataLength += len;
  }
  return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKMPEG2::InsertBytes(mfxU8 *data, mfxU32 len, mfxBitstream *bs)
{
  MFX_CHECK_NULL_PTR1(data)
  MFX_CHECK_NULL_PTR1(bs)
  MFX_CHECK_NULL_PTR1(bs->Data)
  mfxU32 offset = bs->DataOffset + bs->DataLength;
  mfxU32 avail  = bs->MaxLength - offset;
  if(avail < len)
    return MFX_ERR_NOT_ENOUGH_BUFFER;
  memcpy_s(bs->Data + offset, avail, data, len);
  bs->DataLength += len;
  return MFX_ERR_NONE;
}
mfxStatus MFXVideoPAKMPEG2::AlignBytes(mfxU8 pattern, mfxU32 len, mfxBitstream *bs)
{
  MFX_CHECK_NULL_PTR1(bs)
  MFX_CHECK_NULL_PTR1(bs->Data)
  mfxU32 offset = bs->DataOffset + bs->DataLength;
  mfxU32 avail  = bs->MaxLength - offset;
  if(avail < len)
    return MFX_ERR_NOT_ENOUGH_BUFFER;
  memset(bs->Data + offset, pattern, len);
  bs->DataLength += len;
  return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKMPEG2::RunSlicePAK(mfxFrameCUC *cuc)
{
    m_cuc = cuc;
    MPEG2FrameState state;  
    
    mfxStatus sts = preEnc(&state);
    MFX_CHECK_STS(sts);

    sts = RunSlicePAK(cuc,cuc->SliceId,cuc->Bitstream,&state);
    MFX_CHECK_STS(sts);

    postEnc(&state);
    return sts;


}
mfxStatus MFXVideoPAKMPEG2::RunSlicePAK(mfxFrameCUC *cuc, mfxI32 numSlice, mfxBitstream *bs, MPEG2FrameState* pState)
{
  bitBuffer bb;
  mfxStatus sts;
  MFX_CHECK_NULL_PTR1(cuc);
  MFX_CHECK_NULL_PTR1(cuc->FrameParam);
  MFX_CHECK_NULL_PTR1(cuc->Bitstream);
  MFX_CHECK_NULL_PTR1(cuc->ExtBuffer);
  MFX_CHECK_NULL_PTR1(cuc->SliceParam);

  mfxSliceParamMPEG2 *slice = &cuc->SliceParam[numSlice].MPEG2;
  mfxU32 mbstart = slice->FirstMbX + slice->FirstMbY*(cuc->FrameParam->MPEG2.FrameWinMbMinus1+1);
  mfxI32 fcodes[4]; // dir*2+xy
  mfxI32 mvmax[4];
  
  MFX_CHECK_NULL_PTR1(cuc->MbParam);
  MFX_CHECK_NULL_PTR1(cuc->MbParam->Mb);

  m_cuc = cuc;
  sts = loadCUCFrameParams(fcodes, mvmax);
  MFX_CHECK_STS(sts);

  SET_BUFFER(bb, bs);
  MFX_CHECK_NULL_PTR1(bs->Data);

  mfxExtMpeg2Coeffs* coeff = (mfxExtMpeg2Coeffs*)GetExtBuffer( cuc, MFX_CUC_MPEG2_RESCOEFF);
  MFX_CHECK_NULL_PTR2(coeff, coeff->Coeffs);

  sts = sliceCoef(pState, mbstart, slice->NumMb, coeff);
  MFX_CHECK_STS(sts);

  sts = sliceBSP(mbstart, slice->NumMb, fcodes, mvmax, coeff, &bb);
  MFX_CHECK_STS(sts);

  Ipp32s size;
  FLUSH_BITSTREAM(bb, bs)
  size = BITPOS(bb)>>3;
  bs->DataLength += size;

  //cuc->Bitstream->SliceId = 0xF0;
  //cuc->Bitstream->NumSlice ++;
  //cuc->Bitstream->DataFlag |= MFX_BITSTREAM_MBCODE; // or = ??


  m_frame = cuc->FrameParam->MPEG2;
  m_slice = *slice;
  return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKMPEG2::RunSliceBSP(mfxFrameCUC *cuc)
{
  bitBuffer bb;
  mfxStatus sts;
  MFX_CHECK_NULL_PTR1(cuc);
  MFX_CHECK_NULL_PTR1(cuc->FrameParam);
  MFX_CHECK_NULL_PTR1(cuc->Bitstream);
  MFX_CHECK_NULL_PTR1(cuc->ExtBuffer);
  MFX_CHECK_NULL_PTR1(cuc->SliceParam);

  mfxSliceParamMPEG2 *slice = &cuc->SliceParam[cuc->SliceId].MPEG2;
  mfxU32 mbstart = slice->FirstMbX + slice->FirstMbY*(cuc->FrameParam->MPEG2.FrameWinMbMinus1+1);
  mfxI32 fcodes[4]; // dir*2+xy
  mfxI32 mvmax[4];
  MFX_CHECK_NULL_PTR1(cuc->MbParam);
  MFX_CHECK_NULL_PTR1(cuc->MbParam->Mb);

  m_cuc = cuc;
  sts = loadCUCFrameParams(fcodes, mvmax);
  MFX_CHECK_STS(sts);

  mfxBitstream *bs = cuc->Bitstream;
  SET_BUFFER(bb, bs);
  MFX_CHECK_NULL_PTR1(bs->Data);

  mfxExtMpeg2Coeffs* coeff = (mfxExtMpeg2Coeffs*)GetExtBuffer( cuc, MFX_CUC_MPEG2_RESCOEFF);
  MFX_CHECK_NULL_PTR2(coeff, coeff->Coeffs);

  sts = sliceBSP(mbstart, slice->NumMb, fcodes, mvmax, coeff, &bb);
  MFX_CHECK_STS(sts);

  Ipp32s size;
  FLUSH_BITSTREAM(bb, bs)
  size = BITPOS(bb)>>3;
  slice->SliceDataOffset = bs->DataOffset + bs->DataLength;
  slice->SliceDataSize = size;
  cuc->Bitstream->DataLength += size;

  //cuc->Bitstream->SliceId = 0xF0;
  //cuc->Bitstream->NumSlice ++;
  //cuc->Bitstream->DataFlag |= MFX_BITSTREAM_MBCODE; // or = ??


  m_frame = cuc->FrameParam->MPEG2;
  m_slice = *slice;
  return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKMPEG2::RunFramePAK(mfxFrameCUC *cuc)
{
  bitBuffer bb;
  mfxStatus sts;
  MFX_CHECK_NULL_PTR1(cuc);
  MFX_CHECK_NULL_PTR1(cuc->FrameParam);
  MFX_CHECK_NULL_PTR1(cuc->Bitstream);
  MFX_CHECK_NULL_PTR1(cuc->ExtBuffer);
  mfxI32 fcodes[4]; // dir*2+xy
  mfxI32 mvmax[4];
  MPEG2FrameState state;
  MFX_CHECK_NULL_PTR1(cuc->MbParam);
  MFX_CHECK_NULL_PTR1(cuc->MbParam->Mb);

  m_cuc = cuc;
  sts = loadCUCFrameParams(fcodes, mvmax);
  MFX_CHECK_STS(sts);

  if(picture_coding_type == MPEG2_I_PICTURE && !cuc->FrameParam->MPEG2.SecondFieldFlag)
  {
    sts = RunGopHeader(cuc);
    MFX_CHECK_STS(sts);
  }
  sts = RunPicHeader(cuc);
  MFX_CHECK_STS(sts);

  mfxBitstream *bs = cuc->Bitstream;
  //putQMatrices(1, &bb);

  sts = InsertUserData(bs);
  MFX_CHECK_STS(sts);
  
  SET_BUFFER(bb, bs);

  mfxExtMpeg2Coeffs* coeff = (mfxExtMpeg2Coeffs*)GetExtBuffer( cuc, MFX_CUC_MPEG2_RESCOEFF);
  MFX_CHECK_NULL_PTR2(coeff, coeff->Coeffs);

  sts = preEnc(&state);
  MFX_CHECK_STS(sts);

#ifndef MPEG2_PAK_THREADED
  mfxI32 i;


  mfxI32 numSlices = cuc->NumSlice;
  for(i=0; i<numSlices; i++) {
    mfxSliceParamMPEG2 *slice = &cuc->SliceParam[i].MPEG2;
    mfxU32 mbstart = slice->FirstMbX + slice->FirstMbY*(cuc->FrameParam->MPEG2.FrameWinMbMinus1+1);
    sts = sliceCoef(&state, mbstart, slice->NumMb, coeff);
    MFX_CHECK_STS(sts);
    sts = sliceBSP(mbstart, slice->NumMb, fcodes, mvmax, coeff, &bb);
    MFX_CHECK_STS(sts);
  }

#else
  sts = pThreadedMPEG2PAK->EncodeFrame (cuc, &state);
  MFX_CHECK_STS(sts);
#endif 
 
  postEnc(&state);

  Ipp32s size;
  FLUSH_BITSTREAM(bb, bs)
  size = BITPOS(bb)>>3;
  cuc->Bitstream->DataLength += size;

  //cuc->Bitstream->SliceId = 0xF0;
  //cuc->Bitstream->NumSlice = (mfxU8)numSlices;
  //cuc->Bitstream->DataFlag |= MFX_BITSTREAM_MBCODE; // or = ??


  m_frame = cuc->FrameParam->MPEG2;
  return MFX_ERR_NONE;
}

//virtual mfxStatus RunFrameBSP(mfxFrameCUC *cuc);
mfxStatus MFXVideoPAKMPEG2::RunFrameBSP(mfxFrameCUC *cuc)
{
  bitBuffer bb;
  mfxStatus sts;
  mfxI32 i;
  MFX_CHECK_NULL_PTR1(cuc);
  MFX_CHECK_NULL_PTR1(cuc->FrameParam);
  MFX_CHECK_NULL_PTR1(cuc->Bitstream);
  MFX_CHECK_NULL_PTR1(cuc->ExtBuffer);
  mfxI32 fcodes[4]; // dir*2+xy
  mfxI32 mvmax[4];
  MFX_CHECK_NULL_PTR1(cuc->MbParam);
  MFX_CHECK_NULL_PTR1(cuc->MbParam->Mb);

  m_cuc = cuc;
  sts = loadCUCFrameParams(fcodes, mvmax);
  MFX_CHECK_STS(sts);

  if(picture_coding_type == MPEG2_I_PICTURE && !cuc->FrameParam->MPEG2.SecondFieldFlag)
    RunGopHeader(cuc);
  RunPicHeader(cuc);
  mfxBitstream *bs = cuc->Bitstream;
  
  sts = InsertUserData(bs);
  MFX_CHECK_STS(sts);
  //putQMatrices(1, &bb);
  SET_BUFFER(bb, bs);

  mfxExtMpeg2Coeffs* coeff = (mfxExtMpeg2Coeffs*) GetExtBuffer(cuc, MFX_CUC_MPEG2_RESCOEFF);
  MFX_CHECK_NULL_PTR2(coeff, coeff->Coeffs);

  mfxI32 numSlices = cuc->NumSlice;
  for(i=0; i<numSlices; i++) {
    mfxSliceParamMPEG2 *slice = &cuc->SliceParam[i].MPEG2;
    mfxU32 mbstart = slice->FirstMbX + slice->FirstMbY*(cuc->FrameParam->MPEG2.FrameWinMbMinus1+1);
    sts = sliceBSP(mbstart, slice->NumMb, fcodes, mvmax, coeff, &bb);
    MFX_CHECK_STS(sts);
  }

  Ipp32s size;
  FLUSH_BITSTREAM(bb, bs)
  size = BITPOS(bb)>>3;
  cuc->Bitstream->DataLength += size;

  //cuc->Bitstream->SliceId = 0xF0;
  //cuc->Bitstream->NumSlice = (mfxU8)numSlices;
  //cuc->Bitstream->DataFlag |= MFX_BITSTREAM_MBCODE; // or = ??


  m_frame = cuc->FrameParam->MPEG2;
  return MFX_ERR_NONE;
}



// privates:
mfxStatus MFXVideoPAKMPEG2::loadCUCFrameParams(
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
    m_MotionData.f_code[i>>1][i&1] = fcodes[i];
    if(fcodes[i] == 15) continue;
    mvmax[i] = 8<<fcodes[i];
  }
  return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKMPEG2::sliceBSP(mfxU32 mbstart, mfxU32 mbcount,
  mfxI32* fcodes, // dir*2+xy
  mfxI32* mvmax,
  mfxExtMpeg2Coeffs* coeff,
  bitBuffer* outbb
)
{
  Ipp32s dc_dct_pred[3];
  mfxI32 PMV[4][2];
  Ipp32s EOBLen, EOBCode;
  IppVCHuffmanSpec_32s* AC_Tbl;
  mfxI32 mb_addr_incr = 1;
  mfxI32 curquant = -1;
  bitBuffer bb = *outbb;
  MBInfo mbi;
  mfxU32 bl, i, dir;
  mfxI32 pct_idx = picture_coding_type - MPEG2_I_PICTURE;

  MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "MPEG2Enc_sliceBSP");
  try
  {
      if (m_cuc->FrameParam->MPEG2.IntraVLCformat) {
        EOBCode = 6;
        EOBLen  = 4; // (Table B-15)
        AC_Tbl = vlcTableB15;
      } else {
        EOBCode = 2;
        EOBLen  = 2; // (Table B-14)
        AC_Tbl = vlcTableB5c_e;
      }
      DctCoef *pcoeff = (DctCoef *)coeff->Coeffs;
      pcoeff += mbstart * 64*m_block_count;
      // put slice header
      mfxMbCodeMPEG2 *Mb = &m_cuc->MbParam->Mb[mbstart].MPEG2;
      if( m_cuc->FrameParam->MPEG2.FrameHinMbMinus1 < 2800/16 )
      {
        PUT_START_CODE(SLICE_MIN_START + Mb->MbYcnt); // slice_start_code
      }
      else
      {
        PUT_START_CODE(SLICE_MIN_START + (Mb->MbYcnt & 127)); // slice_start_code
        PUT_BITS(Mb->MbYcnt >> 7, 3); // slice_vertical_position_extension
      }
      PUT_BITS((Mb->QpScaleCode<<1), (5+1)); // + extra_bit_slice
      mb_addr_incr = Mb->MbXcnt + 1;
      curquant = Mb->QpScaleCode;
      //slice->SliceHeaderSize = 0xffff; // 32+6 or +9 bits

      for(i=mbstart; i<mbstart+mbcount; i++) {
        Mb = &m_cuc->MbParam->Mb[i].MPEG2;
        mfxI32 mb_type = 0;
        mfxI32 k;

        if(MFX_ERR_NONE != TranslateMfxMbTypeMPEG2(Mb, &mbi))
          return MFX_ERR_UNSUPPORTED;
        mb_type = mbi.mb_type;

        if(MB_SKIPPED(Mb) && i!=mbstart && i+1 != mbstart+mbcount ) {
          mb_addr_incr++;
          pcoeff += 64*m_block_count;
          continue;
        }
        // put mba incr
        while( mb_addr_incr > 33 ) {
          PUT_BITS(0x08, 11); // put macroblock_escape
          mb_addr_incr -= 33;
        }
        PUT_BITS(mfx_mpeg2_AddrIncrementTbl[mb_addr_incr].code, mfx_mpeg2_AddrIncrementTbl[mb_addr_incr].len);
        mb_addr_incr = 1;

        if(Mb->IntraMbFlag) {
          //MB type
          if (Mb->QpScaleCode != curquant) {
            mb_type |= MB_QUANT;
            curquant = Mb->QpScaleCode;
          }
          PUT_BITS(mfx_mpeg2_mbtypetab[pct_idx][mb_type].code,
            mfx_mpeg2_mbtypetab[pct_idx][mb_type].len);
          // dct_type
          if(!m_cuc->FrameParam->MPEG2.FieldPicFlag && !m_cuc->FrameParam->MPEG2.FrameDCTprediction)
            PUT_BITS(Mb->TransformFlag, 1); // or mbi.dct_type
          if(mb_type & MB_QUANT)
            PUT_BITS(Mb->QpScaleCode, 5);
          // ignore concealment MV
          if(i == mbstart || !Mb[-1].IntraMbFlag ) { // reset DC predictors
            Ipp32s dc_init = mfx_mpeg2_ResetTbl[m_cuc->FrameParam->MPEG2.IntraDCprecision]; // where is it?
            dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = dc_init;
          }

          // Blocks
          for(bl=0; bl<(mfxU32)m_block_count; bl++) {
            Ipp32s n, m, dct_diff, run, signed_level;
            Ipp32s absval, size1;
            Ipp32s cind = (bl<4)?0:(1+(bl&1));
            mfxI16 *block = BLOCK_DATA(bl); // need to set

            dct_diff = block[0] - dc_dct_pred[cind];
            dc_dct_pred[cind] = block[0];

            /* generate variable length code for DC coefficient (7.2.1)*/
            if(dct_diff == 0) {
              PUT_BITS(m_DC_Tbl[cind][0].code, m_DC_Tbl[cind][0].len )
            } else {
              m = dct_diff >> 31;
              absval = (dct_diff + m) ^ m;
              for (size1=1; absval >>= 1; size1++);

              /* generate VLC for dct_dc_size (Table B-12 or B-13)*/
              /* append fixed length code (dc_dct_differential)*/
              absval = ((m_DC_Tbl[cind][size1].code - m) << size1) + (dct_diff + m);
              n  = m_DC_Tbl[cind][size1].len + size1;
              PUT_BITS(absval, n )
            }

            run = 0;
            m = 0;
            for(n=1; n < 64; n++) {
              signed_level = block[n]; // stored in scan order
              if( signed_level )
              {
                //mp2PutAC(pBitStream, pOffset, run, signed_level, AC_Tbl);
                Ipp32s level, code, len;
                Ipp32s maxRun = AC_Tbl[0] >> 20;
                Ipp32s addr;
                Ipp32s * table;
                Ipp32u val;

                if(run > maxRun)
                {
                  /* no VLC for this (run, level) combination: use escape coding (7.2.2.3)*/
                  /* ISO/IEC 13818-2 uses a 12 bit code, Table B-16*/
                  signed_level &= (1 << 12) - 1;
                  val = signed_level + (run<<12) + (1<<18);
                  PUT_BITS(val, 24 )
                } else {

                  level = (signed_level < 0) ? -signed_level : signed_level;

                  addr = AC_Tbl[run + 1];
                  table = (Ipp32s*)((Ipp8s*)AC_Tbl + addr);
                  if(level <= table[0])
                  {
                    len  = *(table + signed_level) & 0x1f;
                    code = (*(table + signed_level) >> 16);
                    PUT_BITS(code, len )
                  }
                  else
                  {
                    /* no VLC for this (run, level) combination: use escape coding (7.2.2.3)*/
                    /* ISO/IEC 13818-2 uses a 12 bit code, Table B-16*/
                    signed_level &= (1 << 12) - 1;
                    val = signed_level + (run<<12) + (1<<18);
                    PUT_BITS(val, 24 )
                  }
                }
                m++;
                run = 0;
              }
              else
                run++;
            }
            PUT_BITS(EOBCode, EOBLen )
          }

        } else { // inter
          //MB type
          if (Mb->QpScaleCode != curquant) {
            mb_type |= MB_QUANT;
            curquant = Mb->QpScaleCode;
          }

          mfxU32 CodedBlockPattern;
          MB_COMP_CBP(Mb, CodedBlockPattern);
          if(CodedBlockPattern)
            mb_type |= MB_PATTERN;

          // case NO_MV for P
          if(picture_coding_type == MPEG2_P_PICTURE &&
            mbi.prediction_type == MC_FRAME &&
            (MB_MV(Mb,0,0)[0] | MB_MV(Mb,0,0)[1]) == 0 &&
            MB_FSEL(Mb,0,0) == m_cuc->FrameParam->MPEG2.BottomFieldFlag &&
            CodedBlockPattern ) {
              mb_type &= ~MB_FORWARD; // NO_MV
              for(k=0; k<4; k++)
                PMV[k][0] = PMV[k][1] = 0; // reset MV predictors
    #ifdef MPEG2_ENCODE_DEBUG_HW
              mpeg2_debug_PAK->SetNoMVMb(i);
    #endif //MPEG2_ENCODE_DEBUG_HW
          }

          PUT_BITS(mfx_mpeg2_mbtypetab[pct_idx][mb_type].code,
            mfx_mpeg2_mbtypetab[pct_idx][mb_type].len);

          // MB mode
          if (m_cuc->FrameParam->MPEG2.FieldPicFlag ||
            !m_cuc->FrameParam->MPEG2.FrameDCTprediction) {
              mfxU32 prediction_type = mbi.prediction_type;

              if(m_cuc->FrameParam->MPEG2.FieldPicFlag) {
                if(prediction_type == MC_FRAME)
                  prediction_type = MC_FIELD;
                else if(prediction_type == MC_FIELD)
                  prediction_type = MC_16X8;
              }
              if(mb_type & (MB_FORWARD | MB_BACKWARD))
                PUT_BITS(prediction_type, 2);
          }
          if (!m_cuc->FrameParam->MPEG2.FieldPicFlag &&
            !m_cuc->FrameParam->MPEG2.FrameDCTprediction &&
            CodedBlockPattern) {
              PUT_BITS(Mb->TransformFlag, 1); // or mbi.dct_type
          }
          if(mb_type & MB_QUANT) {
            PUT_BITS(Mb->QpScaleCode, 5);
          }

          if( i == mbstart || Mb[-1].IntraMbFlag
            || (picture_coding_type == MPEG2_P_PICTURE && MB_SKIPPED((Mb-1)))) {
              for(k=0; k<4; k++)
                PMV[k][0] = PMV[k][1] = 0; // reset MV predictors
          }
          // MVs
          for(dir=0; dir<2; dir++){
            mfxI32 ii, xy;
            if((dir==0 && !(mb_type & MB_FORWARD)) ||
              (dir==1 && !(mb_type & MB_BACKWARD))) {
                // update PMV?
                continue;
            }
            for(ii=0; ii<2; ii++) {
              if(ii>=MB_MV_COUNT(mbi)) {
                // update second MV
                PMV[dir*2+1][0] = PMV[dir*2][0];
                PMV[dir*2+1][1] = PMV[dir*2][1];
                continue;
              }
              if(ii>0 && Mb->MbType5Bits == MFX_MBTYPE_INTER_DUAL_PRIME)
                continue;
              if(Mb->FieldMbFlag && Mb->MbType5Bits != MFX_MBTYPE_INTER_DUAL_PRIME )
                PUT_BITS(MB_FSEL(Mb,dir,ii), 1);
              // MV
              for(xy=0; xy<2; xy++) {
                mfxI32 temp, motion_code, motion_residual, tpos;
                mfxI32 mv_shift = 0;
                if(xy == 1 && !m_cuc->FrameParam->MPEG2.FieldPicFlag && mbi.prediction_type != MC_FRAME)
                  mv_shift = 1;
                mfxI32 delta = (MB_MV(Mb,dir,ii)[xy]>>mv_shift) - (PMV[dir*2+ii][xy]>>mv_shift);
                PMV[dir*2+ii][xy] = MB_MV(Mb,dir,ii)[xy];
                /* fold vector difference into [vmin...vmax] */
                if(delta > mvmax[dir*2+xy])     delta -= 2*mvmax[dir*2+xy];
                else if(delta <= -mvmax[dir*2+xy]) delta += 2*mvmax[dir*2+xy];

                if(mvmax[dir*2+xy] == 16 || delta == 0) {
                  PUT_BITS(mfx_mpeg2_MV_VLC_Tbl[16 + delta].code, mfx_mpeg2_MV_VLC_Tbl[16 + delta].len);
                } else {
                  mfxI32 r_size = fcodes[dir*2+xy] - 1;
                  mfxI32 f = 1<<r_size;

                  // present delta as motion_code and motion_residual
                  if( delta < 0 ) {
                    temp = -delta + f - 1;
                    motion_code = temp >> r_size;
                    tpos = 16 - motion_code;
                  } else { // delta > 0
                    temp = delta + f - 1;
                    motion_code = temp >> r_size;
                    tpos = 16 + motion_code;
                  }
                  //mpeg2_assert(motion_code > 0 && motion_code <= 16);
                  // put variable length code for motion_code (6.3.16.3)
                  // put fixed length code for motion_residual
                  motion_residual = (mfx_mpeg2_MV_VLC_Tbl[tpos].code << r_size) + (temp & (f - 1));
                  r_size += mfx_mpeg2_MV_VLC_Tbl[tpos].len;

                  PUT_BITS(motion_residual, r_size);
                }
                if(Mb->MbType5Bits == MFX_MBTYPE_INTER_DUAL_PRIME) {
                  mfxI32 dmv = MB_MV(Mb,1,0)[xy]; // in range [-1,1]
                  motion_residual = (dmv==0)?0:((dmv>0)?2:3);
                  mfxI32 r_size = (dmv==0)?1:2;
                  PUT_BITS(motion_residual, r_size);
                }
              }
            }
          }

          // CBP
          if (CodedBlockPattern) {
            Ipp32s extra_bits = m_block_count - 6;
            Ipp32s cbp6 = (CodedBlockPattern >> extra_bits) & 63;
            PUT_BITS(mfx_mpeg2_CBP_VLC_Tbl[cbp6].code, mfx_mpeg2_CBP_VLC_Tbl[cbp6].len);
            if (extra_bits) {
              PUT_BITS(CodedBlockPattern & ((1 << extra_bits) - 1), extra_bits);
            }
          }

          mfxU32 blockmask = 1<<(m_block_count-1);
          // Blocks
          for(bl=0; bl<(mfxU32)m_block_count; bl++, blockmask>>=1) {
            Ipp32s n, m, run, signed_level;
            mfxI16 *block = BLOCK_DATA(bl); // need to set
            if( !(CodedBlockPattern & blockmask) )
              continue;
            run = 0;
            m = 0;

            for(n=0; n < 64; n++) {
              signed_level = block[n]; // stored in scan order
              if( signed_level ) {
                if(n==0 && abs(signed_level)==1) {
                  Ipp32s tmp = 2 + (signed_level==-1);
                  PUT_BITS(tmp, 2 )
                } else {
                  //mp2PutAC(pBitStream, pOffset, run, signed_level, AC_Tbl);
                  Ipp32s level, code, len;
                  Ipp32s maxRun = AC_Tbl[0] >> 20;
                  Ipp32s addr;
                  Ipp32s * table;
                  Ipp32u val;

                  if(run > maxRun)
                  {
                    /* no VLC for this (run, level) combination: use escape coding (7.2.2.3)*/
                    /* ISO/IEC 13818-2 uses a 12 bit code, Table B-16*/
                    signed_level &= (1 << 12) - 1;
                    val = signed_level + (run<<12) + (1<<18);
                    PUT_BITS(val, 24 )
                  } else {

                    level = (signed_level < 0) ? -signed_level : signed_level;

                    addr = vlcTableB5c_e[run + 1];
                    table = (Ipp32s*)((Ipp8s*)vlcTableB5c_e + addr);
                    if(level <= table[0])
                    {
                      len  = *(table + signed_level) & 0x1f;
                      code = (*(table + signed_level) >> 16);
                      PUT_BITS(code, len )
                    }
                    else
                    {
                      /* no VLC for this (run, level) combination: use escape coding (7.2.2.3)*/
                      /* ISO/IEC 13818-2 uses a 12 bit code, Table B-16*/
                      signed_level &= (1 << 12) - 1;
                      val = signed_level + (run<<12) + (1<<18);
                      PUT_BITS(val, 24 )
                    }
                  }
                }
                m++;
                run = 0;
              }
              else run++;
            }

            PUT_BITS(2, 2)// End of Inter Block
          }

        }
        pcoeff += 64*m_block_count;
      }
  }
  catch (MPEG2_Exceptions exception)
  {
     return exception.GetType();
  }
  *outbb = bb;
  return MFX_ERR_NONE;
}


mfxStatus MFXVideoPAKMPEG2::sliceCoef( MPEG2FrameState* state, mfxU32 mbstart, mfxU32 mbcount, mfxExtMpeg2Coeffs* coeff)
{
  mfxI16      Diff[64*12];
  mfxI16      Block[64];
  
  Ipp32s      i, j, ic, jc, k;
  Ipp32s      Count[12], CodedBlockPattern;
  IppMotionVector2   vector[3][2] = {0,}; // top/bottom/frame F/B
  IppiSize    roi8x8 = {8,8};
  mfxU32      mbnum;
  mfxI32      quant_scale_value;
  MBInfo      mbinfo[2];
  mfxI32      left=0, curr=0;

  MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "MPEG2Enc_sliceCoef");


  const mfxI32 *scan = m_cuc->FrameParam->MPEG2.AlternateScan ? mfx_mpeg2_AlternateScan : mfx_mpeg2_ZigZagScan;
  DctCoef *pcoeff = (DctCoef *)coeff->Coeffs;
  pcoeff += mbstart * 64*m_block_count;
  mfxMbCodeMPEG2 *Mb = &m_cuc->MbParam->Mb[mbstart].MPEG2;
  //slice->SliceHeaderSize = 0xffff; // 32+6 or +9 bits
  
  j = Mb->MbYcnt*16; jc = Mb->MbYcnt*BlkHeight_c;
  for(mbnum=mbstart; mbnum<mbstart+mbcount; mbnum++, left=curr, curr^=1) {
    
    mfxI16   *pDiff  = Diff;
    mfxI16   *pBlock = Block;
   

    Mb = &m_cuc->MbParam->Mb[mbnum].MPEG2;
    i = Mb->MbXcnt*16; ic = Mb->MbXcnt*BlkWidth_c;
    k = Mb->MbXcnt + Mb->MbYcnt * (m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1);
     
    Ipp32s src_offset   = i  +  j * state->YSrcFrameHSize;
    Ipp32s src_offset_c = ic + jc * state->UVSrcFrameHSize;
    Ipp32s out_offset   = i  +  j * state->YOutFrameHSize;
    Ipp32s out_offset_c = ic + jc *state->UVOutFrameHSize;
    Ipp32s ref_offset   = i  +  j * state->YRefFrameHSize;
    Ipp32s ref_offset_c = ic + jc *state->UVRefFrameHSize;

    if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
      src_offset_c *= 2;
      out_offset_c *= 2;
      ref_offset_c *= 2;
    }

    const Ipp8u *YSrcBlock = state->Y_src + src_offset;
    const Ipp8u *USrcBlock = state->U_src + src_offset_c;
    const Ipp8u *VSrcBlock = state->V_src + src_offset_c;

    Ipp8u *YOutBlock = state->Y_out + out_offset;
    Ipp8u *UOutBlock = state->U_out + out_offset_c;
    Ipp8u *VOutBlock = state->V_out + out_offset_c;

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
        ippiCopy_8u_C1R(state->YRefFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + ref_offset, state->YRefFrameHSize, YOutBlock, state->YOutFrameHSize, roi);
        if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
          roi.width = 16;
          roi.height = 8;
          ippiCopy_8u_C1R(state->URefFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + ref_offset_c, state->UVRefFrameHSize*2, UOutBlock, state->UVOutFrameHSize*2, roi);
        } else {
          roi.width = BlkWidth_c;
          roi.height = BlkHeight_c;
          ippiCopy_8u_C1R(state->URefFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + ref_offset_c, state->UVRefFrameHSize, UOutBlock, state->UVOutFrameHSize, roi);
          ippiCopy_8u_C1R(state->VRefFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + ref_offset_c, state->UVRefFrameHSize, VOutBlock, state->UVOutFrameHSize, roi);
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
#ifdef IPP_NV12
             GETDIFF_FRAME_FB_NV12(U, V, Y, c, pDiff);
#else
             GETDIFF_FRAME_FB(U, Y, c, pDiff);
             GETDIFF_FRAME_FB(V, Y, c, pDiff);
#endif
          } else {
            GETDIFF_FRAME_FB(U, UV, c, pDiff);
            GETDIFF_FRAME_FB(V, UV, c, pDiff);
          }
        }
        else if (mbtype & MB_FORWARD) {
          GETDIFF_FRAME(Y, Y, l, pDiff, 0);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
#ifdef IPP_NV12
            GETDIFF_FRAME_NV12(U, V, Y, c, pDiff, 0);
#else
            GETDIFF_FRAME(U, Y, c, pDiff, 0);
            GETDIFF_FRAME(V, Y, c, pDiff, 0);
#endif
          } else {
            GETDIFF_FRAME(U, UV, c, pDiff, 0);
            GETDIFF_FRAME(V, UV, c, pDiff, 0);
          }
        }
        else { // if(mbtype & MB_BACKWARD)
          GETDIFF_FRAME(Y, Y, l, pDiff, 1);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {

#ifdef IPP_NV12
            GETDIFF_FRAME_NV12(U, V, Y, c, pDiff, 1);
#else
            GETDIFF_FRAME(U, Y, c, pDiff, 1);
            GETDIFF_FRAME(V, Y, c, pDiff, 1);
#endif
          } else {
            GETDIFF_FRAME(U, UV, c, pDiff, 1);
            GETDIFF_FRAME(V, UV, c, pDiff, 1);
          }
        }
      } else {
        if (mbtype == (MB_FORWARD|MB_BACKWARD)) {
          GETDIFF_FIELD_FB(Y, Y, l, pDiff);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
#ifdef IPP_NV12
            GETDIFF_FIELD_FB_NV12(U, V, Y, c, pDiff);
#else
            GETDIFF_FIELD_FB(U, Y, c, pDiff);
            GETDIFF_FIELD_FB(V, Y, c, pDiff);
#endif
          } else {
            GETDIFF_FIELD_FB(U, UV, c, pDiff);
            GETDIFF_FIELD_FB(V, UV, c, pDiff);
          }
        }
        else if (mbtype & MB_FORWARD) {
          GETDIFF_FIELD(Y, Y, l, pDiff, 0);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
#ifdef IPP_NV12
            GETDIFF_FIELD_NV12(U, V, Y, c, pDiff, 0);

#else
            GETDIFF_FIELD(U, Y, c, pDiff, 0);
            GETDIFF_FIELD(V, Y, c, pDiff, 0);
#endif
          } else {
            GETDIFF_FIELD(U, UV, c, pDiff, 0);
            GETDIFF_FIELD(V, UV, c, pDiff, 0);
          }
        }
        else { // if(mbtype & MB_BACKWARD)
          GETDIFF_FIELD(Y, Y, l, pDiff, 1);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
#ifdef IPP_NV12
            GETDIFF_FIELD_NV12(U, V, Y, c, pDiff, 1);

#else
            GETDIFF_FIELD(U, Y, c, pDiff, 1);
            GETDIFF_FIELD(V, Y, c, pDiff, 1);
#endif
          } else {
            GETDIFF_FIELD(U, UV, c, pDiff, 1);
            GETDIFF_FIELD(V, UV, c, pDiff, 1);
          }
        }
      }
#ifdef MPEG2_ENCODE_DEBUG_HW
      mpeg2_debug_PAK->GatherInterRefMBlocksData(k,vector,state,&mbinfo[curr]);
#endif // MPEG2_ENCODE_DEBUG_HW
    }

    if ((!m_cuc->FrameParam->MPEG2.FrameDCTprediction) && m_cuc->FrameParam->MPEG2.IntraPicFlag) {
      Ipp32s var_fld = 0, var = 0;
      ippiFrameFieldSAD16x16_8u32s_C1R(YSrcBlock, state->YSrcFrameHSize, &var, &var_fld);
      if (var_fld < var) {
        mbinfo[curr].dct_type = DCT_FIELD;
        Mb->TransformFlag = 1;
      } else {
        mbinfo[curr].dct_type = DCT_FRAME;
        Mb->TransformFlag = 0;
      }
    } 

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
        memset (pcoeff,0,sizeof(DctCoef)*64);
        
        if (!Count[loop]) {
          ippiSet_16s_C1R(0, pDiff + diff_off[loop], dct_step[loop], roi8x8);
#ifdef MPEG2_ENCODE_DEBUG_HW
        mpeg2_debug_PAK->GatherBlockData(k,loop,picture_coding_type,quant_scale_value,InterQM,pBlock,Count[loop],0,0);
#endif // MPEG2_ENCODE_DEBUG_HW
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
#ifdef MPEG2_ENCODE_DEBUG_HW
        mpeg2_debug_PAK->GatherBlockData(k,loop,picture_coding_type,quant_scale_value,InterQM,pBlock,Count[loop],0,0);
#endif // MPEG2_ENCODE_DEBUG_HW
      }
      if (!CodedBlockPattern && mbinfo[curr].prediction_type == MC_FRAME &&
        mbnum > mbstart && mbnum < (mbstart + mbcount - 1) &&
        Mb->MbType5Bits != MFX_MBTYPE_INTER_DUAL_PRIME ) {
          if ((picture_coding_type == MPEG2_P_PICTURE /*&& !ipflag*/
            && !(MB_MV(Mb,0,0)[0] | MB_MV(Mb,0,0)[1])
            && MB_FSEL(Mb,0,0) == m_cuc->FrameParam->MPEG2.BottomFieldFlag)
          ||
            (picture_coding_type == MPEG2_B_PICTURE
            && ((mbinfo[curr].mb_type ^ mbinfo[left].mb_type) & (MB_FORWARD | MB_BACKWARD)) == 0
            && ((mbinfo[curr].mb_type&MB_FORWARD) == 0 ||
                (MB_MV(Mb,0,0)[0] == MB_MV(Mb-1,0,0)[0] &&
                MB_MV(Mb,0,0)[1] == MB_MV(Mb-1,0,0)[1] &&
                MB_FSEL(Mb,0,0) == m_cuc->FrameParam->MPEG2.BottomFieldFlag) )
            && ((mbinfo[curr].mb_type&MB_BACKWARD) == 0 ||
                (MB_MV(Mb,1,0)[0] == MB_MV(Mb-1,1,0)[0] &&
                MB_MV(Mb,1,0)[1] == MB_MV(Mb-1,1,0)[1] &&
                MB_FSEL(Mb,1,0) == m_cuc->FrameParam->MPEG2.BottomFieldFlag))) ){
              Mb->Skip8x8Flag = 0xf;
          if(picture_coding_type == MPEG2_B_PICTURE)
            mbinfo[curr].mb_type = mbinfo[left].mb_type;
          else // if(picture_coding_type == MPEG2_P_PICTURE)
            mbinfo[curr].mb_type = MB_FORWARD;
          mbinfo[curr].skipped = 1;
#ifdef MPEG2_ENCODE_DEBUG_HW
          mpeg2_debug_PAK->SetSkippedMb(k);
#endif // MPEG2_ENCODE_DEBUG_HW
        }
      }
      if(picture_coding_type != MPEG2_B_PICTURE) {

        if(MB_SKIPPED(Mb)) {
          mbinfo[curr].mb_type = MB_FORWARD;
          mbinfo[curr].skipped = 1;
          IppiSize    roi;
          roi.width = BlkWidth_l;
          roi.height = BlkHeight_l;
          ippiCopy_8u_C1R(state->YRefFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + ref_offset, state->YRefFrameHSize, YOutBlock, state->YOutFrameHSize, roi);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
            roi.width = 16;
            roi.height = 8;
            ippiCopy_8u_C1R(state->URefFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + ref_offset_c, state->UVRefFrameHSize*2, UOutBlock, state->UVOutFrameHSize*2, roi);
          } else {
            roi.width = BlkWidth_c;
            roi.height = BlkHeight_c;
            ippiCopy_8u_C1R(state->URefFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + ref_offset_c, state->UVRefFrameHSize, UOutBlock, state->UVOutFrameHSize, roi);
            ippiCopy_8u_C1R(state->VRefFrame[m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + ref_offset_c, state->UVRefFrameHSize, VOutBlock, state->UVOutFrameHSize, roi);
          }
        }
        else if(predtype == MC_FRAME)
        {
          MC_FRAME_F(Y, Y, l, pDiff);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
#ifdef IPP_NV12
          MC_FRAME_F_NV12(U, V, Y, c, pDiff);   
#else
          MC_FRAME_F(U, Y, c, pDiff);
          MC_FRAME_F(V, Y, c, pDiff);
#endif
          } else {
            MC_FRAME_F(U, UV, c, pDiff);
            MC_FRAME_F(V, UV, c, pDiff);
          }
        } else {
          MC_FIELD_F(Y, Y, l, pDiff);
          if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {

#ifdef IPP_NV12
            MC_FIELD_F_NV12(U, V, Y, c, pDiff);   
#else
            MC_FIELD_F(U, Y, c, pDiff);
            MC_FIELD_F(V, Y, c, pDiff);
#endif
          } else {
            MC_FIELD_F(U, UV, c, pDiff);
            MC_FIELD_F(V, UV, c, pDiff);
          }
        }
      }

    } else { // intra
      Ipp32s strideSrc[3];
      Ipp32s strideRec[3];

      Ipp32s *block_offset;
      Ipp32s *block_offset_rec;
      Ipp32s intra_dc_shift = 3 - m_cuc->FrameParam->MPEG2.IntraDCprecision;
      Ipp32s half_intra_dc = (1 << intra_dc_shift) >> 1;

      const Ipp8u *BlockSrc[3] = {YSrcBlock, USrcBlock, VSrcBlock};
      Ipp8u *BlockRec[3] = {YOutBlock, UOutBlock, VOutBlock};

      if (mbinfo[curr].dct_type == DCT_FRAME) {
        block_offset     = src_block_offset_frm;
        block_offset_rec = out_block_offset_frm/*_ref*/;

        strideSrc[0] = state->YSrcFrameHSize;
        strideSrc[1] = state->UVSrcFrameHSize;
        strideSrc[2] = state->UVSrcFrameHSize;

        strideRec[0] = state->YOutFrameHSize;
        strideRec[1] = state->UVOutFrameHSize;
        strideRec[2] = state->UVOutFrameHSize;

      } else {
        block_offset    = src_block_offset_fld;
        block_offset_rec =out_block_offset_fld/*_ref*/;

        strideSrc[0] = 2*state->YSrcFrameHSize;
        strideSrc[1] = state->UVSrcFrameHSize << chroma_fld_flag;
        strideSrc[2] = state->UVSrcFrameHSize << chroma_fld_flag;

        strideRec[0] = 2*state->YOutFrameHSize;
        strideRec[1] = state->UVOutFrameHSize << chroma_fld_flag;
        strideRec[2] = state->UVOutFrameHSize << chroma_fld_flag;

      }
      if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {

        strideSrc[1] = state->YSrcFrameHSize;
        strideSrc[2] = state->YSrcFrameHSize;

        strideRec[1] = state->YOutFrameHSize;
        strideRec[2] = state->YOutFrameHSize;

      }

      Mb->CodedPattern4x4Y = 0xffff;
      Mb->CodedPattern4x4U = 0xffff >> (24-2*m_block_count);
      Mb->CodedPattern4x4V = 0xffff >> (24-2*m_block_count);

      for (Ipp32s blk = 0; blk < m_block_count; blk++, pcoeff += 64, pDiff+=64) {
        mfxI32 cc = mfx_mpeg2_color_index[blk];
        if (blk < 4 || m_info.FrameInfo.FourCC != MFX_FOURCC_NV12) {
          ippiDCT8x8Fwd_8u16s_C1R(BlockSrc[cc] + block_offset[blk], strideSrc[cc], pDiff);
        } else {
#ifndef IPP_NV12
          tmp_ippiDCT8x8FwdUV_8u16s_C1R(BlockSrc[cc], strideSrc[cc], pDiff);
#else
          if(blk == 4)
          {
            ippiDCT8x8Fwd_8u16s_C2P2(BlockSrc[cc], strideSrc[cc],pDiff,pDiff+64);
          }
#endif
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
              ippiDCT8x8Inv_16s8u_C1R (pDiff, BlockRec[cc] + block_offset_rec[blk], strideRec[cc]);
            } else {
#ifndef IPP_NV12
              tmp_ippiDCT8x8InvUV_16s8u_C1R (pDiff, BlockRec[cc], strideRec[cc]);
#else
              if (blk == 5){
                ippiDCT8x8InvOrSet_16s8u_P2C2(pDiff - 64,pDiff,BlockRec[1], strideRec[1],Count[4]!=0? 0:2);
              }
#endif
            }
          } else {
            if (blk < 4 || m_info.FrameInfo.FourCC != MFX_FOURCC_NV12) {
              ippiSet_8u_C1R((Ipp8u)(pDiff[0]/8), BlockRec[cc] + block_offset_rec[blk], strideRec[cc], roi8x8);
            } else {
#ifndef IPP_NV12
              tmp_ippiSet8x8UV_8u_C2R((Ipp8u)(pDiff[0]/8), BlockRec[cc], strideRec[cc]);
#else
              if (blk == 5){
                ippiDCT8x8InvOrSet_16s8u_P2C2(pDiff-64,pDiff,BlockRec[1], strideRec[1],Count[4]!=0? 1:3);
              }             
#endif
            }
          }
        }
#ifdef MPEG2_ENCODE_DEBUG_HW
        mpeg2_debug_PAK->GatherBlockData(k,blk,picture_coding_type,quant_scale_value,IntraQM,pDiff,Count[blk],1,intra_dc_shift);
#endif // MPEG2_ENCODE_DEBUG_HW
      }
    }
    if((DctCoef *)coeff->Coeffs + ((Mb->MbXcnt+1 + Mb->MbYcnt * (m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1))*m_block_count ) * 64 != pcoeff)
      pcoeff = (DctCoef *)coeff->Coeffs + ((Mb->MbXcnt+1 + Mb->MbYcnt * (m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1))*m_block_count ) * 64;
  }

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKMPEG2::preEnc(MPEG2FrameState* state)
{
  mfxFrameSurface *surface = m_cuc->FrameSurface;
  mfxStatus        sts     = MFX_ERR_NONE;
  
  MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "MPEG2Enc_preEnc");  

  // GetFrame stuff
  mfxI32 i, ind[2];
  mfxI32 curind = m_cuc->FrameParam->MPEG2.CurrFrameLabel;
  mfxI32 recind = m_cuc->FrameParam->MPEG2.RecFrameLabel;
  ind[0] = m_cuc->FrameParam->MPEG2.RefFrameListB[0][0];
  ind[1] = m_cuc->FrameParam->MPEG2.RefFrameListB[1][0];

  // check surface dimension
  if (surface->Info.Height < 16*(m_cuc->FrameParam->MPEG2.FrameHinMbMinus1+1) ||
      surface->Info.Width  < 16*(m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1))
    return MFX_ERR_UNSUPPORTED; // not aligned to MB

  state->needFrameUnlock[0] = 0;
  if(!surface->Data[curind]->Y) {    
      sts = ((surface->Data[curind]->reserved[0] == 0) ? 
        m_core->LockExternalFrame(surface->Data[curind]->MemId, surface->Data[curind]):
        m_core->LockFrame(surface->Data[curind]->MemId, surface->Data[curind]));
      MFX_CHECK_STS (sts);
      state->needFrameUnlock[0] = 1;
  }      
  MFX_CHECK(surface->Data[curind]->Pitch < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);

 
  state->YSrcFrameHSize = surface->Data[curind]->Pitch;

  if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
    state->UVSrcFrameHSize = state->YSrcFrameHSize >> 1; // to optimize in future
  } else {
    // now only YV12
    state->UVSrcFrameHSize = state->YSrcFrameHSize >> 1; // surface->Info.ScaleCPitch;
  }

  mfxU32 offl, offc;
  offl = surface->Info.CropX + surface->Info.CropY*state->YSrcFrameHSize;
  offc = (surface->Info.CropX>>1) + 
    (surface->Info.CropY>>1)*state->UVSrcFrameHSize;

  if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12)
  {  
    offc *= 2;
    state->Y_src = (Ipp8u*)surface->Data[curind]->Y + offl;
    state->U_src = (Ipp8u*)surface->Data[curind]->U + offc;
    state->V_src =  state->U_src + 1;
  }
  else
  {
    state->Y_src = (Ipp8u*)surface->Data[curind]->Y + offl;
    state->U_src = (Ipp8u*)surface->Data[curind]->U + offc;
    state->V_src = (Ipp8u*)surface->Data[curind]->V + offc;  
  }
    
  state->needFrameUnlock[3] = 0;
  if(picture_coding_type != MPEG2_B_PICTURE) {
    if(!surface->Data[recind]->Y) {
      
        sts = ((surface->Data[recind]->reserved[0]== 0)? 
        m_core->LockExternalFrame(surface->Data[recind]->MemId, surface->Data[recind]):
        m_core->LockFrame(surface->Data[recind]->MemId, surface->Data[recind]));
        MFX_CHECK_STS (sts);
        state->needFrameUnlock[3] = 1;
    }        
    MFX_CHECK(surface->Data[recind]->Pitch < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);

    state->YOutFrameHSize = surface->Data[recind]->Pitch;

    if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
      state->UVOutFrameHSize = state->YOutFrameHSize >> 1; // to optimize in future
    } else {
      // now only YV12
      state->UVOutFrameHSize = state->YOutFrameHSize >> 1; // surface->Info.ScaleCPitch;
    }
    offl = surface->Info.CropX + surface->Info.CropY*state->YOutFrameHSize;
    offc = (surface->Info.CropX>>1) + 
       (surface->Info.CropY>>1)*state->UVOutFrameHSize;


    state->Y_out = (Ipp8u*)surface->Data[recind]->Y + offl;
    state->U_out = (Ipp8u*)surface->Data[recind]->U + offc;    
    state->V_out = (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12)?
        state->U_out + 1 :(Ipp8u*)surface->Data[recind]->V + offc;
  }
  {
    Ipp32s w_shift = (m_cuc->FrameParam->MPEG2.FieldPicFlag) ? 1 : 0;

    Ipp32s nYPitch = state->YSrcFrameHSize << w_shift;
    Ipp32s nUPitch = state->UVSrcFrameHSize << w_shift;
    Ipp32s nVPitch = state->UVSrcFrameHSize << w_shift;

    src_block_offset_frm[2] = 8*nYPitch;
    src_block_offset_frm[3] = 8*nYPitch + 8;
    src_block_offset_frm[6] = 8*nUPitch;
    src_block_offset_frm[7] = 8*nVPitch;
    src_block_offset_frm[10] = 8*nUPitch + 8;
    src_block_offset_frm[11] = 8*nVPitch + 8;

    src_block_offset_fld[2] = nYPitch;
    src_block_offset_fld[3] = nYPitch + 8;
    src_block_offset_fld[6] = nUPitch;
    src_block_offset_fld[7] = nVPitch;
    src_block_offset_fld[10] = nUPitch + 8;
    src_block_offset_fld[11] = nVPitch + 8;

    nYPitch = state->YOutFrameHSize << w_shift;
    nUPitch = state->UVOutFrameHSize << w_shift;
    nVPitch = state->UVOutFrameHSize << w_shift;

    out_block_offset_frm[2] = 8*nYPitch;
    out_block_offset_frm[3] = 8*nYPitch + 8;
    out_block_offset_frm[6] = 8*nUPitch;
    out_block_offset_frm[7] = 8*nVPitch;
    out_block_offset_frm[10] = 8*nUPitch + 8;
    out_block_offset_frm[11] = 8*nVPitch + 8;

    out_block_offset_fld[2] = nYPitch;
    out_block_offset_fld[3] = nYPitch + 8;
    out_block_offset_fld[6] = nUPitch;
    out_block_offset_fld[7] = nVPitch;
    out_block_offset_fld[10] = nUPitch + 8;
    out_block_offset_fld[11] = nVPitch + 8;
  }

  if (picture_coding_type != MPEG2_I_PICTURE)
  {
      for(i=0; i<2; i++) { // forward/backward
        state->needFrameUnlock[i+1] = 0;
        if(!surface->Data[ind[i]]->Y) {
            sts = ((surface->Data[ind[i]]->reserved[0]==0)?
              m_core->LockExternalFrame(surface->Data[ind[i]]->MemId, surface->Data[ind[i]]):
              m_core->LockFrame(surface->Data[ind[i]]->MemId, surface->Data[ind[i]]));
            MFX_CHECK_STS (sts);
            state->needFrameUnlock[i+1] = 1; 
        }
        MFX_CHECK(surface->Data[ind[i]]->Pitch < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);

        if (i== 0)
        {
            state->YRefFrameHSize = surface->Data[ind[i]]->Pitch;

            if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
              state->UVRefFrameHSize = state->YRefFrameHSize >> 1; // to optimize in future
            } else {
              // now only YV12
              state->UVRefFrameHSize = state->YRefFrameHSize >> 1; // surface->Info.ScaleCPitch;
            }
            offl = surface->Info.CropX + surface->Info.CropY*state->YRefFrameHSize;
            offc = (surface->Info.CropX>>1) + 
                (surface->Info.CropY>>1)*state->UVRefFrameHSize;

        }
        else
        {
            // check pitches. Can be changed, but must be the same in all frames
            if (state->YRefFrameHSize != surface->Data[ind[i]]->Pitch)
              return MFX_ERR_UNSUPPORTED;
        }

        state->YRefFrame[0][i] = (Ipp8u*)surface->Data[ind[i]]->Y + offl;
        state->URefFrame[0][i] = (Ipp8u*)surface->Data[ind[i]]->U + offc;
        state->VRefFrame[0][i] = (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12)?
            state->URefFrame[0][i] + 1 :(Ipp8u*)surface->Data[ind[i]]->V + offc;

        state->YRefFrame[1][i] = state->YRefFrame[0][i] + state->YRefFrameHSize;
        if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
          state->URefFrame[1][i] = state->URefFrame[0][i] + state->UVRefFrameHSize*2;
          state->VRefFrame[1][i] = state->VRefFrame[0][i] + state->UVRefFrameHSize*2;
        } else {
          state->URefFrame[1][i] = state->URefFrame[0][i] + state->UVRefFrameHSize;
          state->VRefFrame[1][i] = state->VRefFrame[0][i] + state->UVRefFrameHSize;
        }
      }
  }
  else
  {
     for(i=0; i<2; i++) { // forward/backward
     {
        state->needFrameUnlock[i+1] = 0;
        state->YRefFrame[0][i] = 0;
        state->URefFrame[0][i] = 0;
        state->VRefFrame[0][i] = 0; 
        state->YRefFrame[1][i] = 0;
        state->URefFrame[1][i] = 0;
        state->VRefFrame[1][i] = 0; 
     }

    }

  }

  
  if (m_cuc->FrameParam->MPEG2.FieldPicFlag) {
    if (!m_cuc->FrameParam->MPEG2.BottomFieldFlag) {
      if (picture_coding_type == MPEG2_P_PICTURE && m_cuc->FrameParam->MPEG2.SecondFieldFlag) {
        state->YRefFrame[1][0] =state->YRefFrame[1][1];  //YRecFrame[1][1];
        if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
          state->URefFrame[1][0] = state->URefFrame[1][1]; //URecFrame[1][1];
          state->VRefFrame[1][0] = state->VRefFrame[1][1]; //VRecFrame[1][1];
        } else {
          state->URefFrame[1][0] = state->URefFrame[1][1] + state->UVRefFrameHSize; //URecFrame[1][1];
          state->VRefFrame[1][0] = state->VRefFrame[1][1] + state->UVRefFrameHSize; //VRecFrame[1][1];
        }
      }
    } else { // (m_cuc->FrameParam->BottomFieldFlag)
      if (picture_coding_type == MPEG2_P_PICTURE && m_cuc->FrameParam->MPEG2.SecondFieldFlag) {
         state->YRefFrame[0][0] = state->YRefFrame[0][1]; //YRecFrame[0][1];
         state->URefFrame[0][0] = state->URefFrame[0][1]; //URecFrame[0][1];
         state->VRefFrame[0][0] = state->VRefFrame[0][1]; //VRecFrame[0][1];
      }
      state->Y_src += state->YSrcFrameHSize;
      state->Y_out += state->YSrcFrameHSize;
      if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
        state->U_src += state->UVSrcFrameHSize*2;
        state->V_src += state->UVSrcFrameHSize*2;
        state->U_out += state->UVOutFrameHSize*2;
        state->V_out += state->UVOutFrameHSize*2;
      } else {
        state->U_src += state->UVSrcFrameHSize;
        state->V_src += state->UVSrcFrameHSize;
        state->U_out += state->UVOutFrameHSize;
        state->V_out += state->UVOutFrameHSize;
      }
    }
    state->YSrcFrameHSize<<=1;
    state->UVSrcFrameHSize<<=1;
    state->YOutFrameHSize<<=1;
    state->UVOutFrameHSize<<=1;
    state->YRefFrameHSize<<=1;
    state->UVRefFrameHSize<<=1;


  }

  if (picture_coding_type == MPEG2_P_PICTURE) { // BW would be used for DMV in DualPrime
    state->YRefFrame[0][1] = state->YRefFrame[0][0];
    state->URefFrame[0][1] = state->URefFrame[0][0];
    state->VRefFrame[0][1] = state->VRefFrame[0][0];
    state->YRefFrame[1][1] = state->YRefFrame[1][0];
    state->URefFrame[1][1] = state->URefFrame[1][0];
    state->VRefFrame[1][1] = state->VRefFrame[1][0];
  }

  return MFX_ERR_NONE;
}

void      MFXVideoPAKMPEG2::postEnc(MPEG2FrameState* state)
{
  mfxFrameSurface *surface = m_cuc->FrameSurface;
  mfxI32 i, ind[2];
  mfxI32 curind = m_cuc->FrameParam->MPEG2.CurrFrameLabel;
  mfxI32 recind = m_cuc->FrameParam->MPEG2.RecFrameLabel;
  ind[0] = m_cuc->FrameParam->MPEG2.RefFrameListB[0][0];
  ind[1] = m_cuc->FrameParam->MPEG2.RefFrameListB[1][0];

  MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "MPEG2Enc_postEnc");  


  if(state->needFrameUnlock[0]) {
    if (surface->Data[curind]->reserved[0] == 0)
      m_core->UnlockExternalFrame(surface->Data[curind]->MemId, surface->Data[curind]);
    else
      m_core->UnlockFrame(surface->Data[curind]->MemId, surface->Data[curind]);
    state->needFrameUnlock[0] = 0;
  }
  if(state->needFrameUnlock[3]) {
    if (surface->Data[recind]->reserved[0]==0)
      m_core->UnlockExternalFrame(surface->Data[recind]->MemId, surface->Data[recind]);
    else
      m_core->UnlockFrame(surface->Data[recind]->MemId, surface->Data[recind]);

    state->needFrameUnlock[3] = 0;
  }
  for(i=0; i<2; i++) { // forward/backward
    if(state->needFrameUnlock[i+1]) {
      if (surface->Data[ind[i]]->reserved[0]== 0)
        m_core->UnlockExternalFrame(surface->Data[ind[i]]->MemId, surface->Data[ind[i]]);
      else
        m_core->UnlockFrame(surface->Data[ind[i]]->MemId, surface->Data[ind[i]]);

      state->needFrameUnlock[i+1] = 0;
    }
  }
  if (m_cuc->FrameParam->MPEG2.FieldPicFlag) {
    // restore params
    state->YSrcFrameHSize>>=1;
    state->UVSrcFrameHSize>>=1;
    state->YOutFrameHSize>>=1;
    state->UVOutFrameHSize>>=1;
    state->YRefFrameHSize>>=1;
    state->UVRefFrameHSize>>=1;

  }
}

// mode == 0 for sequence header, mode == 1 for quant_matrix_extension
mfxStatus MFXVideoPAKMPEG2::putQMatrices( int mode, bitBuffer* outbb )
{
  mfxU32 i;
  bitBuffer bb;
  mfxU32 sameflag[4] = {3,3,3,3};
  mfxExtMpeg2QMatrix* qm = 0;
  mfxExtMpeg2QMatrix defaultQMset;
  if(m_cuc != 0)
    qm = (mfxExtMpeg2QMatrix*)GetExtBuffer(m_cuc, MFX_CUC_MPEG2_QMATRIX);
  if(qm == 0) { // default are used
    if(mode == 0) {
      // reset qm if current is not default
      InvIntraChromaQM = InvIntraQM = 0;
      InvInterChromaQM = InvInterQM = 0;
      IntraQM = _IntraQM;
      IntraChromaQM = _IntraChromaQM;
      InterChromaQM = InterQM = 0;
      for(i=0; i<64; i++) {
        m_qm.IntraQMatrix[i] = m_qm.ChromaIntraQMatrix[i] = mfx_mpeg2_default_intra_qm[i];
        m_qm.InterQMatrix[i] = m_qm.ChromaInterQMatrix[i] = 16;
        _IntraQM[i] = _IntraChromaQM[i] = mfx_mpeg2_default_intra_qm[i];
      }
      return MFX_ERR_NONE;
    } else {
    //  for qm extension and if current are not default need to put them
      for(i=0; i<64; i++) {
        defaultQMset.IntraQMatrix[i] = defaultQMset.ChromaIntraQMatrix[i] = mfx_mpeg2_default_intra_qm[i];
        defaultQMset.InterQMatrix[i] = defaultQMset.ChromaInterQMatrix[i] = 16;
      }
      qm = &defaultQMset;
    }
  }

  bb = *outbb;

  for(i=0; i<64 && sameflag[0]; i++) {
    if(qm->IntraQMatrix[i] != mfx_mpeg2_default_intra_qm[i])
      sameflag[0] &= ~1;
    if(qm->IntraQMatrix[i] != m_qm.IntraQMatrix[i])
      sameflag[0] &= ~2;
  }
  for(i=0; i<64 && sameflag[1]; i++) {
    if(qm->InterQMatrix[i] != 16)
      sameflag[1] &= ~1;
    if(qm->InterQMatrix[i] != m_qm.InterQMatrix[i])
      sameflag[1] &= ~2;
  }

  if(mode == 1 && m_info.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420+0) {
    for(i=0; i<64 && sameflag[2]; i++) {
      if(qm->ChromaIntraQMatrix[i] != qm->IntraQMatrix[i])
        sameflag[2] &= ~1;
      if(qm->ChromaIntraQMatrix[i] != m_qm.ChromaIntraQMatrix[i])
        sameflag[2] &= ~2;
    }
    for(i=0; i<64 && sameflag[3]; i++) {
      if(qm->ChromaInterQMatrix[i] != qm->InterQMatrix[i])
        sameflag[3] &= ~1;
      if(qm->ChromaInterQMatrix[i] != m_qm.ChromaInterQMatrix[i])
        sameflag[3] &= ~2;
    }
  }

  if(mode==0) {
    if(sameflag[0] & 1) {
      PUT_BITS(0,1);
      InvIntraChromaQM = InvIntraQM = 0;
      IntraQM = _IntraQM;
      IntraChromaQM = _IntraChromaQM;
      for(i=0; i<64; i++) {
        _IntraQM[i] = _IntraChromaQM[i] = mfx_mpeg2_default_intra_qm[i];
      }
    } else {
      PUT_BITS(1,1);
      InvIntraQM = _InvIntraQM;
      InvIntraChromaQM = _InvIntraChromaQM;
      IntraQM = _IntraQM;
      IntraChromaQM = _IntraChromaQM;
      for(i=0; i<64; i++) {
        PUT_BITS(qm->IntraQMatrix[mfx_mpeg2_ZigZagScan[i]],8);
        m_qm.IntraQMatrix[i] = qm->IntraQMatrix[i];
        m_qm.ChromaIntraQMatrix[i] = qm->IntraQMatrix[i];
        _InvIntraChromaQM[i] = _InvIntraQM[i] = 1.f/(mfxF32)qm->IntraQMatrix[i];
        _IntraChromaQM[i] = _IntraQM[i] = qm->IntraQMatrix[i];
      }
    }
    if(sameflag[1] & 1) {
      PUT_BITS(0,1);
      InvInterChromaQM = InvInterQM = 0;
      InterChromaQM = InterQM = 0;
    } else {
      PUT_BITS(1,1);
      InvInterQM = _InvInterQM;
      InvInterChromaQM = _InvInterChromaQM;
      InterQM = _InterQM;
      InterChromaQM = _InterChromaQM;
      for(i=0; i<64; i++) {
        PUT_BITS(qm->InterQMatrix[mfx_mpeg2_ZigZagScan[i]],8);
        m_qm.InterQMatrix[i] = qm->InterQMatrix[i];
        m_qm.ChromaInterQMatrix[i] = qm->InterQMatrix[i];
        _InvInterChromaQM[i] = _InvInterQM[i] = 1.f/(mfxF32)qm->InterQMatrix[i];
        _InterChromaQM[i] = _InterQM[i] = qm->InterQMatrix[i];
      }
    }
  }

  if(mode==1) { // qm extension
    if( !(sameflag[0]&2) || !(sameflag[1]&2) ||
        !(sameflag[2]&1) || !(sameflag[3]&1)) {
      PUT_START_CODE(EXT_START_CODE); // ext code
      PUT_BITS(QUANT_ID,4);           // qm ext code
      if(sameflag[0] & 2) {
        PUT_BITS(0,1);
      } else {
        PUT_BITS(1,1);
        InvIntraQM = _InvIntraQM;
        InvIntraChromaQM = _InvIntraChromaQM;
        IntraQM = _IntraQM;
        IntraChromaQM = _IntraChromaQM;
        for(i=0; i<64; i++) {
          PUT_BITS(qm->IntraQMatrix[mfx_mpeg2_ZigZagScan[i]],8);
          m_qm.IntraQMatrix[i] = qm->IntraQMatrix[i];
          m_qm.ChromaIntraQMatrix[i] = qm->IntraQMatrix[i];
          _InvIntraChromaQM[i] = _InvIntraQM[i] = 1.f/(mfxF32)qm->IntraQMatrix[i];
          _IntraChromaQM[i] = _IntraQM[i] = qm->IntraQMatrix[i];
        }
      }
      if(sameflag[1] & 2) {
        PUT_BITS(0,1);
      } else {
        PUT_BITS(1,1);
        InvInterQM = _InvInterQM;
        InvInterChromaQM = _InvInterChromaQM;
        InterQM = _InterQM;
        InterChromaQM = _InterChromaQM;
        for(i=0; i<64; i++) {
          PUT_BITS(qm->InterQMatrix[mfx_mpeg2_ZigZagScan[i]],8);
          m_qm.InterQMatrix[i] = qm->InterQMatrix[i];
          m_qm.ChromaInterQMatrix[i] = qm->InterQMatrix[i];
          _InvInterChromaQM[i] = _InvInterQM[i] = 1.f/(mfxF32)qm->InterQMatrix[i];
          _InterChromaQM[i] = _InterQM[i] = qm->InterQMatrix[i];
        }
      }
      if(m_info.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420+0 ||
        ((sameflag[0] & 2) && (sameflag[2] & 2)) ||  // both unchanged
        (!(sameflag[0] & 2) && (sameflag[2] & 1))) { // both changed to same
          PUT_BITS(0,1);
      } else {
        PUT_BITS(1,1);
        InvIntraChromaQM = _InvIntraChromaQM;
        IntraChromaQM = _IntraChromaQM;
        for(i=0; i<64; i++) {
          PUT_BITS(qm->ChromaIntraQMatrix[mfx_mpeg2_ZigZagScan[i]],8);
          m_qm.ChromaIntraQMatrix[i] = qm->ChromaIntraQMatrix[i];
          _InvIntraChromaQM[i] = 1.f/(mfxF32)qm->ChromaIntraQMatrix[i];
          _IntraChromaQM[i] = qm->ChromaIntraQMatrix[i];
        }
      }
      if(m_info.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420+0 ||
        ((sameflag[1] & 2) && (sameflag[3] & 2)) ||  // both unchanged
        (!(sameflag[1] & 2) && (sameflag[3] & 1))) { // both changed to same
          PUT_BITS(0,1);
      } else {
        PUT_BITS(1,1);
        InvInterChromaQM = _InvInterChromaQM;
        InterChromaQM = _InterChromaQM;
        for(i=0; i<64; i++) {
          PUT_BITS(qm->ChromaIntraQMatrix[mfx_mpeg2_ZigZagScan[i]],8);
          m_qm.ChromaIntraQMatrix[i] = qm->ChromaIntraQMatrix[i];
          _InvInterChromaQM[i] = 1.f/(mfxF32)qm->ChromaInterQMatrix[i];
          _InterChromaQM[i] = qm->ChromaInterQMatrix[i];
        }
      }
    }
    // else don't need qm extension
  }

  *outbb = bb;
  return MFX_ERR_NONE;
}

#endif // MFX_ENABLE_MPEG2_VIDEO_ENCODER
