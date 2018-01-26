//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2018 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined(MFX_VA_WIN) && defined(MFX_ENABLE_MFE)

#include "mfx_mfe_adapter_dxva.h"
#include "vm_interlocked.h"
#include <assert.h>
#include <iterator>
#include "mfx_h264_encode_d3d11.h"

#ifndef MFX_CHECK_WITH_ASSERT
#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }
#endif

#define CTX(dpy) (((VADisplayContextP)dpy)->pDriverContext)


MFEDXVAEncoder::MFEDXVAEncoder() :
      m_refCounter(1)
    , m_pVideoDevice(NULL)
    , m_pMfeContext(NULL)
    , m_framesToCombine(0)
    , m_maxFramesToCombine(0)
    , m_framesCollected(0)
    , m_time_frequency(vm_time_get_frequency())
{
    vm_cond_set_invalid(&m_mfe_wait);
    vm_mutex_set_invalid(&m_mfe_guard);

    m_contexts.reserve(MAX_FRAMES_TO_COMBINE);
    m_streams.reserve(MAX_FRAMES_TO_COMBINE);
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

mfxStatus MFEDXVAEncoder::Create(ID3D11VideoDevice *pVideoDevice,
                                 ID3D11VideoContext *pVideoContext)
{
    assert(pVideoDevice);
    assert(pVideoContext);

    /*if(par.MaxNumFrames == 1)
        return MFX_ERR_UNDEFINED_BEHAVIOR;//encoder should not create MFE for single frame as nothing to be combined.*/
    //Comparing to Linux implemetation we don't have frame amount verification here, but check it in Join implementation
    //ToDo - align linux to the same.
    if (NULL != m_pMfeContext)
    {
        //vm_mutex_lock(&m_mfe_guard);
        //TMP WA for SKL due to number of frames limitation in different scenarios:
        //to simplify submission process and not add additional checks for frame wait depending on input parameters
        //if there are encoder want to run less frames within the same MFE Adapter(parent session) align to less now
        //in general just if someone set 3, but another one set 2 after that(or we need to decrease due to parameters) - use 2 for all.
        //m_maxFramesToCombine = m_maxFramesToCombine > par.MaxNumFrames ? par.MaxNumFrames : m_maxFramesToCombine;
        //vm_mutex_unlock(&m_mfe_guard);
        return MFX_ERR_NONE;
    }
    D3D11_VIDEO_DECODER_DESC video_desc;
    D3D11_VIDEO_DECODER_CONFIG video_config = {0};
    m_pVideoDevice = pVideoDevice;
    m_pVideoContext = pVideoContext;

    if (VM_OK != vm_mutex_init(&m_mfe_guard))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    if (VM_OK != vm_cond_init(&m_mfe_wait))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_maxFramesToCombine = MAX_FRAMES_TO_COMBINE;

    m_streams_pool.clear();
    m_toSubmit.clear();

    UINT profileCount = m_pVideoDevice->GetVideoDecoderProfileCount();
    assert(profileCount > 0);

    HRESULT hr;
    bool isFound = false;
    GUID profileGuid;
    for (UINT indxProfile = 0; indxProfile < profileCount; indxProfile++)
    {
        hr = m_pVideoDevice->GetVideoDecoderProfile(indxProfile, &profileGuid);
        if (hr != S_OK)
            return MFX_ERR_DEVICE_FAILED;
        if (DXVA2_Intel_MFE_AVC == profileGuid)
        {
            isFound = true;
            break;
        }
    }

    if (!isFound)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    video_desc.SampleWidth  = 4096;//hardcoded for AVC now
    video_desc.SampleHeight = 4096;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;
    video_desc.Guid = DXVA2_Intel_MFE_AVC;


    video_config.ConfigDecoderSpecific = ENCODE_ENC_PAK;
    video_config.guidConfigBitstreamEncryption = DXVA_NoEncrypt;
    
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "CreateMFEDevice");
        hr = m_pVideoDevice->CreateVideoDecoder(&video_desc, &video_config, &m_pMfeContext);

        if (hr != S_OK)
            return MFX_ERR_DEVICE_FAILED;
    }
    return MFX_ERR_NONE;
}

mfxStatus MFEDXVAEncoder::Join(mfxExtMultiFrameParam const & par,
                               ENCODE_MULTISTREAM_INFO &info,
                               bool doubleField)
{
    vm_mutex_lock(&m_mfe_guard);//need to protect in case there are streams added/removed in runtime.
    //no specific function needed for DXVA implementation to join stream to MFE
    //stream being added during BeginFrame call.
    StreamsIter_t iter;
    // append the pool with a new item;
    //Adapter requires to assign stream Id here for encoder.
    if (NULL != m_pMfeContext)
    {
        
        //TMP WA for SKL due to number of frames limitation in different scenarios:
        //to simplify submission process and not add additional checks for frame wait depending on input parameters
        //if there are encoder want to run less frames within the same MFE Adapter(parent session) align to less now
        //in general just if someone set 3, but another one set 2 after that(or we need to decrease due to parameters) - use 2 for all.
        m_maxFramesToCombine = m_maxFramesToCombine > par.MaxNumFrames ? par.MaxNumFrames : m_maxFramesToCombine;
    }
    else
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
    info.StreamId = (int)m_streams_pool.size();
    if (info.StreamId != 0)
    {
        assert(info.StreamId);
        return MFX_ERR_UNDEFINED_BEHAVIOR;//something went wrong and we got non zero StreamID from encoder;
    }
    for (iter = m_streams_pool.begin(); iter != m_streams_pool.end(); iter++)
    {
        //if we get into mfxU32 max - fail now(assume we never get so big amount of streams reallocation in one process session)
        //ToDo for fix: start from the beggining and search m_streamsMap, if not found - assign non existing StreamID.
        if (iter->info.StreamId == 0xFFFFFFFF)
            return MFX_ERR_MEMORY_ALLOC;
        else if (iter->info.StreamId >= info.StreamId)
            info.StreamId = iter->info.StreamId+1;
    }
    ENCODE_MULTISTREAM_INFO _info = info;
    m_streams_pool.push_back(m_stream_ids_t(_info, MFX_ERR_NONE, doubleField));
    iter = m_streams_pool.end();
    m_streamsMap.insert(std::pair<mfxU32, StreamsIter_t>(_info.StreamId,--iter));
    // to deal with the situation when a number of sessions < requested
    if (m_framesToCombine < m_maxFramesToCombine)
        ++m_framesToCombine;

    vm_mutex_unlock(&m_mfe_guard);
    return MFX_ERR_NONE;
}

mfxStatus MFEDXVAEncoder::Disjoin(ENCODE_MULTISTREAM_INFO info)
{
    vm_mutex_lock(&m_mfe_guard);//need to protect in case there are streams added/removed in runtime
    std::map<mfxU32, StreamsIter_t>::iterator iter = m_streamsMap.find(info.StreamId);
    //ToDo: Function call ENCODE_MFE_END_STREAM_ID 0x114 needed here.
    //VAStatus vaSts = vaMFReleaseContext(m_vaDisplay, m_pMfeContext, ctx);
    ENCODE_MULTISTREAM_INFO _info = info;
    if(iter == m_streamsMap.end())
    {
        vm_mutex_unlock(&m_mfe_guard);
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    ENCODE_COMPBUFFERDESC  encodeCompBufferDesc;
    memset(&encodeCompBufferDesc,0,sizeof(ENCODE_COMPBUFFERDESC));

    encodeCompBufferDesc.CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_MULTISTREAMS);
    encodeCompBufferDesc.DataSize             = mfxU32(sizeof ENCODE_MULTISTREAM_INFO);
    encodeCompBufferDesc.pCompBuffer          = &_info;

    ENCODE_EXECUTE_PARAMS encodeExecuteParams;
    memset(&encodeExecuteParams, 0, sizeof(ENCODE_EXECUTE_PARAMS));
    encodeExecuteParams.NumCompBuffers = 1;
    encodeExecuteParams.pCompressedBuffers = &encodeCompBufferDesc;

    D3D11_VIDEO_DECODER_EXTENSION decoderExtParams;
    memset(&decoderExtParams, 0, sizeof(D3D11_VIDEO_DECODER_EXTENSION));
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
    vm_mutex_unlock(&m_mfe_guard);
    return MFX_ERR_NONE;
}

mfxStatus MFEDXVAEncoder::Destroy()
{
    vm_mutex_lock(&m_mfe_guard);
    m_streams_pool.clear();
    m_streamsMap.clear();
    SAFE_RELEASE(m_pMfeContext);
    m_pMfeContext = NULL;
    vm_mutex_unlock(&m_mfe_guard);

    vm_mutex_destroy(&m_mfe_guard);
    vm_cond_destroy(&m_mfe_wait);

    return MFX_ERR_NONE;
}

mfxStatus MFEDXVAEncoder::Submit(ENCODE_MULTISTREAM_INFO info, vm_tick timeToWait)
{
    vm_mutex_lock(&m_mfe_guard);
    //stream in pool corresponding to particular context;
    StreamsIter_t cur_stream;
    //we can wait for less frames than expected by pipeline due to avilability.
    mfxU32 framesToSubmit = m_framesToCombine;
    if (m_streams_pool.empty())
    {
        //if current stream came to empty pool - others already submitted or in process of submission
        //start pool from the beggining
        while(!m_submitted_pool.empty())
        {
            m_submitted_pool.begin()->reset();
            m_streams_pool.splice(m_streams_pool.end(), m_submitted_pool,m_submitted_pool.begin());
        }
    }
    if(m_streams_pool.empty())
    {
        vm_mutex_unlock(&m_mfe_guard);
        return MFX_ERR_MEMORY_ALLOC;
    }

    //get curret stream by info
    std::map<mfxU32, StreamsIter_t>::iterator iter = m_streamsMap.find(info.StreamId);
    if(iter == m_streamsMap.end())
    {
        vm_mutex_unlock(&m_mfe_guard);
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    cur_stream = iter->second;
    if(!cur_stream->isFrameSubmitted())
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
        cur_stream->reset();//cleanup stream state
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

    if(!framesToSubmit)
    {
        vm_mutex_unlock(&m_mfe_guard);
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    vm_tick start_tick = vm_time_get_tick();
    vm_tick spent_ticks = 0;

    while (m_framesCollected < framesToSubmit &&
           !cur_stream->isFieldSubmitted() &&
           timeToWait > spent_ticks)
    {
        vm_status res = vm_cond_timedwait(&m_mfe_wait, &m_mfe_guard,
                                          (mfxU32)((timeToWait - spent_ticks)/ m_time_frequency));
        if ((VM_OK == res) || (VM_TIMEOUT == res))
        {
            spent_ticks = (vm_time_get_tick() - start_tick);
        }
        else
        {
            vm_mutex_unlock(&m_mfe_guard);
            return MFX_ERR_UNKNOWN;
        }
    }
    //for interlace we will return stream back to stream pool when first field submitted
    //to submit next one imediately after than, and don't count it as submitted
    if (!cur_stream->isFieldSubmitted())
    {
        // Form a linear array of stream info comp buffers for submission
        for (StreamsIter_t it = m_toSubmit.begin(); it != m_toSubmit.end(); ++it)
        {
            if (!it->isFieldSubmitted())
            {
                m_streams.push_back(it);
                ENCODE_COMPBUFFERDESC  encodeCompBufferDesc;
                memset(&encodeCompBufferDesc, 0, sizeof(ENCODE_COMPBUFFERDESC));

                encodeCompBufferDesc.CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_MULTISTREAMS);
                encodeCompBufferDesc.DataSize             = mfxU32(sizeof ENCODE_MULTISTREAM_INFO);
                encodeCompBufferDesc.pCompBuffer          = &(it->info);
                m_contexts.push_back(encodeCompBufferDesc);
            }
        }

        if (m_framesCollected < m_contexts.size() || m_contexts.empty())
        {
            for (std::vector<StreamsIter_t>::iterator it = m_streams.begin();
                 it != m_streams.end(); ++it)
            {
                (*it)->sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
            // if cur_stream is not in m_streams (somehow)
            cur_stream->sts = MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        else
        {
            HRESULT hr;
            ENCODE_EXECUTE_PARAMS encodeExecuteParams;
            memset(&encodeExecuteParams,0,sizeof(ENCODE_EXECUTE_PARAMS));
            encodeExecuteParams.NumCompBuffers = (UINT)m_contexts.size();
            encodeExecuteParams.pCompressedBuffers = &m_contexts[0];
            //VAStatus vaSts = vaMFSubmit(m_vaDisplay, m_pMfeContext,
            //                             &m_contexts[0], m_contexts.size());
            D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = { 0 };
            decoderExtParams.Function              = ENCODE_MFE_START_ID;
            decoderExtParams.pPrivateInputData     = &encodeExecuteParams;
            decoderExtParams.PrivateInputDataSize  = sizeof(ENCODE_EXECUTE_PARAMS);
            decoderExtParams.pPrivateOutputData    = 0;
            decoderExtParams.PrivateOutputDataSize = 0;
            decoderExtParams.ResourceCount         = 0; 
            decoderExtParams.ppResourceList        = 0;

            hr = DecoderExtension(m_pVideoContext, m_pMfeContext, decoderExtParams);

            mfxStatus tmp_res = (S_OK == hr) ? MFX_ERR_NONE : MFX_ERR_DEVICE_FAILED;
            for (std::vector<StreamsIter_t>::iterator it = m_streams.begin();
                 it != m_streams.end(); ++it)
            {
                (*it)->fieldSubmitted();
                (*it)->sts = tmp_res;
            }
            m_framesCollected -= (mfxU32)m_contexts.size();
        }
        // Broadcast is done before unlock for this case to simplify the code avoiding extra ifs
        vm_cond_broadcast(&m_mfe_wait);
        m_contexts.clear();
        m_streams.clear();
    }

    // This frame can be already submitted or errored
    // we have to return strm to the pool, release mutex and exit
    mfxStatus res = cur_stream->sts;
    if(cur_stream->isFrameSubmitted())
    {
        m_submitted_pool.splice(m_submitted_pool.end(), m_toSubmit, cur_stream);
    }
    else
    {
        cur_stream->resetField();
        m_streams_pool.splice(m_streams_pool.end(), m_toSubmit, cur_stream);
    }
    vm_mutex_unlock(&m_mfe_guard);
    return res;
}

#endif //MFX_VA_LINUX && MFX_ENABLE_MFE
