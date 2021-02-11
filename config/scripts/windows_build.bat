REM These environment variables shoul be set: MFX_HOME MEDIASDK_ROOT INTELMEDIASDK_FFMPEG_ROOT MEDIASDK_TRS_MODULE_PATH MINIDDK_VERSION
@echo off 
setlocal

set WORKSPACE="C:\workspace"
REM all ICC targets
REM set ICC_TARGETS=mfxplugin_hw64 mfx_h265fei_hw64 mfx_hevcd_sw64 mfx_hevce_gacc64 mfx_hevce_sw64 hevc_pp_atom hevc_pp_avx2 hevc_pp_dispatcher hevc_pp_px hevc_pp_sse4 hevc_pp_ssse3 genx_h265_encode_embeded av1_enc_hw av1_pp_avx2 av1_pp_px av1_pp_ssse3 genx_av1_encode_embeded hevce_tests av1_opt mfx_av1e_gacc64 h265_fei av1_fei

set ICC_COMPILER="C:/Program Files (x86)/IntelSWTools/compilers_and_libraries_2020/windows/bin/intel64/icl.exe"
set ICC_TARGETS_64=hevce_tests mfxplugin_hw64 mfxplugin64_av1e_gacc
set ICC_TARGETS_32=mfxplugin_hw32 mfxplugin32_av1e_gacc
set NO_COLOR=1
set CLICOLOR_FORCE=0

if exist %WORKSPACE%\Build rmdir %WORKSPACE%\Build /S /Q
mkdir %WORKSPACE%\Build\uwp\x32 %WORKSPACE%\Build\uwp\x64 %WORKSPACE%\Build\icc\x32 %WORKSPACE%\Build\icc\x64 %WORKSPACE%\Build\msvc\x32 %WORKSPACE%\Build\msvc\x64 %WORKSPACE%\Build\mfts\x32 %WORKSPACE%\Build\mfts\x64 %WORKSPACE%\Build\VPL_build\x32 %WORKSPACE%\Build\VPL_build\x64

set BUILDTREE=%WORKSPACE%\Build\VPL_build\x64
cmake -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib -A x64 -DCMAKE_C_FLAGS_RELEASE="-Wall" -DCMAKE_CXX_FLAGS_RELEASE="-Wall" -DENABLE_X11_DRI3=ON -DENABLE_WAYLAND=ON -DENABLE_TEXTLOG=ON -DENABLE_STAT=ON -DBUILD_ALL=ON -DENABLE_OPENCL=OFF -DMFX_DISABLE_SW_FALLBACK=OFF -DAPI=2.1 > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x64: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release --target libmfx64-gen >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x64: build failed with %ERRORLEVEL%. & exit /B 1)

set BUILDTREE=%WORKSPACE%\Build\VPL_build\x32
cmake -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib -A Win32 -DCMAKE_C_FLAGS_RELEASE="-Wall" -DCMAKE_CXX_FLAGS_RELEASE="-Wall" -DENABLE_X11_DRI3=ON -DENABLE_WAYLAND=ON -DENABLE_TEXTLOG=ON -DENABLE_STAT=ON -DBUILD_ALL=ON -DENABLE_OPENCL=OFF -DMFX_DISABLE_SW_FALLBACK=OFF -DAPI=2.1 > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x32: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release --target libmfx32-gen >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x32: build failed with %ERRORLEVEL%. & exit /B 1)

set BUILDTREE=%WORKSPACE%\Build\msvc\x64
cmake -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib -A x64 -DAPI=latest -DBUILD_VAL_TOOLS=ON -DBUILD_TESTS=ON -DMFX_ENABLE_LP_LOOKAHEAD=ON > %BUILDTREE%\build.log  2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- MSDK x64: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release >> %BUILDTREE%\build.log  2>&1
rem cmake --build %WORKSPACE%\Build\msvc\x64 -j %NUMBER_OF_PROCESSORS% --config Debug --target mfx_loader_dll_hw64
if "%ERRORLEVEL%" neq "0" (
  echo --- MSDK x64: build failed with %ERRORLEVEL%. & exit /B 1)

set BUILDTREE=%WORKSPACE%\Build\msvc\x32
cmake -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib -A Win32 -DAPI=latest -DBUILD_VAL_TOOLS=ON -DBUILD_TESTS=ON -DMFX_ENABLE_LP_LOOKAHEAD=ON > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- MSDK x32: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- MSDK x32: build failed with %ERRORLEVEL%. & exit /B 1)

set BUILDTREE=%WORKSPACE%\Build\uwp\x64
cmake -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0.18362.0 > %BUILDTREE%\build.log  2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- UWP x64: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release >> %BUILDTREE%\build.log  2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- UWP x64: build failed with %ERRORLEVEL%. & exit /B 1)

set BUILDTREE=%WORKSPACE%\Build\uwp\x32
cmake -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib -A Win32 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0.18362.0 > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- UWP x32: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- UWP x32: build failed with %ERRORLEVEL%. & exit /B 1)

set BUILDTREE=%WORKSPACE%\Build\mfts\x64
cmake -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-mfts -A x64 -DAPI=latest > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- MFTS x64: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- MFTS x64: Release build failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config ReleaseInternal >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- MFTS x64: ReleaseInternal build failed with %ERRORLEVEL%. & exit /B 1)


set BUILDTREE=%WORKSPACE%\Build\mfts\x32
cmake -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-mfts -A Win32 -DAPI=latest > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- MFTS x32: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- MFTS x32: Release build failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config ReleaseInternal >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- MFTS x32: ReleaseInternal build failed with %ERRORLEVEL%. & exit /B 1)

call "C:\Program Files (x86)\IntelSWTools\compilers_and_libraries_2020\windows\bin\compilervars.bat" -arch Intel64 vs2019

set BUILDTREE=%WORKSPACE%\Build\icc\x64
cmake -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib -G"Ninja" -DCMAKE_SYSTEM_VERSION=10.0.18362 -DCMAKE_CXX_COMPILER=%ICC_COMPILER% -DCMAKE_C_COMPILER=%ICC_COMPILER% -DAPI=latest -DENABLE_HEVC=ON -DENABLE_AV1=ON -DENABLE_HEVC_ON_GCC=ON > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- ICC x64: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release --target %ICC_TARGETS_64% >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- ICC x64: build failed with %ERRORLEVEL%. & exit /B 1)

call "C:\Program Files (x86)\IntelSWTools\compilers_and_libraries_2020\windows\bin\compilervars.bat" -arch ia32 vs2019

set BUILDTREE=%WORKSPACE%\Build\icc\x32
cmake -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib -G"Ninja" -DCMAKE_SYSTEM_VERSION=10.0.18362 -DCMAKE_CXX_COMPILER=%ICC_COMPILER% -DCMAKE_C_COMPILER=%ICC_COMPILER% -DAPI=latest -DENABLE_HEVC=ON -DENABLE_AV1=ON -DENABLE_HEVC_ON_GCC=ON > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- ICC x32: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release --target %ICC_TARGETS_32% >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- ICC x32: build failed with %ERRORLEVEL%. & exit /B 1)

if exist %WORKSPACE%\output\packages rmdir %WORKSPACE%\output\packages
mkdir %WORKSPACE%\output\packages
powershell $global:ProgressPreference = 'SilentlyContinue'; Compress-Archive -Path %WORKSPACE%\Build\icc\* -DestinationPath %WORKSPACE%\output\packages\icc.zip
powershell $global:ProgressPreference = 'SilentlyContinue'; Compress-Archive -Path %WORKSPACE%\Build\msvc\* -DestinationPath %WORKSPACE%\output\packages\msvc.zip
powershell $global:ProgressPreference = 'SilentlyContinue'; Compress-Archive -Path %WORKSPACE%\Build\uwp\* -DestinationPath %WORKSPACE%\output\packages\uwp.zip
powershell $global:ProgressPreference = 'SilentlyContinue'; Compress-Archive -Path %WORKSPACE%\Build\mfts\* -DestinationPath %WORKSPACE%\output\packages\mfts.zip
