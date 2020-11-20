if(NOT CMAKE_SYSTEM_NAME MATCHES WindowsStore)
  return()
endif()

add_subdirectory(MSDKInterop)
add_subdirectory(PlayerSample)
add_subdirectory(EncoderSample)