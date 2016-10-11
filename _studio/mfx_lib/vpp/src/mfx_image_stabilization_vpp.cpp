/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
//          Resize for Video Pre\Post Processing
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_VPP) && defined(MFX_ENABLE_IMAGE_STABILIZATION_VPP)
#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "mfx_image_stabilization_vpp.h"
#include "mfx_vpp_utils.h"

/* ******************************************************************** */
/*                 implementation of Interface functions [Img Stab]     */
/* ******************************************************************** */

mfxStatus MFXVideoVPPImgStab::Query( mfxExtBuffer* pHint )
{
    if( NULL == pHint )
    {
        return MFX_ERR_NONE;
    }

    mfxExtVPPImageStab* pParam = (mfxExtVPPImageStab*)pHint;

    if( MFX_IMAGESTAB_MODE_UPSCALE == pParam->Mode || 
        MFX_IMAGESTAB_MODE_BOXING  == pParam->Mode)
    {
        return MFX_ERR_NONE;
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    } 

} // static mfxStatus MFXVideoVPPDenoise::Query( mfxExtBuffer* pHint )

#if !defined (MFX_ENABLE_HW_ONLY_VPP)

#include <assert.h>
#include <math.h>
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"

#include "videovme7_5io.h"
#include "videovme.h"
#include "mfx_common_int.h"

// aya: reason is VME. we prefer doesn't modify one
#pragma warning( disable : 4244 )

/* ******************************************************************** */
/*                 configuration params of the alg                      */
/* ******************************************************************** */

const mfxU32 PAR_BYPASS  = 0;
const mfxF64 PAR_SIGMA =1.5;
const mfxU32 PAR_X_DOWNSAMPLE = 160;
const mfxU32 PAR_Y_DOWNSAMPLE = 120;
const mfxU32 PAR_ME_RANGE_X   = 24;
const mfxU32 PAR_ME_RANGE_Y   = 16;
const mfxU32 PAR_MIN_COST_THRESHOLD = 7680;
const mfxU32 PAR_BLOCK_SIZE       = 16;
const mfxF64 PAR_CROP_PERCENTAGE  =    0.09;
const mfxU32 PAR_MOTIONMODEL      = 0;
const mfxU32 PAR_NUM_FRAME_DELAY  = 0;

/* ******************************************************************** */
/*                 prototype of Image Stabilization algorithm           */
/* ******************************************************************** */

Ipp8u Interpolation(
    Ipp8u *p_buf, 
    int pitch, 
    int width,
    int height,
    double x,
    double y, 
    int luma, 
    int mode);

MFXVideoVPPImgStab::Config::Config(void)
: m_sigma(PAR_SIGMA)
, m_DownWidth(PAR_X_DOWNSAMPLE)
, m_DownHeight(PAR_Y_DOWNSAMPLE)
, m_ME_Range_X(PAR_ME_RANGE_X)
, m_ME_Range_Y(PAR_ME_RANGE_Y)
, m_MinCostThreshold(PAR_MIN_COST_THRESHOLD)
, m_BlockSize(PAR_BLOCK_SIZE)
, m_cf(PAR_CROP_PERCENTAGE)
, m_MotionModel(PAR_MOTIONMODEL)
, m_FrameDelay(PAR_NUM_FRAME_DELAY)
{
}


MFXVideoVPPImgStab::MFXVideoVPPImgStab(VideoCORE *core, mfxStatus* sts) : FilterVPP(), mGMV(), mCMV(), mblk()
{
    //MFX_CHECK_NULL_PTR1(core);

    m_core     = core;

    m_mode     = MFX_IMAGESTAB_MODE_BOXING; // defaul mode of IS
    m_pWorkBuf = NULL;
    m_vme      = NULL;

    VPP_CLEAN;

    *sts = MFX_ERR_NONE;
    m_scale_h = 0;
    m_scale_v = 0;
    m_destWidth = 0;
    m_downWidth = 0;
    m_destHeight = 0;
    m_heightScale = 0;
    m_downHeight = 0;
    m_frameCount = 0;
    m_height = 0;
    m_width = 0;
    m_nCrpX = 0;
    m_nCrpY = 0;
    m_firstFrame = 0;
    m_widthScale = 0;

} // MFXVideoVPPImgStab::MFXVideoVPPImgStab(VideoCORE *core, mfxStatus* sts ) : FilterVPP()


MFXVideoVPPImgStab::~MFXVideoVPPImgStab(void)
{
    Close();

    return;

} // MFXVideoVPPImgStab::~MFXVideoVPPImgStab(void)


mfxStatus MFXVideoVPPImgStab::SetParam( mfxExtBuffer* pHint )
{

    mfxStatus mfxSts = MFX_ERR_NONE;

    if( pHint )
    {
        mfxExtVPPImageStab* pImgStabParams = (mfxExtVPPImageStab*)pHint;

        if( pImgStabParams->Mode == MFX_IMAGESTAB_MODE_BOXING )
        {
            m_mode = MFX_IMAGESTAB_MODE_BOXING;
        }
        else if( pImgStabParams->Mode == MFX_IMAGESTAB_MODE_UPSCALE )
        {
            m_mode = MFX_IMAGESTAB_MODE_UPSCALE;
        }
        else
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    return mfxSts;

} // mfxStatus MFXVideoVPPImgStab::SetParam( mfxExtBuffer* pHint )


mfxStatus MFXVideoVPPImgStab::Reset(mfxVideoParam *par)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    IppStatus ippSts = ippStsNoErr;

    mfxU32 sizeBuf = 0;

    MFX_CHECK_NULL_PTR1( par );
    VPP_CHECK_NOT_INITIALIZED;

    mfxExtBuffer* pHint = NULL;

    GetFilterParam(par, MFX_EXTBUFF_VPP_IMAGE_STABILIZATION, &pHint);

    SetParam( pHint );

    /* reset resize work buffer */
    mfxSts = GetBufferSize( &sizeBuf );
    MFX_CHECK_STS( mfxSts );

    if( sizeBuf )
    {
        ippSts = ippsZero_8u( &m_pWorkBuf[0], sizeBuf );
        VPP_CHECK_IPP_STS( ippSts );
    }

    VPP_RESET;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPImgStab::Reset(mfxVideoParam *par)


mfxStatus MFXVideoVPPImgStab::RunFrameVPP(
    mfxFrameSurface1 *in,
    mfxFrameSurface1 *out,
    InternalParam* pParam)
{
    // code error
    mfxStatus mfxSts = MFX_ERR_NONE;

    VPP_CHECK_NOT_INITIALIZED;

    if( NULL == in )
    {
        return MFX_ERR_MORE_DATA;
    }
    if( NULL == out )
    {
        return MFX_ERR_NULL_PTR;
    }
    if( NULL == pParam )
    {
        return MFX_ERR_NULL_PTR;
    }

    mfxSts = DoImageStabilization(
        in, 
        (MFX_IMAGESTAB_MODE_UPSCALE == m_mode) ? &m_preOut : out);
   MFX_CHECK_STS( mfxSts );

    if( MFX_IMAGESTAB_MODE_UPSCALE == m_mode )
    {
        mfxSts = DoUpscale( &m_preOut, out);
        MFX_CHECK_STS( mfxSts );
    }

    if( !m_errPrtctState.isFirstFrameProcessed )
    {
        m_errPrtctState.isFirstFrameProcessed = true;
    }

    pParam->outPicStruct = pParam->inPicStruct;
    pParam->outTimeStamp = pParam->inTimeStamp;

    return mfxSts;

} // mfxStatus MFXVideoVPPImgStab::RunFrameVPP(...)


mfxStatus MFXVideoVPPImgStab::Init(mfxFrameInfo* In, mfxFrameInfo* Out)
{
    //mfxU16    srcW   = 0, srcH = 0;
    //mfxU16    dstW   = 0, dstH = 0;

    MFX_CHECK_NULL_PTR2( In, Out );

    VPP_CHECK_MULTIPLE_INIT;

    // Check InputParam()
    // Check OutputParam()

    /* save init params to prevent core crash */
    m_errPrtctState.In  = *In;
    m_errPrtctState.Out = *Out;

    // configuration
    //-----------------------------------------------------
    m_width = In->Width;
    m_height= In->Height;
    m_downWidth  = m_config.m_DownWidth;
    m_downHeight = m_config.m_DownHeight;
    m_nCrpX = ((mfxU32(m_width*m_config.m_cf) + 8) >> 4) << 4;
    m_nCrpY = ((mfxU32(m_height*m_config.m_cf) + 8) >> 4) << 4;
    m_destWidth  = m_width - (m_nCrpX << 1);
    m_destHeight = m_height - (m_nCrpY << 1);

    m_scale_h = IPP_MAX(1, (m_width)/m_downWidth);
    m_scale_v = IPP_MAX(1, (m_height)/m_downHeight);

    m_widthScale = m_width/m_scale_h;
    m_heightScale = m_height/m_scale_v;

    InitMotionSmoothFilter(m_config.m_sigma);

    m_firstFrame = true;
    m_frameCount = 0;

    m_GMVArray.clear();
    m_CMVArray.clear();
    mBlock.clear();
    mMV.clear();

    ippsZero_8u( (Ipp8u*)m_FrameBuf, 6*sizeof(mfxFrameSurface1*));
    //-----------------------------------------------------

    ippsZero_8u( (Ipp8u*)&m_ref, sizeof(mfxFrameSurface1));
    m_ref.Info = *In;
    m_ref.Info.Width  = (mfxU16)m_widthScale;
    m_ref.Info.Height = (mfxU16)m_heightScale;
    m_ref.Data.Pitch  = (mfxU16)m_widthScale;// it is right!!!

    ippsZero_8u( (Ipp8u*)&m_cur, sizeof(mfxFrameSurface1));
    m_cur.Info = *In;
    m_cur.Info.Width  = (mfxU16)m_widthScale;
    m_cur.Info.Height = (mfxU16)m_heightScale;
    m_cur.Data.Pitch  = (mfxU16)m_widthScale;// it is right!!!

    ippsZero_8u( (Ipp8u*)&m_preOut, sizeof(mfxFrameSurface1));
    m_preOut.Info       = *Out;
    m_preOut.Data.Pitch = m_preOut.Info.Width;


    m_vme = new VideoVME;

    VPP_INIT_SUCCESSFUL;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPImgStab::Init(mfxFrameInfo* In, mfxFrameInfo* Out)


mfxStatus MFXVideoVPPImgStab::Close(void)
{

    VPP_CHECK_NOT_INITIALIZED;

    m_pWorkBuf = NULL;

    for(int i = 0; i < 6; i++)
    {
        if(m_FrameBuf[i])
        {
            m_core->DecreaseReference( &(m_FrameBuf[i]->Data) );
        }
        m_FrameBuf[i] = NULL;
    }

    m_GMVArray.clear();
    m_CMVArray.clear();
    mBlock.clear();
    mMV.clear();

    if( m_vme )
    {
        delete m_vme;
        m_vme = NULL;
    }

    VPP_CLEAN;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPImgStab::Close(void)


mfxStatus MFXVideoVPPImgStab::GetBufferSize( mfxU32* pBufferSize )
{
    SizeInfo sizeInf;

    mfxStatus sts = GetBufferSizeEx( sizeInf );
    MFX_CHECK_STS(sts);

    *pBufferSize = sizeInf.GetTotalSize();

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPImgStab::GetBufferSize( mfxU32* pBufferSize )


// aya: should be synch with GetBufferSize()
mfxStatus MFXVideoVPPImgStab::SetBuffer( mfxU8* pBuffer )
{
    VPP_CHECK_NOT_INITIALIZED;

    SizeInfo sizeInf;
    mfxStatus sts = GetBufferSizeEx( sizeInf );
    MFX_CHECK_STS( sts );

    mfxU32 totalSize = sizeInf.GetTotalSize();

    if( totalSize > 0 )
    {
        MFX_CHECK_NULL_PTR1(pBuffer);

        m_pWorkBuf = pBuffer;

        ippsZero_8u( m_pWorkBuf, totalSize);//sts ignored

        m_ref.Data.Y  = m_pWorkBuf + sizeInf.m_workBufSize;

        m_cur.Data.Y  = m_pWorkBuf + sizeInf.m_workBufSize + sizeInf.m_refSizeY;

        //if( MFX_IMAGESTAB_MODE_UPSCALE == m_mode )
        {
            m_preOut.Data.Y  = m_cur.Data.Y    + sizeInf.m_curSizeY;
            m_preOut.Data.UV = m_preOut.Data.Y + sizeInf.m_preOutSizeY;
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPImgStab::SetBuffer( mfxU8* pBuffer )


// function is called from sync part of VPP only
mfxStatus MFXVideoVPPImgStab::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
{
    if( NULL == in )
    {
        return MFX_ERR_MORE_DATA;
    }
    else if( NULL == out )
    {
        return MFX_ERR_NULL_PTR;
    }

    PassThrough(in, out);

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPImgStab::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )


bool MFXVideoVPPImgStab::IsReadyOutput( mfxRequestType requestType )
{
    if( MFX_REQUEST_FROM_VPP_CHECK == requestType )
    {
        // RS is simple algorithm and need IN to produce OUT
    }

    return false;

} // bool MFXVideoVPPImgStab::IsReadyOutput( mfxRequestType requestType )

/* ******************************************************************** */
/*                 implementation of algorithms [Img Stab]              */
/* ******************************************************************** */

void MFXVideoVPPImgStab::InitMotionSmoothFilter(mfxF64 sigma)
{
    unsigned int i;
    double d = sigma * 2 / (int)(MSF_TAP / 2);
    double v = -sigma * 2;
    double s2 = sigma * sigma;
    double k = 1 / sqrt(2 * 3.14159 * s2), sum = 0;

    for(i = 0; i < MSF_TAP; i++)
    {    
        m_motionSmoothFilter[i] = k * exp(-0.5 * v * v / s2);
        v += d;
        sum += m_motionSmoothFilter[i];
    }

    for(i = 0; i < MSF_TAP; i++)
    {    
        m_motionSmoothFilter[i] /= sum;
    }
    
    return;

} // void MFXVideoVPPImgStab::InitMotionSmoothFilter(void)


mfxStatus MFXVideoVPPImgStab::DoImageStabilization(
    mfxFrameSurface1* in, 
    mfxFrameSurface1* out)
{
    in; out;    

    mfxStatus mfxSts = MFX_ERR_NONE;
    bool bNoOutput   = false;

    if(m_firstFrame)
    {
        m_firstFrame = false;

        mGMV.a = 1.0;
        mGMV.b = mGMV.c = mGMV.d = 0.0;
        mCMV.a = 1.0;
        mCMV.b = mCMV.c = mCMV.d = 0.0;

        m_GMVArray.push_back(mGMV);
        m_CMVArray.push_back(mCMV);

        FrameDownSampling(in, &m_ref);

        m_FrameBuf[0] = in;

        mfxSts = m_core->IncreaseReference( &(m_FrameBuf[0]->Data) );
        MFX_CHECK_STS( mfxSts );

        if (m_config.m_FrameDelay == 0)
        {
            MotionCompensation(
                m_frameCount-m_config.m_FrameDelay,
                m_FrameBuf[m_config.m_FrameDelay],
                out,
                m_width, 
                m_height, 
                m_config.m_FrameDelay);

            PostProcess(out);
        }
        else
        {    
            bNoOutput = true;
        }
        m_frameCount++;

        return (bNoOutput) ? MFX_ERR_MORE_DATA : MFX_ERR_NONE;
    }

    //-----------------------------------------------------
    mBlock.clear();
    mMV.clear();
    int currframe = m_frameCount;currframe;

    FrameDownSampling(in, &m_cur);

    BlockMotionSearch();

    GlobalMPGeneration();
    AccumulatedGlobalMP();

    //update frame buffer
    {
        if(m_FrameBuf[5])
        {
            mfxSts = m_core->DecreaseReference( &(m_FrameBuf[5]->Data) );
            MFX_CHECK_STS(mfxSts);
        }

        for (int i=5; i>0; i--)
        {
            m_FrameBuf[i] = m_FrameBuf[i-1];
        }

        m_FrameBuf[0] = in;

        mfxSts = m_core->IncreaseReference( &(m_FrameBuf[0]->Data) );
        MFX_CHECK_STS( mfxSts );
    }
    
    if (m_frameCount >= m_config.m_FrameDelay)
    {
        MotionCompensation(
                m_frameCount-m_config.m_FrameDelay,
                m_FrameBuf[m_config.m_FrameDelay],
                out,
                m_width, 
                m_height, 
                m_config.m_FrameDelay);

        PostProcess(out);
    }
    else
    {
        bNoOutput = true;
    }
    

    //copy the current image as reference image 
    std::swap(m_ref.Data.Y, m_cur.Data.Y);
    std::swap(m_ref.Data.UV, m_cur.Data.UV);

    m_frameCount++;

    return (bNoOutput) ? MFX_ERR_MORE_DATA : MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPImgStab::DoImageStabilization(...)


mfxStatus MFXVideoVPPImgStab::FrameDownSampling(
    mfxFrameSurface1* in, 
    mfxFrameSurface1* out)
{
    IppStatus ippSts = ippStsNoErr;

    IppiSize srcSize = {in->Info.Width, in->Info.Height};
    IppiRect srcRect = {0, 0, srcSize.width, srcSize.height};
    IppiSize dstSize = {out->Info.Width, out->Info.Height};
    IppiRect dstRect = {0, 0, dstSize.width, dstSize.height};

    Ipp64f xFactor = (Ipp64f)dstSize.width  / (Ipp64f)srcSize.width;
    Ipp64f yFactor = (Ipp64f)dstSize.height / (Ipp64f)srcSize.height;

    Ipp64f    xShift = 0.0, yShift = 0.0;   

    ippSts = ippiResizeSqrPixel_8u_C1R(
        in->Data.Y, 
        srcSize, 
        in->Data.Pitch, 
        srcRect,

        out->Data.Y, 
        out->Data.Pitch,
        dstRect,

        xFactor, 
        yFactor, 
        xShift, 
        yShift, 

        IPPI_INTER_LANCZOS, 
        m_pWorkBuf);

   /* ippSts = ippiResizeYUV420_8u_P2R(
        in->Data.Y, 
        in->Data.Pitch,
        in->Data.UV, 
        in->Data.Pitch,
        srcSize,

        out->Data.Y, 
        out->Data.Pitch,
        out->Data.UV, 
        out->Data.Pitch,
        dstSize,

        IPPI_INTER_LANCZOS, 
        m_pWorkBuf);*/


    VPP_CHECK_IPP_STS(ippSts);

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPImgStab::FrameDownSampling(...)


mfxStatus MFXVideoVPPImgStab::ResizeNV12( 
    mfxFrameSurface1* in, 
    mfxFrameSurface1* out)
{
    IppStatus sts     = ippStsNotSupportedModeErr;
    IppiSize  srcSize = {0,  0};
    IppiSize  dstSize = {0, 0};

    mfxU16 cropX = 0, cropY = 0;

    mfxU32 inOffset0  = 0, inOffset1  = 0;
    mfxU32 outOffset0 = 0, outOffset1 = 0;

    mfxFrameData* inData = &in->Data;
    mfxFrameInfo* inInfo = &in->Info;
    mfxFrameData* outData= &out->Data;
    mfxFrameInfo* outInfo= &out->Info;

    /* [IN] */
    VPP_GET_CROPX(inInfo, cropX);
    VPP_GET_CROPY(inInfo, cropY);
    inOffset0 = cropX + cropY*inData->Pitch;
    inOffset1 = (cropX) + (cropY>>1)*(inData->Pitch);

    /* [OUT] */
    VPP_GET_CROPX(outInfo, cropX);
    VPP_GET_CROPY(outInfo, cropY);
    outOffset0 = cropX + cropY*outData->Pitch;
    outOffset1 = (cropX) + (cropY>>1)*(outData->Pitch);

    const mfxU8* pSrc[2] = {(mfxU8*)inData->Y + inOffset0,
                            (mfxU8*)inData->UV + inOffset1};

    mfxI32 pSrcStep[2] = {inData->Pitch, inData->Pitch};

    mfxU8* pDst[2]   = {(mfxU8*)outData->Y + outOffset0,
                        (mfxU8*)outData->UV + outOffset1};

    mfxI32 pDstStep[2] = {outData->Pitch, outData->Pitch};

    /* common part */
    VPP_GET_WIDTH(inInfo,  srcSize.width);
    VPP_GET_HEIGHT(inInfo, srcSize.height);

    VPP_GET_WIDTH(outInfo,  dstSize.width);
    VPP_GET_HEIGHT(outInfo, dstSize.height);

    // tune part - disabled. reason - BufferSize is critical for
    mfxF64 xFactor, yFactor;
    mfxI32 interpolation;

    xFactor = (mfxF64)dstSize.width  / (mfxF64)srcSize.width;
    yFactor = (mfxF64)dstSize.height / (mfxF64)srcSize.height;

    interpolation = IPPI_INTER_LANCZOS;

    sts = ippiResizeYUV420_8u_P2R(
        pSrc[0], 
        pSrcStep[0],
        pSrc[1], 
        pSrcStep[1],
        srcSize,

        pDst[0], 
        pDstStep[0],
        pDst[1], 
        pDstStep[1],
        dstSize,
        interpolation, 
        m_pWorkBuf);

    if( ippStsNoErr != sts ) return MFX_ERR_UNDEFINED_BEHAVIOR;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPImgStab::ResizeNV12(...)


void MFXVideoVPPImgStab::MotionCompensation(
    int fn,
    mfxFrameSurface1* cur,
    mfxFrameSurface1* out,
    int width, 
    int height, 
    int frame_delay)
{
    width;
    height;
    cur;

    mCMV = GetFilteredMP(fn, frame_delay);

    WarpFrame(cur, out, mCMV, width, height, fn);

} // void CIS::MotionCompensation(...)


MFXVideoVPPImgStab::Global_MV MFXVideoVPPImgStab::GetFilteredMP(int fn, int frm_delay)
{
    Global_MV tmpGMV, filteredGMV;
    int i;
    int n = (int)m_CMVArray.size();
    double sum = 0;

    if(fn == 0)
    {
        filteredGMV.a = 1.0;
        filteredGMV.b = 0.0;
        filteredGMV.c = 0.0;
        filteredGMV.d = 0.0;
    }
    else
    {
        tmpGMV.c = tmpGMV.d = 0.0;

        for(i = fn - MSF_TAP_HALF; i <= fn + frm_delay; i++)
        {
            if(i >=0 && i < n)
            {
                tmpGMV.c += m_CMVArray.at(i).c * m_motionSmoothFilter[i-fn+MSF_TAP_HALF];
                tmpGMV.d += m_CMVArray.at(i).d * m_motionSmoothFilter[i-fn+MSF_TAP_HALF];
                
                sum += m_motionSmoothFilter[i-fn+MSF_TAP_HALF];
            }
        }

        filteredGMV = m_CMVArray.at(fn);
            filteredGMV.a = 1;
            filteredGMV.b = 0;


        filteredGMV.c -= tmpGMV.c/sum; // original
        filteredGMV.d -= tmpGMV.d/sum; // original

        if(m_nCrpX != 0) 
        {
            filteredGMV.c = IPP_MIN(m_nCrpX, IPP_MAX(-(int)m_nCrpX, filteredGMV.c));
            filteredGMV.d = IPP_MIN(m_nCrpY, IPP_MAX(-(int)m_nCrpY, filteredGMV.d));
        }

    }

    return filteredGMV;

} // Global_MV MFXVideoVPPImgStab::GetFilteredMP(int fn, int frm_delay)


void MFXVideoVPPImgStab::WarpFrame(
    mfxFrameSurface1* cur,
    mfxFrameSurface1* out,
    Global_MV mWrpMV,
    int width, 
    int height, 
    int /*fn*/)
{
    mfxU32 i,j;
    double index_x,index_y;
    double a,b,c,d;
    double aa,bb,cc,dd;
    int wid_c = width >> 1;
    int hei_c = height >> 1;
    mfxU32 ii, jj;

    //x' = ax + by + c
    //y' = ay - bx + d
    a = mWrpMV.a;
    b = mWrpMV.b;
    c = mWrpMV.c;
    d = mWrpMV.d;

    //x = aax' + bby' + cc
    //y = aay' - bbx' + dd

    aa = 1.0;
    bb = 0.0;
    cc = -c; // original
    dd = -d; // original


    int offset = 0;
    for(j=m_nCrpY; j<m_destHeight+m_nCrpY; j++)
    {
        for(i=m_nCrpX;i<m_destWidth+m_nCrpX;i++)
        {
            index_x = aa*i - bb*j + cc;
            index_y = bb*i + aa*j + dd;
            ii = i - m_nCrpX;
            jj = j - m_nCrpY;

            if((ii == m_destWidth-1) && (jj == m_destHeight -9))
            {
                bb = 0.0;
            }

            //m_Pout->pY[jj*m_Pout->pitch + ii] = Interpolation(Cur->pY, Cur->pitch, width,height,index_x,index_y,1,0);
            offset = j* out->Data.Pitch + i;
            out->Data.Y[offset] = Interpolation(
                                                        cur->Data.Y, 
                                                        cur->Data.Pitch, 
                                                        width,
                                                        height,
                                                        index_x,
                                                        index_y,
                                                        1,
                                                        0);

            if(jj%2==0&&ii%2==0)
            {
                //int pitchUV = cur->Data.Pitch >> 1;
                //get the U&V component value of pixel (i,j) with bi-linear interpolation 
                //m_Pout->pU[(jj>>1)*(m_Pout->pitch>>1) + (ii>>1)]=Interpolation(Cur->pU, pitchUV, wid_c, hei_c, index_x/2, index_y/2,0,0);
                //m_Pout->pV[(jj>>1)*(m_Pout->pitch>>1) + (ii>>1)]=Interpolation(Cur->pV, pitchUV, wid_c, hei_c, index_x/2, index_y/2,0,0);

                int curPitchUV = cur->Data.Pitch;
                int outPitchUV = out->Data.Pitch;
                offset = (j>>1)*(outPitchUV) + (i>>1)*2;
                out->Data.UV[offset]    = Interpolation(cur->Data.UV,     curPitchUV, wid_c, hei_c, index_x/2, index_y/2,0,0);
                out->Data.UV[offset + 1]= Interpolation(cur->Data.UV + 1, curPitchUV, wid_c, hei_c, index_x/2, index_y/2,0,0);
            }
        }
        aa = 1.0;
    }

    return;

} // void MFXVideoVPPImgStab::WarpFrame(...)


Ipp8u Interpolation(
    Ipp8u *p_buf, 
    int pitch, 
    int width,
    int height,
    double x,
    double y, 
    int luma, 
    int mode)
{
    int i1 = 0, i2 = 0, j1 = 0, j2 = 0; //position of four near pix
    int xx = 0, yy = 0;
    int f1 = 0, f2 = 0, f3 = 0, f4 = 0; //value of four near pix
    double f12 = 0.0, f34 = 0.0;//two interpolated pix value
    int blackval = (luma == 1) ? 16 : 128;
    int mul = (luma == 1) ? 1 : 2;
    int val;

    //get the integer position
    i1=(int)floor(x);
    i2=i1+1;
    j1=(int)floor(y);
    j2=j1+1;

    if((x+0.5) > (double)i2)
        xx = i2;
    else
        xx = i1;
    if((y+0.5) > (double)j2)
        yy = j2;
    else
        yy = j1;

    
    if(i2<0||i1>width-1||j2<0||j1>height-1)
    {//if out of the picture
        f1=f2=f3=f4=blackval;
    }
    else 
    {
        //next deal with some abnomal conditions
        //i.e. some pixels used for interpolation are out of frame
        if(i2==0)
        {
            f1=f3=blackval;
            if((j2==0)||(j1==height-1))
            {
                if(j2 == 0)
                {
                    f2 = blackval;
                    f4 = p_buf[j2*pitch + 0];//[j2][0];
                }
                if(j1==height-1)
                {
                    f2 = p_buf[j1*pitch + 0];//[j1][0];
                    f4 = blackval;
                }
            }
            else
            {
                f2=p_buf[j1*pitch + 0];
                f4=p_buf[j2*pitch + 0];
            }
        }
        else if(i1==width-1)
        {
            f2=f4=blackval;
            if((j2==0)||(j1==height-1))
            {
                if(j2 == 0)
                {
                    f1=blackval;
                    f3=p_buf[j2*pitch + i1*mul];
                }
                if(j1==height-1)
                {
                    f3=blackval;
                    f1=p_buf[j1*pitch + i1*mul];
                }
            }
            else
            {
                f1=p_buf[j1*pitch + i1*mul];
                f3=p_buf[j2*pitch + i1*mul];
            }
        }
        else
        {
            if(j2==0)
            {
                f1=f2=blackval;
                f3=p_buf[j2*pitch + i1*mul];
                f4=p_buf[j2*pitch + i2*mul];
            }
            else if(j1==height-1)
            {
                f3=f4=blackval;
                f1=p_buf[j1*pitch + i1*mul];
                f2=p_buf[j1*pitch + i2*mul];
            }
            else
            {
                f1=p_buf[j1*pitch + i1*mul];
                f3=p_buf[j2*pitch + i1*mul];
                f2=p_buf[j1*pitch + i2*mul];
                f4=p_buf[j2*pitch + i2*mul];
            }
        }
    }

    if(mode)
    {
        //perform interpolation
        f12=(f1+(x-i1)*(f2-f1));
        f34=(f3+(x-i1)*(f4-f3));
        val = (int)(f12+(y-j1)*(f34-f12)+0.5);
        val = IPP_MIN(255, IPP_MAX(0, val));
    }
    else
    {
        if((xx == i1) && (yy == j1))
            val = f1;
        else if((xx == i2) && (yy == j2))
            val = f4;
        else if((xx == i2) && (yy == j1))
            val = f2;
        else
            val = f3;
    }


    Ipp8u ret8u = (Ipp8u)IPP_MIN(val, 255);

    return ret8u;

} // Ipp8u Interpolation(...)


mfxStatus MFXVideoVPPImgStab::GetBufferSizeEx( SizeInfo & sizeInf )
{
    IppiRect  srcRect, dstRect;
    IppStatus ippSts = ippStsNoErr;

    VPP_CHECK_NOT_INITIALIZED;

    // [0] memory for resize work buffer (downScale mode)
    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.width = m_width;
    srcRect.height= m_height;

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.width = m_widthScale;//m_destWidth;
    dstRect.height= m_heightScale;//m_destHeight;

    mfxI32 bufSizeLanczos = 0;
    ippSts = ippiResizeGetBufSize(srcRect, dstRect, 1, IPPI_INTER_LANCZOS, &bufSizeLanczos);
    VPP_CHECK_IPP_STS( ippSts );

    sizeInf.m_workBufSize = bufSizeLanczos;

    // [1] memory for ref/cur surfaces (Y plane only)
    sizeInf.m_curSizeY = m_cur.Data.Pitch * m_cur.Info.Height;
    sizeInf.m_refSizeY = m_ref.Data.Pitch * m_ref.Info.Height;

    // [2] memory for upscale mode
    //if( MFX_IMAGESTAB_MODE_UPSCALE == m_mode )
    {
        sizeInf.m_preOutSizeY  = m_preOut.Data.Pitch * m_preOut.Info.Height;
        sizeInf.m_preOutSizeUV = (m_preOut.Data.Pitch * m_preOut.Info.Height) >> 1;

        IppiSize dstSize = {static_cast<int>(m_width), static_cast<int>(m_height)};// aya: it is correct
        IppiSize srcSize = {static_cast<int>(m_destWidth), static_cast<int>(m_destHeight)};

        ippSts = ippiResizeYUV420GetBufSize(
            srcSize, 
            dstSize, 
            IPPI_INTER_LANCZOS, 
            &bufSizeLanczos);
        VPP_CHECK_IPP_STS( ippSts );

        sizeInf.m_workBufSize = IPP_MAX(sizeInf.m_workBufSize, (mfxU32)bufSizeLanczos);
    }

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPImgStab::GetBufferSizeEx( SizeInfo & sizeInf )


void MFXVideoVPPImgStab::BlockMotionSearch( void )
{
    int k, n;
    int cost = 0, mcost = 0;
    Point tmpbk;
    Point tmpMV;
    Point pred_mv;

    tmpMV.x = 0; tmpMV.y = 0;

    SetUsefulBlock();

    n = (int)mBlock.size();

    for(k=0; k < n; )
    {
        mcost = (1<<20);
        tmpbk = mBlock[k];  //search center and cost center

        pred_mv = GetPredictMV();  //Always (0,0)
        
        VME_IS(
            m_cur.Data.Y, 
            m_ref.Data.Y, 
            m_cur.Info.Width, 
            m_cur.Info.Height, 
            tmpbk, 
            &tmpMV, 
            &cost);//, right
        
        if(cost < (int)m_config.m_MinCostThreshold)
        {
            mMV.push_back(tmpMV);
            k++;
        }
        else
        {
            mBlock.erase(mBlock.begin()+k);
            //mBlock.RemoveAt(k);
            n--;
        }
    }

    return;

} // void MFXVideoVPPImgStab::BlockMotionSearch( void )


void MFXVideoVPPImgStab::SetUsefulBlock(void)
{
    mfxI32 y,x;
    mfxI32 edge = 8;

    mfxI32 blkSize = (mfxI32)m_config.m_BlockSize;
    mfxI32 yEnd = (mfxI32)m_heightScale - blkSize - edge;
    mfxI32 xEnd = (mfxI32)m_widthScale  - blkSize - edge;

    for (y = edge; y <= yEnd; y+=blkSize)
    {
        for (x = edge; x <= xEnd; x+=blkSize)
        {
            mblk.x = x;
            mblk.y = y;
            mBlock.push_back(mblk);
        }
    }

} // void MFXVideoVPPImgStab::SetUsefulBlock(void)


MFXVideoVPPImgStab::Point MFXVideoVPPImgStab::GetPredictMV()
{
    Point predictMV;
    predictMV.x = predictMV.y = 0;

    //we don't predict currently, just use (0,0) as search center

    return predictMV;

} // MFXVideoVPPImgStab::Point MFXVideoVPPImgStab::GetPredictMV()


void MFXVideoVPPImgStab::GlobalMPGeneration( void )
{
    int nCircle;
    int i, j;
    //int *mvsortx = NULL,*mvsorty = NULL;
    std::vector<int> mvsortx;
    std::vector<int> mvsorty;
    int histox[49];
    int histoy[41];
    int histocount = 0;//counts of processed motion vectors

    //initialize hsitogram vectors
    for (i = 0;i < 49;i ++)
    {
        histox[i] = 0;
    }

    for (i = 0;i <41;i ++)
    {
        histoy[i] = 0;
    }

    mGMV.a = 1.0; mGMV.b = 0.0; 

    //get translation motion
    nCircle = (int)mBlock.size();
    //mvsortx = (int*)calloc(nCircle, sizeof(int));
    mvsortx.resize(nCircle);
    //mvsorty = (int*)calloc(nCircle, sizeof(int));
    mvsorty.resize(nCircle);

    for(i = 0; i < nCircle; i++)
    {
        mvsortx[i] = mMV[i].x;
        mvsorty[i] = mMV[i].y;
    }

    //Histogram-like median value computing by taking advantage of the limited range of motion vectors  

    //Build histogram
    for(i = 0; i < nCircle; i ++)
    {
        histox[mvsortx[i]+24]++;
        histoy[mvsorty[i]+20]++;
    }

    //Find out median value from histogram
    if (nCircle == 0)
    {
        mGMV.c = 0;
        mGMV.d = 0;
    }
    else if(nCircle % 2 == 0)
        //if(nCircle % 2 == 0)
    {    
        // If the number of blocks is even, two cases can happen:
        // 1) the median location falls in between two columns of histogram and
        // 2) the median location falls in the middle of a column


        i = 0; //pointer to the second column
        j = 0; //pointer to the frist column
        while (histocount <  nCircle / 2 + 1)
        {

            histocount += histox[i];      
            //i++;
            if ((histox[i] != 0)&&(histocount == nCircle / 2)) 
                //if ((histox[i] != 0)&&(histocount >= nCircle / 2))
            {
                j = i;

            }
            i++;
        }
        if (j != 0)
        {
            //first case
            mGMV.c = (double)(i + j - 2 - 48 + 1) / 2;
        }
        else
        {
            //second case
            mGMV.c = (double)(i + i - 2 - 48) / 2;
        }

        //The same situation for histogram Y
        i = 0;//pointer to the second column
        j = 0;//pointer to the frist column
        histocount = 0;//counts of processed motion vectors
        while (histocount <  nCircle / 2 + 1)
        {

            histocount += histoy[i];      
            //i++;
            if ((histoy[i] != 0)&&(histocount == nCircle / 2)) 
                //if ((histoy[i] != 0)&&(histocount >= nCircle / 2))
            {
                j = i;

            }
            i++;
        }

        if (j != 0)
        {
            mGMV.d = (double)(i + j - 2 - 40 + 1) / 2;
        }
        else
        {
            mGMV.d = (double)(i + i - 2 - 40) / 2;
        }

    }
    else
    {
        // Median value of horizontal motion
        i = 0;
        while (histocount <  nCircle / 2)
        {
            histocount += histox[i];      
            i++;
        }
        mGMV.c = i - 1 - 24;
        // Median value of vertical motion
        histocount = 0;
        i = 0;
        while (histocount <  nCircle / 2)
        {
            histocount += histoy[i];      
            i++;
        }

        mGMV.d = i - 1 -20;
    } 

    mGMV.c *= m_scale_h;
    mGMV.d *= m_scale_v;

    m_GMVArray.push_back(mGMV);

    return;

} // void MFXVideoVPPImgStab::GlobalMPGeneration( void )


void MFXVideoVPPImgStab::AccumulatedGlobalMP( void )
{
    Global_MV mTmpCMV;
    mTmpCMV = m_CMVArray.at( m_CMVArray.size()-1 );

    mCMV.a = 1.0;
    mCMV.b = 0.0;
    mCMV.c = mGMV.c + mTmpCMV.c;
    mCMV.d = mGMV.d + mTmpCMV.d;

    m_CMVArray.push_back(mCMV);

    return;

} // void MFXVideoVPPImgStab::AccumulatedGlobalMP( void )


void MFXVideoVPPImgStab::PostProcess(mfxFrameSurface1* out)
{
    mfxFrameSurface1 processedSurf;
    processedSurf.Data = out->Data;
    processedSurf.Info = out->Info;
    processedSurf.Info.CropX = m_nCrpX;
    processedSurf.Info.CropY = m_nCrpY;
    processedSurf.Info.CropW = m_destWidth;
    processedSurf.Info.CropH = m_destHeight;
            
    mfxStatus sts = SetBackGroundColor( &processedSurf );

    sts;// reserved for the future

} // void MFXVideoVPPImgStab::PostProcess(mfxFrameSurface1* out)


mfxStatus MFXVideoVPPImgStab::DoUpscale( 
    mfxFrameSurface1* in, 
    mfxFrameSurface1* out )
{
    mfxFrameSurface1 src;
    src.Data = in->Data;
    src.Info = in->Info;
    src.Info.CropX = m_nCrpX;
    src.Info.CropY = m_nCrpY;
    src.Info.CropW = m_destWidth;
    src.Info.CropH = m_destHeight;

    mfxFrameSurface1 dst;
    dst.Data = out->Data;
    dst.Info = out->Info;
    dst.Info.CropX = 0;
    dst.Info.CropY = 0;
    dst.Info.CropW = dst.Info.Width;
    dst.Info.CropH = dst.Info.Height;

    return ResizeNV12(&src, &dst);

    //return SurfaceCopy_ROI(&dst, &src, false);

} // mfxStatus MFXVideoVPPImgStab::DoUpscale(...)

/* ******************************************************************** */
/*                 VME PART                                             */
/* ******************************************************************** */
//static mfxU8 FullSP[64] = {
//    0x0F,0xF0,0x01,0x01,0x10,0x10,0x0F,0x0F, 0x0F,0xF0,0xF0,0xF0,0x01,0x01,0x01,0x01,
//    0x10,0x10,0x10,0x10,0x0F,0x0F,0x0F,0x0F, 0x0F,0x0F,0xF1,0x0F,0xF1,0x0F,0xF1,0x0F,
//    0xF1,0x0F,0xF0,0x01,0x01,0x01,0x01,0x01, 0x01,0x01,0x10,0x10,0x10,0x10,0x10,0   ,
//    0   ,0   ,0   ,0   ,0   ,0   ,0   ,0   , 0   ,0   ,0   ,0   ,0   ,0   ,0   ,0   
//};

//            37 1E 1F 20 21 22 23 24
//            36 1D 0C 0D 0E 0F 10 25
//            35 1C 0B 02 03 04 11 26
//            34 1B 0A 01 00 05 12 27
//            33 1A 09 08 07 06 13 28
//            32 19 18 17 16 15 14 29
//            31 30 2F 2E 2D 2C 2B 2A

//            24 25 26 27 28 29 2A 2B
//            23 22 0C 0D 0E 0F 10 2C
//            21 20 0B 02 03 04 11 2D 
//            1F 1D 0A 01 00 05 12 2F
//            1C 1B 09 08 07 06 13 30
//            1A 19 18 17 16 15 14 31
static mfxU8 FullSP[64] = {
    0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x19, 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x19,
    0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x19, 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x19,
    0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x19, 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0,
    0   ,0   ,0   ,0   ,0   ,0   ,0   ,0   , 0   ,0   ,0   ,0   ,0   ,0   ,0   ,0   
};

//            00 01 02 03 04 05 06 07
//            08 09 0A 0B 0C 0D 0E 0F
//            10 11 12 13 14 15 16 17 
//            18 19 1A 1B 1C 1D 1E 1F 
//          20 21 22 23 24 25 26 27
//            28 29 2A 2B 2C 2D 2E 2F

mfxU8  DummyLutMode[4][10] = { {0,0,0,0,0,0,0,0,0,0}, {16,16,16,16,20,16,24,32,16,20},{32,32,32,32,40,32,48,64,32,40}, {64,64,64,64,80,64,80,96,64,72} }; 
mfxU8  DummyLutXY[4][8] = { {0,0,0,0,0,0,0,0}, {0,0,0,0,0x55,0x66,0x77,0x88}, {20,32,40,48,56,64,72,80}, {0x04,0x35,0x38,0x46,0x49,0x57,0x59,0x66}};


void MFXVideoVPPImgStab::VME_IS(
    mfxU8 *src, 
    mfxU8 *ref, 
    int width, 
    int height, 
    Point tmpbk, 
    Point *tmpMV, 
    int *distortion)//, Point  pred_mv
{
    mfxI16Pair srcxy, refxy;

    /*VMEOutput    vout;
    VMEInput    vin;*/
    /*mfxVME7Input VmeI7;
    mfxVME7Output VmeO7;*/
    mfxVME7_5UNIIn VmeIn;
    mfxVME7_5IMEIn VmeInIME;
    mfxVME7_5FBRIn VmeInFBR; 
    mfxVME7_5UNIOutput VmeOut;
    mfxVME7_5IMEOutput VmeOutIME;

    memset(&VmeIn, 0, sizeof(VmeIn));
    memset(&VmeInIME, 0, sizeof(VmeInIME));
    memset(&VmeInFBR, 0, sizeof(VmeInFBR));
    memset(&VmeOut, 0, sizeof(VmeOut));
    memset(&VmeOutIME, 0, sizeof(VmeOutIME));

    // Set image size
    VmeIn.SrcLuma = src;
    VmeIn.SrcPitch = width;
    //m_vme.SetSourceFrame(pb, pw);
    // set reference
    VmeIn.RefLuma[0][0] = ref;
    VmeIn.RefLuma[1][0] = ref;
    VmeIn.RefPitch = VmeIn.RefWidth = (mfxI16)width;
    VmeIn.RefHeight = (mfxI16)height;
    //m_vme.SetReferenceFrame(ref, 0, pw, ph);

    /*VmeI7.DoIntraInter = 1;*/ //default = 3, inter = 1, intra = 2
    //m_vme.SetIntraInterMasterSwitch( 1, 0 );
    
    //m_vme.SetHardwareConstraints( 0 );  //This is a SW C-model only function that can be called any time

    
    // Choose partition mode  
    int splitFlag; //= (mbSize == 3) 
    //    ? (SHP_NO_8X4 | SHP_NO_4X8 | SHP_NO_8X16 | SHP_NO_16X8 | SHP_NO_16X16 | (split==4)*SHP_NO_4X4)
    //    : (SHP_NO_8X4 | SHP_NO_4X8 |  SHP_NO_4X4 );//|  SHP_NO_16X16  | SHP_NO_8X16| SHP_NO_16X8 

    splitFlag = SHP_NO_8X4 | SHP_NO_4X8 | SHP_NO_4X4 | SHP_NO_16X8 | SHP_NO_8X16 | SHP_NO_8X8;//  | SHP_NO_16X16
    
    //Status SetSourceMB( I16PAIR SrcXY, I32 SrcType, I32 IsField );
    const int xOffset = 16;
    const int yOffset = 12;
    const int wndSizeX = 48;  //(wndSizeX-16)/2 = xOffset
    const int wndSizeY = 40;  //(wndSizeY-16)/2 = yOffset
    
    srcxy.x = (short)tmpbk.x;
    srcxy.y = (short)tmpbk.y;


    VmeIn.SrcMB = srcxy;
    
    VmeIn.SrcType = ST_SRC_16X16;   
    //m_vme.SetSourceMB( srcxy, mbSize, 0);

    refxy.x = -xOffset;
    refxy.y = -yOffset;

    VmeIn.Ref[0] = refxy;
    VmeIn.Ref[1] = refxy;
    VmeIn.RefW = wndSizeX;
    VmeIn.RefH = wndSizeY;
    //m_vme.SetReferenceWindows( 1, refxy, refxy, wndSizeX, wndSizeY, 0);
    

    VmeIn.MaxLenSP = 48;//pathLength;  (wndSizeX-16)*(wndSizeY-16)
    VmeIn.MaxNumSU = 48;//pathLength;
    //m_vme.SetSearchPaths( 1, 0, 0, 0, 0, pathLength, pathLength);

    VmeIn.SPCenter[0] = 0x00;// 0x34;
    VmeIn.SPCenter[1] = 0x00;//0x34;
    //m_vme.SetSearchCenters( 1, cctr[0], cctr[1]);

    VmeIn.SadType = 0;
    //m_vme.SetDistortionMethods( 0, 0 );


    //m_vme.SetSkipMVs( 0, NULL );
    
    VmeIn.CostCenter[0].x = -(refxy.x<<2);
    VmeIn.CostCenter[0].y = -(refxy.y<<2);
    VmeIn.CostCenter[1].x = -(refxy.x<<2);
    VmeIn.CostCenter[1].y = -(refxy.y<<2);
    //m_vme.SetCostingCenters( cctr[0], cctr[1] );
    //m_vme.SetEarlyDecisions( 0,0,0,0,0 );
    VmeIn.BidirMask = 0x0F;
    VmeIn.BiWeight = 0; //?
    //m_vme.SetBidirectionalSearchs( 0x0F, 0, 0 );
    
    VmeIn.VmeModes = VM_INTEGER_PEL;  //Cover MeType
/*    VmeI7.SWflags = 0;  */                  //Cover MeType    
    VmeIn.ShapeMask = (mfxU8)splitFlag;
    VmeIn.MaxMvs = 32;
    //m_vme.SetMotionSearchs( 0x30*qpelPrecision /* qpel precision */,
    //    splitFlag ,
    //    0, 32);
    VmeIn.IntraFlags = 0;
    VmeIn.IntraAvail = 0;
    //m_vme.SetIntraPredictions( 0, 0, 0 );

    //m_vme.SetTransform8x8Support( 0 );

    VmeIn.VmeFlags = 0;
    /*VmeI7.EarlySkipSuc = 0;
    VmeI7.EarlyFmeExit = 0;*/ 
    VmeIn.EarlyImeStop = 0;
    /*VmeI7.BPrunTolerance = 0;
    VmeI7.ExtraTolerance = 0;*/

    /*for(int i=0;i<4;i++)
    for(int j=0;j<2;j++)
    {
        VmeI7.SkipCenter[i][j].x = 0;
        VmeI7.SkipCenter[i][j].y = 0;
    }*/
    VmeIn.SkipCenterEnables = 0xFF;


    // First time - set LUT tables
    /*if(!m_vmeSetup)
    {
        m_vmeSetup = 1;
        for(int k=0; k<8; k++)
            m_vme.LoadOneSearchPathFourLutSets(k, &FullSP[0],&DummyLutMode[0][0], &DummyLutXY[0][0]);
    }*/

    memcpy_s(VmeIn.MvCost, sizeof(VmeIn.MvCost), DummyLutXY[0], 8);                    
    memcpy_s(VmeIn.ModeCost, sizeof(VmeIn.ModeCost), DummyLutMode[0], 10);

    memcpy_s(VmeInIME.IMESearchPath0to31, sizeof(VmeInIME.IMESearchPath0to31), FullSP, 32);
    memcpy_s(VmeInIME.IMESearchPath32to55, sizeof(VmeInIME.IMESearchPath32to55), &FullSP[32], 24);
    
    /*VmeI7.Intra4x4ModeMask = 0xFFFF;
    VmeI7.Intra8x8ModeMask = 0xFFFF;
    VmeI7.Intra16x16ModeMask = 0xFF;*/
    VmeIn.MvFlags =INTER_BILINEAR;

    VmeInIME.Multicall = 0x00;  //For multiple output
    VmeIn.FTXCoeffThresh_DC = 0;    
    for(int i=0;i<6;i++)
    {
        VmeIn.FTXCoeffThresh[i] = 0;
    }
    VmeIn.BlockRefId[0] = 0;
    VmeIn.BlockRefId[1] = 0;
    VmeIn.BlockRefId[2] = 0;
    VmeIn.BlockRefId[3] = 0;

    memset(&VmeInIME.RecordDst[0][0],0x7F,16*sizeof(mfxU16));
    memset(&VmeInIME.RecordDst16[0],0x7F,2*sizeof(mfxU16));

    
    // Run VME 
    Status status = m_vme->RunIME(&VmeIn, &VmeInIME, &VmeOut, &VmeOutIME);
    status;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    assert(status == NO_ERR);
    const int mvsIndex = 0;
    int nPartitions = 0; 
    
    // Check mode
    switch(VmeOut.MbMode)
    {
        case 0: 
            nPartitions = 4;
            
            //for (int i = 0; i < nPartitions; i++) 
            {
                tmpMV->x = (VmeOut.Mvs[0][mvsIndex].x)/4;
                tmpMV->y = (VmeOut.Mvs[0][mvsIndex].y)/4;
                *distortion = (int)VmeOut.Distortion[0];
            }
        break;
    
        default:
            assert(0);
            //printf("\n[unexpected mode]");
        return;
    }

} // void MFXVideoVPPImgStab::VME_IS(...)

#endif // #if !defined (MFX_ENABLE_HW_ONLY_VPP)
#endif // MFX_ENABLE_VPP
/* EOF */
