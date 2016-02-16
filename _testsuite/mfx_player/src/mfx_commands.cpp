/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_commands.h"
#include "mfx_serializer.h"

#include <ipps.h>

#include "mfx_component_params.h"

#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

//////////////////////////////////////////////////////////////////////////

seekSourceCommand::seekSourceCommand(IPipelineControl *pHolder)
    : commandBase(pHolder)
    , m_seekType()
{
}

void seekSourceCommand::SetSeekTime(const mfxF64 &fVal)
{
    m_fSeekTime = fVal;
    m_seekType = SEEK_TIME;
}

void seekSourceCommand::SetSeekPercent(const mfxF64 & fVal)
{
    m_fSeekPercent = fVal;
    m_seekType = SEEK_PERCENT;
}

void seekSourceCommand::SetSeekFrames(const mfxU32 & nFrames)
{
    m_uiFramesOffset = nFrames;
    m_seekType = SEEK_NUM_FRAME;
}


mfxStatus seekSourceCommand::Execute()
{
    IBitstreamReader *pSplitter;

    MFX_CHECK_POINTER(m_pControl);
    MFX_CHECK_STS(m_pControl->GetSplitter(&pSplitter));

    //reposition
    switch (m_seekType)
    {
        case SEEK_TIME:
        {
            MFX_CHECK_STS_SKIP(pSplitter->SeekTime(m_fSeekTime), MFX_ERR_UNSUPPORTED);
            PipelineTrace((VM_STRING("Seek to time %.2lf\n"), m_fSeekTime));
            break;
        }
        case SEEK_PERCENT:
        {
            MFX_CHECK_STS_SKIP(pSplitter->SeekPercent(m_fSeekPercent), MFX_ERR_UNSUPPORTED);
            PipelineTrace((VM_STRING("Seek to percent %.2lf\n"), m_fSeekPercent));
            break;
        }
        case SEEK_NUM_FRAME:
        {
            IYUVSource *pSource;
            MFX_CHECK_STS(m_pControl->GetYUVSource(&pSource));
            mfxVideoParam vParam = {};
            MFX_CHECK_STS(pSource->GetVideoParam(&vParam));

            MFX_CHECK_STS_SKIP(pSplitter->SeekFrameOffset(m_uiFramesOffset, vParam.mfx.FrameInfo), MFX_ERR_UNSUPPORTED);
            PipelineTrace((VM_STRING("Seek to position %d\n"), m_uiFramesOffset));

            MFX_CHECK_STS(m_pControl->ResetAfterSeek());
            break;
        }
        default:
        {
            MFX_TRACE_AND_EXIT(MFX_ERR_UNSUPPORTED,(VM_STRING("seekSourceCommand wasn't initialized, seeType=%d"), m_seekType));
            break;
        }
    }

    return MFX_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
seekCommand::seekCommand(IPipelineControl *pHolder)
:seekSourceCommand(pHolder)
{
}

mfxStatus seekCommand::Execute()
{
    IYUVSource        *pSource;
    IMFXVideoVPP      *pVPP;
    IMFXVideoRender   *pRender;

    MFX_CHECK_POINTER(m_pControl);
    MFX_CHECK_STS(m_pControl->GetYUVSource(&pSource));
    MFX_CHECK_STS(m_pControl->GetVPP(&pVPP));
    MFX_CHECK_STS(m_pControl->GetRender(&pRender));

    ////cleaning decoder
    mfxVideoParam vParam = {};

    //don't change current params
    MFX_CHECK_STS(pSource->GetVideoParam(&vParam));
    //pSource->Close();
    //pSource->Init(&vParam);

    MFX_CHECK_STS(pSource->Reset(&vParam));

    ////cleaning next pipeline components
    if (NULL != pVPP)
    {
        MFX_ZERO_MEM(vParam);
        MFX_CHECK_STS(pVPP->GetVideoParam(&vParam));
        MFX_CHECK_STS(pVPP->Reset(&vParam));
    }

    //for example in case of mvc stream render could buffer single view
    //to prevent uncompleted frames appearing renderd should be cleaned up
    if (NULL != pRender)
    {
        MFX_ZERO_MEM(vParam);
        MFX_CHECK_STS(pRender->GetVideoParam(&vParam));
        MFX_CHECK_STS(pRender->Reset(&vParam));
    }
    //MFX_CHECK_STS(pVPP->Reset(&m_RenParams.m_params));

    MFX_CHECK_STS(m_pControl->ResetAfterSeek());

    return seekSourceCommand::Execute();
}

//////////////////////////////////////////////////////////////////////////

skipCommand::skipCommand(IPipelineControl *pHolder) : commandBase(pHolder)
{
    m_iSkipLevel = 0;
}

void skipCommand::SetSkipLevel(const mfxI32 & fVal)
{
    m_iSkipLevel = fVal;
}

mfxStatus skipCommand::Execute()
{
    PipelineTrace((VM_STRING(" SkipLevel = %d\n"), m_iSkipLevel));

    IYUVSource *pSource;
    MFX_CHECK_POINTER(m_pControl);
    MFX_CHECK_STS(m_pControl->GetYUVSource(&pSource));

    ///*MFX_CHECK_STS*/(pSource->SetSkipMode(MFX_SKIPMODE_NOSKIP));

    for(int i = 0; i < abs(m_iSkipLevel); i++)
    {
        pSource->SetSkipMode(m_iSkipLevel > 0 ? MFX_SKIPMODE_MORE : MFX_SKIPMODE_LESS);
    }

    return MFX_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
resetEncCommand::resetEncCommand(IPipelineControl *pHolder)
: commandBase(pHolder)
{
}

void resetEncCommand::SetResetFileName(const tstring &new_file_name)
{
    m_NewFileName = new_file_name;
}

void resetEncCommand::SetResetInputFileName(const tstring &new_file_name)
{
    m_NewInputFileName = new_file_name;
}

void resetEncCommand::SetVppResizing(bool bUseResize)
{
    m_bUseResizing = bUseResize;
}

void    resetEncCommand::SetResetParams(mfxVideoParam * vParam, mfxVideoParam * vParamMask)
{
    if (NULL == vParam || NULL == vParamMask)
        return;

    memcpy(&m_NewParams, vParam, sizeof(*vParam));
    memcpy(&m_NewParamsMask, vParamMask, sizeof(*vParamMask));

    ippsAnd_8u_I((Ipp8u*)&m_NewParamsMask, (Ipp8u*)&m_NewParams, sizeof(m_NewParams));
    ippsNot_8u_I((Ipp8u*)&m_NewParamsMask, sizeof(m_NewParamsMask));
}

mfxStatus    resetEncCommand::Execute()
{
    PipelineTrace((VM_STRING("resetEncCommand::Execute invoked\n")));

    IMFXVideoRender *rnd;
    MFX_CHECK_STS(m_pControl->GetRender(&rnd));

    mfxVideoParam currentParams = {}, newParams;

    MFX_CHECK_STS(rnd->GetVideoParam(&currentParams));

    newParams = currentParams;
    {
        // save ext buffers if those aren't specified in reset params
        auto_ext_buffer_if_exist save_buffers(m_NewParams);

        //applying mask
        ippsAnd_8u_I((Ipp8u*)&m_NewParamsMask, (Ipp8u*)&newParams, sizeof(newParams));
        ippsOr_8u_I((Ipp8u*)&m_NewParams, (Ipp8u*)&newParams, sizeof(newParams));
    }

    //retrieving buffered frames
    MFX_CHECK_STS_SKIP(rnd->RenderFrame(NULL), MFX_ERR_MORE_DATA);

    //also should align height here with current PS
    newParams.mfx.FrameInfo.Height = mfx_align(newParams.mfx.FrameInfo.CropH, (newParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 0x10 : 0x20));

    //resolution also may change
    if (newParams.mfx.FrameInfo.CropW != currentParams.mfx.FrameInfo.CropW ||
        newParams.mfx.FrameInfo.CropH != currentParams.mfx.FrameInfo.CropH )
    {
        if (m_bUseResizing)
        {
            IMFXVideoVPP *pVpp;
            MFX_CHECK_STS(m_pControl->GetVPP(&pVpp));
            //vpp cannot be added now, it should be present initially
            MFX_CHECK_POINTER(pVpp);
            mfxVideoParam vParamVpp = {0};
            MFX_CHECK_STS(pVpp->GetVideoParam(&vParamVpp));
            vParamVpp.vpp.Out.Width  = newParams.mfx.FrameInfo.Width;
            vParamVpp.vpp.Out.Height = newParams.mfx.FrameInfo.Height;
            vParamVpp.vpp.Out.CropX  = newParams.mfx.FrameInfo.CropX;
            vParamVpp.vpp.Out.CropY  = newParams.mfx.FrameInfo.CropY;
            vParamVpp.vpp.Out.CropW  = newParams.mfx.FrameInfo.CropW;
            vParamVpp.vpp.Out.CropH  = newParams.mfx.FrameInfo.CropH;
            MFX_CHECK_STS(pVpp->Reset(&vParamVpp));

            //changing surfaces sizes only
            ComponentParams *pParams;
            MFX_CHECK_STS(m_pControl->GetVppParams(pParams));
            //modify surfaces info
            //TODO: surfacesidx might not be always zero
            ComponentParams::SurfacesContainer::iterator it =
                pParams->GetSurfaceAlloc(currentParams.mfx.FrameInfo.Width, currentParams.mfx.FrameInfo.Height);

            std::vector<SrfEncCtl>  & refSurfaces = it->surfaces;

            for (size_t j = 0; j < refSurfaces.size(); j++)
            {
                SrfEncCtl surf = refSurfaces[j];

                surf.pSurface->Info.Width  = newParams.mfx.FrameInfo.Width;
                surf.pSurface->Info.Height = newParams.mfx.FrameInfo.Height;
                surf.pSurface->Info.CropX  = newParams.mfx.FrameInfo.CropX;
                surf.pSurface->Info.CropY  = newParams.mfx.FrameInfo.CropY;
                surf.pSurface->Info.CropW  = newParams.mfx.FrameInfo.CropW;
                surf.pSurface->Info.CropH  = newParams.mfx.FrameInfo.CropH;
            }
        }
        else
        {
            IYUVSource *pSource;
            MFX_CHECK_STS(m_pControl->GetYUVSource(&pSource));

            pSource->Reset(&newParams);
        }
    }


    PipelineTrace((VM_STRING("\n------------------------------------------------\n")));
    PrintInfo(VM_STRING("MFXVideoEncode::Reset()"), VM_STRING(""));

    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_STS_SKIP(sts = rnd->Reset(&newParams), MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);


    //if reset failed with err incompatible params during command execution we
    //shouldn't reset whole pipeline, let's map status code to pipeline code
    MFX_CHECK_STS(sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM ? sts = PIPELINE_ERROR_RESET_FAILED : sts);


    //changing file name for downstream
    if (!m_NewFileName.empty())
    {
        IFile *pDownStream = NULL;
        MFX_CHECK_STS(rnd->GetDownStream(&pDownStream));

        MFX_CHECK_STS(pDownStream->Close());
        //TODO: hardcode type for NOW
        MFX_CHECK_STS(pDownStream->Open(m_NewFileName.c_str(), VM_STRING("wb")));

        PrintInfo(VM_STRING("New output file"), VM_STRING("%s\n"), m_NewFileName.c_str());
    }

    //WARNING: only works with YUV input
    if (!m_NewInputFileName.empty())
    {
        PrintInfo(VM_STRING("New input file"), m_NewInputFileName.c_str());

        IYUVSource *pYUVSurf;
        m_pControl->GetYUVSource(&pYUVSurf);
        MFX_CHECK_STS_SKIP(sts = pYUVSurf->Reset(&newParams), MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        IBitstreamReader *pReader;
        MFX_CHECK_STS(m_pControl->GetSplitter(&pReader));
        pReader->Close();
        MFX_CHECK_STS(pReader->Init(m_NewInputFileName.c_str()));
        MFX_CHECK_STS(m_pControl->ResetAfterSeek());
    }

    return MFX_ERR_NONE;
}

addExtBufferCommand::addExtBufferCommand(IPipelineControl *pHolder)
    : commandBase(pHolder)
{
}

mfxStatus    addExtBufferCommand::Execute()
{
    if (NULL == m_pBuf) {
        PipelineTrace((VM_STRING("ERROR: [addExtBufferCommand]:Execute buffer is NULL\n")));
        return MFX_ERR_NULL_PTR;
    }
    IVideoEncode *pEnc;
    ICurrentFrameControl *pCtrl;
    MFX_CHECK_STS(m_pControl->GetEncoder(&pEnc));
    MFX_CHECK_POINTER(pCtrl = pEnc->GetInterface<ICurrentFrameControl>());
    pCtrl->AddExtBuffer((mfxExtBuffer&)*m_pBuf);
    PipelineTrace((VM_STRING("ExtBufferAdded: \n%s\n"), Serialize(*m_pBuf).c_str()));

    return MFX_ERR_NONE;
}

void addExtBufferCommand::RegisterExtBuffer( const mfxExtBuffer & refBuf )
{
    m_bufData.resize(refBuf.BufferSz);
    m_pBuf = (mfxExtBuffer*)&m_bufData.front();
    memcpy(m_pBuf, &refBuf, m_bufData.size());
}

mfxStatus removeExtBufferCommand::Execute()
{
    IVideoEncode *pEnc;
    ICurrentFrameControl *pCtrl;
    MFX_CHECK_STS(m_pControl->GetEncoder(&pEnc));
    MFX_CHECK_POINTER(pCtrl = pEnc->GetInterface<ICurrentFrameControl>());

    pCtrl->RemoveExtBuffer(m_nBufferToRemove);
    PipelineTrace((VM_STRING("Reference List UnSelected\n")));

    return MFX_ERR_NONE;
}