/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_bitstream_decoder.h"

BitstreamDecoder::BitstreamDecoder(DecodeContext * pCtx)
    : GenericStateContext<mfxBitstream2 &>(new DecodeState_DecodeHeader())
    , InterfaceProxy<IYUVSource>(pCtx->pSource)
    , m_Ctx(*pCtx)
    , m_surfaces(pCtx->pTime)
    , m_bsRemained()
    , m_Alloc(pCtx->pAlloc)
{
}

BitstreamDecoder::~BitstreamDecoder()
{
    // donot calling to second close inside parent destructor
    m_pTarget.reset(NULL);

    MFXClose(m_Ctx.session);
}

mfxStatus BitstreamDecoder::Write(mfxBitstream * pBs)
{
    mfxBitstream2 bs;
    bs.isNull = NULL == pBs;

    if ( NULL != pBs)
    {
        memcpy(&bs, pBs, sizeof(mfxBitstream));
    }

    mfxStatus sts = Process(bs);

    //skip any warnings and more data since IBitstreamWriter cannot report such codes
    if (sts > 0 || MFX_ERR_MORE_DATA == sts)
        sts = MFX_ERR_NONE;

    return sts;
}



mfxStatus BitstreamDecoder::DecodeHeader(mfxBitstream *bs, mfxVideoParam * /*par*/)
{
    mfxStatus sts;

    MFX_CHECK_STS_SKIP( sts = base::DecodeHeader(bs, &m_Ctx.sInitialParams), MFX_ERR_MORE_DATA);

    return sts;
}

mfxStatus BitstreamDecoder::Init(mfxVideoParam * /*par*/)
{
    //checking context
    MFX_CHECK_POINTER(m_Ctx.pAlloc);

    //initializing surfaces
    mfxFrameAllocRequest request;
    MFX_CHECK_STS(base::QueryIOSurf(&m_Ctx.sInitialParams, &request));

    request.Type |= m_Ctx.request.Type;

    mfxFrameAllocResponse responce;
    MFX_CHECK_STS(m_Ctx.pAlloc->Alloc(m_Ctx.pAlloc->pthis, &request, &responce));

    MFX_CHECK_STS(m_surfaces.Create(responce, m_Ctx.sInitialParams.mfx.FrameInfo));

    MFX_CHECK_STS(base::Init(&m_Ctx.sInitialParams));

    return MFX_ERR_NONE;    
}

mfxStatus BitstreamDecoder::DecodeFrameAsync( mfxBitstream2 &bs
                                            , mfxFrameSurface1 * /*surface_work*/
                                            , mfxFrameSurface1 ** /*surface_out*/
                                            , mfxSyncPoint * /*syncp*/)
{
    mfxBitstream2 inputBs;
    if (bs.isNull || NULL == bs.Data)
        return DecodeFrameAsyncInternal(bs, NULL, NULL, NULL);

    if (0 != m_bsRemained.DataLength)
    {
        //construct new bs;
        if (MFX_ERR_NOT_ENOUGH_BUFFER == BSUtil::MoveNBytesStrict(&m_bsRemained, &bs, bs.DataLength))
        {
            m_bsData.resize(mfx_align(bs.DataLength + m_bsRemained.DataLength, 100*1024));
            m_bsRemained.Data = &m_bsData.front();
            m_bsRemained.MaxLength = m_bsData.size();
            BSUtil::MoveNBytesStrict(&m_bsRemained, &bs, bs.DataLength);
        }
        m_bsRemained.TimeStamp = bs.TimeStamp;
        m_bsRemained.FrameType = bs.FrameType;
        bs = m_bsRemained;
    }
    inputBs = bs;
    mfxStatus sts ;
    for(;;)
    {
        sts = DecodeFrameAsyncInternal(bs, NULL, NULL, NULL);
        
        if (bs.DataLength != inputBs.DataLength && bs.DataLength != 0 )
        {
            continue;
        }
        break;
    }

        //copy remained bs
    if (!bs.isNull)
    {
        m_bsRemained.DataLength = 0;
        if (MFX_ERR_NOT_ENOUGH_BUFFER == BSUtil::MoveNBytesStrict(&m_bsRemained, &bs, bs.DataLength))
        {
            m_bsData.resize(mfx_align(bs.DataLength + m_bsRemained.DataLength, 100*1024));
            m_bsRemained.Data = &m_bsData.front();
            m_bsRemained.MaxLength = m_bsData.size();
            sts = BSUtil::MoveNBytesStrict(&m_bsRemained, &bs, bs.DataLength);
        }
    }


    return sts;
}


mfxStatus BitstreamDecoder::DecodeFrameAsyncInternal( mfxBitstream2 &bs
                                            , mfxFrameSurface1 * /*surface_work*/
                                            , mfxFrameSurface1 ** /*surface_out*/
                                            , mfxSyncPoint * /*syncp*/)

{
    SrfEncCtl         surface;
    Timeout<5>     dec_timeout;
    mfxFrameSurface1 *pDecodedSurface = NULL;
    mfxSyncPoint      syncp = NULL;
    mfxStatus         sts = MFX_ERR_MORE_SURFACE;

    for (; sts == MFX_ERR_MORE_SURFACE || sts == MFX_WRN_DEVICE_BUSY; )
    {
        if (sts == MFX_WRN_DEVICE_BUSY)
        {
            MFX_CHECK_WITH_ERR(dec_timeout.wait(), MFX_WRN_DEVICE_BUSY);
        }

        MFX_CHECK_STS(m_surfaces.FindFree(surface, TimeoutVal<>::val()));

        if (m_Ctx.bCompleteFrame && !bs.isNull)
        {
            bs.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
        }


        sts = base::DecodeFrameAsync(bs, surface.pSurface, &pDecodedSurface, &syncp);
    }

    if (MFX_ERR_NONE == sts && NULL != pDecodedSurface)
    {
        MFX_CHECK_STS(base::SyncOperation(syncp, TimeoutVal<>::val()));
        PipelineTraceDecFrame(2);
    }

    return sts;
}

//////////////////////////////////////////////////////////////////////////

mfxStatus DecodeState_DecodeHeader::HandleParticular(BitstreamDecoder *pSource, mfxBitstream2 & bs)
{
    mfxStatus sts =  pSource->DecodeHeader(&bs, NULL);
    if(MFX_ERR_NONE == sts)
    {
        MFX_CHECK_STS(pSource->SetState(new DecodeState_Init()));
        sts = PIPELINE_WRN_SWITCH_STATE;
    }
    return sts;
}

mfxStatus DecodeState_Init::HandleParticular(BitstreamDecoder *pSource, mfxBitstream2 & /*pData*/)
{
    mfxStatus sts =  pSource->Init(NULL);
    if(MFX_ERR_NONE == sts)
    {
        MFX_CHECK_STS(pSource->SetState(new DecodeState_DecodeFrameAsync()));
        sts = PIPELINE_WRN_SWITCH_STATE;
    }
    return sts;
}

mfxStatus DecodeState_DecodeFrameAsync::HandleParticular(BitstreamDecoder *pSource, mfxBitstream2 & bs)
{
    //end of stream case
    if (bs.isNull || (NULL ==bs.Data && 0 == bs.DataLength))
    {
        mfxStatus sts;
        do
        {
            sts = pSource->DecodeFrameAsync(bs, NULL, NULL, NULL);

            if (sts > 0 && sts != MFX_WRN_IN_EXECUTION)
                sts = MFX_ERR_NONE;
            
        }while(MFX_ERR_NONE == sts);

        return sts;
    }
    return pSource->DecodeFrameAsync(bs, NULL, NULL, NULL);
}
