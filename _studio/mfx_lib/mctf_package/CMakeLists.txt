if( MFX_ENABLE_KERNELS )
    set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

    mfx_include_dirs( )
    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/mctf/include )
    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/mctf/isa )
    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/asc/include )

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../cmrt_cross_platform/include )
    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/include )

    set( sources "" )
    set( sources.plus "" )

    file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/mctf/src/*.cpp")
    list( APPEND sources ${srcs} )

    set( LIBS "" )
    set( defs "${defs} ${WARNING_FLAGS}")
    make_library( mctf  hw static )
    set( defs "" )
endif()
