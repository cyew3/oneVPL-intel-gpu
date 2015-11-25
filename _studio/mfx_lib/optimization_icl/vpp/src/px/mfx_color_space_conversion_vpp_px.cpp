/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 - 2015 Intel Corporation. All Rights Reserved.
//
//
//          ColorSpaceConversion Video Pre\Post Processing
//
*/

#include "mfx_dispatcher.h"

#if defined (MFX_ENABLE_VPP)

#include "mfx_vpp_defs.h"
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"

namespace MFX_PP
{

/* some macros */
#ifndef CHOP
#define CHOP(x)     ((x < 0) ? 0 : ((x > 255) ? 255 : x))
#endif

#ifndef CHOP10
#define CHOP10(x)     ((x < 0) ? 0 : ((x > 1023) ? 1023 : x))
#endif

#ifndef V_POS
#define V_POS(ptr, shift, step, pos) ( (pos) >= 0  ? ( ptr + (shift) + (step) * (pos) )[0] : ( ptr + (shift) - (step) * (-1*pos) )[0])
#endif

IppStatus MFX_Dispatcher::cc_P010_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo* outInfo, mfxFrameData* yv12Data)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
      /* Initial implementation w/o rounding 
       * TODO: add rounding to match reference algorithm 
       */
      sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, 2, (Ipp16u *)yv12Data->Y, yv12Data->Pitch, roiSize);
      IPP_CHECK_STS( sts );
      sts = ippiConvert_16u8u_C1R((const Ipp16u *)yv12Data->Y,  yv12Data->Pitch, outData->Y, outData->Pitch, roiSize); 
      IPP_CHECK_STS( sts );

      roiSize.height >>= 1;
      sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->UV, inData->Pitch, 2, (Ipp16u *)yv12Data->UV, yv12Data->Pitch, roiSize);
      IPP_CHECK_STS( sts );
      sts = ippiConvert_16u8u_C1R((const Ipp16u *)yv12Data->UV,  yv12Data->Pitch, outData->UV, outData->Pitch, roiSize); 
      IPP_CHECK_STS( sts );
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_P010_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus MFX_Dispatcher::cc_P010_to_P210( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo*)
{
    IppStatus sts = ippStsNoErr;
    IppiSize  roiSize = {0, 0};

    VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
    VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        // Copy Y plane as is
        sts = ippiCopy_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, (Ipp16u *)outData->Y, outData->Pitch, roiSize);
        IPP_CHECK_STS( sts );

        // Chroma is interpolated using nearest points that gives closer match with ffmpeg
        mfxU16 *Out;
        mfxU32 InStep  = inData->Pitch  >> 1;
        mfxU32 OutStep = outData->Pitch >> 1;

        mfxU32 InShift  = 0;
        mfxU32 OutShift = 0;
        mfxU32 UShift   = 0;
        mfxU32 VShift   = 0;
        int main   = 0;
        int part   = 0;

        for (mfxU16 j = 0; j < roiSize.height; j++)
        {
            InShift  = ( InStep  ) * ( j / 2 );
            OutShift = ( OutStep ) * j;

            if ( 0 == j || j == (roiSize.height -1 ))
            {
                // First and last lines
                part = 0;
            }
            else if ( j % 2 == 0 )
            {
                // Odd lines
                part = -1;
            }
            else
            {
                // Even lines
                part = 1;
            }

            for (mfxU16 i = 0; i < roiSize.width; i+=2)
            {
                UShift  = InShift + i;
                VShift  = UShift  + 1;
                Out     = outData->U16 + OutShift  + i;

                // U
                Out[0] = CHOP10((3*V_POS(inData->U16, UShift, InStep, main) + V_POS(inData->U16, UShift, InStep, part)) >> 2);
                // V
                Out[1] = CHOP10((3*V_POS(inData->U16, VShift, InStep, main) + V_POS(inData->U16, VShift, InStep, part)) >> 2);
            }
      }
  }
  else
  {
      /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_P210_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus MFX_Dispatcher::cc_NV16_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo* /*outInfo*/)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
      // Copy Y plane as is
      sts = ippiCopy_8u_C1R(inData->Y, inData->Pitch, outData->Y, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      mfxU8  *Out;
      mfxU32 InStep  = inData->Pitch;
      mfxU32 OutStep = outData->Pitch;
      mfxU32 InShift  = 0;
      mfxU32 OutShift = 0;
      mfxU32 UShift   = 0;
      mfxU32 VShift   = 0;

      for (mfxU16 j = 0; j < roiSize.height; j+=2)
      {
          InShift  = ( InStep  ) * j;
          OutShift = ( OutStep ) * (j/2);

          for (mfxU16 i = 0; i < roiSize.width; i+=2)
          {
              UShift  = InShift + i;
              VShift  = UShift  + 1;
              Out     = outData->UV + OutShift  + i;
              if ( j == 0  )
              {
                  Out[0] =  V_POS(inData->UV, UShift, InStep, 0);
                  Out[1] =  V_POS(inData->UV, VShift, InStep, 0);
              }
              else if ( j == ( roiSize.height - 1))
              {
                  Out[0] =  V_POS(inData->UV, UShift, InStep, 1);
                  Out[1] =  V_POS(inData->UV, VShift, InStep, 1);
              }
              else
              {
                  // U
                  Out[0] = CHOP( ((V_POS(inData->UV, UShift, InStep,  0) + V_POS(inData->UV, UShift, InStep, 1) + 1)>>1));
                  // V
                  Out[1] = CHOP( ((V_POS(inData->UV, VShift, InStep,  0) + V_POS(inData->UV, VShift, InStep, 1) + 1)>>1));
              }
          }
      }
  }
  else
  {
      /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;
} // IppStatus cc_NV16_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus MFX_Dispatcher::cc_NV16_to_P210( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo* outInfo)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
      sts = ippiConvert_8u16u_C1R(inData->Y,  inData->Pitch, (Ipp16u *)outData->Y,  outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      sts = ippiLShiftC_16u_C1IR(2, (Ipp16u *)outData->Y, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      sts = ippiConvert_8u16u_C1R(inData->UV, inData->Pitch, (Ipp16u *)outData->UV, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      sts = ippiLShiftC_16u_C1IR(2, (Ipp16u *)outData->UV, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_NV16_to_P210( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus MFX_Dispatcher::cc_P210_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo* /*outInfo*/)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        // Copy Y plane as is
        sts = ippiCopy_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, (Ipp16u *)outData->Y, outData->Pitch, roiSize);
        IPP_CHECK_STS( sts );

        mfxU16 *Out;
        mfxU32 InStep  = inData->Pitch  >> 1;
        mfxU32 OutStep = outData->Pitch >> 1;
        mfxU32 InShift  = 0;
        mfxU32 OutShift = 0;
        mfxU32 UShift   = 0;
        mfxU32 VShift   = 0;

        for (mfxU16 j = 0; j < roiSize.height; j+=2)
        {
            InShift  = ( InStep  ) * j;
            OutShift = ( OutStep ) * (j/2);

            for (mfxU16 i = 0; i < roiSize.width; i+=2)
            {
                UShift  = InShift + i;
                VShift  = UShift  + 1;
                Out     = outData->U16 + OutShift  + i;
                if ( j == 0  )
                {
                    Out[0] =  V_POS(inData->U16, UShift, InStep, 0);
                    Out[1] =  V_POS(inData->U16, VShift, InStep, 0);
                }
                else if ( j == ( roiSize.height - 1))
                {
                    Out[0] =  V_POS(inData->U16, UShift, InStep, 1);
                    Out[1] =  V_POS(inData->U16, VShift, InStep, 1);
                }
                else
                {
                    // U
                    Out[0] = CHOP10( ((V_POS(inData->U16, UShift, InStep,  0) + V_POS(inData->U16, UShift, InStep, 1) + 1)>>1));
                    // V
                    Out[1] = CHOP10( ((V_POS(inData->U16, VShift, InStep,  0) + V_POS(inData->U16, VShift, InStep, 1) + 1)>>1));
                }
            }
        }
    }
    else
    {
        /* Interlaced content is not supported... yet. */
        return ippStsErr;
    }

    return sts;

} // IppStatus cc_P210_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus MFX_Dispatcher::cc_P210_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo* outInfo, mfxFrameData* yv12Data)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
      /* Initial implementation w/o rounding
       * TODO: add rounding to match reference algorithm
       */
      sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, 2, (Ipp16u *)yv12Data->Y, yv12Data->Pitch, roiSize);
      IPP_CHECK_STS( sts );
      sts = ippiConvert_16u8u_C1R((const Ipp16u *)yv12Data->Y,  yv12Data->Pitch, outData->Y, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      roiSize.height >>= 1;
      sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->UV, inData->Pitch * 2, 2, (Ipp16u *)yv12Data->UV, yv12Data->Pitch, roiSize);
      IPP_CHECK_STS( sts );
      sts = ippiConvert_16u8u_C1R((const Ipp16u *)yv12Data->UV,  yv12Data->Pitch, outData->UV, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;
} // IppStatus cc_P210_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus MFX_Dispatcher::cc_P210_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo* outInfo, mfxFrameData* yv12Data)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
      sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, 2, (Ipp16u *)yv12Data->Y, yv12Data->Pitch, roiSize);
      IPP_CHECK_STS( sts );
      sts = ippiConvert_16u8u_C1R((const Ipp16u *)yv12Data->Y,  yv12Data->Pitch, outData->Y, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->UV, inData->Pitch, 2, (Ipp16u *)yv12Data->UV, yv12Data->Pitch, roiSize);
      IPP_CHECK_STS( sts );
      sts = ippiConvert_16u8u_C1R((const Ipp16u *)yv12Data->UV,  yv12Data->Pitch, outData->UV, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;
} // IppStatus cc_P210_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus MFX_Dispatcher::cc_NV12_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo* outInfo)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
      sts = ippiConvert_8u16u_C1R(inData->Y,  inData->Pitch, (Ipp16u *)outData->Y,  outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      sts = ippiLShiftC_16u_C1IR(2, (Ipp16u *)outData->Y, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      roiSize.height >>= 1;
      sts = ippiConvert_8u16u_C1R(inData->UV, inData->Pitch, (Ipp16u *)outData->UV, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      sts = ippiLShiftC_16u_C1IR(2, (Ipp16u *)outData->UV, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_NV12_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus MFX_Dispatcher::cc_NV12_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo*)
{
    IppStatus sts = ippStsNoErr;
    IppiSize  roiSize = {0, 0};

    VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
    VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        // Copy Y plane as is
        sts = ippiCopy_8u_C1R((const Ipp8u *)inData->Y, inData->Pitch, (Ipp8u *)outData->Y, outData->Pitch, roiSize);
        IPP_CHECK_STS( sts );

        // Chroma is interpolated using nearest points that gives closer match with ffmpeg
        mfxU8  *Out;
        mfxU32 InStep  = inData->Pitch;
        mfxU32 OutStep = outData->Pitch;

        mfxU32 InShift  = 0;
        mfxU32 OutShift = 0;
        mfxU32 UShift   = 0;
        mfxU32 VShift   = 0;
        int main   = 0;
        int part   = 0;

        for (mfxU16 j = 0; j < roiSize.height; j++)
        {
            InShift  = ( InStep  ) * ( j / 2 );
            OutShift = ( OutStep ) * j;

            if ( 0 == j || j == (roiSize.height -1 ))
            {
                // First and last lines
                part = 0;
            }
            else if ( j % 2 == 0 )
            {
                // Odd lines
                part = -1;
            }
            else
            {
                // Even lines
                part = 1;
            }

            for (mfxU16 i = 0; i < roiSize.width; i+=2)
            {
                UShift  = InShift + i;
                VShift  = UShift  + 1;
                Out     = outData->UV + OutShift  + i;

                // U
                Out[0] = CHOP((3*V_POS(inData->UV, UShift, InStep, main) + V_POS(inData->UV, UShift, InStep, part)) >> 2);
                // V
                Out[1] = CHOP((3*V_POS(inData->UV, VShift, InStep, main) + V_POS(inData->UV, VShift, InStep, part)) >> 2);
            }
      }
  }
  else
  {
      /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_NV12_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus MFX_Dispatcher::cc_P010_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo* outInfo)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};
  mfxU32 inPitch, outPitch;
  mfxU16 *ptr_y, *ptr_uv;
  mfxU32 *out;
  mfxU32 uv_offset, out_offset;
  mfxU16 y, v,u;
  mfxU16 r, g, b;
  int Y,Cb,Cr;

  inPitch  = inData->Pitch;
  outPitch = outData->Pitch;
  outPitch >>= 2; // U32 pointer is used for output
  inPitch  >>= 1; // U16 pointer is used for input
  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
        ptr_y  = (mfxU16*)inData->Y;
        ptr_uv = (mfxU16*)inData->UV;
        out = (mfxU32*)IPP_MIN( IPP_MIN(outData->R, outData->G), outData->B );

        uv_offset    = 0;
        out_offset   = 0;

        for(mfxI32 j = 0; j < roiSize.height; j++) {
            out_offset = j*outPitch;

            if ( j != 0 && (j%2) == 0 ){
                // Use the same plane twice for 4:2:0
                uv_offset += inPitch;
            }

            for (mfxI32 i = 0; i < roiSize.width; i++) {
                y = ptr_y[j*inPitch + i];
                u = ptr_uv[uv_offset + (i/2)*2];
                v = ptr_uv[uv_offset + (i/2)*2 + 1];

                Y = (int)(y - 64) * 0x000129fa;
                Cb = u - 512;
                Cr = v - 512;
                r = CHOP10((((Y + 0x00019891 * Cr + 0x00008000 )>>16)));
                g = CHOP10((((Y - 0x00006459 * Cb - 0x0000d01f * Cr + 0x00008000  ) >> 16 )));
                b = CHOP10((((Y + 0x00020458 * Cb + 0x00008000  )>>16)));
                out[out_offset++] = (r <<20 | g << 10 | b << 0 );
             }
        }
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_P010_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus MFX_Dispatcher::cc_P210_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo* outInfo)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};
  mfxU32 inPitch, outPitch;
  mfxU16 *ptr_y, *ptr_uv;
  mfxU32 *out;
  mfxU32 uv_offset, out_offset;
  mfxU16 y, v,u;
  mfxU16 r, g, b;
  int Y,Cb,Cr;

  inPitch  = inData->Pitch;
  outPitch = outData->Pitch;
  outPitch >>= 2; // U32 pointer is used for output
  inPitch  >>= 1; // U16 pointer is used for input
  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
        ptr_y  = (mfxU16*)inData->Y;
        ptr_uv = (mfxU16*)inData->UV;
        out = (mfxU32*)IPP_MIN( IPP_MIN(outData->R, outData->G), outData->B );

        uv_offset    = 0;
        out_offset   = 0;

        for(mfxI32 j = 0; j < roiSize.height; j++) {
            out_offset = j*outPitch;
            uv_offset  = j*inPitch;

            for (mfxI32 i = 0; i < roiSize.width; i++) {
                y = ptr_y[j*inPitch + i];
                u = ptr_uv[uv_offset + (i/2)*2];
                v = ptr_uv[uv_offset + (i/2)*2 + 1];

                Y = (int)(y - 64) * 0x000129fa;
                Cb = u - 512;
                Cr = v - 512;
                r = CHOP10((((Y + 0x00019891 * Cr + 0x00008000 )>>16)));
                g = CHOP10((((Y - 0x00006459 * Cb - 0x0000d01f * Cr + 0x00008000  ) >> 16 )));
                b = CHOP10((((Y + 0x00020458 * Cb + 0x00008000  )>>16)));
                out[out_offset++] = (r <<20 | g << 10 | b << 0 );
             }
        }
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_P210_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

} // namespace MFX_PP

#endif // MFX_ENABLE_VPP

/* EOF */
