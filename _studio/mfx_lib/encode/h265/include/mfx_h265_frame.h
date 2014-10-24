//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_FRAME_H__
#define __MFX_H265_FRAME_H__

#include <list>
#include <vector>
#include "vm_interlocked.h"
#include "umc_mutex.h"
#include "mfx_h265_defs.h"
#include "mfx_h265_set.h"

namespace H265Enc {

    struct H265VideoParam;

    class State
    {
    public:
        State()
            : m_free(true)
        {
        }

        bool IsFree() const
        {
            return m_free;
        }

        void SetFree(bool free)
        {
            m_free = free;
        }

    private:
        bool m_free; // resource in use or could be reused (m_free == true means could be reused).
    };

    struct BiFrameLocation
    {
        BiFrameLocation() 
            : miniGopCount(0), encodingOrder(0), refFrameFlag(0) {}

        mfxU32 miniGopCount;    // sequence of B frames between I/P frames
        mfxU32 encodingOrder;   // number within mini-GOP (in encoding order)
        mfxU16 refFrameFlag;    // MFX_FRAMETYPE_REF if B frame is reference
    };

    struct H265CUData;
    class H265Frame// : public State
    {
    public:

        void *mem;
        H265CUData *cu_data;
        Ipp8u *y;
        Ipp8u *uv;

        Ipp32s width;
        Ipp32s height;
        Ipp32s padding;
        Ipp32s pitch_luma_pix;
        Ipp32s pitch_luma_bytes;
        Ipp32s pitch_chroma_pix;
        Ipp32s pitch_chroma_bytes;
        Ipp8u  m_bitDepthLuma;
        Ipp8u  m_bdLumaFlag;
        Ipp8u  m_bitDepthChroma;
        Ipp8u  m_bdChromaFlag;
        Ipp8u  m_chromaFormatIdc;

        mfxU64 m_timeStamp;
        Ipp32u m_picCodeType;
        Ipp32s m_RPSIndex;
        Ipp8u  m_wasLookAheadProcessed;
        Ipp32s m_pyramidLayer;
        Ipp32s m_miniGopCount;
        Ipp32s m_biFramesInMiniGop;
        Ipp32s m_frameOrder;
        Ipp32s m_frameOrderOfLastIdr;
        Ipp32s m_frameOrderOfLastIntra;
        Ipp32s m_frameOrderOfLastAnchor;
        Ipp32s m_poc;
        Ipp32s m_encOrder;
        Ipp8u  m_isShortTermRef;
        Ipp8u  m_isLongTermRef;
        Ipp8u  m_isIdrPic;
        Ipp8u  m_isRef;
        RefPicList m_refPicList[2]; // 2 reflists containing reference frames used by current frame
        Ipp32s m_mapRefIdxL1ToL0[MAX_NUM_REF_IDX];
        Ipp32s m_mapListRefUnique[2][MAX_NUM_REF_IDX];
        Ipp32s m_numRefUnique;
        Ipp32s m_allRefFramesAreFromThePast;

        volatile Ipp32s m_codedRow; // sync info in case of frame threading
        volatile Ipp32u m_refCounter; // to prevent race condition in case of frame threading

        H265Frame()
        {
            ResetMemInfo();
            ResetEncInfo();
            ResetCounters();
        }

        ~H265Frame() { Destroy(); mem = NULL;}

        Ipp8u wasLAProcessed()    { return m_wasLookAheadProcessed; }
        void setWasLAProcessed() { m_wasLookAheadProcessed = true; }
        void unsetWasLAProcessed() { m_wasLookAheadProcessed = false; }

        bool IsReference() const
        {
            return (isShortTermRef() || isLongTermRef() );
        }

        Ipp8u isShortTermRef() const
        {
            return m_isShortTermRef;
        }

        Ipp8u isLongTermRef() const
        {
            return m_isLongTermRef;
        }

        void Create(H265VideoParam *par);
        void CopyFrame(const mfxFrameSurface1 *surface);
        void doPadding();

        void Destroy();
        //void Dump(const vm_char* fname, H265VideoParam *par, TaskList * dpb, Ipp32s frame_num);

        void ResetMemInfo();
        void ResetEncInfo();
        void ResetCounters();
        void CopyEncInfo(const H265Frame* src);

        void AddRef(void)  { vm_interlocked_inc32(&m_refCounter);};
        void Release(void) { vm_interlocked_dec32(&m_refCounter);}; // !!! here not to delete in case of refCounter == 0 !!!
    };


    struct Task
    {
        H265Frame* m_frameOrigin;
        H265Frame* m_frameRecon;

        Ipp32u     m_encOrder;
        Ipp32u     m_frameOrder;
        Ipp64u     m_timeStamp;
        //Ipp32u     m_picCodeType;
        Ipp32u     m_frameType;// full info for bs. m_frameType = m_codeType | isIDR ? | isRef ? 

        H265Slice  m_slices[10];//aya tmp hack
        H265Frame* m_dpb[16];
        Ipp32s     m_dpbSize;

        Ipp8s     m_sliceQpY;
        std::vector<Ipp8s> m_lcuQps; // array for LCU QPs
        H265Slice m_dqpSlice[2*MAX_DQP+1];

        H265ShortTermRefPicSet m_shortRefPicSet[66];
        mfxBitstream *m_bs;

        // quick wa for GAAC
        void*      m_extParam;

        // for frame parallel
        volatile Ipp32u m_ready;  // 0 - task submitted to FrameEncoder (not run!!), 1 - FrameEncoder was run (resource assigned), 2 - task ready, 7 - need repack
        Ipp32s m_encIdx; // we have "N" frameEncoders. this index indicates owner of the task [0, ..., N-1]

        // for threading control
        std::vector<Ipp32u> m_ithreadPool;


        Task()
            : m_frameOrigin(NULL)
            , m_frameRecon(NULL)
            , m_encOrder(Ipp32u(-1))
            , m_frameOrder(Ipp32u(-1))
            , m_timeStamp(0)
        {
            m_bs       = NULL;
            m_extParam = NULL;
            m_ready = 0;
            m_encIdx= -1;
        }

        ~Task() { Destroy(); }

        void Reset()
        {
            m_frameOrigin = NULL;
            m_frameRecon  = NULL;
            m_bs          = NULL;
            m_encOrder    = Ipp32u(-1);
            m_frameOrder  = Ipp32u(-1);
            m_timeStamp   = 0;
            m_bs       = NULL;
            m_extParam = NULL;
            m_ready = 0;
            m_encIdx= -1;
            m_dpbSize     = 0;
        }

        void Create(Ipp32u numCtb, Ipp32u numThreadStructs)
        {
            m_lcuQps.resize(numCtb);
            m_ithreadPool.resize(numThreadStructs, 0);
        }

        void Destroy()
        {
            m_lcuQps.resize(0);
            m_ithreadPool.resize(0);
        }
    };

    typedef std::list<Task*> TaskList;
    typedef std::list<Task*>::iterator   TaskIter;

    typedef std::list<H265Frame*> FramePtrList;
    typedef std::list<H265Frame*>::iterator   FramePtrIter;


    FramePtrIter GetFreeFrame(FramePtrList & queue, H265VideoParam *par);
    void Dump(const vm_char* fname, H265VideoParam *par, H265Frame* frame, TaskList & dpb);

} // namespace

#endif // __MFX_H265_FRAME_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
