echo off
dir /b .\baseproj > baseproj.txt
dir /b .\evalproj > evalproj.txt
fc /l baseproj.txt evalproj.txt
goto result%ERRORLEVEL%
:result0
  echo No differences
goto :EOF
:result1
  "C:\Program Files\KDiff3\kdiff3.exe" baseproj.txt evalproj.txt
goto :EOF
