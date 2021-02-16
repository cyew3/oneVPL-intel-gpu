if (MFX_DISABLE_SW_FALLBACK AND NOT BUILD_VAL_TOOLS)
  return()
endif()

target_sources(umc
  PRIVATE
    include/umc_audio_codec.h
    include/umc_audio_data.h
    include/umc_audio_decoder.h
    include/umc_audio_encoder.h
    include/umc_audio_render.h
    include/umc_default_frame_allocator.h
    include/umc_default_memory_allocator.h
    include/umc_depricated_splitter.h
    include/umc_index.h
    include/umc_media_buffer.h
    include/umc_media_data_ex.h
    include/umc_muxer.h
    include/umc_par_reader.h
    include/umc_splitter_ex.h
    include/umc_splitter.h
    include/umc_video_render.h

    src/umc_audio_codec.cpp
    src/umc_audio_render.cpp
    src/umc_default_frame_allocator.cpp
    src/umc_default_memory_allocator.cpp
    src/umc_deprecated_splitter.cpp
    src/umc_index.cpp
    src/umc_media_buffer.cpp
    src/umc_media_data_ex.cpp
    src/umc_muxer.cpp
    src/umc_par_reader.cpp
    src/umc_splitter.cpp
    src/umc_utils.cpp
    src/umc_video_render.cpp
)
