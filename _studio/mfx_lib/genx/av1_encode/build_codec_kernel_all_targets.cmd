@echo off

if "%1"=="" goto HELP
if "%2"=="" goto HELP
if "%3"=="" goto HELP

set CODEC=%1
set KERNEL=%2
set OPTIONS=%3

echo.
echo ================= Building '%CODEC%_%KERNEL%' for hsw, bdw and skl =================

call build_codec_kernel %CODEC% %KERNEL% hsw %OPTIONS%
if not ERRORLEVEL 0 goto :BUILD_FAILED

call build_codec_kernel %CODEC% %KERNEL% bdw %OPTIONS%
if not ERRORLEVEL 0 goto :BUILD_FAILED

call build_codec_kernel %CODEC% %KERNEL% skl %OPTIONS%
if not ERRORLEVEL 0 goto :BUILD_FAILED

echo ================= OK =================
goto :EOF

:BUILD_FAILED
echo.
echo ERROR: Failed to build kernel
goto :EOF

:HELP
echo Usage: %0 [codec] [kernel] [options]
echo codec = codec, eg. hevce, av1
echo kernel = name of kernel, eg. deblock, analyze_gradient, etc
echo options = additional options for CM compiler, eg. /Qxcm_release to strip binaries
exit /b 1
GOTO :EOF
