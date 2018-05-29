echo off
setlocal EnableDelayedExpansion
for /F %%p in ('dir /b .\%~1') do (
  rem echo %%p
  for /F %%f in ('dir /b .\%~1\%%p') do (
    rem echo %%f
    call :lookfordefinitions %1 %%p %%f
    echo %%p	%%~nf	!pd1!
  )
)

:lookfordefinitions
    set /a idx=0
    for /F "tokens=1,2* delims==" %%i in (.\%~1\%~2\%~3) do (
      if "%%i"=="PreprocessorDefinitions" (
        set pd!idx!=%%j
        rem echo %~1 %~2 %~3 %%i %%j
        call :inc
      )
      if "%%i"=="AdditionalIncludeDirectories" (
        rem echo Includes: %%j
      )
    )
goto :eof

:inc
set /a idx=idx+1
goto :eof
