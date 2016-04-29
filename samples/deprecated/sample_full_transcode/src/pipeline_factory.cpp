/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "mfx_samples_config.h"

#include "pipeline_factory.h"
#include "sysmem_allocator.h"
#include "fileio.h"
#include "cmdline_options.h"
#include "exceptions.h"

#include "itransform.h"
#include "fileio_wrapper.h"
#include "output_format.h"
#include "trace_levels.h"

#if defined(_WIN32) || defined(_WIN64)
#include "d3d_device.h"
#include "d3d_allocator.h"
#include "d3d11_device.h"
#include "d3d11_allocator.h"
#endif

#if defined(LIBVA_SUPPORT)
#include "vaapi_device.h"
#include "vaapi_allocator.h"
#endif


PipelineFactory::PipelineFactory(){
}

PipelineFactory::~PipelineFactory(){
}

MFXVideoSessionExt* PipelineFactory::CreateVideoSession()
{
    return new MFXVideoSessionExt(*this);
}

MFXVideoDECODE* PipelineFactory::CreateVideoDecoder(MFXVideoSessionExt& session)
{
    return new MFXVideoDECODE(session);
}

MFXVideoENCODE* PipelineFactory::CreateVideoEncoder(MFXVideoSessionExt& session)
{
    return new MFXVideoENCODE(session);
}

MFXVideoVPP* PipelineFactory::CreateVideoVPP(MFXVideoSessionExt& session)
{
    return new MFXVideoVPP(session);
}

ITransform*  PipelineFactory::CreateVideoVPPTransform(MFXVideoSessionExt& session, int timeout) {
    return new Transform<MFXVideoVPP>(*this, session, timeout);
}

MFXAudioSession* PipelineFactory::CreateAudioSession(){
    return new MFXAudioSession();
}

MFXAudioDECODE* PipelineFactory::CreateAudioDecoder(MFXAudioSession& session) {
    return new MFXAudioDECODE(session);
}

ITransform* PipelineFactory::CreateAudioDecoderTransform(MFXAudioSession& session, int timeout) {
    return new Transform<MFXAudioDECODE>(*this, session, timeout);
}

ITransform* PipelineFactory::CreateAudioDecoderNullTransform(MFXAudioSession& session, int timeout) {
    return new NullTransform<MFXAudioDECODE>(*this, session, timeout);
}

MFXAudioENCODE* PipelineFactory::CreateAudioEncoder(MFXAudioSession& session)
{
    return new MFXAudioENCODE(session);
}

ITransform* PipelineFactory::CreateAudioEncoderTransform(MFXAudioSession& session, int timeout) {
    return new Transform<MFXAudioENCODE>(*this, session, timeout);
}

ITransform* PipelineFactory::CreateAudioEncoderNullTransform(MFXAudioSession& session, int timeout) {
    return new NullTransform<MFXAudioENCODE>(*this, session, timeout);
}

BaseFrameAllocator* PipelineFactory::CreateFrameAllocator(AllocatorImpl impl)
{
    BaseFrameAllocator* allocator = 0;
    switch (impl)
    {
    case ALLOC_IMPL_SYSTEM_MEMORY:
        allocator = new SysMemFrameAllocator();
        break;

    case ALLOC_IMPL_D3D9_MEMORY:
#if defined(_WIN32) || defined(_WIN64)
        allocator = new D3DFrameAllocator();
#elif defined(LIBVA_SUPPORT)
        allocator = new vaapiFrameAllocator();
#else
        MSDK_TRACE_ERROR(MSDK_STRING(MSDK_STRING("VAAPI not supported")));
        throw AllocatorTypeNotSupportedError();
#endif
        break;
    case ALLOC_IMPL_D3D11_MEMORY:
        #if MFX_D3D11_SUPPORT
        allocator = new D3D11FrameAllocator();
        #else
        MSDK_TRACE_ERROR(MSDK_STRING("D3D11 not supported"));
        throw D3D11NotSupportedError();
        #endif
        break;
    default:
        MSDK_TRACE_ERROR(MSDK_STRING("Allocator type: ")<<impl<<MSDK_STRING("not supported"));
        throw AllocatorTypeNotSupportedError();
    }
    return allocator;
}

MFXMuxer* PipelineFactory::CreateMuxer()
{
    return new MFXMuxer();
}

MuxerWrapper* PipelineFactory::CreateMuxerWrapper(MFXStreamParamsExt & sparams, std::auto_ptr<MFXDataIO>& io)
{
    std::auto_ptr<MFXMuxer> mux(CreateMuxer());

    return new MuxerWrapper(mux, sparams, io);
}

MFXSplitter* PipelineFactory::CreateSplitter()
{
    return new MFXSplitter();
}

ISplitterWrapper* PipelineFactory::CreateSplitterWrapper(std::auto_ptr<MFXDataIO>&io)
{
    std::auto_ptr<MFXSplitter> spl(CreateSplitter());
    return new SplitterWrapper(spl, io);
}

ISplitterWrapper* PipelineFactory::CreateCircularSplitterWrapper(std::auto_ptr<MFXDataIO>&io, mfxU64 nLimit)
{
    std::auto_ptr<MFXSplitter> spl(CreateSplitter());
    return reinterpret_cast<ISplitterWrapper*>(new CircularSplitterWrapper(spl, io, nLimit));
}

MFXDataIO* PipelineFactory::CreateFileIO(const msdk_string& file, const msdk_string& params) {
    return new FileIO(file, params);
}

ISink* PipelineFactory::CreateFileIOWrapper(const msdk_string& path, const msdk_string& params) {
    std::auto_ptr<MFXDataIO> fileio(CreateFileIO(path, params));
    return new DataSink(fileio);
}

CmdLineParser* PipelineFactory::CreateCmdLineParser()
{
    msdk_stringstream HelpHead ;
    HelpHead <<MSDK_STRING("Full Transcoding Sample Version ") << MSDK_SAMPLE_VERSION << std::endl << std::endl <<
               MSDK_STRING("Command line parameters:\n") <<
               MSDK_STRING("Usage: sample_full_transcode.exe ") << OPTION_I <<MSDK_STRING(" InputFile ")<< OPTION_O << MSDK_STRING(" OutputFile [options]\n")<<
               MSDK_STRING("Where [options] can be:\n");

    msdk_stringstream UsageExamples;
    UsageExamples << MSDK_STRING("\nExample:\n") << OPTION_I << MSDK_STRING(" in.m2ts ") << OPTION_O << MSDK_STRING(" out.mp4 ")<< OPTION_VB << MSDK_STRING(" 1000 ") <<
        OPTION_AB << OPTION_SW << MSDK_STRING(" 16") << std::endl
        << OPTION_I << MSDK_STRING(" in.mp4 ") << OPTION_O << MSDK_STRING(" out.m2ts ") << OPTION_VB << MSDK_STRING(" 2000 ") << OPTION_AB << MSDK_STRING(" 52 ") <<
        OPTION_D3D11 << MSDK_STRING(" ") << OPTION_HW << std::endl
        << OPTION_I << MSDK_STRING(" in.mp4 ")<< OPTION_O << MSDK_STRING(" out.abc ") << OPTION_VB << MSDK_STRING(" 2000 ") << OPTION_AB << MSDK_STRING(" 52 ") <<
        OPTION_D3D11 << MSDK_STRING(" ") << OPTION_HW << MSDK_STRING(" ") << OPTION_FORMAT << MSDK_STRING(" m2ts");

    OutputCodec c;
    msdk_stringstream supported_acodec ;
    msdk_stringstream supported_vcodec ;
    supported_acodec << MSDK_CHAR("Audio codec type, supported types: ") << c.ACodec();
    supported_vcodec << MSDK_CHAR("Video codec type, supported types: ") << c.VCodec();

    msdk_stringstream trace_levels_str ;
    trace_levels_str<<MSDK_STRING("set application tracing level: ") << TraceLevels();


    return ::CreateCmdLineParser(Options()
        (ArgHandler<msdk_string>(OPTION_I, MSDK_CHAR("Input file")))
        (ArgHandler<msdk_string>(OPTION_O, MSDK_CHAR("Output file")))
        //(ArgHandler<int>(OPTION_W, MSDK_CHAR("scale input with new frame width")))
        //(ArgHandler<int>(OPTION_H, MSDK_CHAR("scale input with new frame height")))
        (ArgHandler<int>(OPTION_ADEPTH, MSDK_CHAR("Async depth")))
#if defined(MFX_D3D11_SUPPORT)
        (ArgHandler<bool>(OPTION_D3D11, MSDK_CHAR("D3D11 Memory")))
#endif
        (ArgHandler<bool>(OPTION_HW, MSDK_CHAR("Use hardware video library")))
        (ArgHandler<bool>(OPTION_SW, MSDK_CHAR("Use software video library, otherwise hardware library selected")))
        //(ArgHandler<bool>(OPTION_ASW, MSDK_CHAR("Use software audio library, by default it is same as video")))
        (ArgHandler<msdk_string>(OPTION_ACODEC, supported_acodec.str()))
        (ArgHandler<msdk_string>(OPTION_VCODEC, supported_vcodec.str()))
        (ArgHandler<bool>(OPTION_ACODEC_COPY, MSDK_CHAR("Copy audio without transcoding")))
        //(ArgHandler<msdk_string>(OPTION_PLG, MSDK_CHAR("Generic MediaSDK plugin")))
        //(ArgHandler<msdk_string>(OPTION_VDECPLG, MSDK_CHAR("MediaSDK decoder plugin")))
        //(ArgHandler<msdk_string>(OPTION_VENCPLG, MSDK_CHAR("MediaSDK encoder plugin")))
        (ArgHandler<kbps_t>(OPTION_VB, MSDK_CHAR("Video bitrate")))
        (ArgHandler<kbps_t>(OPTION_AB, MSDK_CHAR("Audio bitrate")))
        (ArgHandler<msdk_string>(OPTION_FORMAT, MSDK_CHAR("Output container: mp4, m2ts")))
        (ArgHandler<mfxU64>(OPTION_LOOP, MSDK_CHAR("Transcoding in loop, seconds")))
        (ArgHandler<msdk_string>(OPTION_TRACELEVEL, trace_levels_str.str())),
        HelpHead.str(),
        UsageExamples.str()
        );
}

SamplePool* PipelineFactory::CreateSamplePool(int nFindFreeTimeout)
{
    return new SamplePool(nFindFreeTimeout);
}

TransformStorage* PipelineFactory::CreateTransformStorage()
{
    return new TransformStorage();
}

ITransform* PipelineFactory::CreateVideoDecoderTransform(MFXVideoSessionExt& session, int TimeToWait)
{
    return new Transform<MFXVideoDECODE>(*this, session, TimeToWait);
}

ITransform* PipelineFactory::CreateVideoEncoderTransform(MFXVideoSessionExt& session, int ttl) {
    return new Transform<MFXVideoENCODE>(*this, session, ttl);
}

IPipelineProfile* PipelineFactory::CreatePipelineProfile(const MFXStreamParams &splInfo, CmdLineParser & parser) {
    OutputFormat extensions;
    OutputCodec codecs;
    return new PipelineProfile(splInfo, parser, extensions, codecs);
}

SessionStorage * PipelineFactory::CreateSessionStorage( const MFXSessionInfo &vSession, const MFXSessionInfo &aSession ) {
    return new SessionStorage(*this, vSession, aSession);
}

MFXVideoUSER* PipelineFactory::CreateVideoUserModule(MFXVideoSessionExt& session) {
    return new MFXVideoUSER(session);
}

/*
MFXPlugin* PipelineFactory::CreatePlugin(mfxPluginType type, MFXVideoUSER *usrModule, const msdk_string & pluginFullPath) {
    return LoadPlugin(type, usrModule, pluginFullPath);
}*/

ITransform* PipelineFactory::CreatePluginTransform(MFXVideoSessionExt &session, mfxPluginType type, const msdk_string & pluginFullPath, int timeout) {

    return new Transform<MFXVideoUSER>(*this, session, type, pluginFullPath, timeout);
}

CHWDevice*  PipelineFactory::CreateHardwareDevice(AllocatorImpl impl ){
    switch (impl)
    {
    case ALLOC_IMPL_D3D9_MEMORY:
#if defined(_WIN32) || defined(_WIN64)
        return new CD3D9Device();
#elif defined(LIBVA_SUPPORT)
        return CreateVAAPIDevice();
#else
        MSDK_TRACE_ERROR(MSDK_STRING("VAAPI devise not supported "));
        throw HWDeviceTypeNotSupportedError();
#endif // defined(_WIN32) || defined(_WIN64)
        break;
    case ALLOC_IMPL_D3D11_MEMORY:
#if MFX_D3D11_SUPPORT
        return new CD3D11Device();
#else
        MSDK_TRACE_ERROR(MSDK_STRING("D3D11 not supported"));
        throw D3D11NotSupportedError();
#endif // MFX_D3D11_SUPPORT
        break;
    default:
        MSDK_TRACE_ERROR(MSDK_STRING("Device type: ")<<impl<<MSDK_STRING("not supported"));
        throw HWDeviceTypeNotSupportedError();
    }
}

mfxAllocatorParams* PipelineFactory::CreateAllocatorParam(CHWDevice* device, AllocatorImpl impl) {
    mfxAllocatorParams* params = 0;
    switch (impl)
    {
#if defined(_WIN32) || defined(_WIN64)
    case ALLOC_IMPL_D3D9_MEMORY:
    {
        mfxHDL hdl;
        mfxStatus sts = MFX_ERR_NONE;
        params = new D3DAllocatorParams;
        sts = device->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, &hdl);
        if (sts < 0) {
            MSDK_TRACE_ERROR(MSDK_STRING("D3DDevice::GetHandle, sts=")<<sts);
            if (params)
                delete params;
            throw FactoryGetHWHandleError();
        }
        reinterpret_cast<D3DAllocatorParams*>(params)->pManager = reinterpret_cast<IDirect3DDeviceManager9*>(hdl);
        break;
    }
    case ALLOC_IMPL_D3D11_MEMORY:
    {
#if MFX_D3D11_SUPPORT
        mfxHDL hdl;
        mfxStatus sts = MFX_ERR_NONE;
        params = new D3D11AllocatorParams;
        sts = device->GetHandle(MFX_HANDLE_D3D11_DEVICE, &hdl);
        if (sts < 0) {
            MSDK_TRACE_ERROR(MSDK_STRING("D3D11Device::GetHandle, sts=")<<sts);
            if (params)
                delete params;
            throw FactoryGetHWHandleError();
        }
        reinterpret_cast<D3D11AllocatorParams*>(params)->pDevice = reinterpret_cast<ID3D11Device*>(hdl);
#else
        MSDK_TRACE_ERROR(MSDK_STRING("D3D11 not supported"));
        throw D3D11NotSupportedError();
#endif // MFX_D3D11_SUPPORT
        break;
    }
#else
    case ALLOC_IMPL_D3D9_MEMORY:
    {
#if defined(LIBVA_SUPPORT)
        mfxHDL hdl;
        mfxStatus sts = MFX_ERR_NONE;
        params = new vaapiAllocatorParams;
        sts = device->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
        if (sts < 0) {
            MSDK_TRACE_ERROR(MSDK_STRING("VAAPIDevice::GetHandle, sts=")<<sts);
            if (params)
                delete params;
            throw FactoryGetHWHandleError();
        }
        reinterpret_cast<vaapiAllocatorParams*>(params)->m_dpy = reinterpret_cast<VADisplay>(hdl);
        break;
#else
        MSDK_TRACE_ERROR(MSDK_STRING(MSDK_STRING("VAAPI not supported")));
        throw AllocatorTypeNotSupportedError();
#endif // LIBVA_SUPPORT
    }
#endif // defined(_WIN32) || defined(_WIN64)
    default: {
        params = new SysMemAllocatorParams();
        static_cast<SysMemAllocatorParams*>(params)->pBufferAllocator = 0;
             }
    }
    return params;
}
