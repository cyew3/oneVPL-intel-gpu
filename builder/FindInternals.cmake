##******************************************************************************
##  Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
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
##  Content: Intel(R) Media SDK Samples projects creation and build
##******************************************************************************

function( mfx_include_dirs )
  include_directories (
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/shared/include
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/shared/umc/core/vm/include
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/shared/umc/core/vm_plus/include
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/shared/umc/core/umc/include
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/shared/umc/io/umc_io/include
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/shared/umc/io/umc_va/include
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/shared/umc/io/media_buffers/include
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/shared/include
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/optimization/h265/include
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/optimization/h264/include
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/shared/include
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/fei/include
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/fei/h264_la
    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/SafeStringStaticLibrary/include
  )

  set ( MSDK_STUDIO_ROOT ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio PARENT_SCOPE )
  set ( MSDK_LIB_ROOT    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib PARENT_SCOPE )
  set ( MSDK_UMC_ROOT    ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/shared/umc PARENT_SCOPE )
endfunction()
