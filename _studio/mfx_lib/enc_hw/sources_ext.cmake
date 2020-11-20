if(OPEN_SOURCE)
  return()
endif()

target_link_libraries(enc_hw PUBLIC enc)
set_property(TARGET enc_hw PROPERTY FOLDER "")
