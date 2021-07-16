if (OPEN_SOURCE)
  return()
endif()

if (MFX_BUNDLED_IPP)
  set(IPPLIBS ipp)
else()
  set(IPPLIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
endif()

set ( AV1CPP_LIBS "" )

  # =============================================================================
  add_library(av1_pp_avx2 STATIC)

  target_include_directories(av1_pp_avx2
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/av1
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/avx2
      ${MSDK_LIB_ROOT}/encode/av1/include
  )

  target_compile_definitions(av1_pp_avx2
    PUBLIC
      MFX_MAKENAME_AVX2
      ${API_FLAGS}
  )

  target_sources(av1_pp_avx2
    PRIVATE
      av1/avx2/mfx_av1_satd_avx2.cpp
      av1/avx2/mfx_av1_sse_avx2.cpp
      av1/avx2/mfx_av1_diff_avx2.cpp
      av1/avx2/mfx_av1_sad_avx2.cpp
      av1/avx2/mfx_av1_rscs_avx2.cpp
      av1/avx2/mfx_av1_intrapred_avx2.cpp
      av1/avx2/mfx_av1_deblocking_avx2.cpp
      av1/avx2/mfx_av1_interpolation_avx2.cpp
      av1/avx2/mfx_av1_adds_nv12_avx2.cpp
      av1/avx2/mfx_av1_cdef_avx2.cpp
      av1/avx2/mfx_av1_fwd_transform_avx2.cpp
      av1/avx2/mfx_av1_inv_transform_avx2.cpp
      av1/avx2/mfx_av1_fwd_transform9_avx2.cpp
      av1/avx2/mfx_av1_inv_transform9_avx2.cpp
      av1/avx2/mfx_av1_cfl_avx2.cpp
      av1/avx2/mfx_av1_hbd_intrapred_avx2.cpp
      av1/avx2/mfx_av1_quantization_avx2.cpp
      av1/mfx_av1_opts_common.h
      av1/avx2/mfx_av1_transform_common_avx2.h
      av1/mfx_av1_opts_intrin.h
      av1/mfx_av1_intrapred_common.h
  )

  target_link_libraries(av1_pp_avx2
    PRIVATE 
      ${IPPLIBS}
      mfx_static_lib
      mfx_require_avx2_properties
      mfx_sdl_properties
    )

  list( APPEND AV1CPP_LIBS av1_pp_avx2 )

  # =============================================================================

  add_library(av1_pp_px STATIC)

  target_include_directories(av1_pp_px
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/av1
      ${MSDK_LIB_ROOT}/encode/av1/include
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/px
  )

  target_compile_definitions(av1_pp_px
    PUBLIC
      MFX_MAKENAME_PX
      ${API_FLAGS}
  )

  target_sources(av1_pp_px
    PRIVATE
      av1/px/mfx_av1_cdef_px.cpp
      av1/px/mfx_av1_cfl_px.cpp
      av1/px/mfx_av1_fwd_transform9_px.cpp
      av1/px/mfx_av1_fwd_transform_px.cpp
      av1/px/mfx_av1_get_intra_pred_pels.cpp
      av1/px/mfx_av1_interpolation_px.cpp
      av1/px/mfx_av1_adds_nv12_px.cpp
      av1/px/mfx_av1_deblocking_px.cpp
      av1/px/mfx_av1_diff_px.cpp
      av1/px/mfx_av1_intrapred_px.cpp
      av1/px/mfx_av1_inv_transform9_px.cpp
      av1/px/mfx_av1_inv_transform_px.cpp
      av1/px/mfx_av1_quantization_px.cpp
      av1/px/mfx_av1_rscs_px.cpp
      av1/px/mfx_av1_sad_px.cpp
      av1/px/mfx_av1_satd_px.cpp
      av1/px/mfx_av1_sse_px.cpp
  )

  target_link_libraries(av1_pp_px
    PUBLIC
      ${IPPLIBS}
      mfx_static_lib
    PRIVATE
      mfx_sdl_properties)

  list( APPEND AV1CPP_LIBS av1_pp_px )

  # =============================================================================

  add_library(av1_pp_ssse3 STATIC)

  target_include_directories(av1_pp_ssse3
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/av1
      ${MSDK_LIB_ROOT}/encode/av1/include
  )

  target_compile_definitions(av1_pp_ssse3
    PUBLIC
      MFX_MAKENAME_SSSE3
      ${API_FLAGS}
  )

  target_sources(av1_pp_ssse3
    PRIVATE
      av1/vpx_config.asm
      av1/x86inc.asm
      av1/x86_abi_support.asm
      av1/ssse3/fwd_txfm_sse2.h
      av1/ssse3/inv_txfm_sse2.h
      av1/ssse3/mfx_av1_cfl_ssse3.cpp
      av1/ssse3/mfx_av1_deblocking_sse2.cpp
      av1/ssse3/mfx_av1_interpolation_sse.cpp
      av1/ssse3/mfx_av1_intrapred_ssse3.cpp
      av1/ssse3/mfx_av1_transform_fwd_sse.cpp
      av1/ssse3/mfx_av1_transform_inv_sse.cpp
  )

  target_link_libraries(av1_pp_ssse3
    PUBLIC
      ${IPPLIBS}
      mfx_static_lib
    PRIVATE
      mfx_sdl_properties
      mfx_require_ssse3_properties)

  find_program(YASM_EXE NAMES yasm HINTS "${MSDK_BUILDER_ROOT}/../../../build_tools/yasm-win64" REQUIRED)
  if(NOT YASM_EXE)
     message(SEND_ERROR "yasm was not found : av1_pp_ssse3 could not be compiled")
  endif()

  message("YASM: ${YASM_EXE}")

  if(CMAKE_SYSTEM_NAME MATCHES Windows)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(YASM_ARGS -Xvc -g cv8 -f win64)
    else()
      set(YASM_ARGS -Xvc -g cv8 -f win32)
    endif()
  else()
    set(YASM_ARGS -Xvc -f elf64)
  endif()

  if(YASM_EXE)
    add_custom_command(OUTPUT intrapred_sse2.o COMMAND ${YASM_EXE}
                  ARGS ${YASM_ARGS} -I"." -I"${CMAKE_CURRENT_SOURCE_DIR}/av1" -o intrapred_sse2.o ${CMAKE_CURRENT_SOURCE_DIR}/av1/ssse3/intrapred_sse2.asm)
    add_custom_command(OUTPUT intrapred_ssse3.o COMMAND ${YASM_EXE}
                  ARGS ${YASM_ARGS} -I"." -I"${CMAKE_CURRENT_SOURCE_DIR}/av1" -o intrapred_ssse3.o ${CMAKE_CURRENT_SOURCE_DIR}/av1/ssse3/intrapred_ssse3.asm)
    add_custom_command(OUTPUT vpx_convolve_copy_sse2.o COMMAND ${YASM_EXE}
                  ARGS ${YASM_ARGS} -I"." -I"${CMAKE_CURRENT_SOURCE_DIR}/av1" -o vpx_convolve_copy_sse2.o ${CMAKE_CURRENT_SOURCE_DIR}/av1/ssse3/vpx_convolve_copy_sse2.asm)
    add_custom_command(OUTPUT vpx_subpixel_8t_ssse3.o COMMAND ${YASM_EXE}
                  ARGS ${YASM_ARGS} -I"." -I"${CMAKE_CURRENT_SOURCE_DIR}/av1" -o vpx_subpixel_8t_ssse3.o ${CMAKE_CURRENT_SOURCE_DIR}/av1/ssse3/vpx_subpixel_8t_ssse3.asm)
    add_custom_command(OUTPUT vpx_subpixel_bilinear_ssse3.o COMMAND ${YASM_EXE}
                    ARGS ${YASM_ARGS} -I"." -I"${CMAKE_CURRENT_SOURCE_DIR}/av1" -o vpx_subpixel_bilinear_ssse3.o ${CMAKE_CURRENT_SOURCE_DIR}/av1/ssse3/vpx_subpixel_bilinear_ssse3.asm)

    set(obj_sources
      intrapred_sse2.o
      intrapred_ssse3.o
      vpx_convolve_copy_sse2.o
      vpx_subpixel_8t_ssse3.o
      vpx_subpixel_bilinear_ssse3.o
    )

    SET_SOURCE_FILES_PROPERTIES(${obj_sources}
      PROPERTIES
        EXTERNAL_OBJECT true
        GENERATED true
    )

    target_sources(av1_pp_ssse3 PRIVATE "${obj_sources}")

    list( APPEND AV1CPP_LIBS av1_pp_ssse3 )
  endif()

  set_property(TARGET av1_pp_avx2 av1_pp_px av1_pp_ssse3 PROPERTY FOLDER "optimization/av1")
