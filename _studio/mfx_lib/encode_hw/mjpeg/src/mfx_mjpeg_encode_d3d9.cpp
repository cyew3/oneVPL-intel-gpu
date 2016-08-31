/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2016 Intel Corporation. All Rights Reserved.
//
//
//          MJPEG encoder DXVA2
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_WIN)

#include "mfx_mjpeg_encode_hw_utils.h"
#include "libmfx_core_factory.h"
#include "libmfx_core_interface.h"
#include "jpegbase.h"
#include "mfx_enc_common.h"

#include "mfx_mjpeg_encode_d3d9.h"
#include "libmfx_core_interface.h"

#include "mfx_mjpeg_encode_hw_utils.h"

#include "umc_va_dxva2_protected.h"

using namespace MfxHwMJpegEncode;

void CachedFeedback::Reset(mfxU32 cacheSize)
{
    Feedback init;
    memset(&init, 0, sizeof(init));
    init.bStatus = ENCODE_NOTAVAILABLE;

    m_cache.resize(cacheSize, init);
}

void CachedFeedback::Update(const FeedbackStorage& update)
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

            if (cache == 0)
            {
                assert(!"no space in cache");
                throw std::logic_error(__FUNCSIG__": no space in cache");
            }

            *cache = update[i];
        }
    }
}

const CachedFeedback::Feedback* CachedFeedback::Hit(mfxU32 feedbackNumber) const
{
    for (size_t i = 0; i < m_cache.size(); i++)
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
            return &m_cache[i];

    return 0;
}

void CachedFeedback::Remove(mfxU32 feedbackNumber)
{
    for (size_t i = 0; i < m_cache.size(); i++)
    {
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
        {
            m_cache[i].bStatus = ENCODE_NOTAVAILABLE;
            return;
        }
    }

    assert(!"wrong feedbackNumber");
}

D3D9Encoder::D3D9Encoder()
: m_core(0)
, m_pAuxDevice(0)
, m_infoQueried(false)
, m_guid(GUID_NULL)
, m_width(0)
, m_height(0)
{
    memset(&m_caps, 0, sizeof(m_caps));
}

D3D9Encoder::~D3D9Encoder()
{
    Destroy();
}

mfxStatus D3D9Encoder::CreateAuxilliaryDevice(
    VideoCORE * core,
    mfxU32      width,
    mfxU32      height,
    bool        isTemporal)
{
    m_core = core;
    GUID guid = DXVA2_Intel_Encode_JPEG;

    D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(m_core, MFXICORED3D_GUID);
    MFX_CHECK(pID3D, MFX_ERR_DEVICE_FAILED);


    IDirectXVideoDecoderService *service = 0;
    mfxStatus sts = pID3D->GetD3DService(mfxU16(width), mfxU16(height), &service, isTemporal);
    MFX_CHECK_STS(sts);
    MFX_CHECK(service, MFX_ERR_DEVICE_FAILED);

    AuxiliaryDevice *pAuxDevice = new AuxiliaryDevice();
    sts = pAuxDevice->Initialize(0, service);
    if(sts != MFX_ERR_NONE)
    {
        delete pAuxDevice;
        return sts;
    }

    sts = pAuxDevice->IsAccelerationServiceExist(guid);
    if(sts != MFX_ERR_NONE)
    {
        delete pAuxDevice;
        return sts;
    }

    memset(&m_caps, 0, sizeof(m_caps));
    HRESULT hr = pAuxDevice->Execute(AUXDEV_QUERY_ACCEL_CAPS, &guid, sizeof(guid), &m_caps, sizeof(m_caps));
    if(!SUCCEEDED(hr))
    {
        delete pAuxDevice;
        return MFX_ERR_DEVICE_FAILED;
    }

    m_guid   = guid;
    m_width  = width;
    m_height = height;
    m_pAuxDevice = pAuxDevice;

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::CreateAccelerationService(mfxVideoParam const & par)
{
    par;
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    DXVADDI_VIDEODESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.SampleWidth  = m_width;
    desc.SampleHeight = m_height;
    desc.Format       = D3DDDIFMT_NV12;

    ENCODE_CREATEDEVICE encodeCreateDevice;
    memset(&encodeCreateDevice, 0, sizeof(encodeCreateDevice));
    encodeCreateDevice.pVideoDesc     = &desc;
    encodeCreateDevice.CodingFunction = ENCODE_PAK;
    encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

    HRESULT hr = m_pAuxDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, &m_guid, sizeof(m_guid), &encodeCreateDevice, sizeof(encodeCreateDevice));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryBitstreamBufferInfo(mfxFrameAllocRequest& request)
{
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    if (!m_infoQueried)
    {
        ENCODE_FORMAT_COUNT encodeFormatCount;
        encodeFormatCount.CompressedBufferInfoCount = 0;
        encodeFormatCount.UncompressedFormatCount   = 0;

        HRESULT hr = m_pAuxDevice->Execute(ENCODE_FORMAT_COUNT_ID, (void*)0, 0, &encodeFormatCount, sizeof(encodeFormatCount));
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats;
        encodeFormats.CompressedBufferInfoSize = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
        encodeFormats.UncompressedFormatSize   = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
        encodeFormats.pCompressedBufferInfo    = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats     = &m_uncompBufInfo[0];

        hr = m_pAuxDevice->Execute(ENCODE_FORMATS_ID, (void*)0, 0, &encodeFormats, sizeof(encodeFormats));
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(encodeFormats.CompressedBufferInfoSize > 0, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(encodeFormats.UncompressedFormatSize > 0, MFX_ERR_DEVICE_FAILED);

        m_infoQueried = true;
    }

    size_t i = 0;
    for (; i < m_compBufInfo.size(); i++)
    {
        if (m_compBufInfo[i].Type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
        {
            break;
        }
    }

    MFX_CHECK(i < m_compBufInfo.size(), MFX_ERR_DEVICE_FAILED);

    request.Info.Width  = m_compBufInfo[i].CreationWidth;
    request.Info.Height = m_compBufInfo[i].CreationHeight;
    request.Info.FourCC = m_compBufInfo[i].CompressedFormats;

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryEncodeCaps(JpegEncCaps & caps)
{
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    caps.Baseline = m_caps.Baseline;
    caps.Sequential = m_caps.Sequential;
    caps.Huffman = m_caps.Huffman;

    caps.NonInterleaved = m_caps.NonInterleaved;
    caps.Interleaved = m_caps.Interleaved;

    caps.MaxPicWidth = m_caps.MaxPicWidth;
    caps.MaxPicHeight = m_caps.MaxPicHeight;

    caps.SampleBitDepth = m_caps.SampleBitDepth;
    caps.MaxNumComponent = m_caps.MaxNumComponent;
    caps.MaxNumScan = m_caps.MaxNumScan;
    caps.MaxNumHuffTable = m_caps.MaxNumHuffTable;
    caps.MaxNumQuantTable = m_caps.MaxNumQuantTable;

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::RegisterBitstreamBuffer(mfxFrameAllocResponse& response)
{
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    EmulSurfaceRegister surfaceReg;
    memset(&surfaceReg, 0, sizeof(surfaceReg));
    surfaceReg.type = D3DDDIFMT_INTELENCODE_BITSTREAMDATA;
    surfaceReg.num_surf = response.NumFrameActual;

    MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);
    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxStatus sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&surfaceReg.surface[i]);
        MFX_CHECK_STS(sts);
        MFX_CHECK(surfaceReg.surface[i] != 0, MFX_ERR_UNSUPPORTED);
    }

    HRESULT hr = m_pAuxDevice->BeginFrame(surfaceReg.surface[0], &surfaceReg);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_pAuxDevice->EndFrame(0);

    // Reserve space for feedback reports.
    m_feedbackUpdate.resize(response.NumFrameActual);
    m_feedbackCached.Reset(response.NumFrameActual);

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::Execute(DdiTask &task, mfxHDL surface)
{
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);
    ExecuteBuffers *pExecuteBuffers = task.m_pDdiData;

    mfxU32 compBufferCount = 2 + 
        (pExecuteBuffers->m_pps.NumQuantTable ? 1 : 0) + 
        (pExecuteBuffers->m_pps.NumCodingTable ? 1 : 0) + 
        (pExecuteBuffers->m_pps.NumScan ? 1 : 0) + 
        (pExecuteBuffers->m_payload_list.size() ? 1 : 0);
    std::vector<ENCODE_COMPBUFFERDESC>  encodeCompBufferDesc;
    encodeCompBufferDesc.resize(compBufferCount);
    memset(&encodeCompBufferDesc[0], 0, sizeof(ENCODE_COMPBUFFERDESC) * compBufferCount);

    ENCODE_EXECUTE_PARAMS encodeExecuteParams;
    memset(&encodeExecuteParams, 0, sizeof(ENCODE_EXECUTE_PARAMS));
    encodeExecuteParams.NumCompBuffers = 0;
    encodeExecuteParams.pCompressedBuffers = &encodeCompBufferDesc[0];

    // FIXME: need this until production driver moves to DDI 0.87
    encodeExecuteParams.pCipherCounter                     = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode    = 0;
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;

    UINT & bufCnt = encodeExecuteParams.NumCompBuffers;

    pExecuteBuffers->m_pps.StatusReportFeedbackNumber = task.m_statusReportNumber;

    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTEL_JPEGENCODE_PPSDATA;
    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(pExecuteBuffers->m_pps));
    encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_pps;
    bufCnt++;

    mfxU32 bitstream = task.m_idxBS;
    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_BITSTREAMDATA;
    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(bitstream));
    encodeCompBufferDesc[bufCnt].pCompBuffer = &bitstream;
    bufCnt++;

    mfxU16 i = 0;

    if(pExecuteBuffers->m_pps.NumQuantTable)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTEL_JPEGENCODE_QUANTDATA;
        encodeCompBufferDesc[bufCnt].DataSize = 0;
        encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_dqt_list[0];
        for (i = 0; i < pExecuteBuffers->m_pps.NumQuantTable; i++)
        {
            encodeCompBufferDesc[bufCnt].DataSize += mfxU32(sizeof(pExecuteBuffers->m_dqt_list[i]));
        }
        bufCnt++;
    }

    if(pExecuteBuffers->m_pps.NumCodingTable)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTEL_JPEGENCODE_HUFFTBLDATA;
        encodeCompBufferDesc[bufCnt].DataSize = 0;
        encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_dht_list[0];
        for (i = 0; i < pExecuteBuffers->m_pps.NumCodingTable; i++)
        {
            encodeCompBufferDesc[bufCnt].DataSize += mfxU32(sizeof(pExecuteBuffers->m_dht_list[i]));
        }
        bufCnt++;
    }

    //single interleaved scans only are supported
    for (i = 0; i < pExecuteBuffers->m_pps.NumScan; i++)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTEL_JPEGENCODE_SCANDATA;
        encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(pExecuteBuffers->m_scan_list[i]));
        encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_scan_list[i];
        bufCnt++;
    }

    if (pExecuteBuffers->m_payload_list.size())
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PAYLOADDATA;
        encodeCompBufferDesc[bufCnt].DataSize = mfxU32(pExecuteBuffers->m_payload_base.length);
        encodeCompBufferDesc[bufCnt].pCompBuffer = (void*)pExecuteBuffers->m_payload_base.data;
        bufCnt++;
    }

    try
    {
        HRESULT hr = m_pAuxDevice->BeginFrame((IDirect3DSurface9 *)surface, 0);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        hr = m_pAuxDevice->Execute(ENCODE_ENC_PAK_ID, &encodeExecuteParams, sizeof(encodeExecuteParams));
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        HANDLE handle;
        m_pAuxDevice->EndFrame(&handle);
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryStatus(DdiTask & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "QueryStatus");
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    // After SNB once reported ENCODE_OK for a certain feedbackNumber
    // it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    // As we won't get all bitstreams we need to cache all other statuses. 

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_statusReportNumber);

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
            HRESULT hr = m_pAuxDevice->Execute(
                ENCODE_QUERY_STATUS_ID,
#ifdef NEW_STATUS_REPORTING_DDI_0915
                (void *)&feedbackDescr,
                sizeof(feedbackDescr),
#else // NEW_STATUS_REPORTING_DDI_0915
                (void *)0,
                0,
#endif // NEW_STATUS_REPORTING_DDI_0915
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

mfxStatus D3D9Encoder::UpdateBitstream(
    mfxMemId       MemId,
    DdiTask      & task)
{
    mfxU8      * bsData    = task.bs->Data + task.bs->DataOffset + task.bs->DataLength;
    IppiSize     roi       = {task.m_bsDataLength, 1};
    mfxFrameData bitstream = {0};

    m_core->LockFrame(MemId, &bitstream);
    MFX_CHECK(bitstream.Y != 0, MFX_ERR_LOCK_MEMORY);

    IppStatus sts = ippiCopyManaged_8u_C1R(
        (Ipp8u *)bitstream.Y, task.m_bsDataLength,
        bsData, task.m_bsDataLength,
        roi, IPP_NONTEMPORAL_LOAD);
    assert(sts == ippStsNoErr);

    task.bs->DataLength += task.m_bsDataLength;
    m_core->UnlockFrame(MemId, &bitstream);

    return (sts != ippStsNoErr) ? MFX_ERR_UNKNOWN : MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::Destroy()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_pAuxDevice)
    {
        sts = m_pAuxDevice->ReleaseAccelerationService();
        m_pAuxDevice->Release();
        delete m_pAuxDevice;
        m_pAuxDevice = 0;
    }

    return sts;
}

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_WIN)
