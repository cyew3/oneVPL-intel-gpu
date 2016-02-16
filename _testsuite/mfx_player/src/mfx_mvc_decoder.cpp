/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_mvc_decoder.h"

MVCDecoder::MVCDecoder(bool bGenerateViewIds, mfxVideoParam &frameParam, std::auto_ptr<IYUVSource>& pTarget)
: InterfaceProxy<IYUVSource>(pTarget)
, m_extParams(frameParam)
, m_sequence(m_extParams)
, m_nCurrentViewId()
, m_vParam()
, m_bGenerateViewsId(bGenerateViewIds)
{
    MFXExtBufferPtrRef<mfxExtMVCSeqDesc> extSeq(&m_sequence);

    if (extSeq->View.size() > 0)
    {
        if (extSeq->View.size() > 2)
        {
            m_vParam.mfx.CodecProfile = MFX_PROFILE_AVC_MULTIVIEW_HIGH;
        }else 
        {
            m_vParam.mfx.CodecProfile = MFX_PROFILE_AVC_STEREO_HIGH;
        }
        ///TODO: for now we creating only 1 OP with all views in dependency list
        extSeq->ViewId.resize(extSeq->View.size());

        for (size_t i = 0; i < extSeq->View.size(); i++)
        {
            extSeq->ViewId[i] = extSeq->View[i].ViewId;
        }
        mfxMVCOperationPoint oPoint;
        oPoint.TemporalId = 0;
        oPoint.LevelIdc = 40;
        oPoint.NumViews = (mfxU16)extSeq->View.size();
        oPoint.NumTargetViews = (mfxU16)extSeq->View.size();
        oPoint.TargetViewId = &extSeq->ViewId.front();
        extSeq->OP.push_back(oPoint);
    }
}

mfxStatus MVCDecoder::DecodeFrameAsync( mfxBitstream2 &bs,
                                        mfxFrameSurface1 *surface_work,
                                        mfxFrameSurface1 **surface_out,
                                        mfxSyncPoint *syncp)
{
    mfxStatus sts = InterfaceProxy<IYUVSource>::DecodeFrameAsync(bs, surface_work, surface_out, syncp);

    if (m_bGenerateViewsId && surface_out && *surface_out)
    {
        MFXExtBufferPtrRef<mfxExtMVCSeqDesc> extSeq (&m_sequence);
        (*surface_out)->Info.FrameId.ViewId = extSeq->View[m_nCurrentViewId].ViewId;
        m_nCurrentViewId = (m_nCurrentViewId + 1) % extSeq->View.size();

        //frame order should be corrected as well
        (*surface_out)->Data.FrameOrder /= extSeq->View.size();
    }
    
    return sts;
}

mfxStatus MVCDecoder::DecodeHeaderTarget(mfxBitstream *bs, mfxVideoParam *par)
{
    return InterfaceProxy<IYUVSource>::DecodeHeader(bs, par);
}

mfxStatus MVCDecoder::DecodeHeader(mfxBitstream *bs, mfxVideoParam *par)
{
    mfxStatus sts;

    MFX_CHECK_STS_SKIP(sts = DecodeHeaderTarget(bs, par)
        , MFX_ERR_MORE_DATA
        , MFX_ERR_NOT_ENOUGH_BUFFER);

    if (sts < MFX_ERR_NONE)
        return sts;

    if (NULL != par)
    {
        par->mfx.CodecProfile = m_vParam.mfx.CodecProfile;
    }

    //TODO: cannot use extbuffer helpers on precreated buffers consider to improve helpers by using inplace vectors
#if 0
    //checking attached buffers
    MFXExtBufferPtr<mfxExtMVCSeqDesc> seq(*par);
    if (!seq.get())
    {
        return MFX_ERR_NONE;
    }
    ///using of references reduces number of object created by -> operator
    MFXExtBufferPtrRef<mfxExtMVCSeqDesc>seq_in (*seq);
    MFXExtBufferPtrRef<mfxExtMVCSeqDesc>seq_own(*m_sequence);

    if (seq_in->View.capacity() < seq_own->View.size() || 
        seq_in->ViewId.capacity() < seq_own->ViewId.size() ||
        seq_in->OP.capacity() < seq_own->OP.size())
    {
        seq_in->NumOP = seq_own->OP.size();
        seq_in->NumView = seq_own->View.size();
        seq_in->NumViewId = seq_own->ViewId.size();

        return MFX_ERR_NOT_ENOUGH_BUFFER;
    }
    //coping of extended buffers here
    seq_in->View.insert(seq_in->View.end(), seq_own->View.begin(), seq_own->View.end());
    seq_in->ViewId.insert(seq_in->ViewId.end(), seq_own->ViewId.begin(), seq_own->ViewId.end());
    seq_in->OP.insert(seq_in->OP.end(), seq_own->OP.begin(), seq_own->OP.end());

    //TODO: the only limitation is that operationpoints are points to corresponding ViewId
    for (size_t i =0; i< seq_own->OP.size(); i++)
    {
        //TODO offset already invalid
        int offset = 0;//seq_own->OP[i].TargetViewId - &seq_own->ViewId.front();
        seq_in->OP[i].TargetViewId = seq.get()->ViewId + offset;
    }
#else
    if (NULL != par)
    {
        bool bFound = false;
        mfxU16 j;
        for(j = 0;j<par->NumExtParam; j++)
        {
            if (par->ExtParam[j]->BufferId == BufferIdOf<mfxExtMVCSeqDesc>::id)
            {
                bFound = true;
                break;
            }
        }

        if (bFound)
        {
            MFXExtBufferPtrRef<mfxExtMVCSeqDesc>seq_own(*m_sequence);
            mfxExtMVCSeqDesc * pExtSequence = reinterpret_cast<mfxExtMVCSeqDesc *>(par->ExtParam[j]);

            pExtSequence->NumOP = seq_own->OP.size();
            pExtSequence->NumView = seq_own->View.size();
            pExtSequence->NumViewId = seq_own->ViewId.size();

            //checking for available space
            if (pExtSequence->NumViewAlloc < seq_own->View.size() || 
                pExtSequence->NumViewIdAlloc < seq_own->ViewId.size() ||
                pExtSequence->NumOPAlloc < seq_own->OP.size())
            {
                return MFX_ERR_NOT_ENOUGH_BUFFER;
            }
     
            //coping of extended buffers here
            size_t i ;
            for (i = 0; i < seq_own->View.size(); i++)
            {
                pExtSequence->View[i] = seq_own->View[i];
            }
            for (i = 0; i < seq_own->ViewId.size(); i++)
            {
                pExtSequence->ViewId[i] = seq_own->ViewId[i];
            }
            //TODO: the only limitation is that operationpoints are points to corresponding ViewId
            for (i =0; i< seq_own->OP.size(); i++)
            {
                pExtSequence->OP[i] = seq_own->OP[i];
                //TODO offset already invalid
                int offset = 0;//seq_own->OP[i].TargetViewId - &seq_own->ViewId.front();
                pExtSequence->OP[i].TargetViewId = pExtSequence->ViewId + offset;
            }
        }
    }
#endif
    return sts;
}
