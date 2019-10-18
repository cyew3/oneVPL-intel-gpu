/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019 Intel Corporation. All Rights Reserved.

File Name: mfx_preload_decoder.cpp

\* ****************************************************************************** */

#if (MFX_VERSION >= MFX_VERSION_NEXT)

#include "mfx_pipeline_defs.h"
#include "mfx_anchors_decoder.h"
#include "mfx_bitstream_reader.h"
#include "mfx_exact_frame_bs_reader.h"

#include <vector>

MFXAV1AnchorsDecoder::MFXAV1AnchorsDecoder(
        IVideoSession* session,
        std::unique_ptr<IYUVSource> &&pTarget,
        mfxVideoParam &frameParam,
        IMFXPipelineFactory * pFactory,
        const vm_char *  anchorsFileInput,
        mfxU32 anchorFramesNum)
        : InterfaceProxy<IYUVSource>(std::move(pTarget))
        , m_anchors_source(MFX_LST_ANCHOR_FRAMES_FROM_MFX_SURFACES)
        , m_PreloadedFramesNum(anchorFramesNum)
        , m_PreloadedFramesDecoded(0)
        , bCompleteFrame(true)
        , m_nMaxAsync(2)
{
    m_session = session->GetMFXSession();
    if (anchorsFileInput[0])
    {
        vm_string_strcpy_s(m_srcFile, MFX_ARRAY_SIZE(m_srcFile), anchorsFileInput);
    }
    m_pAnchorsYUVSource.reset(new MFXYUVDecoder(session, frameParam, 30.0, frameParam.mfx.FrameInfo.FourCC, pFactory, nullptr));
}

MFXAV1AnchorsDecoder::MFXAV1AnchorsDecoder(
    IVideoSession* session,
    std::unique_ptr<IYUVSource> &&pTarget,
    mfxVideoParam &frameParam,
    IMFXPipelineFactory * pFactory,
    mfxU32 anchorFramesNum)
    : InterfaceProxy<IYUVSource>(std::move(pTarget))
    , m_anchors_source(MFX_LST_ANCHOR_FRAMES_FIRST_NUM_FROM_MAIN_STREAM)
    , m_PreloadedFramesNum(anchorFramesNum)
    , m_PreloadedFramesDecoded(0)
    , bCompleteFrame(true)
    , m_nMaxAsync(2)
{
    m_session = session->GetMFXSession();
}

mfxStatus MFXAV1AnchorsDecoder::Init(mfxVideoParam *par)
{
    MFX_CHECK_POINTER(par);

    mfxStatus sts = MFX_ERR_NONE;

    if (m_anchors_source == MFX_LST_ANCHOR_FRAMES_FROM_MFX_SURFACES)
    {
        // init YUV decoder
        sts = m_pAnchorsYUVSource->Init(par);
        MFX_CHECK_STS(sts);

        sts = PreloadFrames(*par);
        MFX_CHECK_STS(sts);
    }

    // init AV1 decoder
    sts = m_pTarget->Init(par);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus MFXAV1AnchorsDecoder::DecodeFrameAsync(
    mfxBitstream2 &bs,
    mfxFrameSurface1 *surface_work,
    mfxFrameSurface1 **surface_out,
    mfxSyncPoint *syncp)
{
    if (m_anchors_source == MFX_LST_ANCHOR_FRAMES_FROM_MFX_SURFACES && m_PreloadedFramesDecoded < m_PreloadedFramesNum)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    return m_pTarget->DecodeFrameAsync(bs, surface_work, surface_out, syncp);
}

mfxStatus MFXAV1AnchorsDecoder::Close()
{
    if (m_anchors_source == MFX_LST_ANCHOR_FRAMES_FROM_MFX_SURFACES)
    {
        m_pAnchorsYUVSource->Close();
    }
    return m_pTarget->Close();
}

mfxStatus MFXAV1AnchorsDecoder::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    mfxStatus sts = m_pTarget->QueryIOSurf(par, request);
    if (sts >= MFX_ERR_NONE && request)
    {
        request->NumFrameMin = request->NumFrameMin + (mfxU16)m_PreloadedFramesNum;
        request->NumFrameSuggested = request->NumFrameSuggested + (mfxU16)m_PreloadedFramesNum;
    }
    return sts;
}

mfxStatus MFXAV1AnchorsDecoder::PreloadFrames(mfxVideoParam &par)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFXExtBufferVector extParam(par.ExtParam, par.NumExtParam);
    MFXExtBufferPtr<mfxExtAV1LargeScaleTileParam> extLst(extParam);
    if (NULL == extLst.get())
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (!extLst->Anchors)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if (extLst->AnchorFramesNum != m_PreloadedFramesNum)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    m_anchorSurfaces.assign(extLst->Anchors, extLst->Anchors + extLst->AnchorFramesNum);

    mfxU64 bs_size = par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height * 4;
    sts = m_bitstreamBuf.Init(0, bs_size);
    MFX_CHECK_STS(sts);

    sts = CreateSplitter(par.mfx.FrameInfo);
    MFX_CHECK_STS(sts);

    mfxStatus exit_sts = MFX_ERR_NONE;
    bool      bEOS = false;

    while (!bEOS && m_PreloadedFramesDecoded < m_PreloadedFramesNum && sts != PIPELINE_ERR_STOPPED)
    {
        // Read next frame
        if (sts == MFX_ERR_MORE_DATA || m_bitstreamBuf.DataLength <= 4)
        {
            if (MFX_ERR_NONE != (sts = m_pSpl->ReadNextFrame(m_bitstreamBuf)))
            {
                m_bitstreamBuf.PutBuffer(true);
                bEOS = true;
            }
            else
            {
                m_bitstreamBuf.isNull = true;
                for (; sts != PIPELINE_ERR_STOPPED;)
                {
                    sts = RunDecode(m_bitstreamBuf);
                    if (MFX_ERR_NONE != sts)
                        break;
                }
                m_bitstreamBuf.isNull = false;
                continue;
            }
        }

        sts = MFX_ERR_NONE;

        MFX_CHECK_STS(m_bitstreamBuf.PutBuffer());

        while (MFX_ERR_NONE == sts)
        {
            mfxBitstream2 inputBs;

            MFX_CHECK_STS_SKIP(sts = m_bitstreamBuf.LockOutput(&inputBs), MFX_ERR_MORE_DATA);

            if (MFX_ERR_MORE_DATA == sts && m_pSpl)//still don't have enough data in buffer
            {
                break;
            }

            mfxU32 nInputSize = inputBs.DataLength;

            sts = RunDecode(inputBs);

            MFX_CHECK_STS(m_bitstreamBuf.UnLockOutput(&inputBs));

            //extraordinary exit
            if (sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
            {
                exit_sts = sts;
                bEOS = true;

                //flushing data back to inBSFrame
                sts = m_bitstreamBuf.UndoInputBS();
                MFX_CHECK_STS_SKIP(sts, MFX_ERR_MORE_DATA);
                break;
            }

            if (PIPELINE_ERR_STOPPED == sts)
                break;

            sts = (sts != MFX_WRN_VIDEO_PARAM_CHANGED) ? sts : MFX_ERR_NONE;
            MFX_CHECK_STS_SKIP(sts, MFX_ERR_MORE_DATA);

            //if decoder took something, we may have some more data in m_bitstreambuffer
            //in completeframe mode we need to read new frame always and not to rely onremained data
            if (MFX_ERR_MORE_DATA == sts)
            {
                if (nInputSize != inputBs.DataLength)
                {
                    sts = MFX_ERR_NONE;
                }
                else
                {
                    //if decoder didn't take any data we need to extend buffer if possible
                    mfxU32 nSize;
                    m_bitstreamBuf.GetMinBuffSize(nSize);
                    if (1 != nSize && inputBs.MaxLength != inputBs.DataLength)
                    {
                        //bitstream is limited by cmd line option and we have more data in buffer
                        MFX_CHECK_STS(m_bitstreamBuf.SetMinBuffSize(nSize + 1));
                        sts = MFX_ERR_NONE;
                    }
                }
            }
        } // while constructing frames and decoding
    }

    // to get the last(buffered) decoded frame
    m_bitstreamBuf.isNull = true;
    for (; sts != PIPELINE_ERR_STOPPED;)
    {
        sts = RunDecode(m_bitstreamBuf);
        if (MFX_ERR_NONE != sts)
            break;
    }
    m_bitstreamBuf.isNull = false;

    MFX_CHECK_STS_SKIP(sts, MFX_ERR_MORE_DATA, PIPELINE_ERR_STOPPED);

    return MFX_ERR_NONE;
}

mfxStatus MFXAV1AnchorsDecoder::FindFreeSurface(mfxFrameSurface1 **sfc)
{
    if (m_PreloadedFramesDecoded >= m_PreloadedFramesNum)
        return MFX_ERR_MORE_SURFACE;

    *sfc = m_anchorSurfaces[m_PreloadedFramesDecoded++];

    return MFX_ERR_NONE;
}

mfxStatus MFXAV1AnchorsDecoder::RunDecode(mfxBitstream2 & bs)
{
    mfxFrameSurface1 *pSurface = nullptr;
    mfxFrameSurface1 *pDecodedSurface = nullptr;
    mfxSyncPoint     syncp = nullptr;
    mfxStatus        sts = MFX_ERR_MORE_SURFACE;
    Timeout<MFX_DEC_DEFAULT_TIMEOUT>       dec_timeout;

    for (; sts == MFX_ERR_MORE_SURFACE || sts == MFX_WRN_DEVICE_BUSY || sts == (mfxStatus)MFX_ERR_REALLOC_SURFACE;)
    {
        if (sts == MFX_WRN_DEVICE_BUSY)
        {
            if (m_SyncPoints.empty())
            {
                MFX_CHECK_WITH_ERR(dec_timeout.wait(), MFX_WRN_DEVICE_BUSY);
            }
            else
            {
                MFX_CHECK_STS(m_pAnchorsYUVSource->SyncOperation(m_SyncPoints.begin()->first, TimeoutVal<>::val()));
            }
        }
        MFX_CHECK_STS(FindFreeSurface(&pSurface));

        bs.DataFlag = (mfxU16)(bCompleteFrame ? MFX_BITSTREAM_COMPLETE_FRAME : 0);

        if (m_pSpl == NULL)
            bs.isNull = true;

        sts = m_pAnchorsYUVSource->DecodeFrameAsync(bs, pSurface, &pDecodedSurface, &syncp);
    }


    if (sts != MFX_ERR_NONE)
    {
        if (!bs.isNull || m_SyncPoints.empty())
        {
            if (MFX_ERR_NONE != sts && MFX_ERR_MORE_DATA != sts)
            {
                MFX_CHECK_STS_TRACE_EXPR(sts, m_pAnchorsYUVSource->DecodeFrameAsync(bs, pSurface, &pDecodedSurface, &syncp));
            }
            return sts;
        }
    }

    if (m_nMaxAsync != 0)
    {
        if (NULL != pDecodedSurface)
        {
            IncreaseReference(&pDecodedSurface->Data);
            m_SyncPoints.push_back(MFXAV1AnchorsDecoder::SyncPair(syncp, pDecodedSurface));
        }

        for (; !m_SyncPoints.empty();)
        {
            if (m_SyncPoints.size() == m_nMaxAsync || bs.isNull)
            {
                sts = m_pAnchorsYUVSource->SyncOperation(m_SyncPoints.begin()->first, TimeoutVal<>::val());
                MFX_CHECK_STS(sts);
            }

            //only checking for completed task
            for (; !m_SyncPoints.empty() && MFX_ERR_NONE == m_pAnchorsYUVSource->SyncOperation(m_SyncPoints.begin()->first, 0); )
            {
                pDecodedSurface = m_SyncPoints.begin()->second.pSurface;

                //taking out of queue this surface since it is already synchronized
                m_SyncPoints.pop_front();
                if (NULL != pDecodedSurface)
                {
                    //check exiting status
                    if (MFX_ERR_NONE != (sts = CheckExitingCondition()))
                    {
                        return sts;
                    }
                }
            }

            if (!bs.isNull)
            {
                break;
            }
        }
    }

    return CheckExitingCondition();
}

mfxStatus MFXAV1AnchorsDecoder::CheckExitingCondition()
{
    if (m_PreloadedFramesDecoded >= m_PreloadedFramesNum)
    {
        return PIPELINE_ERR_STOPPED;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXAV1AnchorsDecoder::CreateSplitter(mfxFrameInfo &frameInfo)
{
    sStreamInfo sInfo;

    sInfo.videoType = frameInfo.FourCC;// MFX_FOURCC_NV12;
    sInfo.nWidth = frameInfo.Width;
    sInfo.nHeight = frameInfo.Height;
    sInfo.isDefaultFC = true;
    sInfo.corrupted = 0;

    std::auto_ptr<IBitstreamReader> pSpl;

    pSpl.reset(new BitstreamReader(&sInfo));
    MFX_CHECK_POINTER(pSpl.get());

    pSpl.reset(new ExactLenghtBsReader(GetMinPlaneSize(frameInfo), pSpl.release()));
    MFX_CHECK_POINTER(pSpl.get());

    m_pSpl = pSpl.release();

    mfxStatus sts = m_pSpl->Init(m_srcFile);
    MFX_CHECK_STS(sts);

    // don't request filesize if it's a directory wildcard
    PrintInfo(VM_STRING("Anchors src fileName"), VM_STRING("%s"), m_srcFile);
    PrintInfo(VM_STRING("Anchors src container"), VM_STRING("RAW"));
    PrintInfo(VM_STRING("Anchors src video"), VM_STRING("%s"), GetMFXFourccString(frameInfo.FourCC).c_str());
    PrintInfo(VM_STRING("Anchors src resolution"), VM_STRING("%dx%d"), frameInfo.Width, frameInfo.Height);

    return MFX_ERR_NONE;
}

#endif // (MFX_VERSION >= MFX_VERSION_NEXT)
