mfx_include_dirs( )

include_directories (
  ${CMAKE_SOURCE_DIR}/shared/umc/core/vm/include
  ${CMAKE_SOURCE_DIR}/shared/umc/core/vm_plus/include
  ${CMAKE_SOURCE_DIR}/shared/umc/core/umc/include
)

make_library( umc_core_merged none static )
