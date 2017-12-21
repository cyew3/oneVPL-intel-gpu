@echo off

set _MFX_VERSION_JOINED=0

for /f "tokens=3 delims= " %%i in ('findstr /B /C:"#define MFX_VERSION_M" %~6\mfxdefs.h') do (
    set /a "_MFX_VERSION_JOINED<<=8"
    set /a "_MFX_VERSION_JOINED+=%%i"
)

(
echo PluginVersion = %1
echo APIVersion    = %_MFX_VERSION_JOINED%
echo FileName32    = "%232%7.dll"
echo FileName64    = "%264%7.dll"
echo Type          = %3
echo Default       = %4
) > %5\plugin.cfg

set _MFX_VERSION_JOINED=
