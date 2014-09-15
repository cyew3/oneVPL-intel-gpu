//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#include "mfx_h265_encode_hw_ddi.h"
#include <assert.h>

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

namespace MfxHwH265Encode
{

DriverEncoder* CreatePlatformH265Encoder(MFXCoreInterface* core)
{
    if (core)
    {
        mfxCoreParam par = {}; 

        if (core->GetCoreParam(&par))
            return 0;

        if ((par.Impl & 0xF00) == MFX_IMPL_VIA_D3D9)
            return new D3D9Encoder;
    }

    return 0;
}

mfxStatus QueryHwCaps(MFXCoreInterface* core, GUID guid, ENCODE_CAPS_HEVC & caps)
{
    std::auto_ptr<DriverEncoder> ddi;

    ddi.reset(CreatePlatformH265Encoder(core));
    MFX_CHECK(ddi.get(), MFX_WRN_PARTIAL_ACCELERATION);

    mfxStatus sts = ddi.get()->CreateAuxilliaryDevice(core, guid, 1920, 1088);
    MFX_CHECK_STS(sts);
            
    sts = ddi.get()->QueryEncodeCaps(caps);
    MFX_CHECK_STS(sts);

    return sts;
}

void FillSpsBuffer(
    MfxVideoParam const & par, 
    ENCODE_CAPS_HEVC const & caps,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC & sps)
{
    Zero(sps);

    { // SKL
        sps.SAO_enabled_flag                    = 0;
        sps.pcm_enabled_flag                    = 0;
        sps.pcm_loop_filter_disable_flag        = 1;
        sps.log2_min_coding_block_size_minus3   = 0;
        sps.log2_max_coding_block_size_minus3   = 2;
    }

    sps.wFrameWidthInMinCbMinus1  = 
        par.mfx.FrameInfo.Width  / (1 << (sps.log2_min_coding_block_size_minus3 + 3)) - 1;
    sps.wFrameHeightInMinCbMinus1 =
        par.mfx.FrameInfo.Height / (1 << (sps.log2_min_coding_block_size_minus3 + 3)) - 1;

    sps.general_profile_idc = mfxU8(par.mfx.CodecProfile);
    sps.general_level_idc   =  3 * (par.mfx.CodecLevel & 0xFF);
    sps.general_tier_flag   = !!(par.mfx.CodecLevel & MFX_TIER_HEVC_HIGH);

    sps.MinBitRate      = par.TargetKbps;
    sps.FramesPer100Sec = mfxU16(mfxU64(100) * par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD);

    sps.InitVBVBufferFullnessInBit = 8000 * par.InitialDelayInKB;
    sps.VBVBufferSizeInBit         = 8000 * par.BufferSizeInKB;

    if (caps.MBBRCSupport)
    {
        sps.MBBRC = 0;
    }

    if (caps.ParallelBRC)
    {
        sps.ParallelBRC = 0;

        if (sps.ParallelBRC)
        {
            sps.NumOfBInGop[0]  = par.mfx.GopRefDist - 1;
            sps.NumOfBInGop[1]  = 0;
            sps.NumOfBInGop[2]  = 0;
        }
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        sps.UserMaxFrameSize    = 0;
        sps.AVBRAccuracy        = par.mfx.Accuracy;
        sps.AVBRConvergence     = par.mfx.Convergence * 100;
    }
    
    sps.GopPicSize          = par.mfx.GopPicSize;
    sps.GopRefDist          = mfxU8(par.mfx.GopRefDist);
    sps.GopOptFlag          = mfxU8(par.mfx.GopOptFlag);
    sps.TargetUsage         = mfxU8(par.mfx.TargetUsage);
    sps.RateControlMethod   = mfxU8(par.mfx.RateControlMethod);

    sps.TargetBitRate   = par.TargetKbps;
    sps.MaxBitRate      = par.MaxKbps;

    sps.scaling_list_enable_flag            = 0;
    sps.sps_temporal_mvp_enable_flag        = 0;
    sps.strong_intra_smoothing_enable_flag  = 1;
    sps.amp_enabled_flag                    = 1;
    sps.tiles_fixed_structure_flag          = 0;

    sps.chroma_format_idc           = par.mfx.FrameInfo.ChromaFormat;
    sps.separate_colour_plane_flag  = 0;

    sps.log2_min_transform_block_size_minus2 = 0;
    sps.log2_max_transform_block_size_minus2 = 3;

    if (!sps.pcm_enabled_flag )
    {
        sps.log2_min_PCM_cb_size_minus3 = 0;
        sps.log2_max_PCM_cb_size_minus3 = 0;
    }
}

void FillPpsBuffer(
    MfxVideoParam const & /*par*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps)
{
    Zero(pps);
    
    Fill(pps.CollocatedRefPicIndex, IDX_INVALID);
    
    pps.transform_skip_enabled_flag             = 1;
    pps.sign_data_hiding_flag                   = 0;
    pps.transquant_bypass_enabled_flag          = 0;
    pps.constrained_intra_pred_flag             = 0;

    pps.entropy_coding_sync_enabled_flag        = 0;
    pps.dependent_slice_segments_enabled_flag   = 0;

    pps.weighted_pred_flag                      = 0;
    pps.weighted_bipred_flag                    = 0;
    pps.bEnableGPUWeightedPrediction            = 0;

    pps.loop_filter_across_slices_flag          = 0;
    pps.loop_filter_across_tiles_flag           = 0;
    pps.scaling_list_data_present_flag          = 0;

    pps.no_output_of_prior_pics_flag            = 0;

    pps.QpY = 26;

    pps.cu_qp_delta_enabled_flag = 0;

    if (pps.cu_qp_delta_enabled_flag)
    {
        pps.diff_cu_qp_delta_depth = 0;
    }

    pps.pps_cb_qp_offset = 0;
    pps.pps_cr_qp_offset = 0;

    pps.tiles_enabled_flag = 0;

    if (pps.tiles_enabled_flag)
    {
        pps.num_tile_columns_minus1;
        pps.num_tile_rows_minus1;
        pps.tile_column_width;
        pps.tile_row_height;
    }

    pps.log2_parallel_merge_level_minus2 = 0;
    
    pps.num_ref_idx_l0_default_active_minus1 = 0;
    pps.num_ref_idx_l1_default_active_minus1 = 0;

    pps.bEnableRollingIntraRefresh = 0;

    if (pps.bEnableRollingIntraRefresh)
    {
        pps.IntraInsertionLocation;
        pps.IntraInsertionSize;
        pps.QpDeltaForInsertedIntra;
    }

    pps.LcuMaxBitsizeAllowed       = 0;
    pps.bUseRawPicForRef           = 0;
    pps.bScreenContent             = 0;
}

void FillPpsBuffer(
    Task const & task,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps)
{
    pps.CurrOriginalPic.bPicEntry       = task.m_idxRaw == IDX_INVALID ? task.m_idxRec : task.m_idxRaw;
    pps.CurrOriginalPic.AssociatedFlag  = !!(task.m_frameType & MFX_FRAMETYPE_REF);
    pps.CurrReconstructedPic.bPicEntry  = task.m_idxRec;
    
    for (mfxU16 i = 0; i < 15; i ++)
    {
        pps.RefFrameList[i].bPicEntry = task.m_dpb[0][i].m_idxRec;
        pps.RefFramePOCList[i] = task.m_dpb[0][i].m_poc;
    }

    pps.CodingType      = task.m_codingType;
    pps.NumSlices       = 1;
    pps.CurrPicOrderCnt = task.m_poc;

    pps.StatusReportFeedbackNumber = task.m_statusReportNumber;
}

mfxU16 CodingTypeToSliceType(mfxU16 ct)
{
    switch (ct)
    {
     case CODING_TYPE_I : return 2;
     case CODING_TYPE_P : return 1;
     case CODING_TYPE_B :
     case CODING_TYPE_B1:
     case CODING_TYPE_B2: return 0;
    default: assert(!"invalid coding type"); return 0xFFFF;
    }
}


void FillSliceBuffer(
    MfxVideoParam const & /*par*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & sps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice)
{
    slice.resize(1);

    for (mfxU16 i = 0; i < slice.size(); i ++)
    {
        ENCODE_SET_SLICE_HEADER_HEVC & cs = slice[i];
        Zero(cs);

        cs.slice_id = i;

        cs.slice_segment_address = 0;
        cs.NumLCUsInSlice = (sps.wFrameWidthInMinCbMinus1 + 1) * (sps.wFrameHeightInMinCbMinus1 + 1);
        cs.bLastSliceOfPic = (i == slice.size() - 1);
    }
}

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & pps,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice)
{
    for (mfxU16 i = 0; i < slice.size(); i ++)
    {
        ENCODE_SET_SLICE_HEADER_HEVC & cs = slice[i];

        cs.slice_type = CodingTypeToSliceType(task.m_codingType);

        if (cs.slice_type != 2)
        {
            Copy(cs.RefPicList, task.m_refPicList);
            cs.num_ref_idx_l0_active_minus1 = task.m_numRefActive[0] - 1;

            if (cs.slice_type == 0)
                cs.num_ref_idx_l1_active_minus1 = task.m_numRefActive[1] - 1;
        }

        cs.dependent_slice_segment_flag         = 0;
        cs.slice_temporal_mvp_enable_flag       = 0;
        cs.slice_sao_luma_flag                  = 0;
        cs.slice_sao_chroma_flag                = 0;
        cs.mvd_l1_zero_flag                     = 0;
        cs.cabac_init_flag                      = 0;
        cs.slice_deblocking_filter_disable_flag = 0;
        cs.collocated_from_l0_flag              = 0;

        cs.slice_qp_delta = mfxI8(task.m_qpY) - pps.QpY;
        cs.slice_cb_qp_offset = 0;
        cs.slice_cr_qp_offset = 0;

        //if (pps.weighted_pred_flag || pps.weighted_bipred_flag)
        //{
        //    cs.luma_log2_weight_denom;
        //    cs.chroma_log2_weight_denom;
        //    cs.luma_offset[2][15];
        //    cs.delta_luma_weight[2][15];
        //    cs.chroma_offset[2][15][2];
        //    cs.delta_chroma_weight[2][15][2];
        //}

        cs.MaxNumMergeCand = 5;
    }
}

void CachedFeedback::Reset(mfxU32 cacheSize)
{
    Feedback init = {};
    init.bStatus = ENCODE_NOTAVAILABLE;

    m_cache.resize(cacheSize, init);
}

mfxStatus CachedFeedback::Update(const FeedbackStorage& update)
{
    for (size_t i = 0; i < update.size(); i++)
    {
        if (update[i].bStatus != ENCODE_NOTAVAILABLE)
        {
            Feedback* cache = 0;

            for (size_t j = 0; j < m_cache.size(); j++)
            {
                if (m_cache[j].StatusReportFeedbackNumber == update[i].StatusReportFeedbackNumber)
                {
                    cache = &m_cache[j];
                    break;
                }
                else if (cache == 0 && m_cache[j].bStatus == ENCODE_NOTAVAILABLE)
                {
                    cache = &m_cache[j];
                }
            }
            MFX_CHECK( cache != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
            *cache = update[i];
        }
    }
    return MFX_ERR_NONE;
}

const CachedFeedback::Feedback* CachedFeedback::Hit(mfxU32 feedbackNumber) const
{
    for (size_t i = 0; i < m_cache.size(); i++)
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
            return &m_cache[i];

    return 0;
}

mfxStatus CachedFeedback::Remove(mfxU32 feedbackNumber)
{
    for (size_t i = 0; i < m_cache.size(); i++)
    {
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
        {
            m_cache[i].bStatus = ENCODE_NOTAVAILABLE;
            return MFX_ERR_NONE;
        }
    }

    return MFX_ERR_UNDEFINED_BEHAVIOR;
}

D3D9Encoder::D3D9Encoder()
    : m_core(0)
    , m_auxDevice(0)
    , m_infoQueried(false)
{
    Zero(m_caps);
}

D3D9Encoder::~D3D9Encoder()
{
    Destroy();
}

mfxStatus D3D9Encoder::CreateAuxilliaryDevice(
    MFXCoreInterface * core,
    GUID        guid,
    mfxU32      width,
    mfxU32      height)
{
    m_core = core;
    IDirect3DDeviceManager9 *device = 0;
    mfxStatus sts = MFX_ERR_NONE;

    //D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(m_core, MFXICORED3D_GUID);
    //MFX_CHECK_WITH_ASSERT(pID3D, MFX_ERR_DEVICE_FAILED);
    //
    //sts = pID3D->GetD3DService(mfxU16(width), mfxU16(height), &service);
    //MFX_CHECK_STS(sts);
    //MFX_CHECK(service, MFX_ERR_DEVICE_FAILED);

    sts = core->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&device);
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

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::CreateAccelerationService(MfxVideoParam const & par)
{
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    DXVADDI_VIDEODESC desc = {};
    desc.SampleWidth  = m_width;
    desc.SampleHeight = m_height;
    desc.Format = D3DDDIFMT_NV12;

    ENCODE_CREATEDEVICE encodeCreateDevice = {};
    encodeCreateDevice.pVideoDesc     = &desc;
    encodeCreateDevice.CodingFunction = ENCODE_ENC_PAK;
    encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

    if (par.Protected)
    {
        assert(!"not implemented");
    }

    HRESULT hr = m_auxDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, m_guid, encodeCreateDevice);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    Zero(m_capsQuery);
    hr = m_auxDevice->Execute(ENCODE_ENC_CTRL_CAPS_ID, (void *)0, m_capsQuery);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    Zero(m_capsGet);
    hr = m_auxDevice->Execute(ENCODE_ENC_CTRL_GET_ID, (void *)0, m_capsGet);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        
    FillSpsBuffer(par, m_caps, m_sps);
    FillPpsBuffer(par, m_pps);
    FillSliceBuffer(par, m_sps, m_pps, m_slice);

    m_cu_buffer.resize(m_width * m_height * 3 / 2 + 8, 0);
    m_cu_data.pBuffer       = m_cu_buffer.data();
    m_cu_data.BufferSize    = m_cu_buffer.size();
    //m_cu_data.CUDataOffset = 8;

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::Reset(MfxVideoParam const & /*par*/)
{
    assert(!"not implemented");

    return MFX_ERR_NONE;
}


mfxStatus D3D9Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
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

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryEncodeCaps(ENCODE_CAPS_HEVC & caps)
{
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    caps = m_caps;

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
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

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        // Reserve space for feedback reports.
        m_feedbackUpdate.resize(response.NumFrameActual);
        m_feedbackCached.Reset(response.NumFrameActual);
    }

    return MFX_ERR_NONE;
}

#define ADD_CBD(id, buf, num)\
    compBufDesc[executeParams.NumCompBuffers].CompressedBufferType = (id);   \
    compBufDesc[executeParams.NumCompBuffers].DataSize = sizeof(buf) * (num);\
    compBufDesc[executeParams.NumCompBuffers].pCompBuffer = &buf;            \
    executeParams.NumCompBuffers++;                                          \
    assert(executeParams.NumCompBuffers <= MaxCompBufDesc);

mfxStatus D3D9Encoder::Execute(Task const & task, mfxHDL surface)
{
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    const mfxU32 MaxCompBufDesc = 16;
    ENCODE_COMPBUFFERDESC compBufDesc[MaxCompBufDesc] = {};

    ENCODE_EXECUTE_PARAMS executeParams = {};
    executeParams.pCompressedBuffers = compBufDesc;

    mfxU32 bitstream = task.m_idxBs;
    
    FillPpsBuffer(task, m_pps);
    FillSliceBuffer(task, m_sps, m_pps, m_slice);

    ADD_CBD(D3DDDIFMT_INTELENCODE_SPSDATA,          m_sps,      1);
    ADD_CBD(D3DDDIFMT_INTELENCODE_PPSDATA,          m_pps,      1);
    ADD_CBD(D3DDDIFMT_INTELENCODE_SLICEDATA,        m_slice[0], m_slice.size());
    ADD_CBD(D3DDDIFMT_INTELENCODE_BITSTREAMDATA,    bitstream,  1);
    ADD_CBD(D3DDDIFMT_HEVC_BUFFER_CUDATA,           m_cu_data,  1);

    try
    {
        HANDLE handle;
        HRESULT hr = m_auxDevice->BeginFrame((IDirect3DSurface9 *)surface, 0);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);


        hr = m_auxDevice->Execute(ENCODE_ENC_PAK_ID, executeParams, (void *)0);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    
        m_auxDevice->EndFrame(&handle);
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }


    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryStatus(Task & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "QueryStatus");
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    // After SNB once reported ENCODE_OK for a certain feedbackNumber
    // it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    // As we won't get all bitstreams we need to cache all other statuses.

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_statusReportNumber);

    ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr;
    feedbackDescr.SizeOfStatusParamStruct = sizeof(m_feedbackUpdate[0]);
    feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;

    // if task is not in cache then query its status
    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {
        try
        {
            HRESULT hr = m_auxDevice->Execute(
                ENCODE_QUERY_STATUS_ID,
                (void *)&feedbackDescr,
                sizeof(feedbackDescr),
                &m_feedbackUpdate[0],
                (mfxU32)m_feedbackUpdate.size() * sizeof(m_feedbackUpdate[0]));
            MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
        catch (...)
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        // Put all with ENCODE_OK into cache.
        m_feedbackCached.Update(m_feedbackUpdate);

        feedback = m_feedbackCached.Hit(task.m_statusReportNumber);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        task.m_bsDataLength = feedback->bitstreamSize;
        m_feedbackCached.Remove(task.m_statusReportNumber);
        return MFX_ERR_NONE;

    case ENCODE_NOTREADY:
        return MFX_WRN_DEVICE_BUSY;

    case ENCODE_NOTAVAILABLE:
    case ENCODE_ERROR:
    default:
        assert(!"bad feedback status");
        return MFX_ERR_DEVICE_FAILED;
    }
}

mfxStatus D3D9Encoder::Destroy()
{
    m_auxDevice.reset(0);
    return MFX_ERR_NONE;
}

}; // namespace MfxHwH265Encode
