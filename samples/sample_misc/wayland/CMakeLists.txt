if (NOT ENABLE_WAYLAND)
    return()
endif()

if(CMAKE_SYSTEM_NAME MATCHES Linux)
  pkg_check_modules(PKG_LIBDRM         REQUIRED QUIET libdrm>=2.4.91 IMPORTED_TARGET)
  pkg_check_modules(PKG_LIBDRM_INTEL   REQUIRED QUIET libdrm_intel   IMPORTED_TARGET)
  pkg_check_modules(PKG_LIBVA          REQUIRED QUIET libva>=1.9     IMPORTED_TARGET)
  pkg_check_modules(PKG_WAYLAND_CLIENT REQUIRED QUIET wayland-client IMPORTED_TARGET)
endif()

add_library(mfx_wayland SHARED)
set_property(TARGET mfx_wayland PROPERTY FOLDER "samples")

target_include_directories(mfx_wayland
  PUBLIC
    include
  )

target_compile_definitions(mfx_wayland
  PRIVATE
    ${WARNING_FLAGS}
    $<$<PLATFORM_ID:Linux>:LIBVA_WAYLAND_SUPPORT>
  )

target_compile_features(mfx_wayland PUBLIC cxx_std_11)
target_link_libraries(mfx_wayland
  PUBLIC
    sample_common
    $<$<PLATFORM_ID:Linux>:PkgConfig::PKG_WAYLAND_CLIENT>
    $<$<PLATFORM_ID:Linux>:PkgConfig::PKG_LIBVA>
    $<$<PLATFORM_ID:Linux>:PkgConfig::PKG_LIBDRM>
    $<$<PLATFORM_ID:Linux>:PkgConfig::PKG_LIBDRM_INTEL>
    Threads::Threads
    ${CMAKE_DL_LIBS}
  )

target_sources(mfx_wayland
  PRIVATE
    include/class_wayland.h
    include/listener_wayland.h
    include/wayland-drm-client-protocol.h

    src/class_wayland.cpp
    src/listener_wayland.cpp
    src/wayland-drm-protocol.c
  )

install( TARGETS mfx_wayland LIBRARY DESTINATION ${MFX_SAMPLES_INSTALL_LIB_DIR} )
