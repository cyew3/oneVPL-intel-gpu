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

#include "ffmpeg_reader_writer.h"
#include "mfxsplmux.h"

int ffmpeg_read(void *p, uint8_t *data, int size)
{
    mfxDataIO* pRd = (mfxDataIO*)p;
    mfxBitstream bs;

    bs.Data = data;
    bs.DataLength = 0;
    bs.DataOffset = 0;
    bs.MaxLength = size;

    bs.DataLength = pRd->Read(pRd->pthis, &bs);

    return bs.DataLength;
}

int ffmpeg_write(void *p, uint8_t *data, int size)
{
    mfxDataIO* pRd = (mfxDataIO*)p;
    mfxBitstream bs;

    bs.Data = data;
    bs.DataLength = size;
    bs.DataOffset = 0;
    bs.MaxLength = size;

    pRd->Write(pRd->pthis, &bs);

    return bs.DataLength;
}

int64_t ffmpeg_seek(void *p, int64_t offset, int whence)
{
    mfxSeekOrigin origin;
    int64_t ret;
    mfxDataIO* pRd = (mfxDataIO*)p;
    switch(whence)
    {
    case 0:
        origin = MFX_SEEK_ORIGIN_BEGIN;
        break;
    case 1:
        origin = MFX_SEEK_ORIGIN_CURRENT;
        break;
    default:
        origin = MFX_SEEK_ORIGIN_END;
        break;
    }

    ret = pRd->Seek(pRd->pthis, offset, origin);

    return ret;
}
