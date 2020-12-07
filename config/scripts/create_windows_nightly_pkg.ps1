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
    [string]$MsdkBinaries,

    [Parameter(Mandatory)]
    [string]$IccBinaries
)

$FILES=@(
    'msvc\x32\__bin\RelWithDebInfo\avcrnd.exe',
    'msvc\x32\__bin\RelWithDebInfo\bd_conformance.exe',
    'msvc\x32\__bin\RelWithDebInfo\behavior_tests.exe',
    'msvc\x32\__bin\RelWithDebInfo\bitstreams_parser.exe',
    'msvc\x32\__bin\RelWithDebInfo\bs_parser.dll',
    'msvc\x32\__bin\RelWithDebInfo\bs_trace.exe',
    'msvc\x32\__bin\RelWithDebInfo\fieldweavingtool.exe',
    'msvc\x32\__bin\RelWithDebInfo\FSEP.exe',
    'msvc\x32\__bin\RelWithDebInfo\h264_ff_dumper.exe',
    'msvc\x32\__bin\RelWithDebInfo\h264_fs_dumper.exe',
    'msvc\x32\__bin\RelWithDebInfo\h264_gop_dumper.exe',
    'msvc\x32\__bin\RelWithDebInfo\h264_parser_fei.exe',
    'msvc\x32\__bin\RelWithDebInfo\h264_qp_extractor.exe',
    'msvc\x32\__bin\RelWithDebInfo\hevcrnd.exe',
    'msvc\x32\__bin\RelWithDebInfo\hevc_fs_dumper.exe',
    'msvc\x32\__bin\RelWithDebInfo\hevc_qp_dump.exe',
    'msvc\x32\__bin\RelWithDebInfo\hevc_qp_extractor.exe',
    'msvc\x32\__bin\RelWithDebInfo\hevc_stream_info.exe',
    'msvc\x64\__bin\RelWithDebInfo\libmfxhw64.dll',
    'msvc\x64\__bin\RelWithDebInfo\libmfxsw64.dll',
    'msvc\x32\__bin\RelWithDebInfo\mfx_pavp.dll',
    'msvc\x32\__bin\RelWithDebInfo\mfx_player.exe',
    'msvc\x32\__bin\RelWithDebInfo\mfx_transcoder.exe',
    'msvc\x32\__bin\RelWithDebInfo\mpeg2_stream_info.exe',
    'msvc\x32\__bin\RelWithDebInfo\mpg2rnd.exe',
    'msvc\x32\__bin\RelWithDebInfo\msdk_gmock.exe',
    'msvc\x32\__bin\RelWithDebInfo\msdk_sys_analyzer.exe',
    'msvc\x32\__bin\RelWithDebInfo\msdk_ts.exe',
    'msvc\x32\__bin\RelWithDebInfo\ref_compose.exe',
    'msvc\x32\__bin\RelWithDebInfo\sampler.exe',
    'msvc\x32\__bin\RelWithDebInfo\sample_camera.exe',
    'msvc\x32\__bin\RelWithDebInfo\sample_decode.exe',
    'msvc\x32\__bin\RelWithDebInfo\sample_encode.exe',
    'msvc\x32\__bin\RelWithDebInfo\sample_multi_transcode.exe',
    'msvc\x32\__bin\RelWithDebInfo\sample_rotate_plugin.dll',
    'msvc\x32\__bin\RelWithDebInfo\sample_vpp.exe',
    'msvc\x32\__bin\RelWithDebInfo\scheduler_tests.exe',
    'msvc\x32\__bin\RelWithDebInfo\simple_composite.exe',
    'msvc\x32\__bin\RelWithDebInfo\test_behavior.exe',
    'msvc\x32\__bin\RelWithDebInfo\test_dispatcher.exe',
    'msvc\x32\__bin\RelWithDebInfo\test_dispatcher_2005.exe',
    'msvc\x32\__bin\RelWithDebInfo\test_thread_safety.exe',
    'msvc\x32\__bin\RelWithDebInfo\test_usage_models.exe',
    'msvc\x32\__bin\RelWithDebInfo\test_vpp.exe',
    'msvc\x32\__bin\RelWithDebInfo\test_vpp_multisession.exe',
    'msvc\x32\__bin\RelWithDebInfo\uc_conformance.exe',
    # TODO: add Debug config to build script
    'msvc\x64\__bin\Debug\mfx_loader_dll_hw64.dll',
    # TODO: add VPL build
    'VPL_build\x64\__bin\Release\libmfx64-gen.dll'
)

# TODO: add nuget packages to manifest and copy it to mdp_msdk-val-tools dir
$TRACE_EVENT_FILES=@(
    'mfts\x64\__bin\Release\Microsoft.Diagnostics.Tracing.TraceEvent.dll',
    'mfts\x64\__bin\Release\Microsoft.Diagnostics.Tracing.TraceEvent.xml',
    'mfts\x64\__bin\Release\System.Reactive.Core.dll',
    'mfts\x64\__bin\Release\System.Reactive.Core.xml',
    'mfts\x64\__bin\Release\TraceEventAnalyzer.exe'
)

$MSDK_FILES=@(
    'mfx-tracer_64.dll',
    'hevce_tests.exe',
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
Set-Location -Path $IccBinaries; Copy-Item 'mfxplugin64_hw.dll' -Destination $package_dir\to_archive\build\win_x64\bin

# Copy-Item $Workspace\workspace_snapshot.json -Destination $package_dir\to_archive\build

Rename-Item -Path $package_dir\to_archive\build\win_x64\bin\mfx_loader_dll_hw64.dll -NewName "mfx_loader_dll_hw64_d.dll"

Compress-Archive -Path $package_dir\to_archive\* -DestinationPath $package_dir\$package_name
