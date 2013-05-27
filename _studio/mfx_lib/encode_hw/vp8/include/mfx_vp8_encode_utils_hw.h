/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE) && defined(MFX_VA)

#ifndef _MFX_VP8_ENCODE_UTILS_HW_H_
#define _MFX_VP8_ENCODE_UTILS_HW_H_

#include <vector>
#include <memory>

#include "mfxstructures.h"
#include "mfx_enc_common.h"

namespace MFX_VP8ENC
{

    struct sFrameParams
    {
        bool    bIntra;
        bool    bGold;
        bool    bAltRef;
    };

    enum eTaskStatus
    {
        TASK_FREE =0,
        TASK_INITIALIZED,
        TASK_SUBMITTED,
        READY    
    };
    enum eRefFrames
    {
        REF_BASE = 0,
        REF_GOLD = 1,
        REF_ALT  = 2,   
    };
    enum
    {
        MFX_MEMTYPE_D3D_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME,
        MFX_MEMTYPE_D3D_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME,
        MFX_MEMTYPE_SYS_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY        | MFX_MEMTYPE_EXTERNAL_FRAME,
        MFX_MEMTYPE_SYS_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY        | MFX_MEMTYPE_INTERNAL_FRAME
    };
    static const mfxU16 MFX_IOPATTERN_IN_MASK_SYS_OR_D3D =
        MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY;

    static const mfxU16 MFX_IOPATTERN_IN_MASK =
        MFX_IOPATTERN_IN_MASK_SYS_OR_D3D | MFX_IOPATTERN_IN_OPAQUE_MEMORY;

    struct sFrameEx
    {
        mfxFrameSurface1 *pSurface;
        mfxU32            idInPool;
    };

    inline mfxStatus LockSurface(sFrameEx*  pFrame, VideoCORE* pCore)
    {
        return (pFrame) ? pCore->IncreaseReference(&pFrame->pSurface->Data) : MFX_ERR_NONE; 
    }
    inline mfxStatus FreeSurface(sFrameEx* &pFrame, VideoCORE* pCore)
    {        
        mfxStatus sts = MFX_ERR_NONE;
        if (pFrame)
        {
            sts = pCore->DecreaseReference(&pFrame->pSurface->Data);
            pFrame = 0;
        }
        return sts;   
    }
    inline bool isFreeSurface (sFrameEx* pFrame)
    {
        return (pFrame->pSurface->Data.Locked == 0);
    }
    
    inline bool isOpaq(mfxVideoParam const & video)
    {
        return (video.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)!=0;
    }


    inline mfxU32 CalcNumTasks(mfxVideoParam const & video)
    {
        return video.AsyncDepth + 1;
    }

    inline mfxU32 CalcNumSurfRawLocal(mfxVideoParam const & video, bool bHWImpl, bool bHWInput,bool bRawReference = false)
    {
        return (bHWInput !=  bHWImpl) ? CalcNumTasks(video) + ( bRawReference ? 3 : 0 ):0;
    }

    inline mfxU32 CalcNumSurfRecon(mfxVideoParam const & video)
    {
        return 3 + CalcNumTasks(video);
    }
    inline mfxU32 CalcNumMB(mfxVideoParam const & video)
    {
        return (video.mfx.FrameInfo.Width>>4)*(video.mfx.FrameInfo.Height>>4);    
    }
    inline bool CheckPattern(mfxU32 inPattern)
    {
        inPattern = inPattern & MFX_IOPATTERN_IN_MASK;
        return ( inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
                 inPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY ||
                 inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY);           
    }
    inline bool CheckFrameSize(mfxU32 Width, mfxU32 Height)
    {
        return ( ((Width & 0x0f) == 0) && ((Height& 0x0f) == 0) &&
                 (Width > 0)  && (Width < 0x1FFF) &&
                 (Height > 0) && (Height < 0x1FFF));
    }


    bool isVideoSurfInput(mfxVideoParam const & video);   
    mfxStatus CheckEncodeFrameParam(mfxVideoParam const & video,
                                    mfxEncodeCtrl       * ctrl,
                                    mfxFrameSurface1    * surface,
                                    mfxBitstream        * bs,
                                    bool                  isExternalFrameAllocator);

    mfxStatus GetVideoParam(mfxVideoParam * parDst, mfxVideoParam *videoSrc);
    mfxStatus SetFramesParams(mfxVideoParam * par, mfxU32 frameOrder, sFrameParams *pFrameParams);


    /*------------------------ Utils------------------------------------------------------------------------*/

    template<class T> inline void Zero(T & obj)                { memset(&obj, 0, sizeof(obj)); }
    template<class T> inline void Zero(std::vector<T> & vec)   { memset(&vec[0], 0, sizeof(T) * vec.size()); }
    template<class T> inline void Zero(T * first, size_t cnt)  { memset(first, 0, sizeof(T) * cnt); }

    /* copied from H.264 */
    //-----------------------------------------------------
    // Helper which checks number of allocated frames and auto-free.
    //-----------------------------------------------------
    template<class T> struct ExtBufTypeToId {};

#define BIND_EXTBUF_TYPE_TO_ID(TYPE, ID) template<> struct ExtBufTypeToId<TYPE> { enum { id = ID }; }
    BIND_EXTBUF_TYPE_TO_ID (mfxExtCodingOptionVP8,  MFX_EXTBUFF_VP8_EX_CODING_OPT            );
    BIND_EXTBUF_TYPE_TO_ID (mfxExtOpaqueSurfaceAlloc,MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
#undef BIND_EXTBUF_TYPE_TO_ID

    template <class T> inline void InitExtBufHeader(T & extBuf)
    {
        Zero(extBuf);
        extBuf.Header.BufferId = ExtBufTypeToId<T>::id;
        extBuf.Header.BufferSz = sizeof(T);
    }

    template <class T> struct GetPointedType {};
    template <class T> struct GetPointedType<T *> { typedef T Type; };
    template <class T> struct GetPointedType<T const *> { typedef T Type; };

    struct mfxExtBufferProxy
    {
    public:
        template <typename T> operator T()
        {
            mfxExtBuffer * p = GetExtBuffer(
                m_extParam,
                m_numExtParam,
                ExtBufTypeToId<typename GetPointedType<T>::Type>::id);
            return reinterpret_cast<T>(p);
        }

        template <typename T> friend mfxExtBufferProxy GetExtBuffer(const T & par);

    protected:
        mfxExtBufferProxy(mfxExtBuffer ** extParam, mfxU32 numExtParam)
            : m_extParam(extParam)
            , m_numExtParam(numExtParam)
        {
        }

    private:
        mfxExtBuffer ** m_extParam;
        mfxU32          m_numExtParam;
    };

    template <typename T> mfxExtBufferProxy GetExtBuffer(T const & par)
    {
        return mfxExtBufferProxy(par.ExtParam, par.NumExtParam);
    }

    class MfxFrameAllocResponse : public mfxFrameAllocResponse
    {
    public:
        MfxFrameAllocResponse();

        ~MfxFrameAllocResponse();

        mfxStatus Alloc(
            VideoCORE *            core,
            mfxFrameAllocRequest & req);

        mfxStatus Alloc(
            VideoCORE *            core,
            mfxFrameAllocRequest & req,
            mfxFrameSurface1 **    opaqSurf,
            mfxU32                 numOpaqSurf);

        mfxFrameInfo               m_info;

    private:
        MfxFrameAllocResponse(MfxFrameAllocResponse const &);
        MfxFrameAllocResponse & operator =(MfxFrameAllocResponse const &);

        VideoCORE * m_core;
        mfxU16      m_numFrameActualReturnedByAllocFrames;

        std::vector<mfxFrameAllocResponse> m_responseQueue;
        std::vector<mfxMemId>              m_mids;        
    }; 

    struct FrameLocker
    {
        FrameLocker(VideoCORE * core, mfxFrameData & data, bool external = false)
            : m_core(core)
            , m_data(data)
            , m_memId(data.MemId)
            , m_status(Lock(external))
        {
        }

        FrameLocker(VideoCORE * core, mfxFrameData & data, mfxMemId memId, bool external = false)
            : m_core(core)
            , m_data(data)
            , m_memId(memId)
            , m_status(Lock(external))
        {
        }

        ~FrameLocker() { Unlock(); }

        mfxStatus Unlock()
        {
            mfxStatus mfxSts = MFX_ERR_NONE;

            if (m_status == LOCK_INT)
                mfxSts = m_core->UnlockFrame(m_memId, &m_data);
            else if (m_status == LOCK_EXT)
                mfxSts = m_core->UnlockExternalFrame(m_memId, &m_data);

            m_status = LOCK_NO;
            return mfxSts;
        }

    protected:
        enum { LOCK_NO, LOCK_INT, LOCK_EXT };

        mfxU32 Lock(bool external)
        {
            mfxU32 status = LOCK_NO;

            if (m_data.Y == 0)
            {
                status = external
                    ? (m_core->LockExternalFrame(m_memId, &m_data) == MFX_ERR_NONE ? LOCK_EXT : LOCK_NO)
                    : (m_core->LockFrame(m_memId, &m_data) == MFX_ERR_NONE ? LOCK_INT : LOCK_NO);
            }

            return status;
        }

    private:
        FrameLocker(FrameLocker const &);
        FrameLocker & operator =(FrameLocker const &);

        VideoCORE *    m_core;
        mfxFrameData & m_data;
        mfxMemId       m_memId;
        mfxU32         m_status;
    };

    class VP8MfxParam : public mfxVideoParam
    {
    public:
        VP8MfxParam();
        VP8MfxParam(VP8MfxParam const &);
        VP8MfxParam(mfxVideoParam const &);

        VP8MfxParam & operator = (VP8MfxParam const &);
        VP8MfxParam & operator = (mfxVideoParam const &);


    protected:
        void Construct(mfxVideoParam const & par);


    private:
        mfxExtBuffer *              m_extParam[2];
        mfxExtCodingOptionVP8       m_extOpt;        
        mfxExtOpaqueSurfaceAlloc    m_extOpaque; 
    };
    

    class ExternalFrames
    {
    protected:
        std::vector<sFrameEx>  m_frames;
    public:
        ExternalFrames() {}
        void Init();
        mfxStatus GetFrame(mfxFrameSurface1 *pInFrame, sFrameEx *&pOutFrame);     
     };


    class InternalFrames
    {
    protected:    
        std::vector<sFrameEx>           m_frames;
        MfxFrameAllocResponse           m_response;
        std::vector<mfxFrameSurface1>   m_surfaces;
    public:
        InternalFrames() {}
        mfxStatus Init(VideoCORE *pCore, mfxFrameInfo *pFrameInfo, mfxU32 nFrames, bool bHW);
        sFrameEx * GetFreeFrame();
        mfxStatus  GetFrame(mfxU32 numFrame, sFrameEx * &Frame);

        inline mfxU16 Height()
        {
            return m_response.m_info.Height;
        }
        inline mfxU16 Width()
        {
            return m_response.m_info.Width;
        }
        inline mfxU32 Num()
        {
            return m_response.NumFrameActual;
        }
        inline MfxFrameAllocResponse& GetFrameAllocReponse()  {return m_response;}
    };

    class Task
    {
    public:

        eTaskStatus          m_status;
        sFrameEx            *m_pRawFrame;
        sFrameEx            *m_pRawLocalFrame;        
        mfxBitstream        *m_pBitsteam;
        VideoCORE           *m_pCore;

        sFrameParams         m_sFrameParams;
        sFrameEx            *m_pRecFrame;
        sFrameEx            *m_pRecRefFrames[3];
        bool                 m_bOpaqInput;
        mfxU32              m_frameOrder;


    protected:

        mfxStatus CopyInput();

    public:

        Task ():
          m_status(TASK_FREE),
              m_pRawFrame(0),
              m_pRawLocalFrame(0),
              m_pBitsteam(0),
              m_pRecFrame(0),
              m_pCore(0),
              m_bOpaqInput(false),
              m_frameOrder (0)
          {
              Zero(m_pRecRefFrames);  
              Zero(m_sFrameParams);
          }
          virtual
          ~Task ()
          {
              FreeTask();
          }
          mfxStatus GetOriginalSurface(mfxFrameSurface1 *& pSurface, bool &bExternal);
          mfxStatus GetInputSurface(mfxFrameSurface1 *& pSurface, bool &bExternal);

          virtual
          mfxStatus Init(VideoCORE * pCore, mfxVideoParam *par);
          
          virtual
          mfxStatus InitTask( sFrameEx     *pRawFrame, 
                              mfxBitstream *pBitsteam,
                              mfxU32        frameOrder);
          virtual
          mfxStatus SubmitTask (sFrameEx*  pRecFrame, Task* pPrevTask, sFrameParams* pParams, sFrameEx* pRawLocalFrame);
 
          inline mfxStatus CompleteTask ()
          {
              m_status = READY;
              return MFX_ERR_NONE;
          }
          inline bool isReady()
          {
            return m_status == READY;
          }

          virtual 
          mfxStatus FreeTask();

          inline bool isFreeTask()
          {
              return (m_status == TASK_FREE);        
          }
    };



    template <class TTask>
    inline TTask* GetFreeTask(std::vector<TTask> & tasks)
    {
        std::vector<TTask>::iterator task = tasks.begin();
        for (;task != tasks.end(); task ++)
        {
            if (task->isFreeTask())
            {
                return &task[0]; 
            }            
        } 
        return 0;
    }
    template <class TTask>
    inline mfxStatus FreeTasks(std::vector<TTask> & tasks)
    {
        mfxStatus sts = MFX_ERR_NONE;
        std::vector<TTask>::iterator task = tasks.begin();
        for (;task != tasks.end(); task ++)
        {
            if (!task->isFreeTask())
            {
                sts = task->FreeTask(); 
                MFX_CHECK_STS(sts);
            }            
        } 
        return sts;
    }

    /*for Full Encode (SW or HW)*/
    template <class TTask>
    class TaskManager
    {
    protected:

        VideoCORE*       m_pCore;
        VP8MfxParam      m_video;

        bool    m_bHWFrames;
        bool    m_bHWInput;

        ExternalFrames  m_RawFrames;
        InternalFrames  m_LocalRawFrames;
        InternalFrames  m_ReconFrames;

        std::vector<TTask>  m_Tasks;
        Task*               m_pPrevSubmittedTask;
        mfxU32              m_frameNum;


    public:
        TaskManager ():
          m_bHWFrames(false),
          m_bHWInput(false),
          m_pCore(0),
          m_pPrevSubmittedTask(0),
          m_frameNum(0)
          {}

          ~TaskManager()
          {
            FreeTasks(m_Tasks);
          }

          mfxStatus Init(VideoCORE* pCore, mfxVideoParam *par, bool bHWImpl)
          {
              mfxStatus sts = MFX_ERR_NONE;

              MFX_CHECK(!m_pCore, MFX_ERR_UNDEFINED_BEHAVIOR);

              m_pCore   = pCore;
              m_video   = *par;
              m_bHWFrames = bHWImpl;              
              m_bHWInput = isVideoSurfInput(m_video);              
              m_pPrevSubmittedTask = 0;
              m_frameNum = 0;

              m_RawFrames.Init(); 

              sts = m_LocalRawFrames.Init(m_pCore, &m_video.mfx.FrameInfo, 
                  CalcNumSurfRawLocal(m_video, bHWImpl, m_bHWInput), 
                  m_bHWFrames);
              MFX_CHECK_STS(sts);

              sts = m_ReconFrames.Init(m_pCore, &m_video.mfx.FrameInfo, CalcNumSurfRecon(m_video), m_bHWFrames);
              MFX_CHECK_STS(sts);

              {
                  mfxU32 numTasks = CalcNumTasks(m_video);
                  m_Tasks.resize(numTasks);

                  for (mfxU32 i = 0; i < numTasks; i++)
                  {
                      sts = m_Tasks[i].Init(m_pCore,&m_video);
                      MFX_CHECK_STS(sts);
                  }
              }
              return sts;          
          }
          mfxStatus Reset(mfxVideoParam *par)
          {
              mfxStatus sts = MFX_ERR_NONE;
              MFX_CHECK(m_pCore!=0, MFX_ERR_NOT_INITIALIZED);  

              m_video   = *par;
              m_frameNum = 0;

              MFX_CHECK(m_ReconFrames.Height() >= m_video.mfx.FrameInfo.Height,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
              MFX_CHECK(m_ReconFrames.Width()  >= m_video.mfx.FrameInfo.Width,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

              MFX_CHECK(mfxU16(CalcNumSurfRawLocal(m_video,m_bHWFrames,isVideoSurfInput(m_video))) <= m_LocalRawFrames.Num(),MFX_ERR_INCOMPATIBLE_VIDEO_PARAM); 
              MFX_CHECK(mfxU16(CalcNumSurfRecon(m_video)) <= m_ReconFrames.Num(),MFX_ERR_INCOMPATIBLE_VIDEO_PARAM); 

              m_pPrevSubmittedTask = 0;
              sts = FreeTasks(m_Tasks);

              MFX_CHECK_STS(sts);              
              return MFX_ERR_NONE;
          }
          mfxStatus InitTask(mfxFrameSurface1* pSurface, mfxBitstream* pBitstream, Task* & pOutTask)
          {
              mfxStatus sts = MFX_ERR_NONE;
              Task*     pTask = GetFreeTask(m_Tasks);
              sFrameEx* pRawFrame = 0;

              MFX_CHECK(m_pCore!=0, MFX_ERR_NOT_INITIALIZED);
              MFX_CHECK(pTask!=0,MFX_WRN_DEVICE_BUSY);

              sts = m_RawFrames.GetFrame(pSurface, pRawFrame);
              MFX_CHECK_STS(sts);              

              sts = pTask->InitTask(pRawFrame,pBitstream, m_frameNum);
              MFX_CHECK_STS(sts);

              pOutTask = pTask;

              m_frameNum++;

              return sts;
          }
          mfxStatus SubmitTask(Task*  pTask, sFrameParams *pParams)
          {
              sFrameEx* pRecFrame = 0;
              sFrameEx* pRawLocalFrame = 0;
              mfxStatus sts = MFX_ERR_NONE;

              MFX_CHECK(m_pCore!=0, MFX_ERR_NOT_INITIALIZED);

              pRecFrame = m_ReconFrames.GetFreeFrame();
              MFX_CHECK(pRecFrame!=0,MFX_WRN_DEVICE_BUSY);

              if (m_bHWFrames != m_bHWInput)
              {
                  pRawLocalFrame = m_LocalRawFrames.GetFreeFrame();
                  MFX_CHECK(pRawLocalFrame!= 0,MFX_WRN_DEVICE_BUSY);
              }            

              sts = pTask->SubmitTask(pRecFrame,m_pPrevSubmittedTask,pParams, pRawLocalFrame);
              MFX_CHECK_STS(sts);

              if (m_pPrevSubmittedTask)
              {
                sts = m_pPrevSubmittedTask->FreeTask();
                MFX_CHECK_STS(sts);
              }

              m_pPrevSubmittedTask = pTask;

              return sts;          
          }         

    }; 
}
#endif
#endif