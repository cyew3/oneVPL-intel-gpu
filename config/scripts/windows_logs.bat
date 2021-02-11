@echo off 
setlocal

set NO_COLOR=1
set CLICOLOR_FORCE=0

echo "**** MSVC x64 ****" 
if exist  %WORKSPACE%\Build\msvc\x64\build.log cat %WORKSPACE%\Build\msvc\x64\build.log

echo "**** oneVPL x64 ****" 
if exist %WORKSPACE%\Build\VPL_build\x64\build.log cat %WORKSPACE%\Build\VPL_build\x64\build.log

echo "**** UWP x64 ****" 
if exist %WORKSPACE%\Build\uwp\x64\build.log cat %WORKSPACE%\Build\uwp\x64\build.log

echo "**** MFTS x64 ****" 
if exist %WORKSPACE%\Build\mfts\x64\build.log cat %WORKSPACE%\Build\mfts\x64\build.log

echo "**** MSVC x32 ****" 
if exist  %WORKSPACE%\Build\msvc\x32\build.log cat %WORKSPACE%\Build\msvc\x32\build.log

echo "**** oneVPL x32 ****" 
if exist %WORKSPACE%\Build\VPL_build\x32\build.log cat %WORKSPACE%\Build\VPL_build\x32\build.log

echo "**** UWP x32 ****" 
if exist %WORKSPACE%\Build\uwp\x32\build.log cat %WORKSPACE%\Build\uwp\x32\build.log

echo "**** MFTS x32 ****" 
if exist %WORKSPACE%\Build\mfts\x32\build.log cat %WORKSPACE%\Build\mfts\x32\build.log