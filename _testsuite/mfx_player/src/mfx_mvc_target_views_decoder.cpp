/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */


#include "mfx_pipeline_defs.h"
#include "mfx_mvc_target_views_decoder.h"

TargetViewsDecoder::TargetViewsDecoder( std::vector<std::pair<mfxU16, mfxU16> > & targetViewsMap
                                      , mfxU16      temporalId
                                      , std::unique_ptr<IYUVSource> &&pTarget)
    : InterfaceProxy<IYUVSource>(std::move(pTarget))
    , m_ViewOrder_ViewIdMap(targetViewsMap)
    , m_targetProfile()
    , m_TemporalId(temporalId)
{
    m_TargetExtParams.push_back(new mfxExtMVCSeqDesc());
}

void TargetViewsDecoder::CleanBuffers()
{
    MFXExtBufferPtr<mfxExtMVCSeqDesc> seqDescription(m_TargetExtParams);
    if (NULL != seqDescription.get())
    {
        MFXExtBufferPtrRef<mfxExtMVCSeqDesc> seqDescriptionRef(*seqDescription);
        seqDescriptionRef.View.clear();
        seqDescriptionRef.ViewId.clear();
        seqDescriptionRef.OP.clear();
    }

    MFXExtBufferPtr<mfxExtMVCSeqDesc> seqDescriptionNew(m_ModifiedExtParams);
    if (NULL != seqDescriptionNew.get())
    {
        MFXExtBufferPtrRef<mfxExtMVCSeqDesc> seqDescriptionNewRef(*seqDescriptionNew);

        seqDescriptionNewRef.View.clear();
        seqDescriptionNewRef.ViewId.clear();
        seqDescriptionNewRef.OP.clear();
    }
}

mfxStatus TargetViewsDecoder::DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) 
{
    //TODO: need better this stateless function
    CleanBuffers();

    mfxU16 nOldNExtParams = par->NumExtParam ;
    mfxExtBuffer ** pOldExtParams = par->ExtParam;

    mfxStatus sts = DecodeHeaderTarget(bs, par);

    //in case of error different from more data and more_buffer functions exits
    if(sts != MFX_ERR_MORE_DATA && sts != MFX_ERR_NOT_ENOUGH_BUFFER && sts < MFX_ERR_NONE)
    {
        //swapping extparams back;
        par->NumExtParam      = nOldNExtParams;
        par->ExtParam         = pOldExtParams;

        return sts;
    }

    if (m_ViewOrder_ViewIdMap.size() < 2)
    {
        //the decoder becames AVC
        par->mfx.CodecProfile = 0;
    }
    else if (MFX_ERR_NONE <= sts)
    {
        //decoder remains as MVC but should report different views
        //par->mfx.CodecProfile = 0; //not sure what to point here same profile?
        par->NumExtParam = (mfxU16)m_ModifiedExtParams.size();
        par->ExtParam = &m_ModifiedExtParams;

        std::unique_ptr<IYUVSource> nullSource;
        std::unique_ptr<MVCDecoderHelper> pHlp (new MVCDecoderHelper(false, *par, std::move(nullSource)));

        //swapping extparams back;
        par->NumExtParam      = nOldNExtParams;
        par->ExtParam         = pOldExtParams;

        sts = pHlp->DecodeHeader(bs, par);
    }

    //swapping extparams back;
    par->NumExtParam      = nOldNExtParams;
    par->ExtParam         = pOldExtParams;


    return sts;
}

mfxStatus TargetViewsDecoder::DecodeHeaderTarget(mfxBitstream *bs, mfxVideoParam *par)
{
    par->NumExtParam = (mfxU16)m_TargetExtParams.size();
    par->ExtParam = &m_TargetExtParams;

    mfxStatus sts = InterfaceProxy<IYUVSource>::DecodeHeader(bs, par);

    if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
    {
        {
            //reserving more buffers
            MFXExtBufferPtr<mfxExtMVCSeqDesc> seqDescription(m_TargetExtParams);

            //no buffer at all, don't know how to handle MFX_ERR_NOT_ENOUGH_BUFFER in this situation
            MFX_CHECK_POINTER(seqDescription.get());

            MFXExtBufferPtrRef<mfxExtMVCSeqDesc> seqDescriptionRef(*seqDescription);

            //we cannot create two ref objects because after commiting it is possible to overwrite parameters
            seqDescriptionRef->View.reserve(seqDescriptionRef->NumView);
            seqDescriptionRef->ViewId.reserve(seqDescriptionRef->NumViewId);
            seqDescriptionRef->OP.reserve(seqDescriptionRef->NumOP);
        }

        MFX_CHECK_STS(sts = InterfaceProxy<IYUVSource>::DecodeHeader(bs, par));
    }
 
    if (sts >= MFX_ERR_NONE)
    {
        //TODO: scope required dueto implementation of extbuffer
        // we cannot attach 2 buffers at one time, resulted structure after decomitting extbuffers will be incorrect
        //it will lead to elements doubled for example
        {
            //correcting map : first argument will be an viewid not viewid order
            MFXExtBufferPtr<mfxExtMVCSeqDesc> seqDescription(m_TargetExtParams);
            MFXExtBufferPtrRef<mfxExtMVCSeqDesc> seqDescriptionRef(*seqDescription);

            for(size_t i = 0; i < m_ViewOrder_ViewIdMap.size(); i++)
            {
                //we cannot use such mapping
                MFX_CHECK(!seqDescriptionRef->View.empty());
                //for single view decode we allow to no specify extbuffers
                MFX_CHECK(seqDescriptionRef->View.size() > m_ViewOrder_ViewIdMap[i].first);
                m_ViewId_ViewIdMap [seqDescriptionRef->View[m_ViewOrder_ViewIdMap[i].first].ViewId] = m_ViewOrder_ViewIdMap[i].second;
            }
        }

        //also need to reset remained dependencies if they are broken now
        MFX_CHECK_STS(ResetDependencies());
    }
    
    return sts;
}

mfxStatus TargetViewsDecoder::Init(mfxVideoParam *par)
{
    //TODO: need to transfer extbuffers from par, but now they've taken from previous decode header
    mfxVideoParam vParamForInit = *par;

    //attaching special buffer with target views
    MFXExtBufferPtr<mfxExtMVCTargetViews> viewTargets(m_TargetExtParams);
    
    if (!viewTargets.get())
    {
        m_TargetExtParams.push_back(new mfxExtMVCTargetViews());

        MFXExtBufferPtr<mfxExtMVCTargetViews> viewTargets2(m_TargetExtParams);
        
        viewTargets2->NumView = (mfxU32)m_ViewOrder_ViewIdMap.size();
        
        hash_array<mfxU16, mfxU16>::iterator it;

        size_t i = 0;
        for (it = m_ViewId_ViewIdMap.begin() ; it != m_ViewId_ViewIdMap.end(); it++)
        {
            viewTargets2->ViewId[i++] = it->first;
        }

        viewTargets2->TemporalId = m_TemporalId;
    }
     //-------------------------------------MUST FIX-------------------------------------
    //TODO: workaround issue with opaq buffer not passed 
    mfxExtOpaqueSurfaceAlloc *popaq = NULL;
    for (size_t i = 0; i < par->NumExtParam; i++)
    {
        if (par->ExtParam[i]->BufferId == BufferIdOf<mfxExtOpaqueSurfaceAlloc>::id)
        {
            popaq = (mfxExtOpaqueSurfaceAlloc*)par->ExtParam[i];
        }
    }
    if (NULL != popaq)
    {
        MFXExtBufferPtr<mfxExtOpaqueSurfaceAlloc> extOpaqTest(m_TargetExtParams);
        if (NULL == extOpaqTest.get())
        {
            m_TargetExtParams.push_back(new mfxExtOpaqueSurfaceAlloc());
        }

        MFXExtBufferPtr<mfxExtOpaqueSurfaceAlloc> extOpaq(m_TargetExtParams);
        
        extOpaq->Out = popaq->Out;
    }
    //-------------------------------------MUST FIX-------------------------------------

    vParamForInit.NumExtParam = (mfxU16)m_TargetExtParams.size();
    vParamForInit.ExtParam = &m_TargetExtParams;

    MFX_CHECK_STS(InterfaceProxy<IYUVSource>::Init(&vParamForInit));

    return MFX_ERR_NONE;
}

mfxStatus TargetViewsDecoder::DecodeFrameAsync( mfxBitstream2 &bs
                                              , mfxFrameSurface1 *surface_work
                                              , mfxFrameSurface1 **surface_out
                                              , mfxSyncPoint *syncp)
{
    mfxStatus sts = InterfaceProxy<IYUVSource>::DecodeFrameAsync(bs, surface_work, surface_out, syncp);

    if (surface_out && *surface_out)
    {
        //mapping of input view
        hash_array<mfxU16, mfxU16>::iterator it;
        
        MFX_CHECK(m_ViewId_ViewIdMap.end() != (it = m_ViewId_ViewIdMap.find((*surface_out)->Info.FrameId.ViewId)));

        (*surface_out)->Info.FrameId.ViewId = it->second;
    }
    
    return sts;
}

void TargetViewsDecoder::ResetDependencyForList(mfxU16 nListLen, const mfxU16 * pList, 
                                                mfxU16 &nListLenResult, mfxU16 nMaxLen, mfxU16 * pListResult)
{
    nListLenResult = 0;
    for (int i = 0; i < nListLen; i++)
    {
        hash_array<mfxU16, mfxU16>::iterator it 
            = m_ViewId_ViewIdMap.find(pList[i]);

        if (it != m_ViewId_ViewIdMap.end())
        {
            pListResult[nListLenResult++] = m_ViewId_ViewIdMap[pList[i]];
        }
    }

    // 0 required to be in reference buffers 
    for (mfxU16 n = nListLenResult; n < nMaxLen; n++)
    {
        pListResult[n] = 0;
    }
}

mfxStatus TargetViewsDecoder::ResetDependencies()
{
    if (m_ViewOrder_ViewIdMap.size() < 2)
        return MFX_ERR_NONE;

    MFXExtBufferPtr<mfxExtMVCSeqDesc> seqDescription(m_TargetExtParams);
    MFXExtBufferPtrRef<mfxExtMVCSeqDesc> seqDescriptionRef(*seqDescription);

    {
        MFXExtBufferPtr<mfxExtMVCSeqDesc> modifiedSequence(m_ModifiedExtParams);

        if (!modifiedSequence.get())
        {
            m_ModifiedExtParams.push_back(new mfxExtMVCSeqDesc());
        }
    }

    //copying of original structure
    MFXExtBufferPtr<mfxExtMVCSeqDesc> seqDescriptionNew(m_ModifiedExtParams);
    MFXExtBufferPtrRef<mfxExtMVCSeqDesc> seqDescriptionNewRef(*seqDescriptionNew);

    for (size_t i = 0; i < m_ViewOrder_ViewIdMap.size(); i++)
    {
        seqDescriptionNewRef->ViewId.push_back(m_ViewOrder_ViewIdMap[i].second);
    }

    for(size_t i = 0; i < m_ViewOrder_ViewIdMap.size(); i++)
    {
        //searching for dependencies
        mfxMVCViewDependency &p  = seqDescriptionRef->View[m_ViewOrder_ViewIdMap[i].first];
        mfxMVCViewDependency p1;
        p1.ViewId = m_ViewOrder_ViewIdMap[i].second;
        ResetDependencyForList(p.NumAnchorRefsL0, p.AnchorRefL0,
                               p1.NumAnchorRefsL0, MFX_ARRAY_SIZE(p1.AnchorRefL0), p1.AnchorRefL0);
        ResetDependencyForList(p.NumAnchorRefsL1, p.AnchorRefL1,
                               p1.NumAnchorRefsL1, MFX_ARRAY_SIZE(p1.AnchorRefL1), p1.AnchorRefL1);
        ResetDependencyForList(p.NumNonAnchorRefsL0, p.NonAnchorRefL0,
                               p1.NumNonAnchorRefsL0, MFX_ARRAY_SIZE(p1.NonAnchorRefL0), p1.NonAnchorRefL0);
        ResetDependencyForList(p.NumNonAnchorRefsL1, p.NonAnchorRefL1,
                               p1.NumNonAnchorRefsL1, MFX_ARRAY_SIZE(p1.NonAnchorRefL1), p1.NonAnchorRefL1);

        
        seqDescriptionNewRef->View.push_back(p1);
    }
 
    return MFX_ERR_NONE;
}
