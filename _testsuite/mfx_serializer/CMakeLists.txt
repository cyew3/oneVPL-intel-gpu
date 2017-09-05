include_directories (
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-api/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/include
  ${MSDK_TSUITE_ROOT}/mfx_serializer/include
  ${MSDK_STUDIO_ROOT}/shared/include
)

list(APPEND sources
  ${MSDK_TSUITE_ROOT}/mfx_serializer/src/mfx_serial_utils.cpp
  ${MSDK_TSUITE_ROOT}/mfx_serializer/src/mfx_serializer.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/aliasmanager.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/binary.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/contrib/graphbuilder.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/contrib/graphbuilderadapter.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/conversion.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/directives.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/emitfromevents.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/emitter.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/emitterstate.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/emitterutils.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/exp.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/iterator.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/node.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/nodebuilder.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/nodeownership.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/null.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/ostream.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/parser.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/regex.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/scanner.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/scanscalar.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/scantag.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/scantoken.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/simplekey.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/singledocparser.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/stream.cpp
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/yaml-cpp/src/tag.cpp
)

set( defs "-DMFX_VERSION_USE_LATEST ${WARNING_FLAGS}" )
make_library( mfx_serializer none static )
set( defs "" )
