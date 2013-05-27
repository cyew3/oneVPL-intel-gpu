/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include <map>
#include <algorithm>
#include "mfx_shared_ptr.h"
#include "mfx_ibitstream_converter.h"

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

        pitch = data.Pitch;
        ptr   = data.Y + info.CropX + info.CropY*data.Pitch;

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
        MFX_CHECK_STS(BSConverterUtil::TransFormY(bs, surface));

        mfxU32 planeSize;
        mfxU32 w, h, pitch;
        mfxU8  *ptr;

        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        w = info.CropW;
        h = info.CropH >> 1;
        pitch = data.Pitch;
        ptr = data.UV + info.CropX + (info.CropY >> 1) * pitch;

        // load UV
        if (pitch == w)
        {
            //we can read whole plane directly to surface
            planeSize  = w * h;
            MFX_CHECK_WITH_ERR(planeSize == BSUtil::MoveNBytes(ptr, bs, planeSize), MFX_ERR_MORE_DATA);
        }
        else
        {
            for (mfxU32 i = 0; i < h; i++) 
            {
                MFX_CHECK_WITH_ERR(w == BSUtil::MoveNBytes(ptr + i * pitch, bs, w), MFX_ERR_MORE_DATA);
            }
        }

        return MFX_ERR_NONE;
    }

protected:
    
};

template <>
class BSConvert<MFX_FOURCC_YV12, MFX_FOURCC_YV12>
    : public BSConvertBase<MFX_FOURCC_YV12, MFX_FOURCC_YV12>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_YV12 MFX_PP_COMMA() MFX_FOURCC_YV12>);
public:
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface)
    {
        MFX_CHECK_STS(BSConverterUtil::TransFormY(bs, surface));

        mfxU32 planeSize;
        mfxU32 w, h, i, pitch;
        mfxU8  *ptr, *ptr2;

        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        w = info.CropW >> 1;
        h = info.CropH >> 1;

        pitch = data.Pitch >> 1;

        ptr = data.U + (info.CropX >> 1) + (info.CropY >> 1) * pitch;
        ptr2 = data.V + (info.CropX >> 1) + (info.CropY >> 1) * pitch;

        if(pitch == w) 
        {
            planeSize = w * h;
            MFX_CHECK_WITH_ERR(planeSize == BSUtil::MoveNBytes(ptr, bs, planeSize), MFX_ERR_MORE_DATA);
            MFX_CHECK_WITH_ERR(planeSize == BSUtil::MoveNBytes(ptr2, bs, planeSize), MFX_ERR_MORE_DATA);
        } else 
        {
            for(i = 0; i <h ; i++) 
            {
                MFX_CHECK_WITH_ERR(w == BSUtil::MoveNBytes(ptr + i*pitch, bs, w), MFX_ERR_MORE_DATA);
            }
            for(i = 0; i < h; i++) 
            {
                MFX_CHECK_WITH_ERR(w == BSUtil::MoveNBytes(ptr2 + i*pitch, bs, w), MFX_ERR_MORE_DATA);
            }
        }
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
        MFX_CHECK_STS(BSConverterUtil::TransFormY(bs, surface));

        mfxU32 w, h, i, j, pitch;
        mfxU8  *ptr;

        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        w = info.CropW >> 1;
        h = info.CropH >> 1;

        pitch = data.Pitch;
        ptr   = data.Y + info.CropX + info.CropY*data.Pitch;

        m_chroma_line.resize(w);
        
        pitch >>= 0;
        ptr = data.U + info.CropX + (info.CropY>>1)*pitch;

        // load U
        for (i = 0; i < h; i++) 
        {
            MFX_CHECK_WITH_ERR(w == BSUtil::MoveNBytes(&m_chroma_line.front(), bs, w), MFX_ERR_MORE_DATA);

            for (j=0; j<w; j++)
            {
                ptr[i * pitch + j * 2] = m_chroma_line[j];
            }
        }
        // load V
        for (i = 0; i < h; i++) 
        {
            MFX_CHECK_WITH_ERR(w == BSUtil::MoveNBytes(&m_chroma_line.front(), bs, w), MFX_ERR_MORE_DATA);

            for (j = 0; j < w; j++)
            {
                ptr[i * pitch + j * 2 + 1] = m_chroma_line[j];
            }
        }
        return MFX_ERR_NONE;
    }

private:
    std::vector<mfxU8> m_chroma_line;
};

template <>
class BSConvert<MFX_FOURCC_RGB3, MFX_FOURCC_RGB4>
    : public BSConvertBase<MFX_FOURCC_RGB3, MFX_FOURCC_RGB4>
{
    IMPLEMENT_CLONE(BSConvert<MFX_FOURCC_RGB3 MFX_PP_COMMA() MFX_FOURCC_RGB4>);
public:
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface)
    {
        mfxU32 w, h, i, j;
        mfxU8  *ptr, *ptr2;

        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        w = info.CropW;
        h = info.CropH;

        ptr = data.B + info.CropX * 4 + info.CropY * data.Pitch;
        
        for (i = 0; i < h; i++, ptr += data.Pitch)
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
        mfxU32 w, h, i;
        mfxU8  *ptr;

        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        w = info.CropW;
        h = info.CropH;

        ptr = start_pointer(surface);

        if (data.Pitch == w)
        {
            //copy data directly
            MFX_CHECK_WITH_ERR(w * h * m_sample_size == BSUtil::MoveNBytes(ptr, bs, w * h * m_sample_size), MFX_ERR_MORE_DATA);
        }
        else
        {
            for (i = 0; i < h; i++)
            {
                MFX_CHECK_WITH_ERR(w * m_sample_size == BSUtil::MoveNBytes(ptr, bs, w * m_sample_size), MFX_ERR_MORE_DATA);
                ptr += data.Pitch;
            }
        }

        return MFX_ERR_NONE;
    }
protected:
    
    virtual mfxU8* start_pointer(mfxFrameSurface1 *surface)
    {
        mfxFrameData &data = surface->Data;
        mfxFrameInfo &info = surface->Info;

        return data.B + info.CropX * m_sample_size + info.CropY * data.Pitch;
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

        return data.Y + info.CropX * m_sample_size + info.CropY * data.Pitch;
    }
};
