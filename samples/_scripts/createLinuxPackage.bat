@echo off
IF "%~1%"=="" (
	echo Target Dir is not set
	echo usage: createLinuxPackage.bat ^<TargetDir^> ^<BinarySourceDir^> ^<DocSourceDir^>
	exit /b
)

IF "%~2%"=="" (
	echo Binary Source Dir is not set
	echo usage: createLinuxPackage.bat ^<TargetDir^> ^<BinarySourceDir^> ^<DocSourceDir^>
	exit /b
)

IF "%~3%"=="" (
	echo Docs Source Dir is not set
	echo usage: createLinuxPackage.bat ^<TargetDir^> ^<BinarySourceDir^> ^<DocSourceDir^>
	exit /b
)

xcopy /S builder\*.* %1\builder\

xcopy /S ..\*.c %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S ..\*.cl %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S ..\*.cmake %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S ..\*.cpp %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S ..\*.cproject %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S ..\*.def %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S ..\*.h %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S ..\*.hpp %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S ..\*.map %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S ..\*.pl %1\samples\ /EXCLUDE:exclusions.txt
rem xcopy /S ..\*.project %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S ..\*.txt %1\samples\ /EXCLUDE:exclusions.txt


FOR /f %%f IN ('dir "%1\" /s /b /a-D') DO (dos2unix "%%f")

xcopy /S ..\*.pdf %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S ..\*.so %1\samples\ /EXCLUDE:exclusions.txt

xcopy %2\sample_decode %1\samples\_bin\x64\*
xcopy %2\sample_encode %1\samples\_bin\x64\*
xcopy %2\sample_h265_gaa %1\samples\_bin\x64\*
xcopy %2\sample_multi_transcode %1\samples\_bin\x64\*
xcopy %2\sample_vpp %1\samples\_bin\x64\*
xcopy %2\ocl_rotate.cl %1\samples\_bin\x64\*
xcopy %2\libsample_plugin_opencl.so %1\samples\_bin\x64\*
xcopy %2\libsample_rotate_plugin.so %1\samples\_bin\x64\*

xcopy /S %3\*.pdf %1\samples\
move "%1\samples\Media_Samples_Guide.pdf" "%1\Media_Samples_Guide.pdf"
#xcopy %3\..\..\redist.txt %1\
#copy /Y "%3\..\..\..\..\release\EULAs\Master EULA for Intel Sw Development Products September 2015.pdf" "%1\Samples EULA.pdf"
#xcopy %3\..\..\third_party_programs.txt %1\
#xcopy %3\..\..\site_license_materials.txt %1\
xcopy %3\..\..\README %1\Samples\*


xcopy /S content\* %1\samples\_bin\content\


echo !!!!!!!! DO NOT FORGET TO COPY OPENCL SAMPLES !!!!!!!!