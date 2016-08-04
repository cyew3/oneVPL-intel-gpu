/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */
#include "mfx_pipeline_defs.h"
#include "mfx_metric_calc.h"
#include <math.h>
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"
#include "mfx_renders.h"
//#include "mfx_msu_adapter.h"

MFXPsnrCalc::MFXPsnrCalc()
{
    Reset();
}

MFXPsnrCalc::~MFXPsnrCalc()
{
}

mfxStatus MFXPsnrCalc::Reset()
{
    MFX_ZERO_MEM(m_nSize);
    MFX_ZERO_MEM(m_fpsnrAV);
    MFX_ZERO_MEM(m_fLastResults);
    MFX_ZERO_MEM(m_fpsnrCumulative);

    MFX_FOR(3, m_fpsnrWST[i] = -1);
    MFX_FOR(3, m_fpsnrBST[i] = -1);

    m_nFrame = 0;

    return MFX_ERR_NONE;
}

vm_char* MFXPsnrCalc::GetMetricName()
{
    return const_cast<vm_char*>(VM_STRING("PSNR"));
}

mfxF64 MFXPsnrCalc::CalcPSNR(mfxF64 norm2, mfxI64 size, mfxU32 fourcc)
{
    if(size == 0)
        return 0;

    if (!norm2)
        return 100;

    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
        return 10.0 * log10(((mfxF64) (255.0 * 255.0)) *
            ((mfxF64) size) /
            ((mfxF64) norm2));
    case MFX_FOURCC_P010:
        return 10.0 * log10(((mfxF64) (1023.0 * 1023.0)) *
            ((mfxF64) size) /
            ((mfxF64) norm2));
    default:
        return 0.0;
    }
}


mfxStatus MFXPsnrCalc::GetLastCmpResult(double pResult[3])
{
    MFX_FOR(3, pResult[i] = m_fLastResults[i]);

    return MFX_ERR_NONE;
}

mfxStatus MFXPsnrCalc::GetOveralResult(double pResult[3], mfxU32 fourcc)
{
    MFX_FOR(3, pResult[i] = CalcPSNR(m_fpsnrAV[i], m_nSize[i], fourcc));

    return MFX_ERR_NONE;
}

mfxStatus MFXPsnrCalc::GetAverageResult(double pResult[3])
{
    MFX_FOR(3, pResult[i] = m_nFrame > 0 ? m_fpsnrCumulative[i]/m_nFrame : -1);

    return MFX_ERR_NONE;
}

mfxStatus MFXPsnrCalc::GetMaxResult(mfxF64 pResult[3])
{
    MFX_FOR(3, pResult[i] = m_fpsnrWST[i]);

    return MFX_ERR_NONE;
}

mfxStatus MFXPsnrCalc::GetMinResult(mfxF64 pResult[3])
{
    MFX_FOR(3, pResult[i] = m_fpsnrBST[i]);

    return MFX_ERR_NONE;
}

mfxStatus MFXPsnrCalc::Compare( mfxFrameSurface1 * pIn1
                              , mfxFrameSurface1 * pIn2)

{
    MFX_CHECK_POINTER(pIn1);
    MFX_CHECK_POINTER(pIn2);

    MFX_CHECK(pIn1->Info.CropW == pIn2->Info.CropW);
    MFX_CHECK(pIn1->Info.CropH == pIn2->Info.CropH);
    MFX_CHECK(pIn1->Info.FourCC == pIn2->Info.FourCC);

    MFX_ZERO_MEM(m_fLastResults);

    TIME_START();

    mfxU32 pitch1 = pIn1->Data.PitchLow + ((mfxU32)pIn1->Data.PitchHigh << 16);
    mfxU32 pitch2 = pIn2->Data.PitchLow + ((mfxU32)pIn2->Data.PitchHigh << 16);

    mfxU8* data1 = pIn1->Data.Y + pIn1->Info.CropY * pitch1 + pIn1->Info.CropX;
    mfxU8* data2 = pIn2->Data.Y + pIn2->Info.CropY * pitch2 + pIn2->Info.CropX;

    IppiSize roi = {pIn1->Info.CropW, pIn1->Info.CropH};
    double mse = 0.0;
    double delim = (MFX_FOURCC_P010 == pIn1->Info.FourCC && pIn1->Info.Shift) ? 64.0 : 1.0; // shifted format alighnment

    switch (pIn1->Info.FourCC)
    {
    case MFX_FOURCC_NV12:
        ippiNormDiff_L2_8u_C1R(data1, pitch1, data2, pitch2, roi, &mse);
        m_fLastResults[0] += mse * mse;

        roi.width  = 1;
        roi.height = pIn1->Info.CropW >> 1;
        data1 = pIn1->Data.UV + (pIn1->Info.CropY >> 1) * pitch1 + pIn1->Info.CropX;
        data2 = pIn2->Data.UV + (pIn2->Info.CropY >> 1) * pitch2 + pIn2->Info.CropX;

        for (int i = 0; i < pIn1->Info.CropH >> 1; i++)
        {
            ippiNormDiff_L2_8u_C1R( data1 + i * pitch1, 2, data2 + i * pitch2, 2, roi, &mse);
            m_fLastResults[1] += mse * mse;
        }

        data1 = 1 + pIn1->Data.UV + (pIn1->Info.CropY >> 1) * pitch1 + pIn1->Info.CropX;
        data2 = 1 + pIn2->Data.UV + (pIn2->Info.CropY >> 1) * pitch2 + pIn2->Info.CropX;

        for (int i = 0; i < pIn1->Info.CropH >> 1; i++)
        {
            ippiNormDiff_L2_8u_C1R( data1 + i * pitch1, 2, data2 + i * pitch2, 2, roi, &mse);
            m_fLastResults[2] += mse * mse;
        }

        break;
    case MFX_FOURCC_P010:
        if (pIn1->Info.Shift) ippiLShiftC_16u_C1R((const mfxU16*)data2, pitch2, 6, (mfxU16*)data2, pitch2, roi);
        ippiNormDiff_L2_16u_C1R((mfxU16*)data1, pitch1, (mfxU16*)data2, pitch2, roi, &mse);
        m_fLastResults[0] += (mse / delim) * (mse / delim);

        roi.width  = 1;
        roi.height = pIn1->Info.CropW >> 1;
        data1 = (mfxU8*)((mfxU16*)pIn1->Data.UV + (pIn1->Info.CropY >> 1) * pitch1 + pIn1->Info.CropX);
        data2 = (mfxU8*)((mfxU16*)pIn2->Data.UV + (pIn2->Info.CropY >> 1) * pitch2 + pIn2->Info.CropX);

        for (int i = 0; i < pIn1->Info.CropH >> 1; i++)
        {
            if (pIn1->Info.Shift) ippiLShiftC_16u_C1R((const mfxU16*)data2 + i * pitch2/2, 4, 6, (mfxU16*)data2 + i * pitch2/2, 4, roi);
            ippiNormDiff_L2_16u_C1R( (mfxU16*)data1 + i * pitch1/2, 4, (mfxU16*)data2 + i * pitch2/2, 4, roi, &mse);
            m_fLastResults[1] += (mse / delim) * (mse / delim);
        }

        data1 = (mfxU8*)(1 + (mfxU16*)pIn1->Data.UV + (pIn1->Info.CropY >> 1) * pitch1 + pIn1->Info.CropX);
        data2 = (mfxU8*)(1 + (mfxU16*)pIn2->Data.UV + (pIn2->Info.CropY >> 1) * pitch2 + pIn2->Info.CropX);

        for (int i = 0; i < pIn1->Info.CropH >> 1; i++)
        {
            if (pIn1->Info.Shift) ippiLShiftC_16u_C1R((const mfxU16*)data2 + i * pitch2/2, 4, 6, (mfxU16*)data2 + i * pitch2/2, 4, roi);
            ippiNormDiff_L2_16u_C1R( (mfxU16*)data1 + i * pitch1/2, 4, (mfxU16*)data2 + i * pitch2/2, 4, roi, &mse);
            m_fLastResults[2] += (mse / delim) * (mse / delim);
        }

        break;
    case MFX_FOURCC_YV12:
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    MFX_FOR(3, m_fpsnrAV[i] += (mfxU64)m_fLastResults[i]);

    for (int i = 0; i < 3; i++)
    {
        mfxU64 imageSize = pIn1->Info.CropW * pIn1->Info.CropH;
        if (i != 0)
        {
            imageSize >>= 2;
        }
        m_nSize[i] += imageSize;
        m_fLastResults[i] = CalcPSNR(m_fLastResults[i], imageSize, pIn1->Info.FourCC);
    }

    m_nFrame++;
    TIME_PRINT(VM_STRING("CalcPSNR"));
    //PipelineTrace((VM_STRING("PSNR y=%.2lf u=%.2lf v=%.2lf\n"), m_fLastResults[0], m_fLastResults[1], m_fLastResults[2]));
    MFX_FOR(3, m_fpsnrCumulative[i] += m_fLastResults[i]);
    MFX_FOR(3, if (m_fLastResults[i] > m_fpsnrWST[i]) m_fpsnrWST[i] = m_fLastResults[i]);
    MFX_FOR(3, if (m_fLastResults[i] < m_fpsnrBST[i] || m_fpsnrBST[i] == -1) m_fpsnrBST[i] = m_fLastResults[i]);

    return MFX_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////

#define IPP_INT_PTR(p)  (Ipp8u*)(p)
static int GaussianKernel (int KernelSize, Ipp32f sigma, Ipp32f* pKernel) {
    int i;

    Ipp32f val, sum = 0.0f;
    for (i = 0; i < KernelSize; i++) {
      val = (Ipp32f)(i - KernelSize / 2);
      pKernel[i] = (Ipp32f)exp(- (Ipp32f)(val * val) / (Ipp32f)(2.0f * sigma * sigma));
      sum += pKernel[i];
    }

    for (i = 0; i < KernelSize; i++) pKernel[i] /= sum;

    return 0;
}

static void testFastSSIM_32f (
    const Ipp32f* pSrc1, int src1Step, const Ipp32f* pSrc2, int src2Step, const Ipp32f* pSrc3, int src3Step,
    const Ipp32f* pSrc4, int src4Step, const Ipp32f* pSrc5, int src5Step, Ipp32f* pDst, int dstStep,
    IppiSize roiSize, Ipp32f C1, Ipp32f C2)
{
    int     i, j;
    Ipp32f  t1, t2, t3, t4, *pMx, *pMy, *pSx2, *pSy2, *pSxy, *pDi;

    C2 += C1;
#ifdef _OPENMP
#pragma omp parallel for default(shared) private(i, t1, t2, t3, t4, pMx, pMy, pSx2, pSy2, pSxy, pDi)
#endif
    for(j=0; j<roiSize.height; j++) {
        pMx  = (Ipp32f*)(IPP_INT_PTR(pSrc1)+j*src1Step); pMy  = (Ipp32f*)(IPP_INT_PTR(pSrc2)+j*src2Step);
        pSx2 = (Ipp32f*)(IPP_INT_PTR(pSrc3)+j*src3Step); pSy2 = (Ipp32f*)(IPP_INT_PTR(pSrc4)+j*src4Step);
        pSxy = (Ipp32f*)(IPP_INT_PTR(pSrc5)+j*src5Step); pDi  = (Ipp32f*)(IPP_INT_PTR(pDst )+j*dstStep);
        for(i=0; i<roiSize.width; i++) {
            t1 = (*pMx)*(*pMy); t1 = t1 + t1 + C1; t2 = (*pSxy) + (*pSxy) - t1 + C2;
            t3 = (*pMx)*(*pMx)+(*pMy)*(*pMy) + C1; t4 = (*pSx2) + (*pSy2) - t3 + C2;
            t2 *= t1; t4 *= t3;
            *pDi = (t4 >= IPP_EPS_32F)? (t2/t4) : ((t3 >= IPP_EPS_32F)? (t1/t3) : (1.0f));
            pMx++; pMy++; pSx2++; pSy2++; pSxy++; pDi++;
        }
    }
}

mfxF64 MFXSSIMCalc::ssim(mfxU8* data1, mfxU32 pitch1, mfxU8* data2, mfxU32 pitch2, IppiSize roi, int id)
{
    IppiSize    flt, flt_h;
    int         shift, shift_h;

   //SSIM
    ippiConvert_8u32f_C1R(data1,pitch1,m_mu1,m_step, roi);
    ippiConvert_8u32f_C1R(data2,pitch2,m_mu2,m_step, roi);
    ippiSqr_32f_C1R(m_mu1, m_step, m_mu1_sq, m_step, roi);
    ippiSqr_32f_C1R(m_mu2, m_step, m_mu2_sq, m_step, roi);
    ippiMul_32f_C1R(m_mu1, m_step, m_mu2, m_step, m_mu1_mu2, m_step, roi);

    flt.width  = roi.width  - (mc_ksz[m_xkidx[id]]&~1);
    flt.height = roi.height - (mc_ksz[m_ykidx[id]]&~1);
    shift = (mc_ksz[m_xkidx[id]]>>1) + (mc_ksz[m_ykidx[id]]>>1)*m_step/sizeof(Ipp32f);

    flt_h.width  = roi.width  - (mc_ksz[m_xkidx[id]]&~1);
    flt_h.height = roi.height;
    shift_h = (mc_ksz[m_xkidx[id]]>>1);

    ippiFilterRow_32f_C1R(m_mu1+shift_h, m_step, m_tmp+shift_h, m_step, flt_h, mc_krn[m_xkidx[id]], mc_ksz[m_xkidx[id]], mc_ksz[m_xkidx[id]]>>1);
    ippiFilterColumn_32f_C1R(m_tmp+shift, m_step, m_mu1+shift, m_step, flt, mc_krn[m_ykidx[id]], mc_ksz[m_ykidx[id]], mc_ksz[m_ykidx[id]]>>1);
    ippiFilterRow_32f_C1R(m_mu2+shift_h, m_step, m_tmp+shift_h, m_step, flt_h, mc_krn[m_xkidx[id]], mc_ksz[m_xkidx[id]], mc_ksz[m_xkidx[id]]>>1);
    ippiFilterColumn_32f_C1R(m_tmp+shift, m_step, m_mu2+shift, m_step, flt, mc_krn[m_ykidx[id]], mc_ksz[m_ykidx[id]], mc_ksz[m_ykidx[id]]>>1);
    ippiFilterRow_32f_C1R(m_mu1_sq+shift_h, m_step, m_tmp+shift_h, m_step, flt_h, mc_krn[m_xkidx[id]], mc_ksz[m_xkidx[id]], mc_ksz[m_xkidx[id]]>>1);
    ippiFilterColumn_32f_C1R(m_tmp+shift, m_step, m_mu1_sq+shift, m_step, flt, mc_krn[m_ykidx[id]], mc_ksz[m_ykidx[id]], mc_ksz[m_ykidx[id]]>>1);
    ippiFilterRow_32f_C1R(m_mu2_sq+shift_h, m_step, m_tmp+shift_h, m_step, flt_h, mc_krn[m_xkidx[id]], mc_ksz[m_xkidx[id]], mc_ksz[m_xkidx[id]]>>1);
    ippiFilterColumn_32f_C1R(m_tmp+shift, m_step, m_mu2_sq+shift, m_step, flt, mc_krn[m_ykidx[id]], mc_ksz[m_ykidx[id]], mc_ksz[m_ykidx[id]]>>1);
    ippiFilterRow_32f_C1R(m_mu1_mu2+shift_h, m_step, m_tmp+shift_h, m_step, flt_h, mc_krn[m_xkidx[id]], mc_ksz[m_xkidx[id]], mc_ksz[m_xkidx[id]]>>1);
    ippiFilterColumn_32f_C1R(m_tmp+shift, m_step, m_mu1_mu2+shift, m_step, flt, mc_krn[m_ykidx[id]], mc_ksz[m_ykidx[id]], mc_ksz[m_ykidx[id]]>>1);

    testFastSSIM_32f(m_mu1+shift, m_step, m_mu2+shift, m_step, m_mu1_sq+shift, m_step,
        m_mu2_sq+shift, m_step, m_mu1_mu2+shift, m_step, m_tmp+shift, m_step, flt, m_ssim_c1, m_ssim_c2);

    mfxF64 ssim_idx;
    ippiMean_32f_C1R(m_tmp+shift, m_step, flt, &ssim_idx, ippAlgHintAccurate);

    return ssim_idx;
}

MFXSSIMCalc::MFXSSIMCalc()
: m_width(0)
, m_height(0)
, m_mu1(NULL)
, m_mu2(NULL)
, m_mu1_sq(NULL)
, m_mu2_sq(NULL)
, m_mu1_mu2(NULL)
, m_tmp(NULL)
, m_nSize(0)
{
    m_ssim_c1 = 6.5025f; m_ssim_c2 = 58.5225f;
    GaussianKernel(11, 1.5,   m_kernel_values);
    GaussianKernel( 7, 0.75,  m_kernel_values+11);
    GaussianKernel( 5, 0.375, m_kernel_values+18);
    mc_krn[0] = m_kernel_values; mc_krn[1] = m_kernel_values+11; mc_krn[2] = m_kernel_values+18;
    mc_ksz[0] = 11; mc_ksz[1] = 7; mc_ksz[2] = 5;
    m_xkidx[0] = m_xkidx[1] = m_xkidx[2] = m_ykidx[0] = m_ykidx[1] = m_ykidx[2] = 0;

    m_dataNV12_1[0] = m_dataNV12_1[1] = m_dataNV12_1[2] = 0;
    m_dataNV12_2[0] = m_dataNV12_2[1] = m_dataNV12_2[2] = 0;

    Reset();
}

MFXSSIMCalc::~MFXSSIMCalc()
{
    if( m_mu1 ) ippsFree(m_mu1); 
    if( m_mu2 ) ippsFree(m_mu2); 
    if( m_mu1_sq ) ippsFree(m_mu1_sq); 
    if( m_mu2_sq ) ippsFree(m_mu2_sq); 
    if( m_mu1_mu2 ) ippsFree(m_mu1_mu2); 
    if( m_tmp ) ippsFree(m_tmp);
    if(m_dataNV12_1[0]) ippsFree(m_dataNV12_1[0]);
    if(m_dataNV12_1[1]) ippsFree(m_dataNV12_1[1]);
    if(m_dataNV12_1[2]) ippsFree(m_dataNV12_1[2]);
    if(m_dataNV12_2[0]) ippsFree(m_dataNV12_2[0]);
    if(m_dataNV12_2[1]) ippsFree(m_dataNV12_2[1]);
    if(m_dataNV12_2[2]) ippsFree(m_dataNV12_2[2]);
}

mfxStatus MFXSSIMCalc::Reset()
{
    m_nSize = 0;
    MFX_FOR(3, m_ssimWST[i] = 1.1);
    MFX_FOR(3, m_ssimAV[i] = 0.0);
    MFX_FOR(3, m_ssimBST[i] = 0);
    MFX_FOR(3, m_fLastResults[i] = 1);

    return MFX_ERR_NONE;
}

vm_char* MFXSSIMCalc::GetMetricName()
{
    return const_cast<vm_char*>(VM_STRING("SSIM"));
}

mfxStatus MFXSSIMCalc::GetLastCmpResult(double pResult[3])
{
    if(m_nSize == 0)
        return MFX_ERR_UNKNOWN;

    MFX_FOR(3, pResult[i] = m_fLastResults[i]);
    return MFX_ERR_NONE;
}

mfxStatus MFXSSIMCalc::GetAverageResult(double pResult[3])
{
    // There is no real calculation for SSIM
    MFX_FOR(3, pResult[i] = -1);
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXSSIMCalc::GetOveralResult(double pResult[3], mfxU32)
{
    if(m_nSize)
    {
        MFX_FOR(3, pResult[i] = m_ssimAV[i]/m_nSize);
    }
    else
        return MFX_ERR_UNKNOWN;
    return MFX_ERR_NONE;
}

mfxStatus MFXSSIMCalc::Compare(mfxFrameSurface1 * pIn1, mfxFrameSurface1 * pIn2)
{
    MFX_CHECK_POINTER(pIn1);
    MFX_CHECK_POINTER(pIn2);

    MFX_CHECK(pIn1->Info.CropW == pIn2->Info.CropW);
    MFX_CHECK(pIn1->Info.CropH == pIn2->Info.CropH);
    MFX_CHECK(pIn1->Info.FourCC == pIn2->Info.FourCC);

    MFX_ZERO_MEM(m_fLastResults);
    
    mfxU8* data1[3];
    int    step1[3];
    mfxU8* data2[3];
    int    step2[3];

    TIME_START();

    mfxU32 pitch1 = pIn1->Data.PitchLow + ((mfxU32)pIn1->Data.PitchHigh << 16);
    mfxU32 pitch2 = pIn2->Data.PitchLow + ((mfxU32)pIn2->Data.PitchHigh << 16);

    data1[0] = pIn1->Data.Y + pIn1->Info.CropY * pitch1 + pIn1->Info.CropX;
    data2[0] = pIn2->Data.Y + pIn2->Info.CropY * pitch2 + pIn2->Info.CropX;
    data1[1] = pIn1->Data.U + (pIn1->Info.CropY >> 1) * pitch1 + pIn1->Info.CropX;
    data2[1] = pIn2->Data.U + (pIn2->Info.CropY >> 1) * pitch2 + pIn2->Info.CropX;
    data1[2] = pIn1->Data.V + (pIn1->Info.CropY >> 1) * pitch1 + pIn1->Info.CropX;
    data2[2] = pIn2->Data.V + (pIn2->Info.CropY >> 1) * pitch2 + pIn2->Info.CropX;
    step1[0] = step1[1] = step1[2] = pitch1;
    step2[0] = step2[1] = step2[2] = pitch2;

    IppiSize     roi   = {pIn1->Info.CropW, pIn1->Info.CropH};

    if(roi.width != m_width || roi.height != m_height)
    {
        if( m_mu1 ) ippsFree(m_mu1); 
        if( m_mu2 ) ippsFree(m_mu2); 
        if( m_mu1_sq ) ippsFree(m_mu1_sq); 
        if( m_mu2_sq ) ippsFree(m_mu2_sq); 
        if( m_mu1_mu2 ) ippsFree(m_mu1_mu2); 
        if( m_tmp ) ippsFree(m_tmp);
        m_mu1     = ippiMalloc_32f_C1(roi.width, roi.height, &m_step);
        m_mu2     = ippiMalloc_32f_C1(roi.width, roi.height, &m_step);
        m_mu1_sq  = ippiMalloc_32f_C1(roi.width, roi.height, &m_step);
        m_mu2_sq  = ippiMalloc_32f_C1(roi.width, roi.height, &m_step);
        m_mu1_mu2 = ippiMalloc_32f_C1(roi.width, roi.height, &m_step);
        m_tmp     = ippiMalloc_32f_C1(roi.width, roi.height, &m_step);

        if(pIn1->Info.FourCC == MFX_FOURCC_NV12)
        {
            m_dataNV12_1[0] = ippiMalloc_8u_C1(roi.width, roi.height, &step1[0]);
            m_dataNV12_2[0] = ippiMalloc_8u_C1(roi.width, roi.height, &step2[0]);
            for(int i=1; i<3; i++)
            {
                m_dataNV12_1[i] = ippiMalloc_8u_C1(roi.width>>1, roi.height>>1, &step1[i]);
                m_dataNV12_2[i] = ippiMalloc_8u_C1(roi.width>>1, roi.height>>1, &step2[i]);
                m_xkidx[i]=1;
                m_ykidx[i]=1;
            }
        }
    }

    m_fLastResults[0] = ssim(data1[0], pitch1, data2[0], pitch2, roi, 0);
 
    switch (pIn1->Info.FourCC)
    {
        case MFX_FOURCC_NV12:
        {
            //convert to YV12 
            ippiYCbCr420ToYCrCb420_8u_P2P3R(data1[0], pitch1,
                pIn1->Data.UV + (pIn1->Info.CropY >> 1) * pitch1 + pIn1->Info.CropX, pitch1,
                m_dataNV12_1, step1, roi);
            
            ippiYCbCr420ToYCrCb420_8u_P2P3R(data2[0], pitch2,
                pIn2->Data.UV + (pIn2->Info.CropY >> 1) * pitch2 + pIn2->Info.CropX, pitch2,
                m_dataNV12_2, step2, roi);
            //fprintf(stderr,"cy=%d cx=%d p=%d s=%d\n", pIn2->Info.CropY , pIn2->Info.CropX, pIn2->Data.Pitch, step2[1] );

            for(int i=0; i<3; i++)
            {
                data1[i] = m_dataNV12_1[i];
                data2[i] = m_dataNV12_2[i];
            }
        }
    case MFX_FOURCC_YV12:
        {
            IppiSize roi_uv = {roi.width>>1, roi.height>>1};
            m_fLastResults[1] = ssim(data1[1], step1[1], data2[1], step2[1], roi_uv, 1);
            m_fLastResults[2] = ssim(data1[2], step1[2], data2[2], step2[2], roi_uv, 2);
        }
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    MFX_FOR(3, m_ssimAV[i] += m_fLastResults[i]);

    TIME_PRINT(VM_STRING("CalcSSIM"));
    //PipelineTrace((VM_STRING("PSNR y=%.2lf u=%.2lf v=%.2lf\n"), m_fLastResults[0], m_fLastResults[1], m_fLastResults[2]));
    MFX_FOR(3, if (m_fLastResults[i] < m_ssimWST[i]) m_ssimWST[i] = m_fLastResults[i]);
    MFX_FOR(3, if (m_fLastResults[i] > m_ssimBST[i]) m_ssimBST[i] = m_fLastResults[i]);
    m_nSize++;

    return MFX_ERR_NONE;
}

mfxStatus MFXSSIMCalc::GetMaxResult(mfxF64 pResult[3])
{
    MFX_FOR(3, pResult[i] = m_ssimBST[i]);
    return MFX_ERR_NONE;
}

mfxStatus MFXSSIMCalc::GetMinResult(mfxF64 pResult[3])
{
    MFX_FOR(3, pResult[i] = m_ssimWST[i]);
    return MFX_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////

MFXYUVDump::MFXYUVDump( const vm_char * file1
                      , const vm_char * file2)
{
//    mfxStatus sts;
//    m_pRnd1.reset( new MFXFileWriteRender(NULL, &sts));
    //m_pRnd2.reset( new MFXFileWriteRender(NULL, &sts));
    m_bInited = false;
    vm_string_strcpy_s(m_pFile1, MFX_ARRAY_SIZE(m_pFile1), file1);
    vm_string_strcpy_s(m_pFile2, MFX_ARRAY_SIZE(m_pFile2), file2);
}

MFXYUVDump::~MFXYUVDump()
{
}

mfxStatus MFXYUVDump::Reset()
{
    return MFX_ERR_NONE;
}

vm_char* MFXYUVDump::GetMetricName()
{
    return const_cast<vm_char*>(VM_STRING("DUMP"));
}

mfxStatus MFXYUVDump::Compare(mfxFrameSurface1 * pIn1, mfxFrameSurface1 * pIn2)
{
    MFX_CHECK_POINTER(pIn1);
    MFX_CHECK_POINTER(pIn2);

    MFX_CHECK_POINTER(m_pRnd1.get());
    MFX_CHECK_POINTER(m_pRnd2.get());

    if (!m_bInited)
    {
        m_bInited = true;

        mfxVideoParam vparam;
        vparam.mfx.FrameInfo = pIn1->Info;

        MFX_CHECK_STS(m_pRnd1->Init(&vparam, m_pFile1));
        MFX_CHECK_STS(m_pRnd2->Init(&vparam, m_pFile2));
    }

    MFX_CHECK_STS(m_pRnd1->RenderFrame(pIn1));
    MFX_CHECK_STS(m_pRnd2->RenderFrame(pIn2));

    return MFX_ERR_NONE;
}

mfxStatus MFXYUVDump::GetLastCmpResult(double * /*pResult[3]*/)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXYUVDump::GetOveralResult(double * /*pResult[3]*/, mfxU32)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXYUVDump::GetAverageResult(double pResult[3])
{
     // There is no real calculation
    MFX_FOR(3, pResult[i] = -1);
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXYUVDump::GetMaxResult(mfxF64 pResult[3])
{
    UNREFERENCED_PARAMETER(pResult);
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXYUVDump::GetMinResult(mfxF64 pResult[3])
{
    UNREFERENCED_PARAMETER(pResult);
    return MFX_ERR_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////

ComparatorFactory::ComparatorFactory()
{
    RegisterComparators();
}

ComparatorFactory::~ComparatorFactory()
{
    std::for_each(m_prototypes.begin(), m_prototypes.end(), second_of(deleter<IMFXSurfacesComparator*>()));
}

void ComparatorFactory::RegisterComparators()
{
    m_prototypes[METRIC_SSIM] = new MFXSSIMCalc();
    m_prototypes[METRIC_PSNR] = new MFXPsnrCalc();
    m_prototypes[METRIC_DUMP] = new MFXYUVDump(VM_STRING("c:\\1.yuv"), VM_STRING("c:\\2.yuv"));
}

mfxStatus ComparatorFactory::CreateComparatorByType(MetricType type, std::auto_ptr<IMFXSurfacesComparator> & ppCmp)
{
    if (type == METRIC_NONE)
    {
        return MFX_ERR_NONE;
    }

    IMFXSurfacesComparator *pObj;
    if(NULL == (pObj = m_prototypes[type]))
    {
        return MFX_ERR_UNSUPPORTED;
    }

    ppCmp.reset(pObj->Clone());
    return MFX_ERR_NONE;
}