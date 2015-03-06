/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

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
