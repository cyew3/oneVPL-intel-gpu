Param(
    [Parameter(Mandatory)]
    [string]$Workspace,

    [Parameter(Mandatory)]
    [string]$BuildDir
)

$global:ProgressPreference = 'SilentlyContinue'
$global:ErrorActionPreference = 'Stop'

$CONFIG_INI_DATA = "
version=11.0.0
merit=08000015
apiversion=134
"

$SUPPORT_TXT_DATA = "Please use the following information when submitting customer support requests

Package ID: <ID>
Package Contents: Intel® Media SDK for ICL

Please direct customer support requests through http://www.intel.com/software/products/support

* Other names and brands may be claimed as the property of others."


$CONFIG_FILE_NAME = 'config.ini'
$SUPPORT_FILE_NAME = 'support.txt'

$MFTS_RELEASE_FILES=@(
    'mfts\x32\__bin\Release\h265e_32.vp',
    'mfts\x64\__bin\Release\h265e_64.vp',
    'mfts\x32\__bin\Release\he_32.vp',
    'mfts\x64\__bin\Release\he_64.vp',
    'mfts\x32\__bin\Release\mfx_mft_av1hve_32.dll',
    'mfts\x32\__bin\Release\mfx_mft_av1hve_32.vp',
    'mfts\x64\__bin\Release\mfx_mft_av1hve_64.dll',
    'mfts\x64\__bin\Release\mfx_mft_av1hve_64.vp',
    'mfts\x32\__bin\Release\mfx_mft_encrypt_32.dll',
    'mfts\x64\__bin\Release\mfx_mft_encrypt_64.dll',
    'mfts\x32\__bin\Release\mfx_mft_h264ve_32.dll',
    'mfts\x64\__bin\Release\mfx_mft_h264ve_64.dll',
    'mfts\x32\__bin\Release\mfx_mft_h265ve_32.dll',
    'mfts\x64\__bin\Release\mfx_mft_h265ve_64.dll',
    'mfts\x32\__bin\Release\mfx_mft_mjpgvd_32.dll',
    'mfts\x64\__bin\Release\mfx_mft_mjpgvd_64.dll',
    'mfts\x32\__bin\Release\mfx_mft_vp8vd_32.dll',
    'mfts\x64\__bin\Release\mfx_mft_vp8vd_64.dll',
    'mfts\x32\__bin\Release\mfx_mft_vp9vd_32.dll',
    'mfts\x64\__bin\Release\mfx_mft_vp9vd_64.dll',
    'mfts\x32\__bin\Release\mfx_mft_vp9ve_32.dll',
    'mfts\x64\__bin\Release\mfx_mft_vp9ve_64.dll',
    'mfts\x32\__bin\Release\mj_32.vp',
    'mfts\x64\__bin\Release\mj_64.vp',
    'mfts\x32\__bin\Release\vp9e_32.vp',
    'mfts\x64\__bin\Release\vp9e_64.vp',
    'mfts\x32\__bin\Release\cpa_32.vp',
    'mfts\x64\__bin\Release\cpa_64.vp',
    'mfts\x32\__bin\Release\c_32.cpa',
    'mfts\x64\__bin\Release\c_64.cpa',
    'mfts\x32\__bin\Release\dev_32.vp',
    'mfts\x64\__bin\Release\dev_64.vp'
)

$MFTS_RELEASE_PDB_FILES=@(
    'mfts\x32\__bin\Release\mfx_mft_av1hve_32.pdb',
    'mfts\x64\__bin\Release\mfx_mft_av1hve_64.pdb',
    'mfts\x32\__bin\Release\mfx_mft_encrypt_32.pdb',
    'mfts\x64\__bin\Release\mfx_mft_encrypt_64.pdb',
    'mfts\x32\__bin\Release\mfx_mft_h264ve_32.pdb',
    'mfts\x64\__bin\Release\mfx_mft_h264ve_64.pdb',
    'mfts\x32\__bin\Release\mfx_mft_h265ve_32.pdb',
    'mfts\x64\__bin\Release\mfx_mft_h265ve_64.pdb',
    'mfts\x32\__bin\Release\mfx_mft_mjpgvd_32.pdb',
    'mfts\x64\__bin\Release\mfx_mft_mjpgvd_64.pdb',
    'mfts\x32\__bin\Release\mfx_mft_vp8vd_32.pdb',
    'mfts\x64\__bin\Release\mfx_mft_vp8vd_64.pdb',
    'mfts\x32\__bin\Release\mfx_mft_vp9vd_32.pdb',
    'mfts\x64\__bin\Release\mfx_mft_vp9vd_64.pdb',
    'mfts\x32\__bin\Release\mfx_mft_vp9ve_32.pdb',
    'mfts\x64\__bin\Release\mfx_mft_vp9ve_64.pdb'
)

$MFTS_INTERNAL_FILES=@(
    'mfts\x32\__bin\ReleaseInternal\h265e_32.vp',
    'mfts\x64\__bin\ReleaseInternal\h265e_64.vp',
    'mfts\x32\__bin\ReleaseInternal\he_32.vp',
    'mfts\x64\__bin\ReleaseInternal\he_64.vp',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_av1hve_32.dll',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_av1hve_32.vp',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_av1hve_64.dll',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_av1hve_64.vp',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_encrypt_32.dll',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_encrypt_64.dll',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_h264ve_32.dll',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_h264ve_64.dll',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_h265ve_32.dll',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_h265ve_64.dll',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_mjpgvd_32.dll',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_mjpgvd_64.dll',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_vp8vd_32.dll',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_vp8vd_64.dll',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_vp9vd_32.dll',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_vp9vd_64.dll',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_vp9ve_32.dll',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_vp9ve_64.dll',
    'mfts\x32\__bin\ReleaseInternal\mj_32.vp',
    'mfts\x64\__bin\ReleaseInternal\mj_64.vp',
    'mfts\x32\__bin\ReleaseInternal\vp9e_32.vp',
    'mfts\x64\__bin\ReleaseInternal\vp9e_64.vp',
    'mfts\x32\__bin\ReleaseInternal\cpa_32.vp',
    'mfts\x64\__bin\ReleaseInternal\cpa_64.vp',
    'mfts\x32\__bin\ReleaseInternal\c_32.cpa',
    'mfts\x64\__bin\ReleaseInternal\c_64.cpa',
    'mfts\x32\__bin\ReleaseInternal\dev_32.vp',
    'mfts\x64\__bin\ReleaseInternal\dev_64.vp'
)

$MFTS_INTERNAL_PDB_FILES=@(
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_av1hve_32.pdb',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_av1hve_64.pdb',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_encrypt_32.pdb',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_encrypt_64.pdb',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_h264ve_32.pdb',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_h264ve_64.pdb',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_h265ve_32.pdb',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_h265ve_64.pdb',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_mjpgvd_32.pdb',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_mjpgvd_64.pdb',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_vp8vd_32.pdb',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_vp8vd_64.pdb',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_vp9vd_32.pdb',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_vp9vd_64.pdb',
    'mfts\x32\__bin\ReleaseInternal\mfx_mft_vp9ve_32.pdb',
    'mfts\x64\__bin\ReleaseInternal\mfx_mft_vp9ve_64.pdb'
)

$MSDK_FILES=@(
    'msvc\x32\__bin\Release\libmfxhw32.dll',
    'msvc\x64\__bin\Release\libmfxhw64.dll',
    'msvc\x32\__bin\Release\mfx_loader_dll_hw32.dll',
    'msvc\x64\__bin\Release\mfx_loader_dll_hw64.dll',
    'uwp\x64\__bin\Release\intel_gfx_api-x64.dll',
    'uwp\x32\__bin\Release\intel_gfx_api-x86.dll',
    'icc\x32\_studio\mfx_lib\plugin\mfxplugin32_av1e_gacc.dll',
    'icc\x64\_studio\mfx_lib\plugin\mfxplugin64_av1e_gacc.dll',
    'icc\x32\_studio\mfx_lib\plugin\mfxplugin_hw32.dll',
    'icc\x64\_studio\mfx_lib\plugin\mfxplugin_hw64.dll',
    'VPL_build\x64\__bin\Release\libmfx64-gen.dll',
    'VPL_build\x32\__bin\Release\libmfx32-gen.dll'
)

$MSDK_PDB_FILES=@(
    'uwp\x64\__bin\Release\intel_gfx_api-x64.pdb',
    'uwp\x32\__bin\Release\intel_gfx_api-x86.pdb',
    'msvc\x32\__bin\Release\libmfxhw32.pdb',
    'msvc\x64\__bin\Release\libmfxhw64.pdb',
    'msvc\x32\__bin\Release\mfx_loader_dll_hw32.pdb',
    'msvc\x64\__bin\Release\mfx_loader_dll_hw64.pdb',
    'icc\x32\_studio\mfx_lib\plugin\mfxplugin32_av1e_gacc.pdb',
    'icc\x64\_studio\mfx_lib\plugin\mfxplugin64_av1e_gacc.pdb',
    'icc\x32\_studio\mfx_lib\plugin\mfxplugin_hw32.pdb',
    'icc\x64\_studio\mfx_lib\plugin\mfxplugin_hw64.pdb',
    'VPL_build\x64\__bin\Release\libmfx64-gen.pdb',
    'VPL_build\x32\__bin\Release\libmfx32-gen.pdb'
)

$PACKAGE_NAMES = @(
    'MediaSDK-64-bit',
    'MediaSDK_Release_Internal-64-bit'
)

$package_dir = "$Workspace\output\WindowsDrop"
$package_name = "WindowsDrop.zip"

foreach ($pkg_name in $PACKAGE_NAMES)
{
    $archive_dir = "$package_dir\tmp_dir\${pkg_name}_zip"
    New-Item -Path $archive_dir\pdb -ItemType "directory"

    New-Item -Path $archive_dir -Name $SUPPORT_FILE_NAME -ItemType "file" -Value $SUPPORT_TXT_DATA.Replace("<ID>", $pkg_name)
    New-Item -Path $archive_dir -Name $CONFIG_FILE_NAME -ItemType "file" -Value $CONFIG_INI_DATA

    if ($pkg_name -eq 'MediaSDK-64-bit')
    {
        Set-Location -Path $BuildDir; Copy-Item $MFTS_RELEASE_FILES -Destination $archive_dir;
          Copy-Item $MFTS_RELEASE_PDB_FILES -Destination $archive_dir\pdb
    }
    else
    {
        Set-Location -Path $BuildDir; Copy-Item $MFTS_INTERNAL_FILES -Destination $archive_dir;
          Copy-Item $MFTS_INTERNAL_PDB_FILES -Destination $archive_dir\pdb
    }

    Set-Location -Path $BuildDir; Copy-Item $MSDK_FILES -Destination $archive_dir;
      Copy-Item $MSDK_PDB_FILES -Destination $archive_dir\pdb

    Compress-Archive -Path $archive_dir\* -DestinationPath "$package_dir\tmp_dir\${pkg_name}.zip"
}

Compress-Archive -Path $package_dir\tmp_dir\*.zip -DestinationPath "$package_dir\${package_name}"
