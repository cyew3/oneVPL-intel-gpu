@echo off

set YUV=foreman_352x288_300.yuv
set W=352
set H=288
set NUM_FRAMES=60
set PSNR=20
set DXVA=dxva2.dll

echo Loop through 3 test cases
echo 1) LRB-accelerated encoding, vme on raw(input) frames
echo 2) LRB-accelerated encoding, vme on reconstructed frames
echo 3) MediaSDK software encoding

for %%o in (-RefRaw -RefRec -sw) do (
  echo ############################################ %%o
  del out.264
  mfx_transcoder_d.exe -hw %%o -i %YUV% -w %W% -h %H% -par encode_h264_ip.par -par cfg_h264_quality.par -n %NUM_FRAMES% -o out.264 -dll %DXVA%
  mfx_player_d.exe -i out.264 -cmp %YUV% -n %NUM_FRAMES% -psnr %PSNR% -v
  if ERRORLEVEL 1 (echo SANITY TEST FAILED) else (echo SANITY TEST PASSED)
)
