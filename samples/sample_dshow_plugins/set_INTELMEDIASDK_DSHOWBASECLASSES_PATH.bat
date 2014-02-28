@echo off
if not defined INTELMEDIASDK_DSHOWBASECLASSES_PATH (
if exist "%SystemDrive%\Program Files\Microsoft SDKs\Windows\v7.1\Samples\Multimedia\DirectShow\BaseClasses" (
setx INTELMEDIASDK_DSHOWBASECLASSES_PATH "%SystemDrive%\Program Files\Microsoft SDKs\Windows\v7.1\Samples\Multimedia\DirectShow\BaseClasses" -m
goto :EOF
)
if exist "%SystemDrive%\Program Files\Microsoft SDKs\Windows\v7.0\Samples\Multimedia\DirectShow\BaseClasses" (
setx INTELMEDIASDK_DSHOWBASECLASSES_PATH "%SystemDrive%\Program Files\Microsoft SDKs\Windows\v7.0\Samples\Multimedia\DirectShow\BaseClasses" -m
goto :EOF
)
if exist "%SystemDrive%\Program Files\Microsoft SDKs\Windows\v6.1\Samples\Multimedia\DirectShow\BaseClasses" (
setx INTELMEDIASDK_DSHOWBASECLASSES_PATH "%SystemDrive%\Program Files\Microsoft SDKs\Windows\v6.1\Samples\Multimedia\DirectShow\BaseClasses" -m
goto :EOF
)
if exist "%SystemDrive%\Program Files\Microsoft SDKs\Windows\v6.0\Samples\Multimedia\DirectShow\BaseClasses" (
setx INTELMEDIASDK_DSHOWBASECLASSES_PATH "%SystemDrive%\Program Files\Microsoft SDKs\Windows\v6.0\Samples\Multimedia\DirectShow\BaseClasses" -m
))