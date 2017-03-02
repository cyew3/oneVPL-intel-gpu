@echo off
IF "%~1%"=="" (
	echo Target Dir is not set
	echo Usage: createMSDKInstalled.bat ^<Target_Dir^> ^<MFXLib_API_Dir^> ^<samples_Dir^>
	exit /b
)

IF "%~2%"=="" (
	echo MFXLib_API_Dir is not set
	echo Usage: createMSDKInstalled.bat ^<Target_Dir^> ^<MFXLib_API_Dir^> ^<samples_Dir^>
	exit /b
)

IF "%~3%"=="" (
	echo Sample_Dir is not set
	echo Usage: createMSDKInstalled.bat ^<Target_Dir^> ^<MFXLib_API_Dir^> ^<samples_Dir^>
	exit /b
)


set TargetDir="%~1"
set APIDir="%~2"
set BuildDir="%~2\..\Build"
set SamplesDir="%~3"

mkdir %TargetDir%

mkdir %TargetDir%\bin
mkdir %TargetDir%\bin\win32
mkdir %TargetDir%\bin\x64

copy /Y %BuildDir%\win_Win32\bin\libmfx*.dll %TargetDir%\bin\win32\*.*
copy /Y %BuildDir%\win_x64\bin\libmfx*.dll %TargetDir%\bin\x64\*.*

mkdir %TargetDir%\igfx_s3dcontrol
mkdir %TargetDir%\igfx_s3dcontrol\include
mkdir %TargetDir%\igfx_s3dcontrol\lib\win32
mkdir %TargetDir%\igfx_s3dcontrol\lib\x64

copy %SamplesDir%\..\igfx_s3dcontrol\include\API\*.* %TargetDir%\igfx_s3dcontrol\include\*.*
copy %SamplesDir%\..\igfx_s3dcontrol\_build\lib\win32\*.* %TargetDir%\igfx_s3dcontrol\lib\win32\*.*
copy %SamplesDir%\..\igfx_s3dcontrol\_build\lib\x64\*.* %TargetDir%\igfx_s3dcontrol\lib\x64\*.*


mkdir %TargetDir%\include
rem erase /Q %TargetDir%\include\*.*
copy %APIDir%\include\*.* %TargetDir%\include\*.*

mkdir %TargetDir%\lib
mkdir %TargetDir%\lib\win32
mkdir %TargetDir%\lib\x64

copy /Y %BuildDir%\win_Win32\lib\libmfx.lib %TargetDir%\lib\win32\*.*
copy /Y %BuildDir%\win_Win32\lib\libmfx_d.lib %TargetDir%\lib\win32\*.*
copy /Y %BuildDir%\win_x64\lib\libmfx.lib %TargetDir%\lib\x64\*.*
copy /Y %BuildDir%\win_x64\lib\libmfx_d.lib %TargetDir%\lib\x64\*.*

setx INTELMEDIASDKROOT %TargetDir%