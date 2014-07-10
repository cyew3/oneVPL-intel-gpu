include( ${CMAKE_HOME_DIRECTORY}/../builder/FindOpenCV.cmake )

if(OPENCV_FOUND)

set( PLUGIN_API ${CMAKE_SOURCE_DIR}/sample_user_modules/plugin_api )

include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_user_modules/people_detector_plugin/include
  ${PLUGIN_API}
)

list( APPEND LIBS sample_common opencv_core opencv_highgui opencv_objdetect)

make_library(people_plugin_detector none shared)

endif ( )
