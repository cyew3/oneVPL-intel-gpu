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

#include "mfx_common.h"

#if defined(MFX_VA_WIN) && defined(MFX_ENABLE_MFE) && defined (PRE_SI_TARGET_PLATFORM_GEN12P5)

#include "hevce_ddi_main.h"
#include "mfx_mfe_adapter_dxva.h"
#include "vm_interlocked.h"
#include <assert.h>
#include <iterator>
#include "mfx_h264_encode_d3d11.h"

#define CTX(dpy) (((VADisplayContextP)dpy)->pDriverContext)


MFEDXVAEncoder::MFEDXVAEncoder() :
      m_refCounter(1)
    , m_mfe_wait()
    , m_mfe_guard()
    , m_pVideoDevice(nullptr)
    , m_pVideoContext(nullptr)
    , m_pMfeContext(nullptr)
    , m_framesToCombine(0)
    , m_maxFramesToCombine(MAX_FRAMES_TO_COMBINE)
    , m_framesCollected(0)
    , m_minTimeToWait(0)
{
    m_streams.reserve(m_maxFramesToCombine);
}

ID3D11VideoDecoder* MFEDXVAEncoder::GetVideoDecoder()
{
    return m_pMfeContext;
}

MFEDXVAEncoder::~MFEDXVAEncoder()
{
    Destroy();
}

void MFEDXVAEncoder::AddRef()
{
    vm_interlocked_inc32(&m_refCounter);
}

void MFEDXVAEncoder::Release()
{
    vm_interlocked_dec32(&m_refCounter);

    if (0 == m_refCounter)
        delete this;
}

MFEDXVAEncoder::CAPS MFEDXVAEncoder::GetCaps(MFE_CODEC codec)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MFEDXVAEncoder::GetCaps");

    // Two steps of MFE Caps Query:
    // [1] First with CODEC_MFE
    if (!m_MFE_CAPS)
    {
        m_MFE_CAPS.reset(new MFE_CAPS);

        MFE_CODEC mfeCodec = CODEC_MFE;
        D3D11_VIDEO_DECODER_EXTENSION ext = { ENCODE_QUERY_ACCEL_CAPS_ID, &mfeCodec, sizeof(mfeCodec), m_MFE_CAPS.get(), sizeof(MFE_CAPS) };
        HRESULT hr = DecoderExtension(m_pVideoContext, m_pMfeContext, ext);
        if (FAILED(hr))
            return nullptr;
        // TODO: curently we don't handle MFE_CAPS the right way, just querying. Need to add support.
    }

    // [2] Second with specific MFE CodecID
    // Below we cache query caps results
    MFEDXVAEncoder::CAPS capsPtr = nullptr;
    UINT capsSize = 0;
    switch (codec)
    {
    case CODEC_AVC:
        if (m_avcCAPS)
            return m_avcCAPS.get();
        m_avcCAPS.reset(new ENCODE_CAPS);
        capsPtr = m_avcCAPS.get();
        capsSize = sizeof(ENCODE_CAPS);
        break;
    case CODEC_HEVC:
        if (m_hevcCAPS)
            return m_hevcCAPS.get();
        m_hevcCAPS.reset(new ENCODE_CAPS_HEVC);
        capsPtr = m_hevcCAPS.get();
        capsSize = sizeof(ENCODE_CAPS_HEVC);
        break;
#ifdef MFX_ENABLE_AV1_VIDEO_ENCODE
    case CODEC_AV1:
        if (m_av1CAPS)
            return m_av1CAPS.get();
        m_av1CAPS.reset(new ENCODE_CAPS_AV1);
        capsPtr = m_av1CAPS.get();
        capsSize = sizeof(ENCODE_CAPS_AV1);
        break;
#endif
    default:
        break;
    };

    D3D11_VIDEO_DECODER_EXTENSION ext = { ENCODE_QUERY_ACCEL_CAPS_ID, &codec, sizeof(codec), capsPtr, capsSize };
    HRESULT hr = DecoderExtension(m_pVideoContext, m_pMfeContext, ext);
    if (FAILED(hr))
        return nullptr;

    return capsPtr;
}

mfxStatus MFEDXVAEncoder::Create(ID3D11VideoDevice *pVideoDevice,
                                 ID3D11VideoContext *pVideoContext,
                                 uint32_t width,
                                 uint32_t height)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MFEDXVAEncoder::Create");
    assert(pVideoDevice);
    assert(pVideoContext);

    /*if(par.MaxNumFrames == 1)
        return MFX_ERR_UNDEFINED_BEHAVIOR;//encoder should not create MFE for single frame as nothing to be combined.*/
    //Comparing to Linux implemetation we don't have frame amount verification here, but check it in Join implementation
    //ToDo - align linux to the same.
    if (nullptr != m_pMfeContext)
    {
        //TMP WA for SKL due to number of frames limitation in different scenarios:
        //to simplify submission process and not add additional checks for frame wait depending on input parameters
        //if there are encoder want to run less frames within the same MFE Adapter(parent session) align to less now
        //in general just if someone set 3, but another one set 2 after that(or we need to decrease due to parameters) - use 2 for all.
        return MFX_ERR_NONE;
    }
    D3D11_VIDEO_DECODER_DESC video_desc;
    D3D11_VIDEO_DECODER_CONFIG video_config = {0};
    m_pVideoDevice = pVideoDevice;
    m_pVideoContext = pVideoContext;

    m_streams_pool.clear();
    m_toSubmit.clear();

    UINT profileCount = m_pVideoDevice->GetVideoDecoderProfileCount();
    assert(profileCount > 0);

    HRESULT hr = S_OK;
    bool isFound = false;
    GUID profileGuid;
    for (UINT indxProfile = 0; indxProfile < profileCount; indxProfile++)
    {
        hr = m_pVideoDevice->GetVideoDecoderProfile(indxProfile, &profileGuid);
        if (hr != S_OK)
            return MFX_ERR_DEVICE_FAILED;
        if (DXVA2_Intel_MFE == profileGuid)
        {
            isFound = true;
            break;
        }
    }

    if (!isFound)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    video_desc.SampleWidth  = width;
    video_desc.SampleHeight = height;//current Gen12lp support only same resolution combining, so take initialization size from parameters, later we need to set max resolution for MFE.
    video_desc.OutputFormat = DXGI_FORMAT_NV12;
    video_desc.Guid = DXVA2_Intel_MFE;

    video_config.ConfigDecoderSpecific = ENCODE_ENC_PAK;
    video_config.guidConfigBitstreamEncryption = DXVA_NoEncrypt;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "CreateMFEDevice");
        hr = m_pVideoDevice->CreateVideoDecoder(&video_desc, &video_config, &m_pMfeContext);

        if (hr != S_OK)
        {
            return MFX_ERR_DEVICE_FAILED;
        }
    }
    return MFX_ERR_NONE;
}
mfxStatus MFEDXVAEncoder::reconfigureRestorationCounts(ENCODE_SINGLE_STREAM_INFO &info)
{
    //used in Join function which already covered by mutex
    //accurate calculation should involve float calculation for different framerates
    //and base divisor for all the framerates in pipeline, so far simple cases are using
    //straight forward framerates with integer calculation enough to cover engines load and AVG latency for SKL.
    auto iter = m_streamsMap.find(info.StreamId);
    if(iter == m_streamsMap.end())
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if(m_streamsMap.size()==1)
    {
        //if only one stream - restore every submission
        iter->second->restoreCount = iter->second->restoreCountBase = 1;
        m_minTimeToWait = iter->second->timeout;

        return MFX_ERR_NONE;
    }
    if(iter->second->timeout >= m_minTimeToWait)
    {
        //can be problematic for cases like 24/30, but more like an exception
        iter->second->restoreCount = iter->second->restoreCountBase = (mfxU32)(iter->second->timeout/m_minTimeToWait);

        return MFX_ERR_NONE;
    }
    m_minTimeToWait = iter->second->timeout;
    iter->second->restoreCount = iter->second->restoreCountBase = 1;
    for (auto it = m_streamsMap.begin(); it != m_streamsMap.end();it++)
    {
        if (iter != it)
        {
            it->second->restoreCount = it->second->restoreCountBase = (mfxU32)(it->second->timeout/m_minTimeToWait);
        }
    }
    return MFX_ERR_NONE;
}
mfxStatus MFEDXVAEncoder::Join(const mfxExtMultiFrameParam  & par,
                               ENCODE_SINGLE_STREAM_INFO &info,
                               unsigned long long timeout)
{
    std::lock_guard<std::mutex> guard(m_mfe_guard);//need to protect in case there are streams added/removed in runtime.
    //no specific function needed for DXVA implementation to join stream to MFE
    //stream being added during BeginFrame call.
    StreamsIter_t iter;
    // append the pool with a new item;
    // Adapter requires to assign stream Id here for encoder.
    if (nullptr != m_pMfeContext)
    {
        // To simplify submission process and not add additional checks for frame wait depending on input parameters.
        // If there are encoder want to run less frames within the same MFE Adapter(parent session) then align to less.
        // In general if someone set 3, but another one set 2, after that(or we need to decrease due to parameters) - use 2 for all.
        m_maxFramesToCombine = m_maxFramesToCombine > par.MaxNumFrames ? par.MaxNumFrames : m_maxFramesToCombine;
    }
    else
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
    if (info.StreamId != 0)
    {
        assert(info.StreamId);
        return MFX_ERR_UNDEFINED_BEHAVIOR;//something went wrong and we got non zero StreamID from encoder;
    }
    info.StreamId = (int)m_streams_pool.size();
    for (iter = m_streams_pool.begin(); iter != m_streams_pool.end(); iter++)
    {
        //if we get into uint32_t max - fail now(assume we never get so big amount of streams reallocation in one process session)
        //ToDo for fix: start from the beggining and search m_streamsMap, if not found - assign non existing StreamID.
        if (iter->info.StreamId == 0xFFFFFFFF)
            return MFX_ERR_MEMORY_ALLOC;
        else if (iter->info.StreamId >= info.StreamId)
            info.StreamId = iter->info.StreamId+1;
    }
    m_streams_pool.emplace_back(m_stream_ids_t(info, MFX_ERR_NONE, timeout));
    iter = m_streams_pool.end();
    m_streamsMap.insert(std::pair<uint32_t, StreamsIter_t>(info.StreamId,--iter));
    // to deal with the situation when a number of sessions < requested
    if (m_framesToCombine < m_maxFramesToCombine)
        ++m_framesToCombine;
    //new stream can come with new framerate, possibly need to change existing streams
    //restoration counts to align with new stream
    mfxStatus sts = reconfigureRestorationCounts(info);
    return sts;
}

mfxStatus MFEDXVAEncoder::Disjoin(const ENCODE_SINGLE_STREAM_INFO & info)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MFEDXVAEncoder::Disjoin");
    std::lock_guard<std::mutex> guard(m_mfe_guard);//need to protect in case there are streams added/removed in runtime
    auto iter = m_streamsMap.find(info.StreamId);
    if (iter == m_streamsMap.end())
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    ENCODE_SINGLE_STREAM_INFO _info = info;
    if(iter == m_streamsMap.end())
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    ENCODE_COMPBUFFERDESC  encodeCompBufferDesc = {};

    encodeCompBufferDesc.CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_STREAMINFO);
    encodeCompBufferDesc.DataSize             = uint32_t(sizeof ENCODE_SINGLE_STREAM_INFO);
    encodeCompBufferDesc.pCompBuffer          = &_info;

    ENCODE_EXECUTE_PARAMS encodeExecuteParams = {};
    encodeExecuteParams.NumCompBuffers = 1;
    encodeExecuteParams.pCompressedBuffers = &encodeCompBufferDesc;

    D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = {};
    decoderExtParams.Function              = ENCODE_MFE_END_STREAM_ID;
    decoderExtParams.pPrivateInputData     = &encodeExecuteParams;
    decoderExtParams.PrivateInputDataSize  = sizeof(ENCODE_EXECUTE_PARAMS);
    decoderExtParams.pPrivateOutputData    = 0;
    decoderExtParams.PrivateOutputDataSize = 0;
    decoderExtParams.ResourceCount         = 0;
    decoderExtParams.ppResourceList        = 0;
    HRESULT hr;
    hr = DecoderExtension(m_pVideoContext, m_pMfeContext, decoderExtParams);
    if (hr != S_OK)
        return MFX_ERR_DEVICE_FAILED;

    if (hr != S_OK)
        return MFX_ERR_DEVICE_FAILED;

    m_streams_pool.erase(iter->second);
    m_streamsMap.erase(iter);
    if (m_framesToCombine > 0 && m_framesToCombine >= m_maxFramesToCombine)
        --m_framesToCombine;
    return MFX_ERR_NONE;
}

mfxStatus MFEDXVAEncoder::Destroy()
{
    std::lock_guard<std::mutex> guard(m_mfe_guard);
    m_streams_pool.clear();
    m_streamsMap.clear();
    SAFE_RELEASE(m_pMfeContext);
    m_pMfeContext = nullptr;
    m_MFE_CAPS.reset();
    m_avcCAPS.reset();
    m_hevcCAPS.reset();
#ifdef MFX_ENABLE_AV1_VIDEO_ENCODE
    m_av1CAPS.reset();
#endif
    return MFX_ERR_NONE;
}

mfxStatus MFEDXVAEncoder::Submit(const uint32_t streamId, unsigned long long timeToWait, bool skipFrame)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MFEDXVAEncoder::Submit");
    std::unique_lock<std::mutex> guard(m_mfe_guard);
    //stream in pool corresponding to particular context;
    StreamsIter_t cur_stream;
    bool curStreamSubmitted = false;//current stream submitted frames
    //we can wait for less frames than expected by pipeline due to avilability.
    uint32_t framesToSubmit = m_framesToCombine;
    if (m_streams_pool.empty())
    {
        //if current stream came to empty pool - others already submitted or in process of submission
        //start pool from the beggining
        for (std::list<m_stream_ids_t>::iterator it = m_submitted_pool.begin(); it != m_submitted_pool.end(); it++)
        {
            it->updateRestoreCount();
        }
        std::list<m_stream_ids_t>::iterator it = m_submitted_pool.begin();
        while (it != m_submitted_pool.end())
        {
            for (it = m_submitted_pool.begin(); it != m_submitted_pool.end(); it++)
            {
                if (it->getRestoreCount() == 0 || it->info.StreamId == streamId)
                {
                    it->reset();
                    m_streams_pool.splice(m_streams_pool.end(), m_submitted_pool, it);
                    it = m_submitted_pool.begin();
                    break;
                }
            }
        }
    }
    if (m_streams_pool.empty())
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    //get current stream by info
    auto iter = m_streamsMap.find(streamId);
    if (iter == m_streamsMap.end())
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    cur_stream = iter->second;
    if (!cur_stream->isFrameSubmitted())
    {
        //if current info is not submitted - it is in stream pull
        m_toSubmit.splice(m_toSubmit.end(), m_streams_pool, cur_stream);
    }
    else
    {
        //if current stream submitted - it is in submitted pool
        //in this case mean current stream submitting it's frames faster then others,
        //at least for 2 reasons:
        //1 - most likely: frame rate is higher
        //2 - amount of streams big enough so first stream submit next frame earlier than last('s)
        //submit current
        //take it back from submitted pull
        m_toSubmit.splice(m_toSubmit.end(), m_submitted_pool, cur_stream);
        cur_stream->reset(); //cleanup stream state
    }
    if (skipFrame)
    {
        //if frame is skipped - threat it as submitted without real submission
        //to not lock other streams waiting for it
        m_submitted_pool.splice(m_submitted_pool.end(), m_toSubmit, cur_stream);
        cur_stream->frameSubmitted();
        return MFX_ERR_NONE;
    }
    ++m_framesCollected;
    if (m_streams_pool.empty())
    {
        //if streams are over in a pool - submit available frames
        //such approach helps to fix situation when we got remainder
        //less than number m_framesToCombine, so current stream does not
        //wait to get frames from other encoders in next cycle
        if (m_framesCollected < framesToSubmit)
            framesToSubmit = m_framesCollected;
    }

    if (!framesToSubmit)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    m_mfe_wait.wait_for(guard, std::chrono::microseconds(timeToWait), [this, framesToSubmit, cur_stream]{
        return ((m_framesCollected >= framesToSubmit) || cur_stream->isFrameSubmitted());
    });

    if (!cur_stream->isFrameSubmitted())
    {
        if (!m_framesCollected)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        for (auto it = m_toSubmit.begin(); it != m_toSubmit.end(); ++it)
        {
            if (!it->isFrameSubmitted())
            {
                m_streams.emplace_back(it);
            }
        }
        if (m_framesCollected != m_streams.size() || m_streams.empty())
        {
            for (auto it = m_streams.begin(); it != m_streams.end(); ++it)
            {
                (*it)->sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
            // if cur_stream is not in m_streams (somehow)
            cur_stream->sts = MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        else
        {
            HRESULT hr;
            D3D11_VIDEO_DECODER_EXTENSION extParams = {};
            extParams.Function = ENCODE_MFE_START_ID;

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "ENCODE_MFE_START_ID");
                try
                {
                    hr = DecoderExtension(m_pVideoContext, m_pMfeContext, extParams);
                }
                catch (...)
                {
                    hr = E_FAIL;
                }
            }

            mfxStatus tmp_res = MFX_ERR_NONE;
            if(S_OK != hr)
                tmp_res = MFX_ERR_DEVICE_FAILED;
            for (auto it = m_streams.begin(); it != m_streams.end(); ++it)
            {
                (*it)->frameSubmitted();
                (*it)->sts = tmp_res;
            }
            m_framesCollected -= (uint32_t)m_streams.size();
        }
        // Broadcast is done before unlock for this case to simplify the code avoiding extra ifs

        m_streams.clear();
        curStreamSubmitted = true;
    }

    if(cur_stream->isFrameSubmitted())
    {
        m_submitted_pool.splice(m_submitted_pool.end(), m_toSubmit, cur_stream);
    }
    // This frame can be already submitted or errored
    // put it into submitted pool, release mutex and exit
    mfxStatus res = cur_stream->sts;
    if (curStreamSubmitted)
    {
        m_mfe_wait.notify_all();
    }
    return res;
}

#endif //MFX_VA_WIN && MFX_ENABLE_MFE
