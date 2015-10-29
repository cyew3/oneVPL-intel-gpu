/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2010-2015 Intel Corporation. All Rights Reserved.
//
//
//          GamutCompression Video Post Processing Filter
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP) && !defined(MFX_ENABLE_HW_ONLY_VPP)

#include <math.h>

#include "mfx_vpp_utils.h"
#include "mfx_gamut_compression_vpp.h"

// tuning params for gamut
#define GAMUT_COMPRESSION_FACTOR_PARAM (3)
#define GAMUT_D_IN_PARAM               (820)
#define GAMUT_D_IN_DEFAULT_PARAM       (205)
#define GAMUT_D_OUT_PARAM              (287)
#define GAMUT_D_OUT_DEFAULT_PARAM      (164)

// math constant
#define GAMUT_PI       (3.14159265358979323846)

/* ******************************************************************** */
/*           prototypes of GamutCompression primitives                  */
/* ******************************************************************** */

mfxStatus DecodeXVYCC_NV12_8u32f(mfxFrameSurface1* pSrc, Surface1_32f* pDst );
mfxStatus ConvertYUV2LCH_NV12_32f( Surface1_32f* pSrc,   Surface1_32f* pDst );
mfxStatus EncodeXVYCC_NV12_32f8u( Surface1_32f* pSrc, mfxFrameSurface1* pDst );

/* ******************************************************************** */
/*           implementation of VPP filter [GamutCompression]            */
/* ******************************************************************** */

void MFXVideoVPPGamutCompression::CalculateInternalParams( void )
{
    mfxStatus sts;

    // create tables
    sts = CreateColorTables( m_LumaVertex, m_SatVertex, m_YUV2RGB, m_bBT601 );

    // create div table
    sts = CreateDivTable( m_DivTable );

} // void MFXVideoVPPGamutCompression::CalculateInternalParams( void )


mfxStatus MFXVideoVPPGamutCompression::Query( mfxExtBuffer* pHint )
{
    pHint;
    //if( pHint )
    //{
    //    mfxExtVPPGamutMapping* pGamutParams = (mfxExtVPPGamutMapping*)pHint;    

    //    mfxGamutMode mode = GetGamutMode( pGamutParams->InTransferMatrix, pGamutParams->OutTransferMatrix );

    //    if( GAMUT_INVALID_MODE == mode )
    //    {
    //        pGamutParams->InTransferMatrix = pGamutParams->OutTransferMatrix = 0;

    //        return MFX_ERR_UNSUPPORTED;
    //    }
    //}

    return MFX_ERR_NONE;

} // static mfxStatus MFXVideoVPPGamutCompression::Query( mfxExtBuffer* pHint )


MFXVideoVPPGamutCompression::MFXVideoVPPGamutCompression(VideoCORE *core, mfxStatus* sts)
: FilterVPP()

, m_bBT601(false) // default is BT.709

, m_mode( GAMUT_COMPRESS_ADVANCED_MODE )

, m_CompressionFactor( GAMUT_COMPRESSION_FACTOR_PARAM )
, m_Din( GAMUT_D_IN_PARAM )
, m_Dout( GAMUT_D_OUT_PARAM )
, m_Din_Default( GAMUT_D_IN_DEFAULT_PARAM )
, m_Dout_Default( GAMUT_D_OUT_DEFAULT_PARAM )
{
    m_core = core;

    VPP_CLEAN;

    *sts = MFX_ERR_NONE;

    return;

} // MFXVideoVPPGamutCompression::MFXVideoVPPGamutCompression(VideoCORE *core, mfxStatus* sts) : FilterVPP()


MFXVideoVPPGamutCompression::~MFXVideoVPPGamutCompression(void)
{
    Close();

    return;

} // MFXVideoVPPGamutCompression::~MFXVideoVPPGamutCompression(void)

mfxStatus MFXVideoVPPGamutCompression::SetParam( mfxExtBuffer* pHint )
{
    pHint;
    mfxStatus mfxSts = MFX_ERR_NONE;    

    //if( pHint )
    //{
    //    mfxExtVPPGamutMapping* pGamutParams = (mfxExtVPPGamutMapping*)pHint;

    //    m_mode = GetGamutMode( pGamutParams->InTransferMatrix, pGamutParams->OutTransferMatrix );
    //    if( GAMUT_INVALID_MODE == m_mode )
    //    {
    //        return MFX_ERR_INVALID_VIDEO_PARAM;
    //    }
    //    else
    //    {
    //        if( MFX_TRANSFERMATRIX_BT601 == pGamutParams->OutTransferMatrix )
    //        {
    //            m_bBT601 = true;
    //        }
    //        else
    //        {
    //            m_bBT601 = false;
    //        }
    //    }
    //}

    //CalculateInternalParams();

    return mfxSts;

} // mfxStatus MFXVideoVPPGamutCompression::SetParam( mfxExtBuffer* pHint )


mfxStatus MFXVideoVPPGamutCompression::Reset(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1( par );

    VPP_CHECK_NOT_INITIALIZED;

    VPP_RESET;

    //mfxExtBuffer* pHint;

//    GetFilterParam(par, MFX_EXTBUFF_VPP_GAMUT_MAPPING, &pHint);

//    mfxStatus sts = SetParam( pHint );
    mfxStatus sts = MFX_ERR_NONE;

    return sts;

} // mfxStatus MFXVideoVPPGamutCompression::Reset(mfxVideoParam *par)


mfxStatus MFXVideoVPPGamutCompression::RunFrameVPP(mfxFrameSurface1 *in,
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

    // core processing

    Ipp8u *pSrcY, *pSrcUV;
    Ipp8u *pDstY, *pDstUV;

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

        // (4) GamutCompression
        // //algorithm

        //mfxSts = SurfaceCopy_ROI(out, in);

        mfxSts = DecodeXVYCC_NV12_8u32f(in, &m_frameYUV);
        MFX_CHECK_STS( mfxSts );

        mfxSts = ConvertYUV2LCH_NV12_32f(&m_frameYUV, &m_frameLCH );
        MFX_CHECK_STS( mfxSts );

        mfxSts = FixedHueCompression_NV12_32f( &m_frameCompressedYUV );
        MFX_CHECK_STS( mfxSts );

        mfxSts = EncodeXVYCC_NV12_32f8u(&m_frameCompressedYUV, out);
        MFX_CHECK_STS( mfxSts );

        // debug only
        //mfxSts = EncodeXVYCC_NV12_32f8u(&m_frameYUV, out);
    }


    pParam->outPicStruct = pParam->inPicStruct;
    pParam->outTimeStamp = pParam->inTimeStamp;

    return mfxSts;

} // mfxStatus MFXVideoVPPGamutCompression::RunFrameVPP(mfxFrameSurface1 *in,


mfxStatus MFXVideoVPPGamutCompression::Init(mfxFrameInfo* In, mfxFrameInfo* Out)
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


    /* set params for internal float-point surfaces */
    mfxU16 width  = m_errPrtctState.In.Width;
    mfxU16 height = m_errPrtctState.In.Height;
    mfxU16 pitch  = (mfxU16)VPP_ALIGN32(width);

    m_frameYUV.Pitch  = m_frameLCH.Pitch  = m_frameCompressedLCH.Pitch  = m_frameCompressedYUV.Pitch  = pitch;
    m_frameYUV.Width  = m_frameLCH.Width  = m_frameCompressedLCH.Width  = m_frameCompressedYUV.Width  = width;
    m_frameYUV.Height = m_frameLCH.Height = m_frameCompressedLCH.Height = m_frameCompressedYUV.Height = height;
    m_frameYUV.FourCC = m_frameLCH.FourCC = m_frameCompressedLCH.FourCC = m_frameCompressedYUV.FourCC = MFX_FOURCC_NV12;


    // NULL == set default parameters
    SetParam( NULL );

    VPP_INIT_SUCCESSFUL;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPGamutCompression::Init(mfxFrameInfo* In, mfxFrameInfo* Out)


mfxStatus MFXVideoVPPGamutCompression::Close(void)
{
    mfxStatus mfxSts = MFX_ERR_NONE;

    VPP_CHECK_NOT_INITIALIZED;

    VPP_CLEAN;

    return mfxSts;

} // mfxStatus MFXVideoVPPGamutCompression::Close(void)


mfxStatus MFXVideoVPPGamutCompression::GetBufferSize( mfxU32* pBufferSize )
{
    VPP_CHECK_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(pBufferSize);

    // m_frameYUV/LCH/compressed(LCH/YUV) have the same width/height/pitch/fourcc
    mfxU16 height = m_frameYUV.Height;
    mfxU16 pitch  = m_frameYUV.Pitch;

    *pBufferSize = ((3 * pitch * height * sizeof(Ipp32f)) >> 1 ) << 2;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPGamutCompression::GetBufferSize( mfxU32* pBufferSize )


mfxStatus MFXVideoVPPGamutCompression::SetBuffer( mfxU8* pBuffer )
{
    mfxU32 bufSize = 0;
    mfxStatus sts;

    VPP_CHECK_NOT_INITIALIZED;

    sts = GetBufferSize( &bufSize );
    MFX_CHECK_STS( sts );

    if( 0 == bufSize )
    {
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    }

    MFX_CHECK_NULL_PTR1(pBuffer);

    // OK, need to init 4 float-point surfaces (NV12)
    mfxU32 frameSize  = (bufSize >> 2);      // in bytes
    mfxU32 lumaSize   = (frameSize << 1) / 3;// in bytes
    //mfxU32 chromaSize = (frameSize - lumaSize) >> 1;// in bytes

    // YUV (Y_UV)
    m_frameYUV.pY  = (mfxF32*)pBuffer;
    m_frameYUV.pUV = (mfxF32*)(pBuffer + lumaSize);

    // LCH
    m_frameLCH.pL  = (mfxF32*)(pBuffer + 1*frameSize);
    m_frameLCH.pCH = (mfxF32*)(pBuffer + 1*frameSize + lumaSize);
    //m_frameLCH.pH  = (mfxF32*)(pBuffer + 1*frameSize + lumaSize + chromaSize);

    // compressedLCH
    m_frameCompressedLCH.pL  = (mfxF32*)(pBuffer + 2*frameSize);
    m_frameCompressedLCH.pCH = (mfxF32*)(pBuffer + 2*frameSize + lumaSize);
    //m_frameCompressedLCH.pH  = (mfxF32*)(pBuffer + 2*frameSize + lumaSize + chromaSize);

    // compressedYUV
    m_frameCompressedYUV.pY  = (mfxF32*)(pBuffer + 3*frameSize);
    m_frameCompressedYUV.pUV = (mfxF32*)(pBuffer + 3*frameSize + lumaSize);

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPGamutCompression::SetBuffer( mfxU8* pBuffer )


// function is called from sync part of VPP only
mfxStatus MFXVideoVPPGamutCompression::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
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

} // mfxStatus MFXVideoVPPGamutCompression::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )


bool MFXVideoVPPGamutCompression::IsReadyOutput( mfxRequestType requestType )
{
    if( MFX_REQUEST_FROM_VPP_CHECK == requestType )
    {
        // GamutCompression is simple algorithm and need IN to produce OUT
    }

    return false;

} // bool MFXVideoVPPGamutCompression::IsReadyOutput( mfxRequestType requestType )

/* ******************************************************************** */
/*        implementation of algorithm primitives [GamutMapping]         */
/* ******************************************************************** */

mfxStatus DecodeXVYCC_NV12_8u32f(mfxFrameSurface1* pSrc, Surface1_32f* pDst )
{
    mfxStatus sts = MFX_ERR_NONE;
    const int INTERNAL_PRECISION = 12;
    const int srcPrecision = 8;

    mfxU16 y, x;
    mfxU16 width     = pSrc->Info.Width;
    mfxU16 height    = pSrc->Info.Height;
    mfxU16 srcPitch  = pSrc->Data.Pitch;
    mfxU16 dstPitch  = pDst->Pitch;

    // [Y]
    mfxU8*  pSrcY;
    mfxF32* pDstY;
    for( y = 0; y < height; y++ )
    {
        pSrcY = pSrc->Data.Y + y*srcPitch;
        pDstY = pDst->pY     + y*dstPitch;
        for( x = 0; x < width; x++ )
        {
            //pDstY[x] = (Ipp32f)(((Ipp32f)pSrcY[x] - 16.f) / 219.f);
            pDstY[x] = (Ipp32f)((((pSrcY[x] <<(INTERNAL_PRECISION - srcPrecision)) -256) *4788)>> INTERNAL_PRECISION);
            //pDstY[x] = (Ipp32f)pSrcY[x];
        }
    }

    // [U/V]
    width    >>= 1;
    height   >>= 1;
    //srcPitch >>= 1;
    //dstPitch >>= 1;

    mfxU8*  pSrcUV;
    mfxF32* pDstUV;
    for( y = 0; y < height; y++ )
    {
        pSrcUV = pSrc->Data.UV + y*srcPitch;
        pDstUV = pDst->pUV     + y*dstPitch;

        for( x = 0; x < width; x++ )
        {
            pDstUV[2*x]   =(mfxF32)((((pSrcUV[2*x]   <<(INTERNAL_PRECISION - srcPrecision)) -2048) *4681)>> INTERNAL_PRECISION);//U
            pDstUV[2*x+1] =(mfxF32)((((pSrcUV[2*x+1] <<(INTERNAL_PRECISION - srcPrecision)) -2048) *4681)>> INTERNAL_PRECISION);//V
        }
    }

    return sts;

} // mfxStatus DecodeXVYCC_NV12_8u32f(mfxFrameSurface1* pSrc, Surface1_32f* pDst )


mfxStatus ConvertYUV2LCH_NV12_32f( Surface1_32f* pSrc,   Surface1_32f* pDst )
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxU16 y, x;
    mfxU16 width     = pSrc->Width;
    mfxU16 height    = pSrc->Height;
    mfxU16 srcPitch  = pSrc->Pitch;
    mfxU16 dstPitch  = pDst->Pitch;

    // [Y]
    mfxF32*  pSrcY;
    mfxF32*  pDstL;
    for( y = 0; y < height; y++ )
    {
        pSrcY = pSrc->pY + y*srcPitch;
        pDstL = pDst->pY + y*dstPitch;
        for( x = 0; x < width; x++ )
        {
            pDstL[x] = pSrcY[x];
        }
    }

    // [U/V]
    width    >>= 1;
    height   >>= 1;
    //srcPitch >>= 1;
    //dstPitch >>= 1;

    mfxF32*  pSrcUV;
    mfxF32*  pDstCH;

    for( y = 0; y < height; y++ )
    {
        pSrcUV = pSrc->pUV + y*srcPitch;
        pDstCH = pDst->pCH + y*dstPitch;

        for( x = 0; x < width; x++ )
        {
            mfxF32 srcU = pSrcUV[2*x];
            mfxF32 srcV = pSrcUV[2*x+1];
            mfxF32 dstC, dstH;

            dstC = sqrt(srcU*srcU + srcV*srcV);

            dstH = static_cast <Ipp32f> (atan2(srcV, srcU) * 180/GAMUT_PI);
            if( dstH < 0 )
            {
                dstH+=180;
            }
            dstH = (mfxF32)(0.35156 * (int)(dstH/0.35156+0.5));    // degree

            /*if( approximation)
            {
            }*/

            pDstCH[2*x]   = dstC;
            pDstCH[2*x+1] = dstH;
        }
    }

    return sts;

} // mfxStatus ConvertYUV2LCH_NV12_32f( Surface1_32f* pSrc,   Surface1_32f* pDst )


mfxStatus EncodeXVYCC_NV12_32f8u( Surface1_32f* pSrc, mfxFrameSurface1* pDst )
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxU16 y, x;
    mfxU16 width     = pSrc->Width;
    mfxU16 height    = pSrc->Height;
    mfxU16 srcPitch  = pSrc->Pitch;
    mfxU16 dstPitch  = pDst->Data.Pitch;

    const int m_OutPrecision = 8;
    const int INTERNAL_PRECISION = 12;

    const int maxPixVal = (1 << m_OutPrecision) - 1; // Ya-Ti

    // [Y]
    Ipp32f*  pSrcY;
    Ipp8u* pDstY;
    for( y = 0; y < height; y++ )
    {
        pSrcY = pSrc->pY     + y*srcPitch;
        pDstY = pDst->Data.Y + y*dstPitch;
        for( x = 0; x < width; x++ )
        {
            int dstY = (((int)(pSrcY[x] * (219)))>>INTERNAL_PRECISION) + (16); // resulting in 12-bits representation
            pDstY[x] = (Ipp8u)VPP_RANGE_CLIP(dstY, 0, maxPixVal);
        }
    }

    // [U/V]
    width    >>= 1;
    height   >>= 1;
    //srcPitch >>= 1;
    //dstPitch >>= 1;

    Ipp32f*  pSrcUV;
    Ipp8u*   pDstUV;

    for( y = 0; y < height; y++ )
    {
        pSrcUV = pSrc->pUV     + y*srcPitch;
        pDstUV = pDst->Data.UV + y*dstPitch;

        for( x = 0; x < width; x++ )
        {
            int dstU = (((int)(pSrcUV[2*x] * (224)))>>INTERNAL_PRECISION) + 128;
            pDstUV[2*x] = (Ipp8u)VPP_RANGE_CLIP( dstU, 0,  maxPixVal);

            int dstV = (((int)(pSrcUV[2*x+1] * (224)))>>INTERNAL_PRECISION) + 128;    // resulting in 12-bits representation
            pDstUV[2*x+1] = (Ipp8u)VPP_RANGE_CLIP( dstV, 0,  maxPixVal);
        }
    }

    return sts;

} // mfxStatus EncodeXVYCC_NV12_32f8u( Surface1_32f* pSrc, mfxFrameSurface1* pDst )


mfxStatus MFXVideoVPPGamutCompression::FixedHueCompression_NV12_32f( Surface1_32f* pDst )
{
    mfxStatus sts = MFX_ERR_NONE;

    int angle_index,ev;
    int srcL,srcC,srcY,srcU,srcV,trgY,trgU,trgV,trgL,trgC,trgH,ScalingBasic, ScalingAdvanced;
    float srcH;
    int Sat_Vertex, Luma_Vertex, Luma1, Luma2;    // Ya-Ti -- affecting the detection of out-of-boundary pixels
    int BoundarySlope,VerticalSlope,CompressionSlope, SrcC_x_CompressionSlope;
    int Luma_Intercept_L,Luma_Intercept_RGB,Sat_Intercept_RGB,Luma_Reference, Sat_Reference;
    int Din,Dout,Din_Change,Dout_Change,Dfinal,D_inner,D_outer,D_Ref_Bound,D_Ref_Input,Din_Default,Dout_Default;

    int precision =12;
    int denorm = 1 << precision;

    int x, y, height, width;

    height = pDst->Height;
    width  = pDst->Width;

    bool pixel_out_range= false;
    int m_Pixel_OOR = 0;    // YT: 01/12/2010 -- need to initialize this value for every image; otherwise, it accumulates to the next one
    int m_Pixel_OORD = 0;    // YT: 01/12/2010 -- same reason above

    for( y = 0; y < height; y++ )
    {
        // src
        float* pSrcL  = m_frameLCH.pL  + y*m_frameLCH.Pitch;
        float* pSrcCH = m_frameLCH.pCH + (y >> 1)* (m_frameLCH.Pitch);

        float* pSrcY  = m_frameYUV.pY  + y*m_frameYUV.Pitch;
        float* pSrcUV = m_frameYUV.pUV + (y >> 1)* (m_frameYUV.Pitch);

        // dst
        float* pTrgY = pDst->pY   + y*pDst->Pitch;
        float* pTrgUV = pDst->pUV + (y >> 1)*(pDst->Pitch);

        float* pTrgL  = m_frameCompressedLCH.pL  + y*m_frameCompressedLCH.Pitch;
        float* pTrgCH = m_frameCompressedLCH.pCH + (y>>1)*(m_frameCompressedLCH.Pitch);

        for( x = 0; x < width; x++ )
        {
            int indx1 = x;
            int indx2 = (x >> 1) << 1;
            int indx3 = indx2 + 1;

            // src
            srcL = (int)pSrcL[indx1];
            srcC = (int)pSrcCH[indx2];
            srcH = (float)pSrcCH[indx3];

            srcY = (int)pSrcY[indx1];
            srcU = (int)pSrcUV[indx2];
            srcV = (int)pSrcUV[indx3];

            // dst
            trgY = (int)pTrgY[indx1];
            trgU = (int)pTrgUV[indx2];
            trgV = (int)pTrgUV[indx3];

            trgL = (int)pTrgL[indx1];
            trgC = (int)pTrgCH[indx2];
            trgH = (int)pTrgCH[indx3];

   //         if (m_arctan_approximate)    // TCC module
            //{
            //    angle_index = IndexMapping((int)srcH) ;
            //
            //}
            //else                                        //atan2 function
            {
                angle_index = (int)(srcH/0.35156);
            }

            Sat_Vertex = m_SatVertex[angle_index]; // table is essentially int.
            Luma1 = m_LumaVertex[angle_index];
            Luma2 = denorm-Luma1; //keep 12 bits


            // Ya-Ti
            if(srcV == 0)    // 0 or 180 degree
            {
                Luma_Vertex = (srcU>0) ? Luma1 : Luma2;
            }
            else
            {
                Luma_Vertex = (srcV>0) ? Luma1 : Luma2;
            }
            ////////////////////////////////////////

            //------------------------------------
            ev = (srcL>=Luma_Vertex)?denorm:0;    // YT: 12/01/2009 -- change from "srcL>Luma_Vertex" to "srcL>=Luma_Vertex" to match the following checking condition for in-out-of range pixels


            if( ((srcL - Luma_Vertex)>=0) && ((srcL - ( ((HWDivision((Luma_Vertex - ev), Sat_Vertex, precision))*srcC)>>precision )- ev)>0) )        // YT: need to add "float" since now we haven't used "HWDivision" here yet
            {
                pixel_out_range = true;
                m_Pixel_OOR++;

            }
            else if( ((srcL - Luma_Vertex)<0) && ((srcL - ( ((HWDivision((Luma_Vertex - ev), Sat_Vertex, precision))*srcC)>>precision )- ev)<0) )
            {
                pixel_out_range = true;
                m_Pixel_OOR++;
            }
            else
            {
                pixel_out_range = false;
            }


            //Scaling factor
            BoundarySlope = HWDivision((Luma_Vertex-ev), Sat_Vertex, precision);                    //RGB Boundary

            //if(m_RTL_model)        // YT: RTL_model from Niraj
            //{
            //    BoundarySlope = CLIP_VAL(BoundarySlope, -(1<<14), ((1<<14) - 1));
            //}

            // Sharath: Moved the VerticalSlope after the CLIP_VAL for BoundarySlope
            VerticalSlope = HWDivision((denorm), abs(BoundarySlope), precision);    // YT: 09/03/2009; "(-denorm)" => "(denorm)" according to HW team's feedback

            int VerticalSlope_shifted = VerticalSlope >> m_CompressionFactor;    // YT: 12/08/2009 -- Sharath's input

            SrcC_x_CompressionSlope = (srcC * VerticalSlope) >> (precision + m_CompressionFactor);    // YT: 09/03/2009; added based on HW team's feedback

            //if(m_RTL_model)        // YT: RTL_model from Niraj
            //{
            //    SrcC_x_CompressionSlope = CLIP_VAL(SrcC_x_CompressionSlope, 0, ((1<<14) - 1));
            //    VerticalSlope_shifted = CLIP_VAL(VerticalSlope_shifted, 0, ((1<<14) - 1));    // YT: 12/08/2090 -- Sharath's input, per A spec, the compresion slope is clamped to u2.12
            //}


            if(ev == 0)
            {
                //CompressionSlope = -(VerticalSlope >> m_CompressionFactor);    // YT: 12/08/2009
                CompressionSlope = -(VerticalSlope_shifted);    // YT: 12/08/2009 -- Sharath's input
                Luma_Intercept_L = srcL +((SrcC_x_CompressionSlope)) ;
            }
            else
            {
                //CompressionSlope = VerticalSlope >> m_CompressionFactor;    // YT: 12/08/2009
                CompressionSlope = VerticalSlope_shifted;    // YT: 12/08/2009 -- Sharath's input
                Luma_Intercept_L = srcL - ((SrcC_x_CompressionSlope)) ;
            }

            //if(m_RTL_model)        // YT: RTL_model from Niraj
            //{
            //    Luma_Intercept_L = CLIP_VAL(Luma_Intercept_L, -(1<<14), ((1<<14) - 1));
            //}

            Sat_Intercept_RGB = HWDivision(abs((Luma_Intercept_L-ev)), abs((BoundarySlope-CompressionSlope)), precision);    // YT: 09/03/2009    //Intersection of compression line and RGB boundary

            //if(m_RTL_model)        // YT: RTL_model from Niraj
            //{
            //    Sat_Intercept_RGB = CLIP_VAL(Sat_Intercept_RGB, -(1<<12), ((1<<12) - 1));
            //}

            // Sharath: moved the condition for Sat_Intercept_RGB after the CLIP_VAL of Sat_Intercept_RGB
            if(((Luma_Intercept_L - ev)*(BoundarySlope-CompressionSlope))<0)    // YT: possible for intersection point with negative saturation value
            Sat_Intercept_RGB = -Sat_Intercept_RGB;


            Luma_Intercept_RGB = (((Sat_Intercept_RGB* BoundarySlope)>>precision) + ev);

            //if(m_RTL_model)        // YT: RTL_model from Niraj
            //{
            //    Luma_Intercept_RGB = CLIP_VAL(Luma_Intercept_RGB, -300, 4488);
            //}

            trgC = Sat_Intercept_RGB;

            ScalingBasic = (srcC==0)? denorm :HWDivision(trgC, srcC, precision); // to bypass div by zero

            //if(m_RTL_model)    // YT: RTL_model -- from Niraj: check added to see if this condition hits ever
            //{
            //    ScalingBasic = CLIP_VAL(ScalingBasic, (-(denorm*32)), (32*denorm-1));
            //}


            if ((srcC == 0) || (!pixel_out_range) || ((srcL == Luma_Vertex)&&(srcC <Sat_Vertex)))
            {
                trgY = srcY;
                trgU = srcU;
                trgV = srcV;
            }
            else
            {
                trgY = Luma_Intercept_RGB;
                trgU = (srcU*ScalingBasic)>>precision;
                trgV = (srcV*ScalingBasic)>>precision;

                //if(m_RTL_model)    // YT: RTL model-- from Niraj: would require clamping for all the above conditions
                //{
                //    trgY = CLIP_VAL(trgY, -(1<<13), ((1<<13) - 1));
                //    //SF for basic mode can be greater than 1. So clamping may be essential here.
                //    trgU = CLIP_VAL(trgU, -(1<<13), ((1<<13) - 1));
                //    trgV = CLIP_VAL(trgV, -(1<<13), ((1<<13) - 1));
                //}
            }


            // YT: 01/12/2010 -- The following part originally appears in the advanced compression mode; now is moved to here so that one can output the OORD for basic mode too
            Luma_Reference = ((srcL >= Luma_Vertex) ? IPP_MAX(Luma_Intercept_L, Luma_Vertex) : IPP_MIN(Luma_Intercept_L, Luma_Vertex)); //reference point    // YT: 12/16/2009 -- change from "srcL > Luma_Vertex" to "srcL >= Luma_Vertex" to match the "ev = (srcL>=Luma_Vertex)?denorm:0;    "
            Sat_Reference = (abs(Luma_Reference - Luma_Intercept_L)*abs(BoundarySlope)) >> (precision + m_CompressionFactor);

            //if(m_RTL_model)        // YT: RTL_model from Niraj
            //{
            //    Luma_Reference = CLIP_VAL(Luma_Reference, -(1<<14), ((1<<14) - 1));
            //    Sat_Reference = CLIP_VAL(Sat_Reference, 0, 3311);
            //}

            D_Ref_Bound = (int)(sqrt((float)(Luma_Reference - Luma_Intercept_RGB) * (Luma_Reference - Luma_Intercept_RGB) + (Sat_Reference - Sat_Intercept_RGB) * (Sat_Reference - Sat_Intercept_RGB)));    // YT: ref_dist_brt_dk in Matlab
            D_Ref_Input = (int)(sqrt((float)(Luma_Reference - srcL) * (Luma_Reference - srcL) + (Sat_Reference - srcC) * (Sat_Reference - srcC)));    // YT : pix_dist in Matlab

            //if(m_RTL_model)        // YT: RTL_model from Niraj
            //{
            //    D_Ref_Bound = CLIP_VAL(D_Ref_Bound, 0, ((1<<14) - 1));
            //    D_Ref_Input = CLIP_VAL(D_Ref_Input, 0, ((1<<14) - 1));
            //}
            ///////////////////////////////////////////////////////////////

            // YT: 01/12/2010 -- also output the sum of out-of-range distance for basic mode
            if((pixel_out_range) && (GAMUT_COMPRESS_BASE_MODE == m_mode))    // YT: 08/09/2010 -- adding "!m_advance" so that OORD is not added twice in the advance mode
            {
                if((D_Ref_Input - D_Ref_Bound) > 0)
                {
                    m_Pixel_OORD = m_Pixel_OORD + (D_Ref_Input - D_Ref_Bound);
                }
            }
            ///////////////////////////////////


            if (GAMUT_COMPRESS_ADVANCED_MODE == m_mode)
            {
                //get local version
                Din = m_Din;
                Dout = m_Dout;
                Din_Default = m_Din_Default;
                Dout_Default = m_Dout_Default;

                // YT: 01/12/2010 -- The following part has been moved to the front
                /*
                Luma_Reference = ((srcL >= Luma_Vertex) ? max(Luma_Intercept_L, Luma_Vertex) : min(Luma_Intercept_L, Luma_Vertex)); //reference point    // YT: 12/16/2009 -- change from "srcL > Luma_Vertex" to "srcL >= Luma_Vertex" to match the "ev = (srcL>=Luma_Vertex)?denorm:0;    "
                //Sat_Reference = HWDivision((Luma_Reference - Luma_Intercept_L), CompressionSlope,precision);    //YT: 10/05/2009 -- change to the following according to Sharath's input
                Sat_Reference = (abs(Luma_Reference - Luma_Intercept_L)*abs(BoundarySlope)) >> (precision + m_CompressionFactor);

                if(m_RTL_model)        // YT: RTL_model from Niraj
                {
                    Luma_Reference = CLIP_VAL(Luma_Reference, -(1<<14), ((1<<14) - 1));
                    Sat_Reference = CLIP_VAL(Sat_Reference, 0, 3311);
                }


                D_Ref_Bound = (int)(sqrt((float)(Luma_Reference - Luma_Intercept_RGB) * (Luma_Reference - Luma_Intercept_RGB) + (Sat_Reference - Sat_Intercept_RGB) * (Sat_Reference - Sat_Intercept_RGB)));    // YT: ref_dist_brt_dk in Matlab
                D_Ref_Input = (int)(sqrt((float)(Luma_Reference - srcL) * (Luma_Reference - srcL) + (Sat_Reference - srcC) * (Sat_Reference - srcC)));    // YT : pix_dist in Matlab

                if(m_RTL_model)        // YT: RTL_model from Niraj
                {
                    D_Ref_Bound = CLIP_VAL(D_Ref_Bound, 0, ((1<<14) - 1));
                    D_Ref_Input = CLIP_VAL(D_Ref_Input, 0, ((1<<14) - 1));
                }*/

                // YT: 11/05/2009 -- sum up the distance (d(reference, pixel) - d(reference, boundary)) of out-of-range pixels
                if(pixel_out_range)
                {
                    if((D_Ref_Input - D_Ref_Bound) > 0)
                    {
                        m_Pixel_OORD = m_Pixel_OORD + (D_Ref_Input - D_Ref_Bound);
                    }
                }


                //if(m_RTL_model)        // YT: RTL_model from Niraj(2)
                //{
                //    int LInt_by_Lvertex = HWDivision(Luma_Intercept_RGB, Luma_Vertex, precision);
                //    LInt_by_Lvertex = CLIP_VAL(LInt_by_Lvertex, -(1<<14), ((1<<14)-1));

                //    Din_Change = (int)(Din  + ((Din_Default - Din  )*LInt_by_Lvertex>>precision));
                //    Dout_Change =(int)(Dout + ((Dout_Default - Dout)*LInt_by_Lvertex>>precision));
                //}
                //else
                {
                    Din_Change = (int)(Din  + ((Din_Default - Din  )*(HWDivision(Luma_Intercept_RGB, Luma_Vertex, precision))>>precision));// Further change to integer division; 06/26/2009 RTL feedback
                    Dout_Change =(int)(Dout + ((Dout_Default - Dout)*(HWDivision(Luma_Intercept_RGB, Luma_Vertex, precision))>>precision));// Further change to integer division; 06/26/2009 RTL feedback
                }

                //if(m_RTL_model)        // YT: RTL_model from Niraj
                //{
                //    Din_Change = CLIP_VAL(Din_Change, -(1<<13), ((1<<13) - 1));
                //    Dout_Change = CLIP_VAL(Dout_Change, -(1<<13), ((1<<13) - 1));
                //}

                Din = (srcL >= Luma_Vertex) ? Din_Default : IPP_MAX(Din_Default, Din_Change);    // YT: 12/16/2009 -- change from "srcL > Luma_Vertex" to "srcL >= Luma_Vertex" to be consistent with other places for classifying upper/lower region
                Dout = (srcL >= Luma_Vertex) ? Dout_Default : IPP_MAX(Dout_Default, Dout_Change); // YT: 12/16/2009 -- change from "srcL > Luma_Vertex" to "srcL >= Luma_Vertex" to be consistent with other places for classifying upper/lower region

                //if(m_RTL_model)        // YT: RTL_model from Niraj
                //{
                //    Din = CLIP_VAL(Din, 0, ((1<<13) - 1));
                //    Dout = CLIP_VAL(Dout, 0, ((1<<13) - 1));
                //}

                D_inner = IPP_MAX(D_Ref_Bound-Din , 0);
                D_outer = D_Ref_Bound + Dout;

                //if(m_RTL_model)        // YT: RTL_model from Niraj
                //{
                //    D_inner = CLIP_VAL(D_inner, 0, ((1<<14) - 1));
                //    D_outer = CLIP_VAL(D_outer, 0, ((1<<14) - 1));
                //}


                // Ya-Ti: for strange reason, C does not add D_inner to ((D_Ref_Input - D_inner) * (HWDivision((D_Ref_Bound - D_inner),(D_outer-D_inner),10)))>>10

                //if(m_RTL_model)        // YT: RTL_model from Niraj(2)
                //{
                //    int Din_by_DinDout = HWDivision((D_Ref_Bound - D_inner),(D_outer-D_inner), precision);
                //    Din_by_DinDout = CLIP_VAL(Din_by_DinDout , 0, ((1<<12) - 1));
                //    Dfinal =(D_Ref_Input > D_inner)? (D_inner +  (((D_Ref_Input - D_inner) * Din_by_DinDout )>>precision) ):D_Ref_Input;
                //}
                //else
                {
                    Dfinal =(D_Ref_Input > D_inner)? (D_inner +  (((D_Ref_Input - D_inner) * (HWDivision((D_Ref_Bound - D_inner),(D_outer-D_inner), precision)))>>precision) ):D_Ref_Input; // Ya-Ti 07/13/2009
                }

                //if(m_RTL_model)        // YT: RTL_model from Niraj
                //{
                //    Dfinal = CLIP_VAL(Dfinal, 0, ((1<<14) - 1));
                //}


                //if(m_RTL_model)        // YT: RTL_model from Niraj(2)
                //{
                //    if(D_Ref_Input == 0)    // YT: Need to check this condition first, otherwise will encounter division
                //    trgC = Sat_Reference + (srcC - Sat_Reference);
                //    else
                //    {
                //        int Dpfinal_by_Drefpoint = HWDivision(Dfinal,D_Ref_Input,precision);
                //        Dpfinal_by_Drefpoint = CLIP_VAL(Dpfinal_by_Drefpoint  , 0, ((1<<12) - 1));
                //        trgC = (Sat_Reference + (((srcC - Sat_Reference) * Dpfinal_by_Drefpoint)>>precision));
                //    }
                //}
                //else
                {
                    trgC = (D_Ref_Input ==0)?(Sat_Reference + (srcC - Sat_Reference)):(Sat_Reference + (((srcC - Sat_Reference) * (HWDivision(Dfinal,D_Ref_Input,precision)))>>precision)); // Further change to integer division; 06/26/2009 RTL feedback
                }

                //if(m_RTL_model)        // YT: RTL_model from Niraj
                //{
                //    trgC = CLIP_VAL(trgC, 0, ((1<<12) - 1));
                //}


                // trgL = Luma_Intercept_L + (trgC * CompressionSlope)/denorm;
                trgL = Luma_Intercept_L + ((trgC * CompressionSlope)>>precision); // Removed the divide and replaced it by Right Shift; 06/26/2009 RTL feedback

                //if(m_RTL_model)        // YT: RTL_model from Niraj
                //{
                //    trgL = CLIP_VAL(trgL, -(1<<13), ((1<<13) - 1));
                //}


                ScalingAdvanced = (srcC==0)?denorm:HWDivision(trgC,srcC,precision);// to bypass div by zero

                //scaling

                if (srcC == 0)
                {
                    trgY = srcY;
                    trgU = srcU;
                    trgV = srcV;
                }
                else
                {
                    trgY = trgL;
                    trgU = (int)(srcU*ScalingAdvanced)>>precision;
                    trgV = (int)(srcV*ScalingAdvanced)>>precision;

                    //if(m_RTL_model)        // YT: RTL model from Niraj -- would require clamping for all the above conditions
                    //{
                    //    trgY = CLIP_VAL(trgY, -(1<<13), ((1<<13) - 1));
                    //    trgU = CLIP_VAL(trgU, -(1<<12), ((1<<12) - 1));
                    //    trgV = CLIP_VAL(trgV, -(1<<12), ((1<<12) - 1));
                    //}

                }
            }


            pTrgY[indx1]  = (float)trgY;
            pTrgUV[indx2] = (float)trgU;
            pTrgUV[indx3] = (float)trgV;

            //------------------------------------
        }
    }

    return sts;

} // mfxStatus MFXVideoVPPGamutCompression::FixedHueCompression_NV12_32f( Surface1_32f* pDst )


int FindMsbIndex(Ipp32u x)
{
    if(x == 0)
    {
        return 0;
    }

    Ipp32u idx;
    Ipp32u minIdx = 0;
    Ipp32u maxIdx = 32;
    idx = (maxIdx + minIdx) >> 1;

    while(maxIdx > minIdx)
    {
        Ipp32u t = x >> idx;

        //idx is too small
        if(t > 1)
        {
            minIdx = idx;
        }
        //idx is too large
        else if(t == 0)
        {
            maxIdx = idx;
        }
        //just right
        else
        {
            break;
        }

        idx = (maxIdx + minIdx) >> 1;
    }

    return idx + 1;

} // int FindMsbIndex(Ipp32u x)


int MFXVideoVPPGamutCompression::HWDivision(int nom, int denom, int outBitPrec)
{
    if(0 == denom)
    {
        //ErrMsg("IP_Division:\n"
        //"Division by zero!");
        return 0;
    }

    int signNom, signDenom;
    if(nom < 0)
    {
        signNom = -1;
        nom = -nom;
    }
    else
    {
        signNom = 1;
    }

    if(denom < 0)
    {
        signDenom = -1;
        denom = -denom;
    }
    else
    {
        signDenom = 1;
    }


    //find MSB index of denom
    int MSB_ind = FindMsbIndex(denom);

    //LSB is the original denom without its MSB
    int LSB = denom - (1 << (MSB_ind-1));

    //find index to div table
    int div_table_ind_shift = 1 + GCC_DIV_TABLE_SIZE_BITS - MSB_ind;
    int div_table_ind;
    if(div_table_ind_shift >= 0)
    {
        div_table_ind = LSB << div_table_ind_shift;
    }
    else
    {
        div_table_ind = LSB >> (-div_table_ind_shift);
    }

    //get multiplier from div table
    int div_nom = m_DivTable[div_table_ind];

    //calculate nom/denom
    int output_shift = MSB_ind - 1 + (GCC_DIV_TABLE_PREC_BITS - outBitPrec);

    // The following part has been changed based on the feedback from the HW team -- YT 09/03/2009
    /*
    int half_div = (1 << output_shift) >> 1;
    int res = ((nom * div_nom) + half_div) >> output_shift;
    return signNom * signDenom * res;*/

    // New one from HW team -- YT 09/03/2009
    int half_div = (output_shift < 0) ? ((1 >> abs(output_shift)) >> 1) : (1 << abs(output_shift)) >> 1;
    int sig = (signNom * signDenom);

    int res_tmp =(output_shift < 0 )? sig*((nom * div_nom) + half_div) << abs(output_shift): sig*((nom * div_nom) + half_div) >> abs(output_shift) ;
    int res = (nom == 0)?0:res_tmp;    // YT: 09/29/2009

    return  res;

} // int GamutFilter::HWDivision(int nom, int denom, int outBitPrec)


#endif // MFX_ENABLE_VPP
/* EOF */
