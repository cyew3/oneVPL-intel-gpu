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

#include "splitter_wrapper.h"
#include "exceptions.h"
#include "sample_utils.h"
#include <iostream>
#include "sample_defs.h"
#include "sample_metadata.h"

void SplitterWrapper::InitSplitter() {
    mfxStatus sts = m_splitter->Init(*m_io.get());
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXSplitter::Init, sts = ")<<sts);
        throw SplitterInitError();
    }
}

bool SplitterWrapper::GetSample( SamplePtr & sample ) {
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
        m_EOF = true;
        sample.reset(new MetaSample(META_EOS, 0, 0, 0));
        return true;
    }

    if (sts < MFX_ERR_NONE) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXSplitter::GetBitstream, sts = ")<<sts);
        throw SplitterGetBitstreamError();
    }

    sample.reset(new SplitterSample(*m_splitter.get(), OutputTrack, bs));
    return true;
}
void SplitterWrapper::GetInfo( MFXStreamParams & info) {
    mfxStatus sts = MFX_ERR_NONE;
    sts = m_splitter->GetInfo(info);

    if (sts < MFX_ERR_NONE) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXSplitter::GetInfo, sts=")<<sts);
        throw SplitterGetInfoError();
    }
}