if(NOT CMAKE_SYSTEM_NAME MATCHES WindowsStore OR NOT BUILD_EXTRA)
  return()
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(arch_postfix x64)
else()
  set(arch_postfix x86)
endif()

set(intel_gfx_api_dll intel_gfx_api-${arch_postfix})

add_library(${intel_gfx_api_dll} SHARED)

target_include_directories(${intel_gfx_api_dll}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(${intel_gfx_api_dll}
  PRIVATE
    src/intel_api.def
    src/intel_gfx_api.rc
    src/intel_api_factory.cpp
    src/mfx_dxloader.cpp
    include/intel_api_factory.h
    include/mfx_dxloader.h
    src/resource.h
)

target_compile_options(${intel_gfx_api_dll}
  PRIVATE
    /EHa
    /guard:cf
    /sdl
    /GS
    /WX
)

target_compile_definitions(${intel_gfx_api_dll}
  PRIVATE
    _WINDLL
    NDEBUG
    MEDIASDK_USE_CFGFILES
    MEDIASDK_USE_REGISTRY
    MEDIASDK_UWP_LOADER
    WINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP
)

target_link_libraries(${intel_gfx_api_dll} PRIVATE libmfx_uwp)
target_link_options(${intel_gfx_api_dll}
  PRIVATE
      /DEBUG
      /PDB:$<TARGET_PDB_FILE_DIR:${intel_gfx_api_dll}>/${intel_gfx_api_dll}_full.pdb
      /PDBSTRIPPED:$<TARGET_PDB_FILE_DIR:${intel_gfx_api_dll}>/${intel_gfx_api_dll}.pdb    
)

target_compile_options(${intel_gfx_api_dll} PRIVATE /DEF:"src/intel_api.def")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(uwp_suff x64)
else()
    set(uwp_suff x86)
endif()

set(tmp_path ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/Intel.MediaSDK.${uwp_suff})
set(no_action $<NOT:$<CONFIG:Release>>)

file(TO_NATIVE_PATH ${tmp_path} tmp_path)


add_custom_command(TARGET ${intel_gfx_api_dll} POST_BUILD
    COMMAND
      if not exist ${tmp_path} mkdir ${tmp_path}
    COMMAND
      copy ${tmp_path}\\..\\${intel_gfx_api_dll}.dll ${tmp_path}
    COMMAND
      copy dfp\\AppxManifest.${uwp_suff}.xml ${tmp_path}\\AppxManifest.xml
    COMMAND
      copy dfp\\logo.png ${tmp_path}
    COMMAND
      del /q ${tmp_path}.appx
    COMMAND
      makeappx.exe pack /p ${tmp_path}.appx /d ${tmp_path} /l /h sha256
    COMMAND
      signtool sign /fd sha256 /f dfp/key.pfx ${tmp_path}.appx
    COMMAND
      rmdir /s /q ${tmp_path}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(${intel_gfx_api_dll} PROPERTIES
  VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "${VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION}"
  PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
  INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
)
