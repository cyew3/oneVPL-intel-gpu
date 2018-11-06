IF "%PROCESSOR_ARCHITECTURE%"=="x86" set pc_arch=win32
IF "%PROCESSOR_ARCHITEW6432%"=="x86" set pc_arch=win32
IF "%PROCESSOR_ARCHITECTURE%"=="AMD64" set pc_arch=x64
IF "%PROCESSOR_ARCHITEW6432%"=="AMD64" set pc_arch=x64

if not defined pc_arch set pc_arch=win32

;if not defined test_arch if "%pc_arch%"=="x64" (set test_arch=x64) else (set test_arch=win32)

if not defined target_fnc set target_fnc=dispatch_trace

echo %target_fnc%

@if "%pc_arch%"=="win32" (
 echo.Dispatching trace for win32 platform
 rundll32 msdk_analyzer_mbcs.dll,%target_fnc% %1 %2 %3
)ELSE (
 echo.Dispatching trace for win32 platform
 %SystemRoot%\SysWOW64\rundll32 msdk_analyzer_mbcs.dll,%target_fnc% %1 %2 %3
 echo.
 echo.Dispatching trace for x64 platform
 rundll32 msdk_analyzer_x64_mbcs.dll,%target_fnc%  %1 %2 %3 
)
