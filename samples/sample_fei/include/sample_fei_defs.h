//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2005-2016 Intel Corporation. All Rights Reserved.
//
//
//*/

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

#define IPP_MAX( a, b ) ( ((a) > (b)) ? (a) : (b) )
#define IPP_MIN( a, b ) ( ((a) < (b)) ? (a) : (b) )

#define NumActiveRefP      4
#define NumActiveRefBL0    4
#define NumActiveRefBL1    1
#define NumActiveRefBL1_i  2
#define MaxFeiEncMVPNum    4

#define MSDK_ZERO_ARRAY(VAR, NUM) {memset(VAR, 0, sizeof(*VAR)*NUM);}
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

struct PreEncMVPInfo
{
    bufSet* preenc_output_bufs;
    mfxU32 refIdx[2][2]; // [fieldId][L0L1]
};

//for PreEnc; Enc; Pak reordering
struct iTask
{
    mfxENCInput  in;
    mfxENCOutput out;
    mfxPAKInput  inPAK;
    mfxPAKOutput outPAK;
    BiFrameLocation m_loc;
    mfxSyncPoint EncSyncP;
    mfxI32 encoded;
    bufSet* bufs;
    bufSet* preenc_bufs;
    std::list<PreEncMVPInfo> preenc_mvp_info;
    mfxFrameSurface1 *fullResSurface;

    mfxU16 PicStruct;
    mfxU16 BRefType;

    //..............................reflist control............................................
    ArrayDpbFrame   m_dpb[2];
    ArrayDpbFrame   m_dpbPostEncoding; // dpb after encoding a frame (or 2 fields)
    ArrayU8x33      m_list0[2];
    ArrayU8x33      m_list1[2];
    PairU8          m_type;
    mfxU8           m_fid[2];          // progressive fid=[0,0]; tff fid=[0,1]; bff fid=[1,0]
    mfxU8           m_fieldPicFlag;
    PairI32         m_poc;

    mfxU32  m_frameOrderIdrInDisplayOrder; // most recent idr frame in display order
    mfxU32  m_frameOrderIdr;    // most recent idr frame in encoding order
    mfxU32  m_frameOrderI;      // most recent i frame in encoding order
    mfxU32  m_frameOrder;

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
};

inline mfxU8 GetFirstField(iTask& task)
{
    return (task.PicStruct & MFX_PICSTRUCT_FIELD_BFF) ? 1 : 0;
}

inline mfxI32 GetPoc(iTask& task, mfxU32 parity)
{
    return 2 * ((task.m_frameOrder - task.m_frameOrderIdr) & 0x7fffffff) + (parity != GetFirstField(task));
}

inline mfxU8 ExtractFrameType(iTask& task)
{
    return task.m_type[GetFirstField(task)];
}

inline mfxU8 ExtractFrameType(iTask& task, mfxU32 fieldId)
{
    if (!fieldId){
        return task.m_type[GetFirstField(task)];
    }
    else{
        return task.m_type[!GetFirstField(task)];
    }
}

inline mfxU16 createType(iTask& task)
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

enum
{
    TFIELD = 0,
    BFIELD = 1
};

inline void InitNewDpbFrame(
    DpbFrame &      ref,
    iTask & task,
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

#endif // #define __SAMPLE_FEI_DEFS_H__
