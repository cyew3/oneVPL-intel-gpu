/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#include "circular_splitter.h"
#include "sample_metadata.h"

CircularSplitterWrapper::CircularSplitterWrapper (std::auto_ptr<MFXSplitter>& splitter , std::auto_ptr<MFXDataIO>&io, mfxU64 nSeconds)
    : SplitterWrapper(splitter, io)
    , m_nTimeLimit(nSeconds)
    , m_bEOS(false)
{
    m_timer.Start();
}

bool CircularSplitterWrapper::GetSample(SamplePtr & sample) {
    mfxU32 OutputTrack = 0;
    mfxBitstream bs = {};

    if (m_EOF && (n_TracksInFile - 1 > n_NullIds)) {
        sample.reset(new MetaSample(META_EOS, 0, 0, ++n_NullIds));
        return true;
    }

    if (m_EOF)
        return false;

    // get current sample
    mfxStatus sts = m_splitter->GetBitstream(&OutputTrack, &bs);
    if (sts == MFX_ERR_MORE_DATA) {
        if (m_timer.GetTime() < m_nTimeLimit) {
            sample.reset(new MetaSample(META_EOS_RESET, 0, 0, 0));
            m_splitter->Seek(0);
        } else {
            m_EOF = true;
            sample.reset(new MetaSample(META_EOS, 0, 0, 0));
        }
        return true;
    }

    if (sts < MFX_ERR_NONE) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXSplitter::GetBitstream, sts = ")<<sts);
        throw SplitterGetBitstreamError();
    }

    sample.reset(new SplitterSample(*m_splitter.get(), OutputTrack, bs));
    return true;
}