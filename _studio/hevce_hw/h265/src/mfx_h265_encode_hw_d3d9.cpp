//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_config.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_common.h"
#include "mfx_h265_encode_hw_d3d9.h"

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

namespace MfxHwH265Encode
{
template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
D3D9Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::D3D9Encoder()
    : m_core(0)
    , m_auxDevice(0)
    , m_guid()
    , m_width(0)
    , m_height(0)
    , m_caps()
    , m_capsQuery()
    , m_capsGet()
    , m_infoQueried(false)
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    , m_widi(false)
#endif
    , m_maxSlices(0)
    , m_sps()
    , m_pps()
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    , DDITracer(std::is_same<DDI_SPS, ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT>::value ? ENCODER_REXT : ENCODER_DEFAULT)
#endif //defined(PRE_SI_TARGET_PLATFORM_GEN11)
{
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
D3D9Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::~D3D9Encoder()
{
    Destroy();
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D9Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::CreateAuxilliaryDevice(
    MFXCoreInterface * core,
    GUID        guid,
    mfxU32      width,
    mfxU32      height)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::CreateAuxilliaryDevice");
    m_core = core;

#ifdef HEADER_PACKING_TEST
    m_guid      = guid;
    m_width     = width;
    m_height    = height;
    //m_auxDevice.reset((AuxiliaryDevice*) 0xFFFFFFFF);

    m_caps.CodingLimitSet           = 1;
    m_caps.BitDepth8Only            = 0;
    m_caps.Color420Only             = 0;
    m_caps.SliceStructure           = 4;
    m_caps.SliceIPOnly              = 0;
    m_caps.NoWeightedPred           = 1;
    m_caps.NoMinorMVs               = 1;
    m_caps.RawReconRefToggle        = 1;
    m_caps.NoInterlacedField        = 1;
    m_caps.BRCReset                 = 1;
    m_caps.RollingIntraRefresh      = 0;
    m_caps.UserMaxFrameSizeSupport  = 0;
    m_caps.FrameLevelRateCtrl       = 1;
    m_caps.SliceByteSizeCtrl        = 0;
    m_caps.VCMBitRateControl        = 1;
    m_caps.ParallelBRC              = 1;
    m_caps.TileSupport              = 0;
    m_caps.MaxPicWidth              = 8192;
    m_caps.MaxPicHeight             = 4096;
    m_caps.MaxNum_Reference0        = 3;
    m_caps.MaxNum_Reference1        = 1;
    m_caps.MBBRCSupport             = 1;
    m_caps.TUSupport                = 73;
    m_caps.TileSupport              = 1;
    m_caps.MaxNumOfTileColumnsMinus1= 3;
#else
    IDirect3DDeviceManager9 *device = 0;
    mfxStatus sts = MFX_ERR_NONE;

    sts = core->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&device);

    if (sts == MFX_ERR_NOT_FOUND)
        sts = core->CreateAccelerationDevice(MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&device);

    MFX_CHECK_STS(sts);
    MFX_CHECK(device, MFX_ERR_DEVICE_FAILED);

    std::auto_ptr<AuxiliaryDevice> auxDevice(new AuxiliaryDevice());
    sts = auxDevice->Initialize(device);
    MFX_CHECK_STS(sts);

    sts = auxDevice->IsAccelerationServiceExist(guid);
    MFX_CHECK_STS(sts);

    HRESULT hr = auxDevice->Execute(AUXDEV_QUERY_ACCEL_CAPS, guid, m_caps);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    
    m_guid      = guid;
    m_width     = width;
    m_height    = height;
    m_auxDevice = auxDevice;
#endif
    
    Trace(m_caps, 0);

    sts = HardcodeCaps(m_caps, core, guid);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D9Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::CreateAccelerationService(MfxVideoParam const & par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::CreateAccelerationService");
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    const mfxExtPAVPOption& PAVP = par.m_ext.PAVP;

    m_widi = par.WiDi;
#endif

#ifndef HEADER_PACKING_TEST
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    DXVADDI_VIDEODESC desc = {};
    desc.SampleWidth  = m_width;
    desc.SampleHeight = m_height;
    desc.Format = D3DDDIFMT_NV12;

    ENCODE_CREATEDEVICE encodeCreateDevice = {};
    encodeCreateDevice.pVideoDesc     = &desc;
    encodeCreateDevice.CodingFunction = ENCODE_ENC_PAK;
    encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    encodeCreateDevice.CodingFunction = m_widi ? (ENCODE_ENC_PAK | ENCODE_WIDI) : ENCODE_ENC_PAK;
    D3DAES_CTR_IV        initialCounter = { PAVP.CipherCounter.IV, PAVP.CipherCounter.Count };
    PAVP_ENCRYPTION_MODE encryptionMode = { PAVP.EncryptionType,   PAVP.CounterType         };

    if (par.Protected == MFX_PROTECTION_PAVP || par.Protected == MFX_PROTECTION_GPUCP_PAVP)
    {
        encodeCreateDevice.EncryptionMode        = DXVA2_INTEL_PAVP;
        encodeCreateDevice.CounterAutoIncrement  = PAVP.CounterIncrement;
        encodeCreateDevice.pInitialCipherCounter = &initialCounter;
        encodeCreateDevice.pPavpEncryptionMode   = &encryptionMode;
    }
#endif

    HRESULT hr;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "AUXDEV_CREATE_ACCEL_SERVICE");
        hr = m_auxDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, m_guid, encodeCreateDevice);
    }
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    Zero(m_capsQuery);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "ENCODE_ENC_CTRL_CAPS_ID");
        hr = m_auxDevice->Execute(ENCODE_ENC_CTRL_CAPS_ID, (void *)0, m_capsQuery);
    }
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    Zero(m_capsGet);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "ENCODE_ENC_CTRL_GET_ID");
        hr = m_auxDevice->Execute(ENCODE_ENC_CTRL_GET_ID, (void *)0, m_capsGet);
    }
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
#else
    PAVP;
#endif

    FillSpsBuffer(par, m_caps, m_sps);
    FillPpsBuffer(par, m_pps);
    FillSliceBuffer(par, m_sps, m_pps, m_slice);

    DDIHeaderPacker::Reset(par);
    m_cbd.resize(MAX_DDI_BUFFERS + MaxPackedHeaders());

    m_maxSlices = CeilDiv(par.m_ext.HEVCParam.PicHeightInLumaSamples, par.LCUSize) * CeilDiv(par.m_ext.HEVCParam.PicWidthInLumaSamples, par.LCUSize);

    return MFX_ERR_NONE;
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D9Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::Reset(MfxVideoParam const & par, bool bResetBRC)
{
    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);

    FillSpsBuffer(par, m_caps, m_sps);
    FillPpsBuffer(par, m_pps);
    FillSliceBuffer(par, m_sps, m_pps, m_slice);

    DDIHeaderPacker::Reset(par);
    m_cbd.resize(MAX_DDI_BUFFERS + MaxPackedHeaders());

    m_sps.bResetBRC = bResetBRC;

    return MFX_ERR_NONE;
}


template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D9Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
#ifndef HEADER_PACKING_TEST
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    if (!m_infoQueried)
    {
        ENCODE_FORMAT_COUNT encodeFormatCount;
        encodeFormatCount.CompressedBufferInfoCount = 0;
        encodeFormatCount.UncompressedFormatCount = 0;

        GUID guid = m_auxDevice->GetCurrentGuid();
        HRESULT hr = m_auxDevice->Execute(ENCODE_FORMAT_COUNT_ID, guid, encodeFormatCount);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats;
        encodeFormats.CompressedBufferInfoSize = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
        encodeFormats.UncompressedFormatSize = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
        encodeFormats.pCompressedBufferInfo = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats = &m_uncompBufInfo[0];

        hr = m_auxDevice->Execute(ENCODE_FORMATS_ID, (void *)0, encodeFormats);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(encodeFormats.CompressedBufferInfoSize > 0, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(encodeFormats.UncompressedFormatSize > 0, MFX_ERR_DEVICE_FAILED);

        m_infoQueried = true;
    }

    size_t i = 0;
    for (; i < m_compBufInfo.size(); i++)
        if (m_compBufInfo[i].Type == type)
            break;

    MFX_CHECK(i < m_compBufInfo.size(), MFX_ERR_DEVICE_FAILED);

    request.Info.Width  = m_compBufInfo[i].CreationWidth;
    request.Info.Height = m_compBufInfo[i].CreationHeight;
    request.Info.FourCC = m_compBufInfo[i].CompressedFormats;
#else
    type;
    request.Info.Width  = (mfxU16)m_width;
    request.Info.Height = (mfxU16)m_height;
    request.Info.FourCC = MFX_FOURCC_NV12;
#endif

    return MFX_ERR_NONE;
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D9Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::QueryEncodeCaps(ENCODE_CAPS_HEVC & caps)
{
#ifndef HEADER_PACKING_TEST
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);
#endif

    caps = m_caps;

    return MFX_ERR_NONE;
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D9Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
#ifndef HEADER_PACKING_TEST
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    mfxFrameAllocator & fa = m_core->FrameAllocator();
    EmulSurfaceRegister surfaceReg = {};
    surfaceReg.type = type;
    surfaceReg.num_surf = response.NumFrameActual;

    MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxStatus sts = fa.GetHDL(fa.pthis, response.mids[i], (mfxHDL *)&surfaceReg.surface[i]);
        MFX_CHECK_STS(sts);
        MFX_CHECK(surfaceReg.surface[i], MFX_ERR_UNSUPPORTED);
    }

    HRESULT hr = m_auxDevice->BeginFrame(surfaceReg.surface[0], &surfaceReg);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_auxDevice->EndFrame(0);
#endif

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        // Reserve space for feedback reports.
        mfxU32 feedbackSize = FeedbackSize(m_caps.SliceLevelReportSupport ? QUERY_STATUS_PARAM_SLICE : QUERY_STATUS_PARAM_FRAME, m_maxSlices);
        m_feedbackUpdate.Reset(response.NumFrameActual, feedbackSize);
        m_feedbackCached.Reset(response.NumFrameActual, feedbackSize);
    }

    return MFX_ERR_NONE;
}

#define ADD_CBD(id, buf, num)\
    assert(executeParams.NumCompBuffers < m_cbd.size());\
    m_cbd[executeParams.NumCompBuffers].CompressedBufferType = (id); \
    m_cbd[executeParams.NumCompBuffers].DataSize = (UINT)(sizeof(buf) * (num));\
    m_cbd[executeParams.NumCompBuffers].pCompBuffer = &buf; \
    executeParams.NumCompBuffers++;

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D9Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::Execute(Task const & task, mfxHDL surface)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::Execute");
#ifndef HEADER_PACKING_TEST
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);
#endif

    ENCODE_PACKEDHEADER_DATA * pPH = 0;
    ENCODE_EXECUTE_PARAMS executeParams = {};
#if defined(MFX_SKIP_FRAME_SUPPORT)
    HevcSkipMode skipMode(task.m_SkipMode);
#endif
    executeParams.pCompressedBuffers = &m_cbd[0];
    Zero(m_cbd);

    mfxU32 bitstream = task.m_idxBs;

    if (!m_sps.bResetBRC)
        m_sps.bResetBRC = task.m_resetBRC;

    FillPpsBuffer(task, m_pps);
    FillSliceBuffer(task, m_sps, m_pps, m_slice);
    m_pps.NumSlices = (USHORT)(m_slice.size());

    ADD_CBD(D3DDDIFMT_INTELENCODE_SPSDATA,          m_sps,      1);
    ADD_CBD(D3DDDIFMT_INTELENCODE_PPSDATA,          m_pps,      1);
    ADD_CBD(D3DDDIFMT_INTELENCODE_SLICEDATA,        m_slice[0], m_slice.size());
    ADD_CBD(D3DDDIFMT_INTELENCODE_BITSTREAMDATA,    bitstream,  1);

#if MFX_EXTBUFF_CU_QP_ENABLE
    if (m_pps.cu_qp_delta_enabled_flag && m_sps.RateControlMethod == MFX_RATECONTROL_CQP )
    {
        mfxU32 idxCUQp  = task.m_idxCUQp;
        ADD_CBD(D3DDDIFMT_INTELENCODE_MBQPDATA, idxCUQp,  1);
    }
#endif

    if (task.m_insertHeaders & INSERT_AUD)
    {
        pPH = PackHeader(task, AUD_NUT); assert(pPH);
        ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, *pPH, 1);
    }

    if (task.m_insertHeaders & INSERT_VPS)
    {
        pPH = PackHeader(task, VPS_NUT); assert(pPH);
        ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, *pPH, 1);
    }

    if (task.m_insertHeaders & INSERT_SPS)
    {
        pPH = PackHeader(task, SPS_NUT); assert(pPH);
        ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, *pPH, 1);
    }

    if (task.m_insertHeaders & INSERT_PPS)
    {
        pPH = PackHeader(task, PPS_NUT); assert(pPH);
        ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, *pPH, 1);
    }

    if ((task.m_insertHeaders & INSERT_SEI) || (task.m_ctrl.NumPayload > 0))
    {
        pPH = PackHeader(task, PREFIX_SEI_NUT); assert(pPH);

        if (pPH->DataLength)
        {
            ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, *pPH, 1);
        }
    }

    for (mfxU32 i = 0; i < m_slice.size(); i ++)
    {
#if defined(MFX_SKIP_FRAME_SUPPORT)
        if (skipMode.NeedSkipSliceGen())
        {
            // pack skip slice 
            pPH = PackSkippedSlice(task, i, &m_slice[i].SliceQpDeltaBitOffset); assert(pPH);
            ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA, *pPH, 1);
            if (!skipMode.NeedDriverCall())
            {
                // copy packed sliced into bitstream

                //ENCODE_QUERY_STATUS_PARAMS feedback = { task.m_statusReportNumber, 0, };
                mfxFrameData bs = { 0 };

                FrameLocker lock(m_core, task.m_midBs);
                assert(bs.Y);


                mfxU8 *  bsDataStart = bs.Y;
                mfxU8 *  bsEnd = bs.Y + m_width * m_height;
                mfxU8 *  bsDataEnd = bsDataStart;

                for (UINT i = 0; i < executeParams.NumCompBuffers; i++)
                {
                    if (m_cbd[i].CompressedBufferType == (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA)
                    {
                        ENCODE_PACKEDHEADER_DATA const & data = *(ENCODE_PACKEDHEADER_DATA*)m_cbd[i].pCompBuffer;
                        mfxU8 * sbegin = data.pData + data.DataOffset;
                        bsDataEnd += AddEmulationPreventionAndCopy(sbegin, data.DataLength, bsDataEnd, bsEnd, !!m_pps.bEmulationByteInsertion);
                    }
                }
                //feedback.bitstreamSize = mfxU32(bsDataEnd - bsDataStart);
            }

        }
        else{
            pPH = PackSliceHeader(task, i, &m_slice[i].SliceQpDeltaBitOffset); assert(pPH);
            ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA, *pPH, 1);
        }
#else
        pPH = PackSliceHeader(task, i, &m_slice[i].SliceQpDeltaBitOffset); assert(pPH);
        ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA, *pPH, 1);
#endif
    }
    
    /*if (task.m_ctrl.NumPayload > 0)
    {
        pPH = PackHeader(task, SUFFIX_SEI_NUT); assert(pPH);

        if (pPH->DataLength)
        {
            ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, *pPH, 1);
        }
    }*/

    Trace(executeParams, 0);

    try
    {
#ifdef HEADER_PACKING_TEST
        surface;
        ENCODE_QUERY_STATUS_PARAMS fb = {task.m_statusReportNumber,};
        FrameLocker bs(m_core, task.m_midBs);

        for (mfxU32 i = 0; i < executeParams.NumCompBuffers; i ++)
        {
            pPH = (ENCODE_PACKEDHEADER_DATA*)executeParams.pCompressedBuffers[i].pCompBuffer;

            if (executeParams.pCompressedBuffers[i].CompressedBufferType == D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA)
            {
                memcpy(bs.Y + fb.bitstreamSize, pPH->pData, pPH->DataLength);
                fb.bitstreamSize += pPH->DataLength;
            }
            else if (executeParams.pCompressedBuffers[i].CompressedBufferType == D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA)
            {
                mfxU32 sz = m_width * m_height * 3 / 2 - fb.bitstreamSize;
                HeaderPacker::PackRBSP(bs.Y + fb.bitstreamSize, pPH->pData, sz, CeilDiv(pPH->DataLength, 8));
                fb.bitstreamSize += sz;
            }
        }

        m_feedbackUpdate[0] = fb;
        m_feedbackCached.Update(m_feedbackUpdate);
#else
        HANDLE handle;
        HRESULT hr;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "BeginFrame");
            hr = m_auxDevice->BeginFrame((IDirect3DSurface9 *)surface, 0);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "ENCODE_ENC_PAK_ID");
            hr = m_auxDevice->Execute(ENCODE_ENC_PAK_ID, executeParams, (void *)0);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "EndFrame");
            m_auxDevice->EndFrame(&handle);
        }
#endif
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    m_sps.bResetBRC = 0;


    return MFX_ERR_NONE;
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D9Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::QueryStatus(Task & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::QueryStatus");
#ifndef HEADER_PACKING_TEST
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);
#endif
#if defined(MFX_SKIP_FRAME_SUPPORT)
    HevcSkipMode skipMode(task.m_SkipMode);
    if (!skipMode.NeedDriverCall())
        return MFX_ERR_NONE;
#endif
    // After SNB once reported ENCODE_OK for a certain feedbackNumber
    // it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    // As we won't get all bitstreams we need to cache all other statuses.

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_statusReportNumber);

    // if task is not in cache then query its status
    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {
        ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr = {};
        feedbackDescr.StatusParamType = (m_caps.SliceLevelReportSupport && m_pps.bEnableSliceLevelReport) ? QUERY_STATUS_PARAM_SLICE : QUERY_STATUS_PARAM_FRAME;

        for (;;)
        {
            HRESULT hr;
            feedbackDescr.SizeOfStatusParamStruct = FeedbackSize((ENCODE_QUERY_STATUS_PARAM_TYPE)feedbackDescr.StatusParamType, m_maxSlices);

            m_feedbackUpdate.Reset(m_feedbackUpdate.size(), feedbackDescr.SizeOfStatusParamStruct);

            try
            {
                hr = m_auxDevice->Execute(
                    ENCODE_QUERY_STATUS_ID,
                    (void *)&feedbackDescr,
                    sizeof(feedbackDescr),
                    &m_feedbackUpdate[0],
                    (mfxU32)m_feedbackUpdate.size() * m_feedbackUpdate.feedback_size());
            }
            catch (...)
            {
                return MFX_ERR_DEVICE_FAILED;
            }

            if (hr == E_INVALIDARG && feedbackDescr.StatusParamType == QUERY_STATUS_PARAM_SLICE)
            {
                feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;
                continue;
            }

            MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
            break;
        }

        // Put all with ENCODE_OK into cache.
        m_feedbackCached.Update(m_feedbackUpdate);

        feedback = m_feedbackCached.Hit(task.m_statusReportNumber);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        Trace(*feedback, 0);
        task.m_bsDataLength = feedback->bitstreamSize;

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
        if (m_widi && m_caps.HWCounterAutoIncrementSupport)
        {
            task.m_aes_counter.Count = feedback->aes_counter.Counter;
            task.m_aes_counter.IV    = feedback->aes_counter.IV;
        }
#endif

        {
            mfxExtEncodedSlicesInfo* pESI = ExtBuffer::Get(task.m_ctrl);

            if (pESI)
            {
                pESI->NumEncodedSlice     = feedback->NumberSlices;
                pESI->NumSliceNonCopliant = feedback->NumSlicesNonCompliant;
                pESI->SliceSizeOverflow   = feedback->SliceSizeOverflow;

                if (pESI->NumSliceSizeAlloc && pESI->SliceSize && m_caps.SliceLevelReportSupport)
                {
                    mfxU16* pSize = (mfxU16*)((mfxU8*)feedback + sizeof(ENCODE_QUERY_STATUS_PARAMS) + sizeof(UINT) * 4);
                    CopyN(pESI->SliceSize, pSize, Min(feedback->NumberSlices, pESI->NumSliceSizeAlloc));
                }
            }
        }

        m_feedbackCached.Remove(task.m_statusReportNumber);
        return MFX_ERR_NONE;

    case ENCODE_NOTREADY:
        return MFX_WRN_DEVICE_BUSY;

    case ENCODE_NOTAVAILABLE:
    case ENCODE_ERROR:
    default:
        Trace(*feedback, 0);
        assert(!"bad feedback status");
        return MFX_ERR_DEVICE_FAILED;
    }
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D9Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::Destroy()
{
    m_auxDevice.reset(0);
    return MFX_ERR_NONE;
}

template class D3D9Encoder<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC, ENCODE_SET_PICTURE_PARAMETERS_HEVC, ENCODE_SET_SLICE_HEADER_HEVC>;

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
template class D3D9Encoder<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT, ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT, ENCODE_SET_SLICE_HEADER_HEVC_REXT>;
#endif

}; // namespace MfxHwH265Encode

#endif // #if defined(_WIN32) || defined(_WIN64)
#endif
