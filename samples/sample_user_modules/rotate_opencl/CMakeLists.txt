include( ${CMAKE_HOME_DIRECTORY}/../builder/FindOpenCL.cmake )

set( PLUGIN_API ${CMAKE_SOURCE_DIR}/sample_user_modules/plugin_api )

include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_user_modules/rotate_opencl/include
  ${PLUGIN_API}
)

list( APPEND sources.plus "${PLUGIN_API}/mfx_plugin_module.cpp" )

set( LDFLAGS "${LDFLAGS} -Wl,--version-script=${PLUGIN_API}/mfx_plugin.map" )

# for clCreateFromGLTexture2D and clCreateFromGLTexture3D in CL/cl_gl.h
set( CFLAGS "-DCL_USE_DEPRECATED_OPENCL_1_1_APIS" )

list( APPEND LIBS_VARIANT sample_common )

set(DEPENDENCIES opencl libva)
make_library( sample_plugin_opencl none shared )

if ( DEFINED OPENCL_FOUND )
  # copy .cl file into build directory
  execute_process( COMMAND ${CMAKE_COMMAND} -E copy
  "${CMAKE_SOURCE_DIR}/sample_user_modules/rotate_opencl/src/ocl_rotate.cl"
  "${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}/ocl_rotate.cl" )
endif()
