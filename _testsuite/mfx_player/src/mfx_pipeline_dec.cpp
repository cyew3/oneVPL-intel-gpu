/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_pipeline_dec.h"
#include "mfx_spl_wrapper.h"
#include "mfx_mediasdk_spl_wrapper.h"
#include "mfx_sysmem_allocator.h"
#include "umc_automatic_mutex.h"
#include "vm_sys_info.h"
#include "mfx_buffered_bs_reader.h"
#include "mfx_directory_reader.h"
#include "mfx_ext_buffers.h"
#include "mfx_screen_render.h"
#include "mfx_vaapi_render.h"
#include "mfx_d3d9_device.h"
#include "mfx_d3d11_device.h"
#include "mfx_vaapi_device.h"
#include "mfx_vaapi_device_android.h"
#include "mfx_cmd_option_processor.h"

#include "mfx_dxva2_device.h"
#include "mfx_reorder_render.h"
#include "mfx_multi_render.h"

#ifdef MFX_PIPELINE_SUPPORT_VP8
#include "mfxvp8.h"
#endif

#include "mfxvp9.h"

#include "mfx_serializer.h"
#include "mfx_view_ordered_render.h"
#include "mfx_advance_decoder.h"
//#include "mfx_virtualvpp_adapter.h"
#include "mfx_vpp.h"
#include "mfx_mvc_target_views_decoder.h"
#include "mfx_latency_decoder.h"
#include "mfx_latency_vpp.h"
#include "mfx_perfcounter_time.h"
#include "mfx_bitrate_limited_reader.h"
#include "mfx_mkv_reader.h"
#include "mfx_bayer_reader.h"
#include "mfx_burst_render.h"
#include "mfx_fps_limit_render.h"
#include "mfx_pts_based_activator.h"
#include "mfx_frame_based_activator.h"
#include "mfx_file_generic.h"
#include "mfx_adapters.h"
#include "mfx_bitstream_reader.h"
#include "mfx_separate_files_render.h"
#include "mfx_separate_file.h"
#include "mfx_null_render.h"
#include "mfx_outline_render.h"
#include "mfx_mvc_handler.h"
#include "mfx_frames_drop.h"
#include "mfx_yuv_decoder.h"
#include "mfx_loop_decoder.h"
#include "mfx_message_notifier.h"
#include "mfx_crc_writer.h"
#include "mfx_null_file_writer.h"
#include "mfx_jpeg_bs_parser_decoder.h"
#include "mfx_skip_mode_decoder.h"
#include "mfx_svc_target_layer_decoder.h"
#include "mfx_rotated_decoder.h"
#include "mfx_exact_frame_bs_reader.h"
#include "mfx_utility_allocators.h"
#include "mfx_factory_chain.h"
#include "mfx_timeout.h"
#include "mfx_printinfo_decoder.h"
#include "mfx_factory_default.h"
#include "mfx_verbose_spl.h"
#include "mfx_dxgidebug_device.h"
#include "mfx_hw_device_in_thread.h"

#include "mfxstructures-int.h"

#define MFX_DISPATCHER_LOG
#include "mfx_dispatcher_log.h"

#ifdef LUCAS_DLL
#include "lucas.h"
#endif

#if defined(_WIN32) || defined(_WIN64)
#include "dxva2_spy.h"
#include <psapi.h>
#endif
#if defined(LINUX32) || defined(LINUX64)
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include "mfx_thread.h"

#include <iostream>
#include <math.h>

using namespace std;

#ifndef MFX_HANDLE_TIMING_LOG
#define MFX_HANDLE_TIMING_LOG       ((mfxHandleType)1001)
#endif
#ifndef MFX_HANDLE_EXT_OPTIONS
#define MFX_HANDLE_EXT_OPTIONS      ((mfxHandleType)1002)
#endif
#ifndef MFX_HANDLE_TIMING_SUMMARY
#define MFX_HANDLE_TIMING_SUMMARY   ((mfxHandleType)1003)
#endif
#ifndef MFX_HANDLE_TIMING_TAL
#define MFX_HANDLE_TIMING_TAL       ((mfxHandleType)1004)
#endif

//////////////////////////////////////////////////////////////////////////

MFXDecPipeline::MFXDecPipeline(IMFXPipelineFactory *pFactory)
    : m_YUV_Width(0)
    , m_YUV_Height(0)
    //, m_d3dDeviceManagerSpy()
    , m_RenderType(MFX_SCREEN_RENDER)
    , m_pSpl()
    , m_pVPP()
    , m_pRender()
    , m_inBSFrame()
    //, m_components[eDEC](VM_STRING("Decoder"))
    //, m_components[eVPP](VM_STRING("VPP"))
    //, m_components[eREN](VM_STRING("Encoder"))
    , m_bVPPUpdateInput(false)
    , m_OptProc(false)
    , m_bResetAfterIncompatParams()
    , m_bErrIncompat()
    , m_bErrIncompatValid(true)
    , m_extDecVideoProcessing(new mfxExtDecVideoProcessing())
    , m_extExtCamBlackLevelCorrection(new mfxExtCamBlackLevelCorrection())
    , m_extExtCamWhiteBalance(new mfxExtCamWhiteBalance())
    , m_extExtCamGammaCorrection(new mfxExtCamGammaCorrection())
    , m_extExtColorCorrection3x3(new mfxExtCamColorCorrection3x3())
    , m_externalsync()
    , m_pFactory(pFactory)
{

    m_bStat                 = true;
    m_nDecFrames            = 0;
    m_nVppFrames            = 0;
    m_nEncFrames            = 0;

    m_bInPSNotSpecified     = false;
    m_bPreventBuffering     = false;
    m_bWritePar             = false;
    m_generateRandomSeek    = false;
    m_fLastTime             = 0.0;
    m_fFirstTime            = -1.0;

    m_KernelTime            = 0;
    m_UserTime              = 0;


    m_components.push_back(ComponentParams(VM_STRING("Decoder"), VM_STRING("dec"), pFactory));
    m_components.push_back(ComponentParams(VM_STRING("Vpp"),     VM_STRING("vpp"), pFactory));
    m_components.push_back(ComponentParams(VM_STRING("Encoder"), VM_STRING("enc"), pFactory));
    m_components.push_back(ComponentParams(VM_STRING("Plugin"),  VM_STRING("plugin"), pFactory));

    m_pAllocFactory.reset(new FactoryChain<LCCheckFrameAllocator, RWAllocatorFactory::root>
        (new RWAllocatorFactory(m_pFactory->CreateAllocatorFactory(NULL))));

    for_each(m_components.begin()
        , m_components.end()
        , mem_var_set(&ComponentParams::m_pSession, m_components[eDEC].m_Session.get()));

    m_pRandom = m_pFactory->CreateRandomizer();
}

MFXDecPipeline::~MFXDecPipeline()
{
    ReleasePipeline();
    //cleaning up commands lists
    for (; !m_commands.empty(); m_commands.pop_front())
    {
        delete *m_commands.begin();
    }
    for (; !m_commandsProcessed.empty(); m_commandsProcessed.pop_front())
    {
        delete *m_commandsProcessed.begin();
    }
    for (size_t i = 0; i < m_pFactories.size(); i++)
    {
        delete m_pFactories[i];
    }
    m_pFactories.clear();
}

mfxStatus MFXDecPipeline::CheckParams()
{
    //in case of svc, input files are taken from dependency structure
    //TODO: restore check for non svc mode
    //MFX_CHECK_SET_ERR(0 != m_inParams.strSrcFile[0], PE_INV_INPUT_FILE, MFX_ERR_UNKNOWN);

    if (m_inParams.FrameInfo.Width != 0)
    {
        if (0 == m_inParams.FrameInfo.CropW)
        {
            m_inParams.FrameInfo.CropW = m_inParams.FrameInfo.Width;
            m_inParams.FrameInfo.CropH = m_inParams.FrameInfo.Height;
        }
    }

    if (m_inParams.nSeed == -1)
    {
        m_inParams.nSeed = vm_time_get_current_time();
    }

    m_pRandom->srand(m_inParams.nSeed);
    PrintInfo(VM_STRING("Seed"), VM_STRING("%d"), m_inParams.nSeed);

    if (m_commands.empty() && m_commandsProcessed.empty())
    {
        //process with factories array
        for (size_t i = 0; i < m_pFactories.size(); i++)
        {
            ICommandActivator * pCmd;
            for(int j = 0; NULL != (pCmd = m_pFactories[i]->CreateCommand(j)); j++)
                m_commands.push_back(pCmd);
        }
    }

    if (m_inParams.bOptimizeForPerf)
        m_inParams.nPriority = 1;
    //sets specified process priority level

    if (m_inParams.nPriority != 0)
    {
#if defined(_WIN32) || defined(_WIN64)
        if (m_inParams.nPriority > 0)
            SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);//realtime not required
        else
            SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
#endif // #if defined(_WIN32) || defined(_WIN64)
    }

    if (m_inParams.nBurstDecodeFrames != 0) {
        m_threadPool.reset(new MFXThread::ThreadPool());
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::BuildMFXPart()
{
    TIME_START();

    MFX_CHECK_STS_SET_ERR(CreateDeviceManager(), PE_CREATE_RND);
    TIME_PRINT(VM_STRING("CreateD3DDevice"));

    MFX_CHECK_STS_SET_ERR(CreateYUVSource(), PE_CREATE_DEC);
    TIME_PRINT(VM_STRING("CreateDec"));

    MFX_CHECK_STS_SET_ERR(DecodeHeader(), PE_DECODE_HEADER);
    TIME_PRINT(VM_STRING("DecodeHeader"));

    //some type of renders depends on decoder's output parameters
    MFX_CHECK_STS_SET_ERR(CreateRender(), PE_CREATE_RND);
    TIME_PRINT(VM_STRING("CreateRnd"));

    MFX_CHECK_STS_SET_ERR(CreateVPP(), PE_CREATE_VPP);
    TIME_PRINT(VM_STRING("CreateVPP"));

    MFX_CHECK_STS_SKIP(InitRenderParams(), MFX_ERR_UNSUPPORTED);

    m_components[eDEC].m_params.IOPattern = m_components[eDEC].GetIoPatternOut();
    m_components[eREN].m_params.IOPattern = m_components[eREN].GetIoPatternIn();
    m_components[eVPP].m_params.IOPattern = m_components[eDEC].GetIoPatternIn() | m_components[eREN].GetIoPatternOut();

    if (NULL != m_pRender)
    {
        mfxVideoParam params;
        memcpy(&params, &m_components[eREN].m_params, sizeof(params));
        MFX_CHECK_STS_CUSTOM_HANDLER(m_pRender->Query(&params, &m_components[eREN].m_params), {
            PipelineTrace((VM_STRING("%s"), MFXStructuresPair<mfxVideoParam>(params, m_components[eREN].m_params).Serialize().c_str()));
        });
    }

    //numthread could be modified by decode header
    m_components[eDEC].m_params.mfx.NumThread =  m_components[eDEC].m_NumThread;
    m_components[eREN].m_params.mfx.NumThread =  m_components[eREN].m_NumThread;
    m_components[eVPP].m_params.mfx.NumThread =  m_components[eVPP].m_NumThread;

    //decoder order could be modified by decode header
    if (m_components[eDEC].m_params.mfx.CodecId != MFX_CODEC_JPEG)
        m_components[eDEC].m_params.mfx.DecodedOrder = m_inParams.DecodedOrder;

    //no output frame rate specified
    if (0.0 == m_components[eVPP].m_fFrameRate)
    {
        m_components[eDEC].m_params.mfx.FrameInfo.FrameRateExtN =
            m_components[eREN].m_params.mfx.FrameInfo.FrameRateExtN;
        m_components[eDEC].m_params.mfx.FrameInfo.FrameRateExtD =
            m_components[eREN].m_params.mfx.FrameInfo.FrameRateExtD;
    }

    //vpp size params
    if (m_inParams.bUseVPP)
    {
        m_components[eVPP].m_params.vpp.In  = m_components[eDEC].m_params.mfx.FrameInfo;
        if ( m_components[eVPP].m_rotate == 270 || m_components[eVPP].m_rotate == 90 )
        {
            std::swap(m_components[eREN].m_params.mfx.FrameInfo.Width, m_components[eREN].m_params.mfx.FrameInfo.Height);
            std::swap(m_components[eREN].m_params.mfx.FrameInfo.CropW, m_components[eREN].m_params.mfx.FrameInfo.CropH);
        }

        m_components[eVPP].m_params.vpp.Out = m_components[eREN].m_params.mfx.FrameInfo;
        if ( m_components[eVPP].m_params.vpp.In.FourCC == MFX_FOURCC_R16 )
        {
            m_components[eVPP].m_params.vpp.In.BitDepthLuma  = m_inParams.nInputBitdepth;
            m_components[eVPP].m_params.vpp.In.ChromaFormat  = MFX_CHROMAFORMAT_MONOCHROME;
            m_components[eVPP].m_params.vpp.Out.FourCC       = m_components[eREN].m_params.mfx.FrameInfo.FourCC;
            m_components[eVPP].m_params.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        }
    }
    //overwriting vpp output picstruct if -vpp:picstruct  happened
    if (m_components[eVPP].m_bOverPS)
    {
        m_components[eVPP].m_params.vpp.Out.PicStruct = m_components[eVPP].m_nOverPS;
        m_components[eREN].m_params.mfx.FrameInfo.PicStruct = m_components[eVPP].m_nOverPS;
    }

    //overwriting vpp input picstruct if -dec:picstruct  happened
    if(m_components[eDEC].m_bOverPS)
    {
        mfxFrameInfo &refInfo = m_components[eVPP].m_params.vpp.In;
        bool bProg = m_components[eDEC].m_nOverPS == MFX_PICSTRUCT_PROGRESSIVE;

        refInfo.PicStruct   = m_components[eDEC].m_nOverPS;
        refInfo.Width       = mfx_align(refInfo.Width, 0x10);
        refInfo.Height      = mfx_align(refInfo.Height,(bProg) ? 0x10 : 0x20);
    }

    //cmd line params should be modified for par file
    mfxInfoVPP &rInfo = m_components[eVPP].m_params.vpp;
    m_inParams.nPicStruct = Convert_MFXPS_to_CmdPS( m_inParams.bUseVPP ? rInfo.Out.PicStruct:m_components[eDEC].m_params.mfx.FrameInfo.PicStruct
        , m_inParams.bUseVPP ? m_components[eVPP].m_extCO : m_components[eDEC].m_extCO );


    MFX_CHECK_STS(InitPluginParams());

    MFX_CHECK_STS_SET_ERR(CreateAllocator(), PE_CREATE_ALLOCATOR);
    TIME_PRINT(VM_STRING("CreateAlloc"));

    //async  0 in terms of pipeline structure is equal to -no_pipe_sync
    if (m_inParams.bNoPipelineSync)
    {
        ComponentParams *pLastParams = &m_components[eDEC];
        if (m_inParams.bUseVPP)
        {
            pLastParams = &m_components[eVPP];
        }
        if(m_RenderType & MFX_ENC_RENDER)
        {
            pLastParams = &m_components[eREN];
        }
        mfxU16 last_params_async = pLastParams->m_nMaxAsync;
        std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_nMaxAsync, 0));
        //SET_GLOBAL_PARAM(m_nMaxAsync, 0);
        pLastParams->m_nMaxAsync = last_params_async;
    }

    MFX_CHECK_STS_SET_ERR(InitVPP(), PE_CREATE_VPP);
    TIME_PRINT(VM_STRING("InitVPP"));

    MFX_CHECK_STS_SET_ERR(InitRender(), PE_CREATE_RND);
    TIME_PRINT(VM_STRING("InitRnd"));

    MFX_CHECK_STS_SET_ERR(InitYUVSource(), PE_CREATE_DEC);
    TIME_PRINT(VM_STRING("InitDec"));

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::ReleaseMFXPart()
{
    mfxStatus stsRenderClose = MFX_ERR_NONE;
    if (NULL != m_pRender)
    {
        stsRenderClose = m_pRender->Close();
    }
    //cleaning up a decoder
    m_components[eDEC].m_extParams.clear();
    m_pYUVSource.reset(0);


    //cleaning up VPP and its buffers
    m_components[eVPP].m_extParams.clear();
    MFX_DELETE(m_pVPP);

    //destroying a surfaces
    MFX_CHECK_STS(m_components[eDEC].DestroySurfaces());
    MFX_CHECK_STS(m_components[eVPP].DestroySurfaces());

    MFX_CHECK_STS_SKIP(stsRenderClose, MFX_ERR_NOT_INITIALIZED);
    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::BuildPipeline()
{
    TIME_START();

#ifdef MFX_TIMING
    if (m_bTimingETW)
    {
        MFX::AutoTimer::Init(NULL, m_bTimingETW);
    }
    MFX::AutoTimer timer("BuildPipeline");
#endif

    m_bStat = false;
    ReleasePipeline();
    m_bStat = true;

    //pipeline version printing
    int year, month, day, hh, mm, ss;
    const vm_char *platformStr;
    PipelineBuildTime(platformStr, year, month, day, hh, mm, ss);
    PrintInfo(VM_STRING("Build"), VM_STRING("%s - %d.%d.%d %-2d:%-2d:%-2d"), platformStr, day, month, year, hh,mm,ss);

#if ( defined(_WIN32) || defined(_WIN64) ) && !defined(WIN_TRESHOLD_MOBILE)
    //this is the point with min memory allocated by pipeline
    PROCESS_MEMORY_COUNTERS_EX pmcx = {};
    pmcx.cb = sizeof(pmcx);
    GetProcessMemoryInfo(GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmcx), pmcx.cb);

    PrintInfo(VM_STRING("Memory Private"), VM_STRING("%.2f M"),
        (Ipp64f)(pmcx.PrivateUsage) / (1024 * 1024));

    MEMORYSTATUSEX memEx;
    memEx.dwLength = sizeof (memEx);
    GlobalMemoryStatusEx(&memEx);

    PrintInfo(VM_STRING("Memory AvailPhys"), VM_STRING("%.2f M"),
        (Ipp64f)(memEx.ullAvailPhys)/ (1024 * 1024));
#endif // #if ( defined(_WIN32) || defined(_WIN64) ) && !defined(WIN_TRESHOLD_MOBILE)
#if defined(LINUX32) || defined(LINUX64)
    int mem_used = -1;
    char mem_buffer[128];
    FILE* file = fopen("/proc/self/status", "r");
    if ( file ) {
        while (fgets(mem_buffer, 128, file) != NULL){
            if (strncmp(mem_buffer, "VmRSS:", 6) == 0){
                int len = strlen(mem_buffer);
                int i = 0;
                while(i<128 && (mem_buffer[i] < '0' || mem_buffer[i] > '9')) i++;
                mem_buffer[len-2] = '\0';
                mem_used = atoi(&mem_buffer[i]);
                break;
            }
        }
        fclose (file);
    }
    PrintInfo(VM_STRING("Memory Private"), VM_STRING("%.2f M"),
        (Ipp64f)(mem_used) / (1024));
#endif

#if MFX_D3D11_SUPPORT
    PrintInfo(VM_STRING("D3D11 Support"), VM_STRING("ON"));
#else
    PrintInfo(VM_STRING("D3D11 Support"), VM_STRING("OFF"));
#endif

    mfxStatus res;

    MFX_CHECK_STS_SET_ERR(CheckParams(), PE_CHECK_PARAMS);

    AutoMessageNotifier bufferingNotifier(MSG_BUFFERING_START, MSG_BUFFERING_FINISH, MSG_BUFFERING_FAILED, m_inParams.nTestId);
    MFX_CHECK_STS_SET_ERR(CreateSplitter(), PE_CREATE_SPL);
    TIME_PRINT(VM_STRING("CreateSpl"));
    bufferingNotifier.Complete();

    sStreamInfo streamInfo;
    res = MFX_ERR_NONE;

    // GetStreamInfo
    if (m_inParams.InputCodecType != MFX_CODEC_CAPTURE)
    {
        res = m_pSpl->GetStreamInfo(&streamInfo);
    }

    m_components[eDEC].m_params.mfx.CodecId = m_inParams.InputCodecType;
    if ( MFX_FOURCC_P010 == m_inParams.FrameInfo.FourCC ||
         MFX_FOURCC_P210 == m_inParams.FrameInfo.FourCC ||
         MFX_FOURCC_NV16 == m_inParams.FrameInfo.FourCC ||
         MFX_FOURCC_R16  == m_inParams.FrameInfo.FourCC) {
        m_components[eDEC].m_params.mfx.FrameInfo.FourCC = m_inParams.FrameInfo.FourCC;
    }

    if ( MFX_FOURCC_P210 == m_inParams.FrameInfo.FourCC ||
         MFX_FOURCC_NV16 == m_inParams.FrameInfo.FourCC) {
         m_components[eDEC].m_params.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
    }

    if ( MFX_FOURCC_R16  == m_inParams.FrameInfo.FourCC ||
         MFX_FOURCC_P210 == m_inParams.FrameInfo.FourCC )
    {
        m_components[eDEC].m_params.mfx.FrameInfo.BitDepthChroma = m_components[eDEC].m_params.mfx.FrameInfo.BitDepthLuma = 10;
    }
    if (MFX_ERR_NONE == res && m_inParams.InputCodecType != MFX_CODEC_CAPTURE)
    {
        m_inParams.FrameInfo.Width  = streamInfo.nWidth;
        m_inParams.FrameInfo.Height = streamInfo.nHeight;
        m_components[eDEC].m_params.mfx.CodecId  = streamInfo.videoType;
        //m_inParams.InputCodecType  = streamInfo.videoType;
        m_inParams.isDefaultFC = streamInfo.isDefaultFC;
    }

    // Prepare Bitstream for Constructed Frame
    bool bExtended;
    MFX_CHECK_STS(InitInputBs(bExtended));
    PrintInfo(VM_STRING("CompleteFrame Mode"), m_inParams.bCompleteFrame ? VM_STRING("on") : VM_STRING("off"));
    TIME_PRINT(VM_STRING("AllocInputBS"));

    MFX_CHECK_STS_SET_ERR(CreateCore(), PE_INIT_CORE);
    TIME_PRINT(VM_STRING("CreateCore"));

    MFX_CHECK_STS(BuildMFXPart());

    MFX_CHECK_STS_SET_ERR(WriteParFile(), PE_PAR_FILE);

    PrintInfo(VM_STRING("Number threads"), VM_STRING("%d"), m_components[eDEC].m_params.mfx.NumThread);

    //retransfer processed commands
    std::list<ICommand*>::iterator it;
    for (;!m_commandsProcessed.empty();)
    {
        (*m_commandsProcessed.begin())->Reset();
        m_commands.push_back(*m_commandsProcessed.begin());
        m_commandsProcessed.pop_front();
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::ReleasePipeline()
{
    mfxF64 cpuUsage    = 0;
    mfxF64 decodefps   = 0;
    mfxF64 overallTime = 0.0;

    if (m_bStat)
    {
        mfxU64   KernelTime = 0;
        mfxU64   UserTime = 0;

#if defined(_WIN32) || defined(_WIN64)
        FILETIME notUsedVar;
        GetProcessTimes( GetCurrentProcess()
            , &notUsedVar
            , &notUsedVar
            , (LPFILETIME)&KernelTime
            , (LPFILETIME)&UserTime );
#else
        struct rusage usage;
        getrusage(RUSAGE_SELF,&usage);

        UserTime = 1000 * usage.ru_utime.tv_sec + (Ipp32u)((Ipp64f)usage.ru_utime.tv_usec / (Ipp64f)1000);
        KernelTime = 1000 * usage.ru_stime.tv_sec + (Ipp32u)((Ipp64f)usage.ru_stime.tv_usec / (Ipp64f)1000);
#endif

        m_statTimer.Stop();
        overallTime = m_statTimer.OverallTiming();

        mfxU32 frameCount  = m_nDecFrames;


        if (0.0 != overallTime)
        {
            PipelineTrace((VM_STRING("\n\n")));
            PrintInfo(VM_STRING("Total time"), VM_STRING("%.2f sec"), overallTime);
            ////////////////////////////////////////////////////////////////////////////
            //Decode section
            PrintInfo(VM_STRING("Number decoded frames"), VM_STRING("%d"), frameCount);

            decodefps = frameCount / overallTime;

            if ( m_inParams.bExtendedFpsStat )
            {
                int dist = m_inParams.fps_frame_window ? m_inParams.fps_frame_window : 30 ;
                Ipp64f max_time = 0;
                for(unsigned int i = 0; i < m_time_stampts.size(); i++)
                {
                    if ( i + 1 >= static_cast<unsigned int>(dist) )
                    {
                        if ( i + 1 == static_cast<unsigned int>(dist) )
                        {
                            max_time = m_time_stampts[i] > max_time ? m_time_stampts[i] : max_time;
                        }
                        else
                        {
                            max_time = (m_time_stampts[i] - m_time_stampts[i+1-dist]) > max_time ? (m_time_stampts[i] - m_time_stampts[i+1-dist]) : max_time;
                        }
                    }
                    PrintInfoForce(VM_STRING(" timestamp"), VM_STRING("%llf\n"), m_time_stampts[i]);
                }
                PrintInfoForce(VM_STRING("Min fps per window"), VM_STRING("%.3f fps per %d frames"), dist/max_time, dist);
            }
            PrintInfoForce(VM_STRING("Total fps"), VM_STRING("%.3f fps"), decodefps);
            PrintInfo(VM_STRING("Decode : async queue"), VM_STRING("max: %d, avr: %.2lf"), m_components[eDEC].m_uiMaxAsyncReached, m_components[eDEC].m_fAverageAsync);

            //////////////////////////////////////////////////////////////////////////
            //vpp stat section
            PrintInfo(VM_STRING("VPP    : number frames"), VM_STRING("%d"), m_nVppFrames);
            PrintInfo(VM_STRING("VPP    : async queue"), VM_STRING("max: %d, avr: %.2lf"), m_components[eVPP].m_uiMaxAsyncReached, m_components[eVPP].m_fAverageAsync);


#if defined(_WIN32) || defined(_WIN64)
            cpuUsage  = ( ( ( UserTime - m_UserTime ) + ( KernelTime - m_KernelTime) ) * 1e-5 );
#else
            cpuUsage  = ( ( UserTime - m_UserTime ) + ( KernelTime - m_KernelTime) ) / 10 ;
#endif
            cpuUsage /= overallTime;
            cpuUsage /= vm_sys_info_get_cpu_num();

            //fprintf(stderr, "Total time = %.2f sec\n", m_statTimer.OverallTiming());
        }

        //info about fail necessary in perf file in valid and in failed case

        PerfWriter statFileWriter;
        if (0 != vm_string_strlen(m_inParams.perfFile) && statFileWriter.Init(m_inParams.perfFile, m_inParams.strSrcFile))
        {
            statFileWriter.WritePerfResult(m_statTimer, frameCount);
        }
    }

    MFX_DELETE(m_pSpl);

    mfxStatus release_sts  = ReleaseMFXPart();

    if (m_components[eREN].m_pAllocator != m_components[eDEC].m_pAllocator)
    {
        MFX_DELETE(m_components[eREN].m_pAllocator);
    }

    MFX_DELETE(m_components[eDEC].m_pAllocator);

    m_components[eVPP].m_pAllocator = NULL;

    m_components[eREN].m_extParams.clear();
    MFX_DELETE(m_pRender);

    if (m_bStat && 0.0 != overallTime)
    {
        if (MFX_ENCDEC_RENDER == m_RenderType ||
            MFX_ENC_RENDER    == m_RenderType)
        {
            PrintInfo(VM_STRING("Encode : async queue"), VM_STRING("max: %d, avr: %.2lf"), m_components[eREN].m_uiMaxAsyncReached, m_components[eREN].m_fAverageAsync);
        }

        PrintInfo(VM_STRING("CPU usage"), VM_STRING("%.4f"), cpuUsage);

        //fprintf(stderr, "Decode fps: %.3f fps\n", decodefps);
    }

    m_bStat = false;

    if(NULL != m_components[eDEC].m_pSession)
        m_components[eDEC].m_pSession->Close();

    if (m_components[eREN].m_libType != m_components[eDEC].m_libType && NULL != m_components[eREN].m_pSession)
    {
        m_components[eREN].m_pSession->Close();
    }

    MFX_DELETE_ARRAY(m_inBSFrame.Data);
    //MFX_ZERO_MEM(m_inBSFrame);
    mfxBitstream2_ZERO_MEM(m_inBSFrame);
    m_bitstreamBuf.Close();

    m_fLastTime  =  0;
    m_fFirstTime = -1;
    m_nDecFrames =  0;
    m_nVppFrames =  0;
    m_nEncFrames =  0;

    m_statTimer.Reset();

    //if -n option is used there may be some buffering in decoder's queue
    m_components[eDEC].m_SyncPoints.clear();
    m_components[eVPP].m_SyncPoints.clear();

    //cleaning up render params that values were taken from last query
    //TODO: avoid such hacks - saving async_depth params
    mfxU16 ad = m_components[eREN].m_params.AsyncDepth;
    //TODO:saving chroma format since it stated in command line
    mfxFrameInfo info = m_components[eREN].m_params.mfx.FrameInfo;

    MFX_ZERO_MEM(m_components[eREN].m_params);

    m_components[eREN].m_params.AsyncDepth = ad;
    m_components[eREN].m_params.mfx.FrameInfo.ChromaFormat = info.ChromaFormat;
    m_components[eREN].m_params.mfx.FrameInfo.FourCC = info.FourCC;
    m_components[eREN].m_params.mfx.FrameInfo.Shift = info.Shift;

    MFX_CHECK_STS(release_sts);

    return MFX_ERR_NONE;
}

bool MFXDecPipeline::isErrIncompatParams()
{
    //if value cached
    if (m_bErrIncompatValid)
        return m_bErrIncompat;

    m_bErrIncompatValid = true;

    std::map <IncompatComponent, bool> :: iterator it;
    for (it = m_IPComponents.begin(); it != m_IPComponents.end(); it ++)
    {
        if (it->second)
        {
            return m_bErrIncompat = true;
        }
    }
    return m_bErrIncompat = false;
}

mfxStatus MFXDecPipeline::HandleIncompatParamsCode( mfxStatus error_code, IncompatComponent ID, bool isNull)
{
    //handling of incompatible videoparams only if input data is present,
    //changing resolution gives several buffered frames after incompat params happened
    if (isNull)
    {
        return error_code;
    }

    if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == error_code)
    {
        m_bErrIncompatValid = false;
        m_IPComponents[ID]  = true;
        return error_code;
    }

    //if we have incompatible params we probably can clean it
    if (isErrIncompatParams())
    {
        //if component previously returned err incompatible params
        if (m_IPComponents[ID])
        {
            m_IPComponents[ID]  = false;
            m_bErrIncompatValid = false;

            //all componets passed thru err incompatible params
            if (!isErrIncompatParams())
            {
                m_bResetAfterIncompatParams = false;
            }
        }
    }
    return error_code;
}

mfxStatus MFXDecPipeline::LightReset()
{
    //double reset will goes to long almost infinite run
    MFX_CHECK_STS(ReleaseMFXPart());
    MFX_CHECK_STS(BuildMFXPart());

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::HeavyReset()
{
    MFX_CHECK_STS(ReleaseMFXPart());
    MFX_DELETE(m_pRender);

    m_components[eDEC].m_SyncPoints.clear();
    m_components[eVPP].m_SyncPoints.clear();

    if(NULL != m_components[eDEC].m_pSession)
        m_components[eDEC].m_pSession->Close();

    MFX_CHECK_STS_SET_ERR(CreateCore(), PE_INIT_CORE);
    TIME_PRINT(VM_STRING("CreateCore"));

    m_nDecFrames = 0;
    m_inParams.nFrames = m_inParams.nFramesAfterRecovery;

    mfxFrameInfo info;
    m_pSpl->SeekFrameOffset(0, info);

    MFX_CHECK_STS(BuildMFXPart());

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::InitInputBs(bool &bExtended)
{
    mfxU64 bs_size = m_components[eDEC].m_params.mfx.FrameInfo.Width  *
        m_components[eDEC].m_params.mfx.FrameInfo.Height * 4;
    if (0 == bs_size)
    {
        //taking params from cmdline
        bs_size = m_inParams.FrameInfo.Width * m_inParams.FrameInfo.Height * 4;
    }

    if (0 == bs_size)
    {
        //if we still dont know a resolution lets start from 320x240
        bs_size = 320 * 240 * 3;
    }

    //limit bitstream size with file_size, in some critical resolutions, it is only possible way to not run out of memory
    if (m_inParams.nLimitInputBs)
    {
        bs_size = (std::min)(m_inParams.nLimitInputBs, bs_size);
    }

    bExtended = false;
    if (bs_size > m_inBSFrame.MaxLength)
    {
        bExtended = true;
    }

    //extending bitstream if necessary
    MFX_CHECK_STS(MFXBistreamBuffer::ExtendBs((mfxU32)bs_size, &m_inBSFrame));
    MFX_CHECK_STS(m_bitstreamBuf.Init( m_inParams.nDecBufSize
        , m_inBSFrame.MaxLength));

    PrintInfo( VM_STRING("Input bitstream buffer"), VM_STRING("%d")
        , 0 != m_inParams.nDecBufSize ? m_inParams.nDecBufSize : m_inBSFrame.MaxLength);

    //complete frame is failed if decbufsize used
    if (m_inParams.nDecBufSize !=0 )
        m_inParams.bCompleteFrame = false;

    return MFX_ERR_NONE;
}

class DispatcherLoadedLibrary : public IMsgHandler
{
    tstring m_libPath;
public:
    DispatcherLoadedLibrary()
    {
        DispatchLog::get().AttachSink(/*DL_SINK_PRINTF |*/ DL_SINK_IMsgHandler, this);
    }
    ~DispatcherLoadedLibrary()
    {
        DispatchLog::get().DetachSink(/*DL_SINK_PRINTF |*/ DL_SINK_IMsgHandler, this);
    }

    virtual void Write(int level, int /*opcode*/, const char * msg, va_list argptr)
    {
#if defined(_WIN32) || defined(_WIN64)
        HMODULE libModule;
        if (level == DL_LOADED_LIBRARY)
        {
            char str[32];
            vsprintf_s(str, MFX_ARRAY_SIZE(str), msg, argptr);
            if (1 == sscanf(str, "%p", &libModule))
            {
                vm_char libPath [2048];
                GetModuleFileName(libModule,libPath, MFX_ARRAY_SIZE(libPath));
                m_libPath = libPath;
            }
        }
#endif
    }
    tstring GetPath(){return m_libPath;}
};

mfxStatus MFXDecPipeline::CreateCore()
{
    //shared access prevention section
    {
        AutoPipelineSynhro d3dAcess(m_externalsync);
        DispatcherLoadedLibrary lib; // not thread-safe
        if (m_inParams.bSkipUselessOutput)
            DispatchLog::get().DetachAllSinks();

#if defined(_WIN32) || defined(_WIN64)
        MFX::DXVA2Device dxva2device;
        for (int i = 0; dxva2device.InitDXGI1(i); i++)
        {
            vm_char str[256];
            vm_string_sprintf_s(str, VM_STRING("Graphic VendorID %d"), i);
            PrintInfo(str, VM_STRING("0x%x"), dxva2device.GetVendorID());

            vm_string_sprintf_s(str, VM_STRING("Graphic DeviceID %d"), i);
            PrintInfo(str, VM_STRING("0x%x"), dxva2device.GetDeviceID());

            mfxU64 nDriverVersion = dxva2device.GetDriverVersion();
            vm_string_sprintf_s(str, VM_STRING("Graphic Driver Version %d"), i);
            PrintInfo(str, VM_STRING("%d.%d.%d.%d")
                , (int)((nDriverVersion>>48) & 0xFFFF)
                , (int)((nDriverVersion>>32) & 0xFFFF)
                , (int)((nDriverVersion>>16) & 0xFFFF)
                , (int)(nDriverVersion & 0xFFFF ));
        }
#endif
        if ( m_inParams.bInitEx )
        {
            mfxInitParam params;
            MFX_ZERO_MEM(params);
            params.ExternalThreads = 0;
            params.GPUCopy         = m_inParams.nGpuCopyMode;
            params.Implementation  = m_components[eDEC].m_libType;
            params.ExtParam        = 0;
            params.NumExtParam     = 0;
            params.Version.Major   = MFX_VERSION_MAJOR;
            params.Version.Minor   = MFX_VERSION_MINOR;
            if ( m_components[eDEC].m_pLibVersion )
                params.Version     = *(m_components[eDEC].m_pLibVersion);
            MFX_CHECK_STS(m_components[eDEC].m_pSession->InitEx(params, m_inParams.pMFXLibraryPath));
        }
        else
        {
            MFX_CHECK_STS(m_components[eDEC].m_pSession->Init(m_components[eDEC].m_libType, m_components[eDEC].m_pLibVersion, m_inParams.pMFXLibraryPath));
        }

        m_components[eDEC].m_mfxLibPath = lib.GetPath();
    }

    MFX_CHECK_STS(m_components[eDEC].m_pSession->QueryIMPL(&m_components[eDEC].m_RealImpl));

    //preparation for loading second session
    ComponentParams *pSecondParams = NULL;

    if (m_components[eREN].m_libType != m_components[eDEC].m_libType)
    {
        pSecondParams = &m_components[eREN];
    }
    else if (m_components[eVPP].m_libType != m_components[eDEC].m_libType)
    {
        pSecondParams = &m_components[eVPP];
    }

    if (NULL != pSecondParams)
    {
        //new session pointer should be created
        pSecondParams->m_pSession = pSecondParams->m_Session.get();
    }

    if (m_components[eVPP].m_libType != m_components[eDEC].m_libType)
    {
        m_components[eVPP].m_pSession = pSecondParams->m_pSession;
    }

    if (m_components[eREN].m_libType != m_components[eDEC].m_libType)
    {
        m_components[eREN].m_pSession = pSecondParams->m_pSession;
    }

    if (m_components[eVPP].m_libType == m_components[eDEC].m_libType)
    {
        m_components[eVPP].m_mfxLibPath = m_components[eDEC].m_mfxLibPath;
        m_components[eVPP].m_RealImpl   = m_components[eDEC].m_RealImpl;
    }

    if (m_components[eREN].m_libType == m_components[eDEC].m_libType)
    {
        m_components[eREN].m_mfxLibPath = m_components[eDEC].m_mfxLibPath;
        m_components[eREN].m_RealImpl   = m_components[eDEC].m_RealImpl;
    }

    if (NULL != pSecondParams)
    {
        //creating second session
        MFX_CHECK_STS(pSecondParams->m_pSession->Init(pSecondParams->m_libType, pSecondParams->m_pLibVersion, m_inParams.pMFXLibraryPath));
        MFX_CHECK_STS(pSecondParams->m_pSession->QueryIMPL(&pSecondParams->m_RealImpl));

        pSecondParams->m_mfxLibPath = m_components[eDEC].m_mfxLibPath;

        //join second session if necessary
#if (MFX_VERSION_MAJOR >= 1) && (MFX_VERSION_MINOR >= 1)
        if (m_inParams.bJoinSessions)
        {
            MFX_CHECK_STS(m_components[eDEC].m_pSession->JoinSession(pSecondParams->m_pSession->GetMFXSession()));
        }
#endif
    }

    for_each(m_components.begin(), m_components.end(), mem_fun_ref(&ComponentParams::CorrectParams));

    m_components[eDEC].PrintInfo();

    if (m_RenderType == MFX_ENC_RENDER || m_RenderType == MFX_ENCDEC_RENDER)
    {
        m_components[eREN].PrintInfo();
    }

    return MFX_ERR_NONE;
}

//shortening macro
#define ENABLE_VPP(reason)\
    if (reason)\
{\
    PrintInfo(VM_STRING("Vpp.ENABLED"), _TSTR(reason));\
    m_inParams.bUseVPP = true;\
}

mfxStatus MFXDecPipeline::CreateVPP()
{
    mfxFrameInfo &dec_info = m_components[eDEC].m_params.mfx.FrameInfo;
    mfxFrameInfo &enc_info = m_components[eREN].m_params.mfx.FrameInfo;
    // check VPP enabling
    ENABLE_VPP(m_components[eVPP].m_zoomx);
    ENABLE_VPP(m_components[eVPP].m_zoomy);
    ENABLE_VPP(m_components[eVPP].m_params.vpp.Out.CropW);
    ENABLE_VPP(m_components[eVPP].m_params.vpp.Out.CropH);
    ENABLE_VPP(m_components[eVPP].m_rotate)
    ENABLE_VPP(m_components[eVPP].m_mirroring)
    ENABLE_VPP(dec_info.FourCC != enc_info.FourCC);
    ENABLE_VPP(m_components[eDEC].m_bufType != m_components[eREN].m_bufType)
    ENABLE_VPP(m_components[eDEC].m_params.mfx.FrameInfo.Shift != m_components[eREN].m_params.mfx.FrameInfo.Shift);
    ENABLE_VPP(m_inParams.nDenoiseFactorPlus1);
    ENABLE_VPP(m_inParams.nDetailFactorPlus1);
    ENABLE_VPP(m_inParams.bUseProcAmp);
    ENABLE_VPP(m_inParams.bUseCameraPipe);
    ENABLE_VPP(m_inParams.bSceneAnalyzer);
    ENABLE_VPP(m_inParams.bPAFFDetect);
    ENABLE_VPP(0.0 != m_components[eVPP].m_fFrameRate);
    //ENABLE_VPP(m_components[eDEC].m_params.mfx.FrameInfo.FrameRateExtN != m_components[eREN].m_params.mfx.FrameInfo.FrameRateExtN);
    ENABLE_VPP(m_inParams.bUseVPP_ifdi);
    ENABLE_VPP(m_inParams.nImageStab);
    ENABLE_VPP(m_inParams.bExtVppApi);


    if (m_inParams.bUseVPP)
    {
        m_components[eVPP].m_bufType = m_components[eREN].m_bufType;
        m_components[eVPP].m_bExternalAlloc = m_components[eREN].m_bExternalAlloc;
    }

    ComponentParams *pLastParams = &m_components[eDEC];

    if (m_inParams.bUseVPP)
    {
        pLastParams = &m_components[eVPP];
    }

    if(m_RenderType & MFX_ENC_RENDER || m_inParams.nBurstDecodeFrames)
    {
        pLastParams = &m_components[eREN];
    }

    //if -async 0, but 0 is invalid for last pipeline component
    pLastParams->m_nMaxAsync = IPP_MAX(1, pLastParams->m_nMaxAsync);

    if (!m_inParams.bUseVPP)
        return MFX_ERR_NONE;

    //special case when no PS specified at output VPP, and -dec:picstruct specified defaulting it to dec:picstrcut
    //vpp cannot init if output is unknown,
    if (m_components[eDEC].m_bOverPS && !m_components[eVPP].m_bOverPS && m_bInPSNotSpecified)
    {
        m_components[eVPP].m_bOverPS = true;
        m_components[eVPP].m_nOverPS = m_components[eDEC].m_nOverPS;
        m_components[eVPP].m_extCO   = m_components[eDEC].m_extCO;
    }

    m_components[eVPP].PrintInfo();

    if (0 != vm_string_strlen(m_inParams.pPluginDLL))
    {
        //no support any more for plugin as vpp
        return MFX_ERR_UNSUPPORTED;
    }
    else if (  vm_string_strlen(m_inParams.strVPPPluginGuid)  )
    {
        m_pVPP = m_pFactory->CreateVPP(
            PipelineObjectDesc<IMFXVideoVPP>(m_components[eVPP].m_pSession->GetMFXSession(), m_inParams.strVPPPluginGuid, 1, VPP_MFX_PLUGIN_GUID, NULL));
    }
    else
    {
        m_pVPP = m_pFactory->CreateVPP(
            PipelineObjectDesc<IMFXVideoVPP>(m_components[eVPP].m_pSession->GetMFXSession(), VM_STRING(""), VPP_MFX_NATIVE, NULL));
    }
    MFX_CHECK_WITH_ERR(m_pVPP, MFX_ERR_MEMORY_ALLOC);
    if(m_components[eVPP].m_rotate)
    {
        m_components[eVPP].m_extParams.push_back(new mfxExtVPPRotation());
        MFXExtBufferPtr<mfxExtVPPRotation> pRotate(m_components[eVPP].m_extParams);
        pRotate->Angle = m_components[eVPP].m_rotate;
    }

    if(m_components[eVPP].m_mirroring)
    {
        m_components[eVPP].m_extParams.push_back(new mfxExtVPPMirroring());
        MFXExtBufferPtr<mfxExtVPPMirroring> pMirroring(m_components[eVPP].m_extParams);
        pMirroring->Type = m_components[eVPP].m_mirroring;
    }

    if(m_inParams.bVppScaling)
    {
        m_components[eVPP].m_extParams.push_back(new mfxExtVPPScaling());
        MFXExtBufferPtr<mfxExtVPPScaling> pScaling(m_components[eVPP].m_extParams);
        pScaling->ScalingMode = m_inParams.uVppScalingMode;
    }

    //turnoff default filter if they are not present in cmd line
    if ( ! m_inParams.bUseCameraPipe && ! m_inParams.bExtVppApi )
    {
        // Having DONOTUSE stuff in case of camera/PTIR vpp plug-ins causes fail at VPP Init
        m_components[eVPP].m_extParams.push_back(new mfxExtVPPDoNotUse());
        MFXExtBufferPtr<mfxExtVPPDoNotUse> pExtDoNotUse(m_components[eVPP].m_extParams);

        // turn off denoising (on by default)
        if (!m_inParams.nDenoiseFactorPlus1)
            pExtDoNotUse->AlgList.push_back(MFX_EXTBUFF_VPP_DENOISE);
        else
        {
            m_components[eVPP].m_extParams.push_back(new mfxExtVPPDenoise());
            MFXExtBufferPtr<mfxExtVPPDenoise> pDenoise(m_components[eVPP].m_extParams);
            pDenoise->DenoiseFactor = m_inParams.nDenoiseFactorPlus1 - 1;
        }

        if (!m_inParams.bSceneAnalyzer)
            pExtDoNotUse->AlgList.push_back(MFX_EXTBUFF_VPP_SCENE_ANALYSIS);
   }

    // turn on detail/edge enhancement
    if (m_inParams.nDetailFactorPlus1)
    {
        m_components[eVPP].m_extParams.push_back(new mfxExtVPPDetail());
        MFXExtBufferPtr<mfxExtVPPDetail> pDetail(m_components[eVPP].m_extParams);
        pDetail->DetailFactor = m_inParams.nDetailFactorPlus1 - 1;
    }

    // turn on ProcAmp filter
    if (m_inParams.bUseProcAmp)
    {
        m_components[eVPP].m_extParams.push_back(new mfxExtVPPProcAmp());
        MFXExtBufferPtr<mfxExtVPPProcAmp> pProcAmp(m_components[eVPP].m_extParams);
        pProcAmp->Brightness = m_inParams.m_ProcAmp.Brightness;
        pProcAmp->Contrast   = m_inParams.m_ProcAmp.Contrast;
        pProcAmp->Hue        = m_inParams.m_ProcAmp.Hue;
        pProcAmp->Saturation = m_inParams.m_ProcAmp.Saturation;
    }

    // turn off scene analysis (on by default)
    if (m_inParams.bUseCameraPipe)
    {
        m_components[eVPP].m_extParams.push_back(new mfxExtCamPipeControl());
        MFXExtBufferPtr<mfxExtCamPipeControl> pCameraPipe(m_components[eVPP].m_extParams);
        pCameraPipe->RawFormat = (mfxU16)m_inParams.m_container;
    }
    if (m_inParams.bUseCameraPipePadding)
    {
        m_components[eVPP].m_extParams.push_back(new mfxExtCamPadding());
        MFXExtBufferPtr<mfxExtCamPadding> pCameraPipePadding(m_components[eVPP].m_extParams);
        pCameraPipePadding->Top = pCameraPipePadding->Bottom = pCameraPipePadding->Left = pCameraPipePadding->Right = 8;;
    }

    if ( ! m_extExtCamBlackLevelCorrection.IsZero() )
    {
        m_components[eVPP].m_extParams.push_back(m_extExtCamBlackLevelCorrection);
    }

    if ( ! m_extExtCamGammaCorrection.IsZero() )
    {
        m_components[eVPP].m_extParams.push_back(m_extExtCamGammaCorrection);
    }

    if ( ! m_extExtCamWhiteBalance.IsZero() )
    {
        m_components[eVPP].m_extParams.push_back(m_extExtCamWhiteBalance);
    }

    if ( ! m_extExtColorCorrection3x3.IsZero() )
    {
        m_components[eVPP].m_extParams.push_back(m_extExtColorCorrection3x3);
    }

    if (m_components[eVPP].m_params.vpp.Out.CropW != 0)
    {
        m_inParams.FrameInfo.Width = m_components[eVPP].m_params.vpp.Out.CropW + m_inParams.FrameInfo.CropX;
        m_inParams.FrameInfo.CropW = m_components[eVPP].m_params.vpp.Out.CropW;
    }

    if (m_components[eVPP].m_params.vpp.Out.CropH != 0)
    {
        m_inParams.FrameInfo.Height = m_components[eVPP].m_params.vpp.Out.CropH + m_inParams.FrameInfo.CropY;
        m_inParams.FrameInfo.CropH = m_components[eVPP].m_params.vpp.Out.CropH;
    }

    if (m_components[eVPP].m_zoomx != 0)
    {
        m_inParams.FrameInfo.Width = (mfxU16)(m_components[eDEC].m_params.mfx.FrameInfo.Width * m_components[eVPP].m_zoomx) + m_inParams.FrameInfo.CropX;
        m_inParams.FrameInfo.CropW = mfx_align((mfxU16)(m_components[eDEC].m_params.mfx.FrameInfo.CropW * m_components[eVPP].m_zoomx), 2);
    }

    if (m_components[eVPP].m_zoomy != 0)
    {

        m_inParams.FrameInfo.Height = (mfxU16)(m_components[eDEC].m_params.mfx.FrameInfo.Height * m_components[eVPP].m_zoomy) + m_inParams.FrameInfo.CropY;
        m_inParams.FrameInfo.CropH  = mfx_align((mfxU16)(m_components[eDEC].m_params.mfx.FrameInfo.CropH  * m_components[eVPP].m_zoomy),2);
    }

    if (0 != m_inParams.nAdvanceFRCAlgorithm)
    {
        /*Advance frc buffer if necessary*/
        m_components[eVPP].m_extParams.push_back(new mfxExtVPPFrameRateConversion());

        MFXExtBufferPtr<mfxExtVPPFrameRateConversion> pADVFrc(m_components[eVPP].m_extParams);
        pADVFrc->Algorithm = m_inParams.nAdvanceFRCAlgorithm;
    }

    if (0 != m_inParams.nImageStab)
    {
        m_components[eVPP].m_extParams.push_back(new mfxExtVPPImageStab());
        MFXExtBufferPtr<mfxExtVPPImageStab> imgStab(m_components[eVPP].m_extParams);
        imgStab->Mode = m_inParams.nImageStab;
    }

    //weird requirement to only use douse buffer if alglist not empty certain cases - wont add this in unittest
    if (m_inParams.bPAFFDetect)
    {
        m_components[eVPP].m_extParams.push_back(new mfxExtVPPDoUse());
        MFXExtBufferPtr<mfxExtVPPDoUse> vppDoUse(m_components[eVPP].m_extParams);

        vppDoUse->AlgList.push_back(MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION);
    }

    if (0 != m_inParams.nSVCDownSampling)
    {
        m_components[eVPP].m_extParams.push_back(new mfxExtSVCDownsampling());

        MFXExtBufferPtr<mfxExtSVCDownsampling> svcDonwsampling(m_components[eVPP].m_extParams);
        svcDonwsampling->Algorithm = m_inParams.nSVCDownSampling;
    }


    //    m_components[eVPP].m_extParams.merge( m_components[eDEC].m_extParams.begin()
    //                                        , m_components[eDEC].m_extParams.end());
    // asomsiko: this is workaround for bug in HW library - it is not accepting other protected values.
    MFXExtBufferVector::iterator it = m_components[eDEC].m_extParams.begin();
    for(;it != m_components[eDEC].m_extParams.end(); it++ )
        if (MFX_EXTBUFF_PAVP_OPTION != (**it).BufferId)
            m_components[eVPP].m_extParams.push_back(**it);

    if (0 != m_components[eVPP].m_params.Protected)
        m_components[eVPP].m_params.Protected = MFX_PROTECTION_PAVP;

    m_components[eVPP].AssignExtBuffers();


    //.m_params.NumExtParam = (mfxU16)m_components[eVPP].m_extParams.size();
    //m_components[eVPP].m_params.ExtParam    = &m_components[eVPP].m_extParams;//vector is stored linear in memory

    //   MFX_CHECK_STS(m_pVPP->Init(&m_components[eVPP].m_params));

    //pAlgList.release();
    //pExtDoNotUse.release();

    if (m_components[eVPP].m_bCalcLatency)
    {
        MFX_CHECK_WITH_ERR(m_pVPP = new LatencyVPP(m_components[eREN].m_bCalcLatency && m_inParams.bNoPipelineSync, NULL, &PerfCounterTime::Instance(), m_pVPP), MFX_ERR_MEMORY_ALLOC);
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::InitPluginVppParams(mfxFrameInfo & pluginInfo,
                                              mfxFrameInfo & VppInfo)
{
    bool bEnable = false;

    //checking enabling output
    if (pluginInfo.CropH != 0 ||
        pluginInfo.CropW != 0)
    {
        bEnable = true;
    }


    if (bEnable)
    {
        //default members initialization in transfered from vpp info
        if (0 == pluginInfo.FourCC)
            pluginInfo.FourCC = VppInfo.FourCC;

        if (0 == pluginInfo.PicStruct)
        {
            pluginInfo.PicStruct = VppInfo.PicStruct;
            //we should directly state the picstruct for plugin
            if (0 == pluginInfo.PicStruct)
            {
                pluginInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            }
        }

        if (0 == pluginInfo.CropH)
            pluginInfo.CropH = VppInfo.CropH;

        if (0 == pluginInfo.CropW)
            pluginInfo.CropW = VppInfo.CropW;

        if (0 == pluginInfo.FrameRateExtD)
            pluginInfo.FrameRateExtD = VppInfo.FrameRateExtD;

        if (0 == pluginInfo.FrameRateExtN)
            pluginInfo.FrameRateExtN = VppInfo.FrameRateExtN;


        //members correction
        bool bProgressive
            = (pluginInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE);

        pluginInfo.Width  =  mfx_align(pluginInfo.CropW, 0x10);
        pluginInfo.Height =  mfx_align(pluginInfo.CropH, bProgressive ? 0x10 : 0x20);
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::InitPluginParams()
{
    MFX_CHECK_STS(InitPluginVppParams(m_components[ePLUGIN].m_params.vpp.In
        , m_components[eVPP].m_params.vpp.In));

    MFX_CHECK_STS(InitPluginVppParams(m_components[ePLUGIN].m_params.vpp.Out
        , m_components[eVPP].m_params.vpp.Out));

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::InitVPP()
{
    mfxStatus sts;

    if (NULL == m_pVPP) return MFX_ERR_NONE;

    MFX_CHECK_STS(sts = m_pVPP->Init(&m_components[eVPP].m_params));

    if (m_components[eVPP].m_bHWStrict && MFX_WRN_PARTIAL_ACCELERATION == sts)
    {
        sts = MFX_ERR_UNSUPPORTED;
    }

    mfxFrameInfo & fi = m_components[eVPP].m_params.vpp.Out;
    PrintInfo(VM_STRING("Resized resolution"), VM_STRING("%dx%d"), fi.CropW, fi.CropH);

    return sts;
}

mfxStatus MFXDecPipeline::DecodeHeader()
{
    mfxStatus sts;
    //decode header might overrite some values, fourcc for example
    mfxFrameInfo dec_info = m_components[eDEC].m_params.mfx.FrameInfo;

    if (m_components[eDEC].m_params.mfx.CodecId == MFX_CODEC_CAPTURE)
    {
        m_components[eDEC].m_params.mfx.FrameInfo.Width = mfx_align((mfxU16)(m_inParams.FrameInfo.Width), 0x10);
        m_components[eDEC].m_params.mfx.FrameInfo.Height = mfx_align((mfxU16)(m_inParams.FrameInfo.Height), 0x10);
        m_components[eDEC].m_params.mfx.FrameInfo.CropW = m_inParams.FrameInfo.Width;
        m_components[eDEC].m_params.mfx.FrameInfo.CropH = m_inParams.FrameInfo.Height;
        FrameRate2Code(m_components[eDEC].m_fFrameRate, &m_components[eDEC].m_params.mfx.FrameInfo);
        return MFX_ERR_NONE;
    }

    for (;;)
    {
        //buffer may contain data

        MFX_CHECK_STS_SKIP(sts = m_pYUVSource->DecodeHeader(&m_inBSFrame, &m_components[eDEC].m_params)
            , MFX_ERR_MORE_DATA);

        if (MFX_ERR_MORE_DATA == sts)
        {
            if (m_inBSFrame.MaxLength == m_inBSFrame.DataLength)
            {
                mfxU32 new_length = m_inBSFrame.MaxLength + m_inBSFrame.MaxLength / 2; //grow by 50%
                MFX_CHECK_STS(MFXBistreamBuffer::ExtendBs(new_length, &m_inBSFrame));
            }
        }
        else if (MFX_ERR_NONE <= sts)
        {
            //TODO: create dedicated decoder class
            //in case of SVC codec profile specified we will continue decode header to get info
            switch(m_components[eDEC].m_params.mfx.CodecProfile)
            {
            case MFX_PROFILE_AVC_SCALABLE_BASELINE:
            case MFX_PROFILE_AVC_SCALABLE_HIGH :
                {
                    MFXExtBufferPtr<mfxExtSVCSeqDesc> seqDescription(m_components[eDEC].m_extParams);

                    //bufer was attached and decode header completed successfully
                    if (seqDescription.get())
                    {
                        //PrintInfo(VM_STRING("Number of Views"), VM_STRING("%d"), seqDescription->NumView);
                        break;
                    }

                    m_components[eDEC].m_extParams.push_back(new mfxExtSVCSeqDesc());
                    m_components[eDEC].AssignExtBuffers();
                    continue;
                }
            }

            if (m_components[eDEC].m_params.mfx.CodecId == MFX_CODEC_VP9)
            {
                if (m_inParams.bVP9_DRC)
                {
                    m_components[eDEC].m_params.mfx.SliceGroupsPresent = 1;
                    m_components[eDEC].m_VP9_Smooth_DRC = true;
                }
            }

            if (m_inParams.isPreferNV12 &&
                m_components[eDEC].m_params.mfx.CodecId == MFX_CODEC_HEVC && m_components[eDEC].m_params.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10 &&
                m_components[eDEC].m_params.mfx.FrameInfo.BitDepthLuma == 8 && m_components[eDEC].m_params.mfx.FrameInfo.BitDepthChroma == 8)
            {
                m_components[eDEC].m_params.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
                m_components[eDEC].m_params.mfx.FrameInfo.Shift = 0;
            }

            break;
        }

        MFX_CHECK_STS(m_pSpl->ReadNextFrame(m_inBSFrame));
    }

    //handling of default values
    mfxFrameInfo &info = m_components[eDEC].m_params.mfx.FrameInfo;
    const vm_char * pFrameRateString;

    if (m_components[eDEC].m_bFrameRateUnknown)
    {
        info.FrameRateExtD = 0.0;
        info.FrameRateExtN = 0.0;
        pFrameRateString = VM_STRING("Unknown (provided by cmd), set to 0.0");
    }
    else if (0.0 == info.FrameRateExtD &&
        0.0 == info.FrameRateExtN)
    {
        //framerate is unknown set it to 30
        info.FrameRateExtN = 30;
        info.FrameRateExtD = 1;
        pFrameRateString   = VM_STRING("Unknown, set to %.2lf");
    }else
    {
        if (0.0 != m_components[eDEC].m_fFrameRate)//overriding frame rate values using command line
        {
            //framerate used in vpp/encoder input
            FrameRate2Code(m_components[eDEC].m_fFrameRate, &info);
        }

        pFrameRateString = VM_STRING("%.2lf");
    }

    PrintInfo(VM_STRING("Framerate"), pFrameRateString, (mfxF64)info.FrameRateExtN / (mfxF64)info.FrameRateExtD);

#ifdef LUCAS_DLL
    lucas::CLucasCtx &lucasCtx = lucas::CLucasCtx::Instance();
    if (lucasCtx->fmWrapperPtr)
    {
        lucasCtx->parameter(lucasCtx->fmWrapperPtr, lucas::FM_WRAPPER_PARAM_RESOLUTION_X, m_components[eDEC].m_params.mfx.FrameInfo.CropW);
        lucasCtx->parameter(lucasCtx->fmWrapperPtr, lucas::FM_WRAPPER_PARAM_RESOLUTION_Y, m_components[eDEC].m_params.mfx.FrameInfo.CropH);
#if defined(_WIN32) || defined(_WIN64)
        mfxF64 dFrameRate = (mfxF64)info.FrameRateExtN / (mfxF64)info.FrameRateExtD;
        // dvrogozh: this cast is incompilable. I can't fix.
        lucasCtx->parameter(lucasCtx->fmWrapperPtr, lucas::FM_WRAPPER_PARAM_FRAME_RATE, reinterpret_cast<int>(&dFrameRate));
#endif
    }
#endif // LUCAS_DLL

    //extend input bs if necessary
    bool bExtended;
    MFX_CHECK_STS(InitInputBs(bExtended));

    //in complete frame mode, especially for raw streams guessed resolution may be smaller than one required for completeframe mode
    if (bExtended && m_inParams.bCompleteFrame)
    {
        //unknown if fine, since if means either eof or not enough buffer
        MFX_CHECK_STS_SKIP(m_pSpl->ReadNextFrame(m_inBSFrame), MFX_ERR_UNKNOWN);
    }

    //width and height could be changed during stream parsing
    //m_inParams.FrameInfo = info;

    //NOTE: repeat mode fix bug 8405
    //NOTE: also affects some opaq suites bug 9636
    //NOTE: also affects cropped streams bug 9625
    m_inParams.FrameInfo.Width  = info.CropW;
    m_inParams.FrameInfo.Height = info.CropH;
    m_inParams.FrameInfo.CropW  = info.CropW;
    m_inParams.FrameInfo.CropH  = info.CropH;
    //acpect ratio related bug 9897
    m_inParams.FrameInfo.AspectRatioW  = info.AspectRatioW;
    m_inParams.FrameInfo.AspectRatioH  = info.AspectRatioH;

    if (m_inParams.nPicStruct == PIPELINE_PICSTRUCT_NOT_SPECIFIED )//not initialized from cmd line
    {
        //taking params from header
        m_components[eREN].m_params.mfx.FrameInfo.PicStruct = info.PicStruct;

        //storing in command line parameters
        m_inParams.nPicStruct = Convert_MFXPS_to_CmdPS(info.PicStruct);

        //remembering this allow not to lost input commandline specifics necessary in future
        m_bInPSNotSpecified = true;
    }

    m_inParams.FrameInfo.PicStruct = Convert_CmdPS_to_MFXPS(m_inParams.nPicStruct);

    //overwriting vpp output picstruct if -vpp:picstruct  present
    if (m_components[eVPP].m_bOverPS)
    {
        m_inParams.FrameInfo.PicStruct = m_components[eVPP].m_nOverPS;
    }

    if ((0.0 == m_components[eDEC].m_fFrameRate) && !m_components[eDEC].m_bFrameRateUnknown)
    {
        m_components[eDEC].m_fFrameRate = (mfxF64)info.FrameRateExtN / (mfxF64)info.FrameRateExtD;
    }

    ////special case when decoder initialized with rgb24/rgb32 fourcc
    if (m_components[eDEC].m_params.mfx.CodecId == MFX_CODEC_JPEG &&
         m_components[eDEC].m_params.mfx.FrameInfo.FourCC != MFX_FOURCC_P010 &&
         m_components[eDEC].m_params.mfx.FrameInfo.FourCC != MFX_FOURCC_NV16 &&
         m_components[eDEC].m_params.mfx.FrameInfo.FourCC != MFX_FOURCC_P210)
    {
        m_components[eDEC].m_params.mfx.FrameInfo.FourCC       = dec_info.FourCC;
        m_components[eDEC].m_params.mfx.FrameInfo.ChromaFormat = dec_info.ChromaFormat;
    }

    if ( NOT_ASSIGNED_VALUE != m_inParams.InputPicstruct )
    {
        m_components[eDEC].m_params.mfx.FrameInfo.PicStruct = m_inParams.InputPicstruct;
    }

    if( m_inParams.bAdaptivePlayback )
    {
        mfxExtBuffer decAdaptivePlayback;
        decAdaptivePlayback.BufferId = MFX_EXTBUFF_DEC_ADAPTIVE_PLAYBACK;
        decAdaptivePlayback.BufferSz = sizeof(mfxExtBuffer);
        m_components[eDEC].m_extParams.push_back(&decAdaptivePlayback);
    }
    // Special case when SFC should be used in decoder
    // FourCC and picstruct must be the same as in decoded frame
    if (!m_extDecVideoProcessing.IsZero())
    {
        m_extDecVideoProcessing->Out.PicStruct    = m_components[eDEC].m_params.mfx.FrameInfo.PicStruct;
        m_extDecVideoProcessing->Out.ChromaFormat = m_components[eDEC].m_params.mfx.FrameInfo.ChromaFormat;
        m_extDecVideoProcessing->Out.FourCC       = m_components[eDEC].m_params.mfx.FrameInfo.FourCC;
        m_inParams.FrameInfo.Width  = m_extDecVideoProcessing->Out.CropW;
        m_inParams.FrameInfo.Height = m_extDecVideoProcessing->Out.CropH;
        m_inParams.FrameInfo.CropW  = m_extDecVideoProcessing->Out.CropW;
        m_inParams.FrameInfo.CropH  = m_extDecVideoProcessing->Out.CropH;
        m_components[eDEC].m_extParams.push_back(m_extDecVideoProcessing);
        m_components[eDEC].AssignExtBuffers();
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::CreateSplitter()
{
    std::auto_ptr<IBitstreamReader> pSpl;

    // for capturing we dont need splitter...
    if (m_inParams.InputCodecType == MFX_CODEC_CAPTURE)
        return MFX_ERR_NONE;

    sStreamInfo sInfo;

    if (m_inParams.bMediaSDKSplitter)
    {
        pSpl.reset(new MediaSDKSplWrapper(m_inParams.extractedAudioFile));
    }
    else if (MFX_CONTAINER_MKV == m_inParams.m_container){
         pSpl.reset(new MKVReader());
    }
    else if (MFX_FOURCC_R16  == m_inParams.FrameInfo.FourCC){
         sStreamInfo *pSinfo = NULL;
         pSinfo = & sInfo;
         sInfo.videoType = m_inParams.m_container;
         sInfo.nWidth = 0;
         sInfo.nHeight = 0;
         sInfo.isDefaultFC = false;
         pSpl.reset(new BayerVideoReader(pSinfo));
    }
    else if (!m_inParams.bYuvReaderMode && 0 == m_inParams.InputCodecType)
    {
        pSpl.reset(new UMCSplWrapper(m_inParams.nCorruptionLevel));
    }
    else
    {
        sStreamInfo *pSinfo = NULL;
        if (!m_inParams.bYuvReaderMode && 0 != m_inParams.InputCodecType)
        {
            pSinfo = & sInfo;
            sInfo.corrupted = m_inParams.nCorruptionLevel;
            sInfo.videoType = m_inParams.InputCodecType;
            sInfo.nWidth = 0;
            sInfo.nHeight = 0;
            sInfo.isDefaultFC = false;
        }
        pSpl.reset(new BitstreamReader(pSinfo));

        //dropping flag if any because bitstream reader provides not a frames
        //want do this let's rely on pipeline builder i.e. user Ex. enc->dec pipeline
        //m_inParams.bCompleteFrame = false;
        if (m_inParams.bExactSizeBsReader)
        {
            pSpl.reset(new ExactLenghtBsReader(GetMinPlaneSize(m_inParams.FrameInfo), pSpl.release()));
        }
    }

    MFX_CHECK_POINTER(pSpl.get());

    if (m_inParams.bOptimizeForPerf && !m_bPreventBuffering)
    {
        m_pSpl = new BufferedBitstreamReader(pSpl.get());
        MFX_CHECK_POINTER(m_pSpl);
        pSpl.release();
    }
    else if (vm_string_strchr(m_inParams.strSrcFile, '*'))
    {
#if defined(_WIN32) || defined(_WIN64)
        m_pSpl = new DirectoryBitstreamReader(pSpl.get());
        MFX_CHECK_POINTER(m_pSpl);
        pSpl.release();
#endif
    }
    else
    {
        m_pSpl = pSpl.release();
    }

    if (m_inParams.bOptimizeForPerf && m_bPreventBuffering)
    {
        PipelineTrace((VM_STRING("WARNING : Perfomance Optimizations is limited, input stream buffering turned off \n")));
    }

    if (0 != m_inParams.nLimitFileReader)
    {
        //TODO: vary chunk
        m_pSpl = new BitrateLimitedReader( m_inParams.nLimitFileReader
            , &PerfCounterTime::Instance()
            , m_inParams.nLimitChunkSize //0- //chunk is equal to splitted video frame
            , m_pSpl);
    }

    if (m_inParams.bVerbose)
    {
        m_pSpl = new VerboseSpl(LOG_LEVEL_PERF, m_pSpl);
    }


    mfxStatus sts = m_pSpl->Init(m_inParams.strSrcFile);

    if (MFX_ERR_NONE != sts)
    {
        if (MFX_ERR_NONE > sts)
        {
            PipelineTrace((VM_STRING("FAILED to open input file %s or find splitter\n"), m_inParams.strSrcFile));
        }else
        {
            if (PIPELINE_WRN_BUFFERING_UNAVAILABLE == sts)
            {
                m_bPreventBuffering = true;
            }
        }

        MFX_CHECK_STS(sts);
    }

#ifndef LUCAS_DLL
    //getfilesize
    Ipp64u file_size = 0;
    // don't request filesize if it's a directory wildcard
    if (!vm_string_strchr(m_inParams.strSrcFile, '*') && MFX_CONTAINER_CRMF != m_inParams.m_container &&  MFX_FOURCC_R16 != m_inParams.FrameInfo.FourCC)
        MFX_CHECK_WITH_ERR( 0 != vm_file_getinfo(m_inParams.strSrcFile, &file_size, NULL), MFX_ERR_UNKNOWN);
    PrintInfo(VM_STRING("FileName"), VM_STRING("%s"), m_inParams.strSrcFile);
    PrintInfo(VM_STRING("FileSize"), VM_STRING("%.2f M"), (double)file_size / (1024. * 1024.));
    m_inParams.nLimitInputBs = file_size;
#endif

    if (m_inParams.bYuvReaderMode)
    {
        PrintInfo(VM_STRING("Container"), VM_STRING("RAW"));
        PrintInfo(VM_STRING("Video"), VM_STRING("%s"), GetMFXFourccString(m_inParams.FrameInfo.FourCC).c_str());
        PrintInfo(VM_STRING("Resolution"), VM_STRING("%dx%d"), m_inParams.FrameInfo.Width, m_inParams.FrameInfo.Height);
    }else if (m_inParams.InputCodecType != 0)
    {
        PrintInfo(VM_STRING("Container"), VM_STRING("RAW"));
        PrintInfo(VM_STRING("Video"), VM_STRING("%s"), GetMFXFourccString(m_inParams.InputCodecType).c_str());
    }else if (m_inParams.m_container == MFX_CONTAINER_CRMF)
    {
        PrintInfo(VM_STRING("Container"), VM_STRING("CLRF"));
        PrintInfo(VM_STRING("Video"), VM_STRING("%s"), GetMFXFourccString(m_inParams.InputCodecType).c_str());
    }

    return MFX_ERR_NONE;
}


mfxStatus MFXDecPipeline::CreateFileSink(std::auto_ptr<IFile> &pSink)
{
    if (NULL == m_pRender)
    {
        return  MFX_ERR_NONE;
    }


    //file sink will be decorated or created
    if (NULL == pSink.get())
    {
        PipelineObjectDesc<IFile> dsc(m_inParams.crcFile, UKNOWN_COMPONENT);
        if (m_inParams.bNullFileWriter)
        {
            dsc.Type = FILE_NULL;

        }
        else
        {
            dsc.Type = FILE_GENERIC;
        }
        pSink.reset(m_pFactory->CreateFileWriter(&dsc));
    }

    if (m_inParams.bUseSeparateFileWriter)
    {
        PipelineObjectDesc<IFile> dsc(m_inParams.crcFile, FILE_SEPARATE, pSink);
        pSink.reset(m_pFactory->CreateFileWriter(&dsc));
    }

    if (m_inParams.bCalcCRC)
    {
        PipelineObjectDesc<IFile> dsc(m_inParams.crcFile, FILE_CRC, pSink);
        pSink.reset(m_pFactory->CreateFileWriter(&dsc));
    }

    return MFX_ERR_NONE;
}
mfxStatus MFXDecPipeline::CreateRender()
{
    mfxStatus sts = MFX_ERR_UNKNOWN;

    if (m_inParams.isAllegroTest || m_inParams.isHMTest)
    {
        m_components[eREN].m_params.mfx.FrameInfo.FourCC         = m_components[eDEC].m_params.mfx.FrameInfo.FourCC;
        m_components[eREN].m_params.mfx.FrameInfo.BitDepthLuma   = m_components[eDEC].m_params.mfx.FrameInfo.BitDepthLuma;
        m_components[eREN].m_params.mfx.FrameInfo.BitDepthChroma = m_components[eDEC].m_params.mfx.FrameInfo.BitDepthChroma;
        m_components[eREN].m_params.mfx.FrameInfo.Shift          = m_components[eDEC].m_params.mfx.FrameInfo.Shift;
    }

    //shouldn't recreate existed render in case of light reset
    if(NULL != m_pRender)
    {
        return MFX_ERR_NONE;
    }

    if (m_inParams.outFrameInfo.FourCC == MFX_FOURCC_UNKNOWN)
    {
        m_inParams.outFrameInfo.FourCC = (m_components[eDEC].m_params.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422) ? static_cast<int>(MFX_FOURCC_YV16) : MFX_FOURCC_YV12;
        m_inParams.outFrameInfo.BitDepthLuma = m_components[eDEC].m_params.mfx.FrameInfo.BitDepthLuma;
        m_inParams.outFrameInfo.BitDepthChroma = m_components[eDEC].m_params.mfx.FrameInfo.BitDepthChroma;

        if (m_components[eDEC].m_params.mfx.FrameInfo.FourCC == MFX_FOURCC_AYUV)
            m_inParams.outFrameInfo.FourCC = MFX_FOURCC_AYUV;

        if (m_components[eDEC].m_params.mfx.FrameInfo.FourCC == MFX_FOURCC_P010 || m_components[eDEC].m_params.mfx.FrameInfo.FourCC == MFX_FOURCC_P210 ||
            m_components[eDEC].m_params.mfx.FrameInfo.FourCC == MFX_FOURCC_Y210 || m_components[eDEC].m_params.mfx.FrameInfo.FourCC == MFX_FOURCC_Y216 ||
            m_components[eDEC].m_params.mfx.FrameInfo.FourCC == MFX_FOURCC_Y410)
        {
            switch (m_components[eDEC].m_params.mfx.FrameInfo.ChromaFormat)
            {
                case MFX_CHROMAFORMAT_YUV444:
                    m_inParams.outFrameInfo.FourCC = MFX_FOURCC_YUV444_16;
                    break;
                case MFX_CHROMAFORMAT_YUV422:
                    m_inParams.outFrameInfo.FourCC = MFX_FOURCC_YUV422_16;
                    break;
                case MFX_CHROMAFORMAT_YUV420:
                default:
                    m_inParams.outFrameInfo.FourCC = MFX_FOURCC_YUV420_16;
                    break;
            }
            m_inParams.outFrameInfo.BitDepthLuma = m_components[eDEC].m_params.mfx.FrameInfo.BitDepthLuma;
            m_inParams.outFrameInfo.BitDepthChroma = m_components[eDEC].m_params.mfx.FrameInfo.BitDepthChroma;
        }
    }
    //crc calculation only possible in filewriter render
    if (m_inParams.bCalcCRC )
    {
        //d3d is default render should switch to fw
        if (MFX_SCREEN_RENDER == m_RenderType)
        {
            m_inParams.bNullFileWriter = true;
            m_RenderType = MFX_FW_RENDER;
        }
    }

    if ( MFX_FOURCC_P010 == m_components[eDEC].m_params.mfx.FrameInfo.FourCC ||
         MFX_FOURCC_P210 == m_components[eDEC].m_params.mfx.FrameInfo.FourCC ||
         MFX_FOURCC_NV16 == m_components[eDEC].m_params.mfx.FrameInfo.FourCC){
        m_components[eREN].m_params.mfx.FrameInfo.FourCC         = m_components[eDEC].m_params.mfx.FrameInfo.FourCC;
        m_components[eREN].m_params.mfx.FrameInfo.BitDepthLuma   = m_components[eDEC].m_params.mfx.FrameInfo.BitDepthLuma;
        m_components[eREN].m_params.mfx.FrameInfo.BitDepthChroma = m_components[eDEC].m_params.mfx.FrameInfo.BitDepthChroma;
        m_components[eREN].m_params.mfx.FrameInfo.Shift = m_components[eDEC].m_params.mfx.FrameInfo.Shift;
    }

    if (   MFX_FOURCC_P010    == m_inParams.outFrameInfo.FourCC
        || MFX_FOURCC_P210    == m_inParams.outFrameInfo.FourCC
        || MFX_FOURCC_NV16    == m_inParams.outFrameInfo.FourCC
        || MFX_FOURCC_NV12    == m_inParams.outFrameInfo.FourCC
        || MFX_FOURCC_YUY2    == m_inParams.outFrameInfo.FourCC
        || MFX_FOURCC_A2RGB10 == m_inParams.outFrameInfo.FourCC
        ) {
            m_components[eREN].m_params.mfx.FrameInfo.FourCC = m_inParams.outFrameInfo.FourCC;
    }

    if ( NOT_ASSIGNED_VALUE != m_inParams.OutputPicstruct )
    {
        m_components[eREN].m_params.mfx.FrameInfo.PicStruct = m_inParams.OutputPicstruct;
    }

    FileWriterRenderInputParams renderParams;
    renderParams.info = m_inParams.outFrameInfo;
    renderParams.useSameBitDepthForComponents = m_inParams.isAllegroTest;
    renderParams.use10bitOutput = m_inParams.isAllegroTest;
    renderParams.useHMstyle = m_inParams.isHMTest;
    renderParams.VpxDec16bFormat = m_inParams.VpxDec16bFormat;
    renderParams.alwaysWriteChroma = m_components[eDEC].m_params.mfx.CodecId == MFX_CODEC_HEVC ? false : true;

    switch (m_RenderType)
    {
    case MFX_FW_RENDER :
        m_pRender = new MFXFileWriteRender(renderParams, m_components[eREN].m_pSession, &sts);
        break;
    case MFX_BMP_RENDER: m_pRender = new MFXBMPRender(m_components[eREN].m_pSession, &sts); break;
#if !defined(WIN_TRESHOLD_MOBILE) && ( defined(_WIN32) || defined(_WIN64) )
    case MFX_SCREEN_RENDER:
        {
            ScreenRender::InitParams iParams(m_components[eREN].GetAdapter(), VideoWindow::InitParams(m_inParams.bFullscreen, m_inParams.OverlayText, m_inParams.OverlayTextSize));

            if (m_inParams.m_WallW && m_inParams.m_WallH)
            {
                if (!IsZero(m_inParams.m_directRect))
                {
                    PipelineTrace((VM_STRING("WARNING: Both direct window position, and video wall based position specified")));
                }

                iParams.window.nX = m_inParams.m_WallW;
                iParams.window.nY = m_inParams.m_WallH;
                iParams.window.nPosition = m_inParams.m_WallN ;
            }
            else
            {
                //calc target resolution based on selected layer for svc
                //TODO: fix SVC decoder to provide proper value in decode header
                MFXExtBufferPtr<mfxExtSVCSeqDesc> seqDescription(m_components[eDEC].m_extParams);

                if (!seqDescription.get() || 0 == m_inParams.svc_layer.Header.BufferId ||
                    m_inParams.svc_layer.TargetDependencyID >= MFX_ARRAY_SIZE(seqDescription->DependencyLayer)||
                    !seqDescription->DependencyLayer[m_inParams.svc_layer.TargetDependencyID].Active)
                {
                    if ( m_inParams.bFullscreen || m_inParams.bFadeBackground )
                    {
                        // In case of fullscreen or fade background mode, window needs to be equal to the screen
                        iParams.window.windowSize.right  = GetSystemMetrics(SM_CXSCREEN);
                        iParams.window.windowSize.bottom = GetSystemMetrics(SM_CYSCREEN);
                    }
                    else if (!IsZero(m_inParams.m_directRect))
                    {
                        // It's not fullscreen nor fade mode. Set size of the window equal to params
                        MFX_CHECK(m_inParams.m_directRect.width != 0 && m_inParams.m_directRect.width != 0);
                        IppiRect &r = m_inParams.m_directRect;
                        ::RECT rdirect = {r.x, r.y, r.x + r.width, r.y + r.height};
                        iParams.window.windowSize = rdirect;
                    }
                    else
                    {
                        // No commands got from user. Set up window equal to frame size
                        iParams.window.windowSize.right  = m_components[eDEC].m_params.mfx.FrameInfo.CropW;
                        iParams.window.windowSize.bottom = m_components[eDEC].m_params.mfx.FrameInfo.CropH;
                    }

                    // Part responsible to set up destination region for frames on rendering window
                    if (!IsZero(m_inParams.m_directRect))
                    {
                        MFX_CHECK(m_inParams.m_directRect.width != 0 && m_inParams.m_directRect.width != 0);
                        IppiRect &r = m_inParams.m_directRect;
                        ::RECT rdirect = {r.x, r.y, r.x + r.width, r.y + r.height};
                        if ( ! m_inParams.bFadeBackground )
                        {
                             rdirect.right  -= rdirect.left;
                             rdirect.bottom -= rdirect.top;
                             rdirect.left = 0;
                             rdirect.top  = 0;
                        }
                        iParams.window.directLocation = rdirect;
                    }
                    else
                    {
                        iParams.window.directLocation.right  = m_components[eDEC].m_params.mfx.FrameInfo.CropW;
                        iParams.window.directLocation.bottom = m_components[eDEC].m_params.mfx.FrameInfo.CropH;
                    }
                }
                else
                {
                    iParams.window.directLocation.right  = seqDescription->DependencyLayer[m_inParams.svc_layer.TargetDependencyID].CropW;
                    iParams.window.directLocation.bottom = seqDescription->DependencyLayer[m_inParams.svc_layer.TargetDependencyID].CropH;
                }
            }

            if ( m_inParams.bFadeBackground && iParams.window.directLocation.right > iParams.window.windowSize.right){
                iParams.window.directLocation.right  = iParams.window.windowSize.right;
            }

            if ( m_inParams.bFadeBackground && iParams.window.directLocation.bottom > iParams.window.windowSize.bottom){
                iParams.window.directLocation.bottom = iParams.window.windowSize.bottom;
            }

            if (m_inParams.m_bNowWidowHeader)
            {
                iParams.window.windowsStyle =  WS_POPUP/*| WS_BORDER|WS_MAXIMIZE */;
            }
            else
            {
                iParams.window.windowsStyle = WS_POPUPWINDOW;
            }

            iParams.window.nMonitor = m_inParams.m_nMonitor;

            if (m_components[eDEC].m_bufType == MFX_BUF_HW || m_components[eREN].m_bufType == MFX_BUF_HW  || m_inParams.bCreateD3D)
            {
                iParams.pDevice = m_pHWDevice;
            }
            else if (m_components[eDEC].m_bufType == MFX_BUF_HW_DX11 || m_components[eREN].m_bufType == MFX_BUF_HW_DX11 )
            {
#if MFX_D3D11_SUPPORT
                iParams.pDevice = m_pHWDevice;

#else
                MFX_TRACE_AND_EXIT(MFX_ERR_UNSUPPORTED, (VM_STRING("D3D11 Not supported")));
#endif
            }
            else
            {
                MFX_TRACE_AND_EXIT(MFX_ERR_UNSUPPORTED, (VM_STRING("On screen rendering not supported with system memory\n")));
            }

            if (m_threadPool.get()) {
                iParams.mThreadPool = m_threadPool.get();
            }

            m_pRender = new ScreenRender(m_components[eREN].m_pSession, &sts, iParams);

            //TODO: move initialisation into constructor may be?
            IHWDevice *pDevice = NULL;
            MFX_CHECK_STS(m_pRender->GetDevice(&pDevice));

            break;
        }
#endif // #if defined(_WIN32) || defined(_WIN64)
#if defined(LIBVA_X11_SUPPORT) && !defined(ANDROID)
    case MFX_SCREEN_RENDER:
        {
            ScreenVAAPIRender::InitParams iParams(m_components[eREN].GetAdapter(), XVideoWindow::InitParams(NULL, m_inParams.bFullscreen));

            if (m_inParams.m_WallW && m_inParams.m_WallH)
            {
                iParams.window.nX = m_inParams.m_WallW;
                iParams.window.nY = m_inParams.m_WallH;
                iParams.window.nPosition = m_inParams.m_WallN ;

                if (m_inParams.m_bNowWidowHeader)
                {
                    //iParams.window.windowsStyle =  WS_POPUP|WS_BORDER|WS_MAXIMIZE;
                }
            }
            //iParams.window.nMonitor = m_inParams.m_nMonitor;

            if (m_components[eDEC].m_bufType == MFX_BUF_HW || m_components[eREN].m_bufType == MFX_BUF_HW  || m_inParams.bCreateD3D)
            {
                iParams.pDevice = m_pHWDevice ;
                //sharing this device
                //m_pHWDevice = iParams.pDevice;
            }
            else
            {
                MFX_TRACE_AND_EXIT(MFX_ERR_UNSUPPORTED, (VM_STRING("On screen rendering not supported with system memory\n")));
            }
            try
            {
                m_pRender = new ScreenVAAPIRender(m_components[eREN].m_pSession, &sts, iParams);

                IHWDevice *pDevice = NULL;
                MFX_CHECK_STS(m_pRender->GetDevice(&pDevice));
            }
            catch (std::exception& e)
            {
                MFX_TRACE_ERR((VM_STRING("On screen rendering initialization failed\n")));
                MFX_DELETE(m_pRender);
            }

            break;
        }
#endif
    case MFX_METRIC_CHECK_RENDER:
        {
            MFXMetricComparatorRender *pRender;
            m_pRender = pRender = new MFXMetricComparatorRender(renderParams, m_components[eREN].m_pSession, &sts);
            MFX_CHECK_POINTER(m_pRender);
            if (m_metrics.empty())
            {
                MFX_CHECK_STS(pRender->AddMetric(METRIC_DELTA, 0));
            }else
            {
                for (size_t i = 0; i < m_metrics.size(); i++)
                {
                    pRender->AddMetric(m_metrics[i].first, m_metrics[i].second);
                }
            }
            MFX_CHECK_STS(pRender->SetSurfacesSource(m_inParams.refFile, NULL, m_inParams.bVerbose, m_inParams.nFrames));
            MFX_CHECK_STS(pRender->SetOutputPerfFile(m_inParams.perfFile));
            break;
        }
    case MFX_OUTLINE_RENDER:
        {
            MFXOutlineRender *pRender = new MFXOutlineRender(renderParams, m_components[eREN].m_pSession, &sts);
            MFX_CHECK_WITH_ERR(pRender, MFX_ERR_MEMORY_ALLOC);

            pRender->SetDecoderVideoParamsPtr(&m_components[eDEC].m_params);
            //if outline key with file, then output of outline should be to this file, but bitstream output will be to -o file
            if (0 != vm_string_strlen(m_inParams.strOutlineFile))
            {
                tstring buf = m_inParams.strDstFile;
                vm_string_strcpy(m_inParams.strDstFile, m_inParams.strOutlineFile);
                vm_string_strcpy(m_inParams.strOutlineFile, buf.c_str());
            }
            pRender->SetFiles(m_inParams.strOutlineFile, m_inParams.refFile);
            m_pRender = pRender;
            break;
        }
    case MFX_NO_RENDER :
        {
            m_components[eREN].m_nMaxAsync = 1;
            m_components[eREN].m_params.mfx.FrameInfo.FourCC = m_components[eDEC].m_params.mfx.FrameInfo.FourCC;
            MFX_CHECK_WITH_ERR(m_pRender = new MFXNullRender(m_components[eREN].m_pSession, &sts), MFX_ERR_MEMORY_ALLOC);
            break;
        }
    case MFX_NULL_RENDER :
        {
            m_components[eREN].m_params.mfx.FrameInfo.FourCC = m_components[eDEC].m_params.mfx.FrameInfo.FourCC;
            m_components[eREN].m_nMaxAsync = 1;
            MFX_CHECK_WITH_ERR(m_pRender = new MFXLockUnlockRender(m_components[eREN].m_pSession, &sts), MFX_ERR_MEMORY_ALLOC);
        }
    default:
        break;
    }

    if (NULL != m_pRender)
    {
        MFX_CHECK_STS(DecorateRender());
        //TODO: it is always a part of render creation?
        //        MFX_CHECK_STS(m_pRender->SetOutputFourcc(m_components[eREN].m_params.mfx.FrameInfo.FourCC));
        MFX_CHECK_STS(m_pRender->SetAutoView(m_inParams.bMultiFiles));
    }

    return sts;
}

mfxStatus MFXDecPipeline::DecorateRender()
{
    if (NULL != m_pRender)
    {
        IFile* pFileRenderFile;
        MFX_CHECK_STS(m_pRender->GetDownStream(&pFileRenderFile));

        std::auto_ptr<IFile> pFile(pFileRenderFile);

        MFX_CHECK_STS(CreateFileSink(pFile));

        MFX_CHECK_STS(m_pRender->SetDownStream(pFile.release()));
    }

    //need frame reordering based on decodeorder filed
    if (1 == m_inParams.DecodedOrder)
    {
        MFX_CHECK_WITH_ERR(m_pRender = new MFXDecodeOrderedRender(m_pRender), MFX_ERR_MEMORY_ALLOC);
    }
    //
    if (m_inParams.bMultiFiles)
    {
        MFX_CHECK_WITH_ERR(m_pRender = new MFXMultiRender(m_pRender), MFX_ERR_MEMORY_ALLOC);
    }
    else if (!m_inParams.DecodedOrder)
    {
        MFX_CHECK_WITH_ERR(m_pRender = new MFXViewOrderedRender(m_pRender), MFX_ERR_MEMORY_ALLOC);
    }

    //burst decorator should be after RateControlRender because it issues renderframe calls in different thread
    if (0.0f != m_inParams.fLimitPipelineFps)
    {
        MFX_CHECK_WITH_ERR(m_pRender = new FPSLimiterRender(m_inParams.bVerbose
            , m_inParams.fLimitPipelineFps
            , &PerfCounterTime::Instance()
            , m_pRender), MFX_ERR_MEMORY_ALLOC);
    }

    if (m_inParams.nBurstDecodeFrames != 0)
    {
        MFX_CHECK_WITH_ERR(m_pRender = new BurstRender(m_inParams.bVerbose, m_inParams.nBurstDecodeFrames
            , &PerfCounterTime::Instance(), *m_threadPool.get(), m_pRender), MFX_ERR_MEMORY_ALLOC);
    }

    if (m_inParams.bUseSeparateFileWriter)
    {
        MFX_CHECK_WITH_ERR(m_pRender = new SeparateFilesRender(m_pRender), MFX_ERR_MEMORY_ALLOC);
    }

    //decorating with drop render
    if (m_components[eREN].m_nDropCyle != 0)
    {
        MFX_CHECK_WITH_ERR(m_pRender = new FramesDrop<IMFXVideoRender>(m_components[eREN].m_nDropCount, m_components[eREN].m_nDropCyle, m_pRender), MFX_ERR_MEMORY_ALLOC);
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::InitRenderParams()
{
    //check input params
    mfxInfoMFX &mfx = m_components[eREN].m_params.mfx;

    //prepare mfxVideoParam
    mfx.EncodedOrder           = m_inParams.EncodedOrder;

    //initialize frame rate code only if the weren't passed from cmd
    if (0 != m_components[eVPP].m_fFrameRate)
    {
        FrameRate2Code(m_components[eVPP].m_fFrameRate, &mfx.FrameInfo);
    }else if (0 != m_components[eDEC].m_fFrameRate)
    {
        FrameRate2Code(m_components[eDEC].m_fFrameRate, &mfx.FrameInfo);
    }else
    {
        MFX_CHECK(0 == m_components[eDEC].m_fFrameRate && 0 == m_components[eVPP].m_fFrameRate);
    }
    mfxFrameInfo *p = &m_components[eDEC].m_params.mfx.FrameInfo;
    //VPP output params should be used in encoder initializing in transcoder pipeline
    //BUG: 9222
    if (m_inParams.bUseVPP)
    {
        p = &m_inParams.FrameInfo;

    }
    mfxFrameInfo &refInfoIn  = *p;
    mfxFrameInfo &refInfoOut = mfx.FrameInfo;

    //decparams now used in initrender params since bug9625, for picstruct since it may be changed, or override we use updated value
    refInfoOut.PicStruct = m_inParams.FrameInfo.PicStruct;

    bool bProg = refInfoOut.PicStruct == MFX_PICSTRUCT_PROGRESSIVE;

    refInfoOut.Width        = mfx_align(refInfoIn.Width, 0x10);
    refInfoOut.Height       = mfx_align(refInfoIn.Height,(bProg) ? 0x10 : 0x20);
    //TODO: cropx cropy settings may affect suites that expect encoder to set the crops, we don't have such test cases YET
    //before modifying zero to something please check VCSD100004870
    refInfoOut.CropX          = refInfoIn.CropX;
    refInfoOut.CropY          = refInfoIn.CropY;
    refInfoOut.CropW          = refInfoIn.CropW;
    refInfoOut.CropH          = refInfoIn.CropH;
    refInfoOut.AspectRatioW   = (0 == refInfoOut.AspectRatioW) ? refInfoIn.AspectRatioW : refInfoOut.AspectRatioW;
    refInfoOut.AspectRatioH   = (0 == refInfoOut.AspectRatioH) ? refInfoIn.AspectRatioH : refInfoOut.AspectRatioH;
    refInfoOut.BitDepthChroma = refInfoIn.BitDepthChroma;
    refInfoOut.BitDepthLuma   = refInfoIn.BitDepthLuma;

    //in case of vpp: it's output set by encoder input, so cannot copy
    if (!m_inParams.bUseVPP)
    {
        //refInfoIn.ChromaFormat = m_components[eVPP].m_params.mfx.FrameInfo.ChromaFormat;
        //refInfoIn.FourCC       = m_components[eVPP].m_params.mfx.FrameInfo.FourCC;
        refInfoOut.FourCC       = refInfoIn.FourCC;
        refInfoOut.ChromaFormat = refInfoIn.ChromaFormat;
    }

    if (!m_extDecVideoProcessing.IsZero())
    {
        refInfoOut.Width    = m_extDecVideoProcessing->Out.Width;
        refInfoOut.Height   = m_extDecVideoProcessing->Out.Height;
        refInfoOut.CropX    = m_extDecVideoProcessing->Out.CropX;
        refInfoOut.CropY    = m_extDecVideoProcessing->Out.CropY;
        refInfoOut.CropW    = m_extDecVideoProcessing->Out.CropW;
        refInfoOut.CropH    = m_extDecVideoProcessing->Out.CropH;
    }

    m_components[eREN].m_extParams.merge
        ( m_components[eDEC].m_extParams.begin()
        , m_components[eDEC].m_extParams.end());
    m_components[eREN].AssignExtBuffers();

    //profile copying for mvc only if not specified in command line
    if (0 == m_components[eREN].m_params.mfx.CodecProfile)
    {
        switch (m_components[eDEC].m_params.mfx.CodecProfile)
        {
        case MFX_PROFILE_AVC_STEREO_HIGH :
        case MFX_PROFILE_AVC_MULTIVIEW_HIGH :
            {
                m_components[eREN].m_params.mfx.CodecProfile = m_components[eDEC].m_params.mfx.CodecProfile;
                break;
            }
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::InitRender()
{
    mfxStatus sts;

    if (NULL == m_pRender) return MFX_ERR_NONE;

    MFX_CHECK_STS(sts = m_pRender->Init(&m_components[eREN].m_params, m_inParams.strDstFile));

    if (m_components[eREN].m_bHWStrict && MFX_WRN_PARTIAL_ACCELERATION == sts)
    {
        sts = MFX_ERR_UNSUPPORTED;
    }
    return sts;
}

mfxStatus MFXDecPipeline::CreateYUVSource()
{
    //TODO: is it possible to store decoder setup param???
    mfxVideoParam yuvDecParam = {};
    yuvDecParam.NumExtParam = m_inParams.NumExtParam;
    yuvDecParam.ExtParam = m_inParams.ExtParam;
    yuvDecParam.mfx.FrameInfo = m_inParams.FrameInfo;
    yuvDecParam.mfx.FrameInfo.Width = m_YUV_Width;
    yuvDecParam.mfx.FrameInfo.Height = m_YUV_Height;
    //TODO: prety uslees to have to very interconnected structures
    yuvDecParam.mfx.FrameInfo.FourCC = m_components[eDEC].m_params.mfx.FrameInfo.FourCC;
    yuvDecParam.mfx.FrameInfo.ChromaFormat = m_components[eDEC].m_params.mfx.FrameInfo.ChromaFormat;
    yuvDecParam.mfx.FrameInfo.Shift = m_components[eDEC].m_params.mfx.FrameInfo.Shift;

    //TODO:
    //in case of yuv decoder we should generate viewids inside mvcdecoder
    //however is we will use outline to store viewids sequence viedids should be generated in YUV source
    bool bGenerateViewIds = false;

    if (vm_string_strlen(m_inParams.strDecPluginGuid))
    {
        m_pYUVSource.reset(m_pFactory->CreateDecode(
            PipelineObjectDesc<IYUVSource>(m_components[eDEC].m_pSession->GetMFXSession()
            , m_inParams.strDecPluginGuid
            , 1
            , DECODER_MFX_PLUGIN_GUID
            , NULL)));
    }
    else if (vm_string_strlen(m_inParams.strDecPlugin))
    {
        m_pYUVSource.reset(m_pFactory->CreateDecode(
            PipelineObjectDesc<IYUVSource>(m_components[eDEC].m_pSession->GetMFXSession()
            , m_inParams.strDecPlugin
            , DECODER_MFX_PLUGIN_FILE
            , NULL)));
    }
    else if (m_inParams.bYuvReaderMode ||
             m_inParams.m_container == MFX_CONTAINER_CRMF ||
        MFX_FOURCC_NV12 == m_inParams.InputCodecType ||
        MFX_FOURCC_YV12 == m_inParams.InputCodecType)
    {
        if ((0.0 == m_components[eDEC].m_fFrameRate) && !m_components[eDEC].m_bFrameRateUnknown)
            m_components[eDEC].m_fFrameRate = 30.0;

        //yuv decoder requires this information to be returned on decode header level
        //others decoder will take this information from bitstream itself
        yuvDecParam.mfx.FrameInfo.PicStruct = Convert_CmdPS_to_MFXPS(m_inParams.nPicStruct);

        m_pYUVSource.reset(new MFXYUVDecoder( m_components[eDEC].m_pSession
            , yuvDecParam
            , m_components[eDEC].m_fFrameRate
            , m_inParams.FrameInfo.FourCC
            , m_pFactory.get()
            , m_inParams.strOutlineInputFile));
        bGenerateViewIds = true;
    }
    else
    {
        m_pYUVSource .reset( new MFXDecoder(m_components[eDEC].m_pSession->GetMFXSession()));
    }

    if (m_inParams.nYUVLoop) {
        m_pYUVSource .reset( new MFXLoopDecoder( m_inParams.nYUVLoop, m_pYUVSource));
    }

    if (m_inParams.nDecodeInAdvance) {
        m_pYUVSource .reset( new MFXAdvanceDecoder(m_inParams.nDecodeInAdvance, m_pYUVSource));
    }

    if (!m_viewOrderMap.empty()) {
        m_pYUVSource .reset( new TargetViewsDecoder(m_viewOrderMap, m_inParams.targetViewsTemporalId, m_pYUVSource));
    }

    //dependency structure specified
    if (!m_InputExtBuffers.empty()) {
        m_pYUVSource .reset( new MVCDecoder(bGenerateViewIds, yuvDecParam, m_pYUVSource));
    }

    if (m_components[eDEC].m_bCalcLatency)
    {
        //calc aggregated timestamps only if there is no intermediate synchronizations
        m_pYUVSource .reset( new LatencyDecoder(m_components[eREN].m_bCalcLatency && m_inParams.bNoPipelineSync, NULL, &PerfCounterTime::Instance(), VM_STRING("Decoder"), m_pYUVSource));
    }

    if (!m_InputExtBuffers.empty() || MFX_CODEC_AVC == m_components[eDEC].m_params.mfx.CodecId || MFX_CODEC_VP8 == m_components[eDEC].m_params.mfx.CodecId)
    {
        m_pYUVSource .reset( new MVCHandler<IYUVSource>(m_components[eDEC].m_extParams, m_components[eDEC].m_bForceMVCDetection, m_pYUVSource));
    }

    if (MFX_CODEC_JPEG == m_components[eDEC].m_params.mfx.CodecId)
    {
        m_pYUVSource .reset( new JPEGBsParser(m_inParams.FrameInfo, m_pYUVSource));

        if (0 != m_inParams.nRotation)
        {
            m_pYUVSource .reset( new RotatedDecoder(m_inParams.nRotation, m_pYUVSource));
        }
    }

    if (!m_components[eDEC].m_SkipModes.empty())
    {
        m_pYUVSource .reset( new SkipModesSeterDecoder(m_components[eDEC].m_SkipModes, m_pYUVSource));
    }

    //header is inited if option is specified
    if (m_inParams.svc_layer.Header.BufferId != 0 )
    {
        m_pYUVSource .reset( new TargetLayerSvcDecoder(m_inParams.svc_layer, m_pYUVSource));
    }

    ///should be created on top of decorators
    m_pYUVSource.reset(new PrintInfoDecoder(m_pYUVSource));

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::InitYUVSource()
{
    mfxStatus sts;

    //See MediaSDK manual, 1 means that decorative flags will be produced by decoder
    //for jpeg there is no term extended pictstruct
    if (!m_inParams.bNoExtPicstruct && m_components[eDEC].m_params.mfx.CodecId != MFX_CODEC_JPEG)
    {
        m_components[eDEC].m_params.mfx.ExtendedPicStruct = 1;
    }

    // init decoder
    MFX_CHECK_STS(sts = m_pYUVSource->Init(&m_components[eDEC].m_params));

    if (m_components[eDEC].m_bHWStrict && MFX_WRN_PARTIAL_ACCELERATION == sts)
    {
        sts = MFX_ERR_UNSUPPORTED;
    }

    return sts;
}

#ifdef D3D_SURFACES_SUPPORT
struct GetMonitorRect_data{
    int current;
    int required;
    RECT requiredRect;
};
BOOL CALLBACK GetMonitorRect_MonitorEnumProc(HMONITOR /*hMonitor*/,
                                             HDC /*hdcMonitor*/,
                                             LPRECT lprcMonitor,
                                             LPARAM dwData)
{
    GetMonitorRect_data *data = reinterpret_cast<GetMonitorRect_data *>(dwData);
    RECT r = {0};
    if (NULL == lprcMonitor)
        lprcMonitor = &r;

    if (data->current ==data->required)
        data->requiredRect = *lprcMonitor;
    data->current++;

    return TRUE;
}
#endif

#if defined(D3D_SURFACES_SUPPORT) || defined(MFX_D3D11_SUPPORT)
D3DFORMAT StrToD3DFORMAT(vm_char *format_name, bool directx11 = false)
{
     D3DFORMAT format = D3DFMT_X8R8G8B8;
     if ( directx11 )
         format = (D3DFORMAT)DXGI_FORMAT_B8G8R8A8_UNORM;

     if ( ! format_name )
         return format;

     if (0 == vm_string_strcmp(VM_STRING("D3DFMT_A2B10G10R10"), format_name) ){
         format = D3DFMT_A2B10G10R10;
         PipelineTrace((VM_STRING("D3D back buffer format: D3DFMT_A2B10G10R10\n")));
     }
     else if (0 == vm_string_strcmp(VM_STRING("DXGI_FORMAT_R10G10B10A2_UNORM"), format_name) ){
         format = (D3DFORMAT)DXGI_FORMAT_R10G10B10A2_UNORM;
         PipelineTrace((VM_STRING("D3D back buffer format: DXGI_FORMAT_R10G10B10A2_UNORM\n")));
     }
     else if (0 == vm_string_strcmp(VM_STRING("D3DFMT_A8B8G8R8"), format_name) ){
         format = D3DFMT_A8B8G8R8;
         PipelineTrace((VM_STRING("D3D back buffer format: D3DFMT_A8B8G8R8\n")));
     }
     else if (0 == vm_string_strcmp(VM_STRING("D3DFMT_A2R10G10B10"), format_name) ){
         format = D3DFMT_A2R10G10B10;
         PipelineTrace((VM_STRING("D3D back buffer format: D3DFMT_A2R10G10B10\n")));
     }
     else if (0 == vm_string_strcmp(VM_STRING("D3DFMT_X8R8G8B8"), format_name) ){
         format = D3DFMT_X8R8G8B8;
         PipelineTrace((VM_STRING("D3D back buffer format: D3DFMT_X8R8G8B8\n")));
     }
     else if (0 == vm_string_strcmp(VM_STRING(""), format_name) ){
         if ( directx11 )
         {
            format = (D3DFORMAT)DXGI_FORMAT_B8G8R8A8_UNORM;
            PipelineTrace((VM_STRING("D3D back buffer format: DXGI_FORMAT_B8G8R8A8_UNORM\n")));
         }
         else
         {
            format = D3DFMT_X8R8G8B8;
            PipelineTrace((VM_STRING("D3D back buffer format: D3DFMT_X8R8G8B8\n")));
         }
     }
     else {

         PipelineTrace((VM_STRING("Unsupported back buffer format provided\n")));
         return (D3DFORMAT)0;
     }
     return format;
}
#endif

mfxStatus MFXDecPipeline::CreateDeviceManager()
{
#ifdef LIBVA_SUPPORT
    //if(MFX_IMPL_HARDWARE == m_components[eDEC].m_RealImpl || m_components[eDEC].m_bufType == MFX_BUF_HW || m_components[eREN].m_bufType == MFX_BUF_HW)
    {
        VADisplay va_dpy = NULL;

        if (NULL == va_dpy && NULL != m_pHWDevice.get())
        {
            MFX_CHECK_STS(m_pHWDevice->GetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL *)&va_dpy));
        }

        if (NULL == va_dpy)
        {
            ComponentParams &cparams = m_components[eDEC].m_bufType == MFX_BUF_HW ? m_components[eDEC] : m_components[eREN];
            m_pHWDevice.reset(CreateVAAPIDevice(MFX_LIBVA_DRM));
            if (m_pHWDevice.get())
            {
                MFX_CHECK_STS(m_pHWDevice->Init(0, NULL, !m_inParams.bFullscreen, 0, 1, NULL));
                MFX_CHECK_STS(m_pHWDevice->GetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL *)&va_dpy));
            }
        }
        // Set VA display to MediaSDK session(s)
        if (va_dpy)
        {
            mfxStatus sts = m_components[eDEC].m_pSession->SetHandle(MFX_HANDLE_VA_DISPLAY, va_dpy);
            MFX_CHECK_STS(sts);
            m_components[eVPP].m_pSession->SetHandle(MFX_HANDLE_VA_DISPLAY, va_dpy);
            m_components[eREN].m_pSession->SetHandle(MFX_HANDLE_VA_DISPLAY, va_dpy);

            //MFXLVARender * m_pLVARender = (MFXLVARender *)m_pRender;
            //m_pLVARender->SetHandle(MFX_HANDLE_VA_DISPLAY, va_dpy);
        }
    }
#endif
    if (m_components[eDEC].m_bufType == MFX_BUF_HW || m_components[eREN].m_bufType == MFX_BUF_HW  || m_inParams.bCreateD3D)
    {
#ifdef D3D_SURFACES_SUPPORT

        IDirect3DDeviceManager9 * pMgr = NULL;

        // Create own D3D device manager using helper d3ddevice class
        if (NULL == m_pHWDevice.get())
        {
            GetMonitorRect_data monitor = {0};
            monitor.required = m_inParams.m_nMonitor;
            EnumDisplayMonitors(NULL, NULL, &GetMonitorRect_MonitorEnumProc, (LPARAM)&monitor);

            POINT point  = {monitor.requiredRect.left, monitor.requiredRect.top};
            HWND  hWindow = WindowFromPoint(point);

            //adapter ID will be selected to match library implementation
            ComponentParams &cparams = m_components[eDEC].m_bufType == MFX_BUF_HW ? m_components[eDEC] : m_components[eREN];

            bool use_D3DPP_over = false;
            D3DPRESENT_PARAMETERS D3DPP_over = {0};
            if (m_inParams.bUseOverlay)
            {
                use_D3DPP_over = true;
                D3DPP_over.SwapEffect = D3DSWAPEFFECT_OVERLAY;
            }
            if (0 != m_inParams.Protected)
            {
                use_D3DPP_over = true;
                D3DPP_over.Flags = D3DPRESENTFLAG_VIDEO | D3DPRESENTFLAG_RESTRICTED_CONTENT | D3DPRESENTFLAG_RESTRICT_SHARED_RESOURCE_DRIVER;
            }

            if (use_D3DPP_over)
                m_pHWDevice.reset(new MFXD3D9DeviceEx(D3DPP_over));
            else
                m_pHWDevice.reset(new MFXD3D9Device());

            if (m_threadPool.get()) {
                m_pHWDevice.reset(new MFXHWDeviceInThread(*m_threadPool.get(), m_pHWDevice.release()));
            }

            D3DFORMAT format = StrToD3DFORMAT(m_inParams.BackBufferFormat);
            if ( 0 == format)
            {
                return MFX_ERR_UNSUPPORTED;
            }
            MFX_CHECK_STS(m_pHWDevice->Init(cparams.GetAdapter(), hWindow, !m_inParams.bFullscreen, format, m_inParams.m_nBackbufferCount, m_inParams.dxva2DllName));
        }
        MFX_CHECK_STS(m_pHWDevice->GetHandle(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, (mfxHDL *)&pMgr));

        // DXVA2Dump wrapper
        if (m_inParams.bDXVA2Dump)
        {
            if (!m_dxvaSpy.get())
                m_dxvaSpy.reset(pMgr = CreateDXVASpy(pMgr));
            else
                pMgr = (IDirect3DDeviceManager9 *)m_dxvaSpy.get();
        }

        // Set D3D device manager to MediaSDK session(s)
        if (pMgr)
        {
            MFX_CHECK_STS(m_components[eDEC].m_pSession->SetHandle(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, pMgr));
            MFX_CHECK_STS(m_components[eVPP].m_pSession->SetHandle(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, pMgr));
            MFX_CHECK_STS(m_components[eREN].m_pSession->SetHandle(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, pMgr));

        }
#endif
    }
    else if (m_components[eDEC].m_bufType == MFX_BUF_HW_DX11 || m_components[eREN].m_bufType == MFX_BUF_HW_DX11 )
    {
#if MFX_D3D11_SUPPORT
        //mfxHDL hdl = NULL;
        ID3D11Device *pDevice = NULL;

        //d3d11 device doesnt exist - create own device and context
        if (NULL == m_pHWDevice.get())
        {
            //adapter ID will be selected to match library implementation
            ComponentParams &cparams = m_components[eDEC].m_bufType == MFX_BUF_HW ? m_components[eDEC] : m_components[eREN];
            m_pHWDevice.reset(
                m_inParams.bDxgiDebug ?
                new MFXD3D11DxgiDebugDevice() :
            new MFXD3D11Device());
            DXGI_FORMAT format = (DXGI_FORMAT)StrToD3DFORMAT(m_inParams.BackBufferFormat, true);
            if ( 0 == format)
            {
                return MFX_ERR_UNSUPPORTED;
            }
            MFX_CHECK_STS(m_pHWDevice->Init(cparams.GetAdapter(), NULL, !m_inParams.bFullscreen, format, 1, m_inParams.dxva2DllName, cparams.m_bD39Feat));
        }
        MFX_CHECK_STS(m_pHWDevice->GetHandle(MFX_HANDLE_D3D11_DEVICE, (mfxHDL *)&pDevice));

        // DXVA2Dump wrapper
        if (m_inParams.bDXVA2Dump)
        {
            m_dxvaSpy.reset(pDevice = CreateDXVASpy(pDevice));
        }

        // Set device and context to MediaSDK session(s)
        MFX_CHECK_STS(m_components[eDEC].m_pSession->SetHandle(MFX_HANDLE_D3D11_DEVICE, pDevice));
        MFX_CHECK_STS(m_components[eVPP].m_pSession->SetHandle(MFX_HANDLE_D3D11_DEVICE, pDevice));
        MFX_CHECK_STS(m_components[eREN].m_pSession->SetHandle(MFX_HANDLE_D3D11_DEVICE, pDevice));

#endif
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::CreateAllocator()
{
    //prepare mfxSurface
    mfxU16 numRenSfr    = 1;
    mfxU16 numDecSfr    = 0;
    mfxU16 numVppSfrIn  = 0;
    mfxU16 numVppSfrOut = 0;

    mfxFrameAllocRequest _request[3][2] = {};

    if (NULL != m_pRender)
    {
        MFX_CHECK_STS(m_pRender->QueryIOSurf(&m_components[eREN].m_params, _request[eREN]));

        numRenSfr = IPP_MAX(_request[eREN]->NumFrameSuggested, 1);

        if (m_components[eREN].m_bufType == MFX_BUF_OPAQ)
        {
            PrintInfo(VM_STRING("Encode.Surface Type"), VM_STRING("OPAQ=>%s"), (_request[eREN]->Type & MFX_MEMTYPE_SYSTEM_MEMORY) ? VM_STRING("system"):VM_STRING("D3D"));
        }
    }

    if (NULL != m_pVPP)
    {
        MFX_CHECK_STS(m_pVPP->QueryIOSurf(&m_components[eVPP].m_params, _request[eVPP]));

        numVppSfrIn  = _request[eVPP][0].NumFrameSuggested;
        numVppSfrOut = _request[eVPP][1].NumFrameSuggested;

        if (m_components[eVPP].m_bufType == MFX_BUF_OPAQ)
        {
            PrintInfo(VM_STRING("Vpp.In.Surface Type"),  VM_STRING("OPAQ=>%s"), (_request[eVPP][0].Type & MFX_MEMTYPE_SYSTEM_MEMORY) ? VM_STRING("system"):VM_STRING("D3D"));
            PrintInfo(VM_STRING("Vpp.Out.Surface Type"), VM_STRING("OPAQ=>%s"), (_request[eVPP][1].Type & MFX_MEMTYPE_SYSTEM_MEMORY) ? VM_STRING("system"):VM_STRING("D3D"));
        }
    }
    //always true
    if (m_pYUVSource.get())
    {
        MFX_CHECK_STS(m_pYUVSource->QueryIOSurf(&m_components[eDEC].m_params, _request[eDEC]));

        numDecSfr = _request[eDEC]->NumFrameSuggested;

        if (m_components[eDEC].m_bufType == MFX_BUF_OPAQ)
        {
            PrintInfo(VM_STRING("Decode.Surface Type"), VM_STRING("OPAQ=>%s"), (_request[eDEC]->Type & MFX_MEMTYPE_SYSTEM_MEMORY) ? VM_STRING("system"):VM_STRING("D3D"));
        }

        if (m_inParams.nDecoderSurfs)
            numDecSfr = m_inParams.nDecoderSurfs;
    }

    //if vpp isn't used we will keep frames in a single storage
    mfxU16 nSurfaces;
    mfxFrameAllocRequest request[2] = {};

    if (NULL == m_pVPP)
    {
        nSurfaces = (numDecSfr + numRenSfr - 1 )
            + (IPP_MAX(m_components[eDEC].m_nMaxAsync, 1) - 1)
            + (IPP_MAX(m_components[eREN].m_nMaxAsync, 1) - 1);

        request->NumFrameMin       = nSurfaces;
        request->NumFrameSuggested = nSurfaces;

        memcpy(&request->Info, &m_components[eREN].m_params.mfx.FrameInfo, sizeof(mfxFrameInfo));
        request->Info.CropH = m_components[eDEC].m_params.mfx.FrameInfo.CropH;
        request->Info.CropW = m_components[eDEC].m_params.mfx.FrameInfo.CropW;

        //request type for opaq memory should be used from decoder queryiosurface return value
        if (m_components[eDEC].m_bufType != MFX_BUF_OPAQ &&
            m_components[eDEC].m_params.mfx.CodecId != MFX_CODEC_JPEG)
        {
            request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_ENCODE;
        }
        else
        {
            request->Type = _request[eDEC]->Type ;
        }
        if( (0 == m_components[eDEC].m_params.mfx.CodecId) && (0 != m_components[eREN].m_params.mfx.CodecId) )
        {
             // pure encode case
             request->Type = _request[eREN]->Type;
#if defined(LINUX)
             request->Type |= (m_components[eREN].m_params.mfx.CodecId == MFX_CODEC_JPEG)? MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET : 0;
#endif
        }
        PrintInfo( VM_STRING("Surfaces dec+rnd")
            , VM_STRING("%sx%dx%dx%d : %d(dec) + %d(ren) - 1 + %d(dec:async) + %d(ren:async)")
            , GetMFXFourccString(request->Info.FourCC).c_str()
            , request->Info.Width
            , request->Info.Height
            , nSurfaces
            , numDecSfr
            , numRenSfr
            , IPP_MAX(m_components[eDEC].m_nMaxAsync, 1) - 1
            , IPP_MAX(m_components[eREN].m_nMaxAsync, 1) - 1
        );

        m_components[eDEC].m_bAdaptivePlayback = m_inParams.bAdaptivePlayback;
        MFX_CHECK_STS(m_components[eDEC].AllocFrames(m_pAllocFactory.get(), m_pHWDevice, request, false));

        if (m_components[eDEC].m_bufType == MFX_BUF_OPAQ)
        {
            //points on just registered surfaces
            ComponentParams::SurfacesAllocated & srf_alloc = m_components[eDEC].RegisterAlloc(*request);

            m_components[eDEC].AttachOpaqBuffer(true,  request->Type, srf_alloc.surfacesLinear);
            m_components[eREN].AttachOpaqBuffer(false, request->Type, srf_alloc.surfacesLinear);
        }

        // Set frame allocator
        if (m_components[eREN].m_bExternalAlloc)
        {
            MFX_CHECK_STS(m_components[eREN].m_pSession->SetFrameAllocator(m_components[eDEC].m_pAllocator));
        }
    }
    else
    {
        nSurfaces = (numDecSfr + numVppSfrIn - 1 )
            + (IPP_MAX(m_components[eDEC].m_nMaxAsync, 1) - 1)
            + (IPP_MAX(m_components[eVPP].m_nMaxAsync, 1) - 1);

        request->NumFrameMin       = nSurfaces;
        request->NumFrameSuggested = nSurfaces;

        memcpy(&request->Info, &m_components[eVPP].m_params.vpp.In, sizeof(mfxFrameInfo));
        request->Info.CropH = m_components[eDEC].m_params.mfx.FrameInfo.CropH;
        request->Info.CropW = m_components[eDEC].m_params.mfx.FrameInfo.CropW;

        //request type for opaq memory should be used from decoder qoeryiosurface return value
        if (m_components[eDEC].m_bufType != MFX_BUF_OPAQ &&
            m_components[eDEC].m_params.mfx.CodecId != MFX_CODEC_JPEG)
        {
            request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_VPPIN;
        }
        else
        {
            request->Type = _request[eDEC]->Type;
        }

        PrintInfo( VM_STRING("Surfaces dec+vpp")
            , VM_STRING("%sx%dx%dx%d (dec:%d + vpp_in:%d - 1 + dec_async:%d + vpp_async:%d)")
            , GetMFXFourccString(request->Info.FourCC).c_str()
            , request->Info.Width
            , request->Info.Height
            , nSurfaces
            , numDecSfr
            , numVppSfrIn
            , IPP_MAX(m_components[eDEC].m_nMaxAsync, 1) - 1
            , IPP_MAX(m_components[eVPP].m_nMaxAsync, 1) - 1);

        //setting-up allocator and alloc frames in here
        MFX_CHECK_STS(m_components[eDEC].AllocFrames(m_pAllocFactory.get(), m_pHWDevice, request, false));

        //attaching buffer with opaq memory info to decoder and vpp extparams
        if (m_components[eDEC].m_bufType == MFX_BUF_OPAQ)
        {
            ComponentParams::SurfacesAllocated & srf_alloc = m_components[eDEC].RegisterAlloc(*request);

            m_components[eDEC].AttachOpaqBuffer(true, request->Type, srf_alloc.surfacesLinear);
            m_components[eVPP].AttachOpaqBuffer(false, request->Type, srf_alloc.surfacesLinear);
        }

        nSurfaces = (numRenSfr + numVppSfrOut - 1 )
            + (IPP_MAX(m_components[eREN].m_nMaxAsync, 1) - 1)
            + (IPP_MAX(m_components[eVPP].m_nMaxAsync, 1) - 1);
        request->NumFrameMin       = nSurfaces;
        request->NumFrameSuggested = nSurfaces;
        memcpy(&request->Info, &m_components[eREN].m_params.mfx.FrameInfo, sizeof(mfxFrameInfo));

        PrintInfo( VM_STRING("Surfaces vpp+ren")
            , VM_STRING("%sx%dx%dx%d (vpp_out:%d + ren:%d - 1 + vpp_async:%d + ren_async:%d)")
            , GetMFXFourccString(request->Info.FourCC).c_str()
            , request->Info.Width
            , request->Info.Height
            , nSurfaces
            , numVppSfrOut
            , numRenSfr
            , IPP_MAX(m_components[eVPP].m_nMaxAsync, 1) - 1
            , IPP_MAX(m_components[eREN].m_nMaxAsync, 1) - 1);

        //in case of opaq memory we may selects vpp or renedr request type
        if (m_components[eREN].m_bufType != MFX_BUF_OPAQ)
        {
            request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_FROM_ENCODE;
        }
        else
        {
            request->Type = _request[eREN]->Type;
        }

        //setuping allocator and alloc frames in here
        //for OPAQ memory we dont have allocator pointer
        if (m_components[eDEC].m_bufType == m_components[eVPP].m_bufType)
        {
            m_components[eVPP].m_pAllocator = m_components[eDEC].m_pAllocator;
        }

        MFX_CHECK_STS(m_components[eVPP].AllocFrames(m_pAllocFactory.get(), m_pHWDevice, request, true));

        //attaching buffer with opaq memory info to vpp and encode extparams
        if (m_components[eREN].m_bufType == MFX_BUF_OPAQ)
        {
            ComponentParams::SurfacesAllocated & srf_alloc =  m_components[eVPP].RegisterAlloc(*request);

            m_components[eVPP].AttachOpaqBuffer(true, request->Type, srf_alloc.surfacesLinear);
            m_components[eREN].AttachOpaqBuffer(false, request->Type, srf_alloc.surfacesLinear);
        }

        if (m_components[eREN].m_bExternalAlloc)
        {
            MFX_CHECK_STS(m_components[eREN].m_pSession->SetFrameAllocator(m_components[eVPP].m_pAllocator));
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::Play()
{
    auto_trace_disabler noTracer(m_inParams.bOptimizeForPerf);
    AutoMessageNotifier notifyPlay(MSG_PROCESSING_START, MSG_PROCESSING_FINISH, MSG_PROCESSING_FAILED, m_inParams.nTestId);

    mfxStatus sts      = MFX_ERR_NONE;
    mfxStatus exit_sts = MFX_ERR_NONE;
    bool      bEOS     = false;

    if (m_components[eDEC].m_params.mfx.CodecId != MFX_CODEC_CAPTURE)
        MFX_CHECK_SET_ERR(NULL != m_pSpl, PE_NO_ERROR, MFX_ERR_NULL_PTR);

    //m_bStat = true;
    m_statTimer.Start();

    //cpuusage measurement start
#if defined(_WIN32) || defined(_WIN64)
    FILETIME notUsedVar;

    GetProcessTimes( GetCurrentProcess()
        , &notUsedVar
        , &notUsedVar
        , (LPFILETIME)&m_KernelTime
        , (LPFILETIME)&m_UserTime );
  #define PIPELINE_START   10
  #define PIPELINE_STOP    11

    #ifndef WIN_TRESHOLD_MOBILE
    DWORD  GLBENCH_MESSAGE = 0;
    if (m_inParams.nTestId != NOT_ASSIGNED_VALUE)
    {
        GLBENCH_MESSAGE = RegisterWindowMessage(_T("GLBenchMessage"));
        if (GLBENCH_MESSAGE) SendNotifyMessage(HWND_BROADCAST, GLBENCH_MESSAGE, 0, PIPELINE_START);
    }
    #endif

#else
    struct rusage usage;
    getrusage(RUSAGE_SELF,&usage);

    m_UserTime = 1000 * usage.ru_utime.tv_sec + (Ipp32u)((Ipp64f)usage.ru_utime.tv_usec / (Ipp64f)1000);
    m_KernelTime = 1000 * usage.ru_stime.tv_sec + (Ipp32u)((Ipp64f)usage.ru_stime.tv_usec / (Ipp64f)1000);
#endif

    mfxU32 nminSize;
    bool   bHasFirstFrame = 0 != m_inBSFrame.DataLength;
    m_bitstreamBuf.GetMinBuffSize(nminSize);

    while (!bEOS && sts != PIPELINE_ERR_STOPPED)
    {
        // Read next frame
        if (m_pSpl)
        {
            MPA_TRACE("ReadNextFrame");
            if ((bHasFirstFrame || MFX_ERR_NONE != (sts = m_pSpl->ReadNextFrame(m_inBSFrame))) && m_inBSFrame.DataLength <= 4)
            {
                //checking for all commands executed
                if (m_commands.empty())
                {
                    m_bitstreamBuf.PutBuffer(NULL, true);//say buffer that EOS reached in upstream
                    bEOS = true;
                }
                else
                {
                    m_inBSFrame.isNull = true;
                    for(;sts != PIPELINE_ERR_STOPPED ;)
                    {
                        sts = RunDecode(m_inBSFrame);
                        if (MFX_ERR_NONE != sts)
                            break;
                    }
                    m_inBSFrame.isNull = false;
                    //TODO: redesign this to fix last frame missed if command gets executed due to stream finished processing
                    (*m_commands.begin())->MarkAsReady();
                    //also notify command about eos, some encdless commands should react on that
                    (*m_commands.begin())->NotifyEOS();
                    MFX_CHECK_STS(ProcessTrickCommands());
                    continue;
                }
            }else
            {
                if (!m_inParams.bVerbose)
                    PipelineTraceSplFrame();
            }
        }

        //not first frame
        bHasFirstFrame = false;
        sts = MFX_ERR_NONE;

        MFX_CHECK_STS(m_bitstreamBuf.PutBuffer(&m_inBSFrame));
        MFX_CHECK_STS(m_bitstreamBuf.SetMinBuffSize(nminSize));

        while (MFX_ERR_NONE == sts)
        {
            MFX_CHECK_STS(ProcessTrickCommands());
            mfxBitstream2 inputBs;

            MFX_CHECK_STS_SKIP(sts = m_bitstreamBuf.LockOutput(&inputBs) , MFX_ERR_MORE_DATA);

            if (MFX_ERR_MORE_DATA == sts && m_pSpl)//still don't have enough data in buffer
            {
                break;
            }

            mfxU32 nInputSize = inputBs.DataLength;

            sts = RunDecode(inputBs);

            MFX_CHECK_STS(m_bitstreamBuf.UnLockOutput(&inputBs));

            //extraordinary exit
            if (sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM && m_extDecVideoProcessing.IsZero())
            {
                exit_sts = sts;
                bEOS     = true;
                //flushing data back to inBSFrame
                m_bitstreamBuf.PutBuffer(NULL, true);
                MFX_CHECK_STS(m_bitstreamBuf.SetMinBuffSize(1));
                MFX_CHECK_STS_SKIP(sts = m_bitstreamBuf.LockOutput(&inputBs) , MFX_ERR_MORE_DATA);
                if (MFX_ERR_MORE_DATA == sts)//still don't have enough data in buffer
                {
                    break;
                }
                MFX_CHECK_STS(MFXBistreamBuffer::MoveBsExtended(&m_inBSFrame, &inputBs));
                MFX_CHECK_STS(m_bitstreamBuf.UnLockOutput(&inputBs));
                MFX_CHECK_STS(m_bitstreamBuf.SetMinBuffSize(nminSize));
                break;
            }

            if (PIPELINE_ERR_STOPPED == sts)
                break;

            sts = (sts != MFX_WRN_VIDEO_PARAM_CHANGED) ? sts : MFX_ERR_NONE;
            MFX_CHECK_STS_SKIP(sts, MFX_ERR_MORE_DATA);

            //if decoder took something , we may have some more data in m_bitstreambuffer
            //in completeframe mode we need to read new frame always and not to rely onremained data
            if (MFX_ERR_MORE_DATA == sts)
            {
                if(nInputSize != inputBs.DataLength)
                {
                    sts = MFX_ERR_NONE;
                }else
                {
                    //if decoder didn't take any data we need to extend buffer if possible
                    mfxU32 nSize;
                    m_bitstreamBuf.GetMinBuffSize(nSize);
                    if (1 != nSize && inputBs.MaxLength != inputBs.DataLength)
                    {
                        //bitstream is limited by cmd line option and we have more data in buffer
                        MFX_CHECK_STS(m_bitstreamBuf.SetMinBuffSize(nSize + 1));
                        sts = MFX_ERR_NONE;
                    }
                }
            }

            if(nInputSize != inputBs.DataLength)
            {
                //state when decoder do not accept any data ended, continue with predefined buffer size
                MFX_CHECK_STS(m_bitstreamBuf.SetMinBuffSize(nminSize));
            }
        } // while constructing frames and decoding

    }

    // to get the last(buffered) decoded frame
    m_inBSFrame.isNull = true;
    for(;sts != PIPELINE_ERR_STOPPED ;)
    {
        sts = RunDecode(m_inBSFrame);
        if (MFX_ERR_NONE != sts)
            break;
    }
    m_inBSFrame.isNull = false;

    MFX_CHECK_STS_SKIP(sts, MFX_ERR_MORE_DATA, PIPELINE_ERR_STOPPED);

    //to get empty render's buffers
    //should skip files_sizes_mismatch status, if err incompatible params happened
    mfxStatus vppSkipSts = (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == exit_sts)
        ? PIPELINE_ERR_FILES_SIZES_MISMATCH
        : MFX_ERR_NONE;

    MFX_CHECK_STS_SKIP(RunVPP(NULL), vppSkipSts);

    if (MFX_ERR_NONE == exit_sts)
        notifyPlay.Complete();

#ifndef WIN_TRESHOLD_MOBILE
    #if defined(_WIN32) || defined(_WIN64)
    if (GLBENCH_MESSAGE) SendNotifyMessage(HWND_BROADCAST, GLBENCH_MESSAGE, 0, PIPELINE_STOP);
    #endif
#endif
    return exit_sts;
}



struct dispMapPusher : public std::binary_function<MFXDecodeOrderedRender*, SrfEncCtl, void>
{
    void operator()(MFXDecodeOrderedRender*pRender, SrfEncCtl ctrl)const
    {
        pRender->PushToDispMap(ctrl.pSurface);
    }
};

mfxStatus MFXDecPipeline::RunDecode(mfxBitstream2 & bs)
{
    MPA_TRACE("rundecode");

    mfxFrameSurface1 *pDecodedSurface = NULL;
    SrfEncCtl        inSurface;
    mfxSyncPoint     syncp           = NULL;
    mfxStatus        sts             = MFX_ERR_MORE_SURFACE;
    MFXDecodeOrderedRender* pReoderRnd = dynamic_cast<MFXDecodeOrderedRender*>(m_pRender);
    Timeout<5>    dec_timeout;

    if (m_inParams.encodeExtraParams.nDelayOnMSDKCalls != 0)
    {
        MPA_TRACE("Sleep(nDelayOnMSDKCalls)");
        vm_time_sleep(m_inParams.encodeExtraParams.nDelayOnMSDKCalls);
    }

    for (; sts == MFX_ERR_MORE_SURFACE || sts == MFX_WRN_DEVICE_BUSY || sts == (mfxStatus)MFX_ERR_REALLOC_SURFACE;)
    {
        if (sts == MFX_WRN_DEVICE_BUSY)
        {
            if( m_components[eDEC].m_SyncPoints.empty())
            {
                MFX_CHECK_WITH_ERR(dec_timeout.wait(), MFX_WRN_DEVICE_BUSY);
            }
            else
            {
                MFX_CHECK_STS(m_pYUVSource->SyncOperation(m_components[eDEC].m_SyncPoints.begin()->first, TimeoutVal<>::val()));
            }
        }
        MFX_CHECK_STS(m_components[eDEC].FindFreeSurface(bs.DependencyId, &inSurface, m_pRender));
        if (m_inParams.bPrintSplTimeStamps && !bs.isNull)
        {
            vm_string_printf(VM_STRING("spl_pts = %.2lf\n"), ConvertMFXTime2mfxF64(bs.TimeStamp));
        }

        if (sts == (mfxStatus)MFX_ERR_REALLOC_SURFACE)
        {
            mfxVideoParam param;
            m_pYUVSource->GetVideoParam(&param);
            inSurface.pSurface->Info.Width = param.mfx.FrameInfo.Width;
            inSurface.pSurface->Info.Height = param.mfx.FrameInfo.Height;

            m_components[eDEC].ReallocSurface(inSurface.pSurface);
        }

        bs.DataFlag = (mfxU16)(m_inParams.bCompleteFrame ? MFX_BITSTREAM_COMPLETE_FRAME : 0);

        if (m_pSpl == NULL)
            bs.isNull = true;
#ifdef LIBVA_SUPPORT
        if ( m_inParams.bAdaptivePlayback )
        {
            vaapiMemId *vapi_id = (vaapiMemId *)(inSurface.pSurface->Data.MemId);
            if ( (VASurfaceID)VA_INVALID_ID == *(vapi_id->m_surface))
            {
                m_components[eDEC].m_pAllocator->Lock(m_components[eDEC].m_pAllocator->pthis,inSurface.pSurface->Data.MemId, NULL );
            }

        }
#endif
        sts = m_pYUVSource->DecodeFrameAsync(bs, inSurface.pSurface, &pDecodedSurface, &syncp);
        HandleIncompatParamsCode(sts, IP_DECASYNC, bs.isNull);
    }

    // need to fill map of frames to display
    if (NULL != pReoderRnd && bs.isNull && m_components[eDEC].m_params.mfx.CodecId == MFX_CODEC_VC1)
    {

        std::vector<SrfEncCtl> &sfr = m_components[eDEC].m_Surfaces1.front().surfaces;

        for_each(sfr.begin(), sfr.end(), bind1st(dispMapPusher(), pReoderRnd));
    }

    if (MFX_WRN_VIDEO_PARAM_CHANGED == sts)
        MFX_CHECK_STS_TRACE_EXPR(sts, m_pYUVSource->DecodeFrameAsync(bs, inSurface.pSurface, &pDecodedSurface, &syncp))
    else if (sts != MFX_ERR_NONE)
    {
        if (bs.isNull && !m_components[eDEC].m_SyncPoints.empty())
        {
            //have to drain async queue at this point
        }else
        {
            //only err more_data or video_param_changed is correct err code at this point
            if (MFX_ERR_NONE != sts && MFX_ERR_MORE_DATA != sts)
            {
                MFX_CHECK_STS_TRACE_EXPR(sts, m_pYUVSource->DecodeFrameAsync(bs, inSurface.pSurface, &pDecodedSurface, &syncp));
            }

            return sts;
        }
    }

    if (m_components[eDEC].m_nMaxAsync != 0)
    {
        if (NULL != pDecodedSurface)
        {
            IncreaseReference(&pDecodedSurface->Data);
            m_components[eDEC].m_SyncPoints.push_back(ComponentParams::SyncPair(syncp, pDecodedSurface));
        }

        for (;!m_components[eDEC].m_SyncPoints.empty();)
        {
            if (m_components[eDEC].m_SyncPoints.size() == m_components[eDEC].m_nMaxAsync || bs.isNull)
            {
                //should wait at list one frame here
                sts = m_pYUVSource->SyncOperation(m_components[eDEC].m_SyncPoints.begin()->first, TimeoutVal<>::val());
                if ((MFX_ERR_ABORTED == sts) && (m_components[eDEC].m_params.mfx.CodecId == MFX_CODEC_HEVC))
                {
                    // in case of GPU hang HEVCd plug-in may return ERR_ABORTED.
                    // we shold ignore this and call DecodeFrameAsync to retrieve ERR_GPU_HANG.
                    pDecodedSurface = m_components[eDEC].m_SyncPoints.begin()->second.pSurface;
                    DecreaseReference(&pDecodedSurface->Data);
                    m_components[eDEC].m_SyncPoints.pop_front();

                    sts = MFX_ERR_NONE;
                    break;
                }
                MFX_CHECK_STS(sts);
            }

            //only checking for completed task
            for ( ; !m_components[eDEC].m_SyncPoints.empty() &&
                MFX_ERR_NONE == m_pYUVSource->SyncOperation( m_components[eDEC].m_SyncPoints.begin()->first, 0)
                ; )
            {
                pDecodedSurface = m_components[eDEC].m_SyncPoints.begin()->second.pSurface;

                //taking out of queue this surface since it is already synchronized
                m_components[eDEC].m_SyncPoints.pop_front();

                if (NULL != pDecodedSurface)
                {
                    sts = RunVPP(pDecodedSurface);
                    //always decreasing lock to prevent timeout event happened
                    DecreaseReference(&pDecodedSurface->Data);
                    MFX_CHECK_STS_TRACE_EXPR(sts, RunVPP(pDecodedSurface));
                    //check exiting status
                    if (MFX_ERR_NONE != (sts = CheckExitingCondition()))
                    {
                        return sts;
                    }
                }
            }

            if (!bs.isNull)
            {
                break;
            }
        }
    }else
    {
        //no synhronisation
        if (NULL != pDecodedSurface)
        {
            MFX_CHECK_STS(RunVPP(pDecodedSurface));
        }
    }

    return CheckExitingCondition();
}

mfxStatus MFXDecPipeline::CheckExitingCondition()
{
    if (m_inParams.nFrames && m_nDecFrames >= m_inParams.nFrames)
    {
        return PIPELINE_ERR_STOPPED;
    }

    return MFX_ERR_NONE;
}

mfxStatus  MFXDecPipeline::RunVPP(mfxFrameSurface1 *pSurface)
{
    bool bInSupplied = false;

    if ( m_inParams.bExtendedFpsStat )
        m_time_stampts.push_back(m_statTimer.CurrentTiming());
    if (NULL != pSurface)
    {
        if (m_components[eDEC].m_bPrintTimeStamps)
        {
            vm_string_printf(VM_STRING("dec_time = %.2lf\n"), ConvertMFXTime2mfxF64(pSurface->Data.TimeStamp));
        }

        if (m_components[eDEC].m_bOverPS)
        {
            pSurface->Info.PicStruct = m_components[eDEC].m_nOverPS;
        }

        PipelineTraceDecFrame();

        m_components[eDEC].m_uiMaxAsyncReached = (std::max)(m_components[eDEC].m_uiMaxAsyncReached, (mfxU32)m_components[eDEC].m_SyncPoints.size());
        m_components[eDEC].m_fAverageAsync =
            ((m_components[eDEC].m_fAverageAsync * m_nDecFrames) + m_components[eDEC].m_SyncPoints.size()) / (m_nDecFrames + 1);
        m_nDecFrames++;

        mfxF64 fTime = ConvertMFXTime2mfxF64(pSurface->Data.TimeStamp);

        if (m_fLastTime == 0)
        {
            m_fFirstTime = fTime;
        }

        m_fLastTime = fTime;
    }

    //vpp async
    Timeout<5>    vpp_timeout;
    for (;NULL != m_pVPP;)
    {
        SrfEncCtl     vppOut           = NULL;
        SrfEncCtl     vppWork          = NULL;
        mfxSyncPoint  syncp            = NULL;
        mfxStatus     sts              = MFX_ERR_MORE_SURFACE;
        bool          bOneMoreRunFrame = false;

        if( m_inParams.bExtVppApi )
        {
            // RunFrameVPPAsyncEx needs work surfaces
            MFX_CHECK_STS(m_components[eVPP].FindFreeSurface(NULL != pSurface? pSurface->Info.FrameId.DependencyId : 0,  &vppWork, m_pRender));
        }
        else
        {
            MFX_CHECK_STS(m_components[eVPP].FindFreeSurface(NULL != pSurface? pSurface->Info.FrameId.DependencyId : 0,  &vppOut, m_pRender));
            if ( vppOut.pSurface )
            {
                /* picstruct of the output frame could be incorrect since frames are taken from a single pool. To eliminate
                 * mess, drop picstruct to the original that was used at allocation stage */
                vppOut.pSurface->Info.PicStruct = m_components[eREN].m_params.mfx.FrameInfo.PicStruct;
            }
        }

        if (m_inParams.encodeExtraParams.nDelayOnMSDKCalls != 0) // && sts >= MFX_ERR_NONE)
        {
            MPA_TRACE("Sleep(nDelayOnMSDKCalls)");
            vm_time_sleep(m_inParams.encodeExtraParams.nDelayOnMSDKCalls);
        }

        if ( m_inParams.bExtVppApi )
        {
            //RunFrameVPPAsyncEx decoder-like processing
            sts = m_pVPP->RunFrameVPPAsyncEx(pSurface, vppWork.pSurface, &vppOut.pSurface, NULL, &syncp);
            if (MFX_ERR_MORE_DATA == sts)
            {
                if ( ! m_bVPPUpdateInput && ! bInSupplied )
                {
                    // If input surface was not provided yet, do it at next iteration.
                    m_bVPPUpdateInput = true;
                    bInSupplied       = true;
                    continue;
                }

                // Go back to decoder in order to get next frame
                if(pSurface)
                    return MFX_ERR_NONE;
            }

            m_bVPPUpdateInput = false;

            if ( MFX_ERR_NONE == sts)
            {
                // In case of ERR_NONE need to drain cached frames w/o providing new input.
                bInSupplied      = true;
                bOneMoreRunFrame = true;
            }
        }
        else
        {
            // Usual VPP processing
            sts = m_pVPP->RunFrameVPPAsync(pSurface, vppOut.pSurface, (mfxExtVppAuxData*)vppOut.pCtrl->ExtParam[0], &syncp);
        }

        if (sts == MFX_WRN_DEVICE_BUSY)
        {
            if( m_components[eVPP].m_SyncPoints.empty())
            {
                MFX_CHECK_WITH_ERR(vpp_timeout.wait(), MFX_WRN_DEVICE_BUSY);
            }
            else
            {
                MFX_CHECK_STS(m_components[eVPP].m_pSession->SyncOperation(m_components[eVPP].m_SyncPoints.begin()->first, TimeoutVal<>::val()));
            }
            continue;
        }
        //sts = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        HandleIncompatParamsCode(sts, IP_VPPASYNC, NULL == pSurface);

        if (MFX_ERR_MORE_SURFACE == sts )
        {
            //vpp required to call it one more time, and output already is ready
            bOneMoreRunFrame = true;
            sts = MFX_ERR_NONE;
        }

        //TODO: documentation say nothing about handling of incompatible video params code
        if (sts < MFX_ERR_NONE)
        {
            if (NULL == pSurface && m_components[eVPP].m_SyncPoints.empty())
            {
                break;
            }
            //only err more_data is correct err code at this point
            if (MFX_ERR_MORE_DATA != sts && MFX_ERR_NONE != sts)
            {
                MFX_CHECK_STS_TRACE_EXPR(sts, m_pVPP->RunFrameVPPAsync(pSurface, vppOut.pSurface, (mfxExtVppAuxData*)vppOut.pCtrl->ExtParam[0], &syncp));
            }

            //if last frame is retrieved from buffer we should retrieve it from encoder also
            if (NULL != pSurface)
            {
                //cannot return error codes from runvpp - only true failure
                return MFX_ERR_NONE;
            }
        }

        if (m_components[eVPP].m_nMaxAsync != 0)
        {
            //syncpoint is absent if error more data returned (it is possible only if pSurface ==NULL)
            if (NULL != syncp && NULL != vppOut.pSurface)
            {
                IncreaseReference(&vppOut.pSurface->Data);

                m_components[eVPP].m_SyncPoints.push_back(ComponentParams::SyncPair(syncp, vppOut));
            }

            for (;!m_components[eVPP].m_SyncPoints.empty();)
            {
                if (m_components[eVPP].m_SyncPoints.size() == m_components[eVPP].m_nMaxAsync || NULL == pSurface)
                {
                    //should wait at list one frame here
                    MFX_CHECK_STS(m_pVPP->SyncOperation(m_components[eVPP].m_SyncPoints.begin()->first, TimeoutVal<>::val()));
                }

                //only checking for completed task
                for ( ; !m_components[eVPP].m_SyncPoints.empty() &&
                    MFX_ERR_NONE == m_pVPP->SyncOperation(m_components[eVPP].m_SyncPoints.begin()->first, 0)
                    ; )
                {
                    mfxFrameSurface1 *pVpp   = m_components[eVPP].m_SyncPoints.begin()->second.pSurface;
                    mfxEncodeCtrl * pControl = m_components[eVPP].m_SyncPoints.begin()->second.pCtrl;

                    //taking out first surface since it already synchronized
                    m_components[eVPP].m_SyncPoints.pop_front();

                    if (NULL != pVpp)
                    {
                        MFX_CHECK_STS(RunRender(pVpp, pControl));

                        DecreaseReference(&pVpp->Data);
                    }
                }

                if (NULL != pSurface)
                {
                    break;
                }
            }
        }
        else
        {
            MFX_CHECK_STS(RunRender(vppOut.pSurface, vppOut.pCtrl));
        }

        if (NULL != pSurface && !bOneMoreRunFrame)
            break;
    }

    if(NULL == m_pVPP || NULL == pSurface)
    {
        MFX_CHECK_STS(RunRender(pSurface));
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::RunRender(mfxFrameSurface1* pSurface, mfxEncodeCtrl *pControl)
{
#if defined(_WIN32) || defined(_WIN64)
    vm_char title[1024];
    if ( m_inParams.bUpdateWindowTitle )
    {
        if ( m_inParams.nFrames )
        {
            vm_string_sprintf(title, VM_STRING("Frames processed: %d of %d"), m_nDecFrames, m_inParams.nFrames);
        }
        else
        {
            vm_string_sprintf(title, VM_STRING("Frames processed: %d"), m_nDecFrames);
        }
        SetConsoleTitle(title);
    }
#endif
    if (NULL != pSurface)
    {
        m_components[eVPP].m_uiMaxAsyncReached = (std::max)(m_components[eVPP].m_uiMaxAsyncReached, (mfxU32)m_components[eVPP].m_SyncPoints.size());
        m_components[eVPP].m_fAverageAsync =
            ((m_components[eVPP].m_fAverageAsync * m_nVppFrames) + m_components[eVPP].m_SyncPoints.size()) / (m_nVppFrames + 1);

        m_nVppFrames++;

        if (m_components[eVPP].m_bPrintTimeStamps)
        {
            vm_string_printf(VM_STRING("vpp_time = %.2lf\n"), ConvertMFXTime2mfxF64(pSurface->Data.TimeStamp));
        }
        if (m_components[eVPP].m_bOverPS)
        {
            pSurface->Info.PicStruct = m_components[eVPP].m_nOverPS;
        }
    }

    if(NULL != m_pRender)
    {
        if ( m_components[eREN].m_SkippedFrames.size() > 0 )
        {
            if ( NULL == pControl)
            {
                pControl = new mfxEncodeCtrl;
                memset(pControl, 0, sizeof(mfxEncodeCtrl));
            }
            mfxU32 skipFrame = m_components[eREN].m_SkippedFrames.front();
            if ( (m_nDecFrames) == skipFrame + 1)
            {
                PipelineTrace((VM_STRING("\n Skip frame #%d encoding\n"), skipFrame));
                pControl->SkipFrame = 1;
                m_components[eREN].m_SkippedFrames.erase(m_components[eREN].m_SkippedFrames.begin());
            }
        }

        MFX_CHECK_STS(HandleIncompatParamsCode(m_pRender->RenderFrame(pSurface, pControl), IP_ENCASYNC, NULL == pSurface));
    }

    return MFX_ERR_NONE;
}

vm_char * MFXDecPipeline::GetLastErrString()
{
    static struct
    {
        mfxU32 err;
        vm_char  desc[100];
    } errDetails[] =
    {
        {PE_INV_INPUT_FILE,       VM_STRING("Invalid Input File")},
        {PE_INV_OUTPUT_FILE,      VM_STRING("Invalid Output File")},
        {PE_INV_RENDER_TYPE,      VM_STRING("Invalid Render Type")},
        {PE_INV_LIB_TYPE,         VM_STRING("Invalid Library Type")},
        {PE_INV_BUF_TYPE,         VM_STRING("Invalid Buffer Type")},
        {PE_C_FMT_THIS_MODE,      VM_STRING("Color Format option must not be used with this render")},
        {PE_C_FMT_MUST_THIS_MODE, VM_STRING("Color Format option must be specified for this render")},
        {PE_CHECK_PARAMS,         VM_STRING("Invalid input Params")},
        {PE_CREATE_SPL,           VM_STRING("Unable To Create Splitter")},
        {PE_CREATE_ALLOCATOR,     VM_STRING("Unable To Create Allocator")},
        {PE_CREATE_DEC,           VM_STRING("Unable To Create Decoder")},
        {PE_CREATE_VPP,           VM_STRING("Unable To Create VPP")},
        {PE_CREATE_RND,           VM_STRING("Unable To Create Render")},
        {PE_INIT_CORE,            VM_STRING("Failed to initialize MFX Core")},
        {PE_DECODE_HEADER,        VM_STRING("Failed to DecodeHeader")},
        {PE_PAR_FILE,             VM_STRING("Invalid Parameters File")},
        {PE_OPTION,               VM_STRING("Invalid command line")}
    };

    for (size_t i=0; i < sizeof(errDetails)/sizeof(errDetails[0]); i++)
    {
        if (errDetails[i].err == PipelineGetLastErr())
        {
            return errDetails[i].desc;
        }
    }

    return const_cast<vm_char*>(VM_STRING("Unknown"));
}

mfxStatus MFXDecPipeline::ProcessCommand(vm_char ** &argv, mfxI32 argc, bool bReportError)
{
    MFX_CHECK_SET_ERR(NULL != argv, PE_OPTION, MFX_ERR_NULL_PTR);
    MFX_CHECK_SET_ERR(0 != argc, PE_OPTION, MFX_ERR_UNKNOWN);

    mfxStatus sts;

    if (MFX_ERR_NONE != (sts = ProcessCommandInternal(argv, argc, bReportError)) &&
        MFX_ERR_UNSUPPORTED != sts)
    {
        PipelineTrace((VM_STRING("ERROR: During parsing option: %s\n"), argv[0]));
    }

    return sts;
}

ComponentParams* MFXDecPipeline::GetComponentParams(vm_char SYM)
{
    switch (tolower(SYM))
    {
    case 'd':
        {
            return &m_components[eDEC];
        }
    case 'v':
        {
            m_inParams.bUseVPP = true;
            return &m_components[eVPP];
        }

    case 'r':
    case 'e':
        {
            return &m_components[eREN];
        }
    case 's':
    default :
        return NULL;
    }
}


mfxStatus MFXDecPipeline::ProcessCommandInternal(vm_char ** &argv, mfxI32 argc, bool bReportError)
{
    if (m_OptProc.GetPrint())
    {
        vm_string_printf(VM_STRING("  Decode, VPP specific parameters:\n"));
    }

    vm_char **argvEnd = argv + argc;
    bool bUnused = false;

    for (; argv != argvEnd; argv++)
    {
        int nPattern = 0;
        int nPattern1 =0;
        int nPattern2 =0;
        int nPattern3 =0;
        int nForDeprecatedParams;
        bool bUnhandled = false;

        if (m_OptProc.Check(argv[0], VM_STRING("-par"), VM_STRING("read parameters from par-file; par-file syntax: PARAM = VALUE"), OPT_FILENAME))
        {
            MFX_CHECK(1 + argv != argvEnd);

            Adapter<IProcessCommand, IMFXPipeline> wrapper (this);

            MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.strParFile, MFX_ARRAY_SIZE(m_inParams.strParFile), argv[1]));

            MFX_CHECK_STS(ReadParFile(m_inParams.strParFile, &wrapper));
            argv++;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-version"), VM_STRING("use specific mediasdk API version"), OPT_SPECIAL, VM_STRING("major.minor")))
        {
            DeSerializeHelper<mfxVersion> sVersion;
            MFX_CHECK(sVersion.DeSerialize(++argv, argvEnd));

            //SET_GLOBAL_PARAM(m_libVersion, sVersion);
            std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_libVersion, sVersion));

            for(size_t i = 0; i < m_components.size(); i++)
            {
                m_components[i].m_pLibVersion = &m_components[i].m_libVersion;
            }
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-sw|-swlib"), VM_STRING("use pure-software MediaSDK library (libmfxswXX.dll)")))
        {
            //SET_GLOBAL_PARAM(m_libType, MFX_IMPL_SOFTWARE);
            std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_libType, (int)MFX_IMPL_SOFTWARE));
        }
        else if (0 != (nPattern = m_OptProc.Check(argv[0], VM_STRING("-hw( |:1|:2|:3|:4|:default)")
            , VM_STRING("use hardware-accelerated MediaSDK library (libmfxhw*.dll), also particular device id might be specified"))))
        {
            mfxIMPL impl = MFX_IMPL_UNSUPPORTED;
            switch(nPattern)
            {
            case 1 : impl = MFX_IMPL_HARDWARE_ANY;  break;
            case 2 : impl = MFX_IMPL_HARDWARE; break;
            case 3 : impl = MFX_IMPL_HARDWARE2; break;
            case 4 : impl = MFX_IMPL_HARDWARE3; break;
            case 5 : impl = MFX_IMPL_HARDWARE4; break;
            case 6 : impl = MFX_IMPL_HARDWARE; break;
            }
            std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_libType, impl));
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-hw_strict"), VM_STRING("same as -hw but ensure HW-accelerated codecs (fail otherwise)")))
        {
            std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_libType, (int)MFX_IMPL_HARDWARE_ANY));
            std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_bHWStrict, true));

            //SET_GLOBAL_PARAM(m_bHWStrict, true);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-auto"), VM_STRING("automatically select MediaSDK library (HW or SW) for hw library default device will be used")))
        {
            std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_libType, (int)MFX_IMPL_AUTO));
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-auto:any"), VM_STRING("automatically select MediaSDK library (SW or HW), for HW library any suitable device will be used")))
        {
            std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_libType, (int)MFX_IMPL_AUTO_ANY));
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-mfxdll"), VM_STRING("load MediaSDK DLL library from explicit dll path"), OPT_FILENAME))
        {
            MFX_CHECK(1 + argv != argvEnd);
            MFX_CHECK(0==vm_string_strcpy_s(m_inParams.pMFXLibraryPath,  MFX_ARRAY_SIZE(m_inParams.pMFXLibraryPath), argv[1]));
            argv++;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-InputFile|-i|--input"), VM_STRING("input file"), OPT_FILENAME)||
            0!=(nPattern = m_OptProc.Check(argv[0], VM_STRING("-i:(")VM_STRING(MFX_FOURCC_PATTERN())VM_STRING(")"), VM_STRING("input file is raw YUV, or RGB color format"), OPT_FILENAME)))
        {
            MFX_CHECK(1 + argv != argvEnd);
            argv++;

            mfxFrameInfo info;
            if (MFX_ERR_NONE == GetMFXFrameInfoFromFOURCCPatternIdx(nPattern, info))
            {
                m_inParams.FrameInfo.FourCC = info.FourCC;
                m_inParams.FrameInfo.ChromaFormat = info.ChromaFormat;
                m_inParams.bYuvReaderMode = true;
            }
            else
            {
                //by default input YUV files in yv12
                m_inParams.FrameInfo.FourCC = MFX_FOURCC_YV12;
            }

            m_inParams.m_container = MFX_CONTAINER_RAW;
            vm_string_strcpy_s(m_inParams.strSrcFile, MFX_ARRAY_SIZE(m_inParams.strSrcFile), argv[0]);
        }
        else if (0!=(nPattern = m_OptProc.Check(argv[0], VM_STRING("-i:(h264|mpeg2|vc1|mvc|jpeg|hevc|vp8|h263|null)"), VM_STRING("input stream is raw(non container), pipeline works without demuxing"), OPT_UNDEFINED)))
        {
            MFX_CHECK(1 + argv != argvEnd);
            if (nPattern != 9)
            {
                argv++;
                vm_string_strcpy_s(m_inParams.strSrcFile, MFX_ARRAY_SIZE(m_inParams.strSrcFile), argv[0]);
            }

            switch (nPattern)
            {
            case 1 :
                m_inParams.InputCodecType = MFX_CODEC_AVC;
                break;
            case 2 :
                m_inParams.InputCodecType = MFX_CODEC_MPEG2;
                break;
            case 3 :
                m_inParams.InputCodecType = MFX_CODEC_VC1;
                break;
            case 4:
                m_inParams.InputCodecType = MFX_CODEC_AVC;
                m_components[eDEC].m_bForceMVCDetection = true;
                break;
            case 5 :
                m_inParams.InputCodecType = MFX_CODEC_JPEG;
                break;
            case 6 :
                m_inParams.InputCodecType = MFX_CODEC_HEVC;
                break;
            case 7 :
                m_inParams.InputCodecType = MFX_CODEC_VP8;
                break;
            case 8:
                m_inParams.InputCodecType = MFX_CODEC_H263;
                break;
            case 9:
                m_inParams.InputCodecType = MFX_CODEC_CAPTURE;
                break;
            }
            m_inParams.m_container = MFX_CONTAINER_RAW;
        }
        else HANDLE_INT_OPTION(m_inParams.nLimitChunkSize, VM_STRING("-i:maxdatachunk"), VM_STRING("restrict file reading by special size"))
    else if (0!=(nPattern = m_OptProc.Check(argv[0], VM_STRING("-i:maxbitrate"), VM_STRING("limit input bitrate"), OPT_SPECIAL, VM_STRING("bits/sec"))))
        {
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(m_inParams.nLimitFileReader, argv[1]);
            //conversion to bytes/sec
            m_inParams.nLimitFileReader >>= 3;
            argv++;
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-o:split"), VM_STRING("split outputfile every n-frames"), OPT_INT_32))
    {
        MFX_CHECK(1 + argv != argvEnd);
        mfxU32 nSplitFrames;
        MFX_PARSE_INT(nSplitFrames, argv[1]);
        argv++;

        reopenFileCommand *pCmd = new reopenFileCommand(this);
        MFX_CHECK_WITH_ERR(NULL != pCmd, MFX_ERR_MEMORY_ALLOC);

        FrameBasedCommandActivator *pActivator = new FrameBasedCommandActivator (pCmd, this, true);
        MFX_CHECK_WITH_ERR(NULL != pActivator, MFX_ERR_MEMORY_ALLOC);

        pActivator->SetExecuteFrameNumber(nSplitFrames);
        m_commands.push_back(pActivator);
        m_inParams.bUseSeparateFileWriter = true;
        }
    else if ( 0!= (nPattern1 = m_OptProc.Check(argv[0], VM_STRING("--output|-OutputFile|-o( |:multi)"), VM_STRING("output file. If Multiple flag specified new file will be created for each view"), OPT_FILENAME)) ||
              0!= (nPattern  = m_OptProc.Check(argv[0], VM_STRING("-o:(bmp|")VM_STRING(MFX_FOURCC_PATTERN())VM_STRING(")"), VM_STRING("output file in NV12 ,YV12, RGB32 color format. Default is YV12"), OPT_FILENAME)))
    {
        m_inParams.bMultiFiles = nPattern1 == 3;///-o:multiple is 3rd case

        if (MFX_ERR_NONE != GetMFXFrameInfoFromFOURCCPatternIdx(nPattern - 1, m_inParams.outFrameInfo))
        {
            switch (nPattern)
            {
            case 1:
                {
                    m_inParams.outFrameInfo.FourCC = MFX_FOURCC_RGB4;
                    m_inParams.outFrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
                    m_RenderType = MFX_BMP_RENDER;
                    break;
                }
            }
        }

        if(1 + argv != argvEnd && (argv+1)[0][0] != '-' ){
            argv++;

            MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.strDstFile, MFX_ARRAY_SIZE(m_inParams.strDstFile), argv[0]));

            if (m_RenderType != MFX_METRIC_CHECK_RENDER&&
                m_RenderType != MFX_ENC_RENDER &&
                m_RenderType != MFX_OUTLINE_RENDER&&
                m_RenderType != MFX_BMP_RENDER)
            {
                m_RenderType = MFX_FW_RENDER;
            }

        }
        else {
            m_RenderType = MFX_SCREEN_RENDER;

        }
    }
    else HANDLE_BOOL_OPTION(m_inParams.bFullscreen,      VM_STRING("-fullscreen"),      VM_STRING("Render in fullscreen"));
    else HANDLE_BOOL_OPTION(m_inParams.bFadeBackground,  VM_STRING("-fade_background"), VM_STRING("Make backgrond black"));
    else if (m_OptProc.Check(argv[0], VM_STRING("-sys|-swfrbuf"), VM_STRING("video frames in system memory")))
    {
        std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_bufType, (int)MFX_BUF_SW));
    }
#ifdef D3D_SURFACES_SUPPORT
    else if (m_OptProc.Check(argv[0], VM_STRING("-d3d|-hwfrbuf"), VM_STRING("video frames in D3D surfaces")))
    {
        std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_bufType, MFX_BUF_HW));
    }
#endif
#ifdef LIBVA_SUPPORT
    else if (m_OptProc.Check(argv[0], VM_STRING("-d3d|-hwfrbuf"), VM_STRING("video frames in D3D surfaces")))
    {
        std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_bufType, MFX_BUF_HW));
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-lva|-hwfrbuf"), VM_STRING("video frames in LVA surfaces")))
    {
        std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_bufType, MFX_BUF_HW));
    }
#endif
    else if (m_OptProc.Check(argv[0], VM_STRING("-d3d11"), VM_STRING("SW Mediasdk library will use external memory as d3d11. HW library additionaly will uses d3d11 interfaces internally")))
    {
        std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_libType, (int)MFX_IMPL_VIA_D3D11));
    }
    else if (0 != (nPattern = m_OptProc.Check(argv[0], VM_STRING("-(d|dec:d|enc:d)3d11:single_texture"), VM_STRING("d3d11 allocator will use single texture model, instead of single subresource model for particular memory pool"))))
    {
        std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_libType, (int)MFX_IMPL_VIA_D3D11));
        switch (nPattern)
        {
        case 1:
            m_components[eDEC].m_bD3D11SingeTexture = true;
            break;
        case 2:
            m_components[eREN].m_bD3D11SingeTexture = true;
            break;
        default:
            std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_bD3D11SingeTexture, true));
        }
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-d3d9_feat"), VM_STRING("SW Mediasdk library will use d3d11 device with DX9 feature levels")))
    {
        std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_bD39Feat, true));
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-opaq"), VM_STRING("Mediasdk will decide target memory for video frames. It is D3D or system memory")))
    {
        std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_bufType, MFX_BUF_OPAQ));
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-NumberFrames|-n|--frames"), VM_STRING("number frames to process"), OPT_UINT_32))
    {
        MFX_CHECK(1 + argv != argvEnd);
        argv++;
        MFX_PARSE_INT(m_inParams.nFrames,argv[0]);
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-width|-w"), VM_STRING("video width,  forces to treat InputFile as YUV, in specific color format specified by -i option, default is YUV420"), OPT_UINT_32))
    {
        MFX_CHECK(1 + argv != argvEnd);
        MFX_PARSE_INT(m_inParams.FrameInfo.Width, argv[1]);
        m_YUV_Width = m_inParams.FrameInfo.Width;
        argv++;
        m_inParams.bYuvReaderMode = true;
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-height|-h"), VM_STRING("video height, forces to treat InputFile as YUV, in specific color format specified by -i option, default is YUV420"), OPT_UINT_32))
    {
        MFX_CHECK(1 + argv != argvEnd);
        MFX_PARSE_INT(m_inParams.FrameInfo.Height, argv[1]);
        m_YUV_Height = m_inParams.FrameInfo.Height;
        argv++;
        m_inParams.bYuvReaderMode = true;
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-async"), VM_STRING("maximum number of asynchronious tasks"), OPT_INT_32))
    {
        MFX_CHECK(1 + argv != argvEnd);
        mfxU16 asyncLevel = 0;
        MFX_PARSE_INT(asyncLevel, argv[1]);
        std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_nMaxAsync, asyncLevel));
        argv++;
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-backbuffer"), VM_STRING("backbuffer format for Directx9. Supported formats:\n\t\tD3DFMT_X8R8G8B8 - default\n\t\tD3DFMT_A8R8G8B8\n\t\tD3DFMT_A2R10G10B10"), OPT_FILENAME))
    {
        MFX_CHECK(1 + argv != argvEnd);
        vm_string_strcpy_s(m_inParams.BackBufferFormat, MFX_ARRAY_SIZE(m_inParams.BackBufferFormat), argv[1]);
        argv++;
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-backbuffer_num"), VM_STRING("Number of backbuffers to be used for Directx9-based rendering. Default is 24."), OPT_INT_32))
    {
        MFX_CHECK(1 + argv != argvEnd);

        MFX_PARSE_INT(m_inParams.m_nBackbufferCount, argv[1]);
        argv++;
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-overlay_text"), VM_STRING("Text to be overlayed on rendering screen. DirectX9 only."), OPT_INT_32))
    {
        MFX_CHECK(1 + argv != argvEnd);

        vm_string_strcpy_s(m_inParams.OverlayText, MFX_ARRAY_SIZE(m_inParams.OverlayText), argv[1]);
        if ( ! m_inParams.OverlayTextSize )
            m_inParams.OverlayTextSize = 18;
        argv++;
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-overlay_font_size"), VM_STRING("Size of the overlaying text font. Default is 18"), OPT_INT_32))
    {
        MFX_CHECK(1 + argv != argvEnd);

        MFX_PARSE_INT(m_inParams.OverlayTextSize, argv[1]);
        argv++;
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-NumThread|-t|--threads"), VM_STRING("number of threads"), OPT_INT_32))
    {
        MFX_CHECK(1 + argv != argvEnd);

        mfxU16 nThreads = 0;
        MFX_PARSE_INT(nThreads, argv[1]);
        std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_NumThread, nThreads));

        //SET_GLOBAL_PARAM(m_NumThread, (mfxU16)vm_string_atoi(argv[1]));
        argv++;
    }
#ifdef D3D_SURFACES_SUPPORT
    else if (m_OptProc.Check(argv[0], VM_STRING("-dump"), VM_STRING("dump DXVA2 VLD decode buffers into text file"), OPT_FILENAME))
    {
        MFX_CHECK(1 + argv != argvEnd);
        m_inParams.bDXVA2Dump = true;
        argv++;
        { // convert vm_char to char
            char dir[MAX_PATH];
            char *pDir = dir;
#ifdef UNICODE
            wcstombs(dir, argv[0], 1 + vm_string_strlen(argv[0]));
#else
            pDir = argv[0];
#endif
            SetDumpingDirectory(pDir);
        }
    }
#endif // #ifdef D3D_SURFACES_SUPPORT
    else if (m_OptProc.Check(argv[0], VM_STRING("-dll"), VM_STRING("load specific library instead of dxva2.dll (for LRB Windows emulator)"), OPT_FILENAME))
    {
        MFX_CHECK(1 + argv != argvEnd);
        argv++;
        if (sizeof(void*) == 8) // this option is used for dxvaencode_emu.dll which available in 64-bit version only
        {
            MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.dxva2DllName, MFX_ARRAY_SIZE(m_inParams.dxva2DllName), argv[0]));
        }
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-created3d"), VM_STRING("force to create D3D device manager in application and pass to MediaSDK")))
    {
        m_inParams.bCreateD3D = true;
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-dec:noExtPicstruct"), VM_STRING("prevents decoder from generating extended picstruct codes")))
    {
        m_inParams.bNoExtPicstruct = true;
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-vpp:enable"), VM_STRING("enable VPP")))
    {
        m_inParams.bUseVPP = true;
    }
    else if (m_OptProc.Check(argv[0], VM_STRING("-dec:sw"), VM_STRING("Use MediaSDK SW library for decode")) ||
        m_OptProc.Check(argv[0], VM_STRING("-vpp:sw"), VM_STRING("Use MediaSDK SW library for post/pre-processing")) ||
        m_OptProc.Check(argv[0], VM_STRING("-enc:sw"), VM_STRING("Use MediaSDK SW library for encode")) ||
        m_OptProc.Check(argv[0], VM_STRING("-dec:hw"), VM_STRING("Use MediaSDK HW library for decode")) ||
        m_OptProc.Check(argv[0], VM_STRING("-vpp:hw"), VM_STRING("Use MediaSDK HW library for post/pre-processing")) ||
        m_OptProc.Check(argv[0], VM_STRING("-enc:hw"), VM_STRING("Use MediaSDK HW library for encode")) ||
        m_OptProc.Check(argv[0], VM_STRING("-dec:hw_strict"), VM_STRING("Ensure HW-accelerated decode (fail otherwise)")) ||
        m_OptProc.Check(argv[0], VM_STRING("-vpp:hw_strict"), VM_STRING("Ensure HW-accelerated post/pre-processing (fail otherwise)")) ||
        m_OptProc.Check(argv[0], VM_STRING("-enc:hw_strict"), VM_STRING("Ensure HW-accelerated encode (fail otherwise)")) ||
        m_OptProc.Check(argv[0], VM_STRING("-dec:async"), VM_STRING("maximum number of Decode asynchronous tasks"), OPT_INT_32) ||
        m_OptProc.Check(argv[0], VM_STRING("-vpp:async"), VM_STRING("maximum number of VPP    asynchronous tasks"), OPT_INT_32) ||
        m_OptProc.Check(argv[0], VM_STRING("-enc:async"), VM_STRING("maximum number of Encode asynchronous tasks"), OPT_INT_32)||
        m_OptProc.Check(argv[0], VM_STRING("-(spl|dec|vpp|enc):pts"), VM_STRING("prints component timestamps"))||
        m_OptProc.Check(argv[0], VM_STRING("-dec:fr"), VM_STRING("specifies constant frame rate for the whole pipeline(equals to -f)"))||
        m_OptProc.Check(argv[0], VM_STRING("-vpp:fr"), VM_STRING("do FRC from framerate of input stream to specified value(equals to -frc)"))||
        m_OptProc.Check(argv[0], VM_STRING("-dec:picstruct"), VM_STRING("overrides picture structure for decoder output surface 0=progressive, 1=tff, 2=bff, 3=field tff, 4=field bff"))||
        m_OptProc.Check(argv[0], VM_STRING("-vpp:picstruct"), VM_STRING("overrides picture structure for VPP output surface 0=progressive, 1=tff, 2=bff, 3=field tff, 4=field bff"))||
        m_OptProc.Check(argv[0], VM_STRING("-dec:async_depth"), VM_STRING("fills mfxVideoParam.AsyncDepth to specified value in decoder.QueryIOSurface request"), OPT_INT_32)||
        m_OptProc.Check(argv[0], VM_STRING("-vpp:async_depth"), VM_STRING("fills mfxVideoParam.AsyncDepth to specified value in vpp.QueryIOSurface request"), OPT_INT_32)||
        m_OptProc.Check(argv[0], VM_STRING("-enc:async_depth"), VM_STRING("fills mfxVideoParam.AsyncDepth to specified value in encoder.QueryIOSurface request"), OPT_INT_32)||
        m_OptProc.Check(argv[0], VM_STRING("-spl:print_frame_type"), VM_STRING("traces frame types for splitted frames")) ||
        m_OptProc.Check(argv[0], VM_STRING("-(dec|vpp|enc):latency"), VM_STRING("prints latency for specific Mediasdk component"), OPT_BOOL)||
        m_OptProc.Check(argv[0], VM_STRING("-(dec|vpp|enc):drop_frames"), VM_STRING("drop n frames in end of interval before corresponding component"), OPT_SPECIAL, VM_STRING("DROP INTERVAL"))||
        (0 != (nPattern = m_OptProc.Check(argv[0],
        VM_STRING("-dec:(")VM_STRING(MFX_FOURCC_PATTERN()VM_STRING(")")),
        VM_STRING("output fourcc, valid only for YUV input"), OPT_INT_32)))||
        (0 != (nPattern = m_OptProc.Check(argv[0],
        VM_STRING("-enc:(")VM_STRING(MFX_FOURCC_PATTERN()VM_STRING(")")),
        VM_STRING("output fourcc, valid only for YUV input"), OPT_INT_32))))
    {

        //ComponentParams *pParam = NULL;
        vm_char *sub_option = argv[0] + 5;
        bool *pbPrintTimeStamp;

        ComponentsContainer::iterator component;
        component = std::find_if(m_components.begin(), m_components.end(),
            std::bind2nd(mem_var_isequal<ComponentParams, tstring>(&ComponentParams::m_ShortName), tstring(argv[0]).substr(1, 3)));

        //component = std::find_if(m_components.begin(), m_components.end(), std::bind2nd(NameEqual<ComponentParams>(), tstring(argv[0]+ 1).substr(0,3)));
        //GetComponentParams(pParam, argv[0][1]);

        if (component == m_components.end()  /*'s' == argv[0][1]*/)
        {
            pbPrintTimeStamp = &m_inParams.bPrintSplTimeStamps;
        }else
        {
            pbPrintTimeStamp = &component->m_bPrintTimeStamps;
        }

        if (!vm_string_stricmp(sub_option, VM_STRING("picstruct")))
        {
            MFX_CHECK(1 + argv != argvEnd);
            argv++;

            component->m_bOverPS = true;
            mfxI32 nCmdPS;
            MFX_PARSE_INT(nCmdPS, argv[0]);
            component->m_nOverPS = Convert_CmdPS_to_MFXPS(nCmdPS);
            component->m_extCO   = Convert_CmdPS_to_ExtCO(nCmdPS);
        }
        else if (!vm_string_stricmp(sub_option, VM_STRING("sw")))
        {
            component->m_libType = MFX_IMPL_SOFTWARE;
        }
        else if (!vm_string_stricmp(sub_option, VM_STRING("hw")))
        {
            component->m_libType = MFX_IMPL_HARDWARE;
        }
        else if (!vm_string_stricmp(sub_option, VM_STRING("hw_strict")))
        {
            component->m_libType = MFX_IMPL_HARDWARE;
            component->m_bHWStrict = true;
        }
        else if (!vm_string_stricmp(sub_option, VM_STRING("async")))
        {
            MFX_CHECK(1 + argv != argvEnd);
            argv++;
            component->m_nMaxAsync = (mfxU16)vm_string_atoi(argv[0]);
        }
        else if (!vm_string_stricmp(sub_option, VM_STRING("pts")))
        {
            *pbPrintTimeStamp = true;
            PipelineSetFrameTrace(false);
        }
        else if (!vm_string_stricmp(sub_option, VM_STRING("fr")))
        {
            MFX_CHECK(1 + argv != argvEnd);
            argv++;
            MFX_PARSE_DOUBLE(component->m_fFrameRate, argv[0]);
            if (0 == component->m_fFrameRate)
                component->m_bFrameRateUnknown = true;
        }
        else if (!vm_string_stricmp(sub_option, VM_STRING("async_depth")))
        {
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(component->m_params.AsyncDepth, argv[1]);
            argv++;
        }else if (!vm_string_stricmp(sub_option, VM_STRING("latency")))
        {
            component->m_bCalcLatency = true;
        }else if (!vm_string_stricmp(sub_option, VM_STRING("drop_frames")))
        {
            MFX_CHECK(2 + argv < argvEnd);
            MFX_PARSE_INT(component->m_nDropCount, argv[1]);
            MFX_PARSE_INT(component->m_nDropCyle, argv[2]);
            argv += 2;
        }
        else if (nPattern)
        {
            mfxFrameInfo info;
            if (MFX_ERR_NONE == GetMFXFrameInfoFromFOURCCPatternIdx(nPattern, info))
            {
                component->m_params.mfx.FrameInfo.FourCC       = info.FourCC;
                component->m_params.mfx.FrameInfo.ChromaFormat = info.ChromaFormat;
            }
        }
    }
    else HANDLE_64F_OPTION(m_components[eDEC].m_fFrameRate, VM_STRING("-FrameRate|-f|--fps"), VM_STRING("video framerate (frames per second), used for encoding from yuv files"))
        else HANDLE_64F_OPTION(m_components[eVPP].m_fFrameRate, VM_STRING("-frc"), VM_STRING("do FRC from framerate of input stream to specified value"))
        else HANDLE_INT_OPTION(m_inParams.nAdvanceFRCAlgorithm, VM_STRING("-afrc"), VM_STRING("specifies algorithm for advanced frc(1=PRESERVE_TIMESTAMP, 2=DISTRIBUTED_TIMESTAMP"))
        else HANDLE_INT_OPTION(m_inParams.nDecodeInAdvance, VM_STRING("-dec:advance"), VM_STRING("decode specified number frames in advance before passing to VPP/Encode"))
        else if (0 != (nPattern  = m_OptProc.Check(argv[0], VM_STRING("-(dec|vpp|enc):sys"),  VM_STRING("Use system memory for decoder,or VPP output frames"))) ||
        0 != (nPattern1 = m_OptProc.Check(argv[0], VM_STRING("-(dec|vpp|enc):d3d"),  VM_STRING("Use D3D for decoder,or VPP output frames"))) ||
        0 != (nPattern2 = m_OptProc.Check(argv[0], VM_STRING("-(dec|vpp|enc):d3d11"),  VM_STRING("Use D3D11 for decoder,or VPP output frames"))) ||
        0 != (nPattern3 = m_OptProc.Check(argv[0], VM_STRING("-(dec|vpp|enc):opaq"), VM_STRING("Mediasdk selects prefered memory for decoder,or VPP output frames"))))
        {
            // encoder output frames are always system memory

            MFXBufType bType = MFX_BUF_UNSPECIFIED;
            bType = nPattern  ? MFX_BUF_SW      : bType;
            bType = nPattern1 ? MFX_BUF_HW      : bType;
            bType = nPattern2 ? MFX_BUF_HW_DX11 : bType;
            bType = nPattern3 ? MFX_BUF_OPAQ    : bType;

            ComponentsContainer::iterator component;
            component = std::find_if(m_components.begin(), m_components.end(),
                std::bind2nd(mem_var_isequal<ComponentParams, tstring>(&ComponentParams::m_ShortName), tstring(argv[0]+ 1).substr(0, 3)));

            /*ComponentParams *pParam = NULL;
            GetComponentParams(pParam, argv[0][1]);*/
            //if (NULL != pParam)
            if (m_components.end() != component)
            {
                component->m_bufType = bType;
                //  pParam->m_bExternalAlloc = false;
            }
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-nv12"), VM_STRING("use surfaces with nv12 color format")))
        {
            m_inParams.FrameInfo.FourCC = MFX_FOURCC_NV12;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-yv12"), VM_STRING("use surfaces with yv12 color format")))
        {
            m_inParams.FrameInfo.FourCC = MFX_FOURCC_YV12;
        }
        else HANDLE_INT_OPTION(m_inParams.FrameInfo.CropX, VM_STRING("-cropx"), VM_STRING("X coordinate of crop rectangle"))
        else HANDLE_INT_OPTION(m_inParams.FrameInfo.CropY,VM_STRING("-cropy"), VM_STRING("Y coordinate of crop rectangle"))
        else HANDLE_INT_OPTION(nForDeprecatedParams,VM_STRING("-cropw"), VM_STRING("[DEPRECATED] width of crop rectangle"))
        else HANDLE_INT_OPTION(nForDeprecatedParams,VM_STRING("-croph"), VM_STRING("[DEPRECATED] height of crop rectangle"))
        else if (m_OptProc.Check(argv[0], VM_STRING("-extalloc"), VM_STRING("use external memory allocator (turned on automatically if D3D surfaces)")))
        {
            std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_bExternalAlloc, true));
            //SET_GLOBAL_PARAM(m_bExternalAlloc, true);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-showframes|-v"), VM_STRING("verbose mode")))
        {
            m_inParams.bVerbose = true;
            //PipelineSetFrameTrace(false);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-perf|-p"), VM_STRING("(deprecated) append performance results to file"), OPT_FILENAME))
        {
            MFX_CHECK(1 + argv != argvEnd);
            MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.perfFile, MFX_ARRAY_SIZE(m_inParams.perfFile), argv[1]));
            argv++;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-plugin"), VM_STRING("load plugin from DLL and insert into pipeline"), OPT_FILENAME))
        {
            MFX_CHECK(1 + argv != argvEnd);
            MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.pPluginDLL, MFX_ARRAY_SIZE(m_inParams.pPluginDLL), argv[1]));
            argv++;
            m_inParams.bUseVPP = true;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-plugin:param"), VM_STRING("load plugin parameters from file"), OPT_FILENAME))
        {
            MFX_CHECK(1 + argv != argvEnd);
            MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.pPluginParamFile, MFX_ARRAY_SIZE(m_inParams.pPluginParamFile), argv[1]));
            argv++;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-plugin:rgb32"), VM_STRING("plugin uses rgb32 color format")))
        {
            m_components[ePLUGIN].m_params.mfx.FrameInfo.FourCC = MFX_FOURCC_RGB4;
        }
        else if (0 != (nPattern = m_OptProc.Check(argv[0], VM_STRING("-plugin:(w|h)"), VM_STRING("plugin works on surfaces with specific resolution, pre and post processing used if necessary"))))
        {
            MFX_CHECK(1 + argv != argvEnd);
            switch (nPattern)
            {
            case 1:
                MFX_PARSE_INT(m_components[ePLUGIN].m_params.vpp.In.CropW, argv[1]);
                MFX_PARSE_INT(m_components[ePLUGIN].m_params.vpp.Out.CropW, argv[1]);
                break;
            case 2:
                MFX_PARSE_INT(m_components[ePLUGIN].m_params.vpp.In.CropH, argv[1]);
                MFX_PARSE_INT(m_components[ePLUGIN].m_params.vpp.Out.CropH, argv[1]);
                break;
            default:
                MFX_CHECK(nPattern == 1 || nPattern == 2);
            }
            argv++;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-cmp"), VM_STRING("compare with reference file"), OPT_FILENAME))
        {
            MFX_CHECK(1 + argv != argvEnd);
            MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.refFile, MFX_ARRAY_SIZE(m_inParams.refFile), argv[1]));
            argv++;
            m_RenderType = MFX_METRIC_CHECK_RENDER;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-delta"), VM_STRING("specifies maximum delta, used in combination with -cmp only"), OPT_64F) ||
            m_OptProc.Check(argv[0], VM_STRING("-ssim"), VM_STRING("calculates every frame SSIM. If MIN exist compares with it, used in combination with -cmp only"), OPT_64F) ||
            m_OptProc.Check(argv[0], VM_STRING("-psnr"), VM_STRING("calculates every frame PSNR. If MIN exist compares with it, used in combination with -cmp only"), OPT_64F))
        {
            MetricType type = (argv[0][1] == 'd') ? METRIC_DELTA : ((argv[0][1] == 's') ? METRIC_SSIM : METRIC_PSNR);
            mfxF64 metricVal;
            if (1 + argv != argvEnd)
            {
                if(1 == vm_string_sscanf(argv[1], VM_STRING("%lf"), &metricVal))
                    argv++;
                else
                    metricVal = -1.0;
            } else
            {
                metricVal = -1.0;
            }

            //one more optional parameter
            if (1 + argv < argvEnd && VM_STRING('-') != *argv[1])
            {
                argv++;
                MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.perfFile, MFX_ARRAY_SIZE(m_inParams.perfFile), argv[0]));
            }

            m_metrics.push_back(pair<MetricType, mfxF64>(type, metricVal));
            m_RenderType = MFX_METRIC_CHECK_RENDER;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-crc"), VM_STRING("write four-byte CRC to file"), OPT_FILENAME))
        {
            m_inParams.bCalcCRC = true;

            if (1 + argv < argvEnd && *argv[1] != '-')
            {
                argv++;
                MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.crcFile, MFX_ARRAY_SIZE(m_inParams.crcFile), argv[0]));
            }
        }
        else if (!vm_string_stricmp(argv[0], VM_STRING("-fw"))) // deprecated
        {
            m_RenderType = MFX_FW_RENDER;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-no_render"), VM_STRING("disable rendering(doesn't create render object at all)")))
        {
            if (m_RenderType != MFX_ENC_RENDER &&
                m_RenderType != MFX_ENCDEC_RENDER)
                m_RenderType = MFX_NO_RENDER;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-null_render"), VM_STRING("disable rendering(creates null renderer)")))
        {
            if (m_RenderType != MFX_ENC_RENDER &&
                m_RenderType != MFX_ENCDEC_RENDER)
                m_RenderType = MFX_NULL_RENDER;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-outline"), VM_STRING("creates outline writer, if file_name specified outliner will use this filename instead of output filename(-o)"), OPT_SPECIAL, VM_STRING("[filename]")))
        {
            if (1+argv != argvEnd && argv[1][0]!='-')
            {
                //trying to open file
                vm_file * f = vm_file_fopen(argv[1], VM_STRING("w"));
                if (f)
                {
                    MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.strOutlineFile, MFX_ARRAY_SIZE(m_inParams.strOutlineFile), argv[1]));
                    argv++;
                    vm_file_fclose(f);
                }
            }

            m_RenderType = MFX_OUTLINE_RENDER;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-i:outline"), VM_STRING("specify path to outline, that will be used inside yuv reading routine"), OPT_FILENAME))
        {
            MFX_CHECK(1 + argv != argvEnd);
            MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.strOutlineInputFile, MFX_ARRAY_SIZE(m_inParams.strOutlineInputFile), argv[1]));
            argv++;

        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-repeat"), VM_STRING("repeat mode works only with -o. Writes to file on the first iteration and compare on the following)"), OPT_INT_32))
        {
            m_inParams.nRepeat = 0x7fffffff;
            if (1 + argv < argvEnd)
            {
                m_inParams.nRepeat = vm_string_atoi(argv[1]);
                argv++;
            }
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-fps_frame_window"), VM_STRING("Number of frames in window for per-window fps calculation. Default is 30."), OPT_INT_32))
        {
            if (1 + argv < argvEnd)
            {
                m_inParams.fps_frame_window = vm_string_atoi(argv[1]);
                argv++;
            }
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-timeout"), VM_STRING("repeat mode with timeout (TIME is in minutes!)"), OPT_INT_32))
        {
            m_inParams.nRepeat = 0x7fffffff;
            MFX_CHECK(1 + argv != argvEnd);
            m_inParams.nTimeout = 60 * vm_string_atoi(argv[1]);
            argv++;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-res"), VM_STRING("generates file with stream info"), OPT_FILENAME))
        {
            MFX_CHECK(1 + argv != argvEnd);
            MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.strParFile, MFX_ARRAY_SIZE(m_inParams.strParFile), argv[1]));
            m_bWritePar = true;
            argv++;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-seekframe"), VM_STRING("after achieving frame = FROM, seek to frame number TO, works ony on uncompressed input"), OPT_SPECIAL, VM_STRING("FROM TO")))
        {
            MFX_CHECK(2 + argv < argvEnd);

            seekSourceCommand *pCmd = new seekSourceCommand(this);
            MFX_CHECK_WITH_ERR(NULL != pCmd, MFX_ERR_MEMORY_ALLOC);

            FrameBasedCommandActivator *pActivator = new FrameBasedCommandActivator(pCmd, this);
            MFX_CHECK_WITH_ERR(NULL != pActivator, MFX_ERR_MEMORY_ALLOC);

            mfxU32  uiStartFrame;
            mfxU32  uiSeekFrame;

            MFX_PARSE_INT(uiStartFrame, argv[1]);
            MFX_PARSE_INT(uiSeekFrame, argv[2]);

            pActivator->SetExecuteFrameNumber(uiStartFrame);
            pCmd->SetSeekFrames(uiSeekFrame);

            m_commands.push_back(pActivator);
            //if source file is yuv have to use exact data reader to perform unbuffered seek
            m_inParams.bExactSizeBsReader= true;

            argv += 2;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-numseekframe"), VM_STRING("repeats frames based seeking"), OPT_SPECIAL, VM_STRING("FROM TO NUM")))
        {
            MFX_CHECK(3 + argv < argvEnd);

            mfxU32  uiStartFrame;
            mfxU32  uiSeekFrame;
            mfxU32  nSeek;

            MFX_PARSE_INT(uiStartFrame, argv[1]);
            MFX_PARSE_INT(uiSeekFrame, argv[2]);
            MFX_PARSE_INT(nSeek, argv[3]);

            m_pFactories.push_back(new constnumFactory(nSeek
                , new FrameBasedCommandActivator(new seekSourceCommand(this), this)
                , new seekFrameCmdInitializer(uiStartFrame, uiSeekFrame, m_pRandom)));

            //if source file is yuv have to use exact data reader to perform unbuffered seek
            m_inParams.bExactSizeBsReader= true;

            argv += 3;
        }

        else if (m_OptProc.Check(argv[0], VM_STRING("-seek"), VM_STRING("after achieving position FROM, seek to position TO"), OPT_SPECIAL, VM_STRING("FROM TO")))
        {
            MFX_CHECK(2 + argv < argvEnd);

            seekCommand *pCmd = new seekCommand(this);
            MFX_CHECK_WITH_ERR(NULL != pCmd, MFX_ERR_MEMORY_ALLOC);

            PTSBasedCommandActivator *pActivator = new PTSBasedCommandActivator (pCmd, this);
            MFX_CHECK_WITH_ERR(NULL != pActivator, MFX_ERR_MEMORY_ALLOC);

            double  fMaxWarminUp;
            double  fTimePos;

            MFX_PARSE_DOUBLE(fMaxWarminUp, argv[1]);
            MFX_PARSE_DOUBLE(fTimePos, argv[2]);

            pActivator->SetWarmingUpTime(fMaxWarminUp);
            pCmd->SetSeekTime(fTimePos);

            m_commands.push_back(pActivator);

            argv += 2;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-randseek"), VM_STRING("performs seeks to random positions, MAXFROM - maximum FROM, NUM - number of iterations"), OPT_SPECIAL, VM_STRING("MAXFROM NUM")))
        {
            MFX_CHECK(2 + argv < argvEnd);

            double fMaxWarminUp;
            mfxU32 nSeek;

            MFX_PARSE_DOUBLE(fMaxWarminUp, argv[1]);
            MFX_PARSE_INT(nSeek, argv[2]);

            m_pFactories.push_back(new constnumFactory(nSeek
                , new PTSBasedCommandActivator(new seekCommand(this), this)
                , new randSeekCmdInitializer(fMaxWarminUp, 0, m_pRandom)));

            argv += 2;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-numseek"), VM_STRING("performs seeks to particular position (intended for pure streams) MAXFROM - maximum FROM, TO - seek position, NUM - number of iterations"), OPT_SPECIAL, VM_STRING("MAXFROM TO NUM")))
        {
            MFX_CHECK(3 + argv < argvEnd);

            double  fMaxWarminUp;
            double  fTimePos;
            mfxU32  nSeek;

            MFX_PARSE_DOUBLE(fMaxWarminUp, argv[1]);
            MFX_PARSE_DOUBLE(fTimePos, argv[2]);
            MFX_PARSE_INT(nSeek, argv[3]);

            m_pFactories.push_back(new constnumFactory(nSeek
                , new PTSBasedCommandActivator(new seekCommand(this), this)
                , new randActivationInitializer(fMaxWarminUp, fTimePos, 0, m_pRandom)));

            argv +=3;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-numsourceseek"), VM_STRING("performs seeking (similar to -numseek options) on splitter level only, downstream components : decoder, render, etc., continue to work in normal mode"), OPT_SPECIAL, VM_STRING("FROM TO NUM")))
        {
            MFX_CHECK(3 + argv < argvEnd);

            double  fMaxWarminUp;
            double  fTimePos;
            mfxU32  nSeek;

            MFX_PARSE_DOUBLE(fMaxWarminUp, argv[1]);
            MFX_PARSE_DOUBLE(fTimePos, argv[2]);
            MFX_PARSE_INT(nSeek, argv[3]);

            m_pFactories.push_back(new constnumFactory(nSeek
                , new PTSBasedCommandActivator(new seekSourceCommand(this), this)
                , new randActivationInitializer(fMaxWarminUp, fTimePos, 0, m_pRandom)));

            argv += 3;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-skip"), VM_STRING("after achiving position FROM, set SetSkipMode to DEPTH"), OPT_SPECIAL, VM_STRING("FROM DEPT")))
        {
            MFX_CHECK(2 + argv < argvEnd);

            skipCommand *pCmd = new skipCommand(this);
            MFX_CHECK_WITH_ERR(NULL != pCmd, MFX_ERR_MEMORY_ALLOC);

            PTSBasedCommandActivator *pActivator = new PTSBasedCommandActivator (pCmd, this);
            MFX_CHECK_WITH_ERR(NULL != pActivator, MFX_ERR_MEMORY_ALLOC);

            double fPts;
            MFX_PARSE_DOUBLE(fPts, argv[1]);
            pActivator->SetWarmingUpTime(fPts);

            mfxI32 nSkipLevel;
            MFX_PARSE_INT(nSkipLevel, argv[2]);
            pCmd->SetSkipLevel(nSkipLevel);

            m_commands.push_back(pActivator);
            argv += 2;
        }
        else if (!vm_string_stricmp(argv[0], VM_STRING("-randskip"))) // undocumented?
        {
            MFX_CHECK(3 + argv < argvEnd);

            double  fMaxWarminUp;
            int     nMaxSkip;
            mfxU32  nSeek;

            MFX_PARSE_DOUBLE(fMaxWarminUp, argv[1]);
            MFX_PARSE_INT(nMaxSkip, argv[2]);
            MFX_PARSE_INT(nSeek, argv[3]);

            argv+=3;

            m_pFactories.push_back(new constnumFactory(nSeek
                , new PTSBasedCommandActivator(new skipCommand(this), this)
                , new randActivationInitializer(fMaxWarminUp, 0.0, nMaxSkip, m_pRandom)));
        }
        else HANDLE_BOOL_OPTION(m_inParams.bUseVPP_ifdi, VM_STRING("-deinterlace"), VM_STRING("insert vpp into pipeline only if deinterlacing required"));
        else HANDLE_BOOL_OPTION(m_inParams.bUpdateWindowTitle, VM_STRING("-update_window_title"), VM_STRING("Update window title with progress information"));
        else HANDLE_BOOL_OPTION(m_inParams.bExtendedFpsStat,   VM_STRING("-extended_fps_stat"), VM_STRING("Print extended fps information at the end"));
        else HANDLE_BOOL_OPTION(m_inParams.bExtVppApi,         VM_STRING("-ext_vpp_api"), VM_STRING("Use RunFrameVPPAsyncEx for VPP operations"));
        else HANDLE_INT_OPTION(m_components[eVPP].m_params.vpp.Out.CropW, VM_STRING("-resize_w"), VM_STRING("video width at VPP output"))
        else HANDLE_INT_OPTION(m_components[eVPP].m_params.vpp.Out.CropH, VM_STRING("-resize_h"), VM_STRING("video height at VPP output"))
        else HANDLE_64F_OPTION(m_components[eVPP].m_zoomx, VM_STRING("-zoom_w"), VM_STRING("scale video width"))
        else HANDLE_64F_OPTION(m_components[eVPP].m_zoomx, VM_STRING("-zoom_h"), VM_STRING("scale video height"))
        else HANDLE_64F_OPTION(m_components[eVPP].m_zoomx = m_components[eVPP].m_zoomy, VM_STRING("-zoom"), VM_STRING("scale video (both width and height"))
        else HANDLE_BOOL_OPTION(m_inParams.bSceneAnalyzer, VM_STRING("-sceneanalyzer"), VM_STRING("enable scene analyzer"));
        else HANDLE_INT_OPTION_PLUS1(m_inParams.nDenoiseFactorPlus1, VM_STRING("-denoise"), VM_STRING("enable denoising with specified factor (0..100 inclusive)"))
        else HANDLE_INT_OPTION_PLUS1(m_inParams.nDetailFactorPlus1, VM_STRING("-detail"), VM_STRING("enable detail/edge enhancement with specified factor  (0..100 inclusive)"))
        else HANDLE_INT_OPTION(m_inParams.nDecBufSize,VM_STRING("-decbufsize"), VM_STRING("feed decoder with selected buffer size"))
        else HANDLE_INT_OPTION(m_inParams.nSeed, VM_STRING("-seed"), VM_STRING("seed for srand function, if not specified current time is used"))
        else if (m_OptProc.Check(argv[0], VM_STRING("-nodblk"), VM_STRING("disable deblocking in decoder"), OPT_BOOL))
        {
            m_components[eDEC].m_SkipModes.push_back(MFX_SKIP_MODE_NO_DBLK);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-nostsrep"), VM_STRING("disable status reporting in decoder"), OPT_BOOL))
        {
            m_components[eDEC].m_SkipModes.push_back(MFX_SKIP_MODE_NO_STATUS_REP);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-skipmode"), VM_STRING("set specific skip mode in decoder"), OPT_INT_32))
        {
            MFX_CHECK(1 + argv != argvEnd);
            mfxU16 skipmode;
            MFX_PARSE_INT(skipmode, argv[1]);
            m_components[eDEC].m_SkipModes.push_back(skipmode);
            argv++;
        }

        else HANDLE_BOOL_OPTION(m_inParams.DecodedOrder, VM_STRING("-DecodedOrder"), VM_STRING("disable frame reordering in decoder"));
        else HANDLE_BOOL_OPTION(m_inParams.EncodedOrder, VM_STRING("-EncodedOrder"), VM_STRING("disable frame reordering in encoder"));
        else HANDLE_BOOL_OPTION(m_inParams.bUseSeparateFileWriter, VM_STRING("-separate_file"), VM_STRING("separate output files will be created if RESET in the middle happened"));
        else HANDLE_BOOL_OPTION(m_inParams.bSkipUselessOutput,  VM_STRING("--log_off"), VM_STRING("disable logging of dispatcher"));
        else HANDLE_BOOL_OPTION(m_inParams.bOptimizeForPerf,  VM_STRING("-perf_opt|--no-progress"), VM_STRING("optimize pipeline for accurate performance measurement"));
        else HANDLE_INT_OPTION(m_inParams.encodeExtraParams.nDelayOnMSDKCalls, VM_STRING("-delay"), VM_STRING("Sleep() for specified time (in ms) before calling to any MediaSDK async function"))
        else HANDLE_64F_OPTION(m_inParams.fLimitPipelineFps, VM_STRING("-limit_fps|-fps"), VM_STRING("limit overall fps"))
        else HANDLE_BOOL_OPTION(m_inParams.encodeExtraParams.bZeroBottomStripe, VM_STRING("-zero_stripe"), VM_STRING("force zeroing of stripes in decoded picture"));
        else HANDLE_INT_OPTION(m_inParams.nPriority, VM_STRING("-priority"), VM_STRING("specifies process priority level increment or decrement relatively to normal"))
        else HANDLE_INT_OPTION(m_inParams.nCorruptionLevel, VM_STRING("-corrupt"), VM_STRING("makes input data corrupted, 0x1 - casualbits, 0x2 - packets, 0x4 reorder packets " ))
        else HANDLE_BOOL_OPTION(m_inParams.bJoinSessions,  VM_STRING("-join"), VM_STRING("join SW and HW sessions"));
        else HANDLE_BOOL_OPTION(m_inParams.bCompleteFrame,  VM_STRING("-completeframe"), VM_STRING("try to set bitstream.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME if allowed by other options "));
        else HANDLE_BOOL_OPTION(m_inParams.bNoPipelineSync,  VM_STRING("-no_pipe_sync"), VM_STRING("if no async level specified equals to -async 0, if async level specified skips checks of syncpoints completeness between MSDK components "));
        else HANDLE_BOOL_OPTION(bUnused, VM_STRING("-latency"), VM_STRING("prints latency for Mediasdk components, used in latency analysing suites"))
        {
            std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_bCalcLatency, true));
        }
        else if (0 != (nPattern = m_OptProc.Check(argv[0], VM_STRING("-find_free_surface:(first|oldest|oldest_reverse|random"), VM_STRING("uses particular search algorithm in selecting free surface for processing."), OPT_UNDEFINED)))
        {
            eSurfacesSearchAlgo nAlgo  = USE_FIRST;
            switch (nPattern)
            {
            case 1:
                {
                    nAlgo = USE_FIRST;
                    break;
                }
            case 2:
                {
                    nAlgo = USE_OLDEST_DIRECT;
                    break;
                }
            case 3:
                {
                    nAlgo = USE_OLDEST_REVERSE;
                    break;
                }
            case 4:
                {
                    nAlgo = USE_RANDOM;
                    std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_pRandom, m_pRandom));
                    break;
                }
            }
            std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_nSelectAlgo, nAlgo));
        }
#if defined(_WIN32) || defined(_WIN64)
        else if(m_OptProc.Check(argv[0], VM_STRING("-break"), VM_STRING("call to dbgbreak() immediately at parsing this command ")))
        {
            DebugBreak();
        }
#endif // #if defined(_WIN32) || defined(_WIN64)
        else if (m_OptProc.Check(argv[0], VM_STRING("-async_depth|--frame-threads"), VM_STRING("fills mfxVideoParam.AsyncDepth to specified value in QueryIOSurface request"), OPT_INT_32))
        {
            MFX_CHECK(1 + argv != argvEnd);
            mfxU16 nAsyncDepth;
            MFX_PARSE_INT(nAsyncDepth, argv[1]);
            argv++;

            //todo: how to imporve
            for (size_t i  = 0; i < m_components.size(); i++)
            {
                m_components[i].m_params.AsyncDepth = nAsyncDepth;
            }
            //std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_params.AsyncDepth, nAsyncDepth));
            //SET_GLOBAL_PARAM(m_params.AsyncDepth, nAsyncDepth);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-yuv_loop"), VM_STRING("preload specified number of YUV frames from file and use buffered frames as input in the loop"), OPT_INT_32))
        {
            //SET_GLOBAL_PARAM(m_bExternalAlloc, true);
            std::for_each(m_components.begin(), m_components.end(), mem_var_set(&ComponentParams::m_bExternalAlloc, true));

            MFX_CHECK(1 + argv != argvEnd);
            m_inParams.nYUVLoop = vm_string_atoi(argv[1]);
            argv++;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-in_h263"), VM_STRING("directly specify codec type as h263"), OPT_UNDEFINED))
        {
            m_inParams.InputCodecType = MFX_CODEC_H263;
        } else if (m_OptProc.Check(argv[0], VM_STRING("-in_h264"), VM_STRING("directly specify codec type as h264"), OPT_UNDEFINED))
        {
            m_inParams.InputCodecType = MFX_CODEC_AVC;
            m_inParams.m_container = MFX_CONTAINER_RAW;
        } else if (m_OptProc.Check(argv[0], VM_STRING("-in_mpeg2"), VM_STRING("directly specify codec type as mpeg2"), OPT_UNDEFINED))
        {
            m_inParams.InputCodecType = MFX_CODEC_MPEG2;
            m_inParams.m_container = MFX_CONTAINER_RAW;
        } else if (m_OptProc.Check(argv[0], VM_STRING("-in_vc1"), VM_STRING("directly specify codec type as vc1"), OPT_UNDEFINED))
        {
            m_inParams.InputCodecType = MFX_CODEC_VC1;
            m_inParams.m_container = MFX_CONTAINER_RAW;
        } else if (m_OptProc.Check(argv[0], VM_STRING("-in_jpeg"), VM_STRING("directly specify codec type as jpeg"), OPT_UNDEFINED))
        {
            m_inParams.InputCodecType = MFX_CODEC_JPEG;
            m_inParams.m_container = MFX_CONTAINER_RAW;
        } else if (m_OptProc.Check(argv[0], VM_STRING("-in_vp8"), VM_STRING("directly specify codec type as vp8"), OPT_UNDEFINED))
        {
#ifndef MFX_PIPELINE_SUPPORT_VP8
            MFX_TRACE_AND_EXIT(MFX_ERR_UNSUPPORTED, (VM_STRING("vp8 decoding not supported")));
#else
            m_inParams.InputCodecType = MFX_CODEC_VP8;
#endif
        }
        else
        {
            bUnhandled = true;
        }


        //MS compiller limit of nested cycles - have to separate options
        //http://support.microsoft.com/kb/315481
        if (bUnhandled)
        {
            HANDLE_64F_OPTION_AND_FLAG(m_inParams.m_ProcAmp.Brightness, VM_STRING("-brightness"), VM_STRING("enable ProcAmp filter with specified brightness (from -100.0f to 100.0f by 0.01f, default = 0.0f)"),m_inParams.bUseProcAmp)
        else HANDLE_64F_OPTION_AND_FLAG(m_inParams.m_ProcAmp.Contrast, VM_STRING("-contrast"), VM_STRING("enable ProcAmp filter with specified contrast (from 0.0f to 10.0f by 0.01f, default = 1.0f)"),m_inParams.bUseProcAmp)
            else HANDLE_64F_OPTION_AND_FLAG(m_inParams.m_ProcAmp.Hue, VM_STRING("-hue"), VM_STRING("enable ProcAmp filter with specified hue (from -180.0f to 180.0f by 0.1f, default = 0.0f)"),m_inParams.bUseProcAmp)
            else HANDLE_64F_OPTION_AND_FLAG(m_inParams.m_ProcAmp.Saturation, VM_STRING("-saturation"), VM_STRING("enable ProcAmp filter with specified saturation (from 0.0f to 10.0f by 0.01f, default = 1.0f)"),m_inParams.bUseProcAmp)
            else HANDLE_BOOL_OPTION(m_inParams.bUseCameraPipePadding,          VM_STRING("-camera_padding"), VM_STRING("provide camera pipe padding exttended buffer"));
            else HANDLE_INT_OPTION(m_inParams.m_WallW,VM_STRING("-wall_w"), VM_STRING("width of video wall (several windows without overlapping"))
            else HANDLE_INT_OPTION(m_inParams.m_WallH,VM_STRING("-wall_h"), VM_STRING("height of video wall (several windows without overlapping"))
            else HANDLE_INT_OPTION(m_inParams.m_WallN,VM_STRING("-wall_n"), VM_STRING("number of current window in video wall (several windows without overlapping"))
            else HANDLE_SPECIAL_OPTION(m_inParams.m_directRect, VM_STRING("-window"), VM_STRING("specify direct target windop position "), OPT_SPECIAL, VM_STRING("LEFT TOP WIDTH HEIGHT"))
            else HANDLE_BOOL_OPTION(m_inParams.m_bNowWidowHeader,  VM_STRING("-no_window_title"), VM_STRING("creates rendering window without title bar"));
            else HANDLE_INT_OPTION(m_inParams.m_nMonitor, VM_STRING("-monitor"), VM_STRING("monitor identificator on which to create rendering window"))
            else HANDLE_INT_OPTION(m_inParams.nRotation, VM_STRING("-rotation"), VM_STRING("rotate picture clockwise. only for jpeg decoder"))
            else HANDLE_INT_OPTION(m_components[eVPP].m_rotate, VM_STRING("-rotate"), VM_STRING("insert vpp into pipeline only if rotation required"))
            else HANDLE_INT_OPTION(m_components[eVPP].m_mirroring, VM_STRING("-mirror"), VM_STRING("insert vpp mirroring with specified mode into pipeline"))
            else if (m_OptProc.Check(argv[0], VM_STRING("-camera"), VM_STRING("use camera pipe"), OPT_BOOL))
            {
                m_inParams.bUseCameraPipe = true;
                // Disable HMTest behavior that breaks camera pipe
                m_inParams.isHMTest       = false;
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-viewid_from_order"), VM_STRING("maps veiw in input mvc stream, taking it's order in mvcExcSequenceDescription, to the ViewId in output stream, if absent viewIDs will be copied"), OPT_SPECIAL, VM_STRING("from to")))
            {
                MFX_CHECK(2 + argv < argvEnd);
                mfxU16 nInputOrder, nOutViewId;

                MFX_PARSE_INT(nInputOrder, argv[1]);
                MFX_PARSE_INT(nOutViewId, argv[2]);

                m_viewOrderMap.push_back(std::pair<mfxU16, mfxU16>(nInputOrder, nOutViewId));

                //should have uniq input, and uniq output
                MFX_CHECK(!HasDuplicates(m_viewOrderMap.begin(), m_viewOrderMap.end(), CMPPairs1st<mfxU16>()));
                MFX_CHECK(!HasDuplicates(m_viewOrderMap.begin(), m_viewOrderMap.end(), CMPPairs2nd<mfxU16>()));

                argv += 2;
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-dependency"), VM_STRING("mfxMVCViewDependency structure description - used for encoding from YUV file"), OPT_SPECIAL
                , VM_STRING("ViewId NumAnchorRefsL0 NumAnchorRefsL1 NumNonAnchorRefsL0 NumNonAnchorRefsL1 [arrays]")))
            {
                DeSerializeHelper<mfxMVCViewDependency>  viewDependency;
                MFX_CHECK(viewDependency.DeSerialize(++argv, argvEnd));

                if (!m_InputExtBuffers.get_by_id(BufferIdOf<mfxExtMVCSeqDesc>::id))
                {
                    m_InputExtBuffers.push_back(new mfxExtMVCSeqDesc());
                    m_inParams.NumExtParam = (mfxU16)m_InputExtBuffers.size();
                    m_inParams.ExtParam    = &m_InputExtBuffers;//vector is stored linear in memory
                }

                MFXExtBufferPtr<mfxExtMVCSeqDesc> seqDesc(m_InputExtBuffers);

                seqDesc->View.push_back(viewDependency);
            } else if (m_OptProc.Check(argv[0], VM_STRING("-dec:burst"), VM_STRING("decode several frames at max speed then render it at render speed"), OPT_INT_32))                  \
            {
                MFX_CHECK(1 + argv != argvEnd);
                MFX_PARSE_INT(m_inParams.nBurstDecodeFrames, argv[1])
                    argv++;
            }
            else if (0!=m_OptProc.Check(argv[0], VM_STRING("-dec:prefer_nv12"), VM_STRING("in case of HEVC Main10 profile use NV12 surface if BitdepthLuma and BitDepthChroma equals 8."), OPT_UNDEFINED))
            {
                m_inParams.isPreferNV12 = true;
            }
            else if (0!=(nPattern = m_OptProc.Check(argv[0], VM_STRING("-i:webm"), VM_STRING("input stream is WebM container pipeline works with demuxing"), OPT_UNDEFINED)))
            {
                MFX_CHECK(1 + argv != argvEnd);
                argv++;
                vm_string_strcpy_s(m_inParams.strSrcFile, MFX_ARRAY_SIZE(m_inParams.strSrcFile), argv[0]);
                // WebM re-uses MKV reader
                m_inParams.m_container = MFX_CONTAINER_MKV;
            }
            else if (0!=(nPattern = m_OptProc.Check(argv[0], VM_STRING("-i:bg16"), VM_STRING("input stream is in Bayer BGGR format. For camera pipe."), OPT_UNDEFINED)))
            {
                MFX_CHECK(1 + argv != argvEnd);
                argv++;
                vm_string_strcpy_s(m_inParams.strSrcFile, MFX_ARRAY_SIZE(m_inParams.strSrcFile), argv[0]);
                m_inParams.m_container      = (mfxContainer)MFX_CAM_BAYER_BGGR;
                m_inParams.FrameInfo.FourCC = MFX_FOURCC_R16;
                m_inParams.bYuvReaderMode = true;
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-input-res|--input-res"), VM_STRING("Source picture size [wxh]"), OPT_UINT_32))
            {
                MFX_CHECK(1 + argv != argvEnd);
                MFX_PARSE_INT(m_inParams.FrameInfo.Width, argv[1]);

                vm_string_sscanf(argv[1], VM_STRING("%dx%d"), reinterpret_cast<int*>(&m_inParams.FrameInfo.Width), reinterpret_cast<int*>(&m_inParams.FrameInfo.Height));
                argv++;
                m_inParams.bYuvReaderMode = true;
            }
            else if (0!=(nPattern = m_OptProc.Check(argv[0], VM_STRING("-i:gr16"), VM_STRING("input stream is in Bayer BGGR format. For camera pipe."), OPT_UNDEFINED)))
            {
                MFX_CHECK(1 + argv != argvEnd);
                argv++;
                vm_string_strcpy_s(m_inParams.strSrcFile, MFX_ARRAY_SIZE(m_inParams.strSrcFile), argv[0]);
                m_inParams.m_container      = (mfxContainer)MFX_CAM_BAYER_GRBG;
                m_inParams.FrameInfo.FourCC = MFX_FOURCC_R16;
                m_inParams.bYuvReaderMode = true;
                m_inParams.nInputBitdepth = (8 == m_inParams.nInputBitdepth) ? 10 : m_inParams.nInputBitdepth;
            }
            else if (0!=(nPattern = m_OptProc.Check(argv[0], VM_STRING("-i:gb16"), VM_STRING("input stream is in Bayer BGGR format. For camera pipe."), OPT_UNDEFINED)))
            {
                MFX_CHECK(1 + argv != argvEnd);
                argv++;
                vm_string_strcpy_s(m_inParams.strSrcFile, MFX_ARRAY_SIZE(m_inParams.strSrcFile), argv[0]);
                m_inParams.m_container      = (mfxContainer)MFX_CAM_BAYER_GBRG;
                m_inParams.FrameInfo.FourCC = MFX_FOURCC_R16;
                m_inParams.bYuvReaderMode = true;
                m_inParams.nInputBitdepth = (8 == m_inParams.nInputBitdepth) ? 10 : m_inParams.nInputBitdepth;
            }
            else if (0!=(nPattern = m_OptProc.Check(argv[0], VM_STRING("-i:rg16"), VM_STRING("input stream is in Bayer BGGR format. For camera pipe."), OPT_UNDEFINED)))
            {
                MFX_CHECK(1 + argv != argvEnd);
                argv++;
                vm_string_strcpy_s(m_inParams.strSrcFile, MFX_ARRAY_SIZE(m_inParams.strSrcFile), argv[0]);
                m_inParams.m_container      = (mfxContainer)MFX_CAM_BAYER_RGGB;
                m_inParams.FrameInfo.FourCC = MFX_FOURCC_R16;
                m_inParams.bYuvReaderMode = true;
                m_inParams.nInputBitdepth = (8 == m_inParams.nInputBitdepth) ? 10 : m_inParams.nInputBitdepth;
            }
            else HANDLE_INT_OPTION(m_inParams.targetViewsTemporalId, VM_STRING("-dec:temporalid"), VM_STRING("in case of MVC->AVC and MVC->MVC transcoding,  specifies coresponding field in mfxExtMVCTargetViews structure"))
            else HANDLE_INT_OPTION(m_inParams.nTestId, VM_STRING("-testid"), VM_STRING("testid value used in SendNotifyMessages(WNDBROADCAST,,testid)"))
            else HANDLE_SPECIAL_OPTION(m_inParams.svc_layer, VM_STRING("-svc_layer"), VM_STRING("specify target svc_layer to decode"), OPT_SPECIAL, VM_STRING("temporalId dependencyId qualityId"))
            else HANDLE_BOOL_OPTION(bUnused,  VM_STRING("-rw_alloc"), VM_STRING("pipeline will create real version of RW allocator instead of DUMMY(should be used in d3d11 mode only) "))
            {
                typedef FactoryChain<LockRWEnabledFrameAllocator, RWAllocatorFactory::root> RWEnabledFactory;
                m_pAllocFactory.reset(new  RWEnabledFactory(m_pAllocFactory.release()));
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-syncop_timeout"), VM_STRING("set generic timemout for any single operation call like, MFXSncOperation()"), OPT_INT_32))
            {
                MFX_CHECK(1 + argv != argvEnd);

                mfxU32 nTimeout = 0;
                MFX_PARSE_INT(nTimeout, argv[1]);

                SetTimeout((int)PIPELINE_TIMEOUT_GENERIC, nTimeout);

                argv++;
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-gpu_copy"), VM_STRING("Use specified GPU Copy mode. This option triggers using InitEx instead of Init"), OPT_INT_32))
            {
                MFX_CHECK(1 + argv != argvEnd);
                m_inParams.bInitEx = true;

                MFX_PARSE_INT(m_inParams.nGpuCopyMode, argv[1]);

                argv++;
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-camera_ccm"), VM_STRING("set specific values for camera color correction matrix. 9 float numbers expected."), OPT_INT_32))
            {
                MFX_CHECK(9 + argv != argvEnd);

                for ( int i = 0; i < 3; i++ )
                {
                    for ( int j = 0; j < 3; j++)
                    {
                        MFX_CHECK(1 + argv != argvEnd);
                        argv++;
                        MFX_PARSE_DOUBLE(m_extExtColorCorrection3x3->CCM[i][j], argv[0]);
                    }
                }
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-camera_blacklevel"), VM_STRING("set specific values for camera blacklevel correction filter B G0 G1 R"), OPT_INT_32))
            {
                MFX_CHECK(4 + argv != argvEnd);
                MFX_PARSE_INT(m_extExtCamBlackLevelCorrection->B,  argv[1]);
                MFX_PARSE_INT(m_extExtCamBlackLevelCorrection->G0, argv[2]);
                MFX_PARSE_INT(m_extExtCamBlackLevelCorrection->G1, argv[3]);
                MFX_PARSE_INT(m_extExtCamBlackLevelCorrection->R,  argv[4]);
                argv+=4;
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-camera_gamma_value"), VM_STRING("set specific value for camera gamma correction filter"), OPT_INT_32))
            {
                MFX_CHECK(1 + argv != argvEnd);
                float value;
                MFX_PARSE_DOUBLE(value, argv[1]);

                for (int i = 0; i < 64; i++)
                {
                    unsigned short max_input_level = 1<<m_inParams.nInputBitdepth;
                    gamma_point[i] = (i*2)*(i*2);
                    gamma_correct[i] = (int)(pow((double)gamma_point[i] / (double)max_input_level, (double)1/value) * (double)max_input_level);
                    if ( i==63 ) {
                        gamma_point[i] = 16384-1;
                        gamma_correct[i] = 16384-1;
                    }
                }
                m_extExtCamGammaCorrection->Mode       = MFX_CAM_GAMMA_LUT;
                m_extExtCamGammaCorrection->NumPoints  = 64;
                memcpy(m_extExtCamGammaCorrection->GammaPoint    , gamma_point  , m_extExtCamGammaCorrection->NumPoints*sizeof(mfxU16));
                memcpy(m_extExtCamGammaCorrection->GammaCorrected, gamma_correct, m_extExtCamGammaCorrection->NumPoints*sizeof(mfxU16));
                argv++;
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-camera_gamma_lut"), VM_STRING("use predefined LUT for for camera gamma correction filter")))
            {
                m_extExtCamGammaCorrection->Mode       = MFX_CAM_GAMMA_LUT;
                m_extExtCamGammaCorrection->NumPoints  = 64;
                memcpy(m_extExtCamGammaCorrection->GammaPoint    , gamma_point  , m_extExtCamGammaCorrection->NumPoints*sizeof(mfxU16));
                memcpy(m_extExtCamGammaCorrection->GammaCorrected, gamma_correct, m_extExtCamGammaCorrection->NumPoints*sizeof(mfxU16));
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-camera_whitebalance"), VM_STRING("set specific values for camera whitebalance correction filter B G0 G1 R"), OPT_INT_32))
            {
                MFX_CHECK(4 + argv != argvEnd);
                m_extExtCamWhiteBalance->Mode = MFX_CAM_WHITE_BALANCE_MANUAL;
                MFX_PARSE_DOUBLE(m_extExtCamWhiteBalance->B,  argv[1]);
                MFX_PARSE_DOUBLE(m_extExtCamWhiteBalance->G0, argv[2]);
                MFX_PARSE_DOUBLE(m_extExtCamWhiteBalance->G1, argv[3]);
                MFX_PARSE_DOUBLE(m_extExtCamWhiteBalance->R,  argv[4]);
                argv+=4;
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-vpp_scaling_mode"), VM_STRING("specify scaling mode for VPP resize"), OPT_INT_32))
            {
                MFX_CHECK(1 + argv != argvEnd);
                m_inParams.bVppScaling = true;
                MFX_PARSE_INT(m_inParams.uVppScalingMode,  argv[1]);
                argv+=1;
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-mediasdk_splitter"), VM_STRING("split stream from media container with MediaSDK splitter (ts, mp4)"), OPT_BOOL))
            {
                if (1+argv != argvEnd && argv[1][0]!='-')
                {
                    //trying to open file
                    vm_file * f = vm_file_fopen(argv[1], VM_STRING("wb"));
                    if (f)
                    {
                        MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.extractedAudioFile, MFX_ARRAY_SIZE(m_inParams.extractedAudioFile), argv[1]));
                        argv++;
                        vm_file_fclose(f);
                    }
                }

                m_inParams.bMediaSDKSplitter = true;
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-sfc_in"), VM_STRING("In params for SFC")))
            {
                MFX_CHECK(4 + argv < argvEnd);
                argv ++;
                MFX_PARSE_INT(m_extDecVideoProcessing->In.CropX, argv[0]);
                argv ++;
                MFX_PARSE_INT(m_extDecVideoProcessing->In.CropY, argv[0]);
                argv ++;
                MFX_PARSE_INT(m_extDecVideoProcessing->In.CropW, argv[0]);
                argv ++;
                MFX_PARSE_INT(m_extDecVideoProcessing->In.CropH, argv[0]);
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-sfc_out"), VM_STRING("Out params for SFC")))
            {
                MFX_CHECK(6 + argv < argvEnd);
                argv ++;
                MFX_PARSE_INT(m_extDecVideoProcessing->Out.Width, argv[0]);
                argv ++;
                MFX_PARSE_INT(m_extDecVideoProcessing->Out.Height, argv[0]);
                argv ++;
                MFX_PARSE_INT(m_extDecVideoProcessing->Out.CropX, argv[0]);
                argv ++;
                MFX_PARSE_INT(m_extDecVideoProcessing->Out.CropY, argv[0]);
                argv ++;
                MFX_PARSE_INT(m_extDecVideoProcessing->Out.CropW, argv[0]);
                argv ++;
                MFX_PARSE_INT(m_extDecVideoProcessing->Out.CropH, argv[0]);
            }
            else if (m_OptProc.Check(argv[0], VM_STRING("-sfc_background"), VM_STRING("background params for SFC")))
            {
                MFX_CHECK(3 + argv < argvEnd);
                argv ++;
                MFX_PARSE_INT(m_extDecVideoProcessing->R, argv[0]);
                argv ++;
                MFX_PARSE_INT(m_extDecVideoProcessing->G, argv[0]);
                argv ++;
                MFX_PARSE_INT(m_extDecVideoProcessing->B, argv[0]);
            }
            else HANDLE_INT_OPTION(m_inParams.nInputBitdepth, VM_STRING("-i_bitdepth"), VM_STRING("bitdepth of input raw stream"))
            else HANDLE_BOOL_OPTION(m_inParams.bDisableIpFieldPair, VM_STRING("-disable_ip_field_pair"), VM_STRING("disable i/p field pair"));
            else HANDLE_INT_OPTION(m_inParams.nImageStab, VM_STRING("-stabilize"), VM_STRING("use particular image stabilization mode 1-upscale, 2-boxing"))
            else HANDLE_BOOL_OPTION(m_inParams.bPAFFDetect, VM_STRING("-paff"), VM_STRING("enabled picture structure detection by VPP"));
            else HANDLE_INT_OPTION(m_inParams.nSVCDownSampling, VM_STRING("-downsampling"), VM_STRING("use downsampling algorithm 1-best quality, 2-best speed"))
            else HANDLE_BOOL_OPTION(m_inParams.bDxgiDebug, VM_STRING("-dxgidebug"), VM_STRING("inject dxgidebug.dll to report live objects(dxgilevel memory leaks)"));
            else HANDLE_FILENAME_OPTION(m_inParams.strDecPlugin,     VM_STRING("-decode_plugin"),      VM_STRING("MediaSDK Decoder plugin filename"))
            else HANDLE_FILENAME_OPTION(m_inParams.strDecPluginGuid, VM_STRING("-decode_plugin_guid"), VM_STRING("MediaSDK Decoder plugin GUID"))
            else HANDLE_FILENAME_OPTION(m_inParams.strEncPlugin,     VM_STRING("-encode_plugin"),      VM_STRING("MediaSDK Encoder plugin filename"))
            else HANDLE_FILENAME_OPTION(m_inParams.strEncPluginGuid, VM_STRING("-encode_plugin_guid"), VM_STRING("MediaSDK Encoder plugin GUID"))
            else HANDLE_FILENAME_OPTION(m_inParams.strVPPPluginGuid, VM_STRING("-vpp_plugin_guid"),    VM_STRING("MediaSDK VPP plugin GUID"))
            else HANDLE_BOOL_OPTION(m_inParams.bUseOverlay, VM_STRING("-overlay"), VM_STRING("Use overlay for rendering"));
            else if (0 != (nPattern = m_OptProc.Check(argv[0], VM_STRING("-(dec|enc):shift)"), VM_STRING("Desired data shift in frame data"), OPT_INT_32)))
            {
                ComponentsContainer::iterator component;
                component = std::find_if(m_components.begin(), m_components.end(),
                    std::bind2nd(mem_var_isequal<ComponentParams, tstring>(&ComponentParams::m_ShortName), tstring(argv[0]).substr(1, 3)));

                argv++;
                MFX_CHECK(argv < argvEnd);
                MFX_PARSE_INT(component->m_params.mfx.FrameInfo.Shift, argv[0]);
          }
          else if (m_OptProc.Check(argv[0], VM_STRING("-allegro"), VM_STRING("allegro")))
          {
                m_inParams.outFrameInfo.FourCC = MFX_FOURCC_YUV420_16;
                m_inParams.outFrameInfo.BitDepthLuma = 10;
                m_inParams.outFrameInfo.BitDepthChroma = 10;
                m_inParams.isAllegroTest = true;
          }
          else if (m_OptProc.Check(argv[0], VM_STRING("-hm_disable"), VM_STRING("hm_disable")))
          {
                m_inParams.isHMTest = false;
          }
          else if (m_OptProc.Check(argv[0], VM_STRING("-VpxDec16bFormat"), VM_STRING("Use 10bit (packed to 16bit) format used in vpxdec with option --output-bit-depth=16. Samples are shifted (located in MSBs); 6 LSBs equal to 31 (01 1111). Option works only when output format is YUV420_16, i.e. input is 10b and output FOURCC is not specified (-o out.yuv) ")))
          {
                m_inParams.VpxDec16bFormat = true;
          }
          else if (m_OptProc.Check(argv[0], VM_STRING("-i:picstruct"), VM_STRING("Set picstruct for decoded frames"), OPT_INT_32))
          {
              MFX_CHECK(1 + argv < argvEnd);
              argv ++;
              mfxU16 picstruct;
              MFX_PARSE_INT(picstruct, argv[0]);
              m_inParams.InputPicstruct = Convert_CmdPS_to_MFXPS(picstruct);
          }
          else if (m_OptProc.Check(argv[0], VM_STRING("-o:picstruct"), VM_STRING("Set picstruct for the output frames"), OPT_INT_32))
          {
              MFX_CHECK(1 + argv < argvEnd);
              argv ++;
              mfxU16 picstruct;
              MFX_PARSE_INT(picstruct, argv[0]);
              m_inParams.OutputPicstruct = Convert_CmdPS_to_MFXPS(picstruct);
          }
          else if (m_OptProc.Check(argv[0], VM_STRING("-adaptive_playback"), VM_STRING("Turn on adaptive playback mode. Disabled by default.")))
          {
              m_inParams.bAdaptivePlayback = true;
          }
          else if (m_OptProc.Check(argv[0], VM_STRING("-vp9_drc"), VM_STRING("VP9 smooth DRC algorithm emulation.")))
          {
              m_inParams.bVP9_DRC = true;
          }
          else if (m_OptProc.Check(argv[0], VM_STRING("-gpu_hang_recovery"), VM_STRING("Enables recovery after GPU hang")))
          {
              m_inParams.bGPUHangRecovery = true;
          }
          else if (m_OptProc.Check(argv[0], VM_STRING("-NumberFramesAfterRecovery"), VM_STRING("number frames to process after GPU hang recovery"), OPT_UINT_32))
          {
              MFX_CHECK(1 + argv != argvEnd);
              argv++;
              MFX_PARSE_INT(m_inParams.nFramesAfterRecovery,argv[0]);
          }
          else HANDLE_INT_OPTION(m_inParams.nDecoderSurfs, VM_STRING("-dec:surfs"), VM_STRING("specifies number of surfaces in decoder's pool"))
          else
          {
               MFX_TRACE_AT_EXIT_IF( MFX_ERR_UNSUPPORTED
                    , !bReportError
                    , PE_OPTION
                    , (VM_STRING("ERROR: Unknown option: %s\n"), argv[0]));
          }
        }
    }
    return MFX_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////////

void MFXDecPipeline::PrintCommonHelp()
{
    vm_char *argv[1], **_argv = argv;
    mfxI32 argc = 1;
    argv[0] = const_cast<vm_char*>(VM_STRING("unsupported option"));

    m_OptProc.SetPrint(true);
    ProcessCommand(_argv, argc, false);
    m_OptProc.SetPrint(false);
}

int MFXDecPipeline::PrintHelp()
{
    vm_char * last_err = GetLastErrString();
    if (NULL != last_err)
    {
        vm_string_printf(VM_STRING("ERROR: %s\n"), last_err);
    }

    int year, month, day;
    int h,m,s;
    const vm_char *platform;
    PipelineBuildTime(platform, year, month, day, h, m, s);
    vm_string_printf(VM_STRING("\n"));
    vm_string_printf(VM_STRING("---------------------------------------------------------------------------------------------\n"));
    vm_string_printf(VM_STRING("%s version 1.%d.%d.%d / %s\n"), GetAppName().c_str(), year%100, month, day, platform);
    vm_string_printf(VM_STRING("%s is DXVA2/LibVA testing application developed by Intel/SSG/DPD/VCP/MDP\n"), GetAppName().c_str());
    vm_string_printf(VM_STRING("This application is for Intel INTERNAL use only!\n"));
    vm_string_printf(VM_STRING("For support, usage examples, and recent builds please visit http://goto/%s  \n"), GetAppName().c_str());
    vm_string_printf(VM_STRING("\n"));
    vm_string_printf(VM_STRING("%s supports video decoding on VLD entry point of following codecs:\n"), GetAppName().c_str());
    vm_string_printf(VM_STRING("  MPEG2\n"));
    vm_string_printf(VM_STRING("  H.264\n"));
    vm_string_printf(VM_STRING("  VC-1\n"));
    vm_string_printf(VM_STRING("and stream formats:\n"));
    vm_string_printf(VM_STRING("  Elementary streams (i.e. pure video)\n"));
    vm_string_printf(VM_STRING("  MPEG2 Transport/Program streams\n"));
    vm_string_printf(VM_STRING("  MPEG4 streams\n"));
    vm_string_printf(VM_STRING("  ASF   streams\n"));
    vm_string_printf(VM_STRING("  AVI   streams\n"));
    vm_string_printf(VM_STRING("%s plays back video only (without audio)\n"), GetAppName().c_str());
    vm_string_printf(VM_STRING("---------------------------------------------------------------------------------------------\n"));
    vm_string_printf(VM_STRING("\n"));
    vm_string_printf(VM_STRING("Usage: %s.exe [Parameters]\n\n"), GetAppName().c_str());
    vm_string_printf(VM_STRING("Parameters are:\n"));
    PrintCommonHelp();



    return 1;
}

mfxStatus MFXDecPipeline::GetMulTipleAndReliabilitySettings(mfxU32 &nRepeat, mfxU32 &nTimeout)
{
    nRepeat  = m_inParams.nRepeat;
    nTimeout = m_inParams.nTimeout;
    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::GetGPUErrorHandlingSettings(bool &bHeavyResetAllowed)
{
    bHeavyResetAllowed  = m_inParams.bGPUHangRecovery;
    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::SetRefFile(const vm_char * pRefFile, mfxU32 nDelta)
{
    UNREFERENCED_PARAMETER(nDelta);
    MFX_CHECK_POINTER(pRefFile);
    MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.refFile, MFX_ARRAY_SIZE(m_inParams.refFile), pRefFile));

    if (0 == vm_string_strcmp(pRefFile, m_inParams.strDstFile))
    {
        MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.strDstFile, MFX_ARRAY_SIZE(m_inParams.strDstFile), pRefFile));
        MFX_CHECK(0 == vm_string_strcat_s(m_inParams.strDstFile, MFX_ARRAY_SIZE(m_inParams.strDstFile), VM_STRING(".tmp")));
    }
    m_RenderType = MFX_METRIC_CHECK_RENDER;

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::SetOutFile(const vm_char * pOutFile)
{
    if (NULL != pOutFile)
    {
        MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.strDstFile, MFX_ARRAY_SIZE(m_inParams.strDstFile), pOutFile));
    }
    else
    {
        m_inParams.strDstFile[0] = 0;
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::GetOutFile( vm_char * ppOutFile, int bufsize )
{
    MFX_CHECK_POINTER(ppOutFile);
    vm_string_strcpy_s(ppOutFile, bufsize, m_inParams.strDstFile);
    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::WriteParFile()
{
    if (0 == vm_string_strlen(m_inParams.strParFile) || !m_bWritePar)
        return MFX_ERR_NONE;

    vm_file * fd;
    MFX_CHECK_VM_FOPEN(fd, m_inParams.strParFile, VM_STRING("w"));

    mfxFrameInfo &Info = m_inParams.bUseVPP ? m_components[eVPP].m_params.vpp.Out : m_components[eDEC].m_params.mfx.FrameInfo;

    vm_file_fprintf(fd, VM_STRING("width = %d \n"), Info.CropW);
    vm_file_fprintf(fd, VM_STRING("height= %d \n"), Info.CropH);
    //if (m_inParams.nPicStruct >= 0) cannot affect crash in transcoding
    {
        vm_file_fprintf(fd, VM_STRING("s = %d \n"), m_inParams.nPicStruct);
    }

    vm_file_fprintf(fd, VM_STRING("CropX = %d \n"), Info.CropX);
    vm_file_fprintf(fd, VM_STRING("CropY = %d \n"), Info.CropY);
    //vm_file_fprintf(fd, VM_STRING("CropW = %d \n"), Info.CropW);
    //vm_file_fprintf(fd, VM_STRING("CropH = %d \n"), Info.CropH);

    vm_file_fprintf(fd, VM_STRING("Seed = %d \n"), m_inParams.nSeed);

    vm_file_fprintf(fd, VM_STRING("AspectRatioW = %d \n"), m_components[eREN].m_params.mfx.FrameInfo.AspectRatioW);
    vm_file_fprintf(fd, VM_STRING("AspectRatioH = %d \n"), m_components[eREN].m_params.mfx.FrameInfo.AspectRatioH);

    vm_file_fprintf(fd, VM_STRING("f = %.2f \n"), m_components[eDEC].m_fFrameRate);

    vm_file_close(fd);

    return MFX_ERR_NONE;
}

#define IS_SEPARATOR(ch)  ((ch) <= ' ' || (ch) == '=')

mfxStatus MFXDecPipeline::ReconstructInput( vm_char ** argvStart, mfxI32 argc, void* pReconstrcutedargs)
{
    MFX_CHECK_POINTER(pReconstrcutedargs);
    MFX_CHECK_SET_ERR(NULL != argvStart, PE_NO_ERROR, MFX_ERR_NULL_PTR);
    MFX_CHECK_SET_ERR(0 != argc, PE_NO_ERROR, MFX_ERR_UNKNOWN);

    std::vector<tstring> *ReconstrcutedArgs = reinterpret_cast<std::vector<tstring> *>(pReconstrcutedargs);

    vm_char * const *argvEnd = argvStart + argc;

    for (vm_char ** argv = argvStart; argv != argvEnd; argv++)
    {
        if (m_OptProc.Check(argv[0], VM_STRING("-par"), VM_STRING("read parameters from par-file; par-file syntax: PARAM = VALUE"), OPT_FILENAME))
        {
            MFX_CHECK(1 + argv != argvEnd);
            DecPipeline2ReconstructInput wrapper (this, pReconstrcutedargs);
            MFX_CHECK_STS(ReadParFile(argv[1], &wrapper));
            argv++;
        }else
        {
            ReconstrcutedArgs->push_back(argv[0]);
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::ReadParFile(const vm_char * pInFile, IProcessCommand * pHandler)
{
    vm_char fileLine[2048];
    vm_char *argv[32];
    int   argc;
    //vm_file  *fd;

    MFX_CHECK_POINTER(pHandler);

    std::auto_ptr<IFile> pFileAccess(m_pFactory->CreatePARReader(NULL));
    MFX_CHECK_STS(pFileAccess->Open(pInFile, VM_STRING("r")));

    while (!pFileAccess->isEOF())
    {
        fileLine[0] = ' ';
        mfxStatus sts = pFileAccess->ReadLine(fileLine + 1, MFX_ARRAY_SIZE(fileLine) - 2);
        if(MFX_ERR_NONE != sts)
        {
            if (pFileAccess->isEOF())
                break;
            else
                return sts;
        }

        //vm_char *p = vm_file_fgets(fileLine + 1, MFX_ARRAY_SIZE(fileLine) - 2, fd);
        //if (NULL == p)
        //  break;
        vm_char *p = fileLine + 1;

        if (pHandler->IsPrintParFile())
            PrintInfo(VM_STRING("Par-file"), VM_STRING("%s"), fileLine);

        for (argc = 0; p[0] && p[0] != '#'; p++)
        {
            if (IS_SEPARATOR(p[-1]) && !IS_SEPARATOR(p[0]) && static_cast<unsigned int>(argc) < MFX_ARRAY_SIZE(argv))
            {
                argv[argc++] = p;
            }
            if (IS_SEPARATOR(p[0]))
            {
                p[0] = 0;
            }
        }

        if (!argc)
        {
            continue;
        }

        argv[0]--;
        argv[0][0] = '-';

        int i;
        for (i = 0; i < argc; i++)
        {
            if (!vm_string_stricmp(argv[i], VM_STRING("on")))  vm_string_strcpy_s(argv[i], 3, VM_STRING("16")); // MFX_CODINGOPTION_ON
            if (!vm_string_stricmp(argv[i], VM_STRING("off"))) vm_string_strcpy_s(argv[i], 3, VM_STRING("32")); // MFX_CODINGOPTION_OFF
            if (!vm_string_stricmp(argv[i], VM_STRING("adaptive"))) vm_string_strcpy_s(argv[i], 3, VM_STRING("48")); // MFX_CODINGOPTION_ADAPTIVE
        }

        vm_char **_argv = argv;
        MFX_CHECK_STS(pHandler->ProcessCommand(_argv, argc));
    }

    //vm_file_close(fd);

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::ProcessTrickCommands()
{
    if (m_commands.empty())
        return MFX_ERR_NONE;

    //activate first command
    if (!m_commands.front()->Activate())
        return MFX_ERR_NONE;

    //processing one command
    if (!m_commands.front()->isReady())
        return MFX_ERR_NONE;

    //infact executing
    if (!m_commands.front()->isFinished())
    {
        PipelineTrace((VM_STRING("\nFrame=%d Time=%.2lf - "), m_nDecFrames, m_fLastTime));

        //executing command -> flags manipulations
        MFX_CHECK_STS(m_commands.front()->Execute());

#if ( defined(_WIN32) || defined(_WIN64) ) && !defined(WIN_TRESHOLD_MOBILE)
        //printing of current memory usage
        PROCESS_MEMORY_COUNTERS_EX pmcx = {};
        pmcx.cb = sizeof(pmcx);
        GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmcx), pmcx.cb);

        PrintInfo(VM_STRING("Memory Private"), VM_STRING("%.2f M"),
            (Ipp64f)(pmcx.PrivateUsage) / (1024 * 1024));
#endif // ( defined(_WIN32) || defined(_WIN64) ) && !defined(WIN_TRESHOLD_MOBILE)
        //reseting pipeline time
        m_fLastTime  = 0.0;
        m_fFirstTime = -1.0;
    }
    else if (m_commands.front()->isRepeat())
    {
        //only reset commnd and not remove it from pipeline
        m_commands.front()->Reset();
        MFX_CHECK_STS(ProcessTrickCommands());
    }
    else
    {
        m_commandsProcessed.push_back(m_commands.front());
        m_commands.pop_front();
        MFX_CHECK_STS(ProcessTrickCommands());
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::ReduceMemoryUsage()
{
    if (!m_bPreventBuffering)
    {
        m_bPreventBuffering = true;
        return MFX_ERR_NONE;
    }

    return MFX_ERR_MEMORY_ALLOC;
}

mfxStatus MFXDecPipeline::SetSyncro(IPipelineSynhro * pSynchro)
{
    m_externalsync = pSynchro;
    return MFX_ERR_NONE;
}

mfxF64 MFXDecPipeline::Current()
{
    if (m_fFirstTime == -1.0)
        return -1.0;//no timing received yet

    mfxF64 curtime;

    //initial time should be zero
    if (m_fLastTime == m_fFirstTime)
        curtime = 0.0;
    else
        curtime =  m_fLastTime - m_fFirstTime + ((mfxF64)m_components[eDEC].m_params.mfx.FrameInfo.FrameRateExtD / (mfxF64)m_components[eDEC].m_params.mfx.FrameInfo.FrameRateExtN);

    return curtime;
}

mfxStatus MFXDecPipeline::GetYUVSource(IYUVSource**ppSource)
{
    MFX_CHECK_POINTER(ppSource);

    *ppSource = m_pYUVSource.get();

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::GetSplitter (IBitstreamReader**ppSpliter)
{
    MFX_CHECK_POINTER(ppSpliter);
    MFX_CHECK_POINTER(m_pSpl);

    *ppSpliter = m_pSpl;

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::GetRender (IMFXVideoRender **ppRender)
{
    MFX_CHECK_POINTER(ppRender);
    MFX_CHECK_POINTER(m_pRender);

    *ppRender = m_pRender;

    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::GetTimer (ITime ** ppTimer)
{
    MFX_CHECK_POINTER(ppTimer);
    *ppTimer = this;
    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::GetVPP(IMFXVideoVPP** ppVpp)
{
    MFX_CHECK_POINTER(ppVpp);
    *ppVpp = m_pVPP;
    return MFX_ERR_NONE;
}

mfxStatus MFXDecPipeline::GetVppParams(ComponentParams *& pParams)
{
    MFX_CHECK_POINTER(pParams);
    pParams = &m_components[eVPP];
    return MFX_ERR_NONE;
}


mfxStatus MFXDecPipeline::ResetAfterSeek()
{
    ////splitter output
    m_inBSFrame.DataLength = 0;

    //buffered splitter output prior decoding
    m_bitstreamBuf.Reset();

    ////remove queued sync points
    for ( ; !m_components[eDEC].m_SyncPoints.empty(); m_components[eDEC].m_SyncPoints.pop_front())
    {
        DecreaseReference(&m_components[eDEC].m_SyncPoints.front().second.pSurface->Data);
    }
    for ( ; !m_components[eVPP].m_SyncPoints.empty(); m_components[eVPP].m_SyncPoints.pop_front())
    {
        DecreaseReference(&m_components[eVPP].m_SyncPoints.front().second.pSurface->Data);
    }

    return MFX_ERR_NONE;
}

mfxU32 MFXDecPipeline::GetNumDecodedFrames(void)
{
    return m_nDecFrames;
}


AllocatorAdapterRW::AllocatorAdapterRW(MFXFrameAllocatorRW * allocator)
    : m_allocator(allocator)
{
}

AllocatorAdapterRW::~AllocatorAdapterRW()
{
    delete m_allocator;
}

mfxStatus AllocatorAdapterRW::Init(mfxAllocatorParams *pParams)
{
    return m_allocator->Init(pParams);
}

mfxStatus AllocatorAdapterRW::Close()
{
    return m_allocator->Close();
}

mfxStatus AllocatorAdapterRW::AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxStatus  status =  m_allocator->AllocFrames(request, response);
    if (status < MFX_ERR_NONE)
        return status;

    if (!m_mids.size())
    {
        for (mfxI32 i = 0; i < response->NumFrameActual; i++)
        {
            MidPair pair;
            pair.first = (mfxMemId)(intptr_t)(i + 1);
            pair.second = response->mids[i];
            response->mids[i] = (mfxMemId)(intptr_t)(i + 1);
            m_mids.push_back(pair);
        }
    }

    return status;
}

mfxStatus AllocatorAdapterRW::AllocFrame(mfxFrameSurface1 *surface)
{
    mfxMemId mid = surface->Data.MemId;
    if ((size_t)mid > m_mids.size())
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    surface->Data.MemId = m_mids[(size_t)(mid) - 1].second;
    mfxStatus  status =  m_allocator->AllocFrame(surface);
    if (status < MFX_ERR_NONE)
        return status;

    // update surface after reallocation
    m_mids[(size_t)(mid) - 1].second = surface->Data.MemId;
    surface->Data.MemId = mid;
    return status;
}

mfxStatus AllocatorAdapterRW::FreeFrames(mfxFrameAllocResponse *response)
{
    if (m_mids.size())
    {
        for (mfxI32 i = 0; i < response->NumFrameActual; i++)
        {
            response->mids[i] = m_mids[i].second;
        }
    }

    m_mids.clear();
    return m_allocator->FreeFrames(response);
}

mfxStatus AllocatorAdapterRW::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    return m_allocator->LockFrame(m_mids[(size_t)(mid) - 1].second, ptr);
}

mfxStatus AllocatorAdapterRW::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    return m_allocator->UnlockFrame(m_mids[(size_t)(mid) - 1].second, ptr);
}

mfxStatus AllocatorAdapterRW::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
    return m_allocator->GetFrameHDL(m_mids[(size_t)(mid) - 1].second, handle);
}

mfxStatus AllocatorAdapterRW::LockFrameRW(mfxMemId mid, mfxFrameData *ptr, mfxU8 lockflag /*MFXReadWriteMid::read|write*/)
{
    return m_allocator->LockFrameRW(m_mids[(size_t)(mid) - 1].second, ptr, lockflag);
}

mfxStatus AllocatorAdapterRW::UnlockFrameRW(mfxMemId mid, mfxFrameData *ptr, mfxU8 writeflag /*MFXReadWriteMid::write|0*/)
{
    return m_allocator->UnlockFrameRW(m_mids[(size_t)(mid) - 1].second, ptr, writeflag);
}
