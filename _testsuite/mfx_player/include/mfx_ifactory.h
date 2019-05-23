/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma  once

#include "mfx_ifile.h"
#include "mfx_ivpp.h"
#include "mfx_ivideo_render.h"
#include "mfx_iyuv_source.h"
#include "mfx_iallocator_factory.h"
#include "mfx_ibitstream_converter.h"
#include "mfx_ivideo_encode.h"
#include "mfx_iproxy.h"
#include "mfx_ivideo_session.h"
#include "mfx_irandom.h"
#include "mfx_shared_ptr.h"

//abstraction for creation params
class IPipelineObjectDesc
{
public:
    virtual ~IPipelineObjectDesc(){}
};

//factory that  creates pipeline components
class IMFXPipelineFactory : public EnableProxyForThis<IMFXPipelineFactory>
{
public:
    virtual mfx_shared_ptr<IRandom> CreateRandomizer() = 0;
    virtual IVideoSession     * CreateVideoSession( IPipelineObjectDesc * pParams) = 0;
    virtual IMFXVideoVPP      * CreateVPP   ( const IPipelineObjectDesc & pParams) = 0;
    virtual IYUVSource        * CreateDecode( const IPipelineObjectDesc & pParams) = 0;
    virtual IMFXVideoRender   * CreateRender( IPipelineObjectDesc * pParams) = 0;
    virtual IFile             * CreateFileWriter( IPipelineObjectDesc * pParams) = 0;
    //decorators
    virtual IVideoEncode      * CreateVideoEncode ( IPipelineObjectDesc * pParams) = 0;
    virtual IAllocatorFactory * CreateAllocatorFactory( IPipelineObjectDesc * pParams) = 0;
    //bitstream converter object used by source reader
    virtual  std::unique_ptr<IBitstreamConverterFactory> CreateBitstreamCVTFactory( IPipelineObjectDesc * pParams) = 0;
    virtual IFile *             CreatePARReader(IPipelineObjectDesc * pParams) = 0;
};

//proxy declaration
template<>
class InterfaceProxy <IMFXPipelineFactory>
    : public InterfaceProxyBase<IMFXPipelineFactory>
{
    typedef InterfaceProxyBase<IMFXPipelineFactory> base;

public:
    InterfaceProxy(std::auto_ptr<IMFXPipelineFactory> & pActual)
        : base(pActual)
    {
    }
    virtual mfx_shared_ptr<IRandom> CreateRandomizer()
    {
        return Target().CreateRandomizer();
    }
    virtual IVideoSession     * CreateVideoSession( IPipelineObjectDesc * pParams)
    {
        return m_pTarget->CreateVideoSession(pParams);
    }
    virtual IMFXVideoVPP      * CreateVPP   ( const IPipelineObjectDesc & pParams)
    {
        return m_pTarget->CreateVPP(pParams);
    }
    virtual IYUVSource        * CreateDecode( const IPipelineObjectDesc & pParams) 
    {
        return m_pTarget->CreateDecode(pParams);
    }    
    virtual IMFXVideoRender   * CreateRender( IPipelineObjectDesc * pParams) 
    {
        return m_pTarget->CreateRender(pParams);
    }
    virtual IFile * CreateFileWriter( IPipelineObjectDesc * pParams)
    {
        return m_pTarget->CreateFileWriter(pParams);
    }
    virtual IVideoEncode   * CreateVideoEncode ( IPipelineObjectDesc * pParams) 
    {
        return m_pTarget->CreateVideoEncode(pParams);
    }
    virtual IAllocatorFactory * CreateAllocatorFactory( IPipelineObjectDesc * pParams) 
    {
        return m_pTarget->CreateAllocatorFactory(pParams);
    }
    virtual std::unique_ptr<IBitstreamConverterFactory> CreateBitstreamCVTFactory( IPipelineObjectDesc * pParams)
    {
        return m_pTarget->CreateBitstreamCVTFactory(pParams);
    }
    virtual IFile * CreatePARReader(IPipelineObjectDesc * pParams) 
    {
        return m_pTarget->CreatePARReader(pParams);
    }
};