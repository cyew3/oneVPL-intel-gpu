/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_FRAME_REORDERER_H
#define __MFX_FRAME_REORDERER_H

#include <vector>
#include "mfxstructures.h"
#include "mfxmvc.h"
#include "mfx_frames_new.h"

class MFXFrameReorderer
{
public:
    virtual ~MFXFrameReorderer() {}
    virtual mfxStatus ReorderFrame(mfxFrameSurface1 * in, mfxFrameSurface1 ** out, mfxU16 * frameType) = 0;
};

class MFXH264FrameReorderer : public MFXFrameReorderer
{
public:
    explicit MFXH264FrameReorderer(const mfxVideoParam & par);
    virtual ~MFXH264FrameReorderer();
    virtual mfxStatus ReorderFrame(mfxFrameSurface1 * in, mfxFrameSurface1 ** out, mfxU16 * frameType);

    struct Frame
    {
        Frame()
            : m_surface(0)
            , m_frameOrder(0)
            , m_frameType(0)
            , m_noL1Ref(0)
        {
        }

        mfxFrameSurface1 * m_surface;
        mfxU32 m_frameOrder;
        mfxU32 m_frameOrderIdr; // frameOrder of most recent idr frame (in display order),
                                // identifies gop which current picture belongs to
        mfxU16 m_frameType;
        mfxU8  m_noL1Ref;
    };

protected:

    struct DataPerView
    {
        DataPerView()
            : m_frameOrder(0)
            , m_numFrameInDpb(0)
            , m_numFrameBuffered(0)
            , m_bufferedFrames()
            , m_dpb()
        {
        }

        mfxStatus ReorderFrame(const mfxVideoParam & par, mfxFrameSurface1 * in, mfxFrameSurface1 ** out, mfxU16 * frameType);

        mfxU32 m_frameOrder;
        mfxU32 m_frameOrderIdr; // frameOrder of most recent idr frame (in display order)
        mfxU32 m_numFrameInDpb;
        mfxU32 m_numFrameBuffered;
        mfxU32 m_gopOptFlag;

        std::vector<Frame> m_bufferedFrames;
        std::vector<Frame> m_dpb;
    };

    mfxVideoParam            m_par;
    mfxExtMVCSeqDesc *       m_extMvc;
    std::vector<DataPerView> m_views;
};

class MFXMPEG2FrameReorderer : public MFXFrameReorderer
{
public:
    explicit MFXMPEG2FrameReorderer(const mfxVideoParam& par, mfxStatus & ret);
    virtual ~MFXMPEG2FrameReorderer();
    virtual mfxStatus ReorderFrame(mfxFrameSurface1 * in, mfxFrameSurface1 ** out, mfxU16 * frameType);

protected:
    mfxVideoParam m_par;
    MFXWaitingList m_waitingList;
    MFXGOP_ m_gop;
    mfxU32 m_frameOrder;
};


#endif//__MFX_FRAME_REORDERER_H

