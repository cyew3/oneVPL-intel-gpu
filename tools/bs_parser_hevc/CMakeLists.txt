add_library( bs_parser_hevc_static STATIC )

target_include_directories (bs_parser_hevc_static
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(bs_parser_hevc_static
  PRIVATE
    src/bs_reader.cpp
    src/bs_reader2.cpp
    src/bs_thread.cpp
    src/common_cabac.cpp
    src/dll_main.cpp
    src/hevc_cabac.cpp
    src/hevc_cabac_tables.cpp
    src/hevc_sdec_ctx.cpp
    src/hevc2_cabac.cpp
    src/hevc2_dec.cpp
    src/hevc2_headers.cpp
    src/hevc2_parser.cpp
    src/hevc2_ssdata.cpp
    src/hevc2_trace.cpp
)

target_compile_definitions(bs_parser_hevc_static
  PRIVATE
    MFX_VERSION_USE_LATEST
  )

target_link_libraries(bs_parser_hevc_static
  PRIVATE
    mfx_static_lib
    ${CMAKE_THREAD_LIBS_INIT}
  )
