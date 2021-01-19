Param(
    [Parameter(Mandatory)]
    [string]$Workspace,

    [Parameter(Mandatory)]
    [string]$BuildDir,

    [Parameter(Mandatory)]
    [string]$VPABuildDir,

    [Parameter(Mandatory)]
    [string]$ValidationTools,

    [Parameter(Mandatory)]
    [string]$Service_UWD,

    [Parameter(Mandatory)]
    [string]$AMD64,

    [Parameter(Mandatory)]
    [string]$TestSystem,

    [Parameter(Mandatory)]
    [string]$MsdkBinaries
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
    'msvc\x64\__bin\Release\fieldweavingtool.exe',
    'msvc\x64\__bin\Release\FSEP.exe',
    'msvc\x64\__bin\Release\h264_ff_dumper.exe',
    'msvc\x64\__bin\Release\h264_fs_dumper.exe',
    'msvc\x64\__bin\Release\h264_gop_dumper.exe',
    'msvc\x64\__bin\Release\h264_parser_fei.exe',
    'msvc\x64\__bin\Release\h264_qp_extractor.exe',
    'msvc\x64\__bin\Release\hevcrnd.exe',
    'msvc\x64\__bin\Release\hevc_fs_dumper.exe',
    'msvc\x64\__bin\Release\hevc_qp_dump.exe',
    'msvc\x64\__bin\Release\hevc_qp_extractor.exe',
    'msvc\x64\__bin\Release\hevc_stream_info.exe',
    'msvc\x64\__bin\Release\libmfxhw64.dll',
    'msvc\x64\__bin\Release\libmfxsw64.dll',
    'msvc\x64\__bin\Release\mfx_pavp.dll',
    'msvc\x64\__bin\Release\mfx_player.exe',
    'msvc\x64\__bin\Release\mfx_transcoder.exe',
    'msvc\x64\__bin\Release\mpeg2_stream_info.exe',
    'msvc\x64\__bin\Release\mpg2rnd.exe',
    'msvc\x64\__bin\Release\msdk_gmock.exe',
    'msvc\x64\__bin\Release\msdk_sys_analyzer.exe',
    'msvc\x64\__bin\Release\msdk_ts.exe',
    'msvc\x64\__bin\Release\ref_compose.exe',
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
    'msvc\x64\__bin\Release\test_thread_safety.exe',
    'msvc\x64\__bin\Release\test_usage_models.exe',
    'msvc\x64\__bin\Release\test_vpp.exe',
    'msvc\x64\__bin\Release\test_vpp_multisession.exe',
    'msvc\x64\__bin\Release\uc_conformance.exe',
    'msvc\x64\__bin\Debug\mfx_loader_dll_hw64.dll',
    'icc\x64\_studio\mfx_lib\plugin\mfxplugin_hw64.dll',
    'icc\x64\_testsuite\msdk_ts\unit\hevce_tests\hevce_tests.exe',
    'VPL_build\x64\__bin\Release\libmfx64-gen.dll'
)

$TRACE_EVENT_FILES=@(
    'mfts\x64\__bin\Release\Microsoft.Diagnostics.Tracing.TraceEvent.dll',
    'mfts\x64\__bin\Release\Microsoft.Diagnostics.Tracing.TraceEvent.xml',
    'mfts\x64\__bin\Release\System.Reactive.Core.dll',
    'mfts\x64\__bin\Release\System.Reactive.Core.xml',
    'mfts\x64\__bin\Release\TraceEventAnalyzer.exe'
)

$MSDK_FILES=@(
    'mfx-tracer_64.dll',
    'hevc_random_encoder.exe',
    'InstallerPreReq-x64.exe',
    'test_av1e.exe',
    'test_core.exe',
    'tracer.exe',
    '15dd936825ad475ea34e35f3f54217a6',
    '2fca99749fdb49aeb121a5b63ef568f7',
    '343d689665ba4548829942a32a386569',
    '588f1185d47b42968dea377bb5d0dcb4',
    'ocl_rotate.cl',
    'sample_plugin_opencl.dll',
    'mfx_binmanager.exe'
)

$package_dir="$Workspace\output\WindowsNightlyPackage"
$package_name="WindowsNightlyPackage.zip"

New-Item -ItemType "directory" -Path $package_dir\to_archive\build\win_x64\bin\mft_tools

Copy-Item $TestSystem\* -Destination $package_dir\to_archive -Exclude '.git*' -Recurse
Set-Location -Path $ValidationTools; Copy-Item 'on_platform_changed.bat','after_extract.bat','mfx_binmanager.ps1' -Destination $package_dir\to_archive
Copy-Item $Service_UWD -Destination $package_dir\to_archive -Recurse
Copy-Item $AMD64 -Destination $package_dir\to_archive -Recurse
Copy-Item $VPABuildDir\* -Destination $package_dir\to_archive\build\win_x64\bin -Recurse

# Binaries
Set-Location -Path $BuildDir; Copy-Item $FILES -Destination $package_dir\to_archive\build\win_x64\bin
Set-Location -Path $BuildDir; Copy-Item $TRACE_EVENT_FILES -Destination $package_dir\to_archive\build\win_x64\bin\mft_tools
Set-Location -Path $MsdkBinaries; Copy-Item $MSDK_FILES -Destination $package_dir\to_archive\build\win_x64\bin -Recurse

Copy-Item $Workspace\workspace_snapshot.json -Destination $package_dir\to_archive\build

Rename-Item -Path $package_dir\to_archive\build\win_x64\bin\mfx_loader_dll_hw64.dll -NewName "mfx_loader_dll_hw64_d.dll"

Compress-Archive -Path $package_dir\to_archive\* -DestinationPath $package_dir\$package_name
