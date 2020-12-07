REM These environment variables shoul be set: MFX_HOME MEDIASDK_ROOT INTELMEDIASDK_FFMPEG_ROOT MEDIASDK_TRS_MODULE_PATH MINIDDK_VERSION
@echo off 
setlocal

set WORKSPACE="C:\workspace"
set ICC_TARGETS=mfxplugin_hw64 mfx_h265fei_hw64 mfx_hevcd_sw64 mfx_hevce_gacc64 mfx_hevce_sw64 hevc_pp_atom hevc_pp_avx2 hevc_pp_dispatcher hevc_pp_px hevc_pp_sse4 hevc_pp_ssse3 genx_h265_encode_embeded av1_enc_hw av1_pp_avx2 av1_pp_px av1_pp_ssse3 genx_av1_encode_embeded
REM set ICC_TARGETS=hevce_tests av1_opt mfx_av1e_gacc64 h265_fei av1_fei - not working for now

if exist %WORKSPACE%\Build rmdir %WORKSPACE%\Build /S /Q
mkdir %WORKSPACE%\Build\uwp\x32 %WORKSPACE%\Build\uwp\x64 %WORKSPACE%\Build\icc %WORKSPACE%\Build\msvc\x32 %WORKSPACE%\Build\msvc\x64 %WORKSPACE%\Build\mfts\x32 %WORKSPACE%\Build\mfts\x64 %WORKSPACE%\Build\VPL_build\x32 %WORKSPACE%\Build\VPL_build\x64

call "C:\Program Files (x86)\IntelSWTools\compilers_and_libraries_2020\windows\bin\compilervars.bat" -arch Intel64 vs2019

cmake -B%WORKSPACE%\Build\icc -H%WORKSPACE%\sources\mdp_msdk-lib -G"Ninja Multi-Config" -DCMAKE_SYSTEM_VERSION=10.0.18362 -DCMAKE_CXX_COMPILER="C:/Program Files (x86)/IntelSWTools/compilers_and_libraries_2020/windows/bin/intel64/icl.exe" -DCMAKE_C_COMPILER="C:/Program Files (x86)/IntelSWTools/compilers_and_libraries_2020/windows/bin/intel64/icl.exe" -DAPI=latest -DENABLE_HEVC=ON -DENABLE_AV1=ON -DENABLE_HEVC_ON_GCC=ON
for %%t in (%ICC_TARGETS%) do (
   echo Try to build %%t
   cmake --build %WORKSPACE%\Build\icc -j %NUMBER_OF_PROCESSORS% --config Release --target %%t
)

cmake -B%WORKSPACE%\Build\msvc\x64 -H%WORKSPACE%\sources\mdp_msdk-lib -A x64 -DAPI=latest -DBUILD_VAL_TOOLS=ON -DBUILD_TESTS=ON -DMFX_ENABLE_LP_LOOKAHEAD=ON
cmake --build %WORKSPACE%\Build\msvc\x64 -j %NUMBER_OF_PROCESSORS% --config RelWithDebInfo
cmake --build %WORKSPACE%\Build\msvc\x64 -j %NUMBER_OF_PROCESSORS% --config Debug --target mfx_loader_dll_hw64

cmake -B%WORKSPACE%\Build\msvc\x32 -H%WORKSPACE%\sources\mdp_msdk-lib -A Win32 -DAPI=latest -DBUILD_VAL_TOOLS=ON -DBUILD_TESTS=ON -DMFX_ENABLE_LP_LOOKAHEAD=ON
cmake --build %WORKSPACE%\Build\msvc\x32 -j %NUMBER_OF_PROCESSORS% --config RelWithDebInfo


cmake -B%WORKSPACE%\Build\uwp\x64 -H%WORKSPACE%\sources\mdp_msdk-lib -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0.18362.0
cmake --build %WORKSPACE%\Build\uwp\x64 -j %NUMBER_OF_PROCESSORS% --config RelWithDebInfo

cmake -B%WORKSPACE%\Build\uwp\x32 -H%WORKSPACE%\sources\mdp_msdk-lib -A Win32 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0.18362.0
cmake --build %WORKSPACE%\Build\uwp\x32 -j %NUMBER_OF_PROCESSORS% --config RelWithDebInfo


cmake -B%WORKSPACE%\Build\mfts\x64 -H%WORKSPACE%\sources\mdp_msdk-mfts -A x64 -DAPI=latest
cmake --build %WORKSPACE%\Build\mfts\x64 -j %NUMBER_OF_PROCESSORS% --config Release
cmake --build %WORKSPACE%\Build\mfts\x64 -j %NUMBER_OF_PROCESSORS% --config ReleaseInternal

cmake -B%WORKSPACE%\Build\mfts\x32 -H%WORKSPACE%\sources\mdp_msdk-mfts -A Win32 -DAPI=latest
cmake --build %WORKSPACE%\Build\mfts\x32 -j %NUMBER_OF_PROCESSORS% --config Release
cmake --build %WORKSPACE%\Build\mfts\x32 -j %NUMBER_OF_PROCESSORS% --config ReleaseInternal


cmake -B%WORKSPACE%\Build\VPL_build\x64 -H%WORKSPACE%\sources\mdp_msdk-lib -A x64 -DCMAKE_C_FLAGS_RELEASE="-Wall" -DCMAKE_CXX_FLAGS_RELEASE="-Wall" -DENABLE_X11_DRI3=ON -DENABLE_WAYLAND=ON -DENABLE_TEXTLOG=ON -DENABLE_STAT=ON -DBUILD_ALL=ON -DENABLE_OPENCL=OFF -DMFX_DISABLE_SW_FALLBACK=OFF -DAPI=2.1
cmake --build %WORKSPACE%\Build\VPL_build\x64 -j %NUMBER_OF_PROCESSORS% --config Release --target libmfx64-gen

cmake -B%WORKSPACE%\Build\VPL_build\x32 -H%WORKSPACE%\sources\mdp_msdk-lib -A Win32 -DCMAKE_C_FLAGS_RELEASE="-Wall" -DCMAKE_CXX_FLAGS_RELEASE="-Wall" -DENABLE_X11_DRI3=ON -DENABLE_WAYLAND=ON -DENABLE_TEXTLOG=ON -DENABLE_STAT=ON -DBUILD_ALL=ON -DENABLE_OPENCL=OFF -DMFX_DISABLE_SW_FALLBACK=OFF -DAPI=2.1
cmake --build %WORKSPACE%\Build\VPL_build\x32 -j %NUMBER_OF_PROCESSORS% --config Release --target libmfx32-gen


if exist %WORKSPACE%\output\packages rmdir %WORKSPACE%\output\packages
mkdir %WORKSPACE%\output\packages
powershell Compress-Archive -Path %WORKSPACE%\Build\icc\* -DestinationPath %WORKSPACE%\output\packages\icc.zip
powershell Compress-Archive -Path %WORKSPACE%\Build\msvc\* -DestinationPath %WORKSPACE%\output\packages\msvc.zip
powershell Compress-Archive -Path %WORKSPACE%\Build\uwp\* -DestinationPath %WORKSPACE%\output\packages\uwp.zip
powershell Compress-Archive -Path %WORKSPACE%\Build\mfts\* -DestinationPath %WORKSPACE%\output\packages\mfts.zip

powershell -file %WORKSPACE%\sources\mdp_msdk-lib\config\scripts\create_windows_drop.ps1 -Workspace "C:\workspace" -BuildDir "C:\workspace\Build" -MftsBinaries "C:\workspace\mfts_binaries" -IccBinaries "C:\workspace\icc_binaries"
powershell -file %WORKSPACE%\sources\mdp_msdk-lib\config\scripts\create_windows_tools_drop.ps1 -Workspace "C:\workspace" -BuildDir "C:\workspace\Build" -ValidationTools "C:\workspace\validation_tools" -MsdkBinaries "C:\workspace\msdk_binaries" -VPABuildDir "C:\workspace\vpa_build"
powershell -file %WORKSPACE%\sources\mdp_msdk-lib\config\scripts\create_windows_lucas_pkg.ps1 -Workspace "C:\workspace" -BuildDir "C:\workspace\Build" -ValidationTools "C:\workspace\validation_tools" -IccBinaries "C:\workspace\icc_binaries"
powershell -file %WORKSPACE%\sources\mdp_msdk-lib\config\scripts\create_windows_nightly_pkg.ps1 -Workspace "C:\workspace" -BuildDir "C:\workspace\Build" -VPABuildDir "C:\workspace\VPA_build" -ValidationTools "C:\workspace\validation_tools" -Service_UWD "C:\workspace\service" -AMD64 "C:\workspace\AMD64" -TestSystem "C:\workspace\test_system" -MsdkBinaries "C:\workspace\msdk_binaries" -IccBinaries "C:\workspace\icc_binaries"

exit /b %ERRORLEVEL%
