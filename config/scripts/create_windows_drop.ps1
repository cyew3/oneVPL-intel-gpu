Param(
    [Parameter(Mandatory)]
    [string]$BuildDir,

    [Parameter(Mandatory)]
    [string]$Msdk1xPath,

    [Parameter(Mandatory)]
    [string]$PathToSave
)

Set-PSDebug -Trace 1
$global:ProgressPreference = 'SilentlyContinue'
$global:ErrorActionPreference = 'Stop'

$VPL_FILES=@(
    'vpl\x64\__bin\Release\libmfx64-gen.dll',
    'vpl\x32\__bin\Release\libmfx32-gen.dll'
)

$VPL_PDB_FILES=@(
    'vpl\x64\__bin\Release\libmfx64-gen.pdb',
    'vpl\x32\__bin\Release\libmfx32-gen.pdb'
)

$PACKAGE_NAMES = @(
    'MediaSDK-64-bit',
    'MediaSDK_Release_Internal-64-bit'
)

$package_dir = "$PathToSave\to_archive"
New-Item -ItemType "directory" -Path $package_dir

$package_name = "WindowsDrop.zip"

Expand-Archive -Path "$Msdk1xPath" -DestinationPath $package_dir

foreach ($pkg_name in $PACKAGE_NAMES)
{
    $archive_dir = "$package_dir\${pkg_name}"
    Expand-Archive -Path "$archive_dir.zip" -DestinationPath $archive_dir
    Remove-Item "${archive_dir}.zip"

    Set-Location -Path $BuildDir; Copy-Item $VPL_FILES -Destination $archive_dir;
      Copy-Item $VPL_PDB_FILES -Destination $archive_dir\pdb

    Compress-Archive -Path $archive_dir\* -DestinationPath "${archive_dir}.zip"
}

Compress-Archive -Path $package_dir\*.zip -DestinationPath "$PathToSave\${package_name}"
Remove-Item $package_dir -Recurse