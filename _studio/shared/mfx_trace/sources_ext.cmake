if (NOT CMAKE_SYSTEM_NAME MATCHES Windows)
    return()
endif()

target_include_directories(mfx_trace PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/media_sdk_etw.h
    COMMENT Running Windows Message Compiler to compile Media SDK ETW manifest
    COMMAND mc ARGS -h ${CMAKE_CURRENT_BINARY_DIR} -r ${CMAKE_CURRENT_BINARY_DIR} -z media_sdk_etw -um ${CMAKE_CURRENT_SOURCE_DIR}/media_sdk_etw.man
    DEPENDS media_sdk_etw.man
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

target_sources(mfx_trace
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/media_sdk_etw.h
)
