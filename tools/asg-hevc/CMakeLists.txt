add_executable(asg-hevc)

target_include_directories (asg-hevc
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

target_sources(asg-hevc
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asg-hevc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frame_processor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputparameters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_processor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/util_defs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/block_structures.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frame_marker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frame_reorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/random_generator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mvmvp_processor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/verifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/refcontrol.cpp
  )

target_link_libraries(asg-hevc
  PUBLIC
    mfx
    sample_common
    mfx_static_lib
    mfx_shared_lib
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
  )

target_compile_options(asg-hevc PRIVATE $<IF:$<PLATFORM_ID:Linux>,-std=c++11 -Werror,/std:c++11 /WX->)

target_compile_definitions(asg-hevc PRIVATE MFX_VERSION_USE_LATEST)

install( TARGETS asg-hevc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )