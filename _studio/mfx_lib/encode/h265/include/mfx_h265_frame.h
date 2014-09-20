//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_FRAME_H__
#define __MFX_H265_FRAME_H__

#include <list>
#include "vm_interlocked.h"
#include "mfx_h265_defs.h"
#include "mfx_h265_ctb.h"
#include "mfx_h265_set.h"

namespace H265Enc {


class H265Frame
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
    Ipp32u m_PicCodType;
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

    bool isDisposable()
    {
        return (!m_isShortTermRef && !m_isLongTermRef);
    }

    bool IsReference()
    {
        return (isShortTermRef() || isLongTermRef() );
    }

    // API for short_ref
    Ipp8u isShortTermRef()
    {
        return m_isShortTermRef;
    }
    void SetisShortTermRef()
    {
        m_isShortTermRef = true;
    }
    void unSetisShortTermRef()
    {
        m_isShortTermRef = false;
    }

    // API for long_ref
    Ipp8u isLongTermRef()
    {
        return m_isLongTermRef;
    }

    void SetisLongTermRef()
    {
        m_isLongTermRef = true;
    }

    void unSetisLongTermRef()
    {
        m_isLongTermRef = false;
    }

    void Create(H265VideoParam *par);
    void CopyFrame(const mfxFrameSurface1 *surface);
    void doPadding();
    void doPaddingOneLine(int number, int cuSize, bool isLast);

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
    H265Slice  m_slices[10];//aya tmp hack
    H265Frame* m_dpb[16];
    Ipp32s     m_dpbSize;
    Ipp8s      m_sliceQpY;

    Task() { Reset(); }

    void Reset()
    {
        m_frameOrigin = NULL;
        m_frameRecon  = NULL;
        m_encOrder    = Ipp32u(-1);
        m_dpbSize     = 0;
        m_sliceQpY    = -1;
    }
};

typedef std::list<Task*>   TaskList;
typedef TaskList::iterator TaskIter;

typedef std::list<H265Frame*>  FramePtrList;
typedef FramePtrList::iterator FramePtrIter;


FramePtrIter GetFreeFrame(FramePtrList & queue, H265VideoParam *par);

void Dump(const vm_char* fname, H265VideoParam *par, H265Frame* frame, TaskList & dpb);

} // namespace

#endif // __MFX_H265_FRAME_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
