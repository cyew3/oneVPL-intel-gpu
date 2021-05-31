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

# Expand-Archive -Path "$Msdk1xPath" -DestinationPath $package_dir
.\build_tools\7zip-win64\7z.exe  x -y -bt -bd -mmt "$Msdk1xPath" -o"$package_dir"

foreach ($pkg_name in $PACKAGE_NAMES)
{
    $archive_dir = "$package_dir\${pkg_name}"

    # Expand-Archive -Path "$archive_dir.zip" -DestinationPath $archive_dir
    .\build_tools\7zip-win64\7z.exe  x -y -bt -bd -mmt "$archive_dir.zip" -o"$archive_dir"

    Remove-Item "${archive_dir}.zip"

    Set-Location -Path $BuildDir
    Copy-Item $VPL_FILES -Destination $archive_dir
    Copy-Item $VPL_PDB_FILES -Destination $archive_dir\pdb

    Set-Location -Path $env:WORKSPACE
    #Compress-Archive -Path $archive_dir\* -DestinationPath "${archive_dir}.zip"
    .\build_tools\7zip-win64\7z.exe a -tzip -bt -bd -slp -mx=3 -mmt "${archive_dir}.zip" "$archive_dir\*"
}

# Compress-Archive -Path $package_dir\*.zip -DestinationPath "$PathToSave\${package_name}"
.\build_tools\7zip-win64\7z.exe a -tzip -bt -bd -slp -mx=3 -mmt "$PathToSave\${package_name}" "$package_dir\*.zip"

Remove-Item $package_dir -Recurse