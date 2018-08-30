##******************************************************************************
##  Copyright(C) 2015 Intel Corporation. All Rights Reserved.
##
##  The source code, information  and  material ("Material") contained herein is
##  owned  by Intel Corporation or its suppliers or licensors, and title to such
##  Material remains  with Intel Corporation  or its suppliers or licensors. The
##  Material  contains proprietary information  of  Intel or  its  suppliers and
##  licensors. The  Material is protected by worldwide copyright laws and treaty
##  provisions. No  part  of  the  Material  may  be  used,  copied, reproduced,
##  modified, published, uploaded, posted, transmitted, distributed or disclosed
##  in any way  without Intel's  prior  express written  permission. No  license
##  under  any patent, copyright  or  other intellectual property rights  in the
##  Material  is  granted  to  or  conferred  upon  you,  either  expressly,  by
##  implication, inducement,  estoppel or  otherwise.  Any  license  under  such
##  intellectual  property  rights must  be express  and  approved  by  Intel in
##  writing.
##
##  *Third Party trademarks are the property of their respective owners.
##
##  Unless otherwise  agreed  by Intel  in writing, you may not remove  or alter
##  this  notice or  any other notice embedded  in Materials by Intel or Intel's
##  suppliers or licensors in any way.
##
##******************************************************************************
##  Content: Intel(R) Media SDK Global Configuration of Targets Cmake module
##******************************************************************************

message( STATUS "Global Configuration of Targets" )
if(__TARGET_PLATFORM)
    add_definitions( -DLINUX_TARGET_PLATFORM -DLINUX_TARGET_PLATFORM_${__TARGET_PLATFORM} )
endif()
#
set( T_ARCH "sse4.2" )
#if( __TARGET_PLATFORM STREQUAL "BDW" )
#    set( T_ARCH "avx2" )
#endif()
message( STATUS "Target Architecture to compile: ${T_ARCH}" )
#
if( __TARGET_PLATFORM STREQUAL "BXTMIN" )
    set(MFX_LIB_ONLY TRUE)
    message( STATUS "!!! BXTMIN target MFX_LIB_ONLY !!!" )
endif()

# HEVC plugins disabled by default
set( ENABLE_HEVC FALSE )
set( ENABLE_HEVC_FEI FALSE )

if( MFX_LIB_ONLY )
    set( ENABLE_HEVC_FEI FALSE )
    message("!!! NO HEVC PLUGINS - MFX LIB ONLY BUILD !!!")
else()
    if ((CMAKE_C_COMPILER MATCHES icc) OR ENABLE_HEVC_ON_GCC )
       set(ENABLE_HEVC TRUE)
       set(ENABLE_HEVC_FEI TRUE)
       message( STATUS "  Enabling HEVC plugins build!")
    endif()
endif()

append("-std=c++11" CMAKE_CXX_FLAGS)

# SW HEVC decoder & encoder require SSE4.2
  if (CMAKE_C_COMPILER MATCHES icc)
    append("-xSSE4.2 -static-intel" CMAKE_C_FLAGS)
  else()
    append("-m${T_ARCH}" CMAKE_C_FLAGS)
  endif()

  if (CMAKE_CXX_COMPILER MATCHES icpc)
    append("-xSSE4.2 -static-intel" CMAKE_CXX_FLAGS)
  else()
    append("-m${T_ARCH}" CMAKE_CXX_FLAGS)
  endif()

include(CMakeDependentOption)

option( MFX_ENABLE_KERNELS "Build with advanced media kernels support?" ON )

if ( ${API_VERSION} VERSION_GREATER 1.25 )
  set ( MFX_1_25_OPTIONS_ALLOWED ON )
else()
  set ( MFX_1_25_OPTIONS_ALLOWED OFF )
endif()

option( MFX_ENABLE_USER_DECODE "Enabled user decode plugins?" ON)
option( MFX_ENABLE_USER_ENCODE "Enabled user encode plugins?" ON)
option( MFX_ENABLE_USER_ENC "Enabled user ENC plugins?" ON)
option( MFX_ENABLE_USER_VPP "Enabled user VPP plugins?" ON)

option( MFX_ENABLE_VP8_VIDEO_DECODE "Enabled VP8 decoder?" ON)
option( MFX_ENABLE_VP9_VIDEO_DECODE "Enabled VP9 decoder?" ON)
option( MFX_ENABLE_H265_VIDEO_DECODE "Enabled HEVC decoder?" ON)
option( MFX_ENABLE_MPEG2_VIDEO_DECODE "Enabled MPEG2 decoder?" ON)
option( MFX_ENABLE_MPEG2_VIDEO_ENCODE "Enabled MPEG2 encoder?" ON)
option( MFX_ENABLE_MJPEG_VIDEO_DECODE "Enabled MJPEG decoder?" ON)
option( MFX_ENABLE_MJPEG_VIDEO_ENCODE "Enabled MJPEG encoder?" ON)
option( MFX_ENABLE_VC1_VIDEO_DECODE "Enabled VC1 decoder?" ON)

option( MFX_ENABLE_H264_VIDEO_ENCODE "Enable H.264 (AVC) encoder?" ON)
cmake_dependent_option(
  MFX_ENABLE_H264_VIDEO_FEI_ENCODE "Enable H.264 (AVC) FEI?" ON
  "MFX_ENABLE_H264_VIDEO_ENCODE" OFF)

option( MFX_ENABLE_H265_VIDEO_ENCODE "Enable H.265 (HEVC) encoder?" ON)
cmake_dependent_option(
  MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE "Enable H.265 (HEVC) FEI?" ON
  "MFX_ENABLE_H265_VIDEO_ENCODE" OFF)

option( MFX_ENABLE_VP9_VIDEO_ENCODE "Enable VP9 encoder?" ON)

option( MFX_ENABLE_MCTF "Build with MCTF support?"  ${MFX_1_25_OPTIONS_ALLOWED} )

# Now we will include config file which may overwrite default values of the
# options and options which user provided in a command line.
# It is critically important to include config file _after_ definition of
# all options. Otherwise rewrite of options in a config file will not take
# effect!
if (DEFINED MFX_CONFIG_FILE)
    # Include user provided cmake config file of the format:
    # set( VARIABLE VALUE )
    include(${MFX_CONFIG_FILE})
endif()

configure_file(${BUILDER_ROOT}/mfxconfig.h.in mfxconfig.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
