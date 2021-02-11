if(MFX_BUNDLED_IPP OR NOT BUILD_SAMPLES)
  return()
endif()

add_executable(test_usage_models)

target_include_directories(test_usage_models
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(test_usage_models
  PRIVATE
    src/test_usage_models.cpp
    src/test_usage_models_allocator.cpp
    src/test_usage_models_bitstream_reader.cpp
    src/test_usage_models_bitstream_writer.cpp
    src/test_usage_models_cmdline.cpp
    src/test_usage_models_surfaces_manager.cpp
    src/transcode_model_advanced.cpp
    src/transcode_model_reference.cpp

    include/test_usage_models_allocator.h
    include/test_usage_models_bitstream_manager.h
    include/test_usage_models_bitstream_reader.h
    include/test_usage_models_bitstream_writer.h
    include/test_usage_models_cmdline.h
    include/test_usage_models_exception.h
    include/test_usage_models_surfaces_manager.h
    include/test_usage_models_utils.h
    include/transcode_model_advanced.h
    include/transcode_model_api.h
    include/transcode_model_reference.h
)
target_link_libraries(test_usage_models
  PUBLIC
    shared_utils
    umc
    vm
    vm_plus
    sample_common
    mfx_shared_lib
    IPP::s IPP::core
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
)

target_compile_definitions(test_usage_models PRIVATE MFX_VERSION_USE_LATEST)

set_property(TARGET test_usage_models PROPERTY FOLDER "testsuite")