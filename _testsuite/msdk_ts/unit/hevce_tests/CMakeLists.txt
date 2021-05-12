if(MFX_BUNDLED_IPP)
  return()
endif()

###################################################################################################
# hevce_tests

set( ENABLE_HEVC FALSE )

if ( (CMAKE_C_COMPILER_ID MATCHES Intel) OR (ENABLE_HEVC_ON_GCC AND NOT CMAKE_SYSTEM_NAME MATCHES Windows))
  set(ENABLE_HEVC TRUE)
endif()

if ( ENABLE_HEVC )

# disable_werror()
###################################################################################################
# Target
add_executable(hevce_tests)

get_filename_component( CROSS_REPO_SPACE "${BUILDER_ROOT}/../.." ABSOLUTE )

###################################################################################################
# Includes
target_include_directories(hevce_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MSDK_STUDIO_ROOT}/mfx_lib/encode/h265/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/fs_det/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/fei/h265/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/cmrt_cross_platform/include
    # gtest/gmock
    ${CROSS_REPO_SPACE}/mdp_msdk-contrib/gmock/gtest/include
    ${CROSS_REPO_SPACE}/mdp_msdk-contrib/gmock/include
)

###################################################################################################
# sources
target_sources(hevce_tests
  PRIVATE
    ${MSDK_STUDIO_ROOT}/mfx_lib/shared/src/mfx_enc_common.cpp
    ${MSDK_STUDIO_ROOT}/mfx_lib/shared/src/mfx_common_int.cpp
    hevce_common_api.cpp
    hevce_common_api.h
    hevce_encoded_order.cpp
    hevce_encodeframeasync.cpp
    hevce_gop.cpp
    hevce_init.cpp
    hevce_mock_videocore.cpp
    hevce_mock_videocore.h
    hevce_optimization.cpp
    hevce_padding.cpp
    hevce_query.cpp
    hevce_queryiosurf.cpp
)

###################################################################################################

# Defines and Libs

target_link_libraries(hevce_tests
  PRIVATE
    hevc_pp_dispatcher
    hevc_pp_atom
    hevc_pp_avx2
    hevc_pp_px
    hevc_pp_sse4
    hevc_pp_ssse3
    h265_enc_hw
    h265_fei
    gmock_160
    umc
    vm
    vm_plus
    mfx_trace
    mfx_static_lib
    mfx_shared_lib
    ${ITT_LIBRARIES}
    IPP::core IPP::dc IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::msdk
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    $<$<PLATFORM_ID:Linux>:va>
)

set_property(TARGET hevce_tests PROPERTY FOLDER "testsuite")

endif() # ENABLE_HEVC
