add_executable( sample_videoconf )

target_include_directories(sample_videoconf
  PUBLIC
    ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_common/include
    ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/deprecated/sample_videoconf/include
)

target_sources(sample_videoconf
  PRIVATE
    src/sample_video_conf.cpp
    src/video_conf_pipeline.cpp
  )

target_link_libraries(sample_videoconf PRIVATE libmfx ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})


