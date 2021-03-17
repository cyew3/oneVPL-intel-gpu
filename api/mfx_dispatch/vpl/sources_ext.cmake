# ##############################################################################
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ##############################################################################


target_compile_definitions(VPL PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<PLATFORM_ID:Linux>:_FORTIFY_SOURCE=2>
    )

target_compile_options(VPL PRIVATE
  $<$<AND:$<PLATFORM_ID:Linux>,$<CONFIG:Debug>>:-O0 -g>
  $<$<CXX_COMPILER_ID:MSVC>:
      /EHa
      /Qspectre
      /sdl
  >
  $<$<PLATFORM_ID:Windows>:
    /guard:cf
    /Zc:wchar_t
    /Zc:inline
    /Zc:forScope
    /GS
    /Gy
  >
  $<$<PLATFORM_ID:Linux>:
    -Wall
    -Wformat
    -Wformat-security
    -Werror=format-security
    -Wno-unknown-pragmas
    -Wno-unused
  >
)

target_link_options(VPL PRIVATE
    $<$<PLATFORM_ID:Windows>:
      /DYNAMICBASE
      /LARGEADDRESSAWARE
      /GS
      /DEBUG
      /PDB:$<TARGET_PDB_FILE_DIR:VPL>/libvpl_full.pdb
      /PDBSTRIPPED:$<TARGET_PDB_FILE_DIR:VPL>/libvpl.pdb
    >
    $<$<PLATFORM_ID:Linux>:LINKER:--no-undefined,-z,relro,-z,now,-z,noexecstack,--no-as-needed>
    $<$<PLATFORM_ID:Linux>:-fstack-protector-strong>
  )

set_target_properties(VPL PROPERTIES
    PDB_NAME libvpl_full 
    PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
)