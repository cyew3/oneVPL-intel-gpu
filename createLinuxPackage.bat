@echo off
IF "%~1%"=="" (
	echo Target Dir is not set
	exit /b
)

xcopy /S samples\*.c %1\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.cl %1\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.cmake %1\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.cpp %1\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.cproject %1\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.def %1\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.h %1\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.hpp %1\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.map %1\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.pdf %1\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.pl %1\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.project %1\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.so %1\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.txt %1\ /EXCLUDE:exclusions.txt

