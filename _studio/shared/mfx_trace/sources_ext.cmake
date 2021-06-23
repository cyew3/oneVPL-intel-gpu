if (NOT CMAKE_SYSTEM_NAME MATCHES Windows)
    return()
endif()

target_include_directories(mfx_trace PRIVATE ${CMAKE_BINARY_DIR})

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/media_sdk_etw.h ${CMAKE_BINARY_DIR}/media_sdk_etw.rc
    SOURCES ${MSDK_STUDIO_ROOT}/shared/mfx_trace/media_sdk_etw.man
    COMMENT Running Windows Message Compiler to compile Media SDK ETW manifest
    COMMAND mc ARGS -h "${CMAKE_BINARY_DIR}" -r "${CMAKE_BINARY_DIR}" -z media_sdk_etw -um "${MSDK_STUDIO_ROOT}/shared/mfx_trace/media_sdk_etw.man"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_sources(mfx_trace
  PRIVATE
    ${CMAKE_BINARY_DIR}/media_sdk_etw.h
)
