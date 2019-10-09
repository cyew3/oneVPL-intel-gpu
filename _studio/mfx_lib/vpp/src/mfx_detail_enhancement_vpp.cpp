// Copyright (c) 2010-2019 Intel Corporation
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

#if defined (MFX_ENABLE_VPP)

#include "ippi.h"
#include "umc_defs.h"

#include "mfx_vpp_utils.h"
#include "mfx_detail_enhancement_vpp.h"

#ifndef ALIGN16
#define ALIGN16(SZ) (((SZ + 15) >> 4) << 4) // round up to a multiple of 16
#endif

#define VPP_DETAIL_GAIN_MIN      (0)
#define VPP_DETAIL_GAIN_MAX      (63)
#define VPP_DETAIL_GAIN_MAX_REAL (63)
// replaced range [0...63] to native [0, 100%] wo notification of dev
// so a lot of modification required to provide smooth control (recalculate test streams etc)
// solution is realGain = CLIP(userGain, 0, 63)
#define VPP_DETAIL_GAIN_MAX_USER_LEVEL (100)
#define VPP_DETAIL_GAIN_DEFAULT VPP_DETAIL_GAIN_MIN

/* ******************************************************************** */
/*           implementation of VPP filter [DetailEnhancement]           */
/* ******************************************************************** */

mfxStatus MFXVideoVPPDetailEnhancement::Query( mfxExtBuffer* pHint )
{
    if( NULL == pHint )
    {
        return MFX_ERR_NONE;
    }

    mfxStatus sts = MFX_ERR_NONE;

    mfxExtVPPDetail* pParam = (mfxExtVPPDetail*)pHint;

    if( pParam->DetailFactor > VPP_DETAIL_GAIN_MAX_USER_LEVEL )
    {
        pParam->DetailFactor = VPP_DETAIL_GAIN_MAX_USER_LEVEL;

        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return sts;

} // mfxStatus MFXVideoVPPDetailEnhancement::Query( mfxExtBuffer* pHint )

#if !defined(MFX_ENABLE_HW_ONLY_VPP) // SW ONLY

//Detail Enhancement Filter (DEF) constants
#define VPP_DETAIL_SOBEL_STRONG        8
#define VPP_DETAIL_SOBEL_WEAK          1
#define VPP_DETAIL_STRONG_WEIGHT       6
#define VPP_DETAIL_REGULAR_WEIGHT      3
#define VPP_DETAIL_WEAK_WEIGHT         2

#define VPP_DETAIL_R3_C                ((59+2)>>2)
#define VPP_DETAIL_R3_X                ((25+2)>>2)
#define VPP_DETAIL_R5_C                3
#define VPP_DETAIL_R5_CX               8
#define VPP_DETAIL_R5_X                9

#define VPP_DETAIL_EXT_ROI_X          (5)
#define VPP_DETAIL_EXT_ROI_Y          (5)

/* ******************************************************************** */
/*                       Algorithm Prototypes                           */
/* ******************************************************************** */

mfxStatus CalcSobel_8u_C1R (uint8_t* curRowStart, int srcPitch,
                            uint8_t* SobelRowStart, int sobelPitch,
                            mfxSize size);

mfxStatus CalcDiff_8u_C1R (uint8_t* curRowStart, int srcPitch,
                           uint8_t* DiffRowStart, int diffPitch,
                           mfxSize size);

int Calc5x5Laplacian(int    x,
                     int    org_pix,
                     uint8_t* rowMinus2,
                     uint8_t* rowMinus1,
                     uint8_t* curRow,
                     uint8_t* rowPlus1,
                     uint8_t* rowPlus2);

int Calc3x3Laplacian(int    x,
                     int    org_pix,
                     uint8_t* rowMinus1,
                     uint8_t* curRow,
                     uint8_t* rowPlus1);

MFXVideoVPPDetailEnhancement::MFXVideoVPPDetailEnhancement(VideoCORE *core, mfxStatus* sts) : FilterVPP()
{
    if( core )
    {
        // something
    }

    m_strongSobel    = VPP_DETAIL_SOBEL_STRONG;
    m_weakSobel      = VPP_DETAIL_SOBEL_WEAK;

    m_strongWeight   = VPP_DETAIL_STRONG_WEIGHT;
    m_regularWeight  = VPP_DETAIL_REGULAR_WEIGHT;
    m_weakWeight     = VPP_DETAIL_WEAK_WEIGHT;

    m_gainFactor     = VPP_DETAIL_GAIN_DEFAULT;

    m_divTable[0] = m_divTable[1] = 0;

    for(int counter = 2; counter < 256; counter++)
    {
        m_divTable[counter] = (uint8_t)(0.5 + (1.00/counter) * (1<<8));
    }

    m_pSobelBuffer = m_pDiffBuffer = m_pExtBuffer = NULL;

    VPP_CLEAN;

    m_isFilterActive = true;

    *sts = MFX_ERR_NONE;
    m_internalGainFactor = 0;
    m_sobelPitch = 0;
    m_extPitch = 0;
    m_diffPitch = 0;
    return;

} // MFXVideoVPPDetailEnhancement::MFXVideoVPPDetailEnhancement(...)

MFXVideoVPPDetailEnhancement::~MFXVideoVPPDetailEnhancement(void)
{
    Close();

    return;

} // MFXVideoVPPDetailEnhancement::~MFXVideoVPPDetailEnhancement(void)

mfxStatus MFXVideoVPPDetailEnhancement::SetParam( mfxExtBuffer* pHint )
{
    mfxStatus mfxSts = MFX_ERR_NONE;

    if( pHint )
    {
        mfxExtVPPDetail* pDetailParams = (mfxExtVPPDetail*)pHint;

        m_gainFactor = mfx::clamp<mfxI32>(pDetailParams->DetailFactor,
                                      VPP_DETAIL_GAIN_MIN,
                                      VPP_DETAIL_GAIN_MAX);
    }
    else
    {
        m_gainFactor = VPP_DETAIL_GAIN_DEFAULT;
    }

    m_isFilterActive = true;

    if( VPP_DETAIL_GAIN_DEFAULT == m_gainFactor)
    {
        m_isFilterActive = false;// exclude from pipeline because algorithm doesn't change video data
    }

    m_internalGainFactor = (int)((float)m_gainFactor * (float)VPP_DETAIL_GAIN_MAX_REAL / VPP_DETAIL_GAIN_MAX);

    return mfxSts;

} // mfxStatus MFXVideoVPPDetailEnhancement::SetParam( mfxExtBuffer* pHint )

mfxStatus MFXVideoVPPDetailEnhancement::Reset(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1( par );

    VPP_CHECK_NOT_INITIALIZED;

    VPP_RESET;

    mfxExtBuffer* pHint = NULL;

    GetFilterParam(par, MFX_EXTBUFF_VPP_DETAIL, &pHint);

    SetParam( pHint );

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDetailEnhancement::Reset(mfxVideoParam *par)

mfxStatus MFXVideoVPPDetailEnhancement::RunFrameVPP(mfxFrameSurface1 *in,
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

    uint8_t* pSrcY;
    uint8_t* pDstY;
    uint8_t* pExtBufferROI;
    uint8_t* pSobelBufferROI;
    uint8_t* pDiffBufferROI;

    int stepSrcY, stepDstY;
    mfxSize roiSize;
    IppiRect roi;

    // ROI
    roi.x = in->Info.CropX;
    roi.y = in->Info.CropY;
    roiSize.width = roi.width = in->Info.CropW;
    roiSize.height= roi.height= in->Info.CropH;

    pSrcY = in->Data.Y + (roi.x + roi.y * in->Data.Pitch);
    stepSrcY = in->Data.Pitch;

    pDstY = out->Data.Y + (roi.x + roi.y * out->Data.Pitch);
    stepDstY = out->Data.Pitch;

    // extension of input Y
    pExtBufferROI = m_pExtBuffer + (roi.x + roi.y * m_extPitch);
    ippiCopy_8u_C1R(pSrcY, stepSrcY, pExtBufferROI, m_extPitch, roiSize);

    // ROI for internal Buffers
    pSobelBufferROI = m_pSobelBuffer + (roi.x + roi.y * m_sobelPitch);
    pDiffBufferROI  = m_pDiffBuffer + (roi.x + roi.y * m_diffPitch);

    // Detail Processing
    CalcSobel_8u_C1R( pExtBufferROI, m_extPitch, pSobelBufferROI, m_sobelPitch, roiSize );

    CalcDiff_8u_C1R( pExtBufferROI, m_extPitch, pDiffBufferROI, m_diffPitch, roiSize);

    DetailFilterCore( pExtBufferROI, m_extPitch,
                      pDstY, stepDstY,
                      pSobelBufferROI, m_sobelPitch,
                      pDiffBufferROI,  m_diffPitch,
                      roiSize);

    // UV planes aren't processed
    IppStatus ippSts;
    int pitchSrcUV, pitchDstUV;
    if( MFX_FOURCC_NV12 == in->Info.FourCC )
    {
        uint8_t *pSrcUV;
        uint8_t *pDstUV;

        pitchSrcUV = in->Data.Pitch;
        pitchDstUV = out->Data.Pitch;

        // (2) ROI
        pSrcUV = in->Data.UV + (roi.x + (roi.y >> 1) * pitchSrcUV);
        pDstUV = out->Data.UV + (roi.x + (roi.y >> 1)* pitchDstUV);

        roiSize.height >>= 1;
        ippSts = ippiCopy_8u_C1R(pSrcUV, pitchSrcUV, pDstUV, pitchDstUV, roiSize);
        VPP_CHECK_IPP_STS( ippSts );
    }
    else
    {
        uint8_t *pSrcU, *pSrcV;
        uint8_t *pDstU, *pDstV;

        pitchSrcUV = in->Data.Pitch / 2;
        pitchDstUV = out->Data.Pitch / 2;

        // (2) ROI
        pSrcV  = in->Data.V + (roi.x/2 + (roi.y >> 1) *  pitchSrcUV);
        pSrcU  = in->Data.U + (roi.x/2 + (roi.y >> 1) *  pitchSrcUV);

        pDstV = out->Data.V + (roi.x/2 + (roi.y >> 1) * pitchDstUV);
        pDstU = out->Data.U + (roi.x/2 + (roi.y >> 1) * pitchDstUV);

        roiSize.height >>= 1;
        roiSize.width  >>= 1;

        ippSts = ippiCopy_8u_C1R(pSrcV, pitchSrcUV, pDstV, pitchDstUV, roiSize);
        VPP_CHECK_IPP_STS( ippSts );

        ippSts = ippiCopy_8u_C1R(pSrcU, pitchSrcUV, pDstU, pitchDstUV, roiSize);
        VPP_CHECK_IPP_STS( ippSts );
    }

    pParam->outPicStruct = pParam->inPicStruct;
    pParam->outTimeStamp = pParam->inTimeStamp;

    return mfxSts;

} // mfxStatus MFXVideoVPPDetailEnhancement::RunFrameVPP(mfxFrameSurface1 *in,

mfxStatus MFXVideoVPPDetailEnhancement::Init(mfxFrameInfo* In, mfxFrameInfo* Out)
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

} // mfxStatus MFXVideoVPPDetailEnhancement::Init(mfxFrameInfo* In, mfxFrameInfo* Out)

mfxStatus MFXVideoVPPDetailEnhancement::Close(void)
{
    mfxStatus mfxSts = MFX_ERR_NONE;

    VPP_CHECK_NOT_INITIALIZED;

    VPP_CLEAN;

    return mfxSts;

} // mfxStatus MFXVideoVPPDetailEnhancement::Close(void)


mfxStatus MFXVideoVPPDetailEnhancement::GetBufferSize( mfxU32* pBufferSize )
{
    VPP_CHECK_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(pBufferSize);

    mfxSize size = {m_errPrtctState.In.Width, m_errPrtctState.In.Height};

    int alignExtWidth  = ALIGN16(size.width + 2*VPP_DETAIL_EXT_ROI_X);
    int alignExtHeight = ALIGN16(size.height + 2*VPP_DETAIL_EXT_ROI_Y);

    *pBufferSize = (size.width * size.height) + // sobel requires Y only
                   (size.width * size.height) + // diff - the same
                   (alignExtWidth * alignExtHeight);// extension of input Y plane to simplify logic

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDetailEnhancement::GetBufferSize( mfxU32* pBufferSize )

mfxStatus MFXVideoVPPDetailEnhancement::SetBuffer( mfxU8* pBuffer )
{
    mfxU32 bufSize = 0;
    mfxStatus sts;

    VPP_CHECK_NOT_INITIALIZED;

    sts = GetBufferSize( &bufSize );
    MFX_CHECK_STS( sts );

    if( bufSize )
    {
        MFX_CHECK_NULL_PTR1(pBuffer);

        mfxSize size = {m_errPrtctState.In.Width, m_errPrtctState.In.Height};
        int alignExtWidth = ALIGN16(size.width + 2*VPP_DETAIL_EXT_ROI_X);
        int offset = 0;

        // map memory
        m_sobelPitch = size.width;
        m_pSobelBuffer = pBuffer;
        offset = m_sobelPitch * size.height;

        m_diffPitch = size.width;
        m_pDiffBuffer  = pBuffer + offset;
        offset += m_diffPitch * size.height;

        m_extPitch = alignExtWidth;
        m_pExtBuffer = (pBuffer + offset) + m_extPitch * VPP_DETAIL_EXT_ROI_Y + VPP_DETAIL_EXT_ROI_X;
    }

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDetailEnhancement::SetBuffer( mfxU8* pBuffer )

// function is called from sync part of VPP only
mfxStatus MFXVideoVPPDetailEnhancement::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
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

} // mfxStatus MFXVideoVPPDetailEnhancement::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )

bool MFXVideoVPPDetailEnhancement::IsReadyOutput( mfxRequestType requestType )
{
    if( MFX_REQUEST_FROM_VPP_CHECK == requestType )
    {
        // Detail Enhancement is simple algorithm and need IN to produce OUT
    }

    return false;

} // bool MFXVideoVPPDetailEnhancement::IsReadyOutput( mfxRequestType requestType )

mfxStatus MFXVideoVPPDetailEnhancement::DetailFilterCore( uint8_t* pSrc, int srcPitch,
                                                          uint8_t* pDst, int dstPitch,
                                                          uint8_t* pSobel, int sobelPitch,
                                                          uint8_t* pDiff,  int diffPitch,
                                                          mfxSize size)
{
    const int srcPrecision = 8;//16;
    const int minClip  = (-(1<< (5 + srcPrecision - 8)));
    const int maxClip  = (1<< (5 + srcPrecision - 8)) - 1;
    const int maxVal   = (1<< srcPrecision)- 1;
    const int minSharp = (-(1<<(srcPrecision + 3)));
    const int maxSharp = (1<<(srcPrecision + 3)) - 1;

    int filter = 0;

    for(int row = 0; row < size.height; row++)
    {
        uint8_t *pCurRow    = pSrc + row*srcPitch;  //row
        uint8_t *pRowMinus2 = pCurRow - 2 * srcPitch;//row-2
        uint8_t *pRowMinus1 = pCurRow - 1 * srcPitch;//row-1
        uint8_t *pRowPlus1  = pCurRow + 1 * srcPitch;//row+1
        uint8_t *pRowPlus2  = pCurRow + 2 * srcPitch;//row+2

        uint8_t *pDstRow   = pDst   + row*dstPitch;
        uint8_t *pSobelRow = pSobel + row*sobelPitch;
        uint8_t *pDiffRow  = pDiff  + row*diffPitch;

        for(int col = 0; col < size.width; col++)
        {
            int sharp, localAdjust, diff, dstVal;

            // 5x5 LAPLACIAN
            {
                int srcVal = (int)pCurRow[col];
                int filter5 = Calc5x5Laplacian(col, srcVal, pRowMinus2, pRowMinus1, pCurRow, pRowPlus1, pRowPlus2);
                int filter3 = Calc3x3Laplacian(col, srcVal, pRowMinus1, pCurRow, pRowPlus1);
                filter = filter5 - filter3;
            }

            sharp =  -filter;//14s
            sharp = mfx::clamp(sharp, minSharp, maxSharp);

            localAdjust = m_weakWeight;
            if(pSobelRow[col] > m_weakSobel)
            {
                localAdjust = m_regularWeight;
            }
            if(pSobelRow[col] > m_strongSobel)
            {
                localAdjust = m_strongWeight;
            }

            diff = std::min((8 + pDiffRow[col]), 255); //8 bits

            sharp   = (localAdjust * sharp * m_internalGainFactor + 64) >> 7;
            sharp   = (sharp * (int)m_divTable[diff] + (1 << 7)) >> 8;
            sharp   = mfx::clamp(sharp, minClip, maxClip);

            dstVal = mfx::clamp(sharp + pCurRow[col], 0, maxVal);
            pDstRow[col] = (uint8_t)dstVal;
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDetailEnhancement::DetailFilterCore( ... )

/* ******************************************************************** */
/*                 implementation of algorithms [Detail Enhancement]    */
/* ******************************************************************** */

mfxStatus CalcSobel_8u_C1R (uint8_t* curRowStart, int srcPitch,
                            uint8_t* SobelRowStart, int sobelPitch,
                            mfxSize size)
{
    int row, col;
    uint8_t *curRow, *aboveRow, *belowRow;
    uint8_t *SobelRow;
    int   sobel;

    for(row = 0; row < size.height; row++)
    {
        curRow      = curRowStart + row * srcPitch;
        aboveRow    = curRow - srcPitch;
        belowRow    = curRow + srcPitch;

        SobelRow    = SobelRowStart + (row*sobelPitch);

        for(col = 0; col < size.width; col++)
        {
            int p0, p1, p2, p3, p4, p5, p6, p7, p8;
            /*
            col
            x p0    x p1    x p2


            row     x p3    x p4    x p5


            x p6    x p7    x p8
            */

            p0 = (int)aboveRow[col-1];
            p1 = (int)aboveRow[col];
            p2 = (int)aboveRow[col+1];
            p3 = (int)curRow  [col-1];
            p4 = (int)curRow  [col];
            p5 = (int)curRow  [col+1];
            p6 = (int)belowRow[col-1];
            p7 = (int)belowRow[col];
            p8 = (int)belowRow[col+1];

            int dy  = (p2 + p0 + 2 * p1) >> 3;
            dy -= (p8 + p6 + 2 * p7) >> 3;

            int dx  = (p0 + p6 + 2 * p3) >> 3;
            dx -= (p2 + p8 + 2 * p5) >> 3;

            dx = abs(dx);   //15 bits
            dy = abs(dy);   //15 bits

            sobel = mfx::clamp(((dx+dy+2048)>>(4+8)), 0, 15);
            // +8 in >> to reduce precision by 8 bits
            SobelRow[col] = (uint8_t)sobel;
        }
    }

    return MFX_ERR_NONE;

} // VppStatus CalcSobel_8u_C1R (...)

mfxStatus CalcDiff_8u_C1R (uint8_t* curRowStart, int srcPitch,
                           uint8_t* DiffRowStart, int diffPitch,
                           mfxSize size)
{
    int row, col, counter;
    uint8_t* curRow, *aboveRow, *belowRow;
    uint8_t* DiffRow;
    int diff;

    for(row = 0; row < size.height; row++)
    {
        curRow      = curRowStart + row * srcPitch;
        aboveRow    = curRow - srcPitch;
        belowRow    = curRow + srcPitch;
        DiffRow     = DiffRowStart + (row*diffPitch);

        for(col = 0; col < size.width; col++)
        {
            uint8_t max, min, p[9];

            /*
            col
            x p0    x p1    x p2


            row     x p3    x p4    x p5


            x p6    x p7    x p8
            */

            p[0] = aboveRow [col-1];
            p[1] = aboveRow [col];
            p[2] = aboveRow [col+1];
            p[3] = curRow   [col-1];
            p[4] = curRow   [col];
            p[5] = curRow   [col+1];
            p[6] = belowRow [col-1];
            p[7] = belowRow [col];
            p[8] = belowRow [col+1];

            //find the min and max in the 3x3 block
            max = min = p[0];
            for(counter=1; counter<9; counter++)
            {
                if(p[counter] > max)
                {
                    max = p[counter];
                }
                if(p[counter] < min)
                {
                    min = p[counter];
                }
            }

            diff = max - min;
            DiffRow[col] = (uint8_t)(diff >> 8);
        }
    }

    return MFX_ERR_NONE;

} // VppStatus CalcDiff_8u_C1R(...)

int Calc5x5Laplacian(int    x,
                     int    org_pix,
                     uint8_t* rowMinus2,
                     uint8_t* rowMinus1,
                     uint8_t* curRow,
                     uint8_t* rowPlus1,
                     uint8_t* rowPlus2)
{
    int sm5_pix, p[16];

    /*
    x
    x p1    x p4    x p15   x p5    x p2

    x p10   x       x       x       x p11

    row     x p12   x       x       x       x p13

    x p8    x       x       x       x p9

    x p0    x p7    x p14   x p6    x p3
    */

    p[0]  = (int) rowPlus2  [x-2];
    p[1]  = (int) rowMinus2 [x-2];
    p[2]  = (int) rowMinus2 [x+2];
    p[3]  = (int) rowPlus2  [x+2];
    p[4]  = (int) rowMinus2 [x-1];
    p[5]  = (int) rowMinus2 [x+1];
    p[6]  = (int) rowPlus2  [x+1];
    p[7]  = (int) rowPlus2  [x-1];
    p[8]  = (int) rowPlus1  [x-2];
    p[9]  = (int) rowPlus1  [x+2];
    p[10] = (int) rowMinus1 [x-2];
    p[11] = (int) rowMinus1 [x+2];
    p[12] = (int) curRow    [x-2];
    p[13] = (int) curRow    [x+2];
    p[14] = (int) rowPlus2  [x];
    p[15] = (int) rowMinus2 [x];

    sm5_pix =   VPP_DETAIL_R5_X  * (p[0]  +  p[1]  +  p[2]  +  p[3]  - (org_pix << 2));  //13.5s
    sm5_pix +=  VPP_DETAIL_R5_CX * (p[4]  +  p[5]  +  p[6]  +  p[7]  - (org_pix << 2));  //14.5s
    sm5_pix +=  VPP_DETAIL_R5_CX * (p[8]  +  p[9]  +  p[10] +  p[11] - (org_pix << 2));  //15.5s
    sm5_pix +=  VPP_DETAIL_R5_C  * (p[12] +  p[13] - (org_pix << 1));     //16.5s
    sm5_pix +=  VPP_DETAIL_R5_C  * (p[14] +  p[15] - (org_pix << 1));     //16.5s

    sm5_pix = sm5_pix >> 4;// 16.1s

    return sm5_pix;

} // int Calc5x5Laplacian(...)

int Calc3x3Laplacian(int    x,
                     int    org_pix,
                     uint8_t* rowMinus1,
                     uint8_t* curRow,
                     uint8_t* rowPlus1)
{
    int sm3_pix;
    int p[8];

    /*
    x(p7]   x(p3)   x(p5)

    x(p1)   x       x(p0)

    x(p6)   x(p2)   x(p4)
    */

    p[0] = (int)curRow    [x+1]   - org_pix;
    p[1] = (int)curRow    [x-1]   - org_pix;
    p[2] = (int)rowPlus1  [x]     - org_pix;
    p[3] = (int)rowMinus1 [x]     - org_pix;
    p[4] = (int)rowPlus1  [x+1]   - org_pix;
    p[5] = (int)rowMinus1 [x+1]   - org_pix;
    p[6] = (int)rowPlus1  [x-1]   - org_pix;
    p[7] = (int)rowMinus1 [x-1]   - org_pix;

    sm3_pix  =  VPP_DETAIL_R3_C * (p[0] + p[1] + p[2] + p[3]);     //13.5s
    sm3_pix +=  VPP_DETAIL_R3_X * (p[4] + p[5] + p[6] + p[7]);     //14.5
    sm3_pix  =  sm3_pix >> 4;// 14.1s

    return sm3_pix;

} // int Calc3x3Laplacian(...)

#endif // MFX_ENABLE_HW_ONLY_VPP
#endif // MFX_ENABLE_VPP
/* EOF */
