if(NOT CMAKE_SYSTEM_NAME MATCHES Windows)
  return()
endif()
add_executable(sample_camera)
set_property(TARGET sample_camera PROPERTY FOLDER "samples")
if (NOT CMAKE_C_COMPILER_ID MATCHES Intel)
  set_property(TARGET sample_camera PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

target_include_directories(sample_camera PRIVATE include)
target_link_libraries(sample_camera
  PRIVATE
    sample_common
    mfx_static_lib
    mfx_shared_lib
    mfx_sdl_properties
    $<$<PLATFORM_ID:Windows>:Dwmapi>
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

target_sources(sample_camera
  PRIVATE
    include/camera_render.h
    include/camera_sysmem_allocator.h
    include/pipeline_camera.h
    include/sample_camera_utils.h

    src/camera_render.cpp
    src/camera_sysmem_allocator.cpp
    src/pipeline_camera.cpp
    src/sample_camera_utils.cpp
    src/sample_camera.cpp
  )

install( TARGETS sample_camera RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )
