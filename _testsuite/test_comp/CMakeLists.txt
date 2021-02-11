if(CMAKE_SYSTEM_NAME MATCHES Windows)
  set(test_comp simple_composite)
else()
  set(test_comp test_comp)
  pkg_check_modules(PKG_LIBDRM REQUIRED libdrm IMPORTED_TARGET)
endif()

add_executable(${test_comp})

target_include_directories(${test_comp}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

target_sources(${test_comp}
  PRIVATE
    common/common_utils.h
    common/common_utils.cpp
    simple_composite/src/simple_composite.cpp
)

target_link_libraries(${test_comp}
  PRIVATE
    mfx_shared_lib
    mfx_va_properties
    ${MFX_DISPATCHER}
    $<$<PLATFORM_ID:Linux>:PkgConfig::PKG_LIBDRM>
)

set(common_linux_defs
  LINUX32
  LIBVA_SUPPORT
  LIBVA_DRM_SUPPORT
  $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:LINUX64>
)

target_compile_definitions(${test_comp}
  PRIVATE
    MFX_VERSION_USE_LATEST
    $<$<PLATFORM_ID:Linux>:${common_linux_defs}>
  )

set_property(TARGET ${test_comp} PROPERTY FOLDER "testsuite")
