/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_d3d9.cpp

\* ****************************************************************************** */

#include "mfx_screen_capture_dirty_rect.h"
#include "mfx_utils.h"
#include <memory>
#include <list>
#include <algorithm> //min

#define H_BLOCKS 16
#define W_BLOCKS 16

namespace MfxCapture
{

template<typename T>
T* GetExtendedBuffer(const mfxU32& BufferId, const mfxFrameSurface1* surf)
{
    if(!surf || !BufferId)
        return 0;
    if(!surf->Data.NumExtParam || !surf->Data.ExtParam)
        return 0;
    for(mfxU32 i = 0; i < surf->Data.NumExtParam; ++i)
    {
        if(!surf->Data.ExtParam[i])
            continue;
        if(BufferId == surf->Data.ExtParam[i]->BufferId && sizeof(T) == surf->Data.ExtParam[i]->BufferSz)
            return (T*) surf->Data.ExtParam[i];
    }

    return 0;
}

CpuDirtyRectFilter::CpuDirtyRectFilter(const mfxCoreInterface* _core, bool isSysMem, const mfxVideoParam* par)
    :m_pmfxCore(_core)
{
    memset(&m_PrevSysSurface, 0, sizeof(mfxFrameSurface1));
    memset(&m_CurSysSurface, 0, sizeof(mfxFrameSurface1));

    if( !isSysMem && par )
    {
        memset(&m_PrevSysSurface, 0, sizeof(m_PrevSysSurface));
        m_PrevSysSurface.Info = par->mfx.FrameInfo;
        if(MFX_FOURCC_NV12 == m_PrevSysSurface.Info.FourCC)
        {
            m_PrevSysSurface.Data.Y = new mfxU8[m_PrevSysSurface.Info.Width * m_PrevSysSurface.Info.Height * 3 / 2];
            m_PrevSysSurface.Data.UV = m_PrevSysSurface.Data.Y + m_PrevSysSurface.Info.Width * m_PrevSysSurface.Info.Height;
            m_PrevSysSurface.Data.Pitch = m_PrevSysSurface.Info.Width;
        }
        else if(MFX_FOURCC_RGB4 == m_PrevSysSurface.Info.FourCC || 
                MFX_FOURCC_AYUV_RGB4 == m_PrevSysSurface.Info.FourCC || 
                /*DXGI_FORMAT_AYUV*/100 == m_PrevSysSurface.Info.FourCC)
        {
            m_PrevSysSurface.Data.B = new mfxU8[m_PrevSysSurface.Info.Width * m_PrevSysSurface.Info.Height * 4];
            m_PrevSysSurface.Data.G = m_PrevSysSurface.Data.B + 1;
            m_PrevSysSurface.Data.R = m_PrevSysSurface.Data.B + 2;
            m_PrevSysSurface.Data.A = m_PrevSysSurface.Data.B + 3;

            m_PrevSysSurface.Data.Pitch = 4*m_PrevSysSurface.Info.Width;
        }

        memset(&m_CurSysSurface, 0, sizeof(m_CurSysSurface));
        m_CurSysSurface.Info = par->mfx.FrameInfo;
        if(MFX_FOURCC_NV12 == m_CurSysSurface.Info.FourCC)
        {
            m_CurSysSurface.Data.Y = new mfxU8[m_CurSysSurface.Info.Width * m_CurSysSurface.Info.Height * 3 / 2];
            m_CurSysSurface.Data.UV = m_CurSysSurface.Data.Y + m_CurSysSurface.Info.Width * m_CurSysSurface.Info.Height;
            m_CurSysSurface.Data.Pitch = m_CurSysSurface.Info.Width;
        }
        else if(MFX_FOURCC_RGB4 == m_CurSysSurface.Info.FourCC || 
                MFX_FOURCC_AYUV_RGB4 == m_CurSysSurface.Info.FourCC || 
                /*DXGI_FORMAT_AYUV*/100 == m_CurSysSurface.Info.FourCC)
        {
            m_CurSysSurface.Data.B = new mfxU8[m_CurSysSurface.Info.Width * m_CurSysSurface.Info.Height * 4];
            m_CurSysSurface.Data.G = m_CurSysSurface.Data.B + 1;
            m_CurSysSurface.Data.R = m_CurSysSurface.Data.B + 2;
            m_CurSysSurface.Data.A = m_CurSysSurface.Data.B + 3;

            m_CurSysSurface.Data.Pitch = 4*m_CurSysSurface.Info.Width;
        }
    }
}

CpuDirtyRectFilter::~CpuDirtyRectFilter()
{
    if(MFX_FOURCC_NV12 == m_PrevSysSurface.Info.FourCC)
    {
        if(m_PrevSysSurface.Data.Y)
        {
            delete[] m_PrevSysSurface.Data.Y;
            m_PrevSysSurface.Data.Y = 0;
        }
    } else if(0 != m_PrevSysSurface.Info.FourCC) {
        if(m_PrevSysSurface.Data.B)
        {
            delete[] m_PrevSysSurface.Data.B;
            m_PrevSysSurface.Data.B = 0;
        }
    }

    if(MFX_FOURCC_NV12 == m_CurSysSurface.Info.FourCC)
    {
        if(m_CurSysSurface.Data.Y)
        {
            delete[] m_CurSysSurface.Data.Y;
            m_CurSysSurface.Data.Y = 0;
        }
    } else if(0 != m_CurSysSurface.Info.FourCC) {
        if(m_CurSysSurface.Data.B)
        {
            delete[] m_CurSysSurface.Data.B;
            m_CurSysSurface.Data.B = 0;
        }
    }
    memset(&m_PrevSysSurface, 0, sizeof(mfxFrameSurface1));
    memset(&m_CurSysSurface, 0, sizeof(mfxFrameSurface1));
}

mfxStatus CpuDirtyRectFilter::Init(const mfxFrameInfo& in, const mfxFrameInfo& out)
{
    in;
    out;

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus CpuDirtyRectFilter::RunFrameVPP(mfxFrameSurface1& in, mfxFrameSurface1& out)
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    if(memcmp(&in.Info, &out.Info, sizeof(in.Info)))
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    mfxExtDirtyRect* mfxRect = GetExtendedBuffer<mfxExtDirtyRect>(MFX_EXTBUFF_DIRTY_RECTANGLES, &out);
    mfxExtDirtyRect tmpRect;
    memset(&tmpRect,0,sizeof(tmpRect));

    if(!mfxRect)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxFrameSurface1 _in  = in;
    mfxFrameSurface1 _out = out;
    _in.Data.MemId = 0;
    _out.Data.MemId = 0;
    mfxHDL hdl = 0;
    if(!in.Data.Y && in.Data.MemId)
    {
        mfxRes = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, in.Data.MemId, &hdl);
        if(mfxRes || hdl)
        {
            _in = m_PrevSysSurface;
            mfxRes = m_pmfxCore->CopyFrame(m_pmfxCore->pthis, &m_PrevSysSurface, &in);
            MFX_CHECK_STS(mfxRes);
        }
    }

    hdl = 0;
    if(!out.Data.Y && out.Data.MemId)
    {
        mfxRes = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, out.Data.MemId, &hdl);
        if(mfxRes || hdl)
        {
            _out = m_CurSysSurface;
            mfxRes = m_pmfxCore->CopyFrame(m_pmfxCore->pthis, &m_CurSysSurface, &out);
            MFX_CHECK_STS(mfxRes);
        }
    }
    const bool unlock_in  = _in.Data.Y ? false : true;
    const bool unlock_out = _out.Data.Y ? false : true;

    if(unlock_in)
    {
        mfxRes = m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, in.Data.MemId, &_in.Data);
        if(mfxRes)
            return MFX_ERR_LOCK_MEMORY;
    }
    if(unlock_out)
    {
        mfxRes = m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, out.Data.MemId, &_out.Data);
        if(mfxRes)
            return MFX_ERR_LOCK_MEMORY;
    }

    if(MFX_FOURCC_NV12 == _in.Info.FourCC)
    {
        if(!_in.Data.Y || !_in.Data.UV) 
            return MFX_ERR_LOCK_MEMORY;
    }
    if(MFX_FOURCC_AYUV_RGB4 == _in.Info.FourCC || MFX_FOURCC_RGB4 == _in.Info.FourCC || /*DXGI_FORMAT_AYUV*/100 == _in.Info.FourCC)
    {
        if(!_in.Data.R || !_in.Data.G || !_in.Data.B) 
            return MFX_ERR_LOCK_MEMORY;
    }

    const mfxU32 w_block_size = _in.Info.Width  / W_BLOCKS;
    const mfxU32 h_block_size = _in.Info.Height / H_BLOCKS;
    const mfxU32 in_pitch = _in.Data.Pitch;
    const mfxU32 out_pitch = _out.Data.Pitch;

    tmpRect.NumRect = 0;

    mfxU16 curRect = 0;
    const mfxU32 maxRect = sizeof(tmpRect.Rect) / sizeof(tmpRect.Rect[0]) - 1;

    mfxU16 diffMap[16][16];
    memset(&diffMap,0,sizeof(diffMap));

    if(MFX_FOURCC_NV12 == _in.Info.FourCC)
    {
        for(mfxU32 h = 0; h < _in.Info.Height; h += h_block_size)
        {
            for(mfxU32 w = 0; w < _in.Info.Width; w += w_block_size)
            {
                for(mfxU32 i = 0; i < h_block_size; ++i)
                {
                    //Y
                    if(memcmp(_in.Data.Y + (h + i) * in_pitch + w, _out.Data.Y + (h + i) * out_pitch + w, w_block_size))
                    {
                        if(curRect < maxRect)
                        {
                            ++curRect;

                            tmpRect.Rect[curRect].Top = h;
                            tmpRect.Rect[curRect].Bottom = h + h_block_size;
                            tmpRect.Rect[curRect].Left = w;
                            tmpRect.Rect[curRect].Right = w + w_block_size;

                            diffMap[h/h_block_size][w/w_block_size] = 1;
                        }
                        break;
                    }
                    //UV
                    if(memcmp(_in.Data.UV + ((h + i) >> 1) * in_pitch + w, _out.Data.UV + ((h + i) >> 1) * out_pitch + w, w_block_size))
                    {
                        if(curRect < maxRect)
                        {
                            ++curRect;

                            tmpRect.Rect[curRect].Top = h;
                            tmpRect.Rect[curRect].Bottom = h + h_block_size;
                            tmpRect.Rect[curRect].Left = w;
                            tmpRect.Rect[curRect].Right = w + w_block_size;
                        }
                        break;
                    }
                }
                if(maxRect == curRect)
                    break;
            }
            if(maxRect == curRect)
                break;
        }
    }
    else if(MFX_FOURCC_AYUV_RGB4 == _in.Info.FourCC || MFX_FOURCC_RGB4 == _in.Info.FourCC || /*DXGI_FORMAT_AYUV*/100 == _in.Info.FourCC)
    {
        const mfxU8* ref_ptr = std::min( std::min(_in.Data.R, _in.Data.G), _in.Data.B );
        const mfxU8* cur_ptr = std::min( std::min(_out.Data.R, _out.Data.G), _out.Data.B );

        for(mfxU32 h = 0; h < _in.Info.Height; h += h_block_size)
        {
            for(mfxU32 w = 0; w < _in.Info.Width; w += w_block_size)
            {
                for(mfxU32 i = 0; i < h_block_size; ++i)
                {
                    //ARGB
                    if(memcmp(ref_ptr + (h + i) * in_pitch + 4*w, cur_ptr + (h + i) * out_pitch + 4*w, 4*w_block_size))
                    {
                        if(curRect < maxRect)
                        {
                            ++curRect;

                            tmpRect.Rect[curRect].Top = h;
                            tmpRect.Rect[curRect].Bottom = h + h_block_size;
                            tmpRect.Rect[curRect].Left = w;
                            tmpRect.Rect[curRect].Right = w + w_block_size;
                        }
                        break;
                    }
                }
                if(maxRect == curRect)
                    break;
            }
            if(maxRect == curRect)
                break;
        }
    }
    mfxRect->NumRect = (curRect) ? (curRect + 1) : (0);
    memcpy_s(&mfxRect->Rect, sizeof(mfxRect->Rect), &tmpRect.Rect, sizeof(tmpRect.Rect));

    if(unlock_in)
    {
        mfxRes = m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, in.Data.MemId, &_in.Data);
        if(mfxRes)
            return MFX_ERR_LOCK_MEMORY;
    }
    if(unlock_out)
    {
        mfxRes = m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, out.Data.MemId, &_out.Data);
        if(mfxRes)
            return MFX_ERR_LOCK_MEMORY;
    }

    return MFX_ERR_NONE;
}


mfxStatus CpuDirtyRectFilter::GetParam(mfxFrameInfo& in, mfxFrameInfo& out)
{
    in;
    out;

    return MFX_ERR_UNSUPPORTED;
}



} //namespace MfxCapture