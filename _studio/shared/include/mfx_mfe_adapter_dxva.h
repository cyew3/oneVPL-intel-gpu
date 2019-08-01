// Copyright (c) 2011-2019 Intel Corporation
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

#ifndef _MFX_MFE_ADAPTER_DXVA_
#define _MFX_MFE_ADAPTER_DXVA_

#if defined(MFX_VA_WIN) && defined(MFX_ENABLE_MFE) && !defined(AS_MFX_LA_PLUGIN) && defined (PRE_SI_TARGET_PLATFORM_GEN12P5)
#include <vector>
#include <list>
#include <condition_variable>
#include <mfxstructures.h>
#include "encoding_ddi.h"

struct tagENCODE_CAPS_HEVC;
using ENCODE_CAPS_HEVC = tagENCODE_CAPS_HEVC;

//#define DEBUG_TRACE
class MFEDXVAEncoder
{
    typedef void* CAPS;
    struct m_stream_ids_t
    {
        ENCODE_SINGLE_STREAM_INFO info;
        mfxStatus sts;
        unsigned long long timeout;
        mfxU32 restoreCount;
        mfxU32 restoreCountBase;
        bool      isSubmitted;
        m_stream_ids_t(const ENCODE_SINGLE_STREAM_INFO & _info,
                        mfxStatus _sts,
                        unsigned long long defaultTimeout):
        info(_info),
        sts(_sts),
        timeout(defaultTimeout),
        restoreCount(0),
        restoreCountBase(0),
        isSubmitted(false)
        {
        };
        inline void reset()
        {
            sts = MFX_ERR_NONE;
            restoreCount = restoreCountBase;
            isSubmitted = false;
        };
        inline mfxU32 getRestoreCount()
        {
            return restoreCount;
        };
        inline void updateRestoreCount()
        {
            restoreCount--;
        };
        inline void frameSubmitted()
        {
            isSubmitted = true;
        };
        inline bool isFrameSubmitted()
        {
            return isSubmitted;
        };
    };

public:
    MFEDXVAEncoder();

    virtual
        ~MFEDXVAEncoder();
    mfxStatus Create(ID3D11VideoDevice  *pVideoDevice,
                     ID3D11VideoContext *pVideoContext,
                     uint32_t            width,
                     uint32_t            height);

    mfxStatus Join(mfxExtMultiFrameParam const & par,
                   ENCODE_SINGLE_STREAM_INFO &info,
                   unsigned long long  timeout);
    mfxStatus Disjoin(const ENCODE_SINGLE_STREAM_INFO info);
    mfxStatus Destroy();
    //MSFT runtime restrict multiple contexts per device
    //so for DXVA MFE implementation the same context being used for encoder and MFE submission
    ID3D11VideoDecoder* GetVideoDecoder();
    mfxStatus Submit(const ENCODE_SINGLE_STREAM_INFO info, unsigned long long timeToWait, bool skipFrame);//time passed in vm_tick, so milliseconds to be multiplied by vm_frequency/1000

    //returns pointer to particular caps with only read access, NULL if caps not set.
    CAPS GetCaps(MFE_CODEC);

    virtual void AddRef();
    virtual void Release();

private:
    mfxStatus reconfigureRestorationCounts(ENCODE_SINGLE_STREAM_INFO &info);
    uint32_t      m_refCounter;

    std::condition_variable m_mfe_wait;
    std::mutex              m_mfe_guard;

    ID3D11VideoDevice*  m_pVideoDevice;
    ID3D11VideoContext* m_pVideoContext;
    ID3D11VideoDecoder* m_pMfeContext;

    // a pool (heap) of objects
    std::list<m_stream_ids_t> m_streams_pool;

    // a pool (heap) of objects
    std::list<m_stream_ids_t> m_submitted_pool;

    // a list of objects filled with context info ready to submit
    std::list<m_stream_ids_t> m_toSubmit;

    typedef std::list<m_stream_ids_t>::iterator StreamsIter_t;

    //A number of frames which will be submitted together (Combined)
    uint32_t m_framesToCombine;

    //A desired number of frames which might be submitted. if
    // actual number of sessions less then it,  m_framesToCombine
    // will be adjusted
    uint32_t m_maxFramesToCombine;

    // A counter frames collected for the next submission. These
    // frames will be submitted together either when get equal to
    // m_pipelineStreams or when collection timeout elapses.
    uint32_t m_framesCollected;

    //To save caps for different codecs and reuse - no need in cycling query each time
    std::unique_ptr<MFE_CAPS> m_MFE_CAPS;
    std::unique_ptr<ENCODE_CAPS> m_avcCAPS;
    std::unique_ptr<ENCODE_CAPS_HEVC> m_hevcCAPS;

#ifdef MFX_ENABLE_AV1_VIDEO_ENCODE
    std::unique_ptr<ENCODE_CAPS_AV1> m_av1CAPS;
#endif
    // We need contexts extracted from m_toSubmit to
    // place to a linear vector to pass them to vaMFSubmit
    std::vector<ENCODE_COMPBUFFERDESC> m_contexts;
    // store iterators to particular items
    std::vector<StreamsIter_t> m_streams;
    // store iterators to particular items
    std::map<uint32_t, StreamsIter_t> m_streamsMap;

    //minimal timeout of all streams
    unsigned long long m_minTimeToWait;
    static const uint32_t MAX_FRAMES_TO_COMBINE = 8;
};
#endif // MFX_VA_WIN && MFX_ENABLE_MFE

#endif /* _MFX_MFE_ADAPTER_DXVA */
