mfx_include_dirs( )
# =============================================================================
# msdk_gmock:
#
include_directories (
#  /usr/include/c++/4.6/bits
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-samples/samples/sample_common/include/vm
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-samples/samples/sample_common/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-val-tools/bs_parser/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-api/mediasdk_structures
# ${CMAKE_HOME_DIRECTORY}/igfx_s3dcontrol/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/gmock
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/gmock/gtest/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/gmock/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/include
# ${CMAKE_HOME_DIRECTORY}/include #included by CMake
  ${CMAKE_CURRENT_SOURCE_DIR}/include/gmock
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_testsuite/mfx_serializer/include
)

list(APPEND sources
   ${CMAKE_CURRENT_SOURCE_DIR}/src/test_sample_allocator.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_alloc.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_bitstream.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_common.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_decoder.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_encoder.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_plugin.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_session.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_streams.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_struct.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_surface.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_trace.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_vpp.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_preenc.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_enc.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_pak.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/ts_transcoder.cpp
)

file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/gmock/test_suites/*.cpp" )
list( APPEND sources ${srcs})

add_definitions( -D_VARIADIC_MAX=10 )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

set( LIBS "" )
set( LIBS_SUFFIX "" )

list( APPEND LIBS
  dispatch_trace
  bs_parser
  dl
#      ippvc_l ippcc_l ippi_l ipps_l ippcore_l
  ippi_l ippdc_l ipps_l ippcore_l
  sample_common
  pthread
  mfx_serializer
)
list( APPEND LIBS_SUFFIX gmock )

set( defs "${WARNING_FLAGS}" )
make_executable( msdk_gmock universal )
set( defs "" )

# =============================================================================
# msdk_ts application
set (sources "")

include_directories (
#  /usr/include/c++/4.6/bits
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-val-tools/bs_parser/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../_studio/shared/umc/codec/demuxer/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-samples/samples/sample_common/include
)

list(APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/msdk_ts.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/stdafx.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_blocks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_tools.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_trace.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_verif.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_libva_utils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_sample_allocator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/debug_output.cpp
)

add_definitions( -DUSE_SAMPLE_ALLOCATOR=1 )

set( LIBS "" )
list( APPEND LIBS
  dispatch_trace
  umc
  vm
  vm_plus
  bs_parser
  dl
  ippvc_l ippcc_l ippi_l ipps_l ippcore_l
  sample_common
  pthread
)

set( defs "${WARNING_FLAGS}" )
make_executable( msdk_ts universal )
set( defs "" )

# =============================================================================
# msdk_ts_dll library

set( sources "" )

#find_package ( Perl )
#find_package ( PerlLibs )
set( PERL_EXECUTABLE "perl" )

execute_process ( COMMAND ${PERL_EXECUTABLE}
                  -MConfig -e "print \$Config{version}"
                  OUTPUT_VARIABLE PERL_OUTPUT
                  RESULT_VARIABLE PERL_RETURN_VALUE )
if ( NOT PERL_RETURN_VALUE )
  set ( PERL_VERSION ${PERL_OUTPUT} )
endif ( NOT PERL_RETURN_VALUE )

#if ( PERL_INCLUDE_PATH MATCHES .*-NOTFOUND )
  execute_process ( COMMAND ${PERL_EXECUTABLE}
                    -MConfig -e "print \$Config{archlibexp}"
                    OUTPUT_VARIABLE PERL_OUTPUT
                    RESULT_VARIABLE PERL_RETURN_VALUE )
  if ( NOT PERL_RETURN_VALUE )
    set ( PERL_INCLUDE_PATH ${PERL_OUTPUT}/CORE )
  endif ( NOT PERL_RETURN_VALUE )
#endif ( PERL_INCLUDE_PATH MATCHES .*-NOTFOUND )

#if ( PERL_LIBRARY MATCHES .*-NOTFOUND )
  execute_process ( COMMAND ${PERL_EXECUTABLE}
                    -MConfig -e "print \$Config{libperl}"
                    OUTPUT_VARIABLE PERL_OUTPUT
                    RESULT_VARIABLE PERL_RETURN_VALUE )
  if ( NOT PERL_RETURN_VALUE )
    #set ( PERL_LIBRARY /usr/lib/${PERL_OUTPUT} )
    find_file( PERL_LIBRARY ${PERL_OUTPUT}
          PATHS "/usr/lib/" ${PERL_INCLUDE_PATH} )
  endif ( NOT PERL_RETURN_VALUE )
#endif ( PERL_LIBRARY MATCHES .*-NOTFOUND )

message ( STATUS "  PERL_VERSION:           " ${PERL_VERSION} )
message ( STATUS "  PERL_EXECUTABLE:        " ${PERL_EXECUTABLE} )
message ( STATUS "  PERL_INCLUDE_PATH:      " ${PERL_INCLUDE_PATH} )
message ( STATUS "  PERL_LIBRARY:           " ${PERL_LIBRARY} )

include_directories (
  ${PERL_INCLUDE_PATH}
)

add_definitions( -DUSE_SAMPLE_ALLOCATOR=1 )

list(APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dll_main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/msdk_ts.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/stdafx.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_blocks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_tools.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_trace.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_verif.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_libva_utils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/msdk_ts_wrap.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/src/test_sample_allocator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/debug_output.cpp
)

set( LIBS "" )
list( APPEND LIBS
  dispatch_trace
  umc
  vm
  vm_plus
  bs_parser
  dl
  pthread
  ippvc_l ippcc_l ippi_l ipps_l ippcore_l
  sample_common
  ${PERL_LIBRARY}
)

# set( defs "${WARNING_FLAGS}" )
make_library( msdk_ts_dll universal shared )
# set( defs "" )
