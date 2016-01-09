@echo off

echo analyze_gradient             >  list_of_kernels.tmp
echo analyze_gradient2            >> list_of_kernels.tmp
echo analyze_gradient3            >> list_of_kernels.tmp
echo analyze_gradient8x8          >> list_of_kernels.tmp
echo analyze_gradient_32x32_best  >> list_of_kernels.tmp
echo analyze_gradient_32x32_modes >> list_of_kernels.tmp
echo deblock                      >> list_of_kernels.tmp
echo down_sample_mb               >> list_of_kernels.tmp
echo down_sample_mb_2tiers        >> list_of_kernels.tmp
echo down_sample_mb_4tiers        >> list_of_kernels.tmp
echo hme_and_me_p32_4mv           >> list_of_kernels.tmp
echo ime_3tiers_4mv               >> list_of_kernels.tmp
echo ime_4mv                      >> list_of_kernels.tmp
echo interpolate_frame            >> list_of_kernels.tmp
echo intra_avc                    >> list_of_kernels.tmp
echo me_p16_4mv                   >> list_of_kernels.tmp
echo me_p16_4mv_and_refine_32x32  >> list_of_kernels.tmp
echo me_p32_4mv                   >> list_of_kernels.tmp
echo prepare_src                  >> list_of_kernels.tmp
echo refine_me_p_16x32            >> list_of_kernels.tmp
echo refine_me_p_16x32_sad        >> list_of_kernels.tmp
echo refine_me_p_32x16            >> list_of_kernels.tmp
echo refine_me_p_32x16_sad        >> list_of_kernels.tmp
echo refine_me_p_32x32            >> list_of_kernels.tmp
echo refine_me_p_32x32_sad        >> list_of_kernels.tmp
echo refine_me_p_32x32_satd4x4    >> list_of_kernels.tmp
echo refine_me_p_32x32_satd8x8    >> list_of_kernels.tmp
echo refine_me_p_64x64            >> list_of_kernels.tmp
echo refine_me_p_combine          >> list_of_kernels.tmp
echo refine_me_p_satd4x4_combine  >> list_of_kernels.tmp
echo refine_me_p_satd8x8_combine  >> list_of_kernels.tmp
echo sao                          >> list_of_kernels.tmp
echo sao_apply                    >> list_of_kernels.tmp

for /F %%i in (list_of_kernels.tmp) do (
  call build_kernel.cmd %%i hsw /Qxcm_release
  call build_kernel.cmd %%i bdw /Qxcm_release
  call build_kernel.cmd %%i skl /Qxcm_release
)

del list_of_kernels.tmp
