/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_iyuv_source.h"
#include "mfx_imultiple.h"

class MultiDecode
    : public IMultiple<IYUVSource>
{
public:
    MultiDecode()
    {
        RegisterFunction(&MultiDecode::DecodeFrameAsync);
        RegisterFunction(&MultiDecode::GetVideoParam);
    }
    virtual mfxStatus Init(mfxVideoParam * /*par*/)
    {
        for (size_t i = 0; i < m_items.size(); i++)
        {
            
            MFX_CHECK_STS(ItemFromIdx(i)->Init(&m_vParams[i]));
        }   
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Close()
    {
        ForEach(std::mem_fun(&IYUVSource::Close));
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Query(mfxVideoParam * /*in*/, mfxVideoParam * /*out*/)
    {
        return MFX_ERR_NONE;
    }
    //return number of surfaces for all decoders
    virtual mfxStatus QueryIOSurf(mfxVideoParam * /*par*/, mfxFrameAllocRequest *request)
    {
        mfxU16 nNumFramesMin = 0;
        mfxU16 nNumFramesSuggested = 0;
        for (size_t i = 0; i < m_items.size(); i++)
        {
            MFX_CHECK_STS(ItemFromIdx(i)->QueryIOSurf(&m_vParams[i], request));
            nNumFramesMin += request->NumFrameMin;
            nNumFramesSuggested += request->NumFrameSuggested;
        }   
        request->NumFrameMin = nNumFramesMin;
        request->NumFrameSuggested = nNumFramesSuggested;

        return MFX_ERR_NONE;

        //IYUVSource *current = ItemFromIdx(par->mfx.FrameInfo.FrameId.DependencyId);
        //
        //mfxStatus sts = current->QueryIOSurf(par, request);

        //return sts;
    }
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *  /*stat*/)
    {
        return MFX_ERR_NONE;
    }

    virtual mfxStatus DecodeFrameAsync(
        mfxBitstream2 &bs2,
        mfxFrameSurface1 *surface_work,
        mfxFrameSurface1 **surface_out,
        mfxSyncPoint *syncp)
    {
        IYUVSource *current = ItemFromIdx(bs2.DependencyId);
        mfxStatus sts = current->DecodeFrameAsync(bs2, surface_work, surface_out, syncp);

        if (NULL != syncp && NULL != *syncp)
            m_registeredSyncpoint[*syncp] = current;

        return sts;
    }

    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait)
    {
        IYUVSource *psource;
        MFX_CHECK_POINTER(psource = m_registeredSyncpoint[syncp]);
        return psource->SyncOperation(syncp, wait);
    }
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par)
    {
        //TODO:decode header returns more data until all decoders completed
        m_vParams.resize(m_items.size());
        for (size_t i = 0; i < m_items.size(); i++)
        {
            MFX_ZERO_MEM(m_vParams[i]);
            m_vParams[i].mfx.CodecId = par->mfx.CodecId;
            //TODO: handle decode header statuses
            for(;;)
            {
                MFX_CHECK_STS(ItemFromIdx(i)->DecodeHeader(bs, &m_vParams[i]));
                break;
            }
        }
        //something should be filled in m_vParam, use last element
        if (!m_items.empty())
        {
            *par = m_vParams[m_items.size() - 1];
        }
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        /*TODO: is it possible to call this function with single par???*/
        for (size_t i = 0; i < m_items.size(); i++)
        {
            MFX_CHECK_STS(ItemFromIdx(i)->Reset(par));
        }
        return MFX_ERR_NONE;
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        MFX_CHECK_POINTER(par);
        *par = m_vParams[CurrentItemIdx(&MultiDecode::GetVideoParam)];
        IncrementItemIdx(&MultiDecode::GetVideoParam);

        return MFX_ERR_NONE;
    }
    virtual mfxStatus SetSkipMode(mfxSkipMode /*mode*/)
    {
        return MFX_ERR_NONE;
    }

protected:
    std::map<mfxSyncPoint, IYUVSource*>m_registeredSyncpoint;
    std::vector<mfxVideoParam> m_vParams;
};