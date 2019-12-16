/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: mfxstructures.h

*******************************************************************************/

#pragma once

#include <stdlib.h>
#include <string.h>
#include <vector>
#include "mfxpcp.h" // mfxEncryptedData

#define mfxBitstream2_ZERO_MEM(bs2) {memset(&(bs2), 0, (size_t)(((mfxU8*)&((bs2).m_enryptedData)) - ((mfxU8*)&(bs2)))); (bs2).m_enryptedData.clear(); (bs2).m_enryptedDataBuffer.clear();}

//extension to mediasdk bitstream, that silently can be casted
struct mfxBitstream2 : mfxBitstream
{
    mfxU16 DependencyId;//splitter might want to put source information, analog to FrameId in surface
    bool   isNull; // whether to use null pointer instead of this object

    std::vector<mfxEncryptedData> m_enryptedData;
    std::vector<mfxU8> m_enryptedDataBuffer;

    mfxU32  InputBsLength;  // length of input bitstream
    mfxU32  ReadLength;     // to indicate the bitstream bytes that have been read

   //since it is not a POD, value will be default initialized, we have to create a ctor to use zero initializing
    mfxBitstream2()
    {
        mfxBitstream2_ZERO_MEM(*this);
    }

};

