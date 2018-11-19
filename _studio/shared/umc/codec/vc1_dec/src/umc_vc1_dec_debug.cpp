// Copyright (c) 2004-2018 Intel Corporation
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

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include <stdio.h>
#include <stdarg.h>
#include "umc_vc1_dec_debug.h"

#ifdef VC1_DEBUG_ON
#include "umc_vc1_dec_thread.h"
#endif


const uint32_t  VC1_POSITION  = 0x00000001; // MB, Block positions, skip info
const uint32_t  VC1_CBP       = 0x00000002; // coded block patern info
const uint32_t  VC1_BITBLANES = 0x00000004; // bitplane information
const uint32_t  VC1_QUANT     = 0x00000008; // transform types decoded info
const uint32_t  VC1_TT        = 0x00000010; // transform types decoded info
const uint32_t  VC1_MV        = 0x00000020; // motion vectors info
const uint32_t  VC1_PRED      = 0x00000040; // predicted blocks
const uint32_t  VC1_COEFFS    = 0x00000080; // DC, AC coefficiens
const uint32_t  VC1_RESPEL    = 0x00000100; // pixels befor filtering
const uint32_t  VC1_SMOOTHINT = 0x00000200; // smoothing
const uint32_t  VC1_BFRAMES   = 0x00000400; // B frames log
const uint32_t  VC1_INTENS    = 0x00000800; // intesity compensation tables
const uint32_t  VC1_MV_BBL    = 0x00001000; // deblocking
const uint32_t  VC1_MV_FIELD  = 0x00002000; // motion vectors info for field pic
const uint32_t  VC1_TABLES    = 0x00004000; //VLC tables

const uint32_t  VC1_DEBUG       = 0x00000000;//0x1DBF;//0x00000208; current debug output
const uint32_t  VC1_FRAME_DEBUG = 0; //on/off frame debug
const uint32_t  VC1_FRAME_MIN   = 5; //first frame to debug
const uint32_t  VC1_FRAME_MAX   = 15; //last frame to debug

#ifdef VC1_DEBUG_ON
void VM_Debug::vm_debug_frame(int32_t _cur_frame, int32_t level, const vm_char *format,...)
{
    vm_char line[1024];
    va_list args;
#if defined (_WIN32) && (_DEBUG)
    uint32_t ID = GetCurrentThreadId();
    if ((DebugThreadID == ID)||(_cur_frame >=0)) // only for Win Debug, need to redesign VM::Debug
#else
    if (_cur_frame >=0) // only for Win Debug, need to redesign VM::Debug
#endif
    {
#pragma warning( disable : 4127 ) // set const VC1_FRAME_DEBUG manual to on/off debug
        if (!VC1_FRAME_DEBUG)
        {
            if (!(level & VC1_DEBUG))
                return;

            va_start(args, format);
            vm_string_vsprintf(line, format, args);
            vm_string_fprintf(Logthread0, line);
        }
        else
        {
            static int32_t cur_frame =-1;
            if (_cur_frame >=0)
            {
                cur_frame=_cur_frame;
                return;
            }
            if ((cur_frame >= 0)&&((uint32_t)(cur_frame) <= VC1_FRAME_MAX)&&
                ((uint32_t)(cur_frame) >= VC1_FRAME_MIN ))
            {
                if (!(level & VC1_DEBUG))
                    return;
                va_start(args, format);
                vm_string_vsprintf(line, format, args);
                vm_string_fprintf(Logthread0, line);
            }
        }
    }
    else
    {
        if (!VC1_FRAME_DEBUG)
        {
            if (!(level & VC1_DEBUG))
                return;

            va_start(args, format);
            vm_string_vsprintf(line, format, args);
            vm_string_fprintf(Logthread1, line);
        }
        else
        {
            static int32_t cur_frame =-1;
            if (_cur_frame >=0)
            {
                cur_frame=_cur_frame;
                return;
            }
            if ((cur_frame >= 0)&&((uint32_t)(cur_frame) <= VC1_FRAME_MAX)&&
                ((uint32_t)(cur_frame) >= VC1_FRAME_MIN ))
            {
                if (!(level & VC1_DEBUG))
                    return;
                va_start(args, format);
                vm_string_vsprintf(line, format, args);
                vm_string_fprintf(Logthread1, line);
            }
        }
    }
}
#else

#ifdef _MSVC_LANG
#pragma warning( disable : 4100 ) // disable debug, empty function
#endif
void VM_Debug::vm_debug_frame(int32_t _cur_frame, int32_t level, const vm_char *format,...)
{
    (void)_cur_frame;
    (void)level;
    (void)format;
}
#endif // VC1_DEBUG_ON

#ifdef ALLOW_SW_VC1_FALLBACK
void VM_Debug::_print_macroblocks(VC1Context* pContext)
{
    int32_t i,j;
    uint8_t* pYPlane = pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].m_pY;
    uint8_t* pUPlane = pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].m_pU;
    uint8_t* pVPlane = pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].m_pV;

    int32_t YPitch = pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].m_iYPitch;
    int32_t UPitch = pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].m_iUPitch;
    int32_t VPitch = pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].m_iVPitch;
    uint32_t LeftTopRightPositionFlag = pContext->m_pCurrMB->LeftTopRightPositionFlag;

    int32_t currYoffset;
    int32_t currUoffset;
    int32_t currVoffset;

    uint32_t currMBYpos = pContext->m_pSingleMB->m_currMBYpos;
    uint32_t currMBXpos =pContext->m_pSingleMB->m_currMBXpos;


    int32_t fieldYPitch;
    int32_t fieldUPitch;
    int32_t fieldVPitch;

    if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
    {
        fieldYPitch = 2*YPitch;
        fieldUPitch = 2*UPitch;
        fieldVPitch = 2*VPitch;
    } else
    {
        fieldYPitch = YPitch;
        fieldUPitch = UPitch;
        fieldVPitch = VPitch;
    }

    if (pContext->m_picLayerHeader->CurrField)
        currMBYpos -= (pContext->m_seqLayerHeader.heightMB+1)/2;

    vm_debug_frame(-1,VC1_SMOOTHINT,(vm_char *)"maroblock-%d,%d\n",pContext->m_pSingleMB->m_currMBXpos,pContext->m_pSingleMB->m_currMBYpos);
    if (currMBXpos>0&&currMBYpos>0)
    {
        currYoffset = fieldYPitch*(currMBYpos-1)*VC1_PIXEL_IN_LUMA +
            (currMBXpos-1)*VC1_PIXEL_IN_LUMA;
        currUoffset = fieldUPitch*(currMBYpos-1)*VC1_PIXEL_IN_CHROMA +
            (currMBXpos-1)*VC1_PIXEL_IN_CHROMA;
        currVoffset = fieldVPitch*(currMBYpos-1)*VC1_PIXEL_IN_CHROMA +
            (currMBXpos-1)*VC1_PIXEL_IN_CHROMA;

        if ((pContext->m_picLayerHeader->BottomField)&&(pContext->m_picLayerHeader->FCM == VC1_FieldInterlace))
        {
        currYoffset += YPitch;
        currUoffset += UPitch;
        currVoffset += VPitch;
        }

        vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("left upper maroblock\n luma\n"));
        for (i=0;i<VC1_PIXEL_IN_LUMA;i++)
        {
            for (j=0;j<VC1_PIXEL_IN_LUMA;j++)
                vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("%3d "),pYPlane[currYoffset+fieldYPitch*i+j]);

            vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("\n "));
        }
        vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("U\n"));
        for (i=0;i<VC1_PIXEL_IN_CHROMA;i++)
        {
            for (j=0;j<VC1_PIXEL_IN_CHROMA;j++)
                vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("%3d "),pUPlane[currUoffset+fieldUPitch*i+j]);

           vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("\n "));
        }
        vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("V\n"));
        for (i=0;i<VC1_PIXEL_IN_CHROMA;i++)
        {
            for (j=0;j<VC1_PIXEL_IN_CHROMA;j++)
                vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("%3d "),pVPlane[currVoffset+fieldVPitch*i+j]);

            vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("\n "));
        }
    }

    if (VC1_IS_NO_LEFT_MB(LeftTopRightPositionFlag))
    {
        currYoffset = fieldYPitch*(currMBYpos)*VC1_PIXEL_IN_LUMA +
            (currMBXpos-1)*VC1_PIXEL_IN_LUMA;
        currUoffset = fieldUPitch*(currMBYpos)*VC1_PIXEL_IN_CHROMA +
            (currMBXpos-1)*VC1_PIXEL_IN_CHROMA;
        currVoffset = fieldVPitch*(currMBYpos)*VC1_PIXEL_IN_CHROMA +
            (currMBXpos-1)*VC1_PIXEL_IN_CHROMA;

        if ((pContext->m_picLayerHeader->BottomField)&&(pContext->m_picLayerHeader->FCM == VC1_FieldInterlace))
        {
        currYoffset += YPitch;
        currUoffset += UPitch;
        currVoffset += VPitch;
        }

        vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("left  maroblock\n luma\n"));
        for (i=0;i<VC1_PIXEL_IN_LUMA;i++)
        {
            for (j=0;j<VC1_PIXEL_IN_LUMA;j++)
                vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("%3d "),pYPlane[currYoffset+fieldYPitch*i+j]);

            vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("\n "));
        }
        vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("U\n"));
        for (i=0;i<VC1_PIXEL_IN_CHROMA;i++)
        {
            for (j=0;j<VC1_PIXEL_IN_CHROMA;j++)
                vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("%3d "),pUPlane[currUoffset+fieldUPitch*i+j]);

            vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("\n "));
        }
        vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("V\n"));
        for (i=0;i<VC1_PIXEL_IN_CHROMA;i++)
        {
            for (j=0;j<VC1_PIXEL_IN_CHROMA;j++)
                vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("%3d "),pVPlane[currVoffset+fieldVPitch*i+j]);

            vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("\n "));
        }
    }


    currYoffset = fieldYPitch*(currMBYpos)*VC1_PIXEL_IN_LUMA +
        (currMBXpos)*VC1_PIXEL_IN_LUMA;
    currUoffset = fieldUPitch*(currMBYpos)*VC1_PIXEL_IN_CHROMA +
        (currMBXpos)*VC1_PIXEL_IN_CHROMA;
    currVoffset = fieldVPitch*(currMBYpos)*VC1_PIXEL_IN_CHROMA +
        (currMBXpos)*VC1_PIXEL_IN_CHROMA;

    if ((pContext->m_picLayerHeader->BottomField)&&(pContext->m_picLayerHeader->FCM == VC1_FieldInterlace))
    {
        currYoffset += YPitch;
        currUoffset += UPitch;
        currVoffset += VPitch;
    }

    vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("current maroblock\n luma\n"));
    for (i=0;i<VC1_PIXEL_IN_LUMA;i++)
    {
        for (j=0;j<VC1_PIXEL_IN_LUMA;j++)
            vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("%3d "),pYPlane[currYoffset+fieldYPitch*i+j]);

        vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("\n "));
    }
    vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("U\n"));
    for (i=0;i<VC1_PIXEL_IN_CHROMA;i++)
    {
        for (j=0;j<VC1_PIXEL_IN_CHROMA;j++)
            vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("%3d "),pUPlane[currUoffset+fieldUPitch*i+j]);

        vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("\n "));
    }
    vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("V\n"));
    for (i=0;i<VC1_PIXEL_IN_CHROMA;i++)
    {
        for (j=0;j<VC1_PIXEL_IN_CHROMA;j++)
            vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("%3d "),pVPlane[currVoffset+fieldVPitch*i+j]);

        vm_debug_frame(-1,VC1_SMOOTHINT,VM_STRING("\n "));
    }
}

void VM_Debug::_print_blocks(VC1Context* pContext)
{
    int32_t blk_num;

    uint8_t* pYPlane = pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].m_pY;
    uint8_t* pUPlane = pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].m_pU;
    uint8_t* pVPlane = pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].m_pV;

    int32_t YPitch = pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].m_iYPitch;
    int32_t UPitch = pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].m_iUPitch;
    int32_t VPitch = pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].m_iVPitch;

    int32_t currYoffset;
    int32_t currUoffset;
    int32_t currVoffset;

    uint32_t currMBYpos = pContext->m_pSingleMB->m_currMBYpos;
    uint32_t currMBXpos = pContext->m_pSingleMB->m_currMBXpos;

    int32_t fieldYPitch;
    int32_t fieldUPitch;
    int32_t fieldVPitch;

    if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
    {
        fieldYPitch = 2*YPitch;
        fieldUPitch = 2*UPitch;
        fieldVPitch = 2*VPitch;
    } else
    {
        fieldYPitch = YPitch;
        fieldUPitch = UPitch;
        fieldVPitch = VPitch;
    }


    if (pContext->m_picLayerHeader->CurrField)
        currMBYpos -= pContext->m_seqLayerHeader.heightMB/2;

    for(blk_num = 0; blk_num < 6; blk_num++)
    {
        int32_t i,j;

        currYoffset = fieldYPitch*currMBYpos*VC1_PIXEL_IN_LUMA + currMBXpos*VC1_PIXEL_IN_LUMA;
        currUoffset = fieldUPitch*currMBYpos*VC1_PIXEL_IN_CHROMA + currMBXpos*VC1_PIXEL_IN_CHROMA;
        currVoffset = fieldVPitch*currMBYpos*VC1_PIXEL_IN_CHROMA + currMBXpos*VC1_PIXEL_IN_CHROMA;

        if ((pContext->m_picLayerHeader->BottomField)&&(pContext->m_picLayerHeader->FCM == VC1_FieldInterlace))
        {
        currYoffset += YPitch;
        currUoffset += UPitch;
        currVoffset += VPitch;
        }

        //MB predictiction if any and futher reconstruction

        vm_debug_frame(-1,VC1_RESPEL,VM_STRING("Result block:\n"));
        if(blk_num < 4)
        {
            if(blk_num < 2)
            {
                currYoffset += 8*blk_num;
            }
            else
            {
                currYoffset += 8*fieldYPitch+ 8*(blk_num-2);
            }

            for(i = 0; i < VC1_PIXEL_IN_BLOCK; i++)
            {
                for(j = 0; j < VC1_PIXEL_IN_BLOCK; j++)
                {
                    vm_debug_frame(-1,VC1_RESPEL,VM_STRING("%d "), pYPlane[currYoffset + i*fieldYPitch + j]);
                }
                vm_debug_frame(-1,VC1_RESPEL,VM_STRING("\n "));
            }
        }
        else if(blk_num == 4)
        {
            for(i = 0; i < VC1_PIXEL_IN_BLOCK; i++)
            {
                for(j = 0; j < VC1_PIXEL_IN_BLOCK; j++)
                {
                    vm_debug_frame(-1,VC1_RESPEL,VM_STRING("%d "), pUPlane[currUoffset + i*fieldUPitch + j]);
                }
                vm_debug_frame(-1,VC1_RESPEL,VM_STRING("\n "));
            }
        }
        else
        {
            for(i = 0; i < VC1_PIXEL_IN_BLOCK; i++)
            {
                for(j = 0; j < VC1_PIXEL_IN_BLOCK; j++)
                {
                    vm_debug_frame(-1,VC1_RESPEL,VM_STRING("%d "), pVPlane[currVoffset + i*fieldVPitch + j]);
                }
                vm_debug_frame(-1,VC1_RESPEL,VM_STRING("\n "));
            }
        }
    }
}
#endif // #ifdef ALLOW_SW_VC1_FALLBACK

void VM_Debug::print_bitplane(VC1Bitplane* pBitplane, int32_t width, int32_t height)
{
    int32_t i,j;
    if(pBitplane->m_imode != VC1_BITPLANE_RAW_MODE)
    {
        for(i = 0; i < height; i++)
        {
            for(j = 0; j < width; j++)
            {
                vm_debug_frame(-1,VC1_BITBLANES, VM_STRING("%d\t"), pBitplane->m_databits[i*width + j]);
            }
            vm_debug_frame(-1,VC1_BITBLANES, VM_STRING("\n"));
        }
    }
}
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
