@ECHO OFF
SETLOCAL
setLocal EnableDelayedExpansion

SET _myVar=0
FOR /F %%G in (.svn\entries.) DO (
IF !_myVar! LSS 3 SET /A _myVar+=1 & SET _svn_dir_rev=%%G
)

set _year=%date:~12,2%
set _month=%date:~4,2%
set _day=%date:~7,2%

ECHO 1 VERSIONINFO > libmfxsw.rc
ECHO. FILEVERSION 5,%_year%,%_month%,%_day%   >> libmfxsw.rc
ECHO. PRODUCTVERSION 1   >> libmfxsw.rc
ECHO. FILEOS VOS__WINDOWS32   >> libmfxsw.rc
ECHO. FILETYPE VFT_APP   >> libmfxsw.rc
ECHO. BEGIN   >> libmfxsw.rc
ECHO.   BLOCK "StringFileInfo"   >> libmfxsw.rc
ECHO.   BEGIN   >> libmfxsw.rc
ECHO.     BLOCK "080904b0"   >> libmfxsw.rc
ECHO.     BEGIN   >> libmfxsw.rc
ECHO.       VALUE "CompanyName","Intel Corporation"   >> libmfxsw.rc
ECHO.       VALUE "FileDescription","Intel® Media SDK library"   >> libmfxsw.rc
ECHO.       VALUE "FileVersion","5.%_year%.%_month%.%_day%"   >> libmfxsw.rc
ECHO.       VALUE "InternalName","libmfxhw"   >> libmfxsw.rc
ECHO.       VALUE "LegalCopyright","Copyright© 2003-2011 Intel Corporation"   >> libmfxsw.rc
ECHO.       VALUE "LegalTrademarks","Intel Corporation"   >> libmfxsw.rc
ECHO.       VALUE "ProductName","Intel® Media SDK"   >> libmfxsw.rc
ECHO.       VALUE "ProductVersion","5.0.00002%1.%_svn_dir_rev%"   >> libmfxsw.rc
ECHO.     END   >> libmfxsw.rc
ECHO.   END   >> libmfxsw.rc
ECHO.   BLOCK "VarFileInfo"   >> libmfxsw.rc
ECHO.   BEGIN   >> libmfxsw.rc
ECHO.     VALUE "Translation", 0x409 1200   >> libmfxsw.rc
ECHO.   END   >> libmfxsw.rc
ECHO. END   >> libmfxsw.rc
ENDLOCAL