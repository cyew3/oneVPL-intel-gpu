/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2012 Intel Corporation. All Rights Reserved.

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

IBitstreamConverterFactory * MFXPipelineFactory::CreateBitstreamCVTFactory( IPipelineObjectDesc * /*pParams*/)
{
    //registering converters defined somewhere
    std::auto_ptr<BitstreamConverterFactory>  fac(new BitstreamConverterFactory());

    fac->Register(new BSConvert<MFX_FOURCC_NV12, MFX_FOURCC_NV12>());
    fac->Register(new BSConvert<MFX_FOURCC_R16,  MFX_FOURCC_R16>());
    fac->Register(new BSConvert<MFX_FOURCC_YV12, MFX_FOURCC_NV12>());
    fac->Register(new BSConvert<MFX_FOURCC_YV12, MFX_FOURCC_YV12>());
    fac->Register(new BSConvert<MFX_FOURCC_RGB4, MFX_FOURCC_RGB4>());
    fac->Register(new BSConvert<MFX_FOURCC_RGB3, MFX_FOURCC_RGB4>());
    fac->Register(new BSConvert<MFX_FOURCC_RGB3, MFX_FOURCC_RGB3>());
    fac->Register(new BSConvert<MFX_FOURCC_YUY2, MFX_FOURCC_YUY2>());
    fac->Register(new BSConvert<MFX_FOURCC_P010, MFX_FOURCC_P010>());
    fac->Register(new BSConvert<MFX_FOURCC_NV16, MFX_FOURCC_NV16>());
    fac->Register(new BSConvert<MFX_FOURCC_P210, MFX_FOURCC_P210>());
    fac->Register(new BSConvert<MFX_FOURCC_Y410, MFX_FOURCC_Y410>());

    return fac.release();
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
            std::auto_ptr<IFile> ptr(create->m_pObject);
            return new SeparateFilesWriter(ptr);
        }
        case FILE_CRC :
        {
            std::auto_ptr<IFile> ptr(create->m_pObject);
            return new CRCFileWriter(create->sCrcFile, ptr);
        }
    default:
        return NULL;
    }   
}