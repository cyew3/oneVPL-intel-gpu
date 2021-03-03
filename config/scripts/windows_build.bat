REM These environment variables shoul be set: MFX_HOME MEDIASDK_ROOT INTELMEDIASDK_FFMPEG_ROOT MEDIASDK_TRS_MODULE_PATH MINIDDK_VERSION
@echo off 
setlocal

set WORKSPACE="C:\workspace"
REM set COMMON_VPL_TARGETS=mfx_player mfx_transcoder msdk_gmock sample_multi_transcode
set NO_COLOR=1
set CLICOLOR_FORCE=0

if exist %WORKSPACE%\Build rmdir %WORKSPACE%\Build /S /Q
mkdir %WORKSPACE%\Build\vpl\x32 %WORKSPACE%\Build\vpl\x64

set BUILDTREE=%WORKSPACE%\Build\vpl\x64
cmake -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib -A x64 -DENABLE_TEXTLOG=ON -DENABLE_STAT=ON -DBUILD_VAL_TOOLS=ON -DBUILD_ALL=ON -DENABLE_OPENCL=OFF -DMFX_DISABLE_SW_FALLBACK=OFF -DAPI=2.2 > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x64: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release --target libmfx64-gen mfx_player mfx_transcoder msdk_gmock sample_multi_transcode >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x64: build failed with %ERRORLEVEL%. & exit /B 1)

set BUILDTREE=%WORKSPACE%\Build\vpl\x32
cmake -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib -A Win32 -DENABLE_TEXTLOG=ON -DENABLE_STAT=ON -DBUILD_VAL_TOOLS=ON -DBUILD_ALL=ON -DENABLE_OPENCL=OFF -DMFX_DISABLE_SW_FALLBACK=OFF -DAPI=2.2 > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x32: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release --target libmfx32-gen mfx_player mfx_transcoder msdk_gmock sample_multi_transcode >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x32: build failed with %ERRORLEVEL%. & exit /B 1)

powershell $global:ProgressPreference = 'SilentlyContinue'; Compress-Archive -Path %WORKSPACE%\Build\vpl\* -DestinationPath %WORKSPACE%\output\packages\vpl.zip
