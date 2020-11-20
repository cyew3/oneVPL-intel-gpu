# disable_werror()
add_executable(compare_struct)

target_include_directories(compare_struct
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(compare_struct
  PRIVATE
    compare_struct.cpp
    stdafx.cpp
    stdafx.h
    targetver.h
    ${MSDK_LIB_ROOT}/shared/src/mfx_common_int.cpp
)

target_link_libraries(compare_struct
  PRIVATE
    vm
    vm_plus
    mfx_trace
    ${ITT_LIBRARIES}
    gmock
    gmock_main
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
)

