/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ivideo_render.h"
#include "mfx_isurface_cmp.h"
#include "mfx_singleton.h"

//#define USE_CM 1
#if USE_CM
#include <cm_rt.h>
#endif

//////////////////////////////////////////////////////////////////////////////////////

enum MetricType
{
    METRIC_NONE     = 0,
    METRIC_BITEXACT = 1,
    METRIC_CRC      = 2,
    METRIC_SSIM     = 4,
    METRIC_DELTA    = 8,
    METRIC_PSNR     = 16,
    METRIC_DUMP     = 32,
    METRIC_SSIM_GPU = 64,
};

class ComparatorFactory
    : public MFXSingleton<ComparatorFactory>
{
    friend class MFXSingleton<ComparatorFactory>;
public:
    virtual mfxStatus CreateComparatorByType(MetricType, std::auto_ptr<IMFXSurfacesComparator> & ppCmp);
    virtual ~ComparatorFactory();

protected:
    ComparatorFactory();
    std::map<int, IMFXSurfacesComparator*> m_prototypes;
    void RegisterComparators();
};

class MFXPsnrCalc : public IMFXSurfacesComparator
{
    IMPLEMENT_CLONE(MFXPsnrCalc);
public:
    MFXPsnrCalc();
    virtual ~MFXPsnrCalc();
    virtual mfxStatus Reset();
    virtual mfxStatus Compare(mfxFrameSurface1 * pIn1, mfxFrameSurface1 * pIn2);
    virtual mfxStatus GetLastCmpResult(double pResult[3]);
    virtual mfxStatus GetOveralResult(double pResult[3], mfxU32 fourcc);
    virtual mfxStatus GetAverageResult(double pResult[3]);
    virtual mfxStatus GetMaxResult(mfxF64 pResult[3]);
    virtual mfxStatus GetMinResult(mfxF64 pResult[3]);

    virtual vm_char*  GetMetricName();

protected:
    mfxF64 CalcPSNR(mfxF64 norm2, mfxI64 size, mfxU32 fourcc);

    mfxF64 m_fpsnrAV[3];
    mfxF64 m_fpsnrBST[3];
    mfxF64 m_fpsnrWST[3];
    mfxI64 m_nSize[3];
    mfxF64 m_fLastResults[3];
    // Sum of frames PSNRs
    mfxF64 m_fpsnrCumulative[3];
    mfxI64 m_nFrame;
};

class MFXSSIMCalc : public IMFXSurfacesComparator
{
    IMPLEMENT_CLONE(MFXSSIMCalc);
public:
    MFXSSIMCalc();
    virtual ~MFXSSIMCalc();

    virtual mfxStatus Reset();
    virtual mfxStatus Compare(mfxFrameSurface1 * pIn1, mfxFrameSurface1 * pIn2);
    virtual mfxStatus GetLastCmpResult(double pResult[3]);
    virtual mfxStatus GetOveralResult (double pResult[3], mfxU32);
    virtual mfxStatus GetAverageResult(double pResult[3]);
    virtual mfxStatus GetMaxResult(mfxF64 pResult[3]);
    virtual mfxStatus GetMinResult(mfxF64 pResult[3]);
    virtual vm_char*  GetMetricName();

private:
    Ipp32s  m_width, m_height;
    Ipp32f  *m_mu1, *m_mu2, *m_mu1_sq, *m_mu2_sq, *m_mu1_mu2, *m_tmp;
    Ipp32s  m_step, mc_ksz[3], m_xkidx[3], m_ykidx[3];
    Ipp32f  m_ssim_c1, m_ssim_c2; 
    Ipp32f  m_kernel_values[11+7+5];
    Ipp32f  *mc_krn[3];

    mfxF64 m_ssimAV[3];
    mfxF64 m_ssimBST[3];
    mfxF64 m_ssimWST[3];
    mfxI64 m_nSize;
    mfxF64 m_fLastResults[3];

    mfxU8* m_dataNV12_1[3];
    mfxU8* m_dataNV12_2[3];

    mfxF64 ssim(mfxU8* data1, mfxU32 pitch1, mfxU8* data2, mfxU32 pitch2, IppiSize roi, int id);
};

class MFXYUVDump : public IMFXSurfacesComparator
{
    IMPLEMENT_CLONE(MFXYUVDump);
public:
    MFXYUVDump( const vm_char * file1
              , const vm_char * file2);
    virtual ~MFXYUVDump();
    virtual mfxStatus Reset();
    virtual mfxStatus Compare(mfxFrameSurface1 * pIn1, mfxFrameSurface1 * pIn2);
    virtual mfxStatus GetLastCmpResult(double pResult[3]);
    virtual mfxStatus GetOveralResult (double pResult[3], mfxU32);
    virtual mfxStatus GetAverageResult(double pResult[3]); 
    virtual mfxStatus GetMaxResult(mfxF64 pResult[3]);
    virtual mfxStatus GetMinResult(mfxF64 pResult[3]);
    virtual vm_char*  GetMetricName();

protected:
    std::auto_ptr<IMFXVideoRender> m_pRnd1;
    std::auto_ptr<IMFXVideoRender> m_pRnd2;
    vm_char          m_pFile1[MAX_PATH];
    vm_char          m_pFile2[MAX_PATH];
    bool           m_bInited;
};

