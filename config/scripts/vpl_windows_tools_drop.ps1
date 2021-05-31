Param(
    [Parameter(Mandatory)]
    [string]$BuildDir,

    [Parameter(Mandatory)]
    [string]$Msdk1xPath,

    [Parameter(Mandatory)]
    [string]$PathToSave
)

$global:ProgressPreference = 'SilentlyContinue'
$global:ErrorActionPreference = 'Stop'

$FILES=@(
    'vpl\x64\__bin\Release\mfx_player.exe',
    'vpl\x64\__bin\Release\mfx_transcoder.exe',
    'vpl\x64\__bin\Release\msdk_gmock.exe',
    'vpl\x64\__bin\Release\sample_encode.exe',
    'vpl\x64\__bin\Release\sample_multi_transcode.exe',
    'vpl\x64\__bin\Release\libvpl.dll'
)

$FILES_WIN32=@(
    'vpl\x32\__bin\Release\mfx_player.exe',
    'vpl\x32\__bin\Release\mfx_transcoder.exe'
)

$FILES_WIN64=@(
    'vpl\x64\__bin\Release\mfx_player.exe',
    'vpl\x64\__bin\Release\mfx_transcoder.exe'
)

$package_dir="$PathToSave\to_archive"
New-Item -ItemType "directory" -Path $package_dir

$package_name="VplWindowsToolsDrop.zip"

# Expand-Archive -Path "$Msdk1xPath" -DestinationPath $package_dir
.\build_tools\7zip-win64\7z.exe  x -y -bt -bd -mmt "$Msdk1xPath" -o"$package_dir"

Set-Location -Path $BuildDir; Copy-Item $FILES -Destination $package_dir\imports\mediasdk
Set-Location -Path $BuildDir; Copy-Item $FILES_WIN32 -Destination $package_dir\imports\mediasdk\Win32
Set-Location -Path $BuildDir; Copy-Item $FILES_WIN64 -Destination $package_dir\imports\mediasdk\Win64

Set-Location -Path $env:WORKSPACE

# Compress-Archive -Path $package_dir\* -DestinationPath $PathToSave\$package_name
.\build_tools\7zip-win64\7z.exe a -tzip -bt -bd -slp -mx=3 -mmt "$PathToSave\$package_name" "$package_dir\*"

Remove-Item $package_dir -Recurse
