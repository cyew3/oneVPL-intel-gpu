include( ${MSDK_BUILDER_ROOT}/FindOpenCL.cmake )


if (NOT ENABLE_OPENCL)
  return()
endif()

add_library(sample_plugin_opencl SHARED)
set_property(TARGET sample_plugin_opencl PROPERTY FOLDER "samples")

target_include_directories(sample_plugin_opencl
  PUBLIC
    include
  )

target_compile_definitions(sample_plugin_opencl
  PRIVATE
    ${WARNING_FLAGS}
    CL_USE_DEPRECATED_OPENCL_1_1_APIS
  )

set( PLUGINS_COMMON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../plugins_common_files )

target_link_options(sample_plugin_opencl
  PRIVATE
    $<$<PLATFORM_ID:Linux>:-Wl,--version-script=${PLUGINS_COMMON_PATH}/mfx_plugin.map>
)

target_link_libraries(sample_plugin_opencl
  PUBLIC
    ${OPENCL_LIBRARY}
    sample_common
  )

target_sources(sample_plugin_opencl
  PRIVATE
    include/d3d_utils.h
    include/logger.h
    include/opencl_filter_dx11.h
    include/opencl_filter_dx9.h
    include/opencl_filter.h
    include/opencl_filter_va.h
    include/rotate_plugin_api.h
    include/sample_opencl_plugin.h

    src/opencl_filter.cpp
    src/opencl_filter_dx11.cpp
    src/opencl_filter_dx9.cpp
    src/opencl_filter_va.cpp
    src/sample_opencl_plugin.cpp

    src/ocl_rotate.cl

    ${PLUGINS_COMMON_PATH}/mfx_plugin_module.cpp

    $<$<PLATFORM_ID:Windows>:
      res/resource.h
      res/sample_plugin_opencl.rc
    >
  )

install( TARGETS sample_plugin_opencl LIBRARY DESTINATION ${MFX_SAMPLES_INSTALL_LIB_DIR} )

# copy .cl file into build directory
execute_process( COMMAND ${CMAKE_COMMAND} -E copy
"${CMAKE_CURRENT_SOURCE_DIR}/src/ocl_rotate.cl"
"${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}/ocl_rotate.cl" )

install( FILES src/ocl_rotate.cl DESTINATION ${MFX_SAMPLES_INSTALL_LIB_DIR} )
