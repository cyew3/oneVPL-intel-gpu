/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2021 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ipipeline.h"
#include "mfx_pipeline_dec.h"
#include "mfx_ext_buffers.h"
#include "mfx_encoder_wrap.h"
#include "mfx_svc_serial.h"
#include "mfx_binder_call.h"

#define MAX_EXT_BUFFERS 32

enum //TranscodePipeLineError codes
{
    PE_NO_TARGET_CODEC = 0x100,
};

class MFXTranscodingPipeline : public MFXDecPipeline
{
public :
    MFXTranscodingPipeline(IMFXPipelineFactory *pFactory);
    ~MFXTranscodingPipeline();

    virtual std::unique_ptr<IVideoEncode> CreateEncoder();
    virtual mfxStatus ReleasePipeline();
    virtual vm_char *   GetLastErrString();
    virtual int       PrintHelp();
    mfxStatus         SetRefFile(const vm_char * pRefFile, mfxU32 nDelta);
    tstring GetAppName() { return VM_STRING("mfx_transcoder");}

    //////////////////////////////////////////////////////////////////////////
    //IPipelineControl
    virtual mfxStatus GetEncoder(IVideoEncode **ppAdded);

protected:
    typedef struct OptContainer
    {
        //pattern ()| operators could be use to construct pattern
        const vm_char  *opt_pattern;
        //specifies parameter type : int, bool, etc
        OptParamType   nType;
        union
        {
            void      *  pTarget;
            mfxI32    *  pTargetInt32;
            mfxU32    *  pTargetUInt32;
            mfxU32    *  pTargetUInt64;
            mfxU16    *  pTargetUInt16;
            mfxU8     *  pTargetUInt8;
            mfxI16    *  pTargetInt16;
            mfxI8     *  pTargetInt8;
            mfxF64    *  pTargetF64;
            bool      *  pTargetBool;
        };

        const vm_char   *description;
        IBinderCall<mfxStatus> * pHandler;
        mfxU32 codecId;
    } OptContainer;
    typedef std::list<OptContainer>::iterator OptIter;
    std::list<OptContainer> m_pOptions;

    bool                            m_bPsnrMode;
    bool                            m_bYuvDumpMode;
    bool                            m_bSSIMMode;

    // parameter extension buffers
    MFXExtBufferPtr<mfxExtCodingOption>  m_extCodingOptions;
    MFXExtBufferPtr<mfxExtCodingOption2> m_extCodingOptions2;
    MFXExtBufferPtr<mfxExtCodingOption3> m_extCodingOptions3;
    MFXExtBufferPtr<mfxExtCodingOptionDDI> m_extCodingOptionsDDI;
#if defined(MFX_ENABLE_USER_ENCTOOLS) && defined(MFX_ENABLE_ENCTOOLS)
    MFXExtBufferPtr<mfxExtEncToolsConfig> m_extEncToolsConfig;
#endif
    MFXExtBufferPtr<mfxExtVideoSignalInfo> m_extVideoSignalInfo;
    MFXExtBufferPtr<mfxExtCodingOptionHEVC>  m_extCodingOptionsHEVC;
    MFXExtBufferPtr<mfxExtCodingOptionAV1E>  m_extCodingOptionsAV1E;
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    MFXExtBufferPtr<mfxExtAV1BitstreamParam>    m_extAV1BsParam;
    MFXExtBufferPtr<mfxExtAV1ResolutionParam>   m_extAV1RsParam;
    MFXExtBufferPtr<mfxExtAV1TileParam>         m_extAV1TileParam;
    MFXExtBufferPtr<mfxExtAV1Param>             m_extAV1Param;
    MFXExtBufferPtr<mfxExtAV1AuxData>           m_extAV1AuxData;
    MFXExtBufferPtr<mfxExtAV1Segmentation>      m_extAV1Segmentation;
    std::vector<std::unique_ptr<mfxU8>>         m_segMaps;
#endif
    MFXExtBufferPtr<mfxExtHEVCTiles> m_extHEVCTiles;
    MFXExtBufferPtr<mfxExtHEVCParam> m_extHEVCParam;
    MFXExtBufferPtr<mfxExtVP8CodingOption>  m_extVP8CodingOptions;
    MFXExtBufferPtr<mfxExtVP9Param>  m_extVP9Param;
    MFXExtBufferPtr<mfxExtVP9Segmentation>  m_extVP9Segmentation;
    MFXExtBufferPtr<mfxExtEncoderROI>  m_extEncoderRoi;
    MFXExtBufferPtr<mfxExtDirtyRect>  m_extDirtyRect;
    MFXExtBufferPtr<mfxExtMoveRect>  m_extMoveRect;

    //lync support
    MFXExtBufferPtr<mfxExtCodingOptionSPSPPS> m_extCodingOptionsSPSPPS;
    MFXExtBufferPtr<mfxExtAvcTemporalLayers> m_extAvcTemporalLayers;
    MFXExtBufferPtr<mfxExtVP9TemporalLayers> m_extVP9TemporalLayers;

    MFXExtBufferPtr<mfxExtSVCSeqDesc> m_svcSeq;
    StructureBuilder<mfxExtSVCSeqDesc> m_svcSeqDeserial;
    SerialCollection<tstring> m_filesForDependency;

    MFXExtBufferPtr<mfxExtSVCRateControl> m_svcRateCtrl;
    StructureBuilder<mfxExtSVCRateControl> m_svcRateCtrlDeserial;

    MFXExtBufferPtr<mfxExtEncoderCapability> m_extEncoderCapability;

    MFXExtBufferPtr<mfxExtEncoderResetOption> m_extEncoderReset;
    MFXExtBufferPtr<mfxExtMultiFrameParam> m_extMFEParam;
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    MFXExtBufferPtr<mfxExtTemporalLayers> m_extTemporalLayers;
#endif

    //MFXExtmfxExtBuffer                   m_ExtParamsBuffer[MAX_EXT_BUFFERS];
    mfxVideoParam                   m_EncParams;
    // those vectors are used to store ext buffers for reset_start/reset_end.
    std::auto_ptr<MFXExtBufferVector> m_ExtBuffers;
    std::auto_ptr<MFXExtBufferVector> m_ExtBuffersOld;

    // those vectors are used to store ext buffers for per_frame_start/per_frame_end.
    std::auto_ptr<MFXExtBufferVector> m_ExtBuffersPerFrame;

    // note: this list actually owns the objects it has pointers to
    std::list<MFXExtBufferVector *> m_ExtBufferVectorsContainer;

    //user buffer size support
    mfxU16                          m_nBufferSizeInKB;

    //////////////////////////////////////////////////////////////////////////
    //reset encoder support
    bool                            m_bResetParamsStart;
    mfxVideoParam                   m_EncParamsOld;
    mfxU32                          m_nResetFrame;//number of frame when execute reset
    tstring                         m_FileAfterReset;
    tstring                         m_inFileAfterReset;
    bool                            m_bTrackOptionMask;
    mfxVideoParam                   m_EncParamsMask;
    mfxI32                          m_OldBitRate;
    mfxI32                          m_OldMaxBitrate;
    mfxI32                          m_OldPicStruct;
    bool                            m_bUseResizing;//use vpp resize rather that changing settings in yuv reader

    //////////////////////////////////////////////////////////////////////////
    // support of per frame encoder parameters
    bool                            m_bPerFrameParamsStart;
    mfxU32                          m_nFrame;
    //////////////////////////////////////////////////////////////////////////
    bool                            m_bCreateDecode;
    //////////////////////////////////////////////////////////////////////////
    //AVBR
    mfxU16                          m_Accuracy;
    mfxU16                          m_Convergence;
    mfxU16                          m_OldAccuracy;
    mfxU16                          m_OldConvergence;

    mfxU16                          m_ICQQuality;

    //////////////////////////////////////////////////////////////////////////
    mfxI32                          m_BitRate;
    mfxI32                          m_MaxBitrate;
    mfxI32                          m_BitRateKbps;
    mfxI32                          m_MaxBitrateKbps;
    mfxI32                          m_WinBRCMaxAvgBps;
    mfxU16                          m_WinBRCSize;
    mfxU16                          m_QPI;    // constant quantizer for I frames
    mfxU16                          m_QPP;    // constant quantizer for P frames
    mfxU16                          m_QPB;    // constant quantizer for B frames

    mfxU16                          m_OldQPI;    // constant quantizer for I frames
    mfxU16                          m_OldQPP;    // constant quantizer for P frames
    mfxU16                          m_OldQPB;    // constant quantizer for B frames

    //jpeg specific options
    //////////////////////////////////////////////////////////////////////////
    mfxU16                          m_Interleaved;
    mfxU16                          m_Quality;
    mfxU16                          m_RestartInterval;

    virtual mfxStatus  CreateEncodeWRAPPER(std::unique_ptr<IVideoEncode> &&pEncoder, MFXEncodeWRAPPER ** ppEncoderWrp);
    //overrides from decoding pipeline
    virtual mfxStatus  CheckParams();
    virtual mfxStatus  CreateRender();
    virtual mfxStatus  CreateFileSink(std::auto_ptr<IFile> &pSink);//render ended with file sink object
    virtual mfxStatus  WriteParFile();
    virtual mfxStatus  ProcessCommandInternal(vm_char ** &argv, mfxI32 argc, bool bReportError = true);

    //non cached function - will always check that all dependency layers has it's files sources if so returns 1, 0 otherwise
    inline bool        IsMultiReaderEnabled();

    //for svc case need to have special version of vpp
    mfxStatus          CreateVPP();
    //for multiple decoders at least
    virtual mfxStatus  CreateYUVSource();
    //for multiple input files at least
    virtual mfxStatus  CreateSplitter();
    //for multiple input files at least
    virtual mfxStatus  CreateAllocator();
    virtual void       PrintCommonHelp();

    //////////////////////////////////////////////////////////////////////////
    //to cover encoder specific err_codes
    virtual mfxStatus  ProcessOption(vm_char ** &argv, vm_char **arvend);

    //params corrections that are not suitable in checkoptions and in checkparams
    virtual mfxStatus ApplyBitrateParams();
    virtual mfxStatus ApplyJpegParams();

    template<class T, class T1>  friend class BinderCall_0;

    BinderCall_0<mfxStatus, MFXTranscodingPipeline> m_applyBitrateParams;
    BinderCall_0<mfxStatus, MFXTranscodingPipeline> m_applyJpegParams;
    IVideoEncode *m_pEncoder ;

    //AVC skip frame support
    //////////////////////////////////////////////////////////////////////////
    tstring m_skippedFrames;
};

