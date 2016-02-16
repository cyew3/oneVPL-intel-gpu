/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2016 Intel Corporation. All Rights Reserved.

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
    DECODER_MFX_PLUGIN_FILE, //create mediasdk decoder plugin based on filename
    DECODER_MFX_PLUGIN_GUID,
    DECODER_LAST_IDX //end of enum indicator
};

enum /*Vpp Type*/
{
    VPP_MFX_NATIVE = DECODER_LAST_IDX,
    VPP_PLUGIN_AS_VPP,
    VPP_SVC,
    VPP_MFX_PLUGIN_FILE, //create mediasdk vpp plugin based on filename
    VPP_MFX_PLUGIN_GUID,
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
    ENCODER_MFX_PLUGIN_FILE,
    ENCODER_MFX_PLUGIN_GUID,
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
    PipelineObjectBaseTmpl(){}
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
    PipelineObjectDesc(){}
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
    MFXPipelineObjectDesc(){}
    MFXPipelineObjectDesc(mfxSession ext_session, const tstring &plugin_dll_name, int objType, T *pObj)
        : PipelineObjectBaseTmpl<T>(objType, pObj)
        , splugin (plugin_dll_name)
        , session(ext_session)
    {
    }

    MFXPipelineObjectDesc(mfxSession ext_session, const tstring &plugin_guid, mfxU32 plugin_version, int objType, T *pObj)
        : PipelineObjectBaseTmpl<T>(objType, pObj)
        , sPluginGuid (plugin_guid)
        , PluginVersion (plugin_version)
        , session(ext_session)
    {
    }
    //plugin filename
    tstring splugin;

    //plugin GUID
    tstring sPluginGuid;
    mfxU32  PluginVersion;

    //
    mfxSession session;
};

#define DECL_MEDIASDK_SPEC(class_type)\
template<> class PipelineObjectDesc<class_type> : public MFXPipelineObjectDesc<class_type>\
{\
public:\
    PipelineObjectDesc(){}\
    PipelineObjectDesc(mfxSession ext_session\
        , const tstring &plugin_dll_name\
        , int objType\
        , class_type *pObj)\
        : MFXPipelineObjectDesc<class_type>(ext_session, plugin_dll_name, objType, pObj){}\
    PipelineObjectDesc(mfxSession ext_session\
        , const tstring &uid\
        , mfxU32 version\
        , int objType\
        , class_type *pObj)\
        : MFXPipelineObjectDesc<class_type>(ext_session, uid, version, objType, pObj){}\
};

//mediasdk specialization
DECL_MEDIASDK_SPEC(IYUVSource);
DECL_MEDIASDK_SPEC(IMFXVideoVPP);
DECL_MEDIASDK_SPEC(IVideoEncode);

template <class T>
PipelineObjectDesc<T> make_wrapper(int WrapperId, T * obj)
{
    PipelineObjectDesc<T> dsc(NULL, VM_STRING(""), WrapperId, obj);
    return dsc;
}

template <class T>
PipelineObjectDesc<T> make_wrapper(mfxSession ext_session, const tstring &plugin_dll_name, int WrapperId, T * obj)
{
    PipelineObjectDesc<T> dsc(ext_session, plugin_dll_name, WrapperId, obj);
    return dsc;
}

template <class T>
PipelineObjectDesc<T> make_wrapper(mfxSession ext_session, const tstring &uid, mfxU32 version, int WrapperId, T * obj)
{
    PipelineObjectDesc<T> dsc(ext_session, uid, version, WrapperId, obj);
    return dsc;
}

//rest of pipeline objects - session not required for creation
template<> 
class PipelineObjectDesc<IFile> : public PipelineObjectBaseTmpl<IFile>
{
public:
    PipelineObjectDesc(const tstring &crcFile, int objType)
        //TODO: fix memory leaks by deferring this release as much as possible
        : PipelineObjectBaseTmpl<IFile>(objType, NULL)
        , sCrcFile(crcFile)
    {
    }
    PipelineObjectDesc(const tstring &crcFile, int objType, std::auto_ptr<IFile>& pObj)
        //TODO: fix memory leaks by deferring this release as much as possible
        : PipelineObjectBaseTmpl<IFile>(objType, pObj.release())
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
    virtual IYUVSource          * CreateDecode( const IPipelineObjectDesc & pParams);
    virtual IMFXVideoRender     * CreateRender( IPipelineObjectDesc * pParams);
    virtual IFile               * CreateFileWriter( IPipelineObjectDesc * pParams);
    virtual IVideoEncode        * CreateVideoEncode ( IPipelineObjectDesc * pParams);
    virtual IAllocatorFactory   * CreateAllocatorFactory( IPipelineObjectDesc * pParams);
    virtual IBitstreamConverterFactory * CreateBitstreamCVTFactory( IPipelineObjectDesc * pParams);
    virtual IFile *               CreatePARReader(IPipelineObjectDesc * pParams);
};