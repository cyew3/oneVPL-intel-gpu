Param(
    [Parameter(Mandatory)]
    [string]$BuildDir,

    [Parameter(Mandatory)]
    [string]$PathToSave
)

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

$package_name = "VplWindowsDrop.zip"

foreach ($pkg_name in $PACKAGE_NAMES)
{
    $archive_dir = "$package_dir\${pkg_name}"
    New-Item -Path $archive_dir\pdb -ItemType "directory"

    New-Item -Path $archive_dir -Name $SUPPORT_FILE_NAME -ItemType "file" -Value $SUPPORT_TXT_DATA.Replace("<ID>", $pkg_name)
    New-Item -Path $archive_dir -Name $CONFIG_FILE_NAME -ItemType "file" -Value $CONFIG_INI_DATA

    Set-Location -Path $BuildDir; Copy-Item $VPL_FILES -Destination $archive_dir;
      Copy-Item $VPL_PDB_FILES -Destination $archive_dir\pdb

    Compress-Archive -Path $archive_dir\* -DestinationPath "${archive_dir}.zip"
}

Compress-Archive -Path $package_dir\*.zip -DestinationPath "$PathToSave\${package_name}"
Remove-Item $package_dir -Recurse