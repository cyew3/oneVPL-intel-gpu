if( NOT ENABLE_MONDELLO )
  return()
endif()

add_executable( sample_modello )


target_include_directories(sample_modello
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../sample_common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../sample_misc/wayland/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../sample_plugins/rotate_cpu/include
)

target_sources(sample_modello
  PRIVATE
    src/mondello_util.cpp
    src/pipeline_encode.cpp
    src/pipeline_user.cpp
    src/sample_mondello.cpp
  )

target_link_libraries(sample_modello PRIVATE libmfx ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} sample_common ia_cipf camhal)

install( TARGETS ${target} RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )

