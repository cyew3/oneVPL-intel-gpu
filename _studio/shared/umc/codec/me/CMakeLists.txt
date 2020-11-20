add_library(me STATIC)

target_include_directories(me PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_sources(me
  PRIVATE
    include/umc_me_cost_func.h
    include/umc_me.h
    include/umc_me_m2.h
    include/umc_me_sadt4x4.h
    include/umc_me_structures.h
    include/umc_vec_prediction.h
    include/umc_vme.h
    src/umc_me_avs.cpp
    src/umc_me_avs_reg_table.cpp
    src/umc_me.cpp
    src/umc_me_m2.cpp
    src/umc_me_regr_tbl.cpp
    src/umc_me_sadt4x4.cpp
    src/umc_vec_prediction.cpp
    src/umc_vme.cpp
  )

target_link_libraries(me PUBLIC umc mfx_static_lib)
set_property(TARGET me PROPERTY FOLDER "umc")
