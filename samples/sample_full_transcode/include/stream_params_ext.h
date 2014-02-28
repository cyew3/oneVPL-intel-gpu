//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//


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

