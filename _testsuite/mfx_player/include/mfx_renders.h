/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_RENDERS_H
#define __MFX_RENDERS_H

#include <stdio.h>
#include "mfx_decoder.h"
#include "mfx_io_utils.h"
#include "mfx_metric_calc.h"
#include "mfx_ivideo_render.h"
#include "mfx_ifile.h"
#include "mfx_ivideo_session.h"

enum MFXVideoRenderType
{
    MFX_NO_RENDER           = 0,
    MFX_SCREEN_RENDER       = 0x01,
    MFX_FW_RENDER           = 0x04,
    MFX_ENC_RENDER          = 0x08,
    MFX_ENCDEC_RENDER       = 0x10,
    MFX_METRIC_CHECK_RENDER = 0x20,
    MFX_OUTLINE_RENDER      = 0x40,
    MFX_NULL_RENDER         = 0x80,
    MFX_BMP_RENDER          = 0x100

};

struct FileWriterRenderInputParams
{
    FileWriterRenderInputParams(mfxU32  fourCC = 0)
    {
        info = mfxFrameInfoCpp(0,0,0,0, fourCC);
        useSameBitDepthForComponents = false;
        alwaysWriteChroma = true;
    }

    mfxFrameInfo info;
    bool useSameBitDepthForComponents;
    bool use10bitOutput;
    bool useHMstyle;
    bool VpxDec16bFormat;
    bool alwaysWriteChroma;
};


//////////////////////////////////////////////////////////////////////////
//base implementation for IMFXVideoRender
class MFXVideoRender 
    : public IMFXVideoRender
{
    IMPLEMENT_CLONE(MFXVideoRender);
public:
    MFXVideoRender(IVideoSession *core, mfxStatus *status);
    // IMFXVideoRender
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) ;
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);
    virtual mfxStatus Init(mfxVideoParam *par, const vm_char *pFilename = NULL);
    virtual mfxStatus Close();
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus GetVideoParam(mfxVideoParam *par); 
    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl = NULL);
    virtual mfxStatus WaitTasks(mfxU32 nMilisecconds);
    //virtual mfxStatus SetOutputFourcc(mfxU32 nFourCC);
    virtual mfxStatus SetAutoView(bool bIsAutoViewRender);
    virtual mfxStatus GetDownStream(IFile **ppFile);
    virtual mfxStatus SetDownStream(IFile *ppFile);
    virtual mfxStatus GetDevice(IHWDevice **pDevice);


protected:
//    mfxU32                  m_nFourCC; //output color format
    mfxU32                  m_nFrames;
    IVideoSession          *m_pSessionWrapper;
    bool                    m_bFrameLocked;
    mfxVideoParam           m_VideoParams;//params in init
    bool                    m_bIsViewRender;
    bool                    m_bAutoView;
    mfxU32                  m_nViewId;         
    std::auto_ptr<IFile>    m_pFile;

    virtual mfxStatus LockFrame(mfxFrameSurface1 *surface);
    virtual mfxStatus UnlockFrame(mfxFrameSurface1 *surface);
};

class MFXFileWriteRender : public MFXVideoRender
{
public :
    MFXFileWriteRender(const FileWriterRenderInputParams &params, IVideoSession *core, mfxStatus *status);
    ~MFXFileWriteRender();

    virtual mfxStatus Init(mfxVideoParam *par, const vm_char *pFilename = NULL);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl = NULL);
    virtual mfxStatus Close();

    //Icloneable
    virtual MFXFileWriteRender * Clone();

protected:
    virtual mfxStatus PutData(mfxU8* pData, mfxU32 nSize);
    virtual mfxStatus PutBsData(mfxBitstream *pBs);
    //converted surface passed to this function
    virtual mfxStatus WriteSurface(mfxFrameSurface1*pSurface);

    FileWriterRenderInputParams  m_params;
    
    mfxU32             m_nFourCC; //output color format
    const vm_char *    m_pOpenMode;
    mfxFrameSurface1   m_yv12Surface;
    bool               m_bCreateNewFileOnClose;
    tstring            m_OutputFileName;
    int                m_nTimesClosed;
    std::map<int, char*>m_lucas_buffer; //per view buffer

    struct FramePosition
    {
        mfxU32 m_pixX;
        mfxU32 m_pixY;
        mfxF64 m_valRef;
        mfxF64 m_valTst;
        vm_char  m_comp;//plane
    } m_Current;
    
};

class MFXBMPRender : public MFXFileWriteRender
{
    struct bmpfile_magic {
        unsigned char magic[2];
    };

    struct bmpfile_header {
        mfxU32 filesz;
        mfxU16 creator1;
        mfxU16 creator2;
        mfxU16 bmp_offset;
    };

    struct bmpfile_DIBheader{
        mfxU32 header_sz;
        mfxI32 width;
        mfxI32 height;
        mfxU16 nplanes;
        mfxU16 bitspp;
        mfxU32 compress_type;
        mfxU32 bmp_bytesz;
        mfxI32 hres;
        mfxI32 vres;
        mfxU32 ncolors;
        mfxU32 nimpcolors;
    } ;

public:
    MFXBMPRender(IVideoSession *core, mfxStatus *status);
    ~MFXBMPRender();

    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl = NULL);
protected:
    //writes flipped surface
    mfxStatus WriteSurface(mfxFrameSurface1 * pConvertedSurface);
};

class IMetricComparator
{
public:
    virtual ~IMetricComparator(){}

    //surfaces source either file to be red of single surface pointer
    virtual mfxStatus SetSurfacesSource(const vm_char      *pFile,
                                        mfxFrameSurface1 *pRefSurface,
                                        bool             bVerbose = false,
                                        mfxU32           nLimitFrames = 0) = 0;
    
    //-1 means that metric just calculated and is not used for checking limit condition
    virtual mfxStatus AddMetric(MetricType type, mfxF64 fVal = -1)         = 0;
    //specifies output file for metrics printing    
    virtual mfxStatus SetOutputPerfFile(const vm_char *pOutFile)             = 0;

};
        
class MFXMetricComparatorRender : public MFXFileWriteRender, public IMetricComparator
{
    
public:
    MFXMetricComparatorRender(const FileWriterRenderInputParams & outinfo, IVideoSession *core, mfxStatus *status);
    ~MFXMetricComparatorRender();

    //////////////////////////////////////////////////////////////////////////
    //IMetrics Comparator
    virtual mfxStatus AddMetric(MetricType type, mfxF64 fVal = -1);
    //might be delayed until first frame
    virtual mfxStatus SetSurfacesSource(const vm_char      *pFile,
                                        mfxFrameSurface1 *pRefSurface,
                                        bool             bVerbose = false,
                                        mfxU32           nLimitFrames = 0);//delays until first frame input
    //might be delayed until first frame
    virtual mfxStatus SetOutputPerfFile(const vm_char *pOutFile ); //delays until first frame input
    //////////////////////////////////////////////////////////////////////////

    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl = NULL);
    virtual mfxStatus Init(mfxVideoParam *pInit, const vm_char *pFilename = NULL);
    virtual mfxStatus Close();

    //Icloneable
    virtual MFXMetricComparatorRender * Clone();

protected:
    virtual mfxStatus ReallocArray(mfxU32 nSize);
            void      ReportDifference(const vm_char * metricName);

protected:
    virtual mfxStatus PutData(mfxU8* pData, mfxU32 nSize);
    
    std::auto_ptr<CYUVReader> m_reader;
    mfxFrameSurface1     m_refsurface;
    mfxFrameSurface1    *m_pRefsurface;
    bool                 m_bAllocated;
    mfxU64               m_nInFileLen;
    mfxU64               m_nNewFileOffset;
    mfxU64               m_nOldFileOffset;//offset after processing prev frame
    mfxU8*               m_pRefArray;
    mfxU32               m_nRefArray;
    std::vector<std::pair<IMFXSurfacesComparator *, mfxF64> >m_pComparators;
    std::vector<std::pair<MetricType, mfxF64> > m_metrics_input;
    mfxU8                m_uiDeltaVal;
    mfxU32               m_metricType;
    bool                 m_bFileSourceMode;
    bool                 m_bVerbose;
    mfxU32               m_nFrameToProcess;
    tstring              m_pMetricsOutFileName;
    tstring              m_SurfacesSourceFileName;
    bool                 m_bFirstFrame;//delayed init 
    bool                 m_bDelaySetSurface;
    bool                 m_bDelaySetOutputPerfFile;
};

mfxFrameSurface1* ConvertSurface(mfxFrameSurface1* pSurfaceIn, mfxFrameSurface1* pSurfaceOut, FileWriterRenderInputParams * params);
mfxStatus       AllocSurface(mfxFrameInfo *pTargetInfo, mfxFrameSurface1* pSurfaceOut);

#endif //__MFX_RENDERS_H
