// Copyright (c) 2011-2020 Intel Corporation
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

#if defined (MFX_D3D11_ENABLED)
#if defined (MFX_ENABLE_H264_VIDEO_ENCODE)

#define CHECK_HRES(hRes) \
        if (FAILED(hRes))\
            return MFX_ERR_DEVICE_FAILED;


#include "mfx_h264_encode_hw_utils.h"

#include "mfx_h264_encode_d3d11.h"
#include "libmfx_core_interface.h"
#include "encoder_ddi.hpp"

#include "vm_time.h"
#include "mfx_session.h"

#include "mfx_enc_common.h"

DEFINE_GUID(DXVADDI_Intel_Decode_PrivateData_Report,
0x49761bec, 0x4b63, 0x4349, 0xa5, 0xff, 0x87, 0xff, 0xdf, 0x8, 0x84, 0x66);



using namespace MfxHwH264Encode;

mfxU16 ownConvertD3DFMT_TO_MFX(DXGI_FORMAT format);
mfxU8 convertDX9TypeToDX11Type(mfxU8 type);

D3D11Encoder::D3D11Encoder()
: m_core(nullptr)
, m_pVideoDevice(nullptr)
, m_pVideoContext(nullptr)
, m_pDecoder(nullptr)
, m_guid()
, m_requestedGuid()
, m_caps()
, m_capsQuery()
, m_capsGet()
, m_compBufInfo()
, m_uncompBufInfo()
, m_infoQueried()
, m_sps()
, m_vui()
, m_pps()
, m_slice()
#ifdef MFX_ENABLE_AVC_CUSTOM_QMATRIX
, m_qMatrix()
#endif
, m_compBufDesc()
, m_feedbackUpdate()
, m_feedbackCached()
, m_headerPacker()
, m_reconQueue()
, m_bsQueue()
, m_mbqpQueue()
, m_forcedCodingFunction(0)
, m_numSkipFrames(0)
, m_sizeSkipFrames(0)
, m_skipMode(0)
, m_dirtyRects()
, m_movingRects()
, m_encodeExecuteParams()
, m_encodeInputDesc()
, m_idBit(0)
, m_idMBQP(0)
, m_packedSei()
, m_decoderExtParams()
, m_resourceList()
, m_resourceCount(0)
{
}


D3D11Encoder::~D3D11Encoder()
{
    Destroy();
}


mfxStatus D3D11Encoder::CreateAuxilliaryDevice(
    VideoCORE * core,
    GUID        guid,
    mfxU32      width,
    mfxU32      height,
    bool        isTemporal)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::CreateAuxilliaryDevice");
    MFX_CHECK_NULL_PTR1(core);

    D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(core);

    MFX_CHECK_NULL_PTR1(pD3d11);

    m_core = core;
    mfxStatus sts = Init(
        guid,
        pD3d11->GetD3D11VideoDevice(isTemporal),
        pD3d11->GetD3D11VideoContext(isTemporal),
        width,
        height,
        NULL); // no encryption

    return sts;

} // mfxStatus D3D11Encoder::CreateAuxilliaryDevice(...)


mfxStatus D3D11Encoder::CreateAccelerationService(MfxVideoParam const & par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::CreateAccelerationService");
    D3D11_VIDEO_DECODER_EXTENSION decoderExtParam = {};
    HRESULT hRes;
    mfxExtCodingOption2 const * extCO2 = GetExtBuffer(par);

    if (extCO2)
        m_skipMode = extCO2->SkipFrame;

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if( IsProtectionPavp(par.Protected) )
    {
        Destroy(); //release decoder device

        D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(m_core);
        MFX_CHECK_NULL_PTR1(pD3d11);

        mfxExtPAVPOption const * extPavp = GetExtBuffer(par);

        mfxStatus sts = Init(
            m_guid,
            pD3d11->GetD3D11VideoDevice(),
            pD3d11->GetD3D11VideoContext(),
            par.mfx.FrameInfo.Width,
            par.mfx.FrameInfo.Height,
            extPavp);

        MFX_CHECK_STS(sts);
    }
#endif

    {
        decoderExtParam.Function = ENCODE_ENC_CTRL_CAPS_ID;
        decoderExtParam.pPrivateOutputData = &m_capsQuery;
        decoderExtParam.PrivateOutputDataSize = sizeof(m_capsQuery);

        hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParam);
        CHECK_HRES(hRes);

        decoderExtParam.Function = ENCODE_ENC_CTRL_GET_ID;
        decoderExtParam.pPrivateOutputData = &m_capsGet;
        decoderExtParam.PrivateOutputDataSize = sizeof(m_capsGet);

        hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParam);
        CHECK_HRES(hRes);
    }

    mfxU16 maxNumSlice = GetMaxNumSlices(par);

    m_slice.resize(maxNumSlice);

    mfxU32 const MAX_NUM_PACKED_SPS = 9;
    mfxU32 const MAX_NUM_PACKED_PPS = 9;
    m_compBufDesc.resize(11 + MAX_NUM_PACKED_SPS + MAX_NUM_PACKED_PPS + maxNumSlice);


    Zero(m_sps);
    Zero(m_vui);
    Zero(m_pps);
    Zero(m_slice);

    FillSpsBuffer(par, m_sps);
    FillVuiBuffer(par, m_vui);
    FillConstPartOfPpsBuffer(par, m_caps, m_pps);
    FillConstPartOfSliceBuffer(par, m_slice);

    m_headerPacker.Init(par, m_caps);
    return MFX_ERR_NONE;
}


mfxStatus D3D11Encoder::Reset(
    MfxVideoParam const & par)
{
    mfxU32 oldTargetBitrate = m_sps.TargetBitRate;
    mfxU32 oldMaxBitrate    = m_sps.MaxBitRate;
    mfxU32 oldFrameRate    = m_sps.FramesPer100Sec;
    mfxU32 oldMaxIFS = m_sps.UserMaxIFrameSize;
    mfxU32 oldMaxPBFS = m_sps.UserMaxPBFrameSize;

    mfxExtCodingOption2 const * extCO2 = GetExtBuffer(par);

    if (extCO2)
        m_skipMode = extCO2->SkipFrame;

    FillSpsBuffer(par, m_sps);
    FillVuiBuffer(par, m_vui);
    FillConstPartOfPpsBuffer(par, m_caps, m_pps);

    m_sps.bResetBRC =
        m_sps.TargetBitRate != oldTargetBitrate ||
        m_sps.MaxBitRate    != oldMaxBitrate ||
        m_sps.FramesPer100Sec != oldFrameRate ||
        m_sps.UserMaxIFrameSize != oldMaxIFS ||
        m_sps.UserMaxPBFrameSize != oldMaxPBFS;

    mfxU16 maxNumSlices = GetMaxNumSlices(par);
    m_slice.resize(maxNumSlices);

    m_headerPacker.Init(par, m_caps);

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::Reset(...)


mfxStatus D3D11Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
    type = (D3DDDIFORMAT)convertDX9TypeToDX11Type((mfxU8)type);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::QueryCompBufferInfo");
    MFX_CHECK_WITH_ASSERT(m_pDecoder, MFX_ERR_NOT_INITIALIZED);

    if (!m_infoQueried)
    {
        ENCODE_FORMAT_COUNT encodeFormatCount = {};

        D3D11_VIDEO_DECODER_EXTENSION ext = {};
        ext.Function = ENCODE_FORMAT_COUNT_ID;
        ext.pPrivateOutputData = &encodeFormatCount;
        ext.PrivateOutputDataSize = sizeof(ENCODE_FORMAT_COUNT);

        HRESULT hRes = DecoderExtension(m_pVideoContext, m_pDecoder, ext);
        CHECK_HRES(hRes);
        //---------------------------------------------------

        m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats;
        encodeFormats.CompressedBufferInfoSize = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
        encodeFormats.UncompressedFormatSize = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
        encodeFormats.pCompressedBufferInfo = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats = &m_uncompBufInfo[0];

        Zero(ext);
        ext.Function = ENCODE_FORMATS_ID;
        ext.pPrivateOutputData = &encodeFormats;
        ext.PrivateOutputDataSize = sizeof(ENCODE_FORMATS);

        hRes = DecoderExtension(m_pVideoContext, m_pDecoder, ext);
        CHECK_HRES(hRes);

        MFX_CHECK(encodeFormats.CompressedBufferInfoSize > 0, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(encodeFormats.UncompressedFormatSize > 0, MFX_ERR_DEVICE_FAILED);

        m_infoQueried = true;
    }

    size_t i = 0;
    for (; i < m_compBufInfo.size(); i++)
    {
        if (m_compBufInfo[i].Type == type)
        {
            break;
        }
    }

    MFX_CHECK(i < m_compBufInfo.size(), MFX_ERR_DEVICE_FAILED);

    request.Info.Width  = m_compBufInfo[i].CreationWidth;
    request.Info.Height = m_compBufInfo[i].CreationHeight;
    request.Info.FourCC = ownConvertD3DFMT_TO_MFX( (DXGI_FORMAT)(m_compBufInfo[i].CompressedFormats) ); // P8

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)

mfxStatus D3D11Encoder::QueryEncodeCaps(MFX_ENCODE_CAPS& caps)
{
    MFX_CHECK_WITH_ASSERT(m_pDecoder, MFX_ERR_NOT_INITIALIZED);

    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::QueryEncodeCaps(MFX_ENCODE_CAPS& caps)

mfxStatus D3D11Encoder::QueryMbPerSec(mfxVideoParam const & par, mfxU32 (&mbPerSec)[16])
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::QueryMbPerSec");
    HRESULT hRes;
    ENCODE_QUERY_PROCESSING_RATE_INPUT inPar;
    // Some encoding parameters affect MB processibng rate. Pass them to driver.
    // DDI driver programming notes require to send 0x(ff) for undefined parameters
    inPar.GopPicSize = par.mfx.GopPicSize ? par.mfx.GopPicSize : 0xffff;
    inPar.GopRefDist = (mfxU8)(par.mfx.GopRefDist ? par.mfx.GopRefDist : 0xff);
    inPar.Level = (mfxU8)(par.mfx.CodecLevel ? par.mfx.CodecLevel : 0xff);
    inPar.Profile = (mfxU8)(par.mfx.CodecProfile ? par.mfx.CodecProfile : 0xff);
    inPar.TargetUsage = (mfxU8)(par.mfx.TargetUsage ? par.mfx.TargetUsage : 0xff);
    D3D11_VIDEO_DECODER_EXTENSION decoderExtParam;
    decoderExtParam.Function = ENCODE_QUERY_MAX_MB_PER_SEC_ID;
    decoderExtParam.pPrivateInputData = &inPar;
    decoderExtParam.PrivateInputDataSize = sizeof(ENCODE_QUERY_PROCESSING_RATE_INPUT);
    decoderExtParam.pPrivateOutputData = &mbPerSec[0];
    decoderExtParam.PrivateOutputDataSize = sizeof(mfxU32) * 16;
    decoderExtParam.ResourceCount = 0;
    decoderExtParam.ppResourceList = 0;

    hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParam);
    CHECK_HRES(hRes);

    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::QueryEncCtrlCaps(ENCODE_ENC_CTRL_CAPS& caps)
{
    MFX_CHECK_WITH_ASSERT(m_pDecoder, MFX_ERR_NOT_INITIALIZED);

    caps = m_capsQuery;
    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::GetEncCtrlCaps(ENCODE_ENC_CTRL_CAPS& caps)
{
    MFX_CHECK_WITH_ASSERT(m_pDecoder, MFX_ERR_NOT_INITIALIZED);

    caps = m_capsGet;
    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::SetEncCtrlCaps(ENCODE_ENC_CTRL_CAPS const & caps)
{
    MFX_CHECK_WITH_ASSERT(m_pDecoder, MFX_ERR_NOT_INITIALIZED);

    m_capsGet = caps; // DDI spec: "The application should use the same structure
                      // returned in a previous ENCODE_ENC_CTRL_GET_ID command."

    D3D11_VIDEO_DECODER_EXTENSION decoderExtParam = {};
    decoderExtParam.Function = ENCODE_ENC_CTRL_SET_ID;
    decoderExtParam.pPrivateInputData = &m_capsGet;
    decoderExtParam.PrivateInputDataSize = sizeof(m_capsGet);

    HRESULT hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParam);
    CHECK_HRES(hRes);
    return MFX_ERR_NONE;
}

mfxStatus D3D11Encoder::QueryStatus(DdiTask & task, mfxU32 fieldId, bool useEvent)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::QueryStatus");
    mfxStatus sts = MFX_ERR_NONE;

    {
        sts = D3DXCommonEncoder::QueryStatus(task, fieldId, useEvent);
    }

    return sts;
}

mfxStatus D3D11Encoder::Register(mfxFrameAllocResponse & response, D3DDDIFORMAT type)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::Register");
    // we should register allocated HW bitstreams and recon surfaces
    std::vector<mfxHDLPair> & queue = (type == D3DDDIFMT_NV12) ?
        m_reconQueue: (type == D3DDDIFMT_INTELENCODE_MBQPDATA) ?
        m_mbqpQueue :
        m_bsQueue;

    queue.resize(response.NumFrameActual);

    //wo_d3d11
    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxHDLPair handlePair;

        //mfxStatus sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&handlePair);

        mfxStatus sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&handlePair);
        MFX_CHECK_STS(sts);

        queue[i] = handlePair;
    }

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        // reserved space for feedback reports
        m_feedbackUpdate.resize( response.NumFrameActual );
        m_feedbackCached.Reset( response.NumFrameActual );

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        m_EventCache->Init(response.NumFrameActual);
#endif
    }

    return MFX_ERR_NONE;

} // mfxStatus D3D11Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)


mfxStatus D3D11Encoder::ExecuteImpl(
    mfxHDLPair                 pair,
    DdiTask const &            task,
    mfxU32                     fieldId,
    PreAllocatedVector const & sei)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::Execute");
    m_packedSei = { 0 };

    ID3D11Resource * pSurface = static_cast<ID3D11Resource *>(pair.first);
    UINT subResourceIndex = (UINT)(UINT_PTR)(pair.second);

    HRESULT hr = S_OK;
    UCHAR   SkipFlag = task.SkipFlag();
    mfxU32  skipMode = m_skipMode;
    mfxExtCodingOption2* ctrlOpt2 = GetExtBuffer(task.m_ctrl, MFX_EXTBUFF_CODING_OPTION2);

    if (ctrlOpt2 && ctrlOpt2->SkipFrame <= MFX_SKIPFRAME_BRC_ONLY)
        skipMode = ctrlOpt2->SkipFrame;

    if (skipMode == MFX_SKIPFRAME_BRC_ONLY)
    {
        SkipFlag = 0; // encode current frame as normal
        m_numSkipFrames += (mfxU8)task.m_ctrl.SkipFrame;
    }


    // Execute()

    // mvc hack
    // base view has separate sps/pps
    // all other views share another sps/pps
    mfxU8 initSpsId = m_sps.seq_parameter_set_id;
    mfxU8 initPpsId = m_pps.pic_parameter_set_id;
    //if (task.m_viewIdx != 0)
    {
        m_sps.seq_parameter_set_id = mfxU8((initSpsId + !!task.m_viewIdx) & 0x1f);
        m_pps.pic_parameter_set_id = mfxU8((initPpsId + !!task.m_viewIdx));
        m_pps.seq_parameter_set_id = m_sps.seq_parameter_set_id;
    }

    if (m_sps.UserMaxIFrameSize != task.m_maxIFrameSize)
    {
        m_sps.UserMaxIFrameSize = (UINT)task.m_maxIFrameSize;
        if (task.m_frameOrder)
            m_sps.bResetBRC = true;
    }

    if (m_sps.UserMaxPBFrameSize != task.m_maxPBFrameSize)
    {
        m_sps.UserMaxPBFrameSize = (UINT)task.m_maxPBFrameSize;
    }

    if (task.m_resetBRC && (task.m_type[fieldId] & MFX_FRAMETYPE_IDR))
        m_sps.bResetBRC = true;

    // update sps, pps and slice structures
    {
        size_t slice_size_old = m_slice.size();
        FillVaringPartOfSpsBuffer(m_sps, task, fieldId);

        FillVaringPartOfPpsBuffer(task, fieldId, m_pps, m_dirtyRects, m_movingRects
#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
            , m_headerPacker.GetPackedCqmPpsNum()
#endif
        );

        if (task.m_SliceInfo.size())
            FillVaringPartOfSliceBufferSizeLimited(m_caps, task, fieldId, m_sps, m_pps, m_slice);
        else
            FillVaringPartOfSliceBuffer(m_caps, task, fieldId, m_sps, m_pps,m_slice);

        if (slice_size_old != m_slice.size())
        {
            m_compBufDesc.resize(10 + m_slice.size());
            m_pps.NumSlice = mfxU8(m_slice.size());
        }
    }
    // prepare resource list
    // it contains resources in video memory that needed for the encoding operation
    mfxU32 *RES_ID_BITSREAM = &m_idBit;          // bitstream surface takes first place in resourceList, always zero, initialized in constructor
    mfxU32 RES_ID_ORIGINAL = 1;
    mfxU32 RES_ID_REFERENCE = 2;          // then goes all reference frames from dpb
    mfxU32 RES_ID_RECONSTRUCT = RES_ID_REFERENCE + task.m_idxRecon;
    mfxU32 *RES_ID_MBQP = &m_idMBQP;
    m_idMBQP = RES_ID_REFERENCE + (mfxU32)m_reconQueue.size();
    m_resourceCount = mfxU32(RES_ID_REFERENCE + m_reconQueue.size() + task.m_isMBQP);

    if(m_resourceCount > m_resourceList.size())
        m_resourceList.resize(m_resourceCount);

    m_resourceList[m_idBit] = static_cast<ID3D11Resource *>(m_bsQueue[task.m_idxBs[fieldId]].first); //Bitsream
    m_resourceList[RES_ID_ORIGINAL] = pSurface;

    if (task.m_isMBQP)
        m_resourceList[m_idMBQP]      = static_cast<ID3D11Resource *>(m_mbqpQueue[task.m_idxMBQP].first);

    for (mfxU32 i = 0; i < m_reconQueue.size(); i++)
    {
        m_resourceList[RES_ID_REFERENCE + i] = static_cast<ID3D11Resource *>(m_reconQueue[i].first);
    }

    // [1]. buffers in system memory (configuration buffers)
    //const mfxU32 NUM_COMP_BUFFER = 10;
    //ENCODE_COMPBUFFERDESC encodeCompBufferDesc[NUM_COMP_BUFFER];
    m_encodeExecuteParams = { 0 };
    m_encodeExecuteParams.NumCompBuffers = 0;
    m_encodeExecuteParams.pCompressedBuffers = Begin(m_compBufDesc);

    // FIXME: need this until production driver moves to DDI 0.87
    m_encodeExecuteParams.pCipherCounter                     = 0;
    m_encodeExecuteParams.PavpEncryptionMode.eCounterMode    = 0;

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    m_encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;
#else
    m_encodeExecuteParams.PavpEncryptionMode.eEncryptionType = 1; // none
#endif

    m_sps.bNoAccelerationSPSInsertion = !task.m_insertSps[fieldId];

    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA);
    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize             = mfxU32(sizeof(m_sps));
    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer          = &m_sps;
    m_encodeExecuteParams.NumCompBuffers++;

    if (m_sps.vui_parameters_present_flag)
    {
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_VUIDATA);
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize             = mfxU32(sizeof(m_vui));
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer          = &m_vui;
        m_encodeExecuteParams.NumCompBuffers++;
    }

    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA);
    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize             = mfxU32(sizeof(m_pps));
    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer          = &m_pps;

    m_encodeInputDesc = { 0 };
    m_encodeInputDesc.ArraSliceOriginal = subResourceIndex;
    m_encodeInputDesc.IndexOriginal     = RES_ID_ORIGINAL;
    m_encodeInputDesc.ArraySliceRecon   = (UINT)(size_t(m_reconQueue[task.m_idxRecon].second));
    m_encodeInputDesc.IndexRecon        = RES_ID_RECONSTRUCT;
    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pReserved     = &m_encodeInputDesc;
    m_encodeExecuteParams.NumCompBuffers++;

#ifdef MFX_ENABLE_AVC_CUSTOM_QMATRIX
    if (m_pps.pic_scaling_matrix_present_flag)
    {
        m_qMatrix = task.m_qMatrix;
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA);
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize = mfxU32(sizeof(m_qMatrix));
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer = &m_qMatrix;
        m_pps.pic_scaling_list_present_flag = true;
        m_pps.pic_scaling_matrix_present_flag = true;
        m_encodeExecuteParams.NumCompBuffers++;
    }
    else if (m_sps.seq_scaling_matrix_present_flag)
    {
        m_qMatrix = task.m_qMatrix;
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA);
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize = mfxU32(sizeof(m_qMatrix));
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer = &m_qMatrix;
        m_sps.seq_scaling_list_present_flag = true;
        m_sps.seq_scaling_matrix_present_flag = true;
        m_encodeExecuteParams.NumCompBuffers++;
    }
#endif

    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA);
    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize             = mfxU32(sizeof(m_slice[0]) * m_slice.size());
    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer          = &m_slice[0];
    m_encodeExecuteParams.NumCompBuffers++;

    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA);
    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize             = mfxU32(sizeof(m_idBit));
    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer          = RES_ID_BITSREAM;
    m_encodeExecuteParams.NumCompBuffers++;

    if (task.m_isMBQP)
    {
        const mfxExtMBQP *mbqp = task.m_mbqp;
        mfxU32 wMB = (m_sps.FrameWidth + 15) / 16;
        mfxU32 hMB = (m_sps.FrameHeight + 15) / 16 / (2 - !task.m_fieldPicFlag);
        mfxU32 fieldOffset = (mfxU32)fieldId * (wMB * hMB) * (mfxU32)!!task.m_fieldPicFlag;

        mfxFrameData qpsurf = {};
        FrameLocker lock(m_core, qpsurf, task.m_midMBQP);

        MFX_CHECK_WITH_ASSERT(qpsurf.Y, MFX_ERR_LOCK_MEMORY);
        if (mbqp && mbqp->QP && mbqp->NumQPAlloc >= wMB * hMB) {
            for (mfxU32 i = 0; i < hMB; i++)
                MFX_INTERNAL_CPY(&qpsurf.Y[i * qpsurf.Pitch], &mbqp->QP[fieldOffset + i * wMB], wMB);
        }
#ifdef MFX_ENABLE_APQ_LQ
        else if(task.m_ALQOffset) {
            for (mfxU32 i = 0; i < hMB; i++)
                for (mfxU32 j = 0; j < wMB; j++)
                    qpsurf.Y[i * qpsurf.Pitch + j] = (mfxU8)mfx::clamp(task.m_ALQOffset + task.m_cqpValue[0], 1, 51);
        }
#endif
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA;
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize             = mfxU32(sizeof(m_idMBQP));
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer          = RES_ID_MBQP;
        m_encodeExecuteParams.NumCompBuffers++;
    }

    if (task.m_isMBControl)
    {
        mfxU32 wMB = (m_sps.FrameWidth + 15) / 16;
        mfxU32 hMB = (m_sps.FrameHeight + 15) / 16;

        mfxFrameData mbsurf = {};
        FrameLocker lock(m_core, mbsurf, task.m_midMBControl);

        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)convertDX9TypeToDX11Type(D3DDDIFMT_INTELENCODE_MBCONTROL);
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize = wMB*hMB * sizeof(ENCODE_MBCONTROL);
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer = mbsurf.Y;
        m_encodeExecuteParams.NumCompBuffers++;
    }

    if (task.m_insertAud[fieldId])
    {
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer          = RemoveConst(&m_headerPacker.PackAud(task, fieldId));
        m_encodeExecuteParams.NumCompBuffers++;
    }

    if (task.m_insertSps[fieldId])
    {
        std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSps = m_headerPacker.GetSps();
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer          = RemoveConst(&packedSps[!!task.m_viewIdx]);
        m_encodeExecuteParams.NumCompBuffers++;
    }

    if (task.m_insertPps[fieldId])
    {
        std::vector<ENCODE_PACKEDHEADER_DATA> const & packedPps = m_headerPacker.GetPps();
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer          = RemoveConst(&packedPps[!!task.m_viewIdx]);
        m_encodeExecuteParams.NumCompBuffers++;

#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
        if (IS_CUST_MATRIX(task.m_adaptiveCQMHint))
        {
            mfxU32 extCqmNum = m_headerPacker.GetPackedCqmPpsNum();
            if (extCqmNum > 0)
            {
                std::vector<ENCODE_PACKEDHEADER_DATA> const & packedCqmPps = m_headerPacker.GetPps(true);
                mfxU32 cqmIndex = 0;
                while (cqmIndex < extCqmNum)
                {
                    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
                    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
                    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer = RemoveConst(&packedCqmPps[cqmIndex]);
                    m_encodeExecuteParams.NumCompBuffers++;
                    cqmIndex++;
                }
            }
        }
#endif
    }

    if (sei.Size() > 0)
    {
        m_packedSei.pData                  = RemoveConst(sei.Buffer());
        m_packedSei.BufferSize             = sei.Size();
        m_packedSei.DataLength             = sei.Size();
        m_packedSei.SkipEmulationByteCount = 0; // choose not to let accelerator insert emulation byte

        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer          = &m_packedSei;
        m_encodeExecuteParams.NumCompBuffers++;
    }

    if (SkipFlag != 0)
    {
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
        m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer          = RemoveConst(&m_headerPacker.PackSkippedSlice(task, fieldId));
        m_encodeExecuteParams.NumCompBuffers++;

        if (SkipFlag == 2 && skipMode != MFX_SKIPFRAME_INSERT_NOTHING)
        {
            m_sizeSkipFrames = 0;

            for (UINT i = 0; i < m_encodeExecuteParams.NumCompBuffers; i++)
            {
                if (m_compBufDesc[i].CompressedBufferType == (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA)
                {
                    ENCODE_PACKEDHEADER_DATA const & data = *(ENCODE_PACKEDHEADER_DATA*)m_compBufDesc[i].pCompBuffer;
                    m_sizeSkipFrames += data.DataLength;
                }
            }
        }
    }
    else
    {
        std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSlices = m_headerPacker.PackSlices(task, fieldId);
        for (mfxU32 i = 0; i < packedSlices.size(); i++)
        {
            m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].CompressedBufferType = (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA;
            m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
            m_compBufDesc[m_encodeExecuteParams.NumCompBuffers].pCompBuffer          = RemoveConst(&packedSlices[i]);
            m_encodeExecuteParams.NumCompBuffers++;
        }
    }

    assert(m_encodeExecuteParams.NumCompBuffers <= m_compBufDesc.size());
    m_compBufDesc[m_encodeExecuteParams.NumCompBuffers] = {};
#ifndef MFX_AVC_ENCODING_UNIT_DISABLE
    if (task.m_collectUnitsInfo)
    {
        m_headerPacker.GetHeadersInfo(task.m_headersCache[fieldId], task, fieldId);
    }
#endif

    if(SkipFlag != 1)
    {
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        {
            // allocate the event
            DdiTask & task1 = RemoveConst(task);
            task1.m_GpuEvent[fieldId].m_gpuComponentId = GPU_COMPONENT_ENCODE;
            m_EventCache->GetEvent(task1.m_GpuEvent[fieldId].gpuSyncEvent);

            D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = { 0 };
            decoderExtParams.Function = DXVA2_SET_GPU_TASK_EVENT_HANDLE;
            decoderExtParams.pPrivateInputData = &task1.m_GpuEvent[fieldId];
            decoderExtParams.PrivateInputDataSize = sizeof(task1.m_GpuEvent[fieldId]);
            decoderExtParams.pPrivateOutputData = nullptr;
            decoderExtParams.PrivateOutputDataSize = 0;
            decoderExtParams.ResourceCount = 0;
            decoderExtParams.ppResourceList = nullptr;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "SendGpuEventHandle");
                hr = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParams);
            }
            CHECK_HRES(hr);
        }
#endif

        m_pps.SkipFrameFlag  = SkipFlag ? SkipFlag : !!m_numSkipFrames;
        m_pps.NumSkipFrames  = m_numSkipFrames;
        m_pps.SizeSkipFrames = m_sizeSkipFrames;
        m_numSkipFrames      = 0;
        m_sizeSkipFrames     = 0;
        // [2.4] send to driver
        m_decoderExtParams = { 0 };
        m_decoderExtParams.Function              = ENCODE_ENC_PAK_ID;
        m_decoderExtParams.pPrivateInputData     = &m_encodeExecuteParams;
        m_decoderExtParams.PrivateInputDataSize  = sizeof(ENCODE_EXECUTE_PARAMS);
        m_decoderExtParams.pPrivateOutputData    = 0;
        m_decoderExtParams.PrivateOutputDataSize = 0;
        m_decoderExtParams.ResourceCount         = m_resourceCount;
        m_decoderExtParams.ppResourceList        = &(m_resourceList[0]);
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "Execute");
            hr = DecoderExtension(m_pVideoContext, m_pDecoder, m_decoderExtParams);
        }
        CHECK_HRES(hr);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "EndFrame");
            hr = m_pVideoContext->DecoderEndFrame(m_pDecoder);
        }
        CHECK_HRES(hr);
    }
    else
    {
        ENCODE_QUERY_STATUS_PARAMS feedback = {task.m_statusReportNumber[fieldId], 0,};

        if (skipMode != MFX_SKIPFRAME_INSERT_NOTHING)
        {
            mfxFrameData bs = {0};
            FrameLocker lock(m_core, bs, task.m_midBit[fieldId]);
            assert(bs.Y);

            mfxU8 *  bsDataStart = bs.Y;
            mfxU8 *  bsEnd       = bs.Y + m_sps.FrameWidth * m_sps.FrameHeight;
            mfxU8 *  bsDataEnd   = bsDataStart;

            for (UINT i = 0; i < m_encodeExecuteParams.NumCompBuffers; i++)
            {
                if (m_compBufDesc[i].CompressedBufferType == (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA)
                {
                    ENCODE_PACKEDHEADER_DATA const & data = *(ENCODE_PACKEDHEADER_DATA*)m_compBufDesc[i].pCompBuffer;
                    bsDataEnd += AddEmulationPreventionAndCopy(data, bsDataEnd, bsEnd, !!m_pps.bEmulationByteInsertion);
                }
            }
            feedback.bitstreamSize = mfxU32(bsDataEnd - bsDataStart);
        }

        m_feedbackCached.Update( CachedFeedback::FeedbackStorage(1, feedback) );

        m_numSkipFrames ++;
        m_sizeSkipFrames += feedback.bitstreamSize;
    }

    m_sps.bResetBRC = false;

    // mvc hack
    m_sps.seq_parameter_set_id = initSpsId;
    m_pps.seq_parameter_set_id = initSpsId;
    m_pps.pic_parameter_set_id = initPpsId;
    return MFX_ERR_NONE;

} //  mfxStatus D3D11Encoder::Execute(...)


mfxStatus D3D11Encoder::QueryStatusAsync(
    DdiTask & task,
    mfxU32    fieldId)
{
    mfxU32 curTime = vm_time_get_current_time();
    // After SNB once reported ENCODE_OK for a certain feedbackNumber
    // it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    // As we won't get all bitstreams we need to cache all other statuses.

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_statusReportNumber[fieldId]);
    for(int i=0; i< (int)m_feedbackUpdate.size(); i++)
        m_feedbackUpdate[i].bStatus = ENCODE_ERROR;
#ifdef NEW_STATUS_REPORTING_DDI_0915
    ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr;
    feedbackDescr.SizeOfStatusParamStruct = sizeof(m_feedbackUpdate[0]);
    feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;
#endif // NEW_STATUS_REPORTING_DDI_0915

    // if task is not in cache then query its status

    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {

        {
            try
            {

                D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = { 0 };

                decoderExtParams.Function              = ENCODE_QUERY_STATUS_ID;
                decoderExtParams.pPrivateInputData     = &feedbackDescr;
                decoderExtParams.PrivateInputDataSize  = sizeof(feedbackDescr);
                decoderExtParams.pPrivateOutputData    = &m_feedbackUpdate[0];
                decoderExtParams.PrivateOutputDataSize = mfxU32(m_feedbackUpdate.size() * sizeof(m_feedbackUpdate[0]));
                decoderExtParams.ResourceCount         = 0;
                decoderExtParams.ppResourceList        = 0;
                HRESULT hRes = 0;
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "ENCODE_QUERY_STATUS_ID");
                    hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParams);
                }
                MFX_CHECK(hRes != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
                MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
            }
            catch (...)
            {
                return MFX_ERR_DEVICE_FAILED;
            }
        }
        // Put all with ENCODE_OK into cache.
        m_feedbackCached.Update(m_feedbackUpdate);

        feedback = m_feedbackCached.Hit(task.m_statusReportNumber[fieldId]);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

#ifdef DEBUG_TRACE
    printf("\n\nstatus %d for stream %d, filed %d, feedback %d\n\n", feedback->bStatus, feedback->StreamId, fieldId, feedback->StatusReportFeedbackNumber);
#endif

    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        task.m_bsDataLength[fieldId] = feedback->bitstreamSize;
        task.m_qpY[fieldId] = feedback->AverageQP;
        if (m_capsGet.MAD)
            task.m_mad[fieldId] = feedback->MAD;
        task.m_resetBRC = !!feedback->reserved0; //WiDi w/a
        //for KBL we need retrive counter from HW instead of incrementing ourselfs.
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
        if(m_caps.ddi_caps.HWCounterAutoIncrement && (m_forcedCodingFunction & ENCODE_WIDI))
        {
            task.m_aesCounter[0].Count = feedback->aes_counter.Counter;
            task.m_aesCounter[0].IV = feedback->aes_counter.IV;
        }
#endif
        m_feedbackCached.Remove(task.m_statusReportNumber[fieldId]);
        return MFX_ERR_NONE;

    case ENCODE_NOTREADY:
        if (m_timeoutForTDR && task.CheckForTDRHang(curTime, m_timeoutForTDR))
            return MFX_ERR_GPU_HANG;
        else
            return MFX_WRN_DEVICE_BUSY;

    case ENCODE_NOTAVAILABLE:
    case ENCODE_ERROR:
    default:
        assert(!"bad feedback status");
        return MFX_ERR_DEVICE_FAILED;
    }
} // mfxStatus D3D11Encoder::QueryStatus( DdiTask & task, mfxU32    fieldId)

mfxStatus D3D11Encoder::QueryHWGUID(
            VideoCORE * core,
            GUID        guid,
            bool        isTemporal)
{
    D3D11_VIDEO_DECODER_DESC video_desc = {0};
    D3D11_VIDEO_DECODER_CONFIG video_config = {0};

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::QueryHWGUID");
    MFX_CHECK_NULL_PTR1(core);

    D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(core);
    MFX_CHECK_NULL_PTR1(pD3d11);

    ID3D11VideoDevice *pVideoDevice = pD3d11->GetD3D11VideoDevice(isTemporal);
    MFX_CHECK_NULL_PTR1(pVideoDevice);

    // [1] Query supported decode profiles
    UINT profileCount = pVideoDevice->GetVideoDecoderProfileCount();
    assert( profileCount > 0 );

    bool EncodeGUIDFound = false;
    bool PrivateIntelGUIDFound = false;


    GUID profileGuid;
    for( UINT indxProfile = 0; indxProfile < profileCount; indxProfile++ )
    {
        HRESULT hRes = pVideoDevice->GetVideoDecoderProfile(indxProfile, &profileGuid);
        CHECK_HRES(hRes);

        if(guid == profileGuid)
            EncodeGUIDFound = true;

        if (profileGuid == DXVADDI_Intel_Decode_PrivateData_Report)
            PrivateIntelGUIDFound = true;
    }

    if (EncodeGUIDFound && PrivateIntelGUIDFound)
    {
        video_desc.Guid = DXVADDI_Intel_Decode_PrivateData_Report;
        video_desc.OutputFormat = DXGI_FORMAT_NV12;
        video_desc.SampleWidth = 640;
        video_desc.SampleHeight = 480;

        mfxU32  count = 0;
        HRESULT hr = pVideoDevice->GetVideoDecoderConfigCount(&video_desc, &count);
        CHECK_HRES(hr);

        for (mfxU32 i = 0; i < count; i++)
        {
            hr = pVideoDevice->GetVideoDecoderConfig(&video_desc, i, &video_config);
            CHECK_HRES(hr);

            if (video_config.guidConfigBitstreamEncryption == guid)
            {
                if (video_config.ConfigDecoderSpecific & 0x1) // WiDi usage model - not supported by MFT
                    return MFX_WRN_PARTIAL_ACCELERATION;
                else
                   return MFX_ERR_NONE;
            }

       }
    }
    return MFX_WRN_PARTIAL_ACCELERATION;

} // mfxStatus D3D11Encoder::QueryHWGUID(...)



mfxStatus D3D11Encoder::Destroy()
{
    if (m_requestedGuid == MSDK_Private_Guid_Encode_MVC_Dependent_View)
    {
        // Encoding device was used for MVC dependent view and isn't stored in core.
        // Need to release it here since core knows nothing about it and doesn't release it itself.
        SAFE_RELEASE(m_pDecoder);
    }
    return MFX_ERR_NONE;
} // mfxStatus D3D11Encoder::Destroy()


mfxStatus D3D11Encoder::Init(
    GUID guid,
    ID3D11VideoDevice *pVideoDevice,
    ID3D11VideoContext *pVideoContext,
    mfxU32      width,
    mfxU32      height,
    mfxExtPAVPOption const * extPavp)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::Init");
    MFX_CHECK_NULL_PTR2(pVideoDevice, pVideoContext);
    mfxStatus sts = MFX_ERR_NONE;
    m_pVideoDevice  = pVideoDevice;
    m_pVideoContext = pVideoContext;

    if (   guid == MSDK_Private_Guid_Encode_AVC_Query
        || guid == MSDK_Private_Guid_Encode_MVC_Dependent_View)
        m_guid = DXVA2_Intel_Encode_AVC;
    else if(guid == MSDK_Private_Guid_Encode_AVC_LowPower_Query)
        m_guid = DXVA2_INTEL_LOWPOWERENCODE_AVC;
    else
        m_guid = guid;

    m_requestedGuid = guid;
    HRESULT hRes;

    // To avoid multiple re-creation of AVC encoding device for DX11 it's stored in Core since it was created first time.

    // Device creation for MVC dependent view requires special processing for DX11.
    // For MVC dependent view encoding device from Core can't be used (it's already used for base view). Need to create new one.
    // Also newly created encoding device for MVC dependent view shouldn't be stored inside Core.
    bool bUseDecoderInCore = (guid != MSDK_Private_Guid_Encode_MVC_Dependent_View);

    ComPtrCore<ID3D11VideoDecoder>* pVideoDecoder = QueryCoreInterface<ComPtrCore<ID3D11VideoDecoder>>(m_core, MFXID3D11DECODER_GUID);
    MFX_CHECK(pVideoDecoder, MFX_ERR_UNDEFINED_BEHAVIOR);

    D3D11_VIDEO_DECODER_DESC video_desc = {};
    D3D11_VIDEO_DECODER_CONFIG video_config = {};
    m_pDecoder = nullptr;

    if (pVideoDecoder->get() && bUseDecoderInCore)
    {
        hRes = pVideoDecoder->get()->GetCreationParameters(&video_desc, &video_config);
        CHECK_HRES(hRes);
        // video decoder from Core should be used if it has been created with same resolution
        // also decoder from Core created with lower resolution could be used for Query, QueryIOSurf (but not for Init)
        // for PAVP case new instance of decoder should be created
        if ((width <= video_desc.SampleWidth && height <= video_desc.SampleHeight
            || guid == MSDK_Private_Guid_Encode_AVC_Query
            || guid == MSDK_Private_Guid_Encode_AVC_LowPower_Query)
            && !extPavp
            && video_desc.Guid == m_guid)
            m_pDecoder = pVideoDecoder->get();
    }
    if (!m_pDecoder)
    {
        // [1] Query supported decode profiles
        UINT profileCount = m_pVideoDevice->GetVideoDecoderProfileCount( );
        assert( profileCount > 0 );

        bool isFound = false;
        GUID profileGuid;
        for( UINT indxProfile = 0; indxProfile < profileCount; indxProfile++ )
        {
            hRes = m_pVideoDevice->GetVideoDecoderProfile(indxProfile, &profileGuid);
            CHECK_HRES(hRes);
            if( m_guid == profileGuid )
            {
                isFound = true;
                break;
            }
        }

        if( !isFound )
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        // [2] Query the supported encode functions
        video_desc.SampleWidth  = width;
        video_desc.SampleHeight = height;
        video_desc.OutputFormat = DXGI_FORMAT_NV12;
        video_desc.Guid = m_guid;

        mfxU32 count;

        hRes = m_pVideoDevice->GetVideoDecoderConfigCount(&video_desc, &count);
        CHECK_HRES(hRes);

        // [2] Calling other D3D11 Video Decoder API (as for normal proc)

        // [4] CreateVideoDecoder
        // D3D11_VIDEO_DECODER_DESC video_desc;
        video_desc.SampleWidth  = width;
        video_desc.SampleHeight = height;
        video_desc.OutputFormat = DXGI_FORMAT_NV12;
        video_desc.Guid = m_guid;

        // D3D11_VIDEO_DECODER_CONFIG video_config;
        video_config.ConfigDecoderSpecific = m_forcedCodingFunction ? m_forcedCodingFunction : ENCODE_ENC_PAK;
        video_config.guidConfigBitstreamEncryption = (extPavp) ? DXVA2_INTEL_PAVP : DXVA_NoEncrypt;

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "CreateVideoDecoder");
            hRes  = m_pVideoDevice->CreateVideoDecoder(&video_desc, &video_config, &m_pDecoder);
        }
        CHECK_HRES(hRes);
        if (bUseDecoderInCore)
            *pVideoDecoder = m_pDecoder;
    }

    // ENCRYPTION
    if(extPavp)
    {
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
        D3D11_AES_CTR_IV         initialCounter = { extPavp->CipherCounter.IV, extPavp->CipherCounter.Count };
        PAVP_ENCRYPTION_MODE  encryptionMode = { extPavp->EncryptionType,   extPavp->CounterType         };
        ENCODE_ENCRYPTION_SET encryptSet     = { extPavp->CounterIncrement, &initialCounter, &encryptionMode};

        D3D11_VIDEO_DECODER_EXTENSION encryptParam;
        encryptParam.Function = ENCODE_ENCRYPTION_SET_ID;
        encryptParam.pPrivateInputData     = &encryptSet;
        encryptParam.PrivateInputDataSize  = sizeof(ENCODE_ENCRYPTION_SET);
        encryptParam.pPrivateOutputData    = 0;
        encryptParam.PrivateOutputDataSize = 0;
        encryptParam.ResourceCount         = 0;
        encryptParam.ppResourceList        = 0;

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "ENCRYPTION");
            hRes = DecoderExtension(m_pVideoContext, m_pDecoder, encryptParam);
        }
#else
        hRes = ERROR_UNSUPPORTED_TYPE;
#endif
        CHECK_HRES(hRes);
    }
    {
        // [3] Query the encoding device capabilities
        D3D11_VIDEO_DECODER_EXTENSION ext = {};
        ext.Function = ENCODE_QUERY_ACCEL_CAPS_ID;
        ext.pPrivateOutputData = &m_caps.ddi_caps;
        ext.PrivateOutputDataSize = sizeof(ENCODE_CAPS);

        hRes = DecoderExtension(m_pVideoContext, m_pDecoder, ext);
        //after prev line m_caps set ddi_part, so change our part of caps
        m_caps.CQPSupport = 1;
        m_caps.CBRSupport = 1;
        m_caps.VBRSupport = 1;
        m_caps.AVBRSupport = 1;
#ifndef STRIP_EMBARGO
        if (m_core->GetHWType() >= MFX_HW_DG2)
            m_caps.AVBRSupport = 0;
#endif
        m_caps.AdaptiveMaxFrameSizeSupport = 1;

        CHECK_HRES(hRes);
    }

    if (m_guid == DXVA2_INTEL_LOWPOWERENCODE_AVC)
    {
        // Query the version of ddi
        mfxU32 ddi_version;
        D3D11_VIDEO_DECODER_EXTENSION ext = {};
        ext.Function = ENCODE_QUERY_DRIVER_VERSION_ID;
        ext.pPrivateOutputData = &ddi_version;
        ext.PrivateOutputDataSize = sizeof(ddi_version);

        hRes = DecoderExtension(m_pVideoContext, m_pDecoder, ext);
        CHECK_HRES(hRes);

        EncodeDdiVersion* encDdiVersion = QueryCoreInterface<EncodeDdiVersion>(m_core);
        MFX_CHECK(encDdiVersion, MFX_ERR_UNDEFINED_BEHAVIOR);

        sts = encDdiVersion->SetDdiVersion(MFX_CODEC_AVC, ddi_version);
        MFX_CHECK_STS(sts);
    }

    D3DXCommonEncoder::Init(m_core);
    // [6] specific encoder caps

    // [7] Query encode service caps: see QueryCompBufferInfo


    return sts;

} // void D3D11Encoder::PackSlice(...)


mfxU16 ownConvertD3DFMT_TO_MFX(DXGI_FORMAT format)
{
    switch( format )
    {
    case  DXGI_FORMAT_P8:
        return MFX_FOURCC_P8;

    default:
        //return (mfxU16)MFX_FOURCC_NV12;
        return 0;
    }

} // mfxU16 ownConvertD3DFMT_TO_MFX(DXGI_FORMAT format)

mfxU8 convertDX9TypeToDX11Type(mfxU8 type)
{
    switch(type)
    {
    case D3DDDIFMT_INTELENCODE_SPSDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA;
    case D3DDDIFMT_INTELENCODE_PPSDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA;
    case D3DDDIFMT_INTELENCODE_SLICEDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA;
    case D3DDDIFMT_INTELENCODE_QUANTDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA;
    case D3DDDIFMT_INTELENCODE_BITSTREAMDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA;
    case D3DDDIFMT_INTELENCODE_MBDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA;
    case D3DDDIFMT_INTELENCODE_SEIDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_SEIDATA;
    case D3DDDIFMT_INTELENCODE_VUIDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_VUIDATA;
    case D3DDDIFMT_INTELENCODE_VMESTATE:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_VMESTATE;
    case D3DDDIFMT_INTELENCODE_VMEINPUT:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEINPUT;
    case D3DDDIFMT_INTELENCODE_VMEOUTPUT:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEOUTPUT;
    case D3DDDIFMT_INTELENCODE_EFEDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_EFEDATA;
    case D3DDDIFMT_INTELENCODE_STATDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_STATDATA;
    case D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
    case D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA;
    case D3DDDIFMT_INTELENCODE_MBQPDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA;
    case D3DDDIFMT_INTELENCODE_MBCONTROL:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_MBCONTROL;
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    case D3DDDIFMT_INTELENCODE_SYNCOBJECT:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_SYNCOBJECT;
#endif
    default:
        break;
    }
    return 0xFF;
}


namespace
{


    mfxU32 FindLayerBrcParam(mfxExtSVCRateControl const & extRc, mfxU32 did, mfxU32 qid, mfxU32 tid)
    {
        for (mfxU32 i = 0; i < 1024; i++)
            if (extRc.Layer[i].DependencyId == did &&
                extRc.Layer[i].QualityId    == qid &&
                extRc.Layer[i].TemporalId   == tid)
                return i;
        return 0xffffffff;
    };
};


D3D11SvcEncoder::D3D11SvcEncoder()
: m_core(0)
, m_pVideoDevice(0)
, m_pVideoContext(0)
, m_pDecoder(0)
, m_infoQueried(false)
, m_pVDOView(0)
, m_forcedCodingFunction(0)
, m_caps()
, m_packedSpsPps()
, m_packedPps()
, m_extSvc()
, m_numdl()
, m_numql()
, m_numtl()
{
}

D3D11SvcEncoder::~D3D11SvcEncoder()
{
    Destroy();
}

mfxStatus D3D11SvcEncoder::CreateAuxilliaryDevice(
    VideoCORE * core,
    GUID        guid,
    mfxU32      width,
    mfxU32      height,
    bool        isTemporal)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11SvcEncoder::CreateAuxilliaryDevice");
    MFX_CHECK_NULL_PTR1(core);

    D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(core);

    MFX_CHECK_NULL_PTR1(pD3d11);

    mfxStatus sts = Init(
        guid,
        pD3d11->GetD3D11VideoDevice(isTemporal),
        pD3d11->GetD3D11VideoContext(isTemporal),
        width,
        height);

    m_core = core;
    return sts;
}

mfxStatus D3D11SvcEncoder::CreateAccelerationService(
    MfxVideoParam const & par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11SvcEncoder::CreateAccelerationService");
    mfxExtSpsHeader *  extSps = GetExtBuffer(par);
    mfxExtPpsHeader *  extPps = GetExtBuffer(par);
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    mfxExtSVCSeqDesc * extSvc = GetExtBuffer(par);
    m_extSvc = *extSvc;
#endif

    //m_numdl = par.calcParam.numDependencyLayer;
    //m_numql = par.calcParam.maxNumQualityLayer;
    //m_numtl = par.calcParam.numTemporalLayer;

    m_sps.resize(m_numdl);
    m_pps.resize(m_numdl * m_numql * m_numtl);
    m_slice.resize(par.mfx.NumSlice);
    m_packedSlice.resize(par.mfx.NumSlice);

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    Zero(m_packedSlice);
    Zero(m_packedSpsPps);
    Zero(m_packedPps);

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    for (mfxU32 d = 0; d < m_numdl; d++)
    {
        //::FillSpsBuffer(par, m_sps[d], d); // BUILD DX11 FIX
        mfxU32 did = par.calcParam.did[d]; // BUILD DX11 FIX

        for (mfxU32 q = 0; q < extSvc->DependencyLayer[d].QualityNum; q++)
        {
            for (mfxU32 t = 0; t < extSvc->DependencyLayer[d].TemporalNum; t++)
            {
                mfxU32 ppsid = d * m_numql * m_numtl + q * m_numtl + t;
                mfxU32 tid   = extSvc->DependencyLayer[did].TemporalId[t]; // BUILD DX11 FIX
                //::FillSpsBuffer(par, m_sps[d], d); // BUILD DX11 FIX
                MFX_CHECK_STS(::FillSpsBuffer(par, m_sps[ppsid], did, q, tid)); // BUILD DX11 FIX
                //::FillConstPartOfPpsBuffer(par, m_sps[d], m_pps[ppsid], d, q, t, ppsid); // BUILD DX11 FIX
                ::FillConstPartOfPpsBuffer(par, m_pps[ppsid], did, q, tid); // BUILD DX11 FIX
            }
        }
    }
#endif

    ::FillConstPartOfSliceBuffer(par, m_slice);

    // pack sps and pps nal units here
    // they will not change

    OutputBitstream obs(
        m_packedSpsPpsBuffer,
        m_packedSpsPpsBuffer + MAX_PACKED_SPSPPS_SIZE,
        true); // pack with emulation control

    WriteSpsHeader(obs, *extSps); // base layer sps header
    mfxExtSpsHeader copySps = *extSps;
    for (mfxU32 i = 0; i < par.calcParam.numDependencyLayer; i++)
    {
        copySps.seqParameterSetId         = mfxU8((copySps.seqParameterSetId + 1) % 32);
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
        copySps.picWidthInMbsMinus1       = extSvc->DependencyLayer[par.calcParam.did[i]].Width / 16 - 1;
        copySps.picHeightInMapUnitsMinus1 = extSvc->DependencyLayer[par.calcParam.did[i]].Height / 16 / (2 - copySps.frameMbsOnlyFlag) - 1;

        mfxExtSpsSvcHeader extSvcHead = {};
        InitExtBufHeader(extSvcHead);

        extSvcHead.interLayerDeblockingFilterControlPresentFlag = 1;
        extSvcHead.extendedSpatialScalabilityIdc                = 1;
        extSvcHead.chromaPhaseXPlus1Flag                        = 0;
        extSvcHead.chromaPhaseYPlus1                            = 0;
        extSvcHead.seqRefLayerChromaPhaseXPlus1Flag             = 0;
        extSvcHead.seqRefLayerChromaPhaseYPlus1                 = 0;
        extSvcHead.seqScaledRefLayerLeftOffset                  = mfxI16(extSvc->DependencyLayer[i].ScaledRefLayerOffsets[0]);
        extSvcHead.seqScaledRefLayerTopOffset                   = mfxI16(extSvc->DependencyLayer[i].ScaledRefLayerOffsets[1]);
        extSvcHead.seqScaledRefLayerRightOffset                 = mfxI16(extSvc->DependencyLayer[i].ScaledRefLayerOffsets[2]);
        extSvcHead.seqScaledRefLayerBottomOffset                = mfxI16(extSvc->DependencyLayer[i].ScaledRefLayerOffsets[3]);
        extSvcHead.sliceHeaderRestrictionFlag                   = 0;

        extSvcHead.seqTcoeffLevelPredictionFlag                 = mfxU8(extSvc->DependencyLayer[i].QualityLayer[0].TcoeffPredictionFlag);
        extSvcHead.adaptiveTcoeffLevelPredictionFlag            = 1;
        WriteSpsHeader(obs, *extSps, extSvcHead.Header);
#endif
    }

    mfxU32 spsHeaderLength = (obs.GetNumBits() + 7) / 8;

    WritePpsHeader(obs, *extPps); // base layer pps header
    mfxExtPpsHeader copyPps = *extPps;
    for (mfxU32 i = 0; i < par.calcParam.numDependencyLayer; i++)
    {
        copyPps.seqParameterSetId = mfxU8((copyPps.seqParameterSetId + 1) % 32);
        copyPps.picParameterSetId = copyPps.seqParameterSetId;
        WritePpsHeader(obs, *extPps);
    }

    m_packedSpsPps.pData                  = m_packedSpsPpsBuffer;
    m_packedSpsPps.BufferSize             = MAX_PACKED_SPSPPS_SIZE;
    m_packedSpsPps.DataLength             = (obs.GetNumBits() + 7) / 8;
    m_packedSpsPps.SkipEmulationByteCount = 0; // choose not to let accelerator insert emulation byte

    m_packedPps.pData                  = m_packedSpsPps.pData      + spsHeaderLength;
    m_packedPps.BufferSize             = m_packedSpsPps.BufferSize - spsHeaderLength;
    m_packedPps.DataLength             = m_packedSpsPps.DataLength - spsHeaderLength;
    m_packedPps.SkipEmulationByteCount = 0; // choose not to let accelerator insert emulation byte

    return MFX_ERR_NONE;
}

mfxStatus D3D11SvcEncoder::Reset(
    MfxVideoParam const & par)
{
    par;
    return MFX_ERR_NONE;
}

mfxStatus D3D11SvcEncoder::Register(
    mfxFrameAllocResponse & response,
    D3DDDIFORMAT            type)
{
    std::vector<mfxHDLPair> * queue = (type == D3DDDIFMT_NV12)
        ? m_reconQueue
        : m_bsQueue;

    mfxU32 did = 0;
    for (; did < 8; did++)
        if (queue[did].empty())
            break;

    queue[did].resize(response.NumFrameActual);

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxHDLPair handlePair;

        mfxStatus sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&handlePair);
        MFX_CHECK_STS(sts);

        queue[did][i] = handlePair;
    }

    if( type != D3DDDIFMT_NV12 )
    {
        // reserved space for feedback reports
        m_feedbackUpdate.resize(response.NumFrameActual);
        m_feedbackCached.Reset(response.NumFrameActual);
    }

    return MFX_ERR_NONE;
}


namespace
{
    mfxU8 * PackPrefixNalUnitSvc(
        mfxU8 *         begin,
        mfxU8 *         end,
        DdiTask const & task,
        mfxU32          fieldId)
    {
        mfxU32 idrFlag   = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) ? 1 : 0;
        mfxU32 nalRefIdc = (task.m_type[fieldId] & MFX_FRAMETYPE_REF) ? 1 : 0;

        OutputBitstream obs(begin, end, false);

        obs.PutBits(1, 24);                     // 001
        obs.PutBits(0, 1);                      // forbidden_zero_flag
        obs.PutBits(nalRefIdc, 2);              // nal_ref_idc
        obs.PutBits(NALU_PREFIX, 5);            // nal_unit_type
        obs.PutBits(1, 1);                      // svc_extension_flag
        obs.PutBits(idrFlag, 1);                // idr_flag
        obs.PutBits(task.m_pid, 6);             // priority_id
        obs.PutBits(1, 1);                      // no_inter_layer_pred_flag
        obs.PutBits(0, 3);                      // dependency_id
        obs.PutBits(0, 4);                      // quality_id
        obs.PutBits(task.m_tid, 3);             // temporal_id
        obs.PutBits(0, 1);                      // use_ref_base_pic_flag
        obs.PutBits(1, 1);                      // discardable_flag
        obs.PutBits(0, 1);                      // output_flag
        obs.PutBits(0x3, 2);                    // reserved_three_2bits

        return begin + obs.GetNumBits() / 8;
    }
};


mfxStatus D3D11SvcEncoder::Execute(
    mfxHDLPair                 pair,
    DdiTask const &            task,
    mfxU32                     fieldId,
    PreAllocatedVector const & sei)
{
    sei;

    HRESULT hr = S_OK;
    ID3D11Resource * pInputD3D11Res = static_cast<ID3D11Resource *>(pair.first);

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC outputDesc;
    outputDesc.DecodeProfile = m_guid;
    outputDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
    outputDesc.Texture2D.ArraySlice = UINT(size_t(pair.second));
    SAFE_RELEASE(m_pVDOView);
    hr = m_pVideoDevice->CreateVideoDecoderOutputView(pInputD3D11Res, &outputDesc, &m_pVDOView);
    CHECK_HRES(hr);
    hr = m_pVideoContext->DecoderBeginFrame(m_pDecoder, m_pVDOView, 0, 0);
    CHECK_HRES(hr);

    mfxU32 ppsid = task.m_did * m_numql * m_numtl + task.m_qid * m_numtl + task.m_tid;
    ::FillVaringPartOfPpsBuffer(task, fieldId, m_pps[ppsid]);
    //::FillVaringPartOfSliceBuffer(m_extSvc, task, fieldId, m_sps[m_pps[ppsid].seq_parameter_set_id], m_pps[ppsid], m_slice); // BUILD DX11 FIX
    ::FillVaringPartOfSliceBuffer(m_caps, m_extSvc, task, fieldId, m_sps[ppsid], m_pps[ppsid], m_slice); // BUILD DX11 FIX

    // sps and pps are already packed
    // pack slice header here

    Zero(m_packedSlice);

    mfxU8 * sliceBufferBegin = m_packedSliceBuffer;
    mfxU8 * sliceBufferEnd   = m_packedSliceBuffer + MAX_PACKED_SLICE_SIZE;
    for (mfxU32 i = 0; i < m_slice.size(); i++)
    {
        mfxU8 * prefix = PackPrefixNalUnitSvc(sliceBufferBegin, sliceBufferEnd, false, task, fieldId);

        OutputBitstream obs(prefix, sliceBufferEnd, false); // pack without emulation control
        PackSlice(obs, task, fieldId, i);

        m_packedSlice[i].pData                  = prefix;
        m_packedSlice[i].BufferSize             = UINT(prefix - sliceBufferBegin + (obs.GetNumBits() + 7) / 8);
        m_packedSlice[i].DataLength             = UINT(8 * (prefix - sliceBufferBegin) + obs.GetNumBits()); // for slices length is in bits
        m_packedSlice[i].SkipEmulationByteCount = 3;

        sliceBufferBegin += m_packedSlice[i].BufferSize;
    }

    // prepare resource list
    // it contains resources in video memory that needed for the encoding operation
    mfxU32       RES_ID_BITSTREAM   = 0;          // bitstream surface takes first place in resourceList
    mfxU32       RES_ID_ORIGINAL    = 1;
    mfxU32       RES_ID_REFERENCE   = 2;          // then goes all reference frames from dpb
    mfxU32       RES_ID_RECONSTRUCT = RES_ID_REFERENCE + task.m_idxRecon;

    mfxU32 resourceCount = mfxU32(RES_ID_REFERENCE + m_reconQueue[task.m_did].size());
    std::vector<ID3D11Resource*> resourceList;
    resourceList.resize(resourceCount);

    resourceList[RES_ID_BITSTREAM]   = static_cast<ID3D11Resource*>(m_bsQueue[task.m_did][task.m_idxBs[fieldId]].first);
    resourceList[RES_ID_ORIGINAL]    = pInputD3D11Res;

    for (mfxU32 i = 0; i < m_reconQueue[task.m_did].size(); i++)
    {
        resourceList[RES_ID_REFERENCE + i] = static_cast<ID3D11Resource*>(m_reconQueue[task.m_did][i].first);
    }

    // re-map frame indices to resourceList
    // m_pps.CurrOriginalPic.Index7Bits      = mfxU8(RES_ID_ORIGINAL);
    //m_pps.CurrReconstructedPic.Index7Bits = mfxU8(RES_ID_REFERENCE + task.m_idxRecon);
    for (mfxU32 i = 0; i < task.m_dpb[fieldId].Size(); i++)
    {
      //  m_pps.RefFrameList[i].Index7Bits = mfxU8(RES_ID_REFERENCE + task.m_dpb[fieldId][i].m_frameIdx);
    }


    // [1]. buffers in system memory (configuration buffers)
    //const mfxU32 NUM_COMP_BUFFER = 10;
    //ENCODE_COMPBUFFERDESC encodeCompBufferDesc[NUM_COMP_BUFFER];
    mfxU32 compBufferCount = mfxU32(10 + m_packedSlice.size());
    std::vector<ENCODE_COMPBUFFERDESC>  encodeCompBufferDesc;
    encodeCompBufferDesc.resize(compBufferCount);
    Zero(encodeCompBufferDesc);

    ENCODE_EXECUTE_PARAMS encodeExecuteParams;
    Zero(encodeExecuteParams);
    encodeExecuteParams.NumCompBuffers = 0;
    encodeExecuteParams.pCompressedBuffers = &encodeCompBufferDesc[0];

    // FIXME: need this until production driver moves to DDI 0.87
    encodeExecuteParams.pCipherCounter                     = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode    = 0;
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;
#else
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = 1; // none
#endif

    UINT & bufCnt = encodeExecuteParams.NumCompBuffers;

    if (task.m_insertSps[fieldId])
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA);
        encodeCompBufferDesc[bufCnt].DataSize             = mfxU32(sizeof ENCODE_SET_SEQUENCE_PARAMETERS_SVC);
        //encodeCompBufferDesc[bufCnt].pCompBuffer          = &m_sps[m_pps[ppsid].seq_parameter_set_id]; // BUILD DX11 FIX
        encodeCompBufferDesc[bufCnt].pCompBuffer          = &m_sps[ppsid]; // BUILD DX11 FIX
        bufCnt++;
    }

    ENCODE_INPUT_DESC encodeInputDesc = { 0 };
    encodeInputDesc.ArraSliceOriginal = (UINT)(size_t(pair.second));
    encodeInputDesc.IndexOriginal     = RES_ID_ORIGINAL;
    encodeInputDesc.ArraySliceRecon   = (UINT)(size_t(m_reconQueue[task.m_did][task.m_idxRecon].second));
    encodeInputDesc.IndexRecon        = RES_ID_RECONSTRUCT;

    encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA);
    encodeCompBufferDesc[bufCnt].DataSize             = mfxU32(sizeof ENCODE_SET_PICTURE_PARAMETERS_SVC);
    encodeCompBufferDesc[bufCnt].pCompBuffer          = &m_pps[ppsid];
    encodeCompBufferDesc[bufCnt].pReserved            = &encodeInputDesc;
    bufCnt++;

    encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA);
    encodeCompBufferDesc[bufCnt].DataSize             = mfxU32(sizeof ENCODE_SET_SLICE_HEADER_SVC * m_slice.size());
    encodeCompBufferDesc[bufCnt].pCompBuffer          = &m_slice[0];
    bufCnt++;

    encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA);
    encodeCompBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(RES_ID_BITSTREAM));
    encodeCompBufferDesc[bufCnt].pCompBuffer          = &RES_ID_BITSTREAM;
    bufCnt++;

    encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA);
    encodeCompBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
    encodeCompBufferDesc[bufCnt].pCompBuffer          = task.m_insertSps[fieldId] ? &m_packedSpsPps : &m_packedPps;
    bufCnt++;

    encodeCompBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA);
    encodeCompBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA) * m_packedSlice.size());
    encodeCompBufferDesc[bufCnt].pCompBuffer          = &m_packedSlice[0];
    bufCnt++;

    // [2.4] send to driver
    D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = { 0 };
    decoderExtParams.Function              = ENCODE_ENC_PAK_ID;
    decoderExtParams.pPrivateInputData     = &encodeExecuteParams;
    decoderExtParams.PrivateInputDataSize  = sizeof(ENCODE_EXECUTE_PARAMS);
    decoderExtParams.pPrivateOutputData    = 0;
    decoderExtParams.PrivateOutputDataSize = 0;
    decoderExtParams.ResourceCount         = resourceCount;
    decoderExtParams.ppResourceList        = &resourceList[0];

    hr = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParams);
    CHECK_HRES(hr);

    hr = m_pVideoContext->DecoderEndFrame(m_pDecoder);
    CHECK_HRES(hr);

    return MFX_ERR_NONE;
}

mfxStatus D3D11SvcEncoder::QueryCompBufferInfo(
    D3DDDIFORMAT           type,
    mfxFrameAllocRequest & request)
{
    type;

    MFX_CHECK_WITH_ASSERT(m_pDecoder, MFX_ERR_NOT_INITIALIZED);

    if (!m_infoQueried)
    {
        ENCODE_FORMAT_COUNT encodeFormatCount = {};

        D3D11_VIDEO_DECODER_EXTENSION ext = {};
        ext.Function = ENCODE_FORMAT_COUNT_ID;
        ext.pPrivateOutputData = &encodeFormatCount;
        ext.PrivateOutputDataSize = sizeof(ENCODE_FORMAT_COUNT);

        HRESULT hRes = DecoderExtension(m_pVideoContext, m_pDecoder, ext);
        CHECK_HRES(hRes);
        //---------------------------------------------------

        m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats;
        encodeFormats.CompressedBufferInfoSize = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
        encodeFormats.UncompressedFormatSize = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
        encodeFormats.pCompressedBufferInfo = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats = &m_uncompBufInfo[0];

        Zero(ext);
        ext.Function = ENCODE_FORMATS_ID;
        ext.pPrivateInputData = 0; //m_guid ???
        ext.PrivateInputDataSize = 0; // sizeof(m_guid) ???
        ext.pPrivateOutputData = &encodeFormats;
        ext.PrivateOutputDataSize = sizeof(ENCODE_FORMATS);

        hRes = DecoderExtension(m_pVideoContext, m_pDecoder, ext);
        CHECK_HRES(hRes);

        MFX_CHECK(encodeFormats.CompressedBufferInfoSize > 0, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(encodeFormats.UncompressedFormatSize > 0, MFX_ERR_DEVICE_FAILED);

        m_infoQueried = true;
    }

    size_t i = 0;
    for (; i < m_compBufInfo.size(); i++)
        if (m_compBufInfo[i].Type == D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA)
            break;

    MFX_CHECK(i < m_compBufInfo.size(), MFX_ERR_DEVICE_FAILED);

    request.Info.Width  = m_compBufInfo[i].CreationWidth;
    request.Info.Height = m_compBufInfo[i].CreationHeight;
    request.Info.FourCC = ownConvertD3DFMT_TO_MFX( (DXGI_FORMAT)(m_compBufInfo[i].CompressedFormats) ); // P8

    return MFX_ERR_NONE;
}

mfxStatus D3D11SvcEncoder::QueryEncodeCaps(
    MFX_ENCODE_CAPS & caps)
{
    MFX_CHECK_WITH_ASSERT(m_pDecoder, MFX_ERR_NOT_INITIALIZED);
    caps = m_caps;
    return MFX_ERR_NONE;
}

mfxStatus D3D11SvcEncoder::QueryMbPerSec(mfxVideoParam const & par, mfxU32 (&mbPerSec)[16])
{
    par;
    mbPerSec;

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus D3D11SvcEncoder::QueryStatus(
    DdiTask & task,
    mfxU32    fieldId,
    bool     /* useEvent */)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11SvcEncoder::QueryStatus");
    // After SNB once reported ENCODE_OK for a certain feedbackNumber
    // it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    // As we won't get all bitstreams we need to cache all other statuses.

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_statusReportNumber[fieldId]);

#ifdef NEW_STATUS_REPORTING_DDI_0915
    ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr;
    feedbackDescr.SizeOfStatusParamStruct = sizeof(m_feedbackUpdate[0]);
    feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;
#endif // NEW_STATUS_REPORTING_DDI_0915

    // if task is not in cache then query its status
    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {
        try
        {
            D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = { 0 };

            decoderExtParams.Function              = ENCODE_QUERY_STATUS_ID;
#ifdef NEW_STATUS_REPORTING_DDI_0915
            decoderExtParams.pPrivateInputData     = &feedbackDescr;
            decoderExtParams.PrivateInputDataSize  = sizeof(feedbackDescr);
#else // NEW_STATUS_REPORTING_DDI_0915
            decoderExtParams.pPrivateInputData     = 0;
            decoderExtParams.PrivateInputDataSize  = 0;
#endif // NEW_STATUS_REPORTING_DDI_0915
            decoderExtParams.pPrivateOutputData    = &m_feedbackUpdate[0];
            decoderExtParams.PrivateOutputDataSize = mfxU32(m_feedbackUpdate.size() * sizeof(m_feedbackUpdate[0]));
            decoderExtParams.ResourceCount         = 0;
            decoderExtParams.ppResourceList        = 0;
            HRESULT hRes = 0;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "QUERY");
                hRes = DecoderExtension(m_pVideoContext, m_pDecoder, decoderExtParams);
            }

            MFX_CHECK(hRes != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
        }
        catch (...)
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        // Put all with ENCODE_OK into cache.
        m_feedbackCached.Update(m_feedbackUpdate);

        feedback = m_feedbackCached.Hit(task.m_statusReportNumber[fieldId]);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        task.m_bsDataLength[fieldId] = feedback->bitstreamSize;
        m_feedbackCached.Remove(task.m_statusReportNumber[fieldId]);
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

mfxStatus D3D11SvcEncoder::Destroy()
{
    SAFE_RELEASE(m_pVideoDevice);
    SAFE_RELEASE(m_pVideoContext);
    SAFE_RELEASE(m_pDecoder);
    SAFE_RELEASE(m_pVDOView);
    return MFX_ERR_NONE;
}

mfxStatus D3D11SvcEncoder::Init(
    GUID                 guid,
    ID3D11VideoDevice *  pVideoDevice,
    ID3D11VideoContext * pVideoContext,
    mfxU32               width,
    mfxU32               height)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11SvcEncoder::Init");
    MFX_CHECK_NULL_PTR2(pVideoDevice, pVideoContext);

    m_pVideoDevice  = pVideoDevice;
    m_pVideoContext = pVideoContext;
    m_pVideoDevice->AddRef();
    m_pVideoContext->AddRef();
    m_guid = guid;

    HRESULT hRes;

    // [1] Query supported decode profiles
    UINT profileCount = m_pVideoDevice->GetVideoDecoderProfileCount( );
    assert( profileCount > 0 );

    bool isFound = false;
    GUID profileGuid;
    for( UINT indxProfile = 0; indxProfile < profileCount; indxProfile++ )
    {
        hRes = m_pVideoDevice->GetVideoDecoderProfile(indxProfile, &profileGuid);
        CHECK_HRES(hRes);
        if( guid == profileGuid )
        {
            isFound = true;
            break;
        }
    }

    if( !isFound )
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    // [2] Query the supported encode functions
    D3D11_VIDEO_DECODER_DESC video_desc;
    video_desc.SampleWidth  = width;
    video_desc.SampleHeight = height;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;
    video_desc.Guid = DXVA2_Intel_Encode_AVC;

    D3D11_VIDEO_DECODER_CONFIG video_config = {0};
    mfxU32 count;

    hRes = m_pVideoDevice->GetVideoDecoderConfigCount(&video_desc, &count);
    CHECK_HRES(hRes);

    //for (int i = 0; i < count; i++)
    //{
    //    hRes = m_pVideoDevice->GetVideoDecoderConfig(&video_desc, i, &video_config);
    //    CHECK_HRES(hRes);

        // mfxSts = CheckDXVAConfig(video_desc->Guid, pConfig));
        // MFX_CHECK_STS( mfxSts );
    //}

    // [2] Calling other D3D11 Video Decoder API (as for normal proc)

    // [4] CreateVideoDecoder
    // D3D11_VIDEO_DECODER_DESC video_desc;
    video_desc.SampleWidth  = width;
    video_desc.SampleHeight = height;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;
    video_desc.Guid = DXVA2_Intel_Encode_AVC;

    // D3D11_VIDEO_DECODER_CONFIG video_config;
    video_config.guidConfigBitstreamEncryption = DXVA_NoEncrypt;//encrypto will be added late
    video_config.ConfigDecoderSpecific = m_forcedCodingFunction ? m_forcedCodingFunction : ENCODE_ENC_PAK;

    hRes  = m_pVideoDevice->CreateVideoDecoder(&video_desc, &video_config, &m_pDecoder);
    CHECK_HRES(hRes);

#if 1
    D3D11_VIDEO_DECODER_EXTENSION ext = {};
    ext.Function = ENCODE_QUERY_ACCEL_CAPS_ID;
    ext.pPrivateOutputData = &m_caps.ddi_caps;
    ext.PrivateOutputDataSize = sizeof(ENCODE_CAPS);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "QUERY_ACCEL_CAPS_ID");
        hRes = DecoderExtension(m_pVideoContext, m_pDecoder, ext);
    }
    CHECK_HRES(hRes);
#endif

    return MFX_ERR_NONE;
}

mfxStatus D3D11SvcEncoder::QueryHWGUID(
            VideoCORE * core,
            GUID        guid,
            bool        isTemporal)
{
    MFX_CHECK_NULL_PTR1(core);

    D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(core);
    MFX_CHECK_NULL_PTR1(pD3d11);

    ID3D11VideoDevice *pVideoDevice = pD3d11->GetD3D11VideoDevice(isTemporal);
    MFX_CHECK_NULL_PTR1(pVideoDevice);

    // [1] Query supported decode profiles
    UINT profileCount = pVideoDevice->GetVideoDecoderProfileCount( );
    assert( profileCount > 0 );


    GUID profileGuid;
    for( UINT indxProfile = 0; indxProfile < profileCount; indxProfile++ )
    {
        HRESULT hRes = pVideoDevice->GetVideoDecoderProfile(indxProfile, &profileGuid);
        CHECK_HRES(hRes);
        if(guid == profileGuid)
            return MFX_ERR_NONE;
    }

    return MFX_WRN_PARTIAL_ACCELERATION;

} // mfxStatus D3D11Encoder::CreateAuxilliaryDevice(...)


void D3D11SvcEncoder::PackSlice(
    OutputBitstream & obs,
    DdiTask const &   task,
    mfxU32            fieldId,
    mfxU32            sliceId) const
{
    obs, task, fieldId, sliceId;

} // void D3D11SvcEncoder::PackSlice(...)

#endif // #if defined (MFX_ENABLE_H264_VIDEO_ENCODE)
#endif // #if defined (MFX_D3D11_ENABLED)
/* EOF */
