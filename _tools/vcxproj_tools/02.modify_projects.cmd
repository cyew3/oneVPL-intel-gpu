echo off
rem Check input parameters
if "%~1"=="" goto showhelp
if NOT EXIST "%~1" goto notexist

rem Set environment variables
cls
call "%~dp000.setenv.cmd"

goto dowork

:dowork
set workdir="%~dp0%~1"
if NOT EXIST %workdir% goto notexist
cd %workdir%
for /F "tokens=1,* delims==" %%i in (%~nx1.sln.txt) do (
  echo ---------------------------------------------------------------------------------------------
  echo Project: %%i
  echo File name: %%j
  if exist %%j.bak (
    del /q %%j
    copy %%j.bak %%j
  )
  echo PowerShell.exe -PSConsoleFile "%~dp0remakeproj.ps1" %%j
  PowerShell.exe -ExecutionPolicy Unrestricted -File "%~dp0remakeproj.ps1" %%j
  echo =============================================================================================
)
cd ..
goto :eof

:showhelp
echo Usage
echo filename.cmd path_to_solution_analyze
goto :eof

:notexist
echo File %1 is not exist
echo Usage
echo filename.cmd path_to_solution_analyze
goto :eof