//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#ifndef __FFMPEG_READER_WRITER_H__
#define __FFMPEG_READER_WRITER_H__

#include <stdint.h>

#include "mfxdefs.h"
#include "mfxsmstructures.h"

int ffmpeg_read(void *p, uint8_t *data, int size);
int ffmpeg_write(void *p, uint8_t *data, int size);
int64_t ffmpeg_seek(void *p, int64_t offset, int whence);

#endif // __FFMPEG_READER_WRITER_H__
