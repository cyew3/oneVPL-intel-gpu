/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.


File Name: mfx_encode.h

\* ****************************************************************************** */

#pragma once

#include <limits>
#include "mfx_ivideo_encode.h"

class FieldOutputEncoder : public InterfaceProxy<IVideoEncode>
{
    typedef  InterfaceProxy<IVideoEncode> base;
public:

    FieldOutputEncoder (std::unique_ptr<IVideoEncode> &&pTarget)
        : base(std::move(pTarget))
        , bPairCompleted(true)
    {}

    virtual mfxStatus EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
    {
        mfxStatus sts = MFX_ERR_NONE;

        if (bPairCompleted)
        {
            MFX_CHECK_STS_SKIP(sts = EncodeFirst(ctrl, surface, bs, syncp), MFX_ERR_MORE_DATA, MFX_ERR_MORE_BITSTREAM);
        }

        //lets handle this error codes in outer object
        if (MFX_WRN_DEVICE_BUSY == sts || MFX_ERR_MORE_DATA == sts)
            return sts;

        MFX_CHECK_STS_SKIP(sts = EncodeSecond(ctrl, surface, syncp), MFX_ERR_MORE_DATA);

        return sts;
    }

    //doubling of suggested buffer size
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        mfxStatus sts = base::GetVideoParam(par);
        if (sts >= MFX_ERR_NONE)
        {
            if (par->mfx.BRCParamMultiplier > ((std::numeric_limits<mfxU16>::max)() >> 1))
                return MFX_ERR_DEVICE_FAILED;
            par->mfx.BRCParamMultiplier <<= 1;
        }
        return sts;
    }

    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait)
    {
        container_type::iterator i;
        for( i = m_activeTasks.begin(); i != m_activeTasks.end(); i++)
        {
            if (i->p1 == syncp)
                break;
        }
                //todo: if client waits for unknown syncpoint we assume that this syncpoint already completed and will return no error
        if (i == m_activeTasks.end())
        {
            return MFX_ERR_NONE;
        }

        mfxStatus sts  = MFX_ERR_NONE;

        //wait both subtasks
        MFX_CHECK_STS_SKIP(sts = base::SyncOperation(i->p1, wait), MFX_WRN_IN_EXECUTION);
        if (MFX_WRN_IN_EXECUTION == sts)
            return sts;

        MFX_CHECK_STS_SKIP(sts = base::SyncOperation(i->p2, wait), MFX_WRN_IN_EXECUTION);
        if (MFX_WRN_IN_EXECUTION == sts)
            return sts;

        //merging 1 bitstream
        i->pBsActual->DataLength += i->bs1.DataLength;
        //merging 2nd bitstream
        MFX_CHECK_STS(BSUtil::MoveNBytesTail(i->pBsActual, &i->bs2, i->bs2.DataLength));

        //removing task from pool
        m_activeTasks.erase(i);
        return MFX_ERR_NONE;
    }

protected:
    mfxStatus EncodeFirst(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint * /*syncp*/)
    {
        m_activeTasks.push_back(EncodeTask());

        //assumption made that input bitstream always have enough space for encode 2 fields
        mfxStatus sts  = MFX_ERR_NONE;
        EncodeTask &et = m_activeTasks.back();
        MFX_ZERO_MEM(et);

        et.pBsActual = bs;
        
        et.bs1.MaxLength = BSUtil::WriteAvailTail(bs) >> 1;
        et.bs1.Data = bs->Data + bs->DataOffset + bs->DataLength;

        et.bs2.MaxLength = et.bs1.MaxLength ;
        et.bs2.Data = et.bs1.Data + et.bs1.MaxLength;

        MFX_CHECK_STS_SKIP(sts = base::EncodeFrameAsync(ctrl, surface, &et.bs1, &et.p1)
            , MFX_ERR_MORE_DATA
            , MFX_ERR_MORE_BITSTREAM
            , MFX_WRN_DEVICE_BUSY);

        if (sts == MFX_WRN_DEVICE_BUSY ||
            sts == MFX_ERR_MORE_DATA)//in case of more data we dont have syncpoint, and we dont need to call to encodesecond
        {
            // nothing is queued by encode need to remove from list
            m_activeTasks.pop_back();
        }
        else
        {
            //only 1 part of result completed
            bPairCompleted = false;
        }
        
        return sts;
    }

    mfxStatus EncodeSecond(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxSyncPoint *syncp)
    {
        MFX_CHECK (!m_activeTasks.empty());

        mfxStatus sts  = MFX_ERR_NONE;
        EncodeTask &et = m_activeTasks.back();

        //MFX_ERR_MORE_DATA, and MFX_ERR_MORE_BITSTREAM incorrect status code, because first part already buffered by encoder
        MFX_CHECK_STS(sts = base::EncodeFrameAsync(ctrl, surface, &et.bs2, &et.p2));

        MFX_CHECK_POINTER(syncp);
        
        //first syncpoint from pair 
        *syncp = et.p1;

        //2 part of result completed
        bPairCompleted = true;

        return sts;
    }

    bool bPairCompleted;
    struct EncodeTask
    {
        mfxSyncPoint p1;
        mfxSyncPoint p2;
        mfxBitstream bs1;
        mfxBitstream bs2;
        mfxBitstream *pBsActual;//external bs
    };

    typedef std::list<EncodeTask> container_type;
    container_type m_activeTasks;
};
