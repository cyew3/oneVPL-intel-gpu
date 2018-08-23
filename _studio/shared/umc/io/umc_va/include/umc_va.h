//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_VA_H__
#define __UMC_VA_H__

#include "umc_va_base.h"

#ifndef UMC_VA_LINUX  // get data from umc_va_linux.h now
#include "umc_automatic_mutex.h"
#include "umc_mutex.h"

#include <list>
#include <vector>
#include <algorithm>
using namespace std;

namespace UMC
{

class VACompBuffer : public UMCVACompBuffer
{
public:
    int32_t      frame;
    int32_t      NumOfItem; //number of items in buffer
    bool        locked;
    int32_t      index;
    int32_t      id;

    VACompBuffer(int32_t Type=-1, int32_t Frame=-1, int32_t Index=-1, void* Ptr=NULL, int32_t BufSize=0, int32_t Id=-1)
        : frame(Frame), index(Index), locked(false), id(Id), NumOfItem(1)
    {
        type = Type;
        BufferSize = BufSize;
        ptr = Ptr;
    }

    VACompBuffer(const VACompBuffer &copy)
    {
        *this = copy;
    }

    void SetNumOfItem(int32_t num) { NumOfItem = num; };

    bool operator==(const VACompBuffer& right) const
    {
        return (type==right.type) && (frame==right.frame || frame==-1 || right.frame==-1) && (index==right.index || index==-1 || right.index==-1);  //index
    }

    void    Lock() { locked = true; }
    void    UnLock() { locked = false; }
    bool    IsLocked() const { return locked; }

    class Allocate {
    public:
        void operator ( ) ( VACompBuffer & x )
        {
           x.ptr = malloc(x.BufferSize);
        }
    };

    class Free
    {
    public:
        void operator ( ) ( VACompBuffer & x )
        {
            free(x.ptr);
            x.ptr=NULL;
        }
    };

    class Release
    {
    public:
        void operator ( ) ( VACompBuffer & x )
        {
            x.locked=false;
        }
    };
};

class VideoAcceleratorExt : public VideoAccelerator
{
public:
    VideoAcceleratorExt(void)
        : m_NumOfFrameBuffers(0)
        , m_NumOfCompBuffersSet(4)
        , m_bTightPack(false)
        , m_bLongSliceControl(true)
        , m_bDeblockingBufferNeeded(false)
        , m_FrameBuffers(0)
    {
    };

    virtual ~VideoAcceleratorExt(void)
    {
        Close();
    };

    virtual Status    BeginFrame    (int32_t FrameBufIndex) = 0;
    UMCVAFrameBuffer* GetFrameBuffer(int index); // to get pointer to uncompressed buffer.
    virtual Status    Execute       (void) { return UMC_OK; };
    virtual Status    ReleaseBuffer (int32_t type)=0;
    virtual Status    EndFrame      (void);
    virtual Status    DisplayFrame  (int32_t /*index*/){ return UMC_OK; };

    bool IsTightPack() const
    {
        return m_bTightPack;
    }

    bool IsLongSliceControl() const
    {
        return m_bLongSliceControl;
    }

    bool IsDeblockingBufferNeeded() const
    {
        return m_bDeblockingBufferNeeded;
    }

    //GetCompBuffer function returns compressed buffer from cache if there is one in it or get buffer from HW and put
    //it to cache if there is not. All cached buffers will be released in EndFrame.
    virtual void* GetCompBuffer(int32_t buffer_type, UMCVACompBuffer **buf, int32_t size, int32_t index);

protected:
    virtual VACompBuffer GetCompBufferHW(int32_t type, int32_t size, int32_t index = -1)=0; //get buffer from HW

public:
    int32_t m_NumOfFrameBuffers;
    int32_t m_NumOfCompBuffersSet;

protected:
    bool    m_bTightPack;
    bool    m_bLongSliceControl;
    bool    m_bDeblockingBufferNeeded;
    list<VACompBuffer> m_CachedCompBuffers; //buffers acquired for current frame decoding
    UMCVAFrameBuffer *m_FrameBuffers;
    Mutex m_mGuard;
};

} // namespace UMC

#endif //#ifdef UMC_VA_LINUX

#endif // __UMC_VA_H__
