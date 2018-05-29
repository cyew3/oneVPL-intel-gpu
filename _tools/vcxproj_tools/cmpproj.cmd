echo off
set dir1=%~dp0baseproj
set dir2=%~dp0evalproj
set cmptool="C:\Program Files\KDiff3\kdiff3.exe"

echo First dir: %dir1%
echo Second dir: %dir2%

setlocal ENABLEDELAYEDEXPANSION

rem for /F %%i in ('dir /b %dir1%') do (
for /F %%i in ("libmfxhw.vcxproj") do (
    dir /b %dir1%\%%i > diffbase.txt
    dir /b %dir2%\%%i > diffeval.txt
    fc /l diffbase.txt diffeval.txt > nul
    if not "!ERRORLEVEL!"=="0" (
        echo Contents of %%i are not identical
        %cmptool% diffbase.txt diffeval.txt
    )
rem     for /F %%j in ('dir /b "%dir1%\%%i"') do (
    for /F %%j in ("Release_x64.txt") do (
      fc %dir1%\%%i\%%j %dir2%\%%i\%%j
      if not "!ERRORLEVEL!"=="0" (
        echo Contents of %%i\%%j are not identical
        %cmptool% %dir1%\%%i\%%j %dir2%\%%i\%%j
      )
    )
)


goto :EOF
:result0
  echo No differences
goto :EOF
:result1
   diffbase.txt diffeval.txt
goto :EOF
