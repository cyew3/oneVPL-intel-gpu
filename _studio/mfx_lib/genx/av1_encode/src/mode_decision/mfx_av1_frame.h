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

    struct FrameData : public NonCopyable
    {
        uint8_t *y;
        uint8_t *uv;
        mfxHDL m_handle;
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

    struct TileContexts {
        TileContexts();
        ~TileContexts() { Free(); }
        void Alloc(int32_t tileSb64Cols, int32_t tileSb64Rows);
        void Free();
        void Clear();
        //EntropyContexts ectx;
        int32_t tileSb64Cols;
        int32_t tileSb64Rows;
        uint8_t *aboveNonzero[3];
        uint8_t *leftNonzero[3];
        uint8_t *abovePartition;
        uint8_t *leftPartition;
        uint8_t *aboveTxfm;
        uint8_t *leftTxfm;
    };

    class H265FrameEncoder;
    struct ModeInfo;
    struct AV1MvRefs;

    enum {
        MFX_FRAMETYPE_I = 0x0001,
        MFX_FRAMETYPE_P = 0x0002,
        MFX_FRAMETYPE_B = 0x0004,
    };

    template <typename T> struct pointer_like {
        T obj;

        operator const T*() const { return &obj; }
        operator       T*()       { return &obj; }

        const T *operator->() const { return &obj; }
              T *operator->()       { return &obj; }
    };

    struct FeiOutData {
        FeiOutData() : m_sysmem(nullptr), m_pitch(0) {}
        ~FeiOutData() { Free(); }

        uint8_t *m_sysmem;
        int32_t m_pitch;

        void Alloc(int32_t size, int32_t pitch) {
            m_sysmem = (uint8_t *)_aligned_malloc(size, 0x1000);
            m_pitch = pitch;
        }

        void Free() {
            _aligned_free(m_sysmem);
        }
    };

    struct FeiBufferUp {
        FeiBufferUp() : m_sysmem(nullptr) {}
        ~FeiBufferUp() { Free(); }

        uint8_t *m_sysmem;

        void Alloc(int32_t size) {
            m_sysmem = (uint8_t *)_aligned_malloc(size, 0x1000);
        }

        void Free() {
            _aligned_free(m_sysmem);
        }
    };

    class Frame
    {
    public:
        int32_t m_frameOrder; // keep it first, good for debug

        H265FrameEncoder *m_fenc; // valid while frame is being encoded
        FrameData *m_origin;
        FrameData *m_recon;
        FrameData *m_lowres; // used by lookahead

        ModeInfo *cu_data;

        uint8_t  m_bitDepthLuma;
        uint8_t  m_bdLumaFlag;
        uint8_t  m_bitDepthChroma;
        uint8_t  m_bdChromaFlag;
        uint8_t  m_chromaFormatIdc;

        int32_t m_picStruct;
        int32_t m_secondFieldFlag;
        int32_t m_bottomFieldFlag;
        mfxU64 m_timeStamp;
        uint32_t m_picCodeType;
        int32_t m_RPSIndex;

        // flags indicate that the stages were done on the frame
        uint8_t  m_wasLookAheadProcessed;
        uint32_t m_lookaheadRefCounter;

        int32_t m_pyramidLayer;
        int32_t m_miniGopCount;
        int32_t m_biFramesInMiniGop;
        int32_t m_frameOrderOfLastIdr;
        int32_t m_frameOrderOfLastIntra;
        int32_t m_frameOrderOfLastIntraInEncOrder;
        int32_t m_frameOrderOfLastAnchor;
        int32_t m_poc;
        int32_t m_encOrder;
        uint8_t  m_isShortTermRef;
        uint8_t  m_isIdrPic;
        uint8_t  m_isRef;
        RefPicList m_refPicList[2]; // 2 reflists containing reference frames used by current frame
        int32_t m_mapRefIdxL1ToL0[MAX_NUM_REF_IDX];
        uint32_t m_frameType;// full info for bs. m_frameType = m_codeType | isIDR ? | isRef ?
        int32_t m_sceneOrder;

        Frame *m_dpb[16];
        Frame *m_dpbVP9[NUM_REF_FRAMES]; // VP9 specific
        Frame *m_dpbVP9NextFrame[NUM_REF_FRAMES]; // VP9 specific
        Frame *m_prevFrame; // VP9 specific: used for PrevMvs & PrevRefFrames, shared ownership
        bool m_prevFrameIsOneOfRefs;
        int32_t m_dpbSize;

        uint8_t m_sliceQpY;
        std::vector<int8_t> m_lcuQps; // array for LCU QPs

        // BRC info
        double m_avCmplx;
        double m_CmplxQstep;
        int32_t m_qpBase;
        double m_totAvComplx;
        int32_t m_totComplxCnt;
        double m_complxSum;
        int32_t m_predBits;
        double m_cmplx;
        int32_t m_refQp;

        // VP9 specific
        uint32_t frameSizeInBytes;
        FrameContexts m_contexts;
        FrameContexts m_contextsInit;
        uint8_t m_refFramesContextsVp9[7][7][4]; // [ aboveRefIdxComb + 2 ] [ leftRefIdxComb + 2 ]
        RefFrameContextsAv1 m_refFramesContextsAv1[6][6]; // [ aboveRefIdxComb + 2 ] [ leftRefIdxComb + 2 ]
        uint16_t m_refFrameBitsAv1[6][6][4];
        int32_t intraOnly;
        int32_t refreshFrameFlags;
        int32_t referenceMode;
        int32_t refFrameIdx[AV1_ALTREF_FRAME + 1];
        Frame *refFramesVp9[ALTREF_FRAME + 1];
        Frame *refFramesAv1[AV1_ALTREF_FRAME + 1];
        int8_t refFrameSide[AV1_ALTREF_FRAME + 1];
        int32_t isUniq[AV1_ALTREF_FRAME+1];
        int32_t refFrameSignBiasVp9[ALTREF_FRAME + 1];
        int32_t refFrameSignBiasAv1[AV1_ALTREF_FRAME+1];
        int32_t m_refFrameId[NUM_REF_FRAMES];
        int32_t interpolationFilter;
        int32_t compoundReferenceAllowed;
        std::vector<TileContexts> m_tileContexts;
        int32_t segmentationEnabled;
        int32_t segmentationUpdateMap;
        int32_t showFrame;
        int32_t showExistingFrame; // 1 for frames which are encoded not in-order, 0 for normal frames
        int32_t frameToShowMapIdx; // frames which are encoded not in-order repeated later
        int32_t compFixedRef; // calculated based on refFrameSignBias
        int32_t compVarRef[2]; // calculated based on refFrameSignBias
        int32_t curFrameOffset;
        TplMvRef *m_tplMvs;

        CostType m_lambda;
        CostType m_lambdaSatd;
        uint64_t m_lambdaSatdInt; //  = int(m_lambdaSatd * (1<<24))
        BitCounts bitCount;

        int32_t *m_txkTypes4x4;
        uint32_t isAV1;

        pointer_like<FeiOutData> m_feiInterData[3][4];
        pointer_like<FeiOutData> m_feiInterp[4];

        Frame()
        {
            ResetMemInfo();
            ResetEncInfo();
        }

        ~Frame() { Destroy(); }

        uint8_t wasLAProcessed()    { return m_wasLookAheadProcessed; }
        void setWasLAProcessed() { m_wasLookAheadProcessed = true; }
        void unsetWasLAProcessed() { m_wasLookAheadProcessed = false; }
        int32_t IsIntra() const { return ((m_picCodeType & MFX_FRAMETYPE_I) || intraOnly); }

        void Create(const H265VideoParam *par);
        void Destroy();

        void ResetMemInfo();
        void ResetEncInfo();
    };

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

            std::auto_ptr<T> newFrame(new T());
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
