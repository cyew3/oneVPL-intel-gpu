@echo off
setlocal enabledelayedexpansion

set nx=4
set ny=4


set /a nx_minus1=%nx%-1
set /a ny_minus1=%ny%-1


for /l %%y in (0,1,%ny_minus1%) do (
for /l %%x in (0,1,%nx_minus1%) do (

set /a n=%%x+%%y*%nx%


start /B ..\..\..\build\win_win32\bin\mfx_player -i %1 -hw -no_window_title -wall_n !n! -wall_h %ny% -wall_w %nx% -numsourceseek 100 0 100 >>nul
rem delay
ping -n 1 127.0.0.2
rem>> log\log!n!.txt
)
)
