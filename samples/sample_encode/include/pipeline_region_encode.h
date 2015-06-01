//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2005-2014 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __PIPELINE_REGION_ENCODE_H__
#define __PIPELINE_REGION_ENCODE_H__

#include "pipeline_encode.h"

class CMSDKResource
{
public:
    CMSDKResource()
    {
        pEncoder=NULL;
        pPlugin=NULL;
    }

    MFXVideoSession Session;
    MFXVideoENCODE* pEncoder;
    MFXPlugin* pPlugin;
    CEncTaskPool TaskPool;
};

class CResourcesPool
{
public:
    CResourcesPool()
    {
        size=0;
        m_resources=NULL;
    }

    ~CResourcesPool()
    {
        delete[] m_resources;
    }

    CMSDKResource& operator[](int index){return m_resources[index];}

    int GetSize(){return size;}

    mfxStatus Init(int size,mfxIMPL impl, mfxVersion *pVer)
    {
        MSDK_CHECK_NOT_EQUAL(m_resources, NULL , MFX_ERR_INVALID_HANDLE);
        this->size=size;
        m_resources = new CMSDKResource[size];
        for (int i = 0; i < size; i++)
        {
            mfxStatus sts = m_resources[i].Session.Init(impl, pVer);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        return MFX_ERR_NONE;
    }

    mfxStatus InitTaskPools(CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize, CSmplBitstreamWriter *pOtherWriter = NULL)
    {
        for (int i = 0; i < size; i++)
        {
            mfxStatus sts = m_resources[i].TaskPool.Init(&m_resources[i].Session,pWriter,nPoolSize,nBufferSize,pOtherWriter);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        return MFX_ERR_NONE;
    }

    mfxStatus CreateEncoders()
    {
        for (int i = 0; i < size; i++)
        {
            MFXVideoENCODE* pEnc = new MFXVideoENCODE(m_resources[i].Session);
            MSDK_CHECK_POINTER(pEnc, MFX_ERR_MEMORY_ALLOC);
            m_resources[i].pEncoder=pEnc;
        }
        return MFX_ERR_NONE;
    }

    mfxStatus CreatePlugins(mfxPluginUID pluginGUID, mfxChar* pluginPath)
    {
        for (int i = 0; i < size; i++)
        {
            MFXPlugin* pPlugin = LoadPlugin(MFX_PLUGINTYPE_VIDEO_ENCODE, m_resources[i].Session, pluginGUID, 1, pluginPath, pluginPath? (mfxU32)strlen(pluginPath) : 0);
            if (pPlugin == NULL)
            {
                return MFX_ERR_UNSUPPORTED;
            }
            m_resources[i].pPlugin=pPlugin;
        }
        return MFX_ERR_NONE;
    }

    void CloseAndDeleteEverything()
    {
        for(int i = 0; i < size; i++)
        {
            m_resources[i].TaskPool.Close();
            MSDK_SAFE_DELETE(m_resources[i].pEncoder);
            MSDK_SAFE_DELETE(m_resources[i].pPlugin);
            m_resources[i].Session.Close();
        }
    }


protected:
    CMSDKResource* m_resources;
    int size;
};



/* This class implements a pipeline with 2 mfx components: vpp (video preprocessing) and encode */
class CRegionEncodingPipeline : public CEncodingPipeline
{
public:
    CRegionEncodingPipeline();
    virtual ~CRegionEncodingPipeline();

    virtual mfxStatus Init(sInputParams *pParams);
    virtual mfxStatus Run();
    virtual void Close();
    virtual mfxStatus ResetMFXComponents(sInputParams* pParams);

    void SetMultiView();
    void SetNumView(mfxU32 numViews) { m_nNumView = numViews; }

protected:
    mfxI64 m_timeAll;
    CResourcesPool m_resources;

    mfxExtHEVCRegion m_HEVCRegion;

    virtual mfxStatus InitMfxEncParams(sInputParams *pParams);

    virtual mfxStatus CreateAllocator();

    virtual mfxStatus GetFreeTask(sTask **ppTask, int index);

    virtual MFXVideoSession& GetFirstSession(){return m_resources[0].Session;}
    virtual MFXVideoENCODE* GetFirstEncoder(){return m_resources[0].pEncoder;}
};

#endif // __PIPELINE_REGION_ENCODE_H__
