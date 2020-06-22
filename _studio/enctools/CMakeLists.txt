set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

mfx_include_dirs( )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/aenc/include )

set(sources
    ${CMAKE_CURRENT_SOURCE_DIR}/enctools.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_enctools_brc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_enctools_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_enctools_aenc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_enctools_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_enctools_lpla.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aenc/src/aenc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aenc/src/av1_asc_agop_tree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aenc/src/av1_asc_tree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aenc/src/av1_asc.cpp
    )
set( sources.plus "" )

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )


make_library(enctools hw static )
set( defs "" )
