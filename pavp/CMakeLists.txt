if(NOT CMAKE_SYSTEM_NAME MATCHES Windows OR MFX_BUNDLED_IPP OR NOT BUILD_EXTRA)
  return()
endif()


add_library(pavp_dx9 STATIC)
target_sources(pavp_dx9 PRIVATE core/pavp_dx9/pavp_dx9.cpp)
target_link_libraries(pavp_dx9 PUBLIC mfx_static_lib pavp_sample_common)


add_library(pavp_dx9auxiliary STATIC)
target_sources(pavp_dx9auxiliary PRIVATE core/pavp_dx9auxiliary/pavp_dx9auxiliary.cpp)
target_link_libraries(pavp_dx9auxiliary PUBLIC mfx_static_lib pavp_sample_common PRIVATE Dwmapi)


add_library(pavp_dx11 STATIC)
target_sources(pavp_dx11 PRIVATE core/pavp_dx11/pavp_dx11.cpp)
target_link_libraries(pavp_dx11 PUBLIC mfx_static_lib pavp_sample_common)


add_library(sigma STATIC)
target_include_directories(sigma
  PRIVATE
    core/sigma
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ippcp_6_1/include
)
target_sources(sigma
  PRIVATE
    core/sigma/bignumber.cpp
    core/sigma/bignumber.h
    core/sigma/cdg_status_codes.h
    core/sigma/ecpf.cpp
    core/sigma/ecpf.h
    core/sigma/intelsigma.cpp
    core/sigma/le2be_macros.h
    core/sigma/pf.cpp
    core/sigma/pf.h
    core/sigma/sigma_crypto_utils.cpp
    core/sigma/sigma_crypto_utils.h
)
target_link_libraries(sigma PUBLIC mfx_static_lib pavp_sample_common ippcp_6_1 epid_1_0)


add_library(ippcp_6_1 STATIC)

target_include_directories(ippcp_6_1
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ippcp_6_1/include
)

target_sources(ippcp_6_1
  PRIVATE
    thirdparty/ippcp_6_1/include/ippcp.h
    thirdparty/ippcp_6_1/include/ippdefs.h
    thirdparty/ippcp_6_1/include/ippversion.h

    thirdparty/ippcp_6_1/src/dependenceip.h
    thirdparty/ippcp_6_1/src/dependencesp.h
    thirdparty/ippcp_6_1/src/owncp.h
    thirdparty/ippcp_6_1/src/owndefs.h
    thirdparty/ippcp_6_1/src/pcpbn.h
    thirdparty/ippcp_6_1/src/pcpbn32.h
    thirdparty/ippcp_6_1/src/pcpbn32ca.c
    thirdparty/ippcp_6_1/src/pcpbnca.c
    thirdparty/ippcp_6_1/src/pcpbnext.h
    thirdparty/ippcp_6_1/src/pcpbnextpxca.c
    thirdparty/ippcp_6_1/src/pcpbnpxca.c
    thirdparty/ippcp_6_1/src/pcpbnresource.h
    thirdparty/ippcp_6_1/src/pcpbnresourceca.c
    thirdparty/ippcp_6_1/src/pcpbnsetca.c
    thirdparty/ippcp_6_1/src/pcpbnu.h
    thirdparty/ippcp_6_1/src/pcpbnuca.c
    thirdparty/ippcp_6_1/src/pcpbnuext.h
    thirdparty/ippcp_6_1/src/pcpbnuextpxca.c
    thirdparty/ippcp_6_1/src/pcpbnupxca.c
    thirdparty/ippcp_6_1/src/pcpbnuresource.h
    thirdparty/ippcp_6_1/src/pcpbnusetca.c
    thirdparty/ippcp_6_1/src/pcpciphertool.h
    thirdparty/ippcp_6_1/src/pcpciphertoolpxca.c
    thirdparty/ippcp_6_1/src/pcpcmac.h
    thirdparty/ippcp_6_1/src/pcpeccb.h
    thirdparty/ippcp_6_1/src/pcpeccbpoint.h
    thirdparty/ippcp_6_1/src/pcpeccp.h
    thirdparty/ippcp_6_1/src/pcpeccpdpca.c
    thirdparty/ippcp_6_1/src/pcpeccpgenkeyca.c
    thirdparty/ippcp_6_1/src/pcpeccpinitca.c
    thirdparty/ippcp_6_1/src/pcpeccpmethod.h
    thirdparty/ippcp_6_1/src/pcpeccpmethod128.h
    thirdparty/ippcp_6_1/src/pcpeccpmethod128ca.c
    thirdparty/ippcp_6_1/src/pcpeccpmethod192.h
    thirdparty/ippcp_6_1/src/pcpeccpmethod192ca.c
    thirdparty/ippcp_6_1/src/pcpeccpmethod224.h
    thirdparty/ippcp_6_1/src/pcpeccpmethod224ca.c
    thirdparty/ippcp_6_1/src/pcpeccpmethod256.h
    thirdparty/ippcp_6_1/src/pcpeccpmethod256ca.c
    thirdparty/ippcp_6_1/src/pcpeccpmethod384.h
    thirdparty/ippcp_6_1/src/pcpeccpmethod384ca.c
    thirdparty/ippcp_6_1/src/pcpeccpmethod521.h
    thirdparty/ippcp_6_1/src/pcpeccpmethod521ca.c
    thirdparty/ippcp_6_1/src/pcpeccpmethodcom.h
    thirdparty/ippcp_6_1/src/pcpeccpmethodcomca.c
    thirdparty/ippcp_6_1/src/pcpeccppoint.h
    thirdparty/ippcp_6_1/src/pcpeccppointca.c
    thirdparty/ippcp_6_1/src/pcpeccppointopca.c
    thirdparty/ippcp_6_1/src/pcpeccppublickeyca.c
    thirdparty/ippcp_6_1/src/pcpeccpsetkeyca.c
    thirdparty/ippcp_6_1/src/pcpeccpsigndsaca.c
    thirdparty/ippcp_6_1/src/pcpeccpstdca.c
    thirdparty/ippcp_6_1/src/pcpeccpvalidatekeyca.c
    thirdparty/ippcp_6_1/src/pcpeccpverifydsaca.c
    thirdparty/ippcp_6_1/src/pcpeccrandomizer.h
    thirdparty/ippcp_6_1/src/pcpecgfp.c
    thirdparty/ippcp_6_1/src/pcpecgfp.h
    thirdparty/ippcp_6_1/src/pcpecgfppoint.c
    thirdparty/ippcp_6_1/src/pcpecgfppointca.c
    thirdparty/ippcp_6_1/src/pcpecgfpx.c
    thirdparty/ippcp_6_1/src/pcpecgfpx.h
    thirdparty/ippcp_6_1/src/pcpecgfpxpoint.c
    thirdparty/ippcp_6_1/src/pcpecgfpxpointca.c
    thirdparty/ippcp_6_1/src/pcpgf128mul.h
    thirdparty/ippcp_6_1/src/pcpgfp.c
    thirdparty/ippcp_6_1/src/pcpgfp.h
    thirdparty/ippcp_6_1/src/pcpgfpca.c
    thirdparty/ippcp_6_1/src/pcpgfpext.c
    thirdparty/ippcp_6_1/src/pcpgfpext.h
    thirdparty/ippcp_6_1/src/pcpgfpextca.c
    thirdparty/ippcp_6_1/src/pcpgfpqext.c
    thirdparty/ippcp_6_1/src/pcpgfpqext.h
    thirdparty/ippcp_6_1/src/pcpgfpqextca.c
    thirdparty/ippcp_6_1/src/pcphmac.h
    thirdparty/ippcp_6_1/src/pcphmacsha256ca.c
    thirdparty/ippcp_6_1/src/pcpmontexpbinca.c
    thirdparty/ippcp_6_1/src/pcpmontext.h
    thirdparty/ippcp_6_1/src/pcpmontgomery.h
    thirdparty/ippcp_6_1/src/pcpmontgomeryca.c
    thirdparty/ippcp_6_1/src/pcpmontmulca.c
    thirdparty/ippcp_6_1/src/pcpmontmulpxca.c
    thirdparty/ippcp_6_1/src/pcpmulbnuk.h
    thirdparty/ippcp_6_1/src/pcpmulbnupxca.c
    thirdparty/ippcp_6_1/src/pcpname.h
    thirdparty/ippcp_6_1/src/pcppma.h
    thirdparty/ippcp_6_1/src/pcppma128.c
    thirdparty/ippcp_6_1/src/pcppma128.h
    thirdparty/ippcp_6_1/src/pcppma192.c
    thirdparty/ippcp_6_1/src/pcppma192.h
    thirdparty/ippcp_6_1/src/pcppma224.c
    thirdparty/ippcp_6_1/src/pcppma224.h
    thirdparty/ippcp_6_1/src/pcppma256.c
    thirdparty/ippcp_6_1/src/pcppma256.h
    thirdparty/ippcp_6_1/src/pcppma384.c
    thirdparty/ippcp_6_1/src/pcppma384.h
    thirdparty/ippcp_6_1/src/pcppma521.c
    thirdparty/ippcp_6_1/src/pcppma521.h
    thirdparty/ippcp_6_1/src/pcppmafix.h
    thirdparty/ippcp_6_1/src/pcppoly.h
    thirdparty/ippcp_6_1/src/pcppolyarith.h
    thirdparty/ippcp_6_1/src/pcppolyresource.h
    thirdparty/ippcp_6_1/src/pcpprimeg.h
    thirdparty/ippcp_6_1/src/pcpprimeginitca.c
    thirdparty/ippcp_6_1/src/pcpprimenumber_nnv.h
    thirdparty/ippcp_6_1/src/pcpprng.h
    thirdparty/ippcp_6_1/src/pcpprng_nnv.h
    thirdparty/ippcp_6_1/src/pcpprngenca.c
    thirdparty/ippcp_6_1/src/pcpprnginitca.c
    thirdparty/ippcp_6_1/src/pcpprngsetca.c
    thirdparty/ippcp_6_1/src/pcprij.h
    thirdparty/ippcp_6_1/src/pcprij128ctrca.c
    thirdparty/ippcp_6_1/src/pcprij128decryptcbcca.c
    thirdparty/ippcp_6_1/src/pcprij128decryptecbca.c
    thirdparty/ippcp_6_1/src/pcprij128decryptpxca.c
    thirdparty/ippcp_6_1/src/pcprij128encryptcbcca.c
    thirdparty/ippcp_6_1/src/pcprij128encryptecbca.c
    thirdparty/ippcp_6_1/src/pcprij128encryptpxca.c
    thirdparty/ippcp_6_1/src/pcprij128initca.c
    thirdparty/ippcp_6_1/src/pcprij128safe.c
    thirdparty/ippcp_6_1/src/pcprij128safe.h
    thirdparty/ippcp_6_1/src/pcprij128safedecpxca.c
    thirdparty/ippcp_6_1/src/pcprij128safeencpxca.c
    thirdparty/ippcp_6_1/src/pcprijdecsboxca.c
    thirdparty/ippcp_6_1/src/pcprijdectableca.c
    thirdparty/ippcp_6_1/src/pcprijencsboxca.c
    thirdparty/ippcp_6_1/src/pcprijenctableca.c
    thirdparty/ippcp_6_1/src/pcprijkeysca.c
    thirdparty/ippcp_6_1/src/pcprijtables.h
    thirdparty/ippcp_6_1/src/pcpsha1ca.c
    thirdparty/ippcp_6_1/src/pcpsha1cntca.c
    thirdparty/ippcp_6_1/src/pcpsha1pxca.c
    thirdparty/ippcp_6_1/src/pcpsha256ca.c
    thirdparty/ippcp_6_1/src/pcpsha256cntca.c
    thirdparty/ippcp_6_1/src/pcpsha256pxca.c
    thirdparty/ippcp_6_1/src/pcpshs.h
    thirdparty/ippcp_6_1/src/pcptatep.h
    thirdparty/ippcp_6_1/src/pcptatepade.c
    thirdparty/ippcp_6_1/src/pcptatepadeca.c
    thirdparty/ippcp_6_1/src/pcptf.h
    thirdparty/ippcp_6_1/src/pcpvariant.h
    thirdparty/ippcp_6_1/src/pcpvariant_mapd.h
    thirdparty/ippcp_6_1/src/pcpver.h
    thirdparty/ippcp_6_1/src/precomp.c
    thirdparty/ippcp_6_1/src/precomp.h
)

target_compile_definitions(ippcp_6_1
  PRIVATE
    _IPP_v50_
    _RIPP
    _PX
)

target_link_libraries(ippcp_6_1 PUBLIC mfx_static_lib)


add_library(epid_1_0 STATIC)

target_include_directories(epid_1_0
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/epid_1_0/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ippcp_6_1/include
)

target_sources(epid_1_0
  PRIVATE
    thirdparty/epid_1_0/epid/context.c
    thirdparty/epid_1_0/epid/errors.c
    thirdparty/epid_1_0/epid/memutils.c
    thirdparty/epid_1_0/epid/multiexp.c
    thirdparty/epid_1_0/epid/octstring.c
    thirdparty/epid_1_0/epid/printutils.c
    thirdparty/epid_1_0/epid/sigutils.c
    thirdparty/epid_1_0/epid/verifier.c
    thirdparty/epid_1_0/epid/context.h
    thirdparty/epid_1_0/epid/memutils.h
    thirdparty/epid_1_0/epid/multiexp.h
    thirdparty/epid_1_0/epid/octstring.h
    thirdparty/epid_1_0/epid/printutils.h
    thirdparty/epid_1_0/epid/sigutils.h
    thirdparty/epid_1_0/include/epid_constants.h
    thirdparty/epid_1_0/include/epid_errors.h
    thirdparty/epid_1_0/include/epid_issuer.h
    thirdparty/epid_1_0/include/epid_macros.h
    thirdparty/epid_1_0/include/epid_member.h
    thirdparty/epid_1_0/include/epid_prng.h
    thirdparty/epid_1_0/include/epid_types.h
    thirdparty/epid_1_0/include/epid_verifier.h
)

target_link_libraries(epid_1_0 PUBLIC mfx_static_lib ippcp_6_1)

add_library(pavp_video_lib STATIC)

target_include_directories(pavp_video_lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/samples/pavp_video/pavp_video_lib
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ippcp_6_1/include
)

target_sources(pavp_video_lib
  PRIVATE
    samples/pavp_video/pavp_video_lib/pavp_video_lib.cpp
    samples/pavp_video/pavp_video_lib/pavp_video_lib_dx11.cpp
    samples/pavp_video/pavp_video_lib/pavp_video_lib_dx9.cpp
    samples/pavp_video/pavp_video_lib/pavp_video_lib.h
    samples/pavp_video/pavp_video_lib/pavp_video_lib_dx11.h
    samples/pavp_video/pavp_video_lib/pavp_video_lib_dx9.h
    samples/pavp_video/pavp_video_lib/pavp_video_lib_internal.h
)

target_link_libraries(pavp_video_lib PUBLIC mfx_static_lib pavp_sample_common ippcp_6_1)


add_library(win8ui_video_lib STATIC)

target_include_directories(win8ui_video_lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/samples/win8ui_video/win8ui_video_lib
)

target_sources(win8ui_video_lib
  PRIVATE
      samples/win8ui_video/win8ui_video_lib/device.cpp
      samples/win8ui_video/win8ui_video_lib/videoprotection.cpp
      samples/win8ui_video/win8ui_video_lib/win8ui_video_lib.h
)

target_link_libraries(win8ui_video_lib PUBLIC mfx_static_lib pavp_sample_common)


add_library(pavp_sample_common STATIC)

target_include_directories(pavp_sample_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/samples/pavp_sample_common
)

target_sources(pavp_sample_common
  PRIVATE
    samples/pavp_sample_common/auth_channel_dx11.cpp
    samples/pavp_sample_common/auth_channel_dx9.cpp
    samples/pavp_sample_common/mscrypto.cpp
    samples/pavp_sample_common/mscrypto.h
    samples/pavp_sample_common/pavp_sample_common.cpp
    samples/pavp_sample_common/pavp_sample_common.h
    samples/pavp_sample_common/pavp_sample_common_dx11.cpp
    samples/pavp_sample_common/pavp_sample_common_dx11.h
    samples/pavp_sample_common/pavp_sample_common_dx9.cpp
    samples/pavp_sample_common/pavp_sample_common_dx9.h
    samples/pavp_sample_common/pavp_sample_common_dx9auxuiliary.cpp
    samples/pavp_sample_common/test_keys.cpp
    samples/sigma_sample/sigma_sample.cpp
)

target_link_libraries(pavp_sample_common PUBLIC mfx_static_lib pavp_video_lib win8ui_video_lib PRIVATE bcrypt Crypt32)

set_property(TARGET ippcp_6_1 pavp_sample_common sigma pavp_dx9 pavp_dx9auxiliary pavp_dx11 epid_1_0 pavp_video_lib win8ui_video_lib PROPERTY FOLDER "pavp")
