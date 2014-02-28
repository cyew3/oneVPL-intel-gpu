//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "sample_utils.h"
#include "mfxstructures.h"
#include "fileio.h"
#include "isample.h"
#include "isink.h"
#include "exceptions.h"

class DataSink  : public ISink
{
    std::auto_ptr<MFXDataIO> m_pfio;
public:
    DataSink(std::auto_ptr<MFXDataIO> & fio)
        : m_pfio(fio) {
    }
    virtual void PutSample (SamplePtr & sample) {
        if (sample->HasMetaData(META_EOS)) {
            return;
        }
        if (0 == m_pfio->Write(&sample->GetBitstream())) {
            MSDK_TRACE_ERROR(MSDK_STRING("Muxer put sample error"));
            throw MuxerPutSampleError();
        }
    }
};