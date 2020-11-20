add_library(vm_plus STATIC)
set_property(TARGET vm_plus PROPERTY FOLDER "umc")

target_sources(vm_plus
  PRIVATE
    include/sys/mfx_vm++_pthread_unix.h
    include/sys/mfx_vm++_pthread_windows.h
    include/umc_event.h
    include/umc_mmap.h
    include/umc_mutex.h
    include/umc_semaphore.h
    include/umc_sys_info.h
    include/mfx_vm++_pthread.h

    src/mfx_vm++_pthread.cpp
    src/mfx_vm++_pthread_unix.cpp
    src/mfx_vm++_pthread_windows.cpp
    src/umc_event.cpp
    src/umc_semaphore.cpp
  )


target_include_directories(vm_plus
  PUBLIC
    include
)
target_link_libraries(vm_plus PUBLIC vm umc mfx_static_lib)

include(sources_ext.cmake OPTIONAL)
