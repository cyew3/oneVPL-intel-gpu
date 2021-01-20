Param(
    [Parameter(Mandatory)]
    [string]$ReposDir,

    [Parameter(Mandatory)]
    [string]$PerlPath
)

$global:ProgressPreference = 'SilentlyContinue'
$global:ErrorActionPreference = 'Stop'

$UPDATE_VERSION_INFO_MSDK_PL = "${ReposDir}\mdp_msdk-lib\config\scripts\update_version\update_version_info_git.pl"
$UPDATE_VERSION_INFO_MFT_PL = "${ReposDir}\mdp_msdk-lib\config\scripts\update_version\update_version_info_mft_git.pl"
$UPDATE_VERSION_INFO_UWP_PL = "${ReposDir}\mdp_msdk-lib\config\scripts\update_version\update_version_info_uwp_git.pl"

$RC_FILES = @(
    @{FilePath = 'mdp_msdk-lib\_studio\mfx_lib\libmfxsw.rc';
     PerlScript = $UPDATE_VERSION_INFO_MSDK_PL;
     IsPlugin = $False},
    @{FilePath = 'mdp_msdk-lib\_studio\mfx_lib\libmfxhw.rc';
     PerlScript = $UPDATE_VERSION_INFO_MSDK_PL;
     IsPlugin = $False},
    @{FilePath = 'mdp_msdk-lib\_studio\mfx_lib\plugin\libmfxsw_plugin_hevce.rc';
     PerlScript = $UPDATE_VERSION_INFO_MSDK_PL;
     IsPlugin = $True},
    @{FilePath = 'mdp_msdk-lib\_studio\mfx_lib\plugin\libmfxsw_plugin_hevcd.rc';
     PerlScript = $UPDATE_VERSION_INFO_MSDK_PL;
     IsPlugin = $True},
    @{FilePath = 'mdp_msdk-lib\_studio\mfx_lib\plugin\mfxplugin_hw.rc';
     PerlScript = $UPDATE_VERSION_INFO_MSDK_PL;
     IsPlugin = $True},
    @{FilePath = 'mdp_msdk-lib\_studio\camera_pipe\camera_pipe.rc';
     PerlScript = $UPDATE_VERSION_INFO_MSDK_PL;
     IsPlugin = $True},
    @{FilePath = 'mdp_msdk-lib\_studio\mfx_lib\plugin\libmfxhw_plugin_hevce.rc';
     PerlScript = $UPDATE_VERSION_INFO_MSDK_PL;
     IsPlugin = $True},
    @{FilePath = 'mdp_msdk-lib\_studio\mfx_lib\plugin\libmfxhw_plugin_h264la.rc';
     PerlScript = $UPDATE_VERSION_INFO_MSDK_PL;
     IsPlugin = $True},
    @{FilePath = 'mdp_msdk-lib\_studio\h263d\mfx_h263d_plugin.rc';
     PerlScript = $UPDATE_VERSION_INFO_MSDK_PL;
     IsPlugin = $True},
    @{FilePath = 'mdp_msdk-lib\_studio\h263e\mfx_h263e_plugin.rc';
     PerlScript = $UPDATE_VERSION_INFO_MSDK_PL;
     IsPlugin = $True},
    @{FilePath = 'mdp_msdk-lib\api\mfx_loader_dll\mfx_loader_dll_hw.rc';
     PerlScript = $UPDATE_VERSION_INFO_MSDK_PL;
     IsPlugin = $False},
    @{FilePath = 'mdp_msdk-mfts\samples\sample_plugins\mfoundation\mf_utils\include\mf_version.h';
     PerlScript = $UPDATE_VERSION_INFO_MFT_PL;
     IsPlugin = $False},
    @{FilePath = 'mdp_msdk-lib\api\intel_api_uwp\src\intel_gfx_api.rc';
     PerlScript = $UPDATE_VERSION_INFO_UWP_PL;
     IsPlugin = $False}
)

$build_number=(Get-Content "${ReposDir}\mdp_msdk-lib\config\build_numbers.json" | ConvertFrom-Json).mediasdk.master
write-host "Build Number: ${build_number}"

$text = Get-Content -Path "${ReposDir}\mdp_msdk-lib\api\include\mfxdefs.h" -Encoding UTF8
$minor_api_ver = ([regex]::Matches($text, 'MFX_VERSION_MINOR\s(\d+)')).Groups[1].Value

$product_version=Get-Content -Path "${ReposDir}\mdp_msdk-lib\_studio\product.ver"
$major_ver,$minor_ver=$product_version.Split('.')

$work_dir = Get-Location

foreach ($file_info in $RC_FILES)
{
    # --release 0 is never change. Historical reason of using it.
    $arguments = "$($file_info.PerlScript) --rc ${ReposDir}\$($file_info.FilePath) --release 0 --vc ${build_number} --major_ver ${major_ver} --minor_ver ${minor_ver}"
    
    if ($file_info.FilePath.EndsWith('.rc'))
    {
        $arguments += " --api ${minor_api_ver}"
    }
    
    if ($file_info.IsPlugin)
    {
        $arguments += ' --plugin standard'
    }

    write-host "cmd: ${arguments}"
    Start-Process -FilePath $PerlPath -ArgumentList $arguments -NoNewWindow -Wait
    write-host ('-'*40)
}

$samples_version_file = "${ReposDir}\mdp_msdk-lib\samples\sample_common\include\version.h"
$text = Get-Content -Path $samples_version_file -Encoding UTF8
$define_str = '#define MSDK_BUILD'

if($text -match $define_str)
{
    $text = $text.Replace("${define_str} 0", "${define_str} ${build_number}")
    Set-Content -Path $samples_version_file -Value $text -Encoding UTF8
    write-host "Updated MSDK_BUILD to ${build_number}"
}
