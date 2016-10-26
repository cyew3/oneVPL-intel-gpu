//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

//#define STORE_OUTPUT_YUV
//#define STORE_OUTPUT_AND_DIFF_YUV

#include "umc_h264_task_supplier.h"

namespace UMC
{
#define __YUV_FILE__ "./output.yuv"
#define __DIFF_YUV_FILE__ "D:/Projects/test.adobe/encode/canl1_toshiba_g_nodebl.yuv"

template<typename T> static
bool store_yuv(H264DecoderFrame *frame)
{
    static FILE *yuv_file;
    static FILE *diff_file;

    if (!frame)
        return true;
    if (yuv_file==NULL)
    {
        yuv_file=fopen(__YUV_FILE__,"w+b");
#ifdef STORE_OUTPUT_AND_DIFF_YUV
        diff_file = fopen(__DIFF_YUV_FILE__,"rb");
#endif
    }

    bool res = true;
    if (yuv_file)
    {
        Ipp32s pitch_luma = frame->pitch_luma();
        Ipp32s pitch_chroma = frame->pitch_chroma();
        Ipp32s width = frame->lumaSize().width;
        Ipp32s height = frame->lumaSize().height;

        height = frame->lumaSize().height - frame->m_crop_bottom;

        Ipp32s i;

        T *y  = (T*)frame->m_pYPlane;
        T *u  = (T*)frame->m_pUPlane;
        T *v  = (T*)frame->m_pVPlane;

        for (i=0;i<height;i++, y+=pitch_luma)
        {
            fwrite(y, width*sizeof(T), 1, yuv_file);
#ifdef STORE_OUTPUT_AND_DIFF_YUV
            T diff_y[10000];
            fread(&diff_y[0], width*sizeof(T), 1, diff_file);

            //if (vm_string_strncmp((vm_char*)&diff_y[0], (vm_char*)y, width*sizeof(T)) != 0)
            if (res)
            {
                for (Ipp32s k = 0; k < width; k++)
                {
                    if (diff_y[k] != y[k])
                    {
                        printf("Y plane - (y - %d, x - %d) tst - %d, ref - %d, poc - (%d, %d), index - %d\n", i, k, y[k], diff_y[k], frame->m_PicOrderCnt[0], frame->m_PicOrderCnt[1], frame->m_UID);
                        fflush(stdout);
                        res = false;
                        break;
                    }
                }
            }
#endif

        }

        switch(frame->m_chroma_format)
        {
        case 0:
            return true;
        case 1:
            width/=2;
            height/=2;
            break;
        case 2:
            width/=2;
            break;
        case 3:
            break;
        }

        for (i=0;i<height;i++,u+=pitch_chroma)
        {
            fwrite(u, width*sizeof(T), 1, yuv_file);
#ifdef STORE_OUTPUT_AND_DIFF_YUV
            T diff_y[10000];
            fread(diff_y, width*sizeof(T), 1, diff_file);
            //if (vm_string_strncmp((vm_char*)&diff_y[0], (vm_char*)u, width*sizeof(T)) != 0)
            if (res)
            {
                for (Ipp32s k = 0; k < width; k++)
                {
                    if (diff_y[k] != u[k])
                    {
                        printf("U plane - (y - %d, x - %d) tst - %d, ref - %d, poc - (%d, %d), index - %d\n", i, k, u[k], diff_y[k], frame->m_PicOrderCnt[0], frame->m_PicOrderCnt[1], frame->m_UID);
                        res = false;
                        break;
                    }
                }
            }
#endif

        }

        for (i=0;i<height;i++,v+=pitch_chroma)
        {
            fwrite(v, width*sizeof(T), 1, yuv_file);
#ifdef STORE_OUTPUT_AND_DIFF_YUV
            T diff_y[10000];
            fread(diff_y, width*sizeof(T), 1, diff_file);
            //if (vm_string_strncmp((vm_char*)diff_y, (vm_char*)v, width*sizeof(T)) != 0)
            if (res)
            {
                for (Ipp32s k = 0; k < width; k++)
                {
                    if (diff_y[k] != v[k])
                    {
                        printf("V plane - (y - %d, x - %d) tst - %d, ref - %d, poc - (%d, %d), index - %d\n", i, k, v[k], diff_y[k], frame->m_PicOrderCnt[0], frame->m_PicOrderCnt[1], frame->m_UID);
                        res = false;
                        break;
                    }
                }
            }
#endif
        }

        fflush(yuv_file);
    }

    return res;
}

#ifdef STORE_OUTPUT_YUV
bool CutPlanes(UMC::H264DecoderFrame * pDisplayFrame, UMC_H264_DECODER::H264SeqParamSet * sps)
{
    bool result = true;
    if (sps->bit_depth_luma == 8)
        result = UMC::store_yuv<Ipp8u> (pDisplayFrame);
    else
        result = UMC::store_yuv<Ipp16u> (pDisplayFrame);

    return result;
}
#endif

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
