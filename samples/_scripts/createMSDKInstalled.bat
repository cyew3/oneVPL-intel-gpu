@echo off
IF "%~1%"=="" (
	echo Target Dir is not set
	echo Usage: createMSDKInstalled.bat ^<Target_Dir^> ^<MFXLib_API_Dir^>
	exit /b
)

IF "%~2%"=="" (
	echo MFXLib_API_Dir is not set
	echo Usage: createMSDKInstalled.bat ^<Target_Dir^> ^<MFXLib_API_Dir^> ^<samples_Dir^>
	exit /b
)

set TargetDir="%~1"
set APIDir="%~2"
set BuildDir="%~2\..\Build"

mkdir %TargetDir%

mkdir %TargetDir%\bin
mkdir %TargetDir%\bin\win32
mkdir %TargetDir%\bin\x64

copy /Y %BuildDir%\win_Win32\bin\libmfx*.dll %TargetDir%\bin\win32\*.*
copy /Y %BuildDir%\win_x64\bin\libmfx*.dll %TargetDir%\bin\x64\*.*

rem mkdir %TargetDir%\igfx_s3dcontrol
rem mkdir %TargetDir%\igfx_s3dcontrol\include
rem mkdir %TargetDir%\igfx_s3dcontrol\lib\win32
rem mkdir %TargetDir%\igfx_s3dcontrol\lib\x64

mkdir %TargetDir%\include
rem erase /Q %TargetDir%\include\*.*
copy %APIDir%\include\*.* %TargetDir%\include\*.*

mkdir %TargetDir%\lib
mkdir %TargetDir%\lib\win32
mkdir %TargetDir%\lib\x64

copy /Y %BuildDir%\win_Win32\lib\libmfx_vs2015.lib %TargetDir%\lib\win32\*.*
copy /Y %BuildDir%\win_Win32\lib\libmfx_vs2015_d.lib %TargetDir%\lib\win32\*.*
copy /Y %BuildDir%\win_x64\lib\libmfx_vs2015.lib %TargetDir%\lib\x64\*.*
copy /Y %BuildDir%\win_x64\lib\libmfx_vs2015_d.lib %TargetDir%\lib\x64\*.*

setx INTELMEDIASDKROOT %TargetDir%