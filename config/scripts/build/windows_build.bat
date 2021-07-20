REM These environment variables should be set: WORKSPACE
@echo off 
set NO_COLOR=1
set CLICOLOR_FORCE=0

if exist %WORKSPACE%\Build rmdir %WORKSPACE%\Build /S /Q
mkdir %WORKSPACE%\Build\vpl\x32 %WORKSPACE%\Build\vpl\x64 %WORKSPACE%\Build\dispatcher\x32 %WORKSPACE%\Build\dispatcher\x64

set MFX_HOME=%WORKSPACE%\sources\mdp_msdk-lib
set MEDIASDK_ROOT=%WORKSPACE%\sources\msdk_root
set INTELMEDIASDK_FFMPEG_ROOT=%WORKSPACE%\sources\msdk_root\ffmpeg

set PATH=%PATH%;%WORKSPACE%\build_tools\yasm-win64

cd %WORKSPACE%\build_tools\cmake\bin

call %WORKSPACE%\build_tools\ewdk\BuildEnv\SetupBuildEnv.cmd amd64
call %WORKSPACE%\build_tools\ewdk\BuildEnv\SetupVSEnv.cmd

set MINIDDK_VERSION=%Version_Number%

set WindowsTargetPlatformVersion=%Version_Number%
set VSCMD_DEBUG=1
set WindowsSDKLibVersion=%Version_Number%\
set WindowsSDKVersion=%Version_Number%\
set WindowsSdk80Path=%WDKContentRoot%
set WindowsSdkDir_10=%WDKContentRoot%
set WindowsSdkDir_80=%WDKContentRoot%
set WindowsSdkDir_81=%WDKContentRoot%

set VCLibDirMod=spectre\

set KIT_SHARED_IncludePath=%WDKContentRoot%Include\%Version_Number%\shared
set UCRT_IncludePath=%WDKContentRoot%Include\%Version_Number%\ucrt
set UM_IncludePath=%WDKContentRoot%Include\%Version_Number%\um

set INCLUDE_ORIG=%INCLUDE%
set INCLUDE=%VCToolsInstallDir%include;%UM_IncludePath%;%UCRT_IncludePath%;%KIT_SHARED_IncludePath%;%INCLUDE_ORIG%

set UCRT_LibPath=%WDKContentRoot%Lib\%Version_Number%\ucrt\x64
set UM_LibPath=%WDKContentRoot%Lib\%Version_Number%\um\x64

set LIB_ORIG=%LIB%
set LIB=%VCToolsInstallDir%\lib\%VCLibDirMod%x64;%UM_LibPath%;%UCRT_LibPath%;%LIB_ORIG%

rem Altering PATH to allow detours find sn.exe
set PATH_ORIG=%PATH%
set PATH=%PATH_ORIG%;%WORKSPACE%\build_tools\ewdk\Program Files\Microsoft SDKs\Windows\v10.0A\bin\NETFX 4.8 Tools\x64

REM Needed so cmake can find the custom path to WDK and SDK.
set CMAKE_WINDOWS_KITS_10_DIR=%WDKContentRoot%
set UseMultiToolTask=true


set BUILDTREE=%WORKSPACE%\Build\dispatcher\x64
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_VERSION=%Version_Number% ^
      -B%BUILDTREE% -H%WORKSPACE%\sources\oneVPL-disp -DCMAKE_INSTALL_PREFIX="%BUILDTREE%/install" ^
      -DCMAKE_MAKE_PROGRAM="%WORKSPACE%\build_tools\ninja\ninja.exe" ^
      -DCMAKE_CXX_FLAGS="/DWIN32 /D_WINDOWS /W3 /GR /EHsc /Qspectre" ^
      -DCMAKE_EXE_LINKER_FLAGS_RELEASE="/CGTHREADS:8" -DCMAKE_SHARED_LINKER_FLAGS_RELEASE="/CGTHREADS:8 /DEBUG /PDB:libvpl_full.pdb /PDBSTRIPPED:libvpl.pdb" > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- Dispatcher x64: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- Dispatcher x64: build failed with %ERRORLEVEL%. & exit /B 1)

%WORKSPACE%\build_tools\ninja\ninja.exe -C %BUILDTREE% install >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- Dispatcher x64: local install failed with %ERRORLEVEL%. & exit /B 1)

set BUILDTREE=%WORKSPACE%\Build\vpl\x64
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_VERSION=%Version_Number% ^
      -DENABLE_TEXTLOG=ON -DENABLE_STAT=ON -DBUILD_VAL_TOOLS=ON -DBUILD_ALL=ON ^
      -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib ^
      -DCMAKE_MAKE_PROGRAM="%WORKSPACE%\build_tools\ninja\ninja.exe" ^
      -DCMAKE_EXE_LINKER_FLAGS_RELEASE="/CGTHREADS:8" -DCMAKE_SHARED_LINKER_FLAGS_RELEASE="/CGTHREADS:8" ^
      -DCMAKE_PREFIX_PATH="%WORKSPACE%\Build\dispatcher\x64\install\lib\cmake" ^
      -DUSE_EXTERNAL_DISPATCHER=ON > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x64: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x64: build failed with %ERRORLEVEL%. & exit /B 1)


set UCRT_LibPath=%WDKContentRoot%Lib\%Version_Number%\ucrt\x86
set UM_LibPath=%WDKContentRoot%Lib\%Version_Number%\um\x86

set LIB=%VCToolsInstallDir%\lib\%VCLibDirMod%x86;%VCToolsInstallDir%\atlmfc\lib\%VCLibDirMod%x86;%UM_LibPath%;%UCRT_LibPath%;%LIB_ORIG%
set VSCMD_ARG_TGT_ARCH=x86


set PATH=%PATH_ORIG%;%WORKSPACE%\build_tools\ewdk\Program Files\Microsoft SDKs\Windows\v10.0A\bin\NETFX 4.8 Tools
SET PATH=%PATH:Hostx64\x64=Hostx64\x86%

set

set BUILDTREE=%WORKSPACE%\Build\dispatcher\x32
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_VERSION=%Version_Number% ^
      -B%BUILDTREE% -H%WORKSPACE%\sources\oneVPL-disp -DCMAKE_INSTALL_PREFIX="%BUILDTREE%/install" ^
      -DCMAKE_MAKE_PROGRAM="%WORKSPACE%\build_tools\ninja\ninja.exe" ^
      -DCMAKE_CXX_FLAGS="/DWIN32 /D_WINDOWS /W3 /GR /EHsc /Qspectre" ^
      -DCMAKE_EXE_LINKER_FLAGS_RELEASE="/CGTHREADS:8" -DCMAKE_SHARED_LINKER_FLAGS_RELEASE="/CGTHREADS:8 /DEBUG /PDB:libvpl_full.pdb /PDBSTRIPPED:libvpl.pdb" > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- Dispatcher x32: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- Dispatcher x32: build failed with %ERRORLEVEL%. & exit /B 1)

%WORKSPACE%\build_tools\ninja\ninja.exe -C %BUILDTREE% install >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- Dispatcher x32: local install failed with %ERRORLEVEL%. & exit /B 1)


set BUILDTREE=%WORKSPACE%\Build\vpl\x32
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_VERSION=%Version_Number% ^
      -DENABLE_TEXTLOG=ON -DENABLE_STAT=ON -DBUILD_VAL_TOOLS=ON -DBUILD_ALL=ON ^
      -B%BUILDTREE% -H%WORKSPACE%\sources\mdp_msdk-lib ^
      -DCMAKE_MAKE_PROGRAM="%WORKSPACE%\build_tools\ninja\ninja.exe" ^
      -DCMAKE_EXE_LINKER_FLAGS_RELEASE="/CGTHREADS:8" -DCMAKE_SHARED_LINKER_FLAGS_RELEASE="/CGTHREADS:8" ^
      -DCMAKE_PREFIX_PATH="%WORKSPACE%\Build\dispatcher\x32\install\lib\x86\cmake" ^
      -DUSE_EXTERNAL_DISPATCHER=ON  > %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x32: cmake build tree generation failed with %ERRORLEVEL%. & exit /B 1)
cmake --build %BUILDTREE% -j %NUMBER_OF_PROCESSORS% --config Release >> %BUILDTREE%\build.log 2>&1
if "%ERRORLEVEL%" neq "0" (
  echo --- VPL x32: build failed with %ERRORLEVEL%. & exit /B 1)

cd %WORKSPACE%
 
xcopy Build\dispatcher\x64\libvpl*.* Build\vpl\x64\__bin\Release /q /y
xcopy Build\dispatcher\x32\libvpl*.* Build\vpl\x32\__bin\Release /q /y

.\build_tools\7zip-win64\7z.exe a -tzip -bt -bd -slp -mx=3 -mmt output\packages\windows\vpl.zip "Build\vpl\x??\__???" -xr"!*.ilk"
