@echo off

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

