/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2020 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_PIPELINE_DEC_H
#define __MFX_PIPELINE_DEC_H

#include "mfx_pipeline_defs.h"

#if defined(_WIN64) || defined(_WIN32)
#include "mfxadapter.h"
#endif

#ifdef D3D_SURFACES_SUPPORT
    #include <d3d9.h>
    #include <dxva2api.h>
#endif
//#if MFX_D3D11_SUPPORT
//    #include <d3d11.h>
//#endif
#include "mfx_ipipeline.h"
#include "mfx_renders.h"
#include "mfx_ihw_device.h"
#include "test_statistics.h"
#include "mfx_loop_decoder.h"
#include "mfx_commands.h"
#include "mfx_bitsream_buffer.h"
#include "mfx_cmd_option_processor.h"
#include "mfx_component_params.h"
#include "mfx_commands_initializer.h"
#include "mfx_ifactory.h"
#include "mfx_bitstream2.h"
#ifdef PAVP_BUILD
#include "mfx_bitstream_reader_encryptor.h"
#endif
#include "mfx_ibitstream_reader.h"
#include "mfx_shared_ptr.h"
#include "mfx_thread.h"
#include "shared_utils.h"

#ifdef PAVP_BUILD
typedef enum CPImpl
{
    cpImplUnknown = 0,
    cpImplSW = 1,
    cpImplHW = 2
} CPImpl;
#endif//PAVP_BUILD

//parameters that are set directly from cmd line
struct sCommandlineParams
{
  mfxU32         fps_frame_window;   // Specifies how many frames should be used for per-window fps calculation
  bool           bExtendedFpsStat;   // Show extended FPS related info
  bool           bUpdateWindowTitle; // Show progress in window title

  bool           bExtVppApi;   // Being set to true forces using RunFrameVPPAsyncEx
  bool           bUseExplicitEncShift; // true if shift specified from cmd

  mfxU32         InputCodecType;
  mfxFrameInfo   FrameInfo    = {}; //decode input frame info
  mfxFrameInfo   outFrameInfo = {}; //file writerRender output frameInfo
  mfxExtBuffer** ExtParam;
  mfxU16         NumExtParam;
  //mfxF64         dFrameRate;
  mfxU32         nFrames;
  mfxI32         nPicStruct; // 0-progressive, 1-tff, 2-bff, 3-field tff, 4-field bff
  mfxU32         HWAcceleration; // 0=SW, 1=HW+SW, 2=SW+HW, 3=HW
#if defined(LINUX32) || defined(LINUX64)
  std::string    strDevicePath; // path to device for processing
#endif
  mfxU32         nCorruptionLevel;
  double         fLimitPipelineFps; //sleeps in every frame processing
  mfxU32         nLimitFileReader; //sleeps in every frame processing
  mfxU32         nLimitChunkSize; //limits chunk reading from downstreams
  mfxU64         nLimitInputBs;
  mfxI32         nSeed;
  mfxU16         nInputBitdepth;
  MemoryModel    nMemoryModel;

#if (MFX_VERSION >= MFX_VERSION_NEXT)
  bool           bIgnoreLevelConstrain;
#endif
  bool           bNoVpp;    // don't enable VPP component (ERROR is returned if VPP is necessary)
  bool           bVppScaling;
  mfxU16         uVppScalingMode;
  mfxU16         uVppInterpolationMethod;
  bool           bVppChromaSiting;
  mfxU16         uVppChromaSiting;
  bool           isDefaultFC;
  bool           bVerbose;
  bool           bSkipUselessOutput;
  bool           bPrintSplTimeStamps;
  bool           bYuvReaderMode;//indicates whether yuv reader should be used
  bool           bExactSizeBsReader;
  bool           bDXVA2Dump; //
  bool           bCreateD3D; // force to create D3D device and pass to MediaSDK via SetHandle
  bool           bUseSeparateFileWriter;//forces render to outputs to separate files in case of diffresol
  bool           bOptimizeForPerf;//pipeline will be optimized for performance calculation, i.e. prebuffering
  bool           bJoinSessions;
  bool           bCompleteFrame;//if used along with demuxer will mark input frame as full tru DataFlag
  bool           bFullscreen; //fullscreen mode for D3D render
  bool           bNoPipelineSync;//removes syncpoints waiting between MSDK components
  bool           bNoExtPicstruct;//if specified decoder wont produce extended picstruct combinations
  bool           bMultiFiles;//used in multiview render to allow create output in different files
  bool           bCreateRefListControl;
  bool           bExternalRefListEncodedOrder;
  bool           bCreateEncFrameInfo;
  bool           bCreatePerFrameExtBuf;
  bool           bCalcCRC;//create crc calculation wrapper over filewriter
  bool           bNullFileWriter;
  bool           bDisableIpFieldPair;//disables p field generation
  bool           bPAFFDetect;//disables p field generation
  bool           bDxgiDebug;//
  bool           bMediaSDKSplitter;
  bool           bAdaptivePlayback;
  bool           bVP9_DRC;
  bool           bDisableSurfaceAlign;
  bool           isRawSurfaceLinear;
  bool           bVDSFCFormatSetting;
  vm_char        extractedAudioFile[MAX_FILE_PATH] = {};
  mfxU16         nAdvanceFRCAlgorithm;//if non zero then directly specifies advanced FRC algorithm
  mfxU16         nImageStab;//image stabilization mode
  mfxU16         nSVCDownSampling;//downsampling method
  mfxU16         targetViewsTemporalId;//default value is 7 - will instruct decoder to output all frames for selected views
  mfxU16         nBurstDecodeFrames;
  mfxI32         nYUVLoop;//preload yuv and return as input in the loop
  mfxI32         nDecodeInAdvance;//buffer decoded frames
  mfxU16         DecodedOrder;
  mfxU16         EncodedOrder;
  mfxU32         nDecoderSurfs;
#if (MFX_VERSION >= MFX_VERSION_NEXT)
  mfxU16         DisableFilmGrain;
#endif
  EncodeExtraParams encodeExtraParams;

  mfxU16         InputPicstruct;
  mfxU16         OutputPicstruct;

  vm_char        strSrcFile[MAX_FILE_PATH] = {};
  vm_char        crcFile[MAX_FILE_PATH] = {};
  vm_char        strDstFile[MAX_FILE_PATH] = {};
  vm_char        strParFile[MAX_FILE_PATH] = {};
  vm_char        perfFile[MAX_FILE_PATH] = {};
  vm_char        refFile[MAX_FILE_PATH] = {};
  vm_char        dxva2DllName[MAX_FILE_PATH] = {};

  vm_char        BackBufferFormat[MAX_FILE_PATH] = {};
  //outline
  vm_char        strOutlineFile[MAX_FILE_PATH] = {};     //output used by outline render
  vm_char        strOutlineInputFile[MAX_FILE_PATH] = {};//input used by outline reader

  mfxU32         nHRDBufSizeInKB;
  mfxU32         nHRDInitDelayInKB;
  mfxU32         nMaxBitrate;
  mfxU32         nDecBufSize;

  // Overlay text options
  vm_char        OverlayText[MAX_FILE_PATH] = {};
  int            OverlayTextSize;

  bool           bFadeBackground;

  //jpeg support for rotation field
  mfxU16         nRotation;

  //multiple reliability support
  mfxU32         nRepeat;
  mfxU32         nTimeout;

  //process priority settings
  mfxI8          nPriority;

  //vpp specific
  bool           bUseVPP;
  bool           bUseVPP_ifdi;
  mfxU16         nVppDeinterlacingMode;
  bool           bSceneAnalyzer;
  mfxU16         nDenoiseFactorPlus1;
  mfxU16         nDetailFactorPlus1;
  bool bUseCameraPipe;
  bool bUseCameraPipePadding;
  mfxU16         nFieldProcessing;
  bool           bFieldProcessing;
  bool           bSwapFieldProcessing;
  bool           bFieldWeaving;
  bool           bFieldSplitting;
  bool           bDenoiseMode;
  mfxDenoiseMode nDenoiseMode;
  mfxU16         nDenoiseStrength;

  bool           bUsePrivateDDI;

  bool bUseProcAmp;
  mfxExtVPPProcAmp m_ProcAmp;
  mfxExtVPPFieldProcessing m_FieldProcessing;

  mfxI32         m_ExtOptions;

  // Splitting params
  mfxContainer  m_container;

  // video wall
  mfxU32        m_WallW;
  mfxU32        m_WallH;
  mfxU32        m_WallN;
  // direct window position - shouldn't mess with videowall keys
  IppiRect      m_directRect;

  mfxU32        m_bNowWidowHeader;
  mfxU16        m_nMonitor;//monitor id on which to create rendering window

  bool          bUseOverlay;
  mfxU16        m_nBackbufferCount; // Number of backbuffers to be used for Directx9-based rendering

  // future mark notifications
  mfxU32         nTestId;

  //svc target layer
  mfxExtSvcTargetLayer svc_layer;

  bool isAllegroTest;
  bool isHMTest;
  bool VpxDec16bFormat;
  bool isPreferNV12;
  bool isForceDecodeDump;

  bool           useEncOrderParFile;
  vm_char        encOrderParFile[MAX_FILE_PATH] = {};

  vm_char        encOrderRefListsParFile[MAX_FILE_PATH] = {};

  // true means using InitEx, false - Init
  bool          bInitEx;

  mfxU16        nGpuCopyMode;

  bool          bGPUHangRecovery;
  mfxU32        nFramesAfterRecovery;

#ifdef PAVP_BUILD
  // protected
  mfxU16 Protected; //Protected in mfxVideoParam
  vm_char strPAVPLibPath[MAX_FILE_PATH] = {};

  CPImpl cpImpl;

  mfxU16 ctr_dec_type; //CounterType in mfxExtPAVPOption
  bool startctr_dec_set;
  mfxU64 startctr_dec;

  mfxU16 ctr_enc_type; //CounterType in mfxExtPAVPOption
  bool startctr_enc_set;
  mfxU64 startctr_enc;

  BitstreamReaderEncryptor::params BitstreamReaderEncryptor_params;
  mfxU16 encType; //EncryptionType in mfxExtPAVPOption
  mfxExtPAVPOption extEncPavpOption;
  mfxExtPAVPOption extDecPavpOption;
#endif//PAVP_BUILD

#if (MFX_VERSION >= MFX_VERSION_NEXT)
  mfxU32           AV1LargeScaleTileMode; // MFX_LST_ANCHOR_FRAMES_*
  mfxU32           AV1AnchorFramesNum;
  vm_char          strAV1AnchorFilePath[MAX_FILE_PATH] = {};
#endif

  sCommandlineParams()
  {

#ifdef PAVP_BUILD
      vm_string_strcpy_s(strPAVPLibPath, MFX_ARRAY_SIZE(strPAVPLibPath), VM_STRING("mfx_pavp"));
      Protected = 0;
      cpImpl = cpImplUnknown;
      ctr_dec_type = 0;
      startctr_dec_set = false;
      startctr_dec = 0;

      ctr_enc_type = 0;
      startctr_enc_set = false;
      startctr_enc = 0;

      BitstreamReaderEncryptor_params.isFullEncrypt = false;
      BitstreamReaderEncryptor_params.sparse = false;
      encType = 0;
      extEncPavpOption = {};
      extDecPavpOption = {};
#endif//PAVP_BUILD
      nMemoryModel = GENERAL_ALLOC;
      //default parameters for sCommandLine
      isDefaultFC       =  true;
      //fourcc passed to decoder input(only for yuv case)
      FrameInfo.FourCC  =  MFX_FOURCC_NV12;
      //default set not to monochrome
      FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

      outFrameInfo.FourCC = MFX_FOURCC_UNKNOWN;
      outFrameInfo.BitDepthLuma = 8;
      outFrameInfo.BitDepthChroma = 8;
      outFrameInfo.Shift = 0;
      outFrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

      bSkipUselessOutput = false;
      isAllegroTest = false;
      isHMTest = true;
      VpxDec16bFormat = false;
      isPreferNV12 = false;
      isForceDecodeDump = false;

      useEncOrderParFile = false;

      nPicStruct        =  PIPELINE_PICSTRUCT_NOT_SPECIFIED;
      nSeed             = -1;
      nRepeat           =  1;
      targetViewsTemporalId = 7;

      //Field processing init
      m_FieldProcessing.Header.BufferId = MFX_EXTBUFF_VPP_FIELD_PROCESSING;
      m_FieldProcessing.Header.BufferSz = sizeof(mfxExtVPPFieldProcessing);
      bSwapFieldProcessing = false;
      bFieldProcessing = false;
      bFieldWeaving = false;
      bFieldSplitting = false;

      // ProcAmp default values
      m_ProcAmp.Brightness = 0.0f;
      m_ProcAmp.Contrast   = 1.0f;
      m_ProcAmp.Hue        = 0.0f;
      m_ProcAmp.Saturation = 1.0f;

      m_nBackbufferCount = 24;

      // There is no overlay text by default
      OverlayTextSize = 0;

      bFadeBackground = false;
      bVP9_DRC = false;
      bDisableSurfaceAlign = false;
      isRawSurfaceLinear = false;
      bAdaptivePlayback = false;
      nInputBitdepth = 8;
      bVDSFCFormatSetting = false;

      InputPicstruct  = NOT_ASSIGNED_VALUE;
      OutputPicstruct = NOT_ASSIGNED_VALUE;

#if (MFX_VERSION >= MFX_VERSION_NEXT)
      bIgnoreLevelConstrain = false;
#endif
      bNoVpp = false;
      bVppScaling = false;
      uVppScalingMode = MFX_SCALING_MODE_DEFAULT;
      uVppInterpolationMethod = MFX_INTERPOLATION_DEFAULT;

      bVppChromaSiting = false;
      uVppChromaSiting = MFX_CHROMA_SITING_UNKNOWN;

      nTestId = NOT_ASSIGNED_VALUE;

      bInitEx = false;
      nGpuCopyMode = MFX_GPUCOPY_DEFAULT;

#if (MFX_VERSION >= MFX_VERSION_NEXT)
      AV1LargeScaleTileMode = 0;
      AV1AnchorFramesNum = 0;
      DisableFilmGrain = 0;
#endif
      nYUVLoop = 0;
      nFramesAfterRecovery = 0;
      nImageStab = 0;
      svc_layer = {};
      EncodedOrder = 0;
      bJoinSessions = false;
      bExtVppApi = false;
      bUseExplicitEncShift = false;
      nCorruptionLevel = 0;
      bOptimizeForPerf = false;
      m_FieldProcessing = {};
      nDecBufSize = 0;
      bUseProcAmp = false;
      m_WallH = 0;
      m_bNowWidowHeader = false;
      nFrames = 0;
      bNullFileWriter = false;
      bMultiFiles = false;
      bYuvReaderMode = false;
      bNoExtPicstruct = false;
      nLimitInputBs = 0;
      bUseSeparateFileWriter = false;
      fps_frame_window = 0;
      nDetailFactorPlus1 = 0;
      m_WallW = 0;
      bCompleteFrame = false;
      bVerbose = false;
      bUseVPP = false;
      m_ExtOptions = 0;
      bDisableIpFieldPair = false;
      bDxgiDebug = false;
      bMediaSDKSplitter = false;
      nLimitFileReader = 0;
      bCalcCRC = false;
      m_directRect = {};
      bUpdateWindowTitle = false;
      InputCodecType = 0;
      DecodedOrder = 0;
      bExactSizeBsReader = false;
      nMaxBitrate = 0;
      nFieldProcessing = 0;
      bPrintSplTimeStamps = false;
      nTimeout = 0;
      bDXVA2Dump = false;
      bCreateD3D = false;
      nAdvanceFRCAlgorithm = 0;
      bPAFFDetect = false;
      bUsePrivateDDI = false;
      fLimitPipelineFps = 0.0f;
      nSVCDownSampling = 0;
      encodeExtraParams = {};
      bCreatePerFrameExtBuf = false;
      bSceneAnalyzer = false;
      nPriority = 0;
      bNoPipelineSync = false;
      nHRDInitDelayInKB = 0;
      nRotation = 0;
      ExtParam = nullptr;
      bExternalRefListEncodedOrder = false;
      HWAcceleration = 0;
      bUseOverlay = false;
      nHRDBufSizeInKB = 0;
      m_container = MFX_CONTAINER_RAW;
      bGPUHangRecovery = false;
      bUseCameraPipe = false;
      m_nMonitor = 0;
      NumExtParam = 0;
      m_WallN = 0;
      nDecodeInAdvance = 0;
      nDenoiseFactorPlus1 = 0;
      bDenoiseMode = false;
      nDenoiseMode = MFX_DENOISE_MODE_DEFAULT;
      nDenoiseStrength = 0;
      nLimitChunkSize = 0;
      nDecoderSurfs = 0;
      bUseVPP_ifdi = false;
      nVppDeinterlacingMode = 0;
      nBurstDecodeFrames = 0;
      bFullscreen = false;
      bUseCameraPipePadding = false;
      bCreateRefListControl = false;
      bExtendedFpsStat = false;
      bCreateEncFrameInfo = false;
  }
};

//TODO: remove this
//#ifdef D3D_SURFACES_SUPPORT
//void SetDumpingDirectory(char *dir);
//IDirect3DDeviceManager9 *CreateDXVASpy(IDirect3DDeviceManager9*);
//#endif

class tsRegistryEditor;

class MFXDecPipeline
    : public IMFXPipeline, public IPipelineControl, public ITime
{
public:
    MFXDecPipeline(IMFXPipelineFactory *pFactory);
    virtual ~MFXDecPipeline();

    //IMFXPipeline
    virtual mfxStatus        ProcessCommand(vm_char ** &argv, mfxI32 argc, bool bReportError = true);
    virtual mfxStatus        ReconstructInput( vm_char ** argv, mfxI32 argc, void* pReconstrcutedargs);
    virtual mfxStatus        BuildPipeline();
    // clear pipeline sate except commandline options parser state
    virtual mfxStatus        ReleasePipeline();
    virtual mfxStatus        Play();
    virtual mfxStatus        LightReset();
    virtual mfxStatus        HeavyReset();
    vm_char *                GetLastErrString();
    virtual int              PrintHelp();
    virtual mfxStatus        GetMulTipleAndReliabilitySettings(mfxU32 &nRepeat, mfxU32 &nTimeout);
    virtual mfxStatus        GetGPUErrorHandlingSettings(bool &bHeavyResetAllowed);
    virtual mfxStatus        SetRefFile(const vm_char * pRefFile, mfxU32 nDelta);
    virtual mfxStatus        GetOutFile(vm_char * ppOutFile, int bufsize);
    virtual mfxStatus        SetOutFile(const vm_char * pOutFile);
    virtual mfxStatus        ReduceMemoryUsage();
    virtual mfxStatus        SetSyncro(std::mutex * pSynchro) override;

    //ITime
    virtual mfxU64           GetTick(){return 0;}
    virtual mfxU64           GetFreq(){return 1;}
    virtual mfxF64           Current();
    virtual void             Wait(mfxU32 ){}

    //IPipelineControl
    virtual mfxStatus        GetYUVSource(IYUVSource**ppSource);
    virtual mfxStatus        GetSplitter (IBitstreamReader**ppSpliter);
    virtual mfxStatus        GetRender   (IMFXVideoRender **ppRender);
    virtual mfxStatus        GetTimer    (ITime ** ppTimer);
    virtual mfxStatus        GetVPP      (IMFXVideoVPP** ppVpp);
    virtual mfxStatus        GetVppParams(ComponentParams *& pParams);
    virtual mfxStatus        ResetAfterSeek();
    virtual mfxU32           GetNumDecodedFrames(void);
    virtual mfxStatus        GetEncoder  (IVideoEncode **  /*ppCtrl*/){return MFX_ERR_NONE;}

    tstring GetAppName() { return VM_STRING("mfx_player");}

protected:
    //Resolution of original input YUV
    mfxU32                   m_YUV_Width;
    mfxU32                   m_YUV_Height;
    mfxU32                   m_YUV_CropX;
    mfxU32                   m_YUV_CropY;
    mfxU32                   m_YUV_CropW;
    mfxU32                   m_YUV_CropH;

#if defined(_WIN32) || defined(_WIN64)
    virtual mfxStatus        RegKeySet();

    std::unique_ptr<tsRegistryEditor> m_reg;
#endif

    //Pipeline parameters
    sCommandlineParams       m_inParams = {}; // Note: {} calls constructor but doesn't set all to 0!
    enum
    {
        eDEC,
        eVPP,
        eREN,
        ePLUGIN,
    };

    typedef std::vector<ComponentParams> ComponentsContainer;
    ComponentsContainer      m_components;

    bool                     m_bWritePar;
    bool                     m_bStat;
    bool                     m_bPreventBuffering;//buffered reader wont be used - intends for perfopt with large streams
    bool                     m_bInPSNotSpecified;//behavior specific flag is not an input one
    MFXVideoRenderType       m_RenderType;

    //Pipeline objects
    IBitstreamReader       * m_pSpl;         // splitter
    std::unique_ptr<IYUVSource>             m_pYUVSource;   // decoder
    IMFXVideoVPP           * m_pVPP;         // vpp
    IMFXVideoRender        * m_pRender;      // render

#if defined(LINUX32) || defined(LINUX64)
    std::string              m_strDevicePath; // path to device for processing
#endif

    //d3d9 memory
    mfx_shared_ptr<IHWDevice> m_pHWDevice;
#if defined(D3D_SURFACES_SUPPORT) || MFX_D3D11_SUPPORT==1
    std::auto_ptr<IUnknown> m_dxvaSpy;
#endif

    bool                     m_bVPPUpdateInput;

    //execution time counters
    mfxU32                   m_nDecFrames;
    mfxU32                   m_nVppFrames;
    mfxU32                   m_nEncFrames;
    mfxF64                   m_fLastTime;    //pipeline current time
    mfxF64                   m_fFirstTime;   //time of the first frame
    Timer                    m_statTimer;
    std::vector<Ipp64f>      m_time_stampts;
    CmdOptionProcessor       m_OptProc;

    //components creation
    virtual ComponentParams* GetComponentParams(vm_char SYM);//to parse -dec: -vpp: -enc: set of options
    virtual mfxStatus        ProcessCommandInternal(vm_char ** &argv, mfxI32 argc, bool bReportError = true);
    virtual void             PrintCommonHelp();
    virtual mfxStatus        CheckParams();
    virtual mfxStatus        CreateCore();
    virtual mfxStatus        DecodeHeader();
    virtual mfxStatus        CreateSplitter();
    virtual mfxStatus        CreateVPP();
    virtual mfxStatus        InitVPP();
    virtual mfxStatus        CreateRender();
    virtual mfxStatus        CreateFileSink(std::auto_ptr<IFile> &pSink);//render ended with file sink object
    virtual mfxStatus        DecorateRender();//creates a bunch of decorators for existing render
    virtual mfxStatus        InitRenderParams();
    virtual mfxStatus        InitRender();
    virtual mfxStatus        InitInputBs(bool &bExtended);
    virtual mfxStatus        CreateYUVSource();
    virtual mfxStatus        InitYUVSource();
    virtual mfxStatus        CreateDeviceManager();
    virtual mfxStatus        CreateAllocator();

    //decoding+vpp+rendering
    virtual mfxStatus        RunDecode(mfxBitstream2 & pBs);
    virtual mfxStatus        RunVPP(mfxFrameSurface1 *pSurface);
    virtual mfxStatus        RunRender(mfxFrameSurface1* pSurface, mfxEncodeCtrl *pControl = NULL);

    virtual mfxStatus        WriteParFile();
    virtual mfxStatus        ReadParFile(const vm_char * pInFile, IProcessCommand * pHandler);
    virtual mfxStatus        CheckExitingCondition();//if number frames is limited by cmd line

    //////////////////////////////////////////////////////////////////////////
    //light reset support in case of invalid params, no reset for splitter, cmp render
    virtual mfxStatus        BuildMFXPart();
    virtual mfxStatus        ReleaseMFXPart();

    //////////////////////////////////////////////////////////////////////////
    //trick modes support
    std::vector<ActivatorCommandsFactory*> m_pFactories;
    std::list<ICommandActivator*> m_commands;
    std::list<ICommandActivator*> m_commandsProcessed;
    bool                     m_generateRandomSeek;
    //checking for available Commands
    virtual mfxStatus        ProcessTrickCommands();

    //////////////////////////////////////////////////////////////////////////
    //feeding buffer control support
    MFXBistreamBuffer        m_bitstreamBuf;


    //////////////////////////////////////////////////////////////////////////
    //metrics calc support
    std::vector<std::pair<MetricType, mfxF64> >m_metrics;

    //cpuusage measurement support
    mfxU64                  m_KernelTime;
    mfxU64                  m_UserTime;

    //usual it is run async functions set
    enum IncompatComponent
    {
        IP_DECASYNC = 1,
        IP_VPPASYNC = 2,
        IP_ENCASYNC = 3
    };

    //map that stores error incompatible params for any components calls
    std::map <IncompatComponent, bool> m_IPComponents;
    bool                                m_bResetAfterIncompatParams;
    bool                                m_bErrIncompat;
    bool                                m_bErrIncompatValid;

    //helper funtcions are
    bool isErrIncompatParams();
    mfxStatus HandleIncompatParamsCode( mfxStatus error_code
                                      , IncompatComponent ID
                                      , bool isNull);
    mfxStatus InitBitDepthByFourCC(mfxFrameInfo &info);
    //multivieencoding support
    MFXExtBufferVector m_InputExtBuffers;
    MFXExtBufferPtr<mfxExtDecVideoProcessing>  m_extDecVideoProcessing;

    //map for dec view order to enc view id map
    std::vector<std::pair<mfxU16, mfxU16> > m_viewOrderMap;

    //external synhro holder
    std::mutex* m_externalsync;

    //factory that creates everything inside pipeline
    std::auto_ptr<IMFXPipelineFactory>      m_pFactory;

    //single allocator factory shared among pipeline components
    std::auto_ptr<RWAllocatorFactory::root> m_pAllocFactory;

    mfx_shared_ptr<IRandom> m_pRandom;

    std::auto_ptr<MFXThread::ThreadPool> m_threadPool;

};

#ifdef PAVP_BUILD
#include "mfx_pipeline_protected.h"

class MFXProtectedDecPipeline: public MFXProtectedPipeline<MFXDecPipeline>
{
public:
    MFXProtectedDecPipeline(IMFXPipelineFactory *pFactory)
        :MFXProtectedPipeline<MFXDecPipeline>(pFactory)
    {
    };
protected:
    virtual mfxU32 getOutputCodecId() {return 0;};

};
#endif//PAVP_BUILD

#endif//__MFX_PIPELINE_DEC_H
