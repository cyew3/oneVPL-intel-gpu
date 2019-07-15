/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "mfx_pipeline_transcode.h"
#include "mfx_metric_calc.h"
#include "shared_utils.h"
#include "mfx_ifile.h"

class EncodeDecodeQuality : public MFXEncodeWRAPPER, public IMetricComparator
{
public:
    EncodeDecodeQuality( ComponentParams &refParams
                       , mfxStatus       *status
                       , std::unique_ptr<IVideoEncode> &&pEncode);
    ~EncodeDecodeQuality();

    mfxStatus Init(mfxVideoParam *pInit, const vm_char *pFilename);
    mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);
    mfxStatus RenderFrame(mfxFrameSurface1 *pSurface, mfxEncodeCtrl * pCtrl = NULL);
    mfxStatus Reset(mfxVideoParam *pParam);

    //////////////////////////////////////////////////////////////////////////
    //IMetrics Comparator
    virtual mfxStatus AddMetric(MetricType type, mfxF64 fVal = -1);
    virtual mfxStatus SetSurfacesSource(const vm_char      *pFile,
        mfxFrameSurface1 *pRefSurface,
        bool             bVerbose = false,
        mfxU32           nLimitFrames = 0);
    virtual mfxStatus SetOutputPerfFile(const vm_char *pOutFile);
    //////////////////////////////////////////////////////////////////////////

protected:
    virtual mfxStatus PutData(mfxU8* pData, mfxU32 nSize);
    mfxStatus InitializeDecoding();
    mfxStatus CreateAllocator();
    mfxU16    FindFreeSufrace();
    mfxStatus DecodeHeader();

    //input surfaces
    std::list<mfxFrameSurface1 *> m_RefSurfaces;
    mfxFrameSurface1 **           m_ppMfxSurface;
    mfxU32                        m_cNumberSurfaces;

    //decoding
    sFrameDecoder                 m_mfxDec;
    mfxBitstream                  m_inBits;
    MFXFrameAllocator           * m_pAllocator;
    mfxFrameAllocResponse         m_allocResponce;
    mfxVideoParam                 m_initParam;
    bool                          m_bInitialized;

    //metrics calc object
    MFXMetricComparatorRender   * m_pComparator;
    //putdata cannot report about an error
    mfxStatus                     m_lastDecStatus;
};
