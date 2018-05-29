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
set workdir="%~dp0%~n1"
if NOT EXIST %workdir% mkdir %workdir%
cd %workdir%
%msbuilddump% "%~1"
echo Listing solution projects is done
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