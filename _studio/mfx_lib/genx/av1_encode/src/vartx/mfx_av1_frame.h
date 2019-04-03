// Copyright (c) 2014-2019 Intel Corporation
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

#pragma once

#include <memory>
#include <list>
#include <vector>
#include "mfx_av1_defs.h"
#include "mfx_av1_bit_count.h"

namespace H265Enc {

    struct H265VideoParam;

    struct RefCounter
    {
        RefCounter()
            : m_refCounter(0)
        {}
        volatile uint32_t m_refCounter; // to prevent race condition in case of frame threading
        void AddRef(void)  { m_refCounter++;/*vm_interlocked_inc32(&m_refCounter);*/};
        void Release(void) { m_refCounter--;/*vm_interlocked_dec32(&m_refCounter);*/}; // !!! here not to delete in case of refCounter == 0 !!!
    };

    struct Statistics : public RefCounter
    {
        std::vector<int32_t> m_interSad;
        std::vector<int32_t> m_interSad_pdist_past;
        std::vector<int32_t> m_interSad_pdist_future;

        std::vector<H265MV> m_mv;
        std::vector<H265MV> m_mv_pdist_future;
        std::vector<H265MV> m_mv_pdist_past;

        int32_t m_pitchRsCs4;
        int32_t m_rcscSize[5];
        int32_t* m_rs[5];
        int32_t* m_cs[5];
        std::vector<double> rscs_ctb;
        std::vector<int32_t> sc_mask;
        std::vector<int32_t> qp_mask;
        std::vector<int32_t> coloc_past;
        std::vector<int32_t> coloc_futr;

        std::vector<int32_t> m_intraSatd;
        std::vector<int32_t> m_interSatd;

        double m_frameRs;
        double m_frameCs;
        double SC;
        double TSC;
        double avgsqrSCpp;
        double avgTSC;
        float m_avgBestSatd; //= sum( min{ interSatd[i], intraSatd[i] } ) / {winth*height};
        float m_avgIntraSatd;
        float m_avgInterSatd;
        float m_intraRatio;

        // SceneCut info
        int32_t m_sceneCut;
        int64_t m_metric;// special metric per frame for sceneCut based on +10 frames lookahead

        void ResetAvgMetrics()
        {
            m_frameRs = 0.0;
            m_frameCs = 0.0;
            SC = 0.0;
            TSC = 0.0;
            avgsqrSCpp = 0.0;
            avgTSC = 0.0;
            m_avgBestSatd = 0.f;
            m_avgIntraSatd = 0.f;
            m_intraRatio = 0.f;
            m_sceneCut = 0;
            m_metric    = 0;
        }

        struct AllocInfo { AllocInfo() : width(0), height(0) {} int32_t width, height; };
        Statistics() { Zero(m_rs); Zero(m_cs); };
        void Create(const AllocInfo &allocInfo);
        ~Statistics() { Destroy(); }
        void Destroy();
    };

    struct SceneStats : public RefCounter, public NonCopyable
    {
        // padded (+2 pix) Luma
        __ALIGN16 uint8_t  data[8192]; // aligned pow(2,x) -> extH( 64 + 2*(2)) * extWidth (112 + 2*2)
        uint8_t* Y;

        H265MV mv[112 /*NUM_BLOCKS*/       ];
        float Cs[448 /*LOWRES_SIZE / 16*/ ];
        float Rs[448 /*LOWRES_SIZE / 16*/ ];

        float   avgCs;
        float   avgRs;

        float   AFD;
        float   TSC;
        float   RsCsDiff;
        float   MVdiffVal;

        struct AllocInfo { AllocInfo() : width(0), height(0) {} int32_t width, height; };
        void Create(const AllocInfo &allocInfo);
        ~SceneStats() { Destroy(); }
        void Destroy(){}
    };

    struct FrameData : public RefCounter, public NonCopyable
    {
        uint8_t *y;
        uint8_t *uv;
        void *m_handle;
        int32_t width;
        int32_t height;
        int32_t padding;
        int32_t pitch_luma_pix;
        int32_t pitch_luma_bytes;
        int32_t pitch_chroma_pix;
        int32_t pitch_chroma_bytes;

        FrameData() : y(), uv(), m_handle(), mem(), m_fei() {}
        ~FrameData() { Destroy(); }

        struct AllocInfo {
            AllocInfo()  { Zero(*this); }
            int32_t width;       // in elements
            int32_t height;      // in elements
            int32_t paddingLu;   // in elements
            int32_t paddingChW;  // in elements
            int32_t paddingChH;  // in elements
            int32_t bitDepthLu;
            int32_t bitDepthCh;
            int32_t chromaFormat;
            // for GACC should be requested from CmDevice
            void  *feiHdl;
            int32_t alignment;
            int32_t pitchInBytesLu;
            int32_t pitchInBytesCh;
            int32_t sizeInBytesLu;
            int32_t sizeInBytesCh;
        };

        void Create(const AllocInfo &allocInfo);
        void Destroy();

    private:
        uint8_t *mem;
        void  *m_fei;
    };

    struct FeiInputData : public RefCounter
    {
        FeiInputData() : m_fei(NULL), m_handle(NULL) {}
        ~FeiInputData() { Destroy(); }

        struct AllocInfo { AllocInfo() : feiHdl(NULL) {} void *feiHdl; };
        void Create(const AllocInfo &allocInfo);
        void Destroy();

        void *m_fei;
        void *m_handle;
    };

    struct FeiReconData : public RefCounter
    {
        FeiReconData() : m_fei(NULL), m_handle(NULL) {}
        ~FeiReconData() { Destroy(); }

        struct AllocInfo { AllocInfo() : feiHdl(NULL) {} void *feiHdl; };
        void Create(const AllocInfo &allocInfo);
        void Destroy();

        void *m_fei;
        void *m_handle;
    };

    struct FeiBufferUp : public RefCounter
    {
        FeiBufferUp() : m_fei(), m_handle(), m_allocated(), m_sysmem() {}
        ~FeiBufferUp() { Destroy(); }
        struct AllocInfo { AllocInfo() {Zero(*this);}  void *feiHdl; uint32_t size, alignment; };
        void Create(const AllocInfo &allocInfo);
        void Destroy();

        void  *m_fei;
        void *m_handle;
        uint8_t *m_allocated;
        uint8_t *m_sysmem;
    };

    struct TileContexts {
        TileContexts();
        ~TileContexts() { Free(); }
        void Alloc(int32_t tileSb64Cols, int32_t tileSb64Rows);
        void Free();
        void Clear();
        EntropyContexts ectx;
        int32_t tileSb64Cols;
        int32_t tileSb64Rows;
        uint8_t *aboveNonzero[3];
        uint8_t *leftNonzero[3];
        uint8_t *abovePartition;
        uint8_t *leftPartition;
        uint8_t *aboveTxfm;
        uint8_t *leftTxfm;
#ifdef ADAPTIVE_DEADZONE
        ADzCtx *adzctx[3];
#endif
    };

    class H265FrameEncoder;
    struct ModeInfo;
    struct AV1MvRefs;

    typedef std::list<Frame*> FrameList;
    typedef std::list<Frame*>::iterator   FrameIter;

    void Dump(H265VideoParam *par, Frame* frame, FrameList & dpb);

    void PadRectLuma         (const FrameData &fdata, int32_t fourcc, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth);
    void PadRectChroma       (const FrameData &fdata, int32_t fourcc, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth);
    void PadRectLumaAndChroma(const FrameData &fdata, int32_t fourcc, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth);

    template <class T> class ObjectPool
    {
    public:
        ObjectPool() {}
        ~ObjectPool() { Destroy(); }

        void Destroy() {
            for (typename std::vector<T *>::iterator i = m_objects.begin(); i != m_objects.end(); ++i)
                delete *i;
            m_objects.resize(0);
        }

        void Init(const typename T::AllocInfo &allocInfo, int32_t numPrealloc)
        {
            assert(m_objects.size() == 0);
            m_allocInfo = allocInfo;

            for (int32_t i = 0; i < numPrealloc; i++)
                Allocate();
            for (typename std::vector<T*>::iterator i = m_objects.begin(); i != m_objects.end(); ++i)
                (*i)->Release();

        }

        const typename T::AllocInfo &GetAllocInfo() const { return m_allocInfo; }

        T *Allocate()
        {
            for (typename std::vector<T*>::iterator i = m_objects.begin(); i != m_objects.end(); ++i)
                if (vm_interlocked_cas32(&(*i)->m_refCounter, 1, 0) == 0)
                    return *i;

            std::unique_ptr<T> newFrame(new T());
            newFrame->Create(m_allocInfo);
            newFrame->AddRef();
            m_objects.push_back(newFrame.release());
            return *(--m_objects.end());
        }

    protected:
        std::vector<T *> m_objects;
        typename T::AllocInfo m_allocInfo;
    };

    template <class T> void SafeRelease(T *&obj)
    {
        if (obj) {
            obj->Release();
            obj = NULL;
        }
    }

} // namespace
