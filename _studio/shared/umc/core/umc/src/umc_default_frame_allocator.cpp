// Copyright (c) 2006-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_default_frame_allocator.h"
#include "umc_frame_data.h"
#include "mfx_utils.h"
#include <memory>

namespace UMC
{

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  FrameInformation class implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FrameInformation
{
public:

    FrameInformation();
    virtual ~FrameInformation();

    int32_t GetType() const;

    void ApplyMemory(uint8_t * ptr, size_t pitch);

    size_t CalculateSize(size_t pitch);

    void Reset();

private:

    int32_t m_locks;
    int32_t m_referenceCounter;
    FrameData  m_frame;
    uint8_t*  m_ptr;
    int32_t  m_type;
    int32_t  m_flags;

    friend class DefaultFrameAllocator;

private:
    // Declare private copy constructor to avoid accidental assignment
    // and klocwork complaining.
    FrameInformation(const FrameInformation &);
    FrameInformation & operator = (const FrameInformation &);
};

FrameInformation::FrameInformation()
    : m_locks(0)
    , m_referenceCounter(0)
    , m_frame()
    , m_ptr(0)
    , m_type(0)
    , m_flags(0)
{
}

FrameInformation::~FrameInformation()
{
    delete[] m_ptr;
}

void FrameInformation::Reset()
{
    m_referenceCounter = 0;
    m_locks = 0;
}

int32_t FrameInformation::GetType() const
{
    return m_type;
}

size_t FrameInformation::CalculateSize(size_t pitch)
{
    size_t sz = 0;

    const VideoDataInfo * info = m_frame.GetInfo();

    // set correct width & height to planes
    for (uint32_t i = 0; i < info->GetNumPlanes(); i += 1)
    {
        const VideoDataInfo::PlaneInfo * planeInfo = info->GetPlaneInfo(i);
        size_t planePitch = pitch * planeInfo->m_iSamples * planeInfo->m_iSampleSize >> planeInfo->m_iWidthScale;
        sz += planePitch* planeInfo->m_ippSize.height;
    }

    return sz + 128;
}

void FrameInformation::ApplyMemory(uint8_t * ptr, size_t pitch)
{
    const VideoDataInfo * info = m_frame.GetInfo();
    size_t offset = 0;

    uint8_t* ptr1 = align_pointer<uint8_t*>(ptr, 64);
    offset = ptr1 - ptr;

    // set correct width & height to planes
    for (uint32_t i = 0; i < info->GetNumPlanes(); i += 1)
    {
        const VideoDataInfo::PlaneInfo * planeInfo = info->GetPlaneInfo(i);
        size_t planePitch = pitch * planeInfo->m_iSamples * planeInfo->m_iSampleSize >> planeInfo->m_iWidthScale;
        m_frame.SetPlanePointer(ptr + offset, i, planePitch);
        offset += (planePitch) * planeInfo->m_ippSize.height;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  DefaultFrameAllocator class implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DefaultFrameAllocator::DefaultFrameAllocator()
{
    Init(0);
}

DefaultFrameAllocator::~DefaultFrameAllocator()
{
    Close();
}

Status DefaultFrameAllocator::Init(FrameAllocatorParams* /*pParams*/)
{
    AutomaticUMCMutex guard(m_guard);

    Close();
    return UMC_OK;
}

Status DefaultFrameAllocator::Close()
{
    AutomaticUMCMutex guard(m_guard);

    std::vector<FrameInformation *>::iterator iter = m_frames.begin();
    for (; iter != m_frames.end(); ++iter)
    {
        FrameInformation * info = *iter;
        delete info;
    }

    m_frames.clear();

    return UMC_OK;
}

Status DefaultFrameAllocator::Reset()
{
    AutomaticUMCMutex guard(m_guard);

    std::vector<FrameInformation *>::iterator iter = m_frames.begin();
    for (; iter != m_frames.end(); ++iter)
    {
        FrameInformation * info = *iter;
        info->Reset();
    }

    return UMC_OK;
}

Status DefaultFrameAllocator::Alloc(FrameMemID *pNewMemID, const VideoDataInfo * frameInfo, uint32_t flags)
{
    FrameMemID idx = 0;
    if (!pNewMemID || !frameInfo)
        return UMC_ERR_NULL_PTR;

    AutomaticUMCMutex guard(m_guard);

    std::vector<FrameInformation *>::iterator iter = m_frames.begin();
    for (; iter != m_frames.end(); ++iter)
    {
        FrameInformation * info = *iter;
        if (!info->m_referenceCounter)
        {
            info->m_referenceCounter++;
            *pNewMemID = idx;
            return UMC_OK;
        }
        idx++;
    }

    std::unique_ptr<FrameInformation> frameMID(new FrameInformation());

    frameMID->m_frame.Init(frameInfo, idx, this);

    size_t pitch = mfx::align2_value(frameInfo->GetWidth(), 64);
    size_t size  = frameMID->CalculateSize(pitch);

    frameMID->m_ptr = new uint8_t[size];
    frameMID->m_flags = flags;

    uint8_t *ptr = frameMID->m_ptr;
    frameMID->ApplyMemory(ptr, pitch);

    frameMID->m_referenceCounter = 1;

    FrameInformation *p = frameMID.release();
    m_frames.push_back(p);

    *pNewMemID = (FrameMemID)m_frames.size() - 1;

    return UMC_OK;
}

Status DefaultFrameAllocator::GetFrameHandle(UMC::FrameMemID , void * )
{
    return UMC_ERR_UNSUPPORTED;
}

const FrameData* DefaultFrameAllocator::Lock(FrameMemID mid)
{
    AutomaticUMCMutex guard(m_guard);
    if (mid >= (FrameMemID)m_frames.size())
        return NULL;

    FrameInformation * frameMID = m_frames[mid];
    frameMID->m_locks++;
    return &frameMID->m_frame;
}

Status DefaultFrameAllocator::Unlock(FrameMemID mid)
{
    AutomaticUMCMutex guard(m_guard);
    if (mid >= (FrameMemID)m_frames.size())
        return UMC_ERR_FAILED;

    FrameInformation * frameMID = m_frames[mid];
    frameMID->m_locks--;
    return UMC_OK;
}

Status DefaultFrameAllocator::IncreaseReference(FrameMemID mid)
{
    AutomaticUMCMutex guard(m_guard);
    if (mid >= (FrameMemID)m_frames.size())
        return UMC_ERR_FAILED;

     FrameInformation * frameMID = m_frames[mid];

    frameMID->m_referenceCounter++;

    return UMC_OK;
}

Status DefaultFrameAllocator::DecreaseReference(FrameMemID mid)
{
    AutomaticUMCMutex guard(m_guard);

    if (mid >= (FrameMemID)m_frames.size())
        return UMC_ERR_FAILED;

    FrameInformation * frameMID = m_frames[mid];

    frameMID->m_referenceCounter--;
    if (frameMID->m_referenceCounter == 1)
    {
        if (frameMID->m_locks)
        {
            frameMID->m_referenceCounter++;
            return UMC_ERR_FAILED;
        }

        return Free(mid);
    }

    return UMC_OK;
}

Status DefaultFrameAllocator::Free(FrameMemID mid)
{
    if (mid >= (FrameMemID)m_frames.size())
        return UMC_ERR_FAILED;

    FrameInformation * frameMID = m_frames[mid];
    //delete frameMID;
    frameMID->m_referenceCounter = 0;
    return UMC_OK;
}


} // namespace UMC
