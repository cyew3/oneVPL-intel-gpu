/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#include "muxer_wrapper.h"

bool detail::SamplePoolForMuxer::GetSample( SamplePtr& sample )
{
    if (m_samples.empty())
        return false;
    sample.reset(m_samples.front());
    m_samples.pop_front();
    return true;
}

void detail::SamplePoolForMuxer::PutSample( SamplePtr& sample )
{
    if (m_muxVideoParam.empty()) {
        sample->GetMetaData(META_VIDEOPARAM, m_muxVideoParam);
    }
    if (!m_SpsPps.size()) {
        sample->GetMetaData(META_SPSPPS, m_SpsPps);
    }
    m_samples.push_back(new SampleBitstream(&sample->GetBitstream(), sample->GetTrackID()));
}

bool detail::SamplePoolForMuxer::GetVideoInfo( mfxTrackInfo & track_info)
{
    if (m_muxVideoParam.empty() || !m_SpsPps.size())
        return false;
    mfxVideoParam *vParam = reinterpret_cast<mfxVideoParam*>(&m_muxVideoParam.front());

    //need to get actual frame resolution from encoder
    track_info.VideoParam.FrameInfo = vParam->mfx.FrameInfo;

    //need to get actual seq headers
    if (m_SpsPps.size() > MFX_TRACK_HEADER_MAX_SIZE) {
        MSDK_TRACE_ERROR(MSDK_STRING("Muxer header buffer overflow "));
        throw MuxerHeaderBufferOverflow();
    }
    MSDK_MEMCPY(&track_info.Header, &m_SpsPps.front(), m_SpsPps.size());
    track_info.HeaderLength = (mfxU16)m_SpsPps.size();
    return true;
}

void MuxerWrapper::PutSample( SamplePtr & sample )
{
    if (sample->HasMetaData(META_EOS))
        return;
    if (!m_bInited) {
        //get video info for each video tracks - currently limited to just one
        for (mfxU16 i = 0; i < m_sParams.NumTracks; i++) {
            if (!(m_sParams.TrackInfo[i]->Type & MFX_TRACK_ANY_VIDEO)) {
                continue;
            }
            if (!m_SamplePool.GetVideoInfo(*m_sParams.TrackInfo[i])) {
                m_SamplePool.PutSample(sample);
                return;
            }
        }
        mfxStatus sts = m_muxer->Init(m_sParams, *m_io.get());
        if (sts < 0) {
            MSDK_TRACE_ERROR(MSDK_STRING("Muxer init error: ") << sts);
            throw MuxerInitError();
        }
        SamplePtr DelayedSample;
        while (m_SamplePool.GetSample(DelayedSample)) {
            mfxStatus sts = m_muxer->PutBitstream(m_sParamsExt.TrackMapping()[DelayedSample->GetTrackID()], &DelayedSample->GetBitstream(), 0);

            if (sts < MFX_ERR_NONE) {
                MSDK_TRACE_ERROR(MSDK_CHAR("Muxer put bitstream error: ") << sts <<MSDK_CHAR(", track_id=") << DelayedSample->GetTrackID());
                throw MuxerPutSampleError();
            }
        }
        m_bInited = true;
    }

    mfxStatus sts = m_muxer->PutBitstream(m_sParamsExt.TrackMapping()[sample->GetTrackID()], &sample->GetBitstream(), 0);

    if (sts < MFX_ERR_NONE) {
        MSDK_TRACE_ERROR(MSDK_CHAR("Muxer put bitstream error: ") << sts <<MSDK_CHAR(", track_id=") << sample->GetTrackID());
        throw MuxerPutSampleError();
    }
}