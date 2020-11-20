if(OPEN_SOURCE)
    return()
endif()

#set(IPPLIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)

#target_link_libraries(${plugin_name} PUBLIC "${IPPLIBS}" $<$<NOT:$<PLATFORM_ID:Windows>>:SafeString>)

