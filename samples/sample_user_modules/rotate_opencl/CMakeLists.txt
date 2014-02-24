include( ${CMAKE_HOME_DIRECTORY}/../builder/FindOpenCL.cmake )

set( PLUGIN_API    ${CMAKE_SOURCE_DIR}/sample_user_modules/plugin_api )

if ( DEFINED OPENCL_FOUND )
    include_directories (
      ${CMAKE_SOURCE_DIR}/sample_common/include
      ${CMAKE_SOURCE_DIR}/sample_user_modules/rotate_opencl/include
      ${OPENCL_INCLUDE}
      ${PLUGIN_API}
    )
    link_directories( ${OPENCL_LIBRARY_PATH} )

    set( MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${PLUGIN_API}/mfx_plugin.map" )
    list( APPEND sources.plus "${PLUGIN_API}/mfx_plugin_module.cpp" )

    # for clCreateFromGLTexture2D and clCreateFromGLTexture3D in CL/cl_gl.h
    append("-DCL_USE_DEPRECATED_OPENCL_1_1_APIS" CMAKE_CXX_FLAGS)

    list( APPEND LIBS "/usr/lib/x86_64-linux-gnu/libva.so" )
    list( APPEND LIBS sample_common ${OPENCL_LIBS} )

    make_library( sample_plugin_opencl none shared )
endif ( )
