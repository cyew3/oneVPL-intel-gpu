//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 Intel Corporation. All Rights Reserved.
//

#include <windows.h>
#include <psapi.h>
#include "test_usage_models_cmdline.h"

mfxU32 String2VideoFormat( TCHAR* arg );
TCHAR* VideoFormat2String( mfxU32 FourCC );
mfxU16 String2IOPattern( TCHAR* strInput );
TCHAR* IOPattern2String(mfxU16 IOPattern);
TCHAR* ImpLibrary2String(mfxIMPL impl);
void PrintDllInfo( void );

void CommandLine::PrintUsage(const TCHAR* app)
{
    _tprintf( _T("%s [options] -i InputStream -o OutputStream\n\n") , app);
    _tprintf(
        _T("version: 1.1\n")
        _T("options:\n")
        _T("[-lib   type]      - type (general) of used MediaSDK implementation (sw|hw) \n")
        _T("   [-dec::type]    - type of dec implementation (sw|hw) \n")
        _T("   [-vpp::type]    - type of vpp implementation (sw|hw) \n")
        _T("   [-enc::type]    - type of enc implementation (sw|hw) \n\n")

        _T("[-sfmt  format]    - format of src video (h264|mpeg2|vc1)\n")
        _T("[-dfmt  format]    - format of dst video (h264|mpeg2)\n")
        _T("[-w     width]     - required width  of dst video\n")
        _T("[-h     height]    - required height of dst video\n")
        _T("[-b     bitRate]   - encoded bit rate (Kbits per second)\n")
        _T("[-f     frameRate] - video frame rate (frames per second)\n")
        _T("[-u     target]    - target usage (quality=1|balanced=4|speed=7). default is 1\n\n")

        _T("[-async depth]     - depth of asynchronous pipeline. default is 1\n")
        _T("[-model number]    - number of MediaSDK usage model. default is 0\n\n")
        _T("[-n     frames]    - number of frames to trancode process\n\n")

        _T("[-iopattern mem]   - memory type of used surfaces: (sys|d3d) or\n")
        _T("                   - (sys_to_sys|sys_to_d3d|d3d_to_sys|d3d_to_d3d) if VPP required\n\n\n")
        /*_T("     [-dec::mem]   - memory type of dec surfaces: (sys|d3d) \n")
        _T("     [-vpp::mem]   - memory type of vpp surfaces: (sys_to_sys|sys_to_d3d|d3d_to_sys|d3d_to_d3d)\n")
        _T("     [-enc::mem]   - memory type of enc surfaces: (sys|d3d) \n\n\n")*/

        );

     _tprintf(
        _T("List of supported MediaSDK usage models: [0-8]\n")        
        _T("See detail in file \n")
        _T("{MSDK_ROOT}\\_testsuite\\test_usage_models\\UsageModelList.pdf \n")
        );
    
} // void CommandLine::PrintUsage(const TCHAR* app)


CommandLine::CommandLine(int argc, TCHAR *argv[])
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
,  m_pSrcFileName(0)
,  m_pDstFileName(0)

//,  m_impLib(MFX_IMPL_HARDWARE)

,  m_IOPattern( MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY )
,  m_valid(false)
{
    m_impLib[ _T("general") ] = MFX_IMPL_SOFTWARE; // general session should be inited always

    if (argc < 6)
    {
        //VM_ASSERT(!"too few parameters");
        return;
    }    

    for (int i = 1; i < argc; i++)
    {
        int readVal;

        if ( 0 == _tcscmp(argv[i], _T("-sfmt")) )    
        {
            if (++i < argc)
            {                
                m_srcVideoFormat = String2VideoFormat(argv[i]);
            }
        } 
        else if ( 0 == _tcscmp(argv[i], _T("-dfmt")) )
        {
            if (++i < argc)
            {                
                m_dstVideoFormat = String2VideoFormat(argv[i]);
            }
        }
        else if ( 0 == _tcscmp(argv[i], _T("-w")) )
        {
            if (++i < argc)
            {
                _stscanf_s(argv[i], _T("%i"), &readVal);
                m_width = (mfxU16)readVal;
            }
        }
        else if ( 0 == _tcscmp(argv[i], _T("-h")) )
        {
            if (++i < argc)
            {
                _stscanf_s(argv[i], _T("%i"), &readVal);
                m_height = (mfxU16)readVal;
            }
        }
        else if ( 0 == _tcscmp(argv[i], _T("-b")) )
        {
            if (++i < argc)
            {
                _stscanf_s(argv[i], _T("%i"), &m_bitRate);
            }
        }
        else if ( 0 == _tcscmp(argv[i], _T("-f")) )
        {
            if (++i < argc)
            {
                _stscanf_s(argv[i], _T("%lf"), &m_frameRate);
            }
        }
        else if ( 0 == _tcscmp(argv[i], _T("-u")) )
        {
            if (++i < argc)
            {
                _stscanf_s(argv[i], _T("%i"), &readVal);
                m_targetUsage = (mfxU16)readVal;
            }
        }
        else if ( 0 == _tcscmp(argv[i], _T("-async")) )
        {
            if (++i < argc)
            {           
                _stscanf_s(argv[i], _T("%i"), &readVal);
                m_asyncDepth = (mfxU16)readVal;
                // async depth correction
                if (0 == m_asyncDepth) 
                {
                    m_asyncDepth = 1;
                }
            }
        }
        else if ( 0 == _tcscmp(argv[i], _T("-model")) )
        {
            if (++i < argc)
            {
                _stscanf_s(argv[i], _T("%i"), &readVal);
                m_usageModel = (mfxU16)readVal;
            }
        }
        else if ( 0 == _tcscmp(argv[i], _T("-n")) )
        {
            if (++i < argc)
            {
                _stscanf_s(argv[i], _T("%i"), &m_framesCount);
            }
        }

        // src/dst streams
        else if ( 0 == _tcscmp(argv[i], _T("-o")) )
        {
            if (++i < argc)
            {
                // save output file name                
                m_pDstFileName = argv[i];
            }
        }
        else if ( 0 == _tcscmp(argv[i], _T("-i")) )
        {
            if (++i < argc)
            {
                // save output file name                
                m_pSrcFileName = argv[i];
            }
        }

        // implementation of MFX library in general
        else if (0 == _tcscmp(argv[i], _T("-lib")) )
        {
            if( ++i < argc )
            {
                if (0 == _tcscmp(argv[i], _T("sw")) )
                {
                    m_impLib[_T("general")] = MFX_IMPL_SOFTWARE;
                }
                else if (0 == _tcscmp(argv[i], _T("hw")) )
                {
                    m_impLib[_T("general")] = MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D9;
                }
            }
        }
        // implementation of MFX library for individual component
        else if ( 0 == _tcscmp(argv[i], _T("-dec::sw")) )
        {
            m_impLib[_T("dec")] = MFX_IMPL_SOFTWARE;
        }
        else if ( 0 == _tcscmp(argv[i], _T("-dec::hw")) )
        {
            m_impLib[_T("dec")] = MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D9;
        }
        else if ( 0 == _tcscmp(argv[i], _T("-vpp::sw")) )
        {
            m_impLib[_T("vpp")] = MFX_IMPL_SOFTWARE;
        }
        else if ( 0 == _tcscmp(argv[i], _T("-vpp::hw")) )
        {
            m_impLib[_T("vpp")] = MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D9;
        }
        else if ( 0 == _tcscmp(argv[i], _T("-enc::sw")) )
        {
            m_impLib[_T("enc")] = MFX_IMPL_SOFTWARE;
        }
        else if ( 0 == _tcscmp(argv[i], _T("-enc::hw")) )
        {
            m_impLib[_T("enc")] = MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D9;
        }

        // iopattern
        else if( 0 == _tcscmp(argv[i], _T("-iopattern")) )
        {
            if( ++i < argc )
            {
                m_IOPattern = String2IOPattern( argv[i] );
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
    ////m_impLib[_T("dec")] = MFX_IMPL_SOFTWARE;
    ////m_impLib[_T("vpp")] = MFX_IMPL_HARDWARE;
    ////m_impLib[_T("enc")] = MFX_IMPL_HARDWARE;
    //m_width = m_width = 0;
    //if( m_usageModel > 0 )
    //{
    //    m_impLib[ _T("general") ] = MFX_IMPL_HARDWARE;
    //    m_impLib[ _T("vpp") ]    = MFX_IMPL_SOFTWARE;
    //    m_IOPattern = (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY);
    //    //m_width = m_width = 0;
    //    m_usageModel = USAGE_MODEL_8;
    //}


} // CommandLine::CommandLine(int argc, TCHAR *argv[])


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
    }

} // GetParam( AppParam& param )


void CommandLine::PrintInfo( void )
{
    if ( !m_valid ) 
    {
        return;
    }

    _tprintf(_T("<Configuration of the test:>\n"));
    _tprintf(_T("Input  format\t%s\n"),   VideoFormat2String( m_srcVideoFormat ));
    _tprintf(_T("Output format\t%s\n"),   VideoFormat2String( m_dstVideoFormat ));

    if( m_bitRate > 0 )
    {
        _tprintf(_T("BitRate\t\t%d\n"),     m_bitRate);
    }
    else
    {
        _tprintf(_T("BitRate\t\t%s\n"),     _T("default"));
    }
    _tprintf(_T("Target Usage\t%s\n"),    TargetUsageToStr(m_targetUsage) );
    _tprintf(_T("Async Depth\t%d\n"),     m_asyncDepth);
    _tprintf(_T("MFX Usage Model\t%d\n"), m_usageModel);
    
    //if( m_usageModel < USAGE_MODEL_6  )
    //{        
    //    _tprintf(_T("MFX implement:\t%s\n"), ImpLibrary2String( m_impLib[_T("general")] ));
    //}
    //else
    {
        _tprintf(_T("MFX DEC impl:\t%s\n"), ImpLibrary2String( (m_impLib.find(_T("dec")) != m_impLib.end()) ? m_impLib[_T("dec")] : m_impLib[_T("general")] ));
        if( IsVPPEnable() )
        {
            _tprintf(_T("MFX VPP impl:\t%s\n"), ImpLibrary2String( (m_impLib.find(_T("vpp")) != m_impLib.end()) ? m_impLib[_T("vpp")] : m_impLib[_T("general")] ));
        }
        _tprintf(_T("MFX ENC impl:\t%s\n"), ImpLibrary2String( (m_impLib.find(_T("enc")) != m_impLib.end()) ? m_impLib[_T("enc")] : m_impLib[_T("general")] ));
    }


    _tprintf(_T("IOPattern\t%s\n"), IOPattern2String(m_IOPattern) );

    PrintDllInfo();
  
} // void CommandLine::PrintInfo( void )


mfxU32 String2VideoFormat( TCHAR* arg )
{
    mfxU32 format = MFX_FOURCC_NV12;//default

    if ( 0 == _tcscmp(arg, _T("h264")) ) 
    {
        format = MFX_CODEC_AVC;
    } 
    else if ( 0 == _tcscmp(arg, _T("mpeg2")) ) 
    {
        format = MFX_CODEC_MPEG2;
    } 
    else if ( 0 == _tcscmp(arg, _T("vc1")) ) 
    {
        format = MFX_CODEC_VC1;
    }    

    return format;

} // mfxU32 Str2FourCC( TCHAR* strInput )


TCHAR* VideoFormat2String( mfxU32 FourCC )
{
  TCHAR* strFourCC = _T("h264");//default

  switch ( FourCC )
  {
  case MFX_CODEC_AVC:
    strFourCC = _T("h264");
    break;

  case MFX_CODEC_MPEG2:
    strFourCC = _T("mpeg2");
    break;

  case MFX_CODEC_VC1:
    strFourCC = _T("vc1");
    break;
   
  default:
      strFourCC = _T("NV12");
      break;
  }

  return strFourCC;

} // TCHAR* VideoFormat2String( mfxU32 FourCC )


TCHAR* ImpLibrary2String(mfxIMPL impl)
{
    switch(impl)
    {
        case MFX_IMPL_AUTO:
        {
            return _T("auto");
        }
        case MFX_IMPL_SOFTWARE:
        {
            return _T("SW");
        }
        case (MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D9):
        {
            return _T("HW | via D3D9");
        }
        case (MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11):
        {
            return _T("HW | via D3D11");
        }
        default:
        {
            return _T("unknown");
        }
    }

} // TCHAR* ImpLibraryToStr(mfxIMPL impl)


void PrintDllInfo( void )
{
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

} // void PrintDllInfo( void )


mfxU16 String2IOPattern( TCHAR* strInput )
{
    mfxU16 IOPattern = 0;

    if ( 0 == _tcscmp(strInput, _T("d3d_to_d3d")) ) 
    {
        IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    } 
    else if ( 0 == _tcscmp(strInput, _T("d3d_to_sys")) ) 
    {
        IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    } 
    else if ( 0 == _tcscmp(strInput, _T("sys_to_d3d")) ) 
    {
        IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    } 
    else if ( 0 == _tcscmp(strInput, _T("sys_to_sys")) )
    {
        IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    } 
    else if( 0 == _tcscmp(strInput, _T("d3d")) )
    {
        IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }
    else if( 0 == _tcscmp(strInput, _T("sys")) )
    {
        IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }
    return IOPattern;

} // mfxU16 String2IOPattern( TCHAR* strInput )


TCHAR* IOPattern2String(mfxU16 IOPattern)
{
    switch(IOPattern)
    {
        // multi pattern
        case (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY):
        {
            return _T("video");
        }
        case (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY):
        {
            return _T("video_to_system");
        }
        case (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY):
        {
            return _T("system_to_video");
        }
        case (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY):
        {
            return _T("system");
        }           
        default:
        {
            return _T("unknown");
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
