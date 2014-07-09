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
#include <umc_mutex.h>
extern "C" {
#include "../pa/api.h"
#if defined(LINUX32) || defined (LINUX64)

//Undefine since cm linux include files are defining it itself which cause compilation errors
//  (for reference, CPU version is in C-code and do not use CM headers)
    #undef BOOL
    #undef TRUE
    #undef FALSE
    #undef BYTE
#endif
}
#include "../pacm/pacm.h"
//#include "cmrt_cross_platform.h"

typedef void * AbstractSurface;

class frameSupplier
{
public:
    frameSupplier(std::vector<mfxFrameSurface1*>* _inSurfs, std::vector<mfxFrameSurface1*>* _workSurfs, 
                  std::vector<mfxFrameSurface1*>* _outSurfs, std::map<CmSurface2D*,mfxFrameSurface1*>* _CmToMfxSurfmap,
                  CmDeviceEx* _pCMdevice, mfxCoreInterface* _mfxCore, mfxU16 _IOPattern, bool _isD3D11, UMC::Mutex& _guard);
    virtual ~frameSupplier()
    {
        FreeFrames();
    }
    virtual mfxStatus SetDevice(CmDeviceEx* _pCMdevice);
    virtual void* SetMap(std::map<CmSurface2D*,mfxFrameSurface1*>* _CmToMfxSurfmap);
    virtual CmSurface2D* GetWorkSurfaceCM();
    virtual mfxFrameSurface1* GetWorkSurfaceMfx();
    virtual mfxStatus AddCPUPtirOutSurf(mfxU8* buffer, mfxFrameSurface1* surface);
    virtual mfxStatus AddOutputSurf(mfxFrameSurface1* outSurf);
    virtual mfxStatus FreeFrames();
    virtual mfxStatus FreeSurface(CmSurface2D*& cmSurf);
    virtual mfxStatus CMCopyGPUToGpu(CmSurface2D* cmSurfOut, CmSurface2D* cmSurfIn);
    virtual mfxStatus CMCopySysToGpu(mfxFrameSurface1*& mfxSurf, CmSurface2D*& cmSurfOut);
    virtual mfxStatus CMCopyGpuToSys(CmSurface2D*& cmSurfOut, mfxFrameSurface1*& mfxSurf);
    virtual mfxU32 countFreeWorkSurfs();
    virtual mfxStatus CMCreateSurface2D(mfxFrameSurface1*& mfxSurf, CmSurface2D*& cmSurfOut, bool bCopy);

protected:
    CmDeviceEx* pCMdevice;
    CmQueue *m_pCmQueue;
    mfxCoreInterface* mfxCoreIfce;
    std::vector<mfxFrameSurface1*> *inSurfs;
    std::vector<mfxFrameSurface1*> *workSurfs;
    std::vector<mfxFrameSurface1*> *outSurfs;
    std::map<CmSurface2D*,mfxFrameSurface1*> *CmToMfxSurfmap;
    mfxU16  IOPattern;
    bool    isD3D11;
    UMC::Mutex& m_guard;
};

class PTIR_Processor
{
public:
    //PTIR_Processor();
    virtual ~PTIR_Processor() {}

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
        //uiCount,
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
//    CONSOLE_SCREEN_BUFFER_INFO
//        sbInfo;
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
//    errno_t
//        errorT;
    bool b_firstFrameProceed;
    bool bInited;
    bool isHW;

    virtual mfxStatus Init(mfxVideoParam *)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus Close()
    {
        return MFX_ERR_UNSUPPORTED;
    }
    //virtual mfxStatus PTIR_ProcessFrame(mfxFrameSurface1 *, mfxFrameSurface1 *) = 0
    //{
    //    return MFX_ERR_UNSUPPORTED;
    //}
    virtual mfxStatus Process(mfxFrameSurface1 *surf_in, mfxFrameSurface1 **surf_work, mfxCoreInterface *core, mfxFrameSurface1 **surf_out = 0, bool beof = false)
    {
        return MFX_ERR_UNSUPPORTED;
    }

protected:
    mfxVideoParam       work_par;

    frameSupplier*      frmSupply;
    mfxCoreInterface*   m_pmfxCore;
};

class PTIR_ProcessorCPU : public PTIR_Processor
{
public:
    PTIR_ProcessorCPU(mfxCoreInterface* mfxCore, frameSupplier* _frmSupply);
    virtual ~PTIR_ProcessorCPU();

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Close();
    virtual mfxStatus PTIR_ProcessFrame(mfxFrameSurface1 *surf_in, mfxFrameSurface1 *surf_out, bool beof = 0);
    virtual mfxStatus Process(mfxFrameSurface1 *surf_in, mfxFrameSurface1 **surf_work, mfxCoreInterface *core, mfxFrameSurface1 **surf_out = 0, bool beof = false);

protected:

};

class PTIR_ProcessorCM : public PTIR_Processor
{
public:
    PTIR_ProcessorCM(mfxCoreInterface* mfxCore, frameSupplier* _frmSupply, eMFXHWType HWType);
    virtual ~PTIR_ProcessorCM();

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Close();
    virtual mfxStatus PTIR_ProcessFrame(CmSurface2D *surf_in, CmSurface2D *surf_out, mfxFrameSurface1 **surf_outt = 0, bool beof = 0);
    virtual mfxStatus Process(mfxFrameSurface1 *surf_in, mfxFrameSurface1 **surf_work, mfxCoreInterface *core, mfxFrameSurface1 **surf_out = 0, bool beof = false);

protected:
    template <typename D3DAbstract>
    CmDevice* GetCmDevice(D3DAbstract *pD3D)
    {
        int cmSts = CM_SUCCESS;
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
    CmDevice          *m_pCmDevice;
    CmProgram         *m_pCmProgram;
    CmKernel          *m_pCmKernel1;
    CmKernel          *m_pCmKernel2;
    bool              mb_UsePtirSurfs;
    std::map<CmSurface2D*,mfxFrameSurface1*> CmToMfxSurfmap;
    eMFXHWType HWType;
};

#endif  // __MFX_PTIR_WRAP_INCLUDED__