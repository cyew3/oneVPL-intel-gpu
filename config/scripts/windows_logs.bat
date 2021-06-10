@echo off 
setlocal

set NO_COLOR=1
set CLICOLOR_FORCE=0

echo "**** Dispatcher x64 ****" 
if exist %WORKSPACE%\Build\dispatcher\x64\build.log cat %WORKSPACE%\Build\dispatcher\x64\build.log

echo "**** oneVPL x64 ****" 
if exist %WORKSPACE%\Build\vpl\x64\build.log cat %WORKSPACE%\Build\vpl\x64\build.log

echo "**** Dispatcher x32 ****" 
if exist %WORKSPACE%\Build\dispatcher\x32\build.log cat %WORKSPACE%\Build\dispatcher\x32\build.log

echo "**** oneVPL x32 ****" 
if exist %WORKSPACE%\Build\vpl\x32\build.log cat %WORKSPACE%\Build\vpl\x32\build.log
