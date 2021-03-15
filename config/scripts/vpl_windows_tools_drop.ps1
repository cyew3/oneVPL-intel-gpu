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
    'vpl\x64\__bin\Release\sample_multi_transcode.exe'
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

Expand-Archive -Path "$Msdk1xPath" -DestinationPath $package_dir

Set-Location -Path $BuildDir; Copy-Item $FILES -Destination $package_dir\imports\mediasdk
Set-Location -Path $BuildDir; Copy-Item $FILES_WIN32 -Destination $package_dir\imports\mediasdk\Win32
Set-Location -Path $BuildDir; Copy-Item $FILES_WIN64 -Destination $package_dir\imports\mediasdk\Win64

Compress-Archive -Path $package_dir* -DestinationPath $PathToSave\$package_name
Remove-Item $package_dir -Recurse
