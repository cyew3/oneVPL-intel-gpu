/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

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
    virtual void SetMap(std::map<CmSurface2D*,mfxFrameSurface1*>* _CmToMfxSurfmap);
    virtual void SetFrmBuffer(Frame *_frmBuffer[LASTFRAME]);
    virtual void SetFrmType(const mfxU16& _in, const mfxU16& _out);
    virtual CmSurface2D* GetWorkSurfaceCM();
    virtual mfxFrameSurface1* GetWorkSurfaceMfx();
    virtual mfxStatus AddCPUPtirOutSurf(mfxU8* buffer, mfxFrameSurface1* surface);
    virtual mfxStatus AddOutputSurf(mfxFrameSurface1* outSurf, mfxFrameSurface1* exp_surf = 0);
    virtual mfxStatus FreeFrames();
    virtual mfxStatus FreeSurface(CmSurface2D*& cmSurf);
    virtual mfxStatus CMCopyGPUToGpu(CmSurface2D* cmSurfOut, CmSurface2D* cmSurfIn);
    virtual mfxStatus CMCopySysToGpu(mfxFrameSurface1*& mfxSurf, CmSurface2D*& cmSurfOut);
    virtual mfxStatus CMCopyGpuToSys(CmSurface2D*& cmSurfOut, mfxFrameSurface1*& mfxSurf);
    virtual size_t countFreeWorkSurfs();
    virtual mfxStatus CMCreateSurfaceIn(mfxFrameSurface1*& mfxSurf, CmSurface2D*& cmSurfOut, bool bCopy);
    virtual mfxStatus CMCreateSurfaceOut(mfxFrameSurface1*& mfxSurf, CmSurface2D*& cmSurfOut, bool bCopy);

protected:
    CmDeviceEx* pCMdevice;
    CmQueue *m_pCmQueue;
    mfxCoreInterface* mfxCoreIfce;
    std::vector<mfxFrameSurface1*> *inSurfs;
    std::vector<mfxFrameSurface1*> *workSurfs;
    std::vector<mfxFrameSurface1*> *outSurfs;
    std::map<CmSurface2D*,mfxFrameSurface1*> *CmToMfxSurfmap;
    Frame   **frmBuffer;
    mfxU16  IOPattern;
    mfxU16  opqFrTypeIn;
    mfxU16  opqFrTypeOut;
    bool    isD3D11;
    UMC::Mutex& m_guard;

private:
    //prohobit copy constructor
    frameSupplier(const frameSupplier& that);
    //prohibit assignment operator
    frameSupplier& operator=(const frameSupplier&);
};

class PTIR_Processor
{
public:
    //PTIR_Processor();
    virtual ~PTIR_Processor() {}

    PTIRSystemBuffer           Env;

    unsigned int               i,
                               _uiInWidth,
                               _uiInHeight,
                               uiTimer,
                               uiOpMode,
                               uiProgress,
                               uiStart,
                               uiCount,
                               uiLastFrameNumber,
                               _uiInterlaceParity;
    unsigned char*              pucIO;

    double                       dOutBaseTime,
                               _dFrameRate,
                               dTime,
                               dTimeTotal;
    double                     pts;
    double                     frame_duration;//1000 / _dFrameRate;

    bool bFullFrameRate;
    bool b_firstFrameProceed;
    bool bInited;
    bool isHW;

    virtual mfxStatus Init(mfxVideoParam *, mfxExtVPPDeinterlacing*)
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
    virtual mfxStatus Process(mfxFrameSurface1 *surf_in, mfxFrameSurface1 **surf_work, mfxCoreInterface *core, mfxFrameSurface1 **surf_out = 0, bool beof = false, mfxFrameSurface1 *exp_surf = 0)
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

    virtual mfxStatus Init(mfxVideoParam *par, mfxExtVPPDeinterlacing* pDeinter);
    virtual mfxStatus Close();
    virtual mfxStatus PTIR_ProcessFrame(mfxFrameSurface1 *surf_in, mfxFrameSurface1 *surf_out, bool beof = 0, mfxFrameSurface1 *exp_surf = 0);
    virtual mfxStatus Process(mfxFrameSurface1 *surf_in, mfxFrameSurface1 **surf_work, mfxCoreInterface *core, mfxFrameSurface1 **surf_out = 0, bool beof = false, mfxFrameSurface1 *exp_surf = 0);

protected:
    mfxStatus MFX_PTIR_PutFrame(mfxFrameSurface1 *surf_in, PTIRSystemBuffer *SysBuffer, double dTimestamp);
    mfxStatus OutputFrameToMfx(Frame* frmIn, Frame* frmOut, mfxFrameSurface1 *surf_out, unsigned int * uiLastFrameNumber);

private:
    //prohobit copy constructor
    PTIR_ProcessorCPU(const PTIR_ProcessorCPU& that);
    //prohibit assignment operator
    PTIR_ProcessorCPU& operator=(const PTIR_ProcessorCPU&);
};

class PTIR_ProcessorCM : public PTIR_Processor
{
public:
    PTIR_ProcessorCM(mfxCoreInterface* mfxCore, frameSupplier* _frmSupply, eMFXHWType HWType);
    virtual ~PTIR_ProcessorCM();

    virtual mfxStatus Init(mfxVideoParam *par, mfxExtVPPDeinterlacing* pDeinter);
    virtual mfxStatus Close();
    virtual mfxStatus PTIR_ProcessFrame(mfxFrameSurface1 *surf_in, mfxFrameSurface1 *surf_out, mfxFrameSurface1 **surf_outt = 0, bool beof = 0, mfxFrameSurface1 *exp_surf = 0);
    virtual mfxStatus Process(mfxFrameSurface1 *surf_in, mfxFrameSurface1 **surf_work, mfxCoreInterface *core, mfxFrameSurface1 **surf_out = 0, bool beof = false, mfxFrameSurface1 *exp_surf = 0);

protected:
    mfxStatus MFX_PTIRCM_PutFrame(mfxFrameSurface1 *surf_in, mfxFrameSurface1 *surf_out, Frame* frame, PTIRSystemBuffer *SysBuffer, double dTimestamp);
    mfxStatus OutputFrameToMfx(Frame* frmOut, mfxFrameSurface1* surf_out, mfxFrameSurface1*& exp_surf, unsigned int * uiLastFrameNumber);

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

private:
    //prohobit copy constructor
    PTIR_ProcessorCM(const PTIR_ProcessorCM& that);
    //prohibit assignment operator
    PTIR_ProcessorCM& operator=(const PTIR_ProcessorCM&);
};

#endif  // __MFX_PTIR_WRAP_INCLUDED__