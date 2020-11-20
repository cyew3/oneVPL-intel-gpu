if (Windows)
  set(sample_spl_mux_dispatcher_name sample_spl_mux_dispatcher_v15)
else()
  set(sample_spl_mux_dispatcher_name sample_spl_mux_dispatcher)
endif()

add_library(${sample_spl_mux_dispatcher_name} STATIC)
set_property(TARGET ${sample_spl_mux_dispatcher_name} PROPERTY FOLDER "samples")

target_sources(${sample_spl_mux_dispatcher_name}
  PRIVATE
    include/mux_exposed_functions_has_impl.h
    include/mux_exposed_functions_list.h
    include/spl_exposed_functions_has_impl.h
    include/spl_exposed_functions_list.h

    src/spl_mux_dispatcher.c
  )

target_include_directories(${sample_spl_mux_dispatcher_name}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../api/
  PRIVATE
    ${MSDK_SAMPLES_ROOT}/deprecated/sample_spl_mux/api
    ${CMAKE_HOME_DIRECTORY}/api/include
  )
