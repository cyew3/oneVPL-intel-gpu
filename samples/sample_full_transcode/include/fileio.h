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
#include "mfxsplmux++.h"

class FileIO : public MFXDataIO 
{
    FILE* f;
public:
    FileIO(const msdk_string& file, msdk_string params);
    virtual ~FileIO();
    virtual mfxI32 Read (mfxBitstream *outBitStream);
    virtual mfxI32 Write (mfxBitstream *outBitStream);
    virtual mfxI64 Seek (mfxI64 offset, mfxSeekOrigin origin);
};