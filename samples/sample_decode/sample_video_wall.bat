@echo off
::
:: INTEL CORPORATION PROPRIETARY INFORMATION
:: This software is supplied under the terms of a license agreement or
:: nondisclosure agreement with Intel Corporation and may not be copied
:: or disclosed except in accordance with the terms of that agreement.
:: Copyright (c) 2005-2014 Intel Corporation. All Rights Reserved.
::

setlocal enabledelayedexpansion

set VerticalCells=4
set HorizontalCells=4

rem Set bTitle (0 - off | 1 - on,  will provide rendering rate info) 
set bTitle=0

rem Set MediaSDK implementation (-hw - Hardware,  Use platform-specific implementation of Intel® Media SDK) 
rem set MediaSDKImplementation=-hw


rem Set media type (h264 | mpeg2 | vc1) 
if not defined MediaType set MediaType=h264
rem Set monitor (default: 0) 
set nMonitor=0
rem Set frame rate for rendering
set FrameRate=30
rem Set timeout in seconds
set Timeout=30


if "%1"=="" goto :help

if exist %1\ (set Video_dir=%1) else (if exist %1 (set VideoFile=%1) else (goto :help))

set /a nVideoFiles=0
if defined Video_dir for %%a in (%Video_dir%\*) do set VideoFile!nVideoFiles!=%%a& set /a nVideoFiles+=1
if defined Video_dir if "%nVideoFiles%"=="0" echo Error: No video files found& goto :help

set /a nCells=%VerticalCells% * %HorizontalCells% - 1
set /a nVideoFiles1=0
for /l %%a in (0,1,%nCells%) do (
  if defined Video_dir call :get_VideoFile !nVideoFiles1!
  start /b "" "%~dp0sample_decode.exe" %MediaType% -i "!VideoFile!" %MediaSDKImplementation% -d3d -wall %HorizontalCells% %VerticalCells% %%a %nMonitor% %FrameRate% %bTitle% %Timeout%
  set /a nVideoFiles1+=1
  if !nVideoFiles1! GEQ %nVideoFiles% set /a nVideoFiles1=0
)
goto :eof

:get_VideoFile
set VideoFile=!VideoFile%1!
goto :eof

:help
echo Intel(R) Media SDK Video Wall Sample Script
echo.
echo Usage: %~nx0 [stream ^| streams folder]
echo.
pause