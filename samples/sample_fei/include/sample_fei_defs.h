/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#ifndef __SAMPLE_FEI_DEFS_H__
#define __SAMPLE_FEI_DEFS_H__

#include <mfxfei.h>
#include "mfxmvc.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"
#include <algorithm>
#include <memory>
#include <cstring>
#include <list>
#include <vector>

const mfxU16 MaxFeiEncMVPNum = 4;

#define MSDK_ZERO_ARRAY(VAR, NUM) {memset(VAR, 0, sizeof(VAR[0])*NUM);}
#define SAFE_RELEASE_EXT_BUFSET(SET) {if (SET){ SET->vacant = true; SET = NULL;}}
#define SAFE_DEC_LOCKER(SURFACE){if (SURFACE && SURFACE->Data.Locked) {SURFACE->Data.Locked--;}}
#define SAFE_UNLOCK(SURFACE){if (SURFACE) {SURFACE->Data.Locked = 0;}}
#define SAFE_FCLOSE(FPTR, ERR){ if (FPTR && fclose(FPTR)) { return ERR; }}
#define SAFE_FREAD(PTR, SZ, COUNT, FPTR, ERR) { if (FPTR && (fread(PTR, SZ, COUNT, FPTR) != COUNT)){ return ERR; }}
#define SAFE_FWRITE(PTR, SZ, COUNT, FPTR, ERR) { if (FPTR && (fwrite(PTR, SZ, COUNT, FPTR) != COUNT)){ return ERR; }}
#define SAFE_FSEEK(FPTR, OFFSET, ORIGIN, ERR) { if (FPTR && fseek(FPTR, OFFSET, ORIGIN)){ return ERR; }}

enum
{
    FEI_SLICETYPE_I = 2,
    FEI_SLICETYPE_P = 0,
    FEI_SLICETYPE_B = 1,
};

enum
{
    RPLM_ST_PICNUM_SUB = 0,
    RPLM_ST_PICNUM_ADD = 1,
    RPLM_LT_PICNUM     = 2,
    RPLM_END           = 3,
    RPLM_INTERVIEW_SUB = 4,
    RPLM_INTERVIEW_ADD = 5,
};

enum
{
    MMCO_END            = 0,
    MMCO_ST_TO_UNUSED   = 1,
    MMCO_LT_TO_UNUSED   = 2,
    MMCO_ST_TO_LT       = 3,
    MMCO_SET_MAX_LT_IDX = 4,
    MMCO_ALL_TO_UNUSED  = 5,
    MMCO_CURR_TO_LT     = 6,
};

// for ext buffers management
typedef struct{
    mfxU16      NumExtParam;
    mfxExtBuffer **ExtParam;
} setElem;

typedef struct{
    setElem in;
    setElem out;
} IObuffs;

typedef struct
{
    bool vacant;
    IObuffs I_bufs;
    IObuffs PB_bufs;
} bufSet;

// B frame location struct for reordering

template<class T> inline void Zero(T & obj) { memset(&obj, 0, sizeof(obj)); }
struct BiFrameLocation
{
    BiFrameLocation() { Zero(*this); }

    mfxU32 miniGopCount;    // sequence of B frames between I/P frames
    mfxU32 encodingOrder;   // number within mini-GOP (in encoding order)
    mfxU16 refFrameFlag;    // MFX_FRAMETYPE_REF if B frame is reference
};

template<class T, mfxU32 N>
struct FixedArray
{
    FixedArray()
        : m_numElem(0)
    {
    }

    explicit FixedArray(T fillVal)
        : m_numElem(0)
    {
        Fill(fillVal);
    }

    void PushBack(T const & val)
    {
        //assert(m_numElem < N);
        m_arr[m_numElem] = val;
        m_numElem++;
    }

    void PushFront(T const val)
    {
        //assert(m_numElem < N);
        std::copy(m_arr, m_arr + m_numElem, m_arr + 1);
        m_arr[0] = val;
        m_numElem++;
    }

    void Erase(T * p)
    {
        //assert(p >= m_arr && p <= m_arr + m_numElem);

        m_numElem = mfxU32(
            std::copy(p + 1, m_arr + m_numElem, p) - m_arr);
    }

    void Erase(T * b, T * e)
    {
        //assert(b <= e);
        //assert(b >= m_arr && b <= m_arr + m_numElem);
        //assert(e >= m_arr && e <= m_arr + m_numElem);

        m_numElem = mfxU32(
            std::copy(e, m_arr + m_numElem, b) - m_arr);
    }

    void Resize(mfxU32 size, T fillVal = T())
    {
        //assert(size <= N);
        for (mfxU32 i = m_numElem; i < size; ++i)
            m_arr[i] = fillVal;
        m_numElem = size;
    }

    T * Begin()
    {
        return m_arr;
    }

    T const * Begin() const
    {
        return m_arr;
    }

    T * End()
    {
        return m_arr + m_numElem;
    }

    T const * End() const
    {
        return m_arr + m_numElem;
    }

    T & Back()
    {
        //assert(m_numElem > 0);
        return m_arr[m_numElem - 1];
    }

    T const & Back() const
    {
        //assert(m_numElem > 0);
        return m_arr[m_numElem - 1];
    }

    mfxU32 Size() const
    {
        return m_numElem;
    }

    mfxU32 Capacity() const
    {
        return N;
    }

    T & operator[](mfxU32 idx)
    {
        //assert(idx < N);
        return m_arr[idx];
    }

    T const & operator[](mfxU32 idx) const
    {
        //assert(idx < N);
        return m_arr[idx];
    }

    void Fill(T val)
    {
        for (mfxU32 i = 0; i < N; i++)
        {
            m_arr[i] = val;
        }
    }

    template<mfxU32 M>
    bool operator==(const FixedArray<T, M>& r) const
    {
        //assert(Size() <= N);
        //assert(r.Size() <= M);

        if (Size() != r.Size())
        {
            return false;
        }

        for (mfxU32 i = 0; i < Size(); i++)
        {
            if (m_arr[i] != r[i])
            {
                return false;
            }
        }

        return true;
    }

private:
    T      m_arr[N];
    mfxU32 m_numElem;
};

template <typename T> struct Pair
{
    T top;
    T bot;

    Pair()
    {
    }

    template<typename U> Pair(Pair<U> const & pair)
        : top(static_cast<T>(pair.top))
        , bot(static_cast<T>(pair.bot))
    {
    }

    template<typename U> explicit Pair(U const & value)
        : top(static_cast<T>(value))
        , bot(static_cast<T>(value))
    {
    }

    template<typename U> Pair(U const & t, U const & b)
        : top(static_cast<T>(t))
        , bot(static_cast<T>(b))
    {
    }

    template<typename U>
    Pair<T> & operator =(Pair<U> const & pair)
    {
        Pair<T> tmp(pair);
        std::swap(*this, tmp);
        return *this;
    }

    T & operator[] (mfxU32 parity)
    {
        //assert(parity < 2);
        return (&top)[parity & 1];
    }

    T const & operator[] (mfxU32 parity) const
    {
        //assert(parity < 2);
        return (&top)[parity & 1];
    }
};

struct RefListMod
{
    RefListMod() : m_idc(3), m_diff(0) {}
    RefListMod(mfxU16 idc, mfxU16 diff) : m_idc(idc), m_diff(diff) { /*assert(idc < 6);*/ }
    mfxU16 m_idc;
    mfxU16 m_diff;
};

typedef FixedArray<RefListMod, 32> ArrayRefListMod;
typedef FixedArray<mfxU8, 8>       ArrayU8x8;
typedef FixedArray<mfxU8, 16>      ArrayU8x16;
typedef FixedArray<mfxU8, 32>      ArrayU8x32;
typedef FixedArray<mfxU8, 33>      ArrayU8x33;
typedef FixedArray<mfxU32, 64>     ArrayU32x64;

typedef Pair<mfxU8> PairU8;
typedef Pair<mfxI32> PairI32;

struct DpbFrame
{
    DpbFrame() { Zero(*this); }

    PairI32 m_poc;
    mfxU32  m_frameOrder;
    mfxU32  m_frameNum;
    mfxI32  m_frameNumWrap;
    PairI32 m_picNum;
    mfxU32  m_frameIdx;
    PairU8  m_longTermPicNum;
    PairU8  m_refPicFlag;
    mfxU8   m_longterm; // at least one field is a long term reference
    mfxU8   m_refBase;
};

struct ArrayDpbFrame : public FixedArray<DpbFrame, 16>
{
    ArrayDpbFrame()
        : FixedArray<DpbFrame, 16>()
    {
        m_maxLongTermFrameIdxPlus1.Resize(8, 0);
    }

    ArrayU8x8 m_maxLongTermFrameIdxPlus1; // for each temporal layer
};

inline mfxI32 GetPicNum(ArrayDpbFrame const & dpb, mfxU8 ref)
{
    return dpb[ref & 127].m_picNum[ref >> 7];
}

inline mfxI32 GetPicNumF(ArrayDpbFrame const & dpb, mfxU8 ref)
{
    DpbFrame const & dpbFrame = dpb[ref & 127];
    return dpbFrame.m_refPicFlag[ref >> 7] ? dpbFrame.m_picNum[ref >> 7] : 0x20000;
}

inline mfxU8 GetLongTermPicNum(ArrayDpbFrame const & dpb, mfxU8 ref)
{
    return dpb[ref & 127].m_longTermPicNum[ref >> 7];
}

inline mfxU32 GetLongTermPicNumF(ArrayDpbFrame const & dpb, mfxU8 ref)
{
    DpbFrame const & dpbFrame = dpb[ref & 127];

    return dpbFrame.m_refPicFlag[ref >> 7] && dpbFrame.m_longterm
        ? dpbFrame.m_longTermPicNum[ref >> 7]
        : 0x20;
}

inline mfxI32 GetPoc(ArrayDpbFrame const & dpb, mfxU8 ref)
{
    return dpb[ref & 127].m_poc[ref >> 7];
}

struct DecRefPicMarkingInfo
{
    DecRefPicMarkingInfo() { Zero(*this); }

    void PushBack(mfxU8 op, mfxU32 param0, mfxU32 param1 = 0)
    {
        mmco.PushBack(op);
        value.PushBack(param0);
        value.PushBack(param1);
    }

    mfxU8       no_output_of_prior_pics_flag;
    mfxU8       long_term_reference_flag;
    ArrayU8x32  mmco;       // memory management control operation id
    ArrayU32x64 value;      // operation-dependent data, max 2 per operation
};

struct PreEncOutput
{
    PreEncOutput()
        : output_bufs(NULL)
    {
        refIdx[0][0] = refIdx[0][1] = refIdx[1][0] = refIdx[1][1] = 0;
    }

    PreEncOutput(bufSet* bufs, mfxU8 idx[2][2])
        : output_bufs(bufs)
    {
        refIdx[0][0] = idx[0][0];
        refIdx[0][1] = idx[0][1];
        refIdx[1][0] = idx[1][0];
        refIdx[1][1] = idx[1][1];
    }

    bufSet* output_bufs;
    mfxU8 refIdx[2][2]; // [fieldId][L0L1]
};

enum
{
    TFIELD = 0,
    BFIELD = 1
};

struct iTaskParams
{
    mfxU16 PicStruct;
    mfxU16 BRefType;
    PairU8 FrameType;
    mfxU16 GopPicSize;
    mfxU16 GopRefDist;
    mfxU32 FrameCount;
    mfxU32 FrameOrderIdrInDisplayOrder;
    mfxU16 NumRefActiveP;
    mfxU16 NumRefActiveBL0;
    mfxU16 NumRefActiveBL1;

    mfxFrameSurface1 *InputSurf;
    mfxFrameSurface1 *ReconSurf;
    mfxBitstream     *BitStream;

    explicit iTaskParams()
        : PicStruct(MFX_PICSTRUCT_PROGRESSIVE)
        , BRefType(MFX_B_REF_OFF)
        , FrameType(PairU8(MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF))
        , GopPicSize(1)
        , GopRefDist(1)
        , FrameCount(0)
        , FrameOrderIdrInDisplayOrder(0)
        , NumRefActiveP(0)
        , NumRefActiveBL0(0)
        , NumRefActiveBL1(0)
        , InputSurf(NULL)
        , ReconSurf(NULL)
        , BitStream(NULL)
    {}
};

//for PreEnc; Enc; Pak reordering
struct iTask
{
    explicit iTask(const iTaskParams & task_params)
        : EncSyncP(NULL)
        , encoded(false)
        , bufs(NULL)
        , preenc_bufs(NULL)
        , fullResSurface(NULL)
        , PicStruct(task_params.PicStruct)
        , BRefType(task_params.BRefType)
        , NumRefActiveP(task_params.NumRefActiveP)
        , NumRefActiveBL0(task_params.NumRefActiveBL0)
        , NumRefActiveBL1(task_params.NumRefActiveBL1)
        , m_type(task_params.FrameType)
        , m_fid(PairU8(static_cast<mfxU8>(!!(task_params.PicStruct & MFX_PICSTRUCT_FIELD_BFF)),
                       static_cast<mfxU8>(!(task_params.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) - !!(task_params.PicStruct & MFX_PICSTRUCT_FIELD_BFF))))
        , m_fieldPicFlag(!(task_params.PicStruct & MFX_PICSTRUCT_PROGRESSIVE))
        , m_frameOrderIdr(task_params.FrameOrderIdrInDisplayOrder)
        , m_frameOrderI(0)
        , m_frameOrder(task_params.FrameCount)
        , GopPicSize(task_params.GopPicSize)
        , GopRefDist(task_params.GopRefDist)
        , m_viewIdx(0)
        //, m_nalRefIdc(PairU8(0,0))
        , m_picNum(PairI32(0, 0))
        , m_frameNum(0)
        , m_frameNumWrap(0)
        , m_tid(0)
        , m_tidx(0)
        , m_longTermPicNum(PairU8(0, 0))
        , prevTask(NULL)
    {
        m_list0[0].Fill(0);
        m_list0[1].Fill(0);
        m_list1[0].Fill(0);
        m_list1[1].Fill(0);

        m_initSizeList0[0] = 0;
        m_initSizeList0[1] = 0;
        m_initSizeList1[0] = 0;
        m_initSizeList1[1] = 0;

        memset(&in,     0, sizeof(in));
        memset(&out,    0, sizeof(out));
        memset(&inPAK,  0, sizeof(inPAK));
        memset(&outPAK, 0, sizeof(outPAK));

        /* Below initialized structures required for frames reordering */
        if (m_type[m_fid[0]] & MFX_FRAMETYPE_B)
        {
            m_loc = GetBiFrameLocation(m_frameOrder - m_frameOrderIdr);
            m_type[0] |= m_loc.refFrameFlag;
            m_type[1] |= m_loc.refFrameFlag;
        }

        m_nalRefIdc[m_fid[0]] = m_reference[m_fid[0]] = !!(m_type[m_fid[0]] & MFX_FRAMETYPE_REF);
        m_nalRefIdc[m_fid[1]] = m_reference[m_fid[1]] = !!(m_type[m_fid[1]] & MFX_FRAMETYPE_REF);

        m_poc[0] = 2 * ((m_frameOrder - m_frameOrderIdr) & 0x7fffffff) + (TFIELD != m_type[m_fid[0]]);
        m_poc[1] = 2 * ((m_frameOrder - m_frameOrderIdr) & 0x7fffffff) + (BFIELD != m_type[m_fid[0]]);


        /* Fill information required for encoding */
        if (task_params.InputSurf)
        {
            in.InSurface = task_params.InputSurf;
            in.InSurface->Data.Locked++;
        }

        /* ENC and/or PAK */
        if (task_params.ReconSurf)
        {
            inPAK.InSurface = in.InSurface;
            inPAK.InSurface->Data.Locked++;

            /* ENC & PAK share same reconstructed/reference surface */
           out.OutSurface = task_params.ReconSurf;
           out.OutSurface->Data.Locked++;
           outPAK.OutSurface = task_params.ReconSurf;
           outPAK.OutSurface->Data.Locked++;

           outPAK.Bs = task_params.BitStream;
        }
    }

    ~iTask()
    {
        SAFE_RELEASE_EXT_BUFSET(bufs);
        SAFE_RELEASE_EXT_BUFSET(preenc_bufs);

        ReleasePreEncOutput();

        SAFE_DEC_LOCKER(in.InSurface);
        /* In case of preenc + enc (+ pak) pipeline we need to do decrement manually for both interfaces*/
        SAFE_DEC_LOCKER(inPAK.InSurface);
        SAFE_DEC_LOCKER(fullResSurface);
        SAFE_DEC_LOCKER(out.OutSurface);
        SAFE_DEC_LOCKER(outPAK.OutSurface);
    }

    /* This operator is used to store only necessary information from previous encoding */
    iTask& operator= (const iTask& task)
    {
        if (this == &task)
            return *this;

        m_frameOrderIdr   = task.m_frameOrderIdr;
        m_frameOrderI     = task.m_frameOrderI;
        m_frameIdrCounter = task.m_frameIdrCounter;
        m_nalRefIdc       = task.m_nalRefIdc;
        m_frameNum        = task.m_frameNum;
        m_type            = task.m_type;
        m_dpbPostEncoding = task.m_dpbPostEncoding;
        m_poc             = task.m_poc;
        PicStruct         = task.PicStruct;

        return *this;
    }

    /* Release all output buffers from PreENC multicalls.
       This performed after repacking / output dumping is finished */
    void ReleasePreEncOutput()
    {
        for (std::list<PreEncOutput>::iterator it = preenc_output.begin(); it != preenc_output.end(); ++it)
        {
            SAFE_RELEASE_EXT_BUFSET((*it).output_bufs);
        }

        preenc_output.clear();
    }

    /* These two functions are used to get location of B frame in current mini-GOP.
       Output depends from B-pyramid settings */
    BiFrameLocation GetBiFrameLocation(mfxU32 frameOrder)
    {

        mfxU32 gopPicSize = GopPicSize;
        mfxU32 gopRefDist = GopRefDist;
        mfxU32 biPyramid  = BRefType;

        BiFrameLocation loc;

        if (gopPicSize == 0xffff) //infinite GOP
            gopPicSize = 0xffffffff;

        if (biPyramid != MFX_B_REF_OFF)
        {
            bool ref = false;
            mfxU32 orderInMiniGop = frameOrder % gopPicSize % gopRefDist - 1;

            loc.encodingOrder = GetEncodingOrder(orderInMiniGop, 0, gopRefDist - 1, 0, ref);
            loc.miniGopCount = frameOrder % gopPicSize / gopRefDist;
            loc.refFrameFlag = static_cast<mfxU16>(ref ? MFX_FRAMETYPE_REF : 0);
        }

        return loc;
    }

    mfxU32 GetEncodingOrder(mfxU32 displayOrder, mfxU32 begin, mfxU32 end, mfxU32 counter, bool & ref)
    {
        //assert(displayOrder >= begin);
        //assert(displayOrder <  end);

        ref = (end - begin > 1);

        mfxU32 pivot = (begin + end) / 2;
        if (displayOrder == pivot)
            return counter;
        else if (displayOrder < pivot)
            return GetEncodingOrder(displayOrder, begin, pivot, counter + 1, ref);
        else
            return GetEncodingOrder(displayOrder, pivot + 1, end, counter + 1 + pivot - begin, ref);
    }

    /* These functions counts forward / backward references of current task */
    mfxU32 GetNBackward(mfxU32 fieldId)
    {
        mfxU32 fid = m_fid[fieldId];

        if (m_list0[fid].Size() == 0)
            return 0;

        if (m_list1[fid].Size() == 0 ||
            std::find(m_list0[fid].Begin(), m_list0[fid].End(), *m_list1[fid].Begin())
            == m_list0[fid].End())
        {
            // No forward ref in L0
            return m_list0[fid].Size();
        }
        else
        {
            return static_cast<mfxU32>(std::distance(m_list0[fid].Begin(),
                std::find(m_list0[fid].Begin(), m_list0[fid].End(), *m_list1[fid].Begin())));
        }
    }

    mfxU32 GetNForward(mfxU32 fieldId)
    {
        mfxU32 fid = m_fid[fieldId];

        if (m_list1[fid].Size() == 0)
            return 0;

        if (std::find(m_list1[fid].Begin(), m_list1[fid].End(), *m_list0[fid].Begin())
            == m_list1[fid].End())
        {
            // No backward ref in L1
            return m_list1[fid].Size();
        }
        else
        {
            return static_cast<mfxU32>(std::distance(m_list1[fid].Begin(),
                std::find(m_list1[fid].Begin(), m_list1[fid].End(), *m_list0[fid].Begin())));
        }
    }

    mfxENCInput  in;
    mfxENCOutput out;
    mfxPAKInput  inPAK;
    mfxPAKOutput outPAK;
    BiFrameLocation m_loc;
    mfxSyncPoint EncSyncP;
    bool encoded;
    bufSet* bufs;
    bufSet* preenc_bufs;
    std::list<PreEncOutput> preenc_output;
    mfxFrameSurface1 *fullResSurface;

    mfxU16 PicStruct;
    mfxU16 BRefType;

    mfxU16 NumRefActiveP;   // limits of active
    mfxU16 NumRefActiveBL0; // references for
    mfxU16 NumRefActiveBL1; // reflists management

    //..............................reflist control............................................
    ArrayDpbFrame   m_dpb[2];          // DPB state before encoding first and second fields
    ArrayDpbFrame   m_dpbPostEncoding; // DPB after encoding a frame (or 2 fields)
    ArrayU8x33      m_list0[2];        // L0 list for first and second field
    ArrayU8x33      m_list1[2];        // L1 list for first and second field
    PairU8          m_type;            // type of first and second field
    PairU8          m_fid;             // progressive fid=[0,0]; tff fid=[0,1]; bff fid=[1,0]
    bool            m_fieldPicFlag;    // is interlaced frame
    PairI32         m_poc;             // POC of first and second field

    mfxU32  m_frameOrderIdr;           // most recent idr frame in display order
    mfxU32  m_frameOrderI;             // most recent i frame in display order
    mfxU32  m_frameOrder;              // current frame order in display order
    mfxU16  m_frameIdrCounter;         // number of IDR frames encoded

    mfxU16  GopPicSize;                // GOP size
    mfxU16  GopRefDist;                // number of B frames in mini-GOP + 1

    ArrayRefListMod m_refPicList0Mod[2];
    ArrayRefListMod m_refPicList1Mod[2];
    mfxU32  m_initSizeList0[2];
    mfxU32  m_initSizeList1[2];

    DecRefPicMarkingInfo m_decRefPicMrk[2];    // dec_ref_pic_marking() for current frame

    mfxU32  m_viewIdx;

    PairU8  m_nalRefIdc;

    // from Reconstruct
    PairI32 m_picNum;

    mfxU16  m_frameNum;
    mfxI32  m_frameNumWrap;

    mfxU32  m_tid;              // temporal_id
    mfxU32  m_tidx;             // temporal layer index (in acsending order of temporal_id)

    PairU8  m_longTermPicNum;
    PairU8  m_reference;        // is refrence (short or long term) or not
    //.........................................................................................

    iTask* prevTask;
};

/* This structure represents state of DPB and reference lists of the task being processed */
struct RefInfo
{
    std::vector<mfxFrameSurface1*> reference_frames;
    struct{
        std::vector<mfxU16> dpb_idx;
        std::vector<mfxU16> l0_idx;
        std::vector<mfxU16> l1_idx;
        std::vector<mfxU16> l0_parity;
        std::vector<mfxU16> l1_parity;
    } state[2];

    void Clear()
    {
        reference_frames.clear();

        for (mfxU32 fieldId = 0; fieldId < 2; ++fieldId)
        {
            state[fieldId].dpb_idx.clear();
            state[fieldId].l0_idx.clear();
            state[fieldId].l1_idx.clear();
            state[fieldId].l0_parity.clear();
            state[fieldId].l1_parity.clear();
        }
    }
};

/* Group of functions below implements some usefull operations for current frame / field of the task:
   Frame type extraction, field parity, POC, implements Predicates for DPB management and reference lists construction */

inline mfxU8 GetFirstField(const iTask& task)
{
    return (task.PicStruct & MFX_PICSTRUCT_FIELD_BFF) ? 1 : 0;
}

inline mfxI32 GetPoc(const iTask& task, mfxU32 parity)
{
    return 2 * ((task.m_frameOrder - task.m_frameOrderIdr) & 0x7fffffff) + (parity != GetFirstField(task));
}

inline mfxU8 ExtractFrameType(const iTask& task)
{
    return task.m_type[GetFirstField(task)];
}

inline mfxU8 ExtractFrameType(const iTask& task, mfxU32 fieldId)
{
    if (!fieldId){
        return task.m_type[GetFirstField(task)];
    }
    else{
        return task.m_type[!GetFirstField(task)];
    }
}

inline mfxU16 createType(const iTask& task)
{
    return ((mfxU16)task.m_type[!GetFirstField(task)] << 8) | task.m_type[GetFirstField(task)];
}

inline mfxU8 extractType(mfxU16 type, mfxU32 fieldId)
{
    return fieldId ? (type >> 8) : (type & 255);
}

struct BasePredicateForRefPic
{
    typedef ArrayDpbFrame Dpb;
    typedef mfxU8         Arg;
    typedef bool          Res;

    BasePredicateForRefPic(Dpb const & dpb) : m_dpb(dpb) {}

    void operator =(BasePredicateForRefPic const &);

    Dpb const & m_dpb;
};

struct RefPicNumIsGreater : public BasePredicateForRefPic
{
    RefPicNumIsGreater(Dpb const & dpb) : BasePredicateForRefPic(dpb) {}

    bool operator ()(mfxU8 l, mfxU8 r) const
    {
        return GetPicNum(m_dpb, l) > GetPicNum(m_dpb, r);
    }
};

struct LongTermRefPicNumIsLess : public BasePredicateForRefPic
{
    LongTermRefPicNumIsLess(Dpb const & dpb) : BasePredicateForRefPic(dpb) {}

    bool operator ()(mfxU8 l, mfxU8 r) const
    {
        return GetLongTermPicNum(m_dpb, l) < GetLongTermPicNum(m_dpb, r);
    }
};

struct RefPocIsLess : public BasePredicateForRefPic
{
    RefPocIsLess(Dpb const & dpb) : BasePredicateForRefPic(dpb) {}

    bool operator ()(mfxU8 l, mfxU8 r) const
    {
        return GetPoc(m_dpb, l) < GetPoc(m_dpb, r);
    }
};

struct RefPocIsGreater : public BasePredicateForRefPic
{
    RefPocIsGreater(Dpb const & dpb) : BasePredicateForRefPic(dpb) {}

    bool operator ()(mfxU8 l, mfxU8 r) const
    {
        return GetPoc(m_dpb, l) > GetPoc(m_dpb, r);
    }
};

struct RefPocIsLessThan : public BasePredicateForRefPic
{
    RefPocIsLessThan(Dpb const & dpb, mfxI32 poc) : BasePredicateForRefPic(dpb), m_poc(poc) {}

    bool operator ()(mfxU8 r) const
    {
        return GetPoc(m_dpb, r) < m_poc;
    }

    mfxI32 m_poc;
};

struct RefPocIsGreaterThan : public BasePredicateForRefPic
{
    RefPocIsGreaterThan(Dpb const & dpb, mfxI32 poc) : BasePredicateForRefPic(dpb), m_poc(poc) {}

    bool operator ()(mfxU8 r) const
    {
        return GetPoc(m_dpb, r) > m_poc;
    }

    mfxI32 m_poc;
};

struct RefIsShortTerm : public BasePredicateForRefPic
{
    RefIsShortTerm(Dpb const & dpb) : BasePredicateForRefPic(dpb) {}

    bool operator ()(mfxU8 r) const
    {
        return m_dpb[r & 127].m_refPicFlag[r >> 7] && !m_dpb[r & 127].m_longterm;
    }
};

struct RefIsLongTerm : public BasePredicateForRefPic
{
    RefIsLongTerm(Dpb const & dpb) : BasePredicateForRefPic(dpb) {}

    bool operator ()(mfxU8 r) const
    {
        return m_dpb[r & 127].m_refPicFlag[r >> 7] && m_dpb[r & 127].m_longterm;
    }
};

inline bool RefListHasLongTerm(
    ArrayDpbFrame const & dpb,
    ArrayU8x33 const &    list)
{
    return std::find_if(list.Begin(), list.End(), RefIsLongTerm(dpb)) != list.End();
}

template <class T, class U> struct LogicalAndHelper
{
    typedef typename T::Arg Arg;
    typedef typename T::Res Res;
    T m_pr1;
    U m_pr2;

    LogicalAndHelper(T pr1, U pr2) : m_pr1(pr1), m_pr2(pr2) {}

    Res operator ()(Arg arg) const { return m_pr1(arg) && m_pr2(arg); }
};

template <class T, class U> LogicalAndHelper<T, U> LogicalAnd(T pr1, U pr2)
{
    return LogicalAndHelper<T, U>(pr1, pr2);
}

template <class T> struct LogicalNotHelper
{
    typedef typename T::argument_type Arg;
    typedef typename T::result_type   Res;
    T m_pr;

    LogicalNotHelper(T pr) : m_pr(pr) {}

    Res operator ()(Arg arg) const { return !m_pred(arg); }
};

template <class T> LogicalNotHelper<T> LogicalNot(T pr)
{
    return LogicalNotHelper<T>(pr);
}

inline mfxI8 GetIdxOfFirstSameParity(ArrayU8x33 const & refList, mfxU32 fieldId)
{
    for (mfxU8 i = 0; i < refList.Size(); i++)
    {
        mfxU8 refFieldId = (refList[i] & 128) >> 7;
        if (fieldId == refFieldId)
        {
            return (mfxI8)i;
        }
    }

    return -1;
}

inline void UpdateMaxLongTermFrameIdxPlus1(ArrayU8x8 & arr, mfxU32 curTidx, mfxU32 val)
{
    std::fill(arr.Begin() + curTidx, arr.End(), val);
}

inline void InitNewDpbFrame(
    DpbFrame &      ref,
    iTask &         task,
    mfxU32          fid)
{
    ref.m_poc[0] = GetPoc(task, TFIELD);
    ref.m_poc[1] = GetPoc(task, BFIELD);
    ref.m_frameOrder = task.m_frameOrder;
    ref.m_frameNum = task.m_frameNum;
    ref.m_frameNumWrap = task.m_frameNumWrap;
    ref.m_longTermPicNum[0] = task.m_longTermPicNum[0];
    ref.m_longTermPicNum[1] = task.m_longTermPicNum[1];
    ref.m_longterm = 0;
    ref.m_refBase = 0;

    ref.m_refPicFlag[fid] = !!(task.m_type[fid] & MFX_FRAMETYPE_REF);
    ref.m_refPicFlag[!fid] = !!(task.m_type[!fid] & MFX_FRAMETYPE_REF);
    if (task.m_fieldPicFlag)
        ref.m_refPicFlag[!fid] = 0;
}

inline bool OrderByFrameNumWrap(DpbFrame const & lhs, DpbFrame const & rhs)
{
    if (!lhs.m_longterm && !rhs.m_longterm)
        if (lhs.m_frameNumWrap < rhs.m_frameNumWrap)
            return lhs.m_refBase > rhs.m_refBase;
        else
            return lhs.m_frameNumWrap < rhs.m_frameNumWrap;
    else if (!lhs.m_longterm && rhs.m_longterm)
        return true;
    else if (lhs.m_longterm && !rhs.m_longterm)
        return false;
    else // both long term
        return lhs.m_longTermPicNum[0] < rhs.m_longTermPicNum[0];
}


inline bool OrderByDisplayOrder(DpbFrame const & lhs, DpbFrame const & rhs)
{
    return lhs.m_frameOrder < rhs.m_frameOrder;
}

struct OrderByNearestPrev
{
    mfxU32 m_fo;

    OrderByNearestPrev(mfxU32 displayOrder) : m_fo(displayOrder) {}
    bool operator() (DpbFrame const & l, DpbFrame const & r)
    {
        return (l.m_frameOrder < m_fo) && ((r.m_frameOrder > m_fo) || ((m_fo - l.m_frameOrder) < (m_fo - r.m_frameOrder)));
    }
};

/* This function prints out state of DPB and reference lists for given frame.
   It is disabled by default in release build */
void ShowDpbInfo(iTask *task, int frame_order);

/* This class implements pool of iTasks and manage reordering and reference lists construction for income frame */
class iTaskPool
{
public:
    std::list<iTask*> task_pool;
    iTask* last_encoded_task;
    iTask* task_in_process;
    mfxU16 GopRefDist;
    mfxU16 GopOptFlag;
    mfxU16 NumRefFrame;
    mfxU16 log2frameNumMax;
    mfxU16 refresh_limit;

    explicit iTaskPool(mfxU16 GopRefDist = 1, mfxU16 GopOptFlag = 0, mfxU16 NumRefFrame = 0, mfxU16 limit = 1, mfxU16 log2frameNumMax = 8)
        : last_encoded_task(NULL)
        , task_in_process(NULL)
        , GopRefDist(GopRefDist)
        , GopOptFlag(GopOptFlag)
        , NumRefFrame(NumRefFrame)
        , log2frameNumMax(log2frameNumMax)
        , refresh_limit(limit)
    {}

    ~iTaskPool() { Clear(); }

    /* Update those fields that could be adjusted by MSDK internal checks */
    void Init(mfxU16 RefDist, mfxU16 OptFlag, mfxU16 NumRef, mfxU16 limit, mfxU16 lg2frameNumMax)
    {
        GopRefDist      = RefDist;
        GopOptFlag      = OptFlag;
        NumRefFrame     = NumRef;
        log2frameNumMax = lg2frameNumMax;
        refresh_limit   = limit;
    }

    /* Release all pipeline resources */
    void Clear()
    {
        task_in_process = NULL;

        if (last_encoded_task)
        {
            delete last_encoded_task;
            last_encoded_task = NULL;
        }

        for (std::list<iTask*>::iterator it = task_pool.begin(); it != task_pool.end(); ++it)
        {
            delete *it;
        }

        task_pool.clear();
    }

    /* Add new income task */
    void AddTask(iTask* task){ task_pool.push_back(task); }

    /* Finish processing of current task and erase the oldest task in pool if refresh_limit is achieved */
    void UpdatePool()
    {
        if (!task_in_process) return;

        task_in_process->encoded = true;

        if (!last_encoded_task)
            last_encoded_task = new iTask(iTaskParams());

        *last_encoded_task = *task_in_process;

        task_in_process = NULL;

        if (task_pool.size() >= refresh_limit)
        {
            RemoveProcessedTask();
        }
    }

    /* Find and erase the oldest processed task in pool that is not in DPB of last_encoded_task */
    void RemoveProcessedTask()
    {
        if (last_encoded_task)
        {
            ArrayDpbFrame & dpb = last_encoded_task->m_dpbPostEncoding;
            std::list<mfxU32> FramesInDPB;

            for (mfxU32 i = 0; i < dpb.Size(); i++)
                FramesInDPB.push_back(dpb[i].m_frameOrder);

            for (std::list<iTask*>::iterator it = task_pool.begin(); it != task_pool.end(); ++it)
            {
                if (std::find(FramesInDPB.begin(), FramesInDPB.end(), (*it)->m_frameOrder) == FramesInDPB.end() && (*it)->encoded) // delete task
                {
                    delete (*it);
                    task_pool.erase(it);
                    return;
                }
            }
        }
    }

    /* Find task in pool by its FrameOrder */
    iTask* GetTaskByFrameOrder(mfxU32 frame_order)
    {
        for (std::list<iTask*>::iterator it = task_pool.begin(); it != task_pool.end(); ++it)
        {
            if ((*it)->m_frameOrder == frame_order)
                return *it;
        }

        return NULL;
    }

    /* Find task in pool by its outPAK.OutSurface */
    iTask* GetTaskByPAKOutputSurface(mfxFrameSurface1 *surface)
    {
        for (std::list<iTask*>::iterator it = task_pool.begin(); it != task_pool.end(); ++it)
        {
            if ((*it)->outPAK.OutSurface == surface)
                return *it;
        }

        return NULL;
    }

    /* Count unprocessed tasks */
    mfxU32 CountUnencodedFrames()
    {
        mfxU32 numUnencoded = 0;
        for (std::list<iTask*>::iterator it = task_pool.begin(); it != task_pool.end(); ++it){
            if (!(*it)->encoded){
                ++numUnencoded;
            }
        }

        return numUnencoded;
    }

    /* Adjust type of B frames without L1 references if not only GOP_OPT_STRICT is set */
    void ProcessLastB()
    {
        if (!(GopOptFlag & MFX_GOP_STRICT))
        {
            if (task_pool.back()->m_type[0] & MFX_FRAMETYPE_B) {
                task_pool.back()->m_type[0] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            }
            if (task_pool.back()->m_type[1] & MFX_FRAMETYPE_B) {
                task_pool.back()->m_type[1] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            }
        }
    }

    /* Block of functions below implements task reordering and reference lists management */

    /* Returns reordered task that could be processed or null if more frames are required for references
       (via GetReorderedTask).
       If task is found this function also fills DPB of this task */
    iTask* GetTaskToEncode(bool buffered_frames_processing);

    iTask* GetReorderedTask(bool buffered_frames_processing)
    {
        std::list<iTask*> unencoded_queue;
        for (std::list<iTask*>::iterator it = task_pool.begin(); it != task_pool.end(); ++it){
            if ((*it)->encoded == 0){
                unencoded_queue.push_back(*it);
            }
        }

        std::list<iTask*>::iterator top = ReorderFrame(unencoded_queue), begin = unencoded_queue.begin(), end = unencoded_queue.end();

        bool flush = unencoded_queue.size() < GopRefDist && buffered_frames_processing;

        if (flush && top == end && begin != end)
        {
            if (!!(GopOptFlag & MFX_GOP_STRICT))
            {
                top = begin; // TODO: reorder remaining B frames for B-pyramid when enabled
            }
            else
            {
                top = end;
                --top;
                //assert((*top)->frameType & MFX_FRAMETYPE_B);
                (*top)->m_type[0] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                (*top)->m_type[1] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                top = ReorderFrame(unencoded_queue);
                //assert(top != end || begin == end);
            }
        }

        if (top == end)
            return NULL; //skip B without encoded refs

        return *top;
    }

    /* Finds earliest unencoded frame in display order which references are encoded */
    std::list<iTask*>::iterator ReorderFrame(std::list<iTask*>& unencoded_queue)
    {
        std::list<iTask*>::iterator top = unencoded_queue.begin(), end = unencoded_queue.end();
        while (top != end &&                                     // go through buffered frames in display order and
            (((ExtractFrameType(**top) & MFX_FRAMETYPE_B) &&     // get earliest non-B frame
            CountFutureRefs((*top)->m_frameOrder) == 0) ||       // or B frame with L1 reference
            (*top)->encoded))                                    // which is not encoded yet
        {
            ++top;
        }

        if (top != end && (ExtractFrameType(**top) & MFX_FRAMETYPE_B))
        {
            // special case for B frames (when B pyramid is enabled)
            std::list<iTask*>::iterator i = top;
            while (++i != end &&                                          // check remaining
                (ExtractFrameType(**i) & MFX_FRAMETYPE_B) &&              // B frames
                ((*i)->m_loc.miniGopCount == (*top)->m_loc.miniGopCount)) // from the same mini-gop
            {
                if (!(*i)->encoded && (*top)->m_loc.encodingOrder > (*i)->m_loc.encodingOrder)
                    top = i;
            }
        }

        return top;
    }

    /* Counts encoded future references of frame with given display order */
    mfxU32 CountFutureRefs(mfxU32 frameOrder){
        mfxU32 count = 0;
        for (std::list<iTask*>::iterator it = task_pool.begin(); it != task_pool.end(); ++it)
            if (frameOrder < (*it)->m_frameOrder && (*it)->encoded)
                ++count;
        return count;
    }
};

#endif // #define __SAMPLE_FEI_DEFS_H__
