# disable_werror()
add_executable(compare_struct)

target_include_directories(compare_struct
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(compare_struct
  PRIVATE
    compare_struct.cpp
    ${MSDK_LIB_ROOT}/shared/src/mfx_common_int.cpp
)

target_link_libraries(compare_struct
  PRIVATE
    vm
    vm_plus
    mfx_trace
    $<$<BOOL:${ITT_FOUND}>:itt>
    gtest
    gtest_main
    gmock
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
)

set_target_properties(compare_struct PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE})

add_test(NAME compare_struct
  COMMAND ./compare_struct
  WORKING_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}
)

set_property(TEST compare_struct
  PROPERTY ENVIRONMENT
    "LD_LIBRARY_PATH=${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}"
)
