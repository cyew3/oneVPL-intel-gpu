/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ifactory.h"

//first attempt of parameterizing factory by introducing components types 
//doesn't allow to extend without factory recompilation
enum
{
    UKNOWN_COMPONENT = 0,
    UKNOWN_COMPONENT_LAST_IDX
};

enum /*Decoder type*/
{
    DECODER_MFX_NATIVE = UKNOWN_COMPONENT_LAST_IDX,
    DECODER_BUFFERED,
    DECODER_ADVANCE, // decodes frames in advance
    DECODER_YUV_NATIVE,//decodes only yuv files

    DECODER_LAST_IDX //end of enum indicator
};

enum /*Vpp Type*/
{
    VPP_MFX_NATIVE = DECODER_LAST_IDX,
    VPP_PLUGIN_AS_VPP,
    VPP_SVC,

    VPP_LAST_IDX //end of enum indicator
};


enum /*Render Type*/
{
    RENDER_D3D = VPP_LAST_IDX,
    RENDER_MFX_ENCODER,
    RENDER_VIEW_ORDERED,
    RENDER_ENC_ORDERED,
    RENDER_MULTI,
    RENDER_CRC,
    RENDER_FWR,
    RENDER_METRIC_CALC,
    RENDER_NULL,
    RENDER_BMP,
    RENDER_ENCODE_DECODE,
    RENDER_OUTLINE,
    
    
    //idx 
    RENDER_LAST_IDX,
};

enum /*Render Type*/
{
    ENCODER_MFX_NATIVE = RENDER_LAST_IDX,
    ENCODER_LAST_IDX 
};

enum /*Session Type*/
{
    VIDEO_SESSION_NATIVE = ENCODER_LAST_IDX,
    VIDEO_SESSION_LAST_IDX
};

enum 
{
    FILE_NULL = VIDEO_SESSION_LAST_IDX,
    FILE_GENERIC,
    FILE_SEPARATE,
    FILE_CRC,
    
    //idx
    FILE_LAST_IDX
};

//contains only object type
class PipelineObjectDescBase
    : public IPipelineObjectDesc
{
public:
    int Type;

    PipelineObjectDescBase(int objType = UKNOWN_COMPONENT)
        : Type (objType)
    {
    }
};

//in most case it is required to create wrapper over existing
template<class T>
class PipelineObjectBaseTmpl
    : public PipelineObjectDescBase
{
public:
    PipelineObjectBaseTmpl(int objType, T *pObj)
        : PipelineObjectDescBase(objType)
        , m_pObject(pObj)
    {
    }
    T * m_pObject;
};

template<class T>
class PipelineObjectDesc
    : public PipelineObjectBaseTmpl<T>
{
public:
    PipelineObjectDesc(int objType, T *pObj)
        : PipelineObjectBaseTmpl<T>(objType, pObj)
    {
    }
    mfxSession session;
};

//generic specialization for mediasdk related types
//has session
template<class T>
class MFXPipelineObjectDesc
    : public PipelineObjectBaseTmpl<T>
{
public:
    MFXPipelineObjectDesc(mfxSession ext_session, int objType, T *pObj)
        : PipelineObjectBaseTmpl<T>(objType, pObj)
        , session(ext_session)
    {
    }
    mfxSession session;
};

#define DECL_MEDIASDK_SPEC(class_type)\
template<> class PipelineObjectDesc<class_type> : public MFXPipelineObjectDesc<class_type>\
{\
public:\
    PipelineObjectDesc(mfxSession ext_session, int objType, class_type *pObj)\
        : MFXPipelineObjectDesc<class_type>(ext_session, objType, pObj){}\
};

//mediasdk specialization
DECL_MEDIASDK_SPEC(IYUVSource);
DECL_MEDIASDK_SPEC(IMFXVideoVPP);
DECL_MEDIASDK_SPEC(IVideoEncode);

template <class T>
PipelineObjectDesc<T> make_wrapper(int WrapperId, T * obj)
{
    PipelineObjectDesc<T> dsc(NULL, WrapperId, obj);
    return dsc;
}

//rest of pipeline objects - session not required for creation
template<> 
class PipelineObjectDesc<IFile> : public PipelineObjectBaseTmpl<IFile>
{
public:
    PipelineObjectDesc(const tstring &crcFile, int objType, IFile *pObj)
        : PipelineObjectBaseTmpl<IFile>(objType, pObj)
        , sCrcFile(crcFile)
    {
    }
    tstring sCrcFile;
};

//rationale: accept any desc pointer and cast to templated version
template <class T = void>
class PipelineObjectDescPtr 
{
    const PipelineObjectDesc<T> *p;
public:
    PipelineObjectDescPtr(const IPipelineObjectDesc & obj)
        : p(dynamic_cast<const PipelineObjectDesc<T>*>(&obj))
    {
    }
    PipelineObjectDescPtr(IPipelineObjectDesc * obj)
        : p(dynamic_cast<PipelineObjectDesc<T>*>(obj))
    {
    }
    const  PipelineObjectDesc<T> * operator ->() const
    {
        return p;
    }
};

template <>
class PipelineObjectDescPtr<void> 
{
    PipelineObjectDescBase * p;
public:
    PipelineObjectDescPtr(IPipelineObjectDesc * obj)
        : p(dynamic_cast<PipelineObjectDescBase*>(obj))
    {
    }
    const  PipelineObjectDescBase * operator ->() const
    {
        return p;
    }
};


////TODO: remove specialization for encoder
//#include "mfx_component_params.h"
//template<>
//class PipelineObjectPtr<IMFXVideoRender>
//    : public PipelineObjectDescBase
//{
//public:
//    PipelineObjectPtr<IMFXVideoRender>(IVideoSession *core, int objType, ComponentParams &refParams, mfxStatus *status)
//        : PipelineObjectDescBase(core->GetMFXSession(), objType)
//        , m_refParams(refParams)
//        , m_status (status)
//        , m_core(core)
//    {
//    }
//
//    IVideoSession   * m_core;
//    ComponentParams & m_refParams;
//    mfxStatus       * m_status;
//    IMFXVideoRender * m_pObject;
//    
//    void operator = (const PipelineObjectPtr&){}
//};

//non configurable factory yet
//class DefaultFactory

class MFXPipelineFactory 
    : public IMFXPipelineFactory
{
    mfx_shared_ptr<IRandom> m_sharedRandom;
public:
    MFXPipelineFactory ();

    virtual mfx_shared_ptr<IRandom> CreateRandomizer();
    virtual IVideoSession       * CreateVideoSession( IPipelineObjectDesc * pParams);
    virtual IMFXVideoVPP        * CreateVPP( const IPipelineObjectDesc & pParams);
    virtual IYUVSource          * CreateDecode( IPipelineObjectDesc * pParams);
    virtual IMFXVideoRender     * CreateRender( IPipelineObjectDesc * pParams);
    virtual IFile               * CreateFileWriter( IPipelineObjectDesc * pParams);
    virtual IVideoEncode        * CreateVideoEncode ( IPipelineObjectDesc * pParams);
    virtual IAllocatorFactory   * CreateAllocatorFactory( IPipelineObjectDesc * pParams);
    virtual IBitstreamConverterFactory * CreateBitstreamCVTFactory( IPipelineObjectDesc * pParams);
    virtual IFile *               CreatePARReader(IPipelineObjectDesc * pParams);
};