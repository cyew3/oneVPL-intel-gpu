@echo off
SETLOCAL enabledelayedexpansion

set DXVA=C:\lrb_media\lrbvme\test\MediaSDK\build\win_x64\bin\dxvaencode_emu.dll
rem set DXVA=dxva2.dll

set YUV_DIR=\YUV
set NUM_FRAMES=100
set PSNR=20
set PARAMS=-par cfg_h264_quality.par -n !NUM_FRAMES! -dll !DXVA! -created3d

for /f "tokens=2,3 delims=:,./_- " %%a in ('echo %%date%%') do set DATE=%%a%%b
for /f "tokens=1,2 delims=:,./_- " %%a in ('echo %%time%%') do set TIME=%%a%%b
set OUT_DIR=.\!DATE!_!TIME!
mkdir !OUT_DIR! 2> nul

set TESTLOOP=Library, Input, MemType, NumSlices, RefType, GOPstruct, Interlace
echo !TESTLOOP!

for %%h in (hw sw) do (
  for %%i in (salesman_176x144_449 foreman_352x288_300 foster_720x576) do (
    for %%m in (d3d sys) do (
      for %%s in (1 4) do (
        for %%r in (RefRaw RefRec) do (
          for %%g in (ip ipb) do (
            for %%f in (0 3) do (
              set PATTERN=%%h.%%i.%%m.s%%s.%%r.%%g.f%%f
              set OUT=!OUT_DIR!\out.!PATTERN!.264
              set LOG=!OUT_DIR!\log.!PATTERN!.log
              if NOT %%h.%%r==sw.RefRaw (
                echo. 1>&2
                echo %%h, %%i, %%m, NumSlice=%%s, %%r, %%g, PicStruct=%%f 1>&2
                del !OUT! !LOG! 2> nul
                echo TESTCASE = %%h, %%i, %%m, %%s, %%r, %%g, %%f >> !LOG!
                echo TESTLOOP = !TESTLOOP! >> !LOG!
                echo OUTPUT = out.!PATTERN!.264 >> !LOG!
                dir !DXVA! >> !LOG!
                mfx_transcoder_d.exe !PARAMS! -%%h -par %%i.par -par encode_h264_%%g.par -%%r -PicStruct %%f -NumSlice %%s -%%m -o !OUT! >> !LOG!
                mfx_player_d.exe -i !OUT! -cmp !YUV_DIR!\%%i.yuv -n !NUM_FRAMES! -psnr !PSNR! -v >> !LOG!
                if ERRORLEVEL 1 (echo TEST CASE FAILED) else (echo TEST CASE PASSED)
              )
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
