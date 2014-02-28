//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#ifndef __FFMPEG_READER_WRITER_H__
#define __FFMPEG_READER_WRITER_H__

#include "mfxdefs.h"
#include "mfxsmstructures.h"

mfxI32 ffmpeg_read(void *p, mfxU8 *data, mfxI32 size);
mfxI32 ffmpeg_write(void *p, mfxU8 *data, mfxI32 size);
mfxI64 ffmpeg_seek(void *p, mfxI64 offset, mfxI32 whence);

#endif // __FFMPEG_READER_WRITER_H__
