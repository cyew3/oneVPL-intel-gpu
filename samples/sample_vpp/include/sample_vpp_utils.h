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

#ifndef __SAMPLE_VPP_UTILS_H
#define __SAMPLE_VPP_UTILS_H

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

#include "hw_device.h"

#include "sample_defs.h"

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

#include "vm/strings_defs.h"

#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "mfxmvc.h"
#include "mfxplugin.h"

#include "base_allocator.h"
#include "sample_vpp_config.h"
#include "sample_vpp_roi.h"

// we introduce new macros without error message (returned status only)
// it allows to remove final error message due to EOF
#define IOSTREAM_MSDK_CHECK_NOT_EQUAL(P, X, ERR)          {if ((X) != (P)) {return ERR;}}

/* ************************************************************************* */

enum
{
    VPP_IN   = 0,
    VPP_OUT  = 1,
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

typedef struct _filtersParam
{
    sOwnFrameInfo             *pOwnFrameInfo      ;
    sDIParam                  *pDIParam           ;
    sProcAmpParam             *pProcAmpParam      ;
    sDetailParam              *pDetailParam       ;
    sDenoiseParam             *pDenoiseParam      ;
    sVideoAnalysisParam       *pVAParam           ;
    sIDetectParam             *pIDetectParam      ;
    sFrameRateConversionParam *pFRCParam          ;
    sMultiViewParam           *pMultiViewParam    ;
    sGamutMappingParam        *pGamutParam        ;
    sTccParam                 *pClrSaturationParam;
    sAceParam                 *pContrastParam     ;
    sSteParam                 *pSkinParam         ;
    sIStabParam               *pImgStabParam      ;
    sSVCParam                 *pSVCParam          ;
    sVideoSignalInfoParam     *pVideoSignalInfo   ;
    sMirroringParam           *pMirroringParam;
} sFiltersParam;

struct sInputParams
{
    /* smart filters defined by mismatch btw src & dst */
    std::vector<sOwnFrameInfo            > frameInfoIn;// [0] - in, [1] - out
    std::vector<sOwnFrameInfo            > frameInfoOut;// [0] - in, [1] - out

    /* Video Enhancement Algorithms */
    std::vector<sDIParam                 > deinterlaceParam;
    std::vector<sDenoiseParam            > denoiseParam;
    std::vector<sDetailParam             > detailParam;
    std::vector<sProcAmpParam            > procampParam;
    std::vector<sFrameRateConversionParam> frcParam;

    std::vector<sVideoSignalInfoParam    > videoSignalInfoParam;
    std::vector<sMirroringParam          > mirroringParam;

    // MSDK 3.0
    std::vector<sGamutMappingParam       > gamutParam;
    std::vector<sMultiViewParam          > multiViewParam;

    // MSDK API 1.5
    std::vector<sTccParam                > tccParam;
    std::vector<sAceParam                > aceParam;
    std::vector<sSteParam                > steParam;
    std::vector<sIStabParam              > istabParam;

    // flag describes type of memory
    // true  - frames in video memory (d3d surfaces),
    // false - in system memory
    //bool   bd3dAlloc;
    mfxU16   IOPattern;
    mfxIMPL  ImpLib;
    mfxU32   sptr;
    mfxU16   asyncNum;
    mfxU32   vaType;

    std::vector<mfxU16                   > rotate;

    bool     bScaling;
    mfxU16   scalingMode;

    bool     bInitEx;
    mfxU16   GPUCopyValue;

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
    msdk_char strPlgGuid[MSDK_MAX_FILENAME_LEN];
    bool    need_plugin;

    /* ********************** */
    /* input\output streams   */
    /* ********************** */
    msdk_char  strSrcFile[MSDK_MAX_FILENAME_LEN];
    std::vector<msdk_tstring> strDstFiles;

    msdk_char  strPerfFile[MSDK_MAX_FILENAME_LEN];
    bool  isOutYV12;

    /* Use extended API (RunFrameVPPAsyncEx) */
    bool  use_extapi;

    /* MFXVideoVPP_Reset */
    std::vector<mfxU32> resetFrmNums;

    sOwnFrameInfo inFrameInfo[MAX_INPUT_STREAMS];
    mfxU16        numStreams;
    sOwnFrameInfo outFrameInfo;

    sCompositionParam   compositionParam;

    sInputParams()
    {
        bInitEx             = false;
        bPerf               = false;
        need_plugin         = false;
        use_extapi          = false;
        MSDK_ZERO_MEMORY(strPlgGuid);
        MSDK_ZERO_MEMORY(strSrcFile);
        MSDK_ZERO_MEMORY(strPerfFile);
        strDstFiles.clear();
    }
};

struct sFrameProcessor
{
    MFXVideoSession     mfxSession;
    MFXVideoVPP*        pmfxVPP;
    mfxPluginUID        mfxGuid;
    bool                plugin;

    sFrameProcessor( void ){ pmfxVPP = NULL; plugin = false; return;};
};

struct sMemoryAllocator
{
    MFXFrameAllocator*  pMfxAllocator;
    mfxAllocatorParams* pAllocatorParams;
    bool                bUsedAsExternalAllocator;

    mfxFrameSurface1*     pSurfacesIn[MAX_INPUT_STREAMS]; // SINGLE_IN/OUT/MULTIPLE_INs
    mfxFrameSurface1*     pSurfacesOut;
    mfxFrameAllocResponse responseIn[MAX_INPUT_STREAMS];  // SINGLE_IN/OUT/MULTIPLE_INs
    mfxFrameAllocResponse responseOut;

    mfxFrameSurface1*     pSvcSurfaces[8]; //output surfaces per layer
    mfxFrameAllocResponse svcResponse[8];     //per layer

    CHWDevice* pDevice;

    /*#ifdef D3D_SURFACES_SUPPORT
    IDirect3DDeviceManager9* pd3dDeviceManager;
    #endif

    #ifdef MFX_D3D11_SUPPORT
    ID3D11Device *pD3D11Device;
    ID3D11DeviceContext *pD3D11DeviceContext;
    #endif

    #ifdef LIBVA_SUPPORT
    std::auto_ptr<CLibVA> libvaKeeper;
    #endif*/
};

class PTSMaker;

class CRawVideoReader
{
public :

    CRawVideoReader();
    ~CRawVideoReader();

    void       Close();

    mfxStatus  Init(
        const msdk_char *strFileName,
        PTSMaker *pPTSMaker);

    mfxStatus  PreAllocateFrameChunk(
        mfxVideoParam* pVideoParam,
        sInputParams* pParams,
        MFXFrameAllocator* pAllocator);

    mfxStatus  GetNextInputFrame(
        sMemoryAllocator* pAllocator,
        mfxFrameInfo* pInfo,
        mfxFrameSurface1** pSurface,
        mfxU16 streamIndex);

    mfxStatus  LoadNextFrame(
        mfxFrameData* pData,
        mfxFrameInfo* pInfo);

private:
    mfxStatus  GetPreAllocFrame(mfxFrameSurface1 **pSurface);

    FILE*       m_fSrc;
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
        const msdk_char *strFileName,
        PTSMaker *pPTSMaker,
        bool outYV12  = false);

    mfxStatus  PutNextFrame(
        sMemoryAllocator* pAllocator,
        mfxFrameInfo* pInfo,
        mfxFrameSurface1* pSurface);

private:
    mfxStatus  WriteFrame(
        mfxFrameData* pData,
        mfxFrameInfo* pInfo);

    FILE*      m_fDst;
    PTSMaker                              *m_pPTSMaker;
    bool                                   m_outYV12;
    std::auto_ptr<mfxU8>                   m_outSurfYV12;
};


class GeneralWriter // : public CRawVideoWriter
{
public :

    GeneralWriter();
    ~GeneralWriter();

    void       Close();

    mfxStatus  Init(
        const msdk_char *strFileName,
        PTSMaker *pPTSMaker,
        sSVCLayerDescr*  pDesc = NULL,
        bool outYV12 = false);

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
    CRawVideoReader*    pSrcFileReaders[MAX_INPUT_STREAMS];
    mfxU16              numSrcFiles;

    //CRawVideoWriter*    pDstFileWriter;
    GeneralWriter*      pDstFileWriters;
    mfxU32              dstFileWritersN;

    sFrameProcessor*    pProcessor;
    mfxVideoParam*      pVppParams;
    sMemoryAllocator*   pAllocator;
    sInputParams*       pParams;
    SurfaceVPPStore*    pSurfStore;

    /* VPP extension */
    mfxExtVppAuxData*   pExtVPPAuxData;
    mfxExtVPPDoUse      extDoUse;
    mfxU32              tabDoUseAlg[ENH_FILTERS_COUNT];
    mfxExtBuffer*       pExtBuf[1+ENH_FILTERS_COUNT];

    /* config video enhancement algorithms */
    mfxExtVPPProcAmp    procampConfig;
    mfxExtVPPDetail     detailConfig;
    mfxExtVPPDenoise    denoiseConfig;
    mfxExtVPPRotation   rotationConfig;
    mfxExtVPPScaling    scalingConfig;
    mfxExtVPPFrameRateConversion    frcConfig;
    mfxExtVPPDeinterlacing deinterlaceConfig;
    mfxExtVPPVideoSignalInfo  videoSignalInfoConfig;
    mfxExtVPPMirroring  mirroringConfig;
    mfxExtVPPComposite     compositeConfig;

    // MSDK 3.0
    //  mfxExtVPPGamutMapping gamutConfig;
    mfxExtMVCSeqDesc      multiViewConfig;


    ////MSDK API 1.5
    //mfxExtVPPSkinTone              steConfig;
    //mfxExtVPPColorSaturationLevel  tccConfig;
    mfxExtVPPImageStab             istabConfig;

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
    sInputParams* pInParams,
    mfxU32 paramID);

mfxStatus InitResources(
    sAppResources* pResources,
    mfxVideoParam* pParams,
    sInputParams* pInParams);

void WipeResources(sAppResources* pResources);
void WipeParams(sInputParams* pParams);

mfxStatus UpdateSurfacePool(mfxFrameInfo SurfacesInfo, mfxU16 nPoolSize, mfxFrameSurface1* pSurface);
mfxStatus GetFreeSurface(
    mfxFrameSurface1* pSurfacesPool,
    mfxU16 nPoolSize,
    mfxFrameSurface1** ppSurface);

const msdk_char* IOpattern2Str(
    mfxU32 IOpattern);

mfxStatus vppParseInputString(
    msdk_char* strInput[],
    mfxU8 nArgNum,
    sInputParams* pParams,
    sFiltersParam* pDefaultFiltersParam);

bool CheckInputParams(
    msdk_char* strInput[],
    sInputParams* pParams );

void vppPrintHelp(
    const msdk_char *strAppName,
    const msdk_char *strErrorMessage);

mfxStatus ConfigVideoEnhancementFilters(
    sInputParams* pParams,
    sAppResources* pResources,
    mfxU32 paramID );

const msdk_char* PicStruct2Str( mfxU16  PicStruct );

mfxStatus ParseCompositionParfile(const msdk_char* parFileName, sInputParams* pParams);
#endif /* __SAMPLE_VPP_UTILS_H */
/* EOF */
