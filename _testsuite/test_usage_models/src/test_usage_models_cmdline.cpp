//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2016 Intel Corporation. All Rights Reserved.
//

#include "test_usage_models_cmdline.h"

#if !defined(msdk_sscanf)
  #if defined(_WIN32) || defined(_WIN64)
    # define msdk_sscanf _stscanf_s
  #else
    # define msdk_sscanf sscanf
  #endif
#endif

mfxU32 String2VideoFormat( msdk_char* arg );
msdk_char* VideoFormat2String( mfxU32 FourCC );
mfxU16 String2IOPattern( msdk_char* strInput );
msdk_char* IOPattern2String(mfxU16 IOPattern);
msdk_char* ImpLibrary2String(mfxIMPL impl);
void PrintDllInfo( void );

void CommandLine::PrintUsage(const msdk_char* app)
{
    msdk_printf( MSDK_STRING("%s [options] -i InputStream -o OutputStream\n\n") , app);
    msdk_printf(
        MSDK_STRING("version: 1.1\n")
        MSDK_STRING("options:\n")
        MSDK_STRING("[-lib   type]      - type (general) of used MediaSDK implementation (sw|hw) \n")
        MSDK_STRING("   [-dec::type]    - type of dec implementation (sw|hw) \n")
        MSDK_STRING("   [-vpp::type]    - type of vpp implementation (sw|hw) \n")
        MSDK_STRING("   [-enc::type]    - type of enc implementation (sw|hw) \n\n")

        MSDK_STRING("[-sfmt  format]    - format of src video (h264|mpeg2|vc1)\n")
        MSDK_STRING("[-dfmt  format]    - format of dst video (h264|mpeg2)\n")
        MSDK_STRING("[-w     width]     - required width  of dst video\n")
        MSDK_STRING("[-h     height]    - required height of dst video\n")
        MSDK_STRING("[-b     bitRate]   - encoded bit rate (Kbits per second)\n")
        MSDK_STRING("[-f     frameRate] - video frame rate (frames per second)\n")
        MSDK_STRING("[-u     target]    - target usage (quality=1|balanced=4|speed=7). default is 1\n\n")

        MSDK_STRING("[-async depth]     - depth of asynchronous pipeline. default is 1\n")
        MSDK_STRING("[-model number]    - number of MediaSDK usage model. default is 0\n\n")
        MSDK_STRING("[-n     frames]    - number of frames to trancode process\n\n")

        MSDK_STRING("[-iopattern mem]   - memory type of used surfaces: (sys|d3d) or\n")
        MSDK_STRING("                   - (sys_to_sys|sys_to_d3d|d3d_to_sys|d3d_to_d3d) if VPP required\n\n\n")          
        );
        /*"     [-dec::mem]   - memory type of dec surfaces: (sys|d3d) \n"
          "     [-vpp::mem]   - memory type of vpp surfaces: (sys_to_sys|sys_to_d3d|d3d_to_sys|d3d_to_d3d)\n"
          "     [-enc::mem]   - memory type of enc surfaces: (sys|d3d) \n\n\n"*/
     msdk_printf(
        MSDK_STRING("List of supported MediaSDK usage models: [0-8]\n")
        MSDK_STRING("See detail in file \n")
        MSDK_STRING("{MSDK_ROOT}\\_testsuite\\test_usage_models\\UsageModelList.pdf \n")
        );
    
} // void CommandLine::PrintUsage(const msdk_char* app)

CommandLine::CommandLine(int argc, msdk_char *argv[])
    :  m_srcVideoFormat(0)
    ,  m_dstVideoFormat(0)
    ,  m_width(0)
    ,  m_height(0)
    ,  m_bitRate(0)
    ,  m_frameRate(0) //AYA_DEBUG
    ,  m_targetUsage(0)
    ,  m_asyncDepth(0)
    ,  m_usageModel(0)
    ,  m_framesCount(0)
    ,  m_NumSlice(0)
    ,  m_pSrcFileName(0)
    ,  m_pDstFileName(0)

    //,  m_impLib(MFX_IMPL_HARDWARE)

    ,  m_IOPattern( MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY )
    ,  m_valid(false)
{
    const int DEFAULT_HW_IMPL =
#if defined(_WIN32)
        MFX_IMPL_VIA_D3D9
#else
        MFX_IMPL_VIA_VAAPI
#endif
        ;

    m_impLib[ MSDK_STRING("general") ] = MFX_IMPL_SOFTWARE; // general session should be inited always

    if (argc < 6)
    {
        //VM_ASSERT(!"too few parameters");
        return;
    }    

    for (int i = 1; i < argc; i++)
    {
        int readVal;

        if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-sfmt")) )    
        {
            if (++i < argc)
            {                
                m_srcVideoFormat = String2VideoFormat(argv[i]);
            }
        } 
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-dfmt")) )
        {
            if (++i < argc)
            {                
                m_dstVideoFormat = String2VideoFormat(argv[i]);
            }
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-w")) )
        {
            if (++i < argc)
            {
                msdk_sscanf(argv[i], MSDK_STRING("%i"), &readVal);
                m_width = (mfxU16)readVal;
            }
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-h")) )
        {
            if (++i < argc)
            {
                msdk_sscanf(argv[i], MSDK_STRING("%i"), &readVal);
                m_height = (mfxU16)readVal;
            }
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-b")) )
        {
            if (++i < argc)
            {
                msdk_sscanf(argv[i], MSDK_STRING("%i"), &m_bitRate);
            }
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-f")) )
        {
            if (++i < argc)
            {
                msdk_sscanf(argv[i], MSDK_STRING("%lf"), &m_frameRate);
            }
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-u")) )
        {
            if (++i < argc)
            {
                msdk_sscanf(argv[i], MSDK_STRING("%i"), &readVal);
                m_targetUsage = (mfxU16)readVal;
            }
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-async")) )
        {
            if (++i < argc)
            {           
                msdk_sscanf(argv[i], MSDK_STRING("%i"), &readVal);
                m_asyncDepth = (mfxU16)readVal;
                // async depth correction
                if (0 == m_asyncDepth) 
                {
                    m_asyncDepth = 1;
                }
            }
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-model")) )
        {
            if (++i < argc)
            {
                msdk_sscanf(argv[i], MSDK_STRING("%i"), &readVal);
                m_usageModel = (mfxU16)readVal;
            }
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-n")) )
        {
            if (++i < argc)
            {
                msdk_sscanf(argv[i], MSDK_STRING("%i"), &m_framesCount);
            }
        }

        // src/dst streams
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-o")) )
        {
            if (++i < argc)
            {
                // save output file name                
                m_pDstFileName = argv[i];
            }
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-i")) )
        {
            if (++i < argc)
            {
                // save output file name                
                m_pSrcFileName = argv[i];
            }
        }

        // implementation of MFX library in general
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-lib")) )
        {
            if( ++i < argc )
            {
                if (0 == msdk_strcmp(argv[i], MSDK_STRING("sw")) )
                {
                    m_impLib[MSDK_STRING("general")] = MFX_IMPL_SOFTWARE;
                }
                else if (0 == msdk_strcmp(argv[i], MSDK_STRING("hw")) )
                {
                    m_impLib[MSDK_STRING("general")] = MFX_IMPL_HARDWARE | DEFAULT_HW_IMPL;
                }
            }
        }
        // implementation of MFX library for individual component
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-dec::sw")) )
        {
            m_impLib[MSDK_STRING("dec")] = MFX_IMPL_SOFTWARE;
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-dec::hw")) )
        {
            m_impLib[MSDK_STRING("dec")] = MFX_IMPL_HARDWARE | DEFAULT_HW_IMPL;
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-vpp::sw")) )
        {
            m_impLib[MSDK_STRING("vpp")] = MFX_IMPL_SOFTWARE;
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-vpp::hw")) )
        {
            m_impLib[MSDK_STRING("vpp")] = MFX_IMPL_HARDWARE | DEFAULT_HW_IMPL;
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-enc::sw")) )
        {
            m_impLib[MSDK_STRING("enc")] = MFX_IMPL_SOFTWARE;
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-enc::hw")) )
        {
            m_impLib[MSDK_STRING("enc")] = MFX_IMPL_HARDWARE | DEFAULT_HW_IMPL;
        }

        // iopattern
        else if( 0 == msdk_strcmp(argv[i], MSDK_STRING("-iopattern")) )
        {
            if( ++i < argc )
            {
                m_IOPattern = String2IOPattern( argv[i] );
            }
        }
        else if ( 0 == msdk_strcmp(argv[i], MSDK_STRING("-l")) )
        {
            if (++i < argc)
            {
                msdk_sscanf(argv[i], MSDK_STRING("%i"), &readVal);
                m_NumSlice = (mfxU16)readVal;
            }
        }
    }    

    // validate parameters
    m_valid = true;

    if( !IsVPPEnable() )
    {
        mfxU16 newPattern = m_IOPattern & ( MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_SYSTEM_MEMORY );

        m_IOPattern = newPattern | InvertIOPattern( newPattern );
    }

    //// aya debug only
    //if( m_width == 0 || m_height == 0 ) // VPP disable
    //{
    //    m_width = m_width = 0;
    //}

    ////AYA debug
    ////m_usageModel = USAGE_MODEL_8;
    ////m_asyncDepth = 5;
    ////m_impLib[MSDK_STRING("dec")] = MFX_IMPL_SOFTWARE;
    ////m_impLib[MSDK_STRING("vpp")] = MFX_IMPL_HARDWARE;
    ////m_impLib[MSDK_STRING("enc")] = MFX_IMPL_HARDWARE;
    //m_width = m_width = 0;
    //if( m_usageModel > 0 )
    //{
    //    m_impLib[ MSDK_STRING("general") ] = MFX_IMPL_HARDWARE;
    //    m_impLib[ MSDK_STRING("vpp") ]    = MFX_IMPL_SOFTWARE;
    //    m_IOPattern = (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY);
    //    //m_width = m_width = 0;
    //    m_usageModel = USAGE_MODEL_8;
    //}


} // CommandLine::CommandLine(int argc, msdk_char *argv[])


void CommandLine::GetParam( AppParam& param )
{
    if( m_valid )
    {
        param.srcVideoFormat = m_srcVideoFormat;    
        param.dstVideoFormat = m_dstVideoFormat;    
        param.width          = m_width; 
        param.height         = m_height;
        param.bitRate        = m_bitRate;
        param.frameRate      = m_frameRate;
        param.targetUsage    = m_targetUsage;    
        param.asyncDepth     = m_asyncDepth;
        param.usageModel     = m_usageModel;
        param.framesCount    = m_framesCount;
        param.pSrcFileName   = m_pSrcFileName;
        param.pDstFileName   = m_pDstFileName;
        param.impLib         = m_impLib;
        param.IOPattern      = m_IOPattern;
        param.NumSlice       = m_NumSlice;
    }

} // GetParam( AppParam& param )


void CommandLine::PrintInfo( void )
{
    if ( !m_valid ) 
    {
        return;
    }

    msdk_printf(MSDK_STRING("<Configuration of the test:>\n"));
    msdk_printf(MSDK_STRING("Input  format\t%s\n"),   VideoFormat2String( m_srcVideoFormat ));
    msdk_printf(MSDK_STRING("Output format\t%s\n"),   VideoFormat2String( m_dstVideoFormat ));

    if( m_bitRate > 0 )
    {
        msdk_printf(MSDK_STRING("BitRate\t\t%d\n"),     m_bitRate);
    }
    else
    {
        msdk_printf(MSDK_STRING("BitRate\t\t%s\n"),     MSDK_STRING("default"));
    }
    msdk_printf(MSDK_STRING("Target Usage\t%s\n"),    TargetUsageToStr(m_targetUsage) );
    msdk_printf(MSDK_STRING("Async Depth\t%d\n"),     m_asyncDepth);
    msdk_printf(MSDK_STRING("MFX Usage Model\t%d\n"), m_usageModel);
    
    //if( m_usageModel < USAGE_MODEL_6  )
    //{        
    //    msdk_printf(MSDK_STRING("MFX implement:\t%s\n"), ImpLibrary2String( m_impLib[MSDK_STRING("general")] ));
    //}
    //else
    {
        msdk_printf(MSDK_STRING("MFX DEC impl:\t%s\n"), ImpLibrary2String( (m_impLib.find(MSDK_STRING("dec")) != m_impLib.end()) ? m_impLib[MSDK_STRING("dec")] : m_impLib[MSDK_STRING("general")] ));
        if( IsVPPEnable() )
        {
            msdk_printf(MSDK_STRING("MFX VPP impl:\t%s\n"), ImpLibrary2String( (m_impLib.find(MSDK_STRING("vpp")) != m_impLib.end()) ? m_impLib[MSDK_STRING("vpp")] : m_impLib[MSDK_STRING("general")] ));
        }
        msdk_printf(MSDK_STRING("MFX ENC impl:\t%s\n"), ImpLibrary2String( (m_impLib.find(MSDK_STRING("enc")) != m_impLib.end()) ? m_impLib[MSDK_STRING("enc")] : m_impLib[MSDK_STRING("general")] ));
    }


    msdk_printf(MSDK_STRING("IOPattern\t%s\n"), IOPattern2String(m_IOPattern) );

    PrintDllInfo();
  
} // void CommandLine::PrintInfo( void )


mfxU32 String2VideoFormat( msdk_char* arg )
{
    mfxU32 format = MFX_FOURCC_NV12;//default

    if ( 0 == msdk_strcmp(arg, MSDK_STRING("h264")) ) 
    {
        format = MFX_CODEC_AVC;
    } 
    else if ( 0 == msdk_strcmp(arg, MSDK_STRING("mpeg2")) ) 
    {
        format = MFX_CODEC_MPEG2;
    } 
    else if ( 0 == msdk_strcmp(arg, MSDK_STRING("vc1")) ) 
    {
        format = MFX_CODEC_VC1;
    }    

    return format;

} // mfxU32 Str2FourCC( msdk_char* strInput )


msdk_char* VideoFormat2String( mfxU32 FourCC )
{
  msdk_char* strFourCC = MSDK_STRING("h264");//default

  switch ( FourCC )
  {
  case MFX_CODEC_AVC:
    strFourCC = MSDK_STRING("h264");
    break;

  case MFX_CODEC_MPEG2:
    strFourCC = MSDK_STRING("mpeg2");
    break;

  case MFX_CODEC_VC1:
    strFourCC = MSDK_STRING("vc1");
    break;
   
  default:
      strFourCC = MSDK_STRING("NV12");
      break;
  }

  return strFourCC;

} // msdk_char* VideoFormat2String( mfxU32 FourCC )


msdk_char* ImpLibrary2String(mfxIMPL impl)
{
    switch(impl)
    {
        case MFX_IMPL_AUTO:
        {
            return MSDK_STRING("auto");
        }
        case MFX_IMPL_SOFTWARE:
        {
            return MSDK_STRING("SW");
        }
        case (MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D9):
        {
            return MSDK_STRING("HW | via D3D9");
        }
        case (MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11):
        {
            return MSDK_STRING("HW | via D3D11");
        }
        case (MFX_IMPL_HARDWARE | MFX_IMPL_VIA_VAAPI):
        {
            return MSDK_STRING("HW | via VAAPI");
        }
        default:
        {
            return MSDK_STRING("unknown");
        }
    }

} // msdk_char* ImpLibraryToStr(mfxIMPL impl)


void PrintDllInfo( void )
{
#if defined(_WIN32) || defined(_WIN64)
    HANDLE   hCurrent = GetCurrentProcess();
    HMODULE *pModules;
    DWORD    cbNeeded;
    int      nModules;
    if (NULL == EnumProcessModules(hCurrent, NULL, 0, &cbNeeded))
        return;

    nModules = cbNeeded / sizeof(HMODULE);

    pModules = new HMODULE[nModules];
    if (NULL == pModules)
    {
        return;
    }
    if (NULL == EnumProcessModules(hCurrent, pModules, cbNeeded, &cbNeeded))
    {
        delete []pModules;
        return;
    }

    for (int i = 0; i < nModules; i++)
    {
        TCHAR buf[2048];
        GetModuleFileName(pModules[i], buf, ARRAYSIZE(buf));
        if (_tcsstr(buf, _T("libmfx")))
        {
            _tprintf(_T("MFX dll  path:\t%s\n"),buf);
        }
    }
    delete []pModules;
#endif // #if defined(_WIN32) || defined(_WIN64)
} // void PrintDllInfo( void )


mfxU16 String2IOPattern( msdk_char* strInput )
{
    mfxU16 IOPattern = 0;

    if ( 0 == msdk_strcmp(strInput, MSDK_STRING("d3d_to_d3d")) ) 
    {
        IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    } 
    else if ( 0 == msdk_strcmp(strInput, MSDK_STRING("d3d_to_sys")) ) 
    {
        IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    } 
    else if ( 0 == msdk_strcmp(strInput, MSDK_STRING("sys_to_d3d")) ) 
    {
        IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    } 
    else if ( 0 == msdk_strcmp(strInput, MSDK_STRING("sys_to_sys")) )
    {
        IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    } 
    else if( 0 == msdk_strcmp(strInput, MSDK_STRING("d3d")) )
    {
        IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }
    else if( 0 == msdk_strcmp(strInput, MSDK_STRING("sys")) )
    {
        IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }
    return IOPattern;

} // mfxU16 String2IOPattern( msdk_char* strInput )


msdk_char* IOPattern2String(mfxU16 IOPattern)
{
    switch(IOPattern)
    {
        // multi pattern
        case (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY):
        {
            return MSDK_STRING("video");
        }
        case (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY):
        {
            return MSDK_STRING("video_to_system");
        }
        case (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY):
        {
            return MSDK_STRING("system_to_video");
        }
        case (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY):
        {
            return MSDK_STRING("system");
        }           
        default:
        {
            return MSDK_STRING("unknown");
        }
    }
}


// rules:
// (1) function replaces IN<->OUT. 
// (2) Type of memory (sys/d3d) isn't modified.
// (3) mixed Pattern (ex IN_SYSTEM_MEMORY|OUT_VIDEO_MEMORY) not supported
mfxU16 InvertIOPattern( mfxU16 inPattern )
{
    mfxU16 outPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    switch( inPattern )
    {
        case MFX_IOPATTERN_IN_SYSTEM_MEMORY:
        {
            outPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            break;
        }
        case MFX_IOPATTERN_OUT_SYSTEM_MEMORY:
        {
            outPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
            break;
        }
        case MFX_IOPATTERN_IN_VIDEO_MEMORY:
        {
            outPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
            break;
        }
        case MFX_IOPATTERN_OUT_VIDEO_MEMORY:
        {
            outPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
            break;
        }
        default:
        {
            ;//nothig
        }
    }

    return outPattern;

} // mfxU16 InvertIOPattern( mfxU16 inPattern )


bool CommandLine::IsVPPEnable( void )
{
    if( m_width > 0 && m_height > 0)
    {
        return true;
    }
    else
    {
        return false;
    }

} // bool CommandLine::IsVPPEnable( void )
/* EOF */
