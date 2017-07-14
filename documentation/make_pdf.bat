@echo off

for /f "tokens=*" %%i in ('findstr /C:"API Version" header-template.md') do (
    set _APIVersion=%%i
)

echo %_APIVersion%

rmdir /s /q PDF
mkdir PDF

set _PDF_BODY_OPT=--zoom 1.5
set _Name=mediasdk-man
set _Title=SDK Developer Reference
call :CONVERT

set _Name=mediasdkusr-man
set _Title=SDK Developer Reference Extensions for User-Defined Functions
call :CONVERT

set _PDF_BODY_OPT=--zoom 1.3
set _Name=mediasdkhevcfei-man
set _Title=Reference Manual for HEVC FEI
call :CONVERT

set _PDF_BODY_OPT=

set _Name=mediasdkjpeg-man
set _Title=SDK Developer Reference for JPEG/Motion JPEG
call :CONVERT

set _Name=mediasdkmvc-man
set _Title=SDK Developer Reference for Multi-View Video Coding
call :CONVERT

set _Name=mediasdkvp8-man
set _Title=SDK Developer Reference for VP8
call :CONVERT

set _PDF_BODY_OPT=--zoom 1.5
set _Name=mediasdkfei-man
set _Title=SDK Developer Reference for FEI
call :CONVERT

goto :CLEANUP

:CONVERT
echo Converting "%_Title%"(%_Name%)...
powershell -Command "(gc header-template.md) -replace '#\*\*Title\*\*', '#**%_Title%**' | Out-File -encoding UTF8 header-%_Name%.md"
pandoc --from=markdown_github -t html5 -i header-%_Name%.md -o header.html --css=styles/intel_css_styles.css
pandoc --from=markdown_github -t html5 --columns 9999 -i %_Name%.md -o body.html --css=styles/intel_css_styles_4pdf.css
wkhtmltopdf -B 14mm -T 14mm --title "%_Title%" header.html --zoom 1.5 toc --xsl-style-sheet styles/toc.xsl body.html %_PDF_BODY_OPT% --footer-line --footer-center "%_Title%" --footer-right "%_APIVersion%" --footer-left [page] --footer-font-size 8 --footer-font-name Verdana --footer-spacing 4 PDF/%_Name%.pdf

del header.html
del body.html
del header-%_Name%.md
goto :S_END

:CLEANUP

echo Converting Done

set _Name=
set _Title=
set _APIVersion=

:S_END
