/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_factory_default.h"
#include "mfx_renders.h"
#include "mfx_vpp.h"
#include "mfx_allocators_generic.h"
#include "mfx_bitstream_converters.h"
#include "mfx_file_generic.h"
#include "mfx_encode.h"
#include "mfx_video_session.h"
#include "mfx_null_file_writer.h"
#include "mfx_separate_file.h"
#include "mfx_crc_writer.h"
#include "mfx_svc_vpp.h"
#include "mfx_codec_plugin.h"

MFXPipelineFactory :: MFXPipelineFactory ()
{
    m_sharedRandom = mfx_make_shared<IPPBasedRandom>();
}

IVideoSession     * MFXPipelineFactory ::CreateVideoSession( IPipelineObjectDesc * pParams)
{
    switch (PipelineObjectDescPtr<>(pParams)->Type)
    {
        case VIDEO_SESSION_NATIVE:
        {
            return new MFXVideoSessionImpl();
        }

    default:
        return NULL;
    }
}

IYUVSource      * MFXPipelineFactory ::CreateDecode( const IPipelineObjectDesc & pParams)
{
    PipelineObjectDescPtr<IYUVSource> create(pParams);

    switch (create->Type)
    {
        case DECODER_MFX_NATIVE:
        {
            return new MFXDecoder(create->session);
        }
        case DECODER_MFX_PLUGIN_FILE:
        {
            return new MFXCodecPluginTmpl<MFXDecoder, MFXDecoderPlugin>(create->splugin, create->session);
        }
        case DECODER_MFX_PLUGIN_GUID:
        {
            return new MFXCodecPluginTmpl<MFXDecoder, MFXDecoderPlugin>(create->sPluginGuid, 1, create->session);
        }
    default:
        return NULL;
    }
}

IMFXVideoVPP    * MFXPipelineFactory ::CreateVPP( const IPipelineObjectDesc & pParams)
{
    PipelineObjectDescPtr<IMFXVideoVPP> create(pParams);

    switch (create->Type)
    {
        case VPP_MFX_NATIVE:
        {
            return new MFXVideoVPPImpl(create->session);
        }
        case VPP_SVC :
        {
            return new SVCedVpp(create->m_pObject);
        }
        case VPP_MFX_PLUGIN_FILE:
        {
            return new MFXCodecPluginTmpl<MFXVideoVPPImpl, MFXVPPPlugin>(create->splugin, create->session);
        }
        case VPP_MFX_PLUGIN_GUID:
        {
            return new MFXCodecPluginTmpl<MFXVideoVPPImpl, MFXVPPPlugin>(create->sPluginGuid, 1, create->session);
        }
    default:
        return NULL;
    }
}

IVideoEncode   * MFXPipelineFactory :: CreateVideoEncode ( IPipelineObjectDesc * pParams)
{
    PipelineObjectDescPtr<IVideoEncode> create(pParams);

    switch (create->Type)
    {
        case ENCODER_MFX_NATIVE :
        {
            return new MFXVideoEncode(create->session);
        }
        case ENCODER_MFX_PLUGIN_FILE:
        {
            return new MFXCodecPluginTmpl<MFXVideoEncode, MFXEncoderPlugin>(create->splugin, create->session);
        }
        case ENCODER_MFX_PLUGIN_GUID:
        {
            return new MFXCodecPluginTmpl<MFXVideoEncode, MFXEncoderPlugin>(create->sPluginGuid, 1, create->session);
        }

        default:
            return NULL;
    }
}

IMFXVideoRender * MFXPipelineFactory ::CreateRender( IPipelineObjectDesc * pParams)
{
    PipelineObjectDescPtr<IMFXVideoRender> create (pParams);

    switch (create->Type)
    {
        //TODO:encoder will be created in transcoder lib
        //case RENDER_MFX_ENCODER:
        //{
        //    return new MFXEncodeWRAPPER(create->m_refParams, create->m_status);
        //}
        case RENDER_FWR :
        {
            return NULL;//new MFXFileWriteRender(create->m_core, create->m_status);
        }
    default:
        return NULL;
    }
}

IAllocatorFactory * MFXPipelineFactory::CreateAllocatorFactory( IPipelineObjectDesc * /*pParams*/)
{
    return new DefaultAllocatorFactory();
}

std::unique_ptr<IBitstreamConverterFactory> MFXPipelineFactory::CreateBitstreamCVTFactory( IPipelineObjectDesc * /*pParams*/)
{
    //registering converters defined somewhere
    std::unique_ptr<BitstreamConverterFactory> fac(new BitstreamConverterFactory());

    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_NV12, MFX_FOURCC_NV12)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_R16, MFX_FOURCC_R16)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_YV12, MFX_FOURCC_NV12)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_YV12, MFX_FOURCC_YV12)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_RGB4, MFX_FOURCC_RGB4)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_BGR4, MFX_FOURCC_BGR4)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_RGB4, MFX_FOURCC_BGR4)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_BGR4, MFX_FOURCC_RGB4)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_RGB3, MFX_FOURCC_RGB4)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_RGB3, MFX_FOURCC_RGB3)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_YUY2, MFX_FOURCC_YUY2)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_P010, MFX_FOURCC_P010)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_P210, MFX_FOURCC_P210)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_NV16, MFX_FOURCC_NV16)));

    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_YUV444_8, MFX_FOURCC_AYUV)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_AYUV, MFX_FOURCC_AYUV)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_A2RGB10, MFX_FOURCC_A2RGB10)));

#if (MFX_VERSION >= 1027)
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_Y210, MFX_FOURCC_Y210)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_Y410, MFX_FOURCC_Y410)));
#endif
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_P016, MFX_FOURCC_P016)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_Y216, MFX_FOURCC_Y216)));
    fac->Register(std::unique_ptr<IBitstreamConverter>(new DECL_CONVERTER(MFX_FOURCC_Y416, MFX_FOURCC_Y416)));
#endif

    return std::move(fac);
}

IFile * MFXPipelineFactory::CreatePARReader(IPipelineObjectDesc * /*pParams*/)
{
    return new GenericFile();
}

mfx_shared_ptr<IRandom> MFXPipelineFactory::CreateRandomizer()
{
    return m_sharedRandom;
}

IFile * MFXPipelineFactory::CreateFileWriter( IPipelineObjectDesc * pParams)
{
    PipelineObjectDescPtr<IFile> create (pParams);

    switch (create->Type)
    {
        case FILE_NULL :
        {
            return new NullFileWriter();
        }
        case FILE_GENERIC :
        {
            return new GenericFile();
        }
        case FILE_SEPARATE :
        {
            std::unique_ptr<IFile> ptr(create->m_pObject);
            return new SeparateFilesWriter(std::move(ptr));
        }
        case FILE_CRC :
        {
            std::unique_ptr<IFile> ptr(create->m_pObject);
            return new CRCFileWriter(create->sCrcFile, std::move(ptr));
        }
    default:
        return NULL;
    }
}
