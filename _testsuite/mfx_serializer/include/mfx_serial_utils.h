/******************************************************************************* *\

Copyright (C) 2010-2015 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_serial_utils.h

*******************************************************************************/

#pragma once

#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>

#include "mfxcommon.h"
#include "mfxstructures.h"
#include "mfxjpeg.h"
#include "mfxvp8.h"
#include "mfxmvc.h"

#define MIN(a,b)   (((a) < (b)) ? (a) : (b))

//serialization flags used to choose union to serialize
namespace SerialFlags
{
    enum SerialisationFlags
    {
        //mfxVideoParam
        MFX         = 0,
        VPP         = 1,

        //mfxInfoMFX
        ENCODE      = 0,
        DECODE      = 2,
        JPEG_DECODE = 4,
        JPEG_ENCODE = 8,

        //mfxFrameId
        MVC         = 0,
        SVC         = 16,
    };
}

//union conversions
std::string GetMFXFourCCString(mfxU32 mfxFourcc);
mfxU32      GetMFXFourCCCode(std::string mfxFourcc);
std::string GetMFXCodecIdString(mfxU32 mfxCodec);
mfxU32      GetMFXCodecIdCode(std::string mfxCodec);
std::string GetMFXChromaFormatString(mfxU32 chromaFormat);
mfxU32      GetMFXChromaFormatCode(std::string chromaFormat);
std::string GetMFXStatusString(mfxStatus mfxSts);
mfxU32      GetMFXStatusCode(std::string mfxSts);
std::string GetMFXPicStructString(int PicStruct);
int         GetMFXPicStructCode(std::string PicStruct);
std::string GetMFXFrameTypeString(int FrameType);
int         GetMFXFrameTypeCode(std::string FrameType);
std::string GetMFXBufferIdString(mfxU32 mfxFourcc);
mfxU32      GetMFXBufferIdCode(std::string mfxFourcc);
std::string GetMFXIOPatternString (int IOPattern);
int         GetMFXIOPatternCode(std::string IOPattern);
std::string GetMFXRawDataString(mfxU8* pData, mfxU32 nData);
void        GetMFXRawDataValues(mfxU8* pData, std::string Data);
std::string GetMFXImplString(mfxIMPL impl);
mfxIMPL     GetMFXImplCode(std::string impl);

//time stamp conversions
mfxU64      ConvertmfxF642MFXTime(mfxF64 fTime);
mfxF64      ConvertMFXTime2mfxF64(mfxU64 nTime);

//file operations
bool SaveSerializedStructure(std::string file_name, std::string input);
std::string UploadSerialiedStructures(std::string file_name);
