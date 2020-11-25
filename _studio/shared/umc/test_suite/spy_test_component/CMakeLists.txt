if (CMAKE_SYSTEM_NAME MATCHES Windows OR MFX_BUNDLED_IPP)
  return()
endif()

if(CMAKE_BUILD_TYPE MATCHES release)
  set(OUTLINE_LIB_NAME outline)
else()
  set(OUTLINE_LIB_NAME outline_d)
endif()

pkg_check_modules(PKG_XERCES xerces-c>=3.0 IMPORTED_TARGET)

if( PKG_XERCES_FOUND )
  add_library(${OUTLINE_LIB_NAME} SHARED )

  target_include_directories(${OUTLINE_LIB_NAME}
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/../spy_test_component/include
      ${CMAKE_CURRENT_SOURCE_DIR}/../../codec/color_space_converter/include
      ${CMAKE_CURRENT_SOURCE_DIR}/../../codec/h264_dec/include
      ${CMAKE_CURRENT_SOURCE_DIR}/../spy_test_component/xerces/include
  )

  target_sources(${OUTLINE_LIB_NAME}
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src/checker.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/dll_main.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/outline.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/outline_factory.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/test_component.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/video_data_checker.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/video_data_checker_mfx.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xerces_entry.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xerces_outline_reader.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/xerces_outline_writer.cpp
  )

  target_link_libraries(${OUTLINE_LIB_NAME}
    PRIVATE
      umc_codecs_merged
      color_space_converter
      mpeg2_dec_sw
      h264_dec
      vc1_dec
      jpeg_dec_sw
      media_buffers
      umc_io
      umc
      vm
      vm_plus
      IPP::vc IPP::cc IPP::i IPP::s IPP::dc IPP::core
      pthread
      dl
      PkgConfig::PKG_XERCES
      mfx_static_lib
      mfx_shared_lib
  )

  target_compile_definitions(${OUTLINE_LIB_NAME} PRIVATE XERCES_XML)
endif()
