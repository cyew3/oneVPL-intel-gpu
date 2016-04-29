/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

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