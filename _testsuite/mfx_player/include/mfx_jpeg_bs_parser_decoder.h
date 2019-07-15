/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_iyuv_source.h"
//#include "sample_utils.h"

//parses jpeg bitstream's headers to detect interlacing type
class JPEGBsParser
    : public InterfaceProxy<IYUVSource>
{
    typedef InterfaceProxy<IYUVSource> base;
    mfxFrameInfo m_splInfo;
public :
    JPEGBsParser(const mfxFrameInfo &splInfo, std::unique_ptr<IYUVSource> &&pTarget)
        : base(std::move(pTarget))
        , m_splInfo(splInfo)
    {
    }
    mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par)
    {
        mfxStatus sts = base::DecodeHeader(bs, par);

        if (sts >= MFX_ERR_NONE)
        {
            //we can check bitstream since it already enough 
            MFX_CHECK_STS(sts = MJPEG_AVI_ParsePicStruct(bs));

            //check resolution with container
            if ((bs->PicStruct == MFX_PICSTRUCT_PROGRESSIVE || bs->PicStruct == MFX_PICSTRUCT_UNKNOWN) &&
                 (m_splInfo.Height != 0 && m_splInfo.Height == 2 * par->mfx.FrameInfo.Height)) {
                bs->PicStruct = MFX_PICSTRUCT_FIELD_TFF;
            }

            //allign height appropriatetly
            if (bs->PicStruct != MFX_PICSTRUCT_PROGRESSIVE && bs->PicStruct != MFX_PICSTRUCT_UNKNOWN)
            {
                par->mfx.FrameInfo.Height = (par->mfx.FrameInfo.Height << 1);
                par->mfx.FrameInfo.CropH  = (par->mfx.FrameInfo.CropH << 1);

                par->mfx.FrameInfo.PicStruct = bs->PicStruct;
            }
            else 
            {
                par->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            }
        }

        return sts;
    }

protected:
    // returns false if buf length is insufficient, otherwise
    // skips step bytes in buf with specified length and returns true
    template <typename Buf_t, typename Length_t>
    static bool skip(const Buf_t *&buf, Length_t &length, Length_t step)
    {
        if (length < step)
            return false;

        buf    += step;
        length -= step;

        return true;
    }
    static mfxStatus MJPEG_AVI_ParsePicStruct(mfxBitstream *bitstream)
    {
        // check input for consistency
        if (!bitstream->Data || bitstream->DataLength <= 0)
            return MFX_ERR_MORE_DATA;

        // define JPEG markers
        const mfxU8 APP0_marker [] = { 0xFF, 0xE0 };
        const mfxU8 SOI_marker  [] = { 0xFF, 0xD8 };
        const mfxU8 AVI1        [] = { 'A', 'V', 'I', '1' };

        // size of length field in header
        const mfxU8 len_size  = 2;
        // size of picstruct field in header
        const mfxU8 picstruct_size = 1;

        mfxU32 length = bitstream->DataLength;
        const mfxU8 *ptr = reinterpret_cast<const mfxU8*>(bitstream->Data);

        //search for SOI marker
        while ((length >= sizeof(SOI_marker)) && memcmp(ptr, SOI_marker, sizeof(SOI_marker)))
        {
            skip(ptr, length, (mfxU32)1);
        }

        // skip SOI
        if (!skip(ptr, length, (mfxU32)sizeof(SOI_marker)) || length < sizeof(APP0_marker))
            return MFX_ERR_MORE_DATA;

        // if there is no APP0 marker return
        if (memcmp(ptr, APP0_marker, sizeof(APP0_marker)))
        {
            bitstream->PicStruct = MFX_PICSTRUCT_UNKNOWN;
            return MFX_ERR_NONE;
        }

        // skip APP0 & length value
        if (!skip(ptr, length, (mfxU32)sizeof(APP0_marker) + len_size) || length < sizeof(AVI1))
            return MFX_ERR_MORE_DATA;

        if (memcmp(ptr, AVI1, sizeof(AVI1)))
        {
            bitstream->PicStruct = MFX_PICSTRUCT_UNKNOWN;
            return MFX_ERR_NONE;
        }

        // skip 'AVI1'
        if (!skip(ptr, length, (mfxU32)sizeof(AVI1)) || length < picstruct_size)
            return MFX_ERR_MORE_DATA;

        // get PicStruct
        switch (*ptr)
        {
        case 0:
            bitstream->PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            break;
        case 1:
            bitstream->PicStruct = MFX_PICSTRUCT_FIELD_TFF;
            break;
        case 2:
            bitstream->PicStruct = MFX_PICSTRUCT_FIELD_BFF;
            break;
        default:
            bitstream->PicStruct = MFX_PICSTRUCT_UNKNOWN;
        }

        return MFX_ERR_NONE;
    }


};
