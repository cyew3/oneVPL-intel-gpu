/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "mfx_frame_reorderer.h"
#include "mfx_pipeline_defs.h"

static const mfxU32 MAX_NUM_REF_FRAME = 16;
static const mfxU32 MAX_FRAME_ORDER = 0x80000000;

namespace
{
    // copied from H264 ENCODE to genereate exactly same types
    mfxU8 GetFrameTypeH264(mfxU32 pos, mfxI32 GopPicSize, mfxI32 GopRefDist, mfxI32 IdrInterval, mfxU16 gopOptFlag)
    {
        if( !pos ){
            return MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
        }else{
            if( GopPicSize == 1 ) //Only I frames
            {
                if (IdrInterval == 0)
                    return MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
                else
                    return MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
            }

            mfxU32 frameInGOP = pos;
            if (GopPicSize > 0)
                frameInGOP = pos % GopPicSize;

            if( frameInGOP == 0 ){
                mfxU32 frameInIdrInterval = pos%(GopPicSize * (IdrInterval + 1));
                if (frameInIdrInterval == 0)
                    return MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
                else
                    return MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
            }
            if( GopRefDist == 1 || GopRefDist == 0 ){ //Only I,P frames
                return MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            }else{
                mfxI32 frameInPattern = (frameInGOP - 1) % GopRefDist;
                if (frameInPattern == GopRefDist - 1)
                    return MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                else if (frameInGOP == (mfxU32)(GopPicSize - 1) && (gopOptFlag & MFX_GOP_STRICT) == 0)
                    return MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                else
                    return MFX_FRAMETYPE_B;
            }
        }
    }

    mfxU8 GetFrameTypeH264(mfxU32 pos, const mfxVideoParam& par)
    {
        return GetFrameTypeH264(pos, par.mfx.GopPicSize, par.mfx.GopRefDist, par.mfx.IdrInterval, par.mfx.GopOptFlag);
    }

    bool IsOlder(mfxU32 frameOrder1, mfxU32 frameOrder2)
    {
        return (frameOrder1 - frameOrder2) >= 0x80000000;
    }

    void SwitchNewestB2P(std::vector<MFXH264FrameReorderer::Frame> & bufferedFrames)
    {
        MFXH264FrameReorderer::Frame * newestFrame = 0;

        for (size_t i = 0; i < bufferedFrames.size(); i++)
        {
            if (bufferedFrames[i].m_surface)
            {
                if (newestFrame == 0 || IsOlder(newestFrame->m_frameOrder, bufferedFrames[i].m_frameOrder))
                    newestFrame = &bufferedFrames[i];
            }
        }

        if (newestFrame && (newestFrame->m_frameType & MFX_FRAMETYPE_B))
            newestFrame->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
    }

    mfxU32 CountL1Refs(
        const MFXH264FrameReorderer::Frame& frame,
        const std::vector<MFXH264FrameReorderer::Frame>& dpb,
        mfxU32 numFrameInDpb)
    {
        mfxU32 numL1Refs = 0;
        for (mfxU32 j = 0; j < numFrameInDpb; j++)
        {
            if (IsOlder(frame.m_frameOrder, dpb[j].m_frameOrder))
            {
                numL1Refs++;
            }
        }

        return numL1Refs;
    }

    mfxU32 ViewId2Idx(const mfxExtMVCSeqDesc & desc, mfxU32 viewId)
    {
        mfxU32 i = 0;
        for (; i < desc.NumView; i++)
            if (viewId == desc.View[i].ViewId)
                break;
        return i;
    }
}

MFXH264FrameReorderer::MFXH264FrameReorderer(const mfxVideoParam& par)
: m_par(par)
, m_extMvc(0)
, m_views()
{
    if (par.ExtParam)
        for (mfxU32 i = 0; i < par.NumExtParam && m_extMvc == 0; i++)
            if (par.ExtParam[i] && par.ExtParam[i]->BufferId == MFX_EXTBUFF_MVC_SEQ_DESC)
                m_extMvc = (mfxExtMVCSeqDesc *)par.ExtParam[i];

    m_views.resize(m_extMvc ? m_extMvc->NumView : 1);

    for (size_t i = 0; i < m_views.size(); i++)
    {
        m_views[i].m_frameOrder = MAX_FRAME_ORDER - 1;
        m_views[i].m_gopOptFlag = par.mfx.GopOptFlag;

        m_views[i].m_bufferedFrames.resize(par.mfx.GopRefDist);
        m_views[i].m_dpb.resize(par.mfx.NumRefFrame);
    }
}

MFXH264FrameReorderer::~MFXH264FrameReorderer()
{
}

mfxStatus MFXH264FrameReorderer::ReorderFrame(mfxFrameSurface1 * in, mfxFrameSurface1 ** out, mfxU16 * frameType)
{
    mfxU32 viewIdx = 0;

    // determine viewIdx
    if (in == 0)
    {
        // no more data incoming
        // pop buffered frames
        for (size_t i = 1; i < m_views.size(); i++)
            if (m_views[i].m_numFrameBuffered > m_views[0].m_numFrameBuffered)
            {
                viewIdx = mfxU32(i);
                break;
            }
    }
    else if (m_extMvc != 0)
    {
        // mvc case
        viewIdx = ViewId2Idx(*m_extMvc, in->Info.FrameId.ViewId);
        if (viewIdx >= m_extMvc->NumView)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    else
    {
        // avc case
        viewIdx = 0;
    }

    mfxStatus sts = m_views[viewIdx].ReorderFrame(m_par, in, out, frameType);
    if (sts != MFX_ERR_NONE)
        return sts;

    if (m_extMvc != 0)
    {
        // check if frame type needs to be changed
        // due to view dependencies

        const mfxMVCViewDependency & dep = m_extMvc->View[viewIdx];
        mfxU16 numRefsL0 = (*frameType & MFX_FRAMETYPE_IDR) ? dep.NumAnchorRefsL0 : dep.NumNonAnchorRefsL0;
        mfxU16 numRefsL1 = (*frameType & MFX_FRAMETYPE_IDR) ? dep.NumAnchorRefsL1 : dep.NumNonAnchorRefsL1;

        const mfxU16 MFX_FRAMETYPE_IP  = MFX_FRAMETYPE_P | MFX_FRAMETYPE_B;
        const mfxU16 MFX_FRAMETYPE_IPB = MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B;

        if (numRefsL0 > 0 && (*frameType & MFX_FRAMETYPE_I))
        {
            *frameType &= ~MFX_FRAMETYPE_IPB;
            *frameType |= MFX_FRAMETYPE_P;
        }

        if (numRefsL1 > 0 && (*frameType & MFX_FRAMETYPE_IP))
        {
            *frameType &= ~MFX_FRAMETYPE_IPB;
            *frameType |= MFX_FRAMETYPE_B;
        }
    }

    return sts;
}

namespace
{
    typedef MFXH264FrameReorderer::Frame Frame;

    Frame * FindFrameToEncode(
        std::vector<Frame> &       bufferedFrames,
        std::vector<Frame> const & dpb,
        mfxU32                     numFrameInDpb)
    {
        // find frame to encode
        // it is frame with minimal FrameOrder
        // special check for B frames makes reordering
        Frame * frameToEncode = 0;
        
        // frame with minimal FrameOrder w/o check for B frames
        Frame * oldestFrame = 0;

        for (size_t i = 0; i < bufferedFrames.size(); i++)
        {
            if (bufferedFrames[i].m_surface != 0)
            {
                if (oldestFrame == 0 ||
                    IsOlder(bufferedFrames[i].m_frameOrder, oldestFrame->m_frameOrder))
                {
                    oldestFrame = &bufferedFrames[i];
                }

                if (frameToEncode == 0 ||
                    IsOlder(bufferedFrames[i].m_frameOrder, frameToEncode->m_frameOrder))
                {
                    if ((bufferedFrames[i].m_frameType & MFX_FRAMETYPE_B) &&
                        (bufferedFrames[i].m_noL1Ref == 0))
                    {
                        // count number of list1 references
                        // B frames needs at least one such reference
                        // except for cases when B frame is the last one in GOP
                        if (CountL1Refs(bufferedFrames[i], dpb, numFrameInDpb) > 0)
                        {
                            frameToEncode = &bufferedFrames[i];
                        }
                    }
                    else
                    {
                        frameToEncode = &bufferedFrames[i];
                    }
                }
            }
        }

        if (frameToEncode != 0 && oldestFrame != 0 && IsOlder(oldestFrame->m_frameOrderIdr, frameToEncode->m_frameOrderIdr))
        {
            // no reordering over gop boundaries
            // if there are B frames in current gop, encode it before starting next gop
            frameToEncode = oldestFrame;
        }

        return frameToEncode;
    }
};

mfxStatus MFXH264FrameReorderer::DataPerView::ReorderFrame(const mfxVideoParam & par, mfxFrameSurface1 * in, mfxFrameSurface1 ** out, mfxU16 * frameType)
{
    if (in != 0)
    {
        // find free frame surface in buffer
        // there always should be one
        // otherwise logic error
        Frame * newFrame = 0;
        for (size_t i = 0; i < m_bufferedFrames.size(); i++)
        {
            if (m_bufferedFrames[i].m_surface == 0)
            {
                newFrame = &m_bufferedFrames[i];
                break;
            }
        }
        if (newFrame == 0)
        {
            return MFX_ERR_MORE_SURFACE;
        }

        m_frameOrder = (m_frameOrder + 1) % MAX_FRAME_ORDER;
        mfxU8 type = GetFrameTypeH264(m_frameOrder, par);

        if ((type & MFX_FRAMETYPE_IDR) ||
            ((type & MFX_FRAMETYPE_I) && (m_gopOptFlag & MFX_GOP_CLOSED)))
            m_frameOrderIdr = m_frameOrder;

        /* !!! commented to follow current H264 ENCODE reordering (which seems wrong)

        // mark all buffered B-frames as allowed to have no list1 reference
        if (type & MFX_FRAMETYPE_IDR)
        {
            MarkAllBufferedBFrames(m_bufferedFrames);
        }
        */

        IncreaseReference(&in->Data);
        newFrame->m_surface       = in;
        newFrame->m_frameType     = type;
        newFrame->m_noL1Ref       = 0;
        newFrame->m_frameOrder    = m_frameOrder;
        newFrame->m_frameOrderIdr = m_frameOrderIdr;

        MFX_CHECK(m_numFrameBuffered < m_bufferedFrames.size());
        m_numFrameBuffered++;
    }

    Frame * frameToEncode = FindFrameToEncode(m_bufferedFrames, m_dpb, m_numFrameInDpb);
    
    if (frameToEncode == 0 && in == 0 && m_numFrameBuffered > 0)
    {
        // no more frames incoming
        // and no buffered frame may be encoded
        // this happens when all buffered frames are B frames
        // switch latest one to P frame
        SwitchNewestB2P(m_bufferedFrames);

        // and try again
        frameToEncode = FindFrameToEncode(m_bufferedFrames, m_dpb, m_numFrameInDpb);
    }

    if (frameToEncode == 0)
    {
        return MFX_ERR_MORE_DATA;
    }

    // idr picture reset dpb
    if (frameToEncode->m_frameType & MFX_FRAMETYPE_IDR)
    {
        m_numFrameInDpb = 0;
    }

    // insert current frame in decoded picture buffer (dpb)
    // for correct reordering
    if (frameToEncode->m_frameType & MFX_FRAMETYPE_REF)
    {
        mfxU32 insertAt = m_numFrameInDpb;
        if (m_numFrameInDpb == par.mfx.NumRefFrame)
        {
            // dpb is full, perform sliding window alforithm
            insertAt = 0;
            for (mfxU32 i = 1; i < m_numFrameInDpb; i++)
            {
                if (IsOlder(m_dpb[i].m_frameOrder, m_dpb[insertAt].m_frameOrder))
                {
                    insertAt = i;
                }
            }
        }
        else
        {
            m_numFrameInDpb++;
        }

        m_dpb[insertAt] = *frameToEncode;
    }

    *out = frameToEncode->m_surface;
    (*out)->Data.FrameOrder = frameToEncode->m_frameOrder;
    *frameType = frameToEncode->m_frameType;

    DecreaseReference(&frameToEncode->m_surface->Data);
    frameToEncode->m_surface = 0;

    MFX_CHECK(m_numFrameBuffered > 0);
    m_numFrameBuffered--;
    return MFX_ERR_NONE;
}

static void GetFrameTypeMpeg2(mfxU32 FrameOrder, mfxU16 GOPSize, mfxU16 IPDist, bool bClosedGOP, mfxU16* FrameType)
{
    mfxU32 pos = (GOPSize)? (FrameOrder %(GOPSize)):FrameOrder;

    if (pos == 0 || IPDist == 0)
    {
        *FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
    }
    else
    {
        pos = (bClosedGOP && pos + 1 == GOPSize) ? 0 : (pos % IPDist);
        *FrameType  = (pos != 0) ? (mfxU16)MFX_FRAMETYPE_B : (mfxU16)(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);
    }
}

MFXMPEG2FrameReorderer::MFXMPEG2FrameReorderer(const mfxVideoParam& par, mfxStatus & ret)
: m_par(par)
, m_frameOrder(0)
{
    // these Inits never returns an error
    ret = m_gop.Init(par.mfx.GopRefDist - 1);
    if (MFX_ERR_NONE != ret)
        return;
    ret = m_waitingList.Init(par.mfx.GopPicSize);
    
    if (MFX_ERR_NONE != ret)
        return;
}

MFXMPEG2FrameReorderer::~MFXMPEG2FrameReorderer()
{
}

mfxStatus MFXMPEG2FrameReorderer::ReorderFrame(mfxFrameSurface1 * in, mfxFrameSurface1 ** out, mfxU16 * frameType)
{
    // Add current frame into waiting list
    if (in != 0)
    {
        mfxU16 frameType2 = 0;
        GetFrameTypeMpeg2(
            m_frameOrder,
            m_par.mfx.GopPicSize, 
            m_par.mfx.GopRefDist,
            false/*(mfxParamsVideo.mfx.GopOptFlag & MFX_GOP_CLOSED)!=0*/,
            &frameType2);
                 
        bool bOK = m_waitingList.AddFrame(in, frameType2);
        if (!bOK)
        {
            return MFX_ERR_MORE_SURFACE;
        }

        IncreaseReference(&in->Data);
        in->Data.FrameOrder = m_frameOrder;

        m_frameOrder++;
    }

    // Fill GOP structure using waiting list
    for(;;)
    {
       mfxFrameSurface1* CurFrame = 0;
       mfxU16        CurType  = 0;
       CurFrame = m_waitingList.GetCurrFrame();
       if (!CurFrame)
          break;
       CurType = m_waitingList.GetCurrType();
       if (!m_gop.AddFrame(CurFrame, CurType))
          break;
       m_waitingList.MoveOnNextFrame();
    }

    if (in == 0)
    {
        m_gop.CloseGop();
    }

    // Extract next frame from GOP structure 
    *out = m_gop.GetInFrameForDecoding();
    if (*out == 0)
    {
        return MFX_ERR_MORE_DATA;
    }

    *frameType = m_gop.GetFrameForDecodingType();

    DecreaseReference(&(*out)->Data);
    m_gop.ReleaseCurrentFrame();

    return MFX_ERR_NONE;
}


MFXParFileFrameReorderer::MFXParFileFrameReorderer(const vm_char* file, mfxStatus & ret)
    : m_par_file(0)
    , m_nextFrame(-1)
    , m_nextType(0)
    , m_eof(false)
    , m_nFrames(0)
    , m_nFrames0(0)
{
    ret = MFX_ERR_NONE;

    m_par_file = vm_file_fopen(file, VM_STRING("r"));

    if (!m_par_file)
        ret = MFX_ERR_NULL_PTR;
}

MFXParFileFrameReorderer::~MFXParFileFrameReorderer()
{
    if (m_par_file)
        vm_file_close(m_par_file);
}

mfxStatus MFXParFileFrameReorderer::ReorderFrame(mfxFrameSurface1 * in, mfxFrameSurface1 ** out, mfxU16 * frameType)
{
    mfxU32 cycleCnt = 0;
start:
    if (!m_eof && m_nextFrame < 0)
    {
        vm_char sbuf[256], *pStr;

        pStr = vm_file_fgets(sbuf, 256, m_par_file);
        m_eof = !pStr || (2 != vm_string_sscanf(pStr, VM_STRING("%i %i"), &m_nextFrame, &m_nextType));


        if (m_eof)
        {
            m_nFrames = m_nFrames0;

            vm_file_fseek(m_par_file, 0, VM_FILE_SEEK_SET);

            pStr = vm_file_fgets(sbuf, 256, m_par_file);
            m_eof = !pStr || (2 != vm_string_sscanf(pStr, VM_STRING("%i %i"), &m_nextFrame, &m_nextType));
        }

        m_nextFrame += m_nFrames;
        m_nFrames0++;
    }

    if (m_eof)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (in != 0)
    {
        m_frames[in->Data.FrameOrder] = in;
        IncreaseReference(&in->Data);
    }

    if (m_frames.count(m_nextFrame) == 0)
    {
        if (in == 0 && !m_frames.empty())
        {
            if (cycleCnt++ < 10)
            {
                m_nextFrame = -1;
                goto start;
            }
            else
                return MFX_ERR_MORE_DATA;
        }
        else
            return MFX_ERR_MORE_DATA;
    }

    *out = m_frames[m_nextFrame];
    *frameType = (mfxU16)m_nextType;

    m_frames.erase(m_nextFrame);
    DecreaseReference(&(*out)->Data);

    m_nextFrame = -1;
    m_nextType = 0;

    return MFX_ERR_NONE;
}