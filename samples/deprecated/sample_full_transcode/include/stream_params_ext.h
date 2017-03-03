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


#pragma once
#include <map>
#include <vector>
#include "mfxsplmux++.h"

//differs from base class that this one has tracks mapping information
class MFXStreamParamsExt  {
    //TODO: add tests for m_trackMapping
    std::map<int, mfxU32>      m_tracksMapping;
    std::vector<mfxTrackInfo*> m_TrackPointers;
    std::vector<mfxTrackInfo>  m_Tracks;
    MFXStreamParams            m_streamParams;
public:
    std::map<int, mfxU32>& TrackMapping() {
        return m_tracksMapping;
    }
    std::vector<mfxTrackInfo> & Tracks() {
        return m_Tracks;
    }
    mfxSystemStreamType & SystemType() {
        return m_streamParams.SystemType;
    }
    MFXStreamParams GetMFXStreamParams() {
        //setting up pointer to pointer layout
        for(size_t i =0; i < m_Tracks.size(); i++) {
            m_TrackPointers.push_back(&m_Tracks[i]);
        }

        m_streamParams.NumTracks = (mfxU16)(m_TrackPointers.size());
        m_streamParams.NumTracksAllocated = m_streamParams.NumTracks;
        m_streamParams.TrackInfo = m_TrackPointers.empty() ? NULL : &m_TrackPointers[0];

        return m_streamParams;
    }
};

