/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2017 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include <map>
#include <algorithm>
#include "mfx_shared_ptr.h"
#include "mfx_ibitstream_converter.h"
#include "shared_utils.h"
#include "fast_copy.h"
#include "ippcc.h"

class BitstreamConverterFactory
    : public IBitstreamConverterFactory
{
    typedef std::map <std::pair<mfxU32, mfxU32>, mfx_shared_ptr<IBitstreamConverter> > Container_t;
    Container_t m_converters;
public:

    void Register(mfx_shared_ptr<IBitstreamConverter> p)
    {
        std::pair<mfxU32, mfxU32 > transform_type;
        for (int i = 0; std::make_pair(0u, 0u) != (transform_type = p->GetTransformType(i)); i++)
        {
            Container_t::iterator it = m_converters.find(transform_type);
            if (it != m_converters.end())
            {
                PipelineTrace((VM_STRING("WARNING: converter from %s to %s already exist")
                    , GetMFXFourccString(transform_type.first).c_str()
                    , GetMFXFourccString(transform_type.second).c_str()));
                it->second = p;
            }
            else
            {
                m_converters[transform_type] = p;
            }
        }
    }
    IBitstreamConverter * Create(mfxU32 inFourcc, mfxU32 outFourcc)
    {
        Container_t::iterator i = m_converters.find(std::make_pair(inFourcc, outFourcc));
        if (i == m_converters.end())
        {
            return NULL;
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

//intentionally not support interface
template <mfxU32 in_fmt, mfxU32 out_fmt>
class BSConvert
{
};

template <int in_fmt, int out_fmt>
class BSConvertBase
    : public IBitstreamConverter
{
public:
    virtual std::pair<mfxU32, mfxU32> GetTransformType(int nPosition)
    {
        if (0 == nPosition)
            return std::make_pair(in_fmt, out_fmt);

        return std::make_pair(0, 0);
    }
};

template <>
class BSConvert<MFX_FOURCC_NV12, MFX_FOURCC_NV12>
    : public BSConvertBase<MFX_FOURCC_NV12, MFX_FOURCC_NV12>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_NV12 MFX_PP_COMMA() MFX_FOURCC_NV12>);

public:
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface)
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

template <>
class BSConvert<MFX_FOURCC_NV16, MFX_FOURCC_NV16>
    : public BSConvertBase<MFX_FOURCC_NV16, MFX_FOURCC_NV16>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_NV16 MFX_PP_COMMA() MFX_FOURCC_NV16>);

public:
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface)
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

template <>
class BSConvert<MFX_FOURCC_R16, MFX_FOURCC_R16>
    : public BSConvertBase<MFX_FOURCC_R16, MFX_FOURCC_R16>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_R16 MFX_PP_COMMA() MFX_FOURCC_R16>);
public:
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface)
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
template <>
class BSConvert<MFX_FOURCC_P010, MFX_FOURCC_P010>
    : public BSConvertBase<MFX_FOURCC_P010, MFX_FOURCC_P010>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_P010 MFX_PP_COMMA() MFX_FOURCC_P010>);

public:
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface)
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

template <>
class BSConvert<MFX_FOURCC_P210, MFX_FOURCC_P210>
    : public BSConvertBase<MFX_FOURCC_P210, MFX_FOURCC_P210>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_P210 MFX_PP_COMMA() MFX_FOURCC_P210>);
    
public:
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface)
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

template <>
class BSConvert<MFX_FOURCC_YV12, MFX_FOURCC_YV12>
    : public BSConvertBase<MFX_FOURCC_YV12, MFX_FOURCC_YV12>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_YV12 MFX_PP_COMMA() MFX_FOURCC_YV12>);
public:
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface)
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
        ptr = data.U + (info.CropX >> 1) + (info.CropY >> 1) * pitch;
        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, roi.width, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;

        ptr = data.V + (info.CropX >> 1) + (info.CropY >> 1) * pitch;
        FastCopy::Copy(ptr, pitch, bs->Data + bs->DataOffset, roi.width, roi, COPY_SYS_TO_SYS);
        bs->DataOffset += roi.width * roi.height;
        bs->DataLength -= roi.width * roi.height;

        return MFX_ERR_NONE;
    }
};


template <>
class BSConvert<MFX_FOURCC_YV12, MFX_FOURCC_NV12>
    : public BSConvertBase<MFX_FOURCC_YV12, MFX_FOURCC_NV12>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_YV12 MFX_PP_COMMA() MFX_FOURCC_NV12>);
public:
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface)
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

template <>
class BSConvert<MFX_FOURCC_YUV444_8, MFX_FOURCC_AYUV>
    : public BSConvertBase<MFX_FOURCC_YUV444_8, MFX_FOURCC_AYUV>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_YUV444_8 MFX_PP_COMMA() MFX_FOURCC_AYUV>);
public:
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface)
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

template <>
class BSConvert<MFX_FOURCC_RGB3, MFX_FOURCC_RGB4>
    : public BSConvertBase<MFX_FOURCC_RGB3, MFX_FOURCC_RGB4>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_RGB3 MFX_PP_COMMA() MFX_FOURCC_RGB4>);
public:
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface)
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

//generic copy converter for packeted formats
template <int in_fmt, int out_fmt>
class BSConverterPacketedCopy
    : public BSConvertBase<in_fmt, out_fmt>
{
public:
    BSConverterPacketedCopy()
        : m_sample_size()
    {
    }
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface)
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

template <>
class BSConvert<MFX_FOURCC_RGB4, MFX_FOURCC_RGB4>
    : public BSConverterPacketedCopy<MFX_FOURCC_RGB4, MFX_FOURCC_RGB4>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_RGB4 MFX_PP_COMMA() MFX_FOURCC_RGB4>);
public:
    BSConvert()
    {
        m_sample_size = 4;//base class donot touch this member
    }
};

template <>
class BSConvert<MFX_FOURCC_RGB3, MFX_FOURCC_RGB3>
    : public BSConverterPacketedCopy<MFX_FOURCC_RGB3, MFX_FOURCC_RGB3>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_RGB3 MFX_PP_COMMA() MFX_FOURCC_RGB3>);
public:
    BSConvert()
    {
        m_sample_size = 3;
    }
};

template <>
class BSConvert<MFX_FOURCC_YUY2, MFX_FOURCC_YUY2>
    : public BSConverterPacketedCopy<MFX_FOURCC_YUY2, MFX_FOURCC_YUY2>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_YUY2 MFX_PP_COMMA() MFX_FOURCC_YUY2>);
public:
    BSConvert()
    {
        m_sample_size = 2;
    }

protected:
    virtual mfxU8* start_pointer(mfxFrameSurface1 *surface)
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;
        
        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return data.Y + info.CropX * m_sample_size + info.CropY * pitch;
    }
};

template <>
class BSConvert<MFX_FOURCC_Y210, MFX_FOURCC_Y210>
    : public BSConverterPacketedCopy<MFX_FOURCC_Y210, MFX_FOURCC_Y210>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_Y210 MFX_PP_COMMA() MFX_FOURCC_Y210>);
public:
    BSConvert()
    {
        m_sample_size = 2;
    }

protected:
    virtual mfxU8* start_pointer(mfxFrameSurface1 *surface)
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;
        
        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return data.Y + info.CropX * m_sample_size + info.CropY * pitch;
    }
};

template <>
class BSConvert<MFX_FOURCC_Y410, MFX_FOURCC_Y410>
    : public BSConverterPacketedCopy<MFX_FOURCC_Y410, MFX_FOURCC_Y410>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_Y410 MFX_PP_COMMA() MFX_FOURCC_Y410>);
public:
    BSConvert()
    {
        m_sample_size = 4;
    }

protected:
    virtual mfxU8* start_pointer(mfxFrameSurface1 *surface)
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return (mfxU8*)surface->Data.Y410 + info.CropX * m_sample_size + info.CropY * pitch;
    }
};

template <>
class BSConvert<MFX_FOURCC_A2RGB10, MFX_FOURCC_A2RGB10>
    : public BSConverterPacketedCopy<MFX_FOURCC_A2RGB10, MFX_FOURCC_A2RGB10>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_A2RGB10 MFX_PP_COMMA() MFX_FOURCC_A2RGB10>);
public:
    BSConvert()
    {
        m_sample_size = 4;
    }

protected:
    virtual mfxU8* start_pointer(mfxFrameSurface1 *surface)
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return (mfxU8*)surface->Data.A2RGB10 + info.CropX * m_sample_size + info.CropY * pitch;
    }
};

template <>
class BSConvert<MFX_FOURCC_AYUV, MFX_FOURCC_AYUV>
    : public BSConverterPacketedCopy<MFX_FOURCC_AYUV, MFX_FOURCC_AYUV>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_AYUV MFX_PP_COMMA() MFX_FOURCC_AYUV>);
public:
    BSConvert()
    {
        m_sample_size = 4;
    }

protected:
    virtual mfxU8* start_pointer(mfxFrameSurface1 *surface)
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

        return surface->Data.V + info.CropX * m_sample_size + info.CropY * pitch;
    }
};