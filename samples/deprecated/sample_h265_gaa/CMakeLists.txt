find_path( FEIH265_INCLUDE mfxfeih265.h PATHS ${MFX_API_HOME}/include )

if(FEIH265_INCLUDE MATCHES NOTFOUND)
  set(skipping _force_skipping)
endif()

add_executable( sample_h265_gaa )

target_include_directories(sample_h265_gaa
  PUBLIC
    ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_common/include
    ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_utilities/vpp_plugin/include
    ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/deprecated/sample_h265_gaa/include
)

target_sources(sample_h265_gaa
  PRIVATE
    src/file_input.cpp
    src/file_output.cpp
    src/main.cpp
    src/plugin_wrapper.cpp
  )

target_link_libraries(sample_h265_gaa PRIVATE libmfx ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

