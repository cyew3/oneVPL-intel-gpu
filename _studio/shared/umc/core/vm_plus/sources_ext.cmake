if(MFX_DISABLE_SW_FALLBACK)
  return()
endif()

target_sources(vm_plus
  PRIVATE
    include/umc_event.h
    include/umc_semaphore.h
    include/umc_mmap.h
    include/umc_sys_info.h
    src/umc_event.cpp
    src/umc_semaphore.cpp
    src/umc_mmap.cpp
    src/umc_sys_info.cpp
)
