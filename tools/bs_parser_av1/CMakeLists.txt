add_library( bs_parser_av1_static STATIC )

target_include_directories (bs_parser_av1_static
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(bs_parser_av1_static
  PRIVATE
    src/bs_reader.cpp
    src/bs_reader2.cpp
    src/bs_splitter.cpp
    src/bs_thread.cpp
    src/dll_main.cpp
    src/av1_frame.cpp
    src/av1_uh.cpp
)

target_link_libraries(bs_parser_av1_static
  PRIVATE
    mfx_static_lib
    ${CMAKE_THREAD_LIBS_INIT}
  )
