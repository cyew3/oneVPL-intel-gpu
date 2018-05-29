echo off
rem Check input parameters
if "%~1"=="" goto showhelp
if NOT EXIST "%~1" goto notexist

rem Set environment variables
cls
call "%~dp000.setenv.cmd"

goto dowork

:dowork
echo ---------------------------------------------------------------------------------------------
echo PowerShell.exe -PSConsoleFile "%~dp0remakeproj.ps1" "%~1"
PowerShell.exe -ExecutionPolicy Unrestricted -File "%~dp0remakeproj.ps1" "%~1"
echo =============================================================================================
goto :eof

:showhelp
echo Usage
echo filename.cmd path_to_vcxproj
goto :eof

:notexist
echo File %1 is not exist
echo Usage
echo filename.cmd path_to_vcxproj
goto :eof