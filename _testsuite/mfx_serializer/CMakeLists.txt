add_library(mfx_serializer STATIC)

target_include_directories(mfx_serializer
  PUBLIC
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/include
    ${MSDK_TSUITE_ROOT}/mfx_serializer/include
    ${MSDK_STUDIO_ROOT}/shared/include
)

target_sources(mfx_serializer
  PRIVATE
    ${MSDK_TSUITE_ROOT}/mfx_serializer/src/mfx_serial_utils.cpp
    ${MSDK_TSUITE_ROOT}/mfx_serializer/src/mfx_serializer.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/aliasmanager.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/binary.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/contrib/graphbuilder.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/contrib/graphbuilderadapter.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/conversion.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/directives.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/emitfromevents.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/emitter.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/emitterstate.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/emitterutils.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/exp.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/iterator.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/node.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/nodebuilder.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/nodeownership.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/null.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/ostream.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/parser.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/regex.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/scanner.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/scanscalar.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/scantag.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/scantoken.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/simplekey.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/singledocparser.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/stream.cpp
    ${MSDK_CONTRIB_ROOT}/yaml-cpp/src/tag.cpp
)

target_link_libraries(mfx_serializer PUBLIC mfx_static_lib)

set_property(TARGET mfx_serializer PROPERTY FOLDER "testsuite")
