@echo off
IF "%~1%"=="" (
	echo Target Dir is not set
	echo usage: createLinuxPackage.bat ^<TargetDir^> ^<BinarySourceDir^> ^<DocSourceDir^> ^<OpenCLSamplesSourceDir^>
	exit /b
)

IF "%~2%"=="" (
	echo Binary Source Dir is not set
	echo usage: createLinuxPackage.bat ^<TargetDir^> ^<BinarySourceDir^> ^<DocSourceDir^> ^<OpenCLSamplesSourceDir^>
	exit /b
)

IF "%~3%"=="" (
	echo Docs Source Dir is not set
	echo usage: createLinuxPackage.bat ^<TargetDir^> ^<BinarySourceDir^> ^<DocSourceDir^> ^<OpenCLSamplesSourceDir^>
	exit /b
)

IF "%~4%"=="" (
	echo OpenCL Samples Source Dir is not set
	echo usage: createLinuxPackage.bat ^<TargetDir^> ^<BinarySourceDir^> ^<DocSourceDir^> ^<OpenCLSamplesSourceDir^>
	exit /b
)

erase /S/Q %1\samples\_bin\x64\*
xcopy /Y ..\..\builder\*.cmake %1\builder\
erase %1\builder\open_source_symbols_undefine.list
erase %1\builder\open_source_symbols_define.list
erase %1\builder\open_source_symbols.list
erase %1\builder\open_source_files.txt
erase %1\builder\FindFunctions.cmake.ext 
erase %1\builder\FindGTest.cmake
erase %1\builder\FindInternals.cmake
erase %1\builder\FindIPP.cmake
erase %1\builder\FindIPPTS.cmake
erase %1\builder\FindMDF.cmake
erase %1\builder\ConfTargets.cmake

xcopy /S /Y ..\*.c %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S /Y ..\*.cl %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S /Y ..\*.cmake %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S /Y ..\*.cpp %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S /Y ..\*.cproject %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S /Y ..\*.def %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S /Y ..\*.h %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S /Y ..\*.hpp %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S /Y ..\*.map %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S /Y ..\*.pl %1\samples\ /EXCLUDE:exclusions.txt
rem xcopy /S /Y ..\*.project %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S /Y ..\*.txt %1\samples\ /EXCLUDE:exclusions.txt


FOR /f %%f IN ('dir "%1\" /s /b /a-D') DO (dos2unix "%%f")

xcopy /S /Y ..\*.pdf %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S /Y ..\*.so %1\samples\ /EXCLUDE:exclusions.txt

xcopy /S /Y %3\*.pdf %1\samples\
move "%1\samples\Media_Samples_Guide.pdf" "%1\Media_Samples_Guide.pdf"
xcopy /Y %3\..\..\README %1\Samples\*

xcopy /S /Y %4\ocl_motion_estimation\* %1\samples\ocl_motion_estimation\*
xcopy /S /Y %4\ocl_motion_estimation_advanced\* %1\samples\ocl_motion_estimation_advanced\*

xcopy /S /Y ..\content\* %1\samples\_bin\content\
rem !!! Possibly need to remove this later
erase %1\samples\_bin\content\test_stream_vp9.ivf


Set filename=%1
For %%A in ("%filename%") do (
    Set Folder=%%~dpA
    Set Name=%%~nxA
)
rem "c:\Program Files\7-Zip\7z.exe" a -ttar -so %Name%.tar %1 | "c:\Program Files\7-Zip\7z.exe" a -tgzip -si %1.tar.gz
erase %Name%.tar
..\..\..\mdp_msdk-tools\tar\tar -C %Folder%  --mode="a+rwX" -cvf %Name%.tar %Name%

rem Copying binaries and setting executable flag
xcopy /Y %2\sample_decode %1\samples\_bin\x64\*
xcopy /Y %2\sample_encode %1\samples\_bin\x64\*
xcopy /Y %2\sample_h265_gaa %1\samples\_bin\x64\*
xcopy /Y %2\sample_multi_transcode %1\samples\_bin\x64\*
xcopy /Y %2\sample_vpp %1\samples\_bin\x64\*
xcopy /Y %2\sample_fei %1\samples\_bin\x64\*
xcopy /Y %2\ocl_rotate.cl %1\samples\_bin\x64\*
xcopy /Y %2\libsample_plugin_opencl.so %1\samples\_bin\x64\*
xcopy /Y %2\libsample_rotate_plugin.so %1\samples\_bin\x64\*

..\..\..\mdp_msdk-tools\tar\tar -C %Folder% -cvf %Name%_a.tar --mode=776 %Name%/samples/_bin/x64
..\..\..\mdp_msdk-tools\tar\tar -Avf %Name%.tar --mode=776 %Name%_a.tar

"c:\Program Files\7-Zip\7z.exe" a -tgzip %1.tar.gz %Name%.tar
erase %Name%.tar
erase %Name%_a.tar

rem echo !!!!!!!! DO NOT FORGET TO COPY OPENCL SAMPLES !!!!!!!!
