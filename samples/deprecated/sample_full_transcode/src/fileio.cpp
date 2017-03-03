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

#include "fileio.h"
#include <fstream>
#include <iostream>
#include "sample_defs.h"
#include "exceptions.h"

#if defined(_WIN32) || defined(_WIN64)
#define fseek _fseeki64
#define ftell _ftelli64
#endif

FileIO::FileIO(const msdk_string& file, const msdk_string params) : f(0)
{
    if (file.compare(MSDK_STRING("-"))) {
#if defined(_WIN32) || defined(_WIN64)
        _tfopen_s(&f, file.c_str(), params.c_str());
#else
        f = fopen(file.c_str(), params.c_str());
#endif
        if (!f) {
            MSDK_TRACE_ERROR(MSDK_STRING("cannot open file: ") << file);
            throw FileOpenError();
        }
    }
}

FileIO::~FileIO()
{
    if (f) {
        fclose(f);
        f = NULL;
    }
}

mfxI32 FileIO::Read(mfxBitstream *outBitStream)
{
    mfxI32 nBytesRead = 0;
    nBytesRead = (mfxI32)fread(outBitStream->Data + outBitStream->DataOffset + outBitStream->DataLength, sizeof(mfxU8),
        outBitStream->MaxLength - outBitStream->DataOffset, !f ? stdin : f);
    outBitStream->DataLength = nBytesRead;
    return nBytesRead;
}

mfxI32 FileIO::Write(mfxBitstream *outBitStream)
{
    mfxI32 nBytesWritten = 0;
    if (outBitStream->DataLength + outBitStream->DataOffset  > outBitStream->MaxLength)
        outBitStream->DataLength = outBitStream->MaxLength - outBitStream->DataOffset;

    nBytesWritten = (mfxI32)fwrite(outBitStream->Data, sizeof(mfxU8),
                        outBitStream->DataOffset + outBitStream->DataLength, !f ? stdout : f);

    return nBytesWritten;
}

mfxI64 FileIO::Seek(mfxI64 offset, mfxSeekOrigin origin)
{
    if (!f)
        return -1;

    mfxI64 res = -1;

    switch(origin)
    {
        case MFX_SEEK_ORIGIN_BEGIN:
            res = fseek(f, offset, SEEK_SET);
            break;
        case MFX_SEEK_ORIGIN_CURRENT:
            res = fseek(f, offset, SEEK_CUR);
            break;
        case MFX_SEEK_ORIGIN_END: {
            mfxI64 filePos = 0, fileSize = 0;
            if (!offset) {
                filePos = ftell(f);
            }
            res = fseek(f, offset, SEEK_END);
            if (!offset && !res) {
                fileSize = ftell(f);
                res = fseek(f, filePos, SEEK_SET);
                if (!res)
                    return fileSize;
            }
            break;
        }
    }
    if (!res)
        res = ftell(f);
    return res;
}
