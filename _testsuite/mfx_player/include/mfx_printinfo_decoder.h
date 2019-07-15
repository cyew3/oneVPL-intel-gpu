/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_iyuv_source.h"

/**
  prints verbose information of various decoding stages

  @author esmirno1
*/
class PrintInfoDecoder : public InterfaceProxy<IYUVSource>
{
    typedef InterfaceProxy<IYUVSource> base;

public:
    PrintInfoDecoder(std::unique_ptr<IYUVSource> &&pTarget)
        : base(std::move(pTarget))
    {
    }
    /*
    @brief prints resolution, for SVC, or MVC prints info for each layer/view
    */
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par)
    {
        mfxStatus  sts = base::DecodeHeader(bs, par);
        mfxExtSVCSeqDesc * pSvc = NULL;

        if (MFX_ERR_NONE <= sts)
        {
            //check that SVC seq description was attached
            for (size_t i  = 0; i < par->NumExtParam; i++)
            {
                if (BufferIdOf<mfxExtSVCSeqDesc>::id == par->ExtParam[i]->BufferId)
                {
                    pSvc = reinterpret_cast<mfxExtSVCSeqDesc *>(par->ExtParam[i]);
                    break;
                }
            }
            if (!pSvc)
            {
                //TODO: add mvc specific info
                switch(par->mfx.CodecProfile)
                {
                    case MFX_PROFILE_AVC_SCALABLE_BASELINE:
                    case MFX_PROFILE_AVC_SCALABLE_HIGH :
                        /// shouldn't print yet since outer coder will definetelly attach svc headers
                        return sts;
                    default:
                        break;
                }

            }
        }

        if (NULL != pSvc)
        {
            for (size_t j = 0; j < MFX_ARRAY_SIZE(pSvc->DependencyLayer); j++)
            {
                if (!pSvc->DependencyLayer[j].Active)
                    continue;
                tstringstream stream;
                stream<<VM_STRING("Layer[")<<j<<VM_STRING("] crop XxY WxH");
                PrintInfo(stream.str().c_str(), VM_STRING("%dx%d %dx%d")
                    , pSvc->DependencyLayer[j].CropX
                    , pSvc->DependencyLayer[j].CropY
                    , pSvc->DependencyLayer[j].CropW
                    , pSvc->DependencyLayer[j].CropH);
            }
            
        }
        else if (MFX_ERR_NONE == sts )
        {
            PrintInfo(VM_STRING("Crop XxY WxH"), VM_STRING("%dx%d %dx%d")
                , par->mfx.FrameInfo.CropX
                , par->mfx.FrameInfo.CropY
                , par->mfx.FrameInfo.CropW
                , par->mfx.FrameInfo.CropH);
        }
        
        //decode header algorithm finished
        if (MFX_ERR_NONE == sts || NULL != pSvc)
        {
            //handling of default values
            mfxFrameInfo &info = par->mfx.FrameInfo;
            PrintInfo(VM_STRING("PicStruct"), VM_STRING("%u"), info.PicStruct);
        }

        return sts;
    }

};
