//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
//*/

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