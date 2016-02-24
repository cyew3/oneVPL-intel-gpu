/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2008 - 2016 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __TEST_VPP_UTILS_H
#define __TEST_VPP_UTILS_H

/* ************************************************************************* */

#if defined(_WIN32) || defined(_WIN64)
#define D3D_SURFACES_SUPPORT
#endif // #if defined(_WIN32) || defined(_WIN64)

#ifdef D3D_SURFACES_SUPPORT
#pragma warning(disable : 4201)
#include <d3d9.h>
#include <dxva2api.h>
#include <windows.h>
#include <math.h>
#include <psapi.h>
#endif

#ifdef MFX_D3D11_SUPPORT
#include <d3d11.h>
#endif

#ifdef LIBVA_SUPPORT
#include "vaapi_utils.h"
#include "vaapi_allocator.h"
#endif

#include <map>
#include <stdio.h>
#include <memory>

#include "vm_strings.h"
#include "vm_file.h"

#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "mfxmvc.h"
#include "mfxsvc.h"
#include "mfxplugin.h"

#include "app_defs.h"
#include "base_allocator.h"
#include "test_vpp_config.h"
#include "test_vpp_roi.h"
#include "vm_interlocked.h"

// we introduce new macros without error message (returned status only)
// it allows to remove final error message due to EOF
#define IOSTREAM_CHECK_NOT_EQUAL(P, X, ERR)          {if ((X) != (P)) {return ERR;}}

/* ************************************************************************* */

enum
{
    VPP_IN   = 0,
    VPP_OUT  = 1,
    VPP_WORK = 2
};

enum
{
    ALLOC_IMPL_VIA_SYS      = 0,
    ALLOC_IMPL_VIA_D3D9     = 1,
    ALLOC_IMPL_VIA_D3D11    = 2,
    ALLOC_IMPL_VIA_VAAPI    = 4
};

//#define BACKWARD_COMPATIBILITY

enum
{
    NO_PTR      = 0,
    INPUT_PTR   = 1,
    OUTPUT_PTR  = 2,
    ALL_PTR     = 3
};

typedef struct _ownFrameInfo
{
    mfxU16  Shift;
    mfxU16  nWidth;
    mfxU16  nHeight;
    // ROI
    mfxU16  CropX;
    mfxU16  CropY;
    mfxU16  CropW;
    mfxU16  CropH;
    mfxU32 FourCC;
    mfxI8  PicStruct;
    mfxF64 dFrameRate;

} sOwnFrameInfo;

struct sInputParams
{
    /* smart filters defined by mismatch btw src & dst */
    sOwnFrameInfo frameInfo[2];// [0] - in, [1] - out

    /* Video Enhancement Algorithms */
    sDIParam      deinterlaceParam;
    sDenoiseParam denoiseParam;
    sDetailParam  detailParam;
    sProcAmpParam procampParam;
    sVideoAnalysisParam vaParam;
    sFrameRateConversionParam frcParam;
    sVarianceReportParam varianceParam;
    sIDetectParam idetectParam;

    // MSDK 3.0
    sGamutMappingParam    gamutParam;
    sMultiViewParam       multiViewParam;
    sSVCParam             svcParam;
    // MSDK API 1.5
    sTccParam     tccParam;
    sAceParam     aceParam;
    sSteParam     steParam;
    sIStabParam   istabParam;

    // flag describes type of memory
    // true  - frames in video memory (d3d surfaces),  
    // false - in system memory
    //bool   bd3dAlloc;
    mfxU16   IOPattern;
    mfxIMPL  ImpLib;
    mfxU32   sptr;
    bool     bDefAlloc;
    mfxU16   asyncNum;
    mfxU32   vaType;

    bool     bInitEx;
    mfxU16   GPUCopyValue;

    mfxU16   rotate;

    bool     bPartialAccel;

    bool     bPerf;
    mfxU32   numFrames;
    mfxU16   numRepeat;
    bool     isOutput;
    bool     ptsCheck;
    bool     ptsJump;
    bool     ptsAdvanced;
    mfxF64   ptsFR;

    /* roi checking parameters */
    sROICheckParam roiCheckParam;

    /*  plug-in GUID */
    vm_char strPlgGuid[MAX_FILELEN];
    bool    need_plugin;

    /* ********************** */
    /* input\output streams   */
    /* ********************** */
    vm_char  strSrcFile[MAX_FILELEN];
    vm_char  strDstFile[MAX_FILELEN];

    vm_char  strPerfFile[MAX_FILELEN];
    bool  isOutYV12;

    vm_char  strCRCFile[MAX_FILELEN];
    bool  need_crc;

    /* Use extended API (RunFrameVPPAsyncEx) */
    bool  use_extapi;
};

struct sFrameProcessor
{
public:
    MFXVideoSession     mfxSession;
    MFXVideoVPP*        pmfxVPP;
    mfxPluginUID        mfxGuid;
    bool                plugin;
    bool                inited;
    sFrameProcessor( void )
    { 
        pmfxVPP = NULL;
        plugin = false;
        inited = false;
    };
    sFrameProcessor(const sFrameProcessor &)
    {
        return;
    }
};

struct sMemoryAllocator
{
    MFXFrameAllocator*  pMfxAllocator;
    mfxAllocatorParams* pAllocatorParams;
    bool                bUsedAsExternalAllocator;

    std::vector<mfxFrameSurface1 *>    pSurfaces; // IN/OUT
    std::vector<mfxFrameAllocResponse> response;  // IN/OUT

    mfxFrameSurface1*     pSvcSurfaces[8]; //output surfaces per layer
    mfxFrameAllocResponse svcResponse[8];     //per layer

#ifdef D3D_SURFACES_SUPPORT
    IDirect3DDeviceManager9* pd3dDeviceManager;
#endif

#ifdef MFX_D3D11_SUPPORT
    ID3D11Device *pD3D11Device;
    ID3D11DeviceContext *pD3D11DeviceContext;
#endif

#ifdef LIBVA_SUPPORT
    std::auto_ptr<CLibVA> libvaKeeper;
#endif
};

class PTSMaker;

class CRawVideoReader
{
public :

    CRawVideoReader();
    ~CRawVideoReader();

    void       Close();

    mfxStatus  Init(
        const vm_char *strFileName, 
        PTSMaker *pPTSMaker);

    mfxStatus  PreAllocateFrameChunk(
        mfxVideoParam* pVideoParam, 
        sInputParams* pParams, 
        MFXFrameAllocator* pAllocator);

    mfxStatus  GetNextInputFrame(
        sMemoryAllocator* pAllocator, 
        mfxFrameInfo* pInfo, 
        mfxFrameSurface1** pSurface,
        mfxU32 index);

private:
    mfxStatus  LoadNextFrame(
        mfxFrameData* pData, 
        mfxFrameInfo* pInfo);
    mfxStatus  GetPreAllocFrame(mfxFrameSurface1 **pSurface);

    vm_file*       m_fSrc;
    std::list<mfxFrameSurface1>::iterator m_it;
    std::list<mfxFrameSurface1>           m_SurfacesList;
    bool                                  m_isPerfMode;
    mfxU16                                m_Repeat;

    PTSMaker                             *m_pPTSMaker;

};

class CRawVideoWriter
{
public :

    CRawVideoWriter();
    ~CRawVideoWriter();

    void       Close();

    mfxStatus  Init(
        const vm_char *strFileName, 
        PTSMaker *pPTSMaker, 
        bool outYV12  = false,
        bool need_crc = false);

    mfxStatus  PutNextFrame(
        sMemoryAllocator* pAllocator, 
        mfxFrameInfo* pInfo, 
        mfxFrameSurface1* pSurface);

    mfxU32     GetCRC();

private:
    mfxStatus  WriteFrame(
        mfxFrameData* pData, 
        mfxFrameInfo* pInfo);
    
    mfxStatus  CRC32(
        mfxU8 *data, 
        mfxU32 length);

    vm_file*      m_fDst;
    PTSMaker                              *m_pPTSMaker;
    bool                                   m_outYV12;
    std::auto_ptr<mfxU8>                   m_outSurfYV12;
    Ipp32u                                 m_crc32c;
    bool                                   m_need_crc;

};


class GeneralWriter // : public CRawVideoWriter
{
public :

    GeneralWriter();
    ~GeneralWriter();

    void       Close();

    mfxStatus  Init(
        const vm_char *strFileName, 
        PTSMaker *pPTSMaker,
        sSVCLayerDescr*  pDesc = NULL,
        bool outYV12 = false,
        bool need_crc = false);

    mfxU32     GetCRC(mfxFrameSurface1* pSurface);

    mfxStatus  PutNextFrame(
        sMemoryAllocator* pAllocator, 
        mfxFrameInfo* pInfo, 
        mfxFrameSurface1* pSurface);

private:
    std::auto_ptr<CRawVideoWriter> m_ofile[8];

    bool m_svcMode;
};

// need for support async
class SurfaceVPPStore
{
public:
    struct SurfVPPExt
    {
        SurfVPPExt(mfxFrameSurface1* pSurf = 0, mfxExtVppAuxData* pExt = 0):pSurface(pSurf),pExtVpp(pExt)
        {
        };
        mfxFrameSurface1* pSurface;
        mfxExtVppAuxData* pExtVpp;

    };
    SurfaceVPPStore(){};

    typedef std::pair <mfxSyncPoint, SurfVPPExt> SyncPair;
    std::list<SyncPair> m_SyncPoints;
};

struct sAppResources
{
    sAppResources():
      m_pSrcFileReader(0),
      m_pDstFileWriter(0),
      nFrames(0),
      m_pParams(0),
      work_surf(VPP_OUT),
      next(0)
    {
        bROITest[0] = false;
        bROITest[1] = false;
        ZERO_MEMORY(svcConfig);
        ZERO_MEMORY(m_mfxParamsVideo);
    };

    int ProcessInChain(mfxFrameSurface1 *inputSurface);
    int Process();
    mfxStatus OutputProcessFrame(sAppResources Resources, mfxFrameInfo* frameInfo, mfxU32 &nFrames);
    mfxU32 GetFramesProcessed() { return nFrames; }

    // to prevent incorrect read/write of image in case of CropW/H != width/height 
    mfxFrameInfo        realFrameInfo[2];

    CRawVideoReader*    m_pSrcFileReader;
    GeneralWriter*      m_pDstFileWriter;
    mfxU32              nFrames;
    sFrameProcessor     frameProcessor;
    sFrameProcessor*    pProcessor;
    mfxVideoParam*      pVppParams;
    sMemoryAllocator*   pAllocator;
    sInputParams*       m_pParams;
    SurfaceVPPStore*    pSurfStore;
    unsigned int        work_surf;
    mfxVideoParam       m_mfxParamsVideo;

    /* VPP extension */
    mfxExtVppAuxData*   pExtVPPAuxData;
    mfxExtVPPDoUse      extDoUse;
    mfxU32              tabDoUseAlg[ENH_FILTERS_COUNT];    
    mfxExtBuffer*       pExtBuf[1+ENH_FILTERS_COUNT];
    SurfaceVPPStore     surfStore;
    /* config video enhancement algorithms */
    mfxExtVPPProcAmp    procampConfig;
    mfxExtVPPDetail     detailConfig;
    mfxExtVPPDenoise    denoiseConfig;
    mfxExtVPPRotation   rotationConfig;
    mfxExtVPPFrameRateConversion    frcConfig;
    mfxExtVPPDeinterlacing deinterlaceConfig;

    /* generators for ROI testing */
    ROIGenerator       inROIGenerator;     
    ROIGenerator       outROIGenerator;
    bool               bROITest[2];

    // MSDK 3.0
    //  mfxExtVPPGamutMapping gamutConfig;
    mfxExtMVCSeqDesc      multiViewConfig;

    // SVC mode
    mfxExtSVCSeqDesc      svcConfig;
    mfxFrameInfo          realSvcOutFrameInfo[8];

    ////MSDK API 1.5
    //mfxExtVPPSkinTone              steConfig;
    //mfxExtVPPColorSaturationLevel  tccConfig;
    mfxExtVPPImageStab             istabConfig;


    sAppResources           *next;
    size_t                  indexInPipe;
};

class D3DFrameAllocator;

#ifdef MFX_D3D11_SUPPORT
class D3D11FrameAllocator;
#endif

class SysMemFrameAllocator;

class GeneralAllocator : public BaseFrameAllocator
{
public:
    GeneralAllocator();
    virtual ~GeneralAllocator();    

    virtual mfxStatus Init(mfxAllocatorParams *pParams);
    virtual mfxStatus Close();

    void SetDX11(void);

protected:
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);       

    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response);
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);

    void    StoreFrameMids(bool isD3DFrames, mfxFrameAllocResponse *response);
    bool    isD3DMid(mfxHDL mid);

    std::map<mfxHDL, bool>              m_Mids;
    bool m_isDx11;

#ifdef MFX_D3D11_SUPPORT
    std::auto_ptr<D3D11FrameAllocator>    m_D3D11Allocator;
#endif
#ifdef D3D_SURFACES_SUPPORT
    std::auto_ptr<D3DFrameAllocator>    m_D3DAllocator;
#endif
#ifdef LIBVA_SUPPORT
    std::auto_ptr<vaapiFrameAllocator>    m_vaapiAllocator;
#endif
    std::auto_ptr<SysMemFrameAllocator> m_SYSAllocator;

};

/* ******************************************************************* */
/*                        service functions                            */
/* ******************************************************************* */

void PrintInfo(
    sInputParams* pParams, 
    mfxVideoParam* pMfxParams, 
    MFXVideoSession *pMfxSession);

void PrintDllInfo();

mfxStatus InitParamsVPP(
    mfxVideoParam* pMFXParams, 
    sInputParams* pInParams);

mfxStatus InitResources(
    sAppResources* pResources, 
    mfxVideoParam* pParams, 
    sInputParams* pInParams);

mfxStatus InitAllocator(sAppResources* pResources, mfxVideoParam* pParams, sInputParams* pInParams, bool create, int poolIndex);
mfxStatus AllocateSurfaces(
    sFrameProcessor* pProcessor, 
    sMemoryAllocator* pAllocator, 
    mfxVideoParam* pParams, 
    sInputParams* pInParams,
    mfxU16 &additionalInputSurfs,
    mfxU16 &additionalOutputSurfs,
    int poolIndex);

mfxStatus QuerySurfaces(
    sFrameProcessor* pProcessor, 
    sMemoryAllocator* pAllocator, 
    mfxVideoParam* pParams, 
    sInputParams* pInParams,
    mfxU16 &additionalInputSurfs,
    mfxU16 &additionalOutputSurfs,
    int poolIndex);

mfxStatus InitVPP(sAppResources* pResources, mfxVideoParam* pParams, sInputParams* pInParams);

void WipeResources(sAppResources* pResources);

mfxStatus GetFreeSurface(
    mfxFrameSurface1* pSurfacesPool, 
    mfxU16 nPoolSize, 
    mfxFrameSurface1** ppSurface);

const vm_char* IOpattern2Str(
    mfxU32 IOpattern);

mfxStatus vppParseInputString(
    vm_char* strInput[], 
    mfxU8 nArgNum, 
    std::vector<sInputParams*> &pParams);

bool CheckInputParams(
    vm_char* strInput[], 
    sInputParams* pParams );

void vppPrintHelp(
    const vm_char *strAppName,
    const vm_char *strErrorMessage);

mfxStatus ConfigVideoEnhancementFilters( 
    sInputParams* pParams, 
    sAppResources* pResources );

const vm_char* PicStruct2Str( mfxU16  PicStruct );

const sOwnFrameInfo  defaultOwnFrameInfo = {0, 352, 288, 0, 0, 352, 288, MFX_FOURCC_NV12, MFX_PICSTRUCT_PROGRESSIVE, 30.0};
const sProcAmpParam  defaultProcAmpParam = { 0.0, 1.0, 1.0, 0.0, VPP_FILTER_DISABLED};
const sDetailParam   defaultDetailParam  = { 1,  VPP_FILTER_DISABLED};
const sDenoiseParam  defaultDenoiseParam = { 1,  VPP_FILTER_DISABLED};
const sVideoAnalysisParam defaultVAParam = { VPP_FILTER_DISABLED};
const sVarianceReportParam defaultVarianceParam = { VPP_FILTER_DISABLED};
const sIDetectParam defaultIDetectParam = { VPP_FILTER_DISABLED};
const sFrameRateConversionParam defaultFRCParam = { MFX_FRCALGM_PRESERVE_TIMESTAMP, VPP_FILTER_DISABLED};
//MSDK 3.0
const sMultiViewParam       defaultMultiViewParam     = {1, VPP_FILTER_DISABLED}; 
//MSDK API 1.5
const sGamutMappingParam    defaultGamutParam         = { false,  VPP_FILTER_DISABLED};
const sTccParam defaultClrSaturationParam = { 160, 160, 160, 160, VPP_FILTER_DISABLED };
const sAceParam     defaultContrastParam      = { VPP_FILTER_DISABLED };
const sSteParam         defaultSkinParam          = { 4, VPP_FILTER_DISABLED };
const sIStabParam  defaultImgStabParam   = { MFX_IMAGESTAB_MODE_BOXING, VPP_FILTER_DISABLED };


#endif /* __TEST_VPP_UTILS_H */
/* EOF */
