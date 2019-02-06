// Copyright (c) 2008-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)

#include "mfxvideo++int.h"
#include "umc_mpeg2_enc.h"
#include "mfx_frames.h"
#include "vm_event.h"
#include "mfx_enc_common.h"
#include "mfx_mpeg2_enc_common.h"
#include <mutex>

class MFXVideoENCODEMPEG2;

#ifdef _NEW_THREADING

struct sIntTaskInfo
{
  Ipp32s    numTask;
  MFXVideoENCODEMPEG2 *m_lpOwner;
  vm_event  task_prepared_event;
  vm_event  task_ready_event;
};

/*-------------- Internal tasks for threading ---------------------*/

struct sIntTasks
{
    UMC::MPEG2VideoEncoderBase::threadSpecificData *       m_TaskSpec;
    sIntTaskInfo  *                                        m_TaskInfo;
    mfxU32                                                 m_NumTasks;
    mfxU32                                                 m_CurrTask;
    std::mutex                                             m_mGuard;
    vm_event                                               m_exit_event;

    sIntTasks()
    {
        m_TaskSpec = 0;
        m_TaskInfo = 0;
        m_NumTasks = 0;
        m_CurrTask = 0;
        vm_event_set_invalid (&m_exit_event);        
    }

    mfxStatus GetIntTask(mfxU32 & nTask)
    {
        std::lock_guard<std::mutex> guard(m_mGuard);
        if (m_CurrTask < m_NumTasks)
        {
            mfxStatus sts = MFX_ERR_NONE;
            if (VM_TIMEOUT != vm_event_timed_wait (&m_TaskInfo[m_CurrTask].task_prepared_event,0))
            {
                nTask = m_CurrTask;
                m_CurrTask ++;
            }
            else
            {
                sts = MFX_ERR_MORE_DATA;
            }
            return sts;
        }
        return MFX_ERR_NOT_FOUND;            
    }
};
#endif 


class MFXVideoENCODEMPEG2 : public VideoENCODE {
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXVideoENCODEMPEG2(VideoCORE *core, mfxStatus *sts);
    virtual ~MFXVideoENCODEMPEG2() {Close();}
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);
    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat);
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams);
    virtual mfxStatus EncodeFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *surface, mfxBitstream *bs);
    virtual mfxStatus CancelFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *surface, mfxBitstream *bs);


protected:
    mfxStatus ResetImpl(mfxVideoParam *par);

private:
    UMC::MPEG2VideoEncoderBase m_codec;
    VideoCORE* m_core;
    UMC::VideoBrc *m_brc;
    bool   m_bInitialized;
    SHParametersEx *pSHEx;

    /*struct Surface1Cake { // is used to save allocations
      mfxFrameSurface1 surface1;
      mfxFrameData     data;
      mfxFrameData     *ptr;
    };*/

    struct Input {
      mfxFrameSurface1*  buf_ref[2]; // refs to frame pointers [fwd/bwd], swap [0] and [1] to rotate
      mfxU32             FrameOrderRef[2];
#ifdef ME_REF_ORIGINAL
      // to access original: buf_org[fwd/bwd].
      mfxFrameSurface1*  buf_org[2]; // refs to original pointers [fwd/bwd], swap [0] and [1] to rotate
#endif /* ME_REF_ORIGINAL */

      mfxFrameSurface1*  buf_aux;    // to be used to copy from video memory

      bool           bCopyInputFrames;
      bool           bFirstFrame;

    } frames;

    /* those objects are used for reordening in EncodeFrameCheck */
    MFXGOP*                         m_pGOP;
    MFXWaitingList*                 m_pWaitingList;

    mfxU16            m_GopOptFlag;
    mfxU32            m_IdrInterval;
    bool              m_bAddEOS;
    bool              m_bAddDisplayExt;
    mfxU16            m_IOPattern;
    mfxU16            m_TargetUsage;

    mfxI32            m_InputFrameOrder;
    mfxI32            m_OutputFrameOrder;
    mfxI32            m_nEncodeCalls;
    mfxI32            m_nFrameInGOP;
    mfxU64            m_BitstreamLen;
    mfxU32            m_nThreads;
    
    mfxFrameSurface1*  m_pFramesStore;
    mfxU32             m_nFrames;

    mfxU32             m_bConstantQuant;

    mfxU16         m_picStruct;
    mfxU32         m_BufferSizeInKB;

    mfxU16         m_AspectRatioW;
    mfxU16         m_AspectRatioH;

    mfxU16         m_Width;
    mfxU16         m_Height;

    void           ConfigPolarSurface();

    InputSurfaces m_InputSurfaces;

    mfxFrameSurface1 *GetOriginalSurface(mfxFrameSurface1 *surface)
    {
        return m_InputSurfaces.GetOriginalSurface(surface);
    }


    mfxStatus GetFrame(mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *in, mfxBitstream* bs);
    mfxStatus EncodeFrameReordered(mfxEncodeInternalParams *pInternalParams);
    mfxStatus ReorderFrame(mfxEncodeInternalParams *pInInternalParams, mfxFrameSurface1 *in,
                           mfxEncodeInternalParams *pOutInternalParams, mfxFrameSurface1 **out);


    mfxFrameAllocRequest m_request;   // for reconsruction frames
    mfxFrameAllocResponse m_response; // for reconstruction frames
    
    mfxU32 is_initialized() const {return m_bInitialized;}

#ifdef _NEW_THREADING
    mfxStatus   PutPicture(mfxPayload **pPayloads, mfxU32 numPayloads, bool bEvenPayloads, bool bOddPayloads);
    mfxStatus   CreateTaskInfo ();
    void        DeleteTaskInfo();

public:
    clExtTasks1*   m_pExtTasks;
    sIntTasks         m_IntTasks;
 
    void StartIntTask (mfxU32 nTask)
    {
       MFX::AutoTimer timer("MPEG2Enc_Task");

       m_codec.encodePicture(nTask);
       vm_event_signal(&m_IntTasks.m_TaskInfo[nTask].task_ready_event);

    }
    mfxStatus AbortEncoding ()
    {
        vm_event_signal(&m_IntTasks.m_exit_event);
        return MFX_ERR_NONE;
    }
    virtual
    mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl,
                               mfxFrameSurface1 *surface,
                               mfxBitstream *bs,
                               mfxFrameSurface1 **reordered_surface,
                               mfxEncodeInternalParams *pInternalParams,
                               MFX_ENTRY_POINT *pEntryPoint);
    virtual
    mfxTaskThreadingPolicy GetThreadingPolicy(void) {return MFX_TASK_THREADING_INTRA;}


#endif
};

#endif // MFX_ENABLE_MPEG2_VIDEO_ENCODE
