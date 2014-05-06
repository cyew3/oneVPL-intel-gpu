/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: ptir_vpp_plugin.h

\* ****************************************************************************** */

#if !defined(__MFX_PTIR_WRAP_INCLUDED__)
#define __MFX_PTIR_WRAP_INCLUDED__

#include "mfxvideo.h"
#include "mfxplugin.h"
#include "ptir_vpp_utils.h"
extern "C" {
#include "..\ptir\pa\api.h"
}
#include "..\pacm\pacm.h"
//#include "cmrt_cross_platform.h"

typedef void * AbstractSurface;

class frameSupplier
{
public:
    frameSupplier(std::vector<mfxFrameSurface1*>* _inSurfs, std::vector<mfxFrameSurface1*>* _workSurfs, 
                  std::vector<mfxFrameSurface1*>* _outSurfs, std::map<CmSurface2D*,mfxFrameSurface1*>* _CmToMfxSurfmap,
                  CmDeviceEx* _pCMdevice, mfxCoreInterface* _mfxCore)
    {
        inSurfs        = _inSurfs;
        workSurfs      = _workSurfs;
        outSurfs       = _outSurfs;
        CmToMfxSurfmap = _CmToMfxSurfmap;
        pCMdevice      = _pCMdevice;
        mfxCoreIfce    = _mfxCore;
    }
    virtual ~frameSupplier()
    {
    }
    virtual mfxStatus SetDevice(CmDeviceEx* _pCMdevice)
    {
        mfxI32 result = -1;
        pCMdevice      = _pCMdevice;
        result = (*pCMdevice)->CreateQueue(m_pCmQueue);

        if(!result)
            return MFX_ERR_NONE;
        else
            return MFX_ERR_DEVICE_FAILED;
    }
    virtual void* SetMap(std::map<CmSurface2D*,mfxFrameSurface1*>* _CmToMfxSurfmap)
    {
        CmToMfxSurfmap = _CmToMfxSurfmap;
        return 0;
    }

    virtual CmSurface2D* GetWorkSurfaceCM()
    {
        mfxFrameSurface1* s_out = 0;
        CmSurface2D* cm_out = 0;
        if(0 == workSurfs->size())
            return 0;
        else
        {
            for(std::vector<mfxFrameSurface1*>::iterator it = workSurfs->begin(); it != workSurfs->end(); ++it) {
                if((*it)->Data.Locked < 4)
                {
                    s_out = *it;
                    workSurfs->erase(it);
                    break;
                }
            }
            mfxI32 result = -1;
            result = (*pCMdevice)->CreateSurface2D((IDirect3DSurface9 *) s_out->Data.MemId, cm_out);
            if(result)
                std::cout << "CM ERROR while creating CM surface\n";
            mfxCoreIfce->IncreaseReference(mfxCoreIfce->pthis, &s_out->Data);
            (*CmToMfxSurfmap)[cm_out] = s_out;
            return cm_out;
        }
        return 0;
    }
    virtual mfxFrameSurface1* GetWorkSurfaceMfx()
    {
        mfxFrameSurface1* s_out = 0;
        if(0 == workSurfs->size())
            return 0;
        else
        {
            for(std::vector<mfxFrameSurface1*>::iterator it = workSurfs->begin(); it != workSurfs->end(); ++it) {
                if((*it)->Data.Locked < 4)
                {
                    s_out = *it;
                    workSurfs->erase(it);
                    break;
                }
            }
            return s_out;
        }
        return 0;
    }
    virtual mfxStatus AddCPUPtirOutSurf(mfxU8* buffer, mfxFrameSurface1* surface)
    {
        bool unlockreq = false;
        mfxStatus mfxSts = MFX_ERR_NONE;
        if(surface->Data.MemId)
        {
            unlockreq = true;
            mfxSts = mfxCoreIfce->FrameAllocator.Lock(mfxCoreIfce->FrameAllocator.pthis, surface->Data.MemId, &(surface->Data));
            if(mfxSts)
                return mfxSts;
        }
        mfxSts = Ptir420toMfxNv12(buffer, surface);
        if(mfxSts)
            return mfxSts;
        if(unlockreq)
        {
            mfxSts = mfxCoreIfce->FrameAllocator.Unlock(mfxCoreIfce->FrameAllocator.pthis, surface->Data.MemId, &(surface->Data));
            if(mfxSts)
                return mfxSts;
        }
        mfxSts = AddOutputSurf(surface);
        return mfxSts;
    }
    virtual mfxStatus AddOutputSurf(mfxFrameSurface1* outSurf)
    {
        outSurfs->push_back(outSurf);
        return MFX_ERR_NONE;
    }
    virtual mfxStatus FreeSurface(CmSurface2D* cmSurf)
    {
        mfxFrameSurface1* mfxSurf;
        mfxSurf = (*CmToMfxSurfmap)[cmSurf];
        while(mfxSurf->Data.Locked){
            mfxCoreIfce->DecreaseReference(mfxCoreIfce->pthis, &mfxSurf->Data);
        }
        return MFX_ERR_NONE;
    }

    virtual mfxStatus CMCopyGPUToGpu(CmSurface2D* cmSurfOut, CmSurface2D* cmSurfIn)
    {
        mfxI32 cmSts = 0;
        CM_STATUS sts;
        mfxStatus mfxSts = MFX_ERR_NONE;
        CmEvent* e = NULL;
        cmSts = m_pCmQueue->EnqueueCopyGPUToGPU(cmSurfOut, cmSurfIn, e);

        if (CM_SUCCESS == cmSts && e)
        {
            e->GetStatus(sts);
        
            while (sts != CM_STATUS_FINISHED) 
            {
                e->GetStatus(sts);
            }
        }else{
            mfxSts = MFX_ERR_DEVICE_FAILED;
        }
        if(e) m_pCmQueue->DestroyEvent(e);

        return mfxSts;
    }
    virtual mfxU32 countFreeWorkSurfs()
    {
        return (*workSurfs).size();
    }

protected:
    CmDeviceEx* pCMdevice;
    CmQueue *m_pCmQueue;
    mfxCoreInterface* mfxCoreIfce;
    std::vector<mfxFrameSurface1*> *inSurfs;
    std::vector<mfxFrameSurface1*> *workSurfs;
    std::vector<mfxFrameSurface1*> *outSurfs;
    std::map<CmSurface2D*,mfxFrameSurface1*> *CmToMfxSurfmap;
};

class PTIR_Processor
{
public:
    //PTIR_Processor();
    //virtual ~PTIR_Processor();

    unsigned int
        i,
        uiDeinterlacingMode,
        uiDeinterlacingMeasure,
        uiSupBuf,
        uiCur,
        uiCurTemp,
        uiNext,
        uiTimer,
        uiFrame,
        uiFrameOut,
        uiProgress,
        uiInWidth,
        uiInHeight,
        uiWidth,
        uiHeight,
        uiStart,
        uiCount,
        uiFrameCount,
        uiBufferCount,
        uiSize,
        uiisInterlaced,
        uiTeleCount,
        uiLastFrameNumber,
        uiDispatch;
    unsigned char
        *pucIO;
    Frame
        frmIO[LASTFRAME + 1],
        *frmIn,
        *frmBuffer[LASTFRAME];
    FrameQueue
        fqIn;
    DWORD
        uiBytesRead;
    double
        dTime,
        *dSAD,
        *dRs,
        dPicSize,
        dFrameRate,
        dTimeStamp,
        dBaseTime,
        dOutBaseTime,
        dBaseTimeSw,
        dDeIntTime;
    LARGE_INTEGER
        liTime,
        liFreq,
        liFileSize;
    CONSOLE_SCREEN_BUFFER_INFO
        sbInfo;
    HANDLE
        hIn,
        hOut;
    FILE
        *fTCodeOut;
    BOOL
        patternFound,
        ferror,
        //bisInterlaced,
        bFullFrameRate;
    Pattern
        mainPattern;
    errno_t
        errorT;
    bool b_firstFrameProceed;
    bool bInited;

    virtual mfxStatus Init(mfxVideoParam *)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus Close() = 0
    {
        return MFX_ERR_UNSUPPORTED;
    }
    //virtual mfxStatus PTIR_ProcessFrame(mfxFrameSurface1 *, mfxFrameSurface1 *) = 0
    //{
    //    return MFX_ERR_UNSUPPORTED;
    //}
    virtual mfxStatus Process(mfxFrameSurface1 *surf_in, mfxFrameSurface1 **surf_work, mfxCoreInterface *core, mfxFrameSurface1 **surf_out = 0)
    {
        return MFX_ERR_UNSUPPORTED;
    }

protected:
    mfxVideoParam       work_par;

    frameSupplier* frmSupply;
    mfxCoreInterface* m_pmfxCore;
};

class PTIR_ProcessorCPU : public PTIR_Processor
{
public:
    PTIR_ProcessorCPU(mfxCoreInterface* mfxCore, frameSupplier* _frmSupply);
    virtual ~PTIR_ProcessorCPU();

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Close();
    virtual mfxStatus PTIR_ProcessFrame(mfxFrameSurface1 *surf_in, mfxFrameSurface1 *surf_out);
    virtual mfxStatus Process(mfxFrameSurface1 *surf_in, mfxFrameSurface1 **surf_work, mfxCoreInterface *core, mfxFrameSurface1 **surf_out = 0);

protected:

};

class PTIR_ProcessorCM : public PTIR_Processor
{
public:
    PTIR_ProcessorCM(mfxCoreInterface* mfxCore, frameSupplier* _frmSupply, eMFXHWType HWType);
    virtual ~PTIR_ProcessorCM();

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Close();
    virtual mfxStatus PTIR_ProcessFrame(CmSurface2D *surf_in, CmSurface2D *surf_out, mfxFrameSurface1 **surf_outt = 0);
    virtual mfxStatus Process(mfxFrameSurface1 *surf_in, mfxFrameSurface1 **surf_work, mfxCoreInterface *core, mfxFrameSurface1 **surf_out = 0);

protected:
    template <typename D3DAbstract>
    CmDevice* GetCmDevice(D3DAbstract *pD3D)
    {
        cmStatus cmSts = CM_SUCCESS;
        mfxU32 version;

        if (m_pCmDevice)
            return m_pCmDevice;

        cmSts = ::CreateCmDevice(m_pCmDevice, version, pD3D);
        if (cmSts != CM_SUCCESS)
            return NULL;

        if (CM_1_0 > version)
        {
            return NULL;
        }
        return m_pCmDevice;
    };

    DeinterlaceFilter *deinterlaceFilter;
    CmDevice  *m_pCmDevice;
    CmProgram *m_pCmProgram;
    CmKernel  *m_pCmKernel1;
    CmKernel  *m_pCmKernel2;
    std::map<CmSurface2D*,mfxFrameSurface1*> CmToMfxSurfmap;
    eMFXHWType HWType;
};

#endif  // __MFX_PTIR_WRAP_INCLUDED__