set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}")
mfx_include_dirs( )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../mfx_lib/cmrt_cross_platform/include )

set( defs "${WARNING_FLAGS}" )
set( sources "" )
set( sources.plus "" )

list( APPEND sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asc_AVX2_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asc_C_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asc_SSE4_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/genx_scd_bdw_isa.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/genx_scd_bxt_isa.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/genx_scd_cnl_isa.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/genx_scd_skl_isa.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/IOfunctions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MotionEstimationEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tree.cpp
)

make_library(asc none static )
set( defs "" )
