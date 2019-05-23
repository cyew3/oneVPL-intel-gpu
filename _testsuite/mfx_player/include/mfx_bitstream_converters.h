/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2019 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include <map>
#include <algorithm>
#include "mfx_ibitstream_converter.h"
#include "shared_utils.h"
#include "fast_copy.h"
#include "ippcc.h"

#define DECL_CONVERTER(inFourCC, outFourCC) BSConvert_##inFourCC##_to_##outFourCC

class BitstreamConverterFactory
    : public IBitstreamConverterFactory
{
    using Container_t = std::map <std::pair<mfxU32, mfxU32>, std::unique_ptr<IBitstreamConverter> >;
    Container_t m_converters;

public:
    void Register(std::unique_ptr<IBitstreamConverter> p)
    {
        auto transform_type = p->GetFourCCPair();

        auto it = m_converters.find(transform_type);
        if (it != m_converters.end())
        {
            PipelineTrace((VM_STRING("WARNING: converter from %s to %s already exist")
                , GetMFXFourccString(transform_type.first).c_str()
                , GetMFXFourccString(transform_type.second).c_str()));
            it->second = std::move(p);
        }
        else
        {
            m_converters[transform_type] = std::move(p);
        }
    }
    IBitstreamConverter* MakeConverter(mfxU32 inFourcc, mfxU32 outFourcc)
    {
        Container_t::iterator i = m_converters.find(std::make_pair(inFourcc, outFourcc));
        if (i == m_converters.end())
        {
            return nullptr;
        }

        return i->second->Clone();
    }
};

class BSConverterUtil
{
public:
    static mfxStatus TransFormY(mfxBitstream * bs, mfxFrameSurface1 *surface)
    {
        mfxU32 planeSize;
        mfxU32 w, h, i, pitch;
        mfxU8  *ptr;

        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        w = info.CropW;
        h = info.CropH;

        pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);
        ptr   = data.Y + info.CropX + info.CropY*pitch;

        if(pitch == w) 
        {
            //we can read whole plane directly to surface
            planeSize = w * h;
            MFX_CHECK_WITH_ERR(planeSize == BSUtil::MoveNBytes(ptr, bs, planeSize), MFX_ERR_MORE_DATA);
        } 
        else 
        {
            for(i=0; i<h; i++) 
            {
                MFX_CHECK_WITH_ERR(w == BSUtil::MoveNBytes(ptr + i*pitch, bs, w), MFX_ERR_MORE_DATA);
            }
        }
        return MFX_ERR_NONE;
    }

    static void ZeroedOutsideCropsNV12(mfxFrameSurface1 *surface)
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        if (info.CropW == info.Width && info.CropH == info.Height)
            return;

        if (info.CropY || info.CropX)
        {
            // to complex. further is can be optimized
            memset(data.Y, 0, info.Width * info.Height);
            memset(data.UV, 0, info.Width * info.Height / 2);
            return;
        }

        if (info.CropW != info.Width)
        {
            IppiSize roi = { info.Width - (info.CropW), info.CropH };
            ippiSet_8u_C1R(0, data.Y + info.CropW, pitch, roi);

            roi.height >>= 1;
            ippiSet_8u_C1R(0, data.UV + info.CropW, pitch, roi);
        }

        if (info.CropH != info.Height)
        {
            IppiSize roi = { info.Width, info.Height - info.CropH };
            ippiSet_8u_C1R(0, (Ipp8u*)data.Y + info.CropH * pitch, pitch, roi);

            roi.height >>= 1;
            ippiSet_8u_C1R(0, (Ipp8u*)data.UV + (info.CropH / 2) * pitch, pitch, roi);
        }
    }
};

// Bitstream converters declarations
class DECL_CONVERTER(MFX_FOURCC_NV12, MFX_FOURCC_NV12)
    : public IBitstreamConverter
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_NV12, MFX_FOURCC_NV12));
public:
    DECL_CONVERTER(MFX_FOURCC_NV12, MFX_FOURCC_NV12)()
        : IBitstreamConverter(MFX_FOURCC_NV12, MFX_FOURCC_NV12)
    {}

    mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        if (bs->DataLength < GetMinPlaneSize(info))
        {
            return MFX_ERR_MORE_DATA;
        }
        
#if defined(LINUX32) || defined (LINUX64)
        // on Windows surfaces comes zero-initialized, on Linux have to clear non-aligned stream boundaries
        BSConverterUtil::ZeroedOutsideCropsNV12(surface);
#endif

        IppiSize roi = { info.CropW, info.CropH };
        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);
        mfxU8  *ptr = data.Y + info.CropX + info.CropY*pitch;

        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, info.CropW, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;

        ptr = data.UV + info.CropX + (info.CropY >> 1) * pitch;

        roi.height >>= 1;
        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, info.CropW, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;

        return MFX_ERR_NONE;
    }
};

class DECL_CONVERTER(MFX_FOURCC_NV16, MFX_FOURCC_NV16)
    : public IBitstreamConverter
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_NV16, MFX_FOURCC_NV16));
public:
    DECL_CONVERTER(MFX_FOURCC_NV16, MFX_FOURCC_NV16)()
        : IBitstreamConverter(MFX_FOURCC_NV16, MFX_FOURCC_NV16)
    {}

    mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        if (bs->DataLength < GetMinPlaneSize(info))
        {
            return MFX_ERR_MORE_DATA;
        }

#if defined(LINUX32) || defined (LINUX64)
        // on Windows surfaces comes zero-initialized, on Linux have to clear non-aligned stream boundaries
        memset(data.Y, 0, info.Width * info.Height);
        memset(data.UV, 0, info.Width * info.Height / 2);
#endif
        IppiSize roi = { info.CropW, info.CropH };
        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);
        mfxU8  *ptr = data.Y + info.CropX + info.CropY*pitch;

        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, info.CropW, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;

        ptr = data.UV + info.CropX + info.CropY * pitch;

        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, info.CropW, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;
        return MFX_ERR_NONE;
    }
};

class DECL_CONVERTER(MFX_FOURCC_R16, MFX_FOURCC_R16)
    : public IBitstreamConverter
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_R16, MFX_FOURCC_R16));
public:
    DECL_CONVERTER(MFX_FOURCC_R16, MFX_FOURCC_R16)()
        : IBitstreamConverter(MFX_FOURCC_R16, MFX_FOURCC_R16)
    {}

    mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data      = surface->Data;
        mfxFrameInfo &info      = surface->Info;
#if defined(LINUX32) || defined (LINUX64)  
        memset(data.Y, 0, info.Width * info.Height * 2);
        memset(data.UV, 0, info.Width * info.Height );
#endif
        mfxU32 w, h, i, pitch;
        mfxU16 *ptr = data.Y16;
        if (info.CropH > 0 && info.CropW > 0)
        {
            w = info.CropW;
            h = info.CropH;
        }
        else
        {
            w = info.Width;
            h = info.Height;
        }
        pitch = data.Pitch;
        for (i = 0; i < h; i++)
        {
            MFX_CHECK_WITH_ERR(w * 2 == BSUtil::MoveNBytes((mfxU8 *)ptr + i * pitch, bs, w * 2), MFX_ERR_MORE_DATA);
        }
        return MFX_ERR_NONE;
    }
};
#define CLIP(x) ((x < 0) ? 0 : ((x > 1023) ? 1023 : x))

class DECL_CONVERTER(MFX_FOURCC_P010, MFX_FOURCC_P010)
    : public IBitstreamConverter
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_P010, MFX_FOURCC_P010));
public:
    DECL_CONVERTER(MFX_FOURCC_P010, MFX_FOURCC_P010)()
        : IBitstreamConverter(MFX_FOURCC_P010, MFX_FOURCC_P010)
    {}

    mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        if (bs->DataLength < GetMinPlaneSize(info))
        {
            return MFX_ERR_MORE_DATA;
        }

#if defined(LINUX32) || defined (LINUX64)
        // on Windows surfaces comes zero-initialized, on Linux have to clear non-aligned stream boundaries
        memset(data.Y, 0, info.Width * info.Height * 2);
        memset(data.UV, 0, info.Width * info.Height );
#endif

        IppiSize roi = { info.CropW*2, info.CropH };
        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);
        mfxU8  *ptr = data.Y + (info.CropX * 2) + info.CropY * pitch;

        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, roi.width, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;

        ptr = data.UV + (info.CropX * 2) + (info.CropY >> 1) * pitch;

        roi.height >>= 1;
        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, roi.width, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;
        return MFX_ERR_NONE;
    }
};

class DECL_CONVERTER(MFX_FOURCC_P210, MFX_FOURCC_P210)
    : public IBitstreamConverter
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_P210, MFX_FOURCC_P210));
public:
    DECL_CONVERTER(MFX_FOURCC_P210, MFX_FOURCC_P210)()
        : IBitstreamConverter(MFX_FOURCC_P210, MFX_FOURCC_P210)
    {}

    mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        if (bs->DataLength < GetMinPlaneSize(info))
        {
            return MFX_ERR_MORE_DATA;
        }

#if defined(LINUX32) || defined (LINUX64)  
        // on Windows surfaces comes zero-initialized, on Linux have to clear non-aligned stream boundaries
        memset(data.Y, 0, info.Width * info.Height * 2);
        memset(data.UV, 0, info.Width * info.Height );
#endif

        IppiSize roi = { info.CropW * 2, info.CropH };
        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);
        mfxU8  *ptr = data.Y + (info.CropX * 2) + info.CropY * pitch;

        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, roi.width, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;

        ptr = data.UV + (info.CropX * 2) + info.CropY * pitch;

        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, roi.width, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;
        return MFX_ERR_NONE;
    }
};

class DECL_CONVERTER(MFX_FOURCC_YV12, MFX_FOURCC_YV12)
    : public IBitstreamConverter
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_YV12, MFX_FOURCC_YV12));
public:
    DECL_CONVERTER(MFX_FOURCC_YV12, MFX_FOURCC_YV12)()
        : IBitstreamConverter(MFX_FOURCC_YV12, MFX_FOURCC_YV12)
    {}

    mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 planeSize = GetMinPlaneSize(info);
        if (bs->DataLength < planeSize)
        {
            return MFX_ERR_MORE_DATA;
        }

#if defined(LINUX32) || defined (LINUX64)  
        // on Windows surfaces comes zero-initialized, on Linux have to clear non-aligned stream boundaries
        memset(data.Y, 0, info.Width * info.Height);
        memset(data.V, 0, info.Width * info.Height / 4);
        memset(data.U, 0, info.Width * info.Height / 4);
#endif

        IppiSize roi = { info.CropW, info.CropH };
        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);
        mfxU8  *ptr = data.Y + info.CropX + info.CropY*pitch;

        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, roi.width, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;

        roi.height >>= 1;
        roi.width >>= 1;

        pitch >>= 1;
        ptr = data.V + (info.CropX >> 1) + (info.CropY >> 1) * pitch;
        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, roi.width, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;

        ptr = data.U + (info.CropX >> 1) + (info.CropY >> 1) * pitch;
        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, roi.width, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;

        return MFX_ERR_NONE;
    }
};

class DECL_CONVERTER(MFX_FOURCC_YV12, MFX_FOURCC_NV12)
    : public IBitstreamConverter
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_YV12, MFX_FOURCC_NV12));
public:
    DECL_CONVERTER(MFX_FOURCC_YV12, MFX_FOURCC_NV12)()
        : IBitstreamConverter(MFX_FOURCC_YV12, MFX_FOURCC_NV12)
    {}

    mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 planeSize = GetMinPlaneSize(info);
        if (bs->DataLength < planeSize)
        {
            return MFX_ERR_MORE_DATA;
        }

#if defined(LINUX32) || defined (LINUX64)
        // on Windows surfaces comes zero-initialized, on Linux have to clear non-aligned stream boundaries
        BSConverterUtil::ZeroedOutsideCropsNV12(surface);
#endif

        IppiSize roi = { info.CropW, info.CropH };
        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);
        mfxU8  *ptr = data.Y + info.CropX + info.CropY*pitch;
        mfxU8  *ptrUV = data.UV + info.CropX + (info.CropY >> 1)*pitch;

        Ipp32s pYVUStep[3] = { info.CropW, info.CropW / 2, info.CropW / 2 };
        const Ipp8u *(pYVU[3]);
        pYVU[0] = bs->Data + bs->DataOffset;
        pYVU[1] = pYVU[0] + roi.width * roi.height;
        pYVU[2] = pYVU[1] + roi.width * roi.height/4;

        bs->DataOffset += planeSize;
        bs->DataLength -= planeSize;

        IppStatus sts = ippiYCbCr420_8u_P3P2R(pYVU, pYVUStep, ptr, pitch, ptrUV, pitch, roi);
        if (sts != ippStsNoErr)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        return MFX_ERR_NONE;
    }
};

class DECL_CONVERTER(MFX_FOURCC_YUV444_8, MFX_FOURCC_AYUV)
    : public IBitstreamConverter
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_YUV444_8, MFX_FOURCC_AYUV));
public:
    DECL_CONVERTER(MFX_FOURCC_YUV444_8, MFX_FOURCC_AYUV)()
        : IBitstreamConverter(MFX_FOURCC_YUV444_8, MFX_FOURCC_AYUV)
    {}

    mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 w, h, i, j, pitch;
        mfxU8  *ptr;

        w = info.CropW;
        h = info.CropH;
        pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);
        ptr = data.V + info.CropX + (info.CropY)*pitch;

        m_line.resize(w);

        for (mfxI32 c = 2; c >= 0; c--) // planar YUV -> packed VUYA
        {
            for (i = 0; i < h; i++)
            {
                MFX_CHECK_WITH_ERR(w == BSUtil::MoveNBytes(&m_line.front(), bs, w), MFX_ERR_MORE_DATA);

                const mfxU8* p_src = &m_line.front();
                mfxU8* p_dst = ptr + i*pitch;

                for (j = 0; j < w; j++, p_dst += 4)
                    p_dst[c] = p_src[j];
            }
        }

        return MFX_ERR_NONE;
    }

private:
    std::vector<mfxU8> m_line;
};

class DECL_CONVERTER(MFX_FOURCC_RGB3, MFX_FOURCC_RGB4)
    : public IBitstreamConverter
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_RGB3, MFX_FOURCC_RGB4));
public:
    DECL_CONVERTER(MFX_FOURCC_RGB3, MFX_FOURCC_RGB4)()
        : IBitstreamConverter(MFX_FOURCC_RGB3, MFX_FOURCC_RGB4)
    {}

    mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) override
    {
        mfxU32 w, h, i, j, pitch;
        mfxU8  *ptr, *ptr2;

        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        w = info.CropW;
        h = info.CropH;

        pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        ptr = data.B + info.CropX * 4 + info.CropY * pitch;
        
        for (i = 0; i < h; i++, ptr += pitch)
        {
            ptr2 = ptr;
            for (j =0; j < w; j++)
            {
                MFX_CHECK_WITH_ERR(3 == BSUtil::MoveNBytes(ptr2, bs, 3), MFX_ERR_MORE_DATA);
                //zero for alpha channel
                ptr2[3] = 0;
                ptr2   += 4;
            }
        }
        return MFX_ERR_NONE;
    }
};

//generic swop converter for RGBA/BGRA
class BSConverterRGBASwop
    : public IBitstreamConverter
{
public:
    BSConverterRGBASwop(mfxU32 inFourCC, mfxU32 outFourCC)
        : IBitstreamConverter(inFourCC, outFourCC),
        m_sample_size()
    {
    }
    mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) override
    {
        mfxU32 w, h, i, j, pitch;
        mfxU8  *ptr;
        mfxU8 t_swop;

        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        w = info.CropW;
        h = info.CropH;

        pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);
        ptr = start_pointer(surface);

        if (pitch == w)
        {
            //copy data directly
            MFX_CHECK_WITH_ERR(w * h * m_sample_size == BSUtil::MoveNBytes(ptr, bs, w * h * m_sample_size), MFX_ERR_MORE_DATA);
            //swop
            for (i = 0; i < h; i++)
                for (j = 0; j < w * m_sample_size; j = j + m_sample_size)
                {
                    t_swop = ptr[i * h + j];
                    ptr[i * h + j] = ptr[i * h + j + 2];
                    ptr[i * h + j + 2] = t_swop;
                }
        }
        else
        {
            for (i = 0; i < h; i++)
            {
                MFX_CHECK_WITH_ERR(w * m_sample_size == BSUtil::MoveNBytes(ptr, bs, w * m_sample_size), MFX_ERR_MORE_DATA);
                //swop
                for (j = 0; j < w * m_sample_size; j = j + m_sample_size)
                {
                    t_swop = ptr[j];
                    ptr[j] = ptr[j + 2];
                    ptr[j + 2] = t_swop;
                }
                ptr += pitch;
            }
        }

        return MFX_ERR_NONE;
    }

protected:
    virtual mfxU8* start_pointer(mfxFrameSurface1 *surface)
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return data.R + info.CropX * m_sample_size + info.CropY * pitch;
    }

    mfxU8 m_sample_size;
};

class DECL_CONVERTER(MFX_FOURCC_RGB4, MFX_FOURCC_BGR4)
    : public BSConverterRGBASwop
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_RGB4, MFX_FOURCC_BGR4));
public:
    DECL_CONVERTER(MFX_FOURCC_RGB4, MFX_FOURCC_BGR4)()
        : BSConverterRGBASwop(MFX_FOURCC_RGB4, MFX_FOURCC_BGR4)
    {
        m_sample_size = 4;
    }

protected:
    mfxU8* start_pointer(mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return data.R + info.CropX * m_sample_size + info.CropY * pitch;
    }
};

class DECL_CONVERTER(MFX_FOURCC_BGR4, MFX_FOURCC_RGB4)
    : public BSConverterRGBASwop
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_BGR4, MFX_FOURCC_RGB4));
public:
    DECL_CONVERTER(MFX_FOURCC_BGR4, MFX_FOURCC_RGB4)()
        : BSConverterRGBASwop(MFX_FOURCC_BGR4, MFX_FOURCC_RGB4)
    {
        m_sample_size = 4;
    }

protected:
    mfxU8* start_pointer(mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return data.B + info.CropX * m_sample_size + info.CropY * pitch;
    }
};

//generic copy converter for packeted formats
class BSConverterPacketedCopy
    : public IBitstreamConverter
{
public:
    BSConverterPacketedCopy(mfxU32 inFourCC, mfxU32 outFourCC)
        : IBitstreamConverter(inFourCC, outFourCC),
        m_sample_size()
    {
    }
    mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) override
    {
        mfxU32 w, h, i, pitch;
        mfxU8  *ptr;

        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        w = info.CropW;
        h = info.CropH;

        pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        ptr = start_pointer(surface);

        if (pitch == w)
        {
            //copy data directly
            MFX_CHECK_WITH_ERR(w * h * m_sample_size == BSUtil::MoveNBytes(ptr, bs, w * h * m_sample_size), MFX_ERR_MORE_DATA);
        }
        else
        {
            for (i = 0; i < h; i++)
            {
                MFX_CHECK_WITH_ERR(w * m_sample_size == BSUtil::MoveNBytes(ptr, bs, w * m_sample_size), MFX_ERR_MORE_DATA);
                ptr += pitch;
            }
        }

        return MFX_ERR_NONE;
    }

protected:
    virtual mfxU8* start_pointer(mfxFrameSurface1 *surface)
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return data.B + info.CropX * m_sample_size + info.CropY * pitch;
    }

    mfxU8 m_sample_size;
};

class DECL_CONVERTER(MFX_FOURCC_RGB4, MFX_FOURCC_RGB4)
    : public BSConverterPacketedCopy
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_RGB4, MFX_FOURCC_RGB4));
public:
    DECL_CONVERTER(MFX_FOURCC_RGB4, MFX_FOURCC_RGB4)()
        : BSConverterPacketedCopy(MFX_FOURCC_RGB4, MFX_FOURCC_RGB4)
    {
        m_sample_size = 4;//base class donot touch this member
    }

protected:
    mfxU8* start_pointer(mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return data.B + info.CropX * m_sample_size + info.CropY * pitch;
    }
};

class DECL_CONVERTER(MFX_FOURCC_BGR4, MFX_FOURCC_BGR4)
    : public BSConverterPacketedCopy
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_BGR4, MFX_FOURCC_BGR4));
public:
    DECL_CONVERTER(MFX_FOURCC_BGR4, MFX_FOURCC_BGR4)()
        : BSConverterPacketedCopy(MFX_FOURCC_BGR4, MFX_FOURCC_BGR4)
    {
        m_sample_size = 4;//base class donot touch this member
    }

protected:
    mfxU8* start_pointer(mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return data.R + info.CropX * m_sample_size + info.CropY * pitch;
    }
};

class DECL_CONVERTER(MFX_FOURCC_RGB3, MFX_FOURCC_RGB3)
    : public BSConverterPacketedCopy
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_RGB3, MFX_FOURCC_RGB3));
public:
    DECL_CONVERTER(MFX_FOURCC_RGB3, MFX_FOURCC_RGB3)()
        : BSConverterPacketedCopy(MFX_FOURCC_RGB3, MFX_FOURCC_RGB3)
    {
        m_sample_size = 3;
    }
};

class DECL_CONVERTER(MFX_FOURCC_YUY2, MFX_FOURCC_YUY2)
    : public BSConverterPacketedCopy
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_YUY2, MFX_FOURCC_YUY2));
public:
    DECL_CONVERTER(MFX_FOURCC_YUY2, MFX_FOURCC_YUY2)()
        : BSConverterPacketedCopy(MFX_FOURCC_YUY2, MFX_FOURCC_YUY2)
    {
        m_sample_size = 2;
    }

protected:
    mfxU8* start_pointer(mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return data.Y + info.CropX * m_sample_size + info.CropY * pitch;
    }
};

#if (MFX_VERSION >= MFX_VERSION_NEXT)
class DECL_CONVERTER(MFX_FOURCC_Y210, MFX_FOURCC_Y210)
    : public BSConverterPacketedCopy
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_Y210, MFX_FOURCC_Y210));
public:
    DECL_CONVERTER(MFX_FOURCC_Y210, MFX_FOURCC_Y210)()
        : BSConverterPacketedCopy(MFX_FOURCC_Y210, MFX_FOURCC_Y210)
    {
        m_sample_size = 4;
    }

protected:
    mfxU8* start_pointer(mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return data.Y + info.CropX * m_sample_size + info.CropY * pitch;
    }
};

class DECL_CONVERTER(MFX_FOURCC_Y410, MFX_FOURCC_Y410)
    : public BSConverterPacketedCopy
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_Y410, MFX_FOURCC_Y410));
public:
    DECL_CONVERTER(MFX_FOURCC_Y410, MFX_FOURCC_Y410)()
        : BSConverterPacketedCopy(MFX_FOURCC_Y410, MFX_FOURCC_Y410)
    {
        m_sample_size = 4;
    }

protected:
    mfxU8* start_pointer(mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return (mfxU8*)surface->Data.Y410 + info.CropX * m_sample_size + info.CropY * pitch;
    }
};
#endif // #if (MFX_VERSION >= 1027)

class DECL_CONVERTER(MFX_FOURCC_A2RGB10, MFX_FOURCC_A2RGB10)
    : public BSConverterPacketedCopy
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_A2RGB10, MFX_FOURCC_A2RGB10));
public:
    DECL_CONVERTER(MFX_FOURCC_A2RGB10, MFX_FOURCC_A2RGB10)()
        : BSConverterPacketedCopy(MFX_FOURCC_A2RGB10, MFX_FOURCC_A2RGB10)
    {
        m_sample_size = 4;
    }

protected:
    mfxU8* start_pointer(mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return (mfxU8*)surface->Data.A2RGB10 + info.CropX * m_sample_size + info.CropY * pitch;
    }
};

class DECL_CONVERTER(MFX_FOURCC_AYUV, MFX_FOURCC_AYUV)
    : public BSConverterPacketedCopy
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_AYUV, MFX_FOURCC_AYUV));
public:
    DECL_CONVERTER(MFX_FOURCC_AYUV, MFX_FOURCC_AYUV)()
        : BSConverterPacketedCopy(MFX_FOURCC_AYUV, MFX_FOURCC_AYUV)
    {
        m_sample_size = 4;
    }

protected:
    mfxU8* start_pointer(mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return surface->Data.V + info.CropX * m_sample_size + info.CropY * pitch;
    }
};

#if (MFX_VERSION >= MFX_VERSION_NEXT)
class DECL_CONVERTER(MFX_FOURCC_P016, MFX_FOURCC_P016)
    : public IBitstreamConverter
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_P016, MFX_FOURCC_P016));
public:
    DECL_CONVERTER(MFX_FOURCC_P016, MFX_FOURCC_P016)()
        : IBitstreamConverter(MFX_FOURCC_P016, MFX_FOURCC_P016)
    {}

    mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        if (bs->DataLength < GetMinPlaneSize(info))
        {
            return MFX_ERR_MORE_DATA;
        }

#if defined(LINUX32) || defined (LINUX64)
        // on Windows surfaces comes zero-initialized, on Linux have to clear non-aligned stream boundaries
        memset(data.Y, 0, info.Width * info.Height * 2);
        memset(data.UV, 0, info.Width * info.Height);
#endif

        IppiSize roi = { info.CropW * 2, info.CropH };
        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);
        mfxU8  *ptr = data.Y + (info.CropX * 2) + info.CropY * pitch;

        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, roi.width, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;

        ptr = data.UV + (info.CropX * 2) + (info.CropY >> 1) * pitch;

        roi.height >>= 1;
        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, roi.width, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;
        return MFX_ERR_NONE;
    }
};

class DECL_CONVERTER(MFX_FOURCC_Y216, MFX_FOURCC_Y216)
    : public BSConverterPacketedCopy
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_Y216, MFX_FOURCC_Y216));
public:
    DECL_CONVERTER(MFX_FOURCC_Y216, MFX_FOURCC_Y216)()
        : BSConverterPacketedCopy(MFX_FOURCC_Y216, MFX_FOURCC_Y216)
    {
        m_sample_size = 4;
    }

protected:
    mfxU8* start_pointer(mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return data.Y + info.CropX * m_sample_size + info.CropY * pitch;
    }
};

class DECL_CONVERTER(MFX_FOURCC_Y416, MFX_FOURCC_Y416)
    : public BSConverterPacketedCopy
{
    IMPLEMENT_CLONE(DECL_CONVERTER(MFX_FOURCC_Y416, MFX_FOURCC_Y416));
public:
    DECL_CONVERTER(MFX_FOURCC_Y416, MFX_FOURCC_Y416)()
        : BSConverterPacketedCopy(MFX_FOURCC_Y416, MFX_FOURCC_Y416)
    {
        m_sample_size = 8;
    }

protected:
    mfxU8* start_pointer(mfxFrameSurface1 *surface) override
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return (mfxU8*)surface->Data.Y410 + info.CropX * m_sample_size + info.CropY * pitch;
    }
};
#endif // #if (MFX_VERSION >= MFX_VERSION_NEXT)
