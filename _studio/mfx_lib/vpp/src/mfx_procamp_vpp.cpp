/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2010-2016 Intel Corporation. All Rights Reserved.
//
//
//          PROCessing AMPlified algorithms of Video Pre\Post Processing
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#include <math.h>

#include "mfx_vpp_utils.h"
#include "mfx_procamp_vpp.h"

/* Procamp Filter Parameter Settings
NAME          MIN         MAX         Step        Default
Brightness     -100.0      100.0       0.1         0.0
Contrast          0.0       10.0       0.01        1.0
Hue            -180.0      180.0       0.1         0.0
Saturation         0.0      10.0       0.01        1.0
*/
#define VPP_PROCAMP_BRIGHTNESS_MAX      100.0
#define VPP_PROCAMP_BRIGHTNESS_MIN     -100.0
#define VPP_PROCAMP_BRIGHTNESS_DEFAULT    0.0

#define VPP_PROCAMP_CONTRAST_MAX     10.0
#define VPP_PROCAMP_CONTRAST_MIN      0.0
#define VPP_PROCAMP_CONTRAST_DEFAULT  1.0

#define VPP_PROCAMP_HUE_MAX          180.0
#define VPP_PROCAMP_HUE_MIN         -180.0
#define VPP_PROCAMP_HUE_DEFAULT        0.0

#define VPP_PROCAMP_SATURATION_MAX     10.0
#define VPP_PROCAMP_SATURATION_MIN      0.0
#define VPP_PROCAMP_SATURATION_DEFAULT  1.0

/* ******************************************************************** */
/*           implementation of VPP filter [ProcessingAmplified]         */
/* ******************************************************************** */

mfxStatus MFXVideoVPPProcAmp::Query( mfxExtBuffer* pHint )
{
    if( NULL == pHint )
    {
        return MFX_ERR_NONE;
    }

    mfxStatus sts = MFX_ERR_NONE;

    mfxExtVPPProcAmp* pParam = (mfxExtVPPProcAmp*)pHint;

    /* Brightness */
    if( pParam->Brightness < VPP_PROCAMP_BRIGHTNESS_MIN ||
        pParam->Brightness > VPP_PROCAMP_BRIGHTNESS_MAX )
    {
        VPP_RANGE_CLIP(pParam->Brightness, VPP_PROCAMP_BRIGHTNESS_MIN, VPP_PROCAMP_BRIGHTNESS_MAX);

        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }
    /* Contrast */
    if( pParam->Contrast < VPP_PROCAMP_CONTRAST_MIN || pParam->Contrast > VPP_PROCAMP_CONTRAST_MAX )
    {
        VPP_RANGE_CLIP(pParam->Contrast, VPP_PROCAMP_CONTRAST_MIN, VPP_PROCAMP_CONTRAST_MAX);

        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }
    /* Hue */
    if( pParam->Hue < VPP_PROCAMP_HUE_MIN || pParam->Hue > VPP_PROCAMP_HUE_MAX )
    {
        VPP_RANGE_CLIP(pParam->Hue, VPP_PROCAMP_HUE_MIN, VPP_PROCAMP_HUE_MAX);

        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }
    /* Saturation */
    if( pParam->Saturation < VPP_PROCAMP_SATURATION_MIN ||
        pParam->Saturation > VPP_PROCAMP_SATURATION_MAX)
    {
        VPP_RANGE_CLIP(pParam->Saturation, VPP_PROCAMP_SATURATION_MIN, VPP_PROCAMP_SATURATION_MAX);

        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return sts;

} // static mfxStatus MFXVideoVPPProcAmp::Query( mfxExtBuffer* pHint )

#if !defined(MFX_ENABLE_HW_ONLY_VPP)

#define PI  3.14159265358979323846f

#define VPP_PROCAMP_CONTRAST_PRECISION          7
#define VPP_PROCAMP_SINCOS_PRECISION            8
#define VPP_PROCAMP_Y_MIN                       (16 << 8)
#define VPP_PROCAMP_UV_MID                      (128 << 8)

#define VPP_MAX( a, b ) ( ((a) > (b)) ? (a) : (b) )
#define VPP_MIN( a, b ) ( ((a) < (b)) ? (a) : (b) )

/* ******************************************************************** */
/*                 prototype of ProcAmp algorithms                      */
/* ******************************************************************** */

mfxStatus ProcampFiltering_NV12_8u(
                               Ipp8u *pSrcY,
                               Ipp8u *pSrcUV,
                               Ipp8u *pDstY,
                               Ipp8u *pDstUV,

                               int width,
                               int height,

                               int stepSrcY,
                               int stepSrcUV,
                               int stepDstY,
                               int stepDstUV,

                               int  brightness,
                               int  contrast,
                               int  cosCS,
                               int  sinCS,

                               int  par_y_min,
                               int  par_uv_mid,
                               int  par_contrast_precision,
                               int  par_sincos_precision);

mfxStatus ProcampFiltering_YV12_8u(
                               Ipp8u *pSrcY,
                               Ipp8u *pSrcV,
                               Ipp8u *pSrcU,

                               Ipp8u *pDstY,
                               Ipp8u *pDstV,
                               Ipp8u *pDstU,

                               int width,
                               int height,

                               int stepSrcY,
                               int stepSrcUV,

                               int stepDstY,
                               int stepDstUV,

                               int  brightness,
                               int  contrast,
                               int  cosCS,
                               int  sinCS,

                               int  par_y_min,
                               int  par_uv_mid,
                               int  par_contrast_precision,
                               int  par_sincos_precision);

MFXVideoVPPProcAmp::MFXVideoVPPProcAmp(VideoCORE *core, mfxStatus* sts) : FilterVPP()
{
    if( core )
    {
        // something
    }

    VPP_CLEAN;

    m_brightness = VPP_PROCAMP_BRIGHTNESS_DEFAULT;
    m_contrast   = VPP_PROCAMP_CONTRAST_DEFAULT;
    m_hue        = VPP_PROCAMP_HUE_DEFAULT;
    m_saturation = VPP_PROCAMP_SATURATION_DEFAULT;

    *sts = MFX_ERR_NONE;

    m_isFilterActive = true;
    m_internalContrast = 0;
    m_internalBrightness = 0;
    m_internalCosCS = 0;
    m_internalSinCS = 0;

    return;

} // MFXVideoVPPProcAmp::MFXVideoVPPProcAmp(VideoCORE *core, mfxStatus* sts) : FilterVPP()

MFXVideoVPPProcAmp::~MFXVideoVPPProcAmp(void)
{
    Close();

    return;

} // MFXVideoVPPProcAmp::~MFXVideoVPPProcAmp(void)

mfxStatus MFXVideoVPPProcAmp::SetParam( mfxExtBuffer* pHint )
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    const mfxF64 EPS_PROCAMP_DIFF = 1E-6;

    if( pHint )
    {
        mfxExtVPPProcAmp* pProcAmpParams = (mfxExtVPPProcAmp*)pHint;

        m_brightness = pProcAmpParams->Brightness;
        m_contrast   = pProcAmpParams->Contrast;
        m_hue        = pProcAmpParams->Hue;
        m_saturation = pProcAmpParams->Saturation;

        VPP_RANGE_CLIP(m_brightness, VPP_PROCAMP_BRIGHTNESS_MIN, VPP_PROCAMP_BRIGHTNESS_MAX);
        VPP_RANGE_CLIP(m_contrast, VPP_PROCAMP_CONTRAST_MIN, VPP_PROCAMP_CONTRAST_MAX);
        VPP_RANGE_CLIP(m_hue, VPP_PROCAMP_HUE_MIN, VPP_PROCAMP_HUE_MAX);
        VPP_RANGE_CLIP(m_saturation, VPP_PROCAMP_SATURATION_MIN, VPP_PROCAMP_SATURATION_MAX);
    }
    else
    {
        m_brightness = VPP_PROCAMP_BRIGHTNESS_DEFAULT;
        m_contrast   = VPP_PROCAMP_CONTRAST_DEFAULT;
        m_hue        = VPP_PROCAMP_HUE_DEFAULT;
        m_saturation = VPP_PROCAMP_SATURATION_DEFAULT;
    }

    m_isFilterActive = true;
    if( fabs(m_brightness -  VPP_PROCAMP_BRIGHTNESS_DEFAULT) < EPS_PROCAMP_DIFF &&
        fabs(m_contrast   -  VPP_PROCAMP_CONTRAST_DEFAULT)   < EPS_PROCAMP_DIFF &&
        fabs(m_hue        -  VPP_PROCAMP_HUE_DEFAULT)        < EPS_PROCAMP_DIFF &&
        fabs(m_saturation -  VPP_PROCAMP_SATURATION_DEFAULT) < EPS_PROCAMP_DIFF )
    {
        m_isFilterActive = false; // // exclude from pipeline because algorithm doesn't change video data
    }

    CalculateInternalParams();

    return mfxSts;

} // mfxStatus MFXVideoVPPProcAmp::SetParam( mfxExtBuffer* pHint )

mfxStatus MFXVideoVPPProcAmp::Reset(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1( par );

    VPP_CHECK_NOT_INITIALIZED;

    VPP_RESET;

    mfxExtBuffer* pHint = NULL;

    GetFilterParam(par, MFX_EXTBUFF_VPP_PROCAMP, &pHint);

    SetParam( pHint );

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPProcAmp::Reset(mfxVideoParam *par)

mfxStatus MFXVideoVPPProcAmp::RunFrameVPP(mfxFrameSurface1 *in,
                                          mfxFrameSurface1 *out,
                                          InternalParam* pParam)
{
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

    if( !m_isFilterActive )
    {
        mfxSts = SurfaceCopy_ROI(out, in);

        pParam->outPicStruct = pParam->inPicStruct;
        pParam->outTimeStamp = pParam->inTimeStamp;

        return mfxSts;
    }

    // core processing

    Ipp8u *pSrcY, *pSrcUV, *pSrcU, *pSrcV;
    Ipp8u *pDstY, *pDstUV, *pDstU, *pDstV;

    int stepSrcY, stepSrcUV, stepDstY, stepDstUV;
    int width, height;

    mfxFrameInfo* info = &(in->Info);
    VPP_GET_WIDTH( info, width);
    VPP_GET_HEIGHT(info, height);

    if( MFX_FOURCC_NV12 == in->Info.FourCC )
    {
        // (2) ROI
        pSrcY  = in->Data.Y + (in->Info.CropX + in->Info.CropY * in->Data.Pitch);
        pSrcUV = in->Data.UV + (in->Info.CropX + (in->Info.CropY >> 1) *  in->Data.Pitch);

        pDstY  = out->Data.Y  + (out->Info.CropX + out->Info.CropY * out->Data.Pitch);
        pDstUV = out->Data.UV + (out->Info.CropX + (out->Info.CropY >> 1) *  out->Data.Pitch);

        stepSrcY  = in->Data.Pitch;
        stepSrcUV = in->Data.Pitch;
        stepDstY  = out->Data.Pitch;
        stepDstUV = out->Data.Pitch;

        // (4) ProCamp
        mfxSts = ProcampFiltering_NV12_8u(
                                    pSrcY,
                                    pSrcUV,
                                    pDstY,
                                    pDstUV,

                                    width,
                                    height,

                                    stepSrcY,
                                    stepSrcUV,
                                    stepDstY,
                                    stepDstUV,

                                    m_internalBrightness,
                                    m_internalContrast,
                                    m_internalCosCS,
                                    m_internalSinCS,

                                    VPP_PROCAMP_Y_MIN,
                                    VPP_PROCAMP_UV_MID,
                                    VPP_PROCAMP_CONTRAST_PRECISION,
                                    VPP_PROCAMP_SINCOS_PRECISION);
    }
    else
    {
        // (2) ROI
        pSrcY  = in->Data.Y  + (in->Info.CropX   + in->Info.CropY * in->Data.Pitch);
        pSrcV  = in->Data.V  + (in->Info.CropX/2 + (in->Info.CropY >> 1) *  in->Data.Pitch/2);
        pSrcU  = in->Data.U  + (in->Info.CropX/2 + (in->Info.CropY >> 1) *  in->Data.Pitch/2);

        pDstY = out->Data.Y  + (out->Info.CropX   + out->Info.CropY * out->Data.Pitch);
        pDstV = out->Data.V  + (out->Info.CropX/2 + (out->Info.CropY >> 1) *  out->Data.Pitch/2);
        pDstU = out->Data.U  + (out->Info.CropX/2 + (out->Info.CropY >> 1) *  out->Data.Pitch/2);

        stepSrcY  = in->Data.Pitch;
        stepSrcUV = in->Data.Pitch >> 1;

        stepDstY  = out->Data.Pitch;
        stepDstUV = out->Data.Pitch >> 1;

        // (4) ProCamp
        mfxSts = ProcampFiltering_YV12_8u(
                                    pSrcY,
                                    pSrcV,
                                    pSrcU,

                                    pDstY,
                                    pDstV,
                                    pDstU,

                                    width,
                                    height,

                                    stepSrcY,
                                    stepSrcUV,

                                    stepDstY,
                                    stepDstUV,

                                    m_internalBrightness,
                                    m_internalContrast,
                                    m_internalCosCS,
                                    m_internalSinCS,

                                    VPP_PROCAMP_Y_MIN,
                                    VPP_PROCAMP_UV_MID,
                                    VPP_PROCAMP_CONTRAST_PRECISION,
                                    VPP_PROCAMP_SINCOS_PRECISION);
    }

    pParam->outPicStruct = pParam->inPicStruct;
    pParam->outTimeStamp = pParam->inTimeStamp;

    return mfxSts;

} // mfxStatus MFXVideoVPPProcAmp::RunFrameVPP(mfxFrameSurface1 *in,

mfxStatus MFXVideoVPPProcAmp::Init(mfxFrameInfo* In, mfxFrameInfo* Out)
{
    MFX_CHECK_NULL_PTR2( In, Out );

    VPP_CHECK_MULTIPLE_INIT;

    if( In->Width  != Out->Width  ||
        In->Height != Out->Height ||
        In->FourCC != Out->FourCC ||
        In->PicStruct != Out->PicStruct)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if( (MFX_FOURCC_NV12 != In->FourCC) &&  (MFX_FOURCC_YV12 != In->FourCC) )
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    /* save init params to prevent core crash */
    m_errPrtctState.In  = *In;
    m_errPrtctState.Out = *Out;

    // NULL == set default parameters
    SetParam( NULL );

    VPP_INIT_SUCCESSFUL;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPProcAmp::Init(mfxFrameInfo* In, mfxFrameInfo* Out)

mfxStatus MFXVideoVPPProcAmp::Close(void)
{
    mfxStatus mfxSts = MFX_ERR_NONE;

    VPP_CHECK_NOT_INITIALIZED;

    VPP_CLEAN;

    return mfxSts;

} // mfxStatus MFXVideoVPPProcAmp::Close(void)


mfxStatus MFXVideoVPPProcAmp::GetBufferSize( mfxU32* pBufferSize )
{
    VPP_CHECK_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(pBufferSize);

    *pBufferSize = 0;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPProcAmp::GetBufferSize( mfxU32* pBufferSize )

mfxStatus MFXVideoVPPProcAmp::SetBuffer( mfxU8* pBuffer )
{
    mfxU32 bufSize = 0;
    mfxStatus sts;

    VPP_CHECK_NOT_INITIALIZED;

    sts = GetBufferSize( &bufSize );
    MFX_CHECK_STS( sts );

    // filter dosn't require work buffer, so pBuffer == NULL is OK
    if( bufSize )
    {
        MFX_CHECK_NULL_PTR1(pBuffer);
    }

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPProcAmp::SetBuffer( mfxU8* pBuffer )

// function is called from sync part of VPP only
mfxStatus MFXVideoVPPProcAmp::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
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

} // mfxStatus MFXVideoVPPProcAmp::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )

bool MFXVideoVPPProcAmp::IsReadyOutput( mfxRequestType requestType )
{
    if( MFX_REQUEST_FROM_VPP_CHECK == requestType )
    {
        // ProcAmp is simple algorithm and need IN to produce OUT
    }

    return false;

} // bool MFXVideoVPPProcAmp::IsReadyOutput( mfxRequestType requestType )

/* ******************************************************************** */
/*                 implementation of algorithms [ProcAmp]               */
/* ******************************************************************** */

void MFXVideoVPPProcAmp::CalculateInternalParams( void )
{
    m_internalBrightness = (mfxI32)((m_brightness * 16.0) + 0.5) << 11;
    m_internalContrast   = (mfxI32)((m_contrast * 128.0) + 0.5);
    m_hue                = (mfxF64)(m_hue*PI/180.0);
    m_internalSinCS      = (mfxI32)(0.5 + 256.0 * m_contrast * m_saturation * sin(m_hue));
    m_internalCosCS      = (mfxI32)(0.5 + 256.0 * m_contrast * m_saturation * cos(m_hue));

} // void MFXVideoVPPProcAmp::CalculateInternalParams( void )

mfxStatus ProcampFiltering_NV12_8u(
                               Ipp8u *pSrcY,
                               Ipp8u *pSrcUV,
                               Ipp8u *pDstY,
                               Ipp8u *pDstUV,

                               int width,
                               int height,

                               int stepSrcY,
                               int stepSrcUV,
                               int stepDstY,
                               int stepDstUV,

                               int  brightness,
                               int  contrast,
                               int  cosCS,
                               int  sinCS,

                               int  par_y_min,
                               int  par_uv_mid,
                               int  par_contrast_precision,
                               int  par_sincos_precision)
{
    int row, column;

    int inY, inU, inV;

    int outY, outU, outV;
    int tempY,tempU, tempV;

    // Y plane
    for (row = 0; row < height; row++)
    {
        for (column = 0; column < width; column++)
        {
            inY = (int)pSrcY[column];

            // patch
            inY <<= 8;

            tempY = inY - par_y_min;
            outY = (tempY * contrast) + brightness;
            outY >>= par_contrast_precision;
            outY += par_y_min;

            outY >>= 8;

            outY = VPP_RANGE_CLIP(outY, 0, 255);

            pDstY[column] = (Ipp8u) outY;
        }
        pSrcY  = pSrcY +  stepSrcY;
        pDstY  = pDstY +  stepDstY;
    }

    // UV plane
    int heightUV = height >> 1;

    for (row = 0; row < heightUV; row++)
    {
        for (column = 0; column < width; column += 2)
        {
            inU = (int)pSrcUV[column  ];
            inV = (int)pSrcUV[column+1];

            // patch
            inU <<= 8;
            inV <<= 8;

            tempU = inU - par_uv_mid;
            tempV = inV - par_uv_mid;

            // Apply hue and saturation for chroma
            outU = tempU * cosCS + tempV * sinCS;
            outV = tempV * cosCS - tempU * sinCS;

            outU >>= par_sincos_precision;
            outV >>= par_sincos_precision;

            outU += par_uv_mid;
            outV += par_uv_mid;

            outU >>= 8;
            outV >>= 8;

            outU = VPP_RANGE_CLIP(outU, 0, 255);
            outV = VPP_RANGE_CLIP(outV, 0, 255);

            pDstUV[column  ] = (Ipp8u) outU;
            pDstUV[column+1] = (Ipp8u) outV;
        }
        pSrcUV = pSrcUV + stepSrcUV;
        pDstUV = pDstUV + stepDstUV;
    }

    return MFX_ERR_NONE;

} // mfxStatus ProcampFiltering_NV12_8u(...)

mfxStatus ProcampFiltering_YV12_8u(
                               Ipp8u *pSrcY,
                               Ipp8u *pSrcV,
                               Ipp8u *pSrcU,

                               Ipp8u *pDstY,
                               Ipp8u *pDstV,
                               Ipp8u *pDstU,

                               int width,
                               int height,

                               int stepSrcY,
                               int stepSrcUV,

                               int stepDstY,
                               int stepDstUV,

                               int  brightness,
                               int  contrast,
                               int  cosCS,
                               int  sinCS,

                               int  par_y_min,
                               int  par_uv_mid,
                               int  par_contrast_precision,
                               int  par_sincos_precision)
{
    int row, column;

    int inY, inU, inV;

    int outY, outU, outV;
    int tempY,tempU, tempV;

    // Y plane
    for (row = 0; row < height; row++)
    {
        for (column = 0; column < width; column++)
        {
            inY = (int)pSrcY[column];

            // patch
            inY <<= 8;

            tempY = inY - par_y_min;
            outY = (tempY * contrast) + brightness;
            outY >>= par_contrast_precision;
            outY += par_y_min;

            outY >>= 8;

            outY = VPP_RANGE_CLIP(outY, 0, 255);

            pDstY[column] = (Ipp8u) outY;
        }
        pSrcY  = pSrcY +  stepSrcY;
        pDstY  = pDstY +  stepDstY;
    }

    // V plane
    int heightUV = height >> 1;
    int widthUV  = width >> 1;

    for (row = 0; row < heightUV; row++)
    {
        for (column = 0; column < widthUV; column++)
        {
            inU = (int)pSrcU[column];
            inV = (int)pSrcV[column];

            //patch
            inU <<= 8;
            inV <<= 8;

            tempU = inU - par_uv_mid;
            tempV = inV - par_uv_mid;

            // Apply hue and saturation for chroma
            outU = tempU * cosCS + tempV * sinCS;
            outV = tempV * cosCS - tempU * sinCS;

            outU >>= par_sincos_precision;
            outV >>= par_sincos_precision;

            outU += par_uv_mid;
            outV += par_uv_mid;

            outU >>= 8;
            outV >>= 8;

            outU = VPP_RANGE_CLIP(outU, 0, 255);
            outV = VPP_RANGE_CLIP(outV, 0, 255);

            pDstU[column] = (Ipp8u) outU;
            pDstV[column] = (Ipp8u) outV;
        }
        pSrcU = pSrcU + stepSrcUV;
        pDstU = pDstU + stepDstUV;

        pSrcV = pSrcV + stepSrcUV;
        pDstV = pDstV + stepDstUV;
    }

    return MFX_ERR_NONE;

} // mfxStatus ProcampFiltering_YV12_8u(...)

#endif // MFX_ENABLE_HW_ONLY_VPP
#endif // MFX_ENABLE_VPP
/* EOF */
