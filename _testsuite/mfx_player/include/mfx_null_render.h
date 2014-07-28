/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once
#include "mfx_pipeline_defs.h"


class MFXLockUnlockRender : public MFXVideoRender
{
    IMPLEMENT_CLONE(MFXLockUnlockRender);
public:
    MFXLockUnlockRender(IVideoSession *core, mfxStatus *status):MFXVideoRender(core, status)
    {
    }

    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * /*pCtrl = NULL*/)
    {
        if ( ! surface )
            return MFX_ERR_NONE;

        MFX_CHECK_STS(LockFrame(surface));
        MFX_CHECK_STS(WriteSurface(surface));
        MFX_CHECK_STS(UnlockFrame(surface));

        return MFX_ERR_NONE;
    }

    mfxStatus WriteSurface(mfxFrameSurface1 * pSurface)
    {
        int i;
        mfxFrameInfo * pInfo = &pSurface->Info;
        mfxFrameData * pData = &pSurface->Data;

        Ipp32s crop_x = pInfo->CropX;
        Ipp32s crop_y = pInfo->CropY;
        mfxU32 pitch = pData->PitchLow + ((mfxU32)pData->PitchHigh << 16);

        switch (pSurface->Info.FourCC)
        {
            case MFX_FOURCC_YV12:
            {
                for (i = 0; i < pInfo->CropH; i++)
                {
                    PutData(pData->Y + (crop_y * pitch + crop_x) + i*pitch, pInfo->CropW);
                }

                crop_x >>= 1;
                crop_y >>= 1;

                for (i = 0; i < ((pInfo->CropH + 1) >> 1); i++)
                {
                    PutData(pData->U + (crop_y * pitch/2 + crop_x) + i*pitch/2, (pInfo->CropW + 1) >> 1);
                }

                for (i = 0; i < ((pInfo->CropH + 1) >> 1); i++)
                {
                    PutData(pData->V + (crop_y * pitch/2 + crop_x) + i*pitch/2, (pInfo->CropW + 1) >> 1);
                }
                break;
            }
            case MFX_FOURCC_YUV420_16:
            {
                for (i = 0; i < pInfo->CropH; i++)
                {
                    PutData(pData->Y + (crop_y * pitch + crop_x) + i*pitch, pInfo->CropW*2);
                }
                for (i = 0; i < ((pInfo->CropH + 1) >> 1); i++)
                {
                    PutData(pData->U + (crop_y * pitch/2 + crop_x) + i*pitch/2, pInfo->CropW);
                }
                for (i = 0; i < ((pInfo->CropH + 1) >> 1); i++)
                {
                    PutData(pData->V + (crop_y * pitch/2 + crop_x) + i*pitch/2, pInfo->CropW);
                }
                break;
            }
            case MFX_FOURCC_NV12:
            {
                for (i = 0; i < pInfo->CropH; i++)
                {
                    PutData(pData->Y + (pInfo->CropY * pitch + pInfo->CropX)+ i * pitch, pInfo->CropW);
                }
                for (i = 0; i < pInfo->CropH / 2; i++)
                {
                    PutData(pData->UV + (pInfo->CropY * pitch / 2 + pInfo->CropX) + i * pitch, pInfo->CropW);
                }
                break;
            }
            case MFX_FOURCC_P010:
            {
                for (i = 0; i < pInfo->CropH; i++)
                {
                    PutData(pData->Y + (pInfo->CropY * pitch + pInfo->CropX)+ i * pitch, pInfo->CropW * 2);
                }
                for (i = 0; i < pInfo->CropH / 2; i++)
                {
                    PutData(pData->UV + (pInfo->CropY * pitch / 2 + pInfo->CropX) + i * pitch, pInfo->CropW * 2);
                }
                break;
            }
            case MFX_FOURCC_ARGB16:
            {
                mfxU8 *plane = pData->B + pInfo->CropX * 4 + pInfo->CropY * pitch;
                for (i = 0; i < pInfo->CropH; i++)
                {
                    PutData(plane, pInfo->CropW * 8);
                    plane += pitch;
                }
                break;
            }
            case MFX_FOURCC_RGB4 :
            {
                mfxU8* plane = pData->B + pInfo->CropX * 4;

                for (i = 0; i <pInfo->CropH; i++)
                {
                    PutData(plane, pInfo->CropW * 4);
                    plane += pitch;
                }
                break;
            }
            case MFX_FOURCC_A2RGB10:
            {
                mfxU8* plane = pData->B + pInfo->CropX * 4;

                for (i = 0; i <pInfo->CropH; i++)
                {
                    MFX_CHECK_STS(PutData(plane, pInfo->CropW * 4));
                    plane += pitch;
                }
                break;
            }
    #if defined(_WIN32) || defined(_WIN64)
            case DXGI_FORMAT_AYUV :
            {
                m_Current.m_comp = VM_STRING('R');
                m_Current.m_pixX = 0;
                mfxU8* plane = pData->B + pInfo->CropX * 4;

                for (i = 0; i <pInfo->CropH; i++)
                {
                    m_Current.m_pixY = i;
                    MFX_CHECK_STS(PutData(plane, pInfo->CropW * 4));
                    plane += pitch;
                }
                break;
            }
    #endif
            case MFX_FOURCC_YUY2 :
            {
                mfxU8* plane = pData->Y + pInfo->CropY * pitch + pInfo->CropX * 2;


                for (i = 0; i <pInfo->CropH; i++)
                {
                    MFX_CHECK_STS(PutData(plane, pInfo->CropW * 2));
                    plane += pitch;
                }
                break;
            }
            default:
            {
                MFX_TRACE_AND_EXIT(MFX_ERR_UNSUPPORTED, (VM_STRING("[MFXFileWriteRender] file writing in %s colorformat not supported\n"),
                    GetMFXFourccString(pSurface->Info.FourCC).c_str()));
            }
        }

        return MFX_ERR_NONE;
    }

    mfxStatus PutData(mfxU8* pData, mfxU32 nSize)
    {
        if (NULL == pData)
        {
            return PutBsData(NULL);
        }
        mfxBitstream bs ={0};
        bs.Data = pData;
        bs.DataLength = nSize;
        return PutBsData(&bs);
    }

    mfxStatus PutBsData(mfxBitstream *pBs)
    {
        MFX_CHECK_STS(m_pFile->Write(pBs));
        return MFX_ERR_NONE;
    }
};


class MFXNullRender : public MFXVideoRender
{
    IMPLEMENT_CLONE(MFXNullRender);
public:
    MFXNullRender(IVideoSession *core, mfxStatus *status):MFXVideoRender(core, status)
    {
    }
    //that is why it is NULL
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * surface, mfxEncodeCtrl * pCtrl = NULL)
    {
        //for debugging, it it useful to see when frame actually gets rendered
        PipelineTraceEncFrame(0);
        return MFXVideoRender::RenderFrame(surface, pCtrl);
    }
};
