/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once
#include "sample_defs.h"
#include "hw_device.h"

#if D3D_SURFACES_SUPPORT
#pragma warning(disable : 4201)
#endif

#include <string>
#include <memory>
#include <map>

#include "mfxvideo++.h"
#include "sample_utils.h"
#include "base_allocator.h"
#include "interface.h"

enum MemType {
    SYSTEM_MEMORY = 0x00,
    D3D9_MEMORY   = 0x01,
    D3D11_MEMORY  = 0x02,
};

class VideoConfParams
    : public IInitParams
{
public:
    VideoConfParams():
        nTargetKbps(0),
        bRPMRS(false),
        bUseHWLib(false),
        memType(SYSTEM_MEMORY),
        bCalcLAtency(false),
        nTemporalScalabilityBase(0),
        nTemporalScalabilityLayers(0),
        nRefrType(0),
        nCycleSize(0),
        nQPDelta(0)
    {
    }
public:
    struct SourceInfo
    {
        std::basic_string<msdk_char> srcFile;
        mfxU16 nWidth;      // picture width
        mfxU16 nHeight;     // picture height
        mfxF64 dFrameRate;  // framerate
        SourceInfo():
            nWidth(0),
            nHeight(0),
            dFrameRate(0)
        {
        }
    };

    mfxU16 nTargetKbps;//target bitrate

    bool bRPMRS; //Reference Picture Marking Repetition SEI
    bool bUseHWLib; // whether to use HW mfx library
    MemType memType; // which memory type to use: system, d3d9
    bool bCalcLAtency;// optional printing of latency information per frame

    std::map<int, SourceInfo> sources;
    std::basic_string<msdk_char> dstFile;

    mfxU16 nTemporalScalabilityBase;
    mfxU16 nTemporalScalabilityLayers;

    //dynamic encoder control actions processor
    //prior encoding of particular frame several actions could be applied as a simulation of generic video conferencing infrastructure events
    std::auto_ptr <IActionProcessor> pActionProc;

    //external brc
    std::auto_ptr<IBRC> pBrc;

    //intra refresh
    int nRefrType; // refresh type: no refresh, vertical refresh
    int nCycleSize; // number of pictures within refresh cycle
    int nQPDelta; // QP delta for Intra refresh MBs
};

class VideoConfPipeline
    : public IPipeline
{
public:
    VideoConfPipeline();
    virtual ~VideoConfPipeline();

    //////////////////////////////////////////////////////////////////////////
    //IPipeline
    virtual mfxStatus   Run();
    virtual mfxStatus   Init(IInitParams *);
    virtual mfxStatus   Close();
    virtual mfxStatus   InsertKeyFrame();
    virtual mfxStatus   ScaleBitrate(double dScaleBy);
    virtual mfxStatus   AddFrameToRefList(RefListType refList, mfxU32 nFrameOrder);
    virtual mfxStatus   SetNumL0Active(mfxU16 nActive);
    virtual mfxStatus   ResetPipeline(int nSourceIdx);
    virtual void        PrintInfo();
    static const mfxU16 gopLength = 30;

protected:

    mfxStatus CreateAllocator();
    mfxStatus AllocFrames();
    mfxStatus GetFreeSurface(mfxFrameSurface1*& pSurfacesPool);
    mfxStatus CreateHWDevice();
    //special parameters setup to configure MediaSDK Encoder for producing low latency bitstreams
    mfxStatus InitMfxEncParamsLowLatency();
    //temporal scalability initialisation
    mfxStatus InitTemporalScalability();
    //mostly separated to call allign functions
    void InitFrameInfo(mfxU16 nWidth, mfxU16 nHeight);
    //initiliaze either Mediasdk's BRC or own
    void InitBrc();
    //intra refresh initialization
    void InitIntraRefresh();
    void AttachAvcRefListCtrl();
    mfxStatus InitMFXComponents();
    mfxStatus EncodeFrame(mfxFrameSurface1 *pSurface);

    void DeleteFrames();
    void DeleteHWDevice();
    void DeleteAllocator();

    VideoConfParams m_initParams;
    CSmplBitstreamWriter m_FileWriter;
    CSmplYUVReader m_FileReader;

    //encode query and initialization params
    std::auto_ptr<MFXVideoENCODE> m_encoder;
    MFXVideoSession m_mfxSession;
    mfxVideoParam m_mfxEncParams;
    MFXFrameAllocator* m_pMFXAllocator;
    mfxAllocatorParams* m_pmfxAllocatorParams;
    MemType m_memType;

    CHWDevice *m_hwdev;

    mfxFrameAllocResponse m_EncResponse;  // memory allocation response for encoder
    std::vector <mfxFrameSurface1> m_EncSurfaces;

    mfxBitstream m_encodedBs;
    std::vector <mfxU8> m_bsData; // data for encoded bitstream

    //
    mfxU32 m_nFramesProcessed;

    //ctrl always passed to encode frame async , however it is not NULL only for specific scenario frames
    mfxExtAVCRefListCtrl m_avcRefList;
    std::vector<mfxExtBuffer*> m_extBuffers;
    mfxEncodeCtrl m_ctrl, *m_pCtrl;

    struct RefListElement{
        mfxU32      FrameOrder;
        mfxU16      PicStruct;
        mfxU16      ViewId;
        mfxU32      reserved[2];
    } ;

    struct RefListExt
    {
        std::vector<RefListElement> elements;
        mfxU32 nSize; //currently valid items number
        std::basic_string<msdk_char> name;
        RefListExt()
            : nSize()
        {
        }
    };

    std::vector<RefListExt> m_ReferenceLists; // number of frames currently in specific reference list
    bool m_bAvcRefListAttached ;

    //extcoding options for instructing encoder to specify maxdecodebuffering=1
    mfxExtCodingOption m_extCO;

    //extcoding options for intra refresh
    mfxExtCodingOption2 m_extCO2;

    //
    mfxExtAvcTemporalLayers m_temporalScale;
};