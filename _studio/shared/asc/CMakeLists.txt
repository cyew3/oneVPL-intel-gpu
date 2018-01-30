set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}")

mfx_include_dirs( )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../mfx_lib/cmrt_cross_platform/include )

set( defs "${WARNING_FLAGS}" )
set( sources "" )
set( sources.plus "" )

list( APPEND sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asc_avx2_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asc_c_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asc_sse4_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/genx_scd_bdw_isa.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/genx_scd_bxt_isa.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/genx_scd_cnl_isa.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/genx_scd_skl_isa.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/iofunctions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/motion_estimation_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tree.cpp
)

make_library(asc none static )
set( defs "" )
