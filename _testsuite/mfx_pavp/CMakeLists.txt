if(NOT CMAKE_SYSTEM_NAME MATCHES Windows OR MFX_BUNDLED_IPP OR NOT BUILD_EXTRA OR API_USE_VPL)
  return()
endif()

add_library(sample_protected STATIC)

target_include_directories(sample_protected
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pcdp/samples/sample_common_protected/include
    ${CMAKE_CURRENT_SOURCE_DIR}/pcdp/samples/sample_common_protected/include/vm
    ${CMAKE_CURRENT_SOURCE_DIR}/pcdp/samples/sample_protected/include
    ${MSDK_TSUITE_ROOT}/../igfx_s3dcontrol/include/
    ${MSDK_TSUITE_ROOT}/../igfx_s3dcontrol/include/API/
)

target_sources(sample_protected
  PRIVATE
    pcdp/samples/sample_common_protected/include/base_allocator.h
    pcdp/samples/sample_common_protected/include/d3d_allocator.h
    pcdp/samples/sample_common_protected/include/d3d_device.h
    pcdp/samples/sample_common_protected/include/d3d11_allocator.h
    pcdp/samples/sample_common_protected/include/d3d11_device.h
    pcdp/samples/sample_common_protected/include/hw_device.h
    pcdp/samples/sample_common_protected/include/sample_defs.h
    pcdp/samples/sample_common_protected/include/sample_utils.h
    pcdp/samples/sample_common_protected/include/sysmem_allocator.h
    pcdp/samples/sample_common_protected/include/vpp_ex.h
    pcdp/samples/sample_common_protected/src/base_allocator.cpp
    pcdp/samples/sample_common_protected/src/d3d_allocator.cpp
    pcdp/samples/sample_common_protected/src/d3d_device.cpp
    pcdp/samples/sample_common_protected/src/d3d11_allocator.cpp
    pcdp/samples/sample_common_protected/src/d3d11_device.cpp
    pcdp/samples/sample_common_protected/src/sample_utils.cpp
    pcdp/samples/sample_common_protected/src/sysmem_allocator.cpp
    pcdp/samples/sample_common_protected/src/vpp_ex.cpp

    pcdp/samples/sample_protected/include/abstract_splitter.h
    pcdp/samples/sample_protected/include/avc_bitstream.h
    pcdp/samples/sample_protected/include/avc_headers.h
    pcdp/samples/sample_protected/include/avc_nal_spl.h
    pcdp/samples/sample_protected/include/avc_spl.h
    pcdp/samples/sample_protected/include/avc_structures.h
    pcdp/samples/sample_protected/include/mpeg2_dec_bstream.h
    pcdp/samples/sample_protected/include/mpeg2_spl.h
    pcdp/samples/sample_protected/include/pcdp_exception.h
    pcdp/samples/sample_protected/include/pcp.h
    pcdp/samples/sample_protected/include/pcp_session.h
    pcdp/samples/sample_protected/include/protected.h
    pcdp/samples/sample_protected/include/vc1_spl.h
    pcdp/samples/sample_protected/include/vc1_spl_defs.h
    pcdp/samples/sample_protected/include/vc1_spl_header_parser.h
    pcdp/samples/sample_protected/src/avc_bitstream.cpp
    pcdp/samples/sample_protected/src/avc_nal_spl.cpp
    pcdp/samples/sample_protected/src/avc_spl.cpp
    pcdp/samples/sample_protected/src/mpeg2_spl.cpp
    pcdp/samples/sample_protected/src/pcp.cpp
    pcdp/samples/sample_protected/src/protected.cpp
    pcdp/samples/sample_protected/src/vc1_spl.cpp
    pcdp/samples/sample_protected/src/vc1_spl_bitplanes.cpp
    pcdp/samples/sample_protected/src/vc1_spl_header_parser.cpp
    pcdp/samples/sample_protected/src/vc1_spl_mv.cpp
)

target_link_libraries(sample_protected
  PUBLIC
    mfx_static_lib
    pavp_sample_common
    IPP::dc
    ippcp_6_1
    sigma
    pavp_dx9
    pavp_dx9auxiliary
    pavp_dx11
)

add_library(mfx_pavp SHARED)

target_include_directories(mfx_pavp
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(mfx_pavp
  PRIVATE
    mfx_pavp.cpp
    mfx_pavp.def
    mfx_pavp.h
)

target_link_libraries(mfx_pavp
  PUBLIC
    sample_protected
    pavp_sample_common
    mfx_shared_lib
    umc
)

set_property(TARGET sample_protected mfx_pavp PROPERTY FOLDER "pavp")
