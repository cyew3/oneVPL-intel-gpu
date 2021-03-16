if(MFX_BUNDLED_IPP OR (NOT BUILD_SAMPLES) OR (MFX_DISABLE_SW_FALLBACK AND NOT BUILD_VAL_TOOLS))
  return()
endif()


set(common_sources
  include/mfx_encode_decode_quality.h
  include/mfx_encode_pipeline_config.h
  include/mfx_encode_query_mod4.h
  include/mfx_encoded_frame_info_encoder.h
  include/mfx_encodeorder_encode.h
  include/mfx_encoder_protected.h
  include/mfx_encoder_wrap.h
  include/mfx_factory_transcode.h
  include/mfx_field_output_encode.h
  include/mfx_frame_reorderer.h
  include/mfx_ip_field_pair_disable_encoder.h
  include/mfx_jpeg_encode_wrap.h
  include/mfx_latency_encode.h
  include/mfx_per_frame_ext_buf_encode.h
  include/mfx_pipeline_transcode.h
  include/mfx_protected_transcoding_pipeline.h
  include/mfx_ref_list_control_encode.h
  include/mfx_ref_list_control_encoded_order.h

  src/mfx_encode_decode_quality.cpp
  src/mfx_encode_pipeline_config.cpp
  src/mfx_encode_wrap.cpp
  src/mfx_encoder_protected.cpp
  src/mfx_factory_transcode.cpp
  src/mfx_frame_reorderer.cpp
  src/mfx_latency_encode.cpp
  src/mfx_pipeline_transcode.cpp
  src/mfx_protected_transcoding_pipeline.cpp
  src/mfx_ref_list_control_encode.cpp
  src/mfx_ref_list_control_encoded_order.cpp
)

add_library(mfx_trans_pipeline STATIC ${common_sources})

target_include_directories(mfx_trans_pipeline
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
)

target_link_libraries(mfx_trans_pipeline
  PUBLIC
    mfx_static_lib
    mfx_pipeline
    IPP::i IPP::s IPP::vm IPP::dc IPP::vc IPP::dc IPP::cc IPP::cp IPP::cv IPP::j IPP::msdk IPP::core
  PRIVATE
    mfx_va_properties
)

target_compile_options(mfx_trans_pipeline
  PRIVATE
    $<IF:$<PLATFORM_ID:Linux>,-U MFX_DISABLE_SW_FALLBACK,/UMFX_DISABLE_SW_FALLBACK>
)

target_compile_definitions(mfx_trans_pipeline
  PRIVATE
    MFX_VERSION_USE_LATEST
    $<$<BOOL:${API_USE_VPL}>:MFX_ONEVPL>
)

add_library(mfx_trans_pipeline_lucas STATIC ${common_sources})

target_include_directories(mfx_trans_pipeline_lucas
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
)

target_compile_options(mfx_trans_pipeline_lucas
  PRIVATE
    $<IF:$<PLATFORM_ID:Linux>,-U MFX_DISABLE_SW_FALLBACK,/UMFX_DISABLE_SW_FALLBACK>
)

target_link_libraries(mfx_trans_pipeline_lucas
  PUBLIC
    mfx_static_lib
    mfx_pipeline_lucas
    IPP::i IPP::s IPP::vm IPP::dc IPP::vc IPP::dc  IPP::cc IPP::cp IPP::cv IPP::j IPP::msdk IPP::core
  PRIVATE
    mfx_va_properties
)

target_compile_definitions(mfx_trans_pipeline_lucas
  PRIVATE
    MFX_VERSION_USE_LATES
    LUCAS_DLL
    $<$<BOOL:${API_USE_VPL}>:MFX_ONEVPL>
)

set_property(TARGET mfx_trans_pipeline mfx_trans_pipeline_lucas PROPERTY FOLDER "testsuite")

add_executable(mfx_transcoder)

target_sources(mfx_transcoder
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_transcoder.cpp
)

target_link_libraries(mfx_transcoder
  PUBLIC
    mfx_trans_pipeline
    mfx_shared_lib
    shared_utils
    umc
    vm
    vm_plus
    mfx_trace
    IPP::i IPP::s IPP::vm IPP::dc IPP::vc IPP::dc  IPP::cc IPP::cp IPP::cv IPP::j IPP::msdk IPP::core
    $<$<BOOL:${ITT_FOUND}>:itt>
  PRIVATE
    mfx_va_properties
)

target_compile_definitions(mfx_transcoder
  PRIVATE
    MFX_VERSION_USE_LATEST
    $<$<BOOL:${API_USE_VPL}>:MFX_ONEVPL>
)


add_library(mfx_transcoder_lucas SHARED)

target_sources(mfx_transcoder_lucas
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_transcoder.cpp
)

target_link_libraries(mfx_transcoder_lucas
  PUBLIC
    mfx_trans_pipeline_lucas
    mfx_shared_lib
    shared_utils_lucas
    umc
    vm
    vm_plus
    mfx_trace
    IPP::i IPP::s IPP::vm IPP::dc IPP::vc IPP::dc  IPP::cc IPP::cp IPP::cv IPP::j IPP::msdk IPP::core
    ${ITT_LIBRARIES}
  PRIVATE
    mfx_va_properties
)

target_compile_definitions(mfx_transcoder_lucas
  PRIVATE
    MFX_VERSION_USE_LATEST
    LUCAS_DLL
)

set_property(TARGET mfx_transcoder mfx_transcoder_lucas PROPERTY FOLDER "testsuite")
