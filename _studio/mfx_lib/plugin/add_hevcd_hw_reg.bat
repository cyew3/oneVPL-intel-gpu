:: This scritp adds registry keys for HEVC HW DECODE plugin loading by dispatcher
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

:: set GIUD
set HEVC_HW_PLUGIN_DECODE_GUID=33a61c0b4c27454ca8d85dde757c6f8e
:: set plugin dll names
set HEVC_HW_X86_PLUGIN_DECODE_NAME=mfxplugin32_hevcd_hw.dll
set HEVC_HW_X64_PLUGIN_DECODE_NAME=mfxplugin64_hevcd_hw.dll
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
set MSDK_PLUGIN_DISPATCH_REGISTRY=HKEY_LOCAL_MACHINE\SOFTWARE\Intel\MediaSDK\Dispatch
set MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264=HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Intel\MediaSDK\Dispatch
:: Sub-key name
set MSDK_PLUGIN_SUBKEY=Plugin

:: For each subkey in Dispatch key
for /f "tokens=*" %%A in ('reg query %MSDK_PLUGIN_DISPATCH_REGISTRY% 2^>NUL') do (
    :: HEVC DECODE PLUGIN
    reg add "%%A\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v Type /t REG_DWORD /d 00000001 /f
    reg add "%%A\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v CodecID /t REG_DWORD /d %MSDK_HEVC_CODEC_ID% /f
    reg add "%%A\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v GUID /t REG_BINARY /d %HEVC_HW_PLUGIN_DECODE_GUID% /f
    reg add "%%A\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v Name /t REG_SZ /d "Intel (R) Media SDK HW plugin for HEVC DECODE" /f
    if "%PROCESSOR_ARCHITECTURE%"=="x86" (
    :: for x86 systems ::
        reg add "%%A\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v Path /t REG_SZ /d "%PATH_TO_BRANCH%build\\win_Win32\\bin\\%HEVC_HW_PLUGIN_DECODE_GUID%\\%HEVC_HW_X86_PLUGIN_DECODE_NAME%" /f
    ) else (
    :: for x64 systems
        reg add "%%A\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v Path /t REG_SZ /d "%PATH_TO_BRANCH%build\\win_x64\\bin\\%HEVC_HW_PLUGIN_DECODE_GUID%\\%HEVC_HW_X64_PLUGIN_DECODE_NAME%" /f
    )
    reg add "%%A\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v PluginVersion /t REG_DWORD /d %PLUGIN_VERSION% /f
    reg add "%%A\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v APIVersion /t REG_DWORD /d %MSDK_API_VER% /f
    reg add "%%A\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v Default /t REG_DWORD /d 0 /f
)
:: The same for Wow6432Node
if NOT "%PROCESSOR_ARCHITECTURE%"=="x86" (
    for /f "tokens=*" %%B in ('reg query %MSDK_PLUGIN_DISPATCH_REGISTRY_WOW3264% 2^>NUL') do (
        reg add "%%B\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v Type /t REG_DWORD /d 00000001 /f
        reg add "%%B\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v CodecID /t REG_DWORD /d %MSDK_HEVC_CODEC_ID% /f
        reg add "%%B\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v GUID /t REG_BINARY /d %HEVC_HW_PLUGIN_DECODE_GUID% /f
        reg add "%%B\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v Name /t REG_SZ /d "Intel (R) Media SDK HW plugin for HEVC DECODE" /f
        reg add "%%B\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v Path /t REG_SZ /d "%PATH_TO_BRANCH%build\\win_Win32\\bin\\%HEVC_HW_PLUGIN_DECODE_GUID%\\%HEVC_HW_X86_PLUGIN_DECODE_NAME%" /f
        reg add "%%B\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v PluginVersion /t REG_DWORD /d %PLUGIN_VERSION% /f
        reg add "%%B\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v APIVersion /t REG_DWORD /d %MSDK_API_VER% /f
        reg add "%%B\%MSDK_PLUGIN_SUBKEY%\%HEVC_HW_PLUGIN_DECODE_GUID%" /v Default /t REG_DWORD /d 0 /f
    )
)

timeout 5