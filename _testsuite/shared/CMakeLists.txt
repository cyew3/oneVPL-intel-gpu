add_library(shared_utils STATIC)

set(common_sources
  include/app_defs.h
  include/automatic_pointer.h
  include/automatic_vm_file.h
  include/cc_utils.h
  include/framemodewrapperctx.h
  include/lucas.h
  include/memory_allocator.h
  include/mfx_frames.h
  include/mfx_frames_new.h
  include/mfx_io_utils.h
  include/shared_utils.h
  include/test_h264_dec_command_line.h
  include/test_statistics.h

  src/cc_utils.cpp
  src/memory_allocator.cpp
  src/mfx_io_utils.cpp
  src/shared_utils.cpp
  src/test_h264_dec_command_line.cpp
  src/test_statistics.cpp
)

target_sources(shared_utils
  PRIVATE
    ${common_sources}
)

target_include_directories(shared_utils
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${MSDK_TSUITE_ROOT}/shared/include
)

target_link_libraries(shared_utils
  PUBLIC
    vm_plus
    ${MFX_DISPATCHER}
  )

add_library(shared_utils_lucas STATIC)

target_sources(shared_utils_lucas
  PRIVATE
    ${common_sources}
)

target_include_directories(shared_utils_lucas
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${MSDK_TSUITE_ROOT}/shared/include
)

target_link_libraries(shared_utils_lucas
  PUBLIC
    vm_plus
    ${MFX_DISPATCHER}
  )

target_compile_definitions(shared_utils_lucas PRIVATE LUCAS_DLL)

set_property(TARGET shared_utils shared_utils_lucas PROPERTY FOLDER "testsuite")
