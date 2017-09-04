//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW)

#include <stdexcept>
#include <string>

#include "mfx_vp9_dec_decode_utils.h"

#include "umc_vp9_bitstream.h"
#include "umc_vp9_frame.h"

namespace MfxVP9Decode
{
    void ParseSuperFrameIndex(const mfxU8 *data, size_t data_sz,
                              mfxU32 sizes[8], mfxU32 *count)
    {
        assert(data_sz);
        mfxU8 marker = ReadMarker(data + data_sz - 1);
        *count = 0;

        if ((marker & 0xe0) == 0xc0)
        {
            const mfxU32 frames = (marker & 0x7) + 1;
            const mfxU32 mag = ((marker >> 3) & 0x3) + 1;
            const size_t index_sz = 2 + mag * frames;

            mfxU8 marker2 = ReadMarker(data + data_sz - index_sz);

            if (data_sz >= index_sz && marker2 == marker)
            {
                // found a valid superframe index
                const mfxU8 *x = &data[data_sz - index_sz + 1];

                for (mfxU32 i = 0; i < frames; i++)
                {
                    mfxU32 this_sz = 0;

                    for (mfxU32 j = 0; j < mag; j++)
                        this_sz |= (*x++) << (j * 8);
                    sizes[i] = this_sz;
                }

                *count = frames;
            }
        }
    }

}; // namespace MfxVP9Decode

#endif // _MFX_VP9_DECODE_UTILS_H_
