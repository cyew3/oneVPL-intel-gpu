//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015 Intel Corporation. All Rights Reserved.
//

#include "mfx_screen_capture_dirty_rect.h"
#include "mfx_utils.h"
#include <memory>
#include <list>
#include <algorithm> //min

#define H_BLOCKS 16
#define W_BLOCKS 16

#if !defined(MSDK_ALIGN32)
#define MSDK_ALIGN32(value)                      (((value + 31) >> 5) << 5) // round up to a multiple of 32
#endif

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

CpuDirtyRectFilter::CpuDirtyRectFilter(const mfxCoreInterface* _core)
    :m_pmfxCore(_core)
{
    Mode = SW_DR;
    m_bSysMem = false;
    m_bOpaqMem = false;
}

CpuDirtyRectFilter::~CpuDirtyRectFilter()
{
    FreeSurfs();
}

mfxStatus CpuDirtyRectFilter::Init(const mfxVideoParam* par, bool isSysMem, bool isOpaque)
{
    m_bSysMem = isSysMem;
    m_bOpaqMem = isOpaque;
    if( !isSysMem && par )
    {
        //Create own surface pool
        try{
            AllocSurfs(par);
        } catch (std::bad_alloc&) {
            FreeSurfs();
            return MFX_ERR_MEMORY_ALLOC;
        }
    }
    return MFX_ERR_NONE;
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

    bool release_in = false;
    bool unlock_in = false;
    bool release_out = false;
    bool unlock_out = false;
    mfxFrameSurface1 *inSurf  = GetSysSurface(&in, release_in, unlock_in, m_InSurfPool);
    mfxFrameSurface1 *outSurf  = GetSysSurface(&out, release_out, unlock_out, m_OutSurfPool);
    if(!inSurf || !outSurf)
        return MFX_ERR_DEVICE_FAILED;

    if(MFX_FOURCC_NV12 == inSurf->Info.FourCC)
    {
        if(!inSurf->Data.Y || !inSurf->Data.UV   ||   !outSurf->Data.Y || !outSurf->Data.UV) 
            return MFX_ERR_LOCK_MEMORY;
    }
    if(MFX_FOURCC_AYUV_RGB4 == inSurf->Info.FourCC || MFX_FOURCC_RGB4 == inSurf->Info.FourCC || /*DXGI_FORMAT_AYUV*/100 == inSurf->Info.FourCC)
    {
        if(!inSurf->Data.R || !inSurf->Data.G || !inSurf->Data.B   ||   !outSurf->Data.R || !outSurf->Data.G || !outSurf->Data.B) 
            return MFX_ERR_LOCK_MEMORY;
    }

    const mfxU16 width  = inSurf->Info.CropW ? inSurf->Info.CropW : inSurf->Info.Width;
    const mfxU16 height = inSurf->Info.CropH ? inSurf->Info.CropH : inSurf->Info.Height;

    const mfxU32 w_block_size = width  / W_BLOCKS;
    const mfxU32 h_block_size = height / H_BLOCKS;
    const mfxU32 in_pitch = inSurf->Data.Pitch;
    const mfxU32 out_pitch = outSurf->Data.Pitch;

    tmpRect.NumRect = 0;
    mfxU16 curRect = 0;
    const mfxU32 maxRect = sizeof(tmpRect.Rect) / sizeof(tmpRect.Rect[0]) - 1;

    mfxU8 diffMap[16][16];
    memset(&diffMap,0,sizeof(diffMap));

    if(MFX_FOURCC_NV12 == inSurf->Info.FourCC)
    {
        for(mfxU32 h = 0; h < height; h += h_block_size)
        {
            for(mfxU32 w = 0; w < width; w += IPP_MIN(w_block_size,width - w) )
            {
                for(mfxU32 i = 0; i < IPP_MIN(h_block_size,height-h); ++i)
                {
                    //Y
                    if(memcmp(inSurf->Data.Y + (h + i) * in_pitch + w, outSurf->Data.Y + (h + i) * out_pitch + w, w_block_size))
                    {
                        if(curRect < maxRect)
                        {
                            tmpRect.Rect[curRect].Top = h;
                            tmpRect.Rect[curRect].Bottom = h + IPP_MIN(h_block_size,height-h);
                            tmpRect.Rect[curRect].Left = w;
                            tmpRect.Rect[curRect].Right = w + w_block_size;
                            ++curRect;
                        }
                        break;
                    }
                    //UV
                    if(memcmp(inSurf->Data.UV + ((h + i) >> 1) * in_pitch + w, outSurf->Data.UV + ((h + i) >> 1) * out_pitch + w, w_block_size))
                    {
                        if(curRect < maxRect)
                        {
                            tmpRect.Rect[curRect].Top = h;
                            tmpRect.Rect[curRect].Bottom = h + h_block_size;
                            tmpRect.Rect[curRect].Left = w;
                            tmpRect.Rect[curRect].Right = w + w_block_size;
                            ++curRect;
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
    else if(MFX_FOURCC_AYUV_RGB4 == inSurf->Info.FourCC || MFX_FOURCC_RGB4 == inSurf->Info.FourCC || /*DXGI_FORMAT_AYUV*/100 == inSurf->Info.FourCC)
    {
        const mfxU8* ref_ptr = (std::min)( (std::min)(inSurf->Data.R, inSurf->Data.G), inSurf->Data.B );
        const mfxU8* cur_ptr = (std::min)( (std::min)(outSurf->Data.R, outSurf->Data.G), outSurf->Data.B );

        for(mfxU32 h = 0; h < height; h += h_block_size)
        {
            for(mfxU32 w = 0; w < width; w += IPP_MIN(w_block_size,width - w))
            {
                for(mfxU32 i = 0; i < IPP_MIN(h_block_size,height-h); ++i)
                {
                    //ARGB
                    if(memcmp(ref_ptr + (h + i) * in_pitch + 4*w, cur_ptr + (h + i) * out_pitch + 4*w, 4*w_block_size))
                    {
                        if(curRect < maxRect)
                        {
                            tmpRect.Rect[curRect].Top = h;
                            tmpRect.Rect[curRect].Bottom = h + IPP_MIN(h_block_size,height-h);
                            tmpRect.Rect[curRect].Left = w;
                            tmpRect.Rect[curRect].Right = w + w_block_size;
                            ++curRect;
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

    mfxRect->NumRect = curRect;
    memcpy_s(&mfxRect->Rect, sizeof(mfxRect->Rect), &tmpRect.Rect, sizeof(tmpRect.Rect));

    if(unlock_in)
    {
        mfxRes = m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, inSurf->Data.MemId, &inSurf->Data);
        if(mfxRes)
            return MFX_ERR_LOCK_MEMORY;
    }
    if(unlock_out)
    {
        mfxRes = m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, outSurf->Data.MemId, &outSurf->Data);
        if(mfxRes)
            return MFX_ERR_LOCK_MEMORY;
    }
    if(release_in)
        ReleaseSurf(inSurf);
    if(release_out)
        ReleaseSurf(outSurf);

    return MFX_ERR_NONE;
}

mfxStatus CpuDirtyRectFilter::GetParam(mfxFrameInfo& in, mfxFrameInfo& out)
{
    in;
    out;

    return MFX_ERR_UNSUPPORTED;
}

mfxFrameSurface1* CpuDirtyRectFilter::AllocSurfs(const mfxVideoParam* par)
{
    UMC::AutomaticUMCMutex guard(m_guard);
    if(par)
    {
        //Create own surface pool
        for(mfxU16 i = 0; i < (std::max)(1,(int)par->AsyncDepth); ++i)
        {
            mfxFrameSurface1 inSurf;
            mfxFrameSurface1 outSurf;
            memset(&inSurf,0,sizeof(inSurf));

            inSurf.Info = par->mfx.FrameInfo;
            inSurf.Info.Width = MSDK_ALIGN32(inSurf.Info.Width);
            inSurf.Info.Height = MSDK_ALIGN32(inSurf.Info.Height);

            outSurf = inSurf;
            if(MFX_FOURCC_NV12 == inSurf.Info.FourCC)
            {
                inSurf.Data.Y = new mfxU8[inSurf.Info.Width * inSurf.Info.Height * 3 / 2];
                inSurf.Data.UV = inSurf.Data.Y + inSurf.Info.Width * inSurf.Info.Height;
                inSurf.Data.Pitch = inSurf.Info.Width;
            }
            else if(MFX_FOURCC_RGB4 == inSurf.Info.FourCC || 
                    MFX_FOURCC_AYUV_RGB4 == inSurf.Info.FourCC || 
                    /*DXGI_FORMAT_AYUV*/100 == inSurf.Info.FourCC)
            {
                inSurf.Data.B = new mfxU8[inSurf.Info.Width * inSurf.Info.Height * 4];
                inSurf.Data.G = inSurf.Data.B + 1;
                inSurf.Data.R = inSurf.Data.B + 2;
                inSurf.Data.A = inSurf.Data.B + 3;

                inSurf.Data.Pitch = 4*inSurf.Info.Width;
            }
            m_InSurfPool.push_back(inSurf);

            if(MFX_FOURCC_NV12 == outSurf.Info.FourCC)
            {
                outSurf.Data.Y = new mfxU8[outSurf.Info.Width * outSurf.Info.Height * 3 / 2];
                outSurf.Data.UV = outSurf.Data.Y + outSurf.Info.Width * outSurf.Info.Height;
                outSurf.Data.Pitch = outSurf.Info.Width;
            }
            else if(MFX_FOURCC_RGB4 == outSurf.Info.FourCC || 
                    MFX_FOURCC_AYUV_RGB4 == outSurf.Info.FourCC || 
                    /*DXGI_FORMAT_AYUV*/100 == outSurf.Info.FourCC)
            {
                outSurf.Data.B = new mfxU8[outSurf.Info.Width * outSurf.Info.Height * 4];
                outSurf.Data.G = outSurf.Data.B + 1;
                outSurf.Data.R = outSurf.Data.B + 2;
                outSurf.Data.A = outSurf.Data.B + 3;

                outSurf.Data.Pitch = 4*outSurf.Info.Width;
            }
            m_OutSurfPool.push_back(outSurf);
        }
    }
    return 0;
}

void FreeSurfPool(std::list<mfxFrameSurface1>& surfPool)
{
    if(surfPool.size())
    {
        for(std::list<mfxFrameSurface1>::iterator it = surfPool.begin(); it != surfPool.end(); ++it)
        {
            if( MFX_FOURCC_NV12 == (*it).Info.FourCC )
            {
                if ((*it).Data.Y)
                {
                    delete ((*it).Data.Y);
                    (*it).Data.Y = 0;
                    (*it).Data.UV = 0;
                }
            }
            else if(MFX_FOURCC_RGB4 == (*it).Info.FourCC || 
                    MFX_FOURCC_AYUV_RGB4 == (*it).Info.FourCC || 
                    /*DXGI_FORMAT_AYUV*/100 == (*it).Info.FourCC)
            {
                if ((*it).Data.B)
                {
                    delete ((*it).Data.B);
                    (*it).Data.B = 0;
                    (*it).Data.G = 0;
                    (*it).Data.R = 0;
                    (*it).Data.A = 0;
                }
            }
        }

        surfPool.clear();
    }
}

void CpuDirtyRectFilter::FreeSurfs()
{
    UMC::AutomaticUMCMutex guard(m_guard);

    FreeSurfPool(m_InSurfPool);
    FreeSurfPool(m_OutSurfPool);
}

mfxFrameSurface1* CpuDirtyRectFilter::GetFreeSurf(std::list<mfxFrameSurface1>& lSurfs)
{
    mfxFrameSurface1* s_out;
    if(0 == lSurfs.size())
        return 0;
    else
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        for(std::list<mfxFrameSurface1>::iterator it = lSurfs.begin(); it != lSurfs.end(); ++it)
        {
            if(0 == (*it).Data.Locked)
            {
                s_out = &(*it);
                m_pmfxCore->IncreaseReference(m_pmfxCore->pthis,&s_out->Data);
                return s_out;
            }
        }
    }
    return 0;
}

void CpuDirtyRectFilter::ReleaseSurf(mfxFrameSurface1*& surf)
{
    if(!surf) return;
    m_pmfxCore->DecreaseReference(m_pmfxCore->pthis,&surf->Data);
}

mfxFrameSurface1* CpuDirtyRectFilter::GetSysSurface(const mfxFrameSurface1* surf, bool& release, bool& unlock, std::list<mfxFrameSurface1>& lSurfs)
{
    if(!surf)
        return 0;
    mfxFrameSurface1* realSurf = (mfxFrameSurface1*) surf;
    if(m_bOpaqMem)
    {
        mfxStatus mfxSts = m_pmfxCore->GetRealSurface(m_pmfxCore->pthis, (mfxFrameSurface1*) surf, &realSurf);
        if(MFX_ERR_NONE != mfxSts || 0 == realSurf)
            return 0;
    }
    if(MFX_FOURCC_NV12 == realSurf->Info.FourCC && realSurf->Data.Y && realSurf->Data.UV)
        return realSurf;
    else if(realSurf->Data.R && realSurf->Data.G && realSurf->Data.B)
        return realSurf;

    if(!realSurf->Data.Y && realSurf->Data.MemId)
    {
        mfxHDLPair hdl = {0,0};
        mfxStatus mfxRes = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, realSurf->Data.MemId, (mfxHDL*)&hdl);
        if(MFX_ERR_NONE == mfxRes && hdl.first)
        {
            //input is video surf, copy to internal or lock
            mfxFrameSurface1* _in = GetFreeSurf(lSurfs);
            if(!_in)
                return 0;

            mfxRes = m_pmfxCore->CopyFrame(m_pmfxCore->pthis, _in, realSurf);
            if(mfxRes)
            {
                ReleaseSurf(_in);

                mfxRes = m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, realSurf->Data.MemId, &realSurf->Data);
                if(mfxRes)
                    return 0;
                unlock = true;
                release = false;

                return realSurf;
            }
            else
            {
                unlock = false;
                release = true;

                return _in;
            }
        }
        else
        {
            //input is system, memId
            mfxRes = m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, realSurf->Data.MemId, &realSurf->Data);
            if(mfxRes)
                return 0;
            unlock = true;
            release = false;

            return realSurf;
        }
    }
    else
    {
        return realSurf;
    }

    //return 0;
}

} //namespace MfxCapture
