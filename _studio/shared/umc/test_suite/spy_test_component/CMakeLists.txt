mfx_include_dirs( )

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/../spy_test_component/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../codec/color_space_converter/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../codec/h264_dec/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../spy_test_component/xerces/include
)

if( Linux )
  pkg_check_modules(PKG_XERCES xerces-c>=3.0)
elseif( Darwin )
  set (PKG_XERCES_FOUND false)
endif ()



if( PKG_XERCES_FOUND )
  link_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/xerces/lib )

  list(APPEND sources
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

  list( APPEND LIBS
    umc_codecs_merged
    umc_codecs_merged_sw
    umc_io_merged_sw
    umc_core_merged
    ippvc_l ippcc_l ippi_l ipps_l ippcore_l ippdc_l
    pthread
    dl
    xerces-c
    )

  set( defs "-DXERCES_XML" )
  if(CMAKE_BUILD_TYPE MATCHES release)
     set(OUTLINE_LIB_NAME outline)
  else()
     set(OUTLINE_LIB_NAME outline_d)
  endif()

  make_library(${OUTLINE_LIB_NAME} none shared )
endif()
