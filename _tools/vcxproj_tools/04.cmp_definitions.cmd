echo off
setlocal EnableDelayedExpansion
rem echo Comparison > %~1.txt
for /F %%p in ('dir /b .\%~1_mod') do (
  echo %%p
  for /F %%f in ('dir /b .\%~1_mod\%%p') do (
    rem echo %%f
    call :lookfordefinitions %1 %%p %%f
    set src=!pd1!
    set srcpts=!pts!
    set srctpv=!tpv!
    call :lookfordefinitions %1_mod %%p %%f
    set dst=!pd1!
    set dstpts=!pts!
    set dsttpv=!tpv!
    echo %%~nf
    echo SRC: !src!
    echo MOD: !dst!
    echo SRC: !srcpts!
    echo MOD: !dstpts!
    echo SRC: !srctpv!
    echo MOD: !dsttpv!
  )
  echo ProjectName: !pn!
  echo ProjectGuid: !pg!
  echo =============================================================================================
)

rem del /q dirlist.txt

:lookfordefinitions
    set /a idx=0
    if not exist .\%~1\%~2\%~3 (
      set pd0=N/A
      set pd1=N/A
      set pd2=N/A
      set pd4=N/A
      set pts=N/A
      set tpv=N/A
      goto :eof
    )
    for /F "tokens=1,* delims==" %%i in (.\%~1\%~2\%~3) do (
      if "%%i"=="PreprocessorDefinitions" (
        set pd!idx!=%%j
        rem echo %~1 %~2 %~3 %%i %%j
        call :inc
      )
      if "%%i"=="PlatformToolset" (
        set pts=%%j
      )
      if "%%i"=="WindowsTargetPlatformVersion" (
        set tpv=%%j
      )
      if "%%i"=="ProjectName" (
        set pn=%%j
      )
      if "%%i"=="ProjectGuid" (
        set pg=%%j
      )
    )
goto :eof

:inc
set /a idx=idx+1
goto :eof