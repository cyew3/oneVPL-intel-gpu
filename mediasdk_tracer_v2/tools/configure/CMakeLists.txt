cmake_minimum_required( VERSION 2.6.2 FATAL_ERROR )
project(mfxconf)

set(HEADERS "strfuncs.h" "config_manager.h")
set(SOURCES "config_manager.cpp" "main.cpp")
add_executable(mfx-tracer-config ${SOURCES} ${HEADERS})
