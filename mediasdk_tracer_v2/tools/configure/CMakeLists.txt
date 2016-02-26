cmake_minimum_required( VERSION 2.6.2 FATAL_ERROR )
project(mfx-tracer-config)

set(HEADERS "strfuncs.h" "config_manager.h")
set(SOURCES "config_manager.cpp" "main.cpp")

set(no_warnings "-Wno-unknown-pragmas -Wno-unused")
set(debug_flags "-g -D_DEBUG -O0 -Wall ${no_warnings}")
set(release_flags "-DNDEBUG -O2 -D_FORTIFY_SOURCE=2 -fstack-protector -Wall ${no_warnings}")

set(CMAKE_C_FLAGS "-pipe -fPIC -fpie")
set(CMAKE_CXX_FLAGS "-pipe -fPIC -fpie")
set(CMAKE_C_FLAGS_DEBUG     "${debug_flags}" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE   "${release_flags}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG   "${debug_flags}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "${release_flags}" CACHE STRING "" FORCE)

set(defs "${WARNING_FLAGS}")
add_executable(mfx-tracer-config ${SOURCES} ${HEADERS})
set_target_properties( mfx-tracer-config PROPERTIES LINK_FLAGS
  "-Wl,--no-undefined,-z,relro,-z,now,-z,noexecstack -fstack-protector -pie ${WARNING_FLAGS}" )
target_link_libraries( mfx-tracer-config dl rt )
set(defs "")
