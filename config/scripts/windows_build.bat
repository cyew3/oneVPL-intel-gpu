REM These environment variables shoul be set: MFX_HOME MEDIASDK_ROOT INTELMEDIASDK_FFMPEG_ROOT MEDIASDK_TRS_MODULE_PATH MINIDDK_VERSION
@echo off 
setlocal

set WORKSPACE="C:\workspace"
REM all ICC targets
REM set ICC_TARGETS=mfxplugin_hw64 mfx_h265fei_hw64 mfx_hevcd_sw64 mfx_hevce_gacc64 mfx_hevce_sw64 hevc_pp_atom hevc_pp_avx2 hevc_pp_dispatcher hevc_pp_px hevc_pp_sse4 hevc_pp_ssse3 genx_h265_encode_embeded av1_enc_hw av1_pp_avx2 av1_pp_px av1_pp_ssse3 genx_av1_encode_embeded hevce_tests av1_opt mfx_av1e_gacc64 h265_fei av1_fei

set ICC_TARGETS_64=hevce_tests mfxplugin_hw64 mfxplugin64_av1e_gacc
set ICC_TARGETS_32=mfxplugin_hw32 mfxplugin32_av1e_gacc

if exist %WORKSPACE%\Build rmdir %WORKSPACE%\Build /S /Q
mkdir %WORKSPACE%\Build\uwp\x32 %WORKSPACE%\Build\uwp\x64 %WORKSPACE%\Build\icc\x32 %WORKSPACE%\Build\icc\x64 %WORKSPACE%\Build\msvc\x32 %WORKSPACE%\Build\msvc\x64 %WORKSPACE%\Build\mfts\x32 %WORKSPACE%\Build\mfts\x64 %WORKSPACE%\Build\VPL_build\x32 %WORKSPACE%\Build\VPL_build\x64

call "C:\Program Files (x86)\IntelSWTools\compilers_and_libraries_2020\windows\bin\compilervars.bat" -arch ia32 vs2019

cmake -B%WORKSPACE%\Build\icc\x32 -H%WORKSPACE%\sources\mdp_msdk-lib -G"Ninja" -DCMAKE_SYSTEM_VERSION=10.0.18362 -DCMAKE_CXX_COMPILER="C:/Program Files (x86)/IntelSWTools/compilers_and_libraries_2020/windows/bin/intel64/icl.exe" -DCMAKE_C_COMPILER="C:/Program Files (x86)/IntelSWTools/compilers_and_libraries_2020/windows/bin/intel64/icl.exe" -DAPI=latest -DENABLE_HEVC=ON -DENABLE_AV1=ON -DENABLE_HEVC_ON_GCC=ON
cmake --build %WORKSPACE%\Build\icc\x32 -j %NUMBER_OF_PROCESSORS% --config Release --target %ICC_TARGETS_32%

call "C:\Program Files (x86)\IntelSWTools\compilers_and_libraries_2020\windows\bin\compilervars.bat" -arch Intel64 vs2019

cmake -B%WORKSPACE%\Build\icc\x64 -H%WORKSPACE%\sources\mdp_msdk-lib -G"Ninja" -DCMAKE_SYSTEM_VERSION=10.0.18362 -DCMAKE_CXX_COMPILER="C:/Program Files (x86)/IntelSWTools/compilers_and_libraries_2020/windows/bin/intel64/icl.exe" -DCMAKE_C_COMPILER="C:/Program Files (x86)/IntelSWTools/compilers_and_libraries_2020/windows/bin/intel64/icl.exe" -DAPI=latest -DENABLE_HEVC=ON -DENABLE_AV1=ON -DENABLE_HEVC_ON_GCC=ON
cmake --build %WORKSPACE%\Build\icc\x64 -j %NUMBER_OF_PROCESSORS% --config Release --target %ICC_TARGETS_64%


cmake -B%WORKSPACE%\Build\msvc\x64 -H%WORKSPACE%\sources\mdp_msdk-lib -A x64 -DAPI=latest -DBUILD_VAL_TOOLS=ON -DBUILD_TESTS=ON -DMFX_ENABLE_LP_LOOKAHEAD=ON
cmake --build %WORKSPACE%\Build\msvc\x64 -j %NUMBER_OF_PROCESSORS% --config Release
cmake --build %WORKSPACE%\Build\msvc\x64 -j %NUMBER_OF_PROCESSORS% --config Debug --target mfx_loader_dll_hw64

cmake -B%WORKSPACE%\Build\msvc\x32 -H%WORKSPACE%\sources\mdp_msdk-lib -A Win32 -DAPI=latest -DBUILD_VAL_TOOLS=ON -DBUILD_TESTS=ON -DMFX_ENABLE_LP_LOOKAHEAD=ON
cmake --build %WORKSPACE%\Build\msvc\x32 -j %NUMBER_OF_PROCESSORS% --config Release


cmake -B%WORKSPACE%\Build\uwp\x64 -H%WORKSPACE%\sources\mdp_msdk-lib -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0.18362.0
cmake --build %WORKSPACE%\Build\uwp\x64 -j %NUMBER_OF_PROCESSORS% --config Release

cmake -B%WORKSPACE%\Build\uwp\x32 -H%WORKSPACE%\sources\mdp_msdk-lib -A Win32 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0.18362.0
cmake --build %WORKSPACE%\Build\uwp\x32 -j %NUMBER_OF_PROCESSORS% --config Release


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
powershell $global:ProgressPreference = 'SilentlyContinue'; Compress-Archive -Path %WORKSPACE%\Build\icc\* -DestinationPath %WORKSPACE%\output\packages\icc.zip
powershell $global:ProgressPreference = 'SilentlyContinue'; Compress-Archive -Path %WORKSPACE%\Build\msvc\* -DestinationPath %WORKSPACE%\output\packages\msvc.zip
powershell $global:ProgressPreference = 'SilentlyContinue'; Compress-Archive -Path %WORKSPACE%\Build\uwp\* -DestinationPath %WORKSPACE%\output\packages\uwp.zip
powershell $global:ProgressPreference = 'SilentlyContinue'; Compress-Archive -Path %WORKSPACE%\Build\mfts\* -DestinationPath %WORKSPACE%\output\packages\mfts.zip
