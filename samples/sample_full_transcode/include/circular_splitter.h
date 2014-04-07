/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#pragma once

#include <memory>
#include "isample.h"
#include "splitter_wrapper.h"
#include "mfxsplmux++.h"
#include "fileio.h"
#include "exceptions.h"

class CircularSplitterWrapper : private SplitterWrapper {
public:
    CircularSplitterWrapper( std::auto_ptr<MFXSplitter>& splitter , std::auto_ptr<MFXDataIO>&io, mfxU64 limit);
    virtual bool GetSample(SamplePtr & sample);
protected:
    mfxU64 m_nCycleCounter;
    mfxU64 m_nCycleLimit;
};
