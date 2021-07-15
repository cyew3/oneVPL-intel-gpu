// Copyright (c) 2008-2020 Intel Corporation
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

#if (defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE)) && defined(MFX_VA_WIN)

#include "assert.h"

#include "libmfx_core_interface.h"
#include "mfx_mpeg2_encode_d3d9.h"
#include "fast_copy.h"

#ifndef D3DDDIFMT_NV12
#define D3DDDIFMT_NV12 (D3DDDIFORMAT)(MAKEFOURCC('N', 'V', '1', '2'))
#endif

//#ifndef __SW_ENC
using namespace MfxHwMpeg2Encode;

//-------------------------------------------------------------
//  D3D9Encoder
//-------------------------------------------------------------

D3D9Encoder::D3D9Encoder(VideoCORE* core)
    : m_core(core)
    , m_pDevice()
    , m_layout()
    , m_feedback()
    , m_allocResponseMB()
    , m_allocResponseBS()
    , m_recFrames()
    , m_rawFrames()
    , m_bENC_PAK()
    , m_caps()
{
#ifdef MPEG2_ENC_HW_PERF
    memset (lock_MB_data_time, 0, sizeof(lock_MB_data_time));
    memset (&m_caps, 0, sizeof(ENCODE_CAPS));
    memset (copy_MB_data_time, 0, sizeof(copy_MB_data_time));
#endif
} // D3D9Encoder::D3D9Encoder(VideoCORE* core) : m_core(core)


D3D9Encoder::~D3D9Encoder()
{
    Close();
    if (m_allocResponseMB.NumFrameActual != 0)
    {
        m_core->FreeFrames(&m_allocResponseMB);
        memset(&m_allocResponseMB,0,sizeof(mfxFrameAllocResponse));
    }
    if (m_allocResponseBS.NumFrameActual != 0)
    {
        m_core->FreeFrames(&m_allocResponseBS);
        memset(&m_allocResponseBS,0,sizeof(mfxFrameAllocResponse));
    }

} // D3D9Encoder::~D3D9Encoder()

mfxStatus D3D9Encoder::CreateAuxilliaryDevice(mfxU16)
{
    MFX_CHECK_NULL_PTR1(m_core);

    D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(m_core, MFXICORED3D_GUID);
    MFX_CHECK(pID3D != 0, MFX_ERR_DEVICE_FAILED);

    IDirectXVideoDecoderService * service = 0;
    mfxStatus sts = pID3D->GetD3DService(640, 480, &service, true);
    MFX_CHECK_STS(sts);

    AuxiliaryDevice auxDevice;

    sts = auxDevice.Initialize(0, service);
    MFX_CHECK_STS(sts);

    sts = auxDevice.IsAccelerationServiceExist(DXVA2_Intel_Encode_MPEG2);
    MFX_CHECK_STS(sts);

    mfxU32 sizeofCaps = sizeof m_caps;
    sts = auxDevice.QueryAccelCaps(&DXVA2_Intel_Encode_MPEG2, &m_caps, &sizeofCaps);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

void D3D9Encoder::QueryEncodeCaps(ENCODE_CAPS & caps)
{
    caps = m_caps;
}


mfxStatus D3D9Encoder::Init_MPEG2_ENC(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames)
{
    mfxStatus   sts    = MFX_ERR_NONE;

    sts = Init(DXVA2_Intel_Encode_MPEG2,ENCODE_ENC,pExecuteBuffers);
    MFX_CHECK_STS(sts);

    sts = GetBuffersInfo();
    MFX_CHECK_STS(sts);

    sts = CreateMBDataBuffer(numRefFrames);
    MFX_CHECK_STS(sts);

    sts = CreateCompBuffers(pExecuteBuffers,numRefFrames);
    MFX_CHECK_STS(sts);

    sts = QueryMbDataLayout();
    MFX_CHECK_STS(sts);

    //m_bENC_PAK = false;

    return sts;

} // mfxStatus D3D9Encoder::Init_MPEG2_ENC(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames)


mfxStatus D3D9Encoder::Init_MPEG2_ENCODE(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames)
{
    mfxStatus   sts    = MFX_ERR_NONE; 

    sts = Init(DXVA2_Intel_Encode_MPEG2,ENCODE_ENC_PAK,pExecuteBuffers);
    MFX_CHECK_STS(sts);

    sts = GetBuffersInfo();
    MFX_CHECK_STS(sts);

    sts = CreateBSBuffer(numRefFrames);
    MFX_CHECK_STS(sts);

    //m_bENC_PAK = true;

    return sts;

} // mfxStatus D3D9Encoder::Init_MPEG2_ENCODE(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames)


mfxStatus D3D9Encoder::Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId)
{
    mfxStatus sts = MFX_ERR_UNSUPPORTED; 

    if( ENCODE_ENC_PAK_ID == funcId )
    {
        sts = Init_MPEG2_ENCODE(pExecuteBuffers, numRefFrames);
        m_bENC_PAK = true;
    }
    else if( ENCODE_ENC_ID == funcId )
    {
        sts = Init_MPEG2_ENC(pExecuteBuffers, numRefFrames);
        m_bENC_PAK = false;
    }

    return sts;

} // mfxStatus D3D9Encoder::Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId)


mfxStatus D3D9Encoder::Init(const GUID& guid, ENCODE_FUNC func,ExecuteBuffers* pExecuteBuffers)
{
    mfxStatus   sts    = MFX_ERR_NONE;    
    
    mfxU16          width   = ((pExecuteBuffers->m_sps.FrameWidth +15)>>4)<<4;
    mfxU16          height  = ((pExecuteBuffers->m_sps.FrameHeight+15)>>4)<<4;
    memset (&m_rawFrames,0,sizeof(mfxRawFrames));
    m_feedback.Reset();

    if (!m_pDevice)
    {
        m_pDevice = new AuxiliaryDevice;    
    }
    else
    {
        sts = m_pDevice->Release();
        MFX_CHECK_STS(sts);
    }
    // init 
    {
        D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(m_core, MFXICORED3D_GUID);
        MFX_CHECK(pID3D, MFX_ERR_DEVICE_FAILED);

        IDirectXVideoDecoderService *service = 0;

        sts = pID3D->GetD3DService(width, height, &service);
        MFX_CHECK_STS(sts);

        sts = m_pDevice->Initialize(0, service);
        MFX_CHECK_STS(sts);
    }
    {
        sts = m_pDevice->IsAccelerationServiceExist(guid);
        MFX_CHECK_STS(sts);    
    }
    {
        ENCODE_CAPS caps;
        UINT size = sizeof(ENCODE_CAPS);
        sts = m_pDevice->QueryAccelCaps(const_cast<GUID *>(&guid),&caps,&size);
        MFX_CHECK_STS(sts);
    
    }
    // Acceleration service
    {
        HRESULT hr = 0;
        DXVADDI_VIDEODESC desc = {0};
        desc.SampleWidth = width;
        desc.SampleHeight = height;
        desc.Format = D3DDDIFMT_NV12;

        ENCODE_CREATEDEVICE encodeCreateDevice  = {0};
        encodeCreateDevice.pVideoDesc           = &desc;
        encodeCreateDevice.CodingFunction       = (USHORT)func;

#ifndef PAVP_SUPPORT
        encodeCreateDevice.EncryptionMode       =  DXVA_NoEncrypt;
#else
        encodeCreateDevice.EncryptionMode = pExecuteBuffers->m_encrypt.m_bEncryptionMode ? DXVA2_INTEL_PAVP : DXVA_NoEncrypt;
        if (pExecuteBuffers->m_encrypt.m_bEncryptionMode)
        {
            encodeCreateDevice.CounterAutoIncrement  = pExecuteBuffers->m_encrypt.m_CounterAutoIncrement;
            encodeCreateDevice.pInitialCipherCounter = &pExecuteBuffers->m_encrypt.m_InitialCounter;
            encodeCreateDevice.pPavpEncryptionMode   = &pExecuteBuffers->m_encrypt.m_PavpEncryptionMode;  
        }
#endif

        hr = m_pDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, const_cast<GUID *>(&guid),sizeof(guid), &encodeCreateDevice,sizeof(encodeCreateDevice));
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    }
    {
        ENCODE_ENC_CTRL_CAPS ctrlCaps = pExecuteBuffers->m_caps;
        HRESULT hr = 0; 
        hr = m_pDevice->Execute(ENCODE_ENC_CTRL_CAPS_ID, const_cast<GUID *>(&guid),sizeof(guid), &ctrlCaps, sizeof(ctrlCaps));
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

#ifdef MPEG2_ENC_HW_PERF

    vm_time_init (&copy_MB_data_time[0]);
    vm_time_init (&copy_MB_data_time[1]);
    vm_time_init (&copy_MB_data_time[2]);

    vm_time_init ( &lock_MB_data_time[0]);
    vm_time_init ( &lock_MB_data_time[1]);
    vm_time_init ( &lock_MB_data_time[2]);

#endif 

    return sts;

} // mfxStatus D3D9Encoder::Init(const GUID& guid, ENCODE_FUNC func,ExecuteBuffers* pExecuteBuffers)


mfxStatus D3D9Encoder::GetBuffersInfo()
{
    HRESULT hr = 0;
    ENCODE_FORMAT_COUNT encodeFormatCount = {0};
    GUID    curr_guid = m_pDevice->GetCurrentGuid();

    hr = m_pDevice->Execute(ENCODE_FORMAT_COUNT_ID, &curr_guid,sizeof(GUID),&encodeFormatCount,sizeof(encodeFormatCount));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    MFX_CHECK(encodeFormatCount.CompressedBufferInfoCount>0 && encodeFormatCount.CompressedBufferInfoCount>0, MFX_ERR_DEVICE_FAILED);

    m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
    m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

    ENCODE_FORMATS encodeFormats = {0};
    encodeFormats.CompressedBufferInfoSize  = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
    encodeFormats.UncompressedFormatSize    = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
    encodeFormats.pCompressedBufferInfo     = &m_compBufInfo[0];
    encodeFormats.pUncompressedFormats      = &m_uncompBufInfo[0];
      
    hr = m_pDevice->Execute(ENCODE_FORMATS_ID, 0,0,&encodeFormats,sizeof(encodeFormats));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::GetBuffersInfo() 


mfxStatus D3D9Encoder::QueryMbDataLayout()
{
    HRESULT hr = 0;

    memset(&m_layout, 0, sizeof(m_layout));
    hr = m_pDevice->Execute(MBDATA_LAYOUT_ID, 0, 0,&m_layout,sizeof(m_layout));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::QueryMbDataLayout()


mfxStatus D3D9Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest* pRequest)
{
    size_t i = 0;
    for (; i < m_compBufInfo.size(); i++)
    {
        if (m_compBufInfo[i].Type == type)
        {
            break;
        }
    }
    if (i == m_compBufInfo.size())
        return MFX_ERR_UNSUPPORTED;

    pRequest->Info.Width = m_compBufInfo[i].CreationWidth;
    pRequest->Info.Height = m_compBufInfo[i].CreationHeight;
    pRequest->Info.FourCC = MFX_FOURCC_NV12;
    pRequest->NumFrameMin = m_compBufInfo[i].NumBuffer;
    pRequest->NumFrameSuggested = m_compBufInfo[i].NumBuffer;

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest* pRequest)


mfxStatus D3D9Encoder::Register(const mfxFrameAllocResponse* pResponse, D3DDDIFORMAT type)
{
    EmulSurfaceRegister     SurfaceReg;
    memset(&SurfaceReg, 0, sizeof(SurfaceReg));
    mfxStatus               sts         = MFX_ERR_NONE;;
    HRESULT                 hr          = 0;

    SurfaceReg.type     = type;
    SurfaceReg.num_surf = pResponse->NumFrameActual;
 
    MFX_CHECK(pResponse->mids, MFX_ERR_NULL_PTR);

    for (int i = 0; i < pResponse->NumFrameActual; i++)
    {
        sts = m_core->GetFrameHDL(pResponse->mids[i], (mfxHDL *)&SurfaceReg.surface[i]);        
        MFX_CHECK_STS(sts);
    }

    hr = m_pDevice->BeginFrame(SurfaceReg.surface[0], &SurfaceReg); 
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    hr = m_pDevice->EndFrame(0);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE
    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA && m_bIsBlockingTaskSyncEnabled)
    {
        m_EventCache->Init(256); // allocate a cache for 256 elements as _NUM_STORED_FEEDBACKS
    }
#endif

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::Register(const mfxFrameAllocResponse* pResponse, D3DDDIFORMAT type)


mfxStatus D3D9Encoder::RegisterRefFrames (const mfxFrameAllocResponse* pResponse)
{ 
    m_recFrames.NumFrameActual  = pResponse->NumFrameActual;
    MFX_CHECK(m_recFrames.NumFrameActual <NUM_FRAMES,MFX_ERR_UNSUPPORTED);

    for (int i = 0; i < pResponse->NumFrameActual; i++)
    {
        m_recFrames.mids[i] = pResponse->mids[i];
        m_recFrames.indexes[i] = (mfxU16)i;
    }

    return Register(pResponse, D3DDDIFMT_NV12);

} // mfxStatus D3D9Encoder::RegisterRefFrames (const mfxFrameAllocResponse* pResponse)


mfxI32 D3D9Encoder::GetRecFrameIndex (mfxMemId memID)
{
    for (int i = 0; i <m_recFrames.NumFrameActual;i++)
    {
        if (m_recFrames.mids[i] == memID)
            return i;    
    }

    return -1;

} // mfxI32 D3D9Encoder::GetRecFrameIndex (mfxMemId memID)


mfxI32 D3D9Encoder::GetRawFrameIndex (mfxMemId memID, bool bAddFrames)
{
    for (int i = 0; i <m_rawFrames.NumFrameActual;i++)
    {
        if (m_rawFrames.mids[i] == memID)
            return i;    
    }
    if (bAddFrames && m_rawFrames.NumFrameActual < NUM_FRAMES)
    {
        m_rawFrames.mids[m_rawFrames.NumFrameActual] = memID;
        return m_rawFrames.NumFrameActual++;
    }

    return -1;

} // mfxI32 D3D9Encoder::GetRawFrameIndex (mfxMemId memID, bool bAddFrames)


mfxStatus D3D9Encoder::CreateMBDataBuffer(mfxU32 numRefFrames)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest request = {0};

    // Buffer for MB INFO
    sts = QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_MBDATA, &request);
    MFX_CHECK_STS(sts);

    request.NumFrameMin = (request.NumFrameMin < numRefFrames)? (mfxU16)numRefFrames:request.NumFrameMin;
    request.NumFrameSuggested = (request.NumFrameSuggested < request.NumFrameMin)? request.NumFrameMin:request.NumFrameSuggested;

    if (m_allocResponseMB.NumFrameActual == 0)
    {
        request.Info.FourCC = D3DFMT_P8;
        request.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_INTERNAL_FRAME;
        sts = m_core->AllocFrames(&request, &m_allocResponseMB);
        MFX_CHECK_STS(sts);
    }
    else
    {
        if (m_allocResponseMB.NumFrameActual < request.NumFrameMin)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; 
        }
    }
    sts = Register(&m_allocResponseMB, D3DDDIFMT_INTELENCODE_MBDATA);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::CreateMBDataBuffer(mfxU32 numRefFrames)


mfxStatus D3D9Encoder::CreateCompBuffers(ExecuteBuffers* /*pExecuteBuffers*/, mfxU32 numRefFrames)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest request = {0};

    // Buffer for MB INFO
    sts = QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_MBDATA, &request);
    MFX_CHECK_STS(sts);

    request.NumFrameMin = (request.NumFrameMin < numRefFrames)? (mfxU16)numRefFrames:request.NumFrameMin;
    request.NumFrameSuggested = (request.NumFrameSuggested < request.NumFrameMin)? request.NumFrameMin:request.NumFrameSuggested;

    if (m_allocResponseMB.NumFrameActual == 0)
    {
        request.Info.FourCC = D3DFMT_P8;
        request.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_INTERNAL_FRAME;
        sts = m_core->AllocFrames(&request, &m_allocResponseMB);
        MFX_CHECK_STS(sts);
    }
    else
    {
        if (m_allocResponseMB.NumFrameActual < request.NumFrameMin)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; 
        }
    }
    sts = Register(&m_allocResponseMB, D3DDDIFMT_INTELENCODE_MBDATA);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::CreateCompBuffers(ExecuteBuffers* /*pExecuteBuffers*/, mfxU32 numRefFrames)


mfxStatus D3D9Encoder::CreateBSBuffer(mfxU32 numRefFrames)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest request = {0};

    // Buffer for MB INFO
    sts = QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_BITSTREAMDATA, &request);
    MFX_CHECK_STS(sts);

    request.NumFrameMin = (request.NumFrameMin < numRefFrames)? (mfxU16)numRefFrames:request.NumFrameMin;
    request.NumFrameSuggested = (request.NumFrameSuggested < request.NumFrameMin)? request.NumFrameMin:request.NumFrameSuggested;

    if (m_allocResponseBS.NumFrameActual == 0)
    {
        request.Info.FourCC = D3DFMT_P8;
        request.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_INTERNAL_FRAME;
        sts = m_core->AllocFrames(&request, &m_allocResponseBS);
        MFX_CHECK_STS(sts);
    }
    else
    {
        if (m_allocResponseBS.NumFrameActual < request.NumFrameMin)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; 
        }
    }
    sts = Register(&m_allocResponseBS, D3DDDIFMT_INTELENCODE_BITSTREAMDATA);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::CreateBSBuffer(mfxU32 numRefFrames)


mfxStatus D3D9Encoder::Execute(ExecuteBuffers* pExecuteBuffers, mfxU32 funcId, mfxU8 *pUserData, mfxU32 userDataLen)
{
    const mfxU32    NumCompBuffer = 11;

    ENCODE_COMPBUFFERDESC   encodeCompBufferDesc[NumCompBuffer] = {0};
    ENCODE_PACKEDHEADER_DATA payload = {0};

    ENCODE_EXECUTE_PARAMS encodeExecuteParams = {0};
    encodeExecuteParams.pCompressedBuffers = encodeCompBufferDesc;

#ifndef PAVP_SUPPORT
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = 1; // PAVP_ENCRYPTION_NONE;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode = 1; // PAVP_COUNTER_TYPE_A;
#else
    encodeExecuteParams.PavpEncryptionMode   = pExecuteBuffers->m_encrypt.m_PavpEncryptionMode;
#endif

    UINT& bufCnt = encodeExecuteParams.NumCompBuffers;

    if (pExecuteBuffers->m_bAddSPS)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_SPSDATA;
        encodeCompBufferDesc[bufCnt].DataSize = sizeof(pExecuteBuffers->m_sps);
        encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_sps;
        bufCnt++;
        pExecuteBuffers->m_bAddSPS = 0;

        if (pExecuteBuffers->m_bAddDisplayExt)
        {
            encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_VUIDATA;
            encodeCompBufferDesc[bufCnt].DataSize = sizeof(pExecuteBuffers->m_vui);
            encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_vui;
            bufCnt++;
        }

        if (funcId == ENCODE_ENC_PAK_ID && m_bENC_PAK &&
            (pExecuteBuffers->m_quantMatrix.QmatrixMPEG2.bNewQmatrix[0] ||
             pExecuteBuffers->m_quantMatrix.QmatrixMPEG2.bNewQmatrix[1] ||
             pExecuteBuffers->m_quantMatrix.QmatrixMPEG2.bNewQmatrix[2] ||
             pExecuteBuffers->m_quantMatrix.QmatrixMPEG2.bNewQmatrix[3] ))
        {
            encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_QUANTDATA;
            encodeCompBufferDesc[bufCnt].DataSize = sizeof(pExecuteBuffers->m_quantMatrix);
            encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_quantMatrix;
            bufCnt++;
        }
    }

    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PPSDATA;
    encodeCompBufferDesc[bufCnt].DataSize = sizeof(pExecuteBuffers->m_pps);
    encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_pps;
    bufCnt++;

    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_SLICEDATA;
    encodeCompBufferDesc[bufCnt].DataSize = sizeof(*pExecuteBuffers->m_pSlice) * pExecuteBuffers->m_pps.NumSlice;
    encodeCompBufferDesc[bufCnt].pCompBuffer = pExecuteBuffers->m_pSlice;
    bufCnt++;

    if (funcId == ENCODE_ENC_ID && !m_bENC_PAK)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_MBDATA;
        encodeCompBufferDesc[bufCnt].DataSize = sizeof(pExecuteBuffers->m_idxMb);
        encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_idxMb;
        bufCnt++;
    }
    else if (funcId == ENCODE_ENC_PAK_ID && m_bENC_PAK)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_BITSTREAMDATA;
        encodeCompBufferDesc[bufCnt].DataSize = sizeof(pExecuteBuffers->m_idxBs);
        encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_idxBs;
        bufCnt++;

        if (userDataLen>0 && pUserData)
        {

            payload.pData = pUserData;
            payload.DataLength = payload.BufferSize = userDataLen;

            encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
            encodeCompBufferDesc[bufCnt].DataSize       = sizeof (payload);
            encodeCompBufferDesc[bufCnt].pCompBuffer    = &payload;
            bufCnt++;
        }
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    {
        HRESULT hr = 0;
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_PRIVATE, "DDI_ENC");
        MFX_LTRACE_I(MFX_TRACE_LEVEL_PRIVATE, pExecuteBuffers->m_idxMb);

        hr = m_pDevice->BeginFrame((IDirect3DSurface9 *)pExecuteBuffers->m_pSurface,0);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE
        if (m_bIsBlockingTaskSyncEnabled)
        {
            // allocate the event
            pExecuteBuffers->m_GpuEvent.m_gpuComponentId = GPU_COMPONENT_ENCODE;
            m_EventCache->GetEvent(pExecuteBuffers->m_GpuEvent.gpuSyncEvent);

            hr = m_pDevice->Execute(DXVA2_SET_GPU_TASK_EVENT_HANDLE, &pExecuteBuffers->m_GpuEvent, sizeof(pExecuteBuffers->m_GpuEvent));
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
#endif

        hr = m_pDevice->Execute(funcId, &encodeExecuteParams,sizeof(encodeExecuteParams));
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::Execute(ExecuteBuffers* pExecuteBuffers, mfxU32 funcId)


mfxStatus D3D9Encoder::SetFrames (ExecuteBuffers* pExecuteBuffers)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxI32 ind = 0;

    if (pExecuteBuffers->m_RecFrameMemID)
    {
        ind = GetRecFrameIndex(pExecuteBuffers->m_RecFrameMemID);
    }
    else
    {
        ind = 0xff;
    }

    pExecuteBuffers->m_pps.CurrReconstructedPic.Index7Bits     =  mfxU8(ind < 0 ? 0 : ind);
    pExecuteBuffers->m_pps.CurrReconstructedPic.AssociatedFlag =  0;
    pExecuteBuffers->m_idxMb = (DWORD)ind;
    pExecuteBuffers->m_idxBs = (DWORD)ind;

    if (pExecuteBuffers->m_bUseRawFrames)
    {
        ind = GetRawFrameIndex(pExecuteBuffers->m_CurrFrameMemID,true);
        MFX_CHECK(ind>=0,MFX_ERR_NOT_FOUND);
    }
    //else CurrOriginalPic == CurrReconstructedPic 

    pExecuteBuffers->m_pps.CurrOriginalPic.Index7Bits     =  mfxU8(ind);
    pExecuteBuffers->m_pps.CurrOriginalPic.AssociatedFlag =  0;

    if (pExecuteBuffers->m_RefFrameMemID[0])
    {
        ind =  GetRecFrameIndex(pExecuteBuffers->m_RefFrameMemID[0]);
        MFX_CHECK(ind>=0,MFX_ERR_NOT_FOUND);
        pExecuteBuffers->m_pps.RefFrameList[0].Index7Bits = mfxU8(ind);
        pExecuteBuffers->m_pps.RefFrameList[0].AssociatedFlag = 0;
    }
    else
    {
        pExecuteBuffers->m_pps.RefFrameList[0].bPicEntry = 0xff;
    }

    if (pExecuteBuffers->m_RefFrameMemID[1])
    {
        ind = GetRecFrameIndex(pExecuteBuffers->m_RefFrameMemID[1]);
        MFX_CHECK(ind>=0,MFX_ERR_NOT_FOUND);
        pExecuteBuffers->m_pps.RefFrameList[1].Index7Bits = mfxU8(ind);
        pExecuteBuffers->m_pps.RefFrameList[1].AssociatedFlag = 0;
    }
    else
    {
        pExecuteBuffers->m_pps.RefFrameList[1].bPicEntry = 0xff;
    }

    if (pExecuteBuffers->m_bExternalCurrFrameHDL)
    {
        MFX_CHECK(pExecuteBuffers->m_pSurface, MFX_ERR_UNDEFINED_BEHAVIOR);
    }
    else if (pExecuteBuffers->m_bExternalCurrFrame)
    {
        sts = m_core->GetExternalFrameHDL(pExecuteBuffers->m_CurrFrameMemID,(mfxHDL *)&pExecuteBuffers->m_pSurface);
    }
    else
    {
        sts = m_core->GetFrameHDL(pExecuteBuffers->m_CurrFrameMemID,(mfxHDL *)&pExecuteBuffers->m_pSurface);    
    }
    MFX_CHECK_STS(sts);

    /*printf("CurrOriginalPic %d, CurrReconstructedPic %d, RefFrameList[0] %d, RefFrameList[1] %d\n",
            pExecuteBuffers->m_pps.CurrOriginalPic.Index7Bits,
            pExecuteBuffers->m_pps.CurrReconstructedPic.Index7Bits,
            pExecuteBuffers->m_pps.RefFrameList[0].Index7Bits,
            pExecuteBuffers->m_pps.RefFrameList[1].Index7Bits);*/

    return sts;

} // mfxStatus D3D9Encoder::SetFrames (ExecuteBuffers* pExecuteBuffers)

mfxStatus D3D9Encoder::Execute_ENC (ExecuteBuffers* pExecuteBuffers, mfxU8* , mfxU32 )
{
    MFX::AutoTimer timer(__FUNCTION__);

    mfxStatus sts = MFX_ERR_NONE;

    sts = Execute(pExecuteBuffers, ENCODE_ENC_ID, 0 , 0);
    MFX_CHECK_STS(sts);

    return sts;

} // mfxStatus D3D9Encoder::Execute_ENC (ExecuteBuffers* pExecuteBuffers)


mfxStatus D3D9Encoder::Execute_ENCODE (ExecuteBuffers* pExecuteBuffers, mfxU8 *pUserData, mfxU32 userDataLen)
{
    MFX::AutoTimer timer(__FUNCTION__);

    mfxStatus sts = MFX_ERR_NONE;

    sts = Execute(pExecuteBuffers, ENCODE_ENC_PAK_ID, pUserData, userDataLen);
    MFX_CHECK_STS(sts);    

    return sts;

} // mfxStatus D3D9Encoder::Execute_ENCODE (ExecuteBuffers* pExecuteBuffers)


mfxStatus D3D9Encoder::Execute(ExecuteBuffers* pExecuteBuffers, mfxU8 *pUserData, mfxU32 userDataLen)
{
    mfxStatus sts = MFX_ERR_NONE;

    if( IsFullEncode() )
    {
        sts = Execute_ENCODE(pExecuteBuffers, pUserData, userDataLen);
    }
    else
    {
        sts = Execute_ENC(pExecuteBuffers,0,0);
    }

    return sts;

} // mfxStatus D3D9Encoder::Execute(ExecuteBuffers* pExecuteBuffers)


mfxStatus D3D9Encoder::Close()
{
    mfxStatus sts = MFX_ERR_NONE;
    if (m_pDevice)
    {
#ifdef MPEG2_ENC_HW_PERF

        FILE* f = fopen ("mpeg2_ENK_hw_perf_ex.txt","a+");
        fprintf(f,"%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
            (int)lock_MB_data_time[0].diff,
            (int)lock_MB_data_time[1].diff,
            (int)lock_MB_data_time[2].diff,
            (int)copy_MB_data_time[0].diff,
            (int)copy_MB_data_time[1].diff,
            (int)copy_MB_data_time[2].diff,
            (int)lock_MB_data_time[0].freq);
        fclose(f);
#endif

        m_pDevice->Release();
        delete m_pDevice;
        m_pDevice = 0;
    }

    return sts;

} // mfxStatus D3D9Encoder::Close()


mfxStatus D3D9Encoder::FillMBBufferPointer(ExecuteBuffers* pExecuteBuffers)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyMB");
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameData Frame = {0};

    //pExecuteBuffers->m_idxMb = 0;
    if (pExecuteBuffers->m_idxMb >= DWORD(m_allocResponseMB.NumFrameActual))
    {
        return MFX_ERR_UNSUPPORTED;
    }

    Frame.MemId = m_allocResponseMB.mids[pExecuteBuffers->m_idxMb];
#ifdef MPEG2_ENC_HW_PERF
    if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_I)
    {
        vm_time_start (0,&lock_MB_data_time[0]);
    }
    else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_P)
    {
        vm_time_start (0,&lock_MB_data_time[1]);        
    }
    else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_B)
    {
        vm_time_start (0,&lock_MB_data_time[2]);        
    }
#endif
    sts = m_core->LockFrame(Frame.MemId,&Frame);
    MFX_CHECK_STS(sts);

#ifdef MPEG2_ENC_HW_PERF
    if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_I)
    {
        vm_time_stop (0,&lock_MB_data_time[0]);
    }
    else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_P)
    {
        vm_time_stop (0,&lock_MB_data_time[1]);        
    }
    else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_B)
    {
        vm_time_stop (0,&lock_MB_data_time[2]);        
    }
#endif
    int numMB = 0;
    for (int i = 0; i<(int)pExecuteBuffers->m_pps.NumSlice;i++)
    {
        numMB = numMB + (int)pExecuteBuffers->m_pSlice[i].NumMbsForSlice;
    }
    //{
    //    FILE* f = fopen("MB data.txt","ab+");
    //    fwrite(Frame.Y,1,Frame.Pitch*numMB,f);
    //    fclose(f);
    //}

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyMBData");
#ifdef MPEG2_ENC_HW_PERF
        if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_I)
        {
            vm_time_start (0,&copy_MB_data_time[0]);
        }
        else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_P)
        {
            vm_time_start (0,&copy_MB_data_time[1]);        
        }
        else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_B)
        {
            vm_time_start (0,&copy_MB_data_time[2]);        
        }
#endif
        mfxFrameSurface1 src = {0};
        mfxFrameSurface1 dst = {0};

        src.Data        = Frame;
        src.Data.Y     += m_layout.MB_CODE_offset;
        src.Data.Pitch  = mfxU16(m_layout.MB_CODE_stride);
        src.Info.Width  = mfxU16(sizeof ENCODE_ENC_MB_DATA_MPEG2);
        src.Info.Height = mfxU16(numMB);
        src.Info.FourCC = MFX_FOURCC_P8;
        dst.Data.Y      = (mfxU8 *)pExecuteBuffers->m_pMBs;
        dst.Data.Pitch  = mfxU16(sizeof ENCODE_ENC_MB_DATA_MPEG2);
        dst.Info.Width  = mfxU16(sizeof ENCODE_ENC_MB_DATA_MPEG2);
        dst.Info.Height = mfxU16(numMB);
        dst.Info.FourCC = MFX_FOURCC_P8;

        sts = m_core->DoFastCopyExtended(&dst, &src);
        MFX_CHECK_STS(sts);
#ifdef MPEG2_ENC_HW_PERF
        if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_I)
        {
            vm_time_stop (0,&copy_MB_data_time[0]);
        }
        else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_P)
        {
            vm_time_stop (0,&copy_MB_data_time[1]);        
        }
        else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_B)
        {
            vm_time_stop (0,&copy_MB_data_time[2]);        
        }
#endif
    }

    sts = m_core->UnlockFrame(Frame.MemId);
    MFX_CHECK_STS(sts);

    return sts;

} // mfxStatus D3D9Encoder::FillMBBufferPointer(ExecuteBuffers* pExecuteBuffers)

mfxStatus D3D9Encoder::QueryStatusAsync(mfxU32 nFeedback, mfxU32 &bitstreamSize)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MPEG2 encode DDIQueryTask");
    ENCODE_QUERY_STATUS_PARAMS queryStatus = {};
    ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr;
    feedbackDescr.SizeOfStatusParamStruct = sizeof(ENCODE_QUERY_STATUS_PARAMS);
    feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;
    if (m_feedback.isUpdateNeeded())
    {
        HRESULT hr = m_pDevice->Execute(ENCODE_QUERY_STATUS_ID,
                                        (void *)&feedbackDescr,
                                        sizeof(feedbackDescr),
                                        m_feedback.GetPointer(),
                                        m_feedback.GetSize());
        MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    MFX_CHECK(m_feedback.GetFeedback(nFeedback, queryStatus), MFX_ERR_DEVICE_FAILED);

    MFX_CHECK(queryStatus.bStatus != ENCODE_NOTREADY, MFX_WRN_DEVICE_BUSY);
    MFX_CHECK(queryStatus.bStatus == ENCODE_OK, MFX_ERR_DEVICE_FAILED);
    bitstreamSize = queryStatus.bitstreamSize;

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::FillBSBuffer(mfxU32 nFeedback, mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameData Frame = { 0 };
    mfxU32 bitstreamSize = 0;

    MFX_CHECK(nBitstream < DWORD(m_allocResponseBS.NumFrameActual), MFX_ERR_NOT_FOUND);

    sts = QueryStatusAsync(nFeedback, bitstreamSize);
    if (sts != MFX_ERR_NONE)
        return sts;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MPEG2 encode CopyBitstream");
    Frame.MemId = m_allocResponseBS.mids[nBitstream];
    sts = m_core->LockFrame(Frame.MemId, &Frame);
    MFX_CHECK_STS(sts);

    pEncrypt = pEncrypt;
#ifdef PAVP_SUPPORT
    if (pEncrypt->m_bEncryptionMode)
    {
        MFX_CHECK_NULL_PTR1(pBitstream->EncryptedData);
        MFX_CHECK_NULL_PTR1(pBitstream->EncryptedData->Data);
        MFX_CHECK(pBitstream->EncryptedData->DataLength + pBitstream->EncryptedData->DataOffset + bitstreamSize < pBitstream->EncryptedData->MaxLength, MFX_ERR_NOT_ENOUGH_BUFFER);
        std::copy(Frame.Y, Frame.Y + bitstreamSize, pBitstream->EncryptedData->Data + pBitstream->EncryptedData->DataLength + pBitstream->EncryptedData->DataOffset);
        pBitstream->EncryptedData->DataLength += bitstreamSize;
        pBitstream->EncryptedData->CipherCounter.IV = pEncrypt->m_aesCounter.IV;
        pBitstream->EncryptedData->CipherCounter.Count = pEncrypt->m_aesCounter.Count;
        pEncrypt->m_aesCounter.Increment();
        if (pEncrypt->m_aesCounter.IsWrapped())
        {
              pEncrypt->m_aesCounter.ResetWrappedFlag();
    }
    }
    else
#endif
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyBitsream");
        MFX_CHECK(pBitstream->DataLength + pBitstream->DataOffset + bitstreamSize < pBitstream->MaxLength, MFX_ERR_NOT_ENOUGH_BUFFER);

        IppiSize roi = { static_cast<int>(bitstreamSize), 1 };

        sts = FastCopy::Copy(
            pBitstream->Data + pBitstream->DataLength + pBitstream->DataOffset,
            bitstreamSize,
            Frame.Y, bitstreamSize,
            roi, COPY_VIDEO_TO_SYS);

        MFX_CHECK(sts == MFX_ERR_NONE, MFX_ERR_UNDEFINED_BEHAVIOR);
        pBitstream->DataLength += bitstreamSize;
    }

    sts = m_core->UnlockFrame(Frame.MemId);
    MFX_CHECK_STS(sts);

    return sts;
}

#endif
/* EOF */
