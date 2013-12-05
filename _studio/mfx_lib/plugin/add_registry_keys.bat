:: This scritp adds registry keys for loading plugins by dispatcher
:: Run this script as administrator

@echo off
:: Check for admin rights:
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"
:-------------------------------------
REM --> If error flag set, we do not have admin rights
if '%errorlevel%' NEQ '0' (
    echo Admin rights was not detected
    echo Press any key for UAC prompt for administrator rights
    timeout 15
    goto UACPrompt
) else ( goto gotAdmin )

:UACPrompt
    :: make a UAC request
    echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs"
    :: run this (%~0) script as admin
    echo UAC.ShellExecute "%~0", "", "", "runas", 1 >> "%temp%\getadmin.vbs"

    "%temp%\getadmin.vbs"
    exit /B

:gotAdmin
    if exist "%temp%\getadmin.vbs" ( del "%temp%\getadmin.vbs" )
    pushd "%CD%"
    CD /D "%~dp0"
:--------------------------------------

::@echo on
:: set GIUDs
set HEVC_SW_PLUGIN_ENCODE_GUID=2fca99749fdb49aeb121a5b63ef568f7
set HEVC_SW_PLUGIN_DECODE_GUID=15dd936825ad475ea34e35f3f54217a6
set VPP_SW_PLUGIN_GUID=1840bce0ee644d098ed62f91ae4b6b61
:: set plugin dll names
set HEVC_SW_X86_PLUGIN_ENCODE_NAME=mfxplugin32_hevce_sw.dll
set HEVC_SW_X86_PLUGIN_DECODE_NAME=mfxplugin32_hevcd_sw.dll
set  VPP_SW_X86_PLUGIN_NAME=mfxplugin32_vpp_sw
set HEVC_SW_X64_PLUGIN_ENCODE_NAME=mfxplugin64_hevce_sw.dll
set HEVC_SW_X64_PLUGIN_DECODE_NAME=mfxplugin64_hevcd_sw.dll
set  VPP_SW_X64_PLUGIN_NAME=mfxplugin64_vpp_sw.dll
:: set API ver (decimal 264 = hex 0x018)
set MSDK_API_VER=264
:: set HEVC CodecID (decimal)
set MSDK_HEVC_CODEC_ID=1129727304
:: set Plugin Version
set PLUGIN_VERSION=00000001
::  pseudo-variable expands to the directory with a batch file
set PATH_TO_BRANCH=%~dp0
::  remove sub-folders
set PATH_TO_BRANCH=%PATH_TO_BRANCH:_studio\mfx_lib\plugin\=%
:: replace backslah with double backslash
set PATH_TO_BRANCH=%PATH_TO_BRANCH:\=\\%
:: Uncomment and change for manual setup
     :: set PATH_TO_BRANCH=C:\\MediaSDK
:: Path to Dispatch registry key
set MSDK_PLUGIN_DISPATCH_REGISTRY=HKEY_LOCAL_MACHINE\SOFTWARE\Intel\MediaSDK\Dispatch\Plugin
set MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264=HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Intel\MediaSDK\Dispatch\Plugin

:: HEVC ENCODE PLUGIN
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v Type /t REG_DWORD /d 00000002 /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v CodecID /t REG_DWORD /d %MSDK_HEVC_CODEC_ID% /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v GUID /t REG_BINARY /d %HEVC_SW_PLUGIN_ENCODE_GUID% /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v Name /t REG_SZ /d "Intel (R) Media SDK plugin for HEVC encode" /f
if "%PROCESSOR_ARCHITECTURE%"=="x86" (
:: for x86 systems :: 
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v Path /t REG_SZ /d "%PATH_TO_BRANCH%build\\win_Win32\\bin\\%HEVC_SW_PLUGIN_ENCODE_GUID%\\%HEVC_SW_X86_PLUGIN_ENCODE_NAME%" /f
) else (
:: for x64 systems
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v Path /t REG_SZ /d "%PATH_TO_BRANCH%build\\win_x64\\bin\\%HEVC_SW_PLUGIN_ENCODE_GUID%\\%HEVC_SW_X64_PLUGIN_ENCODE_NAME%" /f
)
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v PluginVersion /t REG_DWORD /d %PLUGIN_VERSION% /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v APIVersion /t REG_DWORD /d %MSDK_API_VER% /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v Default /t REG_DWORD /d 0 /f

:: Wow6432Node for HEVC ENCODE PLUGIN
if NOT "%PROCESSOR_ARCHITECTURE%"=="x86" (
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v Type /t REG_DWORD /d 00000002 /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v CodecID /t REG_DWORD /d %MSDK_HEVC_CODEC_ID% /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v GUID /t REG_BINARY /d %HEVC_SW_PLUGIN_ENCODE_GUID% /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v Name /t REG_SZ /d "Intel (R) Media SDK plugin for HEVC encode" /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v Path /t REG_SZ /d "%PATH_TO_BRANCH%build\\win_Win32\\bin\\%HEVC_SW_PLUGIN_ENCODE_GUID%\\%HEVC_SW_X86_PLUGIN_ENCODE_NAME%" /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v PluginVersion /t REG_DWORD /d %PLUGIN_VERSION% /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v APIVersion /t REG_DWORD /d %MSDK_API_VER% /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_ENCODE_GUID%" /v Default /t REG_DWORD /d 0 /f
)


:: HEVC DECODE PLUGIN
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v Type /t REG_DWORD /d 00000001 /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v CodecID /t REG_DWORD /d %MSDK_HEVC_CODEC_ID% /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v GUID /t REG_BINARY /d %HEVC_SW_PLUGIN_DECODE_GUID% /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v Name /t REG_SZ /d "Intel (R) Media SDK plugin for HEVC DECODE" /f
if "%PROCESSOR_ARCHITECTURE%"=="x86" (
:: for x86 systems :: 
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v Path /t REG_SZ /d "%PATH_TO_BRANCH%build\\win_Win32\\bin\\%HEVC_SW_PLUGIN_DECODE_GUID%\\%HEVC_SW_X86_PLUGIN_DECODE_NAME%" /f
) else (
:: for x64 systems
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v Path /t REG_SZ /d "%PATH_TO_BRANCH%build\\win_x64\\bin\\%HEVC_SW_PLUGIN_DECODE_GUID%\\%HEVC_SW_X64_PLUGIN_DECODE_NAME%" /f
)
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v PluginVersion /t REG_DWORD /d %PLUGIN_VERSION% /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v APIVersion /t REG_DWORD /d %MSDK_API_VER% /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v Default /t REG_DWORD /d 0 /f

:: Wow6432Node for HEVC DECODE PLUGIN
if NOT "%PROCESSOR_ARCHITECTURE%"=="x86" (
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v Type /t REG_DWORD /d 00000001 /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v CodecID /t REG_DWORD /d %MSDK_HEVC_CODEC_ID% /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v GUID /t REG_BINARY /d %HEVC_SW_PLUGIN_DECODE_GUID% /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v Name /t REG_SZ /d "Intel (R) Media SDK plugin for HEVC DECODE" /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v Path /t REG_SZ /d "%PATH_TO_BRANCH%build\\win_Win32\\bin\\%HEVC_SW_PLUGIN_DECODE_GUID%\\%HEVC_SW_X86_PLUGIN_DECODE_NAME%" /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v PluginVersion /t REG_DWORD /d %PLUGIN_VERSION% /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v APIVersion /t REG_DWORD /d %MSDK_API_VER% /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%HEVC_SW_PLUGIN_DECODE_GUID%" /v Default /t REG_DWORD /d 0 /f
)

:: VPP PLUGIN
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%VPP_SW_PLUGIN_GUID%" /v Type /t REG_DWORD /d 00000003 /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%VPP_SW_PLUGIN_GUID%" /v Name /t REG_SZ /d "Intel (R) Media SDK plugin for VPP" /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%VPP_SW_PLUGIN_GUID%" /v GUID /t REG_BINARY /d %VPP_SW_PLUGIN_GUID% /f
if "%PROCESSOR_ARCHITECTURE%"=="x86" (
:: for x86 systems :: 
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%VPP_SW_PLUGIN_GUID%" /v Path /t REG_SZ /d "%PATH_TO_BRANCH%build\\win_Win32\\bin\\%VPP_SW_PLUGIN_GUID%\\%VPP_SW_X86_PLUGIN_NAME%.dll" /f
) else (
:: for x64 systems
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%VPP_SW_PLUGIN_GUID%" /v Path /t REG_SZ /d "%PATH_TO_BRANCH%build\\win_x64\\bin\\%VPP_SW_PLUGIN_GUID%\\%VPP_SW_X64_PLUGIN_NAME%" /f
)
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%VPP_SW_PLUGIN_GUID%" /v PluginVersion /t REG_DWORD /d %PLUGIN_VERSION% /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%VPP_SW_PLUGIN_GUID%" /v APIVersion /t REG_DWORD /d %MSDK_API_VER% /f
reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY%\%VPP_SW_PLUGIN_GUID%" /v Default /t REG_DWORD /d 0 /f

:: Wow6432Node for VPP PLUGIN
if NOT "%PROCESSOR_ARCHITECTURE%"=="x86" (
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%VPP_SW_PLUGIN_GUID%" /v Type /t REG_DWORD /d 00000003 /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%VPP_SW_PLUGIN_GUID%" /v Name /t REG_SZ /d "Intel (R) Media SDK plugin for VPP" /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%VPP_SW_PLUGIN_GUID%" /v GUID /t REG_BINARY /d %VPP_SW_PLUGIN_GUID% /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%VPP_SW_PLUGIN_GUID%" /v Path /t REG_SZ /d "%PATH_TO_BRANCH%build\\win_Win32\\bin\\%VPP_SW_PLUGIN_GUID%\\%VPP_SW_X86_PLUGIN_NAME%.dll" /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%VPP_SW_PLUGIN_GUID%" /v PluginVersion /t REG_DWORD /d %PLUGIN_VERSION% /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%VPP_SW_PLUGIN_GUID%" /v APIVersion /t REG_DWORD /d %MSDK_API_VER% /f
    reg add "%MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264%\%VPP_SW_PLUGIN_GUID%" /v Default /t REG_DWORD /d 0 /f
)

timeout 15