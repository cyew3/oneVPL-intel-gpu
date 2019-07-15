/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once
#include "mfxjpeg.h"

class RotatedDecoder : public InterfaceProxy<IYUVSource>
{
    typedef InterfaceProxy<IYUVSource> base;
    mfxU16 m_rotation;
    std::map<mfxU16, mfxU16> rotations;
public:

    RotatedDecoder(mfxU16 nRotation, std::unique_ptr<IYUVSource> &&pActual)
        : base(std::move(pActual))
        , m_rotation(nRotation)
    {
        //setting up rotation
        rotations[0]   = MFX_ROTATION_0;
        rotations[90]  = MFX_ROTATION_90;
        rotations[180] = MFX_ROTATION_180;
        rotations[270] = MFX_ROTATION_270;
    }

    mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par)
    {
        mfxStatus sts = base::DecodeHeader(bs, par);

        if (sts >= MFX_ERR_NONE)
        {
            MFX_CHECK((par->mfx.Rotation = rotations[m_rotation]) != 0);

            PrintInfo(VM_STRING("Rotation"), VM_STRING("%d"), m_rotation);

            //pattern could be any I hope to check rotation
            mfxU16 ioPattern = par->IOPattern;
            par->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

            mfxFrameAllocRequest req[2];
            MFX_CHECK_STS_SKIP(base::QueryIOSurf(par, req), MFX_WRN_PARTIAL_ACCELERATION);

            par->IOPattern = ioPattern;
            par->mfx.FrameInfo = req->Info;
            //pipeline wont know about rotation at all
            par->mfx.Rotation  = MFX_ROTATION_0;
        }
        return sts;
    }

    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
    {
        MFX_CHECK_POINTER(par);

        mfxVideoParam vParams = *par;
        MFX_CHECK((vParams.mfx.Rotation = rotations[m_rotation]) != 0);

        if(MFX_ROTATION_90 == vParams.mfx.Rotation || MFX_ROTATION_270 == vParams.mfx.Rotation)
        {
            std::swap(vParams.mfx.FrameInfo.Height, vParams.mfx.FrameInfo.Width);
            std::swap(vParams.mfx.FrameInfo.CropW, vParams.mfx.FrameInfo.CropH);
            std::swap(vParams.mfx.FrameInfo.AspectRatioW, vParams.mfx.FrameInfo.AspectRatioH);
            std::swap(vParams.mfx.FrameInfo.CropX, vParams.mfx.FrameInfo.CropY);
        }

        mfxStatus sts = MFX_ERR_NONE;
        MFX_CHECK_STS(sts = base::QueryIOSurf(&vParams, request));

        if(MFX_ROTATION_90 == vParams.mfx.Rotation || MFX_ROTATION_270 == vParams.mfx.Rotation)
        {
            std::swap(request->Info.Height, request->Info.Width);
            std::swap(request->Info.CropW, request->Info.CropH);
            std::swap(request->Info.AspectRatioW, request->Info.AspectRatioH);
            std::swap(request->Info.CropX, request->Info.CropY);
        }

        return sts;
    }

    mfxStatus Init(mfxVideoParam *par)
    {
        MFX_CHECK_POINTER(par);

        MFX_CHECK((par->mfx.Rotation = rotations[m_rotation]) != 0);

        if(MFX_ROTATION_90 == par->mfx.Rotation || MFX_ROTATION_270 == par->mfx.Rotation)
        {
            std::swap(par->mfx.FrameInfo.Height, par->mfx.FrameInfo.Width);
            std::swap(par->mfx.FrameInfo.CropW, par->mfx.FrameInfo.CropH);
            std::swap(par->mfx.FrameInfo.AspectRatioW, par->mfx.FrameInfo.AspectRatioH);
            std::swap(par->mfx.FrameInfo.CropX, par->mfx.FrameInfo.CropY);
        }

        return base::Init(par);
    }



};
