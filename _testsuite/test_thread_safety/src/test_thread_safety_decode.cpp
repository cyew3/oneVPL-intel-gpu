/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: test_thread_safety_h264_decode.cpp

\* ****************************************************************************** */

#include "test_thread_safety.h"
#if (defined ENABLE_TEST_THREAD_SAFETY_FOR_H264_DECODE) || (defined ENABLE_TEST_THREAD_SAFETY_FOR_MPEG2_DECODE) || (defined ENABLE_TEST_THREAD_SAFETY_FOR_VC1_DECODE)

#include "vm_types.h"
#include "test_thread_safety_decode.h"
#include "mfx_default_pipeline_mgr.h"

DecodePipelineForTestThreadSafety::DecodePipelineForTestThreadSafety(IMFXPipelineFactory *pFactory)
    : MFXDecPipeline(pFactory)
{
}

IMFXPipeline * ConfigForTestThreadSafetyDecode::CreatePipeline()
{
    return new DecodePipelineForTestThreadSafety(new MFXPipelineFactory());
}

mfxStatus DecodePipelineForTestThreadSafety::CreateRender()
{
    mfxStatus sts = MFX_ERR_NONE;
    if (m_inParams.outFrameInfo.FourCC == MFX_FOURCC_UNKNOWN)
    {
        m_inParams.outFrameInfo.FourCC = m_components[eDEC].m_params.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422 ? (mfxU32)MFX_FOURCC_YV16 : MFX_FOURCC_YV12;
        m_inParams.outFrameInfo.BitDepthLuma = 8;
        m_inParams.outFrameInfo.BitDepthChroma = 8;
        if (m_components[eDEC].m_params.mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
        {
            m_inParams.outFrameInfo.FourCC = MFX_FOURCC_YUV420_16;
            m_inParams.outFrameInfo.BitDepthLuma = m_components[eDEC].m_params.mfx.FrameInfo.BitDepthLuma;
            m_inParams.outFrameInfo.BitDepthChroma = m_components[eDEC].m_params.mfx.FrameInfo.BitDepthChroma;
            m_inParams.outFrameInfo.Shift          = m_components[eDEC].m_params.mfx.FrameInfo.Shift;
        }
    }
    if (   MFX_FOURCC_P010    == m_inParams.outFrameInfo.FourCC
        || MFX_FOURCC_P210    == m_inParams.outFrameInfo.FourCC
        || MFX_FOURCC_NV16    == m_inParams.outFrameInfo.FourCC
        || MFX_FOURCC_NV12    == m_inParams.outFrameInfo.FourCC
        || MFX_FOURCC_A2RGB10 == m_inParams.outFrameInfo.FourCC
        ) {
            m_components[eREN].m_params.mfx.FrameInfo.FourCC = m_inParams.outFrameInfo.FourCC;
    }
    m_pRender = new OutputYuvTester(m_components[eREN].m_pSession, &sts);
    MFX_CHECK_STS(sts);
    return sts;
}

OutputYuvTester::OutputYuvTester(IVideoSession *core, mfxStatus *status)
: MFXVideoRender(core, status)
, m_handle(0)
{
    if (status)
    {
        *status = MFX_ERR_NONE;
    }
}

mfxStatus OutputYuvTester::Init(mfxVideoParam *par, const vm_char *)
{
    m_video = *par;
    m_handle = outReg->Register();
    return MFX_ERR_NONE;
}

class UnlockOnExit
{
public:
    UnlockOnExit(OutputYuvTester * pthis, mfxFrameSurface1 * arg) : m_pthis(pthis), m_arg(arg) {}
    ~UnlockOnExit() { (void)m_pthis->UnlockFrame(m_arg); }

private:
    OutputYuvTester *  m_pthis;
    mfxFrameSurface1 * m_arg;
};

mfxStatus OutputYuvTester::RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl *)
{
    if (m_handle == 0)
    {
        return MFX_ERR_NULL_PTR;
    }

    if (surface == 0)
    {
        if (m_handle != 0)
        {
            outReg->UnRegister(m_handle);
            m_handle = 0;
        }

        return MFX_ERR_NONE;
    }

    MFX_CHECK_STS(LockFrame(surface));
    UnlockOnExit unlocker(this, surface);

    mfxU8* ptr = surface->Data.Y + surface->Info.CropY * surface->Data.Pitch + surface->Info.CropX;
    for (mfxU32 i = 0; i < surface->Info.CropH; i++, ptr += surface->Data.Pitch)
    {
        if (m_video.mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
        {
            if (outReg->CommitData(m_handle, ptr, surface->Info.CropW * 2) < 0)
                return MFX_ERR_UNKNOWN;
        }
        else
        {
            if (outReg->CommitData(m_handle, ptr, surface->Info.CropW) < 0)
                return MFX_ERR_UNKNOWN;
        }
    }

    if (m_video.mfx.FrameInfo.FourCC == MFX_FOURCC_YV12)
    {
        mfxU32 nCropXYOffset = (surface->Info.CropY >> 1) * surface->Data.Pitch + (surface->Info.CropX >> 1);
        mfxU8* ptr = surface->Data.U + nCropXYOffset;

        for (mfxI32 i = 0; i < (surface->Info.CropH + 1) / 2; i++, ptr += surface->Data.Pitch / 2)
        {
            if (outReg->CommitData(m_handle, ptr, (surface->Info.CropW + 1) / 2) < 0)
                return MFX_ERR_UNKNOWN;
        }

        ptr = surface->Data.V + nCropXYOffset;
        for (mfxI32 i = 0; i < (surface->Info.CropH + 1) / 2; i++, ptr += surface->Data.Pitch / 2)
        {
            if (outReg->CommitData(m_handle, ptr, (surface->Info.CropW + 1) / 2) < 0)
                return MFX_ERR_UNKNOWN;
        }
    }
    else if (m_video.mfx.FrameInfo.FourCC == MFX_FOURCC_NV12)
    {
        mfxU8 buf[8192];
        if (surface->Info.CropW > 8192)
        {
            return MFX_ERR_UNSUPPORTED;
        }
        mfxU32 nCropXYOffset = (surface->Info.CropY >> 1) * surface->Data.Pitch + surface->Info.CropX;

        for (mfxI32 i = 0; i < (surface->Info.CropH + 1) / 2; i++)
        {
            for (mfxI32 j = 0; j < (surface->Info.CropW + 1) / 2; j++)
            {
                buf[j] = surface->Data.UV[i * surface->Data.Pitch + 2 * j + nCropXYOffset];
            }

            if (outReg->CommitData(m_handle, buf, (surface->Info.CropW + 1) / 2) < 0)
                return MFX_ERR_UNKNOWN;
        }

        for (mfxI32 i = 0; i < (surface->Info.CropH + 1) / 2; i++)
        {
            for (mfxI32 j = 0; j < (surface->Info.CropW + 1) / 2; j++)
            {
                buf[j] = surface->Data.UV[i * surface->Data.Pitch + 2 * j + 1 + nCropXYOffset];
            }

            if (outReg->CommitData(m_handle, buf, (surface->Info.CropW + 1) / 2) < 0)
                return MFX_ERR_UNKNOWN;
        }
    }
    else if (m_video.mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
    {
        mfxU8* buf = new mfxU8[surface->Info.CropW * 2];

        for (mfxI32 i = 0; i < (surface->Info.CropH + 1) / 2; i++)
        {
            memcpy(buf, surface->Data.UV + i*surface->Data.Pitch, surface->Info.CropW * 2);
            if (outReg->CommitData(m_handle, buf, surface->Info.CropW * 2) < 0)
                return MFX_ERR_UNKNOWN;
        }

        delete [] buf;
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

mfxStatus OutputYuvTester::Close()
{
    if (m_handle != 0)
    {
        outReg->UnRegister(m_handle);
        m_handle = 0;
    }

    return MFX_ERR_NONE;
}

mfxI32 RunDecode(mfxI32 argc, vm_char** argv, IPipelineSynhro *pExternalSync)
{
    std::auto_ptr<IMFXPipelineConfig> cfg(new ConfigForTestThreadSafetyDecode(argc, argv, pExternalSync));
    MFXPipelineManager defMgr;

    return defMgr.Execute(cfg.get());
}

#else //ENABLE_...

int RunDecode(int, vm_char**, IPipelineSynhro *) { return 1; }

#endif //ENABLE_...
