@echo off
::
:: INTEL CORPORATION PROPRIETARY INFORMATION
:: This software is supplied under the terms of a license agreement or
:: nondisclosure agreement with Intel Corporation and may not be copied
:: or disclosed except in accordance with the terms of that agreement.
:: Copyright (c) 2005-2014 Intel Corporation. All Rights Reserved.
::

if not defined INTELMEDIASDK_WINSDK_PATH (
    if exist "%SystemDrive%\Program Files\Windows Kits\8.1" (
        setx INTELMEDIASDK_WINSDK_PATH "%SystemDrive%\Program Files\Windows Kits\8.1" -m
        goto :EOF
    )
    if exist "%SystemDrive%\Program Files (x86)\Windows Kits\8.1" (
        setx INTELMEDIASDK_WINSDK_PATH "%SystemDrive%\Program Files (x86)\Windows Kits\8.1" -m
        goto :EOF
    )
    if exist "%SystemDrive%\Program Files\Windows Kits\8.0" (
        setx INTELMEDIASDK_WINSDK_PATH "%SystemDrive%\Program Files\Windows Kits\8.0" -m
        goto :EOF
    )
    if exist "%SystemDrive%\Program Files (x86)\Windows Kits\8.0" (
        setx INTELMEDIASDK_WINSDK_PATH "%SystemDrive%\Program Files (x86)\Windows Kits\8.0" -m
        goto :EOF
    )
    if exist "%SystemDrive%\Program Files\Microsoft SDKs\Windows\v7.1" (
        setx INTELMEDIASDK_WINSDK_PATH "%SystemDrive%\Program Files\Microsoft SDKs\Windows\v7.1" -m
        goto :EOF
    )
    if exist "%SystemDrive%\Program Files\Microsoft SDKs\Windows\v7.0" (
        setx INTELMEDIASDK_WINSDK_PATH "%SystemDrive%\Program Files\Microsoft SDKs\Windows\v7.0" -m
        goto :EOF
    )
)

