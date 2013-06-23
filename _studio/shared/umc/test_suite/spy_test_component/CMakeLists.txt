mfx_include_dirs( )

include_directories (
  ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/test_suite/spy_test_component/include
  ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/codec/color_space_converter/include
  ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/codec/h264_dec/include
  ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/test_suite/spy_test_component/xerces/include
)

find_library( XERCES_LIBRARY xerces-c )

if(NOT XERCES_LIBRARY MATCHES NOTFOUND)

  message( -- Found: ${XERCES_LIBRARY} !  )

  link_directories ( ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/test_suite/spy_test_component/xerces/lib )

  list(APPEND sources
    ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/test_suite/spy_test_component/src/checker.cpp
    ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/test_suite/spy_test_component/src/dll_main.cpp
    ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/test_suite/spy_test_component/src/outline.cpp
    ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/test_suite/spy_test_component/src/outline_factory.cpp
    ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/test_suite/spy_test_component/src/test_component.cpp
    ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/test_suite/spy_test_component/src/video_data_checker.cpp
    ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/test_suite/spy_test_component/src/video_data_checker_mfx.cpp
    ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/test_suite/spy_test_component/src/xerces_entry.cpp
    ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/test_suite/spy_test_component/src/xerces_outline_reader.cpp
    ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/test_suite/spy_test_component/src/xerces_outline_writer.cpp
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

  make_library( outline none shared )
else ()
  message( !! ${XERCES_LIBRARY} )
endif()
