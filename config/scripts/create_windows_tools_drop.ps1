Param(
    [Parameter(Mandatory)]
    [string]$Workspace,

    [Parameter(Mandatory)]
    [string]$BuildDir,

    [Parameter(Mandatory)]
    [string]$ValidationTools,

    [Parameter(Mandatory)]
    [string]$MsdkBinaries,

    [Parameter(Mandatory)]
    [string]$VPABuildDir
)

$global:ProgressPreference = 'SilentlyContinue'
$global:ErrorActionPreference = 'Stop'

$FILES=@(
    'msvc\x64\__bin\Release\avcrnd.exe',
    'msvc\x64\__bin\Release\bd_conformance.exe',
    'msvc\x64\__bin\Release\behavior_tests.exe',
    'msvc\x64\__bin\Release\bitstreams_parser.exe',
    'msvc\x64\__bin\Release\bs_parser.dll',
    'msvc\x64\__bin\Release\bs_trace.exe',
    'msvc\x64\__bin\Release\compare_struct.exe',
    'msvc\x64\__bin\Release\fieldweavingtool.exe',
    'msvc\x64\__bin\Release\FSEP.exe',
    'msvc\x64\__bin\Release\h264_ff_dumper.exe',
    'msvc\x64\__bin\Release\h264_fs_dumper.exe',
    'msvc\x64\__bin\Release\h264_gop_dumper.exe',
    'msvc\x64\__bin\Release\h264_qp_extractor.exe',
    'msvc\x64\__bin\Release\hevcrnd.exe',
    'msvc\x64\__bin\Release\hevc_fs_dumper.exe',
    'msvc\x64\__bin\Release\hevc_qp_dump.exe',
    'msvc\x64\__bin\Release\hevc_qp_extractor.exe',
    'msvc\x64\__bin\Release\hevc_stream_info.exe',
    'uwp\x64\__bin\Release\intel_gfx_api-x64.dll',
    'msvc\x64\__bin\Release\libmfxsw64.dll',
    'msvc\x64\__bin\Release\mfx_player.exe',
    'msvc\x64\__bin\Release\mfx_transcoder.exe',
    'msvc\x64\__bin\Release\mpeg2_stream_info.exe',
    'msvc\x64\__bin\Release\mpg2rnd.exe',
    'msvc\x64\__bin\Release\msdk_gmock.exe',
    'msvc\x64\__bin\Release\msdk_ts.exe',
    'msvc\x64\__bin\Release\ref_compose.exe',
    'mfts\x64\__bin\Release\rtest_vista.exe',
    'msvc\x64\__bin\Release\sampler.exe',
    'msvc\x64\__bin\Release\sample_camera.exe',
    'msvc\x64\__bin\Release\sample_decode.exe',
    'msvc\x64\__bin\Release\sample_encode.exe',
    'msvc\x64\__bin\Release\sample_multi_transcode.exe',
    'msvc\x64\__bin\Release\sample_rotate_plugin.dll',
    'msvc\x64\__bin\Release\sample_vpp.exe',
    'msvc\x64\__bin\Release\scheduler_tests.exe',
    'msvc\x64\__bin\Release\simple_composite.exe',
    'msvc\x64\__bin\Release\test_behavior.exe',
    'msvc\x64\__bin\Release\test_dispatcher.exe',
    'msvc\x64\__bin\Release\test_dispatcher_2005.exe',
    'msvc\x64\__bin\Release\test_multi_adapter.exe',
    'msvc\x64\__bin\Release\test_thread_safety.exe',
    'msvc\x64\__bin\Release\test_vpp.exe',
    'msvc\x64\__bin\Release\uc_conformance.exe'
)

$VPA_FILES=@(
    'platforms',
    'Qt5Gui.dll',
    'Qt5Core.dll',
    'msvcr120.dll',
    'msvcp120.dll',
    'IntelVideoProAnalyzerConsole_libFNP.dll',
    'IntelVideoProAnalyzerConsole.exe',
    'intelremotemon.dll',
    'FNP_Act_Installer.dll',
    'COM____TD6N-B5HN2F3T.lic'
)

$FILES_WIN32=@(
    'msvc\x32\__bin\Release\libmfxsw32.dll',
    'msvc\x32\__bin\Release\mfx_player.exe',
    'msvc\x32\__bin\Release\mfx_transcoder.exe'
)

$FILES_WIN64=@(
    'msvc\x64\__bin\Release\libmfxsw64.dll',
    'msvc\x64\__bin\Release\mfx_player.exe',
    'msvc\x64\__bin\Release\mfx_transcoder.exe'
)

$package_dir="$Workspace\output\WindowsToolsDrop"
$package_name="WindowsToolsDrop.zip"

New-Item -ItemType "directory" -Path $package_dir\to_archive\imports\mediasdk\Win32,$package_dir\to_archive\imports\mediasdk\Win64
Set-Location -Path $BuildDir; Copy-Item $FILES -Destination $package_dir\to_archive\imports\mediasdk
Set-Location -Path $BuildDir; Copy-Item $FILES_WIN32 -Destination $package_dir\to_archive\imports\mediasdk\Win32
Set-Location -Path $BuildDir; Copy-Item $FILES_WIN64 -Destination $package_dir\to_archive\imports\mediasdk\Win64
Set-Location -Path $VPABuildDir; Copy-Item $VPA_FILES -Destination $package_dir\to_archive\imports\mediasdk -Recurse

Copy-Item $ValidationTools\mediasdkDir.exe -Destination $package_dir\to_archive
Set-Location -Path $MsdkBinaries;
  Copy-Item 'mfx-tracer_64.dll','test_hevce.exe','ocl_rotate.cl','sample_plugin_opencl.dll','mfx_binmanager.exe' -Destination $package_dir\to_archive\imports\mediasdk

Compress-Archive -Path $package_dir\to_archive\* -DestinationPath $package_dir\$package_name
