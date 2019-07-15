/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_iyuv_source.h"
#include "mfx_mvc_decoder.h"
#include "hash_array.h"

class TargetViewsDecoder : public InterfaceProxy<IYUVSource>
{
public:
    TargetViewsDecoder( std::vector<std::pair<mfxU16, mfxU16> > & targetViewsMap
                      , mfxU16      temporalId
                      , std::unique_ptr<IYUVSource> &&pTarget);

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par);
    virtual mfxStatus DecodeFrameAsync( mfxBitstream2 & bs
                                      , mfxFrameSurface1 *surface_work
                                      , mfxFrameSurface1 **surface_out
                                      , mfxSyncPoint *syncp);


protected:

    mfxStatus DecodeHeaderTarget(mfxBitstream *bs, mfxVideoParam *par);
    mfxStatus ResetDependencies();
    //returns corrected dependencies
    void ResetDependencyForList(mfxU16 nListLen, const mfxU16 * pList,
                                mfxU16 &nListLenResult, mfxU16 nMaxListLen, mfxU16 * pListResult);

    //clean m_target and m_modified buffers structures
    void CleanBuffers();


    std::vector<std::pair<mfxU16, mfxU16> > m_ViewOrder_ViewIdMap;
    hash_array<mfxU16, mfxU16> m_ViewId_ViewIdMap;
    MFXExtBufferVector  m_TargetExtParams;
    MFXExtBufferVector  m_ModifiedExtParams;
    mfxU16  m_targetProfile;
    mfxU16  m_TemporalId;

    class MVCDecoderHelper : public MVCDecoder
    {
    public:
        MVCDecoderHelper( bool bGenerateViewIds
                        , mfxVideoParam &frameParam
                        , std::unique_ptr<IYUVSource> &&pTarget)
            : MVCDecoder(bGenerateViewIds, frameParam, std::move(pTarget) )
        {
        }

    protected:
        virtual mfxStatus DecodeHeaderTarget(mfxBitstream *, mfxVideoParam *)
        {
            return MFX_ERR_NONE;
        }
    };
};
