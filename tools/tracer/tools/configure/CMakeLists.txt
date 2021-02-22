add_executable(mfx-tracer-config)
if (NOT CMAKE_C_COMPILER_ID MATCHES Intel)
  set_property(TARGET mfx-tracer-config PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

target_sources(mfx-tracer-config
  PRIVATE
    config_manager.cpp
    config_manager.h
    main.cpp
    strfuncs.h
)
install(TARGETS mfx-tracer-config RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
