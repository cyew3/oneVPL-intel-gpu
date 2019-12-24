@echo off

echo hme_and_me_p32_4mv          >> list_of_kernels.tmp
echo ime_3tiers_4mv              >> list_of_kernels.tmp
rem echo interpolate_decision        >> list_of_kernels.tmp
rem echo interpolate_decision_single >> list_of_kernels.tmp
echo intra                       >> list_of_kernels.tmp
echo me_p16_4mv                  >> list_of_kernels.tmp
echo me_p16_4mv_and_refine_32x32 >> list_of_kernels.tmp
echo me_p32_4mv                  >> list_of_kernels.tmp
echo mepu                        >> list_of_kernels.tmp
echo mode_decision               >> list_of_kernels.tmp
echo mode_decision_pass2         >> list_of_kernels.tmp
echo prepare_src                 >> list_of_kernels.tmp
echo refine_me_p_64x64           >> list_of_kernels.tmp
rem echo vartx_decision              >> list_of_kernels.tmp

for /F %%i in (list_of_kernels.tmp) do (
  rem call build_codec_kernel.cmd av1 %%i hsw /Qxcm_release
  call build_codec_kernel.cmd av1 %%i bdw /Qxcm_release
  call build_codec_kernel.cmd av1 %%i skl /Qxcm_release
  call build_codec_kernel.cmd av1 %%i cnl /Qxcm_release
  call build_codec_kernel.cmd av1 %%i icl /Qxcm_release
  call build_codec_kernel_cmc.cmd av1 %%i icllp /Qxcm_release
)

del list_of_kernels.tmp
