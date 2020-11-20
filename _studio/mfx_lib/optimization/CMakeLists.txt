set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

# =============================================================================

# =============================================================================
set ( HEVCPP_LIBS "" )
# =============================================================================

if(NOT OPEN_SOURCE)
  if (MFX_BUNDLED_IPP)
    set(IPPLIBS ipp)
  else()
    set(IPPLIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
  endif()
endif()

set( ENABLE_HEVC FALSE )

if ( (CMAKE_C_COMPILER_ID MATCHES Intel) OR ENABLE_HEVC_ON_GCC )
  set(ENABLE_HEVC TRUE)
endif()

if (ENABLE_HEVC)

  add_library(hevc_pp_dispatcher STATIC)

  target_include_directories(hevc_pp_dispatcher
    PUBLIC
      h265/include
      ${MSDK_LIB_ROOT}/encode/h265/include
      ${MSDK_LIB_ROOT}/decode/h265/include
  )

  target_link_libraries(hevc_pp_dispatcher PUBLIC mfx_static_lib umc)

  target_sources(hevc_pp_dispatcher
    PRIVATE
      h265/src/mfx_h265_dispatcher.cpp
  )

  target_compile_definitions(hevc_pp_dispatcher
    PRIVATE
      AS_HEVCD_PLUGIN
      AS_HEVCE_PLUGIN
      "${API_FLAGS}"
  )

  # =============================================================================

  add_library(hevc_pp_atom STATIC)

  target_include_directories(hevc_pp_atom
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/h265/include
      ${MSDK_LIB_ROOT}/encode/h265/include
      ${MSDK_LIB_ROOT}/decode/h265/include
  )

  target_sources(hevc_pp_atom
    PRIVATE
      h265/src/mfx_h265_deblocking_sse.cpp
      h265/src/mfx_h265_interp_tmpl_sse.cpp
      h265/src/mfx_h265_interpolation_sse.cpp
      h265/src/mfx_h265_intragrad_sse.cpp
      h265/src/mfx_h265_intrapred_16u_sse.cpp
      h265/src/mfx_h265_intrapred_sse.cpp
      h265/src/mfx_h265_quantization_sse.cpp
      h265/src/mfx_h265_sad_general_sse.cpp
      h265/src/mfx_h265_sad_multiple_sse.cpp
      h265/src/mfx_h265_sad_sse.cpp
      h265/src/mfx_h265_sao_sse.cpp
      h265/src/mfx_h265_satd_sse.cpp
      h265/src/mfx_h265_transform_fwd_sse.cpp
      h265/src/mfx_h265_transform_inv_sse.cpp
      h265/src/mfx_h265_weighted_pred_sse.cpp
      h265/src/mfx_h265_interpolation_atom.cpp
  )

  target_link_libraries(hevc_pp_atom PUBLIC mfx_static_lib umc)

  target_compile_definitions(hevc_pp_atom
    PRIVATE
      MFX_MAKENAME_ATOM
      AS_HEVCD_PLUGIN
      AS_HEVCE_PLUGIN
      ${API_FLAGS}
  )

  list( APPEND HEVCPP_LIBS hevc_pp_atom )

  # =============================================================================

  add_library(hevc_pp_avx2 STATIC)

  target_include_directories(hevc_pp_avx2
    PUBLIC
      h265/include
      ${MSDK_LIB_ROOT}/encode/h265/include
      ${MSDK_LIB_ROOT}/decode/h265/include
  )

  target_sources( hevc_pp_avx2
    PRIVATE
      h265/src/mfx_h265_interp_tmpl_avx2.cpp
      h265/src/mfx_h265_interpolation_avx2.cpp
      h265/src/mfx_h265_intragrad_avx2.cpp
      h265/src/mfx_h265_intrapred_16u_avx2.cpp
      h265/src/mfx_h265_intrapred_avx2.cpp
      h265/src/mfx_h265_quantization_avx2.cpp
      h265/src/mfx_h265_reordering_avx2.cpp
      h265/src/mfx_h265_sad_avx2.cpp
      h265/src/mfx_h265_sad_general_avx2.cpp
      h265/src/mfx_h265_sad_multiple_avx2.cpp
      h265/src/mfx_h265_sao_avx2.cpp
      h265/src/mfx_h265_satd_avx2.cpp
      h265/src/mfx_h265_transform_fwd_avx2.cpp
      h265/src/mfx_h265_transform_inv_32x32_avx2.cpp
      h265/src/mfx_h265_transform_inv_avx2.cpp
      h265/src/mfx_h265_weighted_pred_avx2.cpp
      h265/src/mfx_h265_transform_fwd_32x32_avx2_nablet.cpp
  )

  target_link_libraries(hevc_pp_avx2
    PUBLIC
      umc
    PRIVATE
      mfx_require_avx2_properties
    )

  target_compile_definitions(hevc_pp_avx2
  PRIVATE
      MFX_MAKENAME_AVX2
      AS_HEVCD_PLUGIN
      AS_HEVCE_PLUGIN
      ${API_FLAGS}
  )

  list( APPEND HEVCPP_LIBS hevc_pp_avx2 )

  # =============================================================================

  add_library(hevc_pp_px STATIC)

  target_include_directories(hevc_pp_px
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/h265/include
      ${MSDK_LIB_ROOT}/encode/h265/include
      ${MSDK_LIB_ROOT}/decode/h265/include
  )

  target_sources(hevc_pp_px
    PRIVATE
      h265/src/mfx_h265_sad_ipp.cpp
      h265/src/mfx_h265_deblocking_px.cpp
      h265/src/mfx_h265_interpolation_px.cpp
      h265/src/mfx_h265_intragrad_px.cpp
      h265/src/mfx_h265_intrapred_16u_px.cpp
      h265/src/mfx_h265_intrapred_px.cpp
      h265/src/mfx_h265_quantization_px.cpp
      h265/src/mfx_h265_sad_multiple_px.cpp
      h265/src/mfx_h265_sao_16u_px.cpp
      h265/src/mfx_h265_sao_px.cpp
      h265/src/mfx_h265_satd_px.cpp
      h265/src/mfx_h265_transform_px.cpp
      h265/src/mfx_h265_weighted_pred_16u_px.cpp
      h265/src/mfx_h265_weighted_pred_px.cpp
  )

  target_link_libraries(hevc_pp_px PUBLIC mfx_static_lib umc)

  target_compile_definitions(hevc_pp_px
    PRIVATE
      MFX_MAKENAME_PX
      AS_HEVCD_PLUGIN
      AS_HEVCE_PLUGIN
      ${API_FLAGS}
  )

  list( APPEND HEVCPP_LIBS hevc_pp_px )

  # =============================================================================


  add_library(hevc_pp_sse4 STATIC)

  target_include_directories(hevc_pp_sse4
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/h265/include
      ${MSDK_LIB_ROOT}/encode/h265/include
      ${MSDK_LIB_ROOT}/decode/h265/include
  )

  target_sources(hevc_pp_sse4
    PRIVATE
      h265/src/mfx_h265_deblocking_sse.cpp
      h265/src/mfx_h265_interp_tmpl_sse.cpp
      h265/src/mfx_h265_interpolation_sse.cpp
      h265/src/mfx_h265_intragrad_sse.cpp
      h265/src/mfx_h265_intrapred_16u_sse.cpp
      h265/src/mfx_h265_intrapred_sse.cpp
      h265/src/mfx_h265_quantization_sse.cpp
      h265/src/mfx_h265_sad_general_sse.cpp
      h265/src/mfx_h265_sad_multiple_sse.cpp
      h265/src/mfx_h265_sad_sse.cpp
      h265/src/mfx_h265_sao_sse.cpp
      h265/src/mfx_h265_satd_sse.cpp
      h265/src/mfx_h265_transform_fwd_sse.cpp
      h265/src/mfx_h265_transform_inv_sse.cpp
      h265/src/mfx_h265_weighted_pred_sse.cpp
      h265/src/mfx_h265_transform_consts.cpp
  )

  target_link_libraries(hevc_pp_sse4
    PUBLIC
      umc
    PRIVATE
      mfx_require_sse4_properties)

  target_compile_definitions(hevc_pp_sse4
  PRIVATE
      MFX_MAKENAME_SSE4
      AS_HEVCD_PLUGIN
      AS_HEVCE_PLUGIN
      ${API_FLAGS}
  )

  list( APPEND HEVCPP_LIBS hevc_pp_sse4 )

  # =============================================================================

  add_library(hevc_pp_ssse3 STATIC)

  target_include_directories(hevc_pp_ssse3
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/h265/include
      ${MSDK_LIB_ROOT}/encode/h265/include
      ${MSDK_LIB_ROOT}/decode/h265/include
  )

  target_sources(hevc_pp_ssse3
    PRIVATE
      h265/src/mfx_h265_deblocking_sse.cpp
      h265/src/mfx_h265_interp_tmpl_sse.cpp
      h265/src/mfx_h265_interpolation_sse.cpp
      h265/src/mfx_h265_intragrad_sse.cpp
      h265/src/mfx_h265_intrapred_16u_sse.cpp
      h265/src/mfx_h265_intrapred_sse.cpp
      h265/src/mfx_h265_quantization_sse.cpp
      h265/src/mfx_h265_sad_general_sse.cpp
      h265/src/mfx_h265_sad_multiple_sse.cpp
      h265/src/mfx_h265_sad_sse.cpp
      h265/src/mfx_h265_sao_sse.cpp
      h265/src/mfx_h265_satd_sse.cpp
      h265/src/mfx_h265_transform_fwd_sse.cpp
      h265/src/mfx_h265_transform_inv_sse.cpp
      h265/src/mfx_h265_weighted_pred_sse.cpp
  )

  target_link_libraries(hevc_pp_ssse3
    PUBLIC 
      umc
    PRIVATE
      mfx_require_ssse3_properties
    )

  target_compile_definitions(hevc_pp_ssse3
  PRIVATE
      MFX_MAKENAME_SSSE3
      AS_HEVCD_PLUGIN
      AS_HEVCE_PLUGIN
      ${API_FLAGS}
  )

  list( APPEND HEVCPP_LIBS hevc_pp_ssse3 )

  set_property(TARGET hevc_pp_dispatcher ${HEVCPP_LIBS} PROPERTY FOLDER "optimization/hevc")

  # =============================================================================
  set ( HEVCPP_LIBS ${HEVCPP_LIBS} PARENT_SCOPE )
  # =============================================================================

endif() # ENABLE_HEVC



#AV1 part

set ( AV1CPP_LIBS "" )
# =============================================================================

set( ENABLE_AV1 FALSE )

if (CMAKE_C_COMPILER_ID MATCHES Intel)
  set(ENABLE_AV1 TRUE)
endif()

if (ENABLE_AV1)
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
      AS_AV1E_PLUGIN
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
      AS_AV1E_PLUGIN
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

  target_link_libraries(av1_pp_px PUBLIC ${IPPLIBS} mfx_static_lib)

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
      AS_AV1E_PLUGIN
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
      mfx_require_ssse3_properties)

  find_program(YASM_EXE NAMES yasm REQUIRED)
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

  set( defs "" )

  # =============================================================================
  set ( AV1CPP_LIBS ${AV1CPP_LIBS} PARENT_SCOPE )
  set ( ENABLE_AV1 ${ENABLE_AV1} PARENT_SCOPE )
  # =============================================================================

endif() # ENABLE_AV1

add_subdirectory(h264)
