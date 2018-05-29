echo off
rem Check input parameters
if "%~1"=="" goto showhelp
if NOT EXIST "%~1" goto notexist

rem Set environment variables
cls
call "%~dp000.setenv.cmd"
echo MSBuildDumper: %msbuilddump%

goto dowork

:dowork
set workdir="%~dp0%~n1_mod"
if NOT EXIST %workdir% mkdir %workdir%
cd %workdir%
%msbuilddump% "%~1"
echo Listing solution projects is done
for /F "tokens=1,* delims==" %%i in (%~nx1.txt) do (
  echo ---------------------------------------------------------------------------------------------
  echo Project: %%i
  echo File name: %%j
  if NOT EXIST %%i mkdir %%i
  cd %%i
  %msbuilddump% %%j %~dp1 %~nx1
  cd ..
  echo =============================================================================================
)
cd ..
goto :eof

:showhelp
echo Usage
echo filename.cmd path_to_solution
goto :eof

:notexist
echo File %1 is not exist
echo Usage
echo filename.cmd path_to_solution
goto :eof