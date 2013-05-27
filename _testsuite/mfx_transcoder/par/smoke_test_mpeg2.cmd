@echo off
SETLOCAL enabledelayedexpansion

set DXVA=C:\lrb_media\lrbvme\test\MediaSDK\build\win_x64\bin\dxvaencode_emu.dll
rem set DXVA=dxva2.dll

set YUV_DIR=\YUV
set NUM_FRAMES=30
set PSNR=20
set PARAMS=-n !NUM_FRAMES! -dll !DXVA! -created3d

for /f "tokens=2,3 delims=:,./_- " %%a in ('echo %%date%%') do set DATE=%%a%%b
for /f "tokens=1,2 delims=:,./_- " %%a in ('echo %%time%%') do set TIME=%%a%%b
set OUT_DIR=.\!DATE!_!TIME!
mkdir !OUT_DIR! 2> nul

set TESTLOOP=Library, Input, MemType, NumSlices, RefType, GOPstruct, Interlace
echo !TESTLOOP!

rem foster_720x576

for %%h in (sw hw) do (
  for %%i in (salesman_176x144_449 foreman_352x288_300) do (
    for %%m in (d3d) do (
      for %%r in (RefRec) do (
        for %%g in (ip) do (
          for %%f in (0) do (
            set PATTERN=mpeg2.%%h.%%i.%%m.%%r.%%g.f%%f
            set OUT=!OUT_DIR!\out.!PATTERN!.m2v
            set LOG=!OUT_DIR!\log.!PATTERN!.log
            if NOT %%h.%%r==sw.RefRaw (
              echo. 1>&2
              echo %%h, %%i, %%m, %%r, %%g, PicStruct=%%f 1>&2
              del !OUT! !LOG! 2> nul
              echo TESTCASE = %%h, %%i, %%m, %%r, %%g, %%f >> !LOG!
              echo TESTLOOP = !TESTLOOP! >> !LOG!
              echo OUTPUT = out.!PATTERN!.264 >> !LOG!
              dir !DXVA! >> !LOG!
              mfx_transcoder_d.exe !PARAMS! -%%h -par %%i.par -par encode_mpeg2_%%g.par -%%r -PicStruct %%f -%%m -o !OUT! >> !LOG!
              mfx_player_d.exe -i !OUT! -cmp !YUV_DIR!\%%i.yuv -n !NUM_FRAMES! -psnr !PSNR! -v >> !LOG!
              if ERRORLEVEL 1 (echo TEST CASE FAILED) else (echo TEST CASE PASSED)
            )
          )
        )
      )
    )
  )
)

echo.
date /t
time /t
echo Smoke test completed!
