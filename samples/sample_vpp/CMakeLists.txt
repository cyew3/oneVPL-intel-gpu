add_executable(sample_vpp)
set_property(TARGET sample_vpp PROPERTY FOLDER "samples")

target_include_directories(sample_vpp PRIVATE include)
target_link_libraries(sample_vpp PRIVATE sample_common)

target_sources(sample_vpp
  PRIVATE
    include/sample_vpp_config.h
    include/sample_vpp_frc_adv.h
    include/sample_vpp_frc.h
    include/sample_vpp_pts.h
    include/sample_vpp_roi.h
    include/sample_vpp_utils.h

    src/sample_vpp_config.cpp
    src/sample_vpp.cpp
    src/sample_vpp_frc_adv.cpp
    src/sample_vpp_frc.cpp
    src/sample_vpp_parser.cpp
    src/sample_vpp_pts.cpp
    src/sample_vpp_roi.cpp
    src/sample_vpp_utils.cpp
  )

install( TARGETS sample_vpp RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )
