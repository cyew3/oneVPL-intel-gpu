rem for /f "delims=" %aaa in ('reg query HKCU\Software\Intel\Mediasdk\Debug\Analyzer /v "log File"') do @set myvar=%aaa 

@echo off 
setlocal enableextensions 

for /f "tokens=1,2,3,*" %%a in ( 
'reg query HKCU\Software\Intel\Mediasdk\Debug\Analyzer /v "log File"' 
) do ( 
set myvar=%%~pd
) 
rem echo.myvar  %myvar%

explorer %myvar%

endlocal 
