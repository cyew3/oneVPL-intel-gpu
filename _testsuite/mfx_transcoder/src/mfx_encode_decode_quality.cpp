/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "mfx_pipeline_defs.h"
#include "mfx_encode_decode_quality.h"
#include "mfx_metric_calc.h"
#include "mfx_utility_allocators.h"
#include "mfx_sysmem_allocator.h"

#ifndef MFX_INFINITE
    #define MFX_INFINITE 0xFFFFFFFF
#endif

EncodeDecodeQuality::EncodeDecodeQuality( ComponentParams &refParams
                                        , mfxStatus *status
                                        , std::auto_ptr<IVideoEncode>& pEncode)
    : MFXEncodeWRAPPER(refParams, status, pEncode)
    , m_ppMfxSurface()
    , m_pAllocator()
    , m_bInitialized()
{
    ZERO_MEMORY(m_mfxDec);
    ZERO_MEMORY(m_inBits);

    *status = MFX_ERR_NONE;
    //decoded frame would be in yv12
    m_pComparator = new MFXMetricComparatorRender(FileWriterRenderInputParams(), refParams.m_pSession, status);
    if (NULL == m_pComparator)
    {
        *status = MFX_ERR_MEMORY_ALLOC;
    }
    else
    {
        m_pComparator->SetDownStream(m_pFile->Clone()); 
    }
}

mfxStatus EncodeDecodeQuality::AddMetric(MetricType type, mfxF64 fVal)
{
    MFX_CHECK_POINTER(m_pComparator);
    return m_pComparator->AddMetric(type, fVal);
}

mfxStatus EncodeDecodeQuality::SetSurfacesSource(const vm_char      *pFile,
                                                 mfxFrameSurface1 *pRefSurface,
                                                 bool             bVerbose,
                                                 mfxU32           nLimitFrames)
{
    MFX_CHECK_POINTER(m_pComparator);
    return m_pComparator->SetSurfacesSource(pFile, pRefSurface, bVerbose, nLimitFrames);
}

mfxStatus EncodeDecodeQuality::SetOutputPerfFile(const vm_char *pOutFile)
{
    MFX_CHECK_POINTER(m_pComparator);
    return m_pComparator->SetOutputPerfFile(pOutFile);
}

EncodeDecodeQuality::~EncodeDecodeQuality()
{
    WipeMFXBitstream(&m_inBits);
    WipeMfxDecoder(&m_mfxDec);

    if (NULL != m_pAllocator)
    {
        for (int i = 0; i < m_allocResponce.NumFrameActual; i++)
        {
            m_pAllocator->Unlock( m_pAllocator->pthis
                                , m_allocResponce.mids[i]
                                , &m_ppMfxSurface[i]->Data);

        }

        (*m_pAllocator->Free)(m_pAllocator->pthis, &m_allocResponce);
        delete m_pAllocator;
        m_pAllocator = NULL;
    }
    
    if (NULL != m_ppMfxSurface)
    {
        for (int i = 0; i < m_allocResponce.NumFrameActual; i++)
        {
            if(NULL != m_ppMfxSurface[i])
            {
                SAFE_DELETE(m_ppMfxSurface[i]);
            }
        }
    }

    SAFE_DELETE_ARRAY(m_ppMfxSurface);
    SAFE_DELETE(m_pComparator);
}

mfxStatus EncodeDecodeQuality::Init(mfxVideoParam *pInit, const vm_char *pFilename)
{
    MFX_CHECK_STS(MFXEncodeWRAPPER::Init(pInit, pFilename));

    // decoder initialization
    m_initParam = *pInit;
    m_initParam.mfx.DecodedOrder = 0;
    MFX_CHECK_STS(InitMfxBitstream(&m_inBits, pInit));

    return MFX_ERR_NONE;
}

mfxStatus EncodeDecodeQuality::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    mfxFrameAllocRequest internalRequest;
    MFX_CHECK_STS(MFXEncodeWRAPPER::QueryIOSurf(par, request));
    sFrameDecoder tmpDecoder;
    
    //sving parameters rejected by decoder
    int nOldPattern = par->IOPattern;
    int nOldNumExtParams = par->NumExtParam;
    par->IOPattern   = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    par->NumExtParam = 0;

    MFX_CHECK_STS(InitMFXDecoder(&tmpDecoder, par, MFX_IMPL_SOFTWARE));
    MFX_CHECK_STS(tmpDecoder.pmfxDEC->QueryIOSurf(par, &internalRequest));
    
    //restoring params
    par->IOPattern   = (mfxU16)nOldPattern;
    par->NumExtParam = (mfxU16)nOldNumExtParams;

    WipeMfxDecoder(&tmpDecoder);
    request->NumFrameMin       = (mfxU16)(request->NumFrameMin + internalRequest.NumFrameMin);
    request->NumFrameSuggested = (mfxU16)(request->NumFrameSuggested + internalRequest.NumFrameSuggested);

    return MFX_ERR_NONE;
}

mfxStatus EncodeDecodeQuality::CreateAllocator()
{
    //prepare mfxSurface
    mfxFrameAllocRequest  request;
    ZERO_MEMORY(request);
    ZERO_MEMORY(m_allocResponce);

    MFX_CHECK_STS(m_mfxDec.pmfxDEC->QueryIOSurf(&m_initParam, &request));

    // Create allocator and frames
    request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    std::auto_ptr<MFXFrameAllocatorRW> rw_ptr (new Adapter<MFXFrameAllocatorRW, MFXFrameAllocator> (new SysMemFrameAllocator));
    m_pAllocator = new LCCheckFrameAllocator(rw_ptr);
    MFX_CHECK_POINTER(m_pAllocator);
    MFX_CHECK_STS(m_pAllocator->Init(NULL));
    MFX_CHECK_STS(m_pAllocator->Alloc(m_pAllocator->pthis, &request, &m_allocResponce));

    // Create mfxFrameSurface1 array
    m_cNumberSurfaces = m_allocResponce.NumFrameActual;
    m_ppMfxSurface = new mfxFrameSurface1 * [m_cNumberSurfaces];
    MFX_CHECK_POINTER(m_ppMfxSurface);

    for (mfxU32 i = 0; i < m_cNumberSurfaces; i++)
    {
        m_ppMfxSurface[i] = new mfxFrameSurface1;
        MFX_CHECK_POINTER(m_ppMfxSurface[i]);
        memset(m_ppMfxSurface[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(m_ppMfxSurface[i]->Info), &m_initParam.mfx.FrameInfo, sizeof(mfxFrameInfo));

        MFX_CHECK_STS( m_pAllocator->Lock( m_pAllocator->pthis
                                         , m_allocResponce.mids[i]
                                         , &m_ppMfxSurface[i]->Data));

    }

    return MFX_ERR_NONE;
}

mfxStatus EncodeDecodeQuality::RenderFrame(mfxFrameSurface1 *pSurface, mfxEncodeCtrl * pCtrl)
{
    // store input surface
    if(pSurface)
    {
        IncreaseReference(&pSurface->Data);
        m_RefSurfaces.push_back(pSurface);
    }

     MFX_CHECK_STS(MFXEncodeWRAPPER::RenderFrame(pSurface, pCtrl));

    // notify EndOfStream
    if (pSurface == NULL)
    {
        for (;;)
        {
            MFX_CHECK_STS(PutData(NULL, 0));
            BREAK_ON_ERROR(m_lastDecStatus);
        }
        MFX_CHECK_STS_SKIP(m_lastDecStatus, MFX_ERR_MORE_DATA);
    }

    return MFX_ERR_NONE;
}

mfxU16 EncodeDecodeQuality::FindFreeSufrace()
{
    int wait_interval   = 10;

    for (mfxU32 j = 0; j < TimeoutVal<>::val(); j += wait_interval)
    {
        for (int i = 0; i < m_allocResponce.NumFrameActual; i++)
        {
            if (0 == m_ppMfxSurface[i]->Data.Locked)
            {
                return (mfxU16)i;
            }
        }
        MPA_TRACE("Sleep(EncodeDecodeQuality::FindFreeSufrace)");
        vm_time_sleep(wait_interval);
    }

    return 0xFF;
}

mfxStatus EncodeDecodeQuality::DecodeHeader()
{
    for (;;)
    {
        //TODO : infinite loop here should be eliminated
        if (MFX_ERR_NONE == m_mfxDec.pmfxDEC->DecodeHeader(&m_inBits, &m_initParam))
            break;
    }
    //copy m_mfxBSFrameBack
    m_inBits.DataLength += m_inBits.DataOffset;
    m_inBits.DataOffset = 0;

    return MFX_ERR_NONE;
}

mfxStatus EncodeDecodeQuality::InitializeDecoding()
{
    if (!m_bInitialized)
    {
        m_initParam.IOPattern   = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        m_initParam.NumExtParam = 0;
        MFX_CHECK_STS(InitMFXDecoder(&m_mfxDec, &m_initParam, MFX_IMPL_SOFTWARE));
        MFX_CHECK_STS(DecodeHeader());
        WipeMfxDecoder(&m_mfxDec);

        m_initParam.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        MFX_CHECK_STS(InitMFXDecoder(&m_mfxDec, &m_initParam, MFX_IMPL_SOFTWARE));
        MFX_CHECK_STS(CreateAllocator());
        MFX_CHECK_STS(m_pComparator->Init(&m_initParam, NULL));
        
        m_bInitialized = true;
    }
    return MFX_ERR_NONE;
}

mfxStatus EncodeDecodeQuality::PutData(mfxU8* pData, mfxU32 nSize)
{
    mfxBitstream *pbs = &m_inBits;

    if (m_pFile->isOpen())
    {
        MFX_CHECK_STS(MFXEncodeWRAPPER::PutData(pData, nSize));
    }

    if (0 != m_inBits.DataLength)
    {
        memmove(m_inBits.Data, m_inBits.Data+m_inBits.DataOffset, m_inBits.DataLength);
    }
    m_inBits.DataOffset = 0;

    while (m_inBits.MaxLength < m_inBits.DataLength + nSize)
    {
        MFX_CHECK_STS(ExtendMFXBitstream(&m_inBits));
    }
    
    // append data
    if (NULL != pData)
    {
        memcpy(m_inBits.Data + m_inBits.DataLength, pData, nSize);
        m_inBits.DataLength += nSize;
    }else
    {
        if (m_inBits.DataLength < 4)
        {
            pbs = NULL;
        }
    }

    MFX_CHECK_STS(InitializeDecoding());

    // decode
    for (;;)
    {
        mfxFrameSurface1 *pDecodedSurface = NULL;
        mfxSyncPoint     syncp            = NULL;
        mfxStatus        sts              = MFX_ERR_MORE_SURFACE;
        mfxU16           nFreeSrfIndex;
        mfxU32           nSleep = 0;

        for (; sts == MFX_ERR_MORE_SURFACE || sts == MFX_WRN_DEVICE_BUSY;)
        {
            if (sts == MFX_WRN_DEVICE_BUSY)
            {
                nSleep++;
                vm_time_sleep(5);
                MFX_CHECK_WITH_ERR(nSleep * 5 < TimeoutVal<>::val(), MFX_WRN_DEVICE_BUSY);
            }

            nFreeSrfIndex = FindFreeSufrace();
            MFX_CHECK(0xFF != nFreeSrfIndex);

            sts = m_mfxDec.pmfxDEC->DecodeFrameAsync(pbs, m_ppMfxSurface[nFreeSrfIndex], &pDecodedSurface, &syncp);
        }

        //reporting error only if bitstream was ended
        m_lastDecStatus = pbs == NULL ? sts : MFX_ERR_NONE;
        if(sts != MFX_ERR_NONE)
        {
            //only err more_data or video_param_changed is correct err code at this point
            MFX_CHECK_STS_SKIP(sts, MFX_ERR_MORE_DATA);
            return MFX_ERR_NONE;
        }
        
        m_mfxDec.session.SyncOperation(syncp, MFX_INFINITE);
        
        MFX_CHECK(!m_RefSurfaces.empty());
        
        //passing to metric comparator render
        MFX_CHECK_STS(m_pComparator->SetSurfacesSource(NULL, *(m_RefSurfaces.begin()), m_ExtraParams.bVerbose));
        MFX_CHECK_STS(m_pComparator->RenderFrame(pDecodedSurface));

        // remove surface from the list
        DecreaseReference(&(*m_RefSurfaces.begin())->Data);
        m_RefSurfaces.pop_front();
        
        // continue if EndOfStream
        if (NULL == pData && m_inBits.DataLength < 4)
        {
            WipeMFXBitstream(&m_inBits);
            m_inBits.DataLength = 0;
        }
        continue;
    }
}

mfxStatus EncodeDecodeQuality::Reset(mfxVideoParam *pParam)
{
    //TODO::check
    WipeMfxDecoder(&m_mfxDec);
    m_mfxDec.pmfxDEC->Reset(&m_initParam);//with same parameters
    m_inBits.DataLength = 0;

    return MFXEncodeWRAPPER::Reset(pParam);
}
