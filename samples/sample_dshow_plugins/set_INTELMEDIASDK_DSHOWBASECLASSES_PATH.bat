@echo off
::
:: INTEL CORPORATION PROPRIETARY INFORMATION
:: This software is supplied under the terms of a license agreement or
:: nondisclosure agreement with Intel Corporation and may not be copied
:: or disclosed except in accordance with the terms of that agreement.
:: Copyright (c) 2005-2014 Intel Corporation. All Rights Reserved.
::

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