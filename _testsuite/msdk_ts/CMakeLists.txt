mfx_include_dirs( )

include_directories (
  /usr/include/c++/4.6/bits
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/include
  ${CMAKE_HOME_DIRECTORY}/_testsuite/bs_parser/include
  ${CMAKE_HOME_DIRECTORY}/_testsuite/shared/include
  ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/codec/demuxer/include
  ${CMAKE_HOME_DIRECTORY}/samples/sample_common/include  
)

list(APPEND sources
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/main.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/msdk_ts.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/stdafx.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/test_blocks.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/test_tools.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/test_trace.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/test_verif.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/test_libva_utils.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/test_sample_allocator.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/debug_output.cpp
)

add_definitions( -DUSE_SAMPLE_ALLOCATOR=1 )

foreach( variant sw x11 drm )
  check_variant( ${variant} configured )
  set( LIBS "" )
  if ( configured )  
    list( APPEND LIBS
      dispatch_trace
      umc_core_merged
      bs_parser
      dl
      ippvc_l ippcc_l ippi_l ipps_l ippcore_l
      sample_common_${variant}
      pthread
    )

    make_executable( msdk_ts ${variant} )
  endif()
endforeach()

# =============================================================================

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
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/dll_main.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/msdk_ts.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/stdafx.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/test_blocks.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/test_tools.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/test_trace.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/test_verif.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/test_libva_utils.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/msdk_ts_wrap.cxx
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/test_sample_allocator.cpp
  ${CMAKE_HOME_DIRECTORY}/_testsuite/msdk_ts/src/debug_output.cpp
)

foreach( variant sw x11 drm)
  check_variant( ${variant} configured )
  set( LIBS "" )
  if ( configured )  
    list( APPEND LIBS
      dispatch_trace
      umc_core_merged
      bs_parser
      dl
      pthread
      ippvc_l ippcc_l ippi_l ipps_l ippcore_l
      sample_common_${variant}
      ${PERL_LIBRARY}
    )

    make_library( msdk_ts_dll ${variant} shared )
  endif()
endforeach()

