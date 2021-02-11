if (NOT MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE)
  return()
endif()

find_path( FEI_INCLUDE mfxfeihevc.h PATHS ${MFX_INCLUDE} )

add_executable(hevc_fei_extractor)

target_include_directories(hevc_fei_extractor
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_sources(hevc_fei_extractor
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

target_compile_definitions(hevc_fei_extractor PRIVATE MFX_VERSION_USE_LATEST)

target_link_libraries(hevc_fei_extractor
  PRIVATE
    mfx_static_lib
    mfx_shared_lib
    ${CMAKE_THREAD_LIBS_INIT}
    bs_parser_hevc_static
)

install( TARGETS hevc_fei_extractor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )

