/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 - 2016 Intel Corporation. All Rights Reserved.
//
//
//          utilities for (SW)VPP processing
//
*/
#include <math.h>
#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#include "mfx_enc_common.h"
#include "mfx_vpp_utils.h"

#include "mfx_vpp_hw.h"

#if defined (MFX_VA)
#include "mfxpcp.h"
#endif

#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"

const mfxU32 g_TABLE_DO_NOT_USE [] =
{
    MFX_EXTBUFF_VPP_DENOISE,
    MFX_EXTBUFF_VPP_SCENE_ANALYSIS,
    MFX_EXTBUFF_VPP_PROCAMP,
    MFX_EXTBUFF_VPP_DETAIL,
    MFX_EXTBUFF_VPP_IMAGE_STABILIZATION,
    MFX_EXTBUFF_VPP_COMPOSITE,
    MFX_EXTBUFF_VPP_ROTATION,
    MFX_EXTBUFF_VPP_SCALING,
    MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO,
    MFX_EXTBUFF_VPP_FIELD_PROCESSING,
    MFX_EXTBUFF_VPP_MIRRORING
};


const mfxU32 g_TABLE_DO_USE [] =
{
    MFX_EXTBUFF_VPP_DENOISE,
    MFX_EXTBUFF_VPP_SCENE_ANALYSIS,
    MFX_EXTBUFF_VPP_PROCAMP,
    MFX_EXTBUFF_VPP_DETAIL,
    MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION,
    MFX_EXTBUFF_VPP_IMAGE_STABILIZATION,
    MFX_EXTBUFF_VPP_COMPOSITE,
    MFX_EXTBUFF_VPP_ROTATION,
    MFX_EXTBUFF_VPP_SCALING,
    MFX_EXTBUFF_VPP_VARIANCE_REPORT,
    MFX_EXTBUFF_VPP_DEINTERLACING,
    MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO,
    MFX_EXTBUFF_VPP_FIELD_PROCESSING,
    MFX_EXTBUFF_VPP_MIRRORING
};


// aya: should be synch with GetConfigSize()
const mfxU32 g_TABLE_CONFIG [] =
{
    MFX_EXTBUFF_VPP_DENOISE,
    MFX_EXTBUFF_VPP_PROCAMP,
    MFX_EXTBUFF_VPP_DETAIL,
    MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION,
    MFX_EXTBUFF_VPP_IMAGE_STABILIZATION,
    MFX_EXTBUFF_VPP_COMPOSITE,
    MFX_EXTBUFF_VPP_ROTATION,
    MFX_EXTBUFF_VPP_DEINTERLACING,
    MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO,
    MFX_EXTBUFF_VPP_FIELD_PROCESSING,
    MFX_EXTBUFF_VPP_SCALING,
    MFX_EXTBUFF_VPP_MIRRORING
};


const mfxU32 g_TABLE_EXT_PARAM [] =
{
    MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION,
    MFX_EXTBUFF_SVC_SEQ_DESC,
    MFX_EXTBUFF_MVC_SEQ_DESC,

    MFX_EXTBUFF_VPP_DONOTUSE,
    MFX_EXTBUFF_VPP_DOUSE,

    // should be the same as g_TABLE_CONFIG
    MFX_EXTBUFF_VPP_DENOISE,
    MFX_EXTBUFF_VPP_PROCAMP,
    MFX_EXTBUFF_VPP_DETAIL,
    MFX_EXTBUFF_VPP_ROTATION,
    MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION,
    MFX_EXTBUFF_VPP_IMAGE_STABILIZATION,
    MFX_EXTBUFF_VPP_COMPOSITE,
    MFX_EXTBUFF_VPP_DEINTERLACING,
    MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO,
    MFX_EXTBUFF_VPP_FIELD_PROCESSING,
    MFX_EXTBUFF_VPP_SCALING,
    MFX_EXTBUFF_VPP_MIRRORING
};

// in according with spec rev. 22583 VPP uses new PicStruct processing
PicStructMode GetPicStructMode(mfxU16 inPicStruct, mfxU16 outPicStruct)
{
    if( (inPicStruct & (MFX_PICSTRUCT_FIELD_BFF | MFX_PICSTRUCT_FIELD_TFF)) &&
        MFX_PICSTRUCT_PROGRESSIVE == outPicStruct)
    {
        return DYNAMIC_DI_PICSTRUCT_MODE;
    }
    else if( MFX_PICSTRUCT_UNKNOWN == inPicStruct && MFX_PICSTRUCT_PROGRESSIVE == outPicStruct)
    {
        return DYNAMIC_DI_PICSTRUCT_MODE;
    }
    else
    {
        return PASS_THROUGH_PICSTRUCT_MODE;
    }

} // PicStructMode GetPicStructMode(mfxU16 inPicStruct, mfxU16 outPicStruct)


// in according with spec requirements VPP controls input picstruct
mfxStatus CheckInputPicStruct( mfxU16 inPicStruct )
{
    /*if( MFX_PICSTRUCT_UNKNOWN == inPicStruct )
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }*/

    const mfxU16 case1 = MFX_PICSTRUCT_PROGRESSIVE;
    const mfxU16 case2 = MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FRAME_DOUBLING;
    const mfxU16 case3 = MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FRAME_TRIPLING;
    const mfxU16 case4 = MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_BFF;
    const mfxU16 case5 = MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF;
    const mfxU16 case6 = MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_BFF | MFX_PICSTRUCT_FIELD_REPEATED;
    const mfxU16 case7 = MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_REPEATED;
    const mfxU16 case8 = MFX_PICSTRUCT_FIELD_BFF;
    const mfxU16 case9 = MFX_PICSTRUCT_FIELD_TFF;

    mfxStatus sts = MFX_ERR_NONE;

    switch( inPicStruct )
    {
        case case1:
        case case2:
        case case3:
        case case4:
        case case5:
        case case6:
        case case7:
        case case8:
        case case9:
        {
            sts = MFX_ERR_NONE;
            break;
        }

        default:
        {
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
        }
    } // switch

    return sts;

} // mfxStatus CheckInputPicStruct( mfxU16 inPicStruct )


// rules for this algorithm see in spec
mfxU16 UpdatePicStruct( mfxU16 inPicStruct, mfxU16 outPicStruct, bool bDynamicDeinterlace, mfxStatus& sts )
{
    mfxU16 resultPicStruct;

    // XXX->UNKOWN
    if( MFX_PICSTRUCT_UNKNOWN == outPicStruct )
    {
        if( (inPicStruct & MFX_PICSTRUCT_PROGRESSIVE) || !bDynamicDeinterlace )
        {
            resultPicStruct = inPicStruct;
        }
        else // DynamicDeinterlace
        {
            resultPicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        }

        sts = MFX_ERR_NONE;
        return resultPicStruct;
    }

    // PROGRESSIVE->PROGRESSIVE
    mfxU16 inPicStructCore  = (mfxU16)(inPicStruct  & MFX_PICSTRUCT_PROGRESSIVE);
    mfxU16 outPicStructCore = (mfxU16)(outPicStruct & MFX_PICSTRUCT_PROGRESSIVE);
    if( inPicStructCore && outPicStructCore )
    {
        mfxU16 decorateFlags = ( MFX_PICSTRUCT_FRAME_DOUBLING |
                                 MFX_PICSTRUCT_FRAME_TRIPLING |
                                 MFX_PICSTRUCT_FIELD_REPEATED |
                                 MFX_PICSTRUCT_FIELD_TFF |
                                 MFX_PICSTRUCT_FIELD_BFF );

        resultPicStruct = outPicStruct | ( inPicStruct & decorateFlags );

        sts = MFX_ERR_NONE;
        return resultPicStruct;
    }
    // PROGRESSIVE->INTERLACE
    else if( (inPicStruct & MFX_PICSTRUCT_PROGRESSIVE) && (outPicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF)) )
    {
        resultPicStruct = outPicStruct;

        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        return resultPicStruct;
    }
    // INTERLACE->PROGRESSIVE
    else if( (inPicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF)) && (outPicStruct & MFX_PICSTRUCT_PROGRESSIVE) )
    {
        resultPicStruct = outPicStruct;

        sts = (bDynamicDeinterlace) ? MFX_ERR_NONE : MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        return resultPicStruct;
    }

    // INTERLACE->INTERLACE
    inPicStructCore  = inPicStruct  & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF);
    outPicStructCore = outPicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF);
    if( inPicStructCore == outPicStructCore ) // the same type (tff->tff or bff->bff)
    {
        mfxU16 decorateFlags = ( MFX_PICSTRUCT_FRAME_DOUBLING |
                                 MFX_PICSTRUCT_FRAME_TRIPLING |
                                 MFX_PICSTRUCT_FIELD_REPEATED );

        resultPicStruct = outPicStruct | ( inPicStruct & decorateFlags );

        //sts = MFX_ERR_NONE;
        sts = ( !bDynamicDeinterlace ) ? MFX_ERR_NONE : MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        return resultPicStruct;
    }
    else if( inPicStructCore && outPicStructCore ) // different type (tff->bff or bff->tff)
    {
        resultPicStruct = outPicStruct;

        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        return resultPicStruct;
    }

    // default
    resultPicStruct = outPicStruct;

    sts = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    return resultPicStruct;

} // void UpdatePicStruct( mfxU16 inPicStruct, mfxU16& outPicStruct, PicStructMode mode )


/* all protection must been done before call */
mfxStatus SurfaceCopy_ROI(mfxFrameSurface1* out, mfxFrameSurface1* in, bool bROIControl)
{
    IppiSize roiSize;
    IppStatus sts;
    mfxFrameData* inData  = &(in->Data);
    mfxFrameData* outData = &(out->Data);
    mfxFrameInfo* inInfo = &(in->Info);
    mfxFrameInfo* outInfo= (bROIControl) ? &(out->Info) : &(in->Info);
    mfxU16  cropX = 0, cropY = 0;

    mfxU32  inOffset0 = 0, inOffset1  = 0;
    mfxU32  outOffset0= 0, outOffset1 = 0;
    mfxU32  inPitch;
    mfxU32  outPitch;

    if( MFX_FOURCC_RGB3 == in->Info.FourCC || MFX_FOURCC_RGB4 == in->Info.FourCC || MFX_FOURCC_A2RGB10 == in->Info.FourCC )
    {
        // in case of RGB we process total frame
        MFX_CHECK_NULL_PTR1(inData->R);
        MFX_CHECK_NULL_PTR1(inData->G);
        MFX_CHECK_NULL_PTR1(inData->B);

        mfxU8* ptrInput = IPP_MIN( IPP_MIN(inData->R, inData->G), inData->B );

        MFX_CHECK_NULL_PTR1(outData->R);
        MFX_CHECK_NULL_PTR1(outData->G);
        MFX_CHECK_NULL_PTR1(outData->B);

        mfxU8* ptrOutput = IPP_MIN( IPP_MIN(outData->R, outData->G), outData->B );

        VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
        VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

        if( MFX_FOURCC_RGB3 == in->Info.FourCC )
        {
            roiSize.width *= 3;
        }
        else
        {
            roiSize.width *= 4;
        }

        sts = ippiCopy_8u_C1R(ptrInput, inData->Pitch, ptrOutput, outData->Pitch, roiSize);
        VPP_CHECK_IPP_STS( sts );

        return MFX_ERR_NONE;
    }

    // in case of YV12/NV12 FRC pay attention cropping process.
    VPP_GET_CROPX(inInfo,  cropX);
    VPP_GET_CROPY(inInfo,  cropY);
    inOffset0  = cropX  + cropY*inData->Pitch;

    if( MFX_FOURCC_YV12 == in->Info.FourCC )
    {
        inOffset1  = (cropX >> 1) + (cropY >> 1)*(inData->Pitch >> 1);
    }
    else //NV12
    {
        inOffset1  = (cropX) + (cropY >> 1)*(inData->Pitch);
    }

    VPP_GET_CROPX(outInfo, cropX);
    VPP_GET_CROPY(outInfo, cropY);
    outOffset0  = cropX + cropY*outData->Pitch;

    if( MFX_FOURCC_YV12 == out->Info.FourCC )
    {
        outOffset1  = (cropX >> 1) + (cropY >> 1)*(outData->Pitch >> 1);
    }
    else
    {
        outOffset1  = (cropX) + (cropY >> 1)*(outData->Pitch);
    }

    VPP_GET_WIDTH(inInfo, roiSize.width);
    VPP_GET_HEIGHT(inInfo, roiSize.height);

    inPitch  = inData->Pitch;
    outPitch = outData->Pitch;

    if ( MFX_FOURCC_P010 == out->Info.FourCC || MFX_FOURCC_P210 == out->Info.FourCC )
    {
        roiSize.width <<= 1;
    }

    sts = ippiCopy_8u_C1R(inData->Y+inOffset0,   inPitch,
                          outData->Y+outOffset0, outPitch, roiSize);
    VPP_CHECK_IPP_STS( sts );

    if( MFX_FOURCC_NV12 == in->Info.FourCC || MFX_FOURCC_P010 == in->Info.FourCC )
    {
        roiSize.height >>= 1;
        sts = ippiCopy_8u_C1R(inData->UV+inOffset1,   inPitch,
                              outData->UV+outOffset1, outPitch, roiSize);
        VPP_CHECK_IPP_STS( sts );
    }
    else if( MFX_FOURCC_NV16 == in->Info.FourCC || MFX_FOURCC_P210 == in->Info.FourCC )
    {
        sts = ippiCopy_8u_C1R(inData->UV+inOffset1,   inPitch,
                              outData->UV+outOffset1, outPitch, roiSize);
        VPP_CHECK_IPP_STS( sts );
    }
    else
    {
        roiSize.height >>= 1;
        roiSize.width  >>= 1;
        sts = ippiCopy_8u_C1R(inData->V+inOffset1, inData->Pitch>>1,
                              outData->V+outOffset1, outData->Pitch >> 1, roiSize);
        VPP_CHECK_IPP_STS( sts );

        sts = ippiCopy_8u_C1R(inData->U+inOffset1, inData->Pitch>>1,
                              outData->U+outOffset1, outData->Pitch >> 1, roiSize);
        VPP_CHECK_IPP_STS( sts );
    }

    if( !bROIControl )
    {
        mfxFrameSurface1 localSurface;
        localSurface.Data = out->Data;
        localSurface.Info = in->Info;

        mfxStatus mfxSts = SetBackGroundColor( &localSurface );
        MFX_CHECK_STS( mfxSts );
    }

    return MFX_ERR_NONE;

} // mfxStatus SurfaceCopy_ROI(mfxFrameSurface1* out, mfxFrameSurface1* in)


mfxStatus SetBackGroundColor(mfxFrameSurface1 *ptr)
{
    const  Ipp8u BLACK_CLR_Y  = 16;
    const  Ipp8u BLACK_CLR_UV = 128;

    const mfxI32 BLACK_CLR_YUY2 = MFX_MAKEFOURCC(BLACK_CLR_Y, BLACK_CLR_UV, BLACK_CLR_Y, BLACK_CLR_UV);
    const mfxI32 BLACK_CLR_RGB4 = 0;//{0, 0, 0, 0}

    IppiSize roiSize;
    IppStatus sts;

    MFX_CHECK_NULL_PTR1(ptr);

    // in accordance with spec out is [NV12, YUY2] only

    if( (ptr->Info.FourCC != MFX_FOURCC_NV12 ) &&
        (ptr->Info.FourCC != MFX_FOURCC_YUY2 ) &&
        (ptr->Info.FourCC != MFX_FOURCC_RGB4 ))
    {
            return MFX_ERR_NONE;
    }

    if( ptr->Info.CropH == ptr->Info.Height &&
        ptr->Info.CropW == ptr->Info.Width  &&
        ptr->Info.CropX == 0 && ptr->Info.CropY == 0 ){

            return MFX_ERR_NONE;
    }

    if( MFX_FOURCC_YUY2 == ptr->Info.FourCC )
    {
        /*roiSize.height = ptr->Info.Height;
        roiSize.width  = ptr->Info.Width >> 1;

        sts = ippiSet_32s_C1R(BLACK_CLR_YUY2, (Ipp32s*)(ptr->Data.Y), ptr->Data.Pitch, roiSize);*/
        //------------------------------------
        //       ROI #1 of frame (UP)
        //------------------------------------
        if( ptr->Info.CropY > 0 )
        {
            // (Y)
            roiSize.height = ptr->Info.CropY;
            roiSize.width  = ptr->Info.Width >> 1;

            sts = ippiSet_32s_C1R(BLACK_CLR_YUY2, (Ipp32s*)(ptr->Data.Y), ptr->Data.Pitch, roiSize);
            VPP_CHECK_IPP_STS( sts );
        }

        //------------------------------------
        //       ROI #2 of frame (BOTTOM)
        //------------------------------------
        if( ptr->Info.CropY + ptr->Info.CropH < ptr->Info.Height )
        {
             // (Y)
             roiSize.height = ptr->Info.Height - (ptr->Info.CropY + ptr->Info.CropH);
             roiSize.width  = ptr->Info.Width >> 1;

             int offset = (ptr->Info.CropY + ptr->Info.CropH) * ptr->Data.Pitch;
             sts = ippiSet_32s_C1R(BLACK_CLR_YUY2, (Ipp32s*)(ptr->Data.Y + offset), ptr->Data.Pitch, roiSize);
             VPP_CHECK_IPP_STS( sts );
        }

        //------------------------------------
        //       ROI #3 of frame (LEFT)
        //------------------------------------
        if( ptr->Info.CropX > 0 )
        {
             // (Y)
             roiSize.height = ptr->Info.CropH;
             roiSize.width  = ptr->Info.CropX >> 1;

             int offset = (ptr->Info.CropY) * ptr->Data.Pitch;
             sts = ippiSet_32s_C1R(BLACK_CLR_YUY2, (Ipp32s*)(ptr->Data.Y + offset), ptr->Data.Pitch, roiSize);
             VPP_CHECK_IPP_STS( sts );
        }

        //------------------------------------
        //       ROI #4 of frame (RIGHT)
        //------------------------------------
        if( ptr->Info.CropX + ptr->Info.CropW < ptr->Info.Width )
        {
             // (Y)
             roiSize.height = ptr->Info.CropH;
             roiSize.width  = ptr->Info.Width - (ptr->Info.CropX + ptr->Info.CropW);
             roiSize.width >>= 1;

             int offset = (ptr->Info.CropY) * ptr->Data.Pitch + (ptr->Info.CropX + ptr->Info.CropW)*2;// AYA correct?
             sts = ippiSet_32s_C1R(BLACK_CLR_YUY2, (Ipp32s*)(ptr->Data.Y + offset), ptr->Data.Pitch, roiSize);
             VPP_CHECK_IPP_STS( sts );
        }
    }
    else if( MFX_FOURCC_NV12 == ptr->Info.FourCC )
    {
        //------------------------------------
        //       ROI #1 of frame (UP)
        //------------------------------------
        if( ptr->Info.CropY > 0 )
        {
            // (Y)
            roiSize.height = ptr->Info.CropY;
            roiSize.width  = ptr->Info.Width;

            sts = ippiSet_8u_C1R(BLACK_CLR_Y, ptr->Data.Y, ptr->Data.Pitch, roiSize);
            VPP_CHECK_IPP_STS( sts );

            // (U/V)
            roiSize.height >>= 1;

            sts = ippiSet_8u_C1R(BLACK_CLR_UV, ptr->Data.UV, ptr->Data.Pitch, roiSize);
            VPP_CHECK_IPP_STS( sts );
        }

        //------------------------------------
        //       ROI #2 of frame (BOTTOM)
        //------------------------------------
        if( ptr->Info.CropY + ptr->Info.CropH < ptr->Info.Height )
        {
             // (Y)
             roiSize.height = ptr->Info.Height - (ptr->Info.CropY + ptr->Info.CropH);
             roiSize.width  = ptr->Info.Width;

             int offset = (ptr->Info.CropY + ptr->Info.CropH) * ptr->Data.Pitch;
             sts = ippiSet_8u_C1R(BLACK_CLR_Y, ptr->Data.Y + offset, ptr->Data.Pitch, roiSize);
             VPP_CHECK_IPP_STS( sts );

             // (U/V)
             roiSize.height  >>= 1;

             offset = ((ptr->Info.CropY + ptr->Info.CropH) >> 1) * ptr->Data.Pitch;
             sts = ippiSet_8u_C1R(BLACK_CLR_UV, ptr->Data.UV + offset, ptr->Data.Pitch, roiSize);
             VPP_CHECK_IPP_STS( sts );
        }

        //------------------------------------
        //       ROI #3 of frame (LEFT)
        //------------------------------------
        if( ptr->Info.CropX > 0 )
        {
             // (Y)
             roiSize.height = ptr->Info.CropH;
             roiSize.width  = ptr->Info.CropX;

             int offset = (ptr->Info.CropY) * ptr->Data.Pitch;
             sts = ippiSet_8u_C1R(BLACK_CLR_Y, ptr->Data.Y + offset, ptr->Data.Pitch, roiSize);
             VPP_CHECK_IPP_STS( sts );

             // (U/V)
             roiSize.height >>= 1;

             offset = (ptr->Info.CropY >> 1) * ptr->Data.Pitch;
             sts = ippiSet_8u_C1R(BLACK_CLR_UV, ptr->Data.UV + offset, ptr->Data.Pitch, roiSize);
             VPP_CHECK_IPP_STS( sts );
        }

        //------------------------------------
        //       ROI #4 of frame (RIGHT)
        //------------------------------------
        if( ptr->Info.CropX + ptr->Info.CropW < ptr->Info.Width )
        {
             // (Y)
             roiSize.height = ptr->Info.CropH;
             roiSize.width  = ptr->Info.Width - (ptr->Info.CropX + ptr->Info.CropW);

             int offset = (ptr->Info.CropY) * ptr->Data.Pitch + (ptr->Info.CropX + ptr->Info.CropW);
             sts = ippiSet_8u_C1R(BLACK_CLR_Y, ptr->Data.Y + offset, ptr->Data.Pitch, roiSize);
             VPP_CHECK_IPP_STS( sts );

             // (U/V)
             roiSize.height >>= 1;

             offset = (ptr->Info.CropY >> 1) * ptr->Data.Pitch + (ptr->Info.CropX + ptr->Info.CropW);
             sts = ippiSet_8u_C1R(BLACK_CLR_UV, ptr->Data.UV + offset, ptr->Data.Pitch, roiSize);
             VPP_CHECK_IPP_STS( sts );
        }
    }
    else if( MFX_FOURCC_RGB4 == ptr->Info.FourCC)
    {
        //------------------------------------
        //       ROI #1 of frame (UP)
        //------------------------------------
        mfxU8* ptrBGRA = IPP_MIN( IPP_MIN(ptr->Data.B, ptr->Data.G), ptr->Data.R );
        MFX_CHECK_NULL_PTR1(ptrBGRA);

        if( ptr->Info.CropY > 0 )
        {
            roiSize.height = ptr->Info.CropY;
            roiSize.width  = ptr->Info.Width*4;

            sts = ippiSet_8u_C1R(BLACK_CLR_RGB4, ptrBGRA, ptr->Data.Pitch, roiSize);
            VPP_CHECK_IPP_STS( sts );
        }

        //------------------------------------
        //       ROI #2 of frame (BOTTOM)
        //------------------------------------
        if( ptr->Info.CropY + ptr->Info.CropH < ptr->Info.Height )
        {
             roiSize.height = ptr->Info.Height - (ptr->Info.CropY + ptr->Info.CropH);
             roiSize.width  = ptr->Info.Width*4;

             int offset = (ptr->Info.CropY + ptr->Info.CropH) * ptr->Data.Pitch;
             sts = ippiSet_8u_C1R(BLACK_CLR_RGB4, ptrBGRA + offset, ptr->Data.Pitch, roiSize);
             VPP_CHECK_IPP_STS( sts );
        }

        //------------------------------------
        //       ROI #3 of frame (LEFT)
        //------------------------------------
        if( ptr->Info.CropX > 0 )
        {
             roiSize.height = ptr->Info.CropH;
             roiSize.width  = ptr->Info.CropX*4;

             int offset = (ptr->Info.CropY) * ptr->Data.Pitch;
             sts = ippiSet_8u_C1R(BLACK_CLR_RGB4, ptrBGRA + offset, ptr->Data.Pitch, roiSize);
             VPP_CHECK_IPP_STS( sts );
        }

        //------------------------------------
        //       ROI #4 of frame (RIGHT)
        //------------------------------------
        if( ptr->Info.CropX + ptr->Info.CropW < ptr->Info.Width )
        {
             roiSize.height = ptr->Info.CropH;
             roiSize.width  = ptr->Info.Width - (ptr->Info.CropX + ptr->Info.CropW);
             roiSize.width *= 4;

             int offset = (ptr->Info.CropY) * ptr->Data.Pitch + (ptr->Info.CropX + ptr->Info.CropW)*4;
             sts = ippiSet_8u_C1R(BLACK_CLR_RGB4, ptrBGRA + offset, ptr->Data.Pitch, roiSize);
             VPP_CHECK_IPP_STS( sts );
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus SetBackGroundColor(mfxFrameSurface1 *ptr)


// this function requires 0!= FrameRate
bool IsFrameRatesCorrespondITC(mfxU32  inFrameRateExtN, mfxU32  inFrameRateExtD,
                               mfxU32  outFrameRateExtN, mfxU32  outFrameRateExtD)
{
    //under investigation. now we decided ITC is enabled 29.97(+/-)0.1 -> 23.976(+/-)0.1
    const mfxF64 EPS = 0.1;
    const mfxF64 IN_FRAME_RATE_ETALON  = 30.0 * 1000.0 / 1001.0;
    const mfxF64 OUT_FRAME_RATE_ETALON = 24.0 * 1000.0 / 1001.0;

    mfxF64 inFrameRate  = CalculateUMCFramerate(inFrameRateExtN,  inFrameRateExtD);
    mfxF64 outFrameRate = CalculateUMCFramerate(outFrameRateExtN, outFrameRateExtD);

    if( fabs(IN_FRAME_RATE_ETALON - inFrameRate) < EPS && fabs(OUT_FRAME_RATE_ETALON - outFrameRate) < EPS )
    {
        return true;
    }

    return false;

} // bool IsFrameRatesCorrespondITC(...)

// this function requires 0!= FrameRate
bool IsFrameRatesCorrespondDI(mfxU32  inFrameRateExtN,  mfxU32  inFrameRateExtD,
                              mfxU32  outFrameRateExtN, mfxU32  outFrameRateExtD,
                              mfxU32* mode)
{
    const mfxU32 RATIO_FOR_SINGLE_FIELD_PROCESSED = 1;
    const mfxU32 RATIO_FOR_BOTH_FIELDS_PROCESSED  = 2;

    // convert to internal mfx frame rates range
    mfxF64 inFrameRate  = CalculateUMCFramerate(inFrameRateExtN,  inFrameRateExtD);
    mfxF64 outFrameRate = CalculateUMCFramerate(outFrameRateExtN, outFrameRateExtD);

    CalculateMFXFramerate(inFrameRate,  &inFrameRateExtN,  &inFrameRateExtD);
    CalculateMFXFramerate(outFrameRate, &outFrameRateExtN, &outFrameRateExtD);

    if( inFrameRateExtD != outFrameRateExtD )
    {
        return false;
    }

    mfxU32 residue = outFrameRateExtN % inFrameRateExtN;
    mfxU32 ratio   = outFrameRateExtN / inFrameRateExtN;

    if( (inFrameRateExtD == outFrameRateExtD) && (0 == residue) &&
        ( (RATIO_FOR_SINGLE_FIELD_PROCESSED == ratio) || ( RATIO_FOR_BOTH_FIELDS_PROCESSED == ratio) ) )
    {
        if( NULL != mode )
        {
            *mode = ratio-1;
        }

        return true;
    }

    return false;

} // bool IsFrameRatesCorrespondDI(...)

bool IsFrameRatesCorrespondMode30i60p(mfxU32  inFrameRateExtN,  mfxU32  inFrameRateExtD,
                                 mfxU32  outFrameRateExtN, mfxU32  outFrameRateExtD)
{
    mfxU32 modeAdvDI;
    bool bResult = IsFrameRatesCorrespondDI(inFrameRateExtN,  inFrameRateExtD,
                                            outFrameRateExtN, outFrameRateExtD, &modeAdvDI);

    if( bResult && modeAdvDI )
    {
        return true;
    }

    return false;

} // bool IsFrameRatesCorrespondMode60i60p(...)

bool IsFrameRatesCorrespondDIorITC(mfxU32  inFrameRateExtN,  mfxU32  inFrameRateExtD,
                                   mfxU32  outFrameRateExtN, mfxU32  outFrameRateExtD)
{
    bool isITCRespond = IsFrameRatesCorrespondITC(inFrameRateExtN, inFrameRateExtD,
                                                  outFrameRateExtN, outFrameRateExtD);

    bool isDIRespond  = IsFrameRatesCorrespondDI(inFrameRateExtN, inFrameRateExtD,
                                                 outFrameRateExtN, outFrameRateExtD, NULL);

    if( isITCRespond || isDIRespond )
    {
        return true;
    }

    return false;

} // bool IsFrameRatesCorrespondDIorITC(...)

mfxStatus GetFilterParam(mfxVideoParam* par, mfxU32 filterName, mfxExtBuffer** ppHint)
{
    MFX_CHECK_NULL_PTR1( par );
    MFX_CHECK_NULL_PTR1( ppHint );

    *ppHint = NULL;

    if( par->ExtParam && par->NumExtParam > 0 )
    {
        mfxU32 paramIndex;

        for( paramIndex = 0; paramIndex < par->NumExtParam; paramIndex++ )
        {
            if( filterName == par->ExtParam[paramIndex]->BufferId )
            {
                *ppHint = par->ExtParam[paramIndex];
                break;
            }
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus GetFilterParam(mfxVideoParam* par, mfxU32 filterName, mfxExtBuffer** ppHint)


bool IsROIConstant(mfxFrameSurface1* pSrc1, mfxFrameSurface1* pSrc2, mfxFrameSurface1* pSrc3)
{
    bool bROIConstant[4] = {false}; // cropX/Y/W/H
    bool bResult;

    // cropX
    if( (pSrc1->Info.CropX == pSrc2->Info.CropX) && (pSrc2->Info.CropX == pSrc3->Info.CropX) )
    {
        bROIConstant[0] = true;
    }

    // cropY
    if( (pSrc1->Info.CropY == pSrc2->Info.CropY) && (pSrc2->Info.CropY == pSrc3->Info.CropY) )
    {
        bROIConstant[1] = true;
    }

    // cropW
    if( (pSrc1->Info.CropW == pSrc2->Info.CropW) && (pSrc2->Info.CropW == pSrc3->Info.CropW) )
    {
        bROIConstant[2] = true;
    }

    // cropH
    if( (pSrc1->Info.CropH == pSrc2->Info.CropH) && (pSrc2->Info.CropH == pSrc3->Info.CropH) )
    {
        bROIConstant[3] = true;
    }

    // make decision
    bResult = bROIConstant[0] && bROIConstant[1] && bROIConstant[2] && bROIConstant[3];

    return bResult;

} // bool IsROIConstant( ... )


bool IsRoiDifferent(mfxFrameSurface1 *input, mfxFrameSurface1 *output)
{
    if ((input->Info.Width == output->Info.Width && input->Info.Height == output->Info.Height) &&
        (input->Info.CropW == output->Info.CropW && input->Info.CropH == output->Info.CropH) &&
        (input->Info.CropX == output->Info.CropX && input->Info.CropY == output->Info.CropY) &&
        (input->Info.PicStruct == output->Info.PicStruct)
        )
    {
        return false;
    }

    return true;

} // bool IsRoiDifferent(mfxFrameSurface1 *input, mfxFrameSurface1 *output)

void ShowPipeline( std::vector<mfxU32> pipelineList )
{
#ifdef _DEBUG
#if defined (_WIN32) || defined(_WIN64)
    mfxU32 filterIndx;
    char cStr[256];

    sprintf_s(cStr, sizeof(cStr), "\nVPP PIPELINE: \n");
    OutputDebugStringA(cStr);

    for( filterIndx = 0; filterIndx < pipelineList.size(); filterIndx++ )
    {
        switch( pipelineList[filterIndx] )
        {
            case (mfxU32)MFX_EXTBUFF_VPP_DENOISE:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "DENOISE");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_RSHIFT_IN:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "MFX_EXTBUFF_VPP_RSHIFT_IN");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_RSHIFT_OUT:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "MFX_EXTBUFF_VPP_RSHIFT_OUT");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_LSHIFT_IN:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "MFX_EXTBUFF_VPP_LSHIFT_IN");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_LSHIFT_OUT:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "MFX_EXTBUFF_VPP_LSHIFT_OUT");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_RESIZE:
            {
                 sprintf_s(cStr, sizeof(cStr), "%s \n", "RESIZE");
                 OutputDebugStringA(cStr);
                 break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "FRC");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DI_30i60p:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "ADV DI");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_ITC:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "ITC");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DI:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "DI");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_CSC:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "CSC_NV12");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_CSC_OUT_RGB4:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "CSC_YUY2");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_CSC_OUT_A2RGB10:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "CSC_A2RGB10");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_SCENE_ANALYSIS:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "SA");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_PROCAMP:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "PROCAMP");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DETAIL:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "DETAIL");
                OutputDebugStringA(cStr);
                break;
            }

#if defined(MFX_ENABLE_IMAGE_STABILIZATION_VPP)
            case (mfxU32)MFX_EXTBUFF_VPP_IMAGE_STABILIZATION:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "IMAGE_STAB");
                OutputDebugStringA(cStr);
                break;
            }
#endif

            case (mfxU32)MFX_EXTBUFF_VPP_VARIANCE_REPORT:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "VARIANCE_REP");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_COMPOSITE:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "COMPOSITE");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_FIELD_PROCESSING:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "VPP_FIELD_PROCESSIN");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_ROTATION:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "MFX_EXTBUFF_VPP_ROTATION");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_SCALING:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "MFX_EXTBUFF_VPP_SCALING");
                OutputDebugStringA(cStr);
                break;
            }

             case (mfxU32)MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO");
                OutputDebugStringA(cStr);
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_MIRRORING:
            {
                sprintf_s(cStr, sizeof(cStr), "%s \n", "MFX_EXTBUFF_VPP_MIRRORING");
                OutputDebugStringA(cStr);
                break;
            }

            default:
            {
            }

        }// CASE
    } //end of filter search

    sprintf_s(cStr, sizeof(cStr), "\n");
    OutputDebugStringA(cStr);
#endif // #if defined(_WIN32) || defined(_WIN64)

#if defined(LINUX) || defined(LINUX32) || defined(LINUX64)
    mfxU32 filterIndx;
    fprintf(stderr, "VPP PIPELINE: \n");

    for( filterIndx = 0; filterIndx < pipelineList.size(); filterIndx++ )
    {
        switch( pipelineList[filterIndx] )
        {
            case (mfxU32)MFX_EXTBUFF_VPP_DENOISE:
            {
                fprintf(stderr, "DENOISE \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_RSHIFT_IN:
            {
                fprintf(stderr, "MFX_EXTBUFF_VPP_RSHIFT_IN \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_RSHIFT_OUT:
            {
                fprintf(stderr, "MFX_EXTBUFF_VPP_RSHIFT_OUT \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_LSHIFT_IN:
            {
                fprintf(stderr, "MFX_EXTBUFF_VPP_LSHIFT_IN \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_LSHIFT_OUT:
            {
                fprintf(stderr, "MFX_EXTBUFF_VPP_LSHIFT_OUT \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_RESIZE:
            {
                 fprintf(stderr, "RESIZE \n");
                 break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
            {
                fprintf(stderr, "FRC \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DI_30i60p:
            {
                fprintf(stderr, "ADV DI 30i60p \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_ITC:
            {
                fprintf(stderr, "ITC \n");

                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DI:
            {
                fprintf(stderr, "DI \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DEINTERLACING:
            {
                fprintf(stderr, "DI EXT BUF\n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_CSC:
            {
                fprintf(stderr, "CSC_NV12 \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_CSC_OUT_RGB4:
            {
                fprintf(stderr,"%s \n", "CSC_YUY2");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_CSC_OUT_A2RGB10:
            {
                fprintf(stderr, "CSC_A2RGB10 \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_SCENE_ANALYSIS:
            {
                fprintf(stderr, "SA \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_PROCAMP:
            {
                fprintf(stderr, "PROCAMP \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DETAIL:
            {
                fprintf(stderr, "DETAIL \n");
                break;
            }

#if defined(MFX_ENABLE_IMAGE_STABILIZATION_VPP)
            case (mfxU32)MFX_EXTBUFF_VPP_IMAGE_STABILIZATION:
            {
                fprintf(stderr, "IMAGE_STAB \n");
                break;
            }
#endif

            case (mfxU32)MFX_EXTBUFF_VPP_VARIANCE_REPORT:
            {
                fprintf(stderr, "VARIANCE_REP \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_COMPOSITE:
            {
                fprintf(stderr, "COMPOSITE \n");
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_FIELD_PROCESSING:
            {
                fprintf(stderr, "VPP_FIELD_PROCESSING \n");
                break;
            }
            case (mfxU32)MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO:
            {
                fprintf(stderr, "VPP_VIDEO_SIGNAL_INFO\n");
                break;
            }
            default:
            {
                fprintf(stderr, "UNKNOUW Filter ID!!! \n");
                break;
            }

        }// CASE
    } //end of filter search

    //fprintf(stderr,"\n");
#endif // #if defined(LINUX) || defined(LINUX32) || defined(LINUX64)

#endif //#ifdef _DEBUG
    return;

} // void ShowPipeline( std::vector<mfxU32> pipelineList )


/* ********************************************************************************************** */
/*                       Pipeline Building Stage                                                  */
/* ********************************************************************************************** */
/* VPP best quality is |CSC| + |DN| + |DI| + |IS| + |RS| + |Detail| + |ProcAmp| + |FRC| + |SA| */
/* SW_VPP reorder |FRC| to meet best speed                                                        */
/* ********************************************************************************************** */

void ReorderPipelineListForQuality( std::vector<mfxU32> & pipelineList )
{
    //mfxU32 newList[MAX_NUM_VPP_FILTERS] = {0};
    std::vector<mfxU32> newList;
    newList.resize( pipelineList.size() );
    mfxU32 index = 0;

    // [-1] Shift is very first, since shifted content is not supported by VPP
    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_RSHIFT_IN ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_RSHIFT_IN;
        index++;
    }

    // [0] canonical order
    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_CSC ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_CSC;
        index++;
    }
    // Resize for Best Speed
    /*if( IsFilterFound( pList, len, MFX_EXTBUFF_VPP_RESIZE ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_RESIZE;
        index++;
    }*/
    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_DENOISE ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_DENOISE;
        index++;
    }
    // DI, advDI, ITC has the same priority
    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_DI ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_DI;
        index++;
    }
    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_DI_30i60p ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_DI_30i60p;
        index++;
    }
    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_ITC ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_ITC;
        index++;
    }
    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_DEINTERLACING ) &&
      ! IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_DI_30i60p     ) &&
      ! IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_DI            ))
    {
        newList[index] = MFX_EXTBUFF_VPP_DEINTERLACING;
        index++;
    }
    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO;
        index++;
    }

    /* [IStab] FILTER */
#if defined(MFX_ENABLE_IMAGE_STABILIZATION_VPP)
    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_IMAGE_STABILIZATION ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_IMAGE_STABILIZATION;
        index++;
    }
#endif

    // Resize for Best Quality
    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_RESIZE ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_RESIZE;
        index++;
    }

    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_DETAIL ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_DETAIL;
        index++;
    }

    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_PROCAMP ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_PROCAMP;
        index++;
    }

    /* [VarianceRep] FILTER */
    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_VARIANCE_REPORT ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_VARIANCE_REPORT;
        index++;
    }

    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION;
        index++;
    }

    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_SCENE_ANALYSIS ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS;
        index++;
    }

    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_CSC_OUT_RGB4 ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_CSC_OUT_RGB4;
        index++;
    }

    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_CSC_OUT_A2RGB10 ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_CSC_OUT_A2RGB10;
        index++;
    }

    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_COMPOSITE ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_COMPOSITE;
        index++;
    }
	
	if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_FIELD_PROCESSING ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_FIELD_PROCESSING;
        index++;
    }

    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_LSHIFT_OUT ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_LSHIFT_OUT;
        index++;
    }
    
    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_ROTATION ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_ROTATION;
        index++;
    }

    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_SCALING ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_SCALING;
        index++;
    }

    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_MIRRORING ) )
    {
        newList[index] = MFX_EXTBUFF_VPP_MIRRORING;
        index++;
    }
    // [1] update
    pipelineList.resize(index);
    for( index = 0; index < (mfxU32)pipelineList.size(); index++ )
    {
        pipelineList[index] = newList[index];
    }

} // void ReorderPipelineListForQuality(std::vector<mfxU32> & pipelineList)


void ReorderPipelineListForSpeed(
    mfxVideoParam* videoParam,
    std::vector<mfxU32> & pipelineList)
{
// aya: disabled perf optimization since MSDK3.0 beta5
#if 0
    // optimization in case of RS
    if( IsFilterFound( pList, len, MFX_EXTBUFF_VPP_RESIZE ) )
    {
        mfxF64 ratioW = (mfxF64)videoParam->vpp.Out.Width / (mfxF64)videoParam->vpp.In.Width;
        mfxF64 ratioH = (mfxF64)videoParam->vpp.Out.Height / (mfxF64)videoParam->vpp.In.Height;

        if( ratioW < 1.0 && ratioH < 1.0 )
        {

            mfxU32 filterIndex = 0;
            mfxU32 filterIndexRS = GetFilterIndex(pList, len, MFX_EXTBUFF_VPP_RESIZE);

            // RESIZE_DOWNSCALE must be first filter in pipeline
            for( filterIndex = filterIndexRS; filterIndex > 0; filterIndex-- )
            {
                VPP_SWAP(pList[filterIndex], pList[filterIndex-1]);
            }
            //exclude CSC
            if( IsFilterFound( pList, len, MFX_EXTBUFF_VPP_CSC ) )
            {
                VPP_SWAP(pList[1], pList[0]);
            }
        }
    }
#endif

#if 1
    // optimization in case of FRC
    if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION ) )
    {
        mfxFrameInfo* in  = &(videoParam->vpp.In);
        mfxFrameInfo* out = &(videoParam->vpp.Out);

        mfxF64 inFrameRate  = CalculateUMCFramerate(in->FrameRateExtN,  in->FrameRateExtD);
        mfxF64 outFrameRate = CalculateUMCFramerate(out->FrameRateExtN, out->FrameRateExtD);

        mfxU32 filterIndex = 0;
        mfxU32 filterIndexFRC = GetFilterIndex(&pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION);

        if( inFrameRate > outFrameRate )
        {
            // FRC_DOWN must be first filter in pipeline
            for( filterIndex = filterIndexFRC; filterIndex > 0; filterIndex-- )
            {
                std::swap(pipelineList[filterIndex], pipelineList[filterIndex-1]);
            }
            //exclude CSC
            if( IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_CSC ) )
            {
                std::swap(pipelineList[1], pipelineList[0]);
            }
        }
    }
#endif

} // void ReorderPipelineListForSpeed(mfxVideoParam* videoParam, std::vector<mfxU32> & pipelineList)


mfxStatus GetPipelineList(
    mfxVideoParam* videoParam,
    //mfxU32* pList,
    std::vector<mfxU32> & pipelineList,
    //mfxU32* pLen,
    bool    bExtended)
{
    mfxInfoVPP*   par = NULL;
    mfxFrameInfo* srcFrameInfo = NULL;
    mfxFrameInfo* dstFrameInfo = NULL;
    mfxU16  srcW = 0, dstW = 0;
    mfxU16  srcH = 0, dstH = 0;
    //mfxU32  lenList = 0;

    MFX_CHECK_NULL_PTR1( videoParam );

    //MFX_CHECK_NULL_PTR2( pList, pLen );

    par = &(videoParam->vpp);
    srcFrameInfo = &(par->In);
    dstFrameInfo = &(par->Out);
    /* ************************************************************************** */
    /* [1] the filter chain first based on input and output mismatch formats only */
    /* ************************************************************************** */
    if( (MFX_FOURCC_RGB4 != par->In.FourCC) || (MFX_FOURCC_RGB4 != par->Out.FourCC) )
    {
        switch (par->In.FourCC)
        {
        case MFX_FOURCC_P210:
             switch (par->Out.FourCC)
            {
            case MFX_FOURCC_A2RGB10:
                pipelineList.push_back(MFX_EXTBUFF_VPP_CSC_OUT_A2RGB10);
                break;
            case MFX_FOURCC_NV12:
            case MFX_FOURCC_NV16:
            case MFX_FOURCC_P010:
                pipelineList.push_back(MFX_EXTBUFF_VPP_CSC);
                break;
            }
            break;
        case MFX_FOURCC_P010:
            switch (par->Out.FourCC)
            {
            case MFX_FOURCC_A2RGB10:
                pipelineList.push_back(MFX_EXTBUFF_VPP_CSC_OUT_A2RGB10);
                break;
            case MFX_FOURCC_NV12:
            case MFX_FOURCC_P210:
                pipelineList.push_back(MFX_EXTBUFF_VPP_CSC);
                break;
            }
            break;

        case MFX_FOURCC_NV12:
            switch (par->Out.FourCC)
            {
            case MFX_FOURCC_NV16:
            case MFX_FOURCC_P010:
                pipelineList.push_back(MFX_EXTBUFF_VPP_CSC);
                break;
            case MFX_FOURCC_RGB4:
                pipelineList.push_back(MFX_EXTBUFF_VPP_CSC_OUT_RGB4);
                break;
            }
            break;
        default:
            switch (par->Out.FourCC)
            {
            case MFX_FOURCC_RGB4:
                pipelineList.push_back(MFX_EXTBUFF_VPP_CSC_OUT_RGB4);
                break;
            }
            break;
        }

        if( MFX_FOURCC_NV12 != par->In.FourCC && MFX_FOURCC_P010 != par->In.FourCC && MFX_FOURCC_P210 != par->In.FourCC)
        {
            /* [Color Space Conversion] FILTER */
            pipelineList.push_back(MFX_EXTBUFF_VPP_CSC);
        }

    }
    else if (!bExtended)
    {
        /* ********************************************************************** */
        /* aya: hack                                                              */
        /* RGB32->RGB32 (resize only) is supported to meet MSDK 3.0 requirements  */
        /* ********************************************************************** */
        pipelineList.push_back(MFX_EXTBUFF_VPP_RESIZE);

        return MFX_ERR_NONE;
    }

    /* VPP natively supports 10bit format w/o shift. If input is shifted,
     * need get it back to normal position.
     */
    if ( ( MFX_FOURCC_P010 == srcFrameInfo->FourCC || MFX_FOURCC_P210 == srcFrameInfo->FourCC)
        && srcFrameInfo->Shift )
    {
        pipelineList.push_back(MFX_EXTBUFF_VPP_RSHIFT_IN);
    }

    /*
     * VPP produces 10bit data w/o shift. If output is requested to be shifted, need to do so
     */
    if ( ( MFX_FOURCC_P010 == dstFrameInfo->FourCC || MFX_FOURCC_P210 == dstFrameInfo->FourCC)
        && dstFrameInfo->Shift )
    {
        pipelineList.push_back(MFX_EXTBUFF_VPP_LSHIFT_OUT);
    }

    /* [Resize] FILTER */
    VPP_GET_REAL_WIDTH(  srcFrameInfo, srcW);
    VPP_GET_REAL_HEIGHT( srcFrameInfo, srcH);

    /* OUT */
    VPP_GET_REAL_WIDTH( dstFrameInfo, dstW);
    VPP_GET_REAL_HEIGHT(dstFrameInfo, dstH);

    { //resize or cropping
        pipelineList.push_back(MFX_EXTBUFF_VPP_RESIZE);
    }

    /* [Deinterlace] FILTER */
    mfxU32 extParamCount        = IPP_MAX(sizeof(g_TABLE_CONFIG) / sizeof(*g_TABLE_CONFIG), videoParam->NumExtParam);
    std::vector<mfxU32> extParamList(extParamCount);

    GetConfigurableFilterList( videoParam, &extParamList[0], &extParamCount );

    mfxU32*   pExtBufList = NULL;
    mfxU32    extBufCount = 0;

    if( 0 != videoParam->NumExtParam && NULL == videoParam->ExtParam )
    {
        return MFX_ERR_NULL_PTR;
    }

    GetDoUseFilterList( videoParam, &pExtBufList, &extBufCount );

    extParamList.insert(extParamList.end(), &pExtBufList[0], &pExtBufList[extBufCount]);
    extParamCount = (mfxU32) extParamList.size();

    PicStructMode picStructMode = GetPicStructMode(par->In.PicStruct, par->Out.PicStruct);

    mfxI32 deinterlacingMode = 0;
    // look for user defined deinterlacing mode
    for (mfxU32 i = 0; i < videoParam->NumExtParam; i++)
    {
        if (videoParam->ExtParam[i] && videoParam->ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DEINTERLACING)
        {
            mfxExtVPPDeinterlacing* extDI = (mfxExtVPPDeinterlacing*) videoParam->ExtParam[i];
            /* MSDK ignored all any DI modes values except two defined:
             * MFX_DEINTERLACING_ADVANCED && MFX_DEINTERLACING_BOB
             * If DI mode in Ext Buffer is not related BOB or ADVANCED Ext buffer ignored
             * */
            if (extDI->Mode == MFX_DEINTERLACING_ADVANCED ||
#if defined (MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP)
                extDI->Mode == MFX_DEINTERLACING_ADVANCED_SCD ||
#endif
                extDI->Mode == MFX_DEINTERLACING_BOB ||
                extDI->Mode == MFX_DEINTERLACING_ADVANCED_NOREF)
            {
                /* DI Ext buffer present
                 * and DI type is correct
                 * */
                deinterlacingMode = extDI->Mode;
            }
            break;
        }
    }
    /* DI configuration cases:
     * Default "-spic 0 -dpic 1" (TFF to progressive ) -> MFX_EXTBUFF_VPP_DI
     * Default "-spic 0 -dpic 1 -sf 30 -df 60" -> MFX_EXTBUFF_VPP_DI_30i60p
     * !!! in both cases above, DI mode, ADVANCED(ADI) or BOB will be selected later, by driver's caps analysis.
     * If ADI reported in driver's Caps MSDK should select ADI mode
     * */

    if ((DYNAMIC_DI_PICSTRUCT_MODE == picStructMode ) || /* configuration via "-spic 0 -spic 1" */
        (0 != deinterlacingMode) ) /* configuration via Ext Buf */
    {
        if( IsFrameRatesCorrespondMode30i60p(par->In.FrameRateExtN,
                                        par->In.FrameRateExtD,
                                        par->Out.FrameRateExtN,
                                        par->Out.FrameRateExtD) )
        {
            pipelineList.push_back(MFX_EXTBUFF_VPP_DI_30i60p);
        }
        else if( IsFrameRatesCorrespondITC(par->In.FrameRateExtN,
                                           par->In.FrameRateExtD,
                                           par->Out.FrameRateExtN,
                                           par->Out.FrameRateExtD) )
        {
            pipelineList.push_back(MFX_EXTBUFF_VPP_ITC);
        }
        else if (0 != deinterlacingMode)
        {
            /* Put DI filter in pipeline only if filter configured via Ext buffer */
            pipelineList.push_back(MFX_EXTBUFF_VPP_DEINTERLACING);
        }
        else if (DYNAMIC_DI_PICSTRUCT_MODE == picStructMode)
        {
            /* Put DI filter in pipeline only if filter configured via default way "-spic 0 -dpic 1" */
            pipelineList.push_back(MFX_EXTBUFF_VPP_DI);
        }

    }

    /* ********************************************************************** */
    /* 2. optional filters, enabled by default, disabled by DO_NOT_USE        */
    /* ********************************************************************** */

    // DO_NOT_USE structure is ignored by VPP since MSDK 3.0

    /* *************************************************************************** */
    /* 3. optional filters, disabled by default, enabled by DO_USE                 */
    /* *************************************************************************** */
    mfxU32*   pExtList = NULL;
    mfxU32    extCount = 0;

    GetDoUseFilterList( videoParam, &pExtList, &extCount );

    /* [Core Frame Rate Conversion] FILTER */
    /* must be used AFTER [Deinterlace] FILTER !!! due to SW performance specific */
    if( !IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_DI_30i60p ) &&
        !IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_ITC ) )
    {
        if( IsFilterFound( pExtList, extCount, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION ) ||
            ( par->In.FrameRateExtN * par->Out.FrameRateExtD != par->Out.FrameRateExtN * par->In.FrameRateExtD ) )
        {
            pipelineList.push_back(MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION);
        }
    }

    mfxU32 searchCount = sizeof(g_TABLE_DO_USE) / sizeof(*g_TABLE_DO_USE);
    mfxU32 fCount      = extCount;
    mfxU32 fIdx = 0;
    for(fIdx = 0; fIdx < fCount; fIdx++)
    {
        if( IsFilterFound( &g_TABLE_DO_USE[0], searchCount, pExtList[fIdx] ) && !IsFilterFound(&pipelineList[0], (mfxU32)pipelineList.size(), pExtList[fIdx]) )
        {
            pipelineList.push_back( pExtList[fIdx] );
        }
    }


    /* *************************************************************************** */
    /* 4. optional filters, disabled by default, enabled by EXT_BUFFER             */
    /* *************************************************************************** */
    mfxU32 configCount = IPP_MAX(sizeof(g_TABLE_CONFIG) / sizeof(*g_TABLE_CONFIG), videoParam->NumExtParam);
    std::vector<mfxU32> configList(configCount);

    GetConfigurableFilterList( videoParam, &configList[0], &configCount );

    /* [FrameRateConversion] FILTER */
    if( IsFilterFound( &configList[0], configCount, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION ) && !IsFilterFound(&pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION) )
    {
        if( !IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_DI_30i60p ) && !IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_ITC ) )
        {
            pipelineList.push_back( MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION );
        }
    }

    /* ROTATION FILTER */
    if( IsFilterFound( &configList[0], configCount, MFX_EXTBUFF_VPP_ROTATION ) && !IsFilterFound(&pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_ROTATION) )
    {
        if( !IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_ROTATION ) )
        {
            pipelineList.push_back( MFX_EXTBUFF_VPP_ROTATION );
        }
    }

    if( IsFilterFound( &configList[0], configCount, MFX_EXTBUFF_VPP_SCALING ) && !IsFilterFound(&pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_SCALING) )
    {
        if( !IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_SCALING ) )
        {
            pipelineList.push_back( MFX_EXTBUFF_VPP_SCALING );
        }
    }

    if( IsFilterFound( &configList[0], configCount, MFX_EXTBUFF_VPP_MIRRORING ) && !IsFilterFound(&pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_MIRRORING) )
    {
        if( !IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_MIRRORING ) )
        {
            pipelineList.push_back( MFX_EXTBUFF_VPP_MIRRORING );
        }
    }

    if( IsFilterFound( &configList[0], configCount, MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO ) && !IsFilterFound(&pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO) )
    {
        if( !IsFilterFound( &pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO ) )
        {
            pipelineList.push_back( MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO );
        }
    }

    searchCount = sizeof(g_TABLE_CONFIG) / sizeof(*g_TABLE_CONFIG);
    fCount      = configCount;
    for(fIdx = 0; fIdx < fCount; fIdx++)
    {
        if( IsFilterFound( g_TABLE_CONFIG, searchCount, configList[fIdx] ) &&
                !IsFilterFound(&pipelineList[0], (mfxU32)pipelineList.size(), configList[fIdx]) )
        {
            /* Add filter to the list.
             * Don't care about duplicates, they will be eliminated by Reorder... calls below 
             */
            pipelineList.push_back(configList[fIdx]);
        } /* if( IsFilterFound( g_TABLE_CONFIG */
    } /*for(fIdx = 0; fIdx < fCount; fIdx++)*/

    /* *************************************************************************** */
    /* 5. reordering for speed/quality                                             */
    /* *************************************************************************** */
    if( pipelineList.size() > 1 )
    {
        ReorderPipelineListForQuality(pipelineList);
        ReorderPipelineListForSpeed(videoParam, pipelineList);
    }

    if( pipelineList.size() > 0 )
    {
        ShowPipeline(pipelineList);
    }
    return ( ( pipelineList.size() > 0 ) ? MFX_ERR_NONE : MFX_ERR_INVALID_VIDEO_PARAM );

} // mfxStatus GetPipelineList(mfxVideoParam* videoParam, std::vector<mfxU32> pipelineList)


bool IsFilterFound( const mfxU32* pList, mfxU32 len, mfxU32 filterName )
{
    mfxU32 i;

    if( 0 == len )
    {
        return false;
    }

    for( i = 0; i < len; i++ )
    {
        if( filterName == pList[i] )
        {
            return true;
        }
    }

    return false;

} // bool IsFilterFound( mfxU32* pList, mfxU32 len, mfxU32 filterName )

// function requires filterName belong pList. all check must be done before
mfxU32 GetFilterIndex( mfxU32* pList, mfxU32 len, mfxU32 filterName )
{
    mfxU32 filterIndex;

    for( filterIndex = 0; filterIndex < len; filterIndex++ )
    {
        if( filterName == pList[filterIndex] )
        {
            return filterIndex;
        }
    }

    return 0;

} // mfxU32 GetFilterIndex( mfxU32* pList, mfxU32 len, mfxU32 filterName )

mfxStatus CheckProtectedMode( mfxU16 mode )
{
    mfxStatus sts = MFX_ERR_INVALID_VIDEO_PARAM;

    if( 0 == mode )
    {
        sts = MFX_ERR_NONE;
    }

    #ifdef MFX_VA
        // hardware protected section
        if (MFX_PROTECTION_PAVP == mode || 0 == mode)
        {
            sts = MFX_ERR_NONE;
        }
        else
        {
            sts = MFX_ERR_INVALID_VIDEO_PARAM;
        }
    #endif

    return sts;

} // mfxStatus CheckProtectedMode( mfxU16 mode )

/* check each field of FrameInfo excluding PicStruct */
mfxStatus CheckFrameInfo(mfxFrameInfo* info, mfxU32 request)
{
    mfxStatus mfxSts = MFX_ERR_NONE;

    /* FourCC */
    switch (info->FourCC)
    {
        case MFX_FOURCC_NV12:
        case MFX_FOURCC_RGB4:
        case MFX_FOURCC_P010:
        case MFX_FOURCC_P210:
        case MFX_FOURCC_NV16:
        case MFX_FOURCC_YUY2:
            break;
        case MFX_FOURCC_IMC3:
        case MFX_FOURCC_YV12:
        case MFX_FOURCC_YUV400:
        case MFX_FOURCC_YUV411:
        case MFX_FOURCC_YUV422H:
        case MFX_FOURCC_YUV422V:
        case MFX_FOURCC_YUV444:
#if defined(MFX_VA_LINUX)
        // UYVY is supported on Linux only
        case MFX_FOURCC_UYVY:
#endif
            if (VPP_OUT == request)
                return MFX_ERR_INVALID_VIDEO_PARAM;
            break;
        case MFX_FOURCC_A2RGB10:
            // 10bit RGB supported as output format only
            if (VPP_IN == request)
                return MFX_ERR_INVALID_VIDEO_PARAM;
            
            break;
        default:
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    /* Picture Size */
    if( 0 == info->Width || 0 == info->Height )
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ( (info->Width & 15 ) != 0 )
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

#if 0
    /* AspectRatio */
    if ((info->AspectRatioW || info->AspectRatioH) && !(info->AspectRatioW && info->AspectRatioH))
        return MFX_ERR_INVALID_VIDEO_PARAM;
#endif

    /* Frame Rate */
    if (0 == info->FrameRateExtN || 0 == info->FrameRateExtD)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    /* checking Height based on PicStruct filed */
    if (MFX_PICSTRUCT_PROGRESSIVE & info->PicStruct)
    {
        if ((info->Height  & 15) !=0)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }
    else if( MFX_PICSTRUCT_FIELD_BFF & info->PicStruct ||
        MFX_PICSTRUCT_FIELD_TFF & info->PicStruct ||
        (MFX_PICSTRUCT_UNKNOWN   == info->PicStruct))
    {
        if ((info->Height  & 15) !=0 ) // in according with internal spec (ISV3b-SNBa rev. 22583)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }
    else//error protection
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return mfxSts;

} // mfxStatus CheckFrameInfo(mfxFrameInfo* info, mfxU32 request)

mfxStatus CompareFrameInfo(mfxFrameInfo* info1, mfxFrameInfo* info2)
{
    MFX_CHECK_NULL_PTR2( info1, info2 );

    if( info1->FourCC != info2->FourCC )
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if( info1->Width < info2->Width )
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if( info1->Height < info2->Height )
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    return MFX_ERR_NONE;

} // mfxStatus CompareFrameInfo(mfxFrameInfo* info1, mfxFrameInfo* info2)

mfxStatus CheckCropParam( mfxFrameInfo* info )
{
    // in according with spec CropW/H are mandatory for VPP
    if( 0 == info->CropH || 0 == info->CropW )
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (info->CropX > info->Width)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (info->CropY > info->Height)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (info->CropX + info->CropW > info->Width)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (info->CropY + info->CropH > info->Height)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;

} // mfxStatus CheckCropParam( mfxFrameInfo* info )

mfxU16 vppMax_16u(const mfxU16* pSrc, int len)
{
    mfxU16 maxElement = 0;

    for (int indx = 0 ; indx < len; indx++)
    {
        if ( pSrc[indx] > maxElement )
        {
            maxElement = pSrc[indx];
        }
    }
    return maxElement;

} // mfxU16 vppMax_16u(const mfxU16* pSrc, int len)

/* ********************************************* */
/* utility for parsering of VPP optional filters */
/* ********************************************* */

void GetDoNotUseFilterList( mfxVideoParam* par, mfxU32** ppList, mfxU32* pLen )
{
    mfxU32 i = 0;
    mfxExtVPPDoNotUse* pVPPHint = NULL;

    /* robustness */
    *ppList = NULL;
    *pLen = 0;

    for( i = 0; i < par->NumExtParam; i++ )
    {
        if( MFX_EXTBUFF_VPP_DONOTUSE == par->ExtParam[i]->BufferId )
        {
            pVPPHint  = (mfxExtVPPDoNotUse*)(par->ExtParam[i]);
            *ppList = pVPPHint->AlgList;
            *pLen  = pVPPHint->NumAlg;

            return;
        }
    }

    return;

} // void GetDoNotUseFilterList( mfxVideoParam* par, mfxU32** ppList, mfxU32* pLen )


void GetDoUseFilterList( mfxVideoParam* par, mfxU32** ppList, mfxU32* pLen )
{
    mfxU32 i = 0;
    mfxExtVPPDoUse* pVPPHint = NULL;

    /* robustness */
    *ppList = NULL;
    *pLen = 0;

    for( i = 0; i < par->NumExtParam; i++ )
    {
        if( MFX_EXTBUFF_VPP_DOUSE == par->ExtParam[i]->BufferId )
        {
            pVPPHint  = (mfxExtVPPDoUse*)(par->ExtParam[i]);
            *ppList = pVPPHint->AlgList;
            *pLen  = pVPPHint->NumAlg;

            return;
        }
    }

    return;

} // void GetDoUseFilterList( mfxVideoParam* par, mfxU32* pList, mfxU32* pLen )


bool CheckFilterList(mfxU32* pList, mfxU32 count, bool bDoUseTable)
{
    bool bResOK = true;
    // strong check
    if( (NULL == pList && count > 0) || (NULL != pList && count == 0) )
    {
        bResOK = false;
        return bResOK;
    }

    mfxU32 searchCount = sizeof(g_TABLE_DO_USE) / sizeof( *g_TABLE_DO_USE );
    mfxU32* pSearchTab = (mfxU32*)&g_TABLE_DO_USE[0];
    if( !bDoUseTable )
    {
        searchCount = sizeof(g_TABLE_DO_NOT_USE) / sizeof( *g_TABLE_DO_NOT_USE );
        pSearchTab = (mfxU32*)&g_TABLE_DO_NOT_USE[0];
    }

    mfxU32 fIdx = 0;
    for( fIdx = 0; fIdx < count; fIdx++ )
    {
        mfxU32 curId = pList[fIdx];

        if( !IsFilterFound(pSearchTab, searchCount, curId) )
        {
            bResOK = false; //invalid ID
        }
        else if( fIdx == count - 1 )
        {
            continue;
        }
        else if( IsFilterFound(pList + 1 + fIdx, count - 1 - fIdx, curId) )
        {
            bResOK = false; //duplicate ID
        }
    }

    return bResOK;

} // bool CheckFilterList(mfxU32* pList, mfxU32 count, bool bDoUseTable)


bool GetExtParamList(
    mfxVideoParam* par,
    mfxU32* pList,
    mfxU32* pLen)
{
    pList;
    mfxU32 fIdx = 0;

    /* robustness */
    *pLen = 0;

    mfxU32 searchCount = sizeof(g_TABLE_EXT_PARAM) / sizeof( *g_TABLE_EXT_PARAM );
    mfxU32  fCount        = par->NumExtParam;
    bool bResOK        = true;

    for( fIdx = 0; fIdx < fCount; fIdx++ )
    {
        mfxU32 curId = par->ExtParam[fIdx]->BufferId;
        if( IsFilterFound(g_TABLE_EXT_PARAM, searchCount, curId) )
        {
            if( !IsFilterFound(pList, *pLen, curId) )
            {
                pList[ (*pLen)++ ] = curId;
            }
            else
            {
                bResOK = false; // duplicate ID
            }
        }
        else
        {
            bResOK = false; //invalid ID
        }
    }

    return bResOK;

} // void GetExtParamList( mfxVideoParam* par, mfxU32* pList, mfxU32* pLen )


void GetConfigurableFilterList( mfxVideoParam* par, mfxU32* pList, mfxU32* pLen )
{
    pList;
    mfxU32 fIdx = 0;

    /* robustness */
    *pLen = 0;

    mfxU32 fCount = par->NumExtParam;
    mfxU32 searchCount = sizeof(g_TABLE_CONFIG) / sizeof( *g_TABLE_CONFIG );

    for( fIdx = 0; fIdx < fCount; fIdx++ )
    {
        mfxU32 curId = par->ExtParam[fIdx]->BufferId;
        if( IsFilterFound(g_TABLE_CONFIG, searchCount, curId) && !IsFilterFound(pList, *pLen, curId) )
        {
            pList[ (*pLen)++ ] = curId;
        }
    }

    return;

} // void GetConfigurableFilterList( mfxVideoParam* par, mfxU32* pList, mfxU32* pLen )


// check is buffer or filter are configurable
bool IsConfigurable( mfxU32 filterId )
{
    mfxU32 searchCount = sizeof(g_TABLE_CONFIG) / sizeof( *g_TABLE_CONFIG );

    return IsFilterFound(g_TABLE_CONFIG, searchCount, filterId) ? true : false;

} // void GetConfigurableFilterList( mfxU32 filterId )


size_t GetConfigSize( mfxU32 filterId )
{
    switch( filterId )
    {
    case MFX_EXTBUFF_VPP_DENOISE:
        {
            return sizeof(mfxExtVPPDenoise);
        }
    case MFX_EXTBUFF_VPP_PROCAMP:
        {
            return sizeof(mfxExtVPPProcAmp);
        }
    case MFX_EXTBUFF_VPP_DETAIL:
        {
            return sizeof(mfxExtVPPDetail);
        }
    case MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
        {
            return sizeof(mfxExtVPPFrameRateConversion);
        }
#if defined(MFX_ENABLE_IMAGE_STABILIZATION_VPP)
    case MFX_EXTBUFF_VPP_IMAGE_STABILIZATION:
        {
            return sizeof(mfxExtVPPImageStab);
        }
#endif
    case MFX_EXTBUFF_VPP_DEINTERLACING:
        {
            return sizeof(mfxExtVPPDeinterlacing);
        }
    /*case MFX_EXTBUFF_VPP_COMPOSITE:
        {
            return sizeof(mfxExtVPPDeinterlacing);
        }???*/

    default:
        return 0;
    }

} // size_t GetConfigSize( mfxU32 filterId )


mfxStatus CheckTransferMatrix( mfxU16 transferMatrix )
{
    transferMatrix;
    //switch( transferMatrix )
    //{
    //    case MFX_TRANSFERMATRIX_BT601:
    //    case MFX_TRANSFERMATRIX_BT709:
    //    case MFX_TRANSFERMATRIX_XVYCC_BT601:
    //    case MFX_TRASNFERMATRIX_XVYCC_BT709:
    //    {
            return MFX_ERR_NONE;
    //    }

    //    default:
    //    {
    //        return MFX_ERR_INVALID_VIDEO_PARAM;
    //    }
    //}

} // mfxStatus CheckTransferMatrix( mfxU16 transferMatrix )


mfxGamutMode GetGamutMode( mfxU16 srcTransferMatrix, mfxU16 dstTransferMatrix )
{
    mfxStatus mfxSts;

    mfxSts = CheckTransferMatrix( srcTransferMatrix );
    if( MFX_ERR_NONE != mfxSts )
    {
        return GAMUT_INVALID_MODE;
    }

    mfxSts = CheckTransferMatrix( dstTransferMatrix );
    if( MFX_ERR_NONE != mfxSts )
    {
        return GAMUT_INVALID_MODE;
    }

    mfxGamutMode mode = GAMUT_COMPRESS_BASE_MODE;

    if( srcTransferMatrix == dstTransferMatrix )
    {
        mode = GAMUT_PASSIVE_MODE;
    }
    //else if( MFX_TRANSFERMATRIX_XVYCC_BT601 == srcTransferMatrix &&
    //         MFX_TRANSFERMATRIX_BT601 == dstTransferMatrix )
    //{
    //    mode   = GAMUT_COMPRESS_ADVANCED_MODE;
    //    //m_bBT601 = true;
    //}
    //else if( MFX_TRASNFERMATRIX_XVYCC_BT709 == srcTransferMatrix &&
    //         MFX_TRANSFERMATRIX_BT709 == dstTransferMatrix )
    //{
    //    mode   = GAMUT_COMPRESS_ADVANCED_MODE;
    //    //m_bBT601 = false;
    //}
    //else
    //{
    //    mode = GAMUT_INVALID_MODE;
    //}

    return mode;

} // mfxGamutMode GetGamutMode( mfxU16 srcTransferMatrix, mfxU16 dstTransferMatrix )


mfxStatus CheckOpaqMode( mfxVideoParam* par, bool bOpaqMode[2] )
{
    if ( (par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) || (par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) )
    {
        mfxExtOpaqueSurfaceAlloc *pOpaqAlloc = 0;

        pOpaqAlloc = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        if (!pOpaqAlloc)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else
        {
            if( par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY )
            {
                if (!(pOpaqAlloc->In.Type & (MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)) && !(pOpaqAlloc->In.Type  & MFX_MEMTYPE_SYSTEM_MEMORY))
                {
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }

                if ((pOpaqAlloc->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY) && (pOpaqAlloc->In.Type & (MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)))
                {
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }

                bOpaqMode[VPP_IN] = true;
            }

            if( par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY )
            {
                if (!(pOpaqAlloc->Out.Type & (MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)) && !(pOpaqAlloc->Out.Type  & MFX_MEMTYPE_SYSTEM_MEMORY))
                {
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }

                if ((pOpaqAlloc->Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY) && (pOpaqAlloc->Out.Type & (MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)))
                {
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }

                bOpaqMode[VPP_OUT] = true;
            }
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus CheckOpaqMode( mfxVideoParam* par, bool bOpaqMode[2] )


mfxStatus GetOpaqRequest( mfxVideoParam* par, bool bOpaqMode[2], mfxFrameAllocRequest requestOpaq[2] )
{
    if( bOpaqMode[VPP_IN] || bOpaqMode[VPP_OUT] )
    {
        mfxExtOpaqueSurfaceAlloc *pOpaqAlloc = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBufferInternal(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        if( bOpaqMode[VPP_IN] )
        {
            requestOpaq[VPP_IN].Info = par->vpp.In;
            requestOpaq[VPP_IN].NumFrameMin = requestOpaq[VPP_IN].NumFrameSuggested = (mfxU16)pOpaqAlloc->In.NumSurface;
            requestOpaq[VPP_IN].Type = (mfxU16)pOpaqAlloc->In.Type;
        }

        if( bOpaqMode[VPP_OUT] )
        {
            requestOpaq[VPP_OUT].Info = par->vpp.Out;
            requestOpaq[VPP_OUT].NumFrameMin = requestOpaq[VPP_OUT].NumFrameSuggested = (mfxU16)pOpaqAlloc->Out.NumSurface;
            requestOpaq[VPP_OUT].Type = (mfxU16)pOpaqAlloc->Out.Type;
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus GetOpaqRequest( mfxVideoParam* par, bool bOpaqMode[2], mfxFrameAllocRequest requestOpaq[2] )


mfxStatus CheckIOPattern_AndSetIOMemTypes(mfxU16 IOPattern, mfxU16* pInMemType, mfxU16* pOutMemType, bool bSWLib)
{
    if ((IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) &&
        (IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) &&
        (IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }


    mfxU16 nativeMemType = (bSWLib) ? (mfxU16)MFX_MEMTYPE_SYSTEM_MEMORY : (mfxU16)MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;

    if( IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY )
    {
          *pInMemType = MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else if (IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
    {
        *pInMemType = MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
    }
    else if (IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        *pInMemType = MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_OPAQUE_FRAME|nativeMemType;
    }
    else
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if( IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY )
    {
        *pOutMemType = MFX_MEMTYPE_FROM_VPPOUT|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else if (IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
    {
        *pOutMemType = MFX_MEMTYPE_FROM_VPPOUT|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
    }
    else if(IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        *pOutMemType = MFX_MEMTYPE_FROM_VPPOUT|MFX_MEMTYPE_OPAQUE_FRAME|nativeMemType;
    }
    else
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;

} // mfxStatus CheckIOPattern_AndSetIOMemTypes(mfxU16 IOPattern,mfxU16* pInMemType, mfxU16* pOutMemType, bool bSWLib)


mfxU16 EstimatePicStruct(
    mfxU32* pVariance,
    mfxU16 width,
    mfxU16 height)
{
    mfxU16 resPicStruct = MFX_PICSTRUCT_UNKNOWN;

    mfxU32 varMin = pVariance[0]* 16 / (width * height);
    mfxU32 varMax = pVariance[1]* 16 / (width * height);

    //mfxU32 v0 = pVariance[0]* 16 / (width * height);
    //mfxU32 v1 = pVariance[1]* 16 / (width * height);
    mfxU32 v2 = pVariance[2]* 16 / (width * height);
    mfxU32 v3 = pVariance[3]* 16 / (width * height);
    mfxU32 v4 = pVariance[4]* 16 / (width * height);

    if(varMin > varMax)
    {
        std::swap(varMin, varMax);
    }

    if(varMax == 0)
    {
        resPicStruct = MFX_PICSTRUCT_UNKNOWN;
    }
    else
    {
        mfxF64 varRatio = (mfxF64)(varMin) / (mfxF64)(varMax);
        mfxF64 result = 100.0 * (1.0 - varRatio);
        mfxU32 absDiff= (mfxU32)abs((int)varMax - (int)varMin);

        bool bProgressive = false;
        if( absDiff < 2 && v2 < 50) // strong progressive;
        {
            bProgressive = true;
        }
        else if( result <= 1.0 && v2 < 50) // middle
        {
            bProgressive = true;
        }
        else if(result <= 1.0 && v2 < 110) // weak
        {
            bProgressive = true;
        }

        if( !bProgressive )
        {
            //printf("\n picstruct = INTERLACE ("); fflush(stderr);
            if( v3 < v4)
            {
                //printf("TFF) \n"); fflush(stderr);
                resPicStruct = MFX_PICSTRUCT_FIELD_TFF;
            }
            else
            {
                //printf("BFF) \n"); fflush(stderr);
                resPicStruct = MFX_PICSTRUCT_FIELD_BFF;
            }
        }
        else
        {
            /*printf("\n picstruct = PROGRESSIVE (%f %i %i) \n",
                result,
                varMin,
                varMax); fflush(stderr);*/
            resPicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        }
    }

    return resPicStruct;

} // mfxU16 EstimatePicStruct( mfxU32* pVariance )

mfxU16 MapDNFactor( mfxU16 denoiseFactor )
{
#if defined(LINUX32) || defined(LINUX64)
    // On Linux detail and de-noise factors mapped to the real libva values
    // at execution time.
    mfxU16 gfxFactor = denoiseFactor;
#else
    mfxU16 gfxFactor = (mfxU16)floor(64.0 / 100.0 * denoiseFactor + 0.5);
#endif

    return gfxFactor;

} // mfxU16 MapDNFactor( mfxU16 denoiseFactor )


mfxStatus CheckExtParam(VideoCORE * core, mfxExtBuffer** ppExtParam, mfxU16 count)
{
    if( (NULL == ppExtParam && count > 0) )
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    bool bError = false;

    // [1] ExtParam
    mfxVideoParam tmpParam;
    tmpParam.ExtParam   = ppExtParam;
    tmpParam.NumExtParam= count;

    mfxU32 extParamCount = sizeof(g_TABLE_EXT_PARAM) / sizeof(*g_TABLE_EXT_PARAM);
    std::vector<mfxU32> extParamList(extParamCount);
    if( !GetExtParamList( &tmpParam, &extParamList[0], &extParamCount ) )
    {
        bError = true;
    }


    // [2] configurable
    mfxU32 configCount = sizeof(g_TABLE_CONFIG) / sizeof(*g_TABLE_CONFIG);
    std::vector<mfxU32> configList(configCount);

    GetConfigurableFilterList( &tmpParam, &configList[0], &configCount );

    //-----------------------------------------------------
    mfxStatus sts = MFX_ERR_NONE, sts_wrn = MFX_ERR_NONE;
    mfxU32 fIdx = 0;
    for(fIdx = 0; fIdx < configCount; fIdx++)
    {
        mfxU32 curId = configList[fIdx];

        mfxExtBuffer* pHint = NULL;
        GetFilterParam( &tmpParam, curId, &pHint);

        //aya: 3 status's could be returned only: MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_UNSUPPORTED
        // AL update: now 4 status, added MFX_WRN_FILTER_SKIPPED
        sts = ExtendedQuery(core, curId, pHint);

        if( MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts || MFX_WRN_FILTER_SKIPPED == sts )
        {
            sts_wrn = sts;
            sts = MFX_ERR_NONE;
        }
        else if( MFX_ERR_UNSUPPORTED == sts )
        {
            bError = true;
            sts = MFX_ERR_NONE;
        }
        MFX_CHECK_STS(sts); // for double check only
    }
    //-----------------------------------------------------

    // [3] Do NOT USE
    mfxU32* pDnuList = NULL;
    mfxU32  dnuCount = 0;
    GetDoNotUseFilterList( &tmpParam, &pDnuList, &dnuCount );

    if( !CheckFilterList(pDnuList, dnuCount, false) )
    {
        bError = true;
    }

    for( mfxU32 extParIdx = 0; extParIdx < count; extParIdx++ )
    {
        // configured via extended parameters filter should not be disabled
        if ( IsFilterFound( pDnuList, dnuCount, ppExtParam[extParIdx]->BufferId ) )
        {
            sts = MFX_ERR_INVALID_VIDEO_PARAM;
        }
        MFX_CHECK_STS(sts);
    }

    // [4] Do USE
    mfxU32* pDO_USE_List = NULL;
    mfxU32  douseCount = 0;
    GetDoUseFilterList( &tmpParam, &pDO_USE_List, &douseCount );

    if( !CheckFilterList(pDO_USE_List, douseCount, true) )
    {
        bError = true;
    }

    // [5] cmp DO_USE vs DO_NOT_USE
    for( fIdx = 0; fIdx < dnuCount; fIdx++ )
    {
        if( IsFilterFound(pDO_USE_List, douseCount, pDnuList[fIdx]) )
        {
            bError = true;
        }
    }


    if( bError )
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    else if( MFX_ERR_NONE != sts_wrn )
    {
        return sts_wrn;
    }
    else
    {
        return MFX_ERR_NONE;
    }

} // mfxStatus CheckExtParam(mfxExtBuffer** ppExtParam, mfxU32 count)


void SignalPlatformCapabilities(
    const mfxVideoParam & param,
    const std::vector<mfxU32> & supportedList)
{
    // fill output DOUSE list
    //if(bCorrectionEnable)
    {
        mfxU32* pDO_USE_List = NULL;
        mfxU32  douseCount = 0;
        GetDoUseFilterList( (mfxVideoParam*)&param, &pDO_USE_List, &douseCount );
        if(douseCount > 0)
        {
            size_t fCount = IPP_MIN(supportedList.size(), douseCount);
            size_t fIdx = 0;
            for(fIdx = 0; fIdx < fCount; fIdx++)
            {
                pDO_USE_List[fIdx] = supportedList[fIdx];
            }

            for(; fIdx< douseCount; fIdx++)
            {
                pDO_USE_List[fIdx] = 0;// EMPTY
            }
        }
    }

    // ExtBuffer Correction (will be fixed late)

} // mfxStatus SignalPlatformCapabilities(...)


// there are some special cases which couldn't be resolve by using capsList and pipelineList only
// ex: FRC_Interpolation, RGB4 for HW etc
mfxStatus CheckLimitationsSW(
    const mfxVideoParam & param,
    const std::vector<mfxU32> & supportedDoUseList,
    bool bCorrectionEnable)
{
    mfxU32  len   = (mfxU32)supportedDoUseList.size();
    mfxU32* pList = (len > 0) ? (mfxU32*)&supportedDoUseList[0] : NULL;

    mfxStatus sts = MFX_ERR_NONE;

    if( param.vpp.In.FourCC == param.vpp.Out.FourCC &&
        MFX_FOURCC_RGB4 == param.vpp.In.FourCC)
    {
        // [1] RGB4->RGB4 - resize only to provide the best quality. make decision from MSDK 1.3
        if(len > 0)
        {
            sts = MFX_WRN_FILTER_SKIPPED;
            if(bCorrectionEnable)
            {
                std::vector <mfxU32> tmpDoUseList; //zero
                SignalPlatformCapabilities(param, tmpDoUseList);
            }
        }
    }
    else if( IsFilterFound(pList, len, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION) && (MFX_FRCALGM_FRAME_INTERPOLATION == GetMFXFrcMode(param)) )
    {
        // [2] Frame Interpolation isn't supported by SW yet, set to supported mode
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        if(bCorrectionEnable)
        {
            SetMFXFrcMode(param, MFX_FRCALGM_PRESERVE_TIMESTAMP);
        }
    }
    else if( IsFilterFound(pList, len, MFX_EXTBUFF_VPP_DEINTERLACING) && GetDeinterlacingMode(param) )
    {
        // [3] Deinterlacing mode isn't supported by SW yet, set to supported mode
        sts = MFX_ERR_UNSUPPORTED;
        if(bCorrectionEnable)
        {
            SetDeinterlacingMode(param, 0);
        }
    }
    else if( IsFilterFound(pList, len, MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO) )
    {
        // [4] Video signal info isn't supported by SW yet, set to supported mode
        sts = MFX_ERR_UNSUPPORTED;
        if(bCorrectionEnable)
        {
            SetSignalInfo(param, MFX_TRANSFERMATRIX_UNKNOWN, MFX_NOMINALRANGE_UNKNOWN);
        }
    }

    return sts;

} // mfxStatus CheckLimitationsSW(...)


mfxStatus CheckLimitationsHW(
    const mfxVideoParam & param,
    const std::vector<mfxU32> & supportedDoUseList,
//    const MfxHwVideoProcessing::mfxVppCaps & caps,
    bool bCorrectionEnable)
{
    mfxU32  len   = (mfxU32)supportedDoUseList.size();
    mfxU32* pList = (len > 0) ? (mfxU32*)&supportedDoUseList[0] : NULL;

    mfxStatus sts = MFX_ERR_NONE;

    // [0] in case of RGB4->RGB4 only resize is supported. (automatically, no actions here)

    // [1] in case of input RGB4,  only resize/FRC/rotation is supported
    if( param.vpp.In.FourCC == MFX_FOURCC_RGB4)
    {
        if(len > (mfxU32)(IsFilterFound(pList, len, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION) + IsFilterFound(pList, len, MFX_EXTBUFF_VPP_ROTATION)))
        {
            sts = MFX_WRN_FILTER_SKIPPED; // some unsupported filters were found
            if(bCorrectionEnable)
            {
                std::vector <mfxU32> tmpZeroDoUseList; //zero
                SignalPlatformCapabilities(param, tmpZeroDoUseList);
            }
        }
        //else
        //{
        //    if( MFX_FRCALGM_FRAME_INTERPOLATION == GetMFXFrcMode(param) )
        //    {
        //        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        //        if(bCorrectionEnable)
        //        {
        //            SetMFXFrcMode(param, MFX_FRCALGM_PRESERVE_TIMESTAMP);
        //        }
        //    }

        //    if(bCorrectionEnable)
        //    {
        //        std::vector <mfxU32> tmpDoUseList1(1);
        //        tmpDoUseList1[0] = MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION;
        //        SignalPlatformCapabilities(param, tmpDoUseList1);
        //    }
        //}
    }
    //// [2] FRC Interpolation limitations
    //else if( IsFilterFound(pList, len, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION) && (MFX_FRCALGM_FRAME_INTERPOLATION == GetMFXFrcMode(param)) )
    //{
    //    if(param.vpp.In.FourCC != MFX_FOURCC_NV12)
    //    {
    //        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    //        if(bCorrectionEnable)
    //        {
    //            SetMFXFrcMode(param, MFX_FRCALGM_PRESERVE_TIMESTAMP);
    //            SignalPlatformCapabilities(param, supportedDoUseList);
    //        }
    //    }
    //    else if( !IsFrcInterpolationEnable(param, caps) )
    //    {
    //        sts =  MFX_WRN_FILTER_SKIPPED;
    //        if(bCorrectionEnable)
    //        {
    //            SetMFXFrcMode(param, MFX_FRCALGM_PRESERVE_TIMESTAMP);
    //            SignalPlatformCapabilities(param, supportedDoUseList);
    //        }
    //    }
    //}

    return sts;

} // mfxStatus CheckLimitationsHW(...)


void ExtractDoUseList(mfxU32* pSrcList, mfxU32 len, std::vector<mfxU32> & dstList)
{
    dstList.resize(0);

    mfxU32 searchCount = sizeof(g_TABLE_DO_USE) / sizeof( *g_TABLE_DO_USE );

    for(mfxU32 searchIdx = 0; searchIdx < searchCount; searchIdx++)
    {
        if( IsFilterFound(pSrcList, len, g_TABLE_DO_USE[searchIdx]) )
        {
            dstList.push_back(g_TABLE_DO_USE[searchIdx]);
        }
    }

} // void ExtractDoUseList(mfxU32* pSrcList, mfxU32 len, std::vector<mfxU32> & dstList)


bool CheckDoUseCompatibility( mfxU32 filterName )
{
    bool bResult = false;

    mfxU32 douseCount = sizeof(g_TABLE_DO_USE) / sizeof( *g_TABLE_DO_USE );

    bResult = IsFilterFound(g_TABLE_DO_USE, douseCount, filterName);

    return bResult;

} // bool CheckDoUseCompatibility( mfxU32 filterName )


mfxStatus GetCrossList(
    const std::vector<mfxU32> & pipelineList,
    const std::vector<mfxU32> & capsList,
    std::vector<mfxU32> & doUseList,
    std::vector<mfxU32> & dontUseList )
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 fIdx;

    for(fIdx = 0; fIdx < pipelineList.size(); fIdx++)
    {
        if( !IsFilterFound(&capsList[0], (mfxU32)capsList.size(), pipelineList[fIdx]) )
        {
            if( CheckDoUseCompatibility( pipelineList[fIdx] ) )
            {
                dontUseList.push_back( pipelineList[fIdx] );
                sts = MFX_WRN_FILTER_SKIPPED;
            }
        }
        else
        {
            doUseList.push_back( pipelineList[fIdx] );
        }
    }

    return sts;

} // mfxStatus GetCrossList(...)


bool IsFrcInterpolationEnable(const mfxVideoParam & param, const MfxHwVideoProcessing::mfxVppCaps & caps)
{
    mfxF64 inFrameRate  = CalculateUMCFramerate(param.vpp.In.FrameRateExtN,  param.vpp.In.FrameRateExtD);
    mfxF64 outFrameRate = CalculateUMCFramerate(param.vpp.Out.FrameRateExtN, param.vpp.Out.FrameRateExtD);
    mfxF64 mfxRatio = inFrameRate == 0 ? 0 : outFrameRate / inFrameRate;

    mfxU32 frcCount = (mfxU32)caps.frcCaps.customRateData.size();
    mfxF64 FRC_EPS = 0.01;

    for(mfxU32 frcIdx = 0; frcIdx < frcCount; frcIdx++)
    {
        MfxHwVideoProcessing::CustomRateData* rateData = (MfxHwVideoProcessing::CustomRateData*)&(caps.frcCaps.customRateData[frcIdx]);
        // it is outFrameRate / inputFrameRate.
        mfxF64 gfxRatio = CalculateUMCFramerate(rateData->customRate.FrameRateExtN, rateData->customRate.FrameRateExtD);

        if( fabs(gfxRatio - mfxRatio) < FRC_EPS )
        {
            return true;
        }
    }

    return false;

} // bool IsFrcInterpolationEnable(const mfxVideoParam & param, const MfxHwVideoProcessing::mfxVppCaps & caps)


void ConvertCaps2ListDoUse(MfxHwVideoProcessing::mfxVppCaps& caps, std::vector<mfxU32>& list)
{
    if(caps.uProcampFilter)
    {
        list.push_back(MFX_EXTBUFF_VPP_PROCAMP);
    }

    if(caps.uDenoiseFilter)
    {
        list.push_back(MFX_EXTBUFF_VPP_DENOISE);
    }

    if(caps.uDetailFilter)
    {
        list.push_back(MFX_EXTBUFF_VPP_DETAIL);
    }

    if(caps.uFrameRateConversion)
    {
        list.push_back(MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION);
    }

    if(caps.uDeinterlacing)
    {
        list.push_back(MFX_EXTBUFF_VPP_DEINTERLACING);
    }

    if(caps.uVideoSignalInfo)
    {
        list.push_back(MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO);
    }

    if(caps.uIStabFilter)
    {
        list.push_back(MFX_EXTBUFF_VPP_IMAGE_STABILIZATION);
    }

    if(caps.uVariance)
    {
        list.push_back(MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION);
    }

    if(caps.uRotation)
    {
        list.push_back(MFX_EXTBUFF_VPP_ROTATION);
    }

    if(caps.uScaling)
    {
        list.push_back(MFX_EXTBUFF_VPP_SCALING);
    }

    /* FIELD Copy is always present*/
    list.push_back(MFX_EXTBUFF_VPP_FIELD_PROCESSING);
    /* Composition is always present*/
    list.push_back(MFX_EXTBUFF_VPP_COMPOSITE);

} // void ConvertCaps2ListDoUse(MfxHwVideoProcessing::mfxVppCaps& caps, std::vector<mfxU32> list)

#endif // MFX_ENABLE_VPP
/* EOF */
