/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#include "ffmpeg_reader_writer.h"
#include "mfxsplmux.h"

mfxI32 ffmpeg_read(void *p, mfxU8 *data, mfxI32 size)
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

mfxI32 ffmpeg_write(void *p, mfxU8 *data, mfxI32 size)
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

mfxI64 ffmpeg_seek(void *p, mfxI64 offset, mfxI32 whence)
{
    mfxSeekOrigin origin;
    mfxI64 ret;
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
