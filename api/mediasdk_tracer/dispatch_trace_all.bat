@echo off

set second_run=
set verbose=%1

set invoked_script=dispatch_trace.bat
set target_fnc=dispatch_trace_skip_analyzer


if "%verbose%"=="-v" ( 
set TMPFILE=%2 
goto :CONTINUE_PROCESSING
)

:GETTEMPNAME
set TMPFILE=%TMP%\mytempfile-%RANDOM%-%TIME:~6,5%.tmp
if exist "%TMPFILE%" GOTO :GETTEMPNAME 

:CONTINUE_PROCESSING


@call %invoked_script% > "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"
@call %invoked_script% -h >> "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"
@call %invoked_script% -ha >> "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"
@call %invoked_script% -h2 >> "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"
@call %invoked_script% -h3 >> "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"
@call %invoked_script% -h4 >> "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"
@call %invoked_script% -s >> "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"
@echo ------------------------------------------------------------------------------------>> "%TMPFILE%"

@if defined second_run goto :SECOND

@echo MEDIASDK libraries that will be used by application
@find "DISPRESULT" "%TMPFILE%"

set second_run=1
set target_fnc=dispatch_trace
GOTO :CONTINUE_PROCESSING
:SECOND

@echo.
@echo MEDIASDK libraries that will be used by msdk_analyzer
@find "DISPRESULT" "%TMPFILE%"

@echo on
@pause