@echo off
setlocal

if "%1"=="" goto HELP
if "%2"=="" goto HELP

set KERNEL=%1
set OPTIONS=%~3

set JIT_TARGET=
if "%2"=="hsw" set JIT_TARGET=gen7_5 & set PLATFORM_NAME=hsw
if "%2"=="bdw" set JIT_TARGET=gen8 & set PLATFORM_NAME=bdw
if "%2"=="skl" set JIT_TARGET=gen9 & set PLATFORM_NAME=skl
if "%JIT_TARGET%"=="" goto HELP

set CURDIR=%cd%
set ISA_FILENAME_BUILD=genx_%KERNEL%
set ISA_FILENAME_FINAL=genx_%KERNEL%_%PLATFORM_NAME%

echo.
echo ================= Build '%KERNEL%' for %PLATFORM_NAME% =================
del /Q %CURDIR%\src\%ISA_FILENAME_FINAL%_isa.cpp
del /Q %CURDIR%\include\%ISA_FILENAME_FINAL%_isa.h

echo === Setup workspace ===
set TMP=%CURDIR%

xcopy /Q /R /Y /I .\include\* %TMP%\include
xcopy /Q /R /Y /I .\src\* %TMP%\src

cd %TMP%

SET VSTOOLS=%VS110COMNTOOLS:~0, -14%vc\bin
IF not EXIST "%VSTOOLS%" GOTO :VSTOOLS_NOT_FOUND
call "%VSTOOLS%\x86_amd64\vcvarsx86_amd64.bat"

SET ICLDIR=%MEDIASDK_ROOT%\CMRT\compiler
IF not EXIST "%ICLDIR%" GOTO :CM_COMPILER_NOT_FOUND
SET PATH=%ICLDIR%\bin;%PATH%
SET INCLUDE=%ICLDIR%\include;%ICLDIR%\include\cm;%MSVSINCL%;%INCLUDE%

echo === Run CM compiler ===
del /Q %ISA_FILENAME_FINAL%.isa %ISA_FILENAME_BUILD%.isa
set CM_COMPILER_OPTIONS=/c /Qunroll1 /Qxcm /Qxcm_jit_target=%JIT_TARGET% %CURDIR%\src\genx_%KERNEL%.cpp /mGLOB_override_limits /mCM_print_asm_count /mCM_printregusage /Dtarget_%JIT_TARGET% %OPTIONS%
echo CM compiler options: %CM_COMPILER_OPTIONS%
icl %CM_COMPILER_OPTIONS%
if not EXIST %ISA_FILENAME_BUILD%.isa goto :BUILD_KERNEL_FAILED
ren %ISA_FILENAME_BUILD%.isa %ISA_FILENAME_FINAL%.isa

echo === Build ISA embedder ===
cl %CURDIR%\src\embed_isa.c /nologo
if not ERRORLEVEL 0 goto :BUILD_EMBEDDER_FAILED

echo === Run ISA embedder ===
embed_isa.exe %ISA_FILENAME_FINAL%.isa
if not ERRORLEVEL 0 goto :EMBED_ISA_FAILED

echo === Copy embedded ISA ===
copy /Y %ISA_FILENAME_FINAL%.cpp %CURDIR%\src\%ISA_FILENAME_FINAL%_isa.cpp
if not ERRORLEVEL 0 goto :COPY_ISA_FAILED
copy /Y %ISA_FILENAME_FINAL%.h %CURDIR%\include\%ISA_FILENAME_FINAL%_isa.h
if not ERRORLEVEL 0 goto :COPY_ISA_FAILED

cd %CURDIR%

echo ================= OK ================= 
goto :EOF

:VSTOOLS_NOT_FOUND
echo ERROR: MSVS2012 not found
goto :EOF

:CM_COMPILER_NOT_FOUND
echo ERROR: CM compiler not found
goto :EOF

:BUILD_KERNEL_FAILED
echo.
echo ERROR: Failed to build kernel
goto :EOF

:BUILD_EMBEDDER_FAILED
echo.
echo ERROR: Failed to build embed_isa
goto :EOF

:EMBED_ISA_FAILED
echo.
echo ERROR: Failed embed_isa
goto :EOF

:COPY_ISA_FAILED
echo.
echo ERROR: Failed to copy embedded isa
goto :EOF

:HELP
echo Usage: %0 [kernel] [platform] [options]
echo kernel = name of kernel, eg. deblock, analyze_gradient, etc
echo platform = hsw/bdw 
echo options = additional options for CM compiler, eg. /Qxcm_release to strip binaries
exit /b 1
GOTO :EOF
