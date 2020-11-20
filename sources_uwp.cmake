if(NOT CMAKE_SYSTEM_NAME MATCHES WindowsStore)
  return()
endif()

foreach(var
  CMAKE_C_FLAGS CMAKE_CXX_FLAGS
  CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELWITHDEBINFO
  CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELWITHDEBINFO)
  # UWP DOES NOT SUPPORT MT
  string(REPLACE "/MT" "/MD" ${var} "${${var}}")
endforeach()

# mdp_msdk-lib\api\intel_api_uwp\dispatcher_uwp\libmfx_uwp.sln -
# mdp_msdk-lib\samples\ExtendedUWPSamples\InteropSamples.sln -
# mdp_msdk-lib\_testsuite\mfx_uwptest\mfx_uwptest.sln -
# mdp_msdk-lib\api\intel_api_uwp\intel_gfx_api.sln -

message(STATUS "VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION ${VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION}")

add_subdirectory(api/intel_api_uwp/dispatcher_uwp)
add_subdirectory(_testsuite/mfx_uwptest/mfx_uwptest)
add_subdirectory(api/intel_api_uwp/)
add_subdirectory(samples/ExtendedUWPSamples)