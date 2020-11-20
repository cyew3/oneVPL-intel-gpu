add_executable(mfx-tracer-config)

target_sources(mfx-tracer-config
  PRIVATE
    config_manager.cpp
    config_manager.h
    main.cpp
    strfuncs.h
)
install(TARGETS mfx-tracer-config RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
