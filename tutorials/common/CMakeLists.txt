
add_library(tutorials_common)

target_sources(tutorials_common
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cmd_options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common_utils.cpp
)

set_target_properties (tutorials_common
    PROPERTIES
        FOLDER "tutorials"
)


target_link_libraries(tutorials_common
  PUBLIC
    ${MFX_DISPATCHER}
    mfx_shared_lib
    mfx_va_properties
    mfx_trace
    ${CMAKE_DL_LIBS}
)

if (CMAKE_SYSTEM_NAME MATCHES Linux)
  target_sources(tutorials_common
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/common_utils_linux.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/common_vaapi.cpp
  )

  include(CheckSymbolExists)
  check_symbol_exists( clock_gettime "time.h" _gcc_CLOCK_GETTIME )

  if (NOT _gcc_CLOCK_GETTIME)
    target_link_libraries( tutorials_common PUBLIC rt)
  endif()

elseif(CMAKE_SYSTEM_NAME MATCHES Windows)
  target_sources(tutorials_common
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/common_utils_windows.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/common_directx11.cpp
  )

  target_compile_definitions(tutorials_common
    PUBLIC
      DX11_D3D
  )

endif()
