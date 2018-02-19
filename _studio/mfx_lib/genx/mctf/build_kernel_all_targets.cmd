@echo off

if "%1"=="" goto HELP
if "%2"=="" goto HELP

set KERNEL=%1
set OPTIONS=%2

echo.
echo ================= Building '%KERNEL%' for bdw =================

call build_kernel %KERNEL% bdw %OPTIONS%
if not ERRORLEVEL 0 goto :BUILD_FAILED

echo.
echo ================= Building '%KERNEL%' for skl =================

call build_kernel %KERNEL% skl %OPTIONS%
if not ERRORLEVEL 0 goto :BUILD_FAILED

rem echo.
rem echo ================= Building '%KERNEL%' for kbl =================

rem call build_kernel %KERNEL% kbl %OPTIONS%
if not ERRORLEVEL 0 goto :BUILD_FAILED

rem echo.
rem echo ================= Building '%KERNEL%' for cnl =================

rem call build_kernel %KERNEL% cnl %OPTIONS%
rem if not ERRORLEVEL 0 goto :BUILD_FAILED

rem call build_kernel %KERNEL% bdw %OPTIONS%
rem if not ERRORLEVEL 0 goto :BUILD_FAILED

echo ================= OK ================= 
goto :EOF

:BUILD_FAILED
echo.
echo ERROR: Failed to build kernel
goto :EOF

:HELP
echo Usage: %0 [kernel] [options]
echo kernel = name of kernel, eg. deblock, analyze_gradient, etc
echo options = additional options for CM compiler, eg. /Qxcm_release to strip binaries
exit /b 1
GOTO :EOF
