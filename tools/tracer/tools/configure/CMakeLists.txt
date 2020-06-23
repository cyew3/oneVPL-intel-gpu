make_executable( mfx-tracer-config universal "nosafestring" )
install(TARGETS mfx-tracer-config RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
