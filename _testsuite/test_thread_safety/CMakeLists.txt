if(MFX_BUNDLED_IPP OR NOT BUILD_SAMPLES OR MFX_DISABLE_SW_FALLBACK)
  return()
endif()

add_executable(test_thread_safety)

target_include_directories (test_thread_safety
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(test_thread_safety
  PRIVATE
    include/test_thread_safety.h
    include/test_thread_safety_cmdline.h
    include/test_thread_safety_decode.h
    include/test_thread_safety_encode.h
    include/test_thread_safety_output.h
    include/test_thread_safety_utils.h
    src/test_thread_safety.cpp
    src/test_thread_safety_cmdline.cpp
    src/test_thread_safety_decode.cpp
    src/test_thread_safety_encode.cpp
    src/test_thread_safety_h264_encpak.cpp
    src/test_thread_safety_mpeg2_encpak.cpp
    src/test_thread_safety_output.cpp
    src/test_thread_safety_vc1_encpak.cpp
)

target_link_libraries(test_thread_safety
  PUBLIC
    mfx_trans_pipeline
    mfx_pipeline
    shared_utils
    sample_common
    mfx
    media_buffers
    umc_io
    umc
    vm
    vm_plus
    $<$<PLATFORM_ID:Windows>:sample_spl_mux_dispatcher_v15>
    $<$<PLATFORM_ID:Linux>:sample_spl_mux_dispatcher>
    mfx_trace
    mfx_shared_lib
    ${ITT_LIBRARIES}
    IPP::vc IPP::cc IPP::dc IPP::i IPP::s IPP::core
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
)

target_compile_definitions(test_thread_safety PRIVATE MFX_VERSION_USE_LATEST)

set_property(TARGET test_thread_safety PROPERTY FOLDER "testsuite")