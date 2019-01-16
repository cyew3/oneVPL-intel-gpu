set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

# =============================================================================

mfx_include_dirs( )

# =============================================================================
set ( HEVCPP_LIBS "" )
# =============================================================================

set( ENABLE_HEVC FALSE )

if ( (CMAKE_C_COMPILER MATCHES icc) OR ENABLE_HEVC_ON_GCC )
  set(ENABLE_HEVC TRUE)
endif()

if (ENABLE_HEVC)

  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../decode/h265/include )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../encode/h265/include )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/h265/include )


  set( defs "-DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN ${API_FLAGS}" )
  set( sources "" )
  set( sources.plus "" )

  list( APPEND sources 
    h265/src/mfx_h265_dispatcher.cpp
  )

  make_library( hevc_pp_dispatcher none static )
  set( defs "" )

  # =============================================================================

  set( defs "-DMFX_MAKENAME_ATOM -DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN ${API_FLAGS}" )
  set( sources "" )
  set( sources.plus "" )

  foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/h265/src )
    file( GLOB_RECURSE srcs "${dir}/*sse.c" "${dir}/*sse.cpp" )
    list( APPEND sources ${srcs})
  endforeach()

  list( APPEND sources 
    h265/src/mfx_h265_interpolation_atom.cpp
  )

  make_library( hevc_pp_atom none static )
  set( defs "" )
  list( APPEND HEVCPP_LIBS hevc_pp_atom )

  # =============================================================================

  set( defs "-DMFX_MAKENAME_AVX2 -DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN ${API_FLAGS}" )
  set( sources "" )
  set( sources.plus "" )

  foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/h265/src )
    file( GLOB_RECURSE srcs "${dir}/*avx2.c" "${dir}/*avx2.cpp" )
    list( APPEND sources ${srcs} )
  endforeach()

  list( APPEND sources 
    h265/src/mfx_h265_transform_fwd_32x32_avx2_nablet.cpp
  )

  make_library( hevc_pp_avx2 none static )
  append_property( hevc_pp_avx2 COMPILE_FLAGS "-mavx2 -march=core-avx2")
  append_property( hevc_pp_avx2 LINK_FLAGS "-mavx2 -march=core-avx2")
  set( defs "" )
  list( APPEND HEVCPP_LIBS hevc_pp_avx2 )

  # =============================================================================

  set( defs "-DMFX_MAKENAME_PX -DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN ${API_FLAGS}" )
  set( sources "" )
  set( sources.plus "" )

  foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/h265/src )
    file( GLOB_RECURSE srcs "${dir}/*px.c" "${dir}/*px.cpp" )
    list( APPEND sources ${srcs})
  endforeach()

  list( APPEND sources 
      h265/src/mfx_h265_sad_ipp.cpp
  )

  make_library( hevc_pp_px none static )
  set( defs "" )
  list( APPEND HEVCPP_LIBS hevc_pp_px )

  # =============================================================================

  set( defs "-DMFX_MAKENAME_SSE4 -DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN ${API_FLAGS}" )
  set( sources "" )
  set( sources.plus "" )

  foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/h265/src )
    file( GLOB_RECURSE srcs "${dir}/*sse.c" "${dir}/*sse.cpp" )
    list( APPEND sources ${srcs})
  endforeach()

  list( APPEND sources 
    h265/src/mfx_h265_transform_consts.cpp
  )

  make_library( hevc_pp_sse4 none static )
  set( defs "" )
  list( APPEND HEVCPP_LIBS hevc_pp_sse4 )

  # =============================================================================

  set( defs "-DMFX_MAKENAME_SSSE3 -DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN ${API_FLAGS}" )
  set( sources "" )
  set( sources.plus "" )

  foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/h265/src )
    file( GLOB_RECURSE srcs "${dir}/*sse.c" "${dir}/*sse.cpp" )
    list( APPEND sources ${srcs} )
  endforeach()

  make_library( hevc_pp_ssse3 none static )
  append_property( hevc_pp_ssse3 COMPILE_FLAGS "-mssse3")
  append_property( hevc_pp_ssse3 LINK_FLAGS "-mssse3" )
  set( defs "" )
  list( APPEND HEVCPP_LIBS hevc_pp_ssse3 )

  # =============================================================================
  set ( HEVCPP_LIBS ${HEVCPP_LIBS} PARENT_SCOPE )
  # =============================================================================

endif() # ENABLE_HEVC



#AV1 part

find_program(YASM_EXE NAMES yasm)

set ( AV1CPP_LIBS "" )
# =============================================================================

set( ENABLE_AV1 FALSE )

if (CMAKE_C_COMPILER MATCHES icc AND YASM_EXE MATCHES yasm)
  set(ENABLE_AV1 TRUE)
endif()


if (ENABLE_AV1)

  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../encode/av1/include )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/av1 )

  # =============================================================================

  set( defs "-DMFX_MAKENAME_AVX2 -DAS_AV1E_PLUGIN ${API_FLAGS}" )
  set( sources "" )
  set( sources.plus "" )

  foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/av1/avx2 )
    file( GLOB_RECURSE srcs "${dir}/*.cpp" )
    list( APPEND sources ${srcs} )
  endforeach()

  make_library( av1_pp_avx2 none static )
  append_property( av1_pp_avx2 COMPILE_FLAGS "-mavx2 -march=core-avx2")
  append_property( av1_pp_avx2 LINK_FLAGS "-mavx2 -march=core-avx2")
  set( defs "" )
  list( APPEND AV1CPP_LIBS av1_pp_avx2 )

  # =============================================================================

  set( defs "-DMFX_MAKENAME_PX -DAS_AV1E_PLUGIN ${API_FLAGS}" )
  set( sources "" )
  set( sources.plus "" )

  foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/av1/px )
    file( GLOB_RECURSE srcs "${dir}/*.cpp" )
    list( APPEND sources ${srcs})
  endforeach()

  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/av1/px )

  make_library( av1_pp_px none static )
  set( defs "" )
  list( APPEND AV1CPP_LIBS av1_pp_px )

  # =============================================================================

  set( defs "-DMFX_MAKENAME_SSSE3 -DAS_AV1E_PLUGIN ${API_FLAGS}" )
  set( sources "" )
  set( sources.plus "" )

  foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/av1/ssse3 )
    file( GLOB_RECURSE srcs "${dir}/*.cpp" )
    list( APPEND sources ${srcs} )
  endforeach()

  set( asm "")
 foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/av1/ssse3 )
    file( GLOB_RECURSE srcs "${dir}/*.asm")
    list( APPEND asm ${srcs} )
  endforeach()

  list( APPEND sources intrapred_sse2.o intrapred_ssse3.o vpx_convolve_copy_sse2.o vpx_subpixel_8t_ssse3.o vpx_subpixel_bilinear_ssse3.o)
  find_program(YASM_EXE NAMES yasm REQUIRED)
  if(NOT YASM_EXE)
     message(SEND_ERROR "yasm was not found : av1_pp_sse3 will not be compiled")
  endif()

  message("YASM: ${YASM_EXE}")

  if(YASM_EXE)

     add_custom_command(OUTPUT intrapred_sse2.o COMMAND ${YASM_EXE}
                   ARGS -Xvc -f elf64  -I"." -I"${CMAKE_CURRENT_SOURCE_DIR}/av1" -o intrapred_sse2.o ${CMAKE_CURRENT_SOURCE_DIR}/av1/ssse3/intrapred_sse2.asm)
     add_custom_command(OUTPUT intrapred_ssse3.o COMMAND ${YASM_EXE}
                   ARGS -Xvc -f elf64  -I"." -I"${CMAKE_CURRENT_SOURCE_DIR}/av1" -o intrapred_ssse3.o ${CMAKE_CURRENT_SOURCE_DIR}/av1/ssse3/intrapred_ssse3.asm)
     add_custom_command(OUTPUT vpx_convolve_copy_sse2.o COMMAND ${YASM_EXE}
                   ARGS -Xvc -f elf64  -I"." -I"${CMAKE_CURRENT_SOURCE_DIR}/av1" -o vpx_convolve_copy_sse2.o ${CMAKE_CURRENT_SOURCE_DIR}/av1/ssse3/vpx_convolve_copy_sse2.asm)
     add_custom_command(OUTPUT vpx_subpixel_8t_ssse3.o COMMAND ${YASM_EXE}
                   ARGS -Xvc -f elf64  -I"." -I"${CMAKE_CURRENT_SOURCE_DIR}/av1" -o vpx_subpixel_8t_ssse3.o ${CMAKE_CURRENT_SOURCE_DIR}/av1/ssse3/vpx_subpixel_8t_ssse3.asm)
     add_custom_command(OUTPUT vpx_subpixel_bilinear_ssse3.o COMMAND ${YASM_EXE}
                     ARGS -Xvc -f elf64  -I"." -I"${CMAKE_CURRENT_SOURCE_DIR}/av1" -o vpx_subpixel_bilinear_ssse3.o ${CMAKE_CURRENT_SOURCE_DIR}/av1/ssse3/vpx_subpixel_bilinear_ssse3.asm)
     make_library( av1_pp_ssse3 none static )

     append_property( av1_pp_ssse3 COMPILE_FLAGS "-mssse3")
     append_property( av1_pp_ssse3 LINK_FLAGS "-mssse3" )
     list( APPEND AV1CPP_LIBS av1_pp_ssse3 )
  endif()

  set( defs "" )

  # =============================================================================
  set ( AV1CPP_LIBS ${AV1CPP_LIBS} PARENT_SCOPE )
  set ( ENABLE_AV1 ${ENABLE_AV1} PARENT_SCOPE )
  # =============================================================================

endif() # ENABLE_AV1






