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

$DISPATCHER_X32 = 'vpl\x32\__bin\Release\libvpl.dll'
$DISPATCHER_X32_PDB = 'vpl\x32\__bin\Release\libvpl.pdb'
$DISPATCHER_X64 = 'vpl\x64\__bin\Release\libvpl.dll'
$DISPATCHER_X64_PDB = 'vpl\x64\__bin\Release\libvpl.pdb'

$DISPATCHER_X32_NAME = 'vpl_dispatcher_32.dll'
$DISPATCHER_X32_PDB_NAME = 'vpl_dispatcher_32.pdb'
$DISPATCHER_X64_NAME = 'vpl_dispatcher_64.dll'
$DISPATCHER_X64_PDB_NAME = 'vpl_dispatcher_64.pdb'

$package_dir = "$PathToSave\to_archive"

$package_name = "VplWindowsDrop.zip"

foreach ($pkg_name in $PACKAGE_NAMES)
{
    $archive_dir = "$package_dir\${pkg_name}"
    New-Item -Path $archive_dir\pdb -ItemType "directory"

    New-Item -Path $archive_dir -Name $SUPPORT_FILE_NAME -ItemType "file" -Value $SUPPORT_TXT_DATA.Replace("<ID>", $pkg_name)
    New-Item -Path $archive_dir -Name $CONFIG_FILE_NAME -ItemType "file" -Value $CONFIG_INI_DATA

    Set-Location -Path $BuildDir; Copy-Item $VPL_FILES -Destination $archive_dir;
      Copy-Item $VPL_PDB_FILES -Destination $archive_dir\pdb;
      Copy-Item $DISPATCHER_X32 -Destination $archive_dir\$DISPATCHER_X32_NAME;
      Copy-Item $DISPATCHER_X32_PDB -Destination $archive_dir\pdb\$DISPATCHER_X32_PDB_NAME;
      Copy-Item $DISPATCHER_X64 -Destination $archive_dir\$DISPATCHER_X64_NAME;
      Copy-Item $DISPATCHER_X64_PDB -Destination $archive_dir\pdb\$DISPATCHER_X64_PDB_NAME;

    Set-Location -Path $env:WORKSPACE
    #Compress-Archive -Path $archive_dir\* -DestinationPath "${archive_dir}.zip"
    .\build_tools\7zip-win64\7z.exe a -tzip -bt -bd -slp -mx=3 -mmt "${archive_dir}.zip" "$archive_dir\*"
}

# Compress-Archive -Path $package_dir\*.zip -DestinationPath "$PathToSave\${package_name}"
.\build_tools\7zip-win64\7z.exe a -tzip -bt -bd -slp -mx=3 -mmt "$PathToSave\${package_name}" "$package_dir\*.zip"

Remove-Item $package_dir -Recurse