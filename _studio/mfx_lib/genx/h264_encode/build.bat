echo off
echo Loading VS environment
call "%VS140COMNTOOLS%vsvars32.bat"
SET INSTALLDIR=%MEDIASDK_ROOT%\CMRT
SET ICLDIR=%INSTALLDIR%\compiler
SET PATH=%ICLDIR%\bin;%ICLDIR%\lib;%PATH%
SET BASE_INCLUDE=%INCLUDE%

if not exist ".\embed_isa.exe" (
  echo Compiling embedding tool
  cl .\src\embed_isa.c
)

setlocal ENABLEDELAYEDEXPANSION

SET INCLUDE=%ICLDIR%\include;%ICLDIR%\include\cm;%BASE_INCLUDE%
rem           NAME     SUFFIX TARGET
call :compile "BDW"    bdw    gen8
call :compile "SKL"    skl    gen9
call :compile "ICL"    icl    gen11
call :compile "ICL LP" icllp  gen11lp
call :compile "TGL"    tgl    gen12
call :compile "TGL LP" tgllp  gen12lp

rem                   NAME     SUFFIX TARGET  FEATURE
call :compile_feature "BDW"    bdw    gen8    h264_scd
call :compile_feature "SKL"    skl    gen9    h264_scd
call :compile_feature "ICL"    icl    gen11   h264_scd
call :compile_feature "ICL LP" icllp  gen11lp h264_scd
call :compile_feature "TGL"    tgl    gen12   h264_scd
call :compile_feature "TGL LP" tgllp  gen12lp h264_scd

rem                   NAME     SUFFIX TARGET  FEATURE
call :compile_feature "SKL"    skl    gen9    histogram
call :compile_feature "ICL"    icl    gen11   histogram
call :compile_feature "ICL LP" icllp  gen11lp histogram
call :compile_feature "TGL"    tgl    gen12   histogram
call :compile_feature "TGL LP" tgllp  gen12lp histogram

SET INCLUDE=%BASE_INCLUDE%
pause

goto :eof

:compile
set platform=%~1
set codename=%~2
set target=%~3
echo Building kernels for !platform!
cmc -c -Qxcm_release -Qxcm_jit_target=!target! -Qxcm_print_asm_count -mCM_no_debug .\src\genx_skl_simple_me.cpp -o .\genx_!codename!_simple_me.isa
if exist genx_!codename!_simple_me.isa (
    embed_isa genx_!codename!_simple_me.isa
    move genx_!codename!_simple_me.cpp .\src\genx_!codename!_simple_me_isa.cpp
    move genx_!codename!_simple_me.h .\include\genx_!codename!_simple_me_isa.h
    call :cleanup
) else (
    echo Cannot build kernels!
    pause
)
goto :eof

:compile_feature
set platform=%~1
set codename=%~2
set target=%~3
set feature=%~4
echo Building kernels for !platform!
cmc -c -Qxcm_release -Qxcm_jit_target=!target! -Qxcm_print_asm_count -mCM_no_debug .\src\genx_!feature!.cpp -o .\genx_!codename!_!feature!.isa
if exist genx_!codename!_!feature!.isa (
    embed_isa genx_!codename!_!feature!.isa
    move genx_!codename!_!feature!.cpp .\src\genx_!codename!_!feature!_isa.cpp
    move genx_!codename!_!feature!.h .\include\genx_!codename!_!feature!_isa.h
    echo Building SUCCESSFUL!
    call :cleanup
) else (
    echo Cannot build kernels! ERROR!
    pause
)
goto :eof

:cleanup
echo Preparing cleanup
del /q .\*.isa
del /q .\*.asm
del /q .\*.dat
del /q .\*.visaasm
del /q .\*.obj
echo Cleanup complete
goto :eof
