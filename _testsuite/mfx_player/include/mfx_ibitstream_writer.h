/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

struct IBitstreamWriter
{
    virtual ~IBitstreamWriter(){}
    virtual mfxStatus Write(mfxBitstream*) = 0;
};
