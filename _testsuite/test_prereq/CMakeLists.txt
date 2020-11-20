if(NOT CMAKE_SYSTEM_NAME MATCHES Windows)
  return()
endif()

add_executable(test_prereq InstallerPreReq.cpp)

target_link_libraries(test_prereq PRIVATE mfx_shared_lib mfx)

target_compile_definitions(test_prereq PRIVATE MFX_VERSION_USE_LATEST)

set_property(TARGET test_prereq PROPERTY FOLDER "testsuite")