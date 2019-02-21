// Copyright (c) 2003-2019 Intel Corporation
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
#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#ifndef __UMC_H264_AVC_CONTROLLER_H
#define __UMC_H264_AVC_CONTROLLER_H

#include "umc_structures.h"
#include "umc_h264_dec_defs_dec.h"
#include "umc_media_data_ex.h"
#include "umc_h264_heap.h"
#include "umc_h264_slice_decoding.h"
#include "umc_h264_frame_info.h"

#include "umc_h264_segment_decoder_mt.h"
#include "umc_h264_headers.h"

#include "test_h264_bsd_dec_utils.h"

namespace UMC
{
class MFXFrame;
class H264DBPList;
struct H264SliceHeader;
class MediaData;
class NALUnitSplitter;
class H264Slice;
class H264DecoderFrame;

class BaseCodecParams;

class MemoryAllocator;

/****************************************************************************************************/
// Skipping class routine
/****************************************************************************************************/
class AVCSkipping
{
public:
    AVCSkipping();

    bool IsShouldSkipDeblocking(H264DecoderFrame * pFrame, Ipp32s field);
    bool IsShouldSkipFrame(H264DecoderFrame * pFrame, Ipp32s field);
    void ChangeVideoDecodingSpeed(Ipp32s& num);
    void Reset();

private:

    Ipp32s m_VideoDecodingSpeed;
    Ipp32s m_SkipCycle;
    Ipp32s m_ModSkipCycle;
    Ipp32s m_PermanentTurnOffDeblocking;
    Ipp32s m_SkipFlag;

    Ipp32s m_NumberOfSkippedFrames;
};

/****************************************************************************************************/
// AVCController
/****************************************************************************************************/
class AVCController : public AVCSkipping
{
public:

    AVCController();
    virtual ~AVCController();

    virtual Status Init(BaseCodecParams *pInit);

    virtual void Reset();
    virtual void Close();

    Status GetInfo(VideoDecoderParams *lpInfo);
    Status GetFrame(MediaData * pSource, MediaData *dst);

    void SetMemoryAllocator(MemoryAllocator *pMemoryAllocator)
    {
        m_pMemoryAllocator = pMemoryAllocator;
    }

    H264DecoderFrame *GetFrameToDisplayInternal(MediaData *dst, bool force);
    virtual bool GetFrameToDisplay(MediaData *dst, bool force);
    Status  SetParams(BaseCodecParams* params);

    bool IsWantToShowFrame(bool force = false);

    H264DBPList  * GetDPBList()
    {
        return m_pDecodedFramesList;
    }

    void AnalyzeSource(mfxBitstream *in);

    void AfterDecoding(mfxFrameCUC * CUC);

    mfxFrameCUC * GetCUCToDecode();
    mfxFrameCUC * GetCUCToDisplay(bool force);

    void PrepareReferenceCUC(mfxFrameCUC * cuc, MFXFrame * frm);

protected:
    friend class NALUnitSplitter;

    void SlideWindow(H264Slice * pSlice, Ipp32s field_index, bool force = false);
    void AddSliceToFrame(H264DecoderFrame *pFrame, H264Slice *pSlice);
    virtual H264DecoderFrame * AddFrame(H264Slice *pSlice);
    bool AddSlice(H264MemoryPiece * pMem, MediaData * pSource);
    virtual void InitFrame(H264DecoderFrame * pFrame, H264Slice *pSlice);
    virtual void CompleteFrame(H264DecoderFrame * pFrame, Ipp32s m_field_index);
    void ProcessNonPairedField(H264DecoderFrame * pFrame);
    virtual Status InitFreeFrame(H264DecoderFrame * pFrame, H264Slice *pSlice);

    void DBPUpdate(H264DecoderFrame * pFrame, Ipp32s field);
    Status UpdateRefPicMarking(H264DecoderFrame * pFrame, H264Slice * pSlice, Ipp32s field_index);

    Status AddSource(MediaData * pSource, MediaData *dst);

    Status DecodeHeaders(MediaDataEx::_MediaDataEx *pSource, H264MemoryPiece * pMem);

    Status ProcessFrameNumGap(H264Slice *slice, Ipp32s field);

    // Obtain free frame from queue
    virtual H264DecoderFrame *GetFreeFrame();

    void   DecodePictureOrderCount(H264Slice *slice, Ipp32s frame_num);

    Status SetDPBSize();

    H264_Heap_Objects           m_ObjHeap;

    Ipp32s m_iThreadNum;

    NALUnitSplitter * m_pNALSplitter;
    H264_Heap      m_Heap;

    // This is the value at which the TR Wraps around for this
    // particular sequence.
    Ipp32s      m_MaxLongTermFrameIdx;

    Ipp32s      m_field_index;

    Headers     m_Headers;

    Ipp64f      m_local_frame_time;
    Ipp64f      m_local_delta_frame_time;

    H264DecoderFrame * m_pFirstUncompletedFrame;
    H264DBPList  *m_pDecodedFramesList;

    H264DecoderFrame *m_pCurrentFrame;
    H264DecoderFrame *m_pLastDisplayed;

    Ipp32s                     m_iCurrentResource;

    Ipp32s                     m_PrevFrameRefNum;
    Ipp32s                     m_FrameNum;
    Ipp32s                     m_PicOrderCnt;
    Ipp32s                     m_PicOrderCntMsb;
    Ipp32s                     m_PicOrderCntLsb;
    Ipp32s                     m_FrameNumOffset;
    Ipp32u                     m_TopFieldPOC;
    Ipp32u                     m_BottomFieldPOC;

    // At least one sequence parameter set and one picture parameter
    // set must have been read before a picture can be decoded.
    bool              m_bSeqParamSetRead;
    bool              m_bPicParamSetRead;

    // Keep track of which parameter set is in use.
    Ipp32s            m_CurrentSeqParamSet;
    Ipp32s            m_CurrentPicParamSet;
    bool              m_WaitForIDR;

    Ipp32s            m_dpbSize;
    Ipp32s            m_maxDecFrameBuffering;
    Ipp32s            m_DPBSizeEx;
    Ipp32s            m_TrickModeSpeed;

    NotifiersChain    m_DefaultNotifyChain;

    Ipp32s m_UIDFrameCounter;

    MemoryAllocator *m_pMemoryAllocator;

private:
    AVCController & operator = (AVCController &)
    {
        return *this;

    } // AVCController & operator = (AVCController &)

    mfxVideoParam m_videoPar;
};

} // namespace UMC

#endif // __UMC_H264_AVC_CONTROLLER_H
#endif // MFX_ENABLE_H264_VIDEO_DECODE
