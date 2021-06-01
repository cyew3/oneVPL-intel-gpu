REM These environment variables shoul be set: MFX_HOME MEDIASDK_ROOT INTELMEDIASDK_FFMPEG_ROOT MEDIASDK_TRS_MODULE_PATH MINIDDK_VERSION
@echo off 
rem set WORKSPACE="C:\workspace"
REM set COMMON_VPL_TARGETS=mfx_player mfx_transcoder msdk_gmock sample_multi_transcode
set NO_COLOR=1
set CLICOLOR_FORCE=0

if exist %WORKSPACE%\Build rmdir %WORKSPACE%\Build /S /Q
mkdir %WORKSPACE%\Build\vpl\x32 %WORKSPACE%\Build\vpl\x64

set MFX_HOME=%WORKSPACE%\sources\mdp_msdk-lib
set MEDIASDK_ROOT=%WORKSPACE%\sources\msdk_root
set INTELMEDIASDK_FFMPEG_ROOT=%WORKSPACE%\sources\msdk_root\ffmpeg

set PATH=%PATH%;%WORKSPACE%\build_tools\yasm-win64

cd %WORKSPACE%\build_tools\cmake\bin

call %WORKSPACE%\build_tools\ewdk\BuildEnv\SetupBuildEnv.cmd amd64
call %WORKSPACE%\build_tools\ewdk\BuildEnv\SetupVSEnv.cmd

set MINIDDK_VERSION=%Version_Number%

set WindowsTargetPlatformVersion=%Version_Number%
set VSCMD_DEBUG=1
set WindowsSDKLibVersion=%Version_Number%\
set WindowsSDKVersion=%Version_Number%\
set WindowsSdk80Path=%WDKContentRoot%
set WindowsSdkDir_10=%WDKContentRoot%
set WindowsSdkDir_80=%WDKContentRoot%
set WindowsSdkDir_81=%WDKContentRoot%

set VCLibDirMod=spectre\

set KIT_SHARED_IncludePath=%WDKContentRoot%Include\%Version_Number%\shared
set UCRT_IncludePath=%WDKContentRoot%Include\%Version_Number%\ucrt
set UM_IncludePath=%WDKContentRoot%Include\%Version_Number%\um

set INCLUDE_ORIG=%INCLUDE%
set INCLUDE=%VCToolsInstallDir%include;%UM_IncludePath%;%UCRT_IncludePath%;%KIT_SHARED_IncludePath%;%INCLUDE_ORIG%

rem set KM_LibPath=%WDKContentRoot%Lib\%Version_Number%\km\x64
set UCRT_LibPath=%WDKContentRoot%Lib\%Version_Number%\ucrt\x64
set UM_LibPath=%WDKContentRoot%Lib\%Version_Number%\um\x64

set LIB_ORIG=%LIB%
set LIB=%VCToolsInstallDir%\lib\%VCLibDirMod%x64;%UM_LibPath%;%UCRT_LibPath%;%LIB_ORIG%

rem Altering PATH to allow detours find sn.exe
set PATH_ORIG=%PATH%
set PATH=%PATH_ORIG%;%WORKSPACE%\build_tools\ewdk\Program Files\Microsoft SDKs\Windows\v10.0A\bin\NETFX 4.8 Tools\x64

REM Needed so cmake can find the custom path to WDK and SDK.
set CMAKE_WINDOWS_KITS_10_DIR=%WDKContentRoot%
set UseMultiToolTask=true


rem call  "%WORKSPACE%\build_tools\ewdk\Program Files\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 %Version_Number% -vcvars_spectre_libs=spectre

rem Check Env vars difference

rem cmd.exe
rem -A x64 -T host=x64  -DCMAKE_CXX_FLAGS="/MP8" -DCMAKE_EXE_LINKER_FLAGS_RELEASE="/CGTHREADS:8" -DCMAKE_SHARED_LINKER_FLAGS_RELEASE="/CGTHREADS:8"

set BUILDTREE=%WORKSPACE%\Build\vpl\x64
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_VERSION=%Version_Number% -DENABLE_TEXTLOG=ON -DENABLE_STAT=ON -DBUILD_VAL_TOOLS=ON -DBUILD_ALL=ON -DENABLE_OPENCL=OFF -DMFX_DISABLE_SW_FALLBACK=OFF -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib -DCMAKE_MAKE_PROGRAM="%WORKSPACE%\build_tools\ninja\ninja.exe" -DCMAKE_EXE_LINKER_FLAGS_RELEASE="/CGTHREADS:8" -DCMAKE_SHARED_LINKER_FLAGS_RELEASE="/CGTHREADS:8" > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x64: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x64: build failed with %ERRORLEVEL%. & exit /B 1)


set UCRT_LibPath=%WDKContentRoot%Lib\%Version_Number%\ucrt\x86
set UM_LibPath=%WDKContentRoot%Lib\%Version_Number%\um\x86

set LIB=%VCToolsInstallDir%\lib\%VCLibDirMod%x86;%VCToolsInstallDir%\atlmfc\lib\%VCLibDirMod%x86;%UM_LibPath%;%UCRT_LibPath%;%LIB_ORIG%
set VSCMD_ARG_TGT_ARCH=x86


set PATH=%PATH_ORIG%;%WORKSPACE%\build_tools\ewdk\Program Files\Microsoft SDKs\Windows\v10.0A\bin\NETFX 4.8 Tools
SET PATH=%PATH:Hostx64\x64=Hostx64\x86%

set

set BUILDTREE=%WORKSPACE%\Build\vpl\x32
cmake  -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_VERSION=%Version_Number% -DENABLE_TEXTLOG=ON -DENABLE_STAT=ON -DBUILD_VAL_TOOLS=ON -DBUILD_ALL=ON -DENABLE_OPENCL=OFF -DMFX_DISABLE_SW_FALLBACK=OFF -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib -DCMAKE_MAKE_PROGRAM="%WORKSPACE%\build_tools\ninja\ninja.exe" -DCMAKE_EXE_LINKER_FLAGS_RELEASE="/CGTHREADS:8" -DCMAKE_SHARED_LINKER_FLAGS_RELEASE="/CGTHREADS:8" > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x32: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x32: build failed with %ERRORLEVEL%. & exit /B 1)

cd %WORKSPACE% 
.\build_tools\7zip-win64\7z.exe a -tzip -bt -bd -slp -mx=3 -mmt output\packages\windows\vpl.zip "Build\vpl\x??\__???" -xr"!*.ilk"
