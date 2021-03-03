Param(
    [Parameter(Mandatory)]
    [string]$Workspace,

    [Parameter(Mandatory)]
    [string]$BuildDir

    [Parameter(Mandatory)]
    [string]$MsdkPath
)

$global:ProgressPreference = 'SilentlyContinue'
$global:ErrorActionPreference = 'Stop'

$VPL_FILES=@(
    'x64\__bin\Release\libmfx64-gen.dll',
    'x32\__bin\Release\libmfx32-gen.dll'
)

$VPL_PDB_FILES=@(
    'x64\__bin\Release\libmfx64-gen.pdb',
    'x32\__bin\Release\libmfx32-gen.pdb'
)

$PACKAGE_NAMES = @(
    'MediaSDK-64-bit',
    'MediaSDK_Release_Internal-64-bit'
)

$package_dir = "$Workspace\output\WindowsDrop"
$package_name = "WindowsDrop.zip"

Expand-Archive -Path "$MsdkPath\Windows\WindowsDrop\*.zip" -DestinationPath $package_dir

foreach ($pkg_name in $PACKAGE_NAMES)
{
    $archive_dir = "$package_dir\${pkg_name}"
    Expand-Archive -Path "$archive_dir.zip" -DestinationPath $archive_dir
    Remove-Item "$archive_dir.zip"

    Set-Location -Path $BuildDir; Copy-Item $VPL_FILES -Destination $archive_dir;
      Copy-Item $VPL_PDB_FILES -Destination $archive_dir\pdb

    Compress-Archive -Path $archive_dir\* -DestinationPath "$package_dir\tmp_dir\${pkg_name}.zip"
}

Compress-Archive -Path $package_dir\tmp_dir\*.zip -DestinationPath "$package_dir\${package_name}"
